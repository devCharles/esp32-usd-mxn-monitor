#pragma once
#include <stdint.h>
#include <math.h>

constexpr int MAX_POINTS = 160;
constexpr uint32_t REFRESH_MS = 300000;
constexpr uint32_t MANUAL_COOLDOWN_MS = 15000;
constexpr uint32_t DIM_MS = 30000;
constexpr uint32_t OFF_MS = 60000;
constexpr uint32_t DOUBLE_PRESS_MS = 450;
constexpr uint8_t MIN_BRIGHTNESS = 1; // Lowest nonzero duty on the 8-bit backlight PWM.

enum class ScreenMode : uint8_t { On=0, Auto=1, Dimmed=2 };
inline ScreenMode nextScreenMode(ScreenMode mode) {
  switch(mode) {
    case ScreenMode::On:return ScreenMode::Auto;
    case ScreenMode::Auto:return ScreenMode::Dimmed;
    default:return ScreenMode::On;
  }
}
inline const char *screenModeLabel(ScreenMode mode) {
  switch(mode) {
    case ScreenMode::Auto:return "AUTO";
    case ScreenMode::Dimmed:return "DIM";
    default:return "ON";
  }
}

enum class ButtonAction { None, Refresh, ToggleMode };

// Delay a single press until the double-press window closes. A double press
// consumes both presses so changing mode never also requests a quote.
struct PowerButton {
  uint32_t firstPress=0,lastPress=0;
  bool pending=false,seenPress=false;
  ButtonAction update(bool pressed,uint32_t now) {
    if(pressed && seenPress && uint32_t(now-lastPress)<80)pressed=false;
    if(pressed) {
      lastPress=now;seenPress=true;
      if(pending && uint32_t(now-firstPress)<=DOUBLE_PRESS_MS) {
        pending=false;return ButtonAction::ToggleMode;
      }
      bool expired=pending;
      firstPress=now;pending=true;
      return expired?ButtonAction::Refresh:ButtonAction::None;
    }
    if(pending && uint32_t(now-firstPress)>=DOUBLE_PRESS_MS) {
      pending=false;return ButtonAction::Refresh;
    }
    return ButtonAction::None;
  }
};

struct Point { uint32_t time; float value; };
struct Quote {
  float price = 0;
  uint32_t marketTime = 0;
  uint32_t fetchedAt = 0;
  uint16_t count = 0;
  Point points[MAX_POINTS] = {};
};

inline bool validPrice(float value) { return isfinite(value) && value > 0 && value < 1000; }
inline bool validQuote(const Quote &q) {
  if (!validPrice(q.price) || q.marketTime < 1700000000 || q.count < 2 || q.count > MAX_POINTS) return false;
  for (unsigned i=0; i<q.count; ++i) {
    if (!validPrice(q.points[i].value) || q.points[i].time < 1700000000 || q.points[i].time > q.marketTime) return false;
    if (i && q.points[i].time <= q.points[i-1].time) return false;
  }
  return true;
}
inline float change(const Quote &q) { return q.price - q.points[0].value; }
inline float changePercent(const Quote &q) { return 100.0f * change(q) / q.points[0].value; }
inline uint8_t displayBrightness(ScreenMode mode, uint32_t idleMs) {
  if (mode==ScreenMode::Dimmed) return MIN_BRIGHTNESS;
  if (mode==ScreenMode::On || idleMs < DIM_MS) return 170;
  return idleMs < OFF_MS ? 22 : 0;
}

// Input axes are normalized for the display (+Y upright, +X clockwise image).
// Require a stable, mostly upright
// gravity vector; lying flat, diagonal poses and shaking retain the last view.
struct OrientationDetector {
  uint8_t rotation=0,candidate=0;
  uint32_t candidateSince=0,lastSample=0;
  float filteredX=0,filteredY=0;
  bool initialized=false,pending=false;
  // Verified on this Waveshare board: upright => raw +X; clockwise 90° => raw -Y.
  bool updateBoard(float x,float y,float z,uint32_t now) {return update(y,x,z,now);}
  bool update(float x,float y,float z,uint32_t now) {
    float norm=x*x+y*y+z*z;
    if(!isfinite(norm) || norm<0.64f || norm>1.44f) {
      pending=false;initialized=false;return false;
    }
    if(!initialized || uint32_t(now-lastSample)>150) {
      filteredX=x;filteredY=y;initialized=true;pending=false;
    } else {filteredX+=0.12f*(x-filteredX);filteredY+=0.12f*(y-filteredY);}
    lastSample=now;
    float ax=fabsf(filteredX),ay=fabsf(filteredY);
    if(fmaxf(ax,ay)<0.65f || fabsf(z)>0.75f || fmaxf(ax,ay)<1.35f*fminf(ax,ay)) {
      pending=false;return false;
    }
    uint8_t proposed=ay>ax?(filteredY>0?0:2):(filteredX>0?1:3);
    if(proposed==rotation){pending=false;return false;}
    if(!pending || candidate!=proposed){candidate=proposed;candidateSince=now;pending=true;return false;}
    if(uint32_t(now-candidateSince)<700)return false;
    rotation=candidate;pending=false;return true;
  }
};

// Require two strong peaks separated by a release, not just a single desk bump.
struct ShakeDetector {
  uint32_t firstPeak = 0, lastTrigger = 0;
  bool peaked = false, armed = false, triggered = false;
  bool update(float magnitude, uint32_t now) {
    if (!isfinite(magnitude)) return false;
    if (triggered && uint32_t(now-lastTrigger) < 2500) return false;
    if (peaked && uint32_t(now-firstPeak) > 700) peaked = false;
    if (magnitude < 1.4f) armed = true;
    if (magnitude < 2.5f || !armed) return false;
    armed = false;
    if (peaked && uint32_t(now-firstPeak) >= 60) {
      peaked=false; triggered=true; lastTrigger=now; return true;
    }
    firstPeak=now; peaked=true; return false;
  }
};
