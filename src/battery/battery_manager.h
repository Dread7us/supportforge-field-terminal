#pragma once

#include <stdint.h>

namespace battery {

// These states describe only evidence read from the documented BQ27220 and
// BQ25896 read-only contracts. No gauge configuration/control path exists here.
enum class State : uint8_t { Available, Charging, Full, Stale, Unknown, NotPresent, Error };

struct Snapshot {
  State state = State::NotPresent;
  bool percentAvailable = false;
  uint8_t percent = 0;
  bool sampleAttempted = false;
  bool sampleValid = false;
  uint32_t lastSampleMs = 0;
  uint32_t lastAttemptMs = 0;
  uint32_t version = 1;
};

class BatteryManager {
 public:
  void begin(bool gaugeObserved, bool chargerObserved);
  void poll(uint32_t nowMs, bool gaugeObserved, bool chargerObserved,
            bool scheduledAwake = true);
  Snapshot snapshot() const { return snapshot_; }
  uint32_t version() const { return snapshot_.version; }

 private:
  void sample(uint32_t nowMs, bool gaugeObserved, bool chargerObserved);
  Snapshot snapshot_{};
  uint32_t nextSampleMs_ = 0;
};

const char* stateName(State value);
bool validPercent(int value);
uint16_t decodeLittleEndianWord(uint8_t low, uint8_t high);
State classifyChargeStatus(uint8_t register0b);

}  // namespace battery