#include "telemetry_manager.h"

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFi.h>

#include "app_config.h"
#include "guardian_parser.h"

namespace telemetry {
namespace {

enum class AttemptKind : uint8_t { Valid, TransportFailure, HttpFailure, InvalidData };
struct AttemptResult {
  AttemptKind kind = AttemptKind::TransportFailure;
  Diagnostic diagnostic = Diagnostic::TcpFail;
  int httpStatus = 0;
  Snapshot telemetry;
};

bool reachedDeadline(uint32_t now, uint32_t deadline) {
  return static_cast<int32_t>(now - deadline) >= 0;
}

Diagnostic transportDiagnostic(int code) {
  return code == HTTPC_ERROR_READ_TIMEOUT
             ? Diagnostic::Timeout : Diagnostic::TcpFail;
}

AttemptResult requestEndpoint(Endpoint endpoint, const char* url,
                              appconfig::TemperatureUnit displayUnit) {
  AttemptResult result;
  HTTPClient http;
  http.setConnectTimeout(appconfig::kHttpConnectTimeoutMs);
  http.setTimeout(appconfig::kHttpReadTimeoutMs);
  http.setReuse(false);
  if (!http.begin(url)) {
    Serial.printf("TELEMETRY endpoint=%s transport=TCP_FAIL path=REDACTED\n", endpointName(endpoint));
    return result;
  }
  http.addHeader("x-guardian-telemetry-token", appconfig::kGuardianToken);
  const int status = http.GET();
  result.httpStatus = status > 0 ? status : 0;
  if (status <= 0) {
    result.diagnostic = transportDiagnostic(status);
    Serial.printf("TELEMETRY endpoint=%s transport=%s path=REDACTED\n",
                  endpointName(endpoint), diagnosticName(result.diagnostic));
    http.end();
    return result;
  }
  Serial.printf("TELEMETRY endpoint=%s http_status=%d path=REDACTED\n",
                endpointName(endpoint), status);
  if (status < 200 || status >= 300) {
    result.kind = AttemptKind::HttpFailure;
    result.diagnostic = status == 401 || status == 403 ? Diagnostic::AuthError : Diagnostic::HttpError;
    http.end();
    return result;
  }
  const int contentLength = http.getSize();
  if (contentLength > static_cast<int>(appconfig::kMaximumResponseBytes)) {
    result.kind = AttemptKind::InvalidData;
    result.diagnostic = Diagnostic::InvalidData;
    http.end();
    return result;
  }
  String body = http.getString();
  http.end();
  if (body.length() > appconfig::kMaximumResponseBytes) {
    result.kind = AttemptKind::InvalidData;
    result.diagnostic = Diagnostic::InvalidData;
    return result;
  }
  result.telemetry.displayTemperatureUnit = displayUnit;
  const ParseResult parsed = parseGuardianPayload(body.c_str(), body.length(), result.telemetry,
                                                   appconfig::kTargetHostName);
  body = String();
  if (!parsed.valid) {
    result.kind = AttemptKind::InvalidData;
    result.diagnostic = Diagnostic::InvalidData;
    Serial.printf("TELEMETRY endpoint=%s parser=INVALID recognized=0\n", endpointName(endpoint));
    return result;
  }
  result.kind = AttemptKind::Valid;
  result.diagnostic = Diagnostic::Online;
  Serial.printf("TELEMETRY endpoint=%s parser=VALID recognized=%u\n",
                endpointName(endpoint), static_cast<unsigned>(parsed.recognizedFields));
  return result;
}

}  // namespace

bool TelemetryManager::begin() {
  mutex_ = xSemaphoreCreateMutex();
  if (!mutex_) return false;
  snapshot_.displayTemperatureUnit = appconfig::kDefaultDisplayTemperatureUnit;
  if (preferences_.begin("sf_telemetry", false)) {
    snapshot_.displayTemperatureUnit = preferences_.getBool("temp_f", false)
        ? appconfig::TemperatureUnit::Fahrenheit : appconfig::TemperatureUnit::Celsius;
  }
  snapshot_.fetchState = appconfig::configured() ? FetchState::Connecting : FetchState::SetupRequired;
  snapshot_.wifiState = appconfig::configured() ? WifiState::Connecting : WifiState::NotConfigured;
  snapshot_.version = 1;
  if (!appconfig::configured()) {
    Serial.println("TELEMETRY configuration=NOT_CONFIGURED local_file=src/secrets.h");
    return true;
  }
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);
  return xTaskCreatePinnedToCore(taskEntry, "guardian", appconfig::kTaskStackBytes,
                                 this, 1, nullptr, 0) == pdPASS;
}

