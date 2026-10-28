#include <cassert>
#include <fstream>
#include <iostream>
#include "QuoteParser.h"

int main(int argc,char **argv) {
  assert(argc==2);
  std::ifstream input(argv[1]);JsonDocument doc;assert(!deserializeJson(doc,input));
  auto meta=doc["chart"]["result"][0]["meta"];
  uint32_t now=meta["regularMarketTime"].as<uint32_t>()+300;
  Quote q;
  assert(parseQuote(doc,q,now));assert(validQuote(q));
  assert(q.count>50 && q.points[q.count-1].value==q.price);
  assert(isfinite(changePercent(q)));
  Quote original=q;
  // Bad responses must not overwrite the last good quote.
  meta["currency"]="USD";assert(!parseQuote(doc,q,now));assert(q.price==original.price);
  meta["currency"]="MXN";
  meta["regularMarketPrice"]=nullptr;assert(!parseQuote(doc,q,now));
  meta["regularMarketPrice"]=original.price;
  meta["regularMarketTime"]=now+3600;assert(!parseQuote(doc,q,now));
  meta["regularMarketTime"]=original.marketTime;
  auto close=doc["chart"]["result"][0]["indicators"]["quote"][0]["close"];
  close[1]=nullptr;assert(parseQuote(doc,q,now));assert(validQuote(q));
  auto ts=doc["chart"]["result"][0]["timestamp"];
  ts[3]=ts[2];assert(!parseQuote(doc,q,now));
  assert(!validPrice(NAN));assert(!validPrice(INFINITY));assert(!validPrice(0));
  assert(displayBrightness(ScreenMode::On,OFF_MS+1)==170);
  assert(displayBrightness(ScreenMode::Auto,DIM_MS-1)==170);
  assert(displayBrightness(ScreenMode::Auto,DIM_MS)==22);
  assert(displayBrightness(ScreenMode::Auto,OFF_MS)==0);
  assert(displayBrightness(ScreenMode::Dimmed,0)==1);
  assert(displayBrightness(ScreenMode::Dimmed,OFF_MS+1)==1);
  assert(displayBrightness(ScreenMode::Dimmed,0xffffffff)==1);
  assert(nextScreenMode(ScreenMode::On)==ScreenMode::Auto);
  assert(nextScreenMode(ScreenMode::Auto)==ScreenMode::Dimmed);
  assert(nextScreenMode(ScreenMode::Dimmed)==ScreenMode::On);
  PowerButton button;
  assert(button.update(true,1000)==ButtonAction::None);
  assert(button.update(false,1449)==ButtonAction::None);
  assert(button.update(false,1450)==ButtonAction::Refresh);
  assert(button.update(false,1500)==ButtonAction::None); // Single fires once.
  assert(button.update(true,2000)==ButtonAction::None);
  assert(button.update(true,2250)==ButtonAction::ToggleMode);
  assert(button.update(false,3000)==ButtonAction::None); // Double never refreshes.
  assert(button.update(true,3100)==ButtonAction::None);
  assert(button.update(true,3130)==ButtonAction::None); // Ignore contact bounce.
  assert(button.update(false,3550)==ButtonAction::Refresh);
  assert(button.update(true,4000)==ButtonAction::None);
  assert(button.update(true,4450)==ButtonAction::ToggleMode); // Window boundary.
  assert(button.update(true,5000)==ButtonAction::None);
  assert(button.update(true,5500)==ButtonAction::Refresh); // Late second press.
  assert(button.update(false,5950)==ButtonAction::Refresh);
  assert(button.update(false,6000)==ButtonAction::None);
  PowerButton wrapped;
  assert(wrapped.update(true,0xffffff00)==ButtonAction::None);
  assert(wrapped.update(true,44)==ButtonAction::ToggleMode);
  assert(wrapped.update(true,1000)==ButtonAction::None);
  assert(wrapped.update(true,1200)==ButtonAction::ToggleMode);
  assert(wrapped.update(true,1400)==ButtonAction::None); // Third press starts anew.
  assert(wrapped.update(false,1850)==ButtonAction::Refresh);
  // A desk bump and ordinary movement must not count as a shake.
  ShakeDetector shake;
  assert(!shake.update(1,100));assert(!shake.update(2.9,200));assert(!shake.update(1,220));
  assert(!shake.update(1,950));assert(!shake.update(2.9,1000));
  assert(!shake.update(1,1080));assert(shake.update(3.1,1150));
  assert(!shake.update(1,1200));assert(!shake.update(3.2,1280));
  assert(!shake.update(1,4000));assert(!shake.update(3.2,4100));assert(!shake.update(1,4180));assert(shake.update(3.2,4260));
  // Timer arithmetic across millis() wrap-around.
  assert(uint32_t(uint32_t(100)-uint32_t(0xfffffff0))==116);
  std::cout<<"PASS: quotes, screen timers, single/double button presses, debounce, wrap-around, shake gating\n";
}
