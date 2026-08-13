#include "weather_manager.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "app_config.h"

namespace weather { namespace {
bool reached(uint32_t n,uint32_t d){return static_cast<int32_t>(n-d)>=0;}
bool changed(const Snapshot&a,const Snapshot&b){return a.state!=b.state||a.source!=b.source||a.configured!=b.configured||a.dataAvailable!=b.dataAvailable||a.temperatureTenths!=b.temperatureTenths||a.feelsLikeTenths!=b.feelsLikeTenths||a.weatherCode!=b.weatherCode||a.temperatureUnit!=b.temperatureUnit||a.showTemperature!=b.showTemperature||a.showCondition!=b.showCondition||a.showCity!=b.showCity||a.showFeelsLike!=b.showFeelsLike||strcmp(a.city,b.city)||a.searchState!=b.searchState||a.resultCount!=b.resultCount;}
String encode(const char*v){String out;for(const unsigned char*p=(const unsigned char*)v;*p;++p){if(isalnum(*p)||*p=='-'||*p=='_'||*p=='.')out+=(char)*p;else if(*p==' ')out+="%20";}return out;}
bool parseCoord(const char*v,double&r){if(!v||!v[0])return false;char*e=nullptr;r=strtod(v,&e);return e&&!*e&&isfinite(r);}
}}
namespace weather {
const char* stateName(State v){switch(v){case State::SetupRequired:return"WX SETUP";case State::Searching:return"WX SEARCHING";case State::WaitingForWifi:return"WX OFFLINE";case State::Online:return"WX ONLINE";case State::Stale:return"WX STALE";case State::Offline:return"WX OFFLINE";case State::GpsFixRequired:return"WX GPS FIX REQUIRED";case State::Disabled:return"WX DISABLED";}return"WX OFFLINE";}
const char* sourceName(LocationSource v){switch(v){case LocationSource::Gps:return"GPS";case LocationSource::City:return"CITY";case LocationSource::Postal:return"POSTAL";case LocationSource::Manual:return"MANUAL";case LocationSource::Disabled:return"DISABLED";default:return"UNCONFIGURED";}}
const char* conditionName(uint8_t c){if(c==0)return"CLEAR";if(c<=3)return"CLOUDY";if(c==45||c==48)return"FOG";if(c>=51&&c<=67)return"RAIN";if(c>=71&&c<=77)return"SNOW";if(c>=80&&c<=82)return"SHOWERS";if(c>=95&&c<=99)return"STORM";return"--";}
bool validCoordinates(double a,double o){return isfinite(a)&&isfinite(o)&&a>=-90&&a<=90&&o>=-180&&o<=180;}
bool validPostal(const char*v,const char*c){if(!v||!c)return false;size_t n=strlen(v);if(!strcmp(c,"US")){if(n!=5)return false;for(size_t i=0;i<n;++i)if(!isdigit((unsigned char)v[i]))return false;return true;}if(strcmp(c,"CA")&&strcmp(c,"GB")&&strcmp(c,"AU"))return false;if(n<3||n>8)return false;for(size_t i=0;i<n;++i)if(!isalnum((unsigned char)v[i])&&v[i]!=' ')return false;return true;}
double distanceKm(double a,double o,double b,double p){constexpr double R=6371.0,D=.017453292519943295;double x=(b-a)*D,y=(p-o)*D,q=sin(x/2)*sin(x/2)+cos(a*D)*cos(b*D)*sin(y/2)*sin(y/2);return R*2*atan2(sqrt(q),sqrt(1-q));}

bool WeatherManager::begin(){
  mutex_=xSemaphoreCreateMutex();
  if(!mutex_||!preferences_.begin("sf_weather",false))return false;
  bool init=preferences_.getBool("initialized",false);
  snapshot_.temperatureUnit=preferences_.getBool("fahrenheit",false)?TemperatureUnit::Fahrenheit:TemperatureUnit::Celsius;
  snapshot_.showTemperature=preferences_.getBool("show_temp",true);
  snapshot_.showCondition=preferences_.getBool("show_cond",true);
  snapshot_.showCity=preferences_.getBool("show_city",true);
  snapshot_.showFeelsLike=preferences_.getBool("show_feels",false);
  if(init){
    snapshot_.source=(LocationSource)preferences_.getUChar("source",0);
    savedLatitude_=preferences_.getDouble("latitude",0);
    savedLongitude_=preferences_.getDouble("longitude",0);
    strlcpy(snapshot_.city,preferences_.getString("label","").c_str(),sizeof(snapshot_.city));
  }else if(appconfig::weatherConfigured()&&parseCoord(appconfig::kWeatherLatitude,savedLatitude_)&&
           parseCoord(appconfig::kWeatherLongitude,savedLongitude_)&&validCoordinates(savedLatitude_,savedLongitude_)){
    snapshot_.source=LocationSource::Manual;
    strlcpy(snapshot_.city,appconfig::kWeatherCityLabel,sizeof(snapshot_.city));
  }
  snapshot_.configured=snapshot_.source!=LocationSource::Unconfigured&&snapshot_.source!=LocationSource::Disabled;
  snapshot_.state=snapshot_.source==LocationSource::Disabled?State::Disabled:
      (snapshot_.configured?(snapshot_.source==LocationSource::Gps?State::GpsFixRequired:State::WaitingForWifi):State::SetupRequired);
  Serial.printf("WEATHER location_source=%s\n",sourceName(snapshot_.source));
  Serial.printf("WEATHER configured=%s\n",snapshot_.configured?"YES":"NO");
  return xTaskCreatePinnedToCore(taskEntry,"weather",appconfig::kWeatherTaskStackBytes,this,1,nullptr,0)==pdPASS;
}
Snapshot WeatherManager::snapshot()const{Snapshot c;if(mutex_&&xSemaphoreTake(mutex_,pdMS_TO_TICKS(20))==pdTRUE){c=snapshot_;xSemaphoreGive(mutex_);}return c;}uint32_t WeatherManager::version()const{return snapshot().version;}void WeatherManager::taskEntry(void*c){((WeatherManager*)c)->run();}
void WeatherManager::run(){for(;;){Snapshot s=snapshot();if(s.searchState==SearchState::Pending)geocode();uint32_t n=millis();if(reached(n,nextPollMs_)||forceRefresh_)poll(n);vTaskDelay(pdMS_TO_TICKS(250));}}
void WeatherManager::setGpsPosition(bool v,double a,double o){if(!mutex_||xSemaphoreTake(mutex_,pdMS_TO_TICKS(20))!=pdTRUE)return;gpsValid_=v&&validCoordinates(a,o);if(gpsValid_){gpsLatitude_=a;gpsLongitude_=o;}xSemaphoreGive(mutex_);}
bool WeatherManager::requestSearch(const char*q,bool postal,const char*c){if(!q||!q[0]||strlen(q)>=sizeof(requestedSearch_)||(postal&&!validPostal(q,c)))return false;if(!mutex_||xSemaphoreTake(mutex_,pdMS_TO_TICKS(20))!=pdTRUE)return false;strlcpy(requestedSearch_,q,sizeof(requestedSearch_));strlcpy(requestedCountry_,c?c:"US",sizeof(requestedCountry_));requestedSearchPostal_=postal;snapshot_.searchState=SearchState::Pending;snapshot_.state=State::Searching;snapshot_.resultCount=0;++snapshot_.version;xSemaphoreGive(mutex_);return true;}
void WeatherManager::geocode(){Snapshot n=snapshot();if(WiFi.status()!=WL_CONNECTED){n.searchState=SearchState::Offline;n.state=State::Offline;publish(n);return;}char q[48]{},c[3]{};if(xSemaphoreTake(mutex_,portMAX_DELAY)==pdTRUE){strlcpy(q,requestedSearch_,sizeof(q));strlcpy(c,requestedCountry_,sizeof(c));requestedSearch_[0]=0;xSemaphoreGive(mutex_);}String url=String("https://geocoding-api.open-meteo.com/v1/search?count=5&language=en&format=json&name=")+encode(q);if(c[0])url+="&countryCode="+String(c);WiFiClientSecure client;client.setInsecure();HTTPClient http;http.setConnectTimeout(appconfig::kHttpConnectTimeoutMs);http.setTimeout(appconfig::kHttpReadTimeoutMs);n.resultCount=0;bool ok=false;if(http.begin(client,url)){int status=http.GET();if(status>=200&&status<300&&http.getSize()<=(int)appconfig::kMaximumGeocodingResponseBytes){String body=http.getString();if(body.length()<=appconfig::kMaximumGeocodingResponseBytes){JsonDocument doc;if(!deserializeJson(doc,body)){for(JsonObjectConst item:doc["results"].as<JsonArrayConst>()){if(n.resultCount>=5)break;double a=item["latitude"]|NAN,o=item["longitude"]|NAN;if(!validCoordinates(a,o))continue;SearchResult&r=n.results[n.resultCount++];const char*name=item["name"]|"",*region=item["admin1"]|"",*nation=item["country"]|"";snprintf(r.label,sizeof(r.label),"%s%s%s, %s",name,region[0]?", ":"",region,nation);r.latitude=a;r.longitude=o;}ok=true;}}body=String();}http.end();}n.searchState=ok&&n.resultCount?SearchState::Complete:SearchState::Invalid;n.state=n.searchState==SearchState::Complete?State::Searching:State::Offline;publish(n);}
bool WeatherManager::persist(LocationSource s,double a,double o,const char*l){
  if(s!=LocationSource::Gps&&s!=LocationSource::Disabled&&!validCoordinates(a,o))return false;
  preferences_.putBool("initialized",true);
  preferences_.putUChar("source",(uint8_t)s);
  if(s!=LocationSource::Gps&&s!=LocationSource::Disabled){
    preferences_.putDouble("latitude",a);preferences_.putDouble("longitude",o);
  }
  preferences_.putString("label",l?l:"");
  Snapshot n=snapshot();n.source=s;n.configured=s!=LocationSource::Unconfigured&&s!=LocationSource::Disabled;
  n.state=s==LocationSource::Disabled?State::Disabled:(s==LocationSource::Gps&&!gpsValid_?State::GpsFixRequired:State::WaitingForWifi);
  n.dataAvailable=false;n.lastSuccessMs=0;n.searchState=SearchState::Idle;
  strlcpy(n.city,l?l:"",sizeof(n.city));savedLatitude_=a;savedLongitude_=o;
  lastRequestPositionValid_=false;nextPollMs_=0;forceRefresh_=true;publish(n);
  Serial.printf("WEATHER location_source=%s\n",sourceName(s));
  Serial.printf("WEATHER configured=%s\n",n.configured?"YES":"NO");
  return true;
}
bool WeatherManager::saveSearchResult(uint8_t i,LocationSource s){Snapshot c=snapshot();if(i>=c.resultCount||(s!=LocationSource::City&&s!=LocationSource::Postal))return false;return persist(s,c.results[i].latitude,c.results[i].longitude,c.results[i].label);}bool WeatherManager::saveManual(double a,double o){return validCoordinates(a,o)&&persist(LocationSource::Manual,a,o,"MANUAL LOCATION");}bool WeatherManager::saveGps(){return gpsValid_&&persist(LocationSource::Gps,0,0,"GPS");}void WeatherManager::disable(){persist(LocationSource::Disabled,0,0,"");}
void WeatherManager::toggleTemperatureUnit(){Snapshot n=snapshot();n.temperatureUnit=n.temperatureUnit==TemperatureUnit::Celsius?TemperatureUnit::Fahrenheit:TemperatureUnit::Celsius;preferences_.putBool("fahrenheit",n.temperatureUnit==TemperatureUnit::Fahrenheit);n.dataAvailable=false;nextPollMs_=0;forceRefresh_=true;publish(n);}void WeatherManager::toggleHomeOption(uint8_t o){Snapshot n=snapshot();if(o==0){n.showTemperature=!n.showTemperature;preferences_.putBool("show_temp",n.showTemperature);}if(o==1){n.showCondition=!n.showCondition;preferences_.putBool("show_cond",n.showCondition);}if(o==2){n.showCity=!n.showCity;preferences_.putBool("show_city",n.showCity);}if(o==3){n.showFeelsLike=!n.showFeelsLike;preferences_.putBool("show_feels",n.showFeelsLike);}publish(n);}bool WeatherManager::requestRefresh(uint32_t n){if(lastManualRefreshMs_&&n-lastManualRefreshMs_<appconfig::kWeatherManualRefreshLimitMs)return false;lastManualRefreshMs_=n;forceRefresh_=true;return true;}
void WeatherManager::poll(uint32_t now){bool manual=forceRefresh_;forceRefresh_=false;Snapshot n=snapshot();if(!n.configured||n.source==LocationSource::Disabled)return;double a=savedLatitude_,o=savedLongitude_;if(n.source==LocationSource::Gps){if (!gpsValid_){n.state=State::GpsFixRequired;nextPollMs_=now+5000;publish(n);return;}a=gpsLatitude_;o=gpsLongitude_;if(!manual&&n.lastSuccessMs&&now-n.lastSuccessMs<appconfig::kWeatherPollIntervalMs){nextPollMs_=n.lastSuccessMs+appconfig::kWeatherPollIntervalMs;return;}if(!manual&&lastRequestPositionValid_&&distanceKm(lastRequestLatitude_,lastRequestLongitude_,a,o)<appconfig::kWeatherGpsMoveThresholdKm){nextPollMs_=now+appconfig::kWeatherPollIntervalMs;return;}}nextPollMs_=now+appconfig::kWeatherPollIntervalMs;if(WiFi.status()!=WL_CONNECTED){n.state=n.dataAvailable&&now-n.lastSuccessMs<=appconfig::kWeatherFreshForMs?State::Stale:State::Offline;publish(n);return;}bool f=n.temperatureUnit==TemperatureUnit::Fahrenheit;String url=String("https://api.open-meteo.com/v1/forecast?latitude=")+String(a,6)+"&longitude="+String(o,6)+"&current=temperature_2m,apparent_temperature,weather_code&temperature_unit="+(f?"fahrenheit":"celsius");WiFiClientSecure client;client.setInsecure();HTTPClient http;http.setConnectTimeout(appconfig::kHttpConnectTimeoutMs);http.setTimeout(appconfig::kHttpReadTimeoutMs);bool ok=false;if(http.begin(client,url)){int status=http.GET();if(status>=200&&status<300&&http.getSize()<=(int)appconfig::kMaximumWeatherResponseBytes){String body=http.getString();if(body.length()<=appconfig::kMaximumWeatherResponseBytes){JsonDocument doc;if(!deserializeJson(doc,body)){JsonObjectConst cur=doc["current"].as<JsonObjectConst>();double t=cur["temperature_2m"]|NAN,feels=cur["apparent_temperature"]|NAN;int code=cur["weather_code"]|-1;if(isfinite(t)&&t>=-100&&t<=150&&code>=0&&code<=255){n.temperatureTenths=(int16_t)lround(t*10);n.feelsLikeAvailable=isfinite(feels)&&feels>=-100&&feels<=150;if(n.feelsLikeAvailable)n.feelsLikeTenths=(int16_t)lround(feels*10);n.weatherCode=(uint8_t)code;n.dataAvailable=true;n.lastSuccessMs=now;n.state=State::Online;ok=true;lastRequestLatitude_=a;lastRequestLongitude_=o;lastRequestPositionValid_=true;}}}body=String();}http.end();}if(!ok)n.state=n.dataAvailable&&now-n.lastSuccessMs<=appconfig::kWeatherFreshForMs?State::Stale:State::Offline;
  Serial.printf("WEATHER result=%s\n",ok?"ONLINE":(n.state==State::Stale?"OFFLINE":"INVALID"));
  publish(n);
}
void WeatherManager::publish(const Snapshot&n){if(xSemaphoreTake(mutex_,portMAX_DELAY)==pdTRUE){uint32_t v=changed(snapshot_,n)?snapshot_.version+1:snapshot_.version;snapshot_=n;snapshot_.version=v;xSemaphoreGive(mutex_);}}
} // namespace weather