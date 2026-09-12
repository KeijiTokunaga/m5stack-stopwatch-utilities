#pragma once
#include <cstdint>
#include <cmath>
namespace aquarium {
class Energy {
public:
  static constexpr uint32_t IDLE_MS=60000;
  uint32_t lastActivity=0;
  bool eco=false, motionReady=false;
  float ax=0,ay=0,az=0;
  void begin(uint32_t now) {lastActivity=now;eco=false;motionReady=false;}
  bool motion(float x,float y,float z,float rx,float ry,float rz) {
    if(!std::isfinite(x+y+z+rx+ry+rz))return false;
    bool moved=motionReady && (std::fabs(x-ax)+std::fabs(y-ay)+std::fabs(z-az)>.10f
      || std::fabs(rx)+std::fabs(ry)+std::fabs(rz)>12);
    ax=x;ay=y;az=z;motionReady=true;return moved;
  }
  void update(uint32_t now,bool active) {
    if(active)lastActivity=now;
    eco=uint32_t(now-lastActivity)>=IDLE_MS;
  }
  int brightness(bool dim) const {return eco?(dim?22:45):(dim?70:150);}
  uint32_t frameInterval() const {return eco?200:33;}
  uint32_t loopDelay() const {return eco?20:5;}
};
}
