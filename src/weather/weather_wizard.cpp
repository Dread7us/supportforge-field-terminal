#include "weather_wizard.h"

#include <Arduino.h>
#include <stdlib.h>
#include <string.h>

namespace weather {
namespace {
constexpr ui::Rect kBack{24,112,112,56};
constexpr ui::Rect kSubmit{278,780,238,64};
constexpr ui::Rect kCancel{24,780,238,64};
constexpr ui::Rect kPicker{24,238,492,420};
constexpr ui::Rect kPickerPage{24,682,150,64};
constexpr ui::Rect kPickerDelete{195,682,150,64};
constexpr ui::Rect kPickerSpace{366,682,150,64};

void setError(WizardSnapshot& state, const char* value) {
  strlcpy(state.error, value ? value : "", sizeof(state.error));
}
}

const char* inputKindName(InputKind kind) {
  switch (kind) {
    case InputKind::City: return "CITY OR PLACE";
    case InputKind::Postal: return "POSTAL CODE";
    case InputKind::Latitude: return "LATITUDE";
    case InputKind::Longitude: return "LONGITUDE";
  }
  return "INPUT";
}

const char* pickerCharacters(InputKind kind, uint8_t page) {
  if (kind == InputKind::Postal) return page % 2 ? "ABCDEFGHIJKLMNOPQRSTUVWXYZ    " : "0123456789ABCDEFGHIJKLMNOPQRST";
  if (kind == InputKind::Latitude || kind == InputKind::Longitude)
    return "0123456789-.                  ";
  return page % 2 ? "UVWXYZ-'0123456789            " : "ABCDEFGHIJKLMNOPQRST          ";
}

void WeatherWizard::open() {
  state_ = WizardSnapshot{};
  state_.active = true;
  state_.step = WizardStep::Choice;
}
void WeatherWizard::openPreferences() {
  state_ = WizardSnapshot{};
  state_.active = true;
  state_.step = WizardStep::Preferences;
}
void WeatherWizard::cancel() { state_.active = false; }

void WeatherWizard::resetInput(InputKind kind) {
  state_.inputKind = kind;
  state_.input[0] = 0;
  state_.characterPage = 0;
  state_.step = WizardStep::Input;
  setError(state_, "");
}

bool WeatherWizard::appendCharacter(char value) {
  char* target = state_.inputKind == InputKind::Latitude ? state_.latitude :
                 (state_.inputKind == InputKind::Longitude ? state_.longitude : state_.input);
  const size_t capacity = state_.inputKind == InputKind::Latitude || state_.inputKind == InputKind::Longitude
                              ? sizeof(state_.latitude) : sizeof(state_.input);
  const size_t length = strlen(target);
  if (length + 1 >= capacity) return false;
  target[length] = value;
  target[length + 1] = 0;
  return true;
}

WizardResult WeatherWizard::tap(ui::Rect point, const location::Snapshot& gps, WeatherManager& manager) {
  if (!state_.active) return WizardResult::None;
  const int x = point.x, y = point.y;
  if (kBack.contains(x,y)) {
    if (state_.step == WizardStep::Choice || state_.step == WizardStep::Preferences) {
      cancel(); return WizardResult::Cancelled;
    }
    if (state_.step == WizardStep::Input && state_.inputKind == InputKind::Longitude) {
      state_.inputKind = InputKind::Latitude; return WizardResult::Changed;
    }
    state_.step = state_.step == WizardStep::Confirm ?
        (state_.pendingSource == LocationSource::Gps ? WizardStep::Gps : WizardStep::Choice) : WizardStep::Choice;
    return WizardResult::Changed;
  }

  if (state_.step == WizardStep::Choice) {
    if (y>=112&&y<168&&x>=150) { state_.step=WizardStep::Preferences; return WizardResult::Changed; }
    if (y>=176&&y<286) { state_.pendingSource=LocationSource::Gps; state_.step=WizardStep::Gps; }
    else if(y>=294&&y<404){state_.pendingSource=LocationSource::City;resetInput(InputKind::City);}
    else if(y>=412&&y<522){state_.pendingSource=LocationSource::Postal;resetInput(InputKind::Postal);}
    else if(y>=530&&y<640){state_.pendingSource=LocationSource::Manual;state_.latitude[0]=state_.longitude[0]=0;resetInput(InputKind::Latitude);}
    else if(y>=648&&y<758){manager.disable();cancel();return WizardResult::Saved;}
    else return WizardResult::None;
    return WizardResult::Changed;
  }

  if (state_.step == WizardStep::Gps) {
    if (kSubmit.contains(x,y)) {
      if (!location::currentFixUsable(gps)) { setError(state_,"VALID GPS FIX REQUIRED"); return WizardResult::Changed; }
      state_.pendingSource=LocationSource::Gps;state_.step=WizardStep::Confirm;setError(state_,"");return WizardResult::Changed;
    }
    if(kCancel.contains(x,y)){cancel();return WizardResult::Cancelled;}
  }

  if (state_.step == WizardStep::Input) {
    char* target = state_.inputKind == InputKind::Latitude ? state_.latitude :
                   (state_.inputKind == InputKind::Longitude ? state_.longitude : state_.input);
    if (kPicker.contains(x,y)) {
      const int column=(x-kPicker.x)/(kPicker.w/6), row=(y-kPicker.y)/(kPicker.h/5);
      const char value=pickerCharacters(state_.inputKind,state_.characterPage)[row*6+column];
      if(value!=' ')appendCharacter(value);return WizardResult::Changed;
    }
    if(kPickerPage.contains(x,y)){++state_.characterPage;return WizardResult::Changed;}
    if(kPickerDelete.contains(x,y)){const size_t n=strlen(target);if(n)target[n-1]=0;return WizardResult::Changed;}
    if(kPickerSpace.contains(x,y)&&state_.inputKind==InputKind::City){appendCharacter(' ');return WizardResult::Changed;}
    if(kPickerSpace.contains(x,y)&&state_.inputKind==InputKind::Postal){
      const char* countries[]={"US","CA","GB","AU"};uint8_t index=0;
      for(uint8_t i=0;i<4;++i)if(!strcmp(state_.country,countries[i]))index=i;
      strlcpy(state_.country,countries[(index+1)%4],sizeof(state_.country));return WizardResult::Changed;
    }
    if(kCancel.contains(x,y)){cancel();return WizardResult::Cancelled;}
    if(kSubmit.contains(x,y)) {
      if (state_.inputKind == InputKind::City) {
        if(strlen(state_.input)<2){setError(state_,"ENTER AT LEAST 2 CHARACTERS");return WizardResult::Changed;}
        manager.requestSearch(state_.input,false,"");state_.step=WizardStep::Results;
      } else if(state_.inputKind==InputKind::Postal) {
        if(!validPostal(state_.input,state_.country)){setError(state_,"INVALID OR UNSUPPORTED POSTAL");return WizardResult::Changed;}
        manager.requestSearch(state_.input,true,state_.country);state_.step=WizardStep::Results;
      } else if(state_.inputKind==InputKind::Latitude) {
        char* end=nullptr;double value=strtod(state_.latitude,&end);
        if(!end||*end||!validCoordinates(value,0)){setError(state_,"LATITUDE MUST BE -90 TO 90");return WizardResult::Changed;}
        resetInput(InputKind::Longitude);
      } else {
        char* endLat=nullptr;char* endLon=nullptr;double lat=strtod(state_.latitude,&endLat),lon=strtod(state_.longitude,&endLon);
        if(!endLat||*endLat||!endLon||*endLon||!validCoordinates(lat,lon)){setError(state_,"LONGITUDE MUST BE -180 TO 180");return WizardResult::Changed;}
        state_.pendingSource=LocationSource::Manual;state_.step=WizardStep::Confirm;
      }
      setError(state_,"");return WizardResult::Changed;
    }
  }

  if(state_.step==WizardStep::Results){
    const Snapshot weather=manager.snapshot();
    if(weather.searchState==SearchState::Complete&&y>=190&&y<690){uint8_t index=(y-190)/100;if(index<weather.resultCount){state_.selectedResult=index;state_.step=WizardStep::Confirm;return WizardResult::Changed;}}
    if(kCancel.contains(x,y)){cancel();return WizardResult::Cancelled;}
  }

  if(state_.step==WizardStep::Confirm){
    if(kCancel.contains(x,y)){cancel();return WizardResult::Cancelled;}
    if(kSubmit.contains(x,y)){
      bool saved=false;
      if(state_.pendingSource==LocationSource::Gps)saved=manager.saveGps();
      else if(state_.pendingSource==LocationSource::Manual)saved=manager.saveManual(strtod(state_.latitude,nullptr),strtod(state_.longitude,nullptr));
      else saved=manager.saveSearchResult(state_.selectedResult,state_.pendingSource);
      if(!saved){setError(state_,"CONFIGURATION NO LONGER VALID");return WizardResult::Changed;}
      cancel();return WizardResult::Saved;
    }
  }

  if(state_.step==WizardStep::Preferences){
    if(y>=190&&y<290)manager.toggleTemperatureUnit();
    else if(y>=306&&y<406)manager.toggleHomeOption(0);
    else if(y>=422&&y<522)manager.toggleHomeOption(1);
    else if(y>=538&&y<638)manager.toggleHomeOption(2);
    else if(y>=654&&y<754)manager.toggleHomeOption(3);
    else if(kCancel.contains(x,y)){cancel();return WizardResult::Cancelled;}
    else return WizardResult::None;
    return WizardResult::Changed;
  }
  return WizardResult::None;
}

}  // namespace weather