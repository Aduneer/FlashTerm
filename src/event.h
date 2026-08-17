#pragma once
#include <ctime>
#include <map>
#include <string>
#include <vector>

#include "schedule.h"

namespace FlashTerm {

// An append-only record of every answer given. The counters on a Flashcard say
// what a card's state *is*; the log says what actually *happened*, which is
// what stats over time, streaks, leech detection and conflict-free sync all
// need and none of which the counters can answer.
//
// Two properties are deliberate:
//
//   * Events are immutable and carry their own id, so merging two machines'
//     logs is a set union rather than a conflict. "I answered this at 09:14"
//     and "I answered it at 21:02" are both simply true.
//   * Timestamps are seconds in UTC, not whole days like the rest of the app.
//     The log is the one place sub-day resolution can live without disturbing
//     the date model, and UTC is what makes two machines' events sortable
//     into one order regardless of timezone.
struct ReviewEvent {
  std::string id;
  std::string card_id;
  std::string timestamp;  // "2026-08-17T14:23:05Z"
  char direction = 'n';   // 'n' asked normally, 'r' asked reversed
  Outcome outcome = Outcome::kIncorrect;
  int box_before = 1;
  int box_after = 1;

  // Set only on an undo event: the id of the answer it takes back. An undo has
  // no result of its own, so `outcome` and both boxes are meaningless there.
  // Taking an answer back is recorded rather than erased, because a line that
  // may already have been synced to another machine cannot be unwritten.
  std::string undoes;

  bool is_undo() const { return !undoes.empty(); }
};

// 16 lowercase hex characters. Random rather than sequential so two machines
// that have never met still cannot mint the same id.
std::string generate_id();

// The current time as a UTC "YYYY-MM-DDTHH:MM:SSZ" stamp.
std::string now_timestamp();

// Seconds since the epoch, or -1 if `text` is not a well-formed stamp. Unix
// time is UTC by definition, so this is arithmetic with no timezone involved.
std::time_t parse_timestamp(const std::string& text);

// The local calendar day a timestamp falls on, as date.h days since the epoch;
// kNoDate if it cannot be parsed. Events are stored in UTC but streaks and
// "reviewed today" are questions about the user's own calendar, so the
// conversion back to local time happens here.
int local_day_of(const std::string& timestamp);

// One event as a CSV record:
//   id,card_id,timestamp,direction,result,box_before,box_after,undoes
// where result is "correct", "partial", "incorrect" or "undo". Logs written
// before hints existed simply contain no "partial" rows.
std::string event_to_csv(const ReviewEvent& event);
// Returns false for records too short or too malformed to be an event; a
// damaged line is skipped rather than taken as a review that never happened.
bool event_from_csv(const std::string& line, ReviewEvent* out);

// The log that belongs to a deck: the deck's path with ".log" appended. The
// suffix is appended rather than replacing the extension so that decks named
// "spanish.txt" and "spanish.csv" do not end up sharing one log.
std::string log_path_for(const std::string& deck_path);

class EventLog {
 public:
  explicit EventLog(std::string path);

  const std::string& path() const { return path_; }
  const std::vector<ReviewEvent>& events() const { return events_; }
  bool empty() const { return events_.empty(); }

  // False when the file does not exist yet; an empty log is a valid state, and
  // is exactly what an existing deck starts from.
  bool load();

  // Appends one line to the file and to the in-memory list. A single short
  // line written in append mode is atomic in practice, which is what lets a
  // log survive being written while Syncthing or Dropbox is watching it.
  //
  // Returns false with `error` set if the file could not be written. The event
  // is kept in memory regardless: failing to log must never cost a review.
  bool append(const ReviewEvent& event, std::string* error = nullptr);

 private:
  std::string path_;
  std::vector<ReviewEvent> events_;
};

// Set union by event id, ordered by timestamp and then id. Merging is how two
// machines' logs become one, and ordering by a field both sides agree on makes
// the result independent of which log was merged into which.
//
// Not yet called by the application: it is the half of file-based sync that
// has to exist before syncing can be more than "last writer wins".
std::vector<ReviewEvent> merge_events(const std::vector<ReviewEvent>& a,
                                      const std::vector<ReviewEvent>& b);

// Folds a log back into per-card counters and Leitner state, keyed by card id.
// Undone answers are skipped, so an answer taken back leaves no trace in the
// result even though it stays visible in the log.
//
// This is the function that makes the counters a derived cache rather than the
// source of truth. It is deliberately not wired into Deck::load() yet: the
// counters in an existing deck have no events behind them, so replaying today
// would report every deck as brand new. Sync is where this gets turned on,
// against a log that covers the whole history.
std::map<std::string, CardState> replay(const std::vector<ReviewEvent>& events);

struct LogStats {
  int reviewed_today = 0;
  int correct_today = 0;
  // Answers that needed the hint. Counted separately because "got it, but only
  // with a nudge" is the interesting middle the plain counters cannot express.
  int hinted_today = 0;
  // Consecutive days of reviewing, counted back from today. A day that has not
  // ended yet cannot break a streak, so an unbroken run that has not been
  // added to since yesterday still counts: the streak is only lost once a
  // whole day passes with nothing in it.
  int current_streak = 0;
};

LogStats summarize(const std::vector<ReviewEvent>& events, int today_days);
}  // namespace FlashTerm
