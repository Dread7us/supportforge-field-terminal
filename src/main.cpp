#include <Arduino.h>
#include <RadioLib.h>
#include <Preferences.h>
#include <SD.h>
#include <SPI.h>
#include <Wire.h>
#include <epdiy.h>
#include <esp_system.h>
#include <WiFi.h>

#ifndef SUPPORTFORGE_PERF_DIAGNOSTICS
#define SUPPORTFORGE_PERF_DIAGNOSTICS 0
#endif

#include "board_profile.h"
#include "app_config.h"
#include "input/touch_controller.h"
#include "location/gps_manager.h"
#include "power/low_power_manager.h"
#include "battery/battery_manager.h"
#include "telemetry/telemetry_manager.h"
#include "time/time_service.h"
#include "ui/ui_components.h"
#include "ui/ui_controller.h"
#include "weather/weather_manager.h"
#include "weather/weather_wizard.h"
#include "network/wifi_manager.h"
#include "utilities/calculator.h"

// UiSnapshot intentionally contains bounded Guardian, weather, time, and
// hardware state. Give Arduino's loop task enough stack for those value
// snapshots and rendering call frames; the framework default is 8192 bytes.
SET_LOOP_TASK_STACK_SIZE(16384);

namespace {

SX1262 radio = new Module(hq::kBoard.loraCs, hq::kBoard.loraIrq,
                          hq::kBoard.loraReset, hq::kBoard.loraBusy);
HardwareSerial gpsSerial(1);

bool sharedRailEnabled = false;
bool radioListening = false;
bool touchObserved = false;
bool rtcObserved = false;
bool chargerObserved = false;
bool gaugeObserved = false;
bool pcaObserved = false;
bool sdObserved = false;
bool gpsObserved = false;
bool gpsFixObserved = false;
bool radioObserved = false;

constexpr const char *kFirmwareId = "UI QUAL 3";
// Revision is retained as audit metadata, but panel history survives MCU resets
// independently of NVS. Every boot therefore performs one bounded physical
// cleanup before the first usable render; matching metadata must never suppress it.
constexpr uint32_t kDisplayCleanupRevision = 11;
Preferences bootPreferences;
bool bootCleanupPending = false;
ui::DisplayCoordinator displayCoordinator;
input::TouchController touchController;
telemetry::TelemetryManager telemetryManager;
battery::BatteryManager batteryManager;
device_time::TimeService timeService;
weather::WeatherManager weatherManager;
power::LowPowerManager lowPowerManager;
location::GpsManager gpsManager(gpsSerial);
weather::WeatherWizard weatherWizard;
network::WifiManager wifiManager;
utilities::Calculator calculator;
bool wifiEditingPassword = false;
uint8_t wifiKeyboardPage = 0;
ui::Page weatherWizardReturnPage = ui::Page::Home;
ui::Page detailReturnPage = ui::Page::Home;
ui::Page timezoneReturnPage = ui::Page::Settings;
ui::Page wifiReturnPage = ui::Page::Settings;
uint8_t systemsSection = 0;
uint32_t observedTelemetryVersion = 0;
uint32_t observedTimeVersion = 0;
uint32_t observedWeatherVersion = 0;
uint32_t observedBatteryVersion = 0;
uint32_t observedGpsVersion = 0;
uint32_t observedLowPowerVersion = 0;
uint32_t observedWifiVersion = 0;
uint32_t nextPerformanceReportMs = 0;
uint32_t firstScreenRenderedAtMs = 0;
bool touchReadyReported = false;
bool localServicesInitialized = false;
bool backgroundWorkersStarted = false;

bool setSharedRail(bool enabled, bool persistGpsPreference = true);

ui::Presence presence(bool observed) {
  return observed ? ui::Presence::Observed : ui::Presence::Unknown;
}

ui::UiSnapshot makeUiSnapshot() {
  ui::UiSnapshot state;
  state.touch = presence(touchObserved);
  state.rtc = presence(rtcObserved);
  state.fuelGauge = presence(gaugeObserved);
  state.storage = presence(sdObserved);
  state.gps = presence(gpsObserved);
  state.gpsFix = gpsFixObserved;
  state.radio = presence(radioObserved);
  state.radioListening = radioListening;
  state.sharedRailEnabled = sharedRailEnabled;
  state.touchMappingVerified = touchController.mappingVerified();
  state.touchSetupStep = touchController.qualificationStep();
  state.touchSetupReady = touchController.mappingVerified() && !touchController.qualifying();
  state.psramAvailable = ESP.getPsramSize() > 0;
  const battery::Snapshot battery = batteryManager.snapshot();
  state.batteryState = battery.state;
  state.batteryPercentAvailable = battery.percentAvailable;
  state.batteryPercent = battery.percent;
  state.batterySampleAttempted = battery.sampleAttempted;
  state.batterySampleValid = battery.sampleValid;
  state.batteryChargeStatusVerified = battery.chargeStatusVerified;
  state.batteryLastSampleMs = battery.lastSampleMs;
  state.batteryLastAttemptMs = battery.lastAttemptMs;
  const device_time::Snapshot clock = timeService.snapshot();
  state.rtcValid = clock.valid;
  state.hour = clock.hour;
  state.minute = clock.minute;
  state.day = clock.day;
  state.month = clock.month;
  state.year = clock.year;
  state.timeSyncState = clock.syncState;
  state.use24Hour = clock.use24Hour;
  state.timezoneIndex = clock.timezoneIndex;
  state.lastSuccessfulTimeSync = clock.lastSuccessfulSync;
  state.weather = weatherManager.snapshot();
  state.weatherWizard = weatherWizard.snapshot();
  state.location = gpsManager.snapshot();
  state.lowPower = lowPowerManager.snapshot();
  state.gps = state.location.state == location::GpsState::Off ? ui::Presence::Unknown :
              (state.location.state == location::GpsState::Error ? ui::Presence::NotPresent : ui::Presence::Observed);
  state.gpsFix = state.location.fixValid;
  state.gpsSatellites = state.location.satellitesValid ? state.location.satellites : 0;
  state.configured = appconfig::configured();
  state.telemetry = telemetryManager.snapshot();
  state.wifi = wifiManager.snapshot();
  strlcpy(state.wifiEntrySsid,wifiManager.entrySsid(),sizeof(state.wifiEntrySsid));
  state.wifiPasswordLength = static_cast<uint8_t>(wifiManager.passwordLength());
  state.wifiEditingPassword = wifiEditingPassword;
  state.wifiKeyboardPage = wifiKeyboardPage;
  strlcpy(state.calculatorDisplay,calculator.display().c_str(),sizeof(state.calculatorDisplay));
  state.calculatorError = calculator.error();
  state.nextPollSeconds = telemetryManager.nextPollInSeconds(millis());
  state.systemsSection = systemsSection;
  state.manualRefreshRateLimited = !displayCoordinator.manualRefreshAvailable(millis());
  state.manualRefreshRemainingSeconds = displayCoordinator.manualRefreshRemainingSeconds(millis());
  state.refreshMode = displayCoordinator.refreshMode();
  state.displayGc16DurationMs = displayCoordinator.lastGc16DurationMs();
  state.displayCleanupDurationMs = displayCoordinator.lastFullCleanupDurationMs();
  state.displayPageTransitionDurationMs = displayCoordinator.lastPageTransitionDurationMs();
  state.displayTouchToActionMs = displayCoordinator.lastTouchToActionMs();
  state.displayQueuedActionCount = touchController.queuedActionCount();
  state.displayCoalescedRenderCount = displayCoordinator.coalescedRenderCount();
  state.firmwareId = kFirmwareId;
  state.buildDate = __DATE__;
  state.buildTime = __TIME__;
  return state;
}

void syncUiState() { displayCoordinator.setSnapshot(makeUiSnapshot()); }

bool guardianCritical(const telemetry::Snapshot& state) {
  const bool explicitOffline = state.explicitSystemStatus && state.systemStatus.available &&
      strcasecmp(state.systemStatus.value, "OFFLINE") == 0;
  const bool confirmedFailures = state.fetchState == telemetry::FetchState::Offline &&
      state.consecutiveFailedCycles >= appconfig::kOfflineFailedCycles;
  return explicitOffline || confirmedFailures || state.fetchState == telemetry::FetchState::AuthError;
}

void applyLowPowerPolicy(uint32_t nowMs, const telemetry::Snapshot& telemetryState) {
  lowPowerManager.setCriticalHold(guardianCritical(telemetryState), nowMs);
  lowPowerManager.poll(nowMs);
  const power::Snapshot state = lowPowerManager.snapshot();
  const bool servicesAwake = !state.active || state.awakeWindow || state.criticalHold;
  telemetryManager.setSuspended(!servicesAwake);
  weatherManager.setSuspended(!servicesAwake);
  if (servicesAwake || (telemetryManager.idle() && weatherManager.idle()))
    wifiManager.setServicesAllowed(servicesAwake);
  if (state.active) {
    if (radioListening) {
      radio.sleep();
      radioListening = false;
    }
  }
  const bool gpsWeatherRequired = weatherManager.snapshot().source == weather::LocationSource::Gps;
  if ((gpsWeatherRequired || (!state.active && gpsManager.enabledPreference())) && !sharedRailEnabled) {
    setSharedRail(true, false);
  } else if (state.active && !gpsWeatherRequired && sharedRailEnabled && !radioListening) {
    setSharedRail(false, false);
  }
}

const char *resetReasonName(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON: return "POWERON";
    case ESP_RST_EXT: return "EXTERNAL";
    case ESP_RST_SW: return "SOFTWARE";
    case ESP_RST_PANIC: return "PANIC";
    case ESP_RST_INT_WDT: return "INT_WATCHDOG";
    case ESP_RST_TASK_WDT: return "TASK_WATCHDOG";
    case ESP_RST_WDT: return "WATCHDOG";
    case ESP_RST_BROWNOUT: return "BROWNOUT";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    default: return "OTHER";
  }
}

