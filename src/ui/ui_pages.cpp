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
  if (!s.batteryPercentAvailable) return battery::stateName(s.batteryState);
  const String percent = String(s.batteryPercent) + "%";
  if (s.batteryState == battery::State::Charging) return percent + " CHARGING";
  if (s.batteryState == battery::State::Full) return percent + " FULL";
  if (s.batteryState == battery::State::Verifying) return percent + " VERIFYING";
  if (s.batteryState == battery::State::Stale) return percent + " STALE";
  return percent;
}

const char* batterySocFreshness(const UiSnapshot& s) {
  if (s.batteryPercentAvailable && s.batterySampleValid) return "LIVE";
  if (s.batteryPercentAvailable) return "LKG";
  return "STALE";
}

const char* batteryChargeInterpretation(const UiSnapshot& s) {
  if (!s.batteryChargeStatusVerified) return "VERIFICATION NEEDED";
  return battery::chargePhaseName(s.batteryChargePhase);
}

String batteryElectrical(const UiSnapshot& s) {
  if (!s.batteryVoltageAvailable && !s.batteryCurrentAvailable) return "--";
  String result = s.batteryVoltageAvailable ? String(s.batteryVoltageMillivolts) + " MV" : "-- MV";
  result += " / ";
  result += s.batteryCurrentAvailable ? String(s.batteryAverageCurrentMilliamps) + " MA" : "-- MA";
  return result;
}

String batteryCapacity(const UiSnapshot& s) {
  return s.batteryCapacityAvailable ? String(s.batteryRemainingCapacityMah) + " / " +
      String(s.batteryFullChargeCapacityMah) + " MAH" : "--";
}

String weatherTemperature(const UiSnapshot& s) {
  if (!s.weather.dataAvailable) return "--";
  const String unit=s.weather.temperatureUnit == weather::TemperatureUnit::Fahrenheit ? " F" : " C";
  String value=String(s.weather.temperatureTenths / 10.0f, 0)+unit;
  if(s.weather.showFeelsLike&&s.weather.feelsLikeAvailable)value+=" / FEELS "+String(s.weather.feelsLikeTenths/10.0f,0)+unit;
  return value;
}

String weatherLocation(const weather::Snapshot& w) {
  if (w.city[0] && strcmp(w.city,"GPS LOCATION") && strcmp(w.city,"MANUAL LOCATION")) {
    return String(w.city) + (w.region[0] ? String(", ") + w.region : "");
  }
  if (w.source == weather::LocationSource::Postal && w.postal[0]) return String("ZIP ") + w.postal;
  if (w.source == weather::LocationSource::Gps) return "LOCATING CITY";
  if (w.source == weather::LocationSource::Manual) return "MANUAL LOCATION";
  return w.configured ? weather::sourceName(w.source) : "WEATHER SETUP";
}

String weatherLocationSecondary(const weather::Snapshot& w) {
  if (w.source == weather::LocationSource::Postal && w.postal[0]) return String("ZIP ") + w.postal;
  if (w.country[0]) return w.country;
  return weather::sourceName(w.source);
}

String weatherValue(bool available, int value, const char* suffix) {
  return available ? String(value) + suffix : "--";
}

String homeTime(const UiSnapshot& s) {
  if (!s.rtcValid) return "--:--";
  uint8_t hour=s.hour;
  String suffix;
  if(!s.use24Hour){suffix=s.hour>=12?" PM":" AM";hour=s.hour%12;if(!hour)hour=12;}
  return String((s.use24Hour&&hour<10)?"0":"")+hour+":"+
      (s.minute<10?"0":"")+s.minute+suffix;
}

String homeDate(const UiSnapshot& s) {
  if(!s.rtcValid||s.month<1||s.month>12)return "LOCAL DATE UNAVAILABLE";
  static const char* months[]={"JANUARY","FEBRUARY","MARCH","APRIL","MAY","JUNE",
      "JULY","AUGUST","SEPTEMBER","OCTOBER","NOVEMBER","DECEMBER"};
  return String(months[s.month-1])+" "+String(s.day)+", "+String(s.year);
}

String homeWeatherPrimary(const weather::Snapshot& w) {
  if(!w.dataAvailable)return weather::stateName(w.state);
  const String unit=w.temperatureUnit==weather::TemperatureUnit::Fahrenheit?" F":" C";
  return String(weather::conditionName(w.weatherCode))+"  "+String(w.temperatureTenths/10.0f,0)+unit;
}

String homeWeatherSecondary(const weather::Snapshot& w) {
  const String unit=w.temperatureUnit==weather::TemperatureUnit::Fahrenheit?" F":" C";
  if(w.dataAvailable&&w.feelsLikeAvailable)return String("FEELS LIKE ")+String(w.feelsLikeTenths/10.0f,0)+unit;
  if(w.dataAvailable&&w.highLowAvailable)return String("HIGH ")+String(w.highTenths/10.0f,0)+unit+" / LOW "+String(w.lowTenths/10.0f,0)+unit;
  if(w.dataAvailable&&w.windAvailable)return String("WIND ")+String(w.windSpeedTenths/10.0f,1)+" MPH";
  if(w.dataAvailable&&w.humidityAvailable)return String("HUMIDITY ")+String(w.humidityPercent)+"%";
  return w.configured?"LIVE WEATHER UNAVAILABLE":"TAP FOR WEATHER SETUP";
}

String homeWifiDetail(const network::Snapshot& wifi) {
  if(wifi.state!=network::State::Connected)return wifi.userConfigured?"SAVED NETWORK OFFLINE":"NO NETWORK CONFIGURED";
  if(wifi.rssiAvailable)return String("SIGNAL ")+String(wifi.rssi)+" DBM";
  return wifi.ssidAvailable?String("NETWORK ")+wifi.ssid:"CONNECTED";
}

String gpsSpeed(const UiSnapshot& s) {
  if (!s.location.speedValid) return String("-- ") + location::speedUnitName(s.location.speedUnit);
  return String(location::displaySpeed(s.location.speedKmh,s.location.speedUnit),0)+" "+location::speedUnitName(s.location.speedUnit);
}

String wholeNumber(double value) {
  char raw[24]{};
  snprintf(raw,sizeof(raw),"%ld",static_cast<long>(lround(value)));
  String digits(raw), grouped;
  const int sign=digits.startsWith("-")?1:0;
  if(sign){grouped="-";digits.remove(0,1);}
  for(unsigned i=0;i<digits.length();++i){
    if(i&&((digits.length()-i)%3==0))grouped+=",";
    grouped+=digits[i];
  }
  return grouped;
}

String gpsElevationValue(const location::Snapshot& gps,bool includeUnit=true) {
  const String unit=location::elevationUnitName(gps.elevationUnit);
  if(!gps.altitudeValid)return includeUnit?String("-- ")+unit:"--";
  const String value=wholeNumber(location::displayElevation(gps.altitudeMetres,gps.elevationUnit));
  return includeUnit?value+" "+unit:value;
}

String elevationFreshness(const location::Snapshot& gps){
  if(!gps.altitudeValid)return location::elevationStatusName(gps);
  const uint32_t ageMs=max(gps.fixAgeMs,gps.altitudeAgeMs);
  return String("FIX AGE ")+String(ageMs/1000)+" SEC";
}

String hdopQuality(const location::Snapshot& gps){
  if(!gps.hdopValid)return "--";
  const char* bucket=gps.hdopHundredths<=200?"GOOD":(gps.hdopHundredths<=500?"FAIR":"POOR");
  return String(bucket)+" / "+String(gps.hdopHundredths/100.0f,1);
}

