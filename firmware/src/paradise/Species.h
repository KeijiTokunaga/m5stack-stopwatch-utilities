#pragma once
#include "Model.h"
namespace paradise {
inline const char* fieldName(int i){static const char* a[]={"りく","みず","そら"};return a[i%3];}
inline const char* stageName(int i){static const char* a[]={"たまご","ベビー","キッズ","ヤング","アダルト"};return a[i%5];}
inline const char* speciesName(int id){
static const char* names[]={
"みゃおっち","ポチっち","ぐまっくす","らっち","まめっち","みみっち","もるもっち","しいぷっち",
"れおぱっち","せびれっち","えりざーどっち","へびーっち","ふらわっち","ぼつねんっち","たすたすっち","しげみさん",
"いるかっち","カメっち","くじらっち","うるおっち","あほろぱっち","いもりっち","かわずっち","びーばーっち",
"たちゅっち","しゃーくっち","アンコっち","オトトっち","くららっち","めんだこっち","あめふらっち","ぐそくっち",
"ほーほっち","もんがっち","いーぐるっち","ばっち","ぴーこっち","ばたっち","くちぱっち","きうぃっち",
"ぱぴよっち","カブトっち","てんとっち","はっちっち","じぇむっち","おれたっち","いしころっち","まぐまっち",
"でかべびまるっち"};return names[id>=0&&id<49?id:0];
}
inline const char* familyName(int field,int family){static const char* a[3][4]={
{"がおがお","とことこ","ぺろぺろ","にょきにょき"},{"すいすい","ぴょこぴょこ","ぴちぴち","ふよふよ"},{"ぱたぱた","ぴよぴよ","ぶんぶん","かちかち"}};return a[field%3][family%4];}
}
