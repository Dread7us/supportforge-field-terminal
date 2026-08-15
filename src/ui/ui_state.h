#pragma once

#include <stdint.h>

#include "telemetry/telemetry_model.h"
#include "battery/battery_manager.h"
#include "time/time_service.h"
#include "weather/weather_manager.h"
#include "weather/weather_wizard.h"
#include "location/gps_manager.h"
#include "power/low_power_manager.h"

namespace ui {

enum class Page : uint8_t {
  Home,
  Systems,
  Radio,
  Location,
  Device,
  Diagnostics,
  DisplayCalibration,
  TextQualification,
  Settings,
  TouchSetup,
  TouchRecalibrateConfirm,
  WeatherSetup,
  DisplayRefreshConfirm,
  SystemHealth,
  SystemMetrics,
  Storage,
  Network,
  WeatherDetail,
  Battery,
  VehicleMotion,
  Altimeter,
  TimezoneSetup,
  LowPowerSetup,
  LowPowerStatus,
  DisplayRefreshMode
};
enum class Presence : uint8_t { Unknown, NotPresent, Observed };
enum class RefreshMode : uint8_t { QuickNavigation, Balanced, BeautifulClean };

const char* refreshModeName(RefreshMode mode);
const char* refreshModeSummary(RefreshMode mode);

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
  uint8_t touchSetupStep = 0;
  bool touchSetupReady = false;
  bool psramAvailable = false;
  bool batteryPercentAvailable = false;
  uint8_t batteryPercent = 0;
  battery::State batteryState = battery::State::NotPresent;
  bool batterySampleAttempted = false;
  bool batterySampleValid = false;
  bool batteryChargeStatusVerified = false;
  uint32_t batteryLastSampleMs = 0;
  uint32_t batteryLastAttemptMs = 0;
  device_time::SyncState timeSyncState = device_time::SyncState::Unsynchronized;
  bool use24Hour = true;
  uint8_t timezoneIndex = 0;
  time_t lastSuccessfulTimeSync = 0;
  weather::Snapshot weather{};
  weather::WizardSnapshot weatherWizard{};
  location::Snapshot location{};
  power::Snapshot lowPower{};
  telemetry::Snapshot telemetry{};
  uint32_t nextPollSeconds = 0;
  uint8_t systemsSection = 0;
  bool manualRefreshRateLimited = false;
  RefreshMode refreshMode = RefreshMode::Balanced;
  uint32_t displayGc16DurationMs = 0;
  uint32_t displayCleanupDurationMs = 0;
  uint32_t displayPageTransitionDurationMs = 0;
  uint32_t displayTouchToActionMs = 0;
  uint32_t displayQueuedActionCount = 0;
  uint32_t displayCoalescedRenderCount = 0;
  const char* firmwareId = "field-terminal-ui-1";
  const char* buildDate = "unknown";
  const char* buildTime = "unknown";
};

bool materiallyDifferent(const UiSnapshot& previous, const UiSnapshot& next);

const char* pageName(Page page);

}  // namespace ui