#pragma once

#include <stdint.h>
#include "ui/ui_theme.h"

namespace input {

struct Point { int x; int y; };
struct Tap { bool accepted; Point point; };

Point transformInvertedPortrait(int rawX, int rawY);

class TouchController {
 public:
  bool begin();
  Tap poll(uint32_t nowMs);
  bool runFourCornerTest();
  bool mappingVerified() const { return mappingVerified_; }

 private:
  bool readRaw(bool& pressed, Point& point);
  uint8_t address_ = 0;
  bool down_ = false;
  bool mappingVerified_ = false;
  Point start_{0,0};
  Point last_{0,0};
  uint32_t downAtMs_ = 0;
  uint32_t lastAcceptedMs_ = 0;
};

}  // namespace input