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

constexpr Rect kNavBounds{0, kContentBottom, 5 * spec::kNavItemWidth, kNavHeight};
constexpr Rect kPrimaryContentBounds{kMargin, kAppBarHeight + 16,
                                      kCanvasWidth - 2 * kMargin,
                                      kContentBottom - kAppBarHeight - 32};
constexpr Rect kDeviceDiagnosticsAction = contractRect(spec::kDeviceCards[3]);
constexpr Rect kDeviceTouchSetupAction = contractRect(spec::kDeviceCards[4]);
constexpr Rect kDeviceRefreshAction{24, 654, 238, 72};
constexpr Rect kDeviceTemperatureAction{278, 654, 238, 72};
constexpr Rect kSystemsSectionAction{24, 742, 492, 72};
constexpr Rect kDeviceSettingsAction = contractRect(spec::kDeviceCards[5]);
constexpr Rect kDeviceDisplayRefreshAction = kDeviceRefreshAction;
constexpr Rect kSettingsTimezoneAction = contractRect(spec::kSettingsCards[1]);
constexpr Rect kSettingsFormatAction = contractRect(spec::kSettingsCards[2]);
constexpr Rect kSettingsSyncAction = contractRect(spec::kSettingsCards[4]);
constexpr Rect kSettingsTouchAction{366, 642, 150, 82};
constexpr Rect kSettingsLowPowerAction{24, 642, 150, 82};
constexpr Rect kSettingsSyncActionCompact{195, 642, 150, 82};
constexpr Rect kSettingsTouchActionCompact = kSettingsTouchAction;
constexpr Rect kTouchRecalibrateCancelAction{24, 742, 238, 72};
constexpr Rect kTouchRecalibrateConfirmAction{278, 742, 238, 72};
constexpr Rect kHomeWeatherAction = contractRect(spec::kHomeCards[5]);
constexpr Rect kLocationGpsPowerAction{24, 582, 238, 72};
constexpr Rect kLocationSpeedUnitAction{278, 582, 238, 72};
constexpr Rect kLocationPrivacyAction{24, 670, 238, 72};
constexpr Rect kLocationWeatherSetupAction{278, 670, 238, 72};
constexpr Rect kLocationWeatherRefreshAction{24, 758, 492, 72};
constexpr Rect kLocationElevationAction{24, 396, 492, 58};
constexpr Rect kSettingsWeatherAction{24, 758, 492, 72};
constexpr Rect kDiagnosticsDisplayCalibrationAction{24, 632, 492, 158};
constexpr Rect kDiagnosticsTextQualificationAction{24, 632, 492, 158};
constexpr Rect kRefreshCancelAction{24, 742, 238, 72};
constexpr Rect kRefreshConfirmAction{278, 742, 238, 72};
constexpr Rect kDetailBackAction{24, 776, 492, 72};
constexpr Rect kHomeHostAction = contractRect(spec::kHomeCards[0]);
constexpr Rect kHomeMetricsAction = contractRect(spec::kHomeCards[1]);
constexpr Rect kHomeWeatherDetailAction = contractRect(spec::kHomeCards[2]);
constexpr Rect kHomeMotionAction = contractRect(spec::kHomeCards[3]);
constexpr Rect kHomeStorageAction{24,706,492,48};
constexpr Rect kHomeNetworkAction{24,658,492,48};
constexpr Rect kHomeBatteryAction{24,610,492,48};
constexpr Rect kHeaderBatteryAction = contractRect(spec::kHeaderBatteryBounds);
constexpr Rect kGlobalRefreshAction = contractRect(spec::kFooterRefreshBounds);
constexpr Rect kHeaderClockAction = contractRect(spec::kHeaderClockBounds);
constexpr Rect kDeviceBatteryAction = contractRect(spec::kDeviceCards[0]);
constexpr Rect kWeatherDetailSetupAction{24,690,492,64};
constexpr Rect kTimezoneBackAction{24,776,492,72};
constexpr Rect kLowPowerBackAction{24,776,492,72};
constexpr Rect kLowPowerExitAction{24,690,492,72};
constexpr Rect kSettingsRefreshModeAction{24, 540, 492, 86};
constexpr Rect kRefreshModeActions[] = {
    {24, 180, 492, 144}, {24, 344, 492, 144}, {24, 508, 492, 164}};
