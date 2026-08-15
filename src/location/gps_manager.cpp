#include "gps_manager.h"

#include <Arduino.h>
#include <math.h>

#include "board_profile.h"

namespace location {
namespace {
constexpr uint32_t kStartGraceMs = 3000;
constexpr uint32_t kFixFreshMs = 5000;
constexpr uint32_t kReceiverErrorMs = 15000;
constexpr uint32_t kPreferenceWriteDelayMs = 750;
constexpr size_t kGpsRxBufferBytes = 4096;
// Five km/h rejects normal stationary GNSS drift and very slow walking noise.
constexpr double kMovingThresholdKmh = 5.0;
constexpr double kSpeedMaterialKmh = 1.0;
constexpr double kCourseMaterialDegrees = 15.0;
constexpr double kElevationMaterialFeet = 10.0;
constexpr double kElevationMaterialMetres = 3.0;

uint8_t cardinalBucket(double degrees) {
  const double normalized = fmod(fmod(degrees, 360.0) + 360.0, 360.0);
  return static_cast<uint8_t>(floor((normalized + 22.5) / 45.0)) % 8;
}

uint8_t courseRenderBucket(double degrees) {
  const double normalized = fmod(fmod(degrees, 360.0) + 360.0, 360.0);
  return static_cast<uint8_t>(floor((normalized + kCourseMaterialDegrees / 2.0) /
                                    kCourseMaterialDegrees));
}

bool materiallyChanged(const Snapshot& a, const Snapshot& b) {
  const uint8_t satBucketA = a.satellitesValid ? min<uint8_t>(a.satellites / 3, 4) : 255;
  const uint8_t satBucketB = b.satellitesValid ? min<uint8_t>(b.satellites / 3, 4) : 255;
  const uint8_t hdopBucketA = a.hdopValid ? min<uint16_t>(a.hdopHundredths / 100, 9) : 255;
  const uint8_t hdopBucketB = b.hdopValid ? min<uint16_t>(b.hdopHundredths / 100, 9) : 255;
  const double elevationThresholdMetres = b.elevationUnit == ElevationUnit::Feet
      ? kElevationMaterialFeet / 3.280839895 : kElevationMaterialMetres;
  return a.state != b.state || a.fixValid != b.fixValid || a.speedValid != b.speedValid ||
         a.altitudeValid != b.altitudeValid ||
         a.speedUnit != b.speedUnit || a.showCoordinates != b.showCoordinates ||
         a.elevationUnit != b.elevationUnit ||
         a.movement != b.movement || a.courseValid != b.courseValid ||
         (a.courseValid && (cardinalBucket(a.courseDegrees) != cardinalBucket(b.courseDegrees) ||
                            courseRenderBucket(a.courseDegrees) != courseRenderBucket(b.courseDegrees))) ||
         satBucketA != satBucketB || hdopBucketA != hdopBucketB ||
         (a.speedValid && fabs(a.speedKmh - b.speedKmh) >= kSpeedMaterialKmh) ||
         (a.altitudeValid && fabs(a.altitudeMetres - b.altitudeMetres) >= elevationThresholdMetres);
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
const char* elevationUnitName(ElevationUnit unit) { return unit == ElevationUnit::Feet ? "FT" : "M"; }
const char* elevationPreferenceName(ElevationUnit unit) {
  return unit == ElevationUnit::Feet ? "FEET" : "METRES";
}
const char* elevationStatusName(const Snapshot& s) {
  if (s.altitudeValid && currentFixUsable(s)) return "GPS FIXED";
  if (s.state == GpsState::Stale ||
      (s.altitudeAgeMs != UINT32_MAX && s.altitudeAgeMs > kFixFreshMs)) return "GPS STALE";
  return "GPS FIX REQUIRED";
}
const char* movementName(MovementState state) {
  return state == MovementState::Moving ? "MOVING" :
         (state == MovementState::Stopped ? "STOPPED" : "--");
}
const char* motionStateName(const Snapshot& s) {
  if (s.state == GpsState::Stale) return "GPS STALE";
  if (s.state == GpsState::Searching || s.state == GpsState::Starting) return "GPS SEARCHING";
  if (!currentFixUsable(s) || !s.speedValid) return "GPS FIX REQUIRED";
  return s.movement == MovementState::Moving ? "MOVING" : "STOPPED";
}
const char* cardinalShort(double degrees) {
  static const char* kNames[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  return kNames[cardinalBucket(degrees)];
}
const char* cardinalLong(double degrees) {
  static const char* kNames[] = {"NORTH", "NORTHEAST", "EAST", "SOUTHEAST",
                                 "SOUTH", "SOUTHWEST", "WEST", "NORTHWEST"};
  return kNames[cardinalBucket(degrees)];
}
double displaySpeed(double speedKmh, SpeedUnit unit) {
  return unit == SpeedUnit::Mph ? speedKmh * 0.621371192 : speedKmh;
}
double displayElevation(double altitudeMetres, ElevationUnit unit) {
  return unit == ElevationUnit::Feet ? altitudeMetres * 3.280839895 : altitudeMetres;
}
bool currentFixUsable(const Snapshot& s) {
  return s.state == GpsState::Fixed && s.fixValid && s.fixAgeMs <= kFixFreshMs;
}
bool GpsManager::begin(bool railEnabled) {
  preferences_.begin("sf_location", false);
  enabledPreference_ = preferences_.getBool("gps_enabled", false);
  snapshot_.speedUnit = preferences_.getBool("speed_mph", true) ? SpeedUnit::Mph : SpeedUnit::Kmh;
  snapshot_.elevationUnit = preferences_.getBool("elev_feet", true)
      ? ElevationUnit::Feet : ElevationUnit::Metres;
  snapshot_.showCoordinates = preferences_.getBool("show_coords", false);
  // Boot hardware state is supplied separately from the persisted user choice;
  // do not erase that choice while the shared rail is still intentionally off.
  railEnabled_ = railEnabled;
  snapshot_.state = GpsState::Off;
  return true;
}

void GpsManager::setRailEnabled(bool enabled) {
  setHardwareEnabled(enabled, true);
}

void GpsManager::setHardwareEnabled(bool enabled, bool persistPreference) {
  railEnabled_ = enabled;
  if (persistPreference) {
    enabledPreference_ = enabled;
    preferencesDirty_ = true;
    preferencesDueMs_ = millis() + kPreferenceWriteDelayMs;
  }
  if (!enabled || !hq::kBoard.gpsCandidateAvailable) {
    if (started_) serial_.end();
    started_ = false;
    Snapshot next = snapshot_;
    next.state = hq::kBoard.gpsCandidateAvailable ? GpsState::Off : GpsState::Error;
    next.fixValid = next.speedValid = next.altitudeValid = false;
    next.altitudeAgeMs = UINT32_MAX;
    next.courseValid = false;
    next.movement = MovementState::Unknown;
    if (materiallyChanged(snapshot_, next)) ++next.version;
    snapshot_ = next;
    return;
  }
  if (!started_) {
    // RX-only is a hard contract. The qualified module TX is observed on the
    // MCU RX pin; the MCU never assigns or drives a GPS TX pin.
    // GC16 rendering is synchronous. A bounded RX buffer lets the independent
    // receiver continue collecting sentences while the panel is updating.
    serial_.setRxBufferSize(kGpsRxBufferBytes);
    serial_.begin(9600, SERIAL_8N1, hq::kBoard.gpsRx, -1);
    started_ = true;
    startedAtMs_ = millis();
    snapshot_.state = GpsState::Starting;
    ++snapshot_.version;
  }
}

void GpsManager::poll(uint32_t nowMs) {
  if (preferencesDirty_ && static_cast<int32_t>(nowMs - preferencesDueMs_) >= 0) {
    preferences_.putBool("gps_enabled", enabledPreference_);
    preferences_.putBool("speed_mph", snapshot_.speedUnit == SpeedUnit::Mph);
    preferences_.putBool("elev_feet", snapshot_.elevationUnit == ElevationUnit::Feet);
    preferences_.putBool("show_coords", snapshot_.showCoordinates);
    preferencesDirty_ = false;
  }
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
  // Elevation comes only from TinyGPSPlus' receiver-provided NMEA altitude
  // field, and is never retained for display after either fix becomes stale.
  const bool altitudeFieldValid = parser_.altitude.isValid() &&
                                  isfinite(parser_.altitude.meters());
  next.altitudeAgeMs = altitudeFieldValid ? parser_.altitude.age() : UINT32_MAX;
  next.altitudeValid = next.fixValid && altitudeFieldValid &&
                       next.altitudeAgeMs <= kFixFreshMs;
  next.altitudeMetres = next.altitudeValid ? parser_.altitude.meters() : 0.0;
  next.movement = next.speedValid ? (next.speedKmh >= kMovingThresholdKmh ? MovementState::Moving
                                                                          : MovementState::Stopped)
                                  : MovementState::Unknown;
  // TinyGPSPlus course is receiver-provided course over ground, not a magnetic
  // compass. It is accepted only with a fresh fix/course and meaningful motion.
  next.courseValid = next.movement == MovementState::Moving && parser_.course.isValid() &&
                     parser_.course.age() <= kFixFreshMs &&
                     isfinite(parser_.course.deg()) && parser_.course.deg() >= 0.0 &&
                     parser_.course.deg() < 360.0;
  next.courseDegrees = next.courseValid ? parser_.course.deg() : 0.0;
  if (materiallyChanged(snapshot_, next)) ++next.version;
  if (next.state != snapshot_.state || next.fixValid != snapshot_.fixValid ||
      next.speedValid != snapshot_.speedValid || next.altitudeValid != snapshot_.altitudeValid) {
    Serial.printf("GPS altitude-valid=%s elevation-unit=%s freshness=%s\n",
                  next.altitudeValid ? "YES" : "NO",
                  elevationPreferenceName(next.elevationUnit), elevationStatusName(next));
  }
  snapshot_ = next;
}

void GpsManager::toggleSpeedUnit() {
  snapshot_.speedUnit = snapshot_.speedUnit == SpeedUnit::Mph ? SpeedUnit::Kmh : SpeedUnit::Mph;
  preferencesDirty_ = true;
  preferencesDueMs_ = millis() + kPreferenceWriteDelayMs;
  ++snapshot_.version;
}

void GpsManager::toggleElevationUnit() {
  snapshot_.elevationUnit = snapshot_.elevationUnit == ElevationUnit::Feet
      ? ElevationUnit::Metres : ElevationUnit::Feet;
  preferencesDirty_ = true;
  preferencesDueMs_ = millis() + kPreferenceWriteDelayMs;
  ++snapshot_.version;
}

void GpsManager::toggleCoordinateVisibility() {
  snapshot_.showCoordinates = !snapshot_.showCoordinates;
  preferencesDirty_ = true;
  preferencesDueMs_ = millis() + kPreferenceWriteDelayMs;
  ++snapshot_.version;
}

}  // namespace location