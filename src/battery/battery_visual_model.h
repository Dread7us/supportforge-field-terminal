#pragma once

#include <stdint.h>

#include "battery_manager.h"

namespace battery {

// The sole UI projection of BatteryManager's selected, evidence-backed value.
// Renderers consume this model and must not select or calculate a percentage.
struct BatteryVisualModel {
  bool percentAvailable = false;
  uint8_t percent = 0;
  DisplaySource source = DisplaySource::Unavailable;
  State state = State::NotPresent;
  bool charging = false;
};

BatteryVisualModel makeBatteryVisualModel(const Snapshot& snapshot);

}  // namespace battery