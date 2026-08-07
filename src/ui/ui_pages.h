#pragma once

#include <stdint.h>
#include "ui_state.h"

namespace ui {
void renderPage(uint8_t* framebuffer, const UiSnapshot& snapshot);
}