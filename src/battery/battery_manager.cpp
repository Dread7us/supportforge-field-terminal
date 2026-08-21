#include "battery_manager.h"

#include <Arduino.h>
#include <Wire.h>

namespace battery {
namespace {
constexpr uint8_t kGaugeAddress = 0x55;
constexpr uint8_t kChargerAddress = 0x6B;
constexpr uint8_t kStateOfChargeRegister = 0x2C;
// BQ27220 standard commands used by LILYGO's read-only gauge path and TI's
// public command table. These are observations only, never control subcommands.
constexpr uint8_t kVoltageRegister = 0x08;
constexpr uint8_t kRemainingCapacityRegister = 0x0C;
constexpr uint8_t kFullChargeCapacityRegister = 0x12;
constexpr uint8_t kAverageCurrentRegister = 0x14;
constexpr uint8_t kChargerStatusRegister = 0x0B;
constexpr uint8_t kChargeStatusMask = 0x18;
constexpr uint8_t kVbusStatusMask = 0xE0;
constexpr uint32_t kNormalSampleIntervalMs = 90UL * 1000UL;
constexpr uint32_t kChargingSampleIntervalMs = 45UL * 1000UL;
constexpr uint32_t kRecoverySampleIntervalMs = 5UL * 1000UL;
constexpr uint8_t kFailuresBeforeError = 3;
constexpr uint32_t kMaximumFreshAgeMs = maximumFreshAgeMs();

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
  uint8_t first[2]{}, second[2]{}, third[2]{};
  if (!readRegister(kGaugeAddress, kStateOfChargeRegister, first, sizeof(first)) ||
      !readRegister(kGaugeAddress, kStateOfChargeRegister, second, sizeof(second))) return false;
  const uint16_t firstValue = decodeLittleEndianWord(first[0], first[1]);
  const uint16_t secondValue = decodeLittleEndianWord(second[0], second[1]);
  if (!validPercent(firstValue) || !validPercent(secondValue)) return false;
  if (firstValue == secondValue) { value = secondValue; return true; }
  // One bounded retry handles a legitimate one-percent transition or one noisy
  // transaction without accepting an unvalidated word.
  if (!readRegister(kGaugeAddress, kStateOfChargeRegister, third, sizeof(third))) return false;
  const uint16_t thirdValue = decodeLittleEndianWord(third[0], third[1]);
  if (!validPercent(thirdValue)) return false;
  if (thirdValue == secondValue || thirdValue == firstValue) { value = thirdValue; return true; }
  if (abs(static_cast<int>(thirdValue) - static_cast<int>(secondValue)) <= 1 &&
      abs(static_cast<int>(secondValue) - static_cast<int>(firstValue)) <= 1) {
    value = thirdValue;
    return true;
  }
  return false;
}

bool readGaugeWord(uint8_t command, uint16_t& value) {
  uint8_t data[2]{};
  if (!readRegister(kGaugeAddress, command, data, sizeof(data))) return false;
  value = decodeLittleEndianWord(data[0], data[1]);
  return true;
}

bool readChargerStatus(uint8_t& value) {
  return readRegister(kChargerAddress, kChargerStatusRegister, &value, 1) ||
         readRegister(kChargerAddress, kChargerStatusRegister, &value, 1);
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

ChargerConnection classifyChargerConnection(uint8_t register0b) {
  // BQ25896 REG0B VBUS_STAT[7:5]: 000 is no input, 001..110 are
  // detected input sources, and 111 is OTG output rather than a charger input.
  const uint8_t vbusStatus = (register0b & kVbusStatusMask) >> 5;
  return vbusStatus >= 1 && vbusStatus <= 6
      ? ChargerConnection::Connected : ChargerConnection::NotConnected;
}

ChargePhase classifyChargePhase(uint8_t register0b) {
  switch ((register0b & kChargeStatusMask) >> 3) {
    case 0: return ChargePhase::NotCharging;
    case 1: return ChargePhase::Precharge;
    case 2: return ChargePhase::FastCharge;
    case 3: return ChargePhase::Complete;
  }
  return ChargePhase::Unknown;
}

const char* chargerConnectionName(ChargerConnection value) {
  switch (value) {
    case ChargerConnection::Connected: return "CONNECTED";
    case ChargerConnection::NotConnected: return "NOT CONNECTED";
    case ChargerConnection::Unknown: return "VERIFICATION NEEDED";
  }
  return "VERIFICATION NEEDED";
}

const char* chargePhaseName(ChargePhase value) {
  switch (value) {
    case ChargePhase::NotCharging: return "NOT CHARGING";
    case ChargePhase::Precharge: return "PRE-CHARGE";
    case ChargePhase::FastCharge: return "FAST CHARGE";
    case ChargePhase::Complete: return "COMPLETE";
    case ChargePhase::Unknown: return "UNVERIFIED";
  }
  return "UNVERIFIED";
}

const char* diagnosisName(Diagnosis value) {
  switch (value) {
    case Diagnosis::BatteryNearReportedSoc: return "GAUGE REPORTS BATTERY NEAR SHOWN SOC";
    case Diagnosis::ChargerNotContinuing: return "INPUT PRESENT; CHARGER NOT CONTINUING";
    case Diagnosis::GaugeStale: return "GAUGE SOC IS STALE OR INVALID";
    case Diagnosis::ChargeCompleteBelowThreshold: return "CHARGE COMPLETE BELOW 95%; VERIFY GAUGE MODEL";
    case Diagnosis::GaugeModelNeedsVerification: return "GAUGE MODEL / BATTERY NEEDS VERIFICATION";
    case Diagnosis::None: return "NO CONTRADICTION OBSERVED";
  }
  return "NO CONTRADICTION OBSERVED";
}

State reconcileStateOfCharge(State chargerState, bool socFresh, uint8_t percent) {
  if (chargerState != State::Full) return chargerState;
  // Charge termination is necessary but not sufficient for FULL. Only a fresh,
  // independently validated gauge SOC at the near-full threshold can confirm it.
  return socFresh && validPercent(percent) && percent >= nearFullThresholdPercent()
             ? State::Full
             : State::Verifying;
}

const char* stateName(State value) {
  switch (value) {
    case State::Available: return "AVAILABLE";
    case State::Charging: return "CHARGING";
    case State::Full: return "FULL";
    case State::Verifying: return "VERIFYING";
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
  if (snapshot_.percentAvailable && snapshot_.hasValidSample &&
      nowMs - snapshot_.lastSampleMs > kMaximumFreshAgeMs) {
    snapshot_.percentAvailable = false;
    snapshot_.sampleValid = false;
    snapshot_.state = State::Stale;
    snapshot_.diagnosis = Diagnosis::GaugeStale;
    ++snapshot_.version;
  }
  if (!scheduledAwake) return;
  if (static_cast<int32_t>(nowMs - nextSampleMs_) < 0) return;
  sample(nowMs, gaugeObserved, chargerObserved);
}

void BatteryManager::sample(uint32_t nowMs, bool gaugeObserved, bool chargerObserved) {
  const Snapshot before = snapshot_;
  const bool retainedPercentFresh = before.percentAvailable && before.hasValidSample &&
      nowMs - before.lastSampleMs <= kMaximumFreshAgeMs;
  snapshot_.sampleAttempted = true;
  snapshot_.lastAttemptMs = nowMs;
  uint16_t soc = 0;
  uint16_t voltage = 0, averageCurrent = 0, remainingCapacity = 0, fullChargeCapacity = 0;
  uint8_t chargerStatus = 0;
  const bool socValid = gaugeObserved && readSoc(soc);
  const bool voltageValid = gaugeObserved && readGaugeWord(kVoltageRegister, voltage);
  const bool currentValid = gaugeObserved && readGaugeWord(kAverageCurrentRegister, averageCurrent);
  const bool remainingValid = gaugeObserved && readGaugeWord(kRemainingCapacityRegister, remainingCapacity);
  const bool fullCapacityValid = gaugeObserved && readGaugeWord(kFullChargeCapacityRegister, fullChargeCapacity) &&
      fullChargeCapacity > 0;
  const bool chargerValid = chargerObserved && readChargerStatus(chargerStatus);
  // SOC and charger status are independent read-only evidence. A failed charger
  // status read must never hide a valid gauge percentage or imply charging.
  snapshot_.sampleValid = socValid;
  snapshot_.chargeStatusVerified = chargerValid;
  snapshot_.chargerConnection = chargerValid
      ? classifyChargerConnection(chargerStatus) : ChargerConnection::Unknown;
  snapshot_.chargePhase = chargerValid ? classifyChargePhase(chargerStatus) : ChargePhase::Unknown;
  snapshot_.voltageAvailable = voltageValid;
  snapshot_.voltageMillivolts = voltageValid ? voltage : 0;
  snapshot_.currentAvailable = currentValid;
  snapshot_.averageCurrentMilliamps = currentValid ? static_cast<int16_t>(averageCurrent) : 0;
  snapshot_.capacityAvailable = remainingValid && fullCapacityValid;
  snapshot_.remainingCapacityMah = snapshot_.capacityAvailable ? remainingCapacity : 0;
  snapshot_.fullChargeCapacityMah = snapshot_.capacityAvailable ? fullChargeCapacity : 0;
  if (socValid) {
    consecutiveSocFailures_ = 0;
    snapshot_.percentAvailable = true;
    snapshot_.hasValidSample = true;
    snapshot_.percent = static_cast<uint8_t>(soc);
    snapshot_.lastSampleMs = nowMs;
  }
  const State rawChargeState = chargerValid ? classifyChargeStatus(chargerStatus) : State::Unknown;
  const State verifiedChargeState = reconcileStateOfCharge(
      rawChargeState, socValid, static_cast<uint8_t>(soc));
  if (chargerValid && (verifiedChargeState == State::Charging || verifiedChargeState == State::Full)) {
    // Charger evidence is independent of gauge evidence. Preserve a fresh LKG
    // percentage when available, but never hide verified charging/full merely
    // because the separate SOC transaction failed.
    snapshot_.percentAvailable = socValid || retainedPercentFresh;
    snapshot_.state = verifiedChargeState;
  } else if (chargerValid && verifiedChargeState == State::Verifying) {
    // Termination with low, stale, or unavailable SOC is contradictory evidence.
    // Preserve a bounded LKG percentage only as data; never promote it to FULL.
    snapshot_.percentAvailable = socValid || retainedPercentFresh;
    snapshot_.state = State::Verifying;
  } else if (!gaugeObserved) {
    consecutiveSocFailures_ = 0;
    snapshot_.state = State::NotPresent;
    snapshot_.percentAvailable = false;
  } else if (!socValid) {
    // A malformed or failed transaction is never accepted as a new sample.
    // Keep a previously validated SOC visible only while that older sample is
    // still inside the freshness bound; sampleValid remains false so the detail
    // page and qualification output truthfully identify the failed attempt.
    if (consecutiveSocFailures_ < UINT8_MAX) ++consecutiveSocFailures_;
    snapshot_.percentAvailable = retainedPercentFresh;
    snapshot_.state = retainedPercentFresh ? State::Stale :
        (consecutiveSocFailures_ >= kFailuresBeforeError ? State::Error : State::Unknown);
  } else {
    snapshot_.state = chargerValid ? verifiedChargeState : State::Available;
  }
  if (!socValid && !retainedPercentFresh) snapshot_.diagnosis = Diagnosis::GaugeStale;
  else if (snapshot_.chargePhase == ChargePhase::Complete &&
           snapshot_.percentAvailable && snapshot_.percent < nearFullThresholdPercent())
    snapshot_.diagnosis = Diagnosis::ChargeCompleteBelowThreshold;
  else if (snapshot_.chargerConnection == ChargerConnection::Connected &&
           snapshot_.chargePhase == ChargePhase::NotCharging &&
           snapshot_.percentAvailable && snapshot_.percent < nearFullThresholdPercent())
    snapshot_.diagnosis = Diagnosis::ChargerNotContinuing;
  else if (snapshot_.capacityAvailable && snapshot_.fullChargeCapacityMah &&
           snapshot_.remainingCapacityMah * 100UL / snapshot_.fullChargeCapacityMah + 10 < snapshot_.percent)
    snapshot_.diagnosis = Diagnosis::GaugeModelNeedsVerification;
  else if (snapshot_.percentAvailable && snapshot_.percent < nearFullThresholdPercent())
    snapshot_.diagnosis = Diagnosis::BatteryNearReportedSoc;
  else snapshot_.diagnosis = Diagnosis::None;
  const bool recoveringSoc = gaugeObserved && !socValid;
  nextSampleMs_ = nowMs + (recoveringSoc ? kRecoverySampleIntervalMs :
      (snapshot_.state == State::Charging ? kChargingSampleIntervalMs : kNormalSampleIntervalMs));
  if (before.state != snapshot_.state ||
      before.percentAvailable != snapshot_.percentAvailable ||
      (snapshot_.percentAvailable && before.percent != snapshot_.percent) ||
      before.sampleValid != snapshot_.sampleValid ||
      before.chargeStatusVerified != snapshot_.chargeStatusVerified ||
      before.chargerConnection != snapshot_.chargerConnection ||
      before.chargePhase != snapshot_.chargePhase || before.diagnosis != snapshot_.diagnosis ||
      before.voltageAvailable != snapshot_.voltageAvailable ||
      (snapshot_.voltageAvailable && before.voltageMillivolts != snapshot_.voltageMillivolts) ||
      before.currentAvailable != snapshot_.currentAvailable ||
      (snapshot_.currentAvailable && before.averageCurrentMilliamps != snapshot_.averageCurrentMilliamps) ||
      before.capacityAvailable != snapshot_.capacityAvailable ||
      (snapshot_.capacityAvailable && (before.remainingCapacityMah != snapshot_.remainingCapacityMah ||
       before.fullChargeCapacityMah != snapshot_.fullChargeCapacityMah))) ++snapshot_.version;
  // Diagnostics expose only bounded state names and validity flags. Raw charger
  // words, SOC values, device secrets, and user data are never emitted.
  Serial.printf("BATTERY read=%s soc_valid=%s charge_status_verified=%s charger_state=%s state=%s freshness=%s\n",
                snapshot_.sampleValid ? "SUCCESS" : "FAILURE",
                socValid ? "YES" : "NO",
                chargerValid ? "YES" : "NO", stateName(rawChargeState),
                stateName(snapshot_.state),
                snapshot_.percentAvailable ? (socValid ? "FRESH" : "LKG") : "UNAVAILABLE");
}

}  // namespace battery