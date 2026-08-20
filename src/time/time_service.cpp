#include "time_service.h"

#include <Arduino.h>
#include <Wire.h>
#include <esp_sntp.h>
#include <sys/time.h>

namespace device_time {
namespace {
constexpr uint8_t kRtcAddress = 0x51;
constexpr time_t kPlausibleEpoch = 1704067200;  // 2024-01-01 UTC, validity only.
constexpr uint32_t kCheckIntervalMs = 1000;
constexpr uint32_t kPreferenceWriteDelayMs = 750;
constexpr const char* kTimezoneLabels[] = {"UTC", "PACIFIC", "MOUNTAIN", "CENTRAL", "EASTERN"};
constexpr const char* kTimezoneRules[] = {
    "UTC0", "PST8PDT,M3.2.0,M11.1.0", "MST7MDT,M3.2.0,M11.1.0",
    "CST6CDT,M3.2.0,M11.1.0", "EST5EDT,M3.2.0,M11.1.0"};
constexpr uint8_t kTimezoneCount = sizeof(kTimezoneLabels) / sizeof(kTimezoneLabels[0]);
constexpr const char* kTimezoneDescriptions[] = {
    "COORDINATED UNIVERSAL TIME", "PACIFIC TIME - DST AUTOMATIC",
    "MOUNTAIN TIME - DST AUTOMATIC", "CENTRAL TIME - DST AUTOMATIC",
    "EASTERN TIME - DST AUTOMATIC"};

uint8_t fromBcd(uint8_t value) { return (value >> 4) * 10 + (value & 0x0F); }
uint8_t toBcd(uint8_t value) { return static_cast<uint8_t>((value / 10) << 4 | (value % 10)); }

bool reached(uint32_t now, uint32_t deadline) {
  return static_cast<int32_t>(now - deadline) >= 0;
}
}

const char* syncStateName(SyncState value) {
  switch (value) {
    case SyncState::Unsynchronized: return "TIME SYNC";
    case SyncState::RtcHoldover: return "RTC HOLDOVER";
    case SyncState::Syncing: return "SYNCING";
    case SyncState::NtpSynchronized: return "NTP SYNCED";
  }
  return "TIME SYNC";
}

const char* timezoneLabel(uint8_t index) {
  return kTimezoneLabels[index < kTimezoneCount ? index : 0];
}

const char* timezoneRule(uint8_t index) {
  return kTimezoneRules[index < kTimezoneCount ? index : 0];
}

const char* timezoneDescription(uint8_t index) {
  return kTimezoneDescriptions[index < kTimezoneCount ? index : 0];
}

uint8_t timezoneCount() { return kTimezoneCount; }

bool TimeService::begin(bool rtcObserved) {
  rtcObserved_ = rtcObserved;
  if (preferences_.begin("sf_time", false)) {
    // Fresh devices begin in Pacific time. The POSIX rule supplies automatic
    // PST/PDT transitions; the system clock itself remains canonical UTC epoch.
    snapshot_.timezoneIndex = min<uint8_t>(preferences_.getUChar("tz", 1), kTimezoneCount - 1);
    snapshot_.use24Hour = preferences_.getBool("hour24", true);
    snapshot_.lastSuccessfulSync = static_cast<time_t>(preferences_.getULong64("last_sync", 0));
  }
  applyTimezone();
  time_t rtcUtc = 0;
  if (loadRtcUtc(rtcUtc)) {
    timeval tv{rtcUtc, 0};
    settimeofday(&tv, nullptr);
    snapshot_.syncState = SyncState::RtcHoldover;
  }
  updateVisibleTime();
  return true;
}

void TimeService::applyTimezone() {
  setenv("TZ", timezoneRule(snapshot_.timezoneIndex), 1);
  tzset();
}

void TimeService::poll(uint32_t nowMs, bool wifiConnected) {
  if (preferencesDirty_ && reached(nowMs, preferencesDueMs_)) {
    preferences_.putBool("hour24", snapshot_.use24Hour);
    preferencesDirty_ = false;
  }
  if (!reached(nowMs, nextCheckMs_)) return;
  nextCheckMs_ = nowMs + kCheckIntervalMs;
  if (wifiConnected && (!ntpStarted_ || forceSync_)) {
    // The fixed-offset SNTP API silently resets the process timezone to UTC on ESP32.
    // Keep SNTP and localtime_r on the same persisted, DST-aware timezone rule.
    configTzTime(timezoneRule(snapshot_.timezoneIndex), "pool.ntp.org", "time.nist.gov");
    ntpStarted_ = true;
    forceSync_ = false;
    if (snapshot_.syncState != SyncState::RtcHoldover &&
        snapshot_.syncState != SyncState::Syncing) {
      snapshot_.syncState = SyncState::Syncing;
      ++snapshot_.version;
    }
  }
  if (ntpStarted_ && sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
    const bool newlySynchronized = snapshot_.syncState != SyncState::NtpSynchronized;
    snapshot_.syncState = SyncState::NtpSynchronized;
    if (newlySynchronized) {
      snapshot_.lastSuccessfulSync = time(nullptr);
      preferences_.putULong64("last_sync", static_cast<uint64_t>(snapshot_.lastSuccessfulSync));
      writeRtcUtc(snapshot_.lastSuccessfulSync);
      ++snapshot_.version;
    }
  }
  updateVisibleTime();
}

void TimeService::cycleTimezone() {
  setTimezone((snapshot_.timezoneIndex + 1) % kTimezoneCount);
}

bool TimeService::setTimezone(uint8_t index) {
  if (index >= kTimezoneCount || index == snapshot_.timezoneIndex) return false;
  snapshot_.timezoneIndex = index;
  applyTimezone();
  // A timezone selection is operational state, not disposable UI state. Commit
  // it before publishing so an immediate reboot retains the selected zone.
  preferences_.putUChar("tz", snapshot_.timezoneIndex);
  publishSettingsChange();
  return true;
}

void TimeService::toggleHourFormat() {
  snapshot_.use24Hour = !snapshot_.use24Hour;
  preferencesDirty_ = true;
  preferencesDueMs_ = millis() + kPreferenceWriteDelayMs;
  publishSettingsChange();
}

void TimeService::requestSync() {
  forceSync_ = true;
  snapshot_.syncState = SyncState::Syncing;
  ++snapshot_.version;
}

void TimeService::publishSettingsChange() {
  updateVisibleTime();
  ++snapshot_.version;
}

void TimeService::updateVisibleTime() {
  const time_t now = time(nullptr);
  tm local{};
  const bool valid = now >= kPlausibleEpoch && localtime_r(&now, &local) != nullptr;
  const uint8_t hour = valid ? static_cast<uint8_t>(local.tm_hour) : 0;
  const uint8_t minute = valid ? static_cast<uint8_t>(local.tm_min) : 0;
  const uint8_t day = valid ? static_cast<uint8_t>(local.tm_mday) : 0;
  const uint8_t month = valid ? static_cast<uint8_t>(local.tm_mon + 1) : 0;
  const uint16_t year = valid ? static_cast<uint16_t>(local.tm_year + 1900) : 0;
  if (snapshot_.valid != valid || snapshot_.hour != hour || snapshot_.minute != minute ||
      snapshot_.day != day || snapshot_.month != month || snapshot_.year != year) {
    snapshot_.valid = valid;
    snapshot_.hour = hour;
    snapshot_.minute = minute;
    snapshot_.day = day;
    snapshot_.month = month;
    snapshot_.year = year;
    ++snapshot_.version;
  }
}

bool TimeService::loadRtcUtc(time_t& value) const {
  if (!rtcObserved_) return false;
  Wire.beginTransmission(kRtcAddress);
  Wire.write(0x02);
  if (Wire.endTransmission(false) != 0 || Wire.requestFrom(kRtcAddress, static_cast<uint8_t>(7)) != 7) return false;
  const uint8_t second = Wire.read();
  const uint8_t minute = Wire.read();
  const uint8_t hour = Wire.read();
  const uint8_t day = Wire.read();
  Wire.read();
  const uint8_t month = Wire.read();
  const uint8_t year = Wire.read();
  if (second & 0x80) return false;
  tm utc{};
  utc.tm_sec = fromBcd(second & 0x7F);
  utc.tm_min = fromBcd(minute & 0x7F);
  utc.tm_hour = fromBcd(hour & 0x3F);
  utc.tm_mday = fromBcd(day & 0x3F);
  utc.tm_mon = fromBcd(month & 0x1F) - 1;
  utc.tm_year = 100 + fromBcd(year);
  setenv("TZ", "UTC0", 1);
  tzset();
  value = mktime(&utc);
  const_cast<TimeService*>(this)->applyTimezone();
  return value >= kPlausibleEpoch;
}

bool TimeService::writeRtcUtc(time_t value) const {
  if (!rtcObserved_ || value < kPlausibleEpoch) return false;
  tm utc{};
  if (!gmtime_r(&value, &utc)) return false;
  Wire.beginTransmission(kRtcAddress);
  Wire.write(0x02);
  Wire.write(toBcd(utc.tm_sec));
  Wire.write(toBcd(utc.tm_min));
  Wire.write(toBcd(utc.tm_hour));
  Wire.write(toBcd(utc.tm_mday));
  Wire.write(toBcd(utc.tm_wday));
  Wire.write(toBcd(utc.tm_mon + 1));
  Wire.write(toBcd((utc.tm_year + 1900) % 100));
  return Wire.endTransmission() == 0;
}

}  // namespace device_time