bool i2cPresent(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

bool i2cReadRegister(uint8_t address, uint8_t reg, uint8_t &value) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(address, static_cast<uint8_t>(1)) != 1) return false;
  value = Wire.read();
  return true;
}

bool i2cWriteRegister(uint8_t address, uint8_t reg, uint8_t value) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

void printResult(const char *subsystem, const char *status,
                 const String &detail) {
  Serial.printf("RESULT subsystem=%s status=%s detail=\"%s\"\n", subsystem,
                status, detail.c_str());
}

void printProfile() {
  Serial.println("\n=== supportFORGE H752-02 hardware qualification ===");
  Serial.println("Product meaning: H752-02 = SX1262 915 MHz + L76K GPS");
  Serial.printf("Candidate profile: %s\nSource: %s\n", hq::kBoard.name,
                hq::kBoard.source);
  Serial.printf("I2C SDA=%d SCL=%d | SPI MISO=%d MOSI=%d SCLK=%d\n",
                hq::kBoard.i2cSda, hq::kBoard.i2cScl, hq::kBoard.spiMiso,
                hq::kBoard.spiMosi, hq::kBoard.spiSclk);
  Serial.printf("LoRa CS=%d IRQ=%d RST=%d BUSY=%d frequency=%.1f MHz\n",
                hq::kBoard.loraCs, hq::kBoard.loraIrq,
                hq::kBoard.loraReset, hq::kBoard.loraBusy,
                hq::kLoRaFrequencyMHz);
  if (hq::kBoard.gpsCandidateAvailable) {
    Serial.printf("GPS candidate RX=%d TX=%d (UART observation is RX-only)\n",
                  hq::kBoard.gpsRx, hq::kBoard.gpsTx);
  } else {
    Serial.println("GPS: unavailable in this comparison profile; no pins guessed");
  }
  Serial.println("Boot policy: one bounded every-boot panel cleanup before usable-page GC16, background services, no RF transmit or GPS TX.");
}

void printHelp() {
  Serial.println("\nCommands:");
  Serial.println("  profile       - print candidate source and pins");
  Serial.println("  i2c           - passive I2C address scan");
  Serial.println("  rail on       - opt in to LoRa/GPS shared 3.3 V rail (current Pro)");
  Serial.println("  rail off      - disable that rail");
  Serial.println("  lora probe    - initialize SX1262 at 915 MHz, then sleep (no TX)");
  Serial.println("  lora rx       - receive-only mode at 915 MHz");
  Serial.println("  lora stop     - put radio to sleep");
  Serial.println("  gps listen    - RX-only 9600-baud NMEA observation for 15 seconds");
  Serial.println("  display test  - render HOME with GC16, then power off");
  Serial.println("  display white-test - once-per-boot cleanup/white diagnostic");
  Serial.println("  page <name>   - HOME/SYSTEMS/RADIO/LOCATION/DEVICE/ALTIMETER/DIAGNOSTICS");
  Serial.println("  framebuffer dump <name> - emit an unrefreshed packed 4-bpp page buffer");
  Serial.println("  touch test    - one redacted coordinate observation");
  Serial.println("  touch debug on/off - explicit raw/transformed coordinate diagnostics");
  Serial.println("  touch reset/status - reset qualification or print local state");
  Serial.println("  help");
}

void scanI2c() {
  unsigned found = 0;
  Serial.println("I2C scan start (read/address probes only)");
  for (uint8_t address = 1; address < 0x7F; ++address) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      Serial.printf("  found 0x%02X\n", address);
      ++found;
    }
  }
  printResult("i2c", found ? "OBSERVED" : "NOT_OBSERVED",
              String(found) + " responding address(es)");
}

void identifyI2c() {
  pcaObserved = i2cPresent(0x20);
  rtcObserved = i2cPresent(0x51);
  chargerObserved = i2cPresent(0x6B);
  gaugeObserved = i2cPresent(0x55);
  const bool touch14 = i2cPresent(0x14);
  const bool touch5d = i2cPresent(0x5D);
  touchObserved = touch14 || touch5d;
  printResult("pca9535", pcaObserved ? "OBSERVED" : "NOT_PRESENT", "address 0x20");
  printResult("rtc", rtcObserved ? "OBSERVED" : "NOT_PRESENT",
              rtcObserved ? "PCF8563-compatible device at 0x51" : "no response at 0x51");
  printResult("bq25896", chargerObserved ? "OBSERVED" : "NOT_PRESENT", "address 0x6B");
  printResult("bq27220", gaugeObserved ? "OBSERVED" : "NOT_PRESENT",
              gaugeObserved ? "presence verified; read-only SOC sampling enabled"
                            : "fuel gauge did not respond");
  printResult("gt911", touchObserved ? "OBSERVED" : "NOT_PRESENT",
              touch14 ? "address 0x14" : (touch5d ? "address 0x5D" : "no response at 0x14/0x5D"));
}

void testSd() {
  pinMode(hq::kBoard.loraCs, OUTPUT);
  digitalWrite(hq::kBoard.loraCs, HIGH);
  SPI.begin(hq::kBoard.spiSclk, hq::kBoard.spiMiso, hq::kBoard.spiMosi);
  if (!SD.begin(hq::kBoard.sdCs, SPI, 1000000)) {
    printResult("microsd", "NOT_PRESENT", "read-only qualification mount failed");
    SPI.end();
    return;
  }
  sdObserved = SD.cardType() != CARD_NONE;
  printResult("microsd", sdObserved ? "OBSERVED" : "NOT_PRESENT",
              sdObserved ? "card initialized; no file created" : "interface initialized; no card");
  SD.end();
  SPI.end();  // Release GPIO matrix ownership before the next EPD refresh.
}

