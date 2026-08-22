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
        self.assertIn("RefreshMode::QuickNavigation", CONTROLLER_H)
        self.assertIn('preferences_.begin("sf_display"', CONTROLLER)
        self.assertIn('getUChar("refresh_mode"', CONTROLLER)
        self.assertIn('putUChar("refresh_mode"', CONTROLLER)
        self.assertIn("requestRender(RenderPriority::Cosmetic)", CONTROLLER)
        self.assertIn("applied=IMMEDIATE", CONTROLLER)

    def test_render_latch_retains_highest_priority_and_coalesces_duplicates(self):
        block = CONTROLLER[CONTROLLER.index("void DisplayCoordinator::requestRender"):
                           CONTROLLER.index("void DisplayCoordinator::setSnapshot")]
        self.assertIn("static_cast<uint8_t>(priority) > static_cast<uint8_t>(pendingRender_)", block)
        self.assertIn("pendingRender_ = priority", block)
        self.assertIn("else ++renderCoalescedCount_", block)
        self.assertLess(block.index("pendingRender_ = priority"), block.index("else ++renderCoalescedCount_"))

    def test_exact_cleanup_policy_separates_boot_fault_and_mode_navigation(self):
        render = CONTROLLER[CONTROLLER.index("bool DisplayCoordinator::renderIfDirty"):
                            CONTROLLER.index("void DisplayCoordinator::printPerformance")]
        self.assertIn("pendingRender_ == RenderPriority::None", render)
        policy = render[render.index("const bool firstUsableFrame"):render.index("bool displayPowered")]
        self.assertIn("!hasPresentedPage_", policy)
        self.assertIn("renderingPriority == RenderPriority::Navigation", policy)
        self.assertIn("hasPresentedPage_ && lastPresentedPage_ != snapshot_.page", policy)
        self.assertIn("bootRecovery && firstUsableFrame && !fullClearUsed_", policy)
        self.assertIn("faultRecoveryCleanup", policy)
        self.assertIn("modeNavigationCleanup", policy)
        self.assertIn("RefreshMode::Balanced", policy)
        self.assertIn("RefreshMode::BeautifulClean", policy)
        self.assertNotIn("RefreshMode::QuickNavigation ||", policy)
        self.assertLess(render.index("memset(compositionBuffer_, 0xFF"), render.index("renderPage(compositionBuffer_"))
        cleanup = render[render.index("if (cleanupThisRender)"):
                         render.index("memset(compositionBuffer_, 0xFF")]
        self.assertEqual(cleanup.count("epd_fullclear"), 1)
        self.assertNotIn("epd_hl_update_screen", cleanup)
        content_start = render.index("memcpy(framebuffer, compositionBuffer_")
        content_update = render[content_start:render.index("epd_poweroff()", content_start)]
        self.assertEqual(content_update.count("epd_hl_update_screen"), 1)
        self.assertIn("MODE_GC16", content_update)

    def test_every_boot_physically_cleans_before_first_usable_frame(self):
        begin = CONTROLLER[CONTROLLER.index("bool DisplayCoordinator::begin"):
                           CONTROLLER.index("bool DisplayCoordinator::setRefreshMode")]
        self.assertIn("stored <= static_cast<uint8_t>(RefreshMode::BeautifulClean)", begin)
        setup = MAIN[MAIN.index("void setup()") : MAIN.index("void loop()")]
        self.assertLess(setup.index("syncUiState()"), setup.index("testDisplay()"))
        self.assertIn("cleanup completes", setup)
        render = CONTROLLER[CONTROLLER.index("bool DisplayCoordinator::renderIfDirty"):
                            CONTROLLER.index("void DisplayCoordinator::printPerformance")]
        self.assertIn("bootRecovery && firstUsableFrame && !fullClearUsed_", render)
        self.assertLess(render.index("epd_fullclear"), render.index("renderPage(compositionBuffer_"))
        self.assertLess(render.index("epd_fullclear"), render.index("epd_hl_update_screen"))
        self.assertIn("bootCleanupPending = true", MAIN)
        self.assertIn("bootCleanupPending && !displayCoordinator.cleanupUsed()", MAIN)

    def test_cosmetic_sources_can_only_request_content_gc16_without_cleanup(self):
        material = STATE[STATE.index("bool materiallyDifferent"):]
        for token in ("a.hour != b.hour", "a.minute != b.minute", "a.telemetry.fetchState",
                      "a.batteryVisual.state", "a.batteryChargerConnection", "a.pressFeedback.active",
                      "a.wifi.version", "strcmp(a.wifiEntrySsid", "a.calculatorDisplay"):
            self.assertIn(token, material)
        snapshot = CONTROLLER[CONTROLLER.index("void DisplayCoordinator::setSnapshot"):
                              CONTROLLER.index("bool DisplayCoordinator::requestPage")]
        self.assertIn("requestRender(RenderPriority::Cosmetic)", snapshot)
        policy = CONTROLLER[CONTROLLER.index("const bool firstUsableFrame"):
                            CONTROLLER.index("bool displayPowered")]
        executable_policy = "\n".join(line for line in policy.splitlines()
                                      if not line.lstrip().startswith("//"))
        for forbidden in ("Cosmetic", "pressFeedback", "telemetry", "battery", "wifi", "minute"):
            self.assertNotIn(forbidden, executable_policy)

    def test_cleanup_has_bounded_balanced_cooldown_but_no_queue_and_never_blocks_navigation(self):
        self.assertIn("kBalancedCleanupCooldownMs", CONTROLLER)
        self.assertIn("balancedCleanupAvailable", CONTROLLER)
        for retired in ("pendingCleanup", "cleanupQueue", "queuedCleanup", "navigationWash"):
            self.assertNotIn(retired, CONTROLLER + CONTROLLER_H)
        request_page = CONTROLLER[CONTROLLER.index("bool DisplayCoordinator::requestPage"):
                                  CONTROLLER.index("bool DisplayCoordinator::inputBlocked")]
        self.assertNotIn("cleanup", request_page.lower())
        self.assertIn("requestRender(RenderPriority::Navigation)", request_page)
        self.assertNotRegex(CONTROLLER_H, r"pendingCleanup|cleanupQueue|queuedCleanup")

    def test_rapid_navigation_cannot_chain_or_queue_full_clears(self):
        latch = CONTROLLER[CONTROLLER.index("void DisplayCoordinator::requestRender"):
                           CONTROLLER.index("void DisplayCoordinator::setSnapshot")]
        self.assertIn("pendingRender_", latch)
        self.assertNotRegex(latch, r"queue|deque|vector")
        render = CONTROLLER[CONTROLLER.index("bool DisplayCoordinator::renderIfDirty"):
                            CONTROLLER.index("void DisplayCoordinator::printPerformance")]
        self.assertEqual(render.count("epd_fullclear"), 1)
        self.assertIn("noteCleanupStarted()", render)
        self.assertLess(render.index("noteCleanupStarted()"), render.index("epd_fullclear"))
        self.assertIn("!failedUpdateRetryUsed_", render)
        self.assertIn("failedUpdateNeedsRecoveryCleanup_ = !cleanupThisRender", render)

    def test_quick_navigation_is_exactly_one_gc16_and_zero_automatic_fullclears(self):
        render = CONTROLLER[CONTROLLER.index("bool DisplayCoordinator::renderIfDirty"):
                            CONTROLLER.index("void DisplayCoordinator::printPerformance")]
        policy = render[render.index("const bool bootCleanup"):render.index("bool displayPowered")]
        self.assertIn("modeNavigationCleanup", policy)
        self.assertIn("RefreshMode::BeautifulClean", policy)
        self.assertIn("RefreshMode::Balanced", policy)
        self.assertNotRegex(policy, r"QuickNavigation\s*==|==\s*RefreshMode::QuickNavigation")
        self.assertEqual(render.count("epd_hl_update_screen"), 1)
        self.assertIn("MODE_GC16", render)
        self.assertLess(render.index("memset(compositionBuffer_, 0xFF"),
                        render.index("renderPage(compositionBuffer_"))
        self.assertLess(render.index("memcpy(framebuffer, compositionBuffer_"),
                        render.index("epd_hl_update_screen"))
        self.assertEqual(render.count("epd_poweroff()"), 3)  # two abort guards + physical update

    def test_visible_mode_tradeoffs_match_runtime_policy(self):
        for phrase in ("no navigation cleanup", "significant cleanup has cooldown",
                       "One cleanup before each page replacement",
                       "repeated navigation cannot chain", "in-page updates stay fast"):
            self.assertIn(phrase, PAGES)
        self.assertIn("FAST: FULL GC16; NO NAVIGATION CLEANUP", STATE)
        self.assertIn("FULL GC16; BOUNDED LAYOUT CLEANUP", STATE)

    def test_mode_page_diagnostics_and_touch_geometry_are_explicit(self):
        for label in ("QUICK", "BALANCED", "BEAUTIFUL", "SELECTED",
                      "DISPLAY MODE", "DISPLAY TIMING", "GC16"):
            self.assertIn(label, PAGES + STATE)
        for action in ("kSettingsRefreshModeAction", "kRefreshModeActions",
                       "kRefreshModeBackAction", "kSettingsLowPowerAction",
                       "kSettingsSyncActionCompact", "kSettingsTouchAction"):
            self.assertIn(action, MAIN + PAGES + THEME)
        self.assertIn("refresh mode choices and Back must be large and non-overlapping", THEME)
        self.assertIn('actionButton(fb,kRefreshModeBackAction,"BACK",false,Icon::ChevronLeft,true)', PAGES)

    def test_device_cleanup_is_immediate_rate_limited_and_state_safe(self):
        self.assertIn("kDeviceDisplayRefreshAction", MAIN + THEME)
        self.assertNotIn("kGlobalRefreshAction", MAIN + THEME)
        self.assertNotIn("kFooterRefreshBounds", THEME)
        self.assertIn("bottomNavigation(fb,s.page)", PAGES)
        self.assertIn("kManualRefreshLimitMs = 45000", CONTROLLER)
        self.assertNotIn("DisplayRefreshConfirm", STATE + MAIN + PAGES)
        self.assertNotIn("route=CONFIRMATION", MAIN)
        self.assertIn("target=CLEAN_DISPLAY immediate=YES", MAIN)
        self.assertIn("displayCoordinator.manualFullRefresh(now,ui::Page::Device)", MAIN)
        manual = CONTROLLER[CONTROLLER.index("bool DisplayCoordinator::manualFullRefresh"):
                            CONTROLLER.index("bool DisplayCoordinator::dumpPackedFramebuffer")]
        for token in ("epd_fullclear", "memset(compositionBuffer_,0xFF",
                      "returnSnapshot.page=returnPage", "returnSnapshot.manualRefreshRateLimited=true",
                      "returnSnapshot.manualRefreshRemainingSeconds=kManualRefreshLimitMs/1000",
                      "MODE_GC16", "epd_poweroff"):
            self.assertIn(token, manual)
        self.assertIn("snapshot_.page=previousPage", manual)
        self.assertIn("requestRender(RenderPriority::Navigation)", manual)
        self.assertIn("pendingRender_=RenderPriority::None", manual)
        self.assertLess(manual.index("pendingRender_=RenderPriority::None"),
                        manual.index("epd_fullclear"))
        self.assertIn("fullClearUsed_=true", manual)
        self.assertIn("++renderRenderedCount_", manual)

    def test_cleanup_is_device_local_and_other_hit_maps_remain_intact(self):
        tap = MAIN[MAIN.index("} else if (action.type == input::ActionType::Tap)"):
                   MAIN.index("} else if (action.type == input::ActionType::SwipeLeft")]
        self.assertNotIn("kGlobalRefreshAction", tap)
        self.assertIn("displayCoordinator.page() == ui::Page::WeatherSetup", tap)
        self.assertIn("displayCoordinator.page() == ui::Page::Device", tap)
        self.assertIn("ui::kDeviceDisplayRefreshAction.contains", tap)

    def test_swipes_are_primary_only_without_skipping_service_polling(self):
        swipe = MAIN[MAIN.index("} else if (action.type == input::ActionType::SwipeLeft"):
                     MAIN.index("// Touch routing always gets the first opportunity")]
        self.assertIn("int index=-1", swipe)
        self.assertIn("if(index>=0)", swipe)
        self.assertNotRegex(swipe, r"if\s*\(index\s*<\s*0\)\s*return")
        for page in ("Home", "Systems", "Radio", "Location", "Device"):
            self.assertIn(f"ui::Page::{page}", swipe)

    def test_settings_back_remains_live_after_visiting_refresh_mode(self):
        self.assertNotIn("detailReturnPage=ui::Page::Settings;destination=ui::Page::DisplayRefreshMode", MAIN)
        generic_back = MAIN[MAIN.index("ui::kDetailBackAction.contains"):
                            MAIN.index("ui::kHeaderBatteryAction.contains")]
        self.assertNotIn("Page::DisplayRefreshMode", generic_back)
        mode_route = MAIN[MAIN.index("displayCoordinator.page() == ui::Page::DisplayRefreshMode"):]
        self.assertIn("kRefreshModeBackAction.contains", mode_route)
        self.assertIn("destination=ui::Page::DisplaySettings", mode_route)

    def test_visible_display_settings_route_reaches_all_modes_from_beautiful(self):
        self.assertIn('"DISPLAY"', PAGES)
        self.assertIn("destination=ui::Page::DisplaySettings", MAIN)
        self.assertIn("kDisplayRefreshSettingsAction", MAIN + PAGES + THEME)
        mode_route = MAIN[MAIN.index("displayCoordinator.page() == ui::Page::DisplayRefreshMode"):]
        self.assertIn("for(uint8_t i=0;i<3;++i)", mode_route)
        self.assertIn("setRefreshMode(static_cast<ui::RefreshMode>(i))", mode_route)
        self.assertIn("selected?String(titles[i])+\" - SELECTED\"", PAGES)

    def test_one_slot_action_capture_is_latest_wins_and_exactly_once(self):
        for token in ("xTaskCreatePinnedToCore", "beginDisplayCapture", "endDisplayCapture",
                      "takeQueuedAction", "queuedActionPending_", "action_queue_hwm=1"):
            self.assertIn(token, MAIN + TOUCH + TOUCH_H)
        capture = TOUCH[TOUCH.index("void TouchController::captureTask()"):
                        TOUCH.index("bool TouchController::beginDisplayCapture")]
        for action in ("ActionType::Tap", "ActionType::SwipeLeft", "ActionType::SwipeRight"):
            self.assertIn(action, capture)
        self.assertIn("if(queuedActionPending_)++coalescedActionCount_", capture)
        self.assertIn("queuedAction_=action;queuedActionPending_=true", capture)
        self.assertNotIn("if(!queuedActionPending_)", capture)
        take = TOUCH[TOUCH.index("bool TouchController::takeQueuedAction"):TOUCH.index("void TouchController::drainStaleReports")]
        self.assertIn("queuedActionPending_=false", take)
        self.assertIn("!inputBlocked&&touchController.takeQueuedAction", MAIN)
        self.assertNotRegex(CONTROLLER_H, r"vector|deque|list<|Queue")

    def test_capture_shutdown_is_nonblocking_and_cleanup_limit_starts_on_real_work(self):
        end = TOUCH[TOUCH.index("void TouchController::endDisplayCapture"):
                    TOUCH.index("bool TouchController::takeQueuedAction")]
        self.assertNotRegex(end, r"while\s*\(|delay\s*\(")
        self.assertIn("displayCaptureFinalizePending_=true", end)
        self.assertIn("displayCaptureSettled()", TOUCH_H)
        self.assertIn("portMUX_TYPE captureMux_", TOUCH_H)
        self.assertNotIn("volatile bool displayCapture", TOUCH_H)
        for lifecycle in ("beginDisplayCapture", "endDisplayCapture", "displayCaptureSettled"):
            block = TOUCH[TOUCH.index(f"TouchController::{lifecycle}"):]
            self.assertIn("portENTER_CRITICAL(&captureMux_)", block[:700])
            self.assertIn("portEXIT_CRITICAL(&captureMux_)", block[:700])
        self.assertIn("captureSettled=touchController.displayCaptureSettled()", MAIN)
        self.assertIn("if(captureSettled&&!queuedAction)action=touchController.poll", MAIN)
        capture = TOUCH[TOUCH.index("void TouchController::captureTask()"):
                        TOUCH.index("bool TouchController::beginDisplayCapture")]
        self.assertIn("notifyDisplayUpdateFinished(millis())", capture)
        manual = CONTROLLER[CONTROLLER.index("bool DisplayCoordinator::manualFullRefresh"):
                            CONTROLLER.index("bool DisplayCoordinator::dumpPackedFramebuffer")]
        self.assertLess(manual.index("epd_poweron()"), manual.index("lastManualRefreshMs_=millis()"))
        self.assertLess(manual.index("lastManualRefreshMs_=millis()"), manual.index("epd_fullclear"))

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