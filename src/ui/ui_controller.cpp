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
constexpr uint32_t kManualRefreshLimitMs = 45000;
constexpr uint32_t kBalancedCleanupCooldownMs = 5UL * 60UL * 1000UL;
static_assert(spec::kFramebufferBitsPerPixel == 4, "renderer requires packed 4-bpp EPDiy frames");
static_assert(spec::kFramebufferStrideBytes * 2 == spec::kPhysicalWidth,
              "packed framebuffer stride must contain exactly two pixels per byte");
static_assert(spec::kCanvasWidth == spec::kPhysicalHeight &&
              spec::kCanvasHeight == spec::kPhysicalWidth,
              "inverted portrait logical and physical dimensions disagree");
}

bool DisplayCoordinator::begin() {
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
  if (preferences_.begin("sf_display", false)) {
    const uint8_t stored = preferences_.getUChar("refresh_mode", static_cast<uint8_t>(RefreshMode::QuickNavigation));
    refreshMode_ = stored <= static_cast<uint8_t>(RefreshMode::BeautifulClean)
        ? static_cast<RefreshMode>(stored) : RefreshMode::QuickNavigation;
  }
  Serial.printf("DISPLAY refresh_mode=%s persistence=NVS immediate=YES\n", refreshModeName(refreshMode_));
  initialized_ = epd_hl_get_framebuffer(&gDisplayState) != nullptr && compositionBuffer_ != nullptr;
  return initialized_;
#endif
}

bool DisplayCoordinator::setRefreshMode(RefreshMode mode) {
  if (mode == refreshMode_) return false;
  refreshMode_ = mode;
  preferences_.putUChar("refresh_mode", static_cast<uint8_t>(mode));
  requestRender(RenderPriority::Cosmetic);
  Serial.printf("DISPLAY refresh_mode=%s persistence=NVS applied=IMMEDIATE\n", refreshModeName(mode));
  return true;
}

void DisplayCoordinator::noteTouchAction(uint32_t actionReadyMs, uint32_t handledMs) {
  if (actionReadyMs) lastTouchToActionMs_ = handledMs - actionReadyMs;
}

void DisplayCoordinator::acceptPress(Page sourcePage, Rect bounds, const char* label,
                                     uint32_t nowMs, Page destinationPage) {
  PressFeedback feedback{};
  feedback.active = true;
  feedback.sourcePage = sourcePage;
  feedback.targetPage = destinationPage;
  feedback.bounds = bounds;
  feedback.acceptedAtMs = nowMs;
  feedback.expiresAtMs = nowMs + 2500;
  feedback.destinationRoute = destinationPage != sourcePage;
  strlcpy(feedback.label, label ? label : "ACCEPTED", sizeof(feedback.label));
  snapshot_.pressFeedback = feedback;
  requestRender(destinationPage == sourcePage ? RenderPriority::Cosmetic : RenderPriority::Navigation);
}

void DisplayCoordinator::requestRender(RenderPriority priority) {
  ++renderRequestedCount_;
  if (pendingRender_ == RenderPriority::None) pendingSinceMs_ = millis();
  if (static_cast<uint8_t>(priority) > static_cast<uint8_t>(pendingRender_)) {
    pendingRender_ = priority;
  } else ++renderCoalescedCount_;
}

void DisplayCoordinator::setSnapshot(const UiSnapshot& snapshot) {
  const Page retainedPage = snapshot_.page;
  const PressFeedback retainedFeedback = snapshot_.pressFeedback;
  if (materiallyDifferent(snapshot_, snapshot)) requestRender(RenderPriority::Cosmetic);
  snapshot_ = snapshot;
  snapshot_.page = retainedPage;
  // Service/state publication must not erase an accepted press that is still
  // waiting for its single coalesced physical render.
  if (retainedFeedback.active) snapshot_.pressFeedback = retainedFeedback;
}

bool DisplayCoordinator::requestPage(Page page, uint32_t nowMs) {
  if (page == snapshot_.page || inputBlocked(nowMs)) return false;
  snapshot_.page = page;
  navigationRequestedMs_ = nowMs;
  requestRender(RenderPriority::Navigation);
  return true;
}

bool DisplayCoordinator::inputBlocked(uint32_t nowMs) const {
  return updating_ || (refreshCount_ && nowMs-lastRefreshMs_<static_cast<uint32_t>(spec::kTouchPostRefreshQuietMs));
}

