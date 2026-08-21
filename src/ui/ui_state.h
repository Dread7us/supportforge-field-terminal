#pragma once

#include <stdint.h>

#include "telemetry/telemetry_model.h"
#include "battery/battery_manager.h"
#include "time/time_service.h"
#include "weather/weather_manager.h"
#include "weather/weather_wizard.h"
#include "location/gps_manager.h"
#include "power/low_power_manager.h"
#include "power/front_light_manager.h"
#include "network/wifi_manager.h"
#include "ui_theme.h"

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
  DisplayRefreshMode,
  DateTimeSettings,
  UnitsSettings,
  LocationPrivacySettings,
  WifiSettings,
  WifiNetworks,
  WifiEntry,
  WifiForgetConfirm,
  Calculator,
  DisplaySettings
};
enum class Presence : uint8_t { Unknown, NotPresent, Observed };
enum class RefreshMode : uint8_t { QuickNavigation, Balanced, BeautifulClean };

struct PressFeedback {
  bool active = false;
  bool destinationRoute = false;
  Page sourcePage = Page::Home;
  Page targetPage = Page::Home;
  Rect bounds{};
  uint8_t radius = 10;
  uint32_t acceptedAtMs = 0;
  uint32_t expiresAtMs = 0;
  char label[40]{};
};

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
  battery::ChargerConnection batteryChargerConnection = battery::ChargerConnection::Unknown;
  battery::ChargePhase batteryChargePhase = battery::ChargePhase::Unknown;
  battery::Diagnosis batteryDiagnosis = battery::Diagnosis::None;
  bool batteryVoltageAvailable = false;
  uint16_t batteryVoltageMillivolts = 0;
  bool batteryCurrentAvailable = false;
  int16_t batteryAverageCurrentMilliamps = 0;
  bool batteryCapacityAvailable = false;
  uint16_t batteryRemainingCapacityMah = 0;
  uint16_t batteryFullChargeCapacityMah = 0;
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
  power::FrontLightSnapshot frontLight{};
  PressFeedback pressFeedback{};
  telemetry::Snapshot telemetry{};
  network::Snapshot wifi{};
  char wifiEntrySsid[network::kMaximumSsidBytes + 1]{};
  uint8_t wifiPasswordLength = 0;
  bool wifiEditingPassword = false;
  uint8_t wifiKeyboardPage = 0;
  char calculatorDisplay[32] = "0";
  bool calculatorError = false;
  uint32_t nextPollSeconds = 0;
  uint8_t systemsSection = 0;
  bool manualRefreshRateLimited = false;
  uint32_t manualRefreshRemainingSeconds = 0;
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