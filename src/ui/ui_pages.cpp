#include "ui_pages.h"

#include "ui_components.h"

namespace ui {
namespace {

String observed(Presence p) {
  return p == Presence::Observed ? "READY" : (p == Presence::NotPresent ? "NOT PRESENT" : "UNVERIFIED");
}

void home(uint8_t* fb,const UiSnapshot& s){
  appBar(fb,s); card(fb,{24,128,492,154},"TERMINAL STATUS","SETUP REQUIRED","Connect supportFORGE to begin monitoring");
  statusPill(fb,{44,238,112,28},"OFFLINE",true);
  const int y=306,w=238,h=112;
  metricTile(fb,{24,y,w,h},Icon::Systems,"SYSTEMS","NOT CONNECTED","No hosts configured");
  metricTile(fb,{278,y,w,h},Icon::Info,"INCIDENTS","--","Awaiting setup");
  metricTile(fb,{24,y+128,w,h},Icon::Radio,"LORA",s.radioListening?"RECEIVING":observed(s.radio),"TX remains locked");
  metricTile(fb,{278,y+128,w,h},Icon::Location,"GPS",s.gpsFix?"FIX ACQUIRED":observed(s.gps),"Coordinates private");
  roundedRect(fb,{24,562,492,252},12,kPaper,kRule); text(fb,44,584,"CURRENT DEVICE",1,kInkMuted);
  labeledRow(fb,{44,610,452,48},"BATTERY",observed(s.fuelGauge));
  labeledRow(fb,{44,658,452,48},"LOCATION",s.gpsFix?"FIX":observed(s.gps));
  labeledRow(fb,{44,706,452,48},"RADIO",s.radioListening?"RX ACTIVE":"TX LOCKED");
  labeledRow(fb,{44,754,452,48},"NEXT SYNC","NOT SCHEDULED",false);
}

void systems(uint8_t* fb,const UiSnapshot& s){
  appBar(fb,s,"SYSTEMS");
  emptyState(fb,{24,144,492,326},Icon::Systems,"NO SYSTEMS YET","Connect supportFORGE to begin monitoring");
  card(fb,{24,494,492,150},"CONFIGURATION","SETUP REQUIRED","Host data will appear here after connection");
  statusPill(fb,{44,680,170,32},"NO FABRICATED DATA");
}

void radioPage(uint8_t* fb,const UiSnapshot& s){
  appBar(fb,s,"RADIO"); card(fb,{24,128,492,156},"SX1262 RADIO",s.radioListening?"RECEIVE ACTIVE":"RECEIVE ONLY","915 MHz region - transmission disabled");
  statusPill(fb,{44,240,126,30},"TX LOCKED",true);
  roundedRect(fb,{24,308,492,246},12,kPaper,kRule);
  labeledRow(fb,{44,326,452,52},"REGION","915 MHZ"); labeledRow(fb,{44,378,452,52},"MODE","RECEIVE ONLY");
  labeledRow(fb,{44,430,452,52},"MODULE",observed(s.radio)); labeledRow(fb,{44,482,452,52},"TRANSMIT","LOCKED",false);
  emptyState(fb,{24,580,492,210},Icon::Radio,"NO MESSAGES","Received LoRa traffic will appear here");
}

void location(uint8_t* fb,const UiSnapshot& s){
  appBar(fb,s,"LOCATION"); card(fb,{24,128,492,156},"L76K GNSS",s.gpsFix?"FIX ACQUIRED":observed(s.gps),s.gpsFix?"Navigation data available locally":"Waiting for qualified satellite data");
  roundedRect(fb,{24,310,492,246},12,kPaper,kRule);
  labeledRow(fb,{44,328,452,52},"MODULE",observed(s.gps)); labeledRow(fb,{44,380,452,52},"FIX",s.gpsFix?"VALID":"NO FIX");
  labeledRow(fb,{44,432,452,52},"SATELLITES",s.gpsSatellites?String(s.gpsSatellites):"--"); labeledRow(fb,{44,484,452,52},"COORDINATES","PRIVATE",false);
  emptyState(fb,{24,582,492,208},Icon::Location,"NO WAYPOINTS","Navigation tools are planned for a future release");
}

void device(uint8_t* fb,const UiSnapshot& s){
  appBar(fb,s,"DEVICE");
  metricTile(fb,{24,128,238,112},Icon::Battery,"BATTERY",observed(s.fuelGauge),"BQ27220 fuel gauge");
  metricTile(fb,{278,128,238,112},Icon::Device,"STORAGE",observed(s.storage),"Read-only status");
  roundedRect(fb,{24,264,492,258},12,kPaper,kRule);
  labeledRow(fb,{44,282,452,52},"RTC",observed(s.rtc)); labeledRow(fb,{44,334,452,52},"TOUCH",observed(s.touch));
  labeledRow(fb,{44,386,452,52},"PSRAM",s.psramAvailable?"READY":"UNAVAILABLE"); labeledRow(fb,{44,438,452,64},"FIRMWARE",s.firmwareId,false);
  roundedRect(fb,{24,550,492,88},10,kSurfaceStrong,kInk); icon(fb,Icon::Device,58,594,28,kInk);
  text(fb,88,570,"HARDWARE DIAGNOSTICS",2,kInk,390); text(fb,88,602,"Safe qualification tools and observed state",1,kInkMuted,390);
  card(fb,{24,666,492,124},"SETTINGS","COMING LATER","No credentials or endpoints stored");
}

void diagnostics(uint8_t* fb,const UiSnapshot& s){
  appBar(fb,s,"HARDWARE DIAGNOSTICS"); statusPill(fb,{24,122,180,30},"OBSERVED STATUS",true);
  roundedRect(fb,{24,170,492,438},12,kPaper,kRule);
  labeledRow(fb,{44,184,452,50},"GT911",observed(s.touch)); labeledRow(fb,{44,234,452,50},"RTC",observed(s.rtc));
  labeledRow(fb,{44,284,452,50},"BQ27220",observed(s.fuelGauge)); labeledRow(fb,{44,334,452,50},"MICROSD",observed(s.storage));
  labeledRow(fb,{44,384,452,50},"L76K",s.gpsFix?"VALID FIX":observed(s.gps)); labeledRow(fb,{44,434,452,50},"SX1262",observed(s.radio));
  labeledRow(fb,{44,484,452,50},"LORA TX","LOCKED"); labeledRow(fb,{44,534,452,50},"TOUCH MAP",s.touchMappingVerified?"VERIFIED":"SERIAL TEST",false);
  card(fb,{24,632,492,158},"GUARDED OPERATIONS","SERIAL COMMANDS ONLY","No TX, VCOM, waveform, or destructive tests exposed");
}

}  // namespace

void renderPage(uint8_t* fb,const UiSnapshot& s){
  clear(fb);
  switch(s.page){case Page::Home:home(fb,s);break;case Page::Systems:systems(fb,s);break;case Page::Radio:radioPage(fb,s);break;case Page::Location:location(fb,s);break;case Page::Device:device(fb,s);break;case Page::Diagnostics:diagnostics(fb,s);break;}
  bottomNavigation(fb,s.page);
}

}  // namespace ui