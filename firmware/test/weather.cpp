#include "Weather.h"
#include <cassert>
#include <fstream>
#include <iterator>
#include <string>
#include <cstdio>
int main(int argc,char** argv){
  setenv("TZ","UTC",1);tzset();
  std::string body=R"JSON({"latitude":34.7,"longitude":135.5,"generationtime_ms":0.12683868408203125,"utc_offset_seconds":32400,"timezone":"Asia/Tokyo","timezone_abbreviation":"GMT+9","elevation":3.0,"current_units":{"time":"iso8601","interval":"seconds","temperature_2m":"°C","weather_code":"wmo code"},"current":{"time":"2026-09-20T12:00","interval":900,"temperature_2m":27.3,"weather_code":3},"daily_units":{"time":"iso8601","weather_code":"wmo code","temperature_2m_max":"°C","temperature_2m_min":"°C","precipitation_probability_max":"%"},"daily":{"time":["2026-09-20","2026-09-21","2026-09-22"],"weather_code":[3,51,3],"temperature_2m_max":[27.4,27.2,28.2],"temperature_2m_min":[22.4,23.9,20.9],"precipitation_probability_max":[55,44,0]}})JSON";
  weather::Forecast f;
  assert(weather::parse(body.c_str(),body.size(),100,f));
  assert(f.valid&&f.temperature>27&&f.days[0].rain==55&&f.days[2].rain==0);
  assert(!weather::stale(f,101,f.observed));
  assert(weather::stale(f,100+weather::refreshMs,f.observed));
  assert(weather::stale(f,101,f.observed+7201));
  assert(weather::stale(f,101,f.observed-3601));
  f.received=UINT32_MAX-100;assert(!weather::stale(f,100,f.observed));
  weather::Refresh schedule;
  assert(!schedule.due(f,100,f.observed));
  assert(schedule.due(f,100,f.observed,true));schedule.requested(100);
  assert(!schedule.due(f,101,f.observed,true));
  assert(schedule.due(f,60100,f.observed,true));
  weather::Forecast empty;
  assert(!schedule.due(empty,60100-1,f.observed));
  assert(schedule.due(empty,60100,f.observed));
  schedule.requested(UINT32_MAX-100);
  assert(!schedule.due(empty,100,f.observed));
  assert(schedule.due(empty,60000,f.observed));
  JsonDocument doc;assert(!deserializeJson(doc,body));
  doc["daily"]["precipitation_probability_max"][0]=nullptr;
  std::string altered;serializeJson(doc,altered);
  assert(weather::parse(altered.c_str(),altered.size(),200,f)&&f.days[0].rain==-1);
  auto reject=[&](const std::string& bad){
    auto old=f;assert(!weather::parse(bad.c_str(),bad.size(),300,f));
    assert(f.received==old.received&&f.temperature==old.temperature);
  };
  for(const char* field:{"weather_code","temperature_2m_max","temperature_2m_min","time","precipitation_probability_max"}){
    deserializeJson(doc,body);doc["daily"].remove(field);altered.clear();serializeJson(doc,altered);reject(altered);
  }
  for(const auto& mutation:{std::pair<const char*,const char*>("27.3","null"),{"27.3",R"("27.3")"},{"27.3","999"},{"32400","0"},{"55,44,0","101,44,0"},{"2026-09-21","2026-09-25"},{"2026-09-20T12:00","2026-09-20T25:00"},{"2026-09-22","2026-09-32"}}){
    altered=body;auto pos=altered.find(mutation.first);assert(pos!=std::string::npos);
    altered.replace(pos,strlen(mutation.first),mutation.second);reject(altered);
  }
  reject("{}");reject(body.substr(0,body.size()/2));reject(std::string(12289,' '));
  assert(weather::codeValid(99)&&!weather::codeValid(100));
  assert(std::string(weather::description(95))=="Thunderstorm");
  if(argc>1){
    std::ifstream file(argv[1]);assert(file.good());std::string live((std::istreambuf_iterator<char>(file)),{});
    assert(weather::parse(live.c_str(),live.size(),400,f));
    printf("LIVE: %.1f C, %s, %s JST\n",f.temperature,weather::description(f.code),f.localTime);
  }
  puts("PASS: forecast parsing, missing/invalid data, cache preservation, freshness, retry throttling, rollover");
}
