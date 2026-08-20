#include "front_light_manager.h"

#include <Arduino.h>

namespace power {

const char* frontLightLevelName(FrontLightLevel level) {
  switch (level) {
    case FrontLightLevel::Off: return "OFF";
    case FrontLightLevel::Low: return "LOW";
    case FrontLightLevel::Medium: return "MEDIUM";
    case FrontLightLevel::High: return "HIGH";
  }
  return "OFF";
}

uint8_t frontLightDuty(FrontLightLevel level) {
  // Official H752-01 factory firmware uses analogWrite duties 0/50/100/230.
  // Keep that sourced 8-bit range and Arduino-ESP32's default PWM timing.
  switch (level) {
    case FrontLightLevel::Low: return 50;
    case FrontLightLevel::Medium: return 100;
    case FrontLightLevel::High: return 230;
    case FrontLightLevel::Off: return 0;
  }
  return 0;
}

bool FrontLightManager::begin(bool candidateAvailable, int pwmPin) {
  snapshot_.candidateAvailable = candidateAvailable && pwmPin == 11;
  pwmPin_ = snapshot_.candidateAvailable ? pwmPin : -1;
  preferencesReady_ = preferences_.begin("sf_frontlight", false);
  const uint8_t stored = preferencesReady_
      ? preferences_.getUChar("level", static_cast<uint8_t>(FrontLightLevel::Off))
      : static_cast<uint8_t>(FrontLightLevel::Off);
  snapshot_.preferred = stored <= static_cast<uint8_t>(FrontLightLevel::High)
      ? static_cast<FrontLightLevel>(stored) : FrontLightLevel::Off;
  if (pwmPin_ >= 0) {
    pinMode(pwmPin_, OUTPUT);
    analogWrite(pwmPin_, 0);  // Active-high PT4103 EN/PWM; boot remains dark.
  }
  // The caller restores the preference only after local hardware/UI startup is stable.
  snapshot_.effective = FrontLightLevel::Off;
  Serial.printf("FRONT_LIGHT candidate=%s gpio=%d preferred=%s boot_output=OFF\n",
                snapshot_.candidateAvailable ? "H752_01_SOURCED" : "UNAVAILABLE",
                pwmPin_, frontLightLevelName(snapshot_.preferred));
  return preferencesReady_;
}

void FrontLightManager::restoreAfterInitialization(bool lowPowerActive) {
  const FrontLightSnapshot before = snapshot_;
  snapshot_.lowPowerSuppressed = lowPowerActive;
  apply();
  publishIfChanged(before);
}

void FrontLightManager::publishIfChanged(const FrontLightSnapshot& before) {
  if (before.preferred != snapshot_.preferred || before.effective != snapshot_.effective ||
      before.candidateAvailable != snapshot_.candidateAvailable ||
      before.lowPowerSuppressed != snapshot_.lowPowerSuppressed) {
    snapshot_.version = before.version + 1;
  }
}

void FrontLightManager::apply() {
  const FrontLightLevel wanted = snapshot_.candidateAvailable && !snapshot_.lowPowerSuppressed
      ? snapshot_.preferred : FrontLightLevel::Off;
  snapshot_.effective = wanted;
  if (pwmPin_ >= 0) analogWrite(pwmPin_, frontLightDuty(wanted));
  Serial.printf("FRONT_LIGHT preferred=%s effective=%s duty=%u low_power=%s\n",
                frontLightLevelName(snapshot_.preferred), frontLightLevelName(snapshot_.effective),
                static_cast<unsigned>(frontLightDuty(snapshot_.effective)),
                snapshot_.lowPowerSuppressed ? "SUPPRESSED" : "NO");
}

bool FrontLightManager::setPreferred(FrontLightLevel level) {
  if (!snapshot_.candidateAvailable || !preferencesReady_ || level == snapshot_.preferred) return false;
  const FrontLightSnapshot before = snapshot_;
  if (preferences_.putUChar("level", static_cast<uint8_t>(level)) != sizeof(uint8_t)) return false;
  snapshot_.preferred = level;
  apply();
  publishIfChanged(before);
  return true;
}

void FrontLightManager::setLowPowerSuppressed(bool suppressed) {
  if (snapshot_.lowPowerSuppressed == suppressed) return;
  const FrontLightSnapshot before = snapshot_;
  snapshot_.lowPowerSuppressed = suppressed;
  apply();
  publishIfChanged(before);
}

}  // namespace power