void centeredText(uint8_t* fb,Rect bounds,const String& value,FontRole role,uint8_t color=kInk){
  const String shown=fittedText(value,role,bounds.w);
  const int x=bounds.x+(bounds.w-textWidth(shown,role))/2;
  text(fb,bounds,max(bounds.x,x),centeredBaseline(bounds,role),shown,role,color);
}

void detailBack(uint8_t* fb){ actionButton(fb,kDetailBackAction,"BACK",false,Icon::ChevronLeft,true); }

String fixAge(const location::Snapshot& gps){
  return gps.fixAgeMs==UINT32_MAX?"--":String(gps.fixAgeMs/1000)+" SEC";
}

void compassRose(uint8_t* fb,Rect b,const location::Snapshot& gps){
  roundedRect(fb,b,14,kPaper,kInk);
  const int cx=b.x+b.w/2,cy=b.y+b.h/2+6,r=min(b.w,b.h)/2-34;
  epd_draw_circle(cx,cy,r,kInk,fb);epd_draw_circle(cx,cy,r-2,kInk,fb);
  text(fb,{cx-12,cy-r-26,24,22},cx-6,cy-r-8,"N",FontRole::Caption,kInk);
  epd_fill_rect({cx-3,cy-r-9,7,18},kInk,fb);
  epd_draw_hline(cx-r,cy,14,kInk,fb);epd_draw_hline(cx+r-14,cy,14,kInk,fb);
  epd_draw_vline(cx,cy+r-14,14,kInk,fb);
  if(!gps.courseValid){
    centeredText(fb,{b.x+18,cy-24,b.w-36,48},"HEADING UNAVAILABLE",FontRole::Body);
    return;
  }
  const double radians=(gps.courseDegrees-90.0)*PI/180.0;
  const int tipX=cx+static_cast<int>(cos(radians)*(r-16));
  const int tipY=cy+static_cast<int>(sin(radians)*(r-16));
  epd_draw_line(cx,cy,tipX,tipY,kInk,fb);
  epd_fill_circle(tipX,tipY,7,kInk,fb);epd_fill_circle(cx,cy,5,kInk,fb);
}

void weatherConditionIcon(uint8_t* fb, int cx, int cy, uint8_t code) {
  const bool clear = code == 0;
  const bool snow = code >= 71 && code <= 77;
  const bool storm = code >= 95 && code <= 99;
  const bool wet = (code >= 51 && code <= 67) || (code >= 80 && code <= 82) || storm;
  if (clear) {
    epd_draw_circle(cx,cy,20,kInk,fb);epd_fill_circle(cx,cy,13,kInk,fb);
    for(int i=0;i<8;++i){const double a=i*PI/4;epd_draw_line(cx+static_cast<int>(cos(a)*27),cy+static_cast<int>(sin(a)*27),cx+static_cast<int>(cos(a)*38),cy+static_cast<int>(sin(a)*38),kInk,fb);}
    return;
  }
  epd_fill_circle(cx-18,cy,18,kInk,fb);epd_fill_circle(cx+4,cy-10,24,kInk,fb);
  epd_fill_circle(cx+28,cy+2,16,kInk,fb);epd_fill_rect({cx-36,cy,76,22},kInk,fb);
  if (storm) { epd_draw_line(cx+2,cy+26,cx-8,cy+48,kInk,fb);epd_draw_line(cx-8,cy+48,cx+4,cy+48,kInk,fb);epd_draw_line(cx+4,cy+48,cx-6,cy+68,kInk,fb); }
  else if (snow) { for(int x=-22;x<=22;x+=22){epd_draw_line(cx+x-6,cy+36,cx+x+6,cy+48,kInk,fb);epd_draw_line(cx+x+6,cy+36,cx+x-6,cy+48,kInk,fb);} }
  else if (wet) { for(int x=-20;x<=20;x+=20) epd_draw_line(cx+x,cy+30,cx+x-7,cy+50,kInk,fb); }
}

void systemHealth(uint8_t* fb,const UiSnapshot& s){
  appBar(fb,s,"SYSTEM HEALTH");
  centeredText(fb,{24,132,492,88},s.telemetry.fetchState==telemetry::FetchState::Online?"ONLINE":telemetry::fetchStateName(s.telemetry.fetchState),FontRole::PageHeading);
  statusPill(fb,{120,224,300,42},s.telemetry.fetchState==telemetry::FetchState::Online?"HOST REPORTING":"ATTENTION REQUIRED",true);
  roundedRect(fb,{24,292,492,360},14,kPaper,kInk);
  labeledRow(fb,{48,314,444,68},"HOST",s.telemetry.host.available?s.telemetry.host.value:"--");
  labeledRow(fb,{48,382,444,68},"GUARDIAN",telemetry::fetchStateName(s.telemetry.fetchState));
  labeledRow(fb,{48,450,444,68},"INCIDENT",s.telemetry.consecutiveFailedCycles?"CHECK":"CLEAR");
  labeledRow(fb,{48,518,444,68},"FRESHNESS",age(millis(),s.telemetry.lastSuccessMs));
  labeledRow(fb,{48,586,444,48},"UPTIME",s.telemetry.uptimeAvailable?uptime(s.telemetry.uptimeSeconds):"--",false);
  detailBack(fb);
}

void systemMetrics(uint8_t* fb,const UiSnapshot& s){
  appBar(fb,s,"SYSTEM METRICS");
  centeredText(fb,{24,128,492,86},percent(s.telemetry.cpuLoad),FontRole::PageHeading);
  centeredText(fb,{24,204,492,34},"CPU LOAD",FontRole::CardHeading);
  roundedRect(fb,{24,266,492,390},14,kPaper,kInk);
  labeledRow(fb,{48,292,444,72},"RAM",percent(s.telemetry.ramPercent));
  labeledRow(fb,{48,364,444,72},"CPU TEMP",temperature(s.telemetry.cpuTemperature,s.telemetry.displayTemperatureUnit));
  labeledRow(fb,{48,436,444,72},"NVME TEMP",temperature(s.telemetry.nvmeTemperature,s.telemetry.displayTemperatureUnit));
  labeledRow(fb,{48,508,444,72},"UPTIME",s.telemetry.uptimeAvailable?uptime(s.telemetry.uptimeSeconds):"--");
  labeledRow(fb,{48,580,444,52},"SOURCE","GUARDIAN",false);
  detailBack(fb);
}

void storageDetail(uint8_t* fb,const UiSnapshot& s){
  appBar(fb,s,"STORAGE");
  centeredText(fb,{24,130,492,82},s.telemetry.diskCount?String(s.telemetry.diskCount):"--",FontRole::PageHeading);
  centeredText(fb,{24,204,492,32},"FILESYSTEMS",FontRole::CardHeading);
  roundedRect(fb,{24,264,492,412},14,kPaper,kInk);
  if(!s.telemetry.diskCount)centeredText(fb,{48,390,444,60},"STORAGE UNAVAILABLE",FontRole::CardHeading);
  for(uint8_t i=0;i<min<uint8_t>(s.telemetry.diskCount,5);++i){
    const telemetry::Disk& d=s.telemetry.disks[i];
    const String label=d.mount.available?d.mount.value:(d.fs.available?d.fs.value:String("DISK ")+String(i+1));
    labeledRow(fb,{48,286+static_cast<int>(i)*70,444,70},label.c_str(),percent(d.usedPercent),i<4);
  }
  detailBack(fb);
}

