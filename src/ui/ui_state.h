#pragma once

#include <stdint.h>

namespace ui {

enum class Page : uint8_t { Home, Systems, Radio, Location, Device, Diagnostics };
enum class Presence : uint8_t { Unknown, NotPresent, Observed };

struct UiSnapshot {
  Page page = Page::Home;
  bool configured = false;
  bool rtcValid = false;
  uint8_t hour = 0;
  uint8_t minute = 0;
  uint8_t day = 0;
  uint8_t month = 0;
  uint16_t year = 0;
  Presence touch = Presence::Unknown;
  Presence rtc = Presence::Unknown;
  Presence fuelGauge = Presence::Unknown;
  Presence storage = Presence::Unknown;
  Presence gps = Presence::Unknown;
  bool gpsFix = false;
  uint8_t gpsSatellites = 0;
  Presence radio = Presence::Unknown;
  bool radioListening = false;
  bool sharedRailEnabled = false;
  bool touchMappingVerified = false;
  bool psramAvailable = false;
  const char* firmwareId = "field-terminal-ui-1";
  const char* buildDate = "unknown";
  const char* buildTime = "unknown";
};

const char* pageName(Page page);

}  // namespace ui