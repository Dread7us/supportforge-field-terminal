#pragma once

#include <stdint.h>
#include "ui_state.h"

namespace ui {

class UiController {
 public:
  bool begin();
  bool requestPage(Page page, uint32_t nowMs);
  void setSnapshot(const UiSnapshot& snapshot);
  bool renderIfDirty(uint32_t nowMs, bool bootRecovery = false);
  bool dirty() const { return dirty_; }
  uint32_t refreshCount() const { return refreshCount_; }
  Page page() const { return snapshot_.page; }

 private:
  UiSnapshot snapshot_{};
  bool initialized_ = false;
  bool dirty_ = true;
  bool fullClearUsed_ = false;
  uint32_t lastRefreshMs_ = 0;
  uint32_t refreshCount_ = 0;
};

}  // namespace ui