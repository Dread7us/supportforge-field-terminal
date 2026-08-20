import pathlib
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


def text(path):
    return (ROOT / path).read_text(encoding="utf-8")


MAIN = text("src/main.cpp")
STATE = text("src/ui/ui_state.h")
PAGES = text("src/ui/ui_pages.cpp")
CONTROLLER = text("src/ui/ui_controller.cpp")
TOUCH = text("src/input/touch_controller.cpp")
FRONT = text("src/power/front_light_manager.cpp")
FRONT_H = text("src/power/front_light_manager.h")
BOARD = text("include/board_profile.h")
BOARD_V7 = text("lib/epdiy/src/board/epd_board_v7.c")
QUALIFICATION = text("docs/HARDWARE_QUALIFICATION.md")
WIZARD = text("src/weather/weather_wizard.cpp")


class PressFeedbackFrontLightTests(unittest.TestCase):
    def test_one_central_feedback_state_and_true_black_white_render(self):
        self.assertIn("struct PressFeedback", STATE)
        self.assertEqual(STATE.count("PressFeedback pressFeedback"), 1)
        self.assertIn("void DisplayCoordinator::acceptPress", CONTROLLER)
        overlay = PAGES[PAGES.index("if(s.pressFeedback.active)"):]
        self.assertIn("roundedRect(fb,feedback,s.pressFeedback.radius,kInk,kInk)", overlay)
        self.assertIn("FontRole::CardHeading,kPaper", overlay)

    def test_enabled_control_categories_route_through_acceptance(self):
        for category in ("kNavBounds", "kSettingsCategoryActions", "kTouchRecalibrateConfirmAction",
                         "kWifiEntryKeys", "kCalculatorKeys", "kDeviceDisplayRefreshAction",
                         "kDisplayFrontLightActions", "WeatherSetup"):
            self.assertIn(category, MAIN)
        for label in ("CLEANING DISPLAY", "SAVE & CONNECT", "NEXT SECTION", "UNIT CHANGED"):
            self.assertIn(label, MAIN)
        self.assertGreaterEqual(MAIN.count("acceptPress("), 15)

    def test_disabled_and_rate_limited_paths_do_not_accept(self):
        cleanup_start = MAIN.index("ui::kDeviceDisplayRefreshAction.contains")
        cleanup_end = MAIN.index("ui::kDeviceLowPowerAction.contains", cleanup_start)
        cleanup = MAIN[cleanup_start:cleanup_end]
        self.assertLess(cleanup.index("manualRefreshAvailable"), cleanup.index("CLEANING DISPLAY"))
        rejected = cleanup[cleanup.index("syncUiState();displayCoordinator.forceDirty()"):]
        self.assertNotIn("acceptPress", rejected)
        self.assertIn("&&wifiManager.saveAndConnect()", MAIN)
        self.assertIn("&&wifiManager.snapshot().userConfigured", MAIN)
        self.assertIn("if(frontLightManager.snapshot().candidateAvailable)", MAIN)
        weather_refresh = MAIN[MAIN.index("ui::kLocationWeatherRefreshAction.contains"):
                               MAIN.index("ui::kSystemsSectionAction.contains")]
        self.assertLess(weather_refresh.index("if(weatherManager.requestRefresh(now))"),
                        weather_refresh.index("acceptPress"))
        self.assertIn("if(!manager.requestRefresh(millis()))return WizardResult::None", WIZARD)

    def test_keyboard_feedback_commits_once(self):
        wifi_start = MAIN.index("} else if(displayCoordinator.page()==ui::Page::WifiEntry){")
        wifi_end = MAIN.index("} else if(displayCoordinator.page()==ui::Page::DateTimeSettings){", wifi_start)
        wifi = MAIN[wifi_start:wifi_end]
        self.assertEqual(wifi.count("appendPassword("), 1)
        self.assertEqual(wifi.count("appendSsid("), 1)
        self.assertEqual(wifi.count("if(accepted)acceptPress"), 1)
        self.assertEqual(wifi.count("if(accepted)testDisplay()"), 1)
        wizard = MAIN[MAIN.index("if (displayCoordinator.page() == ui::Page::WeatherSetup)"):
                      MAIN.index("} else if (displayCoordinator.page() == ui::Page::TouchRecalibrateConfirm)")]
        self.assertLess(wizard.index("if(result==weather::WizardResult::None)return"),
                        wizard.index("displayCoordinator.forceDirty()"))
        self.assertLess(wizard.index("if(result==weather::WizardResult::None)return"),
                        wizard.index("acceptPress(sourcePage"))

    def test_navigation_feedback_is_destination_frame_only(self):
        route = MAIN[MAIN.index("if (displayCoordinator.requestPage(destination"):]
        self.assertLess(route.index("requestPage(destination"), route.index("acceptPress(sourcePage"))
        self.assertEqual(route[:route.index("testDisplay();")].count("testDisplay();"), 0)
        self.assertIn("destinationRoute&&s.pressFeedback.targetPage==s.page", PAGES)

    def test_expiry_never_drives_refresh_and_later_render_clears_bounds(self):
        self.assertIn("expiresAtMs = nowMs + 2500", CONTROLLER)
        self.assertNotIn("expiresAtMs", MAIN)
        self.assertNotIn("expiresAtMs", CONTROLLER[CONTROLLER.index("void DisplayCoordinator::requestRender"):])
        render = CONTROLLER[CONTROLLER.index("bool DisplayCoordinator::renderIfDirty"):
                            CONTROLLER.index("void DisplayCoordinator::printPerformance")]
        self.assertLess(render.index("memset(compositionBuffer_, 0xFF"), render.index("renderPage(compositionBuffer_"))
        self.assertIn("snapshot_.pressFeedback.active = false", render)

    def test_gc16_touch_capture_remains_one_slot(self):
        self.assertIn("if(queuedActionPending_)++coalescedActionCount_", TOUCH)
        self.assertIn("queuedAction_=action;queuedActionPending_=true", TOUCH)
        self.assertIn("!inputBlocked&&touchController.takeQueuedAction", MAIN)

    def test_front_light_ownership_levels_persistence_and_low_power(self):
        self.assertIn("frontLightPwm", BOARD)
        self.assertNotIn("44, 43, 11, true, true, true", BOARD)
        self.assertIn('preferences_.begin("sf_frontlight"', FRONT)
        for duty in ("return 50", "return 100", "return 230", "return 0"):
            self.assertIn(duty, FRONT)
        self.assertIn("candidateAvailable && pwmPin == 11", FRONT)
        self.assertIn("analogWrite(pwmPin_, 0)", FRONT)
        self.assertIn("44, 43, 11, true, true, false", BOARD)
        self.assertIn("restoreAfterInitialization(initialPower.active)", MAIN)
        self.assertIn("setLowPowerSuppressed(state.active)", MAIN)
        self.assertIn("!snapshot_.lowPowerSuppressed", FRONT)

    def test_pending_feedback_survives_service_snapshot_publication(self):
        setter = CONTROLLER[CONTROLLER.index("void DisplayCoordinator::setSnapshot"):
                            CONTROLLER.index("bool DisplayCoordinator::requestPage")]
        self.assertIn("const PressFeedback retainedFeedback", setter)
        self.assertIn("if (retainedFeedback.active) snapshot_.pressFeedback = retainedFeedback", setter)

    def test_front_light_settings_route_and_unavailable_truth(self):
        self.assertIn("Page::DisplaySettings", MAIN + STATE + PAGES)
        self.assertIn("FRONT LIGHT NOT QUALIFIED", PAGES)
        self.assertIn("GPIO 11 conflicts with EPD D10", PAGES)
        self.assertIn("No alternative GPIO will be attempted", PAGES)
        self.assertIn("disabled pending installed-PCB revision proof", BOARD)
        for level in ("Off", "Low", "Medium", "High"):
            self.assertIn(f"FrontLightLevel::{level}", FRONT_H + FRONT + MAIN + PAGES)

    def test_unproven_installed_revision_keeps_gpio11_disabled(self):
        self.assertIn("44, 43, 11, true, true, false", BOARD)
        self.assertIn("#define D10 GPIO_NUM_11", BOARD_V7)
        self.assertIn("front-light capability remains disabled", QUALIFICATION)
        self.assertIn("No\nfront-light claim, write, or probe is introduced", QUALIFICATION)
        self.assertIn("no executable LOW", QUALIFICATION)
        self.assertIn("qualification path is prepared", QUALIFICATION)
        self.assertIn("installed PCB model and revision", QUALIFICATION)
        self.assertIn("Do not\n   expose or test MEDIUM/HIGH", QUALIFICATION)


if __name__ == "__main__":
    unittest.main()