bool DisplayCoordinator::manualRefreshAvailable(uint32_t nowMs) const {
  return !updating_ && (!lastManualRefreshMs_ ||
      nowMs - lastManualRefreshMs_ >= kManualRefreshLimitMs);
}

void DisplayCoordinator::noteCleanupStarted() {
  fullClearUsed_ = true;
}

uint32_t DisplayCoordinator::manualRefreshRemainingSeconds(uint32_t nowMs) const {
  if (manualRefreshAvailable(nowMs)) return 0;
  const uint32_t elapsed = nowMs - lastManualRefreshMs_;
  return elapsed >= kManualRefreshLimitMs ? 0 :
      (kManualRefreshLimitMs - elapsed + 999) / 1000;
}

bool DisplayCoordinator::framebufferGuardsIntact() const {
  if (!guardedAllocation_ || !compositionBuffer_) return false;
  for (size_t index=0; index<kGuardBytes; ++index) {
    if (guardedAllocation_[index] != kGuardPattern ||
        compositionBuffer_[kFramebufferBytes + index] != kGuardPattern) return false;
  }
  return true;
}

bool DisplayCoordinator::renderIfDirty(uint32_t nowMs, bool bootRecovery) {
  if (!initialized_ || pendingRender_ == RenderPriority::None) return false;
  uint8_t* framebuffer = epd_hl_get_framebuffer(&gDisplayState);
  if (!framebuffer) return false;
  const RenderPriority renderingPriority = pendingRender_;
  const uint32_t renderStartedMs = millis();
  lastRenderWaitMs_ = pendingSinceMs_ ? renderStartedMs - pendingSinceMs_ : 0;
  pendingRender_ = RenderPriority::None;
  updating_ = true;
  const bool firstUsableFrame = !hasPresentedPage_;
  const bool fullPageTransition = renderingPriority == RenderPriority::Navigation &&
      hasPresentedPage_ && lastPresentedPage_ != snapshot_.page;
  const bool bootCleanup = bootRecovery && firstUsableFrame && !fullClearUsed_;
  // A failed physical GC16 update is the only automatic fault-recovery cause.
  // It is consumed by one retry and is never converted into a cleanup queue.
  const bool faultRecoveryCleanup = failedUpdateNeedsRecoveryCleanup_;
  const bool balancedCleanupAvailable = !automaticCleanupUsed_ ||
      nowMs - lastAutomaticCleanupMs_ >= kBalancedCleanupCooldownMs;
  const bool modeNavigationCleanup = fullPageTransition && !failedUpdateRetryUsed_ &&
      (refreshMode_ == RefreshMode::BeautifulClean ||
       (refreshMode_ == RefreshMode::Balanced && balancedCleanupAvailable));
  // QUICK navigation is always one full-frame GC16 operation. BALANCED cleanup
  // is bounded by a cooldown; BEAUTIFUL performs at most one cleanup for an
  // actual page replacement. Cosmetic updates cannot satisfy any predicate.
  const bool cleanupThisRender = bootCleanup || faultRecoveryCleanup || modeNavigationCleanup;
  bool displayPowered = false;
  if (cleanupThisRender) {
    // The high-level back buffer starts white and cannot know the panel's
    // retained pre-boot image. Synchronize physical and software white first.
    Serial.printf("DISPLAY cleanup=START page=%s\n", pageName(snapshot_.page));
    epd_poweron();
    displayPowered = true;
    const uint32_t cleanupStartedMs = millis();
    noteCleanupStarted();
    epd_fullclear(&gDisplayState, epd_ambient_temperature());
    if (modeNavigationCleanup) {
      automaticCleanupUsed_ = true;
      lastAutomaticCleanupMs_ = nowMs;
    }
    failedUpdateNeedsRecoveryCleanup_ = false;
    lastFullCleanupDurationMs_ = millis() - cleanupStartedMs;
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
    updating_ = false; requestRender(renderingPriority); return false;
  }
  Serial.println("DISPLAY retained_page_data=NONE white_reset=VERIFIED canaries_pre=INTACT");
  renderPage(compositionBuffer_, snapshot_);
  if (!framebufferGuardsIntact()) {
    if (displayPowered) epd_poweroff();
    Serial.println("DISPLAY canaries=CORRUPTED update=ABORTED power=OFF");
    updating_ = false;
    requestRender(renderingPriority);
    return false;
  }
  Serial.println("DISPLAY canaries=INTACT");
  if (!displayPowered) epd_poweron();
  memcpy(framebuffer, compositionBuffer_, kFramebufferBytes);
  const uint32_t gc16StartedMs = millis();
  const EpdDrawError contentResult = epd_hl_update_screen(
      &gDisplayState, MODE_GC16, epd_ambient_temperature());
  lastGc16DurationMs_ = millis() - gc16StartedMs;
  const EpdDrawError result = contentResult;
  epd_poweroff();  // High voltage is shut down after every attempted update.
  updating_ = false;
  lastRefreshMs_ = millis();
  ++refreshCount_;
  ++renderRenderedCount_;
  lastRenderDurationMs_ = millis() - renderStartedMs;
  if (renderingPriority == RenderPriority::Navigation && navigationRequestedMs_) {
    lastNavigationLatencyMs_ = millis() - navigationRequestedMs_;
    lastPageTransitionDurationMs_ = lastNavigationLatencyMs_;
    navigationRequestedMs_ = 0;
  }
  if (result == EPD_DRAW_SUCCESS) {
    // Consumption never schedules a refresh. The next material composition starts
    // from white and therefore clears the old accepted-control bounds.
    snapshot_.pressFeedback.active = false;
    lastPresentedPage_ = snapshot_.page;
    hasPresentedPage_ = true;
    failedUpdateRetryUsed_ = false;
    failedUpdateNeedsRecoveryCleanup_ = false;
  } else if (!failedUpdateRetryUsed_) {
    failedUpdateRetryUsed_ = true;
    // If this attempt already physically cleaned, another cleanup would chain
    // slow cycles without adding recovery value. Otherwise the one retry gets
    // the narrowly guarded fault-recovery cleanup.
    failedUpdateNeedsRecoveryCleanup_ = !cleanupThisRender;
    requestRender(renderingPriority);
    Serial.println("DISPLAY retry=QUEUED limit=ONE");
  } else {
    Serial.println("DISPLAY retry=SUPPRESSED reason=REPEATED_UPDATE_FAILURE");
  }
  Serial.printf("DISPLAY render=GC16 page=%s mode=%s physical_cleanup=%s gc16_ms=%lu cleanup_ms=%lu transition_ms=%lu count=%lu result=%d\n",
                pageName(snapshot_.page), refreshModeName(refreshMode_), cleanupThisRender?"YES":"NO",
                static_cast<unsigned long>(lastGc16DurationMs_),
                static_cast<unsigned long>(cleanupThisRender?lastFullCleanupDurationMs_:0),
                static_cast<unsigned long>(lastPageTransitionDurationMs_),
                static_cast<unsigned long>(refreshCount_),
                static_cast<int>(result));
  Serial.println("DISPLAY high_voltage=OFF");
  return result == EPD_DRAW_SUCCESS;
}

