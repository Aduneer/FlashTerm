#pragma once
#include <string>
#include <vector>

#include "flashcard.h"

namespace FlashTerm {

// One card serialised as a CSV record: question,answer,tags,correct,incorrect,box
std::string card_to_csv(const Flashcard& card);
// Returns false for records too short to be a card. Missing trailing fields
// fall back to their defaults, so a bare "question,answer" line still loads.
bool card_from_csv(const std::string& line, Flashcard* out);

struct DeckStats {
  int total_cards = 0;
  int total_correct = 0;
  int total_incorrect = 0;
  int box_counts[6] = {0};  // index 0 unused; boxes are 1-5
  double success_rate = 0.0;
  // Lowest success rate among cards that have actually been reviewed.
  const Flashcard* hardest_card = nullptr;
  double hardest_rate = 0.0;
};

struct ImportResult {
  bool ok = false;
  int imported = 0;
  std::string error;
};

class Deck {
 public:
  explicit Deck(std::string path);

  // False when the file does not exist yet; a new deck is not an error.
  bool load();

  // Writes to a temporary file and renames it into place, so an interrupted
  // or failed write can never leave a truncated deck behind.
  bool save(std::string* error = nullptr) const;

  const std::string& path() const { return path_; }

  std::vector<Flashcard>& cards() { return cards_; }
  const std::vector<Flashcard>& cards() const { return cards_; }
  std::size_t size() const { return cards_.size(); }
  bool empty() const { return cards_.empty(); }

  void add(const Flashcard& card) { cards_.push_back(card); }
  bool remove(std::size_t index);

  // Tags as first seen, de-duplicated case-insensitively, sorted A-Z.
  std::vector<std::string> unique_tags() const;
  int count_with_tag(const std::string& tag) const;

  DeckStats stats() const;

 private:
  std::string path_;
  std::vector<Flashcard> cards_;
};

// Appends the cards in `path` to `deck`, preserving review statistics when
// the source file carries them.
ImportResult import_into(Deck& deck, const std::string& path);

// Writes every field, so an exported file can be re-imported without losing
// review history.
bool export_deck(const Deck& deck, const std::string& path, std::string* error);
}  // namespace FlashTerm
