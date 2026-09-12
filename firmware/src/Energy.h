#pragma once
#include <cstdint>
struct Energy {
  uint32_t activity=0;
  void wake(uint32_t now){activity=now;}
  int brightness(uint32_t now,int level,bool charging,bool setup=false) const {
    if(setup)return 90;
    if(charging)return 120;
    uint32_t idle=now-activity;
    if(idle>=30000)return 0;
    if(idle>=15000)return 15;
    return level>=0&&level<=20?40:80;
  }
  uint32_t refresh(int level,bool charging) const {
    if(charging)return 5000;
    if(level>=0&&level<=10)return 0;
    return level>=0&&level<=20?15000:5000;
  }
};
