#include "low_power_manager.h"

#include <Arduino.h>

namespace power {
namespace {
constexpr uint32_t kAwakeWindowMs = 45UL * 1000UL;

bool reached(uint32_t nowMs, uint32_t deadlineMs) {
  return static_cast<int32_t>(nowMs - deadlineMs) >= 0;
}
}

const char* presetName(Preset preset) {
  switch (preset) {
    case Preset::Off: return "OFF";
    case Preset::Min5: return "5 MIN";
    case Preset::Min15: return "15 MIN";
    case Preset::Min30: return "30 MIN";
    case Preset::Min60: return "60 MIN";
  }
  return "OFF";
}

uint32_t presetIntervalMs(Preset preset) {
  switch (preset) {
    case Preset::Min5: return 5UL * 60UL * 1000UL;
    case Preset::Min15: return 15UL * 60UL * 1000UL;
    case Preset::Min30: return 30UL * 60UL * 1000UL;
    case Preset::Min60: return 60UL * 60UL * 1000UL;
    case Preset::Off: return 0;
  }
  return 0;
}

bool LowPowerManager::begin(uint32_t nowMs) {
  if (!preferences_.begin("sf_power", false)) return false;
  const uint8_t stored = preferences_.getUChar("preset", 0);
  snapshot_.preset = stored <= static_cast<uint8_t>(Preset::Min60)
      ? static_cast<Preset>(stored) : Preset::Off;
  snapshot_.active = snapshot_.preset != Preset::Off;
  snapshot_.awakeWindow = !snapshot_.active;
  if (snapshot_.active) scheduleNext(nowMs);
  return true;
}

void LowPowerManager::publishIfChanged(const Snapshot& before) {
  if (before.preset != snapshot_.preset || before.active != snapshot_.active ||
      before.awakeWindow != snapshot_.awakeWindow ||
      before.criticalHold != snapshot_.criticalHold ||
      before.nextWakeMs != snapshot_.nextWakeMs) {
    snapshot_.version = before.version + 1;
  }
}

void LowPowerManager::scheduleNext(uint32_t nowMs) {
  snapshot_.awakeWindow = false;
  snapshot_.windowEndsMs = 0;
  snapshot_.nextWakeMs = nowMs + presetIntervalMs(snapshot_.preset);
}

void LowPowerManager::selectPreset(Preset preset, uint32_t nowMs) {
  const Snapshot before = snapshot_;
  snapshot_.preset = preset;
  snapshot_.active = preset != Preset::Off;
  snapshot_.criticalHold = false;
  if (snapshot_.active) scheduleNext(nowMs);
  else {
    snapshot_.awakeWindow = true;
    snapshot_.nextWakeMs = 0;
    snapshot_.windowEndsMs = 0;
  }
  preferences_.putUChar("preset", static_cast<uint8_t>(preset));
  publishIfChanged(before);
  Serial.printf("LOW_POWER preset=%s active=%s mode=TIMER_MONITORING touch_exit=AVAILABLE\n",
                presetName(preset), snapshot_.active ? "YES" : "NO");
}

void LowPowerManager::exit(uint32_t nowMs) {
  (void)nowMs;
  selectPreset(Preset::Off, nowMs);
}

void LowPowerManager::setCriticalHold(bool critical, uint32_t nowMs) {
  if (!snapshot_.active || snapshot_.criticalHold == critical) return;
  const Snapshot before = snapshot_;
  snapshot_.criticalHold = critical;
  if (critical) {
    snapshot_.awakeWindow = true;
    snapshot_.nextWakeMs = 0;
    snapshot_.windowEndsMs = 0;
  } else {
    scheduleNext(nowMs);
  }
  publishIfChanged(before);
}

void LowPowerManager::poll(uint32_t nowMs) {
  if (!snapshot_.active || snapshot_.criticalHold) return;
  const Snapshot before = snapshot_;
  if (!snapshot_.awakeWindow && snapshot_.nextWakeMs && reached(nowMs, snapshot_.nextWakeMs)) {
    snapshot_.awakeWindow = true;
    snapshot_.nextWakeMs = 0;
    snapshot_.windowEndsMs = nowMs + kAwakeWindowMs;
  } else if (snapshot_.awakeWindow && snapshot_.windowEndsMs && reached(nowMs, snapshot_.windowEndsMs)) {
    scheduleNext(nowMs);
  }
  publishIfChanged(before);
}

}  // namespace power