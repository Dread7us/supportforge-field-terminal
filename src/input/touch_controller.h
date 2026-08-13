#pragma once
#include <Preferences.h>
#include <stdint.h>
#include "ui/ui_theme.h"
namespace input {
struct Point { int x; int y; };
enum class Transform : uint8_t { Identity, InvertX, InvertY, InvertXY, Swap, SwapInvertX, SwapInvertY, SwapInvertXY };
enum class ActionType : uint8_t { None, Tap, SwipeLeft, SwipeRight, QualificationPassed, QualificationRejected };
struct TouchAction { ActionType type; Point point; const char* reason; };
Point applyTransform(Transform transform, int rawX, int rawY);
bool cornerContains(uint8_t corner, Point point);
const char* transformName(Transform transform);
class TouchController {
 public:
  bool begin();
  TouchAction poll(uint32_t nowMs, bool inputBlocked = false);
  bool observeTouch(uint32_t timeoutMs);
  void startQualification();
  void resetQualification();
  void notifyDisplayUpdateFinished(uint32_t nowMs);
  void printStatus() const;
  bool mappingVerified() const { return mappingVerified_; }
  bool qualifying() const { return qualifying_; }
  bool armed() const { return armed_; }
  uint8_t qualificationStep() const { return qualificationStep_; }
  void setDiagnosticMode(bool enabled) { diagnosticMode_ = enabled; }
  bool diagnosticMode() const { return diagnosticMode_; }
  bool sampleSeen() const { return sampleSeen_; }
 private:
  bool readRaw(bool& pressed, Point& raw, Point& transformed);
  void saveQualification(bool verified);
  bool selectPhysicalTransform();
  TouchAction reject(Point point, const char* reason);
  uint8_t address_ = 0;
  bool down_ = false, mappingVerified_ = false, qualifying_ = false;
  bool diagnosticMode_ = false, sampleSeen_ = false, armed_ = false, stableContact_ = false;
  bool releaseObserved_ = false;
  uint8_t qualificationStep_ = 0;
  Transform transform_ = Transform::SwapInvertX;
  Point rawStart_{0,0}, start_{0,0}, last_{0,0}, physicalSamples_[4]{};
  uint32_t downAtMs_ = 0, cleanReleaseSinceMs_ = 0, lastContactReportMs_ = 0;
  Preferences preferences_;
};
}  // namespace input