void networkDetail(uint8_t* fb,const UiSnapshot& s){
  appBar(fb,s,"NETWORK");
  centeredText(fb,{24,130,492,82},telemetry::wifiStateName(s.telemetry.wifiState),FontRole::PageHeading);
  centeredText(fb,{24,204,492,32},"WI-FI",FontRole::CardHeading);
  roundedRect(fb,{24,264,492,412},14,kPaper,kInk);
  labeledRow(fb,{48,286,444,70},"DOWNLOAD",s.telemetry.speedTest.down.available?String(s.telemetry.speedTest.down.value,1)+" MBPS":"--");
  labeledRow(fb,{48,356,444,70},"UPLOAD",s.telemetry.speedTest.up.available?String(s.telemetry.speedTest.up.value,1)+" MBPS":"--");
  labeledRow(fb,{48,426,444,70},"PING",s.telemetry.speedTest.ping.available?String(s.telemetry.speedTest.ping.value,1)+" MS":"--");
  labeledRow(fb,{48,496,444,70},"SIGNAL",s.telemetry.rssiAvailable?String(s.telemetry.rssi)+" DBM":"--");
  labeledRow(fb,{48,566,444,70},"TEST STATUS",s.telemetry.speedTest.status.available?s.telemetry.speedTest.status.value:"--",false);
  detailBack(fb);
}

void weatherDetail(uint8_t* fb,const UiSnapshot& s){
  appBar(fb,s,"WEATHER DETAIL");
  centeredText(fb,{24,116,330,92},s.weather.dataAvailable?weatherTemperature(s):"--",FontRole::PageHeading);
  if(s.weather.dataAvailable)weatherConditionIcon(fb,438,160,s.weather.weatherCode);
  centeredText(fb,{24,204,492,34},s.weather.dataAvailable?weather::conditionName(s.weather.weatherCode):weather::stateName(s.weather.state),FontRole::CardHeading);
  roundedRect(fb,{24,252,492,118},14,kPaper,kInk);
  labeledRow(fb,{48,262,444,44},"LOCATION",weatherLocation(s.weather));
  labeledRow(fb,{48,306,444,44},"SOURCE",weather::sourceName(s.weather.source),false);
  text(fb,{48,346,444,20},48,362,fittedText(weatherLocationSecondary(s.weather),FontRole::Caption,444),FontRole::Caption,kInkMuted);
  roundedRect(fb,{24,390,492,238},14,kPaper,kInk);
  const String unit=s.weather.temperatureUnit==weather::TemperatureUnit::Fahrenheit?" F":" C";
  labeledRow(fb,{48,400,444,46},"FEELS LIKE",s.weather.feelsLikeAvailable?String(s.weather.feelsLikeTenths/10.0f,0)+unit:"--");
  labeledRow(fb,{48,446,444,46},"HIGH / LOW",s.weather.highLowAvailable?String(s.weather.highTenths/10.0f,0)+" / "+String(s.weather.lowTenths/10.0f,0)+unit:"--");
  labeledRow(fb,{48,492,444,46},"HUMIDITY",weatherValue(s.weather.humidityAvailable,s.weather.humidityPercent,"%"));
  labeledRow(fb,{48,538,444,46},"WIND",s.weather.windAvailable?String(s.weather.windSpeedTenths/10.0f,1)+" MPH / "+String(s.weather.windDirectionDegrees)+" DEG":"--");
  labeledRow(fb,{48,584,444,36},"PRECIP CHANCE",weatherValue(s.weather.precipitationAvailable,s.weather.precipitationPercent,"%"),false);
  labeledRow(fb,{48,638,444,40},"UPDATED",s.weather.lastSuccessMs?age(millis(),s.weather.lastSuccessMs):"--",false);
  actionButton(fb,kWeatherDetailSetupAction,"WEATHER SETTINGS",false,Icon::Weather);detailBack(fb);
}

void batteryDetail(uint8_t* fb,const UiSnapshot& s){
  appBar(fb,s,"BATTERY");
  const String primary=s.batteryPercentAvailable?String(s.batteryPercent)+"%":"--";
  centeredText(fb,{24,128,492,94},primary,FontRole::PageHeading);
  centeredText(fb,{24,214,492,40},batteryChargeInterpretation(s),FontRole::CardHeading);
  batteryIcon(fb,{90,294,360,124},s.batteryState,s.batteryPercentAvailable,s.batteryPercent,kInk);
  roundedRect(fb,{24,438,492,300},14,kPaper,kInk);
  labeledRow(fb,{48,448,444,38},"PERCENT / QUALITY",primary+" / "+batterySocFreshness(s));
  labeledRow(fb,{48,486,444,38},"GAUGE UPDATE",s.batteryLastSampleMs?age(millis(),s.batteryLastSampleMs):"--");
  labeledRow(fb,{48,524,444,38},"CHARGER INPUT",battery::chargerConnectionName(s.batteryChargerConnection));
  labeledRow(fb,{48,562,444,38},"CHARGE STATE",batteryChargeInterpretation(s));
  labeledRow(fb,{48,600,444,38},"VOLTAGE / CURRENT",batteryElectrical(s));
  labeledRow(fb,{48,638,444,38},"REMAIN / FULL",batteryCapacity(s));
  labeledRow(fb,{48,676,444,48},"EXPLANATION",battery::diagnosisName(s.batteryDiagnosis),false);
  detailBack(fb);
}

void vehicleMotion(uint8_t* fb,const UiSnapshot& s){
  appBar(fb,s,"VEHICLE MOTION");
  const String speed=s.location.speedValid?String(location::displaySpeed(s.location.speedKmh,s.location.speedUnit),0):"--";
  statusPill(fb,kVehicleGpsStateBounds,String("GPS ")+location::stateName(s.location.state),true);
  // Clear the entire dominant metric region on every composition so a shorter
  // value or "--" can never retain pixels from a prior three-digit speed.
  epd_fill_rect({kVehicleSpeedBounds.x,kVehicleSpeedBounds.y,kVehicleSpeedBounds.w,kVehicleSpeedBounds.h},kPaper,fb);
  centeredText(fb,kVehicleSpeedBounds,speed,FontRole::VehicleSpeed,kInk);
  centeredText(fb,kVehicleSpeedUnitBounds,location::speedUnitName(s.location.speedUnit),FontRole::CardHeading);
  centeredText(fb,kVehicleMovementBounds,location::motionStateName(s.location),FontRole::PageHeading);
  compassRose(fb,kVehicleCompassBounds,s.location);
  roundedRect(fb,kVehicleCourseBounds,14,kPaper,kInk);
  centeredText(fb,{316,430,184,38},s.location.courseValid?location::cardinalShort(s.location.courseDegrees):"--",FontRole::PageHeading);
  centeredText(fb,{316,468,184,30},s.location.courseValid?location::cardinalLong(s.location.courseDegrees):"HEADING UNAVAILABLE",FontRole::Body);
  centeredText(fb,{316,504,184,24},s.location.courseValid?String(s.location.courseDegrees,0)+" DEG":"--",FontRole::Caption);
  centeredText(fb,{316,532,184,22},"GPS COURSE",FontRole::Caption);
  centeredText(fb,{316,556,184,20},"NOT MAGNETIC",FontRole::Caption);
  roundedRect(fb,kVehicleQualityBounds,14,kPaper,kInk);
  labeledRow(fb,{48,610,444,44},"GPS ELEVATION",gpsElevationValue(s.location));
  labeledRow(fb,{48,654,444,44},"SATELLITES",s.location.satellitesValid?String(s.location.satellites):"--");
  labeledRow(fb,{48,698,444,44},"FIX AGE",fixAge(s.location),false);
  detailBack(fb);
}