bool setSharedRail(bool enabled, bool persistGpsPreference) {
  if (!hq::kBoard.hasPca9535) {
    printResult("shared_rail", "NOT_APPLICABLE",
                "legacy H752 source has no PCA9535 rail definition");
    return false;
  }

  uint8_t output = 0;
  uint8_t config = 0;
  if (!i2cReadRegister(hq::kPca9535Address, hq::kPcaPort0Output, output) ||
      !i2cReadRegister(hq::kPca9535Address, hq::kPcaPort0Config, config)) {
    printResult("shared_rail", "FAILED",
                "PCA9535 did not respond at candidate address 0x20");
    return false;
  }

  const uint8_t newOutput = enabled ? output | hq::kSharedRailBit
                                    : output & ~hq::kSharedRailBit;
  const uint8_t newConfig = config & ~hq::kSharedRailBit;
  // Set the output latch before changing the pin direction to avoid a glitch.
  if (!i2cWriteRegister(hq::kPca9535Address, hq::kPcaPort0Output, newOutput) ||
      !i2cWriteRegister(hq::kPca9535Address, hq::kPcaPort0Config, newConfig)) {
    printResult("shared_rail", "FAILED", "PCA9535 register write failed");
    return false;
  }
  sharedRailEnabled = enabled;
  gpsManager.setHardwareEnabled(enabled, persistGpsPreference);
  delay(20);
  printResult("shared_rail", "COMMAND_ACCEPTED",
              enabled ? "candidate P0.0 driven high" : "candidate P0.0 driven low");
  return true;
}

int configureRadio() {
  pinMode(hq::kBoard.sdCs, OUTPUT);
  digitalWrite(hq::kBoard.sdCs, HIGH);
  pinMode(hq::kBoard.loraCs, OUTPUT);
  digitalWrite(hq::kBoard.loraCs, HIGH);
  SPI.begin(hq::kBoard.spiSclk, hq::kBoard.spiMiso, hq::kBoard.spiMosi);

  int state = radio.begin(hq::kLoRaFrequencyMHz, 125.0, 9, 7,
                          RADIOLIB_SX126X_SYNC_WORD_PRIVATE, 10, 8, 2.4, false);
  if (state != RADIOLIB_ERR_NONE) return state;
  return radio.setDio2AsRfSwitch();
}

void releaseSharedSpiForDisplay() {
  if (radioListening) {
    radio.sleep();
    radioListening = false;
    Serial.println("SX1262 receive paused: display owns overlapping candidate GPIOs");
  }
  SPI.end();
}

void probeRadio(bool receive) {
  if (hq::kBoard.hasPca9535 && !sharedRailEnabled) {
    printResult("sx1262", "BLOCKED", "run 'rail on' first");
    return;
  }
  const int state = configureRadio();
  if (state != RADIOLIB_ERR_NONE) {
    printResult("sx1262", "NOT_OBSERVED",
                "RadioLib init code " + String(state));
    return;
  }
  radioObserved = true;
  if (receive) {
    const int rxState = radio.startReceive();
    radioListening = rxState == RADIOLIB_ERR_NONE;
    printResult("sx1262", radioListening ? "OBSERVED_RX" : "FAILED",
                "915 MHz receive-only start code " + String(rxState));
  } else {
    radio.sleep();
    printResult("sx1262", "OBSERVED",
                "initialized at 915 MHz; no packet transmitted; now asleep");
  }
}

void startGpsObservation() {
  if (!hq::kBoard.gpsCandidateAvailable) {
    printResult("l76k", "BLOCKED", "profile has no sourced GPS pin definition");
    return;
  }
  if (!sharedRailEnabled) {
    printResult("l76k", "BLOCKED", "run 'rail on' first");
    return;
  }
  // TX is deliberately -1: qualification observes module output and cannot
  // reconfigure the GNSS receiver during this conservative phase.
  gpsSerial.begin(9600, SERIAL_8N1, hq::kBoard.gpsRx, -1);
  Serial.println("GPS RX-only observation started for 15 seconds; move outdoors for a fix.");

  const uint32_t deadline = millis() + 15000;
  size_t bytes = 0;
  size_t lines = 0;
  bool nmea = false;
  bool validFix = false;
  String line;
  while (static_cast<int32_t>(deadline - millis()) > 0) {
    while (gpsSerial.available()) {
      const char c = static_cast<char>(gpsSerial.read());
      ++bytes;
      if (c == '\n') ++lines;
      if (c == '\n') {
        if (line.startsWith("$GP") || line.startsWith("$GN")) nmea = true;
        if (line.indexOf("GGA") > 0) {
          int field = 0;
          for (int i = 0; i < line.length(); ++i) {
            if (line[i] == ',' && ++field == 6 && i + 1 < line.length() && line[i + 1] != '0' && line[i + 1] != ',') validFix = true;
          }
        }
        line = "";
      } else if (line.length() < 120) line += c;
    }
    delay(2);
  }
  gpsSerial.end();
  gpsObserved = nmea;
  gpsFixObserved = validFix;
  printResult("l76k", !bytes ? "NOT_PRESENT" : (validFix ? "PASS" : (nmea ? "NO_FIX" : "OBSERVED")),
              String(bytes) + " bytes, " + String(lines) +
              " lines; NMEA payload and coordinates redacted");
  syncUiState();
}
void testDisplay() {
#if defined(HQ_PROFILE_LEGACY_H752)
  printResult("display", "BLOCKED",
              "epd_board_v7 belongs to current Pro baseline, not legacy H752");
#else
  // The current-Pro candidate profile overlaps SPI/radio signals with EPD v7
  // parallel data pins. Never refresh while Arduino SPI owns those GPIOs.
  releaseSharedSpiForDisplay();
  const bool capturingTouch=touchController.beginDisplayCapture();
  const bool cleanupRequested = bootCleanupPending && !displayCoordinator.cleanupUsed();
  const bool rendered = displayCoordinator.renderIfDirty(millis(), cleanupRequested);
  if(capturingTouch)touchController.endDisplayCapture();
  if (rendered && !capturingTouch) touchController.notifyDisplayUpdateFinished(millis());
  if (rendered && cleanupRequested && displayCoordinator.cleanupUsed()) {
    const size_t written = bootPreferences.putUInt("display_rev", kDisplayCleanupRevision);
    bootCleanupPending = written == 0;
    Serial.printf("DISPLAY cleanup_revision=%lu persistence=%s\n",
                  static_cast<unsigned long>(kDisplayCleanupRevision),
                  bootCleanupPending ? "FAILED" : "SAVED");
  }
  printResult("display", rendered ? "COMMAND_ACCEPTED" : "NO_CHANGE",
              "portrait GC16 retained page; high voltage off after update");
#endif
}

void testTouch() {
  if (!touchObserved) { printResult("touch", "NOT_PRESENT", "GT911 address absent"); return; }
  Serial.println("Touch observation: touch panel once within 15 seconds.");
  const bool observed = touchController.observeTouch(15000);
  printResult("touch", observed ? "OBSERVED" : "UNVERIFIED",
              observed
                  ? "coordinate captured and clamped; values redacted; run 'touch reset' to requalify mapping"
                  : "controller present; no coordinate observed");
}

void beginConfirmedTouchRecalibration() {
  // This helper is reachable only from the explicit confirmation page or the
  // deliberate serial command. Normal boot never invalidates saved calibration.
  touchController.resetQualification();
  touchController.startQualification();
  syncUiState();
}

void initializeLocalServices() {
  if (localServicesInitialized) return;
  // This bounded bootstrap runs from loop() only after touch has been sampled.
  // Network negotiation remains on the dedicated Guardian/weather core-0 tasks.
  identifyI2c();
  batteryManager.begin(gaugeObserved, chargerObserved);
  gpsManager.begin(false);
  if (gpsManager.enabledPreference()) setSharedRail(true);
  timeService.begin(rtcObserved);
  if (!lowPowerManager.begin(millis())) {
    printResult("low_power", "FAILED", "NVS preferences unavailable");
  }
  wifiManager.begin();
  const power::Snapshot initialPower = lowPowerManager.snapshot();
  telemetryManager.setSuspended(initialPower.active && !initialPower.awakeWindow);
  weatherManager.setSuspended(initialPower.active && !initialPower.awakeWindow);
  localServicesInitialized = true;
  Serial.println("SERVICES local_startup=COMPLETE execution=AFTER_TOUCH_SAMPLE");
}

void startBackgroundWorkers() {
  if (backgroundWorkersStarted) return;
  // Task creation occurs only after the first unblocked touch sample. Wi-Fi and
  // HTTP work then remain on core 0 and never gate loop() page navigation.
  const bool telemetryStarted = telemetryManager.begin();
  if (!telemetryStarted) printResult("telemetry", "FAILED", "local manager initialization failed");
  const bool weatherStarted = weatherManager.begin();
  if (!weatherStarted) printResult("weather", "FAILED", "local manager initialization failed");
  backgroundWorkersStarted = true;
  Serial.printf("SERVICES startup=BACKGROUND guardian=%s weather=%s navigation_blocking=NO\n",
                telemetryStarted ? "STARTED" : "FAILED",
                weatherStarted ? "STARTED" : "FAILED");
}

