import json
import re
import unittest
from pathlib import Path


ROOT = Path(__file__).parents[1]


def text(path):
    return (ROOT / path).read_text(encoding="utf-8")


SPEC = json.loads(text("ui/ui_spec.json"))
METRICS = json.loads(text("tools/ui_font_metrics.json"))
COMPONENTS = text("src/ui/ui_components.cpp")
PAGES = text("src/ui/ui_pages.cpp")
THEME = text("src/ui/ui_theme.h")
MAIN = text("src/main.cpp")
WIFI = text("src/network/wifi_manager.cpp")
WIFI_H = text("src/network/wifi_manager.h")
CONFIG = text("src/app_config.h")
SECRETS_EXAMPLE = text("src/secrets.example.h")
README = text("README.md")
BATTERY = text("src/battery/battery_manager.cpp")
TELEMETRY = text("src/telemetry/telemetry_manager.cpp")
CALCULATOR = text("src/utilities/calculator.cpp")
CALCULATOR_H = text("src/utilities/calculator.h")
UI_STATE = text("src/ui/ui_state.cpp")


def width(value, role):
    advances = METRICS["roles"][role]["advances"]
    return sum(advances[ord(character) - 32] for character in value)


class SettingsWifiCalculatorTests(unittest.TestCase):
    def test_action_labels_use_exact_inset_metric_centering_and_full_clear(self):
        action = COMPONENTS[COMPONENTS.index("void actionButton("):
                            COMPONENTS.index("void text(")]
        self.assertIn("kHorizontalPadding = 16", action)
        self.assertIn("kVerticalPadding = 10", action)
        self.assertIn("epd_fill_rect({bounds.x, bounds.y, bounds.w, bounds.h}, kPaper, fb)", action)
        self.assertIn("centeredBaseline(content, role)", action)
        self.assertNotRegex(action, r"bounds\.y\s*\+\s*\d+.*label")
        content_width = 492 - 32
        content_height = SPEC["geometry"]["minimum_touch_target"] - 20
        labels = ("BACK", "SAVE & CONNECT", "NO USER NETWORK TO FORGET",
                  "TEMPERATURE: FAHRENHEIT", "WEATHER LOCATION SETUP")
        for label in labels:
            role = "CardHeading" if (width(label, "CardHeading") <= content_width and
                                     METRICS["roles"]["CardHeading"]["line_height"] <= content_height) else "Body"
            self.assertLessEqual(width(label, role), content_width, label)
            self.assertLessEqual(METRICS["roles"][role]["line_height"], content_height, label)

    def test_dialog_target_and_card_tile_regions_are_metric_safe(self):
        dialog = COMPONENTS[COMPONENTS.index("void dialog("):
                            COMPONENTS.index("Rect navigationTarget(")]
        self.assertIn("kDialogActionHeight = 56", dialog)
        self.assertIn("kDialogBottomPadding = 24", dialog)
        self.assertIn("kDialogActionHeight >= spec::kMinimumTouchTarget", dialog)
        card = COMPONENTS[COMPONENTS.index("void card("):
                          COMPONENTS.index("void statusPill(")]
        tile = COMPONENTS[COMPONENTS.index("void metricTile("):
                          COMPONENTS.index("void labeledRow(")]
        for section in (card, tile):
            self.assertIn("centeredBaseline(", section)
            self.assertIn("textHeight(", section)
        self.assertIn("bodyY=titleRegion.y+titleRegion.h+4", card)
        self.assertIn("detailY=valueRegion.y+valueRegion.h+4", tile)
        self.assertIn("FontRole::Caption,kPaper", tile)

    def test_wifi_manager_is_bounded_async_owner_and_forget_is_user_only(self):
        self.assertIn("kMaximumScanResults = 6", WIFI_H)
        self.assertIn("kMaximumSsidBytes = 32", WIFI_H)
        self.assertIn("kMaximumPasswordBytes = 63", WIFI_H)
        self.assertIn("WiFi.scanNetworks(true, true, false, 500)", WIFI)
        self.assertIn("WiFi.scanComplete()", WIFI)
        self.assertIn("ScanState::Empty", WIFI + WIFI_H)
        self.assertIn("ScanState::Failed", WIFI + WIFI_H)
        self.assertIn("if (current.scanState == ScanState::Scanning) return", WIFI)
        self.assertIn("reconnectAfterScan_", WIFI + WIFI_H)
        self.assertIn("snapshot_.resultCount < kMaximumScanResults", WIFI)
        services = WIFI[WIFI.index("void WifiManager::setServicesAllowed"):
                        WIFI.index("bool WifiManager::forgetUserCredentials")]
        self.assertIn("snapshot_.scanState = ScanState::Idle", services)
        self.assertIn("snapshot_.resultCount = 0", services)
        self.assertIn("reconnectAfterScan_ = false", services)
        self.assertIn("connectionRequested_ = false", services)
        self.assertNotRegex(WIFI[WIFI.index("bool WifiManager::startScan"):
                                 WIFI.index("void WifiManager::collectScanResults")],
                            r"delay\s*\(|while\s*\(")
        for call in ("WiFi.begin", "WiFi.disconnect", "WiFi.mode"):
            self.assertIn(call, WIFI)
            self.assertNotIn(call, TELEMETRY)
        forget = WIFI[WIFI.index("bool WifiManager::forgetUserCredentials"):
                      WIFI.index("}  // namespace network")]
        self.assertIn('preferences_.remove("ssid")', forget)
        self.assertIn('preferences_.remove("pass")', forget)
        self.assertIn('preferences_.putBool("user", false)', forget)
        self.assertNotIn("preferences_.clear", forget)
        self.assertNotIn("Serial.", WIFI)
        load = WIFI[WIFI.index("bool WifiManager::loadCredential"):
                    WIFI.index("void WifiManager::publish")]
        self.assertNotIn("appconfig::kWifi", load)
        self.assertNotIn("SUPPORTFORGE_WIFI", CONFIG + SECRETS_EXAMPLE)
        self.assertIn("Wi-Fi is intentionally **not** a compile-time secret", README)
        self.assertIn("return to SETUP REQUIRED", PAGES)

    def test_wifi_keyboard_covers_printable_ascii_masks_password_and_refreshes_version(self):
        entry = PAGES[PAGES.index("void wifiEntry("):PAGES.index("void wifiForgetConfirm(")]
        page_literals = entry[entry.index('const char* pages[]='):
                              entry.index('const char* chars=')]
        keyboard = "".join(json.loads(literal) for literal in
                           re.findall(r'"(?:[^"\\]|\\.)*"', page_literals))
        self.assertEqual(len(keyboard), 96)
        self.assertEqual(set(keyboard), {chr(value) for value in range(32, 127)})
        self.assertIn("for(uint8_t i=0;i<s.wifiPasswordLength&&i<24;++i)masked+=\"*\"", PAGES)
        self.assertIn("wifiKeyboardPage=(wifiKeyboardPage+1)%4", MAIN)
        self.assertIn("a.wifi.version != b.wifi.version", UI_STATE)
        self.assertIn("observedWifiVersion", MAIN)
        serial_lines = "\n".join(line for line in WIFI.splitlines() if "Serial." in line)
        self.assertEqual(serial_lines, "")

    def test_canonical_settings_categories_and_routes_are_present(self):
        canonical = ("WI-FI", "DATE & TIME", "DISPLAY", "UNITS", "WEATHER",
                     "LOCATION & PRIVACY", "LOW POWER", "TOUCH", "ABOUT / DIAGNOSTICS")
        settings = PAGES[PAGES.index("void settings("):PAGES.index("void dateTimeSettings(")]
        for label in canonical:
            self.assertIn(f'"{label}"', settings)
        routes = ((0, "WifiSettings"), (1, "DateTimeSettings"), (2, "DisplaySettings"),
                  (3, "UnitsSettings"), (5, "LocationPrivacySettings"))
        for index, page in routes:
            self.assertRegex(MAIN, rf"kSettingsCategoryActions\[{index}\].*Page::{page}")
        for control in ("kDateTimeTimezoneAction", "kDateTimeFormatAction", "kDateTimeSyncAction",
                        "kUnitsTemperatureAction", "kUnitsSpeedAction", "kUnitsElevationAction",
                        "kLocationSettingsGpsAction", "kLocationSettingsPrivacyAction",
                        "kLocationSettingsWeatherAction", "kDetailBackAction"):
            self.assertIn(control, MAIN)
        settings_rects = re.search(r"constexpr Rect kSettingsCategoryActions\[\] = \{(.*?)\};",
                                   THEME, re.S)
        self.assertIsNotNone(settings_rects)
        rects = [tuple(map(int, values)) for values in
                 re.findall(r"\{(\d+),(\d+),(\d+),(\d+)\}", settings_rects.group(1))]
        self.assertEqual(len(rects), 11)
        self.assertTrue(all(height >= 92 for _, _, _, height in rects))
        self.assertLessEqual(max(y + height for _, y, _, height in rects),
                             SPEC["geometry"]["content_bottom"])

    def test_calculator_operations_errors_bounds_and_trailing_display(self):
        for key in ("Decimal", "Add", "Subtract", "Multiply", "Divide", "Equals",
                    "Clear", "Backspace", "ToggleSign"):
            self.assertIn(key, CALCULATOR_H + CALCULATOR)
        self.assertIn("DIVIDE BY ZERO", CALCULATOR)
        self.assertIn("OUT OF RANGE", CALCULATOR)
        self.assertIn("kMaximumInputCharacters = 24", CALCULATOR)
        self.assertIn("kMaximumMagnitude = 1.0e15", CALCULATOR)
        self.assertIn('display_ = "0"; accumulator_ = 0; operation_ = 0', CALCULATOR)
        page = PAGES[PAGES.index("void calculatorPage("):
                     PAGES.index("void displayRefreshMode(")]
        self.assertIn("epd_fill_rect({display.x,display.y,display.w,display.h},kPaper,fb)", page)
        self.assertIn('shown=String("...")+shown.substring(4)', page)
        self.assertIn("kCalculatorBackAction", page + MAIN)
        labels = re.search(r'const char\* labels\[\]=\{([^;]+)\};', page)
        self.assertIsNotNone(labels)
        self.assertEqual(len(re.findall(r'"(?:[^"\\]|\\.)*"', labels.group(1))), 19)
        key_rects = re.search(r"constexpr Rect kCalculatorKeys\[\] = \{(.*?)\};", THEME, re.S)
        self.assertIsNotNone(key_rects)
        rects = [tuple(map(int, values)) for values in
                 re.findall(r"\{(\d+),(\d+),(\d+),(\d+)\}", key_rects.group(1))]
        self.assertEqual(len(rects), 19)
        self.assertTrue(all(width >= SPEC["geometry"]["minimum_touch_target"] and
                            height >= SPEC["geometry"]["minimum_touch_target"]
                            for _, _, width, height in rects))
        self.assertGreaterEqual(max(y + height for _, y, _, height in rects), 920)
        self.assertLessEqual(max(y + height for _, y, _, height in rects),
                             SPEC["canvas"]["height"])

    def test_battery_text_is_metric_centered_and_full_is_verified(self):
        battery = COMPONENTS[COMPONENTS.index("void batteryIcon("):
                             COMPONENTS.index("void circle(")]
        self.assertEqual(battery.count("centeredBaseline(interior, FontRole::Caption)"), 2)
        self.assertNotIn("interior.y + 16", battery)
        app = COMPONENTS[COMPONENTS.index("void appBar("):
                         COMPONENTS.index("void card(")]
        self.assertIn("batteryIcon(fb,batteryGlyph,state.batteryState", app)
        self.assertIn("batteryClip.x+56,spec::kHeaderBaseline-25,80,32", app)
        self.assertNotIn('batteryStateLabel="CHG"', app)
        self.assertIn('batteryStateLabel="FULL"', app)
        self.assertIn("verifiedChargeState == State::Charging || verifiedChargeState == State::Full", BATTERY)

    def test_wifi_back_returns_to_the_route_that_opened_settings(self):
        self.assertIn("ui::Page wifiReturnPage = ui::Page::Settings", MAIN)
        self.assertIn("wifiReturnPage=ui::Page::Device;destination=ui::Page::WifiSettings", MAIN)
        self.assertIn("wifiReturnPage=ui::Page::Settings;destination=ui::Page::WifiSettings", MAIN)
        self.assertIn("destination=wifiReturnPage", MAIN)


if __name__ == "__main__":
    unittest.main()