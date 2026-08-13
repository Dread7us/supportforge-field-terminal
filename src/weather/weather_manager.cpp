#include "weather_manager.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <math.h>
#include <string.h>

#include "app_config.h"

namespace weather {
namespace {
bool reached(uint32_t now, uint32_t deadline) {
  return static_cast<int32_t>(now - deadline) >= 0;
}

bool materiallyChanged(const Snapshot& a, const Snapshot& b) {
  return a.state != b.state || a.dataAvailable != b.dataAvailable ||
         a.temperatureTenths != b.temperatureTenths || a.weatherCode != b.weatherCode ||
         strcmp(a.city, b.city) != 0;
}
}

const char* stateName(State value) {
  switch (value) {
    case State::SetupRequired: return "WX SETUP";
    case State::WaitingForWifi: return "WX OFFLINE";
    case State::Online: return "WX ONLINE";
    case State::Offline: return "WX OFFLINE";
  }
  return "--";
}

const char* conditionName(uint8_t code) {
  if (code == 0) return "CLEAR";
  if (code <= 3) return "CLOUDY";
  if (code == 45 || code == 48) return "FOG";
  if (code >= 51 && code <= 67) return "RAIN";
  if (code >= 71 && code <= 77) return "SNOW";
  if (code >= 80 && code <= 82) return "SHOWERS";
  if (code >= 95 && code <= 99) return "STORM";
  return "--";
}

bool WeatherManager::begin() {
  mutex_ = xSemaphoreCreateMutex();
  if (!mutex_) return false;
  Serial.printf("WEATHER configured=%s\n", appconfig::weatherConfigured() ? "YES" : "NO");
  snapshot_.state = appconfig::weatherConfigured() ? State::WaitingForWifi : State::SetupRequired;
  strlcpy(snapshot_.city, appconfig::weatherConfigured() ? appconfig::kWeatherCityLabel : "", sizeof(snapshot_.city));
  if (!appconfig::weatherConfigured()) {
    Serial.println("WEATHER configuration=NOT_CONFIGURED");
    return true;
  }
  return xTaskCreatePinnedToCore(taskEntry, "weather", appconfig::kWeatherTaskStackBytes,
                                 this, 1, nullptr, 0) == pdPASS;
}

Snapshot WeatherManager::snapshot() const {
  Snapshot copy;
  if (mutex_ && xSemaphoreTake(mutex_, pdMS_TO_TICKS(20)) == pdTRUE) {
    copy = snapshot_;
    xSemaphoreGive(mutex_);
  }
  return copy;
}

uint32_t WeatherManager::version() const { return snapshot().version; }

void WeatherManager::taskEntry(void* context) {
  static_cast<WeatherManager*>(context)->run();
}

void WeatherManager::run() {
  for (;;) {
    const uint32_t now = millis();
    if (reached(now, nextPollMs_)) poll(now);
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void WeatherManager::poll(uint32_t nowMs) {
  nextPollMs_ = nowMs + appconfig::kWeatherPollIntervalMs;
  Snapshot next = snapshot();
  if (WiFi.status() != WL_CONNECTED) {
    next.state = State::WaitingForWifi;
    if (next.lastSuccessMs && nowMs - next.lastSuccessMs > appconfig::kWeatherFreshForMs)
      next.dataAvailable = false;
    publish(next);
    return;
  }
  const bool fahrenheit = appconfig::kWeatherUnit &&
      (appconfig::kWeatherUnit[0] == 'F' || appconfig::kWeatherUnit[0] == 'f');
  const String url = String("https://api.open-meteo.com/v1/forecast?latitude=") +
      appconfig::kWeatherLatitude + "&longitude=" + appconfig::kWeatherLongitude +
      "&current=temperature_2m,weather_code&temperature_unit=" +
      (fahrenheit ? "fahrenheit" : "celsius");
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setConnectTimeout(appconfig::kHttpConnectTimeoutMs);
  http.setTimeout(appconfig::kHttpReadTimeoutMs);
  bool valid = false;
  if (http.begin(client, url)) {
    const int status = http.GET();
    if (status >= 200 && status < 300 &&
        http.getSize() <= static_cast<int>(appconfig::kMaximumWeatherResponseBytes)) {
      String body = http.getString();
      if (body.length() <= appconfig::kMaximumWeatherResponseBytes) {
        JsonDocument document;
        if (!deserializeJson(document, body)) {
          JsonObjectConst current = document["current"].as<JsonObjectConst>();
          if (!current.isNull() && current["temperature_2m"].is<double>() &&
              current["weather_code"].is<int>()) {
            const double temperature = current["temperature_2m"].as<double>();
            const int code = current["weather_code"].as<int>();
            if (isfinite(temperature) && temperature >= -100 && temperature <= 150 &&
                code >= 0 && code <= 255) {
              next.temperatureTenths = static_cast<int16_t>(lround(temperature * 10.0));
              next.weatherCode = static_cast<uint8_t>(code);
              next.dataAvailable = true;
              next.lastSuccessMs = nowMs;
              next.state = State::Online;
              valid = true;
            }
          }
        }
      }
      body = String();
    }
    http.end();
  }
  if (!valid) {
    next.state = State::Offline;
    if (!next.lastSuccessMs || nowMs - next.lastSuccessMs > appconfig::kWeatherFreshForMs)
      next.dataAvailable = false;
  }
  Serial.printf("WEATHER result=%s data_valid=%s\n", stateName(next.state),
                next.dataAvailable ? "YES" : "NO");
  publish(next);
}

void WeatherManager::publish(const Snapshot& next) {
  if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
    const uint32_t version = materiallyChanged(snapshot_, next) ? snapshot_.version + 1
                                                                : snapshot_.version;
    snapshot_ = next;
    snapshot_.version = version;
    xSemaphoreGive(mutex_);
  }
}

}  // namespace weather