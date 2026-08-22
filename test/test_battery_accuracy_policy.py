import unittest
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).parents[1]
BATTERY = (ROOT / "src/battery/battery_manager.cpp").read_text()
PAGES = (ROOT / "src/ui/ui_pages.cpp").read_text()
DOCUMENTED_CAPACITY_MAH = 1500


@dataclass
class Evidence:
    soc: int | None
    remaining: int | None
    full_capacity: int | None
    design_capacity: int | None
    voltage_mv: int | None
    current_ma: int | None
    input_connected: bool | None
    charge_complete: bool | None
    fresh: bool = True


def decide(e: Evidence):
    """Reference truth table matching the firmware's explicit evidence policy."""
    if not e.fresh or e.soc is None or not 0 <= e.soc <= 100:
        return None, "UNAVAILABLE", "STALE"
    capacity_observed = (
        e.remaining is not None
        and e.full_capacity is not None
        and e.design_capacity is not None
    )
    capacity_invalid = capacity_observed and (
        e.remaining > DOCUMENTED_CAPACITY_MAH
        or e.full_capacity <= 0
        or e.full_capacity > DOCUMENTED_CAPACITY_MAH
        or e.design_capacity != DOCUMENTED_CAPACITY_MAH
        or e.remaining > e.full_capacity
    )
    capacity_valid = (
        capacity_observed
        and not capacity_invalid
        and e.full_capacity > 0
        and 0 <= e.remaining
    )
    ratio = round(e.remaining * 100 / e.full_capacity) if capacity_valid else None
    agrees = ratio is not None and abs(ratio - e.soc) <= 3
    full_evidence = (
        ratio == 100
        and e.input_connected is True
        and e.charge_complete is True
        and e.voltage_mv is not None
        and e.voltage_mv >= 4150
        and e.current_ma is not None
        and abs(e.current_ma) <= 100
    )
    if capacity_invalid:
        return e.soc, "GAUGE SOC", "BATTERY GAUGE CAPACITY MODEL INVALID"
    if ratio is not None and not agrees and full_evidence:
        return 100, "CAPACITY RATIO", "FULL"
    if ratio is not None and not agrees:
        return e.soc, "GAUGE SOC", "GAUGE MODEL MISMATCH"
    if not capacity_valid:
        return e.soc, "GAUGE SOC", "CAPACITY DATA UNAVAILABLE"
    if agrees and e.soc < 95 and not full_evidence:
        return e.soc, "GAUGE SOC", "PARTIAL"
    return e.soc, "GAUGE SOC", "FULL" if e.soc == 100 and full_evidence else "AVAILABLE"


