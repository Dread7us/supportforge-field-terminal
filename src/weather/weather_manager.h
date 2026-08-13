#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <stdint.h>

namespace weather {

enum class State : uint8_t { SetupRequired, WaitingForWifi, Online, Offline };

struct Snapshot {
  State state = State::SetupRequired;
  bool dataAvailable = false;
  int16_t temperatureTenths = 0;
  uint8_t weatherCode = 255;
  char city[32]{};
  uint32_t lastSuccessMs = 0;
  uint32_t version = 1;
};

class WeatherManager {
 public:
  bool begin();
  Snapshot snapshot() const;
  uint32_t version() const;

 private:
  static void taskEntry(void* context);
  void run();
  void poll(uint32_t nowMs);
  void publish(const Snapshot& next);
  mutable SemaphoreHandle_t mutex_ = nullptr;
  Snapshot snapshot_{};
  uint32_t nextPollMs_ = 0;
};

const char* stateName(State value);
const char* conditionName(uint8_t weatherCode);

}  // namespace weather