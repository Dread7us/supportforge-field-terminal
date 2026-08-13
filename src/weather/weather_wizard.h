#pragma once

#include <stdint.h>

#include "location/gps_manager.h"
#include "ui/ui_theme.h"
#include "weather_manager.h"

namespace weather {

enum class WizardStep : uint8_t { Choice, Gps, Input, Results, Confirm, Preferences };
enum class InputKind : uint8_t { City, Postal, Latitude, Longitude };
enum class WizardResult : uint8_t { None, Changed, Saved, Cancelled };

struct WizardSnapshot {
  bool active = false;
  WizardStep step = WizardStep::Choice;
  InputKind inputKind = InputKind::City;
  LocationSource pendingSource = LocationSource::Unconfigured;
  char input[32]{};
  char latitude[16]{};
  char longitude[16]{};
  char country[3]{'U','S',0};
  uint8_t characterPage = 0;
  uint8_t selectedResult = 0;
  char error[48]{};
};

class WeatherWizard {
 public:
  void open();
  void openPreferences();
  void cancel();
  WizardSnapshot snapshot() const { return state_; }
  WizardResult tap(ui::Rect point, const location::Snapshot& gps, WeatherManager& manager);

 private:
  void resetInput(InputKind kind);
  bool appendCharacter(char value);
  WizardSnapshot state_{};
};

const char* inputKindName(InputKind kind);
const char* pickerCharacters(InputKind kind, uint8_t page);

}  // namespace weather