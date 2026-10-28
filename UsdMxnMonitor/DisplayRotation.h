#pragma once
#include "Widget.h"

struct DisplayRegion { int x,y,width,height; };

// Keep the LCD in its verified native addressing mode. Software rotation avoids
// controller RAM offsets outside the 240x284 active glass at 180/270 degrees.
inline DisplayRegion rotateStrip(const uint16_t *source,uint16_t *dest,int top,int rows,uint8_t rotation) {
  const int width=viewportWidth(rotation);
  switch(rotation) {
    case 1:
      for(int y=0;y<width;++y)for(int x=0;x<rows;++x)dest[y*rows+x]=source[(rows-1-x)*width+y];
      return {WIDTH-top-rows,0,rows,width};
    case 2:
      for(int i=0;i<width*rows;++i)dest[i]=source[width*rows-1-i];
      return {0,HEIGHT-top-rows,width,rows};
    case 3:
      for(int y=0;y<width;++y)for(int x=0;x<rows;++x)dest[y*rows+x]=source[x*width+width-1-y];
      return {top,0,rows,width};
    default:
      return {0,top,width,rows};
  }
}