Snapshot TelemetryManager::snapshot() const {
  Snapshot copy;
  if (mutex_ && xSemaphoreTake(mutex_, pdMS_TO_TICKS(20)) == pdTRUE) {
    copy = snapshot_;
    xSemaphoreGive(mutex_);
  }
  return copy;
}

uint32_t TelemetryManager::version() const {
  uint32_t version = 0;
  if (mutex_ && xSemaphoreTake(mutex_, pdMS_TO_TICKS(20)) == pdTRUE) {
    version = snapshot_.version;
    xSemaphoreGive(mutex_);
  }
  return version;
}

void TelemetryManager::publish(const Snapshot& next) {
  if (xSemaphoreTake(mutex_, portMAX_DELAY) == pdTRUE) {
    const uint32_t version = snapshot_.version + 1;
    snapshot_ = next;
    snapshot_.version = version;
    xSemaphoreGive(mutex_);
    Serial.printf("TELEMETRY snapshot_version=%lu endpoint=%s state=%s\n",
                  static_cast<unsigned long>(version), endpointName(next.activeEndpoint),
                  fetchStateName(next.fetchState));
  }
}

void TelemetryManager::requestRefresh() {
  portENTER_CRITICAL(&scheduleMux_);
  refreshRequested_ = true;
  portEXIT_CRITICAL(&scheduleMux_);
}

void TelemetryManager::toggleTemperatureUnit() {
  Snapshot next = snapshot();
  const auto selected = next.displayTemperatureUnit == appconfig::TemperatureUnit::Celsius
      ? appconfig::TemperatureUnit::Fahrenheit : appconfig::TemperatureUnit::Celsius;
  if (selected == next.displayTemperatureUnit) return;
  next.displayTemperatureUnit = selected;
  preferences_.putBool("temp_f", selected == appconfig::TemperatureUnit::Fahrenheit);
  publish(next);
}

uint32_t TelemetryManager::nextPollInSeconds(uint32_t nowMs) const {
  portENTER_CRITICAL(&scheduleMux_);
  const uint32_t nextPollMs = nextPollMs_;
  portEXIT_CRITICAL(&scheduleMux_);
  if (!nextPollMs || reachedDeadline(nowMs, nextPollMs)) return 0;
  return (nextPollMs - nowMs + 999) / 1000;
}

void TelemetryManager::taskEntry(void* context) {
  static_cast<TelemetryManager*>(context)->run();
}

void TelemetryManager::updateConnectionState(WifiState state, bool connected) {
  Snapshot next = snapshot();
  if (next.wifiState == state && next.rssiAvailable == connected) return;
  next.wifiState = state;
  next.rssiAvailable = connected;
  if (connected) next.rssi = WiFi.RSSI();
  if (!next.lastSuccessMs) next.fetchState = FetchState::Connecting;
  publish(next);
}