class BatteryAccuracyPolicyTests(unittest.TestCase):
    def test_normal_raw_soc_agreement(self):
        self.assertEqual(
            decide(Evidence(62, 930, 1500, 1500, 3850, -120, False, False)),
            (62, "GAUGE SOC", "PARTIAL"),
        )

    def test_raw_eighty_capacity_full_with_verified_full_evidence(self):
        self.assertEqual(
            decide(Evidence(80, 1500, 1500, 1500, 4190, 12, True, True)),
            (100, "CAPACITY RATIO", "FULL"),
        )

    def test_unavailable_capacity_keeps_raw_soc_and_reports_unavailable(self):
        self.assertEqual(
            decide(Evidence(80, None, None, None, 4109, 0, True, True)),
            (80, "GAUGE SOC", "CAPACITY DATA UNAVAILABLE"),
        )

    def test_impossible_remaining_capacity_is_invalid_model_evidence(self):
        self.assertEqual(
            decide(Evidence(80, 2386, 3000, 3000, 4109, 0, True, True)),
            (80, "GAUGE SOC", "BATTERY GAUGE CAPACITY MODEL INVALID"),
        )

    def test_zero_full_capacity_is_invalid_model_evidence(self):
        self.assertEqual(
            decide(Evidence(80, 1200, 0, 1500, 4109, 0, True, True)),
            (80, "GAUGE SOC", "BATTERY GAUGE CAPACITY MODEL INVALID"),
        )

    def test_design_capacity_must_exactly_match_documented_pack(self):
        for design in (1200, 3000):
            with self.subTest(design=design):
                self.assertEqual(
                    decide(Evidence(80, 1200, 1500, design, 3920, 340, True, False)),
                    (80, "GAUGE SOC", "BATTERY GAUGE CAPACITY MODEL INVALID"),
                )

    def test_missing_design_capacity_disables_ratio_and_partial_inference(self):
        self.assertEqual(
            decide(Evidence(80, 1200, 1500, None, 3920, 340, True, False)),
            (80, "GAUGE SOC", "CAPACITY DATA UNAVAILABLE"),
        )

    def test_valid_contradictory_ratio_is_required_for_mismatch(self):
        self.assertEqual(
            decide(Evidence(80, 1350, 1500, 1500, 4190, 12, True, True)),
            (80, "GAUGE SOC", "GAUGE MODEL MISMATCH"),
        )

    def test_impossible_eighty_percent_capacity_is_never_confirmed_partial(self):
        self.assertEqual(
            decide(Evidence(80, 2386, 3000, 3000, 3920, 340, True, False)),
            (80, "GAUGE SOC", "BATTERY GAUGE CAPACITY MODEL INVALID"),
        )

    def test_charger_completion_stopping_early(self):
        self.assertEqual(
            decide(Evidence(80, 1200, 1500, 1500, 4020, 0, True, False)),
            (80, "GAUGE SOC", "PARTIAL"),
        )
        self.assertIn("Diagnosis::ChargerNotContinuing", BATTERY)

    def test_raw_soc_disagreement_with_valid_nonfull_ratio(self):
        self.assertEqual(
            decide(Evidence(80, 1350, 1500, 1500, 4050, 40, True, False)),
            (80, "GAUGE SOC", "GAUGE MODEL MISMATCH"),
        )

    def test_firmware_read_validation_defect_is_distinct_from_fabricated_capacity(self):
        self.assertEqual(
            decide(Evidence(80, None, 1500, 1500, 4100, 0, True, True)),
            (80, "GAUGE SOC", "CAPACITY DATA UNAVAILABLE"),
        )
        self.assertIn("remainingValid", BATTERY)
        self.assertIn("CapacityFieldStatus::Unavailable", BATTERY)

    def test_bounded_live_record_contains_complete_authorized_evidence(self):
        record = BATTERY[BATTERY.index('const String rawSocText'):]
        for field in ("raw_soc=", "displayed=", "source=", "ratio=", "remaining=",
                      "full=", "design=", "documented_pack=", "voltage=", "current=", "input=", "phase=",
                      "freshness=", "diagnosis="):
            self.assertIn(field, record)
        for forbidden in ("ssid=", "password=", "token=", "url=", "latitude=", "longitude="):
            self.assertNotIn(forbidden, record.lower())
        self.assertIn('const String rawSocText', record)
        self.assertNotRegex(record, r'\?\s*""\s*:\s*"--"')

    def test_stale_values_and_read_failures_never_select_a_percentage(self):
        self.assertEqual(
            decide(Evidence(80, 1500, 1500, 1500, 4190, 0, True, True, fresh=False)),
            (None, "UNAVAILABLE", "STALE"),
        )
        self.assertEqual(
            decide(Evidence(None, 1500, 1500, 1500, 4190, 0, True, True)),
            (None, "UNAVAILABLE", "STALE"),
        )

    def test_firmware_uses_three_bounded_correlated_read_only_frames(self):
        self.assertIn("kRemainingCapacityRegister = 0x10", BATTERY)
        self.assertIn("kFullChargeCapacityRegister = 0x12", BATTERY)
        self.assertIn("kDesignCapacityRegister = 0x3C", BATTERY)
        self.assertNotIn("kRemainingCapacityRegister = 0x0C", BATTERY)
        self.assertIn("kEvidenceSampleCount = 3", BATTERY)
        sample_start = BATTERY.index("EvidenceFrame frames[kEvidenceSampleCount]")
        sample_loop = BATTERY[sample_start : BATTERY.index("uint16_t soc = 0", sample_start)]
        for register in (
            "kStateOfChargeRegister",
            "kRemainingCapacityRegister",
            "kFullChargeCapacityRegister",
            "kDesignCapacityRegister",
            "kVoltageRegister",
            "kAverageCurrentRegister",
            "readChargerStatus",
        ):
            self.assertIn(register, sample_loop)
        self.assertIn("kVoltageSampleToleranceMv", BATTERY)
        self.assertIn("kCurrentSampleToleranceMa", BATTERY)
        self.assertIn("kCapacitySampleToleranceMah", BATTERY)
        self.assertIn("stableCharger", BATTERY)
        self.assertIn("decodeLittleEndianWord(data[0], data[1])", BATTERY)

    def test_no_battery_i2c_writes_beyond_register_pointer_reads(self):
        helper = BATTERY[BATTERY.index("bool readRegister(") : BATTERY.index("bool readGaugeWord(")]
        self.assertEqual(helper.count("Wire.write("), 1)
        self.assertIn("Wire.write(reg)", helper)
        self.assertIn("Wire.endTransmission(false)", helper)
        self.assertIn("Wire.requestFrom(address", helper)
        self.assertEqual(BATTERY.count("Wire.write("), 1)
        self.assertNotRegex(
            BATTERY.lower(),
            r"\b(unseal|seal|reset|calibrat|data.?memory|control.?subcommand)\s*\(",
        )

    def test_full_and_source_contract_is_explicit(self):
        self.assertIn("DisplaySource::GaugeSoc", BATTERY)
        self.assertIn("DisplaySource::CapacityRatio", BATTERY)
        self.assertIn("snapshot_.capacityRatioPercent == 100", BATTERY)
        self.assertIn("voltage >= kFullVoltageEvidenceMv", BATTERY)
        self.assertIn("abs(static_cast<int>(averageCurrent)) <= kFullCurrentEvidenceMa", BATTERY)
        self.assertIn("fullEvidence && percent == 100", BATTERY)
        self.assertNotRegex(BATTERY, r"(?:soc|percent)\s*\*\s*100\s*/\s*80")
        self.assertNotIn("snapshot_.percent = 100", BATTERY)
        for label in (
            '"DISPLAYED SOURCE"',
            '"RAW SOC / RATIO"',
            '"INPUT / CHARGE"',
            '"FRESHNESS"',
            '"EXPLANATION"',
        ):
            self.assertIn(label, PAGES)
        self.assertIn('return "GAUGE SOC"', BATTERY)
        self.assertIn('return "CAPACITY RATIO"', BATTERY)
        self.assertIn("GAUGE MODEL MISMATCH", BATTERY)
        self.assertIn("BATTERY GAUGE CAPACITY MODEL INVALID", BATTERY)
        self.assertIn('return "CAPACITY DATA UNAVAILABLE"', BATTERY)
        self.assertIn("CapacityFieldStatus::Unavailable", BATTERY)
        self.assertIn("CapacityFieldStatus::Invalid", BATTERY)
        self.assertIn("CHARGER COMPLETE; ", PAGES)
        self.assertIn("; CAPACITY DATA UNAVAILABLE", PAGES)
        diagnosis = BATTERY[BATTERY.index("if (!socValid) snapshot_.diagnosis") :
                            BATTERY.index("const bool recoveringSoc")]
        self.assertLess(diagnosis.index("capacityModelInvalid"),
                        diagnosis.index("!snapshot_.capacityAvailable"))
        self.assertLess(diagnosis.index("!snapshot_.capacityAvailable"),
                        diagnosis.index("Diagnosis::GaugeModelMismatch"))
        self.assertEqual(diagnosis.count("Diagnosis::GaugeModelMismatch"), 1)


if __name__ == "__main__":
    unittest.main()