#pragma once
namespace aquarium {
struct Battery {
  int level=-1;
  int update(int measured,bool charging) {
    if(measured<0 || measured>100)return level;
    if(level<0 || charging || measured<level)level=measured;
    return level;
  }
};
}
