#pragma once
#include <ArduinoJson.h>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cstdint>
struct Quote { double bid=0,ask=0; time_t epoch=0; uint32_t received=0; bool open=false; };
inline bool parseQuote(const uint8_t* data,size_t length,uint32_t now,Quote& result) {
  if(length>4096)return false;
  JsonDocument doc;
  if(deserializeJson(doc,data,length))return false;
  if(strcmp(doc["symbol"]|"","USD_JPY"))return false;
  const char* bid=doc["bid"]|"";const char* ask=doc["ask"]|"";char *endBid,*endAsk;
  Quote next;next.bid=strtod(bid,&endBid);next.ask=strtod(ask,&endAsk);
  if(endBid==bid||*endBid||endAsk==ask||*endAsk||!std::isfinite(next.bid)||!std::isfinite(next.ask)||next.bid<=0||next.ask<next.bid)return false;
  const char* stamp=doc["timestamp"]|"";struct tm tm{};
  char* end=strptime(stamp,"%Y-%m-%dT%H:%M:%S",&tm);
  if(!end||(*end!='.'&&*end!='Z'))return false;
  if(*end=='.'){++end;const char* first=end;while(*end>='0'&&*end<='9')++end;if(end==first)return false;}
  if(*end!='Z'||end[1])return false;
  next.epoch=mktime(&tm); if(next.epoch<1700000000)return false;
  const char* state=doc["status"]|"";if(strcmp(state,"OPEN")&&strcmp(state,"CLOSE"))return false;
  next.open=!strcmp(state,"OPEN");next.received=now;result=next;return true;
}
inline bool quoteStale(const Quote& quote,uint32_t now,time_t epoch) {
  return !quote.received||uint32_t(now-quote.received)>15000||epoch-quote.epoch>30||quote.epoch-epoch>30;
}
