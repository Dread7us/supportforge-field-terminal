import re
import unittest
from pathlib import Path


ROOT = Path(__file__).parents[1]


def text(path):
    return (ROOT / path).read_text()


MAIN = text("src/main.cpp")
LOW_POWER = text("src/power/low_power_manager.cpp")
GPS = text("src/location/gps_manager.cpp")
WEATHER = text("src/weather/weather_manager.cpp")
UI_STATE = text("src/ui/ui_state.cpp")
THEME = text("src/ui/ui_theme.h")
PAGES = text("src/ui/ui_pages.cpp")
COMPONENTS = text("src/ui/ui_components.cpp")
DISPLAY = text("src/ui/ui_controller.cpp")
TOUCH = text("src/input/touch_controller.cpp")
WIFI = text("src/network/wifi_manager.cpp")
TELEMETRY = text("src/telemetry/telemetry_manager.cpp")


class PowerInputWeatherContractTests(unittest.TestCase):
    def test_touch_is_sampled_and_routed_before_service_polling(self):
        loop = MAIN[MAIN.index("void loop()") :]
        touch_poll = loop.index("touchController.poll")
        route = loop.index("if (action.type")
        services = [
            loop.index("gpsManager.poll"),
            loop.index("weatherManager.setGpsPosition"),
            loop.index("applyLowPowerPolicy"),
            loop.index("timeService.poll"),
            loop.index("batteryManager.poll"),
        ]
        self.assertLess(touch_poll, route)
        self.assertTrue(all(route < offset for offset in services))

    def test_render_latch_is_bounded_and_sync_gc16_limit_is_truthful(self):
        self.assertIn("RenderPriority::Navigation", DISPLAY)
        self.assertIn("render_queue_hwm=1", DISPLAY)
        self.assertIn("pendingRender_ = priority", DISPLAY)
        self.assertIn("epd_hl_update_screen", DISPLAY)
        self.assertIn("TOUCH display_update=FINISHED queued_action=%s stale_state=%s", TOUCH)
        self.assertIn("if(!preserveQueued)drainStaleReports()", TOUCH)
        self.assertIn('preserveQueued?"PRESERVED":"DRAINED"', TOUCH)
        self.assertIn("mode=POLLING irq=UNAVAILABLE", TOUCH)
        self.assertNotRegex(MAIN + TOUCH, r"touch.*(?:irq|interrupt).*wake|deep.?sleep")

    def test_low_power_presets_windows_critical_hold_and_quiescence(self):
        self.assertIn('preferences_.begin("sf_power"', LOW_POWER)
        self.assertIn('getUChar("preset", 0)', LOW_POWER)
        self.assertIn('putUChar("preset"', LOW_POWER)
        for interval in ("5UL * 60UL", "15UL * 60UL", "30UL * 60UL", "60UL * 60UL"):
            self.assertIn(interval, LOW_POWER)
        self.assertIn("kAwakeWindowMs = 45UL * 1000UL", LOW_POWER)
        self.assertIn("snapshot_.criticalHold", LOW_POWER)
        self.assertIn("telemetryManager.setSuspended(!servicesAwake)", MAIN)
        self.assertIn("weatherManager.setSuspended(!servicesAwake)", MAIN)
        self.assertIn("telemetryManager.idle() && weatherManager.idle()", MAIN)
        self.assertIn("wifiManager.setServicesAllowed(servicesAwake)", MAIN)
        self.assertIn("WiFi.mode(WIFI_OFF)", WIFI)
        self.assertIn("WiFi.disconnect(true, false)", WIFI)
        self.assertNotIn("WiFi.disconnect", TELEMETRY)
        self.assertNotIn("WiFi.begin", TELEMETRY)
        self.assertIn("radio.sleep()", MAIN)
        self.assertNotRegex(MAIN + LOW_POWER, r"esp_deep_sleep|deep.?sleep")

    def test_low_power_shared_rail_preserves_gps_preference(self):
        self.assertIn("void GpsManager::setHardwareEnabled(bool enabled, bool persistPreference)", GPS)
        self.assertIn("if (persistPreference)", GPS)
        self.assertIn("setSharedRail(false, false)", MAIN)
        self.assertIn("gpsWeatherRequired", MAIN)
        self.assertIn("setSharedRail(true,false)", MAIN)
        self.assertIn("gpsManager.setHardwareEnabled(true,false)", MAIN)

    def test_timezone_and_compact_settings_geometry_are_reflowed(self):
        self.assertEqual(THEME.count("{24,150,492,104}"), 1)
        self.assertIn("kTimezoneActions[4].y + kTimezoneActions[4].h < kTimezoneBackAction.y", THEME)
        self.assertIn("b.w-150", PAGES)
        self.assertIn("timezoneDescription(i)", PAGES)
        self.assertIn('"SELECTED":"SELECT"', PAGES)
        self.assertIn("const bool compact = b.h < 100", COMPONENTS)
        self.assertIn("compact ? FontRole::Caption : FontRole::Body", COMPONENTS)

    def test_weather_location_persistence_invalidation_redraw_and_privacy(self):
        for key in ('putString("city"', 'putString("region"', 'putString("country"',
                    'putString("postal"'):
            self.assertIn(key, WEATHER)
        self.assertGreaterEqual(WEATHER.count("clearCurrentConditions(n)"), 2)
        self.assertIn("s.weather.dataAvailable)weatherConditionIcon", PAGES)
        self.assertIn("a.weather.searchState != b.weather.searchState", UI_STATE)
        self.assertIn("a.weather.resultCount != b.weather.resultCount", UI_STATE)
        self.assertIn("a.weather.results[i].label", UI_STATE)
        serial = "\n".join(line for line in WEATHER.splitlines() if "Serial." in line)
        self.assertNotRegex(serial, r"postal|city|latitude|longitude|url|body|open-meteo")

    def test_low_power_ui_has_explicit_exit_and_timer_wording(self):
        for label in ("LOW POWER MODE", "LOW POWER STATUS", "EXIT LOW POWER",
                      "TIMER MONITORING", "TOUCH EXIT"):
            self.assertIn(label, PAGES + UI_STATE)
        self.assertIn("kLowPowerExitAction", MAIN + THEME)


if __name__ == "__main__":
    unittest.main()