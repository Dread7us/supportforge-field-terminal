#include "ui_components.h"

#include <epdiy.h>

namespace ui {
namespace {

const fonts::Font& fontFor(FontRole role) {
  switch (role) {
    case FontRole::Caption: return fonts::kCaption;
    case FontRole::Body: return fonts::kBody;
    case FontRole::CardHeading: return fonts::kCardHeading;
    case FontRole::PageHeading: return fonts::kPageHeading;
    case FontRole::Brand: return fonts::kBrand;
    case FontRole::Metric: return fonts::kMetric;
    case FontRole::HomeClock: return fonts::kHomeClock;
    case FontRole::VehicleSpeed: return fonts::kVehicleSpeed;
    case FontRole::AltimeterMetric: return fonts::kAltimeterMetric;
    case FontRole::Navigation: return fonts::kNavigation;
    case FontRole::QualificationCurrent: return fonts::kQualificationCurrent;
    case FontRole::QualificationRegularAa: return fonts::kQualificationRegularAa;
    case FontRole::QualificationMediumAa: return fonts::kQualificationMediumAa;
    case FontRole::QualificationSemiboldAa: return fonts::kQualificationSemiboldAa;
    case FontRole::QualificationBoldMono: return fonts::kQualificationBoldMono;
  }
  return fonts::kBody;
}

void line(uint8_t* fb, int x1, int y1, int x2, int y2, uint8_t color) {
  // Two-pixel strokes remain legible after GC16 refresh and give every icon the
  // same optical weight as the embedded Inter SemiBold labels.
  epd_draw_line(x1, y1, x2, y2, color, fb);
  if (abs(x2 - x1) >= abs(y2 - y1)) epd_draw_line(x1, y1 + 1, x2, y2 + 1, color, fb);
  else epd_draw_line(x1 + 1, y1, x2 + 1, y2, color, fb);
}

int glyphIndex(char raw) {
  const unsigned char c = static_cast<unsigned char>(raw);
  return c >= 32 && c <= 126 ? c - 32 : '?' - 32;
}

bool inClip(const Rect& clip, int x, int y) {
  return x >= clip.x && y >= clip.y && x < clip.x + clip.w && y < clip.y + clip.h;
}

}  // namespace

void clear(uint8_t* fb) { epd_fill_rect({0, 0, kCanvasWidth, kCanvasHeight}, kPaper, fb); }

void blankRegion(uint8_t* fb, Rect bounds) {
  const int left = max(0, bounds.x);
  const int top = max(0, bounds.y);
  const int right = min(kCanvasWidth, bounds.x + bounds.w);
  const int bottom = min(kCanvasHeight, bounds.y + bounds.h);
  if (right > left && bottom > top) {
    epd_fill_rect({left, top, right - left, bottom - top}, kPaper, fb);
  }
}

void roundedRect(uint8_t* fb, Rect r, int radius, uint8_t fill, uint8_t stroke) {
  radius = min(radius, min(r.w, r.h) / 2);
  epd_fill_rect({r.x + radius, r.y, r.w - 2 * radius, r.h}, fill, fb);
  epd_fill_rect({r.x, r.y + radius, r.w, r.h - 2 * radius}, fill, fb);
  epd_fill_circle(r.x + radius, r.y + radius, radius, fill, fb);
  epd_fill_circle(r.x + r.w - radius - 1, r.y + radius, radius, fill, fb);
  epd_fill_circle(r.x + radius, r.y + r.h - radius - 1, radius, fill, fb);
  epd_fill_circle(r.x + r.w - radius - 1, r.y + r.h - radius - 1, radius, fill, fb);
  if (stroke != fill) {
    // Two logical pixels map to at least two physical panel pixels in portrait.
    epd_draw_rect({r.x, r.y, r.w, r.h}, stroke, fb);
    if (r.w > 2 && r.h > 2) epd_draw_rect({r.x + 1, r.y + 1, r.w - 2, r.h - 2}, stroke, fb);
  }
}

int textWidth(const String& value, FontRole role) {
  const fonts::Font& font = fontFor(role);
  int width = 0;
  for (char c : value) width += font.glyphs[glyphIndex(c)].advance;
  return width;
}

int textHeight(FontRole role) { return fontFor(role).lineHeight; }

bool textFits(const String& value, FontRole role, Rect region) {
  return textWidth(value, role) <= region.w && textHeight(role) <= region.h;
}

String fittedText(const String& value, FontRole role, int width) {
  if (textWidth(value, role) <= width) return value;
  String result = value;
  while (result.length() && textWidth(result + "...", role) > width) {
    result.remove(result.length() - 1);
  }
  return result.length() ? result + "..." : String("--");
}

int centeredBaseline(Rect bounds, FontRole role) {
  const fonts::Font& font = fontFor(role);
  return bounds.y + (bounds.h + font.ascent - font.descent) / 2;
}

void actionButton(uint8_t* fb, Rect bounds, const String& label, bool selected,
                  Icon glyph, bool backLayout) {
  constexpr int kHorizontalPadding = 16;
  constexpr int kVerticalPadding = 10;
  static_assert(kVerticalPadding >= 8, "action labels require safe vertical padding");
  // Buttons are dynamic regions: erase the complete visual bounds first so a
  // shorter replacement label or inverse-state transition cannot retain ink.
  blankRegion(fb, bounds);
  roundedRect(fb, bounds, kRadiusControl, selected ? kInk : kPaper, kInk);
  const Rect content{bounds.x + kHorizontalPadding, bounds.y + kVerticalPadding,
                     bounds.w - 2 * kHorizontalPadding,
                     bounds.h - 2 * kVerticalPadding};
  // A compact character/operator label is itself the approved key graphic. All
  // titled controls receive a visible leading monochrome icon in a fixed column.
  const bool compactKey = bounds.w <= 120 && label.length() <= 3;
  const int iconColumn = compactKey ? 0 : min(kIconBoxSize + kIconLabelGap, content.w / 4);
  const Rect labelContent{content.x + iconColumn, content.y,
                          content.w - iconColumn, content.h};
  const FontRole role = textFits(label, FontRole::CardHeading, content)
                            ? FontRole::CardHeading
                            : FontRole::Body;
  const String shown = fittedText(label, role, labelContent.w);
  const int x = labelContent.x + (labelContent.w - textWidth(shown, role)) / 2;
  // Center the font's ascent/descent box inside the same inset rectangle used
  // for clipping. The former implementation centered against the outer button
  // but clipped against an inset rectangle, which could remove descenders or
  // the top row after generated-font metric changes.
  if (!compactKey) {
    const uint8_t foreground = selected ? kPaper : kInk;
    icon(fb, glyph, content.x + kIconBoxSize / 2,
         content.y + content.h / 2, 20, foreground);
  }
  const int labelX = backLayout ? labelContent.x : x;
  text(fb, labelContent, labelX, centeredBaseline(content, role), shown, role,
       selected ? kPaper : kInk);
}

void text(uint8_t* fb, Rect clip, int x, int baseline, const String& value, FontRole role, uint8_t color) {
  const fonts::Font& font = fontFor(role);
  for (char c : value) {
    const fonts::Glyph& glyph = font.glyphs[glyphIndex(c)];
    const int gx = x + glyph.xOffset;
    const int gy = baseline + glyph.yOffset;
    const int pixels = glyph.width * glyph.height;
    for (int pixel = 0; pixel < pixels; ++pixel) {
      const uint8_t packed = font.bitmap[glyph.offset + pixel / 2];
      const uint8_t coverage = pixel % 2 ? packed >> 4 : packed & 0x0F;
      if (coverage != 0) {
        const int px = gx + pixel % glyph.width;
        const int py = gy + pixel / glyph.width;
        // Coverage is applied once in nibble space. A black foreground maps
        // coverage 15 to exact framebuffer nibble 0; white foreground inverts.
        const uint8_t foreground = color >> 4;
        const uint8_t paper = foreground == 0 ? 15 : 0;
        // Keep both products non-negative. The previous signed-delta form
        // rounded a fully covered black pixel to nibble 1 because C++ integer
        // division truncates negative values toward zero.
        const uint8_t nibble = static_cast<uint8_t>(
            (paper * (15 - coverage) + foreground * coverage + 7) / 15);
        if (inClip(clip, px, py)) epd_draw_pixel(px, py, nibble * 0x11, fb);
      }
    }
    x += glyph.advance;
    if (x >= clip.x + clip.w) break;
  }
}

void icon(uint8_t* fb, Icon value, int cx, int cy, int s, uint8_t color) {
  const int h = s / 2;
  switch (value) {
    case Icon::Home:
      line(fb, cx-h, cy, cx, cy-h, color); line(fb, cx, cy-h, cx+h, cy, color);
      epd_draw_rect({cx-h+3, cy, s-6, h}, color, fb); break;
    case Icon::Systems:
      for (int i=-1;i<=1;++i) epd_draw_rect({cx-h, cy+i*7-2, s, 5}, color, fb); break;
    case Icon::Radio:
      epd_fill_circle(cx, cy+5, 3, color, fb);
      line(fb,cx,cy+2,cx,cy-h,color); epd_draw_circle(cx,cy+3,h/2,color,fb); epd_draw_circle(cx,cy+3,h,color,fb); break;
    case Icon::Location:
      epd_draw_circle(cx,cy-h/3,h/2,color,fb); line(fb,cx-h/2,cy,cx,cy+h,color); line(fb,cx,cy+h,cx+h/2,cy,color); break;
    case Icon::Device:
      // A single outline keeps DEVICE at the same optical weight as the other
      // navigation glyphs. The card helper intentionally uses a two-pixel
      // border and also filled the active inverse icon's interior.
      epd_draw_rect({cx-h,cy-h,s,s},color,fb); epd_fill_circle(cx,cy,3,color,fb); break;
    case Icon::Battery:
      epd_draw_rect({cx-h,cy-h/2,s-3,h},color,fb); epd_fill_rect({cx+h-2,cy-3,3,6},color,fb); break;
    case Icon::Wifi:
      epd_draw_circle(cx,cy+8,h,color,fb); epd_draw_circle(cx,cy+8,h*2/3,color,fb);
      epd_draw_circle(cx,cy+8,h/3,color,fb); epd_fill_circle(cx,cy+8,3,color,fb); break;
    case Icon::Lock:
      epd_draw_rect({cx-h/2,cy,s/2,h/2},color,fb); epd_draw_circle(cx,cy,h/2,color,fb); break;
    case Icon::Check:
      line(fb,cx-h,cy,cx-2,cy+h,color); line(fb,cx-2,cy+h,cx+h,cy-h,color); break;
    case Icon::Info:
      epd_draw_circle(cx,cy,h,color,fb); line(fb,cx,cy-1,cx,cy+h/2,color); epd_fill_circle(cx,cy-h/2,2,color,fb); break;
    case Icon::ChevronLeft:
      line(fb,cx+h/3,cy-h,cx-h/2,cy,color); line(fb,cx-h/2,cy,cx+h/3,cy+h,color); break;
    case Icon::ChevronRight:
      line(fb,cx-h/3,cy-h,cx+h/2,cy,color); line(fb,cx+h/2,cy,cx-h/3,cy+h,color); break;
    case Icon::Refresh:
      epd_draw_circle(cx,cy,h-2,color,fb); line(fb,cx+h-1,cy-h+1,cx+h-1,cy-2,color);
      line(fb,cx+h-1,cy-h+1,cx+2,cy-h+1,color); break;
    case Icon::Settings:
      epd_draw_circle(cx,cy,h-2,color,fb); epd_fill_circle(cx,cy,3,color,fb);
      epd_draw_hline(cx-h-3,cy,5,color,fb); epd_draw_hline(cx+h-2,cy,5,color,fb);
      epd_draw_vline(cx,cy-h-3,5,color,fb); epd_draw_vline(cx,cy+h-2,5,color,fb); break;
    case Icon::Power:
      epd_draw_circle(cx,cy+2,h-2,color,fb);
      epd_fill_rect({cx-2,cy-h-2,5,h+2},color==kPaper?kInk:kPaper,fb);
      line(fb,cx,cy-h,cx,cy+1,color); break;
    case Icon::Search:
      epd_draw_circle(cx-2,cy-2,h-3,color,fb); line(fb,cx+h/3,cy+h/3,cx+h,cy+h,color); break;
    case Icon::Close:
      line(fb,cx-h,cy-h,cx+h,cy+h,color); line(fb,cx+h,cy-h,cx-h,cy+h,color); break;
    case Icon::Clock:
      epd_draw_circle(cx,cy,h,color,fb); line(fb,cx,cy,cx,cy-h+3,color); line(fb,cx,cy,cx+h-3,cy,color); break;
    case Icon::Calculator:
      epd_draw_rect({cx-h,cy-h,s,s},color,fb); epd_draw_hline(cx-h+3,cy-h+6,s-6,color,fb);
      epd_fill_circle(cx-5,cy+3,2,color,fb); epd_fill_circle(cx+5,cy+3,2,color,fb); break;
    case Icon::Display:
      epd_draw_rect({cx-h,cy-h+2,s,s-5},color,fb); line(fb,cx-5,cy+h,cx+5,cy+h,color); break;
    case Icon::Weather:
      epd_draw_circle(cx-4,cy-3,h/2,color,fb); epd_fill_circle(cx-4,cy-3,h/3,color,fb);
      line(fb,cx-8,cy+h/2,cx+h,cy+h/2,color); break;
    case Icon::Cleanup:
      epd_draw_rect({cx-h,cy-h/2,s,h},color,fb); line(fb,cx-h,cy+h,cx+h,cy-h,color); break;
    case Icon::Diagnostics:
      epd_draw_circle(cx,cy,h,color,fb); line(fb,cx-h,cy,cx+h,cy,color); line(fb,cx,cy-h,cx,cy+h,color); break;
    case Icon::Keyboard:
      epd_draw_rect({cx-h,cy-h/2,s,h},color,fb); for(int x=cx-h+4;x<cx+h-2;x+=5)epd_fill_rect({x,cy-3,2,2},color,fb); break;
    case Icon::Delete:
      line(fb,cx-h,cy,cx-h/2,cy-h/2,color); line(fb,cx-h/2,cy-h/2,cx+h,cy-h/2,color);
      line(fb,cx+h,cy-h/2,cx+h,cy+h/2,color); line(fb,cx+h,cy+h/2,cx-h/2,cy+h/2,color); break;
    case Icon::Light:
      epd_draw_circle(cx,cy,h/2,color,fb); for(int i=-1;i<=1;i+=2){line(fb,cx+i*h,cy,cx+i*(h-4),cy,color);line(fb,cx,cy+i*h,cx,cy+i*(h-4),color);} break;
    case Icon::Units:
      line(fb,cx-h,cy+h/2,cx+h,cy+h/2,color); for(int x=cx-h;x<=cx+h;x+=h/2)line(fb,x,cy+h/2,x,cy,color); break;
    case Icon::Privacy:
      epd_draw_circle(cx,cy,h-2,color,fb); epd_fill_circle(cx,cy,3,color,fb); line(fb,cx-h,cy+h,cx+h,cy-h,color); break;
    case Icon::Touch:
      epd_draw_circle(cx,cy-h/3,h/3,color,fb); line(fb,cx,cy,cx,cy+h,color); line(fb,cx,cy+h,cx+h,cy+h/2,color); break;
    case Icon::Next:
      line(fb,cx-h,cy-h,cx,cy,color); line(fb,cx,cy,cx-h,cy+h,color); line(fb,cx+2,cy-h,cx+h,cy,color); line(fb,cx+h,cy,cx+2,cy+h,color); break;
    case Icon::Save:
      epd_draw_rect({cx-h,cy-h,s,s},color,fb); epd_draw_rect({cx-h+4,cy-h+3,s-8,h/2},color,fb); epd_fill_circle(cx,cy+h/2,3,color,fb); break;
  }
}

void selectableCard(uint8_t* fb, Rect b, const String& title,
                    const String& detail, const String& secondary,
                    bool selected, Icon glyph) {
  const uint8_t fg=selected?kPaper:kInk;
  roundedRect(fb,b,12,selected?kInk:kPaper,kInk);
  icon(fb,glyph,b.x+34,b.y+34,24,fg);
  const Rect titleBounds{b.x+62,b.y+12,b.w-82,34};
  text(fb,titleBounds,titleBounds.x,centeredBaseline(titleBounds,FontRole::CardHeading),
       fittedText(title,FontRole::CardHeading,titleBounds.w),FontRole::CardHeading,fg);
  text(fb,{b.x+20,b.y+56,b.w-40,28},b.x+20,b.y+78,
       fittedText(detail,FontRole::Body,b.w-40),FontRole::Body,fg);
  text(fb,{b.x+20,b.y+88,b.w-40,28},b.x+20,b.y+110,
       fittedText(secondary,FontRole::Caption,b.w-40),FontRole::Caption,fg);
}

BatteryIconGeometry batteryIconGeometry(Rect bounds, bool charging) {
  const int terminalWidth = max(3, min(6, bounds.w / 12));
  const int terminalGap = 1;
  const int outlineInset = bounds.h >= 48 ? 4 : 3;
  const Rect body{bounds.x, bounds.y, bounds.w - terminalWidth - terminalGap, bounds.h};
  const Rect interior{body.x + outlineInset, body.y + outlineInset,
                      body.w - 2 * outlineInset, body.h - 2 * outlineInset};
  const Rect terminal{body.x + body.w + terminalGap,
                      body.y + body.h / 3, terminalWidth, max(3, body.h / 3)};
  // Reserve only a tiny corner for the static charge mark. This is not a
  // separate status lane: measured Caption text still has 45 px for "100%".
  const int chargingMarkWidth = charging ? 7 : 0;
  const Rect chargingMark{interior.x + interior.w - chargingMarkWidth,
                          interior.y + interior.h - 8, chargingMarkWidth, 8};
  const Rect labelLane{interior.x, interior.y,
                       interior.w - chargingMarkWidth, interior.h};
  BatteryIconGeometry geometry;
  geometry.bounds = bounds;
  geometry.body = body;
  geometry.interior = interior;
  geometry.label = labelLane;
  geometry.chargingMark = chargingMark;
  geometry.terminal = terminal;
  return geometry;
}

int batteryFillWidth(const BatteryIconGeometry& geometry,
                     const battery::BatteryVisualModel& model) {
  if (!model.percentAvailable || !battery::validPercent(model.percent)) return 0;
  return (geometry.interior.w * model.percent + 50) / 100;
}

void batteryIcon(uint8_t* fb, Rect bounds, const battery::BatteryVisualModel& model,
                 uint8_t color) {
  // Always erase the complete maximum visual extent, including cap and all text,
  // before drawing. This covers 100% -> 8%, charging -> idle, and valid -> --.
  blankRegion(fb, bounds);
  const BatteryIconGeometry geometry = batteryIconGeometry(bounds, model.charging);
  const Rect& body = geometry.body;
  const Rect& interior = geometry.interior;
  const Rect& terminal = geometry.terminal;
  epd_fill_rect({body.x, body.y, body.w, body.h}, kPaper, fb);
  epd_draw_rect({body.x, body.y, body.w, body.h}, color, fb);
  epd_draw_rect({body.x + 1, body.y + 1, body.w - 2, body.h - 2}, color, fb);
  epd_fill_rect({terminal.x, terminal.y, terminal.w, terminal.h}, color, fb);
  if (!model.percentAvailable || !battery::validPercent(model.percent)) {
    const String unknown = "--";
    const int x = geometry.label.x +
        (geometry.label.w - textWidth(unknown, FontRole::Caption)) / 2;
    text(fb, geometry.label, x, centeredBaseline(geometry.label, FontRole::Caption),
         unknown, FontRole::Caption, kInk);
    return;
  }
  const uint8_t bounded = model.percent;
  const int fillWidth = batteryFillWidth(geometry, model);
  const Rect filled{interior.x, interior.y, fillWidth, interior.h};
  const Rect unfilled{interior.x + fillWidth, interior.y,
                      interior.w - fillWidth, interior.h};
  if (filled.w > 0) epd_fill_rect({filled.x, filled.y, filled.w, filled.h}, color, fb);

  const String label = String(bounded) + "%";
  const int labelWidth = textWidth(label, FontRole::Caption);
  const int labelX = geometry.label.x + (geometry.label.w - labelWidth) / 2;
  const int labelBaseline = centeredBaseline(geometry.label, FontRole::Caption);
  const Rect filledLabel{filled.x, filled.y, min(filled.w, geometry.label.w), filled.h};
  const Rect unfilledLabel{max(unfilled.x, geometry.label.x), unfilled.y,
      max(0, geometry.label.x + geometry.label.w - max(unfilled.x, geometry.label.x)),
      unfilled.h};
  if (filledLabel.w > 0) text(fb, filledLabel, labelX, labelBaseline, label, FontRole::Caption, kPaper);
  if (unfilledLabel.w > 0) text(fb, unfilledLabel, labelX, labelBaseline, label, FontRole::Caption, kInk);

  if (model.charging) {
    // Small static corner bolt with a one-pixel paper halo. It never moves
    // and its measured 7 px reservation cannot obscure even the "100%" label.
    const Rect& mark = geometry.chargingMark;
    const int cx = mark.x + mark.w / 2;
    const int top = mark.y + 1;
    const int middle = mark.y + mark.h / 2;
    const int bottom = mark.y + mark.h - 1;
    line(fb, cx + 2, top, cx - 1, middle, kPaper);
    line(fb, cx - 1, middle, cx + 1, middle, kPaper);
    line(fb, cx + 1, middle, cx - 2, bottom, kPaper);
    line(fb, cx + 1, top, cx - 1, middle, kInk);
    line(fb, cx - 1, middle, cx + 1, middle, kInk);
    line(fb, cx + 1, middle, cx - 1, bottom, kInk);
  }
}

void circle(uint8_t* fb, int cx, int cy, int radius, uint8_t color) {
  epd_draw_circle(cx, cy, radius, color, fb);
}

void wifiIcon(uint8_t* fb, Rect bounds, network::State state,
              bool rssiAvailable, int16_t rssi, uint8_t color) {
  blankRegion(fb, bounds);
  constexpr int kBarCount=4,kBarWidth=4,kBarGap=3;
  const int totalWidth=kBarCount*kBarWidth+(kBarCount-1)*kBarGap;
  constexpr int kTallestBarHeight=22;
  const int left=bounds.x+(bounds.w-totalWidth)/2;
  const int sharedCenterY=bounds.y+bounds.h/2;
  const int bottom=sharedCenterY+kTallestBarHeight/2;
  int bars=0;
  if(state==network::State::Connected&&rssiAvailable){
    bars=rssi>=-55?4:(rssi>=-67?3:(rssi>=-78?2:1));
  }else if(state==network::State::Connected){
    bars=4;
  }else if(state==network::State::Connecting){
    bars=1;
  }
  // Compact phone-style strength bars share one flat bottom. Unfilled bars use
  // dark outlines rather than pale pixels, so every state remains crisp in GC16.
  for(int index=0;index<kBarCount;++index){
    const int height=7+index*5;
    const Rect bar{left+index*(kBarWidth+kBarGap),bottom-height,kBarWidth,height};
    if(index<bars)epd_fill_rect({bar.x,bar.y,bar.w,bar.h},color,fb);
    else epd_draw_rect({bar.x,bar.y,bar.w,bar.h},color,fb);
  }
}

void appBar(uint8_t* fb, const UiSnapshot& state, const char* section) {
  const Rect brandClip = contractRect(spec::kHeaderBrandBounds);
  const Rect clockClip = contractRect(spec::kHeaderClockBounds);
  const Rect dateClip = contractRect(spec::kHeaderDateBounds);
  const Rect wifiClip = contractRect(spec::kHeaderWifiBounds);
  const Rect batteryClip = contractRect(spec::kHeaderBatteryBounds);
  static_assert(spec::kHeaderBrandBounds[0] + spec::kHeaderBrandBounds[2] <=
                    spec::kHeaderClockBounds[0], "brand and clock regions overlap");
  static_assert(spec::kHeaderClockBounds[0] + spec::kHeaderClockBounds[2] <=
                    spec::kHeaderDateBounds[0], "clock and date regions overlap");
  static_assert(spec::kHeaderDateBounds[0] + spec::kHeaderDateBounds[2] <=
                    spec::kHeaderWifiBounds[0], "time/date and Wi-Fi regions overlap");
  static_assert(spec::kHeaderWifiBounds[0] + spec::kHeaderWifiBounds[2] <=
                    spec::kHeaderBatteryBounds[0], "Wi-Fi and battery regions overlap");
  static_assert(spec::kHeaderBrandBounds[1] <= spec::kHeaderBaseline &&
                    spec::kHeaderBaseline < spec::kHeaderBrandBounds[1] + spec::kHeaderBrandBounds[3],
                "shared header baseline must remain inside the single row");
  // Clear every independently clipped region before drawing it. The rest of the
  // app bar is already white from full-frame composition; no dark header band is used.
  blankRegion(fb, brandClip);
  blankRegion(fb, clockClip);
  blankRegion(fb, dateClip);
  blankRegion(fb, wifiClip);
  blankRegion(fb, batteryClip);
  text(fb,brandClip,brandClip.x,spec::kHeaderBaseline,
       "supportFORGE",FontRole::CardHeading,kInk);
  // Page titles and product metadata belong to content rather than consuming
  // permanent status-bar space on every screen.
  (void)section;
  String time = "--:--";
  if (state.rtcValid) {
    uint8_t shownHour = state.hour;
    String suffix;
    if (!state.use24Hour) {
      suffix = state.hour >= 12 ? " PM" : " AM";
      shownHour = state.hour % 12;
      if (!shownHour) shownHour = 12;
    }
    time = String((state.use24Hour && shownHour < 10) ? "0" : "") + shownHour + ":" +
           (state.minute < 10 ? "0" : "") + state.minute + suffix;
  }
  static const char* months[]={"JAN","FEB","MAR","APR","MAY","JUN",
                               "JUL","AUG","SEP","OCT","NOV","DEC"};
  const String date=state.rtcValid&&state.month>=1&&state.month<=12?
      String(months[state.month-1])+" "+String(state.day):"TIME SYNC";
  // Time is the dominant status value. Date remains a smaller adjacent cell in
  // the same row, so no hidden secondary text can survive below it.
  const int clockX=clockClip.x+(clockClip.w-textWidth(time,FontRole::CardHeading))/2;
  text(fb,clockClip,max(clockClip.x,clockX),spec::kHeaderBaseline,
       fittedText(time,FontRole::CardHeading,clockClip.w),FontRole::CardHeading,kInk);
  const int dateX=dateClip.x+(dateClip.w-textWidth(date,FontRole::Caption))/2;
  text(fb,dateClip,max(dateClip.x,dateX),spec::kHeaderBaseline,
       date,FontRole::Caption,kInk);
  // Bias the compact signal bars toward the battery while retaining the complete
  // Wi-Fi cell as their transition clear region.
  const Rect wifiGlyph{wifiClip.x + 4, wifiClip.y, wifiClip.w, wifiClip.h};
  wifiIcon(fb,wifiGlyph,state.wifi.state,state.wifi.rssiAvailable,state.wifi.rssi,kInk);
  // Caption metrics measure 100% at 41x19 px. The 64x25 phone-style glyph has
  // a 52x19 interior and a 45 px label region while charging.
  // Explicitly erase both retired extents and the new compact extent;
  // diagnostic words remain on HOME/Details and can never resize this icon.
  const Rect legacy120BatteryGlyph{batteryClip.x + 8, batteryClip.y + 10, 120, 32};
  const Rect legacy96BatteryGlyph{batteryClip.x + 12,
                                  batteryClip.y + (batteryClip.h - 28) / 2, 96, 28};
  const Rect batteryGlyph{batteryClip.x + batteryClip.w - 72,
                          batteryClip.y + (batteryClip.h - 25) / 2, 64, 25};
  blankRegion(fb, legacy120BatteryGlyph);
  blankRegion(fb, legacy96BatteryGlyph);
  blankRegion(fb, batteryGlyph);
  batteryIcon(fb, batteryGlyph, state.batteryVisual, kInk);
  // Erase the complete old-to-new divider band before placing the raised rule.
  // This remains correct if the app bar is ever redrawn from retained pixels.
  epd_fill_rect({0,kAppBarHeight-1,kCanvasWidth,9},kPaper,fb);
  epd_draw_hline(0,kAppBarHeight-1,kCanvasWidth,kRule,fb);
}

void card(uint8_t* fb, Rect b, const char* eyebrow, const String& title, const String& body) {
  roundedRect(fb,b,kRadiusCard,kPaper,kInk);
  const int stripHeight=min(38,b.h/3);
  const Rect clip{b.x+18,b.y+stripHeight+6,b.w-36,b.h-stripHeight-14};
  const bool compact = b.h < 100;
  epd_fill_rect({b.x+16,b.y+stripHeight-2,b.w-32,2},kInk,fb);
  text(fb,{b.x+18,b.y+2,b.w-36,stripHeight-4},b.x+18,
       centeredBaseline({b.x+18,b.y+2,b.w-36,stripHeight-4},FontRole::Caption),
       eyebrow,FontRole::Caption,kInk);
  const FontRole titleRole = compact ? FontRole::CardHeading : FontRole::PageHeading;
  const FontRole bodyRole = compact ? FontRole::Caption : FontRole::Body;
  const Rect titleRegion{clip.x,clip.y,clip.w,min(clip.h,textHeight(titleRole))};
  text(fb,titleRegion,titleRegion.x,centeredBaseline(titleRegion,titleRole),
       fittedText(title,titleRole,titleRegion.w),titleRole,kInk);
  const int bodyY=titleRegion.y+titleRegion.h+4;
  const Rect bodyRegion{clip.x,bodyY,clip.w,max(0,clip.y+clip.h-bodyY)};
  if (body.length() && bodyRegion.h >= textHeight(bodyRole)) {
    text(fb,bodyRegion,bodyRegion.x,centeredBaseline(bodyRegion,bodyRole),
         fittedText(body,bodyRole,bodyRegion.w),bodyRole,kInkMuted);
  }
}

void statusPill(uint8_t* fb, Rect b, const String& label, bool dark) {
  roundedRect(fb,b,b.h/2,dark?kInk:kSurfaceStrong,dark?kInk:kRule);
  const int tx=b.x+max(8,(b.w-textWidth(label,FontRole::Caption))/2);
  text(fb,{b.x+8,b.y,b.w-16,b.h},tx,b.y+(b.h+fonts::kCaption.ascent-fonts::kCaption.descent)/2,
       label,FontRole::Caption,dark?kPaper:kInk);
}

void metricTile(uint8_t* fb, Rect b, Icon glyph, const char* label, const String& value, const String& detail) {
  roundedRect(fb,b,kRadiusControl,kPaper,kInk);
  // The compact inverted header gives icon and title one shared baseline and
  // guarantees full contrast even after repeated physical-panel refreshes.
  epd_fill_rect({b.x+2,b.y+2,b.w-4,36},kInk,fb);
  icon(fb,glyph,b.x+23,b.y+20,20,kPaper);
  text(fb,{b.x+42,b.y+2,b.w-54,34},b.x+42,centeredBaseline({b.x+42,b.y+2,b.w-54,34},FontRole::Caption),label,FontRole::Caption,kPaper);
  const Rect clip{b.x+14,b.y+44,b.w-28,b.h-50};
  const Rect valueRegion{clip.x+2,clip.y,clip.w-2,min(clip.h,textHeight(FontRole::CardHeading))};
  text(fb,valueRegion,valueRegion.x,centeredBaseline(valueRegion,FontRole::CardHeading),
       fittedText(value,FontRole::CardHeading,valueRegion.w),FontRole::CardHeading,kInk);
  const int detailY=valueRegion.y+valueRegion.h+4;
  const Rect detailRegion{clip.x+2,detailY,clip.w-2,max(0,clip.y+clip.h-detailY)};
  if (detail.length() && detailRegion.h >= textHeight(FontRole::Caption)) {
    text(fb,detailRegion,detailRegion.x,centeredBaseline(detailRegion,FontRole::Caption),
         fittedText(detail,FontRole::Caption,detailRegion.w),FontRole::Caption,kInkMuted);
  }
}

void labeledRow(uint8_t* fb, Rect b, const char* label, const String& value, bool divider) {
  const Rect labelClip{b.x,b.y,b.w/2-8,b.h-2};
  const Rect valueClip{b.x+b.w/2,b.y,b.w/2,b.h-2};
  text(fb,labelClip,b.x,b.y+31,label,FontRole::Body,kInkMuted);
  const int w=textWidth(value,FontRole::Body);
  const String shown=fittedText(value,FontRole::Body,valueClip.w);
  const int shownWidth=textWidth(shown,FontRole::Body);
  text(fb,valueClip,max(valueClip.x,b.x+b.w-shownWidth),b.y+31,shown,FontRole::Body,kInk);
  if (divider) epd_draw_hline(b.x,b.y+b.h-1,b.w,kRule,fb);
}

void emptyState(uint8_t* fb, Rect b, Icon glyph, const String& title, const String& body) {
  roundedRect(fb,b,14,kSurfaceSoft,kRule); icon(fb,glyph,b.x+b.w/2,b.y+64,42,kInk);
  const Rect clip{b.x+16,b.y+16,b.w-32,b.h-32};
  int tx=b.x+(b.w-textWidth(title,FontRole::CardHeading))/2;
  text(fb,clip,max(clip.x,tx),b.y+122,title,FontRole::CardHeading,kInk);
  tx=b.x+(b.w-textWidth(body,FontRole::Body))/2;
  text(fb,clip,max(clip.x,tx),b.y+154,body,FontRole::Body,kInkMuted);
}

void dialog(uint8_t* fb, Rect b, const String& title, const String& body,
            const String& actionLabel) {
  epd_fill_rect({0, 0, kCanvasWidth, kCanvasHeight}, kSurface, fb);
  roundedRect(fb, b, 14, kPaper, kInk);
  const Rect clip{b.x+24,b.y+20,b.w-48,b.h-40};
  text(fb,clip,clip.x,b.y+54,title,FontRole::PageHeading,kInk);
  text(fb,clip,clip.x,b.y+92,body,FontRole::Body,kInkMuted);
  constexpr int kDialogActionHeight = 56;
  constexpr int kDialogBottomPadding = 24;
  static_assert(kDialogActionHeight >= spec::kMinimumTouchTarget,
                "dialog action must meet the minimum touch target");
  const Rect action{b.x + 24, b.y + b.h - kDialogBottomPadding - kDialogActionHeight,
                    b.w - 48, kDialogActionHeight};
  actionButton(fb, action, actionLabel, true, Icon::Check);
}

Rect navigationTarget(Page page) {
  int index=0;
  switch(page){case Page::Home:case Page::SystemHealth:case Page::SystemMetrics:case Page::Storage:case Page::Network:case Page::WeatherDetail:case Page::VehicleMotion:case Page::Altimeter:index=0;break;case Page::Systems:index=1;break;case Page::Radio:index=2;break;case Page::Location:index=3;break;case Page::Device:case Page::Diagnostics:case Page::DisplayCalibration:case Page::TextQualification:case Page::Settings:case Page::TouchSetup:case Page::Battery:case Page::TimezoneSetup:case Page::LowPowerSetup:case Page::LowPowerStatus:case Page::DisplayRefreshMode:case Page::TouchRecalibrateConfirm:case Page::DateTimeSettings:case Page::UnitsSettings:case Page::LocationPrivacySettings:case Page::WifiSettings:case Page::WifiNetworks:case Page::WifiEntry:case Page::WifiForgetConfirm:case Page::Calculator:index=4;break;case Page::WeatherSetup:index=0;break;}
  return {index*spec::kNavItemWidth,kContentBottom,spec::kNavItemWidth,kNavHeight};
}

void bottomNavigation(uint8_t* fb, Page selected) {
  static_assert(5*spec::kNavItemWidth==kCanvasWidth,
                "five equal navigation tiles must span the full canvas");
  epd_fill_rect({0,kContentBottom,kCanvasWidth,kNavHeight},kPaper,fb);
  epd_fill_rect({0,kContentBottom,kCanvasWidth,3},kInk,fb);
  const Icon icons[]={Icon::Home,Icon::Systems,Icon::Radio,Icon::Location,Icon::Device};
  const char* labels[]={"HOME","SYSTEMS","RADIO","LOCATION","DEVICE"};
  const Rect selectedTarget=navigationTarget(selected);
  for(int i=0;i<5;++i){
    const Rect target{i*spec::kNavItemWidth,kContentBottom,spec::kNavItemWidth,kNavHeight};
    const bool active=target.x==selectedTarget.x;
    const Rect visual{target.x+5,kContentBottom+8,target.w-10,kNavHeight-14};
    roundedRect(fb,visual,kRadiusControl,active?kInk:kPaper,active?kInk:kPaper);
    icon(fb,icons[i],target.x+target.w/2,kContentBottom+34,28,active?kPaper:kInk);
    const int x=target.x+(target.w-textWidth(labels[i],FontRole::Navigation))/2;
    text(fb,{target.x+3,kContentBottom+50,target.w-6,34},x,kContentBottom+76,
         labels[i],FontRole::Navigation,active?kPaper:kInk);
  }
}

}  // namespace ui