bool requestNamedPage(const String& name) {
  ui::Page page;
  if (name == "home") page = ui::Page::Home;
  else if (name == "systems") page = ui::Page::Systems;
  else if (name == "radio") page = ui::Page::Radio;
  else if (name == "location") page = ui::Page::Location;
  else if (name == "device") page = ui::Page::Device;
  else if (name == "diagnostics") page = ui::Page::Diagnostics;
  else if (name == "calibration" || name == "display calibration") page = ui::Page::DisplayCalibration;
  else if (name == "text qualification") page = ui::Page::TextQualification;
  else if (name == "settings") page = ui::Page::Settings;
  else if (name == "weather setup") page = ui::Page::WeatherSetup;
  else if (name == "system health") page = ui::Page::SystemHealth;
  else if (name == "system metrics") page = ui::Page::SystemMetrics;
  else if (name == "storage") page = ui::Page::Storage;
  else if (name == "network") page = ui::Page::Network;
  else if (name == "weather detail") page = ui::Page::WeatherDetail;
  else if (name == "battery") page = ui::Page::Battery;
  else if (name == "vehicle motion") page = ui::Page::VehicleMotion;
  else if (name == "altimeter" || name == "gps elevation") page = ui::Page::Altimeter;
  else if (name == "display refresh mode") page = ui::Page::DisplayRefreshMode;
  else return false;
  if (!displayCoordinator.requestPage(page, millis())) {
    printResult("ui", "RATE_LIMITED", "page unchanged or refresh cooldown active");
    return true;
  }
  testDisplay();
  return true;
}

bool namedPage(const String& name, ui::Page& page) {
  if (name == "home") page = ui::Page::Home;
  else if (name == "systems") page = ui::Page::Systems;
  else if (name == "radio") page = ui::Page::Radio;
  else if (name == "location") page = ui::Page::Location;
  else if (name == "device") page = ui::Page::Device;
  else if (name == "diagnostics") page = ui::Page::Diagnostics;
  else if (name == "calibration" || name == "display calibration") page = ui::Page::DisplayCalibration;
  else if (name == "text qualification") page = ui::Page::TextQualification;
  else if (name == "settings") page = ui::Page::Settings;
  else if (name == "touch setup") page = ui::Page::TouchSetup;
  else if (name == "recalibrate touch") page = ui::Page::TouchRecalibrateConfirm;
  else if (name == "weather setup") page = ui::Page::WeatherSetup;
  else if (name == "system health") page = ui::Page::SystemHealth;
  else if (name == "system metrics") page = ui::Page::SystemMetrics;
  else if (name == "storage") page = ui::Page::Storage;
  else if (name == "network") page = ui::Page::Network;
  else if (name == "weather detail") page = ui::Page::WeatherDetail;
  else if (name == "battery") page = ui::Page::Battery;
  else if (name == "vehicle motion") page = ui::Page::VehicleMotion;
  else if (name == "altimeter" || name == "gps elevation") page = ui::Page::Altimeter;
  else if (name == "display refresh mode") page = ui::Page::DisplayRefreshMode;
  else return false;
  return true;
}

void processCommand(String command) {
  command.trim();
  command.toLowerCase();
  if (command == "profile") printProfile();
  else if (command == "i2c") scanI2c();
  else if (command == "identify") identifyI2c();
  else if (command == "sd test") testSd();
  else if (command == "rail on") setSharedRail(true);
  else if (command == "rail off") setSharedRail(false);
  else if (command == "lora probe") probeRadio(false);
  else if (command == "lora rx") probeRadio(true);
  else if (command == "lora stop") {
    radio.sleep();
    radioListening = false;
    printResult("sx1262", "STOPPED", "radio sleep requested");
  } else if (command == "gps listen") startGpsObservation();
  else if (command == "display test") { syncUiState(); testDisplay(); }
  else if (command == "display white-test") {
    releaseSharedSpiForDisplay();
    const bool capturingTouch=touchController.beginDisplayCapture();
    const bool rendered=displayCoordinator.renderWhiteTest(millis());
    if(capturingTouch)touchController.endDisplayCapture();
    if(rendered&&!capturingTouch)touchController.notifyDisplayUpdateFinished(millis());
    printResult("display_white_test",rendered?"COMMAND_ACCEPTED":(displayCoordinator.whiteTestUsed()?"GUARD_BLOCKED":"FAILED"),
                rendered?"white framebuffer, black border and identifier rendered once":"once-per-boot guard or display failure");
  }
  else if (command == "touch test") testTouch();
  else if (command == "touch debug on") { touchController.setDiagnosticMode(true); printResult("touch", "DIAGNOSTIC_ENABLED", "raw and transformed coordinates may be logged"); }
  else if (command == "touch debug off") { touchController.setDiagnosticMode(false); printResult("touch", "DIAGNOSTIC_DISABLED", "coordinates redacted"); }
  else if (command == "touch reset") { beginConfirmedTouchRecalibration(); displayCoordinator.requestPage(ui::Page::TouchSetup,millis()); testDisplay(); }
  else if (command == "touch status") touchController.printStatus();
  else if (command.startsWith("framebuffer dump ")) {
    ui::Page page;
    const String name = command.substring(17);
    if (!namedPage(name, page)) {
      printResult("framebuffer_dump", "INVALID_PAGE", name);
    } else {
      const bool dumped = displayCoordinator.dumpPackedFramebuffer(page);
      printResult("framebuffer_dump", dumped ? "COMPLETE" : "FAILED",
                  dumped ? "authoritative composition buffer emitted; display not refreshed"
                         : "buffer unavailable or write incomplete");
    }
  }
  else if (command.startsWith("page ") && requestNamedPage(command.substring(5))) {}
  else if (command == "help" || command.isEmpty()) printHelp();
  else Serial.println("Unknown command. Type 'help'.");
}

}  // namespace

