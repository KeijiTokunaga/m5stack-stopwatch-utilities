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
#include "Location.h"
#include "LocationTrust.h"
#include "Weather.h"
#include "WeatherTrust.h"
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
QueueHandle_t forecasts;
QueueHandle_t locationFixes;
SemaphoreHandle_t mapMutex;
std::atomic<uint32_t> offTicket{0},offAck{0};
Quote quote;
weather::Forecast forecast;
weather::Refresh weatherRefresh;
bool weatherDetails=false;
std::atomic<bool> weatherMode{false},weatherBusy{false};
std::atomic<int> weatherResult{0},weatherHttp{0};
std::atomic<bool> wantNetwork{false},networkIdle{true},fetchRequested{false};
std::atomic<bool> locationRequested{false};
// 0 idle, 1 connecting, 2 scanning, 3 locating, 4 loading map, 5 ready,
// 6 insufficient APs, 7 location service error, 8 map error
std::atomic<int> locationState{0};
// 0 off, 1 connecting, 2 connected, 3 fetching, 4 unavailable, 5 API error, 6 clock error, 7 maintenance
std::atomic<int> networkState{0},selectedNetwork{-1};
std::atomic<int> lastHttpStatus{0},lastApiStatus{-1};
bool ready=false,setupMode=false,setupPending=false,charging=false;
bool locationConsent=false;
bool showLocationQr=false;
String locationProxy,locationToken;
location::Fix currentFix;
uint8_t* mapJpeg=nullptr;size_t mapJpegSize=0;
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
#include "paradise/App.h"