void altimeter(uint8_t* fb,const UiSnapshot& s){
  appBar(fb,s,"GPS ELEVATION");
  centeredText(fb,{24,118,492,36},"GPS-BASED ELEVATION",FontRole::CardHeading);
  roundedRect(fb,{24,166,492,238},16,kPaper,kInk);
  // Explicitly clear and clip the dominant metric on every full-white
  // composition so transitions such as 12,345 -> -- cannot retain stale ink.
  epd_fill_rect({kAltimeterMetricBounds.x,kAltimeterMetricBounds.y,
                 kAltimeterMetricBounds.w,kAltimeterMetricBounds.h},kPaper,fb);
  centeredText(fb,kAltimeterMetricBounds,gpsElevationValue(s.location,false),
               FontRole::AltimeterMetric,kInk);
  centeredText(fb,{36,346,468,38},location::elevationUnitName(s.location.elevationUnit),
               FontRole::PageHeading);
  statusPill(fb,{150,426,240,38},location::elevationStatusName(s.location),true);
  roundedRect(fb,{24,486,492,178},14,kPaper,kInk);
  labeledRow(fb,{48,500,444,48},"SATELLITES",
             s.location.altitudeValid&&s.location.satellitesValid?String(s.location.satellites):"--");
  labeledRow(fb,{48,548,444,48},"HDOP QUALITY",
             s.location.altitudeValid?hdopQuality(s.location):"--");
  labeledRow(fb,{48,596,444,48},"FRESHNESS",elevationFreshness(s.location),false);
  actionButton(fb,kAltimeterUnitAction,
               String("UNIT ")+location::elevationPreferenceName(s.location.elevationUnit),false,Icon::Units);
  detailBack(fb);
}

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
  appBar(fb,s);
  // HOME owns this entire dynamic band. Explicitly erase it before composing
  // the clock, weather, and status regions so shorter values retain no pixels.
  epd_fill_rect({kMargin,kAppBarHeight,kCanvasWidth-2*kMargin,
                 kContentBottom-kAppBarHeight},kPaper,fb);
  const Rect hero=contractRect(spec::kHomeHeroBounds);
  roundedRect(fb,hero,18,kPaper,kInk);
  const Rect clock=contractRect(spec::kHomeClockBounds);
  epd_fill_rect({clock.x,clock.y,clock.w,clock.h},kPaper,fb);
  centeredText(fb,clock,homeTime(s),FontRole::HomeClock);
  centeredText(fb,{40,220,460,30},homeDate(s),FontRole::Body);
  epd_draw_hline(40,254,460,kRule,fb);
  const Rect weatherBounds=contractRect(spec::kHomeWeatherBounds);
  epd_fill_rect({weatherBounds.x,weatherBounds.y,weatherBounds.w,weatherBounds.h},kPaper,fb);
  if(s.weather.dataAvailable)weatherConditionIcon(fb,92,316,s.weather.weatherCode);
  else icon(fb,Icon::Info,92,316,42,kInk);
  centeredText(fb,{148,264,352,28},weatherLocation(s.weather),FontRole::Body);
  centeredText(fb,{148,300,352,34},homeWeatherPrimary(s.weather),FontRole::PageHeading);
  centeredText(fb,{148,342,352,26},homeWeatherSecondary(s.weather),FontRole::Caption);

  metricTile(fb,contractRect(spec::kHomeCards[1]),Icon::Systems,"SUPPORTFORGE / GUARDIAN",
      t.fetchState==telemetry::FetchState::Online?"ONLINE":telemetry::fetchStateName(t.fetchState),
      t.host.available?String("HOST ")+t.host.value:failureDetail(t));
  metricTile(fb,contractRect(spec::kHomeCards[2]),Icon::Systems,"SYSTEM HEALTH",
      String("CPU LOAD ")+percent(t.cpuLoad),String("RAM USED ")+percent(t.ramPercent));
  metricTile(fb,contractRect(spec::kHomeCards[3]),Icon::Wifi,"WI-FI / NETWORK",
      network::stateName(s.wifi.state),homeWifiDetail(s.wifi));
  metricTile(fb,contractRect(spec::kHomeCards[4]),Icon::Battery,"BATTERY PERCENT",
      s.batteryPercentAvailable?String(s.batteryPercent)+"%":"PERCENT UNAVAILABLE",
      String("CHARGE STATE ")+batteryChargeInterpretation(s));
}

void systems(uint8_t* fb,const UiSnapshot& s){
  const Rect* c=reinterpret_cast<const Rect*>(spec::kSystemsCards);
  const telemetry::Snapshot& t=s.telemetry;
  appBar(fb,s,"SYSTEMS");
  const char* sections[]={"OVERVIEW","STORAGE 1 OF 2","STORAGE 2 OF 2","NETWORK"};
  const uint8_t section=s.systemsSection%4;
  card(fb,c[0],sections[section],t.host.available?t.host.value:"--","");
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
  actionButton(fb,kSystemsSectionAction,String("NEXT SECTION: ")+sections[(section+1)%4],false,Icon::Next);
}

void radioPage(uint8_t* fb,const UiSnapshot& s){
  const Rect* c=reinterpret_cast<const Rect*>(spec::kRadioCards);
  appBar(fb,s,"RADIO");
  card(fb,c[0],"RECEIVE ONLY","TRANSMIT LOCKED","SAFETY CONFIGURATION REQUIRED");
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
  labeledRow(fb,{44,406,452,38},"GPS ELEVATION",gpsElevationValue(s.location));
  labeledRow(fb,{44,444,452,38},"FIX AGE",s.location.fixValid?String(s.location.fixAgeMs/1000)+" SEC":"--");
  labeledRow(fb,{44,482,452,38},"ELEVATION STATUS",location::elevationStatusName(s.location));
  String position="PRIVATE";
  if(s.location.showCoordinates&&s.location.fixValid)position=String(s.location.latitude,4)+", "+String(s.location.longitude,4);
  if(s.location.showCoordinates&&s.location.fixValid)labeledRow(fb,{44,520,452,38},"COORDINATES",position,false);
  else labeledRow(fb,{44,520,452,38},"COORDINATES","PRIVATE",false);
  actionButton(fb,kLocationGpsPowerAction,s.location.state==location::GpsState::Off?"START GPS":"STOP GPS",false,Icon::Location);
  actionButton(fb,kLocationSpeedUnitAction,String("UNIT ")+location::speedUnitName(s.location.speedUnit),false,Icon::Units);
  actionButton(fb,kLocationPrivacyAction,s.location.showCoordinates?"HIDE COORDS":"SHOW COORDS",false,Icon::Privacy);
  actionButton(fb,kLocationWeatherSetupAction,"WEATHER SETUP",false,Icon::Weather);
  actionButton(fb,kLocationWeatherRefreshAction,"REFRESH WEATHER NOW",false,Icon::Refresh);
}

