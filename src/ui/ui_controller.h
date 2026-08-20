#pragma once

#include <Preferences.h>
#include <stdint.h>
#include "ui_state.h"

namespace ui {

enum class RenderPriority : uint8_t { None, Cosmetic, Navigation };

// The sole owner of physical display lifecycle, framebuffer composition, and
// bounded render demand. Publishers can only replace snapshots or raise this
// two-level latch; requests never accumulate in an unbounded queue.
class DisplayCoordinator {
 public:
  bool begin();
  bool requestPage(Page page, uint32_t nowMs);
  void setSnapshot(const UiSnapshot& snapshot);
  void requestRender(RenderPriority priority = RenderPriority::Cosmetic);
  void forceDirty() { requestRender(RenderPriority::Cosmetic); }
  bool renderIfDirty(uint32_t nowMs, bool bootRecovery = false);
  bool renderWhiteTest(uint32_t nowMs);
  bool manualFullRefresh(uint32_t nowMs, Page returnPage);
  bool dumpPackedFramebuffer(Page page);
  bool inputBlocked(uint32_t nowMs) const;
  bool dirty() const { return pendingRender_ != RenderPriority::None; }
  RenderPriority pendingPriority() const { return pendingRender_; }
  uint32_t refreshCount() const { return refreshCount_; }
  Page page() const { return snapshot_.page; }
  bool framebufferGuardsIntact() const;
  bool cleanupUsed() const { return fullClearUsed_; }
  bool whiteTestUsed() const { return whiteTestUsed_; }
  bool manualRefreshAvailable(uint32_t nowMs) const;
  uint32_t manualRefreshRemainingSeconds(uint32_t nowMs) const;
  RefreshMode refreshMode() const { return refreshMode_; }
  bool setRefreshMode(RefreshMode mode);
  void noteTouchAction(uint32_t actionReadyMs, uint32_t handledMs);
  uint32_t lastGc16DurationMs() const { return lastGc16DurationMs_; }
  uint32_t lastFullCleanupDurationMs() const { return lastFullCleanupDurationMs_; }
  uint32_t lastPageTransitionDurationMs() const { return lastPageTransitionDurationMs_; }
  uint32_t lastTouchToActionMs() const { return lastTouchToActionMs_; }
  uint32_t coalescedRenderCount() const { return renderCoalescedCount_; }
  void printPerformance() const;

 private:
  UiSnapshot snapshot_{};
  bool initialized_ = false;
  RenderPriority pendingRender_ = RenderPriority::Cosmetic;
  bool fullClearUsed_ = false;  // Records whether this boot has performed any cleanup.
  bool whiteTestUsed_ = false;  // Explicit diagnostic guard: once per boot.
  bool updating_ = false;
  uint32_t lastRefreshMs_ = 0;
  uint32_t refreshCount_ = 0;
  uint32_t lastManualRefreshMs_ = 0;
  bool failedUpdateRetryUsed_ = false;
  uint32_t renderRequestedCount_ = 0, renderRenderedCount_ = 0;
  uint32_t renderCoalescedCount_ = 0, lastRenderDurationMs_ = 0;
  uint32_t pendingSinceMs_ = 0, lastRenderWaitMs_ = 0;
  uint32_t navigationRequestedMs_ = 0, lastNavigationLatencyMs_ = 0;
  RefreshMode refreshMode_ = RefreshMode::Balanced;
  uint32_t lastGc16DurationMs_ = 0, lastFullCleanupDurationMs_ = 0;
  uint32_t lastPageTransitionDurationMs_ = 0, lastTouchToActionMs_ = 0;
  Preferences preferences_;
  uint8_t* guardedAllocation_ = nullptr;
  uint8_t* compositionBuffer_ = nullptr;
};

}  // namespace ui