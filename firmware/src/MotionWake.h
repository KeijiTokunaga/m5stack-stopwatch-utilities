#pragma once
#include <cmath>
#include <cstdint>
// Samples are taken every 50 ms. Two strong samples reject isolated sensor noise.
struct MotionWake {
  bool ready=false;
  float x=0,y=0,z=0;
  unsigned strong=0;
  void reset(){ready=false;strong=0;}
  bool sample(float ax,float ay,float az,float gx,float gy,float gz){
    if(!std::isfinite(ax+ay+az+gx+gy+gz)){reset();return false;}
    if(!ready){x=ax;y=ay;z=az;ready=true;return false;}
    float dx=ax-x,dy=ay-y,dz=az-z;
    bool moving=dx*dx+dy*dy+dz*dz>.22f*.22f ||
                gx*gx+gy*gy+gz*gz>45.f*45.f;
    x+=dx*.1f;y+=dy*.1f;z+=dz*.1f;
    strong=moving?strong+1:0;
    if(strong>=2){strong=0;return true;}
    return false;
  }
};
