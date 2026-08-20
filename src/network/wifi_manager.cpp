#include "wifi_manager.h"

#include <Arduino.h>

#include "app_config.h"

namespace network {
namespace {
constexpr uint32_t kRetryMs = 15000;
bool reached(uint32_t now, uint32_t deadline) {
  return static_cast<int32_t>(now - deadline) >= 0;
}
}

const char* stateName(State state) {
  switch (state) {
    case State::SetupRequired: return "SETUP REQUIRED";
    case State::Disconnected: return "DISCONNECTED";
    case State::Connecting: return "CONNECTING";
    case State::Connected: return "CONNECTED";
  }
  return "DISCONNECTED";
}

bool WifiManager::begin() {
  mutex_ = xSemaphoreCreateMutex();
  if (!mutex_) return false;
  preferences_.begin("sf_wifi", false);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);
  bool userConfigured = false;
  char ssid[kMaximumSsidBytes + 1]{}, password[kMaximumPasswordBytes + 1]{};
  if (loadCredential(ssid, sizeof(ssid), password, sizeof(password), userConfigured)) {
    snapshot_.userConfigured = userConfigured;
    snapshot_.ssidAvailable = true;
    strlcpy(snapshot_.ssid, ssid, sizeof(snapshot_.ssid));
    strlcpy(entrySsid_, ssid, sizeof(entrySsid_));
    strlcpy(entryPassword_, password, sizeof(entryPassword_));
    connectionRequested_ = true;
    snapshot_.state = State::Connecting;
  } else {
    snapshot_.state = State::SetupRequired;
  }
  snapshot_.version = 1;
  memset(password, 0, sizeof(password));
  return true;
}

bool WifiManager::loadCredential(char* ssid, size_t ssidSize, char* password,
                                 size_t passwordSize, bool& userConfigured) {
  userConfigured = preferences_.getBool("user", false);
  if (userConfigured) {
    const String savedSsid = preferences_.getString("ssid", "");
    const String savedPassword = preferences_.getString("pass", "");
    if (savedSsid.length() && savedSsid.length() <= kMaximumSsidBytes &&
        savedPassword.length() <= kMaximumPasswordBytes) {
      strlcpy(ssid, savedSsid.c_str(), ssidSize);
      strlcpy(password, savedPassword.c_str(), passwordSize);
      return true;
    }
  }
  // Wi-Fi is always provisioned locally and persisted in NVS. Compile-time
  // credentials are intentionally not a runtime fallback: a public firmware
  // build must begin in a truthful SETUP REQUIRED state and FORGET must forget.
  return false;
}

void WifiManager::publish(State state, bool connected) {
  if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(20)) != pdTRUE) return;
  bool changed = snapshot_.state != state || snapshot_.rssiAvailable != connected;
  snapshot_.state = state;
  snapshot_.rssiAvailable = connected;
  if (connected) {
    const int16_t rssi = WiFi.RSSI();
    changed = changed || snapshot_.rssi != rssi;
    snapshot_.rssi = rssi;
  }
  if (changed) ++snapshot_.version;
  xSemaphoreGive(mutex_);
}

void WifiManager::beginConnection(uint32_t nowMs) {
  if (!entrySsid_[0]) return;
  WiFi.mode(WIFI_STA);
  WiFi.begin(entrySsid_, entryPassword_);
  attemptStartedMs_ = nowMs;
  connectionRequested_ = false;
  publish(State::Connecting, false);
}

