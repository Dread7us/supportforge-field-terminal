import re
import unittest
from pathlib import Path


ROOT = Path(__file__).parents[1]


def text(path):
    return (ROOT / path).read_text(encoding="utf-8")


CONTROLLER = text("src/ui/ui_controller.cpp")
CONTROLLER_H = text("src/ui/ui_controller.h")
STATE = text("src/ui/ui_state.cpp") + text("src/ui/ui_state.h")
PAGES = text("src/ui/ui_pages.cpp")
THEME = text("src/ui/ui_theme.h")
MAIN = text("src/main.cpp")
TOUCH = text("src/input/touch_controller.cpp")
TOUCH_H = text("src/input/touch_controller.h")


class DisplayRefreshModeTests(unittest.TestCase):
    def test_modes_default_persist_and_apply_immediately(self):
        for token in ("QuickNavigation", "Balanced", "BeautifulClean"):
            self.assertIn(token, STATE + CONTROLLER)
        self.assertIn("RefreshMode::Balanced", CONTROLLER_H)
        self.assertIn('preferences_.begin("sf_display"', CONTROLLER)
        self.assertIn('getUChar("refresh_mode"', CONTROLLER)
        self.assertIn('putUChar("refresh_mode"', CONTROLLER)
        self.assertIn("requestRender(RenderPriority::Cosmetic)", CONTROLLER)
        self.assertIn("applied=IMMEDIATE", CONTROLLER)

    def test_exact_cleanup_policy_is_central_and_only_consumes_real_demand(self):
        render = CONTROLLER[CONTROLLER.index("bool DisplayCoordinator::renderIfDirty"):
                            CONTROLLER.index("void DisplayCoordinator::printPerformance")]
        self.assertIn("pendingRender_ == RenderPriority::None", render)
        self.assertIn("refreshMode_ == RefreshMode::BeautifulClean", render)
        self.assertIn("refreshMode_ == RefreshMode::Balanced && renderingPriority == RenderPriority::Navigation", render)
        self.assertNotIn("RefreshMode::QuickNavigation", render[render.index("const bool cleanupThisRender"):render.index("bool displayPowered")])
        self.assertLess(render.index("memset(compositionBuffer_, 0xFF"), render.index("renderPage(compositionBuffer_"))
        self.assertEqual(render.count("epd_hl_update_screen"), 1)
        self.assertIn("MODE_GC16", render)

    def test_mode_page_diagnostics_and_touch_geometry_are_explicit(self):
        for label in ("QUICK NAVIGATION", "BALANCED", "BEAUTIFUL / CLEAN",
                      "DISPLAY MODE", "DISPLAY TIMING", "GC16"):
            self.assertIn(label, PAGES + STATE)
        for action in ("kSettingsRefreshModeAction", "kRefreshModeActions",
                       "kRefreshModeBackAction", "kSettingsLowPowerAction",
                       "kSettingsSyncActionCompact", "kSettingsTouchAction"):
            self.assertIn(action, MAIN + PAGES + THEME)
        self.assertIn("refresh mode choices and Back must be large and non-overlapping", THEME)
        self.assertIn('actionButton(fb,kRefreshModeBackAction,"BACK")', PAGES)

    def test_global_manual_refresh_is_confirmed_rate_limited_and_state_safe(self):
        self.assertIn("kGlobalRefreshAction", MAIN + THEME)
        self.assertIn("globalRefreshControl(fb)", PAGES)
        self.assertIn("kManualRefreshLimitMs = 45000", CONTROLLER)
        self.assertIn("route=CONFIRMATION", MAIN)
        manual = CONTROLLER[CONTROLLER.index("bool DisplayCoordinator::manualFullRefresh"):
                            CONTROLLER.index("bool DisplayCoordinator::dumpPackedFramebuffer")]
        for token in ("epd_fullclear", "memset(compositionBuffer_,0xFF",
                      "returnSnapshot.page=returnPage", "MODE_GC16", "epd_poweroff"):
            self.assertIn(token, manual)
        self.assertIn("snapshot_.page=previousPage", manual)
        self.assertIn("requestRender(RenderPriority::Navigation)", manual)
        self.assertIn("fullClearUsed_=true", manual)
        self.assertIn("++renderRenderedCount_", manual)

    def test_global_refresh_outranks_page_local_hit_maps(self):
        tap = MAIN[MAIN.index("} else if (action.type == input::ActionType::Tap)"):
                   MAIN.index("} else if (action.type == input::ActionType::SwipeLeft")]
        global_route = tap.index("ui::kGlobalRefreshAction.contains")
        weather_route = tap.index("displayCoordinator.page() == ui::Page::WeatherSetup")
        self.assertLess(global_route, weather_route)
        self.assertIn("WeatherSetup owns most of the canvas", tap)

    def test_settings_back_remains_live_after_visiting_refresh_mode(self):
        self.assertNotIn("detailReturnPage=ui::Page::Settings;destination=ui::Page::DisplayRefreshMode", MAIN)
        generic_back = MAIN[MAIN.index("ui::kDetailBackAction.contains"):
                            MAIN.index("ui::kHeaderBatteryAction.contains")]
        self.assertNotIn("Page::DisplayRefreshMode", generic_back)
        mode_route = MAIN[MAIN.index("displayCoordinator.page() == ui::Page::DisplayRefreshMode"):]
        self.assertIn("kRefreshModeBackAction.contains", mode_route)
        self.assertIn("destination=ui::Page::Settings", mode_route)

    def test_one_slot_navigation_capture_is_first_wins_and_exactly_once(self):
        for token in ("xTaskCreatePinnedToCore", "beginDisplayCapture", "endDisplayCapture",
                      "takeQueuedAction", "queuedActionPending_", "action_queue_hwm=1"):
            self.assertIn(token, MAIN + TOUCH + TOUCH_H)
        self.assertIn("ui::kNavBounds.contains", TOUCH)
        self.assertIn("ui::kDetailBackAction.contains", TOUCH)
        self.assertIn("if(!queuedActionPending_)", TOUCH)
        self.assertIn("else{++coalescedActionCount_;}", TOUCH)
        take = TOUCH[TOUCH.index("bool TouchController::takeQueuedAction"):TOUCH.index("void TouchController::drainStaleReports")]
        self.assertIn("queuedActionPending_=false", take)
        self.assertIn("!inputBlocked&&touchController.takeQueuedAction", MAIN)
        self.assertNotRegex(CONTROLLER_H, r"vector|deque|list<|Queue")

    def test_physical_timings_and_completion_based_quiet_gate_are_reported(self):
        for token in ("gc16_duration_ms", "full_cleanup_duration_ms",
                      "page_transition_duration_ms", "touch_to_action_software_ms"):
            self.assertIn(token, CONTROLLER)
        self.assertGreaterEqual(CONTROLLER.count("lastRefreshMs_=millis()"), 2)
        self.assertNotIn("lastRefreshMs_=nowMs", CONTROLLER)
        manual = CONTROLLER[CONTROLLER.index("bool DisplayCoordinator::manualFullRefresh"):
                            CONTROLLER.index("bool DisplayCoordinator::dumpPackedFramebuffer")]
        self.assertIn("lastGc16DurationMs_=", manual)
        self.assertIn("lastFullCleanupDurationMs_=", manual)


if __name__ == "__main__":
    unittest.main()