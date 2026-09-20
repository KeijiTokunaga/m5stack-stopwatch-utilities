#include <cassert>
#include <string>
#include "Battery.h"
#include "Screens.h"
#include "Quote.h"
#include "Energy.h"
#include "Location.h"
int main(){
 Energy energy;energy.wake(100);
 assert(energy.brightness(15099,50,false)==80);
 assert(energy.brightness(15100,50,false)==15);
 assert(energy.brightness(30100,50,false)==0);
 assert(energy.brightness(30100,50,true)==120);
 assert(energy.refresh(20,false)==15000&&energy.refresh(10,false)==0&&energy.refresh(50,false)==5000);
 assert(energy.refresh(10,true)==5000);
 energy.wake(UINT32_MAX-100);assert(energy.brightness(100,50,false)==80);
 setenv("TZ","UTC",1);tzset();
 aquarium::Screens s;assert(s.dollar());
 for(int app=0;app<6;++app){
   s.openMenu();assert(s.menu&&!s.dollar()&&!s.tank());
   s.selected=app;s.launch();assert(!s.menu&&static_cast<int>(s.app)==app);
   s.openMenu();assert(s.selected==app);
 }
 s.selected=5;s.next();assert(s.selected==0);
 const int centers[]={105,152,199,246,293,340};for(int row=0;row<6;++row)assert(s.hit(233,centers[row])==row);
 assert(s.hit(87,105)==-1&&s.hit(378,105)==-1&&s.hit(233,126)==-1&&s.hit(233,368)==-1);
 s.selected=2;s.launch();assert(s.battery()&&!s.dollar());
 s.openMenu();s.selected=1;s.launch();assert(s.tank()&&!s.battery());
 s.openMenu();s.selected=5;s.launch();assert(s.location()&&!s.dollar());
 aquarium::Battery b;assert(b.update(-1,false)==-1);assert(b.update(70,false)==70);assert(b.update(75,false)==70);assert(b.update(80,true)==80);assert(b.update(101,true)==80);
 std::string valid=R"({"symbol":"USD_JPY","bid":"153.490","ask":"153.587","timestamp":"2026-09-09T22:04:38.478Z","status":"OPEN"})";
 Quote q;assert(parseQuote((const uint8_t*)valid.data(),valid.size(),100,q));assert(q.open&&q.bid==153.49);
 assert(!quoteStale(q,101,q.epoch));assert(quoteStale(q,15101,q.epoch));assert(quoteStale(q,101,q.epoch+31));
 q.received=UINT32_MAX-100;assert(!quoteStale(q,100,q.epoch));
 for(auto change:{std::make_pair("153.490","nan"),std::make_pair("153.490","153x"),std::make_pair("153.587","100"),std::make_pair("USD_JPY","EUR_JPY"),std::make_pair("OPEN","BOGUS")}){
 auto bad=valid;bad.replace(bad.find(change.first),strlen(change.first),change.second);assert(!parseQuote((const uint8_t*)bad.data(),bad.size(),100,q));}
 assert(!parseQuote((const uint8_t*)"{}",2,100,q));
 assert(location::validBssid("00:11:22:33:44:55"));
 assert(!location::validBssid("02:11:22:33:44:55"));
 assert(!location::validBssid("ff:ff:ff:ff:ff:ff"));
 assert(!location::validBssid("00:00:5e:12:34:56"));
 location::Fix fix;
 std::string position=R"({"lat":35.681236,"lng":139.767125,"accuracy":42.5})";
 assert(location::parseFix((const uint8_t*)position.data(),position.size(),fix));
 assert(fix.valid&&fix.lat==35.681236&&fix.lng==139.767125&&fix.accuracy==42.5);
 const char* invalidPosition="{\"lat\":91,\"lng\":0,\"accuracy\":1}";
 assert(!location::parseFix((const uint8_t*)invalidPosition,strlen(invalidPosition),fix));
}
