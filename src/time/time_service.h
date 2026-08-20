#pragma once

#include <Preferences.h>
#include <stdint.h>
#include <time.h>

namespace device_time {

enum class SyncState : uint8_t { Unsynchronized, RtcHoldover, Syncing, NtpSynchronized };

struct Snapshot {
  bool valid = false;
  bool use24Hour = true;
  uint8_t hour = 0;
  uint8_t minute = 0;
  uint8_t day = 0;
  uint8_t month = 0;
  uint16_t year = 0;
  uint8_t timezoneIndex = 1;
  SyncState syncState = SyncState::Unsynchronized;
  time_t lastSuccessfulSync = 0;
  uint32_t version = 1;
};

class TimeService {
 public:
  bool begin(bool rtcObserved);
  void poll(uint32_t nowMs, bool wifiConnected);
  void cycleTimezone();
  bool setTimezone(uint8_t index);
  void toggleHourFormat();
  void requestSync();
  Snapshot snapshot() const { return snapshot_; }
  uint32_t version() const { return snapshot_.version; }

 private:
  void applyTimezone();
  void updateVisibleTime();
  bool loadRtcUtc(time_t& value) const;
  bool writeRtcUtc(time_t value) const;
  void publishSettingsChange();

  Preferences preferences_;
  Snapshot snapshot_{};
  bool rtcObserved_ = false;
  bool ntpStarted_ = false;
  bool forceSync_ = false;
  bool preferencesDirty_ = false;
  uint32_t preferencesDueMs_ = 0;
  uint32_t nextCheckMs_ = 0;
};

const char* syncStateName(SyncState value);
const char* timezoneLabel(uint8_t index);
const char* timezoneRule(uint8_t index);
const char* timezoneDescription(uint8_t index);
uint8_t timezoneCount();

}  // namespace device_time