void WifiManager::poll(uint32_t nowMs, bool servicesAllowed) {
  if (!mutex_) return;
  setServicesAllowed(servicesAllowed);
  const int scan = WiFi.scanComplete();
  if (scan >= 0) collectScanResults();
  else if (scan == WIFI_SCAN_FAILED) {
    bool scanWasActive = false;
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(20)) == pdTRUE) {
      if (snapshot_.scanState == ScanState::Scanning) {
        scanWasActive = true;
        snapshot_.scanState = ScanState::Failed;
        ++snapshot_.version;
      }
      xSemaphoreGive(mutex_);
    }
    if (scanWasActive) {
      if (reconnectAfterScan_ && entrySsid_[0]) connectionRequested_ = true;
      reconnectAfterScan_ = false;
    }
  }
  if (!servicesAllowed_) return;
  const Snapshot current = snapshot();
  // ESP32 station connection and active scanning share one radio. Never call
  // WiFi.begin/disconnect retry logic while an asynchronous scan owns it.
  if (current.scanState == ScanState::Scanning) return;
  if (WiFi.status() == WL_CONNECTED) {
    publish(State::Connected, true);
    return;
  }
  if (connectionRequested_) beginConnection(nowMs);
  const Snapshot afterConnection = snapshot();
  if (afterConnection.state == State::Connecting && attemptStartedMs_ &&
      reached(nowMs, attemptStartedMs_ + appconfig::kWifiConnectTimeoutMs)) {
    WiFi.disconnect(false, false);
    publish(State::Disconnected, false);
    nextAttemptMs_ = nowMs + kRetryMs;
  } else if (afterConnection.state == State::Disconnected && entrySsid_[0] &&
             nextAttemptMs_ && reached(nowMs, nextAttemptMs_)) {
    connectionRequested_ = true;
    nextAttemptMs_ = 0;
  }
}

Snapshot WifiManager::snapshot() const {
  Snapshot copy;
  if (mutex_ && xSemaphoreTake(mutex_, pdMS_TO_TICKS(20)) == pdTRUE) {
    copy = snapshot_;
    xSemaphoreGive(mutex_);
  }
  return copy;
}

uint32_t WifiManager::version() const { return snapshot().version; }

bool WifiManager::startScan() {
  if (!mutex_ || !servicesAllowed_) return false;
  Snapshot current = snapshot();
  if (current.scanState == ScanState::Scanning) return false;
  reconnectAfterScan_ = entrySsid_[0] != 0;
  connectionRequested_ = false;
  nextAttemptMs_ = 0;
  attemptStartedMs_ = 0;
  // Stop association without erasing NVS or the manager's credential buffer so
  // the station radio is fully available to perform a real active scan.
  WiFi.disconnect(false, false);
  WiFi.mode(WIFI_STA);
  WiFi.scanDelete();
  const int result = WiFi.scanNetworks(true, true, false, 500);
  if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(20)) == pdTRUE) {
    snapshot_.scanState = result == WIFI_SCAN_FAILED ? ScanState::Failed : ScanState::Scanning;
    snapshot_.resultCount = 0;
    ++snapshot_.version;
    xSemaphoreGive(mutex_);
  }
  if (result == WIFI_SCAN_FAILED) {
    if (reconnectAfterScan_ && entrySsid_[0]) connectionRequested_ = true;
    reconnectAfterScan_ = false;
    return false;
  }
  return true;
}

void WifiManager::collectScanResults() {
  const int count = WiFi.scanComplete();
  if (count < 0) return;
  if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(20)) == pdTRUE) {
    snapshot_.resultCount = 0;
    for (int i = 0; i < count && snapshot_.resultCount < kMaximumScanResults; ++i) {
      const String ssid = WiFi.SSID(i);
      if (!ssid.length() || ssid.length() > kMaximumSsidBytes) continue;
      ScanResult& result = snapshot_.results[snapshot_.resultCount++];
      strlcpy(result.ssid, ssid.c_str(), sizeof(result.ssid));
      result.rssi = WiFi.RSSI(i);
      result.secure = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
    }
    snapshot_.scanState = snapshot_.resultCount ? ScanState::Complete : ScanState::Empty;
    ++snapshot_.version;
    xSemaphoreGive(mutex_);
  }
  WiFi.scanDelete();
  if (reconnectAfterScan_ && entrySsid_[0]) connectionRequested_ = true;
  reconnectAfterScan_ = false;
}

bool WifiManager::selectNetwork(uint8_t index) {
  const Snapshot current = snapshot();
  if (index >= current.resultCount) return false;
  clearEntry();
  strlcpy(entrySsid_, current.results[index].ssid, sizeof(entrySsid_));
  return true;
}

