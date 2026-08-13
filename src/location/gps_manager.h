#pragma once

#include <HardwareSerial.h>
#include <Preferences.h>
#include <TinyGPSPlus.h>
#include <stdint.h>

namespace location {

enum class GpsState : uint8_t { Off, Starting, Searching, Fixed, Stale, Error };
enum class SpeedUnit : uint8_t { Mph, Kmh };
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
  SpeedUnit speedUnit = SpeedUnit::Mph;
  MovementState movement = MovementState::Unknown;
  bool showCoordinates = false;
  uint32_t version = 1;
};

class GpsManager {
 public:
  explicit GpsManager(HardwareSerial& serial) : serial_(serial) {}
  bool begin(bool railEnabled);
  void setRailEnabled(bool enabled);
  void poll(uint32_t nowMs);
  void toggleSpeedUnit();
  void toggleCoordinateVisibility();
  Snapshot snapshot() const { return snapshot_; }
  uint32_t version() const { return snapshot_.version; }

 private:
  void publish(uint32_t nowMs);
  HardwareSerial& serial_;
  TinyGPSPlus parser_;
  Preferences preferences_;
  Snapshot snapshot_{};
  bool started_ = false;
  bool railEnabled_ = false;
  uint32_t startedAtMs_ = 0;
  uint32_t lastSentenceMs_ = 0;
};

const char* stateName(GpsState state);
const char* speedUnitName(SpeedUnit unit);
const char* movementName(MovementState state);
double displaySpeed(double speedKmh, SpeedUnit unit);
bool currentFixUsable(const Snapshot& snapshot);

}  // namespace location