import json
import re
import unittest
from pathlib import Path


ROOT = Path(__file__).parents[1]


def text(path):
    return (ROOT / path).read_text()


MODEL_H = text("src/battery/battery_visual_model.h")
MODEL = text("src/battery/battery_visual_model.cpp")
MANAGER = text("src/battery/battery_manager.cpp")
STATE_H = text("src/ui/ui_state.h")
MAIN = text("src/main.cpp")
COMPONENTS = text("src/ui/ui_components.cpp")
PAGES = text("src/ui/ui_pages.cpp")
SPEC = json.loads(text("ui/ui_spec.json"))
METRICS = json.loads(text("tools/ui_font_metrics.json"))


class BatteryVisualModelTests(unittest.TestCase):
    HEADER = SPEC["geometry"]["header_battery_bounds"]
    LEGACY_120_GLYPH = (HEADER[0] + 8, HEADER[1] + 10, 120, 32)
    LEGACY_96_GLYPH = (HEADER[0] + 12, HEADER[1] + (HEADER[3] - 28) // 2, 96, 28)
    GLYPH = (HEADER[0] + HEADER[2] - 72, HEADER[1] + (HEADER[3] - 25) // 2, 64, 25)

    @staticmethod
    def geometry(charging):
        x, y, width, height = BatteryVisualModelTests.GLYPH
        terminal_width = max(3, min(6, width // 12))
        body = (x, y, width - terminal_width - 1, height)
        inset = 4 if height >= 48 else 3
        interior = (body[0] + inset, body[1] + inset,
                    body[2] - 2 * inset, body[3] - 2 * inset)
        terminal = (body[0] + body[2] + 1, body[1] + body[3] // 3,
                    terminal_width, max(3, body[3] // 3))
        charging_width = 7 if charging else 0
        charging_lane = (interior[0] + interior[2] - charging_width,
                         interior[1] + interior[3] - 8, charging_width, 8)
        label = (interior[0], interior[1], interior[2] - charging_width, interior[3])
        return body, interior, label, charging_lane, terminal

    def test_one_model_is_the_only_selected_percentage_projection(self):
        self.assertIn("struct BatteryVisualModel", MODEL_H)
        self.assertIn("makeBatteryVisualModel(const Snapshot& snapshot)", MODEL_H + MODEL)
        self.assertIn("snapshot.displaySource != DisplaySource::Unavailable", MODEL)
        self.assertIn("model.percent = model.percentAvailable ? snapshot.percent : 0", MODEL)
        self.assertNotRegex(MODEL, r"remainingCapacity|fullChargeCapacity|capacityRatioPercent")
        self.assertIn("battery::BatteryVisualModel batteryVisual{}", STATE_H)
        self.assertIn("state.batteryVisual = battery::makeBatteryVisualModel(battery)", MAIN)
        for surface in (
            "batteryIcon(fb, batteryGlyph, state.batteryVisual, kInk)",
            "batteryIcon(fb,{90,294,360,124},s.batteryVisual,kInk)",
            "s.batteryVisual.percentAvailable?String(s.batteryVisual.percent)+\"%\"",
        ):
            self.assertIn(surface, COMPONENTS + PAGES)
        for obsolete in ("batteryPercentAvailable", "batteryDisplaySource", "batteryState"):
            self.assertNotIn(obsolete, STATE_H)

    def test_exact_header_geometry_terminal_and_right_edge_safety(self):
        self.assertEqual(self.LEGACY_120_GLYPH, (404, 16, 120, 32))
        self.assertEqual(self.LEGACY_96_GLYPH, (408, 18, 96, 28))
        self.assertEqual(self.GLYPH, (460, 19, 64, 25))
        self.assertLess(self.GLYPH[2], self.LEGACY_96_GLYPH[2])
        self.assertLess(self.GLYPH[3], self.LEGACY_96_GLYPH[3])
        body, interior, label, charging, terminal = self.geometry(False)
        self.assertEqual(body, (460, 19, 58, 25))
        self.assertEqual(interior, (463, 22, 52, 19))
        self.assertEqual(terminal, (519, 27, 5, 8))
        self.assertEqual(terminal[0], body[0] + body[2] + 1)
        self.assertLessEqual(terminal[0] + terminal[2], self.HEADER[0] + self.HEADER[2])
        self.assertLessEqual(self.HEADER[0] + self.HEADER[2], SPEC["canvas"]["width"])
        self.assertEqual(label, interior)
        self.assertEqual(charging[2], 0)

    def test_fill_mapping_for_all_required_values_and_unavailable(self):
        interior_width = self.geometry(False)[1][2]
        values = (0, 1, 8, 10, 59, 80, 95, 99, 100)
        widths = [(interior_width * value + 50) // 100 for value in values]
        self.assertEqual(widths, [0, 1, 4, 5, 31, 42, 49, 51, 52])
        self.assertEqual(widths[0], 0)
        self.assertEqual(widths[-1], interior_width)
        self.assertEqual(0, 0)  # unavailable is explicitly mapped to no fill
        self.assertIn("if (!model.percentAvailable", COMPONENTS)
        self.assertIn("return 0", COMPONENTS[COMPONENTS.index("int batteryFillWidth"):])

    def test_every_required_percentage_and_unknown_fit_measured_caption_lane(self):
        advances = METRICS["roles"]["Caption"]["advances"]
        width = lambda value: sum(advances[ord(char) - 32] for char in value)
        normal_label = self.geometry(False)[2]
        charging_label = self.geometry(True)[2]
        for value in (0, 1, 8, 10, 59, 80, 95, 99, 100):
            label = f"{value}%"
            self.assertLessEqual(width(label), normal_label[2], label)
            self.assertLessEqual(width(label), charging_label[2], f"charging {label}")
        self.assertLessEqual(width("--"), normal_label[2])
        self.assertLessEqual(METRICS["roles"]["Caption"]["line_height"], normal_label[3])
        self.assertIn("const String label = String(bounded) + \"%\"", COMPONENTS)
        self.assertIn("textWidth(label, FontRole::Caption)", COMPONENTS)

    def test_split_contrast_and_corner_charging_mark_never_obscure_percentage(self):
        icon = COMPONENTS[COMPONENTS.index("void batteryIcon("):
                          COMPONENTS.index("void circle(")]
        self.assertIn("text(fb, filledLabel", icon)
        self.assertIn("FontRole::Caption, kPaper", icon)
        self.assertIn("text(fb, unfilledLabel", icon)
        self.assertIn("FontRole::Caption, kInk", icon)
        _, interior, label, charging, _ = self.geometry(True)
        self.assertEqual(label[0] + label[2], charging[0])
        self.assertEqual(charging[0] + charging[2], interior[0] + interior[2])
        self.assertEqual(charging[1] + charging[3], interior[1] + interior[3])
        self.assertEqual(charging[2:], (7, 8))
        self.assertGreater(charging[2], 0)
        self.assertIn("Small static corner bolt", icon)
        self.assertIn("if (model.charging)", icon)
        self.assertNotIn("State::Verifying", icon)

    def test_header_state_words_cannot_change_bounds_and_unknown_is_stable(self):
        app = COMPONENTS[COMPONENTS.index("void appBar("):
                         COMPONENTS.index("void card(")]
        for forbidden in ("batteryStateLabel", '"VERIFY"', '"LIVE"', '"LKG"',
                          '"STALE"', '"FULL"'):
            self.assertNotIn(forbidden, app)
        self.assertEqual(app.count("const Rect batteryGlyph"), 1)
        self.assertEqual(app.count("batteryIcon(fb, batteryGlyph"), 1)
        icon = COMPONENTS[COMPONENTS.index("void batteryIcon("):
                          COMPONENTS.index("void circle(")]
        self.assertIn('const String unknown = "--"', icon)
        self.assertNotRegex(icon, r'"\.\.\."|"/"')

    def test_full_bounds_are_blanked_before_every_icon_transition(self):
        icon = COMPONENTS[COMPONENTS.index("void batteryIcon("):
                          COMPONENTS.index("void circle(")]
        self.assertLess(icon.index("blankRegion(fb, bounds)"),
                        icon.index("batteryIconGeometry(bounds, model.charging)"))
        self.assertIn("100% -> 8%", icon)
        self.assertIn("charging -> idle", icon)
        self.assertIn("valid -> --", icon)
        app = COMPONENTS[COMPONENTS.index("void appBar("):
                         COMPONENTS.index("void card(")]
        self.assertLess(app.index("blankRegion(fb, batteryClip)"),
                        app.index("batteryIcon(fb, batteryGlyph"))
        self.assertIn("blankRegion(fb, legacy120BatteryGlyph)", app)
        self.assertIn("blankRegion(fb, legacy96BatteryGlyph)", app)
        self.assertIn("blankRegion(fb, batteryGlyph)", app)
        self.assertLess(app.index("blankRegion(fb, legacy120BatteryGlyph)"),
                        app.index("batteryIcon(fb, batteryGlyph"))
        self.assertLess(app.index("blankRegion(fb, legacy96BatteryGlyph)"),
                        app.index("batteryIcon(fb, batteryGlyph"))

    def test_capacity_commands_and_all_battery_access_remain_read_only(self):
        self.assertIn("kRemainingCapacityRegister = 0x10", MANAGER)
        self.assertIn("kFullChargeCapacityRegister = 0x12", MANAGER)
        self.assertIn("kDesignCapacityRegister = 0x3C", MANAGER)
        self.assertNotIn("kRemainingCapacityRegister = 0x0C", MANAGER)
        read = MANAGER[MANAGER.index("bool readRegister("):
                       MANAGER.index("bool readGaugeWord(")]
        self.assertEqual(read.count("Wire.write(reg)"), 1)
        self.assertIn("Wire.endTransmission(false)", read)
        self.assertEqual(MANAGER.count("Wire.write("), 1)
        self.assertNotRegex(MANAGER.lower(),
                            r"\b(unseal|seal|reset|calibrat|data.?memory|control.?subcommand)\s*\(")


if __name__ == "__main__":
    unittest.main()