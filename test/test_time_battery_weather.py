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

    def test_unsynced_header_full_date_and_no_second_based_dirtying(self):
        self.assertIn('String time = "--:--"', COMPONENTS)
        self.assertIn(': "TIME SYNC"', COMPONENTS)
        self.assertIn('String(state.year)', COMPONENTS)
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

    def test_battery_is_bounded_truthful_and_has_no_guessed_probe(self):
        for label in ('NOT PRESENT', 'BAT UNKNOWN', 'VALID', 'CHARGING'):
            self.assertIn(label, BATTERY)
        self.assertIn('percentAvailable = false', BATTERY)
        self.assertNotRegex(BATTERY, r'Wire\.|requestFrom|beginTransmission')
        self.assertNotRegex(BATTERY, r'constexpr\s+uint(?:8|16)_t\s+\w*(?:Register|Command)\w*')
        self.assertIn('batteryClassification', UI_STATE)
        self.assertIn('battery::classificationName', PAGES)

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
        self.assertIn('const Rect batteryClip{462, 0, 66', COMPONENTS)
        self.assertIn('icon(fb,Icon::Battery,492,28', COMPONENTS)
        self.assertIn('Page::Settings', MAIN + PAGES)
        for action in ('kDeviceSettingsAction', 'kSettingsTimezoneAction',
                       'kSettingsFormatAction', 'kSettingsSyncAction'):
            self.assertIn(action, MAIN)
        for label in ('TIMEZONE', 'CLOCK FORMAT', 'LAST TIME SYNC', 'SYNC STATUS'):
            self.assertIn(label, PAGES)

    def test_composite_ui_snapshot_has_explicit_loop_stack_budget(self):
        controller = text("src/ui/ui_controller.cpp")
        self.assertIn('SET_LOOP_TASK_STACK_SIZE(16384)', MAIN)
        self.assertNotIn('UiSnapshot candidate = snapshot', controller)
        self.assertNotIn('a.page != b.page', UI_STATE)
        self.assertIn('snapshot_.page = retainedPage', controller)


if __name__ == "__main__":
    unittest.main()