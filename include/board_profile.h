#pragma once

#include <Arduino.h>

namespace hq {

struct BoardProfile {
  const char *name;
  const char *source;
  int i2cSda;
  int i2cScl;
  int spiMiso;
  int spiMosi;
  int spiSclk;
  int sdCs;
  int loraCs;
  int loraIrq;
  int loraReset;
  int loraBusy;
  int gpsRx;
  int gpsTx;
  int frontLightPwm;
  bool hasPca9535;
  bool gpsCandidateAvailable;
  bool frontLightCandidateAvailable;
};

#if defined(HQ_PROFILE_LEGACY_H752)
static constexpr BoardProfile kBoard{
    "legacy H752 comparison",
    "LILYGO T5S3-4.7-e-paper-PRO branch H752 README @ 5067e1f clone",
    6, 5, 8, 17, 18, 16, 46, 3, 43, 44,
    -1, -1, -1, false, false, false};
#else
static constexpr BoardProfile kBoard{
    "H752-02 current-Pro candidate",
    "Pro V1.0 evidence @ 587632e; disabled pending installed-PCB revision proof",
    39, 40, 21, 13, 14, 12, 46, 10, 1, 47,
    44, 43, 11, true, true, false};
#endif

static constexpr float kLoRaFrequencyMHz = 915.0F;
static constexpr uint8_t kPca9535Address = 0x20;
static constexpr uint8_t kPcaPort0Output = 0x02;
static constexpr uint8_t kPcaPort0Config = 0x06;
static constexpr uint8_t kSharedRailBit = 0x01;  // PCA9535 P0.0 / LORA_EN.

}  // namespace hq
