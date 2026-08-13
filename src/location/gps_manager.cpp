#include "gps_manager.h"

#include <Arduino.h>
#include <math.h>

#include "board_profile.h"

namespace location {
namespace {
constexpr uint32_t kStartGraceMs = 3000;
constexpr uint32_t kFixFreshMs = 5000;
constexpr uint32_t kReceiverErrorMs = 15000;
constexpr double kMovingThresholdKmh = 1.0;
constexpr double kSpeedMaterialKmh = 1.0;

bool materiallyChanged(const Snapshot& a, const Snapshot& b) {
  const uint8_t satBucketA = a.satellitesValid ? min<uint8_t>(a.satellites / 3, 4) : 255;
  const uint8_t satBucketB = b.satellitesValid ? min<uint8_t>(b.satellites / 3, 4) : 255;
  const uint8_t hdopBucketA = a.hdopValid ? min<uint16_t>(a.hdopHundredths / 100, 9) : 255;
  const uint8_t hdopBucketB = b.hdopValid ? min<uint16_t>(b.hdopHundredths / 100, 9) : 255;
  return a.state != b.state || a.fixValid != b.fixValid || a.speedValid != b.speedValid ||
         a.speedUnit != b.speedUnit || a.showCoordinates != b.showCoordinates ||
         a.movement != b.movement || satBucketA != satBucketB || hdopBucketA != hdopBucketB ||
         (a.speedValid && fabs(a.speedKmh - b.speedKmh) >= kSpeedMaterialKmh);
}
}

const char* stateName(GpsState state) {
  switch (state) {
    case GpsState::Off: return "OFF";
    case GpsState::Starting: return "STARTING";
    case GpsState::Searching: return "SEARCHING";
    case GpsState::Fixed: return "FIXED";
    case GpsState::Stale: return "STALE";
    case GpsState::Error: return "ERROR";
  }
  return "ERROR";
}

const char* speedUnitName(SpeedUnit unit) { return unit == SpeedUnit::Mph ? "MPH" : "KM/H"; }
const char* movementName(MovementState state) {
  return state == MovementState::Moving ? "MOVING" :
         (state == MovementState::Stopped ? "STOPPED" : "--");
}
double displaySpeed(double speedKmh, SpeedUnit unit) {
  return unit == SpeedUnit::Mph ? speedKmh * 0.621371192 : speedKmh;
}
bool currentFixUsable(const Snapshot& s) {
  return s.state == GpsState::Fixed && s.fixValid && s.fixAgeMs <= kFixFreshMs;
}

bool GpsManager::begin(bool railEnabled) {
  preferences_.begin("sf_location", false);
  snapshot_.speedUnit = preferences_.getBool("speed_mph", true) ? SpeedUnit::Mph : SpeedUnit::Kmh;
  snapshot_.showCoordinates = preferences_.getBool("show_coords", false);
  setRailEnabled(railEnabled);
  return true;
}

void GpsManager::setRailEnabled(bool enabled) {
  railEnabled_ = enabled;
  if (!enabled || !hq::kBoard.gpsCandidateAvailable) {
    if (started_) serial_.end();
    started_ = false;
    Snapshot next = snapshot_;
    next.state = hq::kBoard.gpsCandidateAvailable ? GpsState::Off : GpsState::Error;
    next.fixValid = next.speedValid = false;
    next.movement = MovementState::Unknown;
    if (materiallyChanged(snapshot_, next)) ++next.version;
    snapshot_ = next;
    return;
  }
  if (!started_) {
    // RX-only is a hard contract. The qualified module TX is observed on the
    // MCU RX pin; the MCU never assigns or drives a GPS TX pin.
    serial_.begin(9600, SERIAL_8N1, hq::kBoard.gpsRx, -1);
    started_ = true;
    startedAtMs_ = millis();
    snapshot_.state = GpsState::Starting;
    ++snapshot_.version;
  }
}

void GpsManager::poll(uint32_t nowMs) {
  if (!started_) return;
  size_t budget = 256;
  while (budget-- && serial_.available()) {
    const char value = static_cast<char>(serial_.read());
    parser_.encode(value);
    if (value == '\n') lastSentenceMs_ = nowMs;
  }
  publish(nowMs);
}

void GpsManager::publish(uint32_t nowMs) {
  Snapshot next = snapshot_;
  const bool receiverLive = lastSentenceMs_ && nowMs - lastSentenceMs_ <= kReceiverErrorMs;
  const bool locationValid = parser_.location.isValid();
  const uint32_t age = locationValid ? parser_.location.age() : UINT32_MAX;
  next.fixAgeMs = age;
  next.fixValid = locationValid && age <= kFixFreshMs;
  if (!receiverLive && nowMs - startedAtMs_ > kReceiverErrorMs) next.state = GpsState::Error;
  else if (nowMs - startedAtMs_ <= kStartGraceMs && !lastSentenceMs_) next.state = GpsState::Starting;
  else if (next.fixValid) next.state = GpsState::Fixed;
  else if (locationValid) next.state = GpsState::Stale;
  else next.state = GpsState::Searching;

  next.satellitesValid = receiverLive && parser_.satellites.isValid();
  next.satellites = next.satellitesValid ? min<uint32_t>(parser_.satellites.value(), 255) : 0;
  next.hdopValid = next.fixValid && parser_.hdop.isValid() && parser_.hdop.value() > 0;
  next.hdopHundredths = next.hdopValid ? min<uint32_t>(parser_.hdop.value(), 65535) : 0;
  if (next.fixValid) {
    next.latitude = parser_.location.lat();
    next.longitude = parser_.location.lng();
  }
  // Speed comes only from a fresh receiver-provided GNSS speed field.
  next.speedValid = next.fixValid && parser_.speed.isValid() && parser_.speed.age() <= kFixFreshMs &&
                    isfinite(parser_.speed.kmph()) && parser_.speed.kmph() >= 0.0;
  next.speedKmh = next.speedValid ? parser_.speed.kmph() : 0.0;
  next.movement = next.speedValid ? (next.speedKmh >= kMovingThresholdKmh ? MovementState::Moving
                                                                          : MovementState::Stopped)
                                  : MovementState::Unknown;
  if (materiallyChanged(snapshot_, next)) ++next.version;
  if (next.state != snapshot_.state || next.fixValid != snapshot_.fixValid ||
      next.speedValid != snapshot_.speedValid) {
    Serial.printf("GPS state=%s fix-valid=%s speed-valid=%s\n", stateName(next.state),
                  next.fixValid ? "YES" : "NO", next.speedValid ? "YES" : "NO");
  }
  snapshot_ = next;
}

void GpsManager::toggleSpeedUnit() {
  snapshot_.speedUnit = snapshot_.speedUnit == SpeedUnit::Mph ? SpeedUnit::Kmh : SpeedUnit::Mph;
  preferences_.putBool("speed_mph", snapshot_.speedUnit == SpeedUnit::Mph);
  ++snapshot_.version;
}

void GpsManager::toggleCoordinateVisibility() {
  snapshot_.showCoordinates = !snapshot_.showCoordinates;
  preferences_.putBool("show_coords", snapshot_.showCoordinates);
  ++snapshot_.version;
}

}  // namespace location