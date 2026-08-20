#pragma once

#include <stdint.h>
#include <string.h>

#include "secrets.h"

// Weather settings are optional and deliberately compile to an unconfigured
// state until matching macros are added to the ignored src/secrets.h.
#ifndef SUPPORTFORGE_WEATHER_LATITUDE
#define SUPPORTFORGE_WEATHER_LATITUDE "YOUR_LATITUDE"
#endif
#ifndef SUPPORTFORGE_WEATHER_LONGITUDE
#define SUPPORTFORGE_WEATHER_LONGITUDE "YOUR_LONGITUDE"
#endif
#ifndef SUPPORTFORGE_WEATHER_CITY_LABEL
#define SUPPORTFORGE_WEATHER_CITY_LABEL "YOUR_CITY"
#endif
#ifndef SUPPORTFORGE_WEATHER_UNIT
#define SUPPORTFORGE_WEATHER_UNIT "C"
#endif

namespace appconfig {

enum class TemperatureUnit : uint8_t { Celsius, Fahrenheit };

constexpr const char* kPrimaryTelemetryUrl = SUPPORTFORGE_PRIMARY_TELEMETRY_URL;
constexpr const char* kFallbackTelemetryUrl = SUPPORTFORGE_FALLBACK_TELEMETRY_URL;
constexpr const char* kGuardianToken = SUPPORTFORGE_GUARDIAN_TOKEN;
constexpr const char* kTargetHostName = SUPPORTFORGE_TARGET_HOST_NAME;
constexpr const char* kWeatherLatitude = SUPPORTFORGE_WEATHER_LATITUDE;
constexpr const char* kWeatherLongitude = SUPPORTFORGE_WEATHER_LONGITUDE;
constexpr const char* kWeatherCityLabel = SUPPORTFORGE_WEATHER_CITY_LABEL;
constexpr const char* kWeatherUnit = SUPPORTFORGE_WEATHER_UNIT;

constexpr uint32_t kPollIntervalMs = 60000;
constexpr uint32_t kPrimaryRetryIntervalMs = 60000;
constexpr uint32_t kWifiConnectTimeoutMs = 15000;
constexpr uint32_t kHttpConnectTimeoutMs = 6000;
constexpr uint32_t kHttpReadTimeoutMs = 10000;
constexpr uint32_t kStaleAfterMs = 90000;
constexpr uint32_t kOfflineFailedCycles = 3;
constexpr uint32_t kTaskStackBytes = 12288;
constexpr uint32_t kWeatherPollIntervalMs = 15UL * 60UL * 1000UL;
constexpr uint32_t kWeatherFreshForMs = 45UL * 60UL * 1000UL;
constexpr uint32_t kWeatherManualRefreshLimitMs = 60UL * 1000UL;
constexpr double kWeatherGpsMoveThresholdKm = 10.0;
constexpr uint32_t kWeatherTaskStackBytes = 8192;
constexpr size_t kMaximumWeatherResponseBytes = 4096;
constexpr size_t kMaximumGeocodingResponseBytes = 8192;
constexpr size_t kMaximumResponseBytes = 16384;
constexpr size_t kMaximumDisks = 6;
constexpr size_t kMaximumStringLength = 63;
constexpr bool kQueryTokenCompatibilityEnabled = false;
constexpr TemperatureUnit kGuardianSourceTemperatureUnit = TemperatureUnit::Fahrenheit;
constexpr TemperatureUnit kDefaultDisplayTemperatureUnit = TemperatureUnit::Celsius;

inline bool placeholder(const char* value, const char* expected) {
  return !value || !value[0] || strcmp(value, expected) == 0;
}

inline bool configured() {
  return !placeholder(kGuardianToken, "YOUR_FIELD_TERMINAL_TOKEN") &&
         !placeholder(kGuardianToken, "YOUR_DEVICE_SPECIFIC_TOKEN") &&
         !placeholder(kTargetHostName, "YOUR_MONITORED_HOST") &&
         kPrimaryTelemetryUrl && kPrimaryTelemetryUrl[0] &&
         kFallbackTelemetryUrl && kFallbackTelemetryUrl[0];
}

inline bool weatherConfigured() {
  return !placeholder(kWeatherLatitude, "YOUR_LATITUDE") &&
         !placeholder(kWeatherLongitude, "YOUR_LONGITUDE") &&
         !placeholder(kWeatherCityLabel, "YOUR_CITY");
}

}  // namespace appconfig