void setup() {
  const uint32_t bootStartedMs = millis();
  Serial.begin(115200);

  // Keep shared-bus devices deselected. All other candidate GPIOs remain at
  // reset/input state until the operator invokes a subsystem command.
  pinMode(hq::kBoard.sdCs, OUTPUT);
  digitalWrite(hq::kBoard.sdCs, HIGH);
  pinMode(hq::kBoard.loraCs, OUTPUT);
  digitalWrite(hq::kBoard.loraCs, HIGH);
  Wire.begin(hq::kBoard.i2cSda, hq::kBoard.i2cScl);
  Wire.setClock(100000);

  printProfile();
  Serial.printf("Firmware: %s build=%s %s\n", kFirmwareId, __DATE__, __TIME__);
  Serial.printf("PERF diagnostics=%s\n", SUPPORTFORGE_PERF_DIAGNOSTICS ? "ENABLED" : "DISABLED");
  Serial.println("UI QUAL 3");
  Serial.printf("RESET reason=%s code=%d\n", resetReasonName(esp_reset_reason()), esp_reset_reason());
  Serial.printf("MCU model=%s revision=%d cores=%d cpu=%dMHz flash=%u bytes\n",
                ESP.getChipModel(), ESP.getChipRevision(), ESP.getChipCores(), ESP.getCpuFreqMHz(), ESP.getFlashChipSize());
  Serial.printf("MEM heap_free=%u heap_min=%u heap_largest=%u psram=%u psram_free=%u\n",
                ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap(), ESP.getPsramSize(), ESP.getFreePsram());
  printResult("boot", "OBSERVED", "firmware reached setup; peripherals unconfirmed");
  const uint32_t psramBytes = ESP.getPsramSize();
  printResult("psram", psramBytes ? "OBSERVED" : "NOT_OBSERVED",
              String(psramBytes) + " bytes reported by runtime");
  touchObserved = touchController.begin();
  if (bootPreferences.begin("sf_boot", false)) {
    const uint32_t storedRevision = bootPreferences.getUInt("display_rev", 0);
    bootCleanupPending = true;
    Serial.printf("DISPLAY cleanup_revision stored=%lu required=%lu pending=YES policy=EVERY_BOOT changed=%s\n",
                  static_cast<unsigned long>(storedRevision),
                  static_cast<unsigned long>(kDisplayCleanupRevision),
                  storedRevision == kDisplayCleanupRevision ? "NO" : "YES");
  } else {
    bootCleanupPending = true;
    Serial.println("DISPLAY cleanup_revision storage=UNAVAILABLE pending=YES policy=EVERY_BOOT");
  }
  if (displayCoordinator.begin()) {
    ui::Page initialPage = ui::Page::Home;
    if (!touchController.mappingVerified()) {
      // Qualification state, not controller detection, owns the boot page. If
      // GT911 is absent the visible setup remains truthful instead of presenting
      // an enabled-looking HOME whose navigation silently ignores every touch.
      if (touchObserved) touchController.startQualification();
      initialPage = ui::Page::TouchSetup;
    }
    syncUiState();
    if (initialPage != ui::Page::Home) displayCoordinator.requestPage(initialPage, millis());
    Serial.printf("DISPLAY initial_page=%s touch_qualified=%s\n",
                  ui::pageName(initialPage),
                  touchController.mappingVerified() ? "YES" : "NO");
    // Compose the usable destination directly from the full known-white buffer.
    // renderIfDirty powers HV off before returning. The bounded every-boot panel
    // cleanup is folded into this same first GC16 render rather than adding a screen.
    testDisplay();
    firstScreenRenderedAtMs = millis();
    const uint32_t firstScreenMs = millis() - bootStartedMs;
    Serial.printf("PERF boot_to_first_screen_ms=%lu initial_gc16_count=%lu initial_fullclear_count=%u touch_gate_ms=%d\n",
                  static_cast<unsigned long>(firstScreenMs),
                  static_cast<unsigned long>(displayCoordinator.refreshCount()),
                  displayCoordinator.cleanupUsed() ? 1U : 0U,
                  ui::spec::kTouchPostRefreshQuietMs);

  } else {
    printResult("display", "FAILED", "UI framebuffer initialization failed");
    identifyI2c();
    batteryManager.begin(gaugeObserved, chargerObserved);
    gpsManager.begin(false);
    timeService.begin(rtcObserved);
    lowPowerManager.begin(millis());
    if (!telemetryManager.begin()) printResult("telemetry", "FAILED", "local manager initialization failed");
    if (!weatherManager.begin()) printResult("weather", "FAILED", "local manager initialization failed");
  }
}

