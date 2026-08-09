#include "deck.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <set>
#include <utility>

#include "date.h"
#include "schedule.h"
#include "text.h"

namespace FlashTerm {
namespace {
int parse_int_or(const std::vector<std::string>& fields, size_t index,
                 int fallback) {
  if (fields.size() <= index || fields[index].empty()) return fallback;
  try {
    return std::stoi(fields[index]);
  } catch (const std::exception&) {
    return fallback;
  }
}

std::string errno_message() {
  return std::strerror(errno);
}
}  // namespace

std::string card_to_csv(const Flashcard& card) {
  return escape_csv_field(card.question) + "," + escape_csv_field(card.answer) +
         "," + escape_csv_field(card.tags_to_string()) + "," +
         std::to_string(card.times_correct) + "," +
         std::to_string(card.times_incorrect) + "," +
         std::to_string(card.leitner_box) + "," +
         format_date(card.last_reviewed) + "," + format_date(card.due_date);
}

bool card_from_csv(const std::string& line, Flashcard* out) {
  std::vector<std::string> fields = parse_csv_line(line);
  if (fields.size() < 2) return false;  // needs at least a question and answer

  const std::string tags_str = (fields.size() >= 3) ? fields[2] : "";
  int leitner = parse_int_or(fields, 5, 1);
  leitner = std::min(kMaxBox, std::max(1, leitner));

  *out = Flashcard(fields[0], fields[1], split(tags_str, ';'),
                   std::max(0, parse_int_or(fields, 3, 0)),
                   std::max(0, parse_int_or(fields, 4, 0)), leitner);

  // Absent or unreadable dates leave the card due immediately, which is how
  // decks written before scheduling existed are migrated.
  out->last_reviewed =
      (fields.size() >= 7) ? parse_date(fields[6]) : kNoDate;
  out->due_date = (fields.size() >= 8) ? parse_date(fields[7]) : kNoDate;
  return true;
}

Deck::Deck(std::string path) : path_(std::move(path)) {}

bool Deck::load() {
  cards_.clear();
  std::ifstream file(path_);
  if (!file.is_open()) {
    return false;
  }
  std::string line;
  while (std::getline(file, line)) {
    if (trim(line).empty()) continue;
    Flashcard card("", "");
    if (card_from_csv(line, &card)) {
      cards_.push_back(card);
    }
  }
  return true;
}

bool Deck::save(std::string* error) const {
  const std::string tmp_path = path_ + ".tmp";

  {
    std::ofstream file(tmp_path);
    if (!file) {
      if (error) *error = "cannot write " + tmp_path + ": " + errno_message();
      return false;
    }
    for (const auto& card : cards_) {
      file << card_to_csv(card) << "\n";
    }
    file.flush();
    if (!file) {
      if (error) *error = "failed writing " + tmp_path;
      file.close();
      std::remove(tmp_path.c_str());
      return false;
    }
  }

  if (std::rename(tmp_path.c_str(), path_.c_str()) != 0) {
    if (error) *error = "cannot replace " + path_ + ": " + errno_message();
    std::remove(tmp_path.c_str());
    return false;
  }
  return true;
}

bool Deck::remove(std::size_t index) {
  if (index >= cards_.size()) return false;
  cards_.erase(cards_.begin() + static_cast<std::ptrdiff_t>(index));
  return true;
}

std::vector<std::string> Deck::unique_tags() const {
  std::vector<std::string> unique;
  std::set<std::string> seen;
  for (const auto& card : cards_) {
    for (const auto& tag : card.tags) {
      if (tag.empty()) continue;
      if (seen.insert(to_lowercase(tag)).second) {
        unique.push_back(tag);
      }
    }
  }
  std::sort(unique.begin(), unique.end(),
            [](const std::string& a, const std::string& b) {
              return to_lowercase(a) < to_lowercase(b);
            });
  return unique;
}

int Deck::count_with_tag(const std::string& tag) const {
  const std::string needle = to_lowercase(tag);
  int count = 0;
  for (const auto& card : cards_) {
    for (const auto& card_tag : card.tags) {
      if (to_lowercase(card_tag) == needle) {
        ++count;
        break;
      }
    }
  }
  return count;
}

int Deck::due_count(int today_days) const {
  int count = 0;
  for (const auto& card : cards_) {
    if (is_due(card, today_days)) ++count;
  }
  return count;
}

DeckStats Deck::stats(int today_days) const {
  DeckStats stats;
  stats.total_cards = static_cast<int>(cards_.size());

  int hardest_incorrect = -1;
  double lowest_rate = 101.0;

  for (const auto& card : cards_) {
    stats.total_correct += card.times_correct;
    stats.total_incorrect += card.times_incorrect;
    stats.box_counts[std::min(kMaxBox, std::max(1, card.leitner_box))]++;

    if (is_due(card, today_days)) {
      ++stats.due_count;
    } else if (stats.next_due == kNoDate || card.due_date < stats.next_due) {
      stats.next_due = card.due_date;
    }

    const int reviews = card.times_correct + card.times_incorrect;
    if (reviews == 0) continue;

    const double rate = static_cast<double>(card.times_correct) * 100.0 / reviews;
    // Ties break towards the card that has been missed the most.
    if (rate < lowest_rate ||
        (rate == lowest_rate && card.times_incorrect > hardest_incorrect)) {
      lowest_rate = rate;
      hardest_incorrect = card.times_incorrect;
      stats.hardest_card = &card;
    }
  }

  const int reviews = stats.total_correct + stats.total_incorrect;
  stats.success_rate =
      (reviews == 0) ? 0.0
                     : static_cast<double>(stats.total_correct) * 100.0 / reviews;
  stats.hardest_rate = (stats.hardest_card == nullptr) ? 0.0 : lowest_rate;
  return stats;
}

ImportResult import_into(Deck& deck, const std::string& path) {
  ImportResult result;
  std::ifstream file(path);
  if (!file) {
    result.error = "failed to open " + path + ": " + errno_message();
    return result;
  }

  std::string line;
  while (std::getline(file, line)) {
    if (trim(line).empty()) continue;
    Flashcard card("", "");
    if (card_from_csv(line, &card)) {
      deck.add(card);
      ++result.imported;
    }
  }
  result.ok = true;
  return result;
}

bool export_deck(const Deck& deck, const std::string& path,
                 std::string* error) {
  std::ofstream file(path);
  if (!file) {
    if (error) *error = "failed to write " + path + ": " + errno_message();
    return false;
  }
  for (const auto& card : deck.cards()) {
    file << card_to_csv(card) << "\n";
  }
  file.flush();
  if (!file) {
    if (error) *error = "failed writing " + path;
    return false;
  }
  return true;
}
}  // namespace FlashTerm
