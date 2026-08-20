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

// Shared visual tokens keep every route on the same 8 px rhythm. The panel is
// deliberately pure black and white; hierarchy comes from weight, whitespace,
// shape, and inversion instead of low-contrast gray that fades on e-paper.
constexpr int kRadiusSmall = 8;
constexpr int kRadiusControl = 12;
constexpr int kRadiusCard = 16;
constexpr int kControlInset = 16;
constexpr int kIconBoxSize = 32;
constexpr int kIconLabelGap = 12;

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
constexpr Rect kDeviceDiagnosticsAction{24,550,238,88};
constexpr Rect kDeviceTouchSetupAction = contractRect(spec::kDeviceCards[4]);
constexpr Rect kDeviceRefreshAction{24,654,492,72};
constexpr Rect kDeviceLowPowerAction{278,550,238,88};
constexpr Rect kSystemsSectionAction{24, 742, 492, 72};
constexpr Rect kDeviceSettingsAction = contractRect(spec::kDeviceCards[5]);
constexpr Rect kDeviceDisplayRefreshAction = kDeviceRefreshAction;
constexpr Rect kDeviceWifiAction = contractRect(spec::kDeviceCards[1]);
constexpr Rect kSettingsTimezoneAction = contractRect(spec::kSettingsCards[1]);
constexpr Rect kSettingsFormatAction = contractRect(spec::kSettingsCards[2]);
constexpr Rect kSettingsSyncAction = contractRect(spec::kSettingsCards[4]);
constexpr Rect kSettingsTouchAction{366, 642, 150, 82};
constexpr Rect kSettingsLowPowerAction{24, 642, 150, 82};
constexpr Rect kSettingsTemperatureAction = kSettingsLowPowerAction;
constexpr Rect kSettingsSyncActionCompact{195, 642, 150, 82};
constexpr Rect kSettingsTouchActionCompact = kSettingsTouchAction;
constexpr Rect kTouchRecalibrateCancelAction{24, 742, 238, 72};
constexpr Rect kTouchRecalibrateConfirmAction{278, 742, 238, 72};
constexpr Rect kHomeHeroAction = contractRect(spec::kHomeHeroBounds);
constexpr Rect kHomeClockAction = contractRect(spec::kHomeClockBounds);
constexpr Rect kHomeWeatherAction = contractRect(spec::kHomeWeatherBounds);
constexpr Rect kLocationGpsPowerAction{24, 582, 238, 72};
constexpr Rect kLocationSpeedUnitAction{278, 582, 238, 72};
constexpr Rect kLocationPrivacyAction{24, 670, 238, 72};
constexpr Rect kLocationWeatherSetupAction{278, 670, 238, 72};
constexpr Rect kLocationWeatherRefreshAction{24, 758, 492, 72};
constexpr Rect kLocationElevationAction{24, 396, 492, 58};
constexpr Rect kSettingsWeatherAction{24, 758, 492, 72};
constexpr Rect kDiagnosticsDisplayCalibrationAction{24, 632, 492, 126};
constexpr Rect kDiagnosticsTextQualificationAction{24, 632, 492, 126};
constexpr Rect kDetailBackAction{24, 776, 492, 72};
constexpr Rect kHomeHostAction = contractRect(spec::kHomeCards[1]);
constexpr Rect kHomeMetricsAction = contractRect(spec::kHomeCards[2]);
constexpr Rect kHomeNetworkAction = contractRect(spec::kHomeCards[3]);
constexpr Rect kHomeBatteryAction = contractRect(spec::kHomeCards[4]);
constexpr Rect kHomeWeatherDetailAction = kHomeWeatherAction;
constexpr Rect kHeaderBatteryAction = contractRect(spec::kHeaderBatteryBounds);
constexpr Rect kHeaderWifiAction = contractRect(spec::kHeaderWifiBounds);
constexpr Rect kHeaderClockAction = contractRect(spec::kHeaderClockBounds);
static_assert(kHomeClockAction.y + kHomeClockAction.h <= kHomeWeatherAction.y &&
                  kHomeWeatherAction.y + kHomeWeatherAction.h <= kHomeHostAction.y &&
                  kHomeHostAction.x + kHomeHostAction.w <= kHomeMetricsAction.x &&
                  kHomeNetworkAction.x + kHomeNetworkAction.w <= kHomeBatteryAction.x &&
                  kHomeClockAction.h >= kMinimumTouchTarget &&
                  kHomeWeatherAction.h >= kMinimumTouchTarget,
              "HOME dashboard actions must remain fixed, disjoint qualified targets");
