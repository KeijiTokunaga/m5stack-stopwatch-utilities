#include "Pomodoro.h"
#include <cassert>
#include <cstdio>
int main() {
  Pomodoro t;
  t.toggle(100);
  assert(!t.tick(1100) && t.remaining == 1499000);
  t.toggle(1100);
  assert(!t.tick(999999) && t.remaining == 1499000);
  t.toggle(1000000);
  assert(t.tick(2499000) && t.phase == Pomodoro::ShortBreak && t.completed == 1 && !t.running);
  for (int i = 1; i < 4; ++i) {
    t.toggle(0); assert(t.tick(t.duration()));
    assert(t.phase == Pomodoro::Focus);
    t.toggle(0); assert(t.tick(t.duration()));
  }
  assert(t.phase == Pomodoro::LongBreak && t.duration() == 900000 && t.completed == 4);
  t.skip(); assert(t.phase == Pomodoro::Focus);
  t.skip(); assert(t.completed == 4 && t.phase == Pomodoro::ShortBreak);
  t.reset(); assert(!t.running && t.remaining == 300000);
  Pomodoro wrap;
  wrap.toggle(0xFFFFFF00); wrap.tick(744);
  assert(wrap.remaining == 1499000);
  assert(wrap.restore(2, 12345, 8) && !wrap.running && wrap.remaining == 12345);
  assert(!wrap.restore(3, 10, 0));
  assert(!wrap.restore(0, 0, 0));
  puts("PASS: countdown, pause/resume, four-session cycle, skip, reset, rollover, restore");
}
