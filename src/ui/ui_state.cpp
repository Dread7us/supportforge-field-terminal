#include "ui_state.h"

#include <math.h>
#include <string.h>

namespace ui {

const char* refreshModeName(RefreshMode mode) {
  switch (mode) {
    case RefreshMode::QuickNavigation: return "QUICK NAVIGATION";
    case RefreshMode::Balanced: return "BALANCED";
    case RefreshMode::BeautifulClean: return "BEAUTIFUL";
  }
  return "BALANCED";
}

const char* refreshModeSummary(RefreshMode mode) {
  switch (mode) {
    case RefreshMode::QuickNavigation: return "FAST: FULL GC16; NO NAVIGATION CLEANUP";
    case RefreshMode::Balanced: return "FULL GC16; BOUNDED LAYOUT CLEANUP";
    case RefreshMode::BeautifulClean: return "FULL GC16; CLEAN EACH PAGE REPLACEMENT";
  }
  return "FAST: FULL GC16; NO NAVIGATION CLEANUP";
}

const char* pageName(Page page) {
  switch (page) {
    case Page::Home: return "HOME";
    case Page::Systems: return "SYSTEMS";
    case Page::Radio: return "RADIO";
    case Page::Location: return "LOCATION";
    case Page::Device: return "DEVICE";
    case Page::Diagnostics: return "HARDWARE DIAGNOSTICS";
    case Page::DisplayCalibration: return "DISPLAY CALIBRATION";
    case Page::TextQualification: return "TEXT_QUALIFICATION";
    case Page::Settings: return "SETTINGS";
    case Page::TouchSetup: return "TOUCH SETUP";
    case Page::TouchRecalibrateConfirm: return "RECALIBRATE TOUCH";
    case Page::WeatherSetup: return "WEATHER SETUP";
    case Page::SystemHealth: return "SYSTEM HEALTH";
    case Page::SystemMetrics: return "SYSTEM METRICS";
    case Page::Storage: return "STORAGE";
    case Page::Network: return "NETWORK";
    case Page::WeatherDetail: return "WEATHER DETAIL";
    case Page::Battery: return "BATTERY";
    case Page::VehicleMotion: return "VEHICLE MOTION";
    case Page::Altimeter: return "GPS ELEVATION";
    case Page::TimezoneSetup: return "TIMEZONE SETUP";
    case Page::LowPowerSetup: return "LOW POWER MODE";
    case Page::LowPowerStatus: return "LOW POWER STATUS";
    case Page::DisplayRefreshMode: return "DISPLAY REFRESH MODE";
    case Page::DateTimeSettings: return "DATE & TIME";
    case Page::UnitsSettings: return "UNITS";
    case Page::LocationPrivacySettings: return "LOCATION & PRIVACY";
    case Page::WifiSettings: return "WI-FI";
    case Page::WifiNetworks: return "WI-FI NETWORKS";
    case Page::WifiEntry: return "WI-FI ENTRY";
    case Page::WifiForgetConfirm: return "FORGET WI-FI";
    case Page::Calculator: return "CALCULATOR";
    case Page::DisplaySettings: return "DISPLAY";
  }
  return "HOME";
}

namespace {
bool changed(const telemetry::NumericValue& a, const telemetry::NumericValue& b,
             double threshold) {
  return a.available != b.available ||
         (a.available && fabs(a.value - b.value) >= threshold);
}

double temperatureThreshold(const UiSnapshot& snapshot) {
  return snapshot.telemetry.displayTemperatureUnit == appconfig::TemperatureUnit::Celsius
      ? 1.8 : 1.0;
}
}

bool materiallyDifferent(const UiSnapshot& a, const UiSnapshot& b) {
  // Page changes are owned by DisplayCoordinator::requestPage(). Snapshot updates
  // retain the active page, so comparing it here would require a large
  // temporary UiSnapshot on Arduino's loop-task stack.
  if (a.configured != b.configured ||
      a.rtcValid != b.rtcValid || a.hour != b.hour || a.minute != b.minute ||
      a.day != b.day || a.month != b.month || a.year != b.year ||
      a.timeSyncState != b.timeSyncState || a.use24Hour != b.use24Hour ||
      a.timezoneIndex != b.timezoneIndex ||
      a.lastSuccessfulTimeSync != b.lastSuccessfulTimeSync ||
      a.telemetry.fetchState != b.telemetry.fetchState ||
      a.telemetry.activeEndpoint != b.telemetry.activeEndpoint ||
      a.telemetry.diagnostic != b.telemetry.diagnostic ||
      a.telemetry.consecutiveFailedCycles != b.telemetry.consecutiveFailedCycles ||
      a.telemetry.wifiState != b.telemetry.wifiState ||
      a.telemetry.displayTemperatureUnit != b.telemetry.displayTemperatureUnit ||
      a.systemsSection != b.systemsSection ||
      a.touchMappingVerified != b.touchMappingVerified ||
      a.touchSetupStep != b.touchSetupStep ||
      a.touchSetupReady != b.touchSetupReady ||
      a.batteryVisual.percentAvailable != b.batteryVisual.percentAvailable ||
      (a.batteryVisual.percentAvailable && a.batteryVisual.percent != b.batteryVisual.percent) ||
       a.batteryVisual.source != b.batteryVisual.source ||
       a.batteryVisual.state != b.batteryVisual.state ||
       a.batteryVisual.charging != b.batteryVisual.charging ||
       a.batteryRawSocAvailable != b.batteryRawSocAvailable ||
       (a.batteryRawSocAvailable && a.batteryRawSocPercent != b.batteryRawSocPercent) ||
       a.batteryCapacityRatioAvailable != b.batteryCapacityRatioAvailable ||
       (a.batteryCapacityRatioAvailable &&
        a.batteryCapacityRatioPercent != b.batteryCapacityRatioPercent) ||
       a.batteryFullEvidence != b.batteryFullEvidence ||
       a.batterySampleValid != b.batterySampleValid ||
       a.batteryChargeStatusVerified != b.batteryChargeStatusVerified ||
       a.batteryChargerConnection != b.batteryChargerConnection ||
       a.batteryChargePhase != b.batteryChargePhase ||
       a.batteryDiagnosis != b.batteryDiagnosis ||
       a.batteryVoltageAvailable != b.batteryVoltageAvailable ||
       (a.batteryVoltageAvailable && a.batteryVoltageMillivolts != b.batteryVoltageMillivolts) ||
       a.batteryCurrentAvailable != b.batteryCurrentAvailable ||
       (a.batteryCurrentAvailable && a.batteryAverageCurrentMilliamps != b.batteryAverageCurrentMilliamps) ||
       a.batteryRemainingCapacityStatus != b.batteryRemainingCapacityStatus ||
       a.batteryFullChargeCapacityStatus != b.batteryFullChargeCapacityStatus ||
       a.batteryDesignCapacityStatus != b.batteryDesignCapacityStatus ||
       a.batteryCapacityAvailable != b.batteryCapacityAvailable ||
       a.batteryRemainingCapacityMah != b.batteryRemainingCapacityMah ||
       a.batteryFullChargeCapacityMah != b.batteryFullChargeCapacityMah ||
       a.batteryDesignCapacityMah != b.batteryDesignCapacityMah ||
      a.weather.state != b.weather.state ||
       a.weather.source != b.weather.source ||
       a.weather.configured != b.weather.configured ||
       a.weather.temperatureUnit != b.weather.temperatureUnit ||
       a.weather.showTemperature != b.weather.showTemperature ||
       a.weather.showCondition != b.weather.showCondition ||
       a.weather.showCity != b.weather.showCity ||
       a.weather.showFeelsLike != b.weather.showFeelsLike ||
      a.weather.searchState != b.weather.searchState ||
      a.weather.resultCount != b.weather.resultCount ||
      strcmp(a.weather.city, b.weather.city) || strcmp(a.weather.region, b.weather.region) ||
      strcmp(a.weather.country, b.weather.country) || strcmp(a.weather.postal, b.weather.postal) ||
      a.weather.dataAvailable != b.weather.dataAvailable ||
      a.weather.feelsLikeAvailable != b.weather.feelsLikeAvailable ||
      a.weather.highLowAvailable != b.weather.highLowAvailable ||
      a.weather.humidityAvailable != b.weather.humidityAvailable ||
      a.weather.windAvailable != b.weather.windAvailable ||
      a.weather.precipitationAvailable != b.weather.precipitationAvailable ||
      (a.weather.dataAvailable &&
       (a.weather.temperatureTenths != b.weather.temperatureTenths ||
        a.weather.feelsLikeTenths != b.weather.feelsLikeTenths ||
        a.weather.highTenths != b.weather.highTenths || a.weather.lowTenths != b.weather.lowTenths ||
        a.weather.humidityPercent != b.weather.humidityPercent ||
        a.weather.windSpeedTenths != b.weather.windSpeedTenths ||
        a.weather.windDirectionDegrees != b.weather.windDirectionDegrees ||
        a.weather.precipitationPercent != b.weather.precipitationPercent ||
        a.weather.weatherCode != b.weather.weatherCode)) ||
       a.location.version != b.location.version ||
       a.lowPower.version != b.lowPower.version ||
       a.frontLight.version != b.frontLight.version ||
       a.pressFeedback.active != b.pressFeedback.active ||
       a.pressFeedback.acceptedAtMs != b.pressFeedback.acceptedAtMs ||
       a.wifi.version != b.wifi.version ||
       strcmp(a.wifiEntrySsid, b.wifiEntrySsid) ||
       a.wifiPasswordLength != b.wifiPasswordLength ||
       a.wifiEditingPassword != b.wifiEditingPassword ||
       a.wifiKeyboardPage != b.wifiKeyboardPage ||
       strcmp(a.calculatorDisplay, b.calculatorDisplay) ||
       a.calculatorError != b.calculatorError ||
       a.manualRefreshRateLimited != b.manualRefreshRateLimited ||
       a.refreshMode != b.refreshMode ||
       a.weatherWizard.active != b.weatherWizard.active ||
       a.weatherWizard.step != b.weatherWizard.step ||
       a.weatherWizard.inputKind != b.weatherWizard.inputKind ||
       strcmp(a.weatherWizard.input, b.weatherWizard.input) ||
       strcmp(a.weatherWizard.latitude, b.weatherWizard.latitude) ||
       strcmp(a.weatherWizard.longitude, b.weatherWizard.longitude) ||
       strcmp(a.weatherWizard.country, b.weatherWizard.country) ||
       strcmp(a.weatherWizard.error, b.weatherWizard.error) ||
       a.weatherWizard.characterPage != b.weatherWizard.characterPage ||
       a.telemetry.host.available != b.telemetry.host.available ||
      (a.telemetry.host.available && strcmp(a.telemetry.host.value, b.telemetry.host.value)) ||
      a.telemetry.explicitSystemStatus != b.telemetry.explicitSystemStatus ||
      a.telemetry.systemStatus.available != b.telemetry.systemStatus.available ||
      (a.telemetry.systemStatus.available &&
       strcmp(a.telemetry.systemStatus.value, b.telemetry.systemStatus.value)) ||
      changed(a.telemetry.cpuLoad, b.telemetry.cpuLoad, 2.0) ||
      changed(a.telemetry.ramPercent, b.telemetry.ramPercent, 1.0) ||
      changed(a.telemetry.cpuTemperature, b.telemetry.cpuTemperature,
              temperatureThreshold(b)) ||
      changed(a.telemetry.nvmeTemperature, b.telemetry.nvmeTemperature,
              temperatureThreshold(b)) ||
      a.telemetry.diskCount != b.telemetry.diskCount ||
      changed(a.telemetry.speedTest.down, b.telemetry.speedTest.down, 0.01) ||
      changed(a.telemetry.speedTest.up, b.telemetry.speedTest.up, 0.01) ||
      changed(a.telemetry.speedTest.ping, b.telemetry.speedTest.ping, 0.01) ||
      a.telemetry.speedTest.isRunningAvailable != b.telemetry.speedTest.isRunningAvailable ||
      a.telemetry.speedTest.isRunning != b.telemetry.speedTest.isRunning) return true;
  for (uint8_t i = 0; i < b.weather.resultCount; ++i) {
    if (strcmp(a.weather.results[i].label, b.weather.results[i].label)) return true;
  }
  for (uint8_t i = 0; i < a.telemetry.diskCount; ++i) {
    if (changed(a.telemetry.disks[i].usedPercent, b.telemetry.disks[i].usedPercent, 1.0)) return true;
  }
  return false;
}

}  // namespace ui