void loop() {
  if (Serial.available()) processCommand(Serial.readStringUntil('\n'));
  const uint32_t now = millis();
  const bool captureSettled=touchController.displayCaptureSettled();
  const bool inputBlocked = displayCoordinator.inputBlocked(now) || !captureSettled;
  input::TouchAction action{input::ActionType::None,{0,0},"NONE"};
  // Preserve a captured in-refresh action through the post-update quiet gate;
  // consume it exactly once only when coordinator navigation can accept it.
  const bool queuedAction=captureSettled&&!inputBlocked&&touchController.takeQueuedAction(action);
  if(captureSettled&&!queuedAction)action=touchController.poll(now,inputBlocked);
  if (firstScreenRenderedAtMs && !touchReadyReported && !inputBlocked) {
    touchReadyReported = true;
    Serial.printf("PERF touch_accept_ready_after_first_render_ms=%lu gate_ms=%d\n",
                  static_cast<unsigned long>(now - firstScreenRenderedAtMs),
                  ui::spec::kTouchPostRefreshQuietMs);
  }
  // A render deferred by the post-GC16 quiet period must not depend on another
  // telemetry version change to wake it. The coordinator remains the sole owner
  // and the bounded priority latch still coalesces all pending demand.
  // Input is sampled before any deferred cosmetic GC16. Navigation generated by
  // this sample is routed first; only then may the coordinator consume a render.
  if (action.type == input::ActionType::QualificationRejected) {
    Serial.printf("TOUCH setup=REJECTED step=%u reason=%s\n",
                  static_cast<unsigned>(touchController.qualificationStep()+1),action.reason);
  } else if (action.type == input::ActionType::QualificationPassed) {
    syncUiState();
    if (touchController.mappingVerified()) {
      displayCoordinator.requestPage(ui::Page::Home, millis());
      Serial.println("TOUCH setup=COMPLETE state=TOUCH_READY navigation=UNLOCKED target=HOME");
      testDisplay();
    } else {
      Serial.printf("TOUCH setup=PROGRESS accepted=%u next=%s display_refresh=REQUIRED armed=NO\n",
                    static_cast<unsigned>(touchController.qualificationStep()), action.reason);
      // Keep the guided instruction/progress authoritative after each accepted
      // corner. GC16 remains the only mode; four setup updates are intentional.
      testDisplay();
    }
  } else if (action.type == input::ActionType::Tap) {
    displayCoordinator.noteTouchAction(action.actionReadyMs, millis());
    ui::Page destination = displayCoordinator.page();
    if (displayCoordinator.page() == ui::Page::WeatherSetup) {
      const weather::WizardSnapshot before=weatherWizard.snapshot();
      const weather::WizardResult result = weatherWizard.tap(
          {action.point.x, action.point.y, 1, 1}, gpsManager.snapshot(), weatherManager);
      const weather::WizardSnapshot after=weatherWizard.snapshot();
      if(before.step==weather::WizardStep::Choice&&after.step==weather::WizardStep::Gps){
        // GPS weather temporarily requires receiver hardware, but it must not
        // rewrite the user's independent persisted GPS power preference.
        if(!sharedRailEnabled)setSharedRail(true,false);else gpsManager.setHardwareEnabled(true,false);
      }
      if (result == weather::WizardResult::Saved || result == weather::WizardResult::Cancelled) {
        destination = weatherWizardReturnPage;
      }
      syncUiState();
      displayCoordinator.forceDirty();
      if (destination != displayCoordinator.page()) displayCoordinator.requestPage(destination, millis());
      testDisplay();
      return;
    } else if (displayCoordinator.page() == ui::Page::TouchRecalibrateConfirm) {
      if (ui::kTouchRecalibrateCancelAction.contains(action.point.x, action.point.y)) {
        destination = ui::Page::Settings;
      } else if (ui::kTouchRecalibrateConfirmAction.contains(action.point.x, action.point.y)) {
        beginConfirmedTouchRecalibration();
        destination = ui::Page::TouchSetup;
      }
    } else if (ui::kDetailBackAction.contains(action.point.x, action.point.y) &&
               (displayCoordinator.page()==ui::Page::SystemHealth ||
                displayCoordinator.page()==ui::Page::SystemMetrics ||
                displayCoordinator.page()==ui::Page::Storage ||
                displayCoordinator.page()==ui::Page::Network ||
                displayCoordinator.page()==ui::Page::WeatherDetail ||
                displayCoordinator.page()==ui::Page::Battery ||
                displayCoordinator.page()==ui::Page::VehicleMotion ||
                 displayCoordinator.page()==ui::Page::Altimeter ||
                 displayCoordinator.page()==ui::Page::Settings ||
                 displayCoordinator.page()==ui::Page::DateTimeSettings ||
                 displayCoordinator.page()==ui::Page::UnitsSettings ||
                 displayCoordinator.page()==ui::Page::LocationPrivacySettings ||
                 displayCoordinator.page()==ui::Page::Diagnostics)) {
      destination=detailReturnPage;
    } else if (ui::kHeaderBatteryAction.contains(action.point.x, action.point.y)) {
      detailReturnPage=displayCoordinator.page();destination=ui::Page::Battery;
    } else if (ui::kHeaderWifiAction.contains(action.point.x, action.point.y)) {
      wifiReturnPage=displayCoordinator.page();destination=ui::Page::WifiSettings;
    } else if (ui::kNavBounds.contains(action.point.x, action.point.y)) {
      const int index = action.point.x / ui::spec::kNavItemWidth;
      const ui::Page pages[] = {ui::Page::Home, ui::Page::Systems, ui::Page::Radio,
                                ui::Page::Location, ui::Page::Device};
      destination = pages[constrain(index, 0, 4)];
    } else if (displayCoordinator.page() == ui::Page::Device &&
               ui::kDeviceBatteryAction.contains(action.point.x, action.point.y)) {
      detailReturnPage=ui::Page::Device;destination=ui::Page::Battery;
    } else if (displayCoordinator.page() == ui::Page::Device &&
               ui::kDeviceWifiAction.contains(action.point.x, action.point.y)) {
      wifiReturnPage=ui::Page::Device;destination=ui::Page::WifiSettings;
    } else if (displayCoordinator.page() == ui::Page::Device &&
               ui::kDeviceDiagnosticsAction.contains(action.point.x, action.point.y)) {
      detailReturnPage=ui::Page::Device;destination = ui::Page::Diagnostics;
    } else if (displayCoordinator.page() == ui::Page::Device &&
               ui::kDeviceDisplayRefreshAction.contains(action.point.x, action.point.y)) {
      if (displayCoordinator.manualRefreshAvailable(now)) {
        Serial.println("TOUCH action=SELECTED target=CLEAN_DISPLAY immediate=YES");
        releaseSharedSpiForDisplay();
        const bool capturingTouch=touchController.beginDisplayCapture();
        const bool refreshed=displayCoordinator.manualFullRefresh(now,ui::Page::Device);
        if(capturingTouch)touchController.endDisplayCapture();
        if(refreshed&&!capturingTouch)touchController.notifyDisplayUpdateFinished(millis());
        printResult("manual_display_cleanup",refreshed?"COMPLETE":"FAILED",
                    refreshed?"panel cleaned immediately; DEVICE state preserved":"display failure");
        return;
      }
      syncUiState();displayCoordinator.forceDirty();testDisplay();return;
    } else if (displayCoordinator.page() == ui::Page::Device &&
               ui::kDeviceLowPowerAction.contains(action.point.x, action.point.y)) {
      detailReturnPage=ui::Page::Device;destination=ui::Page::LowPowerSetup;
    } else if (displayCoordinator.page() == ui::Page::Device &&
               ui::kDeviceSettingsAction.contains(action.point.x, action.point.y)) {
      detailReturnPage=ui::Page::Device;destination=ui::Page::Settings;
    } else if (displayCoordinator.page() == ui::Page::Settings && ui::kSettingsCategoryActions[0].contains(action.point.x,action.point.y)) {wifiReturnPage=ui::Page::Settings;destination=ui::Page::WifiSettings;}
    else if (displayCoordinator.page() == ui::Page::Settings && ui::kSettingsCategoryActions[1].contains(action.point.x,action.point.y)) destination=ui::Page::DateTimeSettings;
    else if (displayCoordinator.page() == ui::Page::Settings && ui::kSettingsCategoryActions[2].contains(action.point.x,action.point.y)) destination=ui::Page::DisplayRefreshMode;
    else if (displayCoordinator.page() == ui::Page::Settings && ui::kSettingsCategoryActions[3].contains(action.point.x,action.point.y)) destination=ui::Page::UnitsSettings;
    else if (displayCoordinator.page() == ui::Page::Settings && ui::kSettingsCategoryActions[4].contains(action.point.x,action.point.y)){weatherWizardReturnPage=ui::Page::Settings;weatherWizard.openPreferences();syncUiState();destination=ui::Page::WeatherSetup;}
    else if (displayCoordinator.page() == ui::Page::Settings && ui::kSettingsCategoryActions[5].contains(action.point.x,action.point.y)) destination=ui::Page::LocationPrivacySettings;
    else if (displayCoordinator.page() == ui::Page::Settings && ui::kSettingsCategoryActions[6].contains(action.point.x,action.point.y)){detailReturnPage=ui::Page::Settings;destination=ui::Page::LowPowerSetup;}
    else if (displayCoordinator.page() == ui::Page::Settings && ui::kSettingsCategoryActions[7].contains(action.point.x,action.point.y)) destination=ui::Page::TouchRecalibrateConfirm;
    else if (displayCoordinator.page() == ui::Page::Settings && ui::kSettingsCategoryActions[8].contains(action.point.x,action.point.y)){detailReturnPage=ui::Page::Settings;destination=ui::Page::Diagnostics;}
    else if (displayCoordinator.page() == ui::Page::Settings && ui::kSettingsCategoryActions[9].contains(action.point.x,action.point.y)) destination=ui::Page::Calculator;
    else if (displayCoordinator.page() == ui::Page::Settings && ui::kSettingsCategoryActions[10].contains(action.point.x,action.point.y)) destination=detailReturnPage;
    else if (displayCoordinator.page()==ui::Page::WifiSettings){
      if(ui::kWifiScanAction.contains(action.point.x,action.point.y)){
        destination=wifiManager.startScan()?ui::Page::WifiNetworks:ui::Page::WifiSettings;
      }
      else if(ui::kWifiManualAction.contains(action.point.x,action.point.y)){wifiManager.beginManualNetwork();wifiEditingPassword=false;destination=ui::Page::WifiEntry;}
      else if(ui::kWifiDisconnectAction.contains(action.point.x,action.point.y))wifiManager.disconnect();
      else if(ui::kWifiReconnectAction.contains(action.point.x,action.point.y))wifiManager.reconnect();
      else if(ui::kWifiForgetAction.contains(action.point.x,action.point.y)&&wifiManager.snapshot().userConfigured)destination=ui::Page::WifiForgetConfirm;
      else if(ui::kWifiBackAction.contains(action.point.x,action.point.y))destination=wifiReturnPage;
    } else if(displayCoordinator.page()==ui::Page::WifiNetworks){
      if(ui::kWifiBackAction.contains(action.point.x,action.point.y))destination=ui::Page::WifiSettings;
      else for(uint8_t i=0;i<network::kMaximumScanResults;++i)if(ui::kWifiNetworkActions[i].contains(action.point.x,action.point.y)&&wifiManager.selectNetwork(i)){wifiEditingPassword=true;destination=ui::Page::WifiEntry;break;}
    } else if(displayCoordinator.page()==ui::Page::WifiForgetConfirm){
      if(ui::kWifiEntryCancelAction.contains(action.point.x,action.point.y))destination=ui::Page::WifiSettings;
      else if(ui::kWifiEntrySaveAction.contains(action.point.x,action.point.y)){wifiManager.forgetUserCredentials();destination=ui::Page::WifiSettings;}
    } else if(displayCoordinator.page()==ui::Page::WifiEntry){
      // Printable ASCII is 95 characters; the 96th grid slot deliberately
      // repeats Space so every visible key maps to a valid credential byte.
      const char* pages[]={"ABCDEFGHIJKLMNOPQRSTUVWX","YZabcdefghijklmnopqrstuv","wxyz0123456789!\"#$%&'()","*+,-./:;<=>?@[\\]^_`{|}~  "};
      bool key=false;for(uint8_t i=0;i<24;++i)if(ui::kWifiEntryKeys[i].contains(action.point.x,action.point.y)){if(wifiEditingPassword)wifiManager.appendPassword(pages[wifiKeyboardPage%4][i]);else wifiManager.appendSsid(pages[wifiKeyboardPage%4][i]);key=true;break;}
      if(!key&&ui::kWifiEntryModeAction.contains(action.point.x,action.point.y))wifiEditingPassword=!wifiEditingPassword;
      else if(!key&&ui::kWifiEntryDeleteAction.contains(action.point.x,action.point.y)){if(wifiEditingPassword)wifiManager.backspacePassword();else wifiManager.backspaceSsid();}
      else if(!key&&ui::kWifiEntryNextAction.contains(action.point.x,action.point.y))wifiKeyboardPage=(wifiKeyboardPage+1)%4;
      else if(!key&&ui::kWifiEntryCancelAction.contains(action.point.x,action.point.y)){wifiManager.clearEntry();destination=ui::Page::WifiSettings;}
      else if(!key&&ui::kWifiEntrySaveAction.contains(action.point.x,action.point.y)&&wifiManager.saveAndConnect())destination=ui::Page::WifiSettings;
      syncUiState();testDisplay();return;
    } else if(displayCoordinator.page()==ui::Page::DateTimeSettings){
      if(ui::kDateTimeTimezoneAction.contains(action.point.x,action.point.y)){timezoneReturnPage=ui::Page::DateTimeSettings;destination=ui::Page::TimezoneSetup;}
      else if(ui::kDateTimeFormatAction.contains(action.point.x,action.point.y)){timeService.toggleHourFormat();syncUiState();testDisplay();return;}
      else if(ui::kDateTimeSyncAction.contains(action.point.x,action.point.y)){timeService.requestSync();syncUiState();testDisplay();return;}
    } else if(displayCoordinator.page()==ui::Page::UnitsSettings){
      if(ui::kUnitsTemperatureAction.contains(action.point.x,action.point.y))telemetryManager.toggleTemperatureUnit();
      else if(ui::kUnitsSpeedAction.contains(action.point.x,action.point.y))gpsManager.toggleSpeedUnit();
      else if(ui::kUnitsElevationAction.contains(action.point.x,action.point.y))gpsManager.toggleElevationUnit();
      else destination=displayCoordinator.page();
      syncUiState();displayCoordinator.forceDirty();testDisplay();return;
    } else if(displayCoordinator.page()==ui::Page::LocationPrivacySettings){
      if(ui::kLocationSettingsGpsAction.contains(action.point.x,action.point.y)){
        if(gpsManager.snapshot().state!=location::GpsState::Off)gpsManager.setRailEnabled(false);
        else if(!sharedRailEnabled)setSharedRail(true);else gpsManager.setRailEnabled(true);
      }else if(ui::kLocationSettingsPrivacyAction.contains(action.point.x,action.point.y))gpsManager.toggleCoordinateVisibility();
      else if(ui::kLocationSettingsWeatherAction.contains(action.point.x,action.point.y)){weatherWizardReturnPage=ui::Page::LocationPrivacySettings;weatherWizard.openPreferences();destination=ui::Page::WeatherSetup;}
      syncUiState();displayCoordinator.forceDirty();
    } else if(displayCoordinator.page()==ui::Page::Calculator){
      if(ui::kCalculatorBackAction.contains(action.point.x,action.point.y))destination=ui::Page::Settings;
      else{const utilities::CalculatorKey keys[]={utilities::CalculatorKey::Digit7,utilities::CalculatorKey::Digit8,utilities::CalculatorKey::Digit9,utilities::CalculatorKey::Divide,utilities::CalculatorKey::Digit4,utilities::CalculatorKey::Digit5,utilities::CalculatorKey::Digit6,utilities::CalculatorKey::Multiply,utilities::CalculatorKey::Digit1,utilities::CalculatorKey::Digit2,utilities::CalculatorKey::Digit3,utilities::CalculatorKey::Subtract,utilities::CalculatorKey::Digit0,utilities::CalculatorKey::Decimal,utilities::CalculatorKey::Add,utilities::CalculatorKey::Equals,utilities::CalculatorKey::Clear,utilities::CalculatorKey::Backspace,utilities::CalculatorKey::ToggleSign};for(uint8_t i=0;i<19;++i)if(ui::kCalculatorKeys[i].contains(action.point.x,action.point.y)){calculator.press(keys[i]);syncUiState();testDisplay();return;}}
    } else if (displayCoordinator.page() == ui::Page::Settings &&
               ui::kSettingsTouchAction.contains(action.point.x, action.point.y)) {
      destination=ui::Page::TouchRecalibrateConfirm;
    } else if (displayCoordinator.page() == ui::Page::Settings &&
               ui::kSettingsRefreshModeAction.contains(action.point.x, action.point.y)) {
      destination=ui::Page::DisplayRefreshMode;
    } else if (displayCoordinator.page() == ui::Page::Settings &&
               ui::kSettingsTimezoneAction.contains(action.point.x, action.point.y)) {
      destination=ui::Page::TimezoneSetup;
    } else if (displayCoordinator.page() == ui::Page::Settings &&
               ui::kSettingsFormatAction.contains(action.point.x, action.point.y)) {
      timeService.toggleHourFormat();syncUiState();testDisplay();
    } else if (displayCoordinator.page() == ui::Page::Settings &&
               ui::kSettingsSyncActionCompact.contains(action.point.x, action.point.y)) {
      timeService.requestSync();syncUiState();testDisplay();
    } else if (displayCoordinator.page() == ui::Page::Settings &&
               ui::kSettingsTemperatureAction.contains(action.point.x, action.point.y)) {
      telemetryManager.toggleTemperatureUnit();syncUiState();testDisplay();
    } else if (displayCoordinator.page() == ui::Page::DisplayRefreshMode) {
      if (ui::kRefreshModeBackAction.contains(action.point.x,action.point.y)) destination=ui::Page::Settings;
      else for(uint8_t i=0;i<3;++i) if(ui::kRefreshModeActions[i].contains(action.point.x,action.point.y)){
        displayCoordinator.setRefreshMode(static_cast<ui::RefreshMode>(i));syncUiState();break;
      }
    } else if (displayCoordinator.page() == ui::Page::TimezoneSetup) {
      if (ui::kTimezoneBackAction.contains(action.point.x, action.point.y)) destination=timezoneReturnPage;
      else for(uint8_t i=0;i<device_time::timezoneCount();++i) if(ui::kTimezoneActions[i].contains(action.point.x,action.point.y)){timeService.setTimezone(i);syncUiState();displayCoordinator.forceDirty();break;}
    } else if (displayCoordinator.page() == ui::Page::LowPowerSetup) {
      if (ui::kLowPowerBackAction.contains(action.point.x,action.point.y)) destination=detailReturnPage;
      else {const power::Preset presets[]={power::Preset::Off,power::Preset::Min5,power::Preset::Min15,power::Preset::Min30,power::Preset::Min60};for(uint8_t i=0;i<5;++i)if(ui::kLowPowerPresetActions[i].contains(action.point.x,action.point.y)){lowPowerManager.selectPreset(presets[i],now);syncUiState();destination=presets[i]==power::Preset::Off?detailReturnPage:ui::Page::LowPowerStatus;break;}}
    } else if (displayCoordinator.page() == ui::Page::LowPowerStatus) {
      if (ui::kLowPowerExitAction.contains(action.point.x,action.point.y)){lowPowerManager.exit(now);syncUiState();destination=ui::Page::Device;}
      else if(ui::kLowPowerBackAction.contains(action.point.x,action.point.y))destination=ui::Page::Device;
    } else if (displayCoordinator.page()==ui::Page::Home && ui::kHomeHostAction.contains(action.point.x,action.point.y)) {
      detailReturnPage=ui::Page::Home;destination=ui::Page::SystemHealth;
    } else if (displayCoordinator.page()==ui::Page::Home && ui::kHomeMetricsAction.contains(action.point.x,action.point.y)) {
      detailReturnPage=ui::Page::Home;destination=ui::Page::SystemMetrics;
    } else if (displayCoordinator.page()==ui::Page::Home && ui::kHomeWeatherDetailAction.contains(action.point.x,action.point.y)) {
      if(weatherManager.snapshot().configured){detailReturnPage=ui::Page::Home;destination=ui::Page::WeatherDetail;}
      else{weatherWizardReturnPage=ui::Page::Home;weatherWizard.open();syncUiState();destination=ui::Page::WeatherSetup;}
    } else if (displayCoordinator.page()==ui::Page::Home && ui::kHomeMotionAction.contains(action.point.x,action.point.y)) {
      detailReturnPage=ui::Page::Home;destination=ui::Page::VehicleMotion;
    } else if (displayCoordinator.page()==ui::Page::Location &&
               ui::kLocationElevationAction.contains(action.point.x,action.point.y)) {
      detailReturnPage=ui::Page::Location;destination=ui::Page::Altimeter;
    } else if (displayCoordinator.page()==ui::Page::VehicleMotion &&
               ui::kVehicleLocationAction.contains(action.point.x,action.point.y)) {
      destination=ui::Page::Location;
    } else if (displayCoordinator.page()==ui::Page::VehicleMotion &&
               ui::kVehicleElevationAction.contains(action.point.x,action.point.y)) {
      detailReturnPage=ui::Page::VehicleMotion;destination=ui::Page::Altimeter;
    } else if (displayCoordinator.page()==ui::Page::Altimeter &&
               ui::kAltimeterUnitAction.contains(action.point.x,action.point.y)) {
      gpsManager.toggleElevationUnit();syncUiState();testDisplay();
    } else if (displayCoordinator.page()==ui::Page::Home && ui::kHomeBatteryAction.contains(action.point.x,action.point.y)) {
      detailReturnPage=ui::Page::Home;destination=ui::Page::Battery;
    } else if (displayCoordinator.page()==ui::Page::Home && ui::kHomeNetworkAction.contains(action.point.x,action.point.y)) {
      detailReturnPage=ui::Page::Home;destination=ui::Page::Network;
    } else if (displayCoordinator.page()==ui::Page::Home && ui::kHomeStorageAction.contains(action.point.x,action.point.y)) {
      detailReturnPage=ui::Page::Home;destination=ui::Page::Storage;
    } else if (displayCoordinator.page()==ui::Page::WeatherDetail &&
               ui::kWeatherDetailSetupAction.contains(action.point.x,action.point.y)) {
      weatherWizardReturnPage=ui::Page::WeatherDetail;weatherWizard.open();syncUiState();destination=ui::Page::WeatherSetup;
    } else if (displayCoordinator.page() == ui::Page::Location &&
               ui::kLocationGpsPowerAction.contains(action.point.x, action.point.y)) {
      if (gpsManager.snapshot().state != location::GpsState::Off) gpsManager.setRailEnabled(false);else if (!sharedRailEnabled) setSharedRail(true);else gpsManager.setRailEnabled(true);syncUiState();displayCoordinator.forceDirty();testDisplay();
    } else if (displayCoordinator.page() == ui::Page::Location &&
               ui::kLocationSpeedUnitAction.contains(action.point.x, action.point.y)) {
      gpsManager.toggleSpeedUnit();syncUiState();testDisplay();
    } else if (displayCoordinator.page() == ui::Page::Location &&
               ui::kLocationPrivacyAction.contains(action.point.x, action.point.y)) {
      gpsManager.toggleCoordinateVisibility();syncUiState();testDisplay();
    } else if (displayCoordinator.page() == ui::Page::Location &&
               ui::kLocationWeatherSetupAction.contains(action.point.x, action.point.y)) {
      weatherWizardReturnPage=ui::Page::Location;weatherWizard.open();syncUiState();destination=ui::Page::WeatherSetup;
    } else if (displayCoordinator.page() == ui::Page::Location &&
               ui::kLocationWeatherRefreshAction.contains(action.point.x, action.point.y)) {
      weatherManager.requestRefresh(now);syncUiState();displayCoordinator.forceDirty();testDisplay();
    } else if (displayCoordinator.page() == ui::Page::Systems &&
               ui::kSystemsSectionAction.contains(action.point.x, action.point.y)) {
      systemsSection = (systemsSection + 1) % 4;
      syncUiState();
      testDisplay();
    } else if (displayCoordinator.page() == ui::Page::Diagnostics &&
               ui::kDiagnosticsTextQualificationAction.contains(action.point.x, action.point.y)) {
      destination = ui::Page::TextQualification;
    }
    if (displayCoordinator.requestPage(destination, millis())) {
      if (destination == ui::Page::TouchSetup && !touchController.qualifying()) {
        touchController.startQualification();
        syncUiState();
      }
      Serial.printf("TOUCH navigation=SELECTED target=%u gesture=TAP\n",static_cast<unsigned>(destination));
      testDisplay();
    }
  } else if (action.type == input::ActionType::SwipeLeft || action.type == input::ActionType::SwipeRight) {
    const ui::Page pages[] = {ui::Page::Home,ui::Page::Systems,ui::Page::Radio,ui::Page::Location,ui::Page::Device};
    int index=-1; for(int i=0;i<5;++i) if(displayCoordinator.page()==pages[i]) index=i;
    // Forms, dialogs, calculators, scans, setup, and calibration are all
    // non-primary pages, so recognized movement is deliberately ignored there.
    if(index>=0){
      index=constrain(index+(action.type==input::ActionType::SwipeLeft?1:-1),0,4);
      if(displayCoordinator.requestPage(pages[index],millis())){
        Serial.printf("TOUCH navigation=SELECTED target=%u gesture=%s\n",static_cast<unsigned>(pages[index]),action.type==input::ActionType::SwipeLeft?"SWIPE_LEFT":"SWIPE_RIGHT");
        testDisplay();
      }
    }
  }
  // Touch routing always gets the first opportunity after the safe post-GC16
  // gate. Service initialization starts only after that first unblocked sample,
  // so neither NVS/I2C setup nor Wi-Fi task creation extends the touch gate.
  if (touchReadyReported) {
    initializeLocalServices();
    startBackgroundWorkers();
  }
  gpsManager.poll(now);
  if(localServicesInitialized)wifiManager.poll(now,!lowPowerManager.snapshot().active||lowPowerManager.snapshot().awakeWindow||lowPowerManager.snapshot().criticalHold);
  const location::Snapshot gpsState = gpsManager.snapshot();
  weatherManager.setGpsPosition(location::currentFixUsable(gpsState), gpsState.latitude, gpsState.longitude);
  const uint32_t latestTelemetryVersion = telemetryManager.version();
  const telemetry::Snapshot telemetryState = telemetryManager.snapshot();
  applyLowPowerPolicy(now, telemetryState);
  const power::Snapshot lowPowerState = lowPowerManager.snapshot();
  timeService.poll(now, telemetryState.wifiState == telemetry::WifiState::Connected ||
                        WiFi.status() == WL_CONNECTED);
  batteryManager.poll(now, gaugeObserved, chargerObserved,
                      !lowPowerState.active || lowPowerState.awakeWindow || lowPowerState.criticalHold);
  const uint32_t latestTimeVersion = timeService.version();
  const uint32_t latestWeatherVersion = weatherManager.version();
  const uint32_t latestBatteryVersion = batteryManager.version();
  const uint32_t latestGpsVersion = gpsManager.version();
  const uint32_t latestLowPowerVersion = lowPowerManager.version();
  const uint32_t latestWifiVersion = wifiManager.version();
  if (latestTelemetryVersion != observedTelemetryVersion ||
      latestTimeVersion != observedTimeVersion ||
      latestWeatherVersion != observedWeatherVersion ||
      latestBatteryVersion != observedBatteryVersion ||
      latestGpsVersion != observedGpsVersion ||
      latestLowPowerVersion != observedLowPowerVersion ||
      latestWifiVersion != observedWifiVersion) {
    observedTelemetryVersion = latestTelemetryVersion;
    observedTimeVersion = latestTimeVersion;
    observedWeatherVersion = latestWeatherVersion;
    observedBatteryVersion = latestBatteryVersion;
    observedGpsVersion = latestGpsVersion;
    observedLowPowerVersion = latestLowPowerVersion;
    observedWifiVersion = latestWifiVersion;
    syncUiState();
  }
  if (displayCoordinator.dirty() && touchController.displayCaptureSettled() &&
      !displayCoordinator.inputBlocked(millis())) testDisplay();
  if (radioListening && digitalRead(hq::kBoard.loraIrq)) {
    String packet;
    const int state = radio.readData(packet);
    if (state == RADIOLIB_ERR_NONE) {
      Serial.printf("RX packet=\"%s\" rssi=%.1f snr=%.1f\n", packet.c_str(),
                    radio.getRSSI(), radio.getSNR());
    } else {
      Serial.printf("RX read code=%d\n", state);
    }
    radio.startReceive();
  }
#if SUPPORTFORGE_PERF_DIAGNOSTICS
  if(static_cast<int32_t>(now-nextPerformanceReportMs)>=0){nextPerformanceReportMs=now+60000;touchController.printPerformance();displayCoordinator.printPerformance();Serial.printf("PERF guardian_heartbeat=%lu weather_heartbeat=%lu reset_classification=%s\n",static_cast<unsigned long>(telemetryManager.heartbeat()),static_cast<unsigned long>(weatherManager.heartbeat()),resetReasonName(esp_reset_reason()));}
#endif
  delay(2);
}
