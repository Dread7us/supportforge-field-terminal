#pragma once

#include <math.h>
#include <stdint.h>
#include <string.h>

#include "app_config.h"

namespace telemetry {

enum class WifiState : uint8_t { NotConfigured, Connecting, Connected, Reconnecting, Failed };
enum class FetchState : uint8_t { SetupRequired, Connecting, Online, Stale, Degraded, AuthError, Offline };
enum class Endpoint : uint8_t { None, EP1, EP2 };
enum class Diagnostic : uint8_t { None, Online, AuthError, HttpError, TcpFail, Timeout, InvalidData };

struct NumericValue {
  bool available = false;
  double value = 0.0;
};

struct TextValue {
  bool available = false;
  char value[appconfig::kMaximumStringLength + 1]{};
};

struct Disk {
  TextValue fs;
  TextValue mount;
  bool sizeAvailable = false;
  uint64_t sizeBytes = 0;
  bool usedAvailable = false;
  uint64_t usedBytes = 0;
  bool availableBytesAvailable = false;
  uint64_t availableBytes = 0;
  NumericValue usedPercent;
};

struct SpeedTest {
  NumericValue down;
  NumericValue up;
  NumericValue ping;
  TextValue lastRun;
  bool isRunningAvailable = false;
  bool isRunning = false;
  TextValue status;
  TextValue startedAt;
  TextValue error;
  TextValue provider;
};

struct Snapshot {
  uint32_t version = 0;
  FetchState fetchState = FetchState::SetupRequired;
  uint32_t lastAttemptMs = 0;
  uint32_t lastSuccessMs = 0;
  uint8_t consecutiveFailedCycles = 0;
  Endpoint activeEndpoint = Endpoint::None;
  Diagnostic diagnostic = Diagnostic::None;
  int httpStatus = 0;
  WifiState wifiState = WifiState::NotConfigured;
  bool rssiAvailable = false;
  int16_t rssi = 0;
  TextValue host;
  TextValue systemStatus;
  bool explicitSystemStatus = false;
  NumericValue cpuLoad;
  NumericValue cpuTemperature;
  NumericValue ramUsedGb;
  NumericValue ramTotalGb;
  NumericValue ramPercent;
  Disk disks[appconfig::kMaximumDisks]{};
  uint8_t diskCount = 0;
  NumericValue nvmeTemperature;
  bool uptimeAvailable = false;
  uint64_t uptimeSeconds = 0;
  SpeedTest speedTest;
  uint16_t recognizedFields = 0;
  bool optionalDataMissing = false;
  appconfig::TemperatureUnit displayTemperatureUnit = appconfig::kDefaultDisplayTemperatureUnit;
};

inline const char* endpointName(Endpoint endpoint) {
  return endpoint == Endpoint::EP1 ? "EP1" : (endpoint == Endpoint::EP2 ? "EP2" : "--");
}

inline const char* wifiStateName(WifiState state) {
  switch (state) {
    case WifiState::NotConfigured: return "NOT CONFIGURED";
    case WifiState::Connecting: return "CONNECTING";
    case WifiState::Connected: return "CONNECTED";
    case WifiState::Reconnecting: return "RECONNECTING";
    case WifiState::Failed: return "FAILED";
  }
  return "FAILED";
}

inline const char* fetchStateName(FetchState state) {
  switch (state) {
    case FetchState::SetupRequired: return "SETUP REQUIRED";
    case FetchState::Connecting: return "CONNECTING";
    case FetchState::Online: return "ONLINE";
    case FetchState::Stale: return "STALE";
    case FetchState::Degraded: return "DEGRADED";
    case FetchState::AuthError: return "AUTH ERROR";
    case FetchState::Offline: return "OFFLINE";
  }
  return "OFFLINE";
}

inline const char* diagnosticName(Diagnostic state) {
  switch (state) {
    case Diagnostic::None: return "--";
    case Diagnostic::Online: return "ONLINE";
    case Diagnostic::AuthError: return "AUTH ERROR";
    case Diagnostic::HttpError: return "HTTP ERROR";
    case Diagnostic::TcpFail: return "TCP FAIL";
    case Diagnostic::Timeout: return "TIMEOUT";
    case Diagnostic::InvalidData: return "INVALID DATA";
  }
  return "INVALID DATA";
}

inline double displayTemperature(double sourceValue, appconfig::TemperatureUnit displayUnit) {
  if (appconfig::kGuardianSourceTemperatureUnit == displayUnit) return sourceValue;
  return displayUnit == appconfig::TemperatureUnit::Celsius
             ? (sourceValue - 32.0) * 5.0 / 9.0
             : sourceValue * 9.0 / 5.0 + 32.0;
}

}  // namespace telemetry