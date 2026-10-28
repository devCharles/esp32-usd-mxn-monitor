#define XPOWERS_CHIP_AXP2101
#include <Arduino.h>
#include <Wire.h>
#include <Arduino_GFX_Library.h>
#include <XPowersLib.h>
#include <SensorQMI8658.hpp>
#include <SensorPCF85063.hpp>
#include <TouchDrvCSTXXX.hpp>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <time.h>
#include "Widget.h"
#include "QuoteParser.h"
#include "RootCA.h"
#include "BuildTime.h"
#if __has_include("secrets.h")
#include "secrets.h"
#else
static const char WIFI_SSID[]="";
static const char WIFI_PASSWORD[]="";
#endif

constexpr int LCD_BL=6, SDA_PIN=7, SCL_PIN=8, TOUCH_INT=11;
Arduino_HWSPI bus(3,5,1,2);
Arduino_ST7789 display(&bus,4,0,true,WIDTH,HEIGHT);
XPowersPMU power;
SensorQMI8658 imu;
SensorPCF85063 rtc;
TouchDrvCSTXXX touch;
Preferences prefs;
View view;
ShakeDetector shake;
PowerButton powerButton;
bool pmuOK=false, imuOK=false, touchOK=false, dirty=true;
bool rtcOK=false;
uint16_t strip[WIDTH*STRIP_HEIGHT];
uint32_t lastActivity=0,lastRequest=0,lastResult=0,lastBattery=0,lastPmu=0,lastSensor=0,lastDraw=0;
uint32_t lastTouch=0,noticeUntil=0,retryDelay=REFRESH_MS,blockedUntil=0;
uint32_t chargeStarted=0,lastChargeDraw=0;
bool requestedBefore=false,receivedBefore=false,wasPressed=false;
uint8_t brightness=255;
volatile bool touchInterrupt=false;
char normalStatus[48]="Conectando Wi-Fi...";

struct FetchResult {
  Quote quote;
  bool ok=false;
  int httpCode=0;
  uint32_t serverTime=0;
  char error[40]="Sin conexion";
};
void fetchQuote(FetchResult &result);
QueueHandle_t results;
TaskHandle_t networkTaskHandle=nullptr;

// TZ is fixed to Mexico City (CST6); convert a UTC calendar without mutating TZ.
uint32_t utcEpoch(struct tm value) {
  value.tm_isdst=0;
  return uint32_t(mktime(&value)-21600);
}

void setSystemClock(uint32_t epoch) {
  struct timeval tv={time_t(epoch),0};settimeofday(&tv,nullptr);
}

void saveRtc(uint32_t epoch) {
  if(!rtcOK)return;
  time_t t=epoch;struct tm utc;gmtime_r(&t,&utc);
  rtc.setDateTime(RTC_DateTime(utc));prefs.putBool("clock-v1",true);
}

void initClock() {
  rtcOK=rtc.begin(Wire,SDA_PIN,SCL_PIN);
  uint32_t epoch=0;
  if(rtcOK && prefs.getBool("clock-v1",false) && rtc.isClockIntegrityGuaranteed()) {
    RTC_DateTime d=rtc.getDateTime();
    if(d.getYear()>=2026 && d.getMonth()>=1 && d.getMonth()<=12 && d.getDay()>=1 && d.getDay()<=31) {
      struct tm utc={};utc.tm_year=d.getYear()-1900;utc.tm_mon=d.getMonth()-1;utc.tm_mday=d.getDay();
      utc.tm_hour=d.getHour();utc.tm_min=d.getMinute();utc.tm_sec=d.getSecond();epoch=utcEpoch(utc);
    }
  }
  if(epoch<BUILD_UNIX_TIME-300) {
    // Trusted local build time bootstraps TLS once; HTTPS Date refines it.
    epoch=BUILD_UNIX_TIME;saveRtc(epoch);
    Serial.println("CLOCK seeded from build host");
  } else Serial.println("CLOCK restored from RTC");
  setSystemClock(epoch);
  Serial.printf("CLOCK rtc=%d epoch=%lu\n",rtcOK,(unsigned long)epoch);
}