constexpr Rect kDeviceBatteryAction = contractRect(spec::kDeviceCards[0]);
constexpr Rect kWeatherDetailSetupAction{24,690,492,64};
constexpr Rect kTimezoneBackAction{24,776,492,72};
constexpr Rect kLowPowerBackAction{24,776,492,72};
constexpr Rect kLowPowerExitAction{24,690,492,72};
constexpr Rect kSettingsRefreshModeAction{24, 540, 492, 86};
constexpr Rect kRefreshModeActions[] = {
    {24, 180, 492, 144}, {24, 344, 492, 144}, {24, 508, 492, 164}};
constexpr Rect kRefreshModeBackAction{24, 776, 492, 72};
constexpr Rect kDisplayFrontLightActions[] = {
    {24, 176, 238, 92}, {278, 176, 238, 92},
    {24, 282, 238, 92}, {278, 282, 238, 92}};
constexpr Rect kDisplayRefreshSettingsAction{24, 420, 492, 104};
constexpr Rect kDisplaySettingsBackAction{24, 776, 492, 72};
// Settings deliberately uses nearly all available content height. The former
// 68 px rows and wide dead bands made physical taps unnecessarily demanding.
constexpr Rect kSettingsCategoryActions[] = {
    {24,108,238,92},{278,108,238,92},{24,208,238,92},{278,208,238,92},
    {24,308,238,92},{278,308,238,92},{24,408,238,92},{278,408,238,92},
    {24,508,492,108},{24,624,492,108},{24,740,492,108}};
constexpr Rect kDateTimeTimezoneAction{24,280,492,88};
constexpr Rect kDateTimeFormatAction{24,386,492,88};
constexpr Rect kDateTimeSyncAction{24,492,492,88};
constexpr Rect kUnitsTemperatureAction{24,150,492,100};
constexpr Rect kUnitsSpeedAction{24,270,492,100};
constexpr Rect kUnitsElevationAction{24,390,492,100};
constexpr Rect kLocationSettingsGpsAction{24,304,492,88};
constexpr Rect kLocationSettingsPrivacyAction{24,410,492,88};
constexpr Rect kLocationSettingsWeatherAction{24,516,492,88};
constexpr Rect kWifiScanAction{24,246,238,64};
constexpr Rect kWifiManualAction{278,246,238,64};
constexpr Rect kWifiDisconnectAction{24,326,238,64};
constexpr Rect kWifiReconnectAction{278,326,238,64};
constexpr Rect kWifiForgetAction{24,406,492,64};
constexpr Rect kWifiBackAction{24,776,492,72};
constexpr Rect kWifiNetworkActions[] = {{24,142,492,82},{24,234,492,82},{24,326,492,82},
    {24,418,492,82},{24,510,492,82},{24,602,492,82}};
constexpr Rect kWifiEntryKeys[] = {{24,326,82,68},{106,326,82,68},{188,326,82,68},{270,326,82,68},{352,326,82,68},{434,326,82,68},
    {24,404,82,68},{106,404,82,68},{188,404,82,68},{270,404,82,68},{352,404,82,68},{434,404,82,68},
    {24,482,82,68},{106,482,82,68},{188,482,82,68},{270,482,82,68},{352,482,82,68},{434,482,82,68},
    {24,560,82,68},{106,560,82,68},{188,560,82,68},{270,560,82,68},{352,560,82,68},{434,560,82,68}};
constexpr Rect kWifiEntryModeAction{24,648,150,64};
constexpr Rect kWifiEntryDeleteAction{195,648,150,64};
constexpr Rect kWifiEntryNextAction{366,648,150,64};
constexpr Rect kWifiEntryCancelAction{24,776,238,72};
constexpr Rect kWifiEntrySaveAction{278,776,238,72};
constexpr Rect kCalculatorBackAction{24,72,126,60};
constexpr Rect kCalculatorKeys[] = {
    {24,336,114,108},{150,336,114,108},{276,336,114,108},{402,336,114,108},
    {24,456,114,108},{150,456,114,108},{276,456,114,108},{402,456,114,108},
    {24,576,114,108},{150,576,114,108},{276,576,114,108},{402,576,114,108},
    {24,696,114,108},{150,696,114,108},{276,696,114,108},{402,696,114,228},
    {24,816,114,108},{150,816,114,108},{276,816,114,108}};
static_assert(kSettingsCategoryActions[10].y + kSettingsCategoryActions[10].h <= kContentBottom,
              "settings actions must maximize content without entering the footer");
static_assert(kCalculatorKeys[18].y + kCalculatorKeys[18].h <= kCanvasHeight &&
                  kCalculatorKeys[15].y + kCalculatorKeys[15].h <= kCanvasHeight,
              "calculator keys must maximize the full-screen route without clipping");
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
static_assert(5 * spec::kNavItemWidth == kCanvasWidth,
              "five primary navigation targets must tile the full footer");
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