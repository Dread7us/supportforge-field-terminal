from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]


class UpstreamRecoverySpikeContractTests(unittest.TestCase):
    def test_fast_epd_environment_is_compile_only_and_isolated(self):
        ini = (ROOT / "platformio.ini").read_text(encoding="utf-8")
        section = ini.split("[env:fast_epd_h752_02_compile_only]", 1)[1]
        provenance = (
            ROOT / "evaluation" / "lib" / "FastEPD" / "UPSTREAM.md"
        ).read_text(encoding="utf-8")
        self.assertIn("-<*>", section)
        self.assertIn("+<../evaluation/fast_epd_smoke.cpp>", section)
        self.assertIn("evaluation/lib", section)
        self.assertIn("FastEPD", section)
        self.assertNotIn("github.com", section)
        self.assertIn("95f8696466e386fce84dbe10edb8713a8a9be387", provenance)
        self.assertNotIn("src/secrets.h", section)

    def test_smoke_image_cannot_initialize_hardware_at_runtime(self):
        smoke = (ROOT / "evaluation" / "fast_epd_smoke.cpp").read_text(
            encoding="utf-8"
        )
        setup = smoke.split("void setup()", 1)[1].split("void loop()", 1)[0]
        self.assertNotIn("compileDeterministicSmokeScreen()", setup)
        self.assertNotIn("initPanel", setup)
        self.assertNotIn("fullUpdate", setup)
        self.assertNotIn("partialUpdate", smoke)
        self.assertNotIn("initLights", smoke)

    def test_production_candidate_does_not_inherit_fast_epd(self):
        ini = (ROOT / "platformio.ini").read_text(encoding="utf-8")
        candidate = ini.split("[env:h752_02_candidate]", 1)[1].split("[env:", 1)[0]
        self.assertNotIn("FastEPD", candidate)
        self.assertNotIn("fast_epd_smoke", candidate)
        self.assertNotIn("lib_extra_dirs", candidate)