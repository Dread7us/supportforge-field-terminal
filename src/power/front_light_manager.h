#pragma once

#include <Preferences.h>
#include <stdint.h>

namespace power {

enum class FrontLightLevel : uint8_t { Off, Low, Medium, High };

struct FrontLightSnapshot {
  FrontLightLevel preferred = FrontLightLevel::Off;
  FrontLightLevel effective = FrontLightLevel::Off;
  bool candidateAvailable = false;
  bool lowPowerSuppressed = false;
  uint32_t version = 1;
};

class FrontLightManager {
 public:
  bool begin(bool candidateAvailable, int pwmPin);
  void restoreAfterInitialization(bool lowPowerActive);
  bool setPreferred(FrontLightLevel level);
  void setLowPowerSuppressed(bool suppressed);
  FrontLightSnapshot snapshot() const { return snapshot_; }

 private:
  void apply();
  void publishIfChanged(const FrontLightSnapshot& before);

  Preferences preferences_;
  FrontLightSnapshot snapshot_{};
  int pwmPin_ = -1;
  bool preferencesReady_ = false;
};

const char* frontLightLevelName(FrontLightLevel level);
uint8_t frontLightDuty(FrontLightLevel level);

}  // namespace power