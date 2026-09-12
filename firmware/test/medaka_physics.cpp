#include "../src/medaka/World.h"
#include <cassert>
#include <cstdio>
using namespace aquarium;
int main() {
  World w;
  int population[3]{};for(auto &f:w.fish)++population[static_cast<int>(f.species)];
  for(int n:population)assert(n==3);
  constexpr float dt=1.f/120;
  // Ten minutes: alternating violent shaking and rest, plus repeated taps.
  for(int n=0;n<72000;++n) {
    float t=n*dt, shake=(n%3600<1200)?1.7f:0.f;
    w.sense(shake*std::sin(t*19),1+shake*std::sin(t*13),shake*std::cos(t*17),dt,shake*180*std::sin(t*11),shake*90*std::cos(t*7),shake*250*std::cos(t*19));
    if(n%180==0) w.feed(n%360==0?155:311);
    assert(w.foodCount()<=World::FOOD_MAX);
    if(n%719==0) w.ripple((n*37)%466,100);
    w.step(dt);
    float mean=0;
    for(float h:w.wave) { assert(std::isfinite(h)); assert(std::fabs(h)<97); mean+=h; }
    assert(std::fabs(mean)<.01f);
    for(auto f:w.fish) {
      assert(std::isfinite(f.x+f.y+f.vx+f.vy));
      assert(f.y>=w.surface(f.x)+19.9f);
      assert(std::hypot(f.x-233,f.y-233)<199.1f);
    }
  }
  World calm; calm.ripple(230,100);
  for(int n=0;n<4800;++n) calm.step(dt);
  for(float h:calm.wave) assert(std::fabs(h)<.02f);
  calm.sense(NAN,1,0,dt); assert(std::isfinite(calm.tilt));
  for(int n=0;n<1200;++n) calm.sense(.5f,1,0,dt);
  assert(calm.tilt<-.4f && calm.tilt>=-.43f);
  World rotation; float peak=0,after=0;
  for(int n=0;n<1200;++n) {
    float t=n*dt;
    rotation.sense(0,1,0,dt,0,0,n<240?200*std::cos(t*8):0);
    rotation.step(dt); peak=std::max(peak,std::fabs(rotation.slosh));
    if(n>240&&n<480)after=std::max(after,std::fabs(rotation.slosh));
  }
  assert(peak>25); assert(after>5); assert(std::fabs(rotation.slosh)<1);
  std::printf("Gyro-only: peak %.1f px, after-motion %.1f px\n",peak,after);
  for(float side : {155.f,311.f}) {
    World fed; fed.feed(side); assert(fed.foodCount()==6);
    fed.feed(side); assert(fed.foodCount()==6); // Button spam cannot overfill.
    for(int n=0;n<1800;++n) { fed.sense(0,1,0,dt); fed.step(dt); }
    std::printf("Feeding at %.0f: %u eaten, %d remaining\n",side,fed.eaten,fed.foodCount());
    assert(fed.eaten>=4); assert(fed.foodCount()<=2);
    for(int n=0;n<1800;++n) fed.step(dt);
    assert(fed.foodCount()==0);
  }
  for(auto species : {Species::Medaka,Species::NeonTetra,Species::Guppy}) {
    World single;
    for(auto &f:single.fish) f.species=species;
    single.feed(233);
    for(int n=0;n<1800;++n) { single.sense(0,1,0,dt);single.step(dt); }
    assert(single.eatenBySpecies[static_cast<int>(species)]>=4);
    std::printf("Species %d: %u pellets eaten\n",int(species),single.eaten);
  }
  World roaming;
  float spread=0; int samples=0;
  for(int n=0;n<14400;++n) {
    roaming.sense(0,1,0,dt);roaming.step(dt);
    if(n>2400 && n%120==0) {
      for(int i=0;i<World::COUNT;++i) for(int j=i+1;j<World::COUNT;++j)
        spread+=std::hypot(roaming.fish[i].x-roaming.fish[j].x,roaming.fish[i].y-roaming.fish[j].y);
      ++samples;
    }
  }
  float meanDistance=spread/(samples*36);
  std::printf("Independent swimming: mean pair distance %.1f px\n",meanDistance);
  assert(meanDistance>110);
  World ambience;
  ambience.updateLight(60);assert(std::fabs(ambience.tint[1]-1.12f)<.001f);
  ambience.toggleLight();float saved=ambience.lightClock;
  ambience.updateLight(10);assert(ambience.lightClock==saved);
  ambience.toggleLight();ambience.updateLight(180);assert(ambience.lightClock==0);
  for(int n=0;n<30000;++n) {
    ambience.step(dt);
    for(auto b:ambience.bubbles)assert(std::isfinite(b.x+b.y+b.pop) && b.x>=80 && b.x<=386 && b.y<=410);
  }
  puts("PASS: lighting cycle, freeze/resume and bubble recycling");
  World expiry; expiry.feed(155);
  for(auto &p:expiry.food) if(p.life>0) {p.life=.001f;p.wet=false;}
  expiry.step(dt); assert(expiry.foodCount()==0);
  puts("PASS: ten-minute shake/tap stress, fish containment, wave decay, volume conservation, invalid IMU input, tilt response");
}
