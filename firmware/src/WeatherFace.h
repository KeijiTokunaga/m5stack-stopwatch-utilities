#pragma once
#include "WeatherClock.h"
// All positions refer to the 466px circular display.
void weatherIcon(int code,bool available){
  const uint16_t cloud=rgb(179,182,185),sun=rgb(229,193,80),rain=rgb(93,183,211);
  if(!available){frame.drawCircle(329,179,23,rgb(80,84,88));return;}
  bool clear=code==0,partial=code==1||code==2;
  if(clear||partial)frame.fillCircle(clear?329:309,clear?179:163,clear?26:19,sun);
  if(clear)return;
  frame.fillCircle(311,185,16,cloud);frame.fillCircle(328,174,22,cloud);
  frame.fillCircle(348,185,16,cloud);frame.fillRoundRect(304,182,52,19,9,cloud);
  if(code>=51&&code<=67||code>=80&&code<=82){
    for(int x=315;x<=345;x+=15){frame.drawWideLine(x,211,x-4,220,4,rain);}
  }else if(code>=71&&code<=77||code==85||code==86){
    for(int x=315;x<=345;x+=15)frame.fillCircle(x,214,3,cloud);
  }else if(code>=95){
    frame.fillTriangle(332,202,320,220,331,218,sun);frame.fillTriangle(331,212,323,235,343,212,sun);
  }else if(code==45||code==48){
    frame.drawWideLine(305,210,351,210,3,cloud);frame.drawWideLine(311,219,346,219,3,cloud);
  }
}
void paintWeatherFace(){
  frame.fillSprite(TFT_BLACK);frame.setTextDatum(middle_center);
  auto clock=weather::clockJst(time(nullptr));
  frame.setTextColor(rgb(202,202,205));frame.setFont(&fonts::FreeSans24pt7b);frame.setTextSize(2.5f);
  frame.drawString(clock.hour,163,177);frame.drawString(clock.minute,163,294);
  frame.setTextSize(1);weatherIcon(forecast.code,forecast.valid);
  frame.setFont(&fonts::FreeSansBold12pt7b);frame.setTextColor(rgb(191,188,188));
  frame.drawString(clock.date,329,255);
  frame.setFont(&fonts::FreeSans12pt7b);
  String temp=forecast.valid?String(forecast.temperature,0):String("--");
  int width=frame.textWidth(temp)+25;int x=329-width/2;
  frame.setTextDatum(middle_left);frame.drawString(temp,x,302);
  int unit=x+frame.textWidth(temp)+6;frame.drawCircle(unit,294,3,rgb(191,188,188));frame.drawString("C",unit+7,302);
  frame.setTextDatum(middle_center);frame.setFont(&fonts::Font2);
  const char* state=!clock.valid?"SYNCING TIME / JST":weatherBusy?"UPDATING":
    networkState==1?"CONNECTING":networkState==4?"NO WI-FI":weatherResult==3?"UPDATE FAILED / LAST DATA":
    !forecast.valid?"WAITING FOR WEATHER":weather::stale(forecast,millis(),time(nullptr))?"STALE WEATHER":"";
  frame.setTextColor(rgb(143,122,90));frame.drawString(state,233,368);
  frame.setTextColor(rgb(66,69,72));frame.drawString("OSAKA / Open-Meteo.com",233,405);
  frame.pushSprite(0,0);
}
