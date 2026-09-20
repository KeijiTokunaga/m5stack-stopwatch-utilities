#pragma once
// Shared scene drawing for the device and the desktop reference renderer.
void plant(float root,int height,int index,bool front) {
  float px=root,py=441;
  uint16_t stem=front?color(39,102,77):color(26,75,66);
  for(int j=1;j<=height/6;++j) {
    float s=j*6.f, bend=s/height;
    float x=root+std::sin(world.time*.75f+index+s*.018f)*12*bend+world.current*.12f*bend+world.tilt*s*.24f;
    float y=441-s;
    frame.drawLine(px,py,x,y,stem);
    frame.drawLine(px+1,py,x+1,y,stem);
    if(j%3==0) {
      float side=(j%2)?1:-1;
      frame.fillTriangle(x,y,x+side*(12+bend*7),y-14,x+side*5,y+3,front?color(49,125,88):color(29,85,70));
      frame.drawLine(x,y,x+side*13,y-12,stem);
    }
    px=x;py=y;
  }
}

void drawFish(const Fish &f,int index) {
  bool neon=f.species==Species::NeonTetra, guppy=f.species==Species::Guppy;
  float co=std::cos(f.heading),si=std::sin(f.heading),sc=f.depth*(guppy?1.1f:1.f);
  float tail=std::sin(f.phase)*3.7f, wag=std::sin(f.phase-.8f)*1.5f;
  auto point=[&](float x,float y) { return std::pair<int,int>(f.x+(x*co-y*si)*sc,f.y+(x*si+y*co)*sc); };
  auto tri=[&](float ax,float ay,float bx,float by,float cx,float cy,uint16_t c) {
    auto a=point(ax,ay),b=point(bx,by),d=point(cx,cy); frame.fillTriangle(a.first,a.second,b.first,b.second,d.first,d.second,c);
  };
  bool gold=index%2==0;
  uint16_t fin=gold?color(119,115,66):color(72,119,117);
  uint16_t body=gold?color(206*sc,174*sc,94*sc):color(162*sc,192*sc,180*sc);
  uint16_t back=gold?color(130,120,64):color(76,121,119);
  if(neon) { body=color(90,156,174); back=color(34,85,108); fin=color(54,113,130); }
  if(guppy) { body=color(216,181,112); back=color(106,104,73); fin=color(233,110,54); }
  if(guppy) {
    // A broad, flexible fan tail with rays and spots distinguishes the guppy.
    for(int ray=0;ray<8;++ray) {
      float y=-11+ray*2.75f, ny=y+2.75f;
      float tip=-29+std::sin(f.phase+ray*.5f)*1.6f;
      tri(-13,wag,tip,tail+y,tip,tail+ny,ray%2?color(240,146,57):color(207,73,55));
      auto a=point(-14,wag),b=point(tip,tail+y);
      frame.drawLine(a.first,a.second,b.first,b.second,color(150,67,53));
      if(ray%2==0) { auto spot=point(-24,tail+y*.7f); frame.fillCircle(spot.first,spot.second,1,color(51,75,81)); }
    }
  } else tri(-14,wag,-24,tail-5,-23,tail+5,fin);
  for(int ray=-4;!guppy && ray<=4;ray+=2) {
    auto a=point(-14,wag),b=point(-23,tail+ray);
    frame.drawLine(a.first,a.second,b.first,b.second,back);
  }
  tri(-3,-3,-7,guppy?-12:-7,-11,-2,fin);
  tri(3,2,-3,7+std::sin(f.phase)*1.2f,-7,2,fin);
  const float xs[]={-15,-11,-6,0,6,10,14};
  const float widths[]={.4f,1.2f,2.6f,3.8f,3.5f,2.5f,.5f};
  float bodyHeight=neon?1.45f:guppy?1.15f:1.f;
  for(int j=0;j<6;++j) {
    float x=xs[j],nx=xs[j+1];
    float bend=wag*std::max(0.f,-x)/15,nb=wag*std::max(0.f,-nx)/15;
    tri(x,bend-widths[j]*bodyHeight,nx,nb-widths[j+1]*bodyHeight,nx,nb+widths[j+1]*bodyHeight,body);
    tri(x,bend-widths[j]*bodyHeight,nx,nb+widths[j+1]*bodyHeight,x,bend+widths[j]*bodyHeight,body);
    auto a=point(x,bend-widths[j]*bodyHeight),b=point(nx,nb-widths[j+1]*bodyHeight);
    frame.drawLine(a.first,a.second,b.first,b.second,back);
  }
  if(neon) {
    tri(-11,1,2,1,1,5,color(235,49,67));
    tri(-11,1,1,5,-9,3,color(235,49,67));
    for(int line=-1;line<=0;++line) {
      auto a=point(-11,line),b=point(10,line);
      frame.drawLine(a.first,a.second,b.first,b.second,color(40,231,246));
    }
  }
  auto a=point(-9,0),b=point(9,1);
  if(!neon) frame.drawLine(a.first,a.second,b.first,b.second,gold?color(244,213,140):color(206,227,216));
  auto eye=point(9,-1); frame.fillCircle(eye.first,eye.second,2,color(16,32,30));
  frame.drawPixel(eye.first,eye.second-1,color(242,240,195));
  auto gill=point(5,2),gill2=point(4,-2); frame.drawLine(gill.first,gill.second,gill2.first,gill2.second,fin);
}

