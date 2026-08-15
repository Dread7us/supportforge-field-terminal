#include "battery_manager.h"

#include <Arduino.h>
#include <Wire.h>

namespace battery {
namespace {
constexpr uint8_t kGaugeAddress = 0x55;
constexpr uint8_t kChargerAddress = 0x6B;
constexpr uint8_t kStateOfChargeRegister = 0x2C;
constexpr uint8_t kChargerStatusRegister = 0x0B;
constexpr uint8_t kChargeStatusMask = 0x18;
constexpr uint32_t kNormalSampleIntervalMs = 90UL * 1000UL;
constexpr uint32_t kChargingSampleIntervalMs = 45UL * 1000UL;
constexpr uint32_t kMaximumFreshAgeMs = 3UL * kNormalSampleIntervalMs;

bool readRegister(uint8_t address, uint8_t reg, uint8_t* data, size_t length) {
  Wire.beginTransmission(address);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(address, static_cast<uint8_t>(length)) != length) return false;
  for (size_t index = 0; index < length; ++index) {
    if (!Wire.available()) return false;
    data[index] = static_cast<uint8_t>(Wire.read());
  }
  return true;
}

bool readSoc(uint16_t& value) {
  uint8_t first[2]{}, second[2]{};
  if (!readRegister(kGaugeAddress, kStateOfChargeRegister, first, sizeof(first)) ||
      !readRegister(kGaugeAddress, kStateOfChargeRegister, second, sizeof(second))) return false;
  const uint16_t firstValue = decodeLittleEndianWord(first[0], first[1]);
  const uint16_t secondValue = decodeLittleEndianWord(second[0], second[1]);
  if (firstValue != secondValue || !validPercent(firstValue)) return false;
  value = firstValue;
  return true;
}
}

bool validPercent(int value) { return value >= 0 && value <= 100; }

uint16_t decodeLittleEndianWord(uint8_t low, uint8_t high) {
  return static_cast<uint16_t>(low) | (static_cast<uint16_t>(high) << 8);
}

State classifyChargeStatus(uint8_t register0b) {
  switch ((register0b & kChargeStatusMask) >> 3) {
    case 0: return State::Available;
    case 1:
    case 2: return State::Charging;
    case 3: return State::Full;
  }
  return State::Error;
}

const char* stateName(State value) {
  switch (value) {
    case State::Available: return "AVAILABLE";
    case State::Charging: return "CHARGING";
    case State::Full: return "FULL";
    case State::Stale: return "STALE";
    case State::Unknown: return "BAT UNKNOWN";
    case State::NotPresent: return "NOT PRESENT";
    case State::Error: return "BATTERY ERROR";
  }
  return "BAT UNKNOWN";
}

void BatteryManager::begin(bool gaugeObserved, bool chargerObserved) {
  sample(millis(), gaugeObserved, chargerObserved);
}

void BatteryManager::poll(uint32_t nowMs, bool gaugeObserved, bool chargerObserved,
                          bool scheduledAwake) {
  if (snapshot_.percentAvailable && snapshot_.lastSampleMs &&
      nowMs - snapshot_.lastSampleMs > kMaximumFreshAgeMs) {
    snapshot_.percentAvailable = false;
    snapshot_.sampleValid = false;
    snapshot_.state = State::Stale;
    ++snapshot_.version;
  }
  if (!scheduledAwake) return;
  if (static_cast<int32_t>(nowMs - nextSampleMs_) < 0) return;
  sample(nowMs, gaugeObserved, chargerObserved);
}

void BatteryManager::sample(uint32_t nowMs, bool gaugeObserved, bool chargerObserved) {
  const Snapshot before = snapshot_;
  const bool retainedPercentFresh = before.percentAvailable && before.lastSampleMs &&
      nowMs - before.lastSampleMs <= kMaximumFreshAgeMs;
  snapshot_.sampleAttempted = true;
  snapshot_.lastAttemptMs = nowMs;
  uint16_t soc = 0;
  uint8_t chargerStatus = 0;
  const bool socValid = gaugeObserved && readSoc(soc);
  const bool chargerValid = chargerObserved &&
      readRegister(kChargerAddress, kChargerStatusRegister, &chargerStatus, 1);
  // SOC and charger status are independent read-only evidence. A failed charger
  // status read must never hide a valid gauge percentage or imply charging.
  snapshot_.sampleValid = socValid;
  snapshot_.chargeStatusVerified = chargerValid;
  if (socValid) {
    snapshot_.percentAvailable = true;
    snapshot_.percent = static_cast<uint8_t>(soc);
    snapshot_.lastSampleMs = nowMs;
  }
  if (!gaugeObserved) {
    snapshot_.state = State::NotPresent;
    snapshot_.percentAvailable = false;
  } else if (!socValid) {
    // A malformed or failed transaction is never accepted as a new sample.
    // Keep a previously validated SOC visible only while that older sample is
    // still inside the freshness bound; sampleValid remains false so the detail
    // page and qualification output truthfully identify the failed attempt.
    const bool validatedPercentFresh = socValid || retainedPercentFresh;
    snapshot_.percentAvailable = validatedPercentFresh;
    snapshot_.state = validatedPercentFresh ? State::Error :
        (before.state == State::Stale ? State::Stale : State::Error);
  } else {
    snapshot_.state = chargerValid ? classifyChargeStatus(chargerStatus) : State::Available;
  }
  nextSampleMs_ = nowMs + (snapshot_.state == State::Charging ?
      kChargingSampleIntervalMs : kNormalSampleIntervalMs);
  if (before.state != snapshot_.state ||
      before.percentAvailable != snapshot_.percentAvailable ||
      (snapshot_.percentAvailable && before.percent != snapshot_.percent) ||
      before.sampleValid != snapshot_.sampleValid ||
      before.chargeStatusVerified != snapshot_.chargeStatusVerified) ++snapshot_.version;
  const String percentText = snapshot_.percentAvailable ? String(snapshot_.percent) : String("--");
  const String freshnessText = snapshot_.percentAvailable ?
      String(nowMs - snapshot_.lastSampleMs) : String("--");
  Serial.printf("BATTERY read=%s soc_valid=%s percent=%s charge_status_verified=%s state=%s freshness_ms=%s\n",
                snapshot_.sampleValid ? "SUCCESS" : "FAILURE",
                socValid ? "YES" : "NO", percentText.c_str(),
                chargerValid ? "YES" : "NO", stateName(snapshot_.state),
                freshnessText.c_str());
}

}  // namespace battery