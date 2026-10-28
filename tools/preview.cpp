#include <fstream>
#include <iostream>
#include "Widget.h"
#include "QuoteParser.h"

int main(int argc,char **argv) {
  if(argc<3)return 2;
  std::ifstream input(argv[1]);JsonDocument doc;
  if(deserializeJson(doc,input))return 3;
  View s;
  uint32_t now=doc["chart"]["result"][0]["meta"]["regularMarketTime"].as<uint32_t>()+300;
  if(!parseQuote(doc,s.quote,now))return 4;
  s.hasQuote=true;s.battery=78;
  strcpy(s.status,"Yahoo / 21:35");strcpy(s.quoteTime,"27/10 21:34");
  if(argc>3 && !strcmp(argv[3],"offline")){s.stale=true;s.battery=15;strcpy(s.status,"Sin conexion");}
  if(argc>3 && !strcmp(argv[3],"busy")){s.busy=true;s.frame=4;s.battery=43;s.charging=true;}
  if(argc>3 && !strcmp(argv[3],"empty")){s.hasQuote=false;s.battery=-1;strcpy(s.status,"Conectando Wi-Fi...");strcpy(s.quoteTime,"");}
  if(argc>3 && !strcmp(argv[3],"charging")){s.charging=true;s.chargeAnimationMs=argc>4?strtoul(argv[4],nullptr,10):1200;s.battery=argc>5?atoi(argv[5]):78;}
  for(int i=3;i<argc;++i)if(!strcmp(argv[i],"--landscape"))s.rotation=1;
  const int width=viewportWidth(s.rotation),height=viewportHeight(s.rotation);
  uint16_t buffer[MAX_WIDTH*STRIP_HEIGHT];
  if(argc>3 && !strcmp(argv[3],"plugged-full")){s.battery=100;s.usbConnected=true;}
  if(argc>3 && !strcmp(argv[3],"unplugged-full")){s.battery=100;}
  std::ofstream output(argv[2],std::ios::binary);output<<"P6\n"<<width<<" "<<height<<"\n255\n";
  for(int top=0;top<height;top+=STRIP_HEIGHT){
    int h=height-top<STRIP_HEIGHT?height-top:STRIP_HEIGHT;
    Canvas c{buffer,top,h,width,height};drawWidget(c,s);
    for(int i=0;i<width*h;++i){auto p=buffer[i];char rgb[3]={char(((p>>11)&31)*255/31),char(((p>>5)&63)*255/63),char((p&31)*255/31)};output.write(rgb,3);}
  }
  std::cout<<"Rendered actual firmware UI: "<<argv[2]<<"\n";
}
