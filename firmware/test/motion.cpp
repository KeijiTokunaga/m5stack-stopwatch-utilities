#include "MotionWake.h"
#include "Energy.h"
#include <cassert>
#include <cstdio>
int main(){
 MotionWake m;Energy e;
 assert(!m.sample(0,0,1,0,0,0));
 for(int i=0;i<1000;++i)assert(!m.sample(.01f,0,1,1,1,1));
 assert(e.brightness(30000,50,false)==0);
 assert(!m.sample(.5f,0,1,0,0,0));
 assert(m.sample(.5f,0,1,0,0,0));
 e.wake(30100);assert(e.brightness(30100,50,false)==80);
 assert(e.brightness(45100,50,false)==15);
 assert(e.brightness(60100,50,false)==0);
 m.reset();assert(!m.sample(0,0,1,0,0,0));
 assert(!m.sample(0,0,1,0,60,0));
 assert(m.sample(0,0,1,0,60,0));
 assert(!m.sample(NAN,0,1,0,0,0));
 assert(!m.sample(0,0,1,0,0,0));
 assert(!m.sample(.5f,0,1,0,0,0));
 assert(!m.sample(0,0,1,0,0,0));
 puts("PASS: rest/noise, lift, rotation, invalid input, isolated spike, wake and idle timeout");
}
