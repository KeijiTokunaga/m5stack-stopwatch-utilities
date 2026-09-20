#pragma once
namespace aquarium {
enum class App { Dollar, Aquarium, Battery, Wifi, Pomodoro, Location };
struct Screens {
  App app=App::Dollar;
  bool menu=false;
  int selected=0;
  static constexpr int count=6, left=88, top=85, width=290, rowHeight=40, rowGap=7;
  void openMenu(){selected=static_cast<int>(app);menu=true;}
  void next(){selected=(selected+1)%count;}
  void launch(){app=static_cast<App>(selected);menu=false;}
  static int hit(int x,int y){
    if(x<left||x>=left+width||y<top)return -1;
    int row=(y-top)/(rowHeight+rowGap);
    return row<count&&(y-top)%(rowHeight+rowGap)<rowHeight?row:-1;
  }
  bool dollar() const {return !menu&&app==App::Dollar;}
  bool tank() const {return !menu&&app==App::Aquarium;}
  bool battery() const {return !menu&&app==App::Battery;}
  bool pomodoro() const {return !menu&&app==App::Pomodoro;}
  bool location() const {return !menu&&app==App::Location;}
  const char* name() const {
    if(menu)return "menu";
    switch(app){case App::Aquarium:return "aquarium";case App::Battery:return "battery";case App::Wifi:return "setup";case App::Pomodoro:return "pomodoro";case App::Location:return "location";default:return "fx";}
  }
};
}