// Bound all response storage even if the upstream changes its response format.
class BoundedBody : public Stream {
 public:
  String data;
  bool overflow=false;
  BoundedBody() { data.reserve(16384); }
  size_t write(uint8_t b) override { return write(&b,1); }
  size_t write(const uint8_t *p,size_t n) override {
    if(data.length()+n>32768){overflow=true;return 0;}
    return data.concat(reinterpret_cast<const char*>(p),n)?n:0;
  }
  int available() override{return 0;}
  int read() override{return -1;}
  int peek() override{return -1;}
  void flush() override{}
};

void fetchQuote(FetchResult &result) {
  if(!strlen(WIFI_SSID)) {strcpy(result.error,"Configura Wi-Fi por USB");return;}
  if(WiFi.status()!=WL_CONNECTED) {
    WiFi.disconnect();
    WiFi.begin(WIFI_SSID,WIFI_PASSWORD);
    uint32_t started=millis();
    while(WiFi.status()!=WL_CONNECTED && uint32_t(millis()-started)<20000) vTaskDelay(pdMS_TO_TICKS(100));
  }
  if(WiFi.status()!=WL_CONNECTED)return;
  if(time(nullptr)<1700000000) {
    configTzTime("CST6","time.cloudflare.com","pool.ntp.org","time.google.com");
    uint32_t started=millis();
    while(time(nullptr)<1700000000 && uint32_t(millis()-started)<12000) vTaskDelay(pdMS_TO_TICKS(100));
  }
  if(time(nullptr)<1700000000) {strcpy(result.error,"Sin hora de internet");return;}

  WiFiClientSecure tls;
  tls.setCACert(YAHOO_ROOT_CA);
  tls.setHandshakeTimeout(12);
  HTTPClient http;
  http.useHTTP10(true);
  http.setConnectTimeout(10000);
  http.setTimeout(10000);
  http.setUserAgent("Mozilla/5.0 (USD-MXN personal display)");
  if(!http.begin(tls,"https://query1.finance.yahoo.com/v8/finance/chart/MXN=X?range=5d&interval=60m")) {
    strcpy(result.error,"Error de conexion HTTPS");return;
  }
  const char *headers[]={"Date"};http.collectHeaders(headers,1);
  result.httpCode=http.GET();
  if(result.httpCode!=HTTP_CODE_OK) {
    strcpy(result.error,result.httpCode==429?"Limite API: espera 5 min":"Error al consultar Yahoo");
    http.end();return;
  }
  struct tm serverDate={};
  String date=http.header("Date");
  if(strptime(date.c_str(),"%a, %d %b %Y %H:%M:%S GMT",&serverDate)) {
    uint32_t epoch=utcEpoch(serverDate);
    if(epoch>=BUILD_UNIX_TIME-300) {result.serverTime=epoch;setSystemClock(epoch);}
  }
  BoundedBody body;
  int readBytes=http.getSize()>32768 ? -1 : http.writeToStream(&body);
  http.end();
  if(readBytes<0 || body.overflow) {strcpy(result.error,"Respuesta incompleta");return;}
  JsonDocument filter,doc;
  auto f=filter["chart"]["result"][0];
  f["meta"]["symbol"]=true;f["meta"]["currency"]=true;
  f["meta"]["regularMarketPrice"]=true;f["meta"]["regularMarketTime"]=true;
  f["timestamp"]=true;f["indicators"]["quote"][0]["close"]=true;
  filter["chart"]["error"]=true;
  auto err=deserializeJson(doc,body.data,DeserializationOption::Filter(filter));
  if(err || !parseQuote(doc,result.quote,uint32_t(time(nullptr)))) {
    strcpy(result.error,"Datos no validos");return;
  }
  result.ok=true;
}

void networkWorker(void*) {
  for(;;) {
    ulTaskNotifyTake(pdTRUE,portMAX_DELAY);
    FetchResult result;
    fetchQuote(result);
    xQueueOverwrite(results,&result);
  }
}

void showNotice(const char *message,uint32_t duration=4000) {
  snprintf(view.status,sizeof(view.status),"%s",message);
  noticeUntil=millis()+duration;
  dirty=true;
}

