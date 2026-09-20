#pragma once
#include "Model.h"
#include "Species.h"
#include "Storage.h"
#include "MiniGame.h"
#include <sys/time.h>
namespace paradise {
State state;
Preferences storage;
uint32_t sequence=0,lastUpdate=0,lastSave=0,noticeUntil=0;
bool storageReady=false,saveError=false,corruptSave=false,synced=false;
String notice;
int depth=2,selected=0,editPart=0,nameLetter=0;
char editedName[17]{};
int dateParts[5]={2026,1,1,12,0};
MiniGame game;
unsigned earned=0;
enum Page {Home,Lab,Food,Shop,Games,CareGames,Info,Codex,Settings,Cook,Name,Birthday,Clock,Breed,Confirm,Playing,Result};
Page page=Home;
int confirmAction=0;
const char* tr(const char* ja,const char* en){return state.english?en:ja;}
void tell(const String& text){notice=text;noticeUntil=millis()+2800;}
void save(){
  if(!storageReady||!state.initialized)return;
  Snapshot out{};out.state=state;out.sequence=sequence+1;out.hash=checksum(out);
  if(!valid(out)){saveError=true;return;}
  const char* key=out.sequence%2?"slot1":"slot0";
  if(storage.putBytes(key,&out,sizeof(out))==sizeof(out)){sequence=out.sequence;saveError=false;lastSave=millis();}else saveError=true;
}
void begin(){
  storageReady=storage.begin("paradise",false);saveError=!storageReady;
  Snapshot a{},b{};a.version=b.version=0;
  size_t la=storageReady?storage.getBytesLength("slot0"):0,lb=storageReady?storage.getBytesLength("slot1"):0;
  if(la==sizeof(a))storage.getBytes("slot0",&a,sizeof(a));if(lb==sizeof(b))storage.getBytes("slot1",&b,sizeof(b));
  if(auto* pick=newest(a,b)){state=pick->state;sequence=pick->sequence;}else corruptSave=la||lb;
  synced=state.clockKnown;lastUpdate=millis();lastSave=lastUpdate;
}
int64_t now(){return state.lastEpoch?state.lastEpoch:time(nullptr)>=1700000000?time(nullptr):20000LL*86400+3*3600;}
void enter(){page=Home;selected=0;}
void open(Page p){page=p;selected=0;}
void start(){
  auto opts=state;initialize(state,esp_random(),time(nullptr)>=1700000000?time(nullptr):20000LL*86400+3*3600);
  synced=time(nullptr)>=1700000000;state.clockKnown=synced;state.birthdayMonth=opts.birthdayMonth;state.birthdayDay=opts.birthdayDay;state.english=opts.english;
  memcpy(state.planet,opts.planet,sizeof(state.planet));corruptSave=false;save();tell(tr("エッグバン！星ができた","Egg bang! A new planet"));
}
int menuCount(){
  if(page==Home){if(!state.initialized)return 4;if(state.dead)return 2;return depth==0?4:depth==1?6:depth==2?7:4;}
  switch(page){case Lab:return 8;case Food:return 8;case Shop:return 10;case Cook:return 4;case Games:return 7;case CareGames:return 4;case Settings:return 6;case Breed:return 5;case Codex:return 49;case Birthday:return 3;case Confirm:return 2;default:return 1;}
}
String option(int i){
  if(page==Home){
    if(!state.initialized){const char* j[]={"はじめる","誕生日の設定","日本語 / English","日時を設定"};const char* e[]={"Start","Birthday","Japanese / English","Set date and time"};return state.english?e[i]:j[i];}
    if(state.dead)return i?tr("プロフィール","Profile"):tr("新しい卵を受け取る","Receive a new egg");
    if(depth==0){const char* j[]={"わくせいの情報","宇宙トラベル","デコを飾る","ツーしん（未対応）"};const char* e[]={"Planet info","Space travel","Place decoration","Link: unavailable"};return state.english?e[i]:j[i];}
    if(depth==1){const char* j[]={"うんちそうじ","フィールド切替","火山エッグハント","泉エッグハント","遊具を設置 200G","フィールドへ放す"};const char* e[]={"Clean poop","Change field","Volcano egg hunt","Spring egg hunt","Play equipment 200G","Release adult"};return state.english?e[i]:j[i];}
    if(depth==2){const char* j[]={"ごはん","おやつ 20G","いっしょにあそぶ","おていれ","消灯 / 点灯","死神を追い払う","成長を確認"};const char* e[]={"Food","Snack 20G","Play together","Wash","Lights off / on","Chase reaper","Confirm growth"};return state.english?e[i]:j[i];}
    const char* j[]={"ウイルスを治療","詳しいステータス","環境・栄養マーク","MAX・ミスマーク"};const char* e[]={"Treat one virus","Detailed status","Environment / food","MAX / care marks"};return state.english?e[i]:j[i];
  }
  if(page==Lab){const char* j[]={"インフォメーション","ミニゲーム","ショップ","たまシッター 300G","たまごそうだん","ずかん","ラボコード（未対応）","せってい"};const char* e[]={"Information","Mini games","Shop","Sitter 300G / pickup","Matchmaking","Collection","Lab code: unavailable","Settings"};return state.english?e[i]:j[i];}
  if(page==Shop&&i==8)return tr("食材を調理（Lv5）","Cook ingredients (Lv5)");
  if(page==Shop&&i==9)return tr("選択中デコを売る","Sell selected decoration");
  if(page==Food||page==Shop||page==Cook){const char* j[]={"肉 / 魚 / チキン","野菜 / 海藻 / 穀物","虫 / エビ / はちみつ","ペレット","肉系の料理","野菜系の料理","虫系の料理","バランス料理"};const char* e[]={"Meat / fish / chicken","Veg / kelp / grain","Bug / shrimp / honey","Pellets","Protein meal","Plant meal","Small food meal","Balanced meal"};return String(state.english?e[i]:j[i])+"  "+String(state.items[i]);}
  if(page==Games){const char* j[]={"いそいでミルク","ジャンプでキャッチ","おりょうりあわせ","うんちそうじにん","ほこりバスター","いんせきパンチ","難易度を変更"};const char* e[]={"Milk mixing","Jump and catch","Cooking match","Poop cleaner","Dust buster","Meteor punch","Toggle difficulty"};return state.english?e[i]:j[i];}
  if(page==CareGames){const char* j[]={"こっちむいてホイ","はたあげ","UFOシャッフル","おさんぽ"};const char* e[]={"Look this way","Flag raising","UFO shuffle","Walking"};return state.english?e[i]:j[i];}
  if(page==Settings){const char* j[]={"惑星名を変更","誕生日","日本語 / English","音 ON / OFF","明るさ","日時を設定"};const char* e[]={"Rename planet","Birthday","Japanese / English","Sound on / off","Brightness","Set date and time"};return state.english?e[i]:j[i];}
  if(page==Breed)return i==4?tr("候補を入れ替える","New candidates"):String(tr("候補 ","Partner "))+String(i+1)+"  "+String(state.candidates[i].eyes)+" / "+String(state.candidates[i].color);
  if(page==Codex)return state.discovered&(uint64_t(1)<<i)?speciesName(i):"???";
  if(page==Birthday)return i==0?String(tr("月 ","Month "))+state.birthdayMonth:i==1?String(tr("日 ","Day "))+state.birthdayDay:tr("保存して戻る","Save and return");
  if(page==Confirm)return i?tr("やめる","Cancel"):tr("実行する","Confirm");
  return tr("戻る","Back");
}
void startGame(int kind,bool lab){
  if((lab&&state.level<1)||(!lab&&!awake(state))){tell(tr("今はあそべません","Not available now"));return;}
  bool hard=game.hard&&state.level>=9;game.begin(state,kind,lab,hard,millis());page=Playing;
}
void finishGame(){
  earned=reward(state,game.stars(),game.lab,game.hard);
  if(game.kind==5&&game.score>=3){state.meteor=0;state.meteorTimer=0;}
  else if(game.kind==5&&state.meteor){state.viruses=3;state.meteor=0;}
  page=Result;selected=0;save();
}
void back(){if(page==Playing){game.done=true;page=Home;return;}if(page==Home){depth=std::max(0,depth-1);selected=0;}else open(Home);}
void execute(){
  bool ok=false;
  if(page==Home){
    if(!state.initialized){if(selected==0)start();else if(selected==1)open(Birthday);else if(selected==2)state.english=!state.english;else {time_t stamp=now()+32400;tm date{};gmtime_r(&stamp,&date);dateParts[0]=date.tm_year+1900;dateParts[1]=date.tm_mon+1;dateParts[2]=date.tm_mday;dateParts[3]=date.tm_hour;dateParts[4]=date.tm_min;editPart=0;open(Clock);}return;}
    if(state.dead){if(selected==0){confirmAction=0;open(Confirm);}else open(Info);return;}
    if(depth==0){if(selected==0){open(Info);return;}if(selected==1)ok=travel(state);if(selected==2)ok=decorate(state);if(selected==3){tell(tr("専用機との通信は未対応","Original device link unavailable"));return;}}
    if(depth==1){if(selected==0)ok=clean(state);if(selected==1)ok=changeField(state);if(selected==2||selected==3){int n=hunt(state,now(),selected==3);save();tell(n?String(tr("食材ゲット ","Ingredients +"))+n:tr("昼間・1日1回です","Daytime, once per day"));return;}if(selected==4)ok=equipment(state);if(selected==5){confirmAction=1;open(Confirm);return;}}
    if(depth==2){if(selected==0){open(Food);return;}if(selected==1)ok=snack(state);if(selected==2){open(CareGames);return;}if(selected==3)ok=wash(state,now());if(selected==4)ok=lights(state,now());if(selected==5)ok=rescue(state);if(selected==6)ok=confirmGrowth(state);}
    if(depth==3){if(selected==0)ok=cure(state);else{open(Info);return;}}
  }else if(page==Lab){switch(selected){case 0:open(Info);return;case 1:open(Games);return;case 2:open(Shop);return;case 3:ok=sitter(state,now());break;case 4:open(Breed);return;case 5:open(Codex);return;case 6:tell(tr("コードの規則が未提供です","No code rules supplied"));return;case 7:open(Settings);return;}}
  else if(page==Food)ok=feed(state,selected%4,selected>=4);
  else if(page==Shop){if(selected==8){open(Cook);return;}ok=selected==9?sell(state):buy(state,selected);}
  else if(page==Cook)ok=cook(state,selected);
  else if(page==Games){if(selected==6){if(state.level>=9)game.hard=!game.hard;tell(state.level>=9?(game.hard?"HARD":"NORMAL"):tr("Lv9で解放","Unlock at Lv9"));return;}startGame(selected,true);return;}
  else if(page==CareGames){startGame(6+selected,false);return;}
  else if(page==Breed){if(selected==4){reroll(state);ok=true;}else{int result=breed(state,selected,now());save();tell(result==1?tr("新しい命が誕生！","A new egg!"):result==0?tr("また会いにきてね","Try this partner again"):tr("大人・1日失敗3回まで","Adult, max 3 failures/day"));if(result==1)open(Home);return;}}
  else if(page==Settings){
    if(selected==0){memset(editedName,0,sizeof(editedName));nameLetter=0;open(Name);return;}
    if(selected==1){open(Birthday);return;}
    if(selected==2){state.english=!state.english;ok=true;}
    if(selected==3){state.sound=!state.sound;ok=true;}
    if(selected==4){state.brightness=state.brightness>=140?40:state.brightness+20;ok=true;}
    if(selected==5){time_t t=now()+32400;tm date{};gmtime_r(&t,&date);dateParts[0]=date.tm_year+1900;dateParts[1]=date.tm_mon+1;dateParts[2]=date.tm_mday;dateParts[3]=date.tm_hour;dateParts[4]=date.tm_min;editPart=0;open(Clock);return;}
  }else if(page==Birthday){if(selected==0){state.birthdayMonth=state.birthdayMonth%12+1;state.birthdayDay=std::min<int>(state.birthdayDay,monthDays(state.birthdayMonth));}else if(selected==1)state.birthdayDay=state.birthdayDay%monthDays(state.birthdayMonth)+1;else {save();open(state.initialized?Settings:Home);}return;}
  else if(page==Confirm){if(selected==0)ok=confirmAction?release(state):newEgg(state);open(Home);}
  else {open(Home);return;}
  save();tell(ok?tr("できました！","Done!"):tr("条件・ポイントを確認","Check status / level / coins"));
}
void input(bool blue){
  if(page==Playing){game.input(state,blue,millis());if(game.done)finishGame();return;}
  if(page==Name){const char* alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_";if(!blue)nameLetter=(nameLetter+1)%38;else if(strlen(editedName)<16){int n=strlen(editedName);editedName[n]=alphabet[nameLetter];editedName[n+1]=0;}return;}
  if(page==Clock){
    if(!blue){int minimum[]={2024,1,1,0,0},maximum[]={2099,12,31,23,59};if(++dateParts[editPart]>maximum[editPart])dateParts[editPart]=minimum[editPart];}
    else if(++editPart==5){tm t{};t.tm_year=dateParts[0]-1900;t.tm_mon=dateParts[1]-1;t.tm_mday=dateParts[2];t.tm_hour=dateParts[3];t.tm_min=dateParts[4];time_t utc=mktime(&t)-32400;
      if(t.tm_mon!=dateParts[1]-1||t.tm_mday!=dateParts[2]){editPart=2;tell(tr("日付を確認してください","Invalid date"));return;}
      timeval value{utc,0};settimeofday(&value,nullptr);state.lastEpoch=utc;synced=true;state.clockKnown=true;save();open(state.initialized?Settings:Home);
    }return;
  }
  if(blue)execute();else selected=(selected+1)%menuCount();
}
void touch(int x,int y){
  if(page==Playing){input(x>=233);return;}
  if(page==Name){if(y>330&&x<233){int n=strlen(editedName);if(n)editedName[n-1]=0;}else if(y>330){if(strlen(editedName)){memcpy(state.planet,editedName,17);save();open(Settings);}}else input(x>=233);return;}
  if(y<90&&state.initialized){open(Lab);return;}
  if(page==Home&&y>=390){depth=std::min(3,std::max(0,(x-83)/75));selected=0;return;}
  if(y>=300&&y<385)execute();else if(x<100)back();else input(false);
}
void update(uint32_t ms){
  uint8_t wasAlert=state.alert,wasMeteor=state.meteor;
  if(ms-lastUpdate>=1000){uint32_t elapsed=(ms-lastUpdate)/1000;lastUpdate+=elapsed*1000;
    if(state.initialized){int64_t epoch=time(nullptr);bool real=epoch>=1700000000;
      if(real&&!synced){state.lastEpoch=epoch;synced=true;state.clockKnown=true;}
      else {if(real&&epoch<state.lastEpoch)state.lastEpoch=epoch;advance(state,real?epoch:state.lastEpoch+elapsed);}
      if(ms-lastSave>=60000)save();
    }
  }
  if(state.sound&&!state.sleeping&&!state.sitter&&!::pomodoro::alarmActive&&((state.alert&~wasAlert)||(state.meteor&&!wasMeteor)))M5.Speaker.tone(660,150);
  if(page==Playing){game.tick(state,ms);if(game.done)finishGame();}
}
void text(const String& value,int x,int y,uint16_t color=0xFFFF,int size=1){if(state.english)::frame.setFont(&fonts::Font2);else ::frame.setFont(&fonts::efontJA_16);::frame.setTextSize(size);::frame.setTextDatum(middle_center);::frame.setTextColor(color);::frame.drawString(value,x,y);::frame.setTextSize(1);}
uint16_t ink=0xFFFF;
void character(int x,int y,int scale=1){
  static const uint32_t colors[]={0xFFD282,0xF6A5BB,0x94DDD5,0xB9A6EF,0xBADD8B,0xA3C6F0,0xF3B177,0xFAEAB7,0xE2ABCB,0x8CC7B7,0xCEB8F3,0xA4DBF6,0xEFC2A0,0xA8BA91,0xD2D9E7,0xEDD2E0};
  uint32_t c=colors[state.genes.color%16];uint16_t body=rgb(c>>16,(c>>8)&255,c&255),dark=rgb(35,43,51);
  int bounce=state.sleeping||state.dead?0:int(sinf(millis()/350.f)*3);y+=bounce;
  if(state.stage==Egg){::frame.fillEllipse(x,y,35,44,body);::frame.drawLine(x-27,y,x-10,y-10,dark);::frame.drawLine(x-10,y-10,x+5,y+9,dark);::frame.drawLine(x+5,y+9,x+29,y-7,dark);return;}
  int radius=state.giant?65:state.stage==Baby?31:state.stage==Kids?39:48;
  if(state.genus==2){::frame.fillEllipse(x-radius,y+5,22,12,body);::frame.fillEllipse(x+radius,y+5,22,12,body);}
  if(state.family==0){::frame.fillTriangle(x-37,y-24,x-30,y-65,x-5,y-33,body);::frame.fillTriangle(x+37,y-24,x+30,y-65,x+5,y-33,body);}
  else if(state.family==1){::frame.fillEllipse(x-21,y-44,12,28,body);::frame.fillEllipse(x+21,y-44,12,28,body);}
  else if(state.family==3){::frame.fillCircle(x-25,y-37,17,body);::frame.fillCircle(x,y-47,17,body);::frame.fillCircle(x+25,y-37,17,body);}
  ::frame.fillEllipse(x,y,radius,radius-2,body);::frame.fillEllipse(x-25,y+radius-2,16,8,body);::frame.fillEllipse(x+25,y+radius-2,16,8,body);
  int gap=12+state.genes.eyes%3*4,shape=state.genes.eyes/3;
  for(int sign:{-1,1}){int ex=x+sign*gap;
    if(state.dead){::frame.drawWideLine(ex-5,y-7,ex+5,y+3,3,dark);::frame.drawWideLine(ex+5,y-7,ex-5,y+3,3,dark);}
    else if(state.sleeping)::frame.drawWideLine(ex-5,y,ex+5,y,3,dark);
    else{::frame.fillEllipse(ex,y-4,3+shape%4,5+shape/4,dark);::frame.fillCircle(ex-1,y-6,2,0xFFFF);if(shape%2)::frame.drawWideLine(ex-5,y-15,ex+4,y-12,2,dark);}
  }
  ::frame.drawArc(x,y+7,10,8,20,160,dark);::frame.fillCircle(x-29,y+10,5,rgb(245,150,164));::frame.fillCircle(x+29,y+10,5,rgb(245,150,164));
  if(state.sleeping)text("Z z",x+67,y-40,rgb(195,206,238));
  if(state.viruses)text(String("+ ")+state.viruses,x+75,y+5,rgb(214,153,242));
  if(state.reaperAge)text("!",x+75,y-45,rgb(249,102,104),2);
}
void paintGame(){
  ::frame.fillSprite(rgb(16,23,44));text(game.lab?tr("ラボゲーム","LAB GAME"):tr("いっしょにあそぶ","PLAY TOGETHER"),233,70,0xFFFF,2);
  text(String(game.round+1)+" / 5   SCORE "+game.score,233,116,rgb(173,207,227));
  if(game.kind==0){text(tr("黄色・青を交互に！","Alternate YELLOW / BLUE"),233,177);::frame.drawRoundRect(175,204,116,92,14,0xFFFF);::frame.fillRect(183,286-std::min(game.steps,30)*2,100,std::min(game.steps,30)*2,rgb(245,231,189));text(String(game.steps),233,324);}
  else if(game.timed()){
    text(game.kind==5?tr("中央で隕石をパンチ","Punch meteor at center"):game.kind==3?tr("中央でごみを回収","Collect at the center"):game.kind==9?tr("中央でジャンプ！","Jump at the center"):tr("中央でキャッチ！","Catch at the center"),233,175);
    ::frame.fillRoundRect(103,232,260,16,8,rgb(58,68,91));::frame.fillRect(213,219,40,42,rgb(91,153,128));::frame.fillCircle(103+game.position(millis())*260/100,240,12,rgb(245,201,104));
    text(tr("青でアクション","BLUE: action"),233,320);
  }else if(game.kind==7){text(game.target%2?tr("青い旗！","BLUE FLAG!"):tr("黄色い旗！","YELLOW FLAG!"),233,227,game.target%2?rgb(107,178,246):rgb(247,214,99),2);text(tr("同じ色のボタンを押す","Press the matching button"),233,306);}
  else{
    bool reveal=game.kind!=8||millis()-game.roundAt<1200;
    text(reveal?String(tr("目標 ","TARGET "))+String(game.target+1):tr("どこにいた？","Where was it?"),233,178,rgb(245,205,125),2);
    for(int i=0;i<3;++i){int x=137+i*96;::frame.fillRoundRect(x-35,226,70,63,13,i==game.cursor?rgb(71,130,145):rgb(37,48,75));text(String(i+1),x,256,0xFFFF,2);}
    text(tr("黄で選択・青で決定","YELLOW: select / BLUE: choose"),233,333);
  }
  text(tr("青長押しで戻る","Hold BLUE: cancel"),233,396,rgb(142,151,174));::frame.pushSprite(0,0);
}
void paint(){
  if(page==Playing){paintGame();return;}
  uint16_t accent=rgb(153,220,181);::frame.fillSprite(rgb(12,26,34));
  if(page==Name){text(tr("惑星の名前","PLANET NAME"),233,90,accent,2);text(editedName,233,178);const char* chars="ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_";text(String(chars[nameLetter]),233,246,0xFFFF,3);text(tr("黄 次の文字 / 青 追加","YELLOW next / BLUE add"),233,295);text(tr("削除","Delete"),145,350);text(tr("保存","Save"),321,350);::frame.pushSprite(0,0);return;}
  if(page==Clock){text(tr("日本時間の設定","SET JST CLOCK"),233,90,accent,2);for(int i=0;i<5;++i)text(String(dateParts[i]),233,150+i*40,i==editPart?accent:0xFFFF);text(tr("黄 +1 / 青 次へ","YELLOW +1 / BLUE next"),233,389);::frame.pushSprite(0,0);return;}
  text(page==Home?String(state.planet):page==Lab?tr("ラボ","LAB"):page==Info?tr("インフォメーション","INFORMATION"):page==Result?tr("ゲーム結果","RESULT"):tr("PARADISE","PARADISE"),233,56,accent,2);
  text(String("Lv ")+state.level+"    "+state.coins+" G    "+tr("ラボ:上をタップ","Tap top for lab"),233,94,rgb(160,182,193));
  if(page==Home){
    text(state.dead?tr("星になりました","Rest in peace"):String(stageName(state.stage))+" / "+fieldName(state.field),233,127,rgb(229,211,167));
    if(depth==0){::frame.fillCircle(233,216,63,rgb(53,101,119));::frame.fillEllipse(214,203,31,17,rgb(94,155,114));::frame.fillEllipse(257,231,26,21,rgb(94,155,114));for(int i=0;i<9;++i)::frame.fillCircle(107+i*31,163+(i%3)*22,2,0xFFFF);}
    else if(depth==3){::frame.drawCircle(233,211,69,rgb(95,174,184));::frame.drawCircle(233,211,44,rgb(120,188,167));for(int i=0;i<state.viruses;++i)::frame.fillCircle(187+i*46,192+(i%2)*35,9,rgb(183,126,222));text(String(tr("ミス ","Miss "))+state.misses+" / "+tr("病気 ","Virus ")+state.viruses,233,272);}
    else{::frame.fillEllipse(233,265,102,14,rgb(29,57,64));character(233,212);for(int i=0;i<state.poop;++i)::frame.fillCircle(120+i*16,259,6,rgb(167,112,74));if(state.equipment[state.field]){::frame.drawLine(340,211,340,265,accent);::frame.drawLine(325,213,355,213,accent);}}
    if(depth!=3)text(String(tr("おなか ","FOOD "))+state.food+"/4    "+tr("ごきげん ","JOY ")+state.joy+"/4",233,289);
    if(state.stage==Adult&&!state.dead)text(speciesName(species(state)),233,155);
    if(state.sitter||state.latePickup)text(tr("シッター / お迎え","SITTER / PICK UP"),233,178,accent);
    if(state.alert||state.meteor)text(state.meteor?tr("隕石！ラボでパンチ","METEOR! Lab: punch"):tr("よびだし！お世話してね","CARE CALL!"),233,306,rgb(249,173,119));
  }else if(page==Info){
    text(String(tr("世代 ","Generation "))+state.generation+" / "+state.age/1440+tr("日"," days"),233,150);
    text(String(tr("環境マーク ","Environment "))+state.fieldMarks+" / 4",233,182);
    text(String(tr("栄養 ","Nutrition "))+state.foodMarks[0]+" / "+state.foodMarks[1]+" / "+state.foodMarks[2]+" / "+state.foodMarks[3],233,214);
    text(String(tr("MAX 食 / 機嫌 ","MAX food / joy "))+state.fullFood+" / "+state.fullJoy,233,246);
    text(String(tr("ミス ","Misses "))+state.misses+"   "+tr("燃料 ","Fuel ")+state.fuel+"%",233,278);
  }else if(page==Result){text(String(game.stars())+" / 3 STARS",233,175,accent,2);text(game.lab?String("+")+earned+" G":tr("いっしょに遊んだ！","We played together!"),233,244);}
  else if(page==Confirm){text(confirmAction?tr("今の子をフィールドへ","Release current adult?"):tr("新しい世代をはじめる？","Start a new generation?"),233,204);text(tr("お世話できなくなります","Current pet will be replaced"),233,248,rgb(241,172,137));}
  else if(page==Codex){if(state.discovered&(uint64_t(1)<<selected))text(speciesName(selected),233,197,accent,2);else text("?",233,204,accent,4);text(tr("育てたアダルトを記録","Raised adults are recorded"),233,267);}
  else{character(233,212);text(page==Games?(game.hard?"HARD / Lv9":"NORMAL"):page==Shop?tr("食材50G / 料理100G","Ingredient 50G / meal 100G"):page==Breed?tr("候補ごとに相性が育つ","Retry a partner for better odds"):"",233,286);}
  ::frame.fillRoundRect(80,322,306,50,15,rgb(44,83,86));text(option(selected),233,348,0xFFFF);
  text(String(selected+1)+" / "+menuCount()+"    "+tr("黄:次 青:決定","YELLOW next / BLUE choose"),233,388,rgb(151,174,185));
  if(page==Home){const char* z[]={"SPACE","FIELD","PET","CELL"};for(int i=0;i<4;++i){::frame.fillRoundRect(85+i*75,409,71,22,7,i==depth?rgb(62,126,117):rgb(27,44,53));text(z[i],120+i*75,420);}}
  if(saveError||corruptSave){::frame.fillRect(83,270,300,38,rgb(89,41,43));text(tr("保存エラー / 記録を確認","SAVE ERROR / check storage"),233,289);}
  if(int32_t(noticeUntil-millis())>0){::frame.fillRoundRect(71,268,324,44,10,rgb(61,74,84));text(notice,233,290);}
  ::frame.pushSprite(0,0);
}
}