void device(uint8_t* fb,const UiSnapshot& s){
  const Rect* c=reinterpret_cast<const Rect*>(spec::kDeviceCards);
  appBar(fb,s,"DEVICE");
  metricTile(fb,c[0],Icon::Battery,"TERMINAL BATTERY",batteryStatus(s),"Terminal power status");
  metricTile(fb,c[1],Icon::Device,"WI-FI",network::stateName(s.wifi.state),s.wifi.rssiAvailable?String(s.wifi.rssi)+" DBM":"--");
  roundedRect(fb,c[2],12,kPaper,kRule);
  labeledRow(fb,{44,282,452,52},"ENDPOINT",telemetry::endpointName(s.telemetry.activeEndpoint)); labeledRow(fb,{44,334,452,52},"LAST TELEMETRY",age(millis(),s.telemetry.lastSuccessMs));
  labeledRow(fb,{44,386,452,52},"REFRESH INTERVAL","60 SEC"); labeledRow(fb,{44,438,452,64},"TEMPERATURE",s.telemetry.displayTemperatureUnit==appconfig::TemperatureUnit::Celsius?"CELSIUS":"FAHRENHEIT",false);
  actionButton(fb,kDeviceDiagnosticsAction,"DIAGNOSTICS",false,Icon::Diagnostics);
  actionButton(fb,kDeviceLowPowerAction,"LOW POWER",false,Icon::Power);
  const String cleanupLabel=s.manualRefreshRateLimited?
      String("DISPLAY CLEANUP AVAILABLE IN ")+String(s.manualRefreshRemainingSeconds)+" SECONDS":
      String("CLEAN DISPLAY");
  actionButton(fb,kDeviceDisplayRefreshAction,cleanupLabel,!s.manualRefreshRateLimited,Icon::Cleanup);
  actionButton(fb,kDeviceSettingsAction,"MAIN SETTINGS",false,Icon::Settings);
}

void diagnostics(uint8_t* fb,const UiSnapshot& s){
  const Rect* c=reinterpret_cast<const Rect*>(spec::kDiagnosticsCards);
  appBar(fb,s,"HARDWARE DIAGNOSTICS"); statusPill(fb,{24,122,180,30},"OBSERVED STATUS",true);
  text(fb,{410,122,106,30},410,144,"UI QUAL 3",FontRole::Caption,kInk);
  roundedRect(fb,c[0],12,kPaper,kRule);
  labeledRow(fb,{44,184,452,50},"GT911",observed(s.touch)); labeledRow(fb,{44,234,452,50},"RTC",observed(s.rtc));
  labeledRow(fb,{44,284,452,50},"BQ27220",batteryStatus(s)); labeledRow(fb,{44,334,452,50},"MICROSD",observed(s.storage));
  labeledRow(fb,{44,384,452,50},"L76K",s.gpsFix?"VALID FIX":observed(s.gps)); labeledRow(fb,{44,434,452,50},"SX1262",observed(s.radio));
  labeledRow(fb,{44,484,452,50},"DISPLAY MODE",refreshModeName(s.refreshMode));
  const String displayTiming=String("GC16 ")+String(s.displayGc16DurationMs)+" / CLEAN "+String(s.displayCleanupDurationMs)+" MS";
  labeledRow(fb,{44,534,452,50},"DISPLAY TIMING",displayTiming,false);
  roundedRect(fb,kDiagnosticsDisplayCalibrationAction,12,kSurfaceSoft,kInk);
  icon(fb,Icon::Info,64,684,30,kInk);
  text(fb,{96,650,394,34},96,678,"TEXT QUALIFICATION",FontRole::PageHeading,kInk);
  text(fb,{96,690,394,52},96,714,"Pure-black references, real weights, AA and mono",FontRole::Caption,kInkMuted);
  text(fb,{96,718,394,30},96,742,"OPEN PHYSICAL TEST PAGE",FontRole::CardHeading,kInk);
  detailBack(fb);
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
  text(fb,{c[0].x+16,c[0].y+16,c[0].w-32,38},c[0].x+16,c[0].y+42,s.touchSetupReady?"TOUCH READY":"TOUCH SETUP",FontRole::PageHeading,kInk);
  text(fb,{c[0].x+16,c[0].y+54,c[0].w-32,30},c[0].x+16,c[0].y+78,s.touchSetupReady?"Qualification saved":"Tap each target",FontRole::Body,kInk);
  const String progress=s.touchSetupReady?"CORNER 4 OF 4":String("CORNER ")+String(min(static_cast<int>(s.touchSetupStep)+1,4))+" OF 4";
  text(fb,{c[0].x+16,c[0].y+92,c[0].w-32,32},c[0].x+16,c[0].y+116,progress,FontRole::CardHeading,kInk);
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
  appBar(fb,s,"MAIN SETTINGS");
  text(fb,{24,78,492,28},24,100,"MAIN SETTINGS",FontRole::PageHeading,kInk);
  const char* labels[]={"WI-FI","DATE & TIME","DISPLAY","UNITS","WEATHER",
      "LOCATION & PRIVACY","LOW POWER","TOUCH","ABOUT / DIAGNOSTICS",
      "CALCULATOR","BACK"};
  const Icon glyphs[]={Icon::Wifi,Icon::Clock,Icon::Display,Icon::Units,Icon::Weather,
      Icon::Privacy,Icon::Power,Icon::Touch,Icon::Diagnostics,Icon::Calculator,Icon::ChevronLeft};
  for(uint8_t i=0;i<11;++i)actionButton(fb,kSettingsCategoryActions[i],labels[i],
      i==0&&s.wifi.state==network::State::SetupRequired,glyphs[i],i==10);
}

void displaySettings(uint8_t* fb,const UiSnapshot& s){
  appBar(fb,s,"DISPLAY");
  text(fb,{24,102,492,42},24,134,"FRONT LIGHT",FontRole::PageHeading,kInk);
  if(!s.frontLight.candidateAvailable){
    card(fb,{24,176,492,198},"HARDWARE STATUS","FRONT LIGHT NOT QUALIFIED",
         "GPIO 11 conflicts with EPD D10. No alternative GPIO will be attempted.");
  }else{
    const power::FrontLightLevel levels[]={power::FrontLightLevel::Off,
        power::FrontLightLevel::Low,power::FrontLightLevel::Medium,power::FrontLightLevel::High};
    for(uint8_t i=0;i<4;++i){
      const bool selected=s.frontLight.preferred==levels[i];
      String label=power::frontLightLevelName(levels[i]);
      if(selected)label+=" - SAVED";
      actionButton(fb,kDisplayFrontLightActions[i],label,selected,Icon::Light);
    }
    const String effective=s.frontLight.lowPowerSuppressed?"OFF - LOW POWER":
        String(power::frontLightLevelName(s.frontLight.effective));
    labeledRow(fb,{48,544,444,64},"CURRENT OUTPUT",effective,false);
    text(fb,{24,620,492,58},24,644,"GPIO 11 CANDIDATE - PHYSICAL CONFIRMATION REQUIRED",
         FontRole::Caption,kInk);
  }
  actionButton(fb,kDisplayRefreshSettingsAction,
               String("REFRESH MODE: ")+refreshModeName(s.refreshMode),false,Icon::Display);
  actionButton(fb,kDisplaySettingsBackAction,"BACK",false,Icon::ChevronLeft,true);
}

void dateTimeSettings(uint8_t* fb,const UiSnapshot& s){
  appBar(fb,s,"DATE & TIME");
  card(fb,{24,112,492,146},"LAST SYNCHRONIZATION",s.rtcValid?lastTimeSync(s):"TIME SYNC REQUIRED",device_time::syncStateName(s.timeSyncState));
  actionButton(fb,kDateTimeTimezoneAction,String("TIMEZONE: ")+device_time::timezoneLabel(s.timezoneIndex),false,Icon::Location);
  actionButton(fb,kDateTimeFormatAction,s.use24Hour?"FORMAT: 24 HOUR":"FORMAT: 12 HOUR",false,Icon::Clock);
  actionButton(fb,kDateTimeSyncAction,"SYNC TIME NOW",false,Icon::Refresh);
  detailBack(fb);
}

