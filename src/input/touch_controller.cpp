#include "touch_controller.h"

#include <Arduino.h>
#include <Wire.h>

namespace input {

Point transformInvertedPortrait(int rawX, int rawY) {
  return {constrain(539 - rawY, 0, ui::kCanvasWidth - 1),
          constrain(rawX, 0, ui::kCanvasHeight - 1)};
}

bool TouchController::begin() {
  for (uint8_t candidate : {static_cast<uint8_t>(0x14), static_cast<uint8_t>(0x5D)}) {
    Wire.beginTransmission(candidate);
    if (Wire.endTransmission() == 0) { address_ = candidate; break; }
  }
  return address_ != 0;
}

bool TouchController::readRaw(bool& pressed, Point& point) {
  if (!address_) return false;
  Wire.beginTransmission(address_); Wire.write(0x81); Wire.write(0x4E);
  if (Wire.endTransmission(false) != 0 || Wire.requestFrom(address_, static_cast<uint8_t>(5)) != 5) return false;
  const uint8_t status=Wire.read(), xl=Wire.read(), xh=Wire.read(), yl=Wire.read(), yh=Wire.read();
  pressed=(status&0x80)&&(status&0x0F); point=transformInvertedPortrait(xl|(xh<<8),yl|(yh<<8));
  if(status&0x80){Wire.beginTransmission(address_);Wire.write(0x81);Wire.write(0x4E);Wire.write(0);Wire.endTransmission();}
  return true;
}

Tap TouchController::poll(uint32_t nowMs) {
  bool pressed=false; Point p{}; if(!readRaw(pressed,p)) return {false,{0,0}};
  if(pressed&&!down_){down_=true;start_=last_=p;downAtMs_=nowMs;return {false,p};}
  if(pressed&&down_){last_=p;return {false,p};}
  if(!pressed&&down_){down_=false;const int dx=abs(last_.x-start_.x),dy=abs(last_.y-start_.y);
    const bool accepted=mappingVerified_&&dx<24&&dy<24&&nowMs-downAtMs_>=35&&nowMs-downAtMs_<1200&&nowMs-lastAcceptedMs_>600;
    if(accepted)lastAcceptedMs_=nowMs;return {accepted,start_};}
  return {false,p};
}

bool TouchController::runFourCornerTest() {
  if(!address_) return false;
  const Point expected[]={{0,0},{539,0},{0,959},{539,959}};
  const char* names[]={"TOP_LEFT","TOP_RIGHT","BOTTOM_LEFT","BOTTOM_RIGHT"};
  for(int i=0;i<4;++i){
    Serial.printf("TOUCH corner=%s action=PRESS timeout=15s\n",names[i]);
    const uint32_t deadline=millis()+15000; bool hit=false;
    while(static_cast<int32_t>(deadline-millis())>0){bool pressed=false;Point p{};if(readRaw(pressed,p)&&pressed){
      hit=abs(p.x-expected[i].x)<=90&&abs(p.y-expected[i].y)<=110;
      Serial.printf("TOUCH corner=%s x=%d y=%d status=%s\n",names[i],p.x,p.y,hit?"PASS":"FAIL");
      while(pressed){delay(25);readRaw(pressed,p);} break;} delay(20);}
    if(!hit){mappingVerified_=false;Serial.println("TOUCH navigation=LOCKED mapping=UNVERIFIED");return false;}
  }
  mappingVerified_=true; Serial.println("TOUCH navigation=ENABLED mapping=FOUR_CORNERS_VERIFIED"); return true;
}

}  // namespace input