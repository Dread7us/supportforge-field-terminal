#pragma once

#include <Preferences.h>
#include <stdint.h>

namespace power {

enum class Preset : uint8_t { Off, Min5, Min15, Min30, Min60 };

struct Snapshot {
  Preset preset = Preset::Off;
  bool active = false;
  bool awakeWindow = true;
  bool criticalHold = false;
  uint32_t nextWakeMs = 0;
  uint32_t windowEndsMs = 0;
  uint32_t version = 1;
};

class LowPowerManager {
 public:
  bool begin(uint32_t nowMs);
  void selectPreset(Preset preset, uint32_t nowMs);
  void exit(uint32_t nowMs);
  void setCriticalHold(bool critical, uint32_t nowMs);
  void poll(uint32_t nowMs);
  Snapshot snapshot() const { return snapshot_; }
  uint32_t version() const { return snapshot_.version; }

 private:
  void publishIfChanged(const Snapshot& before);
  void scheduleNext(uint32_t nowMs);

  Preferences preferences_;
  Snapshot snapshot_{};
};

const char* presetName(Preset preset);
uint32_t presetIntervalMs(Preset preset);

}  // namespace power