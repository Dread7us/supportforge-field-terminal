#pragma once

#include <stdint.h>

namespace ui {

constexpr int kCanvasWidth = 540;
constexpr int kCanvasHeight = 960;
constexpr int kMargin = 24;
constexpr int kGrid = 8;
constexpr int kAppBarHeight = 104;
constexpr int kNavHeight = 96;
constexpr int kContentBottom = kCanvasHeight - kNavHeight;
constexpr int kMinimumTouchTarget = 56;
constexpr bool kPartialRefreshEnabled = false;

constexpr uint8_t kInk = 0x00;
constexpr uint8_t kInkMuted = 0x55;
constexpr uint8_t kRule = 0xA0;
constexpr uint8_t kSurfaceStrong = 0xC4;
constexpr uint8_t kSurface = 0xDD;
constexpr uint8_t kSurfaceSoft = 0xEE;
constexpr uint8_t kPaper = 0xFF;

struct Rect {
  int x;
  int y;
  int w;
  int h;

  constexpr bool contains(int px, int py) const {
    return px >= x && py >= y && px < x + w && py < y + h;
  }
};

constexpr Rect kNavBounds{0, kContentBottom, kCanvasWidth, kNavHeight};
constexpr Rect kPrimaryContentBounds{kMargin, kAppBarHeight + 16,
                                      kCanvasWidth - 2 * kMargin,
                                      kContentBottom - kAppBarHeight - 32};

}  // namespace ui