void TelemetryManager::run() {
  for (;;) {
    const uint32_t now = millis();
    const wl_status_t wifi = WiFi.status();
    if (wifi == WL_CONNECTED) {
      reconnectDelayMs_ = 2000;
      updateConnectionState(WifiState::Connected, true);
      portENTER_CRITICAL(&scheduleMux_);
      const bool refreshRequested = refreshRequested_;
      const uint32_t nextPollMs = nextPollMs_;
      if (refreshRequested) refreshRequested_ = false;
      portEXIT_CRITICAL(&scheduleMux_);
      if (refreshRequested || !nextPollMs || reachedDeadline(now, nextPollMs)) {
        performPollingCycle(now);
      } else {
        Snapshot current = snapshot();
        if (current.lastSuccessMs && current.fetchState != FetchState::Stale &&
            reachedDeadline(now, current.lastSuccessMs + appconfig::kStaleAfterMs)) {
          current.fetchState = FetchState::Stale;
          publish(current);
        }
      }
    } else {
      Snapshot current = snapshot();
      if ((current.wifiState == WifiState::Connecting || current.wifiState == WifiState::Reconnecting) &&
          reachedDeadline(now, wifiAttemptStartedMs_ + appconfig::kWifiConnectTimeoutMs)) {
        WiFi.disconnect();
        current.wifiState = WifiState::Failed;
        current.rssiAvailable = false;
        publish(current);
        nextWifiAttemptMs_ = now + reconnectDelayMs_;
        reconnectDelayMs_ = min<uint32_t>(reconnectDelayMs_ * 2, 60000);
      } else if (current.wifiState != WifiState::Connecting &&
                 (!nextWifiAttemptMs_ || reachedDeadline(now, nextWifiAttemptMs_))) {
        current.wifiState = current.lastSuccessMs ? WifiState::Reconnecting : WifiState::Connecting;
        current.rssiAvailable = false;
        if (!current.lastSuccessMs) current.fetchState = FetchState::Connecting;
        publish(current);
        WiFi.begin(appconfig::kWifiSsid, appconfig::kWifiPassword);
        wifiAttemptStartedMs_ = now;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

void TelemetryManager::performPollingCycle(uint32_t nowMs) {
  Snapshot before = snapshot();
  before.lastAttemptMs = nowMs;
  const bool primaryProbe = preferredEndpoint_ == Endpoint::EP2 &&
      (!nextPrimaryProbeMs_ || reachedDeadline(nowMs, nextPrimaryProbeMs_));
  Endpoint first = primaryProbe ? Endpoint::EP1 : preferredEndpoint_;
  const char* firstUrl = first == Endpoint::EP1 ? appconfig::kPrimaryTelemetryUrl
                                                : appconfig::kFallbackTelemetryUrl;
  AttemptResult result = requestEndpoint(first, firstUrl, before.displayTemperatureUnit);
  Endpoint active = first;
  if (result.kind == AttemptKind::TransportFailure && first == Endpoint::EP1) {
    result = requestEndpoint(Endpoint::EP2, appconfig::kFallbackTelemetryUrl,
                             before.displayTemperatureUnit);
    active = Endpoint::EP2;
  }

  Snapshot next = before;
  if (result.kind == AttemptKind::Valid) {
    const uint32_t retainedVersion = next.version;
    const WifiState wifiState = next.wifiState;
    const int16_t rssi = next.rssi;
    next = result.telemetry;
    next.version = retainedVersion;
    next.wifiState = wifiState;
    next.rssiAvailable = true;
    next.rssi = rssi;
    next.lastAttemptMs = nowMs;
    next.lastSuccessMs = nowMs;
    next.consecutiveFailedCycles = 0;
    next.activeEndpoint = active;
    next.diagnostic = Diagnostic::Online;
    next.httpStatus = result.httpStatus;
    const bool explicitOffline = next.explicitSystemStatus &&
        strcasecmp(next.systemStatus.value, "OFFLINE") == 0;
    next.fetchState = explicitOffline ? FetchState::Offline
        : (active == Endpoint::EP2 || next.optionalDataMissing ? FetchState::Degraded : FetchState::Online);
    if (active == Endpoint::EP2) {
      preferredEndpoint_ = Endpoint::EP2;
      nextPrimaryProbeMs_ = nowMs + appconfig::kPrimaryRetryIntervalMs;
    } else {
      preferredEndpoint_ = Endpoint::EP1;
      nextPrimaryProbeMs_ = 0;
    }
  } else {
    next.httpStatus = result.httpStatus;
    next.diagnostic = result.diagnostic;
    next.consecutiveFailedCycles = min<uint8_t>(next.consecutiveFailedCycles + 1, 255);
    if (result.diagnostic == Diagnostic::AuthError) next.fetchState = FetchState::AuthError;
    else if (next.consecutiveFailedCycles >= appconfig::kOfflineFailedCycles) next.fetchState = FetchState::Offline;
    else if (!next.lastSuccessMs) next.fetchState = FetchState::Connecting;
    if (primaryProbe && result.kind == AttemptKind::TransportFailure) {
      nextPrimaryProbeMs_ = nowMs + appconfig::kPrimaryRetryIntervalMs;
    }
  }
  publish(next);
  portENTER_CRITICAL(&scheduleMux_);
  nextPollMs_ = millis() + appconfig::kPollIntervalMs;
  portEXIT_CRITICAL(&scheduleMux_);
}

}  // namespace telemetry