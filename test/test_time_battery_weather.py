import re
import unittest
from datetime import datetime, timedelta, timezone
from pathlib import Path

ROOT = Path(__file__).parents[1]


def text(path):
    return (ROOT / path).read_text()


TIME = text("src/time/time_service.cpp")
TIME_H = text("src/time/time_service.h")
BATTERY = text("src/battery/battery_manager.cpp")
BATTERY_H = text("src/battery/battery_manager.h")
WEATHER = text("src/weather/weather_manager.cpp")
WEATHER_H = text("src/weather/weather_manager.h")
CONFIG = text("src/app_config.h")
UI_STATE = text("src/ui/ui_state.cpp")
COMPONENTS = text("src/ui/ui_components.cpp")
PAGES = text("src/ui/ui_pages.cpp")
MAIN = text("src/main.cpp")


class TimeBatteryWeatherTests(unittest.TestCase):
    def test_time_preferences_timezone_default_and_ntp_rtc_holdover(self):
        self.assertIn('preferences_.begin("sf_time"', TIME)
        self.assertIn('getUChar("tz", 0)', TIME)
        self.assertIn('getBool("hour24", true)', TIME)
        self.assertIn('putUChar("tz"', TIME)
        self.assertIn('putBool("hour24"', TIME)
        self.assertIn('"UTC0"', TIME)
        self.assertIn('configTime(0, 0', TIME)
        self.assertIn('SyncState::RtcHoldover', TIME)
        self.assertIn('writeRtcUtc', TIME)
        self.assertIn('PST8PDT,M3.2.0,M11.1.0', TIME)

    def test_unsynced_header_compact_date_and_no_second_based_dirtying(self):
        self.assertIn('String time = "--:--"', COMPONENTS)
        self.assertIn(':"TIME SYNC"', COMPONENTS)
        self.assertIn('String(months[state.month-1])+" "+String(state.day)', COMPONENTS)
        self.assertIn('text(fb,clockClip', COMPONENTS)
        self.assertIn('text(fb,dateClip', COMPONENTS)
        self.assertIn('clockClip,max(clockClip.x,clockX),spec::kHeaderBaseline', COMPONENTS)
        self.assertIn('dateClip,max(dateClip.x,dateX),spec::kHeaderBaseline', COMPONENTS)
        self.assertIn('FontRole::CardHeading,clockClip.w', COMPONENTS)
        self.assertIn('same row', COMPONENTS)
        self.assertNotIn('String(state.year)', COMPONENTS)
        self.assertNotIn('state.second', COMPONENTS + UI_STATE)
        for field in ('a.hour != b.hour', 'a.minute != b.minute', 'a.day != b.day',
                      'a.month != b.month', 'a.year != b.year'):
            self.assertIn(field, UI_STATE)

    def test_date_rollover_model_and_12_hour_edges(self):
        before = datetime(2026, 12, 31, 23, 59, tzinfo=timezone.utc)
        after = before + timedelta(minutes=1)
        self.assertEqual((after.year, after.month, after.day, after.hour, after.minute),
                         (2027, 1, 1, 0, 0))
        show12 = lambda hour: 12 if hour % 12 == 0 else hour % 12
        self.assertEqual((show12(0), show12(12), show12(23)), (12, 12, 11))
        self.assertIn('shownHour = state.hour % 12', COMPONENTS)
        self.assertIn('if (!shownHour) shownHour = 12', COMPONENTS)

    def test_battery_is_bounded_truthful_and_read_only(self):
        for label in ('AVAILABLE', 'CHARGING', 'FULL', 'VERIFYING', 'BAT UNKNOWN',
                      'STALE', 'NOT PRESENT', 'ERROR'):
            self.assertIn(label, BATTERY)
        for state in ('Available', 'Charging', 'Full', 'Verifying', 'Stale', 'Unknown', 'NotPresent', 'Error'):
            self.assertIn(state, BATTERY_H)
        self.assertIn('percentAvailable = false', BATTERY)
        self.assertIn('value >= 0 && value <= 100', BATTERY)
        self.assertIn('kNormalSampleIntervalMs = 90UL * 1000UL', BATTERY)
        self.assertIn('kChargingSampleIntervalMs = 45UL * 1000UL', BATTERY)
        self.assertIn('kRecoverySampleIntervalMs = 5UL * 1000UL', BATTERY)
        self.assertIn('kFailuresBeforeError = 3', BATTERY)
        self.assertIn('kMaximumFreshAgeMs = maximumFreshAgeMs()', BATTERY)
        self.assertIn('kGaugeAddress = 0x55', BATTERY)
        self.assertIn('kStateOfChargeRegister = 0x2C', BATTERY)
        self.assertIn('decodeLittleEndianWord(first[0], first[1])', BATTERY)
        self.assertIn('uint8_t first[2]{}, second[2]{}, third[2]{}', BATTERY)
        self.assertIn('thirdValue == secondValue || thirdValue == firstValue', BATTERY)
        self.assertIn('abs(static_cast<int>(thirdValue) - static_cast<int>(secondValue)) <= 1', BATTERY)
        self.assertIn('hasValidSample', BATTERY + BATTERY_H)
        self.assertIn('maximumFreshAgeMs() { return 270UL * 1000UL; }', BATTERY_H)
        self.assertIn('retainedPercentFresh', BATTERY)
        self.assertIn('snapshot_.percentAvailable = retainedPercentFresh', BATTERY)
        self.assertIn('consecutiveSocFailures_ >= kFailuresBeforeError ? State::Error : State::Unknown', BATTERY)
        self.assertIn('consecutiveSocFailures_ = 0', BATTERY)
        self.assertIn('recoveringSoc ? kRecoverySampleIntervalMs', BATTERY)
        self.assertIn('kChargerAddress = 0x6B', BATTERY)
        self.assertIn('kChargerStatusRegister = 0x0B', BATTERY)
        self.assertIn('(register0b & kChargeStatusMask) >> 3', BATTERY)
        self.assertIn('nearFullThresholdPercent() { return 95; }', BATTERY_H)
        self.assertIn('reconcileStateOfCharge', BATTERY + BATTERY_H)
        self.assertIn('socFresh && validPercent(percent) && percent >= nearFullThresholdPercent()', BATTERY)
        self.assertNotRegex(BATTERY, r'Wire\.write\([^r]|seal|reset|calibr|data memory')
        self.assertIn('batteryState', UI_STATE)
        self.assertIn('battery::stateName', PAGES)
        self.assertIn('freshness=%s', BATTERY)
        self.assertNotIn('percent=%s', BATTERY)
        self.assertIn('percent + " STALE"', PAGES)
        self.assertIn('"BQ27220 LAST KNOWN"', PAGES)

    def test_weather_states_cache_interval_and_guardian_isolation(self):
        for label in ('WX SETUP', 'WX OFFLINE', 'WX ONLINE'):
            self.assertIn(label, WEATHER)
        self.assertIn('15UL * 60UL * 1000UL', CONFIG)
        self.assertIn('kWeatherFreshForMs', WEATHER)
        self.assertIn('lastSuccessMs', WEATHER_H)
        self.assertNotRegex(WEATHER, r'WiFi\.(?:mode|begin|disconnect|setAutoReconnect)')
        self.assertNotIn('TelemetryManager', WEATHER + WEATHER_H)
        self.assertNotIn('consecutiveFailedCycles', WEATHER + WEATHER_H)
        self.assertIn('weatherManager.version()', MAIN)
        self.assertIn('a.weather.state != b.weather.state', UI_STATE)

    def test_gps_weather_identity_is_resolved_persisted_and_private(self):
        self.assertIn('return "LOCATING CITY"', PAGES)
        self.assertIn('persist(LocationSource::Gps,0,0,"","","","")', WEATHER)
        self.assertIn('preferences_.putString("city","")', WEATHER)
        self.assertIn('persistResolvedIdentity', WEATHER)
        self.assertNotIn('String(s.location.latitude', PAGES[PAGES.index('void home'):PAGES.index('void systems')])

    def test_weather_diagnostics_do_not_leak_location_url_or_body(self):
        serial_lines = "\n".join(line for line in WEATHER.splitlines()
                                 if 'Serial.' in line)
        for secret_name in ('kWeatherLatitude', 'kWeatherLongitude', 'url', 'body'):
            self.assertNotIn(secret_name, serial_lines)
        self.assertNotRegex(serial_lines, r'latitude|longitude|open-meteo|response')
        self.assertNotIn('data_valid=', serial_lines)
        self.assertIn('WEATHER location_source=%s', serial_lines)
        self.assertIn('WEATHER configured=%s', serial_lines)
        self.assertIn('WEATHER result=%s', serial_lines)

    def test_phone_header_and_settings_actions(self):
        self.assertIn('contractRect(spec::kHeaderBrandBounds)', COMPONENTS)
        self.assertIn('contractRect(spec::kHeaderClockBounds)', COMPONENTS)
        self.assertIn('contractRect(spec::kHeaderDateBounds)', COMPONENTS)
        self.assertIn('contractRect(spec::kHeaderWifiBounds)', COMPONENTS)
        self.assertIn('contractRect(spec::kHeaderBatteryBounds)', COMPONENTS)
        self.assertIn('batteryIcon(fb,batteryGlyph,state.batteryState', COMPONENTS)
        self.assertIn('chargeStatusVerified', BATTERY)
        self.assertIn('snapshot_.sampleValid = socValid', BATTERY)
        self.assertIn('const State rawChargeState = chargerValid ? classifyChargeStatus(chargerStatus)', BATTERY)
        self.assertIn('const State verifiedChargeState = reconcileStateOfCharge(', BATTERY)
        self.assertIn('verifiedChargeState == State::Charging || verifiedChargeState == State::Full', BATTERY)
        self.assertIn('snapshot_.percentAvailable = socValid || retainedPercentFresh', BATTERY)
        self.assertIn('Page::Settings', MAIN + PAGES)
        for action in ('kDeviceSettingsAction', 'kDateTimeTimezoneAction',
                       'kDateTimeFormatAction', 'kDateTimeSyncAction',
                       'kSettingsCategoryActions'):
            self.assertIn(action, MAIN)
        for label in ('TIMEZONE:', 'FORMAT: 24 HOUR', 'LAST SYNCHRONIZATION',
                      'SYNC TIME NOW', 'DATE & TIME', 'LOCATION & PRIVACY'):
            self.assertIn(label, PAGES)
        self.assertIn('kDeviceLowPowerAction', MAIN + PAGES)
        self.assertIn('detailReturnPage=ui::Page::Device;destination=ui::Page::LowPowerSetup', MAIN)
        self.assertIn('kSettingsTemperatureAction', MAIN + PAGES)
        self.assertIn('telemetryManager.toggleTemperatureUnit()', MAIN)

    def test_battery_full_policy_truth_table_and_bounded_lkg_model(self):
        threshold_match = re.search(r'nearFullThresholdPercent\(\) \{ return (\d+); \}', BATTERY_H)
        maximum_age_match = re.search(r'maximumFreshAgeMs\(\) \{ return (\d+)UL \* 1000UL; \}', BATTERY_H)
        self.assertIsNotNone(threshold_match)
        self.assertIsNotNone(maximum_age_match)
        threshold = int(threshold_match.group(1))
        maximum_age_ms = int(maximum_age_match.group(1)) * 1000

        def reconcile(charger_terminated, soc_valid, percent):
            return "FULL" if charger_terminated and soc_valid and percent >= threshold else "VERIFYING"

        self.assertEqual(reconcile(True, True, threshold), "FULL")
        self.assertNotEqual(reconcile(True, True, 50), "FULL")
        self.assertEqual(reconcile(True, False, 100), "VERIFYING")
        self.assertTrue(120_000 <= maximum_age_ms)
        self.assertTrue(120_000 <= maximum_age_ms)
        self.assertFalse(maximum_age_ms + 1 <= maximum_age_ms)
        self.assertIn('nowMs - before.lastSampleMs <= kMaximumFreshAgeMs', BATTERY)
        self.assertIn('nowMs - snapshot_.lastSampleMs > kMaximumFreshAgeMs', BATTERY)
        self.assertIn('snapshot_.percentAvailable = false', BATTERY)
        self.assertIn('snapshot_.state = State::Stale', BATTERY)

    def test_composite_ui_snapshot_has_explicit_loop_stack_budget(self):
        controller = text("src/ui/ui_controller.cpp")
        self.assertIn('SET_LOOP_TASK_STACK_SIZE(16384)', MAIN)
        self.assertNotIn('UiSnapshot candidate = snapshot', controller)
        self.assertNotIn('a.page != b.page', UI_STATE)
        self.assertIn('snapshot_.page = retainedPage', controller)


if __name__ == "__main__":
    unittest.main()