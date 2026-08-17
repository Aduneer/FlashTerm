#include "event.h"

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <random>
#include <set>
#include <utility>

#include "date.h"
#include "text.h"

namespace FlashTerm {
namespace {
constexpr std::time_t kSecondsPerDay = 86400;

std::mt19937_64& id_rng() {
  static std::mt19937_64 generator(std::random_device{}());
  return generator;
}

std::string errno_message() {
  return std::strerror(errno);
}

int clamp_box(int box) { return std::min(kMaxBox, std::max(1, box)); }

int parse_int_or(const std::vector<std::string>& fields, std::size_t index,
                 int fallback) {
  if (fields.size() <= index || fields[index].empty()) return fallback;
  try {
    return std::stoi(fields[index]);
  } catch (const std::exception&) {
    return fallback;
  }
}

std::string field_or_empty(const std::vector<std::string>& fields,
                           std::size_t index) {
  return (fields.size() > index) ? trim(fields[index]) : std::string();
}

// Events sort by when they happened, and by id when that is not enough. The id
// tie-break exists so that two events recorded in the same second still have
// one agreed order on every machine.
bool earlier(const ReviewEvent& a, const ReviewEvent& b) {
  if (a.timestamp != b.timestamp) return a.timestamp < b.timestamp;
  return a.id < b.id;
}

// Ids of answers that were taken back, so both replay and summarize can ignore
// them without walking the log twice each.
std::set<std::string> undone_ids(const std::vector<ReviewEvent>& events) {
  std::set<std::string> undone;
  for (const auto& event : events) {
    if (event.is_undo()) undone.insert(event.undoes);
  }
  return undone;
}
}  // namespace

std::string generate_id() {
  const std::uint64_t value = id_rng()();
  char buffer[17];
  std::snprintf(buffer, sizeof(buffer), "%016llx",
                static_cast<unsigned long long>(value));
  return buffer;
}

std::string now_timestamp() {
  const std::time_t now = std::time(nullptr);
  std::tm utc{};
#if defined(_WIN32)
  gmtime_s(&utc, &now);
#else
  gmtime_r(&now, &utc);
#endif

  char buffer[32];
  std::snprintf(buffer, sizeof(buffer), "%04d-%02u-%02uT%02u:%02u:%02uZ",
                utc.tm_year + 1900, static_cast<unsigned>(utc.tm_mon + 1),
                static_cast<unsigned>(utc.tm_mday),
                static_cast<unsigned>(utc.tm_hour),
                static_cast<unsigned>(utc.tm_min),
                static_cast<unsigned>(utc.tm_sec));
  return buffer;
}

std::time_t parse_timestamp(const std::string& text) {
  if (text.size() != 20 || text[4] != '-' || text[7] != '-' ||
      text[10] != 'T' || text[13] != ':' || text[16] != ':' ||
      text[19] != 'Z') {
    return -1;
  }

  int year = 0;
  unsigned month = 0;
  unsigned day = 0;
  unsigned hour = 0;
  unsigned minute = 0;
  unsigned second = 0;
  if (std::sscanf(text.c_str(), "%4d-%2u-%2uT%2u:%2u:%2uZ", &year, &month, &day,
                  &hour, &minute, &second) != 6) {
    return -1;
  }
  if (month < 1 || month > 12 || day < 1 || day > 31 || hour > 23 ||
      minute > 59 || second > 60) {
    return -1;
  }

  const int days = days_from_civil(year, month, day);
  // Round-tripping rejects days that do not exist, such as 2026-02-30.
  if (format_date(days) != text.substr(0, 10)) return -1;

  return static_cast<std::time_t>(days) * kSecondsPerDay +
         static_cast<std::time_t>(hour) * 3600 +
         static_cast<std::time_t>(minute) * 60 +
         static_cast<std::time_t>(second);
}

int local_day_of(const std::string& timestamp) {
  const std::time_t when = parse_timestamp(timestamp);
  if (when < 0) return kNoDate;

  std::tm local{};
#if defined(_WIN32)
  localtime_s(&local, &when);
#else
  localtime_r(&when, &local);
#endif
  return days_from_civil(local.tm_year + 1900,
                         static_cast<unsigned>(local.tm_mon + 1),
                         static_cast<unsigned>(local.tm_mday));
}

std::string event_to_csv(const ReviewEvent& event) {
  std::string result = "undo";
  if (!event.is_undo()) {
    switch (event.outcome) {
      case Outcome::kCorrect: result = "correct"; break;
      case Outcome::kPartial: result = "partial"; break;
      case Outcome::kIncorrect: result = "incorrect"; break;
    }
  }
  return escape_csv_field(event.id) + "," + escape_csv_field(event.card_id) +
         "," + escape_csv_field(event.timestamp) + "," +
         std::string(1, event.direction) + "," + result + "," +
         std::to_string(event.box_before) + "," +
         std::to_string(event.box_after) + "," + escape_csv_field(event.undoes);
}

bool event_from_csv(const std::string& line, ReviewEvent* out) {
  const std::vector<std::string> fields = parse_csv_line(line);
  if (fields.size() < 5) return false;

  ReviewEvent event;
  event.id = trim(fields[0]);
  event.card_id = trim(fields[1]);
  event.timestamp = trim(fields[2]);
  if (event.id.empty() || event.card_id.empty()) return false;
  if (parse_timestamp(event.timestamp) < 0) return false;

  const std::string direction = trim(fields[3]);
  event.direction = (direction == "r") ? 'r' : 'n';

  const std::string result = to_lowercase(trim(fields[4]));
  // Anything unrecognised is read as a failed answer rather than rejected: a
  // future version's richer outcome still counts as a review that happened.
  event.outcome = (result == "correct")   ? Outcome::kCorrect
                  : (result == "partial") ? Outcome::kPartial
                                          : Outcome::kIncorrect;
  event.box_before = clamp_box(parse_int_or(fields, 5, 1));
  event.box_after = clamp_box(parse_int_or(fields, 6, 1));
  event.undoes = field_or_empty(fields, 7);

  // The result column and the undo target have to agree: an event that claims
  // to take something back without naming it says nothing, and one that names
  // a target while reporting a result is two events at once.
  if ((result == "undo") != !event.undoes.empty()) return false;

  *out = event;
  return true;
}

std::string log_path_for(const std::string& deck_path) {
  return deck_path + ".log";
}

EventLog::EventLog(std::string path) : path_(std::move(path)) {}

bool EventLog::load() {
  events_.clear();
  std::ifstream file(path_);
  if (!file.is_open()) return false;

  std::string line;
  while (std::getline(file, line)) {
    if (trim(line).empty()) continue;
    ReviewEvent event;
    if (event_from_csv(line, &event)) {
      events_.push_back(event);
    }
  }
  return true;
}

bool EventLog::append(const ReviewEvent& event, std::string* error) {
  events_.push_back(event);

  // Opened per append rather than held open for the session: reviews happen at
  // human speed, so the cost is irrelevant, and a file handle kept open across
  // a whole session would keep writing into a log that a sync tool had already
  // replaced underneath it.
  std::ofstream file(path_, std::ios::app);
  if (!file) {
    if (error) *error = "cannot write " + path_ + ": " + errno_message();
    return false;
  }
  file << event_to_csv(event) << "\n";
  file.flush();
  if (!file) {
    if (error) *error = "failed writing " + path_;
    return false;
  }
  return true;
}

std::vector<ReviewEvent> merge_events(const std::vector<ReviewEvent>& a,
                                      const std::vector<ReviewEvent>& b) {
  std::map<std::string, ReviewEvent> by_id;
  for (const auto* side : {&a, &b}) {
    for (const auto& event : *side) {
      auto found = by_id.find(event.id);
      if (found == by_id.end()) {
        by_id.emplace(event.id, event);
        continue;
      }
      // One id carrying two different records can only mean a damaged copy.
      // Keeping the lexicographically smaller one is arbitrary, but it is the
      // same choice on both machines, which is what matters.
      if (event_to_csv(event) < event_to_csv(found->second)) {
        found->second = event;
      }
    }
  }

  std::vector<ReviewEvent> merged;
  merged.reserve(by_id.size());
  for (auto& entry : by_id) {
    merged.push_back(entry.second);
  }
  std::sort(merged.begin(), merged.end(), earlier);
  return merged;
}

std::map<std::string, CardState> replay(
    const std::vector<ReviewEvent>& events) {
  std::vector<ReviewEvent> ordered = events;
  std::sort(ordered.begin(), ordered.end(), earlier);
  const std::set<std::string> undone = undone_ids(ordered);

  std::map<std::string, CardState> states;
  for (const auto& event : ordered) {
    if (event.is_undo() || undone.count(event.id) > 0) continue;

    CardState& state = states[event.card_id];
    // A partial counts against the score exactly as record_answer scores it;
    // what makes it different is the box, and the box is carried by the event.
    if (event.outcome == Outcome::kCorrect) {
      ++state.times_correct;
    } else {
      ++state.times_incorrect;
    }
    state.leitner_box = event.box_after;

    // The schedule is a function of the box and the day, so it is recomputed
    // rather than stored: an event says what happened, not what was planned.
    const int day = local_day_of(event.timestamp);
    if (day != kNoDate) {
      state.last_reviewed = day;
      state.due_date = day + interval_for_box(event.box_after);
    }
  }
  return states;
}

LogStats summarize(const std::vector<ReviewEvent>& events, int today_days) {
  const std::set<std::string> undone = undone_ids(events);

  LogStats stats;
  std::set<int> active_days;
  for (const auto& event : events) {
    if (event.is_undo() || undone.count(event.id) > 0) continue;

    const int day = local_day_of(event.timestamp);
    if (day == kNoDate) continue;
    active_days.insert(day);

    if (day == today_days) {
      ++stats.reviewed_today;
      if (event.outcome == Outcome::kCorrect) ++stats.correct_today;
      if (event.outcome == Outcome::kPartial) ++stats.hinted_today;
    }
  }

  // Today is still in progress, so an empty today does not end a streak that
  // ran through yesterday. Two empty days in a row does.
  int day = (active_days.count(today_days) > 0) ? today_days : today_days - 1;
  while (active_days.count(day) > 0) {
    ++stats.current_streak;
    --day;
  }
  return stats;
}
}  // namespace FlashTerm
