#include <cassert>
#include <vector>
#include <iostream>
#include "DisplayRotation.h"

int main() {
  OrientationDetector o;
  uint32_t now=100;
  auto hold=[&](float x,float y,float z,int samples=120){for(int i=0;i<samples;++i){o.update(x,y,z,now);now+=10;}};
  hold(0,1,0);assert(o.rotation==0);
  hold(1,0,0,20);hold(0,1,0);assert(o.rotation==0); // A brief turn must not rotate.
  hold(1,0,0);assert(o.rotation==1);
  hold(0,0,1);assert(o.rotation==1); // Flat on a table retains last view.
  hold(0,-1,0);assert(o.rotation==2);
  hold(-1,0,0);assert(o.rotation==3);
  hold(.71f,.71f,0);assert(o.rotation==3); // No diagonal flapping.
  hold(3,0,0);assert(o.rotation==3); // Strong shaking is not orientation.
  hold(0,1,0,20);assert(o.rotation==3);
  hold(0,1,0);assert(o.rotation==0);
  OrientationDetector wrap;
  for(uint32_t i=0;i<100;++i)wrap.update(1,0,0,uint32_t(0xffffff00)+i*10);
  assert(wrap.rotation==1);
  OrientationDetector board;
  uint32_t boardTime=0;
  auto pose=[&](float x,float y){for(int i=0;i<120;++i){board.updateBoard(x,y,0,boardTime);boardTime+=10;}};
  pose(1,0);assert(board.rotation==0);
  pose(0,-1);assert(board.rotation==3); // Device clockwise => counterclockwise image.
  pose(-1,0);assert(board.rotation==2);
  pose(0,1);assert(board.rotation==1);

  // Verify every pixel, panel boundary and final partial strip in all 4 rotations.
  for(uint8_t rotation=0;rotation<4;++rotation) {
    int w=viewportWidth(rotation),h=viewportHeight(rotation);
    std::vector<uint16_t> logical(w*h),native(WIDTH*HEIGHT),source(MAX_WIDTH*STRIP_HEIGHT),dest(MAX_WIDTH*STRIP_HEIGHT+2,0xbeef);
    std::vector<int> coverage(WIDTH*HEIGHT,0);
    for(int i=0;i<w*h;++i)logical[i]=uint16_t(i*17+5);
    for(int top=0;top<h;top+=STRIP_HEIGHT) {
      int rows=std::min(STRIP_HEIGHT,h-top);
      std::copy(logical.begin()+top*w,logical.begin()+(top+rows)*w,source.begin());
      auto r=rotateStrip(source.data(),dest.data()+1,top,rows,rotation);
      assert(r.x>=0 && r.y>=0 && r.x+r.width<=WIDTH && r.y+r.height<=HEIGHT);
      assert(dest.front()==0xbeef && dest.back()==0xbeef);
      const uint16_t *pixels=rotation?dest.data()+1:source.data();
      for(int y=0;y<r.height;++y)for(int x=0;x<r.width;++x) {
        int pos=(r.y+y)*WIDTH+r.x+x;native[pos]=pixels[y*r.width+x];++coverage[pos];
      }
    }
    for(int n:coverage)assert(n==1);
    for(int y=0;y<h;++y)for(int x=0;x<w;++x) {
      int px=x,py=y;
      if(rotation==1){px=WIDTH-1-y;py=x;}
      if(rotation==2){px=WIDTH-1-x;py=HEIGHT-1-y;}
      if(rotation==3){px=y;py=HEIGHT-1-x;}
      assert(native[py*WIDTH+px]==logical[y*w+x]);
    }
  }
  std::cout<<"PASS: stable orientation, shake/flat/diagonal rejection, timer wrap, all pixels in four rotations\n";
}
