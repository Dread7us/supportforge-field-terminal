#include "ui_controller.h"

#include <Arduino.h>
#include <epdiy.h>
#include <esp_heap_caps.h>

#include "ui_pages.h"
#include "ui_components.h"
#include "ui_theme.h"

namespace ui {

namespace {
EpdiyHighlevelState gDisplayState;
constexpr size_t kFramebufferBytes = spec::kFramebufferStrideBytes * spec::kPhysicalHeight;
constexpr size_t kGuardBytes = 64;
constexpr uint8_t kGuardPattern = 0xA5;
static_assert(spec::kFramebufferBitsPerPixel == 4, "renderer requires packed 4-bpp EPDiy frames");
static_assert(spec::kFramebufferStrideBytes * 2 == spec::kPhysicalWidth,
              "packed framebuffer stride must contain exactly two pixels per byte");
static_assert(spec::kCanvasWidth == spec::kPhysicalHeight &&
              spec::kCanvasHeight == spec::kPhysicalWidth,
              "inverted portrait logical and physical dimensions disagree");
}

bool UiController::begin() {
#if defined(HQ_PROFILE_LEGACY_H752)
  return false;
#else
  epd_init(&epd_board_v7, &ED047TC1, EPD_LUT_64K);
  epd_set_vcom(1560);  // Preserve the physically verified LILYGO baseline.
  gDisplayState = epd_hl_init(EPD_BUILTIN_WAVEFORM);
  epd_set_rotation(EPD_ROT_INVERTED_PORTRAIT);
  guardedAllocation_ = static_cast<uint8_t*>(heap_caps_malloc(
      kFramebufferBytes + 2 * kGuardBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (!guardedAllocation_) guardedAllocation_ = static_cast<uint8_t*>(
      heap_caps_malloc(kFramebufferBytes + 2 * kGuardBytes, MALLOC_CAP_8BIT));
  if (guardedAllocation_) {
    memset(guardedAllocation_, kGuardPattern, kGuardBytes);
    compositionBuffer_ = guardedAllocation_ + kGuardBytes;
    memset(compositionBuffer_, 0xFF, kFramebufferBytes);
    memset(compositionBuffer_ + kFramebufferBytes, kGuardPattern, kGuardBytes);
  }
  initialized_ = epd_hl_get_framebuffer(&gDisplayState) != nullptr && compositionBuffer_ != nullptr;
  return initialized_;
#endif
}

void UiController::setSnapshot(const UiSnapshot& snapshot) {
  const Page retainedPage = snapshot_.page;
  if (materiallyDifferent(snapshot_, snapshot)) dirty_ = true;
  snapshot_ = snapshot;
  snapshot_.page = retainedPage;
}

bool UiController::requestPage(Page page, uint32_t nowMs) {
  if (page == snapshot_.page || inputBlocked(nowMs)) return false;
  snapshot_.page = page;
  dirty_ = true;
  return true;
}

bool UiController::inputBlocked(uint32_t nowMs) const {
  return updating_ || (refreshCount_ && nowMs-lastRefreshMs_<static_cast<uint32_t>(spec::kTouchPostRefreshQuietMs));
}

bool UiController::framebufferGuardsIntact() const {
  if (!guardedAllocation_ || !compositionBuffer_) return false;
  for (size_t index=0; index<kGuardBytes; ++index) {
    if (guardedAllocation_[index] != kGuardPattern ||
        compositionBuffer_[kFramebufferBytes + index] != kGuardPattern) return false;
  }
  return true;
}

bool UiController::renderIfDirty(uint32_t nowMs, bool bootRecovery) {
  if (!initialized_ || !dirty_) return false;
  uint8_t* framebuffer = epd_hl_get_framebuffer(&gDisplayState);
  if (!framebuffer) return false;
  updating_ = true;
  const bool cleanupThisRender = bootRecovery && !fullClearUsed_;
  bool displayPowered = false;
  if (cleanupThisRender) {
    // The high-level back buffer starts white and cannot know the panel's
    // retained pre-boot image. Synchronize physical and software white first.
    Serial.printf("DISPLAY cleanup=START page=%s\n", pageName(snapshot_.page));
    epd_poweron();
    displayPowered = true;
    epd_fullclear(&gDisplayState, epd_ambient_temperature());
    fullClearUsed_ = true;
    Serial.println("DISPLAY cleanup=COMPLETE method=LILYGO_EPD_FULLCLEAR");
  }
  // Compose off-screen from a complete known-white 960x540 packed 4-bpp frame.
  // Rotation maps the logical 540x960 coordinates; 0xFF means two white nibbles.
  memset(compositionBuffer_, 0xFF, kFramebufferBytes);
  Serial.printf("DISPLAY framebuffer=RESET_WHITE bytes=%u page=%s\n",
                static_cast<unsigned>(kFramebufferBytes), pageName(snapshot_.page));
  if (!framebufferGuardsIntact()) {
    if (displayPowered) epd_poweroff();
    Serial.println("DISPLAY canaries=CORRUPTED phase=PRE_COMPOSITION update=ABORTED");
    updating_ = false; dirty_ = true; return false;
  }
  Serial.println("DISPLAY retained_page_data=NONE white_reset=VERIFIED canaries_pre=INTACT");
  renderPage(compositionBuffer_, snapshot_);
  if (!framebufferGuardsIntact()) {
    if (displayPowered) epd_poweroff();
    Serial.println("DISPLAY canaries=CORRUPTED update=ABORTED power=OFF");
    updating_ = false;
    dirty_ = true;
    return false;
  }
  Serial.println("DISPLAY canaries=INTACT");
  memcpy(framebuffer, compositionBuffer_, kFramebufferBytes);
  if (!displayPowered) epd_poweron();
  const EpdDrawError result = epd_hl_update_screen(&gDisplayState, MODE_GC16, epd_ambient_temperature());
  epd_poweroff();  // High voltage is shut down after every attempted update.
  updating_ = false;
  lastRefreshMs_ = nowMs;
  ++refreshCount_;
  dirty_ = result != EPD_DRAW_SUCCESS;
  Serial.printf("DISPLAY render=GC16 page=%s count=%lu result=%d\n",
                pageName(snapshot_.page), static_cast<unsigned long>(refreshCount_),
                static_cast<int>(result));
  Serial.println("DISPLAY high_voltage=OFF");
  return result == EPD_DRAW_SUCCESS;
}

bool UiController::renderWhiteTest(uint32_t nowMs) {
  if (!initialized_ || whiteTestUsed_ || updating_) return false;
  whiteTestUsed_ = true;
  uint8_t* framebuffer=epd_hl_get_framebuffer(&gDisplayState);
  if(!framebuffer) return false;
  updating_=true;
  Serial.println("DISPLAY white_test=START guard=ONCE_PER_BOOT cleanup=START");
  epd_poweron();
  epd_fullclear(&gDisplayState,epd_ambient_temperature());
  memset(compositionBuffer_,0xFF,kFramebufferBytes);
  if(!framebufferGuardsIntact()){
    epd_poweroff();updating_=false;
    Serial.println("DISPLAY white_test=ABORTED canaries=CORRUPTED high_voltage=OFF");
    return false;
  }
  epd_draw_rect({4,4,kCanvasWidth-8,kCanvasHeight-8},kInk,compositionBuffer_);
  epd_draw_rect({5,5,kCanvasWidth-10,kCanvasHeight-10},kInk,compositionBuffer_);
  text(compositionBuffer_,{20,20,500,64},20,56,"WHITE TEST",FontRole::PageHeading,kInk);
  memcpy(framebuffer,compositionBuffer_,kFramebufferBytes);
  const EpdDrawError result=epd_hl_update_screen(&gDisplayState,MODE_GC16,epd_ambient_temperature());
  epd_poweroff();updating_=false;lastRefreshMs_=nowMs;++refreshCount_;
  Serial.printf("DISPLAY white_test=COMPLETE render=GC16 result=%d canaries=INTACT\n",static_cast<int>(result));
  Serial.println("DISPLAY high_voltage=OFF");
  return result==EPD_DRAW_SUCCESS;
}

bool UiController::dumpPackedFramebuffer(Page page) {
  if (!initialized_ || updating_ || !compositionBuffer_) return false;
  updating_ = true;
  memset(compositionBuffer_, 0xFF, kFramebufferBytes);
  UiSnapshot dumpSnapshot = snapshot_;
  dumpSnapshot.page = page;
  renderPage(compositionBuffer_, dumpSnapshot);
  if (!framebufferGuardsIntact()) {
    updating_ = false;
    Serial.println("FRAMEBUFFER_DUMP error=CANARY_CORRUPTION");
    return false;
  }
  Serial.printf("FRAMEBUFFER_DUMP_BEGIN page=%s bytes=%u format=EPDIY_4BPP_PACKED\n",
                pageName(page), static_cast<unsigned>(kFramebufferBytes));
  Serial.flush();
  const size_t written = Serial.write(compositionBuffer_, kFramebufferBytes);
  Serial.flush();
  Serial.printf("\nFRAMEBUFFER_DUMP_END page=%s bytes=%u written=%u\n",
                pageName(page), static_cast<unsigned>(kFramebufferBytes),
                static_cast<unsigned>(written));
  updating_ = false;
  return written == kFramebufferBytes;
}

}  // namespace ui