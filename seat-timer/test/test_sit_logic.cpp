// Host-side tests for the seat-time state machine.
// Run: g++ -std=c++17 test/test_sit_logic.cpp -o /tmp/t && /tmp/t
#include <cstdio>
#include <cstdlib>
#include "../src/sit_logic.h"

using namespace sit;

static int failures = 0;
#define CHECK(cond)                                                   \
  do {                                                                \
    if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); failures++; } \
  } while (0)

constexpr uint32_t STEP = 200;  // firmware samples every 200 ms
constexpr uint32_t SEC = 1000, MIN = 60 * SEC;

struct Sim {
  Tracker  t;
  uint32_t now = 0;
  Output   last;
  int      beeps = 0;
  int      breaksDone = 0;
  int      satDowns = 0, stoodUps = 0;
  explicit Sim(uint32_t start = 0) : now(start) {}
  // Advance `ms` with constant presence, sampling every STEP.
  void run(uint32_t ms, bool present) {
    for (uint32_t e = 0; e < ms; e += STEP) {
      now += STEP;
      last = t.update(now, present);
      if (last.beep) beeps++;
      if (last.breakCompleted) breaksDone++;
      if (last.satDown) satDowns++;
      if (last.stoodUp) stoodUps++;
    }
  }
};

static void passerby_does_not_start_session() {
  Sim s;
  s.run(4 * SEC, true);
  CHECK(s.last.state == State::Empty);
  s.run(10 * SEC, false);
  CHECK(s.last.state == State::Empty);
  CHECK(s.breaksDone == 0);
}

static void sitting_starts_after_confirm() {
  Sim s;
  s.run(6 * SEC, true);
  CHECK(s.last.state == State::Sitting);
  // session start is backdated to first detection
  CHECK(s.last.sittingMs >= 5 * SEC);
}

static void warning_then_overdue_with_repeat_reminders() {
  Sim s;
  s.run(27 * MIN, true);
  CHECK(s.last.state == State::Sitting);
  s.run(2 * MIN, true);  // 29 min
  CHECK(s.last.state == State::Warning);
  CHECK(s.beeps == 0);
  s.run(1 * MIN + STEP, true);  // just past 30 min
  CHECK(s.last.state == State::Overdue);
  CHECK(s.beeps == 1);
  s.run(89 * SEC, true);
  CHECK(s.beeps == 1);
  s.run(2 * SEC, true);
  CHECK(s.beeps == 2);
  s.run(10 * MIN, true);
  CHECK(s.beeps >= 8);
}

static void short_dropout_is_ignored() {
  Sim s;
  s.run(10 * MIN, true);
  s.run(1 * SEC, false);
  CHECK(s.last.state == State::Sitting);
  s.run(1 * SEC, true);
  CHECK(s.last.state == State::Sitting);
  CHECK(s.breaksDone == 0);
}

static void short_standup_does_not_reset() {
  Sim s;
  s.run(31 * MIN, true);
  CHECK(s.last.state == State::Overdue);
  int before = s.beeps;
  s.run(59 * SEC, false);  // just under the 1-minute break
  CHECK(s.last.state == State::Away);
  CHECK(s.beeps == before);  // no beeping while away
  CHECK(s.breaksDone == 0);
  s.run(1 * SEC, true);  // came back too early
  CHECK(s.last.state == State::Overdue);
  CHECK(s.beeps == before + 1);  // reminds immediately (last remind >90s ago)
  CHECK(s.last.state == State::Overdue);
}

static void real_break_resets_session() {
  Sim s;
  s.run(31 * MIN, true);
  s.run(60 * SEC, false);  // 1 minute away completes the break
  CHECK(s.last.state == State::Empty);
  CHECK(s.breaksDone == 1);
  CHECK(s.last.breaks == 1);
  s.run(10 * SEC, true);
  CHECK(s.last.state == State::Sitting);
  CHECK(s.last.sittingMs < 15 * SEC);
}

static void new_day_gap_resets_break_count() {
  Sim s;
  s.run(10 * MIN, true);
  s.run(3 * MIN, false);
  CHECK(s.last.breaks == 1);
  s.run(10 * MIN, true);
  s.run(3 * MIN, false);
  CHECK(s.last.breaks == 2);
  s.run(5 * 60 * MIN, false);  // 5 hours away
  s.run(10 * SEC, true);
  CHECK(s.last.state == State::Sitting);
  CHECK(s.last.breaks == 0);
}

static void survives_millis_wraparound() {
  Sim s(0xFFFFFFFFu - 10 * MIN);  // wraps during the session
  s.run(31 * MIN, true);
  CHECK(s.last.state == State::Overdue);
  CHECK(s.beeps == 1);
  s.run(2 * MIN, false);
  CHECK(s.last.state == State::Empty);
}

// One cue when a session starts, one when you get up, one when you sit back
// down before the break completes; nothing for a passer-by or sensor jitter.
static void sit_and_stand_cues() {
  Sim s;
  s.run(4 * SEC, true);              // passer-by: no session
  s.run(10 * SEC, false);
  CHECK(s.satDowns == 0 && s.stoodUps == 0);
  s.run(6 * SEC, true);              // sits down
  CHECK(s.satDowns == 1 && s.stoodUps == 0);
  s.run(2 * SEC, false);             // 2 s dropout = jitter
  s.run(5 * SEC, true);
  CHECK(s.satDowns == 1 && s.stoodUps == 0);
  s.run(10 * SEC, false);            // gets up
  CHECK(s.stoodUps == 1);
  s.run(1 * SEC, true);              // back within the break: counts at once
  CHECK(s.satDowns == 2);
  s.run(3 * MIN, false);             // real break; session ends while away
  CHECK(s.stoodUps == 2 && s.satDowns == 2);
  CHECK(s.last.state == State::Empty);
}

int main() {
  sit_and_stand_cues();
  passerby_does_not_start_session();
  sitting_starts_after_confirm();
  warning_then_overdue_with_repeat_reminders();
  short_dropout_is_ignored();
  short_standup_does_not_reset();
  real_break_resets_session();
  new_day_gap_resets_break_count();
  survives_millis_wraparound();
  if (failures) { std::printf("%d failure(s)\n", failures); return 1; }
  std::printf("all tests passed\n");
  return 0;
}
