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
    case FontRole::Navigation: return fonts::kNavigation;
    case FontRole::QualificationCurrent: return fonts::kQualificationCurrent;
    case FontRole::QualificationRegularAa: return fonts::kQualificationRegularAa;
    case FontRole::QualificationMediumAa: return fonts::kQualificationMediumAa;
    case FontRole::QualificationSemiboldAa: return fonts::kQualificationSemiboldAa;
    case FontRole::QualificationBoldMono: return fonts::kQualificationBoldMono;
  }
  return fonts::kBody;
}

int glyphIndex(char raw) {
  const unsigned char c = static_cast<unsigned char>(raw);
  return c >= 32 && c <= 126 ? c - 32 : '?' - 32;
}

bool inClip(const Rect& clip, int x, int y) {
  return x >= clip.x && y >= clip.y && x < clip.x + clip.w && y < clip.y + clip.h;
}

void line(uint8_t* fb, int x1, int y1, int x2, int y2, uint8_t color) {
  epd_draw_line(x1, y1, x2, y2, color, fb);
}

}  // namespace

void clear(uint8_t* fb) { epd_fill_rect({0, 0, kCanvasWidth, kCanvasHeight}, kPaper, fb); }

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
    case Icon::Lock:
      epd_draw_rect({cx-h/2,cy,s/2,h/2},color,fb); epd_draw_circle(cx,cy,h/2,color,fb); break;
    case Icon::Check:
      line(fb,cx-h,cy,cx-2,cy+h,color); line(fb,cx-2,cy+h,cx+h,cy-h,color); break;
    case Icon::Info:
      epd_draw_circle(cx,cy,h,color,fb); line(fb,cx,cy-1,cx,cy+h/2,color); epd_fill_circle(cx,cy-h/2,2,color,fb); break;
  }
}

void circle(uint8_t* fb, int cx, int cy, int radius, uint8_t color) {
  epd_draw_circle(cx, cy, radius, color, fb);
}

void appBar(uint8_t* fb, const UiSnapshot& state, const char* section) {
  epd_fill_rect({0,0,kCanvasWidth,kAppBarHeight},kPaper,fb);
  const Rect brandClip{kMargin, 0, 294, kAppBarHeight - 1};
  const Rect clockClip{326, 0, 132, kAppBarHeight - 1};
  const Rect batteryClip{462, 0, 66, kAppBarHeight - 1};
  text(fb,brandClip,kMargin,38,"supportFORGE",FontRole::Brand,kInk);
  // The product subtitle is invariant. Page names belong to page content, never
  // in the brand/status header where they could recreate the photographed overlap.
  (void)section;
  text(fb,brandClip,kMargin,70,"FIELD TERMINAL",FontRole::Body,kInkMuted);
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
  const int timeX = clockClip.x + clockClip.w - textWidth(time,FontRole::CardHeading);
  text(fb,clockClip,max(clockClip.x,timeX),32,time,FontRole::CardHeading,kInk);
  String date = state.rtcValid ? String(state.year) + "-" +
      (state.month < 10 ? "0" : "") + state.month + "-" +
      (state.day < 10 ? "0" : "") + state.day : "TIME SYNC";
  const int dateX = clockClip.x + clockClip.w - textWidth(date,FontRole::Caption);
  text(fb,clockClip,max(clockClip.x,dateX),68,date,FontRole::Caption,kInkMuted);
  icon(fb,Icon::Battery,492,28,28,kInk);
  const String battery = state.batteryPercentAvailable ? String(state.batteryPercent)+"%" : "--";
  const int batteryX = batteryClip.x + (batteryClip.w-textWidth(battery,FontRole::Caption))/2;
  text(fb,batteryClip,max(batteryClip.x,batteryX),67,battery,FontRole::Caption,kInkMuted);
  epd_draw_hline(0,kAppBarHeight-1,kCanvasWidth,kRule,fb);
}

void card(uint8_t* fb, Rect b, const char* eyebrow, const String& title, const String& body) {
  roundedRect(fb,b,12,kSurfaceSoft,kRule);
  const Rect clip{b.x+18,b.y+12,b.w-36,b.h-24};
  text(fb,clip,clip.x,b.y+31,eyebrow,FontRole::Caption,kInkMuted);
  text(fb,clip,clip.x,b.y+61,title,FontRole::PageHeading,kInk);
  if (body.length()) text(fb,clip,clip.x,b.y+91,body,FontRole::Body,kInkMuted);
}

