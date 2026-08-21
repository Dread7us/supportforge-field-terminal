#pragma once

#include <Preferences.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <string.h>

namespace network {

constexpr uint8_t kMaximumScanResults = 6;
constexpr size_t kMaximumSsidBytes = 32;
constexpr size_t kMaximumPasswordBytes = 63;

enum class State : uint8_t { SetupRequired, Disconnected, Connecting, Connected };
enum class ScanState : uint8_t { Idle, Scanning, Complete, Empty, Failed };

struct ScanResult {
  char ssid[kMaximumSsidBytes + 1]{};
  int16_t rssi = 0;
  bool secure = false;
};

struct Snapshot {
  State state = State::SetupRequired;
  ScanState scanState = ScanState::Idle;
  bool userConfigured = false;
  bool ssidAvailable = false;
  char ssid[kMaximumSsidBytes + 1]{};
  bool rssiAvailable = false;
  int16_t rssi = 0;
  ScanResult results[kMaximumScanResults]{};
  uint8_t resultCount = 0;
  uint32_t version = 0;
};

const char* stateName(State state);

class WifiManager {
 public:
  bool begin();
  void poll(uint32_t nowMs, bool servicesAllowed = true);
  Snapshot snapshot() const;
  uint32_t version() const;
  bool startScan();
  bool selectNetwork(uint8_t index);
  void beginManualNetwork();
  void appendSsid(char value);
  void backspaceSsid();
  void appendPassword(char value);
  void backspacePassword();
  void clearEntry();
  bool saveAndConnect();
  void reconnect();
  void disconnect();
  void setServicesAllowed(bool allowed);
  bool forgetUserCredentials();
  const char* entrySsid() const { return entrySsid_; }
  size_t passwordLength() const { return strnlen(entryPassword_, sizeof(entryPassword_)); }

 private:
  void publish(State state, bool connected);
  void beginConnection(uint32_t nowMs);
  void collectScanResults();
  bool loadCredential(char* ssid, size_t ssidSize, char* password, size_t passwordSize,
                      bool& userConfigured);

  mutable SemaphoreHandle_t mutex_ = nullptr;
  Preferences preferences_;
  Snapshot snapshot_{};
  char entrySsid_[kMaximumSsidBytes + 1]{};
  char entryPassword_[kMaximumPasswordBytes + 1]{};
  uint32_t attemptStartedMs_ = 0;
  uint32_t nextAttemptMs_ = 0;
  bool connectionRequested_ = false;
  bool reconnectAfterScan_ = false;
  bool servicesAllowed_ = true;
  uint8_t publishedSignalBucket_ = 0;
};

}  // namespace network