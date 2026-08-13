#pragma once

#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include <freertos/semphr.h>

#include "telemetry_model.h"

namespace telemetry {

class TelemetryManager {
 public:
  bool begin();
  Snapshot snapshot() const;
  uint32_t version() const;
  void requestRefresh();
  void toggleTemperatureUnit();
  uint32_t nextPollInSeconds(uint32_t nowMs) const;

 private:
  static void taskEntry(void* context);
  void run();
  void publish(const Snapshot& next);
  void updateConnectionState(WifiState state, bool connected);
  void performPollingCycle(uint32_t nowMs);

  mutable SemaphoreHandle_t mutex_ = nullptr;
  mutable portMUX_TYPE scheduleMux_ = portMUX_INITIALIZER_UNLOCKED;
  Snapshot snapshot_{};
  Preferences preferences_;
  bool refreshRequested_ = false;
  uint32_t nextPollMs_ = 0;
  uint32_t nextWifiAttemptMs_ = 0;
  uint32_t wifiAttemptStartedMs_ = 0;
  uint32_t reconnectDelayMs_ = 2000;
  uint32_t nextPrimaryProbeMs_ = 0;
  Endpoint preferredEndpoint_ = Endpoint::EP1;
};

}  // namespace telemetry