String htmlEscape(String value){value.replace("&","&amp;");value.replace("\"","&quot;");value.replace("<","&lt;");value.replace(">","&gt;");return value;}
void configurePortal(){
  server.on("/",HTTP_GET,[]{
    if(!setupMode){server.send(404);return;}
    String page=R"HTML(<!doctype html><meta charset="utf-8"><meta name="viewport" content="width=device-width"><title>StopWatch Wi-Fi</title><body style="font:17px system-ui;max-width:440px;margin:auto;padding:24px"><h1>StopWatch Wi-Fi</h1><p>自宅Wi-Fiを優先し、つながらなければiPhone共有へ接続します。</p><form method="post" action="/save">)HTML";
    for(int i=0;i<2;i++){
      page+="<h2>"+String(i?"iPhoneのインターネット共有":"自宅Wi-Fi")+"</h2><p><input name='ssid"+String(i)+"' maxlength='32' placeholder='Wi-Fi名（SSID）' value=\""+htmlEscape(networks[i].ssid)+"\"></p><p><input type='password' name='pass"+String(i)+"' maxlength='63' placeholder='Wi-Fiパスワード'></p>";
    }
    page+=R"HTML(<p>同じSSIDでパスワードを空欄にすると保存済み設定を維持。SSIDを空欄にするとその接続先を削除します。少なくとも1件設定してください。</p><p>iPhoneは「ほかの人の接続を許可」と「互換性を優先」をオンにし、接続時は共有設定画面を開いてください。</p><hr><h2>Location / Google Maps</h2><p>同意した場合だけ、周辺Wi-FiのBSSID・電波強度・チャンネルを指定したプロキシ経由でGoogleへ送信して現在地を推定します。通信先とネットワーク事業者は通信に必要なIPアドレスを扱います。SSIDとWi-Fiパスワードは送信しません。地図画像は端末へ保存しません。</p><p><a href="https://github.com/KeijiTokunaga/m5stack-stopwatch-utilities/blob/main/PRIVACY.md">アプリのプライバシーポリシー</a> / <a href="https://github.com/KeijiTokunaga/m5stack-stopwatch-utilities/blob/main/TERMS.md">利用規約</a> / <a href="https://policies.google.com/privacy">Googleプライバシーポリシー</a> / <a href="https://maps.google.com/help/terms_maps/">Google Maps利用規約</a></p>)HTML";
    page+="<p><input name='locationProxy' maxlength='180' placeholder='https://your-proxy.vercel.app' value=\""+htmlEscape(locationProxy)+"\"></p><p><input type='password' name='locationToken' maxlength='200' placeholder='端末トークン（空欄なら保存済みを維持）'></p>";
    page+="<p><label><input type='checkbox' name='locationConsent' value='yes' "+String(locationConsent?"checked":"")+"> 上記の位置推定のための送信に同意する</label></p><button>保存して再起動</button></form></body>";
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
    bool consent=server.hasArg("locationConsent");
    String proxy=server.arg("locationProxy"),token=server.arg("locationToken");proxy.trim();token.trim();
    if(consent){
      if(token.isEmpty()&&proxy==locationProxy)token=locationToken;
      if(!proxy.startsWith("https://")||proxy.length()>180||proxy.indexOf('?',8)>=0||proxy.indexOf('#',8)>=0||token.length()<16||token.length()>200){
        server.send(400,"text/plain; charset=utf-8","LocationのHTTPSプロキシURLまたは端末トークンを確認してください");return;
      }
      while(proxy.endsWith("/"))proxy.remove(proxy.length()-1);
    }else{proxy="";token="";}
    doc["location"]["proxy"]=proxy;doc["location"]["token"]=token;doc["location"]["consent"]=consent;
    String encoded;serializeJson(doc,encoded);Preferences prefs;
    if(!prefs.begin("fx-wifi",false)){server.send(500,"text/plain","Storage error");return;}
    auto bytes=prefs.putString("networks",encoded);prefs.end();
    if(bytes!=encoded.length()){server.send(500,"text/plain","Storage error");return;}
    server.send(200,"text/plain; charset=utf-8","保存しました。再起動後、iPhoneのインターネット共有をオンにしてください。");restartAt=millis()+1500;
  });
}
void startPortal(){
  setupMode=true;setupPending=false;portalStarted=millis();
  char pass[33];snprintf(pass,sizeof(pass),"%08lx%08lx%08lx%08lx",(unsigned long)esp_random(),(unsigned long)esp_random(),(unsigned long)esp_random(),(unsigned long)esp_random());apPassword=pass;
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
struct TlsCpuClock {
  uint32_t previous=getCpuFrequencyMhz();
  TlsCpuClock(){setCpuFrequencyMhz(240);}
  ~TlsCpuClock(){setCpuFrequencyMhz(previous);}
};
bool locationConfigured(){return locationConsent&&!locationProxy.isEmpty()&&!locationToken.isEmpty();}
bool getLocation(){
  locationState=2;
  int found=WiFi.scanNetworks(false,true,false,300);
  JsonDocument request;JsonArray aps=request["wifiAccessPoints"].to<JsonArray>();
  for(int i=0;i<found&&aps.size()<20;i++){
    String mac=WiFi.BSSIDstr(i);int rssi=WiFi.RSSI(i),channel=WiFi.channel(i);
    if(!location::validBssid(mac.c_str())||rssi<-128||rssi>-10||channel<1||channel>196)continue;
    JsonObject ap=aps.add<JsonObject>();ap["macAddress"]=mac;ap["signalStrength"]=rssi;ap["channel"]=channel;
  }
  WiFi.scanDelete();
  if(aps.size()<2){locationState=6;return false;}
  String body;serializeJson(request,body);locationState=3;
  TlsCpuClock clock;
  WiFiClientSecure client;client.setCACert(LOCATION_ROOT_CA);client.setHandshakeTimeout(6);
  HTTPClient http;http.setConnectTimeout(6000);http.setTimeout(8000);
  String auth="Bearer "+locationToken;
  if(!http.begin(client,locationProxy+"/api/locate")){locationState=7;return false;}
  http.addHeader("Authorization",auth);http.addHeader("Content-Type","application/json");
  int status=http.POST((uint8_t*)body.c_str(),body.length());int responseSize=http.getSize();
  String response=status==200&&responseSize>0&&responseSize<=2048?http.getString():"";http.end();
  location::Fix fix;
  if(status!=200||!location::parseFix((const uint8_t*)response.c_str(),response.length(),fix)){locationState=7;return false;}
  JsonDocument resultDoc;
  if(deserializeJson(resultDoc,response)||!resultDoc["mapUrl"].is<const char*>()){locationState=7;return false;}
  String mapUrl=resultDoc["mapUrl"].as<String>();
  locationState=4;
  WiFiClientSecure googleClient;googleClient.setCACert(LOCATION_ROOT_CA);googleClient.setHandshakeTimeout(6);
  HTTPClient image;image.setConnectTimeout(6000);image.setTimeout(12000);
  if(!image.begin(googleClient,mapUrl)){locationState=8;return false;}
  status=image.GET();int length=image.getSize();
  if(status!=200||length<=0||length>262144){image.end();locationState=8;return false;}
  uint8_t* next=(uint8_t*)heap_caps_malloc(length,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  if(!next){image.end();locationState=8;return false;}
  size_t read=image.getStreamPtr()->readBytes(next,length);image.end();
  if(read!=(size_t)length||length<2||next[0]!=0xff||next[1]!=0xd8){free(next);locationState=8;return false;}
  if(!wantNetwork){free(next);locationState=0;return false;}
  xSemaphoreTake(mapMutex,portMAX_DELAY);uint8_t* old=mapJpeg;mapJpeg=next;mapJpegSize=length;xSemaphoreGive(mapMutex);free(old);
  xQueueOverwrite(locationFixes,&fix);locationState=5;return true;
}
bool getWeather(){
  weatherResult=1;weatherHttp=0;networkState=3;
  if(time(nullptr)<1700000000){weatherResult=4;networkState=6;return false;}
  TlsCpuClock clock;
  WiFiClientSecure client;client.setCACert(WEATHER_ROOT_CA);client.setHandshakeTimeout(4);
  HTTPClient http;http.setConnectTimeout(4000);http.setTimeout(4000);
  bool success=false;
  if(http.begin(client,weather::url)){
    weatherHttp=http.GET();
    if(weatherHttp==200&&wantNetwork){
      struct Body : Stream {
        String text;
        int available() override{return 0;} int read() override{return -1;} int peek() override{return -1;} void flush() override{}
        size_t write(uint8_t c) override{return write(&c,1);}
        size_t write(const uint8_t* data,size_t size) override{
          if(size>12288-text.length())return 0;
          return text.concat(reinterpret_cast<const char*>(data),size)?size:0;
        }
      } body;
      if(http.getSize()<=12288&&http.writeToStream(&body)>0){
        weather::Forecast next;
        if(weather::parse(body.text.c_str(),body.text.length(),millis(),next)&&wantNetwork){xQueueOverwrite(forecasts,&next);success=true;}
      }
    }
    http.end();
  }
  weatherResult=success?2:3;networkState=success?2:5;return success;
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
      }else if(locationRequested.exchange(false)){
        getLocation();
      }else if(fetchRequested&&(!failedFetch||millis()-lastFetch>=30000)){
        networkState=3;bool weather=weatherMode.load();weatherBusy=weather;fetchRequested=false;
        failedFetch=weather?!getWeather():!getQuote();weatherBusy=false;lastFetch=millis();
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
#include "WeatherFace.h"
void render() {
  frame.fillSprite(rgb(8,17,26));frame.setTextDatum(middle_center);
  if(screens.menu) {
    label("APPS",55,4,rgb(150,174,191));
    const char* names[]={"USD / JPY","Aquarium","Battery","Wi-Fi Settings","Pomodoro","Weather","Paradise","Location"};
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
  } else if(screens.paradise()) {
    paradise::paint();return;
  } else if(screens.weather()) {
    if(!weatherDetails){paintWeatherFace();return;}
    label("OSAKA WEATHER",65,4,rgb(150,200,225));
    const char* state=weatherBusy?"UPDATING":networkState==1?"CONNECTING":networkState==4?"NO WI-FI / RETRY":
      weatherResult==4?"WAITING FOR CLOCK":weatherResult==3?"UPDATE FAILED / LAST DATA":
      !forecast.valid?"WAITING FOR FORECAST":weather::stale(forecast,millis(),time(nullptr))?"STALE / LAST FORECAST":"CURRENT / 3-DAY FORECAST";
    label(state,100,2,rgb(231,203,100));
    if(forecast.valid){
      frame.setTextSize(2);label(String(forecast.temperature,1)+" C",149,4);frame.setTextSize(1);
      label(weather::description(forecast.code),194,4,rgb(123,217,227));
      label("DATE     HIGH / LOW      RAIN MAX",230,2,rgb(150,174,191));
      for(int i=0;i<3;++i){const auto& day=forecast.days[i];int y=257+i*44;frame.fillRoundRect(88,y-14,290,40,8,rgb(19,35,47));String rain=day.rain<0?String("--"):String(day.rain)+"%";label(String(day.date+5)+"    "+String(day.high,0)+" / "+String(day.low,0)+" C    "+rain,y,2);label(weather::description(day.code),y+17,2,rgb(150,174,191));}
      label(String("As of ")+forecast.localTime+" JST",384,2,rgb(150,174,191));
    }else{label("Waiting for Osaka weather",190,4);label("Saved Wi-Fi / iPhone hotspot",242);label("BLUE HOLD: Wi-Fi SETUP",291,2,rgb(231,203,100));}
    label("Open-Meteo.com",410,2,rgb(150,174,191));
  } else if(screens.battery()) {
    uint16_t accent=level>=0&&level<=20?rgb(235,158,85):rgb(113,207,176);
    label("BATTERY",103,4,rgb(159,194,183));
    frame.drawRoundRect(133,146,190,78,12,accent);frame.fillRoundRect(325,169,9,31,3,accent);
    if(level>0)frame.fillRoundRect(140,153,176*level/100,64,6,accent);
    frame.setTextSize(3);label(level<0?"--":String(level)+"%",275,4);frame.setTextSize(1);
    label(level<0?"UNAVAILABLE":charging?"CHARGING":"ESTIMATED",331);
    label("HOLD YELLOW: APPS",374,2,rgb(231,203,100));
    label(String("FW v")+kFirmwareVersion,405,2,rgb(159,194,183));
  } else if(screens.location()) {
    label("LOCATION",43,4,rgb(150,174,191));
    if(!locationConfigured()){
      label("SETUP AND CONSENT REQUIRED",190,2,rgb(242,182,95));
      label("Open Wi-Fi Settings",225);label("to enable Google Maps",253);
    }else if(locationState==5&&currentFix.valid&&showLocationQr){
      String mapsLink="https://www.google.com/maps/search/?api=1&query="+String(currentFix.lat,6)+","+String(currentFix.lng,6);
      frame.fillRect(123,83,220,220,0xFFFF);frame.qrcode(mapsLink.c_str(),133,93,200,6,true);
      label("SCAN TO OPEN IN GOOGLE MAPS",342,2,rgb(150,174,191));
      label("BLUE: MAP",399,2,rgb(231,203,100));
    }else if(locationState==5&&currentFix.valid){
      xSemaphoreTake(mapMutex,portMAX_DELAY);
      if(mapJpeg&&mapJpegSize)frame.drawJpg(mapJpeg,mapJpegSize,73,73);
      xSemaphoreGive(mapMutex);
      label("Estimated accuracy: "+String(currentFix.accuracy,0)+" m",421,2,rgb(150,174,191));
    }else{
      const char* state=locationState==1?"CONNECTING":locationState==2?"SCANNING WI-FI":locationState==3?"LOCATING":locationState==4?"LOADING MAP":locationState==6?"NOT ENOUGH ACCESS POINTS":locationState==7?"LOCATION SERVICE ERROR":locationState==8?"MAP DOWNLOAD ERROR":"PRESS TO LOCATE";
      label(state,211,4,locationState>=6?rgb(242,182,95):rgb(108,223,179));
      label("Google Maps",255,2,rgb(150,174,191));
      label("YELLOW / TAP: UPDATE",399,2,rgb(231,203,100));
    }
  } else if(setupMode) {
    label("Wi-Fi SETUP",100,4);label("Connect to StopWatch-FX",160);
    label("Password: "+apPassword,205,2);label("Open in your browser",260);
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
  if(screens.dollar()){weatherMode=false;fetchRequested=true;lastRequest=now;}
  if(screens.weather()){weatherDetails=false;weatherMode=true;fetchRequested=false;}
  if(screens.tank())medaka::enter(now);
  if(screens.paradise())paradise::enter();
  if(screens.location()&&locationConfigured()){showLocationQr=false;locationState=1;locationRequested=true;}
}
void setup(){
  auto cfg=M5.config();cfg.internal_spk=true;cfg.internal_mic=false;cfg.internal_imu=true;
  M5.begin(cfg);Serial.begin(115200);M5.Display.setRotation(0);M5.Display.setBrightness(80);
  frame.setColorDepth(16);frame.setPsram(true);ready=frame.createSprite(466,466)!=nullptr;
  quotes=xQueueCreate(1,sizeof(Quote));forecasts=xQueueCreate(1,sizeof(weather::Forecast));locationFixes=xQueueCreate(1,sizeof(location::Fix));mapMutex=xSemaphoreCreateMutex();
  if(!ready||!quotes||!forecasts||!locationFixes||!mapMutex){ready=false;M5.Display.drawString("Memory allocation failed",100,233);return;}
  medaka::imu=M5.Imu.isEnabled();pomodoro::begin();paradise::begin();
  setCpuFrequencyMhz(80);WiFi.mode(WIFI_OFF);WiFi.persistent(false);
  Preferences prefs;prefs.begin("fx-wifi",true);String stored=prefs.getString("networks");JsonDocument doc;
  if(!stored.isEmpty()&&!deserializeJson(doc,stored)){
    for(int i=0;i<2;i++){networks[i].ssid=doc["networks"][i]["ssid"]|"";networks[i].pass=doc["networks"][i]["pass"]|"";}
    locationProxy=doc["location"]["proxy"]|"";locationToken=doc["location"]["token"]|"";locationConsent=doc["location"]["consent"]|false;
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
  paradise::update(now);
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
  }else if(screens.paradise()){
    if(blueHeld)paradise::back();
    else if(blueClick)paradise::input(true);
    else if(yellowClick)paradise::input(false);
    if(touch.wasClicked()&&!wasOff)paradise::touch(touch.x,touch.y);
  }else if(screens.pomodoro()){
    if(yellowClick)pomodoro::action('a');
    if(blueHeld)pomodoro::action('R');
    else if(blueClick)pomodoro::action('r');
    if(touch.wasClicked())pomodoro::touch(touch.x,touch.y);
  }else if((screens.dollar()||screens.weather())&&(blueHeld||touch.wasHold())){requestSetup();consumed=true;}
  if(screens.weather()&&(blueClick||touch.wasClicked())){weatherDetails=!weatherDetails;consumed=true;}
  if(setupPending&&offAck==offTicket)startPortal();
  if(setupMode){server.handleClient();if(now-portalStarted>=120000){stopPortal();screens.openMenu();}}
  bool screenChanged=old!=screens.app||wasMenu!=screens.menu;
  if(screenChanged&&old==aquarium::App::Location&&!screens.location()){
    xSemaphoreTake(mapMutex,portMAX_DELAY);free(mapJpeg);mapJpeg=nullptr;mapJpegSize=0;xSemaphoreGive(mapMutex);
    currentFix=location::Fix{};showLocationQr=false;locationState=0;
  }
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
  if(screens.paradise()&&brightness>15)brightness=std::min(brightness,int(paradise::state.brightness));
  if(brightness!=appliedBrightness){appliedBrightness=brightness;if(brightness==0)M5.Display.sleep();else{M5.Display.wakeup();M5.Display.setBrightness(brightness);}}
  uint32_t interval=energy.refresh(level,charging);
  bool manual=!consumed&&screens.dollar()&&(blueClick||yellowClick||touch.wasClicked()||(wasOff&&interaction));
  bool locationQr=!consumed&&screens.location()&&locationState==5&&blueClick;
  if(locationQr){showLocationQr=!showLocationQr;consumed=true;}
  bool locationManual=!consumed&&screens.location()&&(yellowClick||touch.wasClicked());
  bool hasNetwork=!networks[0].ssid.isEmpty()||!networks[1].ssid.isEmpty();
  if(manual){if(!hasNetwork)requestSetup();else{fetchRequested=true;lastRequest=now;}}
  if(locationManual){if(!hasNetwork||!locationConfigured())requestSetup();else{locationState=1;locationRequested=true;}}
  bool viewing=screens.dollar()&&brightness>0;
  if(viewing&&interval&&now-lastRequest>=interval){fetchRequested=true;lastRequest=now;}
  bool weatherVisible=screens.weather()&&brightness>0;
  if(weatherVisible){
    weatherMode=true;bool refreshClick=!consumed&&yellowClick;
    if(!weatherBusy&&!fetchRequested&&weatherRefresh.due(forecast,now,time(nullptr),refreshClick)){
      if(!hasNetwork)requestSetup();else{weatherRefresh.requested(now);fetchRequested=true;}
    }
  }
  bool locating=screens.location()&&locationConfigured()&&(locationRequested||(locationState.load()>=1&&locationState.load()<=4));
  wantNetwork=hasNetwork&&((viewing&&(interval>0||fetchRequested||networkState==3))||
    (screens.weather()&&brightness>0&&(fetchRequested||weatherBusy))||locating);
  bool changed=false;Quote next;
  if(xQueueReceive(quotes,&next,0)==pdTRUE&&next.epoch>=quote.epoch){quote=next;changed=true;if(historyCount==120){memmove(history,history+1,119*sizeof(float));historyCount--;}history[historyCount++]=(quote.bid+quote.ask)/2;}
  location::Fix fix;if(xQueueReceive(locationFixes,&fix,0)==pdTRUE){currentFix=fix;changed=true;}
  weather::Forecast nextForecast;if(xQueueReceive(forecasts,&nextForecast,0)==pdTRUE){forecast=nextForecast;changed=true;}
  if(brightness>0&&(changed||interaction||motionWoke||now-lastFrame>=(screens.tank()?medaka::energy.frameInterval():screens.pomodoro()?200:screens.paradise()?50:1000)||screenChanged)){lastFrame=now;render();}
  if(Serial.available()){
    char command=Serial.read();
    if(command=='P'){openApps();screens.selected=static_cast<int>(aquarium::App::Paradise);launchApp(now);render();}
    else if(command=='F'){
      Serial.print("FRAME_RGB 466 466\n");
      uint8_t row[466*3];
      for(int y=0;y<466;++y){frame.readRectRGB(0,y,466,1,row);Serial.write(row,sizeof(row));}
    }
    else if(command=='?')Serial.printf("FX HOTSPOT version=%s wifi=%d network=%d state=%d quote=%d battery=%d charging=%d brightness=%d screen=%s heap=%u http=%d api=%d cpu=%u weather=%d weather_http=%d pet_stage=%u pet_level=%u pet_saved=%u\n",kFirmwareVersion,WiFi.getMode()!=WIFI_OFF,selectedNetwork.load(),networkState.load(),quote.received!=0,level,charging,appliedBrightness,screens.name(),ESP.getFreeHeap(),lastHttpStatus.load(),lastApiStatus.load(),getCpuFrequencyMhz(),forecast.valid,weatherHttp.load(),paradise::state.stage,paradise::state.level,paradise::sequence);
  }
  delay(screens.tank()?medaka::energy.loopDelay():brightness?20:50);
}
