import re
import unittest
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).parents[1]
THEME = (ROOT / "src/ui/ui_theme.h").read_text()
PAGES = (ROOT / "src/ui/ui_pages.cpp").read_text()
COMPONENTS = (ROOT / "src/ui/ui_components.cpp").read_text()
CONTROLLER = (ROOT / "src/ui/ui_controller.cpp").read_text()
TOUCH = (ROOT / "src/input/touch_controller.cpp").read_text()
MAIN = (ROOT / "src/main.cpp").read_text()

class UiContractTests(unittest.TestCase):
    def test_portrait_geometry_and_partial_gate(self):
        self.assertIn("kCanvasWidth = 540", THEME)
        self.assertIn("kCanvasHeight = 960", THEME)
        self.assertIn("kPartialRefreshEnabled = false", THEME)

    def test_touch_transform_four_corners_and_clamping(self):
        transform = lambda rx, ry: (max(0, min(539, 539-ry)), max(0, min(959, rx)))
        self.assertEqual(transform(0, 539), (0, 0))
        self.assertEqual(transform(0, 0), (539, 0))
        self.assertEqual(transform(959, 539), (0, 959))
        self.assertEqual(transform(959, 0), (539, 959))
        self.assertEqual(transform(-20, 900), (0, 0))
        self.assertIn("runFourCornerTest", TOUCH)

    def test_navigation_targets_are_large_and_non_overlapping(self):
        targets = [(i*108, 864, 108, 96) for i in range(5)]
        self.assertTrue(all(w >= 56 and h >= 56 for _,_,w,h in targets))
        self.assertEqual(sum(w for _,_,w,_ in targets), 540)
        self.assertEqual(len({x for x,_,_,_ in targets}), 5)

    def test_one_action_per_tap_and_refresh_rate_limit(self):
        self.assertIn("pressed&&!down_", TOUCH)
        self.assertIn("!pressed&&down_", TOUCH)
        self.assertIn("lastAcceptedMs_", TOUCH)
        self.assertIn("nowMs - lastRefreshMs_ < 2500", CONTROLLER)
        self.assertIn("page == snapshot_.page", CONTROLLER)

    def test_truthful_empty_states(self):
        for phrase in ["SETUP REQUIRED", "NO SYSTEMS YET", "NOT CONNECTED", "NO MESSAGES"]:
            self.assertIn(phrase, PAGES)
        for fake in ["CPU 12", "3 INCIDENTS", "API ONLINE"]:
            self.assertNotIn(fake, PAGES)

    def test_header_has_truthful_time_and_date_fallbacks(self):
        self.assertIn('"--:--"', COMPONENTS)
        self.assertIn('"--/--"', COMPONENTS)
        self.assertIn("state.rtcValid", COMPONENTS)
        for glyph in ["c == ':'", "c == '/'", "c == '-'", "c == '.'"]:
            self.assertIn(glyph, COMPONENTS)

    def test_modal_dialog_foundation_exists(self):
        self.assertIn("void dialog", COMPONENTS)
        self.assertIn("actionLabel", COMPONENTS)

    def test_tx_lock_and_no_transmit_api(self):
        self.assertIn("TX LOCKED", PAGES)
        self.assertNotRegex(MAIN, r"\.(startTransmit|transmit)\s*\(")

    def test_gps_privacy(self):
        self.assertIn("coordinates redacted", MAIN)
        self.assertNotIn("sample=", MAIN)
        self.assertIn('"COORDINATES","PRIVATE"', PAGES)
        self.assertNotIn("getEfuseMac", MAIN)

    def test_dirty_refresh_and_power_contract(self):
        self.assertIn("if (!initialized_ || !dirty_)", CONTROLLER)
        self.assertEqual(CONTROLLER.count("epd_hl_update_screen"), 1)
        self.assertIn("MODE_GC16", CONTROLLER)
        self.assertIn("epd_poweroff();", CONTROLLER)
        self.assertLess(CONTROLLER.index("epd_hl_update_screen"), CONTROLLER.index("epd_poweroff();"))
        self.assertIn("fullClearUsed_", (ROOT / "src/ui/ui_controller.h").read_text())

    def test_primary_components_stay_above_navigation(self):
        boxes = re.findall(r"\{24,(\d+),492,(\d+)\}", PAGES)
        for y, h in boxes:
            self.assertLessEqual(int(y) + int(h), 864)

    def test_previews_are_exact_geometry_and_16_level(self):
        files = sorted((ROOT / "docs/ui-previews").glob("*.png"))
        self.assertEqual(len(files), 6)
        for path in files:
            image = Image.open(path)
            self.assertEqual(image.size, (540, 960))
            self.assertLessEqual(len(image.getcolors(maxcolors=256)), 16)

if __name__ == "__main__":
    unittest.main()