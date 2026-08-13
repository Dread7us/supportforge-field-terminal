#pragma once

#include <stdint.h>
#include "ui_spec_generated.h"

namespace ui {

constexpr int kCanvasWidth = spec::kCanvasWidth;
constexpr int kCanvasHeight = spec::kCanvasHeight;
constexpr int kMargin = spec::kMargin;
constexpr int kGrid = spec::kGrid;
constexpr int kAppBarHeight = spec::kAppBarHeight;
constexpr int kNavHeight = spec::kNavHeight;
constexpr int kContentBottom = spec::kContentBottom;
constexpr int kMinimumTouchTarget = spec::kMinimumTouchTarget;
constexpr bool kPartialRefreshEnabled = false;
constexpr bool kHighContrastQualificationTheme = true;

constexpr uint8_t kInk = spec::kInk;
constexpr uint8_t kInkMuted = spec::kInkMuted;
constexpr uint8_t kRule = spec::kRule;
constexpr uint8_t kSurfaceStrong = spec::kSurfaceStrong;
constexpr uint8_t kSurface = spec::kSurface;
constexpr uint8_t kSurfaceSoft = spec::kSurfaceSoft;
constexpr uint8_t kPaper = spec::kPaper;
static_assert(kInk == 0x00 && kInkMuted == 0x00 && kRule == 0x00,
              "required EPD UI information must be pure black");
static_assert(kSurfaceStrong == 0xFF && kSurface == 0xFF &&
                  kSurfaceSoft == 0xFF && kPaper == 0xFF,
              "required EPD UI backgrounds must be pure white");

struct Rect {
  int x;
  int y;
  int w;
  int h;

  constexpr bool contains(int px, int py) const {
    return px >= x && py >= y && px < x + w && py < y + h;
  }
};

constexpr Rect contractRect(const int (&rect)[4]) {
  return {rect[0], rect[1], rect[2], rect[3]};
}

constexpr Rect kNavBounds{0, kContentBottom, kCanvasWidth, kNavHeight};
constexpr Rect kPrimaryContentBounds{kMargin, kAppBarHeight + 16,
                                      kCanvasWidth - 2 * kMargin,
                                      kContentBottom - kAppBarHeight - 32};
constexpr Rect kDeviceDiagnosticsAction = contractRect(spec::kDeviceCards[3]);
constexpr Rect kDeviceTouchSetupAction = contractRect(spec::kDeviceCards[4]);
constexpr Rect kDeviceRefreshAction{24, 654, 238, 72};
constexpr Rect kDeviceTemperatureAction{278, 654, 238, 72};
constexpr Rect kSystemsSectionAction{24, 742, 492, 72};
constexpr Rect kDeviceSettingsAction = contractRect(spec::kDeviceCards[5]);
constexpr Rect kSettingsTimezoneAction = contractRect(spec::kSettingsCards[1]);
constexpr Rect kSettingsFormatAction = contractRect(spec::kSettingsCards[2]);
constexpr Rect kSettingsSyncAction = contractRect(spec::kSettingsCards[4]);
constexpr Rect kHomeWeatherAction = contractRect(spec::kHomeCards[5]);
constexpr Rect kLocationGpsPowerAction{24, 582, 238, 72};
constexpr Rect kLocationSpeedUnitAction{278, 582, 238, 72};
constexpr Rect kLocationPrivacyAction{24, 670, 238, 72};
constexpr Rect kLocationWeatherSetupAction{278, 670, 238, 72};
constexpr Rect kLocationWeatherRefreshAction{24, 758, 492, 72};
constexpr Rect kSettingsWeatherAction{24, 758, 492, 72};
constexpr Rect kDiagnosticsDisplayCalibrationAction{24, 632, 492, 158};
constexpr Rect kDiagnosticsTextQualificationAction{24, 632, 492, 158};

}  // namespace ui