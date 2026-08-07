#include <Arduino.h>
#include <RadioLib.h>
#include <SD.h>
#include <SPI.h>
#include <Wire.h>
#include <epdiy.h>
#include <esp_system.h>

#include "board_profile.h"
#include "input/touch_controller.h"
#include "ui/ui_components.h"
#include "ui/ui_controller.h"

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

constexpr const char *kFirmwareId = "h752_02_candidate qualification-v2";
ui::UiController uiController;
input::TouchController touchController;

ui::Presence presence(bool observed) {
  return observed ? ui::Presence::Observed : ui::Presence::Unknown;
}

uint8_t fromBcd(uint8_t value) { return (value >> 4) * 10 + (value & 0x0F); }

void readRtc(ui::UiSnapshot& state) {
  if (!rtcObserved) return;
  Wire.beginTransmission(0x51);
  Wire.write(0x02);
  if (Wire.endTransmission(false) != 0 || Wire.requestFrom(0x51, 7) != 7) return;
  const uint8_t second = Wire.read();
  const uint8_t minute = Wire.read();
  const uint8_t hour = Wire.read();
  const uint8_t day = Wire.read();
  Wire.read();  // weekday
  const uint8_t month = Wire.read();
  const uint8_t year = Wire.read();
  if (second & 0x80) return;  // PCF8563 voltage-low flag: time is not trustworthy.
  state.minute = fromBcd(minute & 0x7F);
  state.hour = fromBcd(hour & 0x3F);
  state.day = fromBcd(day & 0x3F);
  state.month = fromBcd(month & 0x1F);
  state.year = 2000 + fromBcd(year);
  state.rtcValid = state.hour < 24 && state.minute < 60 && state.day > 0 &&
                   state.day <= 31 && state.month > 0 && state.month <= 12;
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
  state.psramAvailable = ESP.getPsramSize() > 0;
  state.firmwareId = "FIELD UI 1";
  state.buildDate = __DATE__;
  state.buildTime = __TIME__;
  readRtc(state);
  return state;
}

void syncUiState() { uiController.setSnapshot(makeUiSnapshot()); }

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
  Serial.println("Boot policy: no rail enable, RF transmit, or GPS TX; one GC16 HOME render.");
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
  Serial.println("  display clear - guarded boot-recovery full-clear plus HOME render");
  Serial.println("  page <name>   - HOME/SYSTEMS/RADIO/LOCATION/DEVICE/DIAGNOSTICS");
  Serial.println("  touch corners - non-destructive four-corner mapping qualification");
  Serial.println("  touch test    - one redacted coordinate observation");
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
  chargerObserved = i2cPresent(0x6A);
  gaugeObserved = i2cPresent(0x55);
  const bool touch14 = i2cPresent(0x14);
  const bool touch5d = i2cPresent(0x5D);
  touchObserved = touch14 || touch5d;
  printResult("pca9535", pcaObserved ? "OBSERVED" : "NOT_PRESENT", "address 0x20");
  printResult("rtc", rtcObserved ? "OBSERVED" : "NOT_PRESENT",
              rtcObserved ? "PCF8563-compatible device at 0x51" : "no response at 0x51");
  printResult("bq25896", chargerObserved ? "OBSERVED" : "NOT_PRESENT", "address 0x6A");
  printResult("bq27220", gaugeObserved ? "OBSERVED" : "NOT_PRESENT", "address 0x55");
  printResult("gt911", touchObserved ? "OBSERVED" : "NOT_PRESENT",
              touch14 ? "address 0x14" : (touch5d ? "address 0x5D" : "no response at 0x14/0x5D"));
}

void testSd() {
  pinMode(hq::kBoard.loraCs, OUTPUT);
  digitalWrite(hq::kBoard.loraCs, HIGH);
  SPI.begin(hq::kBoard.spiSclk, hq::kBoard.spiMiso, hq::kBoard.spiMosi);
  if (!SD.begin(hq::kBoard.sdCs, SPI, 1000000)) {
    printResult("microsd", "NOT_PRESENT", "read-only qualification mount failed");
    return;
  }
  sdObserved = SD.cardType() != CARD_NONE;
  printResult("microsd", sdObserved ? "OBSERVED" : "NOT_PRESENT",
              sdObserved ? "card initialized; no file created" : "interface initialized; no card");
  SD.end();
}

