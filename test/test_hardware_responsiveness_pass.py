import json
import re
import unittest
from pathlib import Path


ROOT = Path(__file__).parents[1]


def text(path):
    return (ROOT / path).read_text(encoding="utf-8")


MAIN = text("src/main.cpp")
TOUCH = text("src/input/touch_controller.cpp")
TOUCH_H = text("src/input/touch_controller.h")
STATE = text("src/ui/ui_state.cpp")
PAGES = text("src/ui/ui_pages.cpp")
THEME = text("src/ui/ui_theme.h")
CONTROLLER = text("src/ui/ui_controller.cpp")
CONTROLLER_H = text("src/ui/ui_controller.h")
BATTERY = text("src/battery/battery_manager.cpp")
COMPONENTS = text("src/ui/ui_components.cpp")
SPEC = json.loads(text("ui/ui_spec.json"))
BOARD_V7 = text("lib/epdiy/src/board/epd_board_v7.c")


class HardwareResponsivenessPassTests(unittest.TestCase):
    def test_diagnostics_environment_inherits_candidate_flags(self):
        platformio = text("platformio.ini")
        self.assertIn("[env:h752_02_diagnostics]", platformio)
        self.assertIn("${env:h752_02_candidate.build_flags}", platformio)
        self.assertIn("-DSUPPORTFORGE_PERF_DIAGNOSTICS=1", platformio)
        self.assertIn('PERF diagnostics=%s', MAIN)

    def test_touch_affine_mapping_is_bounded_and_persisted_after_four_samples(self):
        for token in ("applyCalibration", "calculateCalibrationBounds",
                      'putInt("min_x"', 'putInt("max_x"', 'putInt("min_y"',
                      'putInt("max_y"', 'putBool("bounds"'):
            self.assertIn(token, TOUCH + TOUCH_H)
        final = TOUCH[TOUCH.index("if(qualificationStep_==4)"):]
        self.assertLess(final.index("selectPhysicalTransform"),
                        final.index("calculateCalibrationBounds"))
        self.assertLess(final.index("calculateCalibrationBounds"),
                        final.index("saveQualification(true)"))
        self.assertIn("constrain(x,0,ui::kCanvasWidth-1)", TOUCH)
        self.assertIn("constrain(y,0,ui::kCanvasHeight-1)", TOUCH)

    def test_recalibration_requires_confirmation_and_boot_preserves_saved_data(self):
        self.assertIn("Page::TouchRecalibrateConfirm", MAIN + PAGES + STATE)
        self.assertIn("kSettingsTouchAction", MAIN + THEME)
        self.assertIn("RESET TOUCH CALIBRATION", PAGES)
        confirm = MAIN[MAIN.index("Page::TouchRecalibrateConfirm"):
                       MAIN.index("kDetailBackAction.contains")]
        self.assertIn("kTouchRecalibrateConfirmAction.contains", confirm)
        self.assertIn("beginConfirmedTouchRecalibration()", confirm)
        setup = MAIN[MAIN.index("void setup()"):MAIN.index("void loop()")]
        self.assertNotIn("resetQualification", setup)
        for field in ("touchMappingVerified", "touchSetupStep", "touchSetupReady"):
            self.assertIn(f"a.{field} != b.{field}", STATE)

    def test_stale_reports_and_duplicate_actions_are_bounded(self):
        self.assertIn("kMaximumDrainReports=4", TOUCH)
        self.assertIn("for(uint8_t i=0;i<kMaximumDrainReports;++i)", TOUCH)
        self.assertIn("drainStaleReports();", TOUCH)
        self.assertIn("armed_=false", TOUCH[TOUCH.index("if(!pressed&&down_)"):])
        self.assertNotRegex(TOUCH, r"while\s*\([^)]*Wire")
        self.assertLessEqual(SPEC["touch"]["clean_release_ms"], 200)
        self.assertLessEqual(SPEC["touch"]["post_refresh_quiet_ms"], 200)
        capture = TOUCH[TOUCH.index("void TouchController::captureTask()"):
                        TOUCH.index("bool TouchController::beginDisplayCapture")]
        self.assertIn("queuedAction_=action", capture)
        self.assertIn("queuedActionPending_=true", capture)
        self.assertNotRegex(TOUCH_H, r"vector|deque|list<|xQueue")

    def test_touch_routes_before_services_polling_and_cosmetic_rendering(self):
        loop = MAIN[MAIN.index("void loop()") :]
        route = loop.index("action.type == input::ActionType::Tap")
        for later in ("gpsManager.poll(now)", "wifiManager.poll(now",
                      "batteryManager.poll(now", "displayCoordinator.dirty()"):
            self.assertLess(route, loop.index(later), later)

    def test_age_countdowns_do_not_dirty_but_material_telemetry_does(self):
        state = STATE
        self.assertNotIn("nextPollSeconds", state)
        self.assertNotIn("manualRefreshRemainingSeconds", state)
        for material in ("telemetry.fetchState", "telemetry.systemStatus",
                         "telemetry.cpuLoad", "telemetry.ramPercent"):
            self.assertIn(material, state)
        self.assertIn("materiallyDifferent(snapshot_, snapshot)", CONTROLLER)
        self.assertIn("requestRender(RenderPriority::Cosmetic)", CONTROLLER)

    def test_performance_diagnostics_are_compile_time_gated_and_bounded(self):
        self.assertIn("#define SUPPORTFORGE_PERF_DIAGNOSTICS 0", MAIN)
        self.assertIn("#if SUPPORTFORGE_PERF_DIAGNOSTICS", MAIN)
        for token in ("touch_received", "touch_accepted", "touch_debounced",
                      "touch_dropped", "press_to_action_ms", "action_queue_hwm=1",
                      "render_requested", "render_rendered", "render_coalesced",
                      "render_wait_ms", "gc16_duration_ms", "navigation_latency_ms",
                      "heap_free", "psram_free", "guardian_heartbeat",
                      "weather_heartbeat", "reset_classification"):
            self.assertIn(token, MAIN + TOUCH + CONTROLLER)
        self.assertNotRegex(CONTROLLER_H, r"vector|deque|list<|Queue")

    def test_unknown_battery_stays_truthful_but_ui_is_complete(self):
        self.assertIn("kNormalSampleIntervalMs = 90UL * 1000UL", BATTERY)
        self.assertIn("kChargingSampleIntervalMs = 45UL * 1000UL", BATTERY)
        self.assertIn("kMaximumFreshAgeMs", BATTERY)
        self.assertIn("State::Stale", BATTERY)
        self.assertIn("percentAvailable = false", BATTERY)
        self.assertIn("kGaugeAddress = 0x55", BATTERY)
        self.assertIn("kStateOfChargeRegister = 0x2C", BATTERY)
        self.assertIn("kEvidenceSampleCount = 3", BATTERY)
        self.assertIn("EvidenceFrame frames[kEvidenceSampleCount]", BATTERY)
        self.assertIn("stableWords(frames, &EvidenceFrame::soc", BATTERY)
        self.assertIn("snapshot_.displaySource = DisplaySource::Unavailable", BATTERY)
        self.assertIn("kChargerAddress = 0x6B", BATTERY)
        self.assertIn("kChargerStatusRegister = 0x0B", BATTERY)
        self.assertIn("kChargeStatusMask = 0x18", BATTERY)
        self.assertNotRegex(BATTERY, r"Wire\.write\([^r]|seal|reset|calibr|data memory")
        icon = COMPONENTS[COMPONENTS.index("void batteryIcon("):
                          COMPONENTS.index("void circle(")]
        self.assertIn('const String unknown = "--"', icon)
        self.assertIn("epd_fill_rect({body.x, body.y, body.w, body.h}, kPaper", icon)
        self.assertIn("text(fb, filledLabel, labelX", icon)
        self.assertIn("text(fb, unfilledLabel, labelX", icon)
        self.assertIn("if (model.charging)", icon)
        self.assertIn("void batteryDetail", PAGES)
        self.assertIn('return "LKG"', PAGES)
        self.assertIn('"DISPLAYED SOURCE",battery::displaySourceName', PAGES)
        self.assertIn('"RAW SOC / RATIO",batteryRawSoc(s)+" / "+batteryCapacityRatio(s)', PAGES)

    def test_unverified_physical_controls_are_not_exposed(self):
        # Retained current-Pro EPD source assigns IO48 to CKV, so it cannot be
        # treated as an application button in this candidate profile.
        self.assertIn("#define CKV GPIO_NUM_48", BOARD_V7)
        self.assertRegex(BOARD_V7, r"\.ckv\s*=\s*CKV")
        for forbidden in ("STATUS LIGHT ON", "SOUND ALERTS ON", "POWER OPTIONS",
                          "POWER OFF"):
            self.assertNotIn(forbidden, MAIN + PAGES)
        self.assertNotRegex(MAIN, r"pinMode\s*\(\s*48\b|digitalRead\s*\(\s*48\b")

    def test_display_invariants_and_single_owner_remain_intact(self):
        self.assertIn("EPD_ROT_INVERTED_PORTRAIT", CONTROLLER)
        self.assertIn("epd_set_vcom(1560)", CONTROLLER)
        self.assertIn("MODE_GC16", CONTROLLER)
        self.assertNotIn("MODE_DU", CONTROLLER)
        self.assertIn("epd_poweroff", CONTROLLER)
        physical = re.compile(
            r"epd_(?:init|set_vcom|set_rotation|poweron|poweroff|fullclear|hl_update_screen)"
        )
        owners = [path.relative_to(ROOT).as_posix() for path in (ROOT / "src").rglob("*.cpp")
                  if physical.search(path.read_text(encoding="utf-8"))]
        self.assertEqual(owners, ["src/ui/ui_controller.cpp"])


if __name__ == "__main__":
    unittest.main()