import math
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).parents[1]


def text(path):
    return (ROOT / path).read_text()


GPS = text("src/location/gps_manager.cpp")
GPS_H = text("src/location/gps_manager.h")
WEATHER = text("src/weather/weather_manager.cpp")
WEATHER_H = text("src/weather/weather_manager.h")
WIZARD = text("src/weather/weather_wizard.cpp")
PAGES = text("src/ui/ui_pages.cpp")
MAIN = text("src/main.cpp")
CONFIG = text("src/app_config.h")
TELEMETRY = text("src/telemetry/telemetry_manager.cpp")


class LocationWeatherSetupTests(unittest.TestCase):
    def test_gps_truthful_states_and_fresh_receiver_speed_only(self):
        for state in ("Off", "Starting", "Searching", "Fixed", "Stale", "Error"):
            self.assertIn(f"GpsState::{state}", GPS)
        self.assertIn("kFixFreshMs = 5000", GPS)
        self.assertIn("parser_.location.age()", GPS)
        self.assertIn("parser_.speed.isValid()", GPS)
        self.assertIn("parser_.speed.age() <= kFixFreshMs", GPS)
        self.assertIn("next.speedValid = next.fixValid", GPS)
        self.assertNotRegex(GPS, r"distanceKm|last.*latitude|location.*speed")
        self.assertIn('return String("-- ") + location::speedUnitName', PAGES)

    def test_speed_conversion_preference_and_material_redraw_threshold(self):
        self.assertIn("0.621371192", GPS)
        self.assertAlmostEqual(100 * 0.621371192, 62.1371192)
        self.assertIn('preferences_.begin("sf_location"', GPS)
        self.assertIn('getBool("speed_mph", true)', GPS)
        self.assertIn('putBool("speed_mph"', GPS)
        self.assertIn("kSpeedMaterialKmh = 1.0", GPS)
        self.assertIn("fabs(a.speedKmh - b.speedKmh) >= kSpeedMaterialKmh", GPS)

    def test_auto_location_and_weather_source_persist_in_nvs(self):
        self.assertIn('preferences_.begin("sf_weather"', WEATHER)
        self.assertIn('putUChar("source"', WEATHER)
        self.assertIn('getUChar("source",0)', WEATHER)
        for source in ("Gps", "City", "Postal", "Manual", "Disabled"):
            self.assertIn(f"LocationSource::{source}", WEATHER)
        self.assertNotIn("putString(\"source", GPS)

    def test_gps_weather_requires_current_fix(self):
        self.assertIn("if (!location::currentFixUsable(gps))", WIZARD)
        self.assertIn('"VALID GPS FIX REQUIRED"', WIZARD)
        self.assertIn("if (!gpsValid_)", WEATHER)
        self.assertIn("State::GpsFixRequired", WEATHER)

    def test_gps_weather_time_distance_and_manual_rate_limits(self):
        self.assertIn("15UL * 60UL * 1000UL", CONFIG)
        self.assertIn("kWeatherGpsMoveThresholdKm = 10.0", CONFIG)
        self.assertIn("distanceKm(lastRequestLatitude_", WEATHER)
        self.assertIn("!manual&&n.lastSuccessMs", WEATHER)
        self.assertIn("!manual&&lastRequestPositionValid_", WEATHER)
        self.assertIn("kWeatherManualRefreshLimitMs", WEATHER)
        # Haversine sanity model: one degree longitude at equator is ~111 km.
        radius = 6371.0
        distance = radius * 2 * math.atan2(math.sin(math.radians(1) / 2),
                                           math.sqrt(1 - math.sin(math.radians(1) / 2) ** 2))
        self.assertGreater(distance, 100)

    def test_city_results_bounded_selectable_and_not_static(self):
        self.assertIn("SearchResult results[5]", WEATHER_H)
        self.assertIn("if(n.resultCount>=5)break", WEATHER)
        self.assertIn("manager.requestSearch(state_.input,false", WIZARD)
        self.assertIn("manager.saveSearchResult", WIZARD)
        self.assertIn("weather.resultCount", WIZARD)
        self.assertIn("pickerCharacters", WIZARD)

    def test_postal_country_validation_and_unsupported_rejection(self):
        for country in ('"US"', '"CA"', '"GB"', '"AU"'):
            self.assertIn(country, WEATHER + WIZARD)
        self.assertIn('strcmp(c,"CA")&&strcmp(c,"GB")&&strcmp(c,"AU")', WEATHER)
        self.assertIn('"INVALID OR UNSUPPORTED POSTAL"', WIZARD)
        self.assertIn('String("COUNTRY ")+w.country', PAGES)

    def test_manual_coordinate_ranges_and_disclosure_confirmation(self):
        self.assertIn("a>=-90&&a<=90&&o>=-180&&o<=180", WEATHER)
        self.assertIn('"LATITUDE MUST BE -90 TO 90"', WIZARD)
        self.assertIn('"LONGITUDE MUST BE -180 TO 180"', WIZARD)
        self.assertIn("COORDINATES WILL BE SENT TO THE WEATHER PROVIDER", PAGES)
        self.assertIn("SAVE & FETCH", PAGES)

    def test_privacy_diagnostics_are_allowlisted(self):
        serial = "\n".join(line.strip() for line in (GPS + "\n" + WEATHER).splitlines()
                           if "Serial." in line)
        allowed_prefixes = (
            'Serial.printf("GPS state=%s fix-valid=%s speed-valid=%s',
            'Serial.printf("WEATHER location_source=%s',
            'Serial.printf("WEATHER configured=%s',
            'Serial.printf("WEATHER result=%s',
        )
        for line in serial.splitlines():
            self.assertTrue(line.startswith(allowed_prefixes), line)
        for forbidden in ("latitude", "longitude", "postal", "requestedSearch_", "url", "body",
                          "open-meteo", "kWeatherLatitude", "kWeatherLongitude"):
            self.assertNotIn(forbidden, serial)

    def test_guardian_isolation_and_hardware_contracts(self):
        self.assertNotIn("TelemetryManager", WEATHER + WEATHER_H + GPS + GPS_H)
        self.assertNotIn("consecutiveFailedCycles", WEATHER + WEATHER_H)
        self.assertNotIn("WeatherManager", TELEMETRY)
        self.assertIn("hq::kBoard.gpsRx, -1", GPS)
        self.assertNotRegex(GPS, r"serial_\.(?:write|print|printf)")
        self.assertNotIn(".transmit(", MAIN)
        self.assertIn("gpsManager.begin(false)", MAIN)
        self.assertIn("setSharedRail(true)", MAIN)

    def test_location_and_wizard_entry_points_and_preferences(self):
        for action in ("kHomeWeatherAction", "kLocationWeatherSetupAction", "kSettingsWeatherAction"):
            self.assertIn(action, MAIN)
        for label in ("WEATHER LOCATION", "USE GPS", "SEARCH CITY", "ZIP / POSTAL",
                      "ENTER COORDINATES", "DISABLE WEATHER", "DISPLAY PREFERENCES"):
            self.assertIn(label, PAGES)
        for key in ("fahrenheit", "show_temp", "show_cond", "show_city", "show_feels"):
            self.assertIn(key, WEATHER)


if __name__ == "__main__":
    unittest.main()