bool setSharedRail(bool enabled) {
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
void testDisplay(bool bootRecovery = false) {
#if defined(HQ_PROFILE_LEGACY_H752)
  printResult("display", "BLOCKED",
              "epd_board_v7 belongs to current Pro baseline, not legacy H752");
#else
  const bool rendered = uiController.renderIfDirty(millis(), bootRecovery);
  printResult("display", rendered ? "COMMAND_ACCEPTED" : "NO_CHANGE",
              "portrait GC16 retained page; high voltage off after update");
#endif
}

void testTouch() {
  const uint8_t address = i2cPresent(0x14) ? 0x14 : (i2cPresent(0x5D) ? 0x5D : 0);
  if (!address) { printResult("touch", "NOT_PRESENT", "GT911 address absent"); return; }
  Serial.println("Touch observation: touch panel once within 15 seconds.");
  const uint32_t deadline = millis() + 15000;
  while (static_cast<int32_t>(deadline - millis()) > 0) {
    Wire.beginTransmission(address); Wire.write(0x81); Wire.write(0x4E);
    if (Wire.endTransmission(false) == 0 && Wire.requestFrom(address, (uint8_t)5) == 5) {
      uint8_t status = Wire.read(); uint8_t xl = Wire.read(); uint8_t xh = Wire.read(); uint8_t yl = Wire.read(); uint8_t yh = Wire.read();
      if ((status & 0x80) && (status & 0x0F)) {
        const int rawX = xl | (xh << 8), rawY = yl | (yh << 8);
        input::transformInvertedPortrait(rawX, rawY);
        printResult("touch", "OBSERVED",
                    "coordinate captured and clamped; values redacted; run 'touch corners' to qualify mapping");
        Wire.beginTransmission(address); Wire.write(0x81); Wire.write(0x4E); Wire.write(0); Wire.endTransmission();
        return;
      }
    }
    delay(20);
  }
  printResult("touch", "UNVERIFIED", "controller present; no coordinate observed");
}

bool requestNamedPage(const String& name) {
  ui::Page page;
  if (name == "home") page = ui::Page::Home;
  else if (name == "systems") page = ui::Page::Systems;
  else if (name == "radio") page = ui::Page::Radio;
  else if (name == "location") page = ui::Page::Location;
  else if (name == "device") page = ui::Page::Device;
  else if (name == "diagnostics") page = ui::Page::Diagnostics;
  else return false;
  if (!uiController.requestPage(page, millis())) {
    printResult("ui", "RATE_LIMITED", "page unchanged or refresh cooldown active");
    return true;
  }
  testDisplay();
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
  else if (command == "display clear") { syncUiState(); testDisplay(true); }
  else if (command == "touch test") testTouch();
  else if (command == "touch corners") { touchController.runFourCornerTest(); syncUiState(); }
  else if (command.startsWith("page ") && requestNamedPage(command.substring(5))) {}
  else if (command == "help" || command.isEmpty()) printHelp();
  else Serial.println("Unknown command. Type 'help'.");
}

}  // namespace

void setup() {
  Serial.begin(115200);
  const uint32_t waitUntil = millis() + 3000;
  while (!Serial && static_cast<int32_t>(waitUntil - millis()) > 0) delay(10);

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
  Serial.printf("RESET reason=%s code=%d\n", resetReasonName(esp_reset_reason()), esp_reset_reason());
  Serial.printf("MCU model=%s revision=%d cores=%d cpu=%dMHz flash=%u bytes\n",
                ESP.getChipModel(), ESP.getChipRevision(), ESP.getChipCores(), ESP.getCpuFreqMHz(), ESP.getFlashChipSize());
  Serial.printf("MEM heap_free=%u heap_min=%u heap_largest=%u psram=%u psram_free=%u\n",
                ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap(), ESP.getPsramSize(), ESP.getFreePsram());
  printResult("boot", "OBSERVED", "firmware reached setup; peripherals unconfirmed");
  const uint32_t psramBytes = ESP.getPsramSize();
  printResult("psram", psramBytes ? "OBSERVED" : "NOT_OBSERVED",
              String(psramBytes) + " bytes reported by runtime");
  identifyI2c();
  touchController.begin();
  syncUiState();
  if (uiController.begin()) testDisplay(false);
  else printResult("display", "FAILED", "UI framebuffer initialization failed");
  printHelp();
}

void loop() {
  if (Serial.available()) processCommand(Serial.readStringUntil('\n'));
  const input::Tap tap = touchController.poll(millis());
  if (tap.accepted) {
    ui::Page destination = uiController.page();
    if (ui::kNavBounds.contains(tap.point.x, tap.point.y)) {
      const int index = tap.point.x / 108;
      const ui::Page pages[] = {ui::Page::Home, ui::Page::Systems, ui::Page::Radio,
                                ui::Page::Location, ui::Page::Device};
      destination = pages[constrain(index, 0, 4)];
    } else if (uiController.page() == ui::Page::Device &&
               ui::Rect{24, 550, 492, 88}.contains(tap.point.x, tap.point.y)) {
      destination = ui::Page::Diagnostics;
    }
    if (uiController.requestPage(destination, millis())) testDisplay();
  }
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
  delay(2);
}