void wakeScreen() {lastActivity=millis();dirty=true;}

void requestRefresh(bool manual,const char *trigger) {
  uint32_t now=millis();
  if(manual)wakeScreen();
  if(view.busy)return;
  if(blockedUntil && int32_t(blockedUntil-now)>0) {
    if(manual)showNotice("Limite API: espera 5 min");
    return;
  }
  if(requestedBefore && uint32_t(now-lastRequest)<MANUAL_COOLDOWN_MS) {
    if(manual){char b[40];snprintf(b,sizeof(b),"Espera %lus",(unsigned long)((MANUAL_COOLDOWN_MS-(now-lastRequest)+999)/1000));showNotice(b);}
    return;
  }
  if(!networkTaskHandle){showNotice("Error de memoria");return;}
  view.busy=true;dirty=true;lastRequest=now;requestedBefore=true;
  Serial.printf("REFRESH start trigger=%s heap=%u\n",trigger,ESP.getFreeHeap());
  xTaskNotifyGive(networkTaskHandle);
}

void formatTime(uint32_t epoch,char *out,size_t len,const char *format) {
  time_t t=epoch;
  struct tm local;
  localtime_r(&t,&local);
  strftime(out,len,format,&local);
}

void handleResult() {
  FetchResult result;
  if(xQueueReceive(results,&result,0)!=pdTRUE)return;
  view.busy=false;lastResult=millis();receivedBefore=true;dirty=true;
  if(result.ok && view.hasQuote && result.quote.marketTime<view.quote.marketTime) {
    result.ok=false;strcpy(result.error,"Cotizacion atrasada");
  }
  if(result.ok) {
    if(result.serverTime)saveRtc(result.serverTime);
    bool unchanged=view.hasQuote && view.quote.price==result.quote.price;
    bool save=!view.hasQuote || view.quote.marketTime!=result.quote.marketTime || view.quote.price!=result.quote.price;
    view.quote=result.quote;view.hasQuote=true;view.stale=false;retryDelay=REFRESH_MS;
    char checked[12];formatTime(result.quote.fetchedAt,checked,sizeof(checked),"%H:%M");
    snprintf(normalStatus,sizeof(normalStatus),"Yahoo / %s",checked);
    char message[48];snprintf(message,sizeof(message),"%s %s",unchanged?"Sin cambios":"Actualizado",checked);
    showNotice(message,7000);
    formatTime(result.quote.marketTime,view.quoteTime,sizeof(view.quoteTime),"%d/%m %H:%M");
    // Persist only changed market samples, not every tap or closed-market poll.
    if(save)prefs.putBytes("quote-v1",&view.quote,sizeof(view.quote));
    Serial.printf("QUOTE ok price=%.4f points=%u market=%lu battery=%d heap=%u\n",view.quote.price,view.quote.count,(unsigned long)view.quote.marketTime,view.battery,ESP.getFreeHeap());
  } else {
    view.stale=true;retryDelay=result.httpCode==429?REFRESH_MS:60000;
    if(result.httpCode==429)blockedUntil=millis()+REFRESH_MS;
    snprintf(normalStatus,sizeof(normalStatus),"%s",result.error);
    showNotice(normalStatus,7000);
    Serial.printf("QUOTE error http=%d reason=%s heap=%u\n",result.httpCode,result.error,ESP.getFreeHeap());
  }
}

void toggleMode() {
  view.mode=nextScreenMode(view.mode);
  prefs.putUChar("screen-mode",static_cast<uint8_t>(view.mode));
  wakeScreen();
  const char *message=view.mode==ScreenMode::Dimmed?"Dimmed: brillo minimo":
                      view.mode==ScreenMode::Auto?"Auto: se apaga en 60s":"Siempre encendida";
  showNotice(message,5000);
  Serial.printf("MODE %s\n",screenModeLabel(view.mode));
}

