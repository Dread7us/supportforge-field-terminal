#pragma once

#include <stdint.h>

namespace battery {

// These states describe only evidence read from the documented BQ27220 and
// BQ25896 read-only contracts. No gauge configuration/control path exists here.
enum class State : uint8_t { Available, Charging, Full, Verifying, Stale, Unknown, NotPresent, Error };

enum class ChargerConnection : uint8_t { Unknown, NotConnected, Connected };
enum class ChargePhase : uint8_t { Unknown, NotCharging, Precharge, FastCharge, Complete };
enum class Diagnosis : uint8_t {
  None, BatteryNearReportedSoc, ChargerNotContinuing, GaugeStale,
  ChargeCompleteBelowThreshold, GaugeModelNeedsVerification
};

struct Snapshot {
  State state = State::NotPresent;
  bool percentAvailable = false;
  uint8_t percent = 0;
  bool sampleAttempted = false;
  bool sampleValid = false;
  bool chargeStatusVerified = false;
  ChargerConnection chargerConnection = ChargerConnection::Unknown;
  ChargePhase chargePhase = ChargePhase::Unknown;
  Diagnosis diagnosis = Diagnosis::None;
  bool voltageAvailable = false;
  uint16_t voltageMillivolts = 0;
  bool currentAvailable = false;
  int16_t averageCurrentMilliamps = 0;
  bool capacityAvailable = false;
  uint16_t remainingCapacityMah = 0;
  uint16_t fullChargeCapacityMah = 0;
  bool hasValidSample = false;
  uint32_t lastSampleMs = 0;
  uint32_t lastAttemptMs = 0;
  uint32_t version = 1;
};

class BatteryManager {
 public:
  void begin(bool gaugeObserved, bool chargerObserved);
  void poll(uint32_t nowMs, bool gaugeObserved, bool chargerObserved,
            bool scheduledAwake = true);
  Snapshot snapshot() const { return snapshot_; }
  uint32_t version() const { return snapshot_.version; }

 private:
  void sample(uint32_t nowMs, bool gaugeObserved, bool chargerObserved);
  Snapshot snapshot_{};
  uint32_t nextSampleMs_ = 0;
  uint8_t consecutiveSocFailures_ = 0;
};

const char* stateName(State value);
bool validPercent(int value);
uint16_t decodeLittleEndianWord(uint8_t low, uint8_t high);
State classifyChargeStatus(uint8_t register0b);
ChargerConnection classifyChargerConnection(uint8_t register0b);
ChargePhase classifyChargePhase(uint8_t register0b);
State reconcileStateOfCharge(State chargerState, bool socFresh, uint8_t percent);
const char* chargerConnectionName(ChargerConnection value);
const char* chargePhaseName(ChargePhase value);
const char* diagnosisName(Diagnosis value);
constexpr uint8_t nearFullThresholdPercent() { return 95; }
constexpr uint32_t maximumFreshAgeMs() { return 270UL * 1000UL; }

}  // namespace battery