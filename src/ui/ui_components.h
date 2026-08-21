#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "ui_state.h"
#include "ui_fonts_generated.h"
#include "ui_theme.h"

namespace ui {

enum class Icon : uint8_t {
  Home, Systems, Radio, Location, Device, Battery, Wifi, Lock, Check, Info,
  ChevronLeft, ChevronRight, Refresh, Settings, Power, Search, Close,
  Clock, Calculator, Display, Weather, Cleanup, Diagnostics, Keyboard, Delete,
  Light, Units, Privacy, Touch, Next, Save
};
enum class FontRole : uint8_t {
  Caption, Body, CardHeading, PageHeading, Brand, Metric, HomeClock, VehicleSpeed, AltimeterMetric, Navigation,
  QualificationCurrent, QualificationRegularAa, QualificationMediumAa,
  QualificationSemiboldAa, QualificationBoldMono
};

void clear(uint8_t* fb);
void roundedRect(uint8_t* fb, Rect rect, int radius, uint8_t fill, uint8_t stroke);
void text(uint8_t* fb, Rect clip, int x, int baseline, const String& value,
          FontRole role = FontRole::Body, uint8_t color = kInk);
int textWidth(const String& value, FontRole role = FontRole::Body);
int textHeight(FontRole role = FontRole::Body);
bool textFits(const String& value, FontRole role, Rect region);
String fittedText(const String& value, FontRole role, int width);
int centeredBaseline(Rect bounds, FontRole role);
void actionButton(uint8_t* fb, Rect bounds, const String& label, bool selected,
                  Icon glyph, bool backLayout = false);
void selectableCard(uint8_t* fb, Rect bounds, const String& title,
                    const String& detail, const String& secondary,
                    bool selected, Icon glyph);
void icon(uint8_t* fb, Icon value, int cx, int cy, int size, uint8_t color = kInk);
void batteryIcon(uint8_t* fb, Rect bounds, battery::State state,
                 bool percentAvailable, uint8_t percent, uint8_t color = kInk);
void wifiIcon(uint8_t* fb, Rect bounds, network::State state,
              bool rssiAvailable, int16_t rssi, uint8_t color = kInk);
void circle(uint8_t* fb, int cx, int cy, int radius, uint8_t color = kInk);
void appBar(uint8_t* fb, const UiSnapshot& state, const char* section = nullptr);
void card(uint8_t* fb, Rect bounds, const char* eyebrow, const String& title,
          const String& body = "");
void statusPill(uint8_t* fb, Rect bounds, const String& label, bool dark = false);
void metricTile(uint8_t* fb, Rect bounds, Icon glyph, const char* label,
                const String& value, const String& detail = "");
void labeledRow(uint8_t* fb, Rect bounds, const char* label, const String& value,
                bool divider = true);
void emptyState(uint8_t* fb, Rect bounds, Icon glyph, const String& title,
                const String& body);
void dialog(uint8_t* fb, Rect bounds, const String& title, const String& body,
            const String& actionLabel);
void bottomNavigation(uint8_t* fb, Page selected);
Rect navigationTarget(Page page);

}  // namespace ui