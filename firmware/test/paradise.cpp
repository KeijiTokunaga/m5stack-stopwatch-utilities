#include "paradise/Model.h"
#include "paradise/Storage.h"
#include "paradise/MiniGame.h"
#include <cassert>
#include <cstdio>
using namespace paradise;
constexpr int64_t noon=1789873200; // tests explicitly set local hour below
int64_t at(int h){return 20000LL*86400+(h-9)*3600;}
State adult(){State s;initialize(s,12,at(12));s.stage=Adult;s.completedAdults=1;s.level=3;return s;}
int main(){
 State s;initialize(s,5,at(12));assert(valid(s));advance(s,at(12)+60);assert(s.stage==Baby);
 s.food=s.joy=4;s.decay=0;minute(s,at(12));minute(s,at(12));assert(s.food==4);minute(s,at(12));assert(s.food==3);
 s=adult();s.food=s.joy=0;s.viruses=3;s.sleeping=true;auto original=s;for(int i=0;i<30;++i)minute(s,at(23));assert(s.misses==0&&s.sickAge==0&&s.decay==0);
 s=adult();assert(sitter(s,at(12))&&s.coins==200);for(int i=0;i<30;++i)minute(s,at(12));assert(s.food==4&&s.decay==0);minute(s,at(19));assert(!s.sitter&&s.latePickup);assert(sitter(s,at(19))&&s.joy==3);
 s=adult();s.food=0;for(int i=0;i<19;++i)minute(s,at(12));assert(s.misses==0&&(s.alert&1));minute(s,at(12));assert(s.misses==1&&!(s.alert&1));for(int i=0;i<10;++i)minute(s,at(12));assert(s.misses==1);assert(feed(s,3));assert(s.call[0]==0&&!(s.missed&1));
 s=adult();s.food=3;assert(feed(s,3)&&s.fullFood==1);assert(!feed(s,3)&&s.fullFood==1);
 s.misses=0;s.fullFood=s.fullJoy=5;assert(adultRank(s)==0);s.fullJoy=4;assert(adultRank(s)==1);s.misses=2;assert(adultRank(s)==2);s.misses=6;assert(adultRank(s)==3);
 s=adult();s.poop=2;s.poopAge=29;assert(clean(s)&&s.fuel==50);s.poop=2;s.poopAge=30;assert(clean(s)&&s.fuel==50);
 s=adult();s.viruses=3;s.sickAge=119;minute(s,at(12));assert(s.reaperAge==1&&!cure(s));for(int i=0;i<10;++i)assert(rescue(s));assert(!s.reaperAge);for(int i=0;i<3;++i)assert(cure(s));assert(!s.viruses&&!s.sickAge);
 s=adult();s.viruses=3;s.sickAge=120;s.reaperAge=9;minute(s,at(12));assert(s.dead);assert(newEgg(s)&&!s.dead&&s.stage==Egg&&s.generation==2);
 s=adult();s.sleeping=false;assert(lights(s,at(19))&&s.sleeping);assert(!lights(s,at(5)));assert(lights(s,at(6))&&!s.sleeping);
 initialize(s,7,at(12));s.stage=Baby;s.fieldMarks=4;assert(confirmGrowth(s)&&s.stage==Kids&&s.level==1);s.stageAge=1439;s.foodMarks[2]=3;minute(s,at(12));assert(s.stage==Young&&s.family==2);s.stageAge=1439;minute(s,at(12));assert(s.stage==Adult&&s.completedAdults==1&&s.level==3);
 assert(release(s)&&s.stage==Egg&&s.level==4&&s.residentCount[0]==1);s.stage=Baby;assert(changeField(s)&&s.field==1&&s.level==5&&s.fieldMarks==0);
 s.stageAge=239;s.fieldMarks=0;minute(s,at(12));assert(s.giant&&s.stage==Adult&&!release(s)&&!changeField(s)&&breed(s,0,at(12))==-1);
 s=adult();s.coins=1000;assert(buy(s,0)&&s.items[0]==1&&s.coins==950);s.level=5;s.items[0]=2;assert(cook(s,0)&&s.items[0]==0&&s.items[4]==1);s.food=2;assert(feed(s,0,true)&&s.food==4&&s.items[4]==0);
 s=adult();assert(hunt(s,at(12),false)>0);assert(hunt(s,at(13),true)==0);assert(hunt(s,at(23)+86400,true)==0);assert(hunt(s,at(12)+86400,true)>0);
 s=adult();s.level=6;s.fuel=100;assert(travel(s)&&s.fuel==0);assert(!travel(s));assert(decorate(s));auto money=s.coins;assert(sell(s)&&s.coins>=money+500);
 s=adult();s.breedDay=day(at(12));s.breedFailures=3;assert(breed(s,0,at(12))==-1);assert(breed(s,0,at(12)+86400)>=0);
 s=adult();s.sleeping=true;auto joy=s.joy;assert(reward(s,3,true,false)==300&&s.joy==joy);s.level=9;assert(reward(s,3,true,true)==800);
 Snapshot a{},b{};a.state=adult();a.sequence=3;a.hash=checksum(a);b=a;b.sequence=4;b.hash=checksum(b);assert(newest(a,b)==&b);b.state.coins++;assert(newest(a,b)==&a);a.hash++;assert(!newest(a,b));
 s=adult();MiniGame g;g.begin(s,1,false,false,100);g.input(s,true,900);assert(g.score==1&&g.round==1);g.begin(s,0,true,false,0);for(int i=0;i<30;++i)g.input(s,i%2,i*100);g.tick(s,10000);assert(g.done&&g.stars()==3);
 // Equivalent minute updates for sparse versus frequent rendering.
 State a1=adult(),a2=a1;advance(a1,at(12)+1800);for(int i=1;i<=1800;++i)advance(a2,at(12)+i);assert(a1.food==a2.food&&a1.joy==a2.joy&&a1.stageAge==a2.stageAge&&a1.misses==a2.misses);
 // Inheritance must use only the selected two parents, across all four pairings.
 unsigned combinations=0;
 for(unsigned seed=1;seed<=500;++seed){auto family=adult();family.seed=seed;family.genes={1,2};family.candidates[0]={3,4};family.dates[0]=10;
   if(breed(family,0,at(12))==1){assert((family.genes.eyes==1||family.genes.eyes==3)&&(family.genes.color==2||family.genes.color==4));combinations|=1u<<((family.genes.eyes==3?1:0)+(family.genes.color==4?2:0));}}
 assert(combinations==15);
 // Simulate random play/care over months; preserve all persistence invariants.
 State stress=adult();for(int i=0;i<100000;++i){int action=random(stress)%12;int64_t stamp=at(12)+int64_t(i)*60;
   if(stress.dead)newEgg(stress);
   switch(action){case 0:feed(stress,3);break;case 1:snack(stress);break;case 2:clean(stress);break;case 3:cure(stress);break;case 4:rescue(stress);break;case 5:confirmGrowth(stress);break;case 6:reward(stress,2,false,false);break;case 7:hunt(stress,stamp,false);break;case 8:lights(stress,stamp);break;case 9:changeField(stress);break;case 10:release(stress);break;default:break;}
   advance(stress,stamp);assert(valid(stress));}
 puts("PASS: growth, care, grace, sleep, sitter, illness/rescue/death, economy, hunting, inheritance limits, levels, snapshots, minigames, elapsed time");
}
