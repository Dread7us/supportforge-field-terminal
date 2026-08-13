#include "ui_pages.h"

#include <epdiy.h>

#include "ui_components.h"

namespace ui {
namespace {

String percent(const telemetry::NumericValue& value) {
  return value.available ? String(value.value, 0) + "%" : "--";
}

String temperature(const telemetry::NumericValue& value, appconfig::TemperatureUnit unit) {
  if (!value.available) return "--";
  return String(telemetry::displayTemperature(value.value, unit), 0) +
         (unit == appconfig::TemperatureUnit::Celsius ? " C" : " F");
}

String age(uint32_t nowMs, uint32_t thenMs) {
  if (!thenMs) return "--";
  const uint32_t seconds = (nowMs - thenMs) / 1000;
  if (seconds < 60) return String(seconds) + " SEC AGO";
  if (seconds < 3600) return String(seconds / 60) + " MIN AGO";
  return String(seconds / 3600) + " HR AGO";
}

String uptime(uint64_t seconds) {
  const uint64_t days = seconds / 86400;
  const uint8_t hours = (seconds % 86400) / 3600;
  return days ? String(static_cast<unsigned long>(days)) + "D " + String(hours) + "H"
              : String(hours) + "H " + String(static_cast<unsigned>((seconds % 3600) / 60)) + "M";
}

String storageSummary(const telemetry::Snapshot& t) {
  if (!t.diskCount) return "--";
  const telemetry::Disk& disk = t.disks[0];
  String name = disk.mount.available ? disk.mount.value : (disk.fs.available ? disk.fs.value : "DISK 1");
  return name + " " + percent(disk.usedPercent);
}

String failureDetail(const telemetry::Snapshot& t) {
  if (t.consecutiveFailedCycles && t.consecutiveFailedCycles < appconfig::kOfflineFailedCycles)
    return String("CHECK ") + t.consecutiveFailedCycles + " OF 3";
  if (t.fetchState == telemetry::FetchState::AuthError) return "CHECK DEVICE TOKEN";
  return t.explicitSystemStatus ? "EXPLICIT SERVICE STATUS" : telemetry::diagnosticName(t.diagnostic);
}

String observed(Presence p) {
  return p == Presence::Observed ? "READY" : (p == Presence::NotPresent ? "NOT PRESENT" : "UNVERIFIED");
}

String batteryStatus(const UiSnapshot& s) {
  return s.batteryPercentAvailable ? String(s.batteryPercent) + "%"
                                   : battery::classificationName(s.batteryClassification);
}

String weatherTemperature(const UiSnapshot& s) {
  if (!s.weather.dataAvailable) return "--";
  const String unit=s.weather.temperatureUnit == weather::TemperatureUnit::Fahrenheit ? " F" : " C";
  String value=String(s.weather.temperatureTenths / 10.0f, 0)+unit;
  if(s.weather.showFeelsLike&&s.weather.feelsLikeAvailable)value+=" / FEELS "+String(s.weather.feelsLikeTenths/10.0f,0)+unit;
  return value;
}

String gpsSpeed(const UiSnapshot& s) {
  if (!s.location.speedValid) return String("-- ") + location::speedUnitName(s.location.speedUnit);
  return String(location::displaySpeed(s.location.speedKmh,s.location.speedUnit),0)+" "+location::speedUnitName(s.location.speedUnit);
}

void actionButton(uint8_t* fb,Rect bounds,const String& label){roundedRect(fb,bounds,10,kPaper,kInk);text(fb,{bounds.x+12,bounds.y+12,bounds.w-24,bounds.h-20},bounds.x+16,bounds.y+42,label,FontRole::CardHeading,kInk);}

String lastTimeSync(const UiSnapshot& s) {
  if (!s.lastSuccessfulTimeSync) return "--";
  tm local{};
  if (!localtime_r(&s.lastSuccessfulTimeSync, &local)) return "--";
  char value[24]{};
  snprintf(value, sizeof(value), "%04d-%02d-%02d %02d:%02d",
           local.tm_year + 1900, local.tm_mon + 1, local.tm_mday,
           local.tm_hour, local.tm_min);
  return String(value);
}

void home(uint8_t* fb,const UiSnapshot& s){
  const telemetry::Snapshot& t=s.telemetry;
  // SETUP REQUIRED remains an explicit high-contrast state through fetchStateName.
  appBar(fb,s); card(fb,contractRect(spec::kHomeCards[0]),"MONITORED HOST",
      t.host.available?t.host.value:"--",telemetry::fetchStateName(t.fetchState));
  statusPill(fb,{44,238,148,28},telemetry::endpointName(t.activeEndpoint),true);
  metricTile(fb,contractRect(spec::kHomeCards[1]),Icon::Systems,"CPU",percent(t.cpuLoad),temperature(t.cpuTemperature,t.displayTemperatureUnit));
  metricTile(fb,contractRect(spec::kHomeCards[2]),Icon::Info,"RAM",percent(t.ramPercent),
             t.ramUsedGb.available&&t.ramTotalGb.available?String(t.ramUsedGb.value,1)+" / "+String(t.ramTotalGb.value,1)+" GB":"--");
  metricTile(fb,contractRect(spec::kHomeCards[3]),Icon::Device,"STORAGE",storageSummary(t),"Primary host disk");
  metricTile(fb,contractRect(spec::kHomeCards[4]),Icon::Info,"INCIDENT",
             t.fetchState==telemetry::FetchState::Offline?"OFFLINE":(t.consecutiveFailedCycles?"CHECK":"CLEAR"),failureDetail(t));
  roundedRect(fb,contractRect(spec::kHomeCards[5]),12,kPaper,kRule);
  text(fb,{44,574,452,30},44,596,"FIELD CONDITIONS",FontRole::Caption,kInkMuted);
  labeledRow(fb,{44,610,452,48},"WEATHER",s.weather.dataAvailable&&s.weather.showCondition?weather::conditionName(s.weather.weatherCode):weather::stateName(s.weather.state));
  labeledRow(fb,{44,658,452,48},"TEMPERATURE",s.weather.showTemperature?weatherTemperature(s):"HIDDEN");
  labeledRow(fb,{44,706,452,48},"CITY",s.weather.showCity&&s.weather.city[0]?String(s.weather.city):"--");
  labeledRow(fb,{44,754,452,48},"TERMINAL BATTERY",batteryStatus(s),false);
}

void systems(uint8_t* fb,const UiSnapshot& s){
  const Rect* c=reinterpret_cast<const Rect*>(spec::kSystemsCards);
  const telemetry::Snapshot& t=s.telemetry;
  appBar(fb,s,"SYSTEMS");
  const char* sections[]={"OVERVIEW","STORAGE 1 OF 2","STORAGE 2 OF 2","NETWORK"};
  const uint8_t section=s.systemsSection%4;
  card(fb,c[0],sections[section],t.host.available?t.host.value:"--",telemetry::fetchStateName(t.fetchState));
  if(section==0){
    labeledRow(fb,{44,266,452,48},"CPU",percent(t.cpuLoad));
    labeledRow(fb,{44,314,452,48},"RAM",percent(t.ramPercent));
    labeledRow(fb,{44,362,452,48},"CPU TEMP",temperature(t.cpuTemperature,t.displayTemperatureUnit));
    labeledRow(fb,{44,410,452,48},"UPTIME",t.uptimeAvailable?uptime(t.uptimeSeconds):"--",false);
    card(fb,c[1],"HOST THERMAL","NVME "+temperature(t.nvmeTemperature,t.displayTemperatureUnit),"Source contract: Fahrenheit");
  }else if(section==1||section==2){
    roundedRect(fb,c[0],12,kPaper,kRule);
    const uint8_t firstDisk=section==1?0:3;
    const uint8_t lastDisk=min<uint8_t>(t.diskCount,firstDisk+3);
    for(uint8_t i=firstDisk;i<lastDisk;++i){
      const telemetry::Disk& d=t.disks[i];
      const String label=d.mount.available?d.mount.value:(d.fs.available?d.fs.value:String("DISK ")+String(i+1));
      const uint8_t row=i-firstDisk;
      labeledRow(fb,{44,174+static_cast<int>(row)*76,452,76},label.c_str(),percent(d.usedPercent),row<2);
    }
    card(fb,c[1],sections[section],String(t.diskCount)+" DISK(S)","Up to 3 disks per touch page");
  }else{
    roundedRect(fb,c[0],12,kPaper,kRule);
    labeledRow(fb,{44,184,452,58},"DOWNLOAD",t.speedTest.down.available?String(t.speedTest.down.value,1)+" MBPS":"--");
    labeledRow(fb,{44,242,452,58},"UPLOAD",t.speedTest.up.available?String(t.speedTest.up.value,1)+" MBPS":"--");
    labeledRow(fb,{44,300,452,58},"PING",t.speedTest.ping.available?String(t.speedTest.ping.value,1)+" MS":"--");
    labeledRow(fb,{44,358,452,58},"STATUS",t.speedTest.status.available?t.speedTest.status.value:"--");
    labeledRow(fb,{44,416,452,48},"PROVIDER",t.speedTest.provider.available?t.speedTest.provider.value:"--",false);
    card(fb,c[1],"DATA FRESHNESS",age(millis(),t.lastSuccessMs),telemetry::endpointName(t.activeEndpoint));
  }
  roundedRect(fb,kSystemsSectionAction,10,kSurfaceStrong,kInk);
  text(fb,{44,754,452,40},44,784,String("NEXT SECTION: ")+sections[(section+1)%4],FontRole::CardHeading,kInk);
}

void radioPage(uint8_t* fb,const UiSnapshot& s){
  const Rect* c=reinterpret_cast<const Rect*>(spec::kRadioCards);
  appBar(fb,s,"RADIO"); card(fb,c[0],"SX1262 RADIO",s.radioListening?"RECEIVE ACTIVE":"RECEIVE ONLY","915 MHz region - transmission disabled");
  statusPill(fb,{44,240,126,30},"TX LOCKED",true);
  roundedRect(fb,c[1],12,kPaper,kRule);
  labeledRow(fb,{44,326,452,52},"REGION","915 MHZ"); labeledRow(fb,{44,378,452,52},"MODE","RECEIVE ONLY");
  labeledRow(fb,{44,430,452,52},"MODULE",observed(s.radio)); labeledRow(fb,{44,482,452,52},"TRANSMIT","LOCKED",false);
  emptyState(fb,c[2],Icon::Radio,"NO MESSAGES","Received LoRa traffic will appear here");
}

void location(uint8_t* fb,const UiSnapshot& s){
  appBar(fb,s,"LOCATION");
  card(fb,{24,128,492,138},"L76K GPS",String("GPS ")+location::stateName(s.location.state),
       s.location.state==location::GpsState::Fixed?"Current receiver fix":"No guessed location or speed");
  roundedRect(fb,{24,282,492,276},12,kPaper,kRule);
  labeledRow(fb,{44,292,452,38},"SPEED",gpsSpeed(s));
  labeledRow(fb,{44,330,452,38},"MOVEMENT",location::movementName(s.location.movement));
  labeledRow(fb,{44,368,452,38},"SATELLITES",s.location.satellitesValid?String(s.location.satellites):"--");
  labeledRow(fb,{44,406,452,38},"HDOP",s.location.hdopValid?String(s.location.hdopHundredths/100.0f,1):"--");
  labeledRow(fb,{44,444,452,38},"FIX AGE",s.location.fixValid?String(s.location.fixAgeMs/1000)+" SEC":"--");
  labeledRow(fb,{44,482,452,38},"WEATHER LOC",s.weather.source==weather::LocationSource::Gps?"GPS AUTO":"AUTO OFF");
  String position="PRIVATE";
  if(s.location.showCoordinates&&s.location.fixValid)position=String(s.location.latitude,4)+", "+String(s.location.longitude,4);
  if(s.location.showCoordinates&&s.location.fixValid)labeledRow(fb,{44,520,452,38},"COORDINATES",position,false);
  else labeledRow(fb,{44,520,452,38},"COORDINATES","PRIVATE",false);
  actionButton(fb,kLocationGpsPowerAction,s.location.state==location::GpsState::Off?"START GPS":"STOP GPS");
  actionButton(fb,kLocationSpeedUnitAction,String("UNIT ")+location::speedUnitName(s.location.speedUnit));
  actionButton(fb,kLocationPrivacyAction,s.location.showCoordinates?"HIDE COORDS":"SHOW COORDS");
  actionButton(fb,kLocationWeatherSetupAction,"WEATHER SETUP");
  actionButton(fb,kLocationWeatherRefreshAction,"REFRESH WEATHER NOW");
}

void device(uint8_t* fb,const UiSnapshot& s){
  const Rect* c=reinterpret_cast<const Rect*>(spec::kDeviceCards);
  appBar(fb,s,"DEVICE");
  metricTile(fb,c[0],Icon::Battery,"FIELD BATTERY",batteryStatus(s),"Field Terminal device");
  metricTile(fb,c[1],Icon::Device,"WI-FI",telemetry::wifiStateName(s.telemetry.wifiState),s.telemetry.rssiAvailable?String(s.telemetry.rssi)+" DBM":"--");
  roundedRect(fb,c[2],12,kPaper,kRule);
  labeledRow(fb,{44,282,452,52},"ENDPOINT",telemetry::endpointName(s.telemetry.activeEndpoint)); labeledRow(fb,{44,334,452,52},"LAST TELEMETRY",age(millis(),s.telemetry.lastSuccessMs));
  labeledRow(fb,{44,386,452,52},"REFRESH INTERVAL","60 SEC"); labeledRow(fb,{44,438,452,64},"TEMPERATURE",s.telemetry.displayTemperatureUnit==appconfig::TemperatureUnit::Celsius?"CELSIUS":"FAHRENHEIT",false);
  roundedRect(fb,kDeviceDiagnosticsAction,10,kSurfaceStrong,kInk); icon(fb,Icon::Device,58,594,28,kInk);
  text(fb,{88,558,390,42},88,590,"HARDWARE DIAGNOSTICS",FontRole::CardHeading,kInk);
  text(fb,{88,596,390,30},88,616,"Safe qualification tools and observed state",FontRole::Caption,kInkMuted);
  roundedRect(fb,kDeviceRefreshAction,10,kSurfaceSoft,kRule); text(fb,{38,664,210,36},38,690,"REFRESH NOW",FontRole::CardHeading,kInk);
  roundedRect(fb,kDeviceTemperatureAction,10,kSurfaceSoft,kRule); text(fb,{292,664,210,36},292,690,"C / F UNIT",FontRole::CardHeading,kInk);
  card(fb,c[5],"DEVICE SETTINGS","TIME / TIMEZONE","Tap to configure");
}

void diagnostics(uint8_t* fb,const UiSnapshot& s){
  const Rect* c=reinterpret_cast<const Rect*>(spec::kDiagnosticsCards);
  appBar(fb,s,"HARDWARE DIAGNOSTICS"); statusPill(fb,{24,122,180,30},"OBSERVED STATUS",true);
  text(fb,{410,122,106,30},410,144,"UI QUAL 3",FontRole::Caption,kInk);
  roundedRect(fb,c[0],12,kPaper,kRule);
  labeledRow(fb,{44,184,452,50},"GT911",observed(s.touch)); labeledRow(fb,{44,234,452,50},"RTC",observed(s.rtc));
  labeledRow(fb,{44,284,452,50},"BQ27220",batteryStatus(s)); labeledRow(fb,{44,334,452,50},"MICROSD",observed(s.storage));
  labeledRow(fb,{44,384,452,50},"L76K",s.gpsFix?"VALID FIX":observed(s.gps)); labeledRow(fb,{44,434,452,50},"SX1262",observed(s.radio));
  labeledRow(fb,{44,484,452,50},"LORA TX","LOCKED"); labeledRow(fb,{44,534,452,50},"TOUCH MAP",s.touchMappingVerified?"VERIFIED":"SERIAL TEST",false);
  roundedRect(fb,kDiagnosticsDisplayCalibrationAction,12,kSurfaceSoft,kInk);
  icon(fb,Icon::Info,64,684,30,kInk);
  text(fb,{96,650,394,34},96,678,"TEXT QUALIFICATION",FontRole::PageHeading,kInk);
  text(fb,{96,690,394,52},96,714,"Pure-black references, real weights, AA and mono",FontRole::Caption,kInkMuted);
  text(fb,{96,738,394,30},96,758,"OPEN PHYSICAL TEST PAGE",FontRole::CardHeading,kInk);
}

void calibrationRuler(uint8_t* fb){
  epd_draw_rect({0,0,kCanvasWidth,kCanvasHeight},kInk,fb);
  epd_draw_rect({19,19,kCanvasWidth-38,kCanvasHeight-38},kRule,fb);
  for(int x=0;x<kCanvasWidth;x+=20){
    const int tick=(x%100==0)?14:8;
    epd_draw_vline(x,0,tick,kInk,fb);
    epd_draw_vline(x,kCanvasHeight-tick,tick,kInk,fb);
  }
  for(int y=0;y<kCanvasHeight;y+=20){
    const int tick=(y%100==0)?14:8;
    epd_draw_hline(0,y,tick,kInk,fb);
    epd_draw_hline(kCanvasWidth-tick,y,tick,kInk,fb);
  }
}

void displayCalibration(uint8_t* fb,const UiSnapshot& s){
  (void)s;
  const Rect content{24,20,492,832};
  text(fb,content,28,52,"supportFORGE",FontRole::Brand,kInk);
  text(fb,content,28,76,"FIELD TERMINAL",FontRole::Body,kInkMuted);
  text(fb,content,28,108,"PANEL GRAYSCALE QUALIFICATION",FontRole::PageHeading,kInk);

  const char* roleNames[]={"CAPTION 12","BODY 16","CARD HEADING 20","PAGE HEADING 24","BRAND 30","METRIC 28"};
  const FontRole roles[]={FontRole::Caption,FontRole::Body,FontRole::CardHeading,FontRole::PageHeading,FontRole::Brand,FontRole::Metric};
  int baseline=132;
  for(int i=0;i<6;++i){
    text(fb,{28,baseline-22,484,34},28,baseline,roleNames[i],roles[i],kInk);
    baseline += (i<1?24:(i<3?28:36));
  }

  roundedRect(fb,{24,316,492,166},8,kPaper,kRule);
  text(fb,{32,322,476,24},32,340,"ABCDEFGHIJKLMNOPQRSTUVWXYZ",FontRole::Caption,kInk);
  text(fb,{32,346,476,24},32,364,"abcdefghijklmnopqrstuvwxyz",FontRole::Caption,kInk);
  text(fb,{32,370,476,24},32,388,"0123456789",FontRole::Body,kInk);
  text(fb,{32,394,476,24},32,412,"! ? . , : ; - _ / \\ ( ) [ ] { } + = @ # $ % & *",FontRole::Caption,kInk);
  text(fb,{32,420,476,22},32,438,"SHORT",FontRole::Caption,kInkMuted);
  text(fb,{32,442,476,22},32,460,"MEDIUM STATUS LABEL",FontRole::Caption,kInkMuted);
  text(fb,{32,462,476,18},32,478,"LONG LABEL FOR CLIPPING AND SPACING VERIFICATION",FontRole::Caption,kInkMuted);

  roundedRect(fb,{24,494,492,94},8,kSurfaceSoft,kRule);
  text(fb,{32,502,476,22},32,520,"LEFT ALIGNED",FontRole::Body,kInk);
  const String center="CENTER ALIGNED";
  text(fb,{32,528,476,22},32+(476-textWidth(center,FontRole::Body))/2,546,center,FontRole::Body,kInk);
  const String right="RIGHT ALIGNED";
  text(fb,{32,554,476,22},508-textWidth(right,FontRole::Body),572,right,FontRole::Body,kInk);

  text(fb,{28,600,484,22},28,618,"SEVEN LABELED GRAYSCALE SWATCHES",FontRole::Caption,kInkMuted);
  // This diagnostic strip is the sole intentional grayscale use in this build.
  const uint8_t shades[]={0xFF,0xCC,0xAA,0x88,0x66,0x33,0x00};
  const char* labels[]={"15 WHITE","12","10","8","6","3","0 BLACK"};
  for(int i=0;i<7;++i){
    const int x=28+i*69;
    epd_fill_rect({x,628,62,46},shades[i],fb);
    epd_draw_rect({x,628,62,46},kInk,fb);
    const int labelX=x+(62-textWidth(labels[i],FontRole::Caption))/2;
    text(fb,{x,676,62,22},max(x,labelX),694,labels[i],FontRole::Caption,kInk);
  }

  roundedRect(fb,{24,710,492,126},8,kPaper,kInk);
  text(fb,{34,718,472,24},34,738,"CARD BOUNDARY / FRAMEBUFFER SAFE AREA",FontRole::Caption,kInk);
  text(fb,{34,746,472,26},34,766,"Baseline: Agjpq 0123 !?",FontRole::Body,kInk);
  text(fb,{34,778,472,34},34,806,"METRIC 0123456789",FontRole::Metric,kInk);
  text(fb,{34,810,472,22},34,828,"20 PX EDGE RULER - OUTER LINE IS FRAMEBUFFER BOUND",FontRole::Caption,kInkMuted);

  bottomNavigation(fb,Page::DisplayCalibration);
  calibrationRuler(fb);
}

void textQualification(uint8_t* fb,const UiSnapshot& s){
  (void)s;
  text(fb,{24,16,492,40},24,48,"TEXT QUALIFICATION",FontRole::PageHeading,kInk);
  text(fb,{24,56,492,28},24,78,"REFERENCE SHAPES AND FINAL-SIZE FONT RASTERS",FontRole::Caption,kInk);
  epd_fill_rect({24,94,492,54},kInk,fb);
  text(fb,{38,98,464,46},38,132,"SOLID BLACK 0x0",FontRole::CardHeading,kPaper);
  epd_fill_rect({24,164,492,3},kInk,fb);
  text(fb,{24,174,492,24},24,194,"3 PX PURE-BLACK LINE - SAME AS GLYPH CORES",FontRole::Caption,kInk);
  struct Sample { const char* label; FontRole role; };
  const Sample samples[]={
    {"CURRENT ATKINSON REGULAR 18 MONO",FontRole::QualificationCurrent},
    {"INTER REGULAR 18 STRONG AA",FontRole::QualificationRegularAa},
    {"INTER MEDIUM 18 STRONG AA",FontRole::QualificationMediumAa},
    {"INTER SEMIBOLD 20 STRONG AA",FontRole::QualificationSemiboldAa},
    {"INTER BOLD 20 MONO",FontRole::QualificationBoldMono},
  };
  int y=218;
  for(const Sample& sample:samples){
    text(fb,{24,y,492,22},24,y+17,sample.label,FontRole::Caption,kInk);
    text(fb,{24,y+24,492,30},24,y+48,"supportFORGE Field Terminal",sample.role,kInk);
    y+=70;
  }
  roundedRect(fb,{24,574,492,70},8,kPaper,kInk);
  text(fb,{38,582,464,26},38,604,"BLACK ON WHITE - INTER SEMIBOLD AA",FontRole::Caption,kInk);
  text(fb,{38,608,464,30},38,632,"supportFORGE Field Terminal",FontRole::QualificationSemiboldAa,kInk);
  epd_fill_rect({24,658,492,70},kInk,fb);
  text(fb,{38,666,464,26},38,688,"WHITE ON BLACK - INTER SEMIBOLD AA",FontRole::Caption,kPaper);
  text(fb,{38,692,464,30},38,716,"supportFORGE Field Terminal",FontRole::QualificationSemiboldAa,kPaper);
  roundedRect(fb,{24,744,492,84},8,kPaper,kInk);
  text(fb,{38,754,464,26},38,776,"METRIC / STATUS SAMPLE",FontRole::Caption,kInk);
  text(fb,{38,784,464,36},38,812,"CPU 42%  RAM 61%  ONLINE",FontRole::CardHeading,kInk);
  bottomNavigation(fb,Page::TextQualification);
}

void touchSetup(uint8_t* fb,const UiSnapshot& s){
  const Rect* c=reinterpret_cast<const Rect*>(spec::kTouchSetupCards);
  roundedRect(fb,c[0],14,kPaper,kInk);
  text(fb,{144,176,252,38},144,202,s.touchSetupReady?"TOUCH READY":"TOUCH SETUP",FontRole::PageHeading,kInk);
  text(fb,{144,214,252,30},144,238,s.touchSetupReady?"Qualification saved":"Tap each target",FontRole::Body,kInk);
  const String progress=s.touchSetupReady?"CORNER 4 OF 4":String("CORNER ")+String(min(static_cast<int>(s.touchSetupStep)+1,4))+" OF 4";
  text(fb,{144,252,252,32},144,276,progress,FontRole::CardHeading,kInk);
  struct SetupPoint { int x; int y; };
  constexpr int inset=spec::kTouchTargetInset;
  const SetupPoint corners[]={{inset,inset},{kCanvasWidth-1-inset,inset},
                              {inset,kCanvasHeight-1-inset},{kCanvasWidth-1-inset,kCanvasHeight-1-inset}};
  const char* labels[]={"TOP LEFT","TOP RIGHT","BOTTOM LEFT","BOTTOM RIGHT"};
  const int i=s.touchSetupReady?3:min(static_cast<int>(s.touchSetupStep),3);
  const SetupPoint target=corners[i];
  for(int r=spec::kTouchTargetRadius-3;r<=spec::kTouchTargetRadius;++r) circle(fb,target.x,target.y,r,kInk);
  epd_fill_rect({target.x-3,target.y-spec::kTouchTargetRadius+8,7,2*spec::kTouchTargetRadius-16},kInk,fb);
  epd_fill_rect({target.x-spec::kTouchTargetRadius+8,target.y-3,2*spec::kTouchTargetRadius-16,7},kInk,fb);
  epd_fill_rect({target.x-7,target.y-7,15,15},kInk,fb);
  const int labelWidth=textWidth(labels[i],FontRole::CardHeading);
  const int labelX=max(28,min(kCanvasWidth-28-labelWidth,target.x-labelWidth/2));
  const int labelY=target.y < kCanvasHeight/2 ? target.y+76 : target.y-66;
  text(fb,{24,labelY-24,492,32},labelX,labelY,labels[i],FontRole::CardHeading,kInk);
  roundedRect(fb,c[1],8,kPaper,kInk);
  const String build="UI QUAL 3";
  text(fb,{c[1].x+16,c[1].y+16,c[1].w-32,c[1].h-32},
       c[1].x+(c[1].w-textWidth(build,FontRole::PageHeading))/2,c[1].y+54,
       build,FontRole::PageHeading,kInk);
  roundedRect(fb,c[2],8,kPaper,kInk);
  text(fb,{c[2].x+16,c[2].y+12,c[2].w-32,28},c[2].x+22,c[2].y+34,
       "Touch navigation",FontRole::Body,kInk);
  text(fb,{c[2].x+16,c[2].y+40,c[2].w-32,28},c[2].x+22,c[2].y+62,
       "unlocks after setup",FontRole::Body,kInk);
  text(fb,{c[2].x+16,c[2].y+78,c[2].w-32,28},c[2].x+22,c[2].y+100,
       s.touchSetupReady?"QUALIFIED":"WAIT FOR ARMED",FontRole::Caption,kInk);
}

void settings(uint8_t* fb,const UiSnapshot& s){
  const Rect* c=reinterpret_cast<const Rect*>(spec::kSettingsCards);
  appBar(fb,s,"SETTINGS");
  card(fb,c[0],"DEVICE SETTINGS","LOCAL TIME",device_time::syncStateName(s.timeSyncState));
  card(fb,c[1],"TIMEZONE",device_time::timezoneLabel(s.timezoneIndex),"Tap to select next");
  card(fb,c[2],"CLOCK FORMAT",s.use24Hour?"24 HOUR":"12 HOUR","Tap to toggle");
  card(fb,c[3],"LAST TIME SYNC",lastTimeSync(s),s.lastSuccessfulTimeSync?"Local display time":"No successful NTP sync");
  card(fb,{24,642,492,82},"SYNC STATUS",device_time::syncStateName(s.timeSyncState),"Tap to request NTP");
  actionButton(fb,kSettingsWeatherAction,"WEATHER SETUP & DISPLAY");
}

void weatherSetup(uint8_t* fb,const UiSnapshot& s){
  const weather::WizardSnapshot&w=s.weatherWizard;
  clear(fb);text(fb,{24,20,492,54},24,52,"WEATHER SETUP",FontRole::PageHeading,kInk);
  actionButton(fb,{24,112,112,56},"BACK");
  if(w.step==weather::WizardStep::Choice){
    text(fb,{24,74,492,30},24,96,"WEATHER LOCATION",FontRole::CardHeading,kInk);
    actionButton(fb,{150,112,366,56},"DISPLAY PREFERENCES");
    const char* labels[]={"USE GPS","SEARCH CITY","ZIP / POSTAL","ENTER COORDINATES","DISABLE WEATHER"};
    for(int i=0;i<5;++i)actionButton(fb,{24,176+i*118,492,110},labels[i]);
  }else if(w.step==weather::WizardStep::Gps){
    card(fb,{24,190,492,220},"USE GPS","WEATHER FOLLOWS THIS TERMINAL",String("GPS ")+location::stateName(s.location.state));
    text(fb,{44,430,452,80},44,458,"A VALID CURRENT FIX IS REQUIRED. COORDINATES STAY HIDDEN.",FontRole::Body,kInk);
    actionButton(fb,{24,780,238,64},"CANCEL");actionButton(fb,{278,780,238,64},"CONTINUE");
  }else if(w.step==weather::WizardStep::Input){
    const char* value=w.inputKind==weather::InputKind::Latitude?w.latitude:(w.inputKind==weather::InputKind::Longitude?w.longitude:w.input);
    card(fb,{24,176,492,50},weather::inputKindName(w.inputKind),value[0]?value:"--",
         w.inputKind==weather::InputKind::Postal?String("COUNTRY ")+w.country:"");
    const char* chars=weather::pickerCharacters(w.inputKind,w.characterPage);
    for(int row=0;row<5;++row)for(int col=0;col<6;++col){Rect key{24+col*82,238+row*84,78,76};roundedRect(fb,key,8,kPaper,kInk);char label[2]{chars[row*6+col],0};if(label[0]!=' ')text(fb,key,key.x+26,key.y+48,label,FontRole::CardHeading,kInk);}
    actionButton(fb,{24,682,150,64},"PAGE");actionButton(fb,{195,682,150,64},"DELETE");actionButton(fb,{366,682,150,64},w.inputKind==weather::InputKind::Postal?String("COUNTRY ")+w.country:"SPACE");
    actionButton(fb,{24,780,238,64},"CANCEL");actionButton(fb,{278,780,238,64},w.inputKind==weather::InputKind::Longitude?"CONFIRM":"NEXT");
  }else if(w.step==weather::WizardStep::Results){
    text(fb,{24,178,492,34},24,202,s.weather.searchState==weather::SearchState::Pending?"SEARCHING...":"SELECT A RESULT",FontRole::CardHeading,kInk);
    for(uint8_t i=0;i<s.weather.resultCount&&i<5;++i)actionButton(fb,{24,220+i*100,492,88},s.weather.results[i].label);
    actionButton(fb,{24,780,238,64},"CANCEL");
  }else if(w.step==weather::WizardStep::Confirm){
    String source=weather::sourceName(w.pendingSource);String label=source;
    if((w.pendingSource==weather::LocationSource::City||w.pendingSource==weather::LocationSource::Postal)&&w.selectedResult<s.weather.resultCount)label=s.weather.results[w.selectedResult].label;
    card(fb,{24,190,492,220},"CONFIRM LOCATION",label,String("SOURCE ")+source);
    text(fb,{44,438,452,100},44,466,w.pendingSource==weather::LocationSource::Manual?"COORDINATES WILL BE SENT TO THE WEATHER PROVIDER.":"NO WEATHER REQUEST UNTIL YOU SAVE.",FontRole::Body,kInk);
    actionButton(fb,{24,780,238,64},"CANCEL");actionButton(fb,{278,780,238,64},"SAVE & FETCH");
  }else{
    text(fb,{24,178,492,34},24,202,"WEATHER DISPLAY",FontRole::CardHeading,kInk);
    actionButton(fb,{24,220,492,86},s.weather.temperatureUnit==weather::TemperatureUnit::Celsius?"TEMPERATURE CELSIUS":"TEMPERATURE FAHRENHEIT");
    actionButton(fb,{24,322,492,86},s.weather.showTemperature?"CURRENT TEMP ON":"CURRENT TEMP OFF");
    actionButton(fb,{24,424,492,86},s.weather.showCondition?"CONDITION ON":"CONDITION OFF");
    actionButton(fb,{24,526,492,86},s.weather.showCity?"CITY LABEL ON":"CITY LABEL OFF");
    actionButton(fb,{24,628,492,86},s.weather.showFeelsLike?"FEELS-LIKE ON":"FEELS-LIKE OFF");
    actionButton(fb,{24,780,238,64},"DONE");
  }
  if(w.error[0])text(fb,{24,746,492,28},24,770,w.error,FontRole::Caption,kInk);
}

}  // namespace

void renderPage(uint8_t* fb,const UiSnapshot& s){
  clear(fb);
  switch(s.page){case Page::Home:home(fb,s);break;case Page::Systems:systems(fb,s);break;case Page::Radio:radioPage(fb,s);break;case Page::Location:location(fb,s);break;case Page::Device:device(fb,s);break;case Page::Diagnostics:diagnostics(fb,s);break;case Page::DisplayCalibration:displayCalibration(fb,s);break;case Page::TextQualification:textQualification(fb,s);break;case Page::Settings:settings(fb,s);break;case Page::TouchSetup:touchSetup(fb,s);break;case Page::WeatherSetup:weatherSetup(fb,s);break;}
  if(s.page!=Page::TouchSetup&&s.page!=Page::DisplayCalibration&&s.page!=Page::TextQualification&&s.page!=Page::WeatherSetup) bottomNavigation(fb,s.page);
}

}  // namespace ui