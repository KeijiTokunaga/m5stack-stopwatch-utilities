#pragma once
#include <stdint.h>

// Clock-independent state machine. All durations are milliseconds.
class Pomodoro {
 public:
  enum Phase : uint8_t { Focus, ShortBreak, LongBreak };
  Phase phase = Focus;
  uint32_t completed = 0;
  uint32_t remaining = 25UL * 60 * 1000;
  bool running = false;

  uint32_t duration() const {
    return (phase == Focus ? 25UL : phase == ShortBreak ? 5UL : 15UL) * 60000;
  }
  void toggle(uint32_t now) { running = !running; last = now; }
  void reset() { running = false; remaining = duration(); }
  void skip() { advance(false); }
  bool tick(uint32_t now) {
    if (!running) return false;
    const uint32_t elapsed = now - last;  // Works across millis() rollover.
    last = now;
    if (elapsed < remaining) { remaining -= elapsed; return false; }
    advance(true);
    return true;
  }
  bool restore(uint8_t savedPhase, uint32_t savedRemaining, uint32_t count) {
    if (savedPhase > LongBreak) return false;
    phase = static_cast<Phase>(savedPhase);
    if (!savedRemaining || savedRemaining > duration()) {
      phase = Focus;
      remaining = duration();
      return false;
    }
    remaining = savedRemaining;
    completed = count;
    running = false; // Never silently count time while powered off.
    return true;
  }
 private:
  uint32_t last = 0;
  void advance(bool earned) {
    if (phase == Focus) {
      if (earned) ++completed;
      phase = earned && completed % 4 == 0 ? LongBreak : ShortBreak;
    } else phase = Focus;
    reset(); // Each next session starts deliberately.
  }
};