void unitsSettings(uint8_t* fb,const UiSnapshot& s){
  appBar(fb,s,"UNITS");
  actionButton(fb,kUnitsTemperatureAction,s.telemetry.displayTemperatureUnit==appconfig::TemperatureUnit::Celsius?"TEMPERATURE: CELSIUS":"TEMPERATURE: FAHRENHEIT",false,Icon::Units);
  actionButton(fb,kUnitsSpeedAction,String("SPEED: ")+location::speedUnitName(s.location.speedUnit),false,Icon::Units);
  actionButton(fb,kUnitsElevationAction,String("ELEVATION: ")+location::elevationPreferenceName(s.location.elevationUnit),false,Icon::Units);
  detailBack(fb);
}

void locationPrivacySettings(uint8_t* fb,const UiSnapshot& s){
  appBar(fb,s,"LOCATION & PRIVACY");
  card(fb,{24,128,492,150},"GPS RECEIVER",location::stateName(s.location.state),"GPS remains receive-only");
  actionButton(fb,kLocationSettingsGpsAction,s.location.state==location::GpsState::Off?"START GPS":"STOP GPS",false,Icon::Location);
  actionButton(fb,kLocationSettingsPrivacyAction,s.location.showCoordinates?"COORDINATES: VISIBLE":"COORDINATES: PRIVATE",s.location.showCoordinates,Icon::Privacy);
  actionButton(fb,kLocationSettingsWeatherAction,"WEATHER LOCATION SETUP",false,Icon::Weather);
  detailBack(fb);
}

void wifiSettings(uint8_t* fb,const UiSnapshot& s){
  appBar(fb,s,"WI-FI");
  card(fb,{24,104,492,126},"CONNECTION",network::stateName(s.wifi.state),
       s.wifi.ssidAvailable?s.wifi.ssid:"NO NETWORK CONFIGURED");
  actionButton(fb,kWifiScanAction,s.wifi.scanState==network::ScanState::Scanning?"SCANNING...":"SCAN NETWORKS",false,Icon::Search);
  actionButton(fb,kWifiManualAction,"MANUAL NETWORK",false,Icon::Keyboard);
  actionButton(fb,kWifiDisconnectAction,"DISCONNECT",false,Icon::Power);
  actionButton(fb,kWifiReconnectAction,"RECONNECT",false,Icon::Refresh);
  actionButton(fb,kWifiForgetAction,s.wifi.userConfigured?"FORGET SAVED NETWORK":"NO USER NETWORK TO FORGET",false,Icon::Delete);
  text(fb,{24,494,492,56},24,520,"First boot requires local setup. Saved credentials stay on this device.",FontRole::Caption,kInk);
  actionButton(fb,kWifiBackAction,"BACK",false,Icon::ChevronLeft,true);
}

void wifiNetworks(uint8_t* fb,const UiSnapshot& s){
  appBar(fb,s,"WI-FI NETWORKS");
  const String scanHeading=s.wifi.scanState==network::ScanState::Scanning?"SCANNING - UI REMAINS ACTIVE":
      (s.wifi.scanState==network::ScanState::Failed?"SCAN FAILED - BACK TO RETRY":
       (s.wifi.scanState==network::ScanState::Empty?"SCAN COMPLETE - NO VISIBLE NETWORKS":"SELECT A NETWORK"));
  text(fb,{24,88,492,34},24,114,scanHeading,FontRole::CardHeading,kInk);
  if(!s.wifi.resultCount&&s.wifi.scanState==network::ScanState::Empty)
    centeredText(fb,{24,300,492,80},"NO VISIBLE NETWORKS - RETRY OR ENTER MANUALLY",FontRole::Body);
  else if(!s.wifi.resultCount&&s.wifi.scanState==network::ScanState::Failed)
    centeredText(fb,{24,300,492,80},"RADIO SCAN FAILED - BACK AND RETRY",FontRole::Body);
  for(uint8_t i=0;i<s.wifi.resultCount&&i<network::kMaximumScanResults;++i){
    const network::ScanResult&r=s.wifi.results[i];
    actionButton(fb,kWifiNetworkActions[i],String(r.secure?"[LOCK] ":"[OPEN] ")+r.ssid+"  "+String(r.rssi)+" DBM",false,r.secure?Icon::Lock:Icon::Wifi);
  }
  actionButton(fb,kWifiBackAction,"BACK",false,Icon::ChevronLeft,true);
}

void wifiEntry(uint8_t* fb,const UiSnapshot& s){
  appBar(fb,s,"WI-FI CREDENTIALS");
  card(fb,{24,92,492,96},"NETWORK NAME",s.wifiEntrySsid&&s.wifiEntrySsid[0]?s.wifiEntrySsid:"ENTER SSID");
  String masked;
  for(uint8_t i=0;i<s.wifiPasswordLength&&i<24;++i)masked+="*";
  card(fb,{24,202,492,96},"PASSWORD",masked.length()?masked:"MASKED ENTRY");
  // Printable ASCII is 95 characters; repeat Space in the final grid slot so
  // all 24 visible keys on every page have a valid matching route character.
  const char* pages[]={"ABCDEFGHIJKLMNOPQRSTUVWX","YZabcdefghijklmnopqrstuv","wxyz0123456789!\"#$%&'()","*+,-./:;<=>?@[\\]^_`{|}~  "};
  const char* chars=pages[s.wifiKeyboardPage%4];
  for(uint8_t i=0;i<24;++i){String key=String(chars[i]);actionButton(fb,kWifiEntryKeys[i],key,s.wifiEditingPassword,Icon::Keyboard);}
  actionButton(fb,kWifiEntryModeAction,s.wifiEditingPassword?"PASSWORD":"SSID",false,Icon::Keyboard);
  actionButton(fb,kWifiEntryDeleteAction,"BACKSPACE",false,Icon::Delete);
  actionButton(fb,kWifiEntryNextAction,"MORE KEYS",false,Icon::Next);
  actionButton(fb,kWifiEntryCancelAction,"CANCEL",false,Icon::Close);
  actionButton(fb,kWifiEntrySaveAction,"SAVE & CONNECT",true,Icon::Save);
}

void wifiForgetConfirm(uint8_t* fb,const UiSnapshot& s){
  appBar(fb,s,"FORGET NETWORK");
  card(fb,{24,176,492,310},"DELIBERATE CONFIRMATION","FORGET USER-SAVED WI-FI?","The terminal will return to SETUP REQUIRED.");
  actionButton(fb,kWifiEntryCancelAction,"CANCEL",false,Icon::Close);
  actionButton(fb,kWifiEntrySaveAction,"CONFIRM FORGET",true,Icon::Delete);
}

void calculatorPage(uint8_t* fb,const UiSnapshot&s){
  text(fb,{24,14,492,44},24,48,"CALCULATOR",FontRole::PageHeading,kInk);
  actionButton(fb,kCalculatorBackAction,"BACK",false,Icon::ChevronLeft,true);
  const Rect display{24,144,492,176};
  epd_fill_rect({display.x,display.y,display.w,display.h},kPaper,fb);
  roundedRect(fb,display,14,kPaper,kInk);
  String shown=s.calculatorDisplay[0]?s.calculatorDisplay:"0";
  while(shown.length()>4&&textWidth(shown,FontRole::PageHeading)>display.w-40)
    shown=String("...")+shown.substring(4);
  const int x=display.x+display.w-20-textWidth(shown,FontRole::PageHeading);
  text(fb,{display.x+20,display.y+20,display.w-40,display.h-40},max(display.x+20,x),centeredBaseline({display.x+20,display.y+20,display.w-40,display.h-40},FontRole::PageHeading),shown,FontRole::PageHeading,kInk);
  const char* labels[]={"7","8","9","/","4","5","6","*","1","2","3","-","0",".","+","=","C","DEL","+/-"};
  for(uint8_t i=0;i<19;++i)actionButton(fb,kCalculatorKeys[i],labels[i],i==15,Icon::Calculator);
}

