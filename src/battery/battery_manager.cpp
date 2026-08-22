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
constexpr uint8_t kRemainingCapacityRegister = 0x10;
constexpr uint8_t kFullChargeCapacityRegister = 0x12;
constexpr uint8_t kAverageCurrentRegister = 0x14;
constexpr uint8_t kDesignCapacityRegister = 0x3C;
constexpr uint8_t kChargerStatusRegister = 0x0B;
constexpr uint8_t kChargeStatusMask = 0x18;
constexpr uint8_t kVbusStatusMask = 0xE0;
constexpr uint32_t kNormalSampleIntervalMs = 90UL * 1000UL;
constexpr uint32_t kChargingSampleIntervalMs = 45UL * 1000UL;
constexpr uint32_t kRecoverySampleIntervalMs = 5UL * 1000UL;
constexpr uint8_t kFailuresBeforeError = 3;
constexpr uint32_t kMaximumFreshAgeMs = maximumFreshAgeMs();
constexpr uint16_t kFullVoltageEvidenceMv = 4150;
constexpr int16_t kFullCurrentEvidenceMa = 100;
constexpr uint8_t kCapacityAgreementTolerancePercent = 3;
constexpr uint8_t kEvidenceSampleCount = 3;
constexpr uint16_t kVoltageSampleToleranceMv = 30;
constexpr uint16_t kCurrentSampleToleranceMa = 150;
constexpr uint16_t kCapacitySampleToleranceMah = 3;
// LILYGO specifies the T5-4.7 E-Paper S3 Pro pack as 3.7 V / 1500 mAh, and
// its official H752-01 BQ27220 data-memory image sets Full Charge Capacity and
// Design Capacity to 1500 mAh. Larger observations contradict that model; they
// do not prove that the physical pack has a larger capacity.
constexpr uint16_t kDocumentedPackCapacityMah = 1500;

struct EvidenceFrame {
  uint16_t soc = 0;
  uint16_t voltage = 0;
  uint16_t remainingCapacity = 0;
  uint16_t fullChargeCapacity = 0;
  uint16_t designCapacity = 0;
  int16_t averageCurrent = 0;
  uint8_t chargerStatus = 0;
  bool socValid = false;
  bool voltageValid = false;
  bool remainingValid = false;
  bool fullCapacityValid = false;
  bool designCapacityValid = false;
  bool currentValid = false;
  bool chargerValid = false;
};

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

bool readGaugeWord(uint8_t command, uint16_t& value) {
  uint8_t data[2]{};
  if (!readRegister(kGaugeAddress, command, data, sizeof(data))) return false;
  value = decodeLittleEndianWord(data[0], data[1]);
  return true;
}

bool readChargerStatus(uint8_t& value) {
  return readRegister(kChargerAddress, kChargerStatusRegister, &value, 1);
}

uint16_t spread(uint16_t a, uint16_t b, uint16_t c) {
  return max(a, max(b, c)) - min(a, min(b, c));
}

uint16_t median(uint16_t a, uint16_t b, uint16_t c) {
  return a + b + c - min(a, min(b, c)) - max(a, max(b, c));
}

bool stableWords(const EvidenceFrame (&frames)[kEvidenceSampleCount],
                 uint16_t EvidenceFrame::*field, bool EvidenceFrame::*valid,
                 uint16_t tolerance, uint16_t& value) {
  if (!(frames[0].*valid) || !(frames[1].*valid) || !(frames[2].*valid)) return false;
  const uint16_t a = frames[0].*field, b = frames[1].*field, c = frames[2].*field;
  if (spread(a, b, c) > tolerance) return false;
  value = median(a, b, c);
  return true;
}

bool stableCurrent(const EvidenceFrame (&frames)[kEvidenceSampleCount], int16_t& value) {
  if (!frames[0].currentValid || !frames[1].currentValid || !frames[2].currentValid) return false;
  const int16_t low = min(frames[0].averageCurrent,
      min(frames[1].averageCurrent, frames[2].averageCurrent));
  const int16_t high = max(frames[0].averageCurrent,
      max(frames[1].averageCurrent, frames[2].averageCurrent));
  if (static_cast<uint16_t>(high - low) > kCurrentSampleToleranceMa) return false;
  value = static_cast<int16_t>(frames[0].averageCurrent + frames[1].averageCurrent +
      frames[2].averageCurrent - low - high);
  return true;
}

