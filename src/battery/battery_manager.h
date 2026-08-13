#pragma once

#include <stdint.h>

namespace battery {

enum class Classification : uint8_t { NotPresent, Unknown, Valid, Charging };

struct Snapshot {
  Classification classification = Classification::NotPresent;
  bool percentAvailable = false;
  uint8_t percent = 0;
  uint32_t version = 1;
};

class BatteryManager {
 public:
  void begin(bool gaugeObserved);
  void poll(uint32_t nowMs, bool gaugeObserved);
  Snapshot snapshot() const { return snapshot_; }
  uint32_t version() const { return snapshot_.version; }

 private:
  void classify(bool gaugeObserved);
  Snapshot snapshot_{};
  uint32_t nextSampleMs_ = 0;
};

const char* classificationName(Classification value);

}  // namespace battery