void WifiManager::beginManualNetwork() { clearEntry(); }
void WifiManager::appendSsid(char value) {
  const size_t length = strnlen(entrySsid_, sizeof(entrySsid_));
  if (length < kMaximumSsidBytes && value >= 32 && value <= 126) {
    entrySsid_[length] = value; entrySsid_[length + 1] = 0;
  }
}
void WifiManager::backspaceSsid() {
  const size_t length = strnlen(entrySsid_, sizeof(entrySsid_));
  if (length) entrySsid_[length - 1] = 0;
}
void WifiManager::appendPassword(char value) {
  const size_t length = strnlen(entryPassword_, sizeof(entryPassword_));
  if (length < kMaximumPasswordBytes && value >= 32 && value <= 126) {
    entryPassword_[length] = value; entryPassword_[length + 1] = 0;
  }
}
void WifiManager::backspacePassword() {
  const size_t length = strnlen(entryPassword_, sizeof(entryPassword_));
  if (length) entryPassword_[length - 1] = 0;
}
void WifiManager::clearEntry() {
  memset(entrySsid_, 0, sizeof(entrySsid_));
  memset(entryPassword_, 0, sizeof(entryPassword_));
}

bool WifiManager::saveAndConnect() {
  const size_t ssidLength = strnlen(entrySsid_, sizeof(entrySsid_));
  const size_t passwordLength = strnlen(entryPassword_, sizeof(entryPassword_));
  if (!ssidLength || ssidLength > kMaximumSsidBytes || passwordLength > kMaximumPasswordBytes) return false;
  if (preferences_.putString("ssid", entrySsid_) != ssidLength ||
      preferences_.putString("pass", entryPassword_) != passwordLength ||
      !preferences_.putBool("user", true)) return false;
  if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(20)) == pdTRUE) {
    snapshot_.userConfigured = true;
    snapshot_.ssidAvailable = true;
    strlcpy(snapshot_.ssid, entrySsid_, sizeof(snapshot_.ssid));
    snapshot_.state = State::Connecting;
    ++snapshot_.version;
    xSemaphoreGive(mutex_);
  }
  WiFi.disconnect(false, false);
  connectionRequested_ = true;
  return true;
}

void WifiManager::reconnect() { if (entrySsid_[0]) connectionRequested_ = true; }
void WifiManager::disconnect() {
  WiFi.disconnect(false, false);
  nextAttemptMs_ = 0;
  publish(entrySsid_[0] ? State::Disconnected : State::SetupRequired, false);
}

void WifiManager::setServicesAllowed(bool allowed) {
  if (!mutex_) return;
  if (servicesAllowed_ == allowed) return;
  servicesAllowed_ = allowed;
  if (!allowed) {
    WiFi.scanDelete();
    reconnectAfterScan_ = false;
    connectionRequested_ = false;
    attemptStartedMs_ = 0;
    nextAttemptMs_ = 0;
    if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(20)) == pdTRUE) {
      const bool scanChanged = snapshot_.scanState != ScanState::Idle || snapshot_.resultCount != 0;
      snapshot_.scanState = ScanState::Idle;
      snapshot_.resultCount = 0;
      if (scanChanged) ++snapshot_.version;
      xSemaphoreGive(mutex_);
    }
    WiFi.disconnect(true, false);
    WiFi.mode(WIFI_OFF);
    publish(entrySsid_[0] ? State::Disconnected : State::SetupRequired, false);
    return;
  }
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(false);
  if (entrySsid_[0]) connectionRequested_ = true;
}

bool WifiManager::forgetUserCredentials() {
  if (!snapshot().userConfigured) return false;
  preferences_.remove("ssid");
  preferences_.remove("pass");
  preferences_.putBool("user", false);
  WiFi.disconnect(false, false);
  clearEntry();
  if (xSemaphoreTake(mutex_, pdMS_TO_TICKS(20)) == pdTRUE) {
    snapshot_.userConfigured = false;
    snapshot_.ssidAvailable = false;
    snapshot_.ssid[0] = 0;
    snapshot_.state = State::SetupRequired;
    ++snapshot_.version;
    xSemaphoreGive(mutex_);
  }
  return true;
}

}  // namespace network