void readBattery() {
  int battery=pmuOK?power.getBatteryPercent():-1;
  if(battery<0 || battery>100)battery=-1;
  bool charging=pmuOK && power.isCharging();
  bool usbConnected=pmuOK && power.isVbusIn();
  if(charging && !view.charging)chargeStarted=millis();
  if(battery!=view.battery || charging!=view.charging || usbConnected!=view.usbConnected) {
    dirty=true;
    Serial.printf("BATTERY percent=%d charging=%d usb=%d\n",battery,charging,usbConnected);
  }
  view.battery=battery;view.charging=charging;view.usbConnected=usbConnected;
}

void initHardware() {
  Wire.begin(SDA_PIN,SCL_PIN);
  Wire.setTimeOut(30);
  pmuOK=power.begin(Wire,AXP2101_SLAVE_ADDRESS,SDA_PIN,SCL_PIN);
  if(pmuOK) {
    // Preserve factory power rails, charge voltage and charge current.
    // Only enable telemetry and the short-press event; never reset the gauge.
    power.enableBattDetection();power.enableBattVoltageMeasure();power.enableVbusVoltageMeasure();power.enableGauge();
    power.disableIRQ(XPOWERS_AXP2101_ALL_IRQ);power.clearIrqStatus();
    power.enableIRQ(XPOWERS_AXP2101_PKEY_SHORT_IRQ);
  }
  touch.setPins(-1,TOUCH_INT);
  touchOK=touch.begin(Wire,0x15,SDA_PIN,SCL_PIN);
  if(touchOK) {
    pinMode(TOUCH_INT,INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(TOUCH_INT),[](){touchInterrupt=true;},FALLING);
  }
  imuOK=imu.begin(Wire,QMI8658_L_SLAVE_ADDRESS,SDA_PIN,SCL_PIN);
  if(imuOK) {
    imu.configAccelerometer(SensorQMI8658::ACC_RANGE_8G,SensorQMI8658::ACC_ODR_125Hz,SensorQMI8658::LPF_OFF);
    imu.disableGyroscope();imu.enableAccelerometer();
  }
  if(!display.begin(40000000))Serial.println("ERROR display init");
  ledcAttach(LCD_BL,5000,8);
  brightness=displayBrightness(view.mode,0);
  ledcWrite(LCD_BL,brightness);
  readBattery();
  Serial.printf("HARDWARE pmu=%d touch=%d imu=%d battery=%d charging=%d voltage=%u\n",pmuOK,touchOK,imuOK,view.battery,view.charging,pmuOK?power.getBattVoltage():0);
}

void drawScreen(bool screenshot=false) {
  // Freeze the frame across all strips so the spinner never tears.
  if(screenshot)Serial.println("FRAME RGB565 240 284");
  for(int top=0;top<HEIGHT;top+=STRIP_HEIGHT) {
    int h=HEIGHT-top<STRIP_HEIGHT?HEIGHT-top:STRIP_HEIGHT;
    Canvas c{strip,top,h};drawWidget(c,view);
    if(screenshot)Serial.write(reinterpret_cast<uint8_t*>(strip),WIDTH*h*2);
    else display.draw16bitRGBBitmap(0,top,strip,WIDTH,h);
    yield();
  }
  if(screenshot)Serial.println("\nFRAME END");
}

void drawChargingBar() {
  // Animate just 720 pixels without redrawing the quote or changing backlight.
  Canvas c{strip,5,3};drawBatteryBar(c,view);
  display.draw16bitRGBBitmap(0,5,strip,WIDTH,3);
}

