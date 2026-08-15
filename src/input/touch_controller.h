#pragma once
#include <Preferences.h>
#include <stdint.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "ui/ui_theme.h"
namespace input {
struct Point { int x; int y; };
enum class Transform : uint8_t { Identity, InvertX, InvertY, InvertXY, Swap, SwapInvertX, SwapInvertY, SwapInvertXY };
enum class ActionType : uint8_t { None, Tap, SwipeLeft, SwipeRight, QualificationPassed, QualificationRejected };
struct TouchAction {
  TouchAction(ActionType actionType, Point actionPoint, const char* actionReason,
              uint32_t startedMs = 0, uint32_t readyMs = 0)
      : type(actionType), point(actionPoint), reason(actionReason),
        pressStartedMs(startedMs), actionReadyMs(readyMs) {}
  ActionType type;
  Point point;
  const char* reason;
  uint32_t pressStartedMs;
  uint32_t actionReadyMs;
};
Point applyTransform(Transform transform, int rawX, int rawY);
Point applyCalibration(Point transformed, int minX, int maxX, int minY, int maxY);
bool cornerContains(uint8_t corner, Point point);
const char* transformName(Transform transform);
class TouchController {
 public:
  bool begin();
  TouchAction poll(uint32_t nowMs, bool inputBlocked = false);
  bool observeTouch(uint32_t timeoutMs);
  void startQualification();
  void resetQualification();
  bool beginDisplayCapture();
  void endDisplayCapture();
  bool takeQueuedAction(TouchAction& action);
  void notifyDisplayUpdateFinished(uint32_t nowMs);
  void printStatus() const;
  bool mappingVerified() const { return mappingVerified_; }
  bool qualifying() const { return qualifying_; }
  bool armed() const { return armed_; }
  uint8_t qualificationStep() const { return qualificationStep_; }
  void setDiagnosticMode(bool enabled) { diagnosticMode_ = enabled; }
  bool diagnosticMode() const { return diagnosticMode_; }
  bool sampleSeen() const { return sampleSeen_; }
  uint32_t queuedActionCount() const { return queuedActionCount_; }
  uint32_t coalescedActionCount() const { return coalescedActionCount_; }
  void printPerformance() const;
 private:
  static void captureTaskEntry(void* context);
  void captureTask();
  bool readRaw(bool& pressed, Point& raw, Point& transformed);
  void saveQualification(bool verified);
  bool selectPhysicalTransform();
  void calculateCalibrationBounds();
  void drainStaleReports();
  TouchAction reject(Point point, const char* reason);
  uint8_t address_ = 0;
  bool down_ = false, mappingVerified_ = false, qualifying_ = false;
  bool diagnosticMode_ = false, sampleSeen_ = false, armed_ = false, stableContact_ = false;
  bool releaseObserved_ = false;
  uint8_t qualificationStep_ = 0;
  Transform transform_ = Transform::SwapInvertX;
  Point rawStart_{0,0}, start_{0,0}, last_{0,0}, physicalSamples_[4]{};
  int calibrationMinX_ = 0, calibrationMaxX_ = ui::kCanvasWidth - 1;
  int calibrationMinY_ = 0, calibrationMaxY_ = ui::kCanvasHeight - 1;
  bool calibratedBounds_ = false;
  uint32_t downAtMs_ = 0, cleanReleaseSinceMs_ = 0, lastContactReportMs_ = 0;
  uint32_t receivedCount_ = 0, acceptedCount_ = 0, debouncedCount_ = 0, droppedCount_ = 0;
  uint32_t lastPressToActionMs_ = 0, maximumPressToActionMs_ = 0;
  volatile bool displayCaptureActive_ = false, displayCaptureIdle_ = true;
  bool queuedActionPending_ = false;
  TouchAction queuedAction_{ActionType::None,{0,0},"EMPTY"};
  uint32_t queuedActionCount_ = 0, coalescedActionCount_ = 0;
  TaskHandle_t captureTaskHandle_ = nullptr;
  portMUX_TYPE queueMux_ = portMUX_INITIALIZER_UNLOCKED;
  Preferences preferences_;
};
}  // namespace input
