#include "battery_visual_model.h"

namespace battery {

BatteryVisualModel makeBatteryVisualModel(const Snapshot& snapshot) {
  BatteryVisualModel model{};
  model.state = snapshot.state;
  model.charging = snapshot.state == State::Charging;
  // BatteryManager alone selects GAUGE SOC versus CAPACITY RATIO. Reject any
  // inconsistent projection rather than allowing a UI surface to invent data.
  model.percentAvailable = snapshot.percentAvailable &&
      snapshot.displaySource != DisplaySource::Unavailable &&
      validPercent(snapshot.percent);
  model.percent = model.percentAvailable ? snapshot.percent : 0;
  model.source = model.percentAvailable ? snapshot.displaySource : DisplaySource::Unavailable;
  return model;
}

}  // namespace battery