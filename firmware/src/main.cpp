#include "Version.h"
#include <M5Unified.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <atomic>
#include <ArduinoJson.h>
#include <time.h>
#include "Battery.h"
#include "Screens.h"
#include "Quote.h"
#include "Energy.h"
#include "MotionWake.h"
#include "Trust.h"
#include "medaka/World.h"
#include "medaka/Energy.h"
#include "pomodoro/Pomodoro.h"

M5Canvas frame(&M5.Display);
aquarium::Battery battery;
aquarium::Screens screens;
Energy energy;
MotionWake motionWake;
uint32_t lastMotionSample=0;
WebServer server(80);
struct Network {String ssid,pass;};
Network networks[2];
QueueHandle_t quotes;
std::atomic<uint32_t> offTicket{0},offAck{0};
Quote quote;
std::atomic<bool> wantNetwork{false},networkIdle{true},fetchRequested{false};
// 0 off, 1 connecting, 2 connected, 3 fetching, 4 unavailable, 5 API error, 6 clock error, 7 maintenance
std::atomic<int> networkState{0},selectedNetwork{-1};
std::atomic<int> lastHttpStatus{0},lastApiStatus{-1};
bool ready=false,setupMode=false,setupPending=false,charging=false;
int level=-1,appliedBrightness=-1;
String apPassword;
uint32_t lastFrame=0,lastBattery=0,lastRequest=0,portalStarted=0,restartAt=0;
float history[120];int historyCount=0;
uint16_t rgb(int r,int g,int b){return M5.Display.color565(r,g,b);}

namespace medaka {
using namespace aquarium;
World world;
aquarium::Energy energy;
bool imu=false,night=false;
uint32_t frames=0,previous=0;
float accumulator=0;
uint16_t color(int r,int g,int b){
  float k=night?.40f:1.f;
  return rgb(clamp(r*k*world.tint[0],0,255),clamp(g*k*world.tint[1],0,255),clamp(b*k*world.tint[2],0,255));
}
#include "medaka/Scene.h"
void enter(uint32_t now){energy.begin(now);previous=now;accumulator=0;}
void update(uint32_t now,bool interaction){
  float elapsed=std::min((now-previous)*.001f,.2f);previous=now;
  bool moved=false;
  if(imu&&M5.Imu.update()){
    auto data=M5.Imu.getImuData();
    moved=energy.motion(data.accel.x,data.accel.y,data.accel.z,data.gyro.x,data.gyro.y,data.gyro.z);
    world.sense(data.accel.x,data.accel.y,data.accel.z,elapsed,data.gyro.x,data.gyro.y,data.gyro.z);
  }
  energy.update(now,moved||interaction||world.foodCount()>0);
  accumulator+=elapsed;
  while(accumulator>=1.f/120){world.step(1.f/120);accumulator-=1.f/120;}
}
}

#include "pomodoro/App.h"

