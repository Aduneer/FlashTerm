#include "sync.h"

#include <dirent.h>

#include <algorithm>
#include <map>
#include <ostream>
#include <set>
#include <string>
#include <vector>

#include "date.h"
#include "event.h"
#include "schedule.h"
#include "text.h"

namespace FlashTerm {
namespace {
// Columns the question gets before the change description starts, so a run
// over a few cards reads as a table rather than a ragged list. Measured in
// terminal columns, so a CJK deck lines up too.
constexpr std::size_t kQuestionWidth = 32;

bool starts_with(const std::string& text, const std::string& prefix) {
  return text.size() >= prefix.size() &&
         text.compare(0, prefix.size(), prefix) == 0;
}

bool ends_with(const std::string& text, const std::string& suffix) {
  return text.size() >= suffix.size() &&
         text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// The directory to list, the prefix that turns an entry in it back into a
// usable path, and the file name to match against. A path with no slash lives
// in the working directory, which opendir spells "." and paths spell "".
void split_path(const std::string& path, std::string* dir, std::string* prefix,
                std::string* name) {
  const std::size_t slash = path.find_last_of('/');
  if (slash == std::string::npos) {
    *dir = ".";
    *prefix = "";
    *name = path;
    return;
  }
  *dir = (slash == 0) ? std::string("/") : path.substr(0, slash);
  *prefix = path.substr(0, slash + 1);
  *name = path.substr(slash + 1);
}

const CardState& state_of(const std::map<std::string, CardState>& states,
                          const std::string& card_id) {
  static const CardState kNothingYet;
  const auto found = states.find(card_id);
  return (found == states.end()) ? kNothingYet : found->second;
}

bool same_schedule(const CardState& a, const CardState& b) {
  return a.leitner_box == b.leitner_box && a.last_reviewed == b.last_reviewed &&
         a.due_date == b.due_date;
}

std::string join(const std::vector<std::string>& parts) {
  std::string joined;
  for (const auto& part : parts) {
    if (!joined.empty()) joined += ", ";
    joined += part;
  }
  return joined;
}

// Signed, because a delta can go down: a conflict copy may contain the undo of
// an answer this machine already counted, and "-1 correct" is what that is.
std::string signed_count(int delta, const char* what) {
  return (delta > 0 ? "+" : "") + std::to_string(delta) + " " + what;
}
}  // namespace

bool is_conflict_copy(const std::string& log_name,
                      const std::string& candidate) {
  if (candidate == log_name || candidate.size() <= log_name.size()) return false;

  const std::size_t dot = log_name.find_last_of('.');
  const std::string stem =
      (dot == std::string::npos) ? log_name : log_name.substr(0, dot);
  const std::string extension =
      (dot == std::string::npos) ? std::string() : log_name.substr(dot);

  std::string inserted;
  if (starts_with(candidate, log_name)) {
    // Appended: "spanish.txt.log.sync-conflict-...".
    inserted = candidate.substr(log_name.size());
  } else if (!extension.empty() && starts_with(candidate, stem) &&
             ends_with(candidate, extension)) {
    // Inserted before the extension, which is what Syncthing and Dropbox
    // actually do: "spanish.txt.sync-conflict-....log".
    inserted = candidate.substr(
        stem.size(), candidate.size() - stem.size() - extension.size());
  } else {
    return false;
  }

  // The word itself is the test, rather than each client's exact spelling.
  // Every one of them says "conflict" somewhere, none of them says it by
  // accident, and a rule that has to be extended per sync tool is a rule that
  // will be out of date the first time someone uses a different one.
  return to_lowercase(inserted).find("conflict") != std::string::npos;
}

std::vector<std::string> find_conflict_copies(const std::string& log_path) {
  std::string dir;
  std::string prefix;
  std::string name;
  split_path(log_path, &dir, &prefix, &name);

  std::vector<std::string> found;
  DIR* handle = opendir(dir.c_str());
  if (handle == nullptr) return found;
  while (const dirent* entry = readdir(handle)) {
    const std::string candidate = entry->d_name;
    if (is_conflict_copy(name, candidate)) found.push_back(prefix + candidate);
  }
  closedir(handle);

  std::sort(found.begin(), found.end());
  return found;
}

AbsorbResult absorb_conflicts(Deck& deck, std::ostream& out,
                              std::ostream& errors) {
  AbsorbResult result;
  const std::string log_path = deck.log().path();

  const std::vector<std::string> copies = find_conflict_copies(log_path);
  result.copies = static_cast<int>(copies.size());
  if (copies.empty()) {
    out << "No sync-conflict copies of " << log_path << " to absorb.\n";
    return result;
  }

  out << "Reading "
      << count_label(result.copies, "sync-conflict copy", "sync-conflict copies")
      << " of " << log_path << ":\n";

  const std::vector<ReviewEvent> ours = deck.log().events();
  std::vector<ReviewEvent> merged = ours;
  for (const auto& copy : copies) {
    EventLog other(copy);
    // The copy was found by listing the directory, so failing to read it means
    // something is wrong with the file rather than with the guess that it
    // exists -- and absorbing part of a conflict is worse than absorbing none.
    if (!other.load()) {
      errors << "FlashTerm: cannot read " << copy << "\n";
      result.failed = true;
      return result;
    }
    out << "  " << copy << ": "
        << count_label(static_cast<int>(other.events().size()), "event",
                       "events")
        << "\n";
    merged = merge_events(merged, other.events());
  }

  result.absorbed =
      static_cast<int>(merged.size()) - static_cast<int>(ours.size());
  if (result.absorbed <= 0) {
    out << "Nothing new: " << log_path << " already has every event in "
        << ((result.copies == 1) ? "that copy" : "those copies") << ".\n";
    return result;
  }

  std::string error;
  if (!deck.log().rewrite(merged, &error)) {
    errors << "FlashTerm: " << error << "\n";
    result.failed = true;
    return result;
  }
  out << "Absorbed "
      << count_label(result.absorbed, "new event", "new events") << " into "
      << log_path << ", now "
      << count_label(static_cast<int>(merged.size()), "event", "events")
      << ".\n";

  // The whole point of replaying twice: the difference between the two is
  // exactly what the other machine did, and nothing else. Applying `after` on
  // its own would quietly throw away every review a deck did before the log
  // existed, which is the reason replay() has never been wired into load().
  const std::map<std::string, CardState> before = replay(ours);
  const std::map<std::string, CardState> after = replay(merged);

  std::set<std::string> in_deck;
  for (const auto& card : deck.cards()) in_deck.insert(card.id);
  for (const auto& entry : after) {
    if (in_deck.count(entry.first) > 0) continue;
    const CardState& was = state_of(before, entry.first);
    if (was.times_correct != entry.second.times_correct ||
        was.times_incorrect != entry.second.times_incorrect ||
        !same_schedule(was, entry.second)) {
      ++result.unknown_cards;
    }
  }

  for (auto& card : deck.cards()) {
    const auto found = after.find(card.id);
    if (found == after.end()) continue;
    const CardState& now = found->second;
    const CardState& was = state_of(before, card.id);

    const int gained_correct = now.times_correct - was.times_correct;
    const int gained_incorrect = now.times_incorrect - was.times_incorrect;
    if (gained_correct == 0 && gained_incorrect == 0 &&
        same_schedule(was, now)) {
      continue;
    }

    std::vector<std::string> changes;
    card.times_correct += gained_correct;
    card.times_incorrect += gained_incorrect;
    if (gained_correct != 0) {
      changes.push_back(signed_count(gained_correct, "correct"));
    }
    if (gained_incorrect != 0) {
      changes.push_back(signed_count(gained_incorrect, "incorrect"));
    }

    // Whether the merged log gets to say what this card's schedule is.
    //
    // It does when this machine's log already had events for the card: replay
    // is then the whole story for it, and following the log backwards is right
    // too, since an absorbed undo takes an answer back rather than adding one.
    //
    // It does not when the deck's own state is newer than anything the log
    // knows about, which is what a card reviewed before the log existed looks
    // like: an older event is not evidence about today's schedule, however new
    // it is to this machine.
    const bool log_covers_card = before.find(card.id) != before.end();
    if (!same_schedule(was, now) &&
        (log_covers_card || now.last_reviewed >= card.last_reviewed)) {
      if (now.leitner_box != card.leitner_box) {
        changes.push_back("box " + std::to_string(card.leitner_box) + " -> " +
                          std::to_string(now.leitner_box));
      }
      if (now.due_date != card.due_date) {
        changes.push_back("due " + format_date(now.due_date));
      }
      card.leitner_box = now.leitner_box;
      card.last_reviewed = now.last_reviewed;
      card.due_date = now.due_date;
    }
    if (changes.empty()) continue;

    ++result.cards_updated;
    out << "  "
        << pad_right(truncate(card.question, kQuestionWidth), kQuestionWidth)
        << "  " << join(changes) << "\n";
  }

  if (result.cards_updated > 0) {
    if (!deck.save(&error)) {
      errors << "FlashTerm: could not save " << deck.path() << ": " << error
             << "\n";
      result.failed = true;
      return result;
    }
    out << "Updated "
        << count_label(result.cards_updated, "card", "cards") << " in "
        << deck.path() << ".\n";
  } else {
    out << "No card in this deck changed.\n";
  }

  // Not a problem to fix: a deck and its log are two files, and nothing makes
  // them arrive in the same order. Saying so beats an unexplained gap between
  // what the log records and what the deck counts.
  if (result.unknown_cards > 0) {
    out << count_label(result.unknown_cards, "card", "cards")
        << " named by those events "
        << ((result.unknown_cards == 1) ? "is" : "are")
        << " not in this deck.\n"
        << ((result.unknown_cards == 1) ? "Its" : "Their")
        << " events stay in the log, and will count once the deck catches "
           "up.\n";
  }

  // Left in place on purpose: absorbing again finds nothing new, so keeping
  // them costs only disk, while deleting the wrong file costs a history that
  // exists nowhere else.
  out << "The " << ((result.copies == 1) ? "copy was" : "copies were")
      << " left in place. Delete when you are happy with the result:\n";
  for (const auto& copy : copies) {
    out << "  rm " << copy << "\n";
  }
  return result;
}
}  // namespace FlashTerm
