"""Render structural design previews using the firmware's approved font assets."""
import json
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT=Path(__file__).parents[1]; SPEC=json.loads((ROOT/"ui/ui_spec.json").read_text()); OUT=ROOT/"docs/ui-previews"
W,H=SPEC["canvas"]["width"],SPEC["canvas"]["height"];G,P=SPEC["geometry"],SPEC["palette"]
INK,MUTED,RULE,STRONG,SOFT,PAPER=P["ink"],P["ink_muted"],P["rule"],P["surface_strong"],P["surface_soft"],P["paper"]
FONT_PATH=ROOT/SPEC["font"]["source"]
FONTS={z:ImageFont.truetype(str(FONT_PATH),size=size) for z,size in ((1,12),(2,16),(3,24))}
BRAND=ImageFont.truetype(str(FONT_PATH),size=30)
PAGES={"home-setup":"HOME","systems-empty":"SYSTEMS","radio-receive-only":"RADIO","location-gps":"LOCATION","device-status":"DEVICE","hardware-diagnostics":"HARDWARE DIAGNOSTICS"}

def tw(s,z=2):return round(FONTS[z].getlength(s))
def text(d,xy,s,z=2,c=INK,mw=0,a="left"):
 x,y=xy
 if a=="center":x-=tw(s,z)//2
 if a=="right":x-=tw(s,z)
 if mw:
  while s and tw(s,z)>mw:s=s[:-1]
 d.text((x,y),s,font=FONTS[z],fill=c,anchor="la",stroke_width=0)
