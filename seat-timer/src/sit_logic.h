// Seat-time state machine. Pure C++, no Arduino dependency, so it can be
// unit-tested on the host (see test/test_sit_logic.cpp).
//
// All times are milliseconds from a free-running counter (millis()).
// Only differences are used, so the 49-day millis() wraparound is safe.
#pragma once
#include <stdint.h>

namespace sit {

struct Config {
  uint32_t sitConfirmMs  = 5000UL;            // presence must last this long to count as "sat down"
  uint32_t breakMs       = 60000UL;           // absence this long ends the session (counts as a break)
  uint32_t awayShowMs    = 3000UL;            // absence shorter than this is treated as sensor jitter
  uint32_t sitLimitMs    = 30UL * 60 * 1000;  // remind after this much continuous sitting
  uint32_t warnBeforeMs  = 2UL * 60 * 1000;   // pre-warning window before the limit
  uint32_t remindEveryMs = 90000UL;           // repeat reminder interval while still seated
  uint32_t newDayGapMs   = 4UL * 3600 * 1000; // absence this long resets the break counter
};

enum class State : uint8_t {
  Empty,    // no session
  Sitting,  // session running, below warning threshold
  Warning,  // session running, within warnBeforeMs of the limit
  Overdue,  // session past the limit, person is seated
  Away,     // session running but person absent (< breakMs); break in progress
};

struct Output {
  State    state          = State::Empty;
  bool     beep           = false;  // true on the tick a reminder fires
  uint16_t reminderIndex  = 0;      // 1 for first reminder of the session, 2 for second, ...
  bool     breakCompleted = false;  // true on the tick a session ends
  bool     overdueAtBreak = false;  // with breakCompleted: was the session past the limit
  bool     satDown        = false;  // true on the tick a session starts or you return from Away
  bool     stoodUp        = false;  // true on the tick a running session goes Away
  uint32_t sittingMs      = 0;      // session length so far (includes short absences)
  uint32_t awayMs         = 0;      // current absence length (0 if present)
  uint16_t breaks         = 0;      // completed sessions since last new-day reset
};

class Tracker {
 public:
  explicit Tracker(const Config& cfg = Config()) : cfg_(cfg) {}

  Output update(uint32_t now, bool present) {
    Output out = step(now, present);
    bool wasSeated = prev_ != State::Empty && prev_ != State::Away;
    bool isSeated  = out.state != State::Empty && out.state != State::Away;
    out.satDown = isSeated && !wasSeated;
    out.stoodUp = out.state == State::Away && wasSeated;
    prev_ = out.state;
    return out;
  }

  const Config& config() const { return cfg_; }

 private:
  Output step(uint32_t now, bool present) {
    Output out;

    if (present) {
      if (!presentRun_) { presentRun_ = true; presentSince_ = now; }
      lastPresent_ = now;
    } else {
      presentRun_ = false;
    }

    if (!session_) {
      if (presentRun_ && now - presentSince_ >= cfg_.sitConfirmMs) {
        if (hadSession_ && presentSince_ - lastSessionEnd_ >= cfg_.newDayGapMs) breaks_ = 0;
        session_       = true;
        sessionStart_  = presentSince_;
        reminders_     = 0;
      }
    } else {
      uint32_t away = present ? 0 : now - lastPresent_;
      if (away >= cfg_.breakMs) {
        out.breakCompleted = true;
        out.overdueAtBreak = (lastPresent_ - sessionStart_) >= cfg_.sitLimitMs;
        session_        = false;
        hadSession_     = true;
        lastSessionEnd_ = lastPresent_;
        if (breaks_ < 0xFFFF) breaks_++;
      }
    }

    out.breaks = breaks_;
    if (!session_) { out.state = State::Empty; return out; }

    out.sittingMs = now - sessionStart_;
    out.awayMs    = present ? 0 : now - lastPresent_;

    if (out.awayMs >= cfg_.awayShowMs) {
      out.state = State::Away;
      return out;
    }

    if (out.sittingMs >= cfg_.sitLimitMs) {
      out.state = State::Overdue;
      // Only beep while actually detected; jitter dropouts just delay the beep.
      if (present && (reminders_ == 0 || now - lastRemind_ >= cfg_.remindEveryMs)) {
        lastRemind_ = now;
        if (reminders_ < 0xFFFF) reminders_++;
        out.beep          = true;
        out.reminderIndex = reminders_;
      }
    } else if (out.sittingMs + cfg_.warnBeforeMs >= cfg_.sitLimitMs) {
      out.state = State::Warning;
    } else {
      out.state = State::Sitting;
    }
    return out;
  }

  Config   cfg_;
  State    prev_           = State::Empty;
  bool     presentRun_     = false;
  uint32_t presentSince_   = 0;
  uint32_t lastPresent_    = 0;
  bool     session_        = false;
  uint32_t sessionStart_   = 0;
  bool     hadSession_     = false;
  uint32_t lastSessionEnd_ = 0;
  uint32_t lastRemind_     = 0;
  uint16_t reminders_      = 0;
  uint16_t breaks_         = 0;
};

}  // namespace sit
