import importlib.util
import json
import tempfile
import unittest
import hashlib
from pathlib import Path
from PIL import Image
ROOT=Path(__file__).parents[1]
SPEC=json.loads((ROOT/'ui/ui_spec.json').read_text())
def txt(p): return (ROOT/p).read_text()
THEME=txt('src/ui/ui_theme.h'); GENERATED=txt('src/ui/ui_spec_generated.h'); FONTS=txt('src/ui/ui_fonts_generated.h')
PAGES=txt('src/ui/ui_pages.cpp'); COMPONENTS=txt('src/ui/ui_components.cpp'); CONTROLLER=txt('src/ui/ui_controller.cpp')
TOUCH=txt('src/input/touch_controller.cpp'); MAIN=txt('src/main.cpp')
METRICS=json.loads(txt('tools/ui_font_metrics.json')); GENERATOR=txt('tools/generate_font_assets.py')
def load(name,path):
 s=importlib.util.spec_from_file_location(name,path); m=importlib.util.module_from_spec(s); s.loader.exec_module(m); return m
contract_gen=load('contract_gen',ROOT/'tools/generate_ui_contract.py'); framebuffer=load('framebuffer',ROOT/'tools/framebuffer_to_png.py')
class UiContractTests(unittest.TestCase):
 def test_generated_contract_and_weighted_embedded_fonts(self):
  self.assertEqual(GENERATED,contract_gen.render_header(SPEC)); self.assertEqual(SPEC['font']['family'],'Inter')
  self.assertIn('Inter 4.1 and Atkinson Hyperlegible (OFL-1.1)',FONTS)
  expected={'Caption':('Inter-SemiBold.ttf',15),'Body':('Inter-SemiBold.ttf',18),'CardHeading':('Inter-SemiBold.ttf',22),'PageHeading':('Inter-Bold.ttf',28),'Brand':('Inter-Bold.ttf',32),'Metric':('Inter-Bold.ttf',30),'Navigation':('Inter-SemiBold.ttf',15)}
  for role,(source,size) in expected.items():
   data=METRICS['roles'][role]; self.assertTrue(data['source'].endswith(source)); self.assertEqual(data['size'],size); self.assertEqual(data['mode'],'reinforced_aa'); self.assertGreaterEqual(data['black_core_percent'],70.0); self.assertTrue((ROOT/data['source']).is_file())
  self.assertIn("'rasterization':'final_size_no_scaling'",GENERATOR.replace(' ','')); self.assertNotIn('resize(',GENERATOR); self.assertIn('int8_t xOffset; int8_t yOffset',FONTS)
  self.assertIn('pixel / 2',COMPONENTS); self.assertIn('coverage',COMPONENTS)
  self.assertIn('paper * (15 - coverage) + foreground * coverage + 7',COMPONENTS)
  # Renderer interpolation must preserve exact endpoints and the deliberate
  # qualification AA levels in both black-on-white and white-on-black modes.
  blend=lambda foreground,paper,coverage:(paper*(15-coverage)+foreground*coverage+7)//15
  self.assertEqual([blend(0,15,c) for c in (15,13,11,0)],[0,2,4,15])
  self.assertEqual([blend(15,0,c) for c in (15,13,11,0)],[15,13,11,0])
 def test_font_licenses_and_distinct_weight_files(self):
  self.assertIn('SIL OPEN FONT LICENSE Version 1.1',txt('assets/fonts/Inter-OFL.txt'))
  self.assertIn('SIL OPEN FONT LICENSE Version 1.1',txt('assets/fonts/OFL.txt'))
  paths=[ROOT/'assets/fonts'/f'Inter-{weight}.ttf' for weight in ('Regular','Medium','SemiBold','Bold')]
  self.assertEqual(len({hashlib.sha256(path.read_bytes()).hexdigest() for path in paths}),4)
  for path in paths:self.assertGreater(path.stat().st_size,400000)
 def test_high_contrast_palette(self):
  p=SPEC['palette']; self.assertEqual({p[k] for k in ('ink','ink_muted','rule')},{0}); self.assertEqual({p[k] for k in ('surface_strong','surface','surface_soft','paper')},{255})
  self.assertIn('kHighContrastQualificationTheme = true',THEME); self.assertIn('sole intentional grayscale use',PAGES)
 def test_ui_qual_3_bounds_visibility_and_serial(self):
  e=next(x for x in SPEC['pages']['touch_setup']['text'] if x[-1]=='UI QUAL 3'); c=SPEC['pages']['touch_setup']['cards'][1]
  self.assertGreaterEqual(e[0]-c[0],16); self.assertGreaterEqual(e[1]-c[1],16); self.assertLessEqual(e[0]+e[2],c[0]+c[2]-16); self.assertLessEqual(e[1]+e[3],c[1]+c[3]-16)
  self.assertIn('const String build="UI QUAL 3"',PAGES); self.assertGreaterEqual(MAIN.count('UI QUAL 3'),2)
 def test_touch_target_inset_and_acceptance(self):
  t=SPEC['touch']; self.assertGreaterEqual(t['target_inset'],70); self.assertLessEqual(t['target_inset'],90); self.assertLessEqual(t['target_inset'],t['corner_x']); self.assertLessEqual(t['target_inset'],t['corner_y'])
  self.assertIn('kTouchTargetRadius',PAGES); self.assertIn('cornerContains',TOUCH)
 def test_release_arm_stale_update_and_one_cycle(self):
  self.assertEqual(SPEC['touch']['clean_release_ms'],500); self.assertEqual(SPEC['touch']['post_refresh_quiet_ms'],500)
  for value in ('stale_state=DRAINED','PRESS_BEFORE_ARM','DISPLAY_UPDATE_ACTIVE','kTouchCleanReleaseMs','if(pressed&&!down_)','if(!pressed&&down_)'): self.assertIn(value,TOUCH)
  release=TOUCH[TOUCH.index('if(!pressed&&down_)'):]; self.assertIn('armed_=false',release); self.assertEqual(release.count('++qualificationStep_'),1); self.assertIn('notifyDisplayUpdateFinished',MAIN)
  self.assertIn('RELEASE_BASELINE_REQUIRED',TOUCH); self.assertIn('CONTACT_REPORT_GAP',TOUCH); self.assertIn('kQualificationSchema=3',TOUCH)
 def test_all_candidate_transforms_and_four_sample_gate(self):
  for name in ('Identity','InvertX','InvertY','InvertXY','Swap','SwapInvertX','SwapInvertY','SwapInvertXY'): self.assertIn('Transform::'+name,TOUCH)
  self.assertIn('for(Transform c:kCandidates)',TOUCH); self.assertIn('for(uint8_t i=0;i<4;++i)',TOUCH); self.assertIn('basis=FOUR_PHYSICAL_TARGET_SAMPLES',TOUCH)
  self.assertIn('kExpectedTargets',TOUCH); self.assertNotIn('kExpectedCorners',TOUCH)
  final=TOUCH[TOUCH.index('if(qualificationStep_==4)'):]; self.assertLess(final.index('selectPhysicalTransform'),final.index('saveQualification(true)'))
 def test_framebuffer_reset_and_canaries(self):
  self.assertEqual(SPEC['framebuffer']['stride_bytes']*SPEC['canvas']['physical_height'],259200)
  marks=['memset(compositionBuffer_, 0xFF, kFramebufferBytes)','canaries_pre=INTACT','renderPage(compositionBuffer_, snapshot_)','Serial.println("DISPLAY canaries=INTACT")','memcpy(framebuffer, compositionBuffer_, kFramebufferBytes)']; pos=[CONTROLLER.index(x) for x in marks]; self.assertEqual(pos,sorted(pos))
  self.assertIn('retained_page_data=NONE white_reset=VERIFIED',CONTROLLER); self.assertIn('kGuardBytes = 64',CONTROLLER)
 def test_white_test_guard_partial_off_gc16_and_poweroff(self):
  self.assertIn('whiteTestUsed_ || updating_',CONTROLLER); self.assertIn('"WHITE TEST"',CONTROLLER); self.assertIn('command == "display white-test"',MAIN); self.assertIn('kPartialRefreshEnabled = false',THEME); self.assertNotIn('MODE_DU',CONTROLLER); self.assertGreaterEqual(CONTROLLER.count('MODE_GC16'),2); self.assertGreaterEqual(CONTROLLER.count('epd_poweroff'),2)
 def test_cleanup_schema_and_preserved_policy(self):
  self.assertIn('kDisplayCleanupRevision = 6',MAIN); self.assertIn('bootRecovery && !fullClearUsed_',CONTROLLER); render=CONTROLLER[CONTROLLER.index('bool UiController::renderIfDirty'):CONTROLLER.index('bool UiController::renderWhiteTest')]; self.assertEqual(render.count('epd_fullclear'),1)
  self.assertIn('EPD_ROT_INVERTED_PORTRAIT',CONTROLLER); self.assertIn('epd_set_vcom(1560)',CONTROLLER); self.assertNotIn('.transmit(',MAIN); self.assertIn('hq::kBoard.gpsRx, -1',MAIN)
 def test_navigation_device_icon_has_uniform_single_outline(self):
  device=COMPONENTS[COMPONENTS.index('case Icon::Device:'):COMPONENTS.index('case Icon::Battery:')]
  self.assertIn('epd_draw_rect',device); self.assertNotIn('roundedRect',device); self.assertNotIn('kPaper',device)
 def test_text_qualification_content_and_diagnostics_only_route(self):
  for phrase in ('TEXT QUALIFICATION','SOLID BLACK 0x0','3 PX PURE-BLACK LINE','supportFORGE Field Terminal','CPU 42%  RAM 61%  ONLINE','BLACK ON WHITE','WHITE ON BLACK','INTER SEMIBOLD 20 STRONG AA','INTER BOLD 20 MONO'): self.assertIn(phrase,PAGES)
  self.assertIn('kDiagnosticsTextQualificationAction',MAIN); self.assertNotIn('Page::TextQualification',MAIN[MAIN.index('const ui::Page pages[]'):MAIN.index('destination = pages')])
  self.assertIn('epd_fill_rect({24,94,492,54},kInk,fb)',PAGES); self.assertIn('epd_fill_rect({24,164,492,3},kInk,fb)',PAGES)
 def test_navigation_real_semibold_visible_and_centered(self):
  self.assertEqual(SPEC['navigation']['labels'],['HOME','SYSTEMS','RADIO','LOCATION','DEVICE']); self.assertEqual(len(SPEC['navigation']['bounds']),5)
  nav=METRICS['roles']['Navigation']; self.assertEqual((nav['weight'],nav['size']),('SemiBold',15))
  advances=nav['advances']; width=lambda s:sum(advances[ord(c)-32] for c in s)
  for label,bounds in zip(SPEC['navigation']['labels'],SPEC['navigation']['bounds']): self.assertLess(width(label),bounds[2]-6); self.assertEqual(bounds[1]+bounds[3],960)
  self.assertIn('FontRole::Navigation',COMPONENTS); self.assertIn('active?kPaper:kInk',COMPONENTS)
 def test_primary_palette_and_required_labels_never_pale(self):
  self.assertNotIn('opacity',COMPONENTS.lower()+PAGES.lower()); self.assertNotIn('alpha',COMPONENTS.lower()+PAGES.lower())
  for label in ('supportFORGE','FIELD TERMINAL','SETUP REQUIRED','SYSTEMS','RADIO','LOCATION','DEVICE','UI QUAL 3'): self.assertIn(label,PAGES+COMPONENTS+MAIN)
  self.assertEqual(SPEC['palette']['ink'],0); self.assertEqual(SPEC['palette']['ink_muted'],0)
 def test_dump_is_authoritative_unpowered_and_touch_persistence_preserved(self):
  dump=CONTROLLER[CONTROLLER.index('bool UiController::dumpPackedFramebuffer'):]
  self.assertIn('renderPage(compositionBuffer_, dumpSnapshot)',dump); self.assertIn('Serial.write(compositionBuffer_, kFramebufferBytes)',dump); self.assertNotIn('epd_poweron',dump); self.assertNotIn('epd_hl_update_screen',dump)
  self.assertIn('saveQualification(true)',TOUCH); self.assertIn('mappingVerified()',MAIN); self.assertNotIn('resetQualification();',MAIN[MAIN.index('void setup()'):])
 def test_framebuffer_dump_polarity(self):
  packed=bytearray([0xFF]*framebuffer.FRAME_BYTES); packed[0]=0x0F
  with tempfile.TemporaryDirectory() as d:
   source=Path(d)/'f.bin'; output=Path(d)/'f.png'; source.write_bytes(packed); framebuffer.convert(source,output,portrait=False)
   with Image.open(output) as image: self.assertEqual((image.getpixel((0,0)),image.getpixel((1,0))),(255,0))
 def test_framebuffer_portrait_rotation_is_inverted_portrait(self):
  packed=bytearray([0xFF]*framebuffer.FRAME_BYTES); packed[(framebuffer.HEIGHT-1)*framebuffer.STRIDE]=0xF0
  with tempfile.TemporaryDirectory() as d:
   source=Path(d)/'f.bin'; output=Path(d)/'f.png'; source.write_bytes(packed); framebuffer.convert(source,output,portrait=True)
   with Image.open(output) as image: self.assertEqual(image.size,(540,960)); self.assertEqual(image.getpixel((0,0)),0)
 def test_python_cache_hygiene(self):
  ignore=txt('.gitignore'); self.assertIn('__pycache__/',ignore); self.assertIn('*.pyc',ignore); self.assertTrue((ROOT/'test/__init__.py').is_file())
 def test_live_telemetry_preserves_radio_location_display_and_privacy_contracts(self):
  manager=txt('src/telemetry/telemetry_manager.cpp'); parser=txt('src/telemetry/guardian_parser.cpp'); model=txt('src/telemetry/telemetry_model.h'); config=txt('src/app_config.h')
  self.assertIn('ArduinoJson@7.2.1',txt('platformio.ini')); self.assertIn('src/secrets.h',txt('.gitignore')); self.assertTrue((ROOT/'src/secrets.example.h').is_file())
  self.assertIn('x-guardian-telemetry-token',manager); self.assertNotIn('?token=',manager+config); self.assertIn('path=REDACTED',manager)
  self.assertIn('kQueryTokenCompatibilityEnabled = false',config); self.assertIn('kPollIntervalMs = 60000',config); self.assertIn('kOfflineFailedCycles = 3',config)
  self.assertIn('parseGuardianPayload',parser); self.assertIn('JsonObjectConst root',parser); self.assertIn('recognizedFields',model)
  for phrase in ('915 MHz region - transmission disabled','TX LOCKED','RECEIVE ONLY','COORDINATES","PRIVATE') : self.assertIn(phrase,PAGES)
  self.assertNotIn('.transmit(',MAIN); self.assertIn('hq::kBoard.gpsRx, -1',MAIN); self.assertIn('MODE_GC16',CONTROLLER); self.assertNotIn('MODE_DU',CONTROLLER); self.assertIn('epd_poweroff()',CONTROLLER)
if __name__=='__main__': unittest.main()
