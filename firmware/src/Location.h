#pragma once
#include <ArduinoJson.h>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace location {
struct Fix {
  double lat=0,lng=0,accuracy=0;
  bool valid=false;
};

inline int hex(char value){
  if(value>='0'&&value<='9')return value-'0';
  if(value>='a'&&value<='f')return value-'a'+10;
  if(value>='A'&&value<='F')return value-'A'+10;
  return -1;
}

inline bool validBssid(const char* mac){
  if(!mac||std::strlen(mac)!=17)return false;
  uint8_t bytes[6]{};
  for(int i=0;i<6;i++){
    int high=hex(mac[i*3]),low=hex(mac[i*3+1]);
    if(high<0||low<0||(i<5&&mac[i*3+2]!=':'))return false;
    bytes[i]=uint8_t(high*16+low);
  }
  if((bytes[0]&0x03)!=0)return false;
  bool broadcast=true;for(uint8_t value:bytes)broadcast&=value==0xff;
  if(broadcast)return false;
  return !(bytes[0]==0x00&&bytes[1]==0x00&&bytes[2]==0x5e);
}

inline bool parseFix(const uint8_t* data,size_t size,Fix& output){
  JsonDocument doc;
  if(!data||!size||size>2048||deserializeJson(doc,data,size))return false;
  if(!doc["lat"].is<double>()||!doc["lng"].is<double>()||!doc["accuracy"].is<double>())return false;
  Fix next;
  next.lat=doc["lat"].as<double>();next.lng=doc["lng"].as<double>();next.accuracy=doc["accuracy"].as<double>();
  if(!std::isfinite(next.lat)||next.lat<-90||next.lat>90||!std::isfinite(next.lng)||next.lng<-180||next.lng>180||
     !std::isfinite(next.accuracy)||next.accuracy<0)return false;
  next.valid=true;output=next;return true;
}
}
