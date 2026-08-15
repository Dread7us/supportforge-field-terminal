#pragma once

#include <HardwareSerial.h>
#include <Preferences.h>
#include <TinyGPSPlus.h>
#include <stdint.h>

namespace location {

enum class GpsState : uint8_t { Off, Starting, Searching, Fixed, Stale, Error };
enum class SpeedUnit : uint8_t { Mph, Kmh };
enum class ElevationUnit : uint8_t { Feet, Metres };
enum class MovementState : uint8_t { Unknown, Stopped, Moving };

struct Snapshot {
  GpsState state = GpsState::Off;
  bool fixValid = false;
  bool speedValid = false;
  bool satellitesValid = false;
  uint8_t satellites = 0;
  bool hdopValid = false;
  uint16_t hdopHundredths = 0;
  uint32_t fixAgeMs = UINT32_MAX;
  double latitude = 0.0;
  double longitude = 0.0;
  double speedKmh = 0.0;
  bool altitudeValid = false;
  uint32_t altitudeAgeMs = UINT32_MAX;
  double altitudeMetres = 0.0;
  bool courseValid = false;
  double courseDegrees = 0.0;
  SpeedUnit speedUnit = SpeedUnit::Mph;
  ElevationUnit elevationUnit = ElevationUnit::Feet;
  MovementState movement = MovementState::Unknown;
  bool showCoordinates = false;
  uint32_t version = 1;
};

class GpsManager {
 public:
  explicit GpsManager(HardwareSerial& serial) : serial_(serial) {}
  bool begin(bool railEnabled);
  void setRailEnabled(bool enabled);
  void setHardwareEnabled(bool enabled, bool persistPreference);
  void poll(uint32_t nowMs);
  void toggleSpeedUnit();
  void toggleElevationUnit();
  void toggleCoordinateVisibility();
  Snapshot snapshot() const { return snapshot_; }
  uint32_t version() const { return snapshot_.version; }
  bool enabledPreference() const { return enabledPreference_; }

 private:
  void publish(uint32_t nowMs);
  HardwareSerial& serial_;
  TinyGPSPlus parser_;
  Preferences preferences_;
  Snapshot snapshot_{};
  bool started_ = false;
  bool railEnabled_ = false;
  bool enabledPreference_ = false;
  bool preferencesDirty_ = false;
  uint32_t preferencesDueMs_ = 0;
  uint32_t startedAtMs_ = 0;
  uint32_t lastSentenceMs_ = 0;
};

const char* stateName(GpsState state);
const char* speedUnitName(SpeedUnit unit);
const char* elevationUnitName(ElevationUnit unit);
const char* elevationPreferenceName(ElevationUnit unit);
const char* elevationStatusName(const Snapshot& snapshot);
const char* movementName(MovementState state);
const char* motionStateName(const Snapshot& snapshot);
const char* cardinalShort(double degrees);
const char* cardinalLong(double degrees);
double displaySpeed(double speedKmh, SpeedUnit unit);
double displayElevation(double altitudeMetres, ElevationUnit unit);
bool currentFixUsable(const Snapshot& snapshot);

}  // namespace location