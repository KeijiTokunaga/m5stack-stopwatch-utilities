#include "WeatherClock.h"
#include <cassert>
#include <cstring>
#include <cstdio>
int main(){
  assert(!weather::clockJst(0).valid);
  // 2026-09-20 14:59 UTC -> 23:59 JST, then next day.
  tm t{};t.tm_year=126;t.tm_mon=8;t.tm_mday=20;t.tm_hour=14;t.tm_min=59;
  time_t utc=timegm(&t);auto a=weather::clockJst(utc),b=weather::clockJst(utc+60);
  assert(!strcmp(a.hour,"23")&&!strcmp(a.minute,"59")&&!strcmp(a.date,"09/20"));
  assert(!strcmp(b.hour,"00")&&!strcmp(b.minute,"00")&&!strcmp(b.date,"09/21"));
  t.tm_mon=11;t.tm_mday=31;utc=timegm(&t);b=weather::clockJst(utc+60);
  assert(!strcmp(b.date,"01/01"));
  puts("PASS: JST clock, unknown time, minute/day/year rollover");
}
