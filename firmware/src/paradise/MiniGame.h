#pragma once
#include "Model.h"
namespace paradise {
struct MiniGame {
  int kind=0,round=0,score=0,cursor=0,target=0,lastButton=-1,steps=0;
  bool lab=false,hard=false,done=false;
  uint32_t started=0,roundAt=0;
  void begin(State& s,int k,bool bank,bool difficult,uint32_t now){*this=MiniGame{};kind=k;lab=bank;hard=difficult;started=roundAt=now;target=random(s)%3;}
  int position(uint32_t now)const{return int((now-roundAt)%(hard?1000:1600))*100/(hard?1000:1600);}
  bool timed()const{return kind==1||kind==3||kind==5||kind==9;}
  void next(State& s,uint32_t now){if(++round>=5)done=true;else{roundAt=now;target=random(s)%3;cursor=0;}}
  void tick(State& s,uint32_t now){
    if(done)return;
    if(kind==0){if(now-started>=10000){score=std::min(5,steps/6);done=true;}return;}
    if(now-started>=45000){done=true;return;}
    if(now-roundAt>=6000)next(s,now);
  }
  void input(State& s,bool blue,uint32_t now){
    if(done)return;
    if(kind==0){if(lastButton!=int(blue)){++steps;lastButton=blue;}return;}
    if(kind==7){if(int(blue)==target%2)++score;next(s,now);return;}
    if(!blue){cursor=(cursor+1)%3;return;}
    if(kind==8&&now-roundAt<1200)return;
    bool win=timed()?(std::abs(position(now)-50)<(hard?9:17)):
      kind==4?cursor==target:kind==6?cursor==target:cursor==target;
    if(win)++score;next(s,now);
  }
  int stars()const{return score>=5?3:score>=3?2:score>=1?1:0;}
};
}
