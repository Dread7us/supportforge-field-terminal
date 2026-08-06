#include <Arduino.h>
#include <RadioLib.h>
#include <SPI.h>
#include <Wire.h>
#include <epdiy.h>

#include "board_profile.h"

namespace {

SX1262 radio = new Module(hq::kBoard.loraCs, hq::kBoard.loraIrq,
                          hq::kBoard.loraReset, hq::kBoard.loraBusy);
HardwareSerial gpsSerial(1);

bool sharedRailEnabled = false;
bool radioListening = false;
bool displayInitialized = false;
EpdiyHighlevelState displayState;

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
  Serial.println("Boot policy: no rail enable, RF transmit, GPS TX, or EPD power-on.");
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
  Serial.println("  display test  - one conservative black/white pattern, then power off");
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
  String sample;
  while (static_cast<int32_t>(deadline - millis()) > 0) {
    while (gpsSerial.available()) {
      const char c = static_cast<char>(gpsSerial.read());
      ++bytes;
      if (c == '\n') ++lines;
      if (sample.length() < 160 && c >= 0x20 && c <= 0x7E) sample += c;
    }
    delay(2);
  }
  gpsSerial.end();
  printResult("l76k", bytes ? "SERIAL_OBSERVED" : "NOT_OBSERVED",
              String(bytes) + " bytes, " + String(lines) + " lines, sample=" + sample);
}

void testDisplay() {
#if defined(HQ_PROFILE_LEGACY_H752)
  printResult("display", "BLOCKED",
              "epd_board_v7 belongs to current Pro baseline, not legacy H752");
#else
  Serial.println("Display test: initializing official current-Pro epd_board_v7 candidate.");
  if (!displayInitialized) {
    epd_init(&epd_board_v7, &ED047TC1, EPD_LUT_64K);
    epd_set_vcom(1560);  // Official display_test baseline; do not tune blindly.
    displayState = epd_hl_init(EPD_BUILTIN_WAVEFORM);
    displayInitialized = true;
  }
  uint8_t *framebuffer = epd_hl_get_framebuffer(&displayState);
  if (!framebuffer) {
    printResult("display", "FAILED", "framebuffer allocation failed");
    return;
  }
  epd_hl_set_all_white(&displayState);
  const EpdRect outer{20, 20, static_cast<int>(epd_width() - 40),
                      static_cast<int>(epd_height() - 40)};
  const EpdRect center{static_cast<int>(epd_width() / 4),
                       static_cast<int>(epd_height() / 4),
                       static_cast<int>(epd_width() / 2),
                       static_cast<int>(epd_height() / 2)};
  epd_draw_rect(outer, 0x00, framebuffer);
  epd_fill_rect(center, 0x00, framebuffer);
  epd_poweron();
  const EpdDrawError result =
      epd_hl_update_screen(&displayState, MODE_DU, epd_ambient_temperature());
  epd_poweroff();
  printResult("display", result == EPD_DRAW_SUCCESS ? "COMMAND_ACCEPTED" : "FAILED",
              "physical pattern must be visually reported; draw code " + String(result));
#endif
}

void processCommand(String command) {
  command.trim();
  command.toLowerCase();
  if (command == "profile") printProfile();
  else if (command == "i2c") scanI2c();
  else if (command == "rail on") setSharedRail(true);
  else if (command == "rail off") setSharedRail(false);
  else if (command == "lora probe") probeRadio(false);
  else if (command == "lora rx") probeRadio(true);
  else if (command == "lora stop") {
    radio.sleep();
    radioListening = false;
    printResult("sx1262", "STOPPED", "radio sleep requested");
  } else if (command == "gps listen") startGpsObservation();
  else if (command == "display test") testDisplay();
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
  printResult("boot", "OBSERVED", "firmware reached setup; peripherals unconfirmed");
  const uint32_t psramBytes = ESP.getPsramSize();
  printResult("psram", psramBytes ? "OBSERVED" : "NOT_OBSERVED",
              String(psramBytes) + " bytes reported by runtime");
  printHelp();
}

void loop() {
  if (Serial.available()) processCommand(Serial.readStringUntil('\n'));
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
