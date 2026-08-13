#pragma once

#include <stdint.h>
#include "ui_state.h"

namespace ui {

class UiController {
 public:
  bool begin();
  bool requestPage(Page page, uint32_t nowMs);
  void setSnapshot(const UiSnapshot& snapshot);
  void forceDirty() { dirty_ = true; }
  bool renderIfDirty(uint32_t nowMs, bool bootRecovery = false);
  bool renderWhiteTest(uint32_t nowMs);
  bool dumpPackedFramebuffer(Page page);
  bool inputBlocked(uint32_t nowMs) const;
  bool dirty() const { return dirty_; }
  uint32_t refreshCount() const { return refreshCount_; }
  Page page() const { return snapshot_.page; }
  bool framebufferGuardsIntact() const;
  bool cleanupUsed() const { return fullClearUsed_; }
  bool whiteTestUsed() const { return whiteTestUsed_; }

 private:
  UiSnapshot snapshot_{};
  bool initialized_ = false;
  bool dirty_ = true;
  bool fullClearUsed_ = false;  // RAM guard: at most once during this boot.
  bool whiteTestUsed_ = false;  // Explicit diagnostic guard: once per boot.
  bool updating_ = false;
  uint32_t lastRefreshMs_ = 0;
  uint32_t refreshCount_ = 0;
  uint8_t* guardedAllocation_ = nullptr;
  uint8_t* compositionBuffer_ = nullptr;
};

}  // namespace ui