void DisplayCoordinator::printPerformance()const{Serial.printf("PERF refresh_mode=%s render_requested=%lu render_rendered=%lu render_coalesced=%lu touch_to_action_software_ms=%lu render_wait_ms=%lu gc16_duration_ms=%lu full_cleanup_duration_ms=%lu page_transition_duration_ms=%lu navigation_latency_ms=%lu render_queue_hwm=1 heap_free=%u heap_min=%u psram_free=%u\n",refreshModeName(refreshMode_),static_cast<unsigned long>(renderRequestedCount_),static_cast<unsigned long>(renderRenderedCount_),static_cast<unsigned long>(renderCoalescedCount_),static_cast<unsigned long>(lastTouchToActionMs_),static_cast<unsigned long>(lastRenderWaitMs_),static_cast<unsigned long>(lastGc16DurationMs_),static_cast<unsigned long>(lastFullCleanupDurationMs_),static_cast<unsigned long>(lastPageTransitionDurationMs_),static_cast<unsigned long>(lastNavigationLatencyMs_),ESP.getFreeHeap(),ESP.getMinFreeHeap(),ESP.getFreePsram());}

bool DisplayCoordinator::renderWhiteTest(uint32_t nowMs) {
  if (!initialized_ || whiteTestUsed_ || updating_) return false;
  whiteTestUsed_ = true;
  uint8_t* framebuffer=epd_hl_get_framebuffer(&gDisplayState);
  if(!framebuffer) return false;
  updating_=true;
  Serial.println("DISPLAY white_test=START guard=ONCE_PER_BOOT cleanup=START");
  epd_poweron();
  const uint32_t cleanupStartedMs=millis();
  noteCleanupStarted();
  epd_fullclear(&gDisplayState,epd_ambient_temperature());
  fullClearUsed_=true;
  lastFullCleanupDurationMs_=millis()-cleanupStartedMs;
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
  const uint32_t gc16StartedMs=millis();
  const EpdDrawError result=epd_hl_update_screen(&gDisplayState,MODE_GC16,epd_ambient_temperature());
  lastGc16DurationMs_=millis()-gc16StartedMs;
  epd_poweroff();updating_=false;lastRefreshMs_=millis();++refreshCount_;
  Serial.printf("DISPLAY white_test=COMPLETE render=GC16 result=%d canaries=INTACT\n",static_cast<int>(result));
  Serial.println("DISPLAY high_voltage=OFF");
  return result==EPD_DRAW_SUCCESS;
}

