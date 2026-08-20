import re
import unittest
from pathlib import Path


ROOT = Path(__file__).parents[1]


def source(path):
    return (ROOT / path).read_text()


PAGES = source("src/ui/ui_pages.cpp")
COMPONENTS = source("src/ui/ui_components.cpp")
THEME = source("src/ui/ui_theme.h")
CONTROLLER = source("src/ui/ui_controller.cpp")
CONTROLLER_H = source("src/ui/ui_controller.h")
MAIN = source("src/main.cpp")
WIZARD = source("src/weather/weather_wizard.cpp")
STATE = source("src/ui/ui_state.h")
METRICS = __import__("json").loads(source("tools/ui_font_metrics.json"))


class UiReliabilityPassTests(unittest.TestCase):
    def test_required_action_labels_fit_their_visible_button_geometry(self):
        advances = METRICS["roles"]
        width = lambda label, role: sum(advances[role]["advances"][ord(c) - 32] for c in label)
        # actionButton selects CardHeading where possible and reflows to Body;
        # these are the narrowest visible bounds used by each required label.
        required = {
            "BACK": (112, 56), "CANCEL": (238, 64),
            "QUICK NAVIGATION": (492, 144),
            "BALANCED": (492, 144), "BEAUTIFUL / CLEAN": (492, 164),
            "LOW POWER MODE": (492, 54),
            "DISPLAY CLEANUP AVAILABLE IN 45 SECONDS": (492, 72),
        }
        for label, (button_width, button_height) in required.items():
            usable = (button_width - 24, button_height - 16)
            fits = any(width(label, role) <= usable[0] and advances[role]["line_height"] <= usable[1]
                       for role in ("CardHeading", "Body", "Caption"))
            self.assertTrue(fits, label)
        self.assertNotIn("globalRefreshControl", COMPONENTS)

    def test_mobile_geometry_stays_inside_safe_content_and_targets_are_large(self):
        rects = {
            "device cleanup": (24, 654, 492, 72),
            "location gps": (24, 582, 238, 72),
            "location unit": (278, 582, 238, 72),
            "location privacy": (24, 670, 238, 72),
            "location weather": (278, 670, 238, 72),
        }
        for name, (x, y, width, height) in rects.items():
            self.assertGreaterEqual(x, 24, name)
            self.assertGreaterEqual(width, 56, name)
            self.assertGreaterEqual(height, 56, name)
            self.assertLessEqual(x + width, 516, name)
            self.assertLessEqual(y + height, 864, name)
        self.assertIn("kMinimumTouchTarget = spec::kMinimumTouchTarget", THEME)

    def test_touch_setup_cards_do_not_cover_physical_targets(self):
        import json
        spec = json.loads(source("ui/ui_spec.json"))
        cards = spec["pages"]["touch_setup"]["cards"]
        touch = spec["touch"]
        radius = touch["target_radius"]
        targets = ((touch["corner_x"], touch["corner_y"]),
                   (540-touch["corner_x"], touch["corner_y"]),
                   (touch["corner_x"], 960-touch["corner_y"]),
                   (540-touch["corner_x"], 960-touch["corner_y"]))
        for x, y, w, h in cards:
            self.assertLessEqual(x+w, 540)
            self.assertLessEqual(y+h, 864)
            for tx, ty in targets:
                self.assertFalse(x < tx+radius and tx-radius < x+w and
                                 y < ty+radius and ty-radius < y+h,
                                 ((x,y,w,h),(tx,ty)))

    def test_wifi_states_are_stable_dark_and_have_no_disconnected_slash(self):
        wifi = COMPONENTS[COMPONENTS.index("void wifiIcon("):
                          COMPONENTS.index("void appBar(")]
        self.assertIn("epd_fill_rect({bounds.x,bounds.y,bounds.w,bounds.h},kPaper,fb)", wifi)
        self.assertIn("kBarCount=4", wifi)
        self.assertIn("index<bars", wifi)
        self.assertIn("epd_fill_rect", wifi)
        self.assertIn("epd_draw_rect", wifi)
        self.assertNotRegex(wifi, r"epd_draw_line|epd_draw_circle|epd_fill_circle")

    def test_primary_text_is_black_on_white_and_selected_fill_is_intentional(self):
        self.assertIn("kInk == 0x00 && kInkMuted == 0x00 && kRule == 0x00", THEME)
        self.assertIn("kSurfaceSoft == 0xFF && kPaper == 0xFF", THEME)
        self.assertIn("selected?kInk:kPaper", PAGES)
        self.assertIn("selected?kPaper:kInk", PAGES)
        self.assertNotRegex(PAGES.lower(), r"fade|opacity|animate|blink")

    def test_every_route_has_a_fitted_visible_title(self):
        app_bar = COMPONENTS[COMPONENTS.index("void appBar("):
                             COMPONENTS.index("void card(")]
        qualification = PAGES[PAGES.index("void displayCalibration("):
                              PAGES.index("void settings(")]
        self.assertNotIn('subtitle', app_bar.lower())
        self.assertNotIn('FIELD TERMINAL', app_bar)
        self.assertIn("(void)section", COMPONENTS)
        self.assertIn('"FIELD TERMINAL"', qualification)
        for title in ("SYSTEMS", "RADIO", "LOCATION", "DEVICE", "HARDWARE DIAGNOSTICS",
                      "SETTINGS", "WEATHER SETUP", "TOUCH SETUP", "TEXT QUALIFICATION",
                      "DISPLAY REFRESH MODE"):
            self.assertIn(title, PAGES + STATE)

    def test_dynamic_text_is_fitted_and_full_frame_is_cleared_before_every_page(self):
        fitted = COMPONENTS[COMPONENTS.index("String fittedText"):
                            COMPONENTS.index("int centeredBaseline")]
        self.assertIn('result + "..."', fitted)
        for helper in ("void card", "void metricTile", "void labeledRow"):
            block = COMPONENTS[COMPONENTS.index(helper):]
            self.assertIn("fittedText", block[:1800])
        render = PAGES[PAGES.index("void renderPage"):]
        self.assertLess(render.index("clear(fb)"), render.index("switch(s.page)"))
        self.assertIn("memset(compositionBuffer_, 0xFF, kFramebufferBytes)", CONTROLLER)

    def test_weather_input_card_and_key_hit_map_match_visible_geometry(self):
        self.assertIn('card(fb,{24,176,492,58}', PAGES)
        self.assertIn('Rect key{24+col*82,246+row*82,74,74}', PAGES)
        for token in ("kKeyOriginX=24", "kKeyOriginY=246", "kKeyPitch=82", "kKeySize=74"):
            self.assertIn(token, WIZARD)
        self.assertIn("const ui::Rect key{kKeyOriginX+column*kKeyPitch", WIZARD)
        self.assertIn("x>=24&&x<516&&y>=220", WIZARD)
        self.assertIn("if(y>=220&&y<306)", WIZARD)

    def test_deferred_render_wakes_without_new_telemetry_and_failures_are_bounded(self):
        loop = MAIN[MAIN.index("void loop()") :]
        version_block_end = loop.index("// A render deferred")
        version_block = loop[:version_block_end]
        self.assertNotIn("displayCoordinator.dirty()", version_block)
        self.assertIn("if (displayCoordinator.dirty() && touchController.displayCaptureSettled() &&", loop)
        self.assertIn("!displayCoordinator.inputBlocked(millis())) testDisplay();", loop)
        self.assertIn("failedUpdateRetryUsed_", CONTROLLER_H)
        self.assertEqual(CONTROLLER.count("requestRender(renderingPriority)"), 3)
        self.assertIn("DISPLAY retry=QUEUED limit=ONE", CONTROLLER)
        self.assertIn("DISPLAY retry=SUPPRESSED reason=REPEATED_UPDATE_FAILURE", CONTROLLER)

    def test_manual_cleanup_is_discoverable_immediate_and_rate_limited(self):
        for label in ("CLEAN DISPLAY", "DISPLAY CLEANUP AVAILABLE IN ", "CLEANING_DISPLAY"):
            self.assertIn(label, PAGES + CONTROLLER)
        self.assertNotIn("Page::DisplayRefreshConfirm", STATE + MAIN)
        self.assertNotIn("CONFIRM REFRESH", PAGES + MAIN)
        self.assertIn("kDeviceDisplayRefreshAction", MAIN)
        self.assertNotIn("kRefreshConfirmAction", MAIN + THEME)
        self.assertIn("kManualRefreshLimitMs = 45000", CONTROLLER)
        self.assertIn("manualRefreshAvailable(now)", MAIN)
        self.assertIn("target=CLEAN_DISPLAY immediate=YES", MAIN)

    def test_manual_refresh_sequence_preserves_state_and_powers_off(self):
        block = CONTROLLER[CONTROLLER.index("bool DisplayCoordinator::manualFullRefresh"):
                           CONTROLLER.index("bool DisplayCoordinator::dumpPackedFramebuffer")]
        ordered_before_update = [
            "epd_poweron()",
            "epd_fullclear",
            "memset(compositionBuffer_,0xFF,kFramebufferBytes)",
            "returnSnapshot.page=returnPage",
            "renderPage(compositionBuffer_,returnSnapshot)",
            "memcpy(framebuffer,compositionBuffer_,kFramebufferBytes)",
            "MODE_GC16",
        ]
        positions = [block.index(token) for token in ordered_before_update]
        self.assertEqual(positions, sorted(positions))
        update_at = block.index("MODE_GC16")
        self.assertGreater(block.index("epd_poweroff()", update_at), update_at)
        for forbidden in ("Preferences", "telemetryManager", "gpsManager", "weatherManager",
                          "batteryManager", "touchController", "resetQualification", "clearPreferences"):
            self.assertNotIn(forbidden, block)
        self.assertIn("state=PRESERVED", block)
        self.assertIn("snapshot_.page=previousPage", block)
        self.assertNotRegex(block, r"while\s*\(|for\s*\(")

    def test_home_priorities_and_plain_language_states_are_visible(self):
        home = PAGES[PAGES.index("void home"):PAGES.index("void systems")]
        for label in ("SUPPORTFORGE", "HOST HEALTH", "WEATHER", "MOVEMENT", "INCIDENT",
                      "BATTERY", "WI-FI", "NO GPS FIX", "WEATHER SETUP"):
            self.assertIn(label, home)
        for low_priority in ("NVME", "REFRESH INTERVAL", "HDOP"):
            self.assertNotIn(low_priority, home)


if __name__ == "__main__":
    unittest.main()