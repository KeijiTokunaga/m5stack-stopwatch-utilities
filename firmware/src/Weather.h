#pragma once
#include <ArduinoJson.h>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <ctime>

namespace weather {
constexpr uint32_t refreshMs=30UL*60*1000, retryMs=60000;
constexpr const char* url="https://api.open-meteo.com/v1/forecast?latitude=34.6937&longitude=135.5023&current=temperature_2m,weather_code&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max&timezone=Asia%2FTokyo&forecast_days=3";
struct Day { char date[11]{}; float high=0,low=0; int code=0,rain=-1; };
struct Forecast {
  bool valid=false;
  uint32_t received=0;
  time_t observed=0;
  char localTime[17]{};
  float temperature=0;
  int code=0;
  Day days[3];
};
inline bool codeValid(int c){
  return (c>=0&&c<=3)||c==45||c==48||c==51||c==53||c==55||c==56||c==57||
    c==61||c==63||c==65||c==66||c==67||c==71||c==73||c==75||c==77||
    c==80||c==81||c==82||c==85||c==86||c==95||c==96||c==99;
}
inline const char* description(int c){
  if(c==0)return "Clear";
  if(c==1)return "Mostly clear";
  if(c==2)return "Partly cloudy";
  if(c==3)return "Overcast";
  if(c==45||c==48)return "Fog";
  if(c>=95)return "Thunderstorm";
  if((c>=71&&c<=77)||c==85||c==86)return "Snow";
  if(c==56||c==57||c==66||c==67)return "Freezing rain";
  if(c>=51&&c<=55)return "Drizzle";
  if(c>=80&&c<=82)return "Rain showers";
  if(c>=61&&c<=65)return "Rain";
  return "Unknown";
}
// API local timestamps use JST; mktime operates in the firmware's UTC timezone.
inline time_t localEpoch(const char* text,bool withTime){
  if(strlen(text)!=(withTime?16u:10u))return 0;
  tm parsed{};
  if(!strptime(text,withTime?"%Y-%m-%dT%H:%M":"%Y-%m-%d",&parsed))return 0;
  tm original=parsed;time_t value=mktime(&parsed);
  if(parsed.tm_year!=original.tm_year||parsed.tm_mon!=original.tm_mon||parsed.tm_mday!=original.tm_mday||
     parsed.tm_hour!=original.tm_hour||parsed.tm_min!=original.tm_min||value<1700000000)return 0;
  return value-9*3600;
}
inline bool temperature(JsonVariantConst v,float& value){
  if(!v.is<float>())return false;
  value=v.as<float>();return std::isfinite(value)&&value>=-90&&value<=65;
}
inline bool parse(const char* body,size_t length,uint32_t now,Forecast& result){
  if(length>12288)return false;
  JsonDocument doc;
  if(deserializeJson(doc,body,length)||doc["error"]==true||doc["utc_offset_seconds"]!=32400)return false;
  Forecast next;
  const char* stamp=doc["current"]["time"]|"";
  next.observed=localEpoch(stamp,true);if(!next.observed)return false;
  if(!temperature(doc["current"]["temperature_2m"],next.temperature))return false;
  if(!doc["current"]["weather_code"].is<int>())return false;
  next.code=doc["current"]["weather_code"];if(!codeValid(next.code))return false;
  memcpy(next.localTime,stamp,17);
  auto daily=doc["daily"];
  for(const char* field:{"time","weather_code","temperature_2m_max","temperature_2m_min","precipitation_probability_max"})
    if(!daily[field].is<JsonArray>()||daily[field].size()!=3)return false;
  time_t previous=0;
  for(int i=0;i<3;++i){
    auto& d=next.days[i];const char* date=daily["time"][i]|"";
    time_t epoch=localEpoch(date,false);
    if(!epoch||(i&&epoch-previous!=86400)||(!i&&strncmp(date,stamp,10)))return false;
    previous=epoch;memcpy(d.date,date,11);
    if(!temperature(daily["temperature_2m_max"][i],d.high)||!temperature(daily["temperature_2m_min"][i],d.low)||d.low>d.high)return false;
    if(!daily["weather_code"][i].is<int>())return false;
    d.code=daily["weather_code"][i];if(!codeValid(d.code))return false;
    auto rain=daily["precipitation_probability_max"][i];
    if(!rain.isNull()){
      if(!rain.is<int>())return false;
      d.rain=rain;if(d.rain<0||d.rain>100)return false;
    }
  }
  next.valid=true;next.received=now;result=next;return true;
}
inline bool stale(const Forecast& f,uint32_t now,time_t epoch){
  return !f.valid||uint32_t(now-f.received)>=refreshMs||
    (epoch>=1700000000&&(epoch-f.observed>7200||f.observed-epoch>3600));
}
struct Refresh {
  bool attempted=false;
  uint32_t last=0;
  bool due(const Forecast& f,uint32_t now,time_t epoch,bool manual=false)const {
    return (!attempted||uint32_t(now-last)>=retryMs)&&(manual||stale(f,now,epoch));
  }
  void requested(uint32_t now){attempted=true;last=now;}
};
}