bool DisplayCoordinator::manualFullRefresh(uint32_t nowMs, Page returnPage) {
  if (!initialized_ || !manualRefreshAvailable(nowMs) || !compositionBuffer_) return false;
  uint8_t* framebuffer=epd_hl_get_framebuffer(&gDisplayState);
  if(!framebuffer)return false;
  updating_=true;
  // This operation composes and presents the destination itself. Consume any
  // older cosmetic demand so it cannot redraw stale UI afterward.
  pendingRender_=RenderPriority::None;
  pendingSinceMs_=0;
  navigationRequestedMs_=0;
  const Page previousPage=snapshot_.page;
  Serial.printf("DISPLAY cleanup=ACKNOWLEDGED message=CLEANING_DISPLAY return_page=%s\n",
                pageName(returnPage));
  epd_poweron();
  // Start the cooldown only after the panel cleanup has physically begun. A tap
  // rejected before this point remains eligible instead of consuming the limit.
  lastManualRefreshMs_=millis();
  const uint32_t cleanupStartedMs=millis();
  noteCleanupStarted();
  epd_fullclear(&gDisplayState,epd_ambient_temperature());
  fullClearUsed_=true;
  lastFullCleanupDurationMs_=millis()-cleanupStartedMs;
  memset(compositionBuffer_,0xFF,kFramebufferBytes);
  UiSnapshot returnSnapshot=snapshot_;
  returnSnapshot.page=returnPage;
  // The restored Device page must immediately describe the newly active limit.
  // Keep this static until another legitimate redraw; a one-second EPD timer
  // would waste power and create avoidable panel updates.
  returnSnapshot.manualRefreshRateLimited=true;
  returnSnapshot.manualRefreshRemainingSeconds=kManualRefreshLimitMs/1000;
  renderPage(compositionBuffer_,returnSnapshot);
  if(!framebufferGuardsIntact()){
    epd_poweroff();updating_=false;
    Serial.println("DISPLAY cleanup=ABORTED canaries=CORRUPTED high_voltage=OFF");
    return false;
  }
  memcpy(framebuffer,compositionBuffer_,kFramebufferBytes);
  const uint32_t gc16StartedMs=millis();
  const EpdDrawError result=epd_hl_update_screen(&gDisplayState,MODE_GC16,epd_ambient_temperature());
  lastGc16DurationMs_=millis()-gc16StartedMs;
  epd_poweroff();updating_=false;lastRefreshMs_=millis();++refreshCount_;++renderRenderedCount_;
  lastRenderDurationMs_=lastFullCleanupDurationMs_+lastGc16DurationMs_;
  if(result==EPD_DRAW_SUCCESS){snapshot_=returnSnapshot;snapshot_.pressFeedback.active=false;lastPresentedPage_=returnPage;hasPresentedPage_=true;failedUpdateRetryUsed_=false;}
  else{snapshot_.page=previousPage;requestRender(RenderPriority::Navigation);}
  Serial.printf("DISPLAY cleanup=COMPLETE method=FULLCLEAR render=GC16 page=%s result=%d state=PRESERVED gc16_ms=%lu cleanup_ms=%lu\n",
                pageName(returnPage),static_cast<int>(result),static_cast<unsigned long>(lastGc16DurationMs_),static_cast<unsigned long>(lastFullCleanupDurationMs_));
  Serial.println("DISPLAY high_voltage=OFF");
  return result==EPD_DRAW_SUCCESS;
}

bool DisplayCoordinator::dumpPackedFramebuffer(Page page) {
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