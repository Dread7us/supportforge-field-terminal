#include "ui_components.h"

#include <epdiy.h>

namespace ui {
namespace {

// Deterministic ASCII-only 5x7 instrument face. Unsupported glyphs become spaces.
const uint8_t kFont[][5] = {
  {0,0,0,0,0},{0x3E,0x51,0x49,0x45,0x3E},{0,0x42,0x7F,0x40,0},{0x42,0x61,0x51,0x49,0x46},
  {0x21,0x41,0x45,0x4B,0x31},{0x18,0x14,0x12,0x7F,0x10},{0x27,0x45,0x45,0x45,0x39},{0x3C,0x4A,0x49,0x49,0x30},{1,0x71,9,5,3},{0x36,0x49,0x49,0x49,0x36},{6,0x49,0x49,0x29,0x1E},
  {0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},{0x7F,0x41,0x41,0x22,0x1C},{0x7F,0x49,0x49,0x49,0x41},{0x7F,9,9,9,1},{0x3E,0x41,0x49,0x49,0x7A},{0x7F,8,8,8,0x7F},{0,0x41,0x7F,0x41,0},{0x20,0x40,0x41,0x3F,1},{0x7F,8,0x14,0x22,0x41},{0x7F,0x40,0x40,0x40,0x40},{0x7F,2,0x0C,2,0x7F},{0x7F,4,8,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},{0x7F,9,9,9,6},{0x3E,0x41,0x51,0x21,0x5E},{0x7F,9,0x19,0x29,0x46},{0x46,0x49,0x49,0x49,0x31},{1,1,0x7F,1,1},{0x3F,0x40,0x40,0x40,0x3F},{0x1F,0x20,0x40,0x20,0x1F},{0x3F,0x40,0x38,0x40,0x3F},{0x63,0x14,8,0x14,0x63},{7,8,0x70,8,7},{0x61,0x51,0x49,0x45,0x43},
  {0,0x36,0x36,0,0},{0x20,0x10,8,4,2},{8,8,8,8,8},{0,0x60,0x60,0,0}
};

int glyphIndex(char raw) {
  const char c = raw >= 'a' && raw <= 'z' ? raw - 32 : raw;
  if (c >= '0' && c <= '9') return 1 + c - '0';
  if (c >= 'A' && c <= 'Z') return 11 + c - 'A';
  if (c == ':') return 37;
  if (c == '/') return 38;
  if (c == '-') return 39;
  if (c == '.') return 40;
  return 0;
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
  if (stroke != fill) epd_draw_rect({r.x, r.y, r.w, r.h}, stroke, fb);
}

int textWidth(const String& value, int scale) { return value.length() * 6 * scale; }

void text(uint8_t* fb, int x, int y, const String& value, int scale, uint8_t color, int maxWidth) {
  const int limit = maxWidth > 0 ? x + maxWidth : kCanvasWidth;
  for (char c : value) {
    if (x + 5 * scale > limit) break;
    const int index = glyphIndex(c);
    for (int col = 0; col < 5; ++col) {
      for (int row = 0; row < 7; ++row) {
        if (kFont[index][col] & (1 << row))
          epd_fill_rect({x + col * scale, y + row * scale, scale, scale}, color, fb);
      }
    }
    x += 6 * scale;
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
      roundedRect(fb,{cx-h,cy-h,s,s},4,kPaper,color); epd_fill_circle(cx,cy,3,color,fb); break;
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

void appBar(uint8_t* fb, const UiSnapshot& state, const char* section) {
  epd_fill_rect({0,0,kCanvasWidth,kAppBarHeight},kPaper,fb);
  text(fb,kMargin,20,"support",2,kInkMuted); text(fb,kMargin+84,16,"FORGE",3,kInk);
  text(fb,kMargin,54,section ? section : "FIELD TERMINAL",2,kInkMuted);
  String time = state.rtcValid ? String(state.hour < 10 ? "0" : "") + state.hour + ":" +
                (state.minute < 10 ? "0" : "") + state.minute : "--:--";
  text(fb,420,21,time,2,kInk,96);
  String date = state.rtcValid ? String(state.month < 10 ? "0" : "") + state.month + "/" +
                (state.day < 10 ? "0" : "") + state.day : "--/--";
  text(fb,420,49,date,1,kInkMuted,48);
  icon(fb,Icon::Battery,484,67,26,kInk); text(fb,502,60,"--",1,kInkMuted,24);
  epd_draw_hline(0,kAppBarHeight-1,kCanvasWidth,kRule,fb);
}

void card(uint8_t* fb, Rect b, const char* eyebrow, const String& title, const String& body) {
  roundedRect(fb,b,12,kSurfaceSoft,kRule);
  text(fb,b.x+20,b.y+18,eyebrow,1,kInkMuted,b.w-40);
  text(fb,b.x+20,b.y+44,title,3,kInk,b.w-40);
  if (body.length()) text(fb,b.x+20,b.y+78,body,2,kInkMuted,b.w-40);
}

void statusPill(uint8_t* fb, Rect b, const String& label, bool dark) {
  roundedRect(fb,b,b.h/2,dark?kInk:kSurfaceStrong,dark?kInk:kRule);
  const int scale=1; const int tx=b.x+max(8,(b.w-textWidth(label,scale))/2);
  text(fb,tx,b.y+(b.h-7*scale)/2,label,scale,dark?kPaper:kInk,b.w-16);
}

void metricTile(uint8_t* fb, Rect b, Icon glyph, const char* label, const String& value, const String& detail) {
  roundedRect(fb,b,10,kPaper,kRule); icon(fb,glyph,b.x+28,b.y+28,22,kInk);
  text(fb,b.x+50,b.y+21,label,1,kInkMuted,b.w-60);
  text(fb,b.x+16,b.y+55,value,2,kInk,b.w-32);
  if (detail.length()) text(fb,b.x+16,b.y+82,detail,1,kInkMuted,b.w-32);
}

void labeledRow(uint8_t* fb, Rect b, const char* label, const String& value, bool divider) {
  text(fb,b.x,b.y+20,label,2,kInkMuted,b.w/2);
  const int w=textWidth(value,2); text(fb,max(b.x+b.w/2,b.x+b.w-w),b.y+20,value,2,kInk,b.w/2);
  if (divider) epd_draw_hline(b.x,b.y+b.h-1,b.w,kRule,fb);
}

void emptyState(uint8_t* fb, Rect b, Icon glyph, const String& title, const String& body) {
  roundedRect(fb,b,14,kSurfaceSoft,kRule); icon(fb,glyph,b.x+b.w/2,b.y+64,42,kInk);
  int tx=b.x+(b.w-textWidth(title,2))/2; text(fb,max(b.x+16,tx),b.y+108,title,2,kInk,b.w-32);
  tx=b.x+(b.w-textWidth(body,1))/2; text(fb,max(b.x+16,tx),b.y+144,body,1,kInkMuted,b.w-32);
}

void dialog(uint8_t* fb, Rect b, const String& title, const String& body,
            const String& actionLabel) {
  epd_fill_rect({0, 0, kCanvasWidth, kCanvasHeight}, kSurface, fb);
  roundedRect(fb, b, 14, kPaper, kInk);
  text(fb, b.x + 24, b.y + 28, title, 3, kInk, b.w - 48);
  text(fb, b.x + 24, b.y + 76, body, 2, kInkMuted, b.w - 48);
  const Rect action{b.x + 24, b.y + b.h - 76, b.w - 48, 52};
  roundedRect(fb, action, 10, kInk, kInk);
  text(fb, action.x + max(12, (action.w - textWidth(actionLabel, 2)) / 2),
       action.y + 18, actionLabel, 2, kPaper, action.w - 24);
}

Rect navigationTarget(Page page) {
  int index=0;
  switch(page){case Page::Home:index=0;break;case Page::Systems:index=1;break;case Page::Radio:index=2;break;case Page::Location:index=3;break;case Page::Device:case Page::Diagnostics:index=4;break;}
  return {index*108,kContentBottom,108,kNavHeight};
}

void bottomNavigation(uint8_t* fb, Page selected) {
  epd_fill_rect({0,kContentBottom,kCanvasWidth,kNavHeight},kPaper,fb);
  epd_draw_hline(0,kContentBottom,kCanvasWidth,kInk,fb);
  const Page pages[]={Page::Home,Page::Systems,Page::Radio,Page::Location,Page::Device};
  const Icon icons[]={Icon::Home,Icon::Systems,Icon::Radio,Icon::Location,Icon::Device};
  const char* labels[]={"HOME","SYSTEMS","RADIO","LOCATION","DEVICE"};
  for(int i=0;i<5;++i){
    const bool active=(selected==pages[i])||(selected==Page::Diagnostics&&i==4);
    if(active) epd_fill_rect({i*108,kContentBottom+1,108,5},kInk,fb);
    icon(fb,icons[i],i*108+54,kContentBottom+35,24,active?kInk:kInkMuted);
    const int x=i*108+(108-textWidth(labels[i],1))/2;
    text(fb,x,kContentBottom+67,labels[i],1,active?kInk:kInkMuted,104);
  }
}

}  // namespace ui