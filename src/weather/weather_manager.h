#pragma once

#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
#include <freertos/semphr.h>
#include <stdint.h>

namespace weather {

enum class State : uint8_t {
  SetupRequired, Searching, WaitingForWifi, Online, Stale, Offline, GpsFixRequired, Disabled
};
enum class LocationSource : uint8_t { Unconfigured, Gps, City, Postal, Manual, Disabled };
enum class TemperatureUnit : uint8_t { Celsius, Fahrenheit };
enum class SearchState : uint8_t { Idle, Pending, Complete, Offline, Invalid };

struct SearchResult {
  char label[48]{};
  char city[32]{};
  char region[32]{};
  char country[24]{};
  char postal[12]{};
  double latitude = 0.0;
  double longitude = 0.0;
};

struct Snapshot {
  State state = State::SetupRequired;
  LocationSource source = LocationSource::Unconfigured;
  bool configured = false;
  bool dataAvailable = false;
  bool feelsLikeAvailable = false;
  bool highLowAvailable = false;
  bool humidityAvailable = false;
  bool windAvailable = false;
  bool precipitationAvailable = false;
  int16_t temperatureTenths = 0;
  int16_t feelsLikeTenths = 0;
  int16_t highTenths = 0;
  int16_t lowTenths = 0;
  uint8_t humidityPercent = 0;
  uint16_t windSpeedTenths = 0;
  uint16_t windDirectionDegrees = 0;
  uint8_t precipitationPercent = 0;
  uint8_t weatherCode = 255;
  TemperatureUnit temperatureUnit = TemperatureUnit::Celsius;
  bool showTemperature = true;
  bool showCondition = true;
  bool showCity = true;
  bool showFeelsLike = false;
  char city[32]{};
  char region[32]{};
  char country[24]{};
  char postal[12]{};
  uint32_t lastSuccessMs = 0;
  SearchState searchState = SearchState::Idle;
  uint8_t resultCount = 0;
  SearchResult results[5]{};
  uint32_t version = 1;
};

class WeatherManager {
 public:
  bool begin();
  Snapshot snapshot() const;
  uint32_t version() const;
  void setGpsPosition(bool valid, double latitude, double longitude);
  bool requestSearch(const char* query, bool postal, const char* countryCode);
  bool saveSearchResult(uint8_t index, LocationSource source);
  bool saveManual(double latitude, double longitude);
  bool saveGps();
  void disable();
  void toggleTemperatureUnit();
  void toggleHomeOption(uint8_t option);
  bool requestRefresh(uint32_t nowMs);
  void setSuspended(bool suspended);
  bool suspended() const;
  bool idle() const;
  uint32_t heartbeat() const;

 private:
  static void taskEntry(void* context);
  void run();
  void poll(uint32_t nowMs);
  void geocode();
  bool resolveIdentity(Snapshot& next, double latitude, double longitude);
  void persistIdentity(const Snapshot& next);
  void persistResolvedIdentity(const Snapshot& next);
  void publish(const Snapshot& next);
  bool persist(LocationSource source, double latitude, double longitude,
               const char* city, const char* region, const char* country,
               const char* postal);
  mutable SemaphoreHandle_t mutex_ = nullptr;
  mutable portMUX_TYPE controlMux_ = portMUX_INITIALIZER_UNLOCKED;
  Preferences preferences_;
  Snapshot snapshot_{};
  double savedLatitude_ = 0.0;
  double savedLongitude_ = 0.0;
  bool gpsValid_ = false;
  double gpsLatitude_ = 0.0;
  double gpsLongitude_ = 0.0;
  bool requestedSearchPostal_ = false;
  char requestedSearch_[48]{};
  char requestedCountry_[3]{'U', 'S', 0};
  bool forceRefresh_ = false;
  bool displayPreferencesDirty_ = false;
  uint32_t displayPreferencesDueMs_ = 0;
  uint32_t nextPollMs_ = 0;
  uint32_t lastManualRefreshMs_ = 0;
  double lastRequestLatitude_ = 0.0;
  double lastRequestLongitude_ = 0.0;
  bool lastRequestPositionValid_ = false;
  bool suspended_ = false;
  bool inFlight_ = false;
  uint32_t heartbeat_ = 0;
};

const char* stateName(State value);
const char* sourceName(LocationSource value);
const char* conditionName(uint8_t weatherCode);
bool validCoordinates(double latitude, double longitude);
bool validPostal(const char* value, const char* countryCode);
double distanceKm(double lat1, double lon1, double lat2, double lon2);

}  // namespace weather