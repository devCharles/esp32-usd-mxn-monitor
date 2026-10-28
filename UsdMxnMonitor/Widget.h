#pragma once
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "Model.h"
#include "Fonts.h"

constexpr int WIDTH=240, HEIGHT=284, STRIP_HEIGHT=20;
constexpr int MAX_WIDTH=284;
inline int viewportWidth(uint8_t rotation){return rotation&1?HEIGHT:WIDTH;}
inline int viewportHeight(uint8_t rotation){return rotation&1?WIDTH:HEIGHT;}
struct Color { uint8_t r,g,b; };
constexpr Color WHITE{235,235,240}, GRAY{139,139,148}, GREEN{48,209,88}, RED{255,83,94}, BLUE{74,172,255};
inline uint16_t rgb565(Color c) { return ((c.r&248)<<8)|((c.g&252)<<3)|(c.b>>3); }

// The same bounded renderer runs on the device and in the desktop preview.
// Each strip consumes at most 11.36 KB, leaving RAM for verified TLS and Wi-Fi.
struct Canvas {
  uint16_t *buffer;
  int top, height;
  int width=WIDTH,screenHeight=HEIGHT;
  void pixel(int x,int y,Color c,uint8_t alpha=255) {
    if (x<0 || x>=width || y<top || y>=top+height || !alpha) return;
    auto &d=buffer[(y-top)*width+x];
    if (alpha<255) {
      int r=((d>>11)&31)*255/31, g=((d>>5)&63)*255/63, b=(d&31)*255/31;
      c={uint8_t((c.r*alpha+r*(255-alpha))/255),uint8_t((c.g*alpha+g*(255-alpha))/255),uint8_t((c.b*alpha+b*(255-alpha))/255)};
    }
    d=rgb565(c);
  }
  void rect(int x,int y,int w,int h,Color c,uint8_t alpha=255) {
    int y0=y>top?y:top, y1=y+h<top+height?y+h:top+height;
    for(int yy=y0;yy<y1;++yy) for(int xx=x;xx<x+w;++xx) pixel(xx,yy,c,alpha);
  }
  void line(int x0,int y0,int x1,int y1,Color c,uint8_t alpha=255) {
    int dx=abs(x1-x0),sx=x0<x1?1:-1,dy=-abs(y1-y0),sy=y0<y1?1:-1,e=dx+dy;
    while(true) { pixel(x0,y0,c,alpha); if(x0==x1 && y0==y1)break; int e2=2*e; if(e2>=dy){e+=dy;x0+=sx;} if(e2<=dx){e+=dx;y0+=sy;} }
  }
  int textWidth(const char *s,const Font &f) {
    int w=0; for(;*s;++s) if(uint8_t(*s)>=f.first && uint8_t(*s)<=f.last) w+=f.glyphs[uint8_t(*s)-f.first].advance;
    return w;
  }
  void text(const char *s,int x,int baseline,const Font &f,Color c) {
    for(;*s;++s) {
      uint8_t ch=*s; if(ch<f.first || ch>f.last)continue;
      const Glyph &g=f.glyphs[ch-f.first];
      if(baseline+g.top<top+height && baseline+g.top+g.h>top) {
        for(int yy=0;yy<g.h;++yy)for(int xx=0;xx<g.w;++xx) {
          int n=yy*g.w+xx;uint8_t packed=f.pixels[g.offset+n/2];
          pixel(x+g.left+xx,baseline+g.top+yy,c,((n&1)?(packed&15):(packed>>4))*17);
        }
      }
      x+=g.advance;
    }
  }
  void right(const char *s,int x,int baseline,const Font &f,Color c) { text(s,x-textWidth(s,f),baseline,f,c); }
};

struct View {
  Quote quote;
  bool hasQuote=false, stale=false, busy=false, charging=false, usbConnected=false;
  ScreenMode mode=ScreenMode::On;
  int battery=-1;
  unsigned frame=0;
  uint32_t chargeAnimationMs=0;
  uint8_t rotation=0;
  char status[48]="Conectando Wi-Fi...";
  char quoteTime[24]="";
};

inline bool batteryBarVisible(const View &s) {
  return !(s.usbConnected && s.battery==100);
}

inline void drawBatteryBar(Canvas &c,const View &s) {
  if(!batteryBarVisible(s))return;
  // Gray track distinguishes an empty/unknown battery from the dark background.
  c.rect(0,5,c.width,3,{46,46,50});
  if(s.battery>=0) {
    Color battery=s.charging?BLUE:(s.battery<=20?RED:GREEN);
    int level=s.battery>100?100:s.battery;
    int filled=c.width*level/100;
    c.rect(0,5,filled,3,battery);
    if(s.charging) {
      // Clip the traveling highlight to the actual charge level.
      constexpr int radius=28;
      int center=-radius+int((s.chargeAnimationMs%2400)*(filled+2*radius)/2400);
      for(int x=0;x<filled;++x) {
        int distance=abs(x-center);
        if(distance<radius) {
          int strength=(radius-distance)*255/radius;
          c.rect(x,5,1,3,{230,248,255},uint8_t(strength*strength/255));
        }
      }
    }
  } else {
    for(int x=0;x<c.width;x+=8)c.rect(x,5,4,3,GRAY);
  }
}