bool stableCharger(const EvidenceFrame (&frames)[kEvidenceSampleCount], uint8_t& value) {
  if (!frames[0].chargerValid || !frames[1].chargerValid || !frames[2].chargerValid ||
      frames[0].chargerStatus != frames[1].chargerStatus ||
      frames[1].chargerStatus != frames[2].chargerStatus) return false;
  value = frames[2].chargerStatus;
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

const char* displaySourceName(DisplaySource value) {
  switch (value) {
    case DisplaySource::GaugeSoc: return "GAUGE SOC";
    case DisplaySource::CapacityRatio: return "CAPACITY RATIO";
    case DisplaySource::Unavailable: return "UNAVAILABLE";
  }
  return "UNAVAILABLE";
}

const char* capacityFieldStatusName(CapacityFieldStatus value) {
  switch (value) {
    case CapacityFieldStatus::Available: return "AVAILABLE";
    case CapacityFieldStatus::Unavailable: return "UNAVAILABLE";
    case CapacityFieldStatus::Invalid: return "INVALID";
  }
  return "UNAVAILABLE";
}

const char* diagnosisName(Diagnosis value) {
  switch (value) {
    case Diagnosis::BatteryNearReportedSoc: return "GAUGE REPORTS BATTERY NEAR SHOWN SOC";
    case Diagnosis::ChargerNotContinuing: return "INPUT PRESENT; CHARGER NOT CONTINUING";
    case Diagnosis::GaugeStale: return "GAUGE SOC IS STALE OR INVALID";
    case Diagnosis::GenuinePartialCharge: return "PARTIAL CHARGE CONFIRMED BY VOLTAGE / CAPACITY";
    case Diagnosis::GaugeModelMismatch: return "GAUGE MODEL MISMATCH: VALID CAPACITY RATIO CONTRADICTS SOC";
    case Diagnosis::CapacityDataUnavailable: return "CAPACITY DATA UNAVAILABLE";
    case Diagnosis::CapacityModelInvalid: return "BATTERY GAUGE CAPACITY MODEL INVALID";
    case Diagnosis::None: return "NO CONTRADICTION OBSERVED";
  }
  return "NO CONTRADICTION OBSERVED";
}

State reconcileStateOfCharge(State chargerState, bool fullEvidence, uint8_t percent) {
  if (chargerState != State::Full) return chargerState;
  // The charger's termination bit is necessary but insufficient. FULL requires
  // one fresh, corroborated evidence batch and an authoritative 100% source.
  return fullEvidence && percent == 100
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
    snapshot_.displaySource = DisplaySource::Unavailable;
    snapshot_.rawSocAvailable = false;
    snapshot_.capacityRatioAvailable = false;
    snapshot_.fullEvidence = false;
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
  snapshot_.sampleAttempted = true;
  snapshot_.lastAttemptMs = nowMs;
  EvidenceFrame frames[kEvidenceSampleCount]{};
  for (uint8_t index = 0; index < kEvidenceSampleCount; ++index) {
    EvidenceFrame& frame = frames[index];
    frame.socValid = gaugeObserved && readGaugeWord(kStateOfChargeRegister, frame.soc) &&
        validPercent(frame.soc);
    frame.remainingValid = gaugeObserved &&
        readGaugeWord(kRemainingCapacityRegister, frame.remainingCapacity);
    frame.fullCapacityValid = gaugeObserved &&
        readGaugeWord(kFullChargeCapacityRegister, frame.fullChargeCapacity);
    frame.designCapacityValid = gaugeObserved &&
        readGaugeWord(kDesignCapacityRegister, frame.designCapacity);
    frame.voltageValid = gaugeObserved && readGaugeWord(kVoltageRegister, frame.voltage);
    uint16_t currentWord = 0;
    frame.currentValid = gaugeObserved && readGaugeWord(kAverageCurrentRegister, currentWord);
    frame.averageCurrent = static_cast<int16_t>(currentWord);
    frame.chargerValid = chargerObserved && readChargerStatus(frame.chargerStatus);
  }
  uint16_t soc = 0, voltage = 0, remainingCapacity = 0, fullChargeCapacity = 0,
      designCapacity = 0;
  int16_t averageCurrent = 0;
  uint8_t chargerStatus = 0;
  const bool socValid = stableWords(frames, &EvidenceFrame::soc, &EvidenceFrame::socValid, 1, soc) &&
      validPercent(soc);
  const bool voltageValid = stableWords(frames, &EvidenceFrame::voltage,
      &EvidenceFrame::voltageValid, kVoltageSampleToleranceMv, voltage);
  const bool currentValid = stableCurrent(frames, averageCurrent);
  const bool remainingValid = stableWords(frames, &EvidenceFrame::remainingCapacity,
      &EvidenceFrame::remainingValid, kCapacitySampleToleranceMah, remainingCapacity);
  const bool fullCapacityValid = stableWords(frames, &EvidenceFrame::fullChargeCapacity,
      &EvidenceFrame::fullCapacityValid, kCapacitySampleToleranceMah, fullChargeCapacity);
  const bool designCapacityValid = stableWords(frames, &EvidenceFrame::designCapacity,
      &EvidenceFrame::designCapacityValid, kCapacitySampleToleranceMah, designCapacity);
  const bool chargerValid = stableCharger(frames, chargerStatus);
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
  snapshot_.averageCurrentMilliamps = currentValid ? averageCurrent : 0;
  snapshot_.remainingCapacityStatus = !remainingValid ? CapacityFieldStatus::Unavailable :
      (remainingCapacity > kDocumentedPackCapacityMah ? CapacityFieldStatus::Invalid :
       CapacityFieldStatus::Available);
  snapshot_.fullChargeCapacityStatus = !fullCapacityValid ? CapacityFieldStatus::Unavailable :
      (fullChargeCapacity == 0 || fullChargeCapacity > kDocumentedPackCapacityMah
          ? CapacityFieldStatus::Invalid : CapacityFieldStatus::Available);
  snapshot_.designCapacityStatus = !designCapacityValid ? CapacityFieldStatus::Unavailable :
      (designCapacity != kDocumentedPackCapacityMah
          ? CapacityFieldStatus::Invalid : CapacityFieldStatus::Available);
  if (snapshot_.remainingCapacityStatus == CapacityFieldStatus::Available &&
      snapshot_.fullChargeCapacityStatus == CapacityFieldStatus::Available &&
      remainingCapacity > fullChargeCapacity) {
    snapshot_.remainingCapacityStatus = CapacityFieldStatus::Invalid;
  }
  snapshot_.capacityAvailable =
      snapshot_.remainingCapacityStatus == CapacityFieldStatus::Available &&
      snapshot_.fullChargeCapacityStatus == CapacityFieldStatus::Available &&
      snapshot_.designCapacityStatus == CapacityFieldStatus::Available;
  // Retain stable raw observations even when they are invalid so diagnostics
  // can show the contradiction (for example 2386 / 3000 mAh) transparently.
  snapshot_.remainingCapacityMah = remainingValid ? remainingCapacity : 0;
  snapshot_.fullChargeCapacityMah = fullCapacityValid ? fullChargeCapacity : 0;
  snapshot_.designCapacityMah = designCapacityValid ? designCapacity : 0;
  snapshot_.rawSocAvailable = socValid;
  snapshot_.rawSocPercent = socValid ? static_cast<uint8_t>(soc) : 0;
  snapshot_.capacityRatioAvailable = snapshot_.capacityAvailable;
  snapshot_.capacityRatioPercent = snapshot_.capacityAvailable
      ? static_cast<uint8_t>((remainingCapacity * 100UL + fullChargeCapacity / 2) /
            fullChargeCapacity) : 0;
  const bool capacityAgrees = socValid && snapshot_.capacityRatioAvailable &&
      abs(static_cast<int>(snapshot_.capacityRatioPercent) - static_cast<int>(soc)) <=
          kCapacityAgreementTolerancePercent;
  const bool terminatedAtFullVoltage = chargerValid &&
      snapshot_.chargerConnection == ChargerConnection::Connected &&
      snapshot_.chargePhase == ChargePhase::Complete && voltageValid &&
      voltage >= kFullVoltageEvidenceMv && currentValid &&
      abs(static_cast<int>(averageCurrent)) <= kFullCurrentEvidenceMa;
  const bool capacityProvesFull = snapshot_.capacityRatioAvailable &&
      snapshot_.capacityRatioPercent == 100 && terminatedAtFullVoltage;
  // A 100% display always requires a valid 100% capacity ratio. Raw SOC remains
  // authoritative when it agrees; only the contradictory 80/100-style case
  // selects the ratio, and only with independent charger/voltage/current proof.
  snapshot_.fullEvidence = socValid && capacityProvesFull;
  if (socValid) {
    consecutiveSocFailures_ = 0;
    snapshot_.percentAvailable = true;
    snapshot_.hasValidSample = true;
    snapshot_.percent = static_cast<uint8_t>(soc);
    snapshot_.displaySource = DisplaySource::GaugeSoc;
    snapshot_.lastSampleMs = nowMs;
  }
  if (socValid && !capacityAgrees && capacityProvesFull) {
    snapshot_.percent = snapshot_.capacityRatioPercent;
    snapshot_.displaySource = DisplaySource::CapacityRatio;
  }
  const State rawChargeState = chargerValid ? classifyChargeStatus(chargerStatus) : State::Unknown;
  const State verifiedChargeState = reconcileStateOfCharge(
      rawChargeState, snapshot_.fullEvidence, snapshot_.percent);
  if (chargerValid && (verifiedChargeState == State::Charging || verifiedChargeState == State::Full)) {
    // Charger evidence is independent, but the displayed source must still be
    // part of this same fresh validated evidence batch.
    snapshot_.percentAvailable = socValid;
    snapshot_.state = verifiedChargeState;
  } else if (chargerValid && verifiedChargeState == State::Verifying) {
    // Termination with low, stale, or unavailable SOC is contradictory evidence.
    // Never retain stale percentage data or promote incomplete evidence to FULL.
    snapshot_.percentAvailable = socValid;
    snapshot_.state = State::Verifying;
  } else if (!gaugeObserved) {
    consecutiveSocFailures_ = 0;
    snapshot_.state = State::NotPresent;
    snapshot_.percentAvailable = false;
  } else if (!socValid) {
    // A malformed or failed transaction is never accepted as a new sample.
    // A failed fresh evidence batch cannot retain an authoritative percentage.
    if (consecutiveSocFailures_ < UINT8_MAX) ++consecutiveSocFailures_;
    snapshot_.percentAvailable = false;
    snapshot_.displaySource = DisplaySource::Unavailable;
    snapshot_.state = consecutiveSocFailures_ >= kFailuresBeforeError ? State::Error : State::Stale;
  } else {
    snapshot_.state = chargerValid ? verifiedChargeState : State::Available;
  }
  const bool capacityModelInvalid =
      snapshot_.remainingCapacityStatus == CapacityFieldStatus::Invalid ||
      snapshot_.fullChargeCapacityStatus == CapacityFieldStatus::Invalid ||
      snapshot_.designCapacityStatus == CapacityFieldStatus::Invalid;
  if (!socValid) snapshot_.diagnosis = Diagnosis::GaugeStale;
  else if (capacityModelInvalid)
    snapshot_.diagnosis = Diagnosis::CapacityModelInvalid;
  else if (!snapshot_.capacityAvailable)
    snapshot_.diagnosis = Diagnosis::CapacityDataUnavailable;
  else if (snapshot_.capacityRatioAvailable && !capacityAgrees)
    snapshot_.diagnosis = Diagnosis::GaugeModelMismatch;
  else if (snapshot_.chargerConnection == ChargerConnection::Connected &&
           snapshot_.chargePhase == ChargePhase::NotCharging &&
           snapshot_.percentAvailable && snapshot_.percent < nearFullThresholdPercent())
    snapshot_.diagnosis = Diagnosis::ChargerNotContinuing;
  else if (socValid && capacityAgrees && soc < nearFullThresholdPercent() &&
           !terminatedAtFullVoltage)
    snapshot_.diagnosis = Diagnosis::GenuinePartialCharge;
  else if (snapshot_.percentAvailable && snapshot_.percent < nearFullThresholdPercent())
    snapshot_.diagnosis = Diagnosis::BatteryNearReportedSoc;
  else snapshot_.diagnosis = Diagnosis::None;
  const bool recoveringSoc = gaugeObserved && !socValid;
  nextSampleMs_ = nowMs + (recoveringSoc ? kRecoverySampleIntervalMs :
      (snapshot_.state == State::Charging ? kChargingSampleIntervalMs : kNormalSampleIntervalMs));
  if (before.state != snapshot_.state ||
       before.percentAvailable != snapshot_.percentAvailable ||
      (snapshot_.percentAvailable && before.percent != snapshot_.percent) ||
       before.displaySource != snapshot_.displaySource ||
       before.rawSocAvailable != snapshot_.rawSocAvailable ||
       (snapshot_.rawSocAvailable && before.rawSocPercent != snapshot_.rawSocPercent) ||
       before.capacityRatioAvailable != snapshot_.capacityRatioAvailable ||
       (snapshot_.capacityRatioAvailable && before.capacityRatioPercent != snapshot_.capacityRatioPercent) ||
       before.fullEvidence != snapshot_.fullEvidence ||
      before.sampleValid != snapshot_.sampleValid ||
      before.chargeStatusVerified != snapshot_.chargeStatusVerified ||
      before.chargerConnection != snapshot_.chargerConnection ||
      before.chargePhase != snapshot_.chargePhase || before.diagnosis != snapshot_.diagnosis ||
      before.voltageAvailable != snapshot_.voltageAvailable ||
      (snapshot_.voltageAvailable && before.voltageMillivolts != snapshot_.voltageMillivolts) ||
      before.currentAvailable != snapshot_.currentAvailable ||
      (snapshot_.currentAvailable && before.averageCurrentMilliamps != snapshot_.averageCurrentMilliamps) ||
      before.remainingCapacityStatus != snapshot_.remainingCapacityStatus ||
      before.fullChargeCapacityStatus != snapshot_.fullChargeCapacityStatus ||
      before.designCapacityStatus != snapshot_.designCapacityStatus ||
      before.capacityAvailable != snapshot_.capacityAvailable ||
      before.remainingCapacityMah != snapshot_.remainingCapacityMah ||
      before.fullChargeCapacityMah != snapshot_.fullChargeCapacityMah ||
      before.designCapacityMah != snapshot_.designCapacityMah) ++snapshot_.version;
  // One bounded read-only evidence record contains only authorized battery and
  // charger observations. It never emits raw charger words or unrelated data.
  const String rawSocText = snapshot_.rawSocAvailable ? String(snapshot_.rawSocPercent) : "--";
  const String displayedText = snapshot_.percentAvailable ? String(snapshot_.percent) : "--";
  const String ratioText = snapshot_.capacityRatioAvailable
      ? String(snapshot_.capacityRatioPercent) : "--";
  const String remainingText = snapshot_.remainingCapacityStatus != CapacityFieldStatus::Unavailable
      ? String(snapshot_.remainingCapacityMah) : "--";
  const String fullText = snapshot_.fullChargeCapacityStatus != CapacityFieldStatus::Unavailable
      ? String(snapshot_.fullChargeCapacityMah) : "--";
  const String designText = snapshot_.designCapacityStatus != CapacityFieldStatus::Unavailable
      ? String(snapshot_.designCapacityMah) : "--";
  const String voltageText = snapshot_.voltageAvailable ? String(snapshot_.voltageMillivolts) : "--";
  const String currentText = snapshot_.currentAvailable
      ? String(snapshot_.averageCurrentMilliamps) : "--";
  Serial.printf("BATTERY read=%s samples=%u raw_soc=%s displayed=%s source=%s ratio=%s remaining=%s full=%s design=%s documented_pack=%u voltage=%s current=%s input=%s phase=%s freshness=%s diagnosis=\"%s\"\n",
                snapshot_.sampleValid ? "SUCCESS" : "FAILURE",
                 static_cast<unsigned>(kEvidenceSampleCount),
                 rawSocText.c_str(), displayedText.c_str(),
                 displaySourceName(snapshot_.displaySource),
                 ratioText.c_str(), remainingText.c_str(), fullText.c_str(),
                 designText.c_str(), static_cast<unsigned>(kDocumentedPackCapacityMah),
                 voltageText.c_str(), currentText.c_str(),
                 chargerConnectionName(snapshot_.chargerConnection),
                 chargePhaseName(snapshot_.chargePhase),
                snapshot_.percentAvailable ? (socValid ? "FRESH" : "LKG") : "UNAVAILABLE",
                 diagnosisName(snapshot_.diagnosis));
}

}  // namespace battery