void displayRefreshMode(uint8_t* fb,const UiSnapshot& s){
  appBar(fb,s,"DISPLAY REFRESH MODE");
  text(fb,{24,112,492,44},24,142,"SELECT QUALITY VERSUS SPEED",FontRole::CardHeading,kInk);
  const char* titles[]={"QUICK","BALANCED","BEAUTIFUL"};
  const char* line1[]={"Physical cleanup on every page change.","Physical cleanup on every page change.","Physical cleanup on every page change."};
  const char* line2[]={"Fastest in-page GC16; no ghosting accepted.","In-page telemetry uses one normal GC16.","Quality priority; transitions remain bounded."};
  for(uint8_t i=0;i<3;++i){
    const Rect b=kRefreshModeActions[i];const bool selected=static_cast<uint8_t>(s.refreshMode)==i;
    const String title=selected?String(titles[i])+" - SELECTED":String(titles[i]);
    selectableCard(fb,b,title,line1[i],line2[i],selected,Icon::Display);
  }
  actionButton(fb,kRefreshModeBackAction,"BACK",false,Icon::ChevronLeft,true);
}

void timezoneSetup(uint8_t* fb,const UiSnapshot& s){
  appBar(fb,s,"TIMEZONE SETUP");
  text(fb,{24,102,492,30},24,126,"SELECT LOCAL TIMEZONE",FontRole::CardHeading,kInk);
  for(uint8_t i=0;i<device_time::timezoneCount();++i){
    const Rect b=kTimezoneActions[i];
    selectableCard(fb,b,device_time::timezoneLabel(i),device_time::timezoneDescription(i),
                   i==s.timezoneIndex?"SELECTED":"SELECT",i==s.timezoneIndex,Icon::Location);
  }
  actionButton(fb,kTimezoneBackAction,"BACK",false,Icon::ChevronLeft,true);
}

void lowPowerSetup(uint8_t* fb,const UiSnapshot& s){
  appBar(fb,s,"LOW POWER MODE");
  text(fb,{24,102,492,54},24,126,"TIMER MONITORING - TOUCH EXIT",FontRole::CardHeading,kInk);
  const power::Preset presets[]={power::Preset::Off,power::Preset::Min5,power::Preset::Min15,power::Preset::Min30,power::Preset::Min60};
  for(uint8_t i=0;i<5;++i){
    const bool selected=s.lowPower.preset==presets[i];
    actionButton(fb,kLowPowerPresetActions[i],String(i?"CHECK EVERY ":"MODE ")+power::presetName(presets[i]),selected,Icon::Power);
  }
  actionButton(fb,kLowPowerBackAction,"BACK",false,Icon::ChevronLeft,true);
}

void lowPowerStatus(uint8_t* fb,const UiSnapshot& s){
  appBar(fb,s,"LOW POWER STATUS");
  centeredText(fb,{24,130,492,82},s.lowPower.criticalHold?"ALERT HOLD":"LOW POWER",FontRole::PageHeading);
  statusPill(fb,{120,224,300,42},s.lowPower.awakeWindow?"SCHEDULED CHECK":"MONITORING",true);
  roundedRect(fb,{24,292,492,330},14,kPaper,kInk);
  labeledRow(fb,{48,314,444,64},"PRESET",power::presetName(s.lowPower.preset));
  labeledRow(fb,{48,378,444,64},"WI-FI",s.lowPower.awakeWindow?"AWAKE WINDOW":"OFF BETWEEN CHECKS");
  labeledRow(fb,{48,442,444,64},"DISPLAY","HV OFF AFTER GC16");
  labeledRow(fb,{48,506,444,64},"TOUCH","EXIT AVAILABLE");
  labeledRow(fb,{48,570,444,38},"SLEEP TYPE","TIMER MONITORING",false);
  actionButton(fb,kLowPowerExitAction,"EXIT LOW POWER",true,Icon::Power);
  actionButton(fb,kLowPowerBackAction,"DEVICE",false,Icon::Device);
}

void touchRecalibrateConfirm(uint8_t* fb,const UiSnapshot& s){
  appBar(fb,s,"RECALIBRATE TOUCH");
  const Rect panel{24,188,492,642};
  roundedRect(fb,panel,14,kPaper,kInk);
  const Rect clip{48,208,444,510};
  text(fb,clip,48,242,"RESET TOUCH CALIBRATION",FontRole::PageHeading,kInk);
  text(fb,clip,48,292,"Your saved calibration remains active",FontRole::Body,kInkMuted);
  text(fb,clip,48,324,"until you confirm.",FontRole::Body,kInkMuted);
  text(fb,clip,48,382,"After confirmation, tap each of the four",FontRole::Body,kInkMuted);
  text(fb,clip,48,414,"visible corner targets in order.",FontRole::Body,kInkMuted);
  actionButton(fb,kTouchRecalibrateCancelAction,"CANCEL",false,Icon::Close);
  actionButton(fb,kTouchRecalibrateConfirmAction,"RECALIBRATE",true,Icon::Touch);
}

