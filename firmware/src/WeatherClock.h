#pragma once
#include <ctime>
namespace weather {
struct Clock {char hour[3]="--",minute[3]="--",date[6]="--/--";bool valid=false;};
inline Clock clockJst(time_t utc){
  Clock out;if(utc<1700000000)return out;
  time_t jst=utc+9*3600;tm t{};gmtime_r(&jst,&t);
  strftime(out.hour,sizeof(out.hour),"%H",&t);
  strftime(out.minute,sizeof(out.minute),"%M",&t);
  strftime(out.date,sizeof(out.date),"%m/%d",&t);out.valid=true;return out;
}
}
