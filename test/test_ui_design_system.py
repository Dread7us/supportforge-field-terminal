import re
import unittest
from pathlib import Path


ROOT = Path(__file__).parents[1]


def source(path):
    return (ROOT / path).read_text(encoding="utf-8")


THEME = source("src/ui/ui_theme.h")
COMPONENTS = source("src/ui/ui_components.cpp")
PAGES = source("src/ui/ui_pages.cpp")
CONTROLLER = source("src/ui/ui_controller.cpp")


class UiDesignSystemTests(unittest.TestCase):
    def test_shared_tokens_define_one_spacing_shape_and_icon_language(self):
        for token in ("kRadiusSmall", "kRadiusControl", "kRadiusCard",
                      "kControlInset", "kIconBoxSize", "kIconLabelGap"):
            self.assertIn(token, THEME)
        action = COMPONENTS[COMPONENTS.index("void actionButton("):
                            COMPONENTS.index("void text(")]
        self.assertIn("roundedRect(fb, bounds, kRadiusControl", action)
        self.assertIn("icon(fb, actionIcon(label)", action)
        self.assertIn("content.x + kIconBoxSize / 2", action)
        self.assertIn("content.y + content.h / 2", action)
        self.assertIn("labelContent", action)

    def test_all_universal_action_icons_have_dark_and_inverse_rendering(self):
        enum = source("src/ui/ui_components.h")
        for icon in ("ChevronLeft", "ChevronRight", "Refresh", "Settings",
                     "Power", "Search", "Close"):
            self.assertIn(icon, enum)
            self.assertIn(f"case Icon::{icon}", COMPONENTS)
        action = COMPONENTS[COMPONENTS.index("void actionButton("):
                            COMPONENTS.index("void text(")]
        self.assertIn("selected ? kPaper : kInk", action)
        self.assertNotRegex(action.lower(), r"opacity|alpha|gray|grey")

    def test_navigation_icon_and_label_centers_share_fixed_geometry(self):
        nav = COMPONENTS[COMPONENTS.index("void bottomNavigation("):]
        self.assertIn("target.x+target.w/2", nav)
        self.assertIn("kContentBottom+34", nav)
        self.assertIn("kContentBottom+76", nav)
        self.assertIn("active?kPaper:kInk", nav)
        self.assertEqual(nav.count("icon(fb,icons[i]"), 1)
        self.assertNotRegex(nav.lower(), r"opacity|alpha")

    def test_every_titled_shared_action_clears_before_drawing(self):
        action = COMPONENTS[COMPONENTS.index("void actionButton("):
                            COMPONENTS.index("void text(")]
        clear = "epd_fill_rect({bounds.x, bounds.y, bounds.w, bounds.h}, kPaper, fb)"
        self.assertLess(action.index(clear), action.index("roundedRect(fb, bounds"))
        self.assertLess(action.index("roundedRect(fb, bounds"), action.index("icon(fb, actionIcon"))
        self.assertIn("actionButton(fb,kSystemsSectionAction", PAGES)
        self.assertIn("actionButton(fb,kDeviceSettingsAction", PAGES)

    def test_redesign_does_not_change_epaper_refresh_reliability(self):
        self.assertIn("kPartialRefreshEnabled = false", THEME)
        self.assertNotIn("MODE_DU", CONTROLLER)
        self.assertIn("MODE_GC16", CONTROLLER)
        self.assertNotRegex(COMPONENTS + PAGES, r"delay\s*\(")
        self.assertNotRegex(COMPONENTS + PAGES.lower(), r"animat|blink|frame\s*\+\+")
        # Full clear remains confined to controller-owned boot/manual/white-test
        # reliability flows, never ordinary component or page composition.
        self.assertNotIn("epd_fullclear", COMPONENTS + PAGES)
        self.assertNotIn("epd_hl_update_screen", COMPONENTS + PAGES)

    def test_ordinary_page_composition_starts_from_white_for_stale_pixel_safety(self):
        render = PAGES[PAGES.index("void renderPage("):]
        self.assertLess(render.index("clear(fb)"), render.index("switch(s.page)"))
        for helper in ("void actionButton", "void appBar", "void metricTile"):
            block = COMPONENTS[COMPONENTS.index(helper):]
            self.assertIn("epd_fill_rect", block[:2200], helper)


if __name__ == "__main__":
    unittest.main()