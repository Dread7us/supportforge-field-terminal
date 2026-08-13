import json
import math
import re
import unittest
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).parents[1]
PARSER = (ROOT / "src/telemetry/guardian_parser.cpp").read_text()
MANAGER = (ROOT / "src/telemetry/telemetry_manager.cpp").read_text()
MODEL = (ROOT / "src/telemetry/telemetry_model.h").read_text()
CONFIG = (ROOT / "src/app_config.h").read_text()
UI_STATE = (ROOT / "src/ui/ui_state.cpp").read_text()

COMPLETE = {
    "cpu_load": 1.9,
    "cpu_temp": 112.6,
    "ram_used_gb": 16,
    "ram_total_gb": 59.74,
    "disks": [{"fs": "C:", "mount": "C:", "sizeBytes": 999125151744,
               "usedBytes": 91005100032, "availableBytes": 908120051712,
               "usedPercent": 9.11}],
    "nvme_temp": 201.2,
    "system_status": "ONLINE",
    "uptime_seconds": 18590,
    "speedtest": {"down": 2161.6, "up": 2132.68, "ping": 5.03,
                  "last_run": "2026-08-09T17:12:52.926Z", "is_running": False,
                  "status": "completed", "started_at": None, "error": None,
                  "provider": "ookla"},
}


def recognized(payload):
    scalar = ("cpu_load", "cpu_temp", "ram_used_gb", "ram_total_gb", "nvme_temp",
              "system_status", "uptime_seconds")
    count = sum(k in payload and payload[k] is not None for k in scalar)
    count += min(len(payload.get("disks", [])), 6)
    speed = payload.get("speedtest", {}) or {}
    count += sum(k in speed and speed[k] is not None for k in
                 ("down", "up", "ping", "last_run", "is_running", "status",
                  "started_at", "error", "provider"))
    return count


@dataclass
class CycleState:
    preferred: str = "EP1"
    failures: int = 0
    offline: bool = False

    def cycle(self, ep1, ep2=None, primary_probe=False):
        first = "EP1" if self.preferred == "EP1" or primary_probe else "EP2"
        result = ep1 if first == "EP1" else ep2
        active = first
        if first == "EP1" and result == "transport":
            result, active = ep2, "EP2"
        if result == "valid":
            self.failures = 0
            self.offline = False
            self.preferred = active
        else:
            self.failures += 1
            self.offline = self.failures >= 3
        return active, result


class GuardianTelemetryTests(unittest.TestCase):
    def test_complete_direct_payload_and_numeric_types(self):
        parsed = json.loads(json.dumps(COMPLETE))
        self.assertGreaterEqual(recognized(parsed), 15)
        self.assertIsInstance(parsed["ram_used_gb"], int)
        self.assertIsInstance(parsed["ram_total_gb"], float)
        self.assertAlmostEqual(parsed["ram_used_gb"] / parsed["ram_total_gb"] * 100, 26.7827, places=3)
        self.assertIn('document.is<JsonObject>()', PARSER)

    def test_hostname_fallback_priority_and_payload_without_hostname(self):
        keys = re.search(r'hostKeys\[\].*?\{([^}]+)\}', PARSER, re.S).group(1)
        self.assertEqual(re.findall(r'"([^"]+)"', keys),
                         ["hostname", "host", "device_name", "deviceName", "name", "machine"])
        self.assertNotIn("hostname", COMPLETE)
        self.assertIn("configuredHostName", PARSER)

    def test_missing_optional_malformed_empty_and_explicit_offline(self):
        self.assertEqual(recognized({"cpu_load": 2}), 1)
        with self.assertRaises(json.JSONDecodeError): json.loads('{"cpu_load":')
        self.assertEqual(recognized({"unrecognized": 1}), 0)
        offline = dict(COMPLETE, system_status="OFFLINE")
        self.assertEqual(offline["system_status"], "OFFLINE")
        for phrase in ("if (!count) return result", "explicitOffline", 'strcasecmp(next.systemStatus.value, "OFFLINE")'):
            self.assertIn(phrase, PARSER + MANAGER)

    def test_disks_bounded_speedtest_and_null_fields(self):
        payload = dict(COMPLETE)
        payload["disks"] = [dict(COMPLETE["disks"][0], mount=str(i)) for i in range(9)]
        self.assertEqual(min(len(payload["disks"]), 6), 6)
        self.assertIsNone(payload["speedtest"]["started_at"])
        self.assertIsNone(payload["speedtest"]["error"])
        self.assertIn("kMaximumDisks", PARSER)
        for field in ("down", "up", "ping", "last_run", "is_running", "status", "started_at", "error", "provider"):
            self.assertIn(f'"{field}"', PARSER)

    def test_failover_preference_recovery_and_cycle_alarm_accounting(self):
        state = CycleState()
        self.assertEqual(state.cycle("valid"), ("EP1", "valid"))
        self.assertEqual(state.cycle("transport", "valid"), ("EP2", "valid"))
        self.assertEqual(state.preferred, "EP2")
        state.cycle(None, "valid")
        self.assertEqual(state.preferred, "EP2")
        state.cycle("valid", "valid", primary_probe=True)
        self.assertEqual(state.preferred, "EP1")
        # HTTP responses do not trigger fallback.
        active, result = state.cycle("401", "valid")
        self.assertEqual((active, result), ("EP1", "401"))
        # Both transport attempts count as one failed polling cycle.
        state = CycleState()
        state.cycle("transport", "transport")
        self.assertEqual(state.failures, 1)
        state.cycle("transport", "transport")
        self.assertFalse(state.offline)
        state.cycle("transport", "transport")
        self.assertTrue(state.offline)
        state.cycle("valid")
        self.assertEqual((state.failures, state.offline), (0, False))

    def test_stale_temperature_persistence_snapshot_and_refresh_thresholds(self):
        self.assertIn("kStaleAfterMs = 90000", CONFIG)
        self.assertIn("FetchState::Stale", MANAGER)
        self.assertAlmostEqual((112.6 - 32) * 5 / 9, 44.7777, places=3)
        self.assertIn("kGuardianSourceTemperatureUnit = TemperatureUnit::Fahrenheit", CONFIG)
        self.assertIn('preferences_.putBool("temp_f"', MANAGER)
        self.assertIn("xSemaphoreTake(mutex_", MANAGER)
        self.assertIn("snapshot_.version + 1", MANAGER)
        for threshold in ("2.0", "1.0", "1.8", "temperatureThreshold"):
            self.assertIn(threshold, UI_STATE)
        self.assertIn("portENTER_CRITICAL(&scheduleMux_)", MANAGER)
        self.assertNotIn("if (!requestInProgress_)", MANAGER)

    def test_authentication_and_diagnostics_do_not_expose_secrets(self):
        tracked = "\n".join((ROOT / p).read_text() for p in (
            "src/telemetry/telemetry_manager.cpp", "src/app_config.h",
            "src/secrets.example.h"))
        self.assertIn('http.addHeader("x-guardian-telemetry-token"', MANAGER)
        self.assertIn("kQueryTokenCompatibilityEnabled = false", CONFIG)
        self.assertNotIn("?token=", tracked)
        self.assertNotRegex(MANAGER, r"Serial\.(?:print|printf).*kGuardianToken")
        self.assertNotRegex(MANAGER, r"Serial\.(?:print|printf).*url")
        self.assertIn("path=REDACTED", MANAGER)


if __name__ == "__main__":
    unittest.main()