void renderScene() {
  frame.fillSprite(color(8,19,25));
  // Submerged depth gradient. Only the sky above the free surface is erased.
  for(int y=50;y<466;++y) {
    float d=(y-50)/416.f;
    frame.drawFastHLine(0,y,466,color(12+7*d,59-31*d,65-28*d));
  }
  for(int x=0;x<466;++x) {
    int h=world.surface(x);
    frame.drawFastVLine(x,0,h,color(8,19,25));
    frame.drawFastVLine(x,h,4,color(59,119,116));
    frame.drawPixel(x,h,color(155,204,182));
    if(x%3==0) frame.drawPixel(x,h+7,color(30,87,84));
  }
  // Low-contrast broad light shafts and broken caustics near the gravel.
  for(int i=0;i<4;++i) {
    int x=65+i*103+std::sin(world.time*.18f+i)*14;
    frame.fillTriangle(x,world.surface(x)+12,x-33,370,x-8,370,color(17,51,54));
  }
  for(int i=0;i<18;++i) {
    int x=40+(i*71)%380,y=357+(i*43)%58;
    x+=std::sin(world.time*.65f+i)*9+world.current*.04f;
    for(int j=0;j<19;++j) {
      int yy=y+std::sin(j*.17f+world.time*.5f+i)*3;
      frame.drawPixel(x+j,yy,color(37,66,57));
    }
  }
  for(int i=0;i<8;++i) plant(38+i*55,65+(i*37)%125,i,false);
  for(int i=0;i<30;++i) {
    float x=25+(i*97)%416+std::sin(world.time*.22f+i)*8;
    float y=145+std::fmod(i*31+world.time*(1+i%3),265.f);
    if(y>world.surface(x)+9) frame.drawPixel(x,y,color(65,109,105));
  }
  for(const auto &b:world.bubbles) {
    if(b.pop>0) {
      float r=(.45f-b.pop)*19;
      frame.drawEllipse(b.x,world.surface(b.x)+2,2+r,1+r*.22f,color(64,116,116));
    } else {
      frame.drawEllipse(b.x,b.y,b.radius,b.radius+1,color(46,103,110));
      frame.drawPixel(b.x-1,b.y-1,color(149,199,190));
    }
  }
  // Traveling highlights trace the actual free surface.
  for(int i=0;i<5;++i) {
    int x=45+int(std::fmod(world.time*9+i*83,375.f));
    for(int j=0;j<14;++j) frame.drawPixel(x+j,world.surface(x+j)+1,color(175,210,190));
  }
  for(const auto &p:world.food) if(p.life>0) {
    frame.fillCircle(p.x,p.y,2,color(206,157,76));
    frame.drawPixel(p.x,p.y-1,color(245,217,137));
  }
  for(int i=0;i<World::COUNT;++i) drawFish(world.fish[i],i);
  // Gravel sits inside the circular viewport; no rectangular tank frame.
  for(int y=418;y<466;++y) frame.drawFastHLine(0,y,466,color(31-(y-418)/5,42-(y-418)/5,35-(y-418)/5));
  for(int i=0;i<115;++i) {
    int x=(i*137+29)%466,y=421+(i*41)%45;
    int shade=(i*17)%29;
    frame.fillEllipse(x,y,2+i%4,1+i%2,color(40+shade,48+shade,38+shade));
  }
  plant(73,93,11,true); plant(390,119,14,true); plant(410,77,19,true);
  // Small moving glass glints; no permanent labels or menus in viewing mode.
  for(int i=0;i<24;++i) {
    float a=(211+i*.9f)*kPi/180;
    frame.fillCircle(233+std::cos(a)*222,233+std::sin(a)*222,1,color(83,117,113));
  }
  if(!imu) {
    frame.setTextColor(color(143,164,156));
    frame.setTextDatum(middle_center);
    frame.setFont(&fonts::Font2);
    frame.drawString("IMU unavailable",233,390);
  }
  if(world.lightNotice>0) {
    frame.drawEllipse(233,58,12,12,color(143,187,177));
    if(world.lightFrozen) {
      frame.drawFastVLine(230,53,10,color(189,224,203));
      frame.drawFastVLine(236,53,10,color(189,224,203));
    } else frame.fillTriangle(230,52,230,64,239,58,color(189,224,203));
  }
  frame.pushSprite(0,0);
  ++frames;
}
