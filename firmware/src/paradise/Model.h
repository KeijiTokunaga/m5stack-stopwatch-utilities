#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <type_traits>
namespace paradise {
enum Stage:uint8_t {Egg,Baby,Kids,Young,Adult};
struct Genes {uint8_t eyes=0,color=0;};
struct Resident {Genes genes;uint8_t species=0;int32_t huntDay=-1;};
struct State {
  uint32_t seed=1,generation=1,coins=500,age=0,stageAge=0;
  uint16_t completedAdults=0,misses=0,fullFood=0,fullJoy=0;
  uint8_t stage=Egg,field=0,genus=0,family=3,rank=0,level=0,food=4,joy=4;
  uint8_t viruses=0,poop=0,fuel=0,fieldMarks=0;
  uint16_t dwell=0,decay=0,poopTimer=0,poopAge=0,sickAge=0,reaperAge=0,meteorTimer=0;
  uint8_t call[3]{},missed=0,alert=0,rescueHits=0,meteor=0;
  bool initialized=false,dead=false,sleeping=false,sitter=false,latePickup=false,giant=false;
  bool changedField=false,sound=true,english=false,clockKnown=false;
  uint8_t birthdayMonth=1,birthdayDay=1,brightness=80;
  char planet[17]="PARADISE";
  Genes genes;
  uint16_t foodPoints[4]{},foodMarks[4]{},items[8]{},deco[6]{};
  uint8_t equipment[3]{},decoration=0;
  uint8_t residentCount[3]{};
  Resident residents[3][4]{};
  Genes candidates[4]{};uint8_t dates[4]{};
  uint8_t breedFailures=0;
  int32_t breedDay=-1,huntDay=-1,bathDay=-1;
  uint64_t discovered=0;
  int64_t lastEpoch=0;
};
static_assert(std::is_trivially_copyable<State>::value,"NVS snapshot must be POD");
inline uint32_t random(State& s){s.seed^=s.seed<<13;s.seed^=s.seed>>17;s.seed^=s.seed<<5;return s.seed?s.seed:(s.seed=1);}
inline int day(int64_t t){return int((t+32400)/86400);}
inline int hour(int64_t t){return int((t+32400)/3600%24);}
inline uint8_t adultRank(const State& s){return s.misses==0&&s.fullFood>=5&&s.fullJoy>=5?0:s.misses<=1?1:s.misses<=5?2:3;}
inline int species(const State& s){return s.giant?48:s.genus*16+s.family*4+s.rank;}
inline void credit(State& s,unsigned n){s.coins=std::min(999999u,s.coins+n);}
inline bool awake(const State& s){return s.initialized&&!s.dead&&!s.sleeping&&!s.sitter&&s.stage!=Egg;}
inline bool callActive(const State& s,int i){return i==0?s.food==0:i==1?s.joy==0:s.viruses!=0;}
inline void clearCalls(State& s){for(int i=0;i<3;++i)if(!callActive(s,i)){s.call[i]=0;s.missed&=~(1<<i);s.alert&=~(1<<i);}}
inline void progress(State& s){
  if(s.stage>=Kids)s.level=std::max<int>(s.level,1);
  if(s.stage>=Young)s.level=std::max<int>(s.level,2);
  if(s.completedAdults)s.level=std::max<int>(s.level,3);
  if(s.completedAdults&&s.generation>=2)s.level=std::max<int>(s.level,4);
  if(s.level>=4&&s.changedField)s.level=std::max<int>(s.level,5);
  if(s.level>=5&&s.completedAdults>=2)s.level=std::max<int>(s.level,std::min<int>(10,4+s.completedAdults));
}
inline void reroll(State& s){for(int i=0;i<4;++i){s.candidates[i]={uint8_t(random(s)%51),uint8_t(random(s)%16)};s.dates[i]=0;}}
inline void egg(State& s,Genes genes){
  s.age=s.stageAge=0;s.stage=Egg;s.food=s.joy=4;s.dead=s.giant=s.sleeping=s.sitter=s.latePickup=false;
  s.viruses=s.poop=s.fuel=s.fieldMarks=0;s.dwell=s.decay=s.poopTimer=s.poopAge=s.sickAge=s.reaperAge=s.meteorTimer=0;
  s.misses=s.fullFood=s.fullJoy=0;s.missed=s.alert=s.rescueHits=s.meteor=0;s.genes=genes;s.genus=s.field;s.family=3;s.rank=0;
  std::memset(s.call,0,sizeof(s.call));std::memset(s.foodPoints,0,sizeof(s.foodPoints));std::memset(s.foodMarks,0,sizeof(s.foodMarks));
  s.huntDay=s.bathDay=-1;reroll(s);progress(s);
}
inline void initialize(State& s,uint32_t seed,int64_t now){s=State{};s.initialized=true;s.seed=seed?seed:1;s.lastEpoch=now;egg(s,{uint8_t(random(s)%51),uint8_t(random(s)%16)});}
inline void becomeAdult(State& s,bool giant=false){
  s.stage=Adult;s.stageAge=0;s.giant=giant;s.rank=adultRank(s);if(s.completedAdults<65535)++s.completedAdults;
  s.discovered|=uint64_t(1)<<species(s);progress(s);
}
inline bool confirmGrowth(State& s){if(s.stage!=Baby||s.fieldMarks<4||s.dead)return false;s.stage=Kids;s.stageAge=0;s.genus=s.field;progress(s);return true;}
inline void minute(State& s,int64_t epoch){
  if(!s.initialized||s.dead)return;
  ++s.age;++s.stageAge;int h=hour(epoch);
  if(h>=22||h<6)s.sleeping=true;
  else if(h>=10&&h<19)s.sleeping=false;
  if(s.sitter&&h>=19){s.sitter=false;s.latePickup=true;}
  if(s.stage==Egg){if(s.stageAge>=1){s.stage=Baby;s.stageAge=0;}return;}
  if(s.stage==Baby){
    if(++s.dwell>=15){s.dwell=0;s.fieldMarks=std::min<int>(4,s.fieldMarks+1);}
    if(s.stageAge>=240&&s.fieldMarks<4)becomeAdult(s,true);
  }else if(s.stage==Kids&&s.stageAge>=1440){
    s.stage=Young;s.stageAge=0;s.family=3;
    for(int i=0;i<3;++i)if(s.foodMarks[i]>s.foodMarks[s.family])s.family=i;
    progress(s);
  }else if(s.stage==Young&&s.stageAge>=1440)becomeAdult(s);
  if(s.sleeping||s.sitter)return;
  if(++s.decay>=(s.stage==Baby?3:20)){s.decay=0;if(s.food)--s.food;if(s.joy)--s.joy;}
  if(s.poop&&s.poopAge<65535)++s.poopAge;
  if(++s.poopTimer>=60){s.poopTimer=0;if(!s.poop)s.poopAge=0;s.poop=std::min<int>(4,s.poop+1);}
  if(!s.viruses&&((s.poop&&s.poopAge>=60)||(s.food==0&&s.call[0]>=15)))s.viruses=3;
  for(int i=0;i<3;++i){
    if(callActive(s,i)&&!(s.missed&(1<<i))){s.alert|=1<<i;if(++s.call[i]>=20){if(s.misses<65535)++s.misses;s.missed|=1<<i;s.alert&=~(1<<i);}}
  }
  clearCalls(s);
  if(s.viruses){if(s.sickAge<65535)++s.sickAge;if(s.sickAge>=120&&++s.reaperAge>=10){s.dead=true;s.alert=0;}}
  else{s.sickAge=s.reaperAge=s.rescueHits=0;}
  if(s.meteor){if(++s.meteor>=3){s.viruses=3;s.meteor=0;s.meteorTimer=0;}}
  else if(++s.meteorTimer>=360){s.meteorTimer=0;s.meteor=1;}
}
// Minute-boundary updates preserve physiology across irregular frame rates.
inline void advance(State& s,int64_t now){
  if(!s.initialized||now<=s.lastEpoch)return;
  while(s.lastEpoch/60<now/60){s.lastEpoch=(s.lastEpoch/60+1)*60;minute(s,s.lastEpoch);if(s.dead){s.lastEpoch=now;return;}}
  s.lastEpoch=now;
}
inline void gainFood(State& s,int n){bool low=s.food<4;s.food=std::min<int>(4,s.food+n);if(low&&s.food==4&&s.fullFood<65535)++s.fullFood;clearCalls(s);}
inline void gainJoy(State& s,int n){bool low=s.joy<4;s.joy=std::min<int>(4,s.joy+n);if(low&&s.joy==4&&s.fullJoy<65535)++s.fullJoy;clearCalls(s);}
inline bool feed(State& s,int type,bool cooked=false){
  if(!awake(s)||type<0||type>3||s.food==4)return false;
  int item=type+(cooked?4:0);if(type!=3||cooked){if(!s.items[item])return false;--s.items[item];}
  gainFood(s,cooked?2:1);
  s.foodPoints[type]+=cooked?2:1;
  if(s.foodPoints[type]>=5){s.foodPoints[type]-=5;if(s.foodMarks[type]<65535)++s.foodMarks[type];}
  return true;
}
inline bool snack(State& s){if(!awake(s)||s.joy==4||s.coins<20)return false;s.coins-=20;gainJoy(s,1);return true;}
inline bool wash(State& s,int64_t now){if(!awake(s)||s.bathDay==day(now))return false;s.bathDay=day(now);gainJoy(s,1);return true;}
inline bool clean(State& s){if(!awake(s)||!s.poop)return false;if(s.poopAge<30)s.fuel=std::min<int>(100,s.fuel+25*s.poop);s.poop=s.poopAge=0;return true;}
inline bool cure(State& s){if(!awake(s)||!s.viruses||s.reaperAge)return false;--s.viruses;if(!s.viruses)s.sickAge=0;clearCalls(s);return true;}
inline bool rescue(State& s){if(!awake(s)||!s.reaperAge)return false;if(++s.rescueHits>=10){s.sickAge=s.reaperAge=s.rescueHits=0;}return true;}
inline bool lights(State& s,int64_t now){
  if(!s.initialized||s.dead||s.sitter)return false;int h=hour(now);
  if(h>=19&&h<22&&!s.sleeping){s.sleeping=true;return true;}
  if(h>=6&&h<10&&s.sleeping){s.sleeping=false;return true;}return false;
}
inline bool sitter(State& s,int64_t now){
  if(s.latePickup){s.latePickup=false;if(s.joy)--s.joy;return true;}
  if(s.sitter){s.sitter=false;return true;}
  if(!awake(s)||hour(now)<6||hour(now)>=19||s.coins<300)return false;s.coins-=300;s.sitter=true;return true;
}
inline int fields(const State& s){return s.level>=6?3:s.level>=4?2:1;}
inline bool changeField(State& s){if(!awake(s)||s.giant||fields(s)<2)return false;s.field=(s.field+1)%fields(s);s.changedField=true;if(s.stage==Baby){s.dwell=s.fieldMarks=0;}progress(s);return true;}
inline bool buy(State& s,int item){if(s.level<1||item<0||item>7||s.coins<(item<4?50u:100u)||s.items[item]>=999)return false;s.coins-=item<4?50:100;++s.items[item];return true;}
inline bool cook(State& s,int type){if(s.level<5||type<0||type>3||s.items[type]<2||s.items[type+4]>=999)return false;s.items[type]-=2;++s.items[type+4];return true;}
inline bool equipment(State& s){if(s.level<2||s.coins<200)return false;s.coins-=200;s.equipment[s.field]=(s.equipment[s.field]+1)%4;return true;}
inline bool travel(State& s){if(!awake(s)||s.level<6||s.fuel<100)return false;s.fuel=0;int destinations=std::min<int>(6,s.level-3);int destination=random(s)%destinations;if(s.deco[destination]<999)++s.deco[destination];return true;}
inline bool decorate(State& s){if(s.level<6)return false;for(int k=1;k<=6;++k){int i=(s.decoration+k)%6;if(s.deco[i]){s.decoration=i;return true;}}return false;}
inline bool sell(State& s){if(!s.deco[s.decoration])return false;--s.deco[s.decoration];credit(s,500+100*(s.decoration%3));return true;}
inline int hunt(State& s,int64_t now,bool spring){
  if(!awake(s)||hour(now)>=19||hour(now)<6||s.huntDay==day(now))return 0;
  s.huntDay=day(now);int partners=0;
  for(int f=0;f<3&&partners<2;++f)for(int i=0;i<s.residentCount[f]&&partners<2;++i)if(s.residents[f][i].huntDay!=day(now)){s.residents[f][i].huntDay=day(now);++partners;}
  // Missing partners are white NPC helpers: always a three-member expedition.
  bool great=s.joy==4&&random(s)%10==0;int n=great?9:1+random(s)%5;
  int type=spring?1:0;s.items[type]=std::min<int>(999,s.items[type]+n);return n;
}
inline bool release(State& s){
  if(!awake(s)||s.stage!=Adult||s.giant||s.residentCount[s.field]>=4)return false;
  auto& r=s.residents[s.field][s.residentCount[s.field]++];r.genes=s.genes;r.species=species(s);r.huntDay=s.huntDay;
  ++s.generation;egg(s,{uint8_t(random(s)%51),uint8_t(random(s)%16)});return true;
}
inline int breed(State& s,int choice,int64_t now){
  if(!awake(s)||s.stage!=Adult||s.giant||s.level<3||choice<0||choice>=4)return -1;
  if(s.breedDay!=day(now)){s.breedDay=day(now);s.breedFailures=0;}
  if(s.breedFailures>=3)return -1;
  int chance=std::min(95,40+15*int(s.dates[choice])+(s.food==4&&s.joy==4?20:0));
  if(random(s)%100>=unsigned(chance)){++s.breedFailures;s.dates[choice]=std::min<int>(10,s.dates[choice]+1);return 0;}
  Genes parent=s.genes,other=s.candidates[choice];uint32_t mix=random(s)%4;
  Genes child{mix&1?other.eyes:parent.eyes,mix&2?other.color:parent.color};++s.generation;egg(s,child);return 1;
}
inline bool newEgg(State& s){if(!s.dead)return false;++s.generation;egg(s,{uint8_t(random(s)%51),uint8_t(random(s)%16)});return true;}
inline unsigned reward(State& s,int stars,bool lab,bool hard){
  stars=std::max(0,std::min(3,stars));if(!stars)return 0;
  if(lab){if(s.level<1)return 0;unsigned amount=hard&&s.level>=9?(stars==3?800:stars*200):stars*100;credit(s,amount);return amount;}
  if(awake(s))gainJoy(s,stars>=2?2:1);return 0;
}
inline int monthDays(int month){static const int days[]={31,29,31,30,31,30,31,31,30,31,30,31};return days[std::max(1,std::min(12,month))-1];}
inline bool valid(const State& s){
  if(!s.seed||s.stage>Adult||s.field>=3||s.genus>=3||s.family>=4||s.rank>=4||s.level>10||s.food>4||s.joy>4||s.coins>999999||s.genes.eyes>=51||s.genes.color>=16||s.viruses>3||s.poop>4||s.fuel>100||s.fieldMarks>4||s.brightness<15||s.brightness>160||s.birthdayMonth<1||s.birthdayMonth>12||s.birthdayDay<1||s.birthdayDay>monthDays(s.birthdayMonth)||s.planet[16]!=0||s.lastEpoch<0||s.lastEpoch>4102444800LL)return false;
  for(int i=0;i<3;++i){if(s.residentCount[i]>4||s.call[i]>20)return false;for(auto& r:s.residents[i])if(r.genes.eyes>=51||r.genes.color>=16||r.species>48)return false;}
  for(auto& g:s.candidates)if(g.eyes>=51||g.color>=16)return false;
  for(auto n:s.items)if(n>999)return false;
  return true;
}
}