void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println("USD/MXN Monitor v1.4");
  setenv("TZ","CST6",1);tzset(); // Mexico City, UTC-6 year-round.
  prefs.begin("usd-mxn",false);
  // Migrate the saved ON/AUTO preference from v1.1 without losing the user's mode.
  uint8_t legacyMode=prefs.getBool("always-on",true)?0:1;
  uint8_t savedMode=prefs.getUChar("screen-mode",legacyMode);
  view.mode=savedMode<=2?static_cast<ScreenMode>(savedMode):ScreenMode::On;
  if(prefs.getBytesLength("quote-v1")==sizeof(Quote)) {
    Quote cached;
    prefs.getBytes("quote-v1",&cached,sizeof(cached));
    if(validQuote(cached)) {
      view.quote=cached;view.hasQuote=true;view.stale=true;
      formatTime(cached.marketTime,view.quoteTime,sizeof(view.quoteTime),"%d/%m %H:%M");
      strcpy(normalStatus,"Datos guardados");strcpy(view.status,normalStatus);
    }
  }
  initHardware();
  initClock();
  lastActivity=millis();
  WiFi.mode(WIFI_STA);WiFi.setSleep(true);WiFi.setAutoReconnect(true);
  configTzTime("CST6","time.cloudflare.com","pool.ntp.org","time.google.com");
  results=xQueueCreate(1,sizeof(FetchResult));
  if(results && xTaskCreate(networkWorker,"quote-net",12288,nullptr,1,&networkTaskHandle)==pdPASS)requestRefresh(false,"boot");
  else {networkTaskHandle=nullptr;showNotice("Error de memoria",60000);}
  drawScreen();dirty=false;
}

void loop() {
  if(results)handleResult();
  uint32_t now=millis();
  bool powerPressed=false;
  if(pmuOK && uint32_t(now-lastPmu)>=20) {
    lastPmu=now;power.getIrqStatus();
    powerPressed=power.isPekeyShortPressIrq();
    power.clearIrqStatus();
  }
  if(powerPressed)wakeScreen();
  switch(powerButton.update(powerPressed,now)) {
    case ButtonAction::Refresh:
      Serial.println("BUTTON single");requestRefresh(true,"button");break;
    case ButtonAction::ToggleMode:
      Serial.println("BUTTON double");toggleMode();break;
    default:break;
  }
  if(touchOK && (touchInterrupt || uint32_t(now-lastTouch)>=25)) {
    touchInterrupt=false;lastTouch=now;
    int16_t x,y;
    bool pressed=touch.getPoint(&x,&y,1)>0;
    if(pressed && !wasPressed && brightness==0) {
      wakeScreen();Serial.println("TOUCH wake");
    }
    wasPressed=pressed;
  }
  if(imuOK && uint32_t(now-lastSensor)>=10) {
    lastSensor=now;
    float x,y,z;
    if(imu.getDataReady() && imu.getAccelerometer(x,y,z) && shake.update(sqrtf(x*x+y*y+z*z),now))requestRefresh(true,"shake");
  }
  if(uint32_t(now-lastBattery)>=1000){lastBattery=now;readBattery();}
  if(!view.busy && (!requestedBefore || (receivedBefore && uint32_t(millis()-lastResult)>=retryDelay)))requestRefresh(false,"timer");
  if(noticeUntil && int32_t(now-noticeUntil)>=0){noticeUntil=0;snprintf(view.status,sizeof(view.status),"%s",normalStatus);dirty=true;}
  uint8_t target=displayBrightness(view.mode,uint32_t(millis()-lastActivity));
  if(target!=brightness){brightness=target;ledcWrite(LCD_BL,brightness);dirty=true;Serial.printf("DISPLAY brightness=%u\n",brightness);}
  if(view.busy && uint32_t(now-lastDraw)>=150){++view.frame;dirty=true;}
  view.chargeAnimationMs=uint32_t(millis()-chargeStarted)%2400;
  if(dirty && brightness>0){drawScreen();dirty=false;lastDraw=now;lastChargeDraw=now;}
  else if(view.charging && view.battery>=0 && batteryBarVisible(view) && brightness>0 && uint32_t(now-lastChargeDraw)>=40) {
    drawChargingBar();lastChargeDraw=now;
  }
  // Local USB diagnostics: same production paths for refresh and mode control.
  if(Serial.available()) {
    char c=Serial.read();
    if(c=='r')requestRefresh(true,"serial");
    else if(c=='m')toggleMode();
    else if(c=='s')drawScreen(true);
    else if(c=='d')Serial.printf("DIAG battery=%d charging=%d usb=%d bar=%d pmu=%d touch=%d imu=%d mode=%s brightness=%u wifi=%d heap=%u\n",view.battery,view.charging,view.usbConnected,batteryBarVisible(view),pmuOK,touchOK,imuOK,screenModeLabel(view.mode),brightness,WiFi.status(),ESP.getFreeHeap());
  }
  delay(5);
}
