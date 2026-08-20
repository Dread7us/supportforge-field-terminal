import importlib.util
import json
import tempfile
import unittest
import hashlib
import re
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
 def test_header_regions_are_fixed_disjoint_and_inside_app_bar(self):
  g=SPEC['geometry']; names=('header_brand_bounds','header_clock_bounds','header_date_bounds','header_wifi_bounds','header_battery_bounds')
  regions=[g[name] for name in names]
  for x,y,w,h in regions:
   self.assertGreater(w,0); self.assertGreater(h,0); self.assertGreaterEqual(x,0); self.assertGreaterEqual(y,0)
   self.assertLessEqual(x+w,SPEC['canvas']['width']); self.assertLessEqual(y+h,g['app_bar_height'])
  intersects=lambda a,b:a[0]<b[0]+b[2] and b[0]<a[0]+a[2] and a[1]<b[1]+b[3] and b[1]<a[1]+a[3]
  for i,left in enumerate(regions):
   for right in regions[i+1:]: self.assertFalse(intersects(left,right),(left,right))
  self.assertIn('static_assert(spec::kHeaderBrandBounds',COMPONENTS)
  self.assertIn('"brand and clock regions overlap"',COMPONENTS)
  self.assertIn('"clock and date regions overlap"',COMPONENTS)
  self.assertIn('"time/date and Wi-Fi regions overlap"',COMPONENTS)
  self.assertIn('"Wi-Fi and battery regions overlap"',COMPONENTS)
  self.assertNotIn('footer_refresh_bounds',SPEC['geometry'])
  self.assertEqual(g['app_bar_height'],64)
  self.assertEqual(len({region[1] for region in regions}),1)
  self.assertEqual(len({region[3] for region in regions}),1)
  self.assertGreaterEqual(g['header_clock_bounds'][3],METRICS['roles']['Caption']['line_height'])
  self.assertGreaterEqual(g['header_date_bounds'][3],METRICS['roles']['Caption']['line_height'])
 def test_header_is_white_clipped_aligned_and_brand_drawn_once(self):
  app=COMPONENTS[COMPONENTS.index('void appBar('):COMPONENTS.index('void card(')]
  self.assertEqual(app.count('"supportFORGE"'),1)
  self.assertEqual(app.count('epd_fill_rect({brandClip.x'),1)
  self.assertEqual(app.count('epd_fill_rect({clockClip.x'),1)
  self.assertEqual(app.count('epd_fill_rect({dateClip.x'),1)
  self.assertEqual(app.count('epd_fill_rect({wifiClip.x'),1)
  self.assertEqual(app.count('epd_fill_rect({batteryClip.x'),1)
  self.assertEqual(app.count(',kPaper,fb);'),6)
  self.assertNotIn('kInk,fb);\n  const Rect brandClip',app)
  for token in ('brandClip,brandClip.x,spec::kHeaderBaseline',
                'clockClip,max(clockClip.x,clockX),spec::kHeaderBaseline',
                'dateClip,max(dateClip.x,dateX),spec::kHeaderBaseline',
                'stateClip,stateClip.x,spec::kHeaderBaseline'):
   self.assertIn(token,app)
  self.assertEqual(app.count('centeredBaseline('),0)
  self.assertNotIn('subtitle',app.lower())
  self.assertNotIn('"FIELD TERMINAL"',app)
  self.assertIn('clockClip.w-textWidth(time,FontRole::CardHeading)',app)
  self.assertIn('dateClip.w-textWidth(date,FontRole::Caption)',app)
  self.assertEqual(app.count('epd_draw_hline'),1)
  self.assertIn('epd_fill_rect({0,kAppBarHeight-1,kCanvasWidth,9},kPaper,fb)',app)
  def width(value,role): return sum(METRICS['roles'][role]['advances'][ord(c)-32] for c in value)
  self.assertLessEqual(width('12:59 PM','CardHeading'),SPEC['geometry']['header_clock_bounds'][2])
  self.assertLessEqual(width('--:--','CardHeading'),SPEC['geometry']['header_clock_bounds'][2])
  self.assertLessEqual(width('TIME SYNC','Caption'),SPEC['geometry']['header_date_bounds'][2])
  brand=METRICS['roles']['CardHeading']; brand_width=width('supportFORGE','CardHeading')
  self.assertLessEqual(brand_width,SPEC['geometry']['header_brand_bounds'][2])
  self.assertLessEqual(brand['line_height'],SPEC['geometry']['header_brand_bounds'][3])
  self.assertIn('batteryClip.x+56,spec::kHeaderBaseline-25,80,32',app)
  self.assertIn('batteryStateLabel="FULL"',app)
  self.assertIn('batteryStateLabel="VERIFY"',app)
  self.assertIn('batteryStateLabel="LKG"',app)
  self.assertIn('batteryStateLabel="ERR"',app)
 def test_header_unavailable_states_and_battery_fill_truthfulness(self):
  app=COMPONENTS[COMPONENTS.index('void appBar('):COMPONENTS.index('void card(')]
  battery_icon=COMPONENTS[COMPONENTS.index('void batteryIcon('):COMPONENTS.index('void circle(')]
  self.assertIn('String time = "--:--"',app); self.assertIn(':"TIME SYNC"',app)
  self.assertIn('if (!percentAvailable || !battery::validPercent(percent))',battery_icon)
  self.assertIn('min<uint8_t>(percent, 100)',battery_icon)
  self.assertIn('const String unknown = "--"',battery_icon)
  self.assertIn('const Rect filled',battery_icon); self.assertIn('const Rect unfilled',battery_icon)
  self.assertIn('label, FontRole::Caption, kPaper',battery_icon)
  self.assertIn('label, FontRole::Caption, kInk',battery_icon)
  self.assertGreater(battery_icon.index('if (!percentAvailable'),battery_icon.index('epd_draw_rect'))
  self.assertGreater(battery_icon.index('const Rect filled'),battery_icon.index('if (!percentAvailable'))
 def test_battery_percentage_text_fits_every_value_and_uses_split_contrast(self):
  advances=METRICS['roles']['Caption']['advances']; width=lambda s:sum(advances[ord(c)-32] for c in s)
  # Compact header glyph is 80 px; body/interior subtract 5 and then 6.
  interior_width=80-5-6
  self.assertTrue(all(width(str(value)) <= interior_width for value in (0,9,10,87,99,100)))
  self.assertLessEqual(width('--'),interior_width)
  battery_icon=COMPONENTS[COMPONENTS.index('void batteryIcon('):COMPONENTS.index('void circle(')]
  self.assertIn('text(fb, filled, labelX',battery_icon)
  self.assertIn('text(fb, unfilled, labelX',battery_icon)
  self.assertIn('state == battery::State::Charging',battery_icon)
  self.assertNotRegex(battery_icon.lower(),r'millis\(|delay\(|frame\s*\+\+')
 def test_wifi_header_vertical_geometry_and_visual_weight(self):
  g=SPEC['geometry']; wifi=g['header_wifi_bounds']
  self.assertEqual(g['header_baseline'],41)
  self.assertEqual(wifi[1],6)
  self.assertEqual(wifi[1]+wifi[3],58)
  icon=COMPONENTS[COMPONENTS.index('void wifiIcon('):COMPONENTS.index('void appBar(')]
  self.assertIn('constexpr int kBarCount=4,kBarWidth=4,kBarGap=3',icon)
  self.assertIn('bottom=spec::kHeaderBaseline+2',icon)
  self.assertIn('if(index<bars)epd_fill_rect',icon)
  self.assertIn('else epd_draw_rect',icon)
  self.assertNotRegex(icon,r'epd_draw_circle|epd_fill_circle|epd_draw_line|cos\(|sin\(')
  home=SPEC['pages']['home']['cards']
  self.assertEqual(home[0],[24,80,492,330])
  self.assertEqual(home[-1],[278,620,238,178])
  self.assertLess(home[-1][1]+home[-1][3],SPEC['geometry']['content_bottom'])
  home_renderer=PAGES[PAGES.index('void home('):PAGES.index('void systems(')]
  clear='epd_fill_rect({kMargin,kAppBarHeight,kCanvasWidth-2*kMargin,'
  self.assertIn(clear,home_renderer)
  self.assertLess(home_renderer.index(clear),home_renderer.index('const Rect hero=contractRect(spec::kHomeHeroBounds)'))
 def test_display_completion_establishes_idle_release_without_sacrificing_next_tap(self):
  finished=TOUCH[TOUCH.index('void TouchController::notifyDisplayUpdateFinished'):
                 TOUCH.index('bool TouchController::readRaw')]
  self.assertIn('releaseObserved_=true',finished)
  self.assertIn('cleanReleaseSinceMs_=nowMs',finished)
  self.assertIn('release_baseline=ESTABLISHED',finished)
  self.assertNotIn('releaseObserved_=false',finished)
 def test_generated_contract_and_weighted_embedded_fonts(self):
  self.assertEqual(GENERATED,contract_gen.render_header(SPEC)); self.assertEqual(SPEC['font']['family'],'Inter')
  self.assertIn('Inter 4.1 and Atkinson Hyperlegible (OFL-1.1)',FONTS)
  expected={'Caption':('Inter-SemiBold.ttf',15),'Body':('Inter-SemiBold.ttf',18),'CardHeading':('Inter-SemiBold.ttf',22),'PageHeading':('Inter-Bold.ttf',28),'Brand':('Inter-Bold.ttf',32),'Metric':('Inter-Bold.ttf',30),'Navigation':('Inter-SemiBold.ttf',15)}
  for role,(source,size) in expected.items():
   data=METRICS['roles'][role]; self.assertTrue(data['source'].endswith(source)); self.assertEqual(data['size'],size); self.assertEqual(data['mode'],'reinforced_aa'); self.assertGreaterEqual(data['black_core_percent'],70.0); self.assertTrue((ROOT/data['source']).is_file())
  self.assertIn("'rasterization':'final_size_no_scaling'",GENERATOR.replace(' ','')); self.assertNotIn('resize(',GENERATOR); self.assertIn('int16_t xOffset; int16_t yOffset',FONTS)
  self.assertIn('pixel / 2',COMPONENTS); self.assertIn('coverage',COMPONENTS)
  self.assertIn('paper * (15 - coverage) + foreground * coverage + 7',COMPONENTS)
  # Renderer interpolation must preserve exact endpoints and the deliberate
  # qualification AA levels in both black-on-white and white-on-black modes.
  blend=lambda foreground,paper,coverage:(paper*(15-coverage)+foreground*coverage+7)//15
  self.assertEqual([blend(0,15,c) for c in (15,13,11,0)],[0,2,4,15])
  self.assertEqual([blend(15,0,c) for c in (15,13,11,0)],[15,13,11,0])
 def test_vehicle_motion_giant_speed_geometry_fit_and_clean_redraw(self):
  speed=METRICS['roles']['VehicleSpeed']; self.assertEqual((speed['weight'],speed['size']),('Bold',152))
  self.assertEqual(speed['mode'],'reinforced_aa'); self.assertGreaterEqual(speed['black_core_percent'],70.0)
  advances=speed['advances']; width=lambda s:sum(advances[ord(c)-32] for c in s)
  bounds=(24,136,492,186)
  for value in ('0','9','65','100','999','--'):
   self.assertLessEqual(width(value),bounds[2]-48,value)
   self.assertLessEqual(speed['line_height'],bounds[3],value)
  theme=txt('src/ui/ui_theme.h')
  self.assertIn('constexpr Rect kVehicleSpeedBounds{24, 136, 492, 186}',theme)
  self.assertIn('vehicle speed must remain the dominant metric region',theme)
  motion=PAGES[PAGES.index('void vehicleMotion('):PAGES.index('void altimeter(')]
  clear='epd_fill_rect({kVehicleSpeedBounds.x,kVehicleSpeedBounds.y,kVehicleSpeedBounds.w,kVehicleSpeedBounds.h},kPaper,fb)'
  draw='centeredText(fb,kVehicleSpeedBounds,speed,FontRole::VehicleSpeed,kInk)'
  self.assertIn(clear,motion); self.assertIn(draw,motion); self.assertLess(motion.index(clear),motion.index(draw))
  self.assertNotRegex(motion,r'kInkMuted|delay\(|millis\(|requestRender')
  self.assertIn('compassRose(fb,kVehicleCompassBounds',motion)
  self.assertIn('roundedRect(fb,kVehicleQualityBounds',motion)
  self.assertIn('detailBack(fb)',motion)
  self.assertIn('displayCoordinator.page()==ui::Page::VehicleMotion',MAIN)
  self.assertIn('name == "vehicle motion"',MAIN)
  self.assertIn('kVehicleLocationAction.contains',MAIN)
 def test_altimeter_metric_geometry_fit_clean_redraw_and_static_design(self):
  metric=METRICS['roles']['AltimeterMetric']; self.assertEqual((metric['weight'],metric['size']),('Bold',92))
  advances=metric['advances']; width=lambda s:sum(advances[ord(c)-32] for c in s)
  bounds=(36,196,468,150)
  for value in ('0','999','1,245','12,345','--'):
   self.assertLessEqual(width(value),bounds[2],value)
   self.assertLessEqual(metric['line_height'],bounds[3],value)
  alt=PAGES[PAGES.index('void altimeter('):PAGES.index('String lastTimeSync(')]
  clear='epd_fill_rect({kAltimeterMetricBounds.x,kAltimeterMetricBounds.y,'
  draw='centeredText(fb,kAltimeterMetricBounds,gpsElevationValue(s.location,false),'
  self.assertIn(clear,alt); self.assertIn(draw,alt); self.assertLess(alt.index(clear),alt.index(draw))
  for label in ('GPS-BASED ELEVATION','GPS FIXED','SATELLITES','HDOP QUALITY','FRESHNESS','BACK'):
   self.assertIn(label,PAGES+txt('src/location/gps_manager.cpp'))
  self.assertNotRegex(alt.lower(),r'animate|graph|gauge|delay\(|while\s*\(')
  self.assertIn('kAltimeterUnitAction.contains',MAIN)
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
  self.assertGreaterEqual(SPEC['touch']['clean_release_ms'],100); self.assertLessEqual(SPEC['touch']['clean_release_ms'],200)
  self.assertGreaterEqual(SPEC['touch']['post_refresh_quiet_ms'],100); self.assertLessEqual(SPEC['touch']['post_refresh_quiet_ms'],200)
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
 def test_display_coordinator_is_single_owner_and_render_latch_is_bounded(self):
  header=txt('src/ui/ui_controller.h')
  self.assertIn('class DisplayCoordinator',header)
  self.assertIn('enum class RenderPriority',header)
  self.assertIn('pendingRender_ = RenderPriority::Cosmetic',header)
  self.assertNotRegex(header,r'Queue|vector|deque|list<')
  self.assertIn('requestRender(RenderPriority::Navigation)',CONTROLLER)
  self.assertIn('pendingRender_ = RenderPriority::None',CONTROLLER)
  self.assertIn('requestRender(renderingPriority)',CONTROLLER)
  sources=list((ROOT/'src').rglob('*.cpp'))
  physical=re.compile(r'epd_(?:init|set_vcom|set_rotation|poweron|poweroff|fullclear|hl_init|hl_get_framebuffer|hl_update_screen)')
  owners=[path for path in sources if physical.search(path.read_text())]
  self.assertEqual(owners,[ROOT/'src/ui/ui_controller.cpp'])
  for worker in ('src/telemetry/telemetry_manager.cpp','src/weather/weather_manager.cpp','src/location/gps_manager.cpp','src/battery/battery_manager.cpp'):
   self.assertNotRegex(txt(worker),r'epd_|renderPage|renderIfDirty')
 def test_touch_render_gate_cannot_replay_or_double_navigate(self):
  self.assertIn('if(blocked)',TOUCH)
  blocked=TOUCH[TOUCH.index('if(blocked)'):TOUCH.index('if(!armed_)')]
  self.assertIn('armed_=stableContact_=false',blocked)
  self.assertIn('DISPLAY_UPDATE_ACTIVE',blocked)
  self.assertIn('notifyDisplayUpdateFinished',MAIN)
  self.assertIn('stale_state=DRAINED',TOUCH)
 def test_white_test_guard_partial_off_gc16_and_poweroff(self):
  self.assertIn('whiteTestUsed_ || updating_',CONTROLLER); self.assertIn('"WHITE TEST"',CONTROLLER); self.assertIn('command == "display white-test"',MAIN); self.assertIn('kPartialRefreshEnabled = false',THEME); self.assertNotIn('MODE_DU',CONTROLLER); self.assertGreaterEqual(CONTROLLER.count('MODE_GC16'),2); self.assertGreaterEqual(CONTROLLER.count('epd_poweroff'),2)
 def test_cleanup_schema_and_every_boot_physical_history_policy(self):
  self.assertIn('kDisplayCleanupRevision = 11',MAIN); self.assertIn('bootRecovery && !fullClearUsed_',CONTROLLER); render=CONTROLLER[CONTROLLER.index('bool DisplayCoordinator::renderIfDirty'):CONTROLLER.index('bool DisplayCoordinator::renderWhiteTest')]; self.assertEqual(render.count('epd_fullclear'),1)
  setup=MAIN[MAIN.index('void setup()'):MAIN.index('void loop()')]
  self.assertGreaterEqual(setup.count('bootCleanupPending = true'),2)
  self.assertIn('policy=EVERY_BOOT',setup)
  self.assertIn('usable destination first',setup)
  self.assertIn('kBootRecoveryGraceMs',CONTROLLER)
  self.assertNotIn('bootCleanupPending = storedRevision != kDisplayCleanupRevision',setup)
  self.assertNotIn('revision-gated cleanup',MAIN)
  self.assertIn('manualFullRefresh',CONTROLLER); self.assertIn('renderWhiteTest',CONTROLLER)
  self.assertIn('EPD_ROT_INVERTED_PORTRAIT',CONTROLLER); self.assertIn('epd_set_vcom(1560)',CONTROLLER); self.assertNotIn('.transmit(',MAIN); self.assertIn('hq::kBoard.gpsRx, -1',MAIN)
 def test_startup_renders_usable_page_once_then_starts_workers_in_background(self):
  setup=MAIN[MAIN.index('void setup()'):MAIN.index('void loop()')]
  state=txt('src/ui/ui_state.h')
  for forbidden in ('Page::Startup','StartupStage','void startup(','connectionWindowEnds','percentages represent completed milestones','FULL-SCREEN GC16'):
   self.assertNotIn(forbidden,MAIN+PAGES+state)
  display_branch=setup[setup.index('if (displayCoordinator.begin())'):setup.index('} else {\n    printResult("display", "FAILED"')]
  self.assertEqual(display_branch.count('testDisplay();'),1)
  self.assertNotIn('telemetryManager.begin()',display_branch)
  self.assertNotIn('weatherManager.begin()',display_branch)
  self.assertNotRegex(display_branch,r'while\s*\(|delay\s*\(')
  self.assertIn('initial_gc16_count=%lu',display_branch); self.assertIn('initial_fullclear_count=%u',display_branch)
  self.assertIn('if (initialPage != ui::Page::Home) displayCoordinator.requestPage(initialPage',display_branch)
  loop=MAIN[MAIN.index('void loop()'):]
  touch_poll=loop.index('touchController.poll')
  worker_start=loop.index('startBackgroundWorkers();')
  local_start=loop.index('initializeLocalServices();')
  self.assertLess(touch_poll,worker_start)
  self.assertLess(touch_poll,local_start)
  self.assertIn('if (touchReadyReported)',loop)
  self.assertIn('touch_accept_ready_after_first_render_ms',loop)
  workers=MAIN[MAIN.index('void startBackgroundWorkers()'):MAIN.index('bool requestNamedPage')]
  self.assertIn('telemetryManager.begin()',workers); self.assertIn('weatherManager.begin()',workers)
  self.assertIn('SERVICES startup=BACKGROUND',workers); self.assertIn('navigation_blocking=NO',workers)
  for service in ('identifyI2c()','batteryManager.begin','gpsManager.begin','timeService.begin','lowPowerManager.begin'):
   self.assertIn(service,MAIN[MAIN.index('void initializeLocalServices()'):MAIN.index('bool requestNamedPage')])
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
  self.assertNotIn('utility',SPEC['navigation'])
  all_bounds=SPEC['navigation']['bounds']
  self.assertTrue(all(b[1:]==[864,108,96] for b in SPEC['navigation']['bounds']))
  self.assertTrue(all(a[0]+a[2]==b[0] for a,b in zip(all_bounds,all_bounds[1:])))
  self.assertEqual(all_bounds[-1][0]+all_bounds[-1][2],SPEC['canvas']['width'])
  self.assertTrue(all(b[2]>=SPEC['geometry']['minimum_touch_target'] and b[3]>=SPEC['geometry']['minimum_touch_target'] for b in all_bounds))
  self.assertIn('five equal navigation tiles must span the full canvas',COMPONENTS)
  self.assertNotIn('globalRefreshControl',COMPONENTS)
  self.assertIn('FontRole::Navigation',COMPONENTS); self.assertIn('active?kPaper:kInk',COMPONENTS)
 def test_primary_palette_and_required_labels_never_pale(self):
  self.assertNotIn('opacity',COMPONENTS.lower()+PAGES.lower()); self.assertNotIn('alpha',COMPONENTS.lower()+PAGES.lower())
  for label in ('supportFORGE','SETUP REQUIRED','SYSTEMS','RADIO','LOCATION','DEVICE','UI QUAL 3'): self.assertIn(label,PAGES+COMPONENTS+MAIN)
  app=COMPONENTS[COMPONENTS.index('void appBar('):COMPONENTS.index('void card(')]
  self.assertNotIn('FIELD TERMINAL',app); self.assertIn('FIELD TERMINAL',PAGES)
  self.assertEqual(SPEC['palette']['ink'],0); self.assertEqual(SPEC['palette']['ink_muted'],0)
 def test_dump_is_authoritative_unpowered_and_touch_persistence_preserved(self):
  dump=CONTROLLER[CONTROLLER.index('bool DisplayCoordinator::dumpPackedFramebuffer'):]
  self.assertIn('renderPage(compositionBuffer_, dumpSnapshot)',dump); self.assertIn('Serial.write(compositionBuffer_, kFramebufferBytes)',dump); self.assertNotIn('epd_poweron',dump); self.assertNotIn('epd_hl_update_screen',dump)
  self.assertIn('saveQualification(true)',TOUCH); self.assertIn('mappingVerified()',MAIN)
  setup=MAIN[MAIN.index('void setup()'):MAIN.index('void loop()')]
  self.assertNotIn('resetQualification();',setup)
  self.assertIn('beginConfirmedTouchRecalibration()',MAIN)
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
  for phrase in ('SAFETY CONFIGURATION REQUIRED','TRANSMIT LOCKED','RECEIVE ONLY','COORDINATES","PRIVATE') : self.assertIn(phrase,PAGES)
  self.assertNotIn('.transmit(',MAIN); self.assertIn('hq::kBoard.gpsRx, -1',MAIN); self.assertIn('MODE_GC16',CONTROLLER); self.assertNotIn('MODE_DU',CONTROLLER); self.assertIn('epd_poweroff()',CONTROLLER)
if __name__=='__main__': unittest.main()
