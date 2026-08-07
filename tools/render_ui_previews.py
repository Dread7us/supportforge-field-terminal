"""Deterministic 540x960 grayscale previews for the retained field UI.

The geometry and copy mirror src/ui/ui_theme.h and src/ui/ui_pages.cpp. No
device data, credentials, endpoint, MAC, NMEA payload, or coordinates are used.
"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

W, H = 540, 960
MARGIN, APP, NAV, CONTENT_BOTTOM = 24, 104, 96, 864
INK, MUTED, RULE, STRONG, SURFACE, SOFT, PAPER = 0, 85, 160, 196, 221, 238, 255
OUT = Path(__file__).parents[1] / "docs" / "ui-previews"
FONT = ImageFont.load_default()

PAGES = {
    "home-setup": "HOME",
    "systems-empty": "SYSTEMS",
    "radio-receive-only": "RADIO",
    "location-gps": "LOCATION",
    "device-status": "DEVICE",
    "hardware-diagnostics": "HARDWARE DIAGNOSTICS",
}

def text(draw, xy, value, size=16, fill=INK, anchor=None):
    font = ImageFont.truetype("arial.ttf", size) if Path("C:/Windows/Fonts/arial.ttf").exists() else FONT
    draw.text(xy, value, font=font, fill=fill, anchor=anchor)

def rr(draw, box, radius=12, fill=SOFT, outline=RULE, width=1):
    draw.rounded_rectangle(box, radius=radius, fill=fill, outline=outline, width=width)

def app(draw, section):
    text(draw, (24, 18), "support", 18, MUTED)
    text(draw, (100, 13), "FORGE", 28, INK)
    text(draw, (24, 55), "FIELD TERMINAL" if section == "HOME" else section, 17, MUTED)
    text(draw, (420, 20), "--:--", 18)
    text(draw, (420, 48), "--/--", 11, MUTED)
    draw.rectangle((466, 59, 497, 74), outline=INK, width=2)
    draw.rectangle((498, 64, 501, 69), fill=INK)
    draw.line((0, 103, W, 103), fill=RULE)

def card(draw, box, eyebrow, title, body):
    rr(draw, box)
    x, y = box[0] + 20, box[1] + 16
    text(draw, (x, y), eyebrow, 12, MUTED)
    text(draw, (x, y + 25), title, 25)
    text(draw, (x, y + 61), body, 15, MUTED)

def row(draw, y, label, value):
    text(draw, (44, y + 16), label, 15, MUTED)
    text(draw, (496, y + 16), value, 15, INK, "ra")
    draw.line((44, y + 46, 496, y + 46), fill=RULE)

def empty(draw, box, title, body):
    rr(draw, box)
    cx = (box[0] + box[2]) // 2
    draw.ellipse((cx - 25, box[1] + 38, cx + 25, box[1] + 88), outline=INK, width=3)
    text(draw, (cx, box[1] + 112), title, 20, anchor="ma")
    text(draw, (cx, box[1] + 150), body, 13, MUTED, "ma")

def nav(draw, selected):
    labels = ["HOME", "SYSTEMS", "RADIO", "LOCATION", "DEVICE"]
    draw.rectangle((0, CONTENT_BOTTOM, W, H), fill=PAPER)
    draw.line((0, CONTENT_BOTTOM, W, CONTENT_BOTTOM), fill=INK, width=2)
    selected = "DEVICE" if selected == "HARDWARE DIAGNOSTICS" else selected
    for i, label in enumerate(labels):
        cx = i * 108 + 54
        if label == selected:
            draw.rectangle((i * 108, CONTENT_BOTTOM + 1, (i + 1) * 108, CONTENT_BOTTOM + 6), fill=INK)
        draw.ellipse((cx - 10, CONTENT_BOTTOM + 22, cx + 10, CONTENT_BOTTOM + 42), outline=INK if label == selected else MUTED, width=2)
        text(draw, (cx, CONTENT_BOTTOM + 67), label, 11, INK if label == selected else MUTED, "ma")

def render(page):
    image = Image.new("L", (W, H), PAPER)
    d = ImageDraw.Draw(image)
    app(d, page)
    if page == "HOME":
        card(d, (24, 128, 516, 282), "TERMINAL STATUS", "SETUP REQUIRED", "Connect supportFORGE to begin monitoring")
        rr(d, (44, 238, 156, 266), 14, INK, INK); text(d, (100, 252), "OFFLINE", 11, PAPER, "mm")
        tiles = [(24,306,"SYSTEMS","NOT CONNECTED"),(278,306,"INCIDENTS","--"),(24,434,"LORA","UNVERIFIED"),(278,434,"GPS","UNVERIFIED")]
        for x,y,label,value in tiles:
            rr(d,(x,y,x+238,y+112),10,PAPER,RULE); text(d,(x+16,y+18),label,12,MUTED); text(d,(x+16,y+54),value,18)
        rr(d,(24,562,516,814),12,PAPER,RULE); text(d,(44,582),"CURRENT DEVICE",12,MUTED)
        for y,l,v in [(610,"BATTERY","UNVERIFIED"),(658,"LOCATION","UNVERIFIED"),(706,"RADIO","TX LOCKED"),(754,"NEXT SYNC","NOT SCHEDULED")]: row(d,y,l,v)
    elif page == "SYSTEMS":
        empty(d,(24,144,516,470),"NO SYSTEMS YET","Connect supportFORGE to begin monitoring")
        card(d,(24,494,516,644),"CONFIGURATION","SETUP REQUIRED","Host data will appear here after connection")
    elif page == "RADIO":
        card(d,(24,128,516,284),"SX1262 RADIO","RECEIVE ONLY","915 MHz region - transmission disabled")
        rr(d,(44,240,170,270),15,INK,INK); text(d,(107,255),"TX LOCKED",11,PAPER,"mm")
        rr(d,(24,308,516,554),12,PAPER,RULE)
        for y,l,v in [(326,"REGION","915 MHZ"),(378,"MODE","RECEIVE ONLY"),(430,"MODULE","UNVERIFIED"),(482,"TRANSMIT","LOCKED")]: row(d,y,l,v)
        empty(d,(24,580,516,790),"NO MESSAGES","Received LoRa traffic will appear here")
    elif page == "LOCATION":
        card(d,(24,128,516,284),"L76K GNSS","UNVERIFIED","Waiting for qualified satellite data")
        rr(d,(24,310,516,556),12,PAPER,RULE)
        for y,l,v in [(328,"MODULE","UNVERIFIED"),(380,"FIX","NO FIX"),(432,"SATELLITES","--"),(484,"COORDINATES","PRIVATE")]: row(d,y,l,v)
        empty(d,(24,582,516,790),"NO WAYPOINTS","Navigation tools are planned for a future release")
    elif page == "DEVICE":
        for x,label,value in [(24,"BATTERY","UNVERIFIED"),(278,"STORAGE","UNVERIFIED")]:
            rr(d,(x,128,x+238,240),10,PAPER,RULE); text(d,(x+16,148),label,12,MUTED); text(d,(x+16,182),value,18)
        rr(d,(24,264,516,522),12,PAPER,RULE)
        for y,l,v in [(282,"RTC","READY"),(334,"TOUCH","READY"),(386,"PSRAM","READY"),(438,"FIRMWARE","FIELD UI 1")]: row(d,y,l,v)
        rr(d,(24,550,516,638),10,STRONG,INK); text(d,(88,570),"HARDWARE DIAGNOSTICS",18); text(d,(88,604),"Safe qualification tools and observed state",12,MUTED)
        card(d,(24,666,516,790),"SETTINGS","COMING LATER","No credentials or endpoints stored")
    else:
        rr(d,(24,122,204,152),15,INK,INK); text(d,(114,137),"OBSERVED STATUS",11,PAPER,"mm")
        rr(d,(24,170,516,608),12,PAPER,RULE)
        for y,l,v in [(184,"GT911","READY"),(234,"RTC","READY"),(284,"BQ27220","READY"),(334,"MICROSD","UNVERIFIED"),(384,"L76K","READY"),(434,"SX1262","READY"),(484,"LORA TX","LOCKED"),(534,"TOUCH MAP","SERIAL TEST")]: row(d,y,l,v)
        card(d,(24,632,516,790),"GUARDED OPERATIONS","SERIAL COMMANDS ONLY","No TX, VCOM, waveform, or destructive tests exposed")
    nav(d, page)
    image = image.quantize(colors=16).convert("L")
    return image

def main():
    OUT.mkdir(parents=True, exist_ok=True)
    for filename, page in PAGES.items():
        render(page).save(OUT / f"{filename}.png", optimize=False)
    print(f"Rendered {len(PAGES)} deterministic previews to {OUT}")

if __name__ == "__main__":
    main()