constexpr Rect kRefreshModeBackAction{24, 776, 492, 72};
constexpr Rect kLowPowerPresetActions[] = {
    {24,176,492,92}, {24,282,492,92}, {24,388,492,92},
    {24,494,492,92}, {24,600,492,92}};
constexpr Rect kTimezoneActions[] = {
    {24,150,492,104}, {24,268,492,104}, {24,386,492,104},
    {24,504,492,104}, {24,622,492,104}};
static_assert(kTimezoneActions[4].y + kTimezoneActions[4].h < kTimezoneBackAction.y,
              "timezone options and Back must not overlap");
static_assert(kLowPowerPresetActions[4].y + kLowPowerPresetActions[4].h < kLowPowerBackAction.y,
              "low-power options and Back must not overlap");
static_assert(kGlobalRefreshAction.w >= kMinimumTouchTarget &&
                  kGlobalRefreshAction.h >= kMinimumTouchTarget,
              "global refresh must remain a deliberate touch target");
static_assert(kNavBounds.x + kNavBounds.w == kGlobalRefreshAction.x &&
                  kGlobalRefreshAction.x + kGlobalRefreshAction.w == kCanvasWidth,
              "footer navigation and refresh regions must tile the canvas");
static_assert(kRefreshModeActions[0].h >= kMinimumTouchTarget &&
                  kRefreshModeActions[1].h >= kMinimumTouchTarget &&
                  kRefreshModeActions[2].h >= kMinimumTouchTarget &&
                  kRefreshModeActions[2].y + kRefreshModeActions[2].h < kRefreshModeBackAction.y,
              "refresh mode choices and Back must be large and non-overlapping");

// Dedicated Vehicle Motion geometry. The speed rectangle is deliberately the
// largest metric region on the route and is cleared independently before text.
constexpr Rect kVehicleGpsStateBounds{180, 102, 180, 30};
constexpr Rect kVehicleSpeedBounds{24, 136, 492, 186};
constexpr Rect kVehicleSpeedUnitBounds{24, 322, 492, 34};
constexpr Rect kVehicleMovementBounds{24, 358, 492, 42};
constexpr Rect kVehicleCompassBounds{24, 418, 260, 166};
constexpr Rect kVehicleCourseBounds{300, 418, 216, 166};
constexpr Rect kVehicleQualityBounds{24, 602, 492, 154};
constexpr Rect kVehicleElevationAction{24, 602, 492, 58};
constexpr Rect kVehicleLocationAction{150, 88, 240, 48};
constexpr Rect kAltimeterMetricBounds{36, 196, 468, 150};
constexpr Rect kAltimeterUnitAction{278, 688, 238, 64};
static_assert(kLocationElevationAction.w >= kMinimumTouchTarget &&
                  kLocationElevationAction.h >= kMinimumTouchTarget &&
                  kVehicleElevationAction.w >= kMinimumTouchTarget &&
                  kVehicleElevationAction.h >= kMinimumTouchTarget,
              "elevation rows must remain qualified touch targets");
static_assert(kVehicleSpeedBounds.w > kVehicleCompassBounds.w &&
                  kVehicleSpeedBounds.h > kVehicleQualityBounds.h,
              "vehicle speed must remain the dominant metric region");
static_assert(kVehicleSpeedBounds.y >= kVehicleGpsStateBounds.y + kVehicleGpsStateBounds.h &&
                  kVehicleSpeedBounds.y + kVehicleSpeedBounds.h <= kVehicleSpeedUnitBounds.y &&
                  kVehicleMovementBounds.y + kVehicleMovementBounds.h <= kVehicleCompassBounds.y &&
                  kVehicleCompassBounds.y + kVehicleCompassBounds.h <= kVehicleQualityBounds.y &&
                  kVehicleQualityBounds.y + kVehicleQualityBounds.h <= kDetailBackAction.y,
              "vehicle motion regions must not overlap");

}  // namespace ui