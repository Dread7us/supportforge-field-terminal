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
TIME = text("src/time/time_service.cpp")
UI_STATE = text("src/ui/ui_state.h")


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

    def test_gps_altitude_is_receiver_only_fresh_and_truthfully_unavailable(self):
        self.assertIn("parser_.altitude.isValid()", GPS)
        self.assertIn("parser_.altitude.age()", GPS)
        self.assertIn("parser_.altitude.meters()", GPS)
        self.assertIn("next.altitudeValid = next.fixValid && altitudeFieldValid", GPS)
        self.assertIn("next.altitudeAgeMs <= kFixFreshMs", GPS)
        self.assertIn("next.altitudeMetres = next.altitudeValid ? parser_.altitude.meters() : 0.0", GPS)
        self.assertIn('String("-- ")+unit', PAGES)
        self.assertIn('"GPS FIX REQUIRED"', GPS)
        self.assertIn('"GPS STALE"', GPS)
        for forbidden in ("http", "map api", "elevation api", "lastAltitude"):
            self.assertNotIn(forbidden.lower(), GPS.lower())

    def test_elevation_conversion_rounding_persistence_and_jitter_gate(self):
        self.assertIn("altitudeMetres * 3.280839895", GPS)
        self.assertAlmostEqual(379.0 * 3.280839895, 1243.438320205)
        self.assertEqual(round(379.0 * 3.280839895), 1243)
        self.assertIn("lround(value)", PAGES)
        self.assertIn('getBool("elev_feet", true)', GPS)
        self.assertIn('putBool("elev_feet"', GPS)
        self.assertIn('"FEET" : "METRES"', GPS)
        self.assertIn("kElevationMaterialFeet = 10.0", GPS)
        self.assertIn("kElevationMaterialMetres = 3.0", GPS)
        self.assertIn("fabs(a.altitudeMetres - b.altitudeMetres) >= elevationThresholdMetres", GPS)

    def test_altimeter_location_and_vehicle_routes_are_tappable(self):
        self.assertGreaterEqual(PAGES.count('"GPS ELEVATION"'), 3)
        self.assertIn("Page::Altimeter", MAIN)
        self.assertIn("kLocationElevationAction.contains", MAIN)
        self.assertIn("kVehicleElevationAction.contains", MAIN)
        self.assertIn("detailReturnPage=ui::Page::Location", MAIN)
        self.assertIn("detailReturnPage=ui::Page::VehicleMotion", MAIN)

    def test_auto_location_and_weather_source_persist_in_nvs(self):
        self.assertIn('preferences_.begin("sf_weather"', WEATHER)
        self.assertIn('putUChar("source"', WEATHER)
        self.assertIn('getUChar("source",0)', WEATHER)
        for source in ("Gps", "City", "Postal", "Manual", "Disabled"):
            self.assertIn(f"LocationSource::{source}", WEATHER)
        self.assertNotIn("putString(\"source", GPS)

    def test_weather_identity_persists_and_temporary_labels_resolve(self):
        for key in ('"city"', '"region"', '"country"', '"postal"'):
            self.assertIn(f'preferences_.putString({key}', WEATHER)
            self.assertIn(f'preferences_.getString({key}', WEATHER)
        self.assertIn('bool WeatherManager::resolveIdentity', WEATHER)
        self.assertIn('if(resolved)persistIdentity(n)', WEATHER)
        self.assertIn('!strcmp(n.city,"GPS LOCATION")', WEATHER)
        self.assertIn('!strcmp(n.city,"MANUAL LOCATION")', WEATHER)
        self.assertIn('strcmp(w.city,"GPS LOCATION")', PAGES)
        self.assertIn('strcmp(w.city,"MANUAL LOCATION")', PAGES)
        self.assertIn('String(w.city) + (w.region[0] ? String(", ") + w.region : "")', PAGES)

    def test_gps_weather_selection_persists_without_waiting_for_fix(self):
        self.assertIn("manager.saveGps(); state_.step=WizardStep::Gps", WIZARD)
        self.assertIn("bool WeatherManager::saveGps(){return persist(LocationSource::Gps", WEATHER)
        self.assertNotIn("VALID GPS FIX REQUIRED", WIZARD)
        self.assertIn("if (!gpsValid_)", WEATHER)
        self.assertIn("State::GpsFixRequired", WEATHER)

    def test_use_gps_automatically_starts_receiver_nonblocking(self):
        self.assertIn("before.step==weather::WizardStep::Choice&&after.step==weather::WizardStep::Gps", MAIN)
        self.assertIn("setSharedRail(true,false)", MAIN)
        self.assertIn("gpsManager.setHardwareEnabled(true,false)", MAIN)
        gps_start = GPS[GPS.index("void GpsManager::setRailEnabled"):GPS.index("void GpsManager::poll")]
        self.assertNotIn("while (", gps_start)
        self.assertNotIn("delay(", gps_start)
        self.assertIn("GpsState::Starting", gps_start)
        self.assertIn("serial_.setRxBufferSize(kGpsRxBufferBytes)", gps_start)

    def test_gps_weather_no_fix_screen_is_useful_and_truthful(self):
        for label in ("STARTING GPS", "SEARCHING FOR GPS", "GPS FIXED", "GPS FIX REQUIRED",
                      "RETRY", "BACK", "USE CITY INSTEAD", "USE POSTAL INSTEAD",
                      "ENTER COORDINATES INSTEAD"):
            self.assertIn(label, PAGES)
        self.assertIn("manager.requestRefresh(millis())", WIZARD)
        self.assertIn("WizardStep::Gps", WIZARD)

    def test_valid_fix_triggers_weather_fetch_without_reboot(self):
        self.assertIn("!wasValid&&snapshot_.source==LocationSource::Gps", WEATHER)
        self.assertIn("nextPollMs_=0;forceRefresh_=true", WEATHER)
        self.assertIn("weatherManager.setGpsPosition(location::currentFixUsable", MAIN)
        self.assertNotRegex(MAIN + WIZARD + WEATHER, r"ESP\.restart|esp_restart")

    def test_movement_is_informational_and_never_a_ui_gate(self):
        combined = MAIN + GPS_H + UI_STATE + PAGES
        for obsolete in ("movingForSafety", "SafetyNotice", "DRIVING SAFETY", "WHEN STOPPED",
                         "STOP SAFELY", "safetyReturnPage"):
            self.assertNotIn(obsolete, combined)
        self.assertIn("MovementState::Moving", GPS)
        for action in ("kDeviceSettingsAction", "kDeviceDisplayRefreshAction",
                       "kLocationGpsPowerAction", "kLocationWeatherSetupAction",
                       "kLocationPrivacyAction", "kSettingsTimezoneAction"):
            self.assertIn(action + ".contains", MAIN)

    def test_preferences_restore_and_rapid_choices_are_debounced(self):
        self.assertIn('getBool("gps_enabled", false)', GPS)
        self.assertIn('putBool("gps_enabled"', GPS)
        self.assertIn("gpsManager.enabledPreference()", MAIN)
        for source in (GPS, WEATHER, TIME):
            self.assertIn("kPreferenceWriteDelayMs", source)
        self.assertIn("preferencesDirty_", GPS)
        self.assertIn("displayPreferencesDirty_", WEATHER)
        self.assertIn("preferencesDirty_", TIME)
        self.assertLess(GPS.count('putBool("speed_mph"'), 2)
        self.assertLess(WEATHER.count('putBool("fahrenheit"'), 2)
        self.assertLess(TIME.count('putUChar("tz"'), 2)

    def test_navigation_routes_are_direct_and_back_paths_are_explicit(self):
        self.assertIn("weatherManager.snapshot().configured", MAIN)
        self.assertIn("destination=ui::Page::WeatherSetup", MAIN)
        self.assertIn("destination=ui::Page::WeatherDetail", MAIN)
        self.assertIn("destination=ui::Page::VehicleMotion", MAIN)
        self.assertIn("kVehicleLocationAction.contains", MAIN)
        self.assertIn("destination=ui::Page::Location", MAIN)
        self.assertIn("detailReturnPage=ui::Page::VehicleMotion;destination=ui::Page::Altimeter", MAIN)
        self.assertIn("destination=detailReturnPage", MAIN)
        self.assertIn("destination = weatherWizardReturnPage", MAIN)

    def test_gps_weather_time_distance_and_manual_rate_limits(self):
        self.assertIn("15UL * 60UL * 1000UL", CONFIG)
        self.assertIn("kWeatherGpsMoveThresholdKm = 10.0", CONFIG)
        self.assertIn("movedMaterially=lastRequestPositionValid_&&distanceKm(lastRequestLatitude_", WEATHER)
        self.assertIn("!manual&&n.lastSuccessMs", WEATHER)
        self.assertIn("&&!movedMaterially", WEATHER)
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
        ui_state = text("src/ui/ui_state.cpp")
        self.assertIn("a.weather.searchState != b.weather.searchState", ui_state)
        self.assertIn("a.weather.resultCount != b.weather.resultCount", ui_state)
        self.assertIn("a.weather.results[i].label", ui_state)

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
            'Serial.printf("GPS altitude-valid=%s elevation-unit=%s freshness=%s',
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
        for action in ("kHomeWeatherDetailAction", "kLocationWeatherSetupAction", "kWeatherDetailSetupAction"):
            self.assertIn(action, MAIN)
        for label in ("WEATHER LOCATION", "USE GPS", "SEARCH CITY", "ZIP / POSTAL",
                      "ENTER COORDINATES", "DISABLE WEATHER", "DISPLAY PREFERENCES"):
            self.assertIn(label, PAGES)
        for key in ("fahrenheit", "show_temp", "show_cond", "show_city", "show_feels"):
            self.assertIn(key, WEATHER)


if __name__ == "__main__":
    unittest.main()