void weatherSetup(uint8_t* fb,const UiSnapshot& s){
  const weather::WizardSnapshot&w=s.weatherWizard;
  text(fb,{24,20,270,54},24,52,"WEATHER SETUP",FontRole::PageHeading,kInk);
  actionButton(fb,{24,112,112,56},"BACK",false,Icon::ChevronLeft,true);
  if(w.step==weather::WizardStep::Choice){
    text(fb,{24,74,492,30},24,96,"WEATHER LOCATION",FontRole::CardHeading,kInk);
    actionButton(fb,{150,112,366,56},"DISPLAY PREFERENCES",false,Icon::Display);
    const char* labels[]={"USE GPS","SEARCH CITY","ZIP / POSTAL","ENTER COORDINATES","DISABLE WEATHER"};
    const Icon glyphs[]={Icon::Location,Icon::Search,Icon::Keyboard,Icon::Location,Icon::Power};
    for(int i=0;i<5;++i)actionButton(fb,{24,176+i*118,492,110},labels[i],false,glyphs[i]);
  }else if(w.step==weather::WizardStep::Gps){
    const char* gpsStatus=s.location.state==location::GpsState::Starting?"STARTING GPS":
        (location::currentFixUsable(s.location)?"GPS FIXED":
         (s.location.state==location::GpsState::Searching?"SEARCHING FOR GPS":"GPS FIX REQUIRED"));
    card(fb,{24,190,492,220},"USE GPS",gpsStatus,"WEATHER MODE SAVED - COORDINATES PRIVATE");
    actionButton(fb,{24,520,492,70},"USE CITY INSTEAD",false,Icon::Search);
    actionButton(fb,{24,600,492,70},"USE POSTAL INSTEAD",false,Icon::Keyboard);
    actionButton(fb,{24,680,492,70},"ENTER COORDINATES INSTEAD",false,Icon::Location);
    actionButton(fb,{24,780,238,64},"BACK",false,Icon::ChevronLeft,true);actionButton(fb,{278,780,238,64},"RETRY",false,Icon::Refresh);
  }else if(w.step==weather::WizardStep::Input){
    const char* value=w.inputKind==weather::InputKind::Latitude?w.latitude:(w.inputKind==weather::InputKind::Longitude?w.longitude:w.input);
    card(fb,{24,176,492,58},weather::inputKindName(w.inputKind),value[0]?value:"--");
    const char* chars=weather::pickerCharacters(w.inputKind,w.characterPage);
    // Each single-character key label is its approved compact key graphic.
    for(int row=0;row<5;++row)for(int col=0;col<6;++col){Rect key{24+col*82,246+row*82,74,74};roundedRect(fb,key,8,kPaper,kInk);char label[2]{chars[row*6+col],0};if(label[0]!=' '){const String keyLabel(label);text(fb,key,key.x+(key.w-textWidth(keyLabel,FontRole::CardHeading))/2,centeredBaseline(key,FontRole::CardHeading),keyLabel,FontRole::CardHeading,kInk);}}
    actionButton(fb,{24,682,150,64},"PAGE",false,Icon::Next);actionButton(fb,{195,682,150,64},"DELETE",false,Icon::Delete);actionButton(fb,{366,682,150,64},w.inputKind==weather::InputKind::Postal?String("COUNTRY ")+w.country:"SPACE",false,Icon::Keyboard);
    actionButton(fb,{24,780,238,64},"CANCEL",false,Icon::Close);actionButton(fb,{278,780,238,64},w.inputKind==weather::InputKind::Longitude?"CONFIRM":"NEXT",false,Icon::Check);
  }else if(w.step==weather::WizardStep::Results){
    text(fb,{24,178,492,34},24,202,s.weather.searchState==weather::SearchState::Pending?"SEARCHING...":"SELECT A RESULT",FontRole::CardHeading,kInk);
    for(uint8_t i=0;i<s.weather.resultCount&&i<5;++i)actionButton(fb,{24,220+i*100,492,88},s.weather.results[i].label,false,Icon::Location);
    actionButton(fb,{24,780,238,64},"CANCEL",false,Icon::Close);
  }else if(w.step==weather::WizardStep::Confirm){
    String source=weather::sourceName(w.pendingSource);String label=source;
    if((w.pendingSource==weather::LocationSource::City||w.pendingSource==weather::LocationSource::Postal)&&w.selectedResult<s.weather.resultCount)label=s.weather.results[w.selectedResult].label;
    card(fb,{24,190,492,220},"CONFIRM LOCATION",label,String("SOURCE ")+source);
    text(fb,{44,438,452,100},44,466,w.pendingSource==weather::LocationSource::Manual?"COORDINATES WILL BE SENT TO THE WEATHER PROVIDER.":"NO WEATHER REQUEST UNTIL YOU SAVE.",FontRole::Body,kInk);
    actionButton(fb,{24,780,238,64},"CANCEL",false,Icon::Close);actionButton(fb,{278,780,238,64},"SAVE & FETCH",true,Icon::Save);
  }else{
    text(fb,{24,178,492,34},24,202,"WEATHER DISPLAY",FontRole::CardHeading,kInk);
    actionButton(fb,{24,220,492,86},s.weather.temperatureUnit==weather::TemperatureUnit::Celsius?"TEMPERATURE: CELSIUS":"TEMPERATURE: FAHRENHEIT",true,Icon::Units);
    actionButton(fb,{24,322,492,86},s.weather.showTemperature?"CURRENT TEMPERATURE: ON":"CURRENT TEMPERATURE: OFF",s.weather.showTemperature,Icon::Weather);
    actionButton(fb,{24,424,492,86},s.weather.showCondition?"CONDITION: ON":"CONDITION: OFF",s.weather.showCondition,Icon::Weather);
    actionButton(fb,{24,526,492,86},s.weather.showCity?"CITY LABEL: ON":"CITY LABEL: OFF",s.weather.showCity,Icon::Location);
    actionButton(fb,{24,628,492,86},s.weather.showFeelsLike?"FEELS-LIKE: ON":"FEELS-LIKE: OFF",s.weather.showFeelsLike,Icon::Weather);
    actionButton(fb,{24,780,238,64},"DONE",true,Icon::Check);
  }
  if(w.error[0])text(fb,{24,746,492,28},24,770,w.error,FontRole::Caption,kInk);
}

}  // namespace

void renderPage(uint8_t* fb,const UiSnapshot& s){
  clear(fb);
  switch(s.page){case Page::Home:home(fb,s);break;case Page::Systems:systems(fb,s);break;case Page::Radio:radioPage(fb,s);break;case Page::Location:location(fb,s);break;case Page::Device:device(fb,s);break;case Page::Diagnostics:diagnostics(fb,s);break;case Page::DisplayCalibration:displayCalibration(fb,s);break;case Page::TextQualification:textQualification(fb,s);break;case Page::Settings:settings(fb,s);break;case Page::DisplaySettings:displaySettings(fb,s);break;case Page::DateTimeSettings:dateTimeSettings(fb,s);break;case Page::UnitsSettings:unitsSettings(fb,s);break;case Page::LocationPrivacySettings:locationPrivacySettings(fb,s);break;case Page::WifiSettings:wifiSettings(fb,s);break;case Page::WifiNetworks:wifiNetworks(fb,s);break;case Page::WifiEntry:wifiEntry(fb,s);break;case Page::WifiForgetConfirm:wifiForgetConfirm(fb,s);break;case Page::Calculator:calculatorPage(fb,s);break;case Page::TouchSetup:touchSetup(fb,s);break;case Page::TouchRecalibrateConfirm:touchRecalibrateConfirm(fb,s);break;case Page::WeatherSetup:weatherSetup(fb,s);break;case Page::SystemHealth:systemHealth(fb,s);break;case Page::SystemMetrics:systemMetrics(fb,s);break;case Page::Storage:storageDetail(fb,s);break;case Page::Network:networkDetail(fb,s);break;case Page::WeatherDetail:weatherDetail(fb,s);break;case Page::Battery:batteryDetail(fb,s);break;case Page::VehicleMotion:vehicleMotion(fb,s);break;case Page::Altimeter:altimeter(fb,s);break;case Page::TimezoneSetup:timezoneSetup(fb,s);break;case Page::LowPowerSetup:lowPowerSetup(fb,s);break;case Page::LowPowerStatus:lowPowerStatus(fb,s);break;case Page::DisplayRefreshMode:displayRefreshMode(fb,s);break;}
  if(s.page!=Page::TouchSetup&&s.page!=Page::DisplayCalibration&&
     s.page!=Page::TextQualification&&s.page!=Page::Calculator) bottomNavigation(fb,s.page);
  if(s.pressFeedback.active){
    Rect feedback=s.pressFeedback.bounds;
    String label=s.pressFeedback.label;
    if(s.pressFeedback.destinationRoute&&s.pressFeedback.targetPage==s.page){
      const Rect nav=navigationTarget(s.page);
      if(nav.w>0){feedback=nav;label=pageName(s.page);}
      else{feedback={24,72,492,52};label=String("OPENED ")+pageName(s.page);}
    }
    if((!s.pressFeedback.destinationRoute&&s.pressFeedback.sourcePage==s.page)||
       (s.pressFeedback.destinationRoute&&s.pressFeedback.targetPage==s.page)){
      roundedRect(fb,feedback,s.pressFeedback.radius,kInk,kInk);
      const String shown=fittedText(label,FontRole::CardHeading,feedback.w-20);
      text(fb,feedback,feedback.x+(feedback.w-textWidth(shown,FontRole::CardHeading))/2,
           centeredBaseline(feedback,FontRole::CardHeading),shown,FontRole::CardHeading,kPaper);
    }
  }
}

}  // namespace ui