void statusPill(uint8_t* fb, Rect b, const String& label, bool dark) {
  roundedRect(fb,b,b.h/2,dark?kInk:kSurfaceStrong,dark?kInk:kRule);
  const int tx=b.x+max(8,(b.w-textWidth(label,FontRole::Caption))/2);
  text(fb,{b.x+8,b.y,b.w-16,b.h},tx,b.y+(b.h+fonts::kCaption.ascent-fonts::kCaption.descent)/2,
       label,FontRole::Caption,dark?kPaper:kInk);
}

void metricTile(uint8_t* fb, Rect b, Icon glyph, const char* label, const String& value, const String& detail) {
  roundedRect(fb,b,10,kPaper,kRule); icon(fb,glyph,b.x+28,b.y+28,22,kInk);
  const Rect clip{b.x+14,b.y+8,b.w-28,b.h-16};
  text(fb,clip,b.x+48,b.y+31,label,FontRole::Caption,kInkMuted);
  text(fb,clip,b.x+16,b.y+67,value,FontRole::CardHeading,kInk);
  if (detail.length()) text(fb,clip,b.x+16,b.y+94,detail,FontRole::Caption,kInkMuted);
}

void labeledRow(uint8_t* fb, Rect b, const char* label, const String& value, bool divider) {
  const Rect labelClip{b.x,b.y,b.w/2-8,b.h-2};
  const Rect valueClip{b.x+b.w/2,b.y,b.w/2,b.h-2};
  text(fb,labelClip,b.x,b.y+31,label,FontRole::Body,kInkMuted);
  const int w=textWidth(value,FontRole::Body);
  text(fb,valueClip,max(valueClip.x,b.x+b.w-w),b.y+31,value,FontRole::Body,kInk);
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
  const Rect action{b.x + 24, b.y + b.h - 76, b.w - 48, 52};
  roundedRect(fb, action, 10, kInk, kInk);
  text(fb,{action.x+12,action.y,action.w-24,action.h},
       action.x+max(12,(action.w-textWidth(actionLabel,FontRole::Body))/2),action.y+33,
       actionLabel,FontRole::Body,kPaper);
}

Rect navigationTarget(Page page) {
  int index=0;
  switch(page){case Page::Home:index=0;break;case Page::Systems:index=1;break;case Page::Radio:index=2;break;case Page::Location:index=3;break;case Page::Device:case Page::Diagnostics:case Page::DisplayCalibration:case Page::TextQualification:case Page::Settings:case Page::TouchSetup:index=4;break;}
  return {index*spec::kNavItemWidth,kContentBottom,spec::kNavItemWidth,kNavHeight};
}

void bottomNavigation(uint8_t* fb, Page selected) {
  epd_fill_rect({0,kContentBottom,kCanvasWidth,kNavHeight},kPaper,fb);
  epd_fill_rect({0,kContentBottom,kCanvasWidth,3},kInk,fb);
  const Page pages[]={Page::Home,Page::Systems,Page::Radio,Page::Location,Page::Device};
  const Icon icons[]={Icon::Home,Icon::Systems,Icon::Radio,Icon::Location,Icon::Device};
  const char* labels[]={"HOME","SYSTEMS","RADIO","LOCATION","DEVICE"};
  for(int i=0;i<5;++i){
    const bool active=(selected==pages[i])||((selected==Page::Diagnostics||selected==Page::DisplayCalibration||selected==Page::TextQualification||selected==Page::Settings||selected==Page::TouchSetup)&&i==4);
    const Rect target{i*spec::kNavItemWidth,kContentBottom,spec::kNavItemWidth,kNavHeight};
    epd_fill_rect({target.x,kContentBottom+3,target.w,kNavHeight-3},active?kInk:kPaper,fb);
    epd_draw_rect({target.x,kContentBottom+3,target.w,kNavHeight-3},kInk,fb);
    epd_draw_rect({target.x+1,kContentBottom+4,target.w-2,kNavHeight-5},kInk,fb);
    icon(fb,icons[i],target.x+54,kContentBottom+39,28,active?kPaper:kInk);
    const int x=target.x+(target.w-textWidth(labels[i],FontRole::Navigation))/2;
    text(fb,{target.x+3,kContentBottom+54,target.w-6,39},x,kContentBottom+80,
         labels[i],FontRole::Navigation,active?kPaper:kInk);
  }
}

}  // namespace ui