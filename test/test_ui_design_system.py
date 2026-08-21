import re
import unittest
from pathlib import Path


ROOT = Path(__file__).parents[1]


def source(path):
    return (ROOT / path).read_text(encoding="utf-8")


THEME = source("src/ui/ui_theme.h")
COMPONENTS_H = source("src/ui/ui_components.h")
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
        self.assertIn("icon(fb, glyph", action)
        self.assertIn("content.x + kIconBoxSize / 2", action)
        self.assertIn("content.y + content.h / 2", action)
        self.assertIn("labelContent", action)
        declaration = COMPONENTS_H[COMPONENTS_H.index("void actionButton("):
                                   COMPONENTS_H.index("void selectableCard(")]
        self.assertIn("bool selected,", declaration)
        self.assertIn("Icon glyph, bool backLayout = false", declaration)
        self.assertNotIn("Icon::", declaration)

    def test_all_universal_action_icons_have_dark_and_inverse_rendering(self):
        enum = source("src/ui/ui_components.h")
        for icon in ("ChevronLeft", "ChevronRight", "Refresh", "Settings", "Power",
                     "Search", "Close", "Clock", "Calculator", "Display", "Weather",
                     "Cleanup", "Diagnostics", "Keyboard", "Delete", "Light", "Units",
                     "Privacy", "Touch", "Next", "Save"):
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
        self.assertLess(action.index("roundedRect(fb, bounds"), action.index("icon(fb, glyph"))
        self.assertIn("actionButton(fb,kSystemsSectionAction", PAGES)
        self.assertIn("actionButton(fb,kDeviceSettingsAction", PAGES)

    def test_back_arrow_is_explicit_and_compact_keys_are_approved_graphics(self):
        calls = re.findall(r'actionButton\([^;]+\);', PAGES)
        back_calls = [call for call in calls if '"BACK"' in call]
        self.assertTrue(back_calls)
        for call in back_calls:
            self.assertIn('Icon::ChevronLeft,true', call)
        for call in calls:
            if 'Icon::ChevronLeft' in call:
                self.assertIn('"BACK"', call)
        action = COMPONENTS[COMPONENTS.index("void actionButton("):
                            COMPONENTS.index("void text(")]
        self.assertIn("bounds.w <= 120 && label.length() <= 3", action)
        self.assertIn("if (!compactKey)", action)
        self.assertIn("const int labelX = backLayout ? labelContent.x : x", action)

    def test_shared_selectable_cards_and_wifi_icon_use_fixed_icon_geometry(self):
        selectable = COMPONENTS[COMPONENTS.index("void selectableCard("):
                                COMPONENTS.index("void batteryIcon(")]
        self.assertIn("icon(fb,glyph", selectable)
        self.assertIn("selected?kPaper:kInk", selectable)
        self.assertIn("selectableCard(fb,b,title", PAGES)
        self.assertIn("Icon::Display", PAGES)
        self.assertIn("Icon::Location", PAGES)
        wifi = COMPONENTS[COMPONENTS.index("void wifiIcon("):
                          COMPONENTS.index("void appBar(")]
        self.assertIn("epd_fill_rect({bounds.x,bounds.y,bounds.w,bounds.h},kPaper,fb)", wifi)
        self.assertIn("sharedCenterY=bounds.y+bounds.h/2", wifi)
        self.assertIn("bottom=sharedCenterY+kTallestBarHeight/2", wifi)

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

    def test_page_changes_use_physical_cleanup_then_one_content_gc16(self):
        render = CONTROLLER[CONTROLLER.index("bool DisplayCoordinator::renderIfDirty"):
                            CONTROLLER.index("void DisplayCoordinator::printPerformance")]
        self.assertIn("const bool fullPageTransition = renderingPriority == RenderPriority::Navigation", render)
        self.assertIn("hasPresentedPage_ && lastPresentedPage_ != snapshot_.page", CONTROLLER)
        self.assertIn("const bool cleanupThisRender = (bootRecovery && firstUsableFrame && !fullClearUsed_) ||", render)
        self.assertEqual(render.count("epd_fullclear"), 1)
        self.assertEqual(render.count("epd_hl_update_screen"), 1)
        self.assertLess(render.index("epd_fullclear"), render.index("epd_hl_update_screen"))
        self.assertIn('cleanupThisRender?"YES":"NO"', render)
        self.assertNotIn("navigationWash", render)

    def test_ordinary_page_composition_starts_from_white_for_stale_pixel_safety(self):
        render = PAGES[PAGES.index("void renderPage("):]
        self.assertLess(render.index("clear(fb)"), render.index("switch(s.page)"))
        for helper in ("void actionButton", "void appBar", "void metricTile"):
            block = COMPONENTS[COMPONENTS.index(helper):]
            self.assertIn("epd_fill_rect", block[:2200], helper)


if __name__ == "__main__":
    unittest.main()