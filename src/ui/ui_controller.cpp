#include "ui_controller.h"

#include <Arduino.h>
#include <epdiy.h>

#include "ui_pages.h"
#include "ui_theme.h"

namespace ui {

namespace { EpdiyHighlevelState gDisplayState; }

bool UiController::begin() {
#if defined(HQ_PROFILE_LEGACY_H752)
  return false;
#else
  epd_init(&epd_board_v7, &ED047TC1, EPD_LUT_64K);
  epd_set_vcom(1560);  // Preserve the physically verified LILYGO baseline.
  gDisplayState = epd_hl_init(EPD_BUILTIN_WAVEFORM);
  epd_set_rotation(EPD_ROT_INVERTED_PORTRAIT);
  initialized_ = epd_hl_get_framebuffer(&gDisplayState) != nullptr;
  return initialized_;
#endif
}

void UiController::setSnapshot(const UiSnapshot& snapshot) {
  const Page retainedPage = snapshot_.page;
  snapshot_ = snapshot;
  snapshot_.page = retainedPage;
  dirty_ = true;
}

bool UiController::requestPage(Page page, uint32_t nowMs) {
  if (page == snapshot_.page || (refreshCount_ && nowMs - lastRefreshMs_ < 2500)) return false;
  snapshot_.page = page;
  dirty_ = true;
  return true;
}

bool UiController::renderIfDirty(uint32_t nowMs, bool bootRecovery) {
  if (!initialized_ || !dirty_) return false;
  uint8_t* framebuffer = epd_hl_get_framebuffer(&gDisplayState);
  if (!framebuffer) return false;
  renderPage(framebuffer, snapshot_);  // Complete coherent page before panel power-on.
  epd_poweron();
  if (bootRecovery && !fullClearUsed_) {
    epd_fullclear(&gDisplayState, epd_ambient_temperature());
    fullClearUsed_ = true;
  }
  const EpdDrawError result = epd_hl_update_screen(&gDisplayState, MODE_GC16, epd_ambient_temperature());
  epd_poweroff();  // High voltage is shut down after every attempted update.
  lastRefreshMs_ = nowMs;
  ++refreshCount_;
  dirty_ = result != EPD_DRAW_SUCCESS;
  Serial.printf("DISPLAY render=GC16 count=%lu power=OFF result=%d\n",
                static_cast<unsigned long>(refreshCount_), static_cast<int>(result));
  return result == EPD_DRAW_SUCCESS;
}

}  // namespace ui