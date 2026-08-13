#include "ui_state.h"

#include <math.h>
#include <string.h>

namespace ui {

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
  // Page changes are owned by UiController::requestPage(). Snapshot updates
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
      a.batteryPercentAvailable != b.batteryPercentAvailable ||
      (a.batteryPercentAvailable && a.batteryPercent != b.batteryPercent) ||
      a.batteryClassification != b.batteryClassification ||
      a.weather.state != b.weather.state ||
      a.weather.dataAvailable != b.weather.dataAvailable ||
      (a.weather.dataAvailable &&
       (a.weather.temperatureTenths != b.weather.temperatureTenths ||
        a.weather.weatherCode != b.weather.weatherCode ||
        strcmp(a.weather.city, b.weather.city))) ||
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
  for (uint8_t i = 0; i < a.telemetry.diskCount; ++i) {
    if (changed(a.telemetry.disks[i].usedPercent, b.telemetry.disks[i].usedPercent, 1.0)) return true;
  }
  return false;
}

}  // namespace ui