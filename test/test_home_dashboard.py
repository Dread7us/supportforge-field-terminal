import json
import unittest
from datetime import datetime, timezone
from pathlib import Path
from zoneinfo import ZoneInfo

ROOT = Path(__file__).parents[1]
text = lambda path: (ROOT / path).read_text()
SPEC = json.loads(text("ui/ui_spec.json"))
PAGES = text("src/ui/ui_pages.cpp")
THEME = text("src/ui/ui_theme.h")
MAIN = text("src/main.cpp")
TIME = text("src/time/time_service.cpp")
STATE = text("src/ui/ui_state.cpp")
METRICS = json.loads(text("tools/ui_font_metrics.json"))


class HomeDashboardTests(unittest.TestCase):
    def test_pacific_default_dst_rule_and_immediate_persistence(self):
        self.assertIn('getUChar("tz", 1)', TIME)
        self.assertIn('PST8PDT,M3.2.0,M11.1.0', TIME)
        self.assertIn('configTzTime(timezoneRule(snapshot_.timezoneIndex)', TIME)
        self.assertNotIn('configTime(0, 0', TIME)
        setter = TIME[TIME.index("bool TimeService::setTimezone"):TIME.index("void TimeService::toggleHourFormat")]
        apply_at = setter.index("applyTimezone()")
        persist_at = setter.index('preferences_.putUChar("tz"')
        publish_at = setter.index("publishSettingsChange()")
        self.assertLess(apply_at, persist_at)
        self.assertLess(persist_at, publish_at)
        pacific = ZoneInfo("America/Los_Angeles")
        winter = datetime(2026, 1, 15, 20, tzinfo=timezone.utc).astimezone(pacific)
        summer = datetime(2026, 7, 15, 20, tzinfo=timezone.utc).astimezone(pacific)
        self.assertEqual((winter.hour, winter.utcoffset().total_seconds()), (12, -8 * 3600))
        self.assertEqual((summer.hour, summer.utcoffset().total_seconds()), (13, -7 * 3600))

    def test_clock_changes_only_on_minute_or_material_state(self):
        self.assertNotIn("second", text("src/time/time_service.h").lower())
        self.assertNotIn("state.second", PAGES + STATE)
        self.assertIn("snapshot_.minute != minute", TIME)
        self.assertIn("a.hour != b.hour || a.minute != b.minute", STATE)
        self.assertIn("kCheckIntervalMs = 1000", TIME)

    def test_home_hero_geometry_and_clock_font_fit(self):
        g = SPEC["geometry"]
        hero, clock, weather = (g[k] for k in ("home_hero_bounds", "home_clock_bounds", "home_weather_bounds"))
        self.assertEqual(hero, [24, 80, 492, 330])
        self.assertLessEqual(clock[1] + clock[3], weather[1])
        self.assertLessEqual(weather[1] + weather[3], hero[1] + hero[3])
        font = METRICS["roles"]["HomeClock"]
        width = lambda value: sum(font["advances"][ord(c) - 32] for c in value)
        for value in ("00:00", "23:59", "12:59 PM", "--:--"):
            self.assertLessEqual(width(value), clock[2], value)
        self.assertLessEqual(font["line_height"], clock[3])
        self.assertIn("epd_fill_rect({clock.x,clock.y,clock.w,clock.h},kPaper,fb)", PAGES)
        self.assertIn("FontRole::HomeClock", PAGES)

    def test_live_weather_city_and_truthful_fallbacks(self):
        home = PAGES[PAGES.index("void home("):PAGES.index("void systems(")]
        for value in ("weatherLocation(s.weather)", "homeWeatherPrimary(s.weather)",
                      "homeWeatherSecondary(s.weather)", "weatherConditionIcon"):
            self.assertIn(value, home)
        for label in ("LOCAL DATE UNAVAILABLE", "LIVE WEATHER UNAVAILABLE",
                      "TAP FOR WEATHER SETUP", "PERCENT UNAVAILABLE",
                      "NO NETWORK CONFIGURED", "CHARGE STATE"):
            self.assertIn(label, PAGES)
        for vague in ('"GPS LOCATION"', '"WEATHER LOCATION"', '"STORED"'):
            self.assertNotIn(vague, home)

    def test_dashboard_cards_and_intuitive_routes(self):
        home = PAGES[PAGES.index("void home("):PAGES.index("void systems(")]
        for label in ("SUPPORTFORGE / GUARDIAN", "SYSTEM HEALTH", "CPU LOAD",
                      "RAM USED", "WI-FI / NETWORK", "BATTERY PERCENT"):
            self.assertIn(label, home)
        self.assertIn("kHomeClockAction", THEME + MAIN)
        self.assertIn("destination=ui::Page::DateTimeSettings", MAIN)
        self.assertIn("kHomeWeatherDetailAction", MAIN)
        self.assertIn("destination=ui::Page::WeatherDetail", MAIN)
        self.assertIn("detailReturnPage=ui::Page::Home", MAIN)


if __name__ == "__main__":
    unittest.main()