inline void drawWidget(Canvas &c,const View &s) {
  bool landscape=c.width>c.screenHeight;
  int right=c.width-20,headerShift=landscape?6:0;
  int periodY=landscape?82:93;
  int priceY=c.screenHeight-(landscape?35:37),footerY=c.screenHeight-17;
  for(int y=c.top;y<c.top+c.height;++y) {
    const int v=24-(y*21/c.screenHeight);
    // Ordered dithering prevents visible bands in the RGB565 dark gradient.
    static const uint8_t dither[4][4]={{0,4,1,5},{6,2,7,3},{1,5,0,4},{7,3,6,2}};
    for(int x=0;x<c.width;++x){int d=dither[y%4][x%4];c.pixel(x,y,{uint8_t(v+d),uint8_t(v+d/2),uint8_t(v+1+d)});}
  }
  drawBatteryBar(c,s);
  Color accent=s.hasQuote?(change(s.quote)>=0?GREEN:RED):GRAY;
  // Filled triangle, mirroring the reference widget's direction marker.
  for(int y=0;y<11;++y) {
    int w=s.hasQuote && change(s.quote)<0?11-y:y+1;
    c.rect(20+(11-w)/2,31+y-headerShift,w,1,accent);
  }
  c.text("MXN=X",37,44-headerShift,fontTitle,WHITE);
  c.text("USD/MXN",20,66-headerShift,fontLabel,GRAY);
  char buf[48];
  if(s.hasQuote) {
    snprintf(buf,sizeof(buf),"%+.2f",change(s.quote));c.right(buf,right,43-headerShift,fontLabel,accent);
    snprintf(buf,sizeof(buf),"%+.2f%%",changePercent(s.quote));c.right(buf,right,65-headerShift,fontLabel,accent);
  }
  c.text("5 DIAS",20,periodY,fontSmall,GRAY);
  c.right(s.quoteTime,right,periodY,fontSmall,GRAY);

  int left=20,top=landscape?97:111,bottom=landscape?144:181;
  if(s.hasQuote) {
    const Quote &q=s.quote;
    float lo=q.price,hi=q.price;
    for(unsigned i=0;i<q.count;++i) { lo=fminf(lo,q.points[i].value);hi=fmaxf(hi,q.points[i].value); }
    float pad=fmaxf((hi-lo)*0.15f,0.015f);lo-=pad;hi+=pad;
    auto py=[&](float v){return bottom-int((v-lo)/(hi-lo)*(bottom-top));};
    int baseline=py(q.points[0].value);
    for(int x=left;x<right;x+=6)c.rect(x,baseline,3,1,accent,100);
    // Equal spacing between hourly market observations omits closed weekends,
    // as finance widgets typically do. Null observations are removed by parser.
    for(unsigned i=1;i<q.count;++i) {
      int x0=left+(right-left)*(i-1)/(q.count-1),x1=left+(right-left)*i/(q.count-1);
      int y0=py(q.points[i-1].value),y1=py(q.points[i].value);
      for(int x=x0;x<x1;++x) {
        int yy=y0+(y1-y0)*(x-x0)/(x1-x0);
        for(int y=yy+1;y<=bottom+7;++y)c.pixel(x,y,accent,uint8_t(42*(bottom+8-y)/(bottom+8-top)));
      }
      c.line(x0,y0,x1,y1,accent);c.line(x0,y0+1,x1,y1+1,accent,175);
    }
    snprintf(buf,sizeof(buf),"%.2f",q.price);
    if(c.textWidth(buf,fontPrice)<=c.width-36)c.right(buf,right+2,priceY,fontPrice,WHITE);
    else c.right(buf,right,priceY-8,fontTitle,WHITE);
  } else {
    for(int x=left;x<right;x+=8)c.rect(x,(top+bottom)/2,4,1,GRAY,100);
    c.right("--.--",right+2,priceY,fontPrice,GRAY);
  }
  Color statusColor=s.stale?Color{255,178,76}:GRAY;
  if(s.busy) {
    int phase=s.frame%12;
    for(int i=0;i<8;++i)c.rect(20+i*3,footerY-6,2,2,BLUE,uint8_t(30+((i+phase)%8)*28));
    c.text("Actualizando...",50,footerY,fontSmall,BLUE);
  } else c.text(s.status,20,footerY,fontSmall,statusColor);
  c.right(screenModeLabel(s.mode),right,footerY,fontSmall,GRAY);
}
