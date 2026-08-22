#include <Arduino.h>
#include <FastEPD.h>

#if !defined(FAST_EPD_COMPILE_ONLY)
#error "This source is restricted to the compile-only FastEPD environment."
#endif

namespace {

constexpr int kPanelWidth = 960;
constexpr int kPanelHeight = 540;

// Keep this routine in the linked image so the spike proves the official profile,
// 4-bpp drawing, and full-update APIs resolve. It is deliberately never called:
// the upstream BB_PANEL_LILYGO_T5PRO pin map conflicts with the verified current-Pro
// schematic and must not touch a physical panel without a corrected reviewed profile.
__attribute__((used, noinline)) int compileDeterministicSmokeScreen() {
  FASTEPD display;
  int result = display.initPanel(BB_PANEL_LILYGO_T5PRO);
  if (result != BBEP_SUCCESS) return result;
  result = display.setMode(BB_MODE_4BPP);
  if (result != BBEP_SUCCESS) return result;
  display.fillScreen(0x0F);
  display.fillRect(0, 0, kPanelWidth, 72, 0x00);
  display.drawRect(24, 104, kPanelWidth - 48, kPanelHeight - 128, 0x00);
  display.setTextColor(0x00);
  display.setFont(FONT_12x16);
  display.drawString("SUPPORTFORGE FASTEPD COMPILE-ONLY", 48, 136);
  display.drawString("ED047TC1 960x540 / FULL 4-BPP ONLY", 48, 168);
  return display.fullUpdate(CLEAR_SLOW, false);
}

// An address reference prevents link-time section garbage collection while still
// making hardware invocation impossible from setup().
int (*volatile kSmokeLinkReference)() = &compileDeterministicSmokeScreen;

}  // namespace

void setup() {
  (void)kSmokeLinkReference;
  Serial.begin(115200);
  Serial.println("FASTEPD COMPILE-ONLY IMAGE: HARDWARE INITIALIZATION DISABLED");
}

void loop() { delay(1000); }