def rr(d,r,rad=12,fill=SOFT,out=RULE):x,y,w,h=r;d.rounded_rectangle((x,y,x+w-1,y+h-1),radius=rad,fill=fill,outline=out)
def line(d,a,b,c=INK):d.line((*a,*b),fill=c)
def icon(d,v,cx,cy,s=24,c=INK):
 h=s//2
 if v=="HOME":line(d,(cx-h,cy),(cx,cy-h),c);line(d,(cx,cy-h),(cx+h,cy),c);d.rectangle((cx-h+3,cy,cx+h-3,cy+h),outline=c)
 elif v=="SYSTEMS":
  for i in (-1,0,1):d.rectangle((cx-h,cy+i*7-2,cx+h-1,cy+i*7+2),outline=c)
 elif v=="RADIO":d.ellipse((cx-3,cy+2,cx+3,cy+8),fill=c);line(d,(cx,cy+2),(cx,cy-h),c);d.ellipse((cx-h//2,cy+3-h//2,cx+h//2,cy+3+h//2),outline=c);d.ellipse((cx-h,cy+3-h,cx+h,cy+3+h),outline=c)
 elif v=="LOCATION":d.ellipse((cx-h//2,cy-h//3-h//2,cx+h//2,cy-h//3+h//2),outline=c);line(d,(cx-h//2,cy),(cx,cy+h),c);line(d,(cx,cy+h),(cx+h//2,cy),c)
 elif v=="DEVICE":d.rounded_rectangle((cx-h,cy-h,cx+h,cy+h),radius=4,fill=PAPER,outline=c);d.ellipse((cx-3,cy-3,cx+3,cy+3),fill=c)
 elif v=="BATTERY":d.rectangle((cx-h,cy-h//2,cx+h-3,cy+h//2),outline=c);d.rectangle((cx+h-2,cy-3,cx+h+1,cy+3),fill=c)
 elif v=="CHECK":line(d,(cx-h,cy),(cx-2,cy+h),c);line(d,(cx-2,cy+h),(cx+h,cy-h),c)
 elif v=="INFO":d.ellipse((cx-h,cy-h,cx+h,cy+h),outline=c);line(d,(cx,cy-1),(cx,cy+h//2),c);d.ellipse((cx-2,cy-h//2-2,cx+2,cy-h//2+2),fill=c)
def card(d,r,e,t,b=""):
 rr(d,r);x,y,w,h=r;text(d,(x+20,y+18),e,1,MUTED,w-40);text(d,(x+20,y+44),t,3,INK,w-40)
 if b:text(d,(x+20,y+78),b,1,MUTED,w-40)
def pill(d,r,s,dark=False):rr(d,r,r[3]//2,INK if dark else STRONG,INK if dark else RULE);text(d,(r[0]+r[2]//2,r[1]+(r[3]-7)//2),s,1,PAPER if dark else INK,a="center")
def tile(d,r,l,v,detail="",g="INFO"):
 rr(d,r,10,PAPER,RULE);x,y,w,h=r;icon(d,g,x+28,y+28,22);text(d,(x+50,y+21),l,1,MUTED,w-60);text(d,(x+16,y+55),v,2,INK,w-32)
 if detail:text(d,(x+16,y+82),detail,1,MUTED,w-32)
def row(d,r,l,v,div=True):x,y,w,h=r;text(d,(x,y+20),l,2,MUTED,w//2);text(d,(x+w,y+20),v,2,INK,w//2,"right");div and d.line((x,y+h-1,x+w-1,y+h-1),fill=RULE)
def empty(d,r,t,b,g):rr(d,r,14);x,y,w,h=r;cx=x+w//2;icon(d,g,cx,y+64,42);text(d,(cx,y+108),t,2,a="center");text(d,(cx,y+144),b,1,MUTED,a="center")
def app(d,s):d.text((24,8),"supportFORGE",font=BRAND,fill=INK);text(d,(24,52),"FIELD TERMINAL",2,MUTED);text(d,(516,12),"--:--",3,a="right");text(d,(420,49),"--/--",1,MUTED,48);icon(d,"BATTERY",484,72,24);text(d,(502,60),"--",1,MUTED,24);d.line((0,103,539,103),fill=RULE)
def nav(d,s):
 if s=="HARDWARE DIAGNOSTICS":s="DEVICE"
 d.rectangle((0,864,539,959),fill=PAPER);d.line((0,864,539,864),fill=INK)
 for i,l in enumerate(SPEC["navigation"]["labels"]):
  x=i*108;active=l==s
  if active:d.rectangle((x,865,x+107,869),fill=INK)
  icon(d,("HOME","SYSTEMS","RADIO","LOCATION","DEVICE")[i],x+54,899,24,INK if active else MUTED);text(d,(x+54,931),l,1,INK if active else MUTED,a="center")
def render(page):
 im=Image.new("L",(W,H),PAPER);d=ImageDraw.Draw(im);app(d,page)
 if page=="HOME":
  card(d,(24,128,492,154),"TERMINAL STATUS","SETUP REQUIRED","CONNECT SUPPORTFORGE TO BEGIN MONITORING");pill(d,(44,238,132,28),"TOUCH READY",True);tile(d,(24,306,238,112),"SYSTEMS","NOT CONNECTED","NO HOSTS CONFIGURED","SYSTEMS");tile(d,(278,306,238,112),"INCIDENTS","--","AWAITING SETUP","INFO");tile(d,(24,434,238,112),"LORA","UNVERIFIED","TX REMAINS LOCKED","RADIO");tile(d,(278,434,238,112),"GPS","UNVERIFIED","COORDINATES PRIVATE","LOCATION");rr(d,(24,562,492,252),12,PAPER,RULE);text(d,(44,584),"CURRENT DEVICE",1,MUTED)
  for i,x in enumerate((("BATTERY","UNVERIFIED"),("LOCATION","UNVERIFIED"),("RADIO","TX LOCKED"),("NEXT SYNC","NOT SCHEDULED"))):row(d,(44,610+i*48,452,48),*x,i<3)
 elif page=="SYSTEMS":empty(d,(24,144,492,326),"NO SYSTEMS YET","CONNECT SUPPORTFORGE TO BEGIN MONITORING","SYSTEMS");card(d,(24,494,492,150),"CONFIGURATION","SETUP REQUIRED","HOST DATA WILL APPEAR HERE AFTER CONNECTION");pill(d,(44,680,170,32),"NO FABRICATED DATA")
 elif page=="RADIO":
  card(d,(24,128,492,156),"SX1262 RADIO","RECEIVE ONLY","915 MHZ REGION - TRANSMISSION DISABLED");pill(d,(44,240,126,30),"TX LOCKED",True);rr(d,(24,308,492,246),12,PAPER,RULE)
  for i,x in enumerate((("REGION","915 MHZ"),("MODE","RECEIVE ONLY"),("MODULE","UNVERIFIED"),("TRANSMIT","LOCKED"))):row(d,(44,326+i*52,452,52),*x,i<3)
  empty(d,(24,580,492,210),"NO MESSAGES","RECEIVED LORA TRAFFIC WILL APPEAR HERE","RADIO")
 elif page=="LOCATION":
  card(d,(24,128,492,156),"L76K GNSS","UNVERIFIED","WAITING FOR QUALIFIED SATELLITE DATA");rr(d,(24,310,492,246),12,PAPER,RULE)
  for i,x in enumerate((("MODULE","UNVERIFIED"),("FIX","NO FIX"),("SATELLITES","--"),("COORDINATES","PRIVATE"))):row(d,(44,328+i*52,452,52),*x,i<3)
  empty(d,(24,582,492,208),"NO WAYPOINTS","NAVIGATION TOOLS ARE PLANNED FOR A FUTURE RELEASE","LOCATION")
 elif page=="DEVICE":tile(d,(24,128,238,112),"BATTERY","UNVERIFIED","BQ27220 FUEL GAUGE","BATTERY");tile(d,(278,128,238,112),"STORAGE","UNVERIFIED","READ-ONLY STATUS","DEVICE");rr(d,(24,264,492,258),12,PAPER,RULE);[row(d,(44,282+i*52,452,52),*x,i<3) for i,x in enumerate((("RTC","READY"),("TOUCH","READY"),("PSRAM","READY"),("FIRMWARE","FIELD UI 1")))];rr(d,(24,550,492,88),10,STRONG,INK);icon(d,"DEVICE",58,594,28);text(d,(88,570),"HARDWARE DIAGNOSTICS",2,mw=390);text(d,(88,602),"SAFE QUALIFICATION TOOLS AND OBSERVED STATE",1,MUTED,390);rr(d,(24,654,492,72));icon(d,"CHECK",58,690,24);text(d,(88,672),"REPEAT TOUCH SETUP",2);text(d,(88,702),"RE-CHECK ALL FOUR PHYSICAL CORNERS",1,MUTED);card(d,(24,742,492,72),"SETTINGS","COMING LATER")
 else:
  pill(d,(24,122,180,30),"OBSERVED STATUS",True);rr(d,(24,170,492,438),12,PAPER,RULE)
  for i,x in enumerate((("GT911","READY"),("RTC","READY"),("BQ27220","READY"),("MICROSD","UNVERIFIED"),("L76K","READY"),("SX1262","READY"),("LORA TX","LOCKED"),("TOUCH MAP","VERIFIED"))):row(d,(44,184+i*50,452,50),*x,i<7)
  card(d,(24,632,492,158),"GUARDED OPERATIONS","SERIAL COMMANDS ONLY","NO TX VCOM WAVEFORM OR DESTRUCTIVE TESTS EXPOSED")
 nav(d,page);return im
def main():
 OUT.mkdir(parents=True,exist_ok=True)
 for f,p in PAGES.items():render(p).save(OUT/f"{f}.png",optimize=False)
 print(f"Rendered {len(PAGES)} design previews to {OUT}")
 print("Typography uses the approved TTF and firmware role sizes. These are structurally equivalent Pillow previews, not framebuffer-identical output.")
if __name__=="__main__":main()