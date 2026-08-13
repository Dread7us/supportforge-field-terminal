#include "battery_manager.h"

namespace battery {
namespace {
constexpr uint32_t kSampleIntervalMs = 5UL * 60UL * 1000UL;
}

const char* classificationName(Classification value) {
  switch (value) {
    case Classification::NotPresent: return "NOT PRESENT";
    case Classification::Unknown: return "BAT UNKNOWN";
    case Classification::Valid: return "VALID";
    case Classification::Charging: return "CHARGING";
  }
  return "BAT UNKNOWN";
}

void BatteryManager::begin(bool gaugeObserved) {
  classify(gaugeObserved);
  nextSampleMs_ = kSampleIntervalMs;
}

void BatteryManager::poll(uint32_t nowMs, bool gaugeObserved) {
  if (static_cast<int32_t>(nowMs - nextSampleMs_) < 0) return;
  nextSampleMs_ = nowMs + kSampleIntervalMs;
  classify(gaugeObserved);
}

void BatteryManager::classify(bool gaugeObserved) {
  // Presence at 0x55 is verified, but this repository does not yet contain a
  // physically qualified BQ27220 data-memory profile/register contract. Do not
  // read guessed SOC, flags, scaling, or charger state.
  const Classification next = gaugeObserved ? Classification::Unknown
                                             : Classification::NotPresent;
  if (snapshot_.classification != next || snapshot_.percentAvailable) {
    snapshot_.classification = next;
    snapshot_.percentAvailable = false;
    snapshot_.percent = 0;
    ++snapshot_.version;
  }
}

}  // namespace battery