String htmlEscape(String value){value.replace("&","&amp;");value.replace("\"","&quot;");value.replace("<","&lt;");value.replace(">","&gt;");return value;}
void configurePortal(){
  server.on("/",HTTP_GET,[]{
    if(!setupMode){server.send(404);return;}
    String page=R"HTML(<!doctype html><meta charset="utf-8"><meta name="viewport" content="width=device-width"><title>StopWatch Wi-Fi</title><body style="font:17px system-ui;max-width:440px;margin:auto;padding:24px"><h1>StopWatch Wi-Fi</h1><p>自宅Wi-Fiを優先し、つながらなければiPhone共有へ接続します。</p><form method="post" action="/save">)HTML";
    for(int i=0;i<2;i++){
      page+="<h2>"+String(i?"iPhoneのインターネット共有":"自宅Wi-Fi")+"</h2><p><input name='ssid"+String(i)+"' maxlength='32' placeholder='Wi-Fi名（SSID）' value=\""+htmlEscape(networks[i].ssid)+"\"></p><p><input type='password' name='pass"+String(i)+"' maxlength='63' placeholder='Wi-Fiパスワード'></p>";
    }
    page+=R"HTML(<p>同じSSIDでパスワードを空欄にすると保存済み設定を維持。SSIDを空欄にするとその接続先を削除します。少なくとも1件設定してください。</p><p>iPhoneは「ほかの人の接続を許可」と「互換性を優先」をオンにし、接続時は共有設定画面を開いてください。</p><button>保存して再起動</button></form></body>)HTML";
    server.sendHeader("Cache-Control","no-store");server.send(200,"text/html; charset=utf-8",page);
  });
  server.on("/save",HTTP_POST,[]{
    if(!setupMode){server.send(404);return;}
    JsonDocument doc;bool any=false;
    for(int i=0;i<2;i++){
      String ssid=server.arg("ssid"+String(i)),pass=server.arg("pass"+String(i));
      if(!ssid.isEmpty()&&ssid==networks[i].ssid&&pass.isEmpty())pass=networks[i].pass;
      if(ssid.isEmpty())pass="";
      if(ssid.length()>32||pass.length()>63||(!pass.isEmpty()&&pass.length()<8)){server.send(400,"text/plain; charset=utf-8","SSIDまたはパスワードの長さを確認してください");return;}
      any|=!ssid.isEmpty();doc["networks"][i]["ssid"]=ssid;doc["networks"][i]["pass"]=pass;
    }
    if(!any){server.send(400,"text/plain; charset=utf-8","接続先を1件以上入力してください");return;}
    String encoded;serializeJson(doc,encoded);Preferences prefs;
    if(!prefs.begin("fx-wifi",false)){server.send(500,"text/plain","Storage error");return;}
    auto bytes=prefs.putString("networks",encoded);prefs.end();
    if(bytes!=encoded.length()){server.send(500,"text/plain","Storage error");return;}
    server.send(200,"text/plain; charset=utf-8","保存しました。再起動後、iPhoneのインターネット共有をオンにしてください。");restartAt=millis()+1500;
  });
}
void startPortal(){
  setupMode=true;setupPending=false;portalStarted=millis();
  char pass[13];snprintf(pass,sizeof(pass),"%08lx",(unsigned long)esp_random());apPassword=pass;
  WiFi.mode(WIFI_AP);WiFi.softAP("StopWatch-FX",apPassword.c_str());server.begin();
}
void stopPortal(){server.stop();WiFi.softAPdisconnect(true);WiFi.mode(WIFI_OFF);setupMode=false;energy.wake(millis());}
bool waitConnected(uint32_t limit){
  uint32_t start=millis();
  while(wantNetwork&&WiFi.status()!=WL_CONNECTED&&millis()-start<limit)vTaskDelay(pdMS_TO_TICKS(100));
  return wantNetwork&&WiFi.status()==WL_CONNECTED;
}
bool connectNetwork(){
  networkState=1;
  for(int i=0;i<2&&wantNetwork;i++){
    if(networks[i].ssid.isEmpty())continue;
    WiFi.disconnect(false,false);WiFi.begin(networks[i].ssid.c_str(),networks[i].pass.c_str());
    if(waitConnected(8000)){selectedNetwork=i;networkState=2;return true;}
  }
  WiFi.disconnect(false,false);WiFi.mode(WIFI_OFF);selectedNetwork=-1;networkState=4;return false;
}
bool getQuote(){
  networkState=3;
  lastHttpStatus=0;lastApiStatus=-1;
  if(time(nullptr)<1700000000){networkState=6;return false;}
  // TLS key exchange at 80 MHz can starve CPU 0's idle task long enough to
  // trip the watchdog. Restore the idle clock on every exit, after TLS cleanup.
  struct TlsCpuClock {
    uint32_t previous=getCpuFrequencyMhz();
    TlsCpuClock(){setCpuFrequencyMhz(240);}
    ~TlsCpuClock(){setCpuFrequencyMhz(previous);}
  } tlsCpuClock;
  WiFiClientSecure client;client.setCACert(ROOT_CA);client.setHandshakeTimeout(4);
  HTTPClient http;http.setConnectTimeout(4000);http.setTimeout(4000);
  bool success=false,maintenance=false;
  if(http.begin(client,"https://forex-api.coin.z.com/public/v1/ticker")){
    lastHttpStatus=http.GET();
    if(lastHttpStatus==200&&wantNetwork){
      String body=http.getString();JsonDocument doc;
      if(body.length()<=16384&&!deserializeJson(doc,body)&&doc["status"].is<int>()){
        lastApiStatus=doc["status"].as<int>();maintenance=lastApiStatus==5;
        if(lastApiStatus==0)for(JsonObject row:doc["data"].as<JsonArray>())if(strcmp(row["symbol"]|"","USD_JPY")==0){
          String encoded;serializeJson(row,encoded);Quote next;
          if(parseQuote((const uint8_t*)encoded.c_str(),encoded.length(),millis(),next)&&wantNetwork){xQueueOverwrite(quotes,&next);success=true;}break;
        }
      }
    }
    http.end();
  }
  networkState=success?2:maintenance?7:5;return success;
}
void networkTask(void*){
  bool active=false,attempted=false,failedFetch=false;
  uint32_t lastAttempt=0,lastFetch=0;
  for(;;){
    if(!wantNetwork){
      if(active){WiFi.disconnect(false,false);WiFi.mode(WIFI_OFF);active=false;}
      networkState=0;selectedNetwork=-1;networkIdle=true;offAck=offTicket.load();attempted=false;failedFetch=false;
    }else{
      networkIdle=false;active=true;
      if(WiFi.status()!=WL_CONNECTED){
        if(!attempted||millis()-lastAttempt>=60000){
          attempted=true;WiFi.mode(WIFI_STA);WiFi.setAutoReconnect(false);WiFi.setSleep(true);
          bool connected=connectNetwork();lastAttempt=millis();
          if(connected){
            configTime(0,0,"time.cloudflare.com","pool.ntp.org");uint32_t start=millis();
            while(wantNetwork&&time(nullptr)<1700000000&&millis()-start<5000)vTaskDelay(pdMS_TO_TICKS(100));
          }
        }
      }else if(fetchRequested&&(!failedFetch||millis()-lastFetch>=30000)){
        networkState=3;fetchRequested=false;failedFetch=!getQuote();lastFetch=millis();
      }
    }
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}
const char* networkStatus(){
  if(setupPending)return "OPENING SETUP";
  if(networkState==1)return "CONNECTING";
  if(networkState==4)return "NO WI-FI / RETRY 60s";
  if(networkState==5)return "API ERROR / RETRY";
  if(networkState==6)return "WAITING FOR CLOCK";
  if(networkState==7)return "API MAINTENANCE";
  if(!quote.received)return "WAITING FOR RATE";
  if(quoteStale(quote,millis(),time(nullptr)))return "STALE / LAST QUOTE";
  if(!quote.open)return "MARKET CLOSED";
  return selectedNetwork==1?"iPHONE HOTSPOT":"HOME WI-FI";
}
void label(const String& s,int y,int font=2,uint16_t color=0xFFFF) {
  frame.setTextColor(color);frame.setFont(font==7?&fonts::Font7:font==4?&fonts::Font4:&fonts::Font2);frame.drawString(s,233,y);
}
void render() {
  frame.fillSprite(rgb(8,17,26));frame.setTextDatum(middle_center);
  if(screens.menu) {
    label("APPS",72,4,rgb(150,174,191));
    const char* names[]={"USD / JPY","Aquarium","Battery","Wi-Fi Settings","Pomodoro"};
    for(int i=0;i<aquarium::Screens::count;++i){
      int y=aquarium::Screens::top+i*(aquarium::Screens::rowHeight+aquarium::Screens::rowGap);
      frame.fillRoundRect(aquarium::Screens::left,y,aquarium::Screens::width,aquarium::Screens::rowHeight,12,
        i==screens.selected?rgb(36,86,92):rgb(19,35,47));
      frame.setFont(&fonts::Font4);frame.setTextColor(0xFFFF);
      frame.drawString(names[i],233,y+aquarium::Screens::rowHeight/2);
    }
    label("YELLOW: NEXT   BLUE: OPEN",399,2,rgb(231,203,100));
  } else if(screens.tank()) {
    medaka::renderScene();return;
  } else if(screens.pomodoro()) {
    pomodoro::paint();return;
  } else if(screens.battery()) {
    uint16_t accent=level>=0&&level<=20?rgb(235,158,85):rgb(113,207,176);
    label("BATTERY",103,4,rgb(159,194,183));
    frame.drawRoundRect(133,146,190,78,12,accent);frame.fillRoundRect(325,169,9,31,3,accent);
    if(level>0)frame.fillRoundRect(140,153,176*level/100,64,6,accent);
    frame.setTextSize(3);label(level<0?"--":String(level)+"%",275,4);frame.setTextSize(1);
    label(level<0?"UNAVAILABLE":charging?"CHARGING":"ESTIMATED",331);
    label("HOLD YELLOW: APPS",374,2,rgb(231,203,100));
    label(String("FW v")+kFirmwareVersion,405,2,rgb(159,194,183));
  } else if(setupMode) {
    label("Wi-Fi SETUP",100,4);label("Connect to StopWatch-FX",160);
    label("Password: "+apPassword,205,4);label("Open in your browser",260);
    label("http://192.168.4.1",300,4);label("2.4 GHz Wi-Fi only",350);
  } else if(setupPending) {
    label("OPENING Wi-Fi SETUP",200,4);
    label("HOLD YELLOW: APPS",300);
  } else {
    bool stale=quoteStale(quote,millis(),time(nullptr));
    const char* state=networkStatus();
    label("USD / JPY",78,4,rgb(150,174,191));label(state,117,2,stale?rgb(242,182,95):rgb(108,223,179));
    frame.setTextSize(2);label(quote.received?String((quote.bid+quote.ask)/2,3):"---.---",194,7);frame.setTextSize(1);
    label("MID  /  JPY per USD",245,2,rgb(150,174,191));
    label(quote.received?"BID "+String(quote.bid,3)+"   ASK "+String(quote.ask,3):"Waiting for market data",281);
    if(historyCount>1) {
      float lo=history[0],hi=lo;for(int i=1;i<historyCount;i++){lo=std::min(lo,history[i]);hi=std::max(hi,history[i]);}
      float span=std::max(hi-lo,.005f);
      for(int i=1;i<historyCount;i++)frame.drawLine(90+(i-1)*286/119,345-(history[i-1]-lo)*40/span,90+i*286/119,345-(history[i]-lo)*40/span,rgb(108,223,179));
    }
    char updated[40]="No quote yet";
    if(quote.received) {time_t jst=quote.epoch+9*3600; struct tm tm;gmtime_r(&jst,&tm);strftime(updated,sizeof(updated),"%m/%d %H:%M:%S JST",&tm);}
    label(updated,371,2,rgb(150,174,191));label("BAT "+(level<0?String("--"):String(level)+"%")+"  /  ECO",399,2,rgb(100,130,150));
  }
  frame.pushSprite(0,0);
}
void requestSetup(){screens.app=aquarium::App::Wifi;screens.menu=false;wantNetwork=false;offTicket++;setupPending=true;}
void openApps(){
  setupPending=false;
  if(setupMode)stopPortal();
  wantNetwork=false;
  screens.openMenu();
}
void launchApp(uint32_t now){
  screens.launch();energy.wake(now);
  if(screens.app==aquarium::App::Wifi)requestSetup();
  if(screens.dollar()){fetchRequested=true;lastRequest=now;}
  if(screens.tank())medaka::enter(now);
}
void setup(){
  auto cfg=M5.config();cfg.internal_spk=true;cfg.internal_mic=false;cfg.internal_imu=true;
  M5.begin(cfg);Serial.begin(115200);M5.Display.setRotation(0);M5.Display.setBrightness(80);
  frame.setColorDepth(16);frame.setPsram(true);ready=frame.createSprite(466,466)!=nullptr;
  quotes=xQueueCreate(1,sizeof(Quote));
  if(!ready||!quotes){ready=false;M5.Display.drawString("Memory allocation failed",100,233);return;}
  medaka::imu=M5.Imu.isEnabled();pomodoro::begin();
  setCpuFrequencyMhz(80);WiFi.mode(WIFI_OFF);WiFi.persistent(false);
  Preferences prefs;prefs.begin("fx-wifi",true);String stored=prefs.getString("networks");JsonDocument doc;
  if(!stored.isEmpty()&&!deserializeJson(doc,stored)){
    for(int i=0;i<2;i++){networks[i].ssid=doc["networks"][i]["ssid"]|"";networks[i].pass=doc["networks"][i]["pass"]|"";}
  }else{networks[0].ssid=prefs.getString("ssid");networks[0].pass=prefs.getString("pass");}
  prefs.end();configurePortal();energy.wake(millis());fetchRequested=true;
  if(xTaskCreatePinnedToCore(networkTask,"forex",12288,nullptr,1,nullptr,0)!=pdPASS){ready=false;M5.Display.drawString("Network task failed",100,233);return;}
  if(networks[0].ssid.isEmpty()&&networks[1].ssid.isEmpty())requestSetup();
  Serial.printf("STOPWATCH FX HOTSPOT v%s ready\n",kFirmwareVersion);
}
void loop(){
  M5.update();if(!ready){delay(50);return;}
  uint32_t now=millis();auto old=screens.app;bool wasMenu=screens.menu;
  pomodoro::update(now);
  bool yellowHeld=M5.BtnA.wasHold(),yellowClick=M5.BtnA.wasClicked(),blueHeld=M5.BtnB.wasHold(),blueClick=M5.BtnB.wasClicked();
  auto touch=M5.Touch.getDetail();
  bool interaction=M5.BtnA.wasPressed()||M5.BtnB.wasPressed()||touch.wasPressed()||yellowHeld||yellowClick||blueHeld||blueClick;
  bool wasOff=appliedBrightness==0;if(interaction)energy.wake(now);
  bool consumed=false;
  if(yellowHeld){openApps();consumed=true;}
  else if(screens.menu){
    consumed=true;
    if(yellowClick)screens.next();
    else if(blueClick)launchApp(now);
    else if(touch.wasClicked()&&!wasOff){
      int row=aquarium::Screens::hit(touch.x,touch.y);
      if(row>=0){screens.selected=row;launchApp(now);}
    }
  }else if(screens.tank()){
    if(yellowClick)medaka::world.feed(155);
    if(blueClick)medaka::world.feed(311);
    if(touch.wasHold())medaka::world.toggleLight();
    if(touch.wasClicked())medaka::world.ripple(touch.x,125);
  }else if(screens.pomodoro()){
    if(yellowClick)pomodoro::action('a');
    if(blueHeld)pomodoro::action('r');
    else if(blueClick)pomodoro::action('m');
    if(touch.wasClicked())pomodoro::touch(touch.x,touch.y);
  }else if(screens.dollar()&&(blueHeld||touch.wasHold())){requestSetup();consumed=true;}
  if(setupPending&&offAck==offTicket)startPortal();
  if(setupMode){server.handleClient();if(now-portalStarted>=120000){stopPortal();screens.openMenu();}}
  bool screenChanged=old!=screens.app||wasMenu!=screens.menu;
  if(screens.tank())medaka::update(now,interaction||touch.isPressed());
  bool motionWoke=false;
  if(!screens.pomodoro()||screenChanged)motionWake.reset();
  if(screens.pomodoro()&&medaka::imu&&now-lastMotionSample>=50){
    lastMotionSample=now;
    if(M5.Imu.update()){
      auto data=M5.Imu.getImuData();
      motionWoke=motionWake.sample(data.accel.x,data.accel.y,data.accel.z,data.gyro.x,data.gyro.y,data.gyro.z);
      if(motionWoke)energy.wake(now);
    }
  }
  if(restartAt&&int32_t(now-restartAt)>=0)ESP.restart();
  if(now-lastBattery>=30000||lastBattery==0||screenChanged){lastBattery=now;charging=M5.Power.isCharging()==m5::Power_Class::is_charging_t::is_charging;level=battery.update(M5.Power.getBatteryLevel(),charging);}
  int brightness=screens.tank()?medaka::energy.brightness(false):energy.brightness(now,level,charging,setupMode||setupPending);
  if(brightness!=appliedBrightness){appliedBrightness=brightness;if(brightness==0)M5.Display.sleep();else{M5.Display.wakeup();M5.Display.setBrightness(brightness);}}
  uint32_t interval=energy.refresh(level,charging);
  bool manual=!consumed&&screens.dollar()&&(blueClick||yellowClick||touch.wasClicked()||(wasOff&&interaction));
  bool hasNetwork=!networks[0].ssid.isEmpty()||!networks[1].ssid.isEmpty();
  if(manual){if(!hasNetwork)requestSetup();else{fetchRequested=true;lastRequest=now;}}
  bool viewing=screens.dollar()&&brightness>0;
  if(viewing&&interval&&now-lastRequest>=interval){fetchRequested=true;lastRequest=now;}
  wantNetwork=viewing&&hasNetwork&&(interval>0||fetchRequested||networkState==3);
  bool changed=false;Quote next;
  if(xQueueReceive(quotes,&next,0)==pdTRUE&&next.epoch>=quote.epoch){quote=next;changed=true;if(historyCount==120){memmove(history,history+1,119*sizeof(float));historyCount--;}history[historyCount++]=(quote.bid+quote.ask)/2;}
  if(brightness>0&&(changed||interaction||motionWoke||now-lastFrame>=(screens.tank()?medaka::energy.frameInterval():screens.pomodoro()?200:1000)||screenChanged)){lastFrame=now;render();}
  if(Serial.available()&&Serial.read()=='?')Serial.printf("FX HOTSPOT version=%s wifi=%d network=%d state=%d quote=%d battery=%d charging=%d brightness=%d screen=%s heap=%u http=%d api=%d cpu=%u\n",kFirmwareVersion,WiFi.getMode()!=WIFI_OFF,selectedNetwork.load(),networkState.load(),quote.received!=0,level,charging,appliedBrightness,screens.name(),ESP.getFreeHeap(),lastHttpStatus.load(),lastApiStatus.load(),getCpuFrequencyMhz());
  delay(screens.tank()?medaka::energy.loopDelay():brightness?20:50);
}
