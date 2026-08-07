#include "ui_state.h"

namespace ui {

const char* pageName(Page page) {
  switch (page) {
    case Page::Home: return "HOME";
    case Page::Systems: return "SYSTEMS";
    case Page::Radio: return "RADIO";
    case Page::Location: return "LOCATION";
    case Page::Device: return "DEVICE";
    case Page::Diagnostics: return "HARDWARE DIAGNOSTICS";
  }
  return "HOME";
}

}  // namespace ui