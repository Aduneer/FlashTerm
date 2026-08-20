#pragma once
#include <string>
#include <vector>

#include "event.h"
#include "flashcard.h"

namespace FlashTerm {

// One card as a CSV record:
//   question,answer,tags,correct,incorrect,box,last_reviewed,due_date,id,
//   audio,image
// Trailing columns are optional when reading, so a deck written by any earlier
// version still loads; writing always emits all of them.
std::string card_to_csv(const Flashcard& card);
// Returns false for records too short to be a card. Missing trailing fields
// fall back to their defaults, so a bare "question,answer" line still loads
// and a pre-scheduling six-field deck simply comes back due immediately.
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
  int due_count = 0;
  // Soonest due date among cards that are not due yet; kNoDate if none.
  int next_due = kNoDate;
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
  //
  // A write that would reproduce the file byte for byte is skipped and
  // reported as success. Saving is unconditional at every call site -- after
  // every answer, every edit, and on the way out -- which is what makes an
  // interrupted session cost nothing; the cost is that merely opening a deck
  // used to rewrite it. See the comment on the check itself.
  bool save(std::string* error = nullptr) const;

  const std::string& path() const { return path_; }

  // Resolves a path stored in the deck against the deck's own directory rather
  // than the working directory, so that a deck and the files beside it survive
  // being moved, synced or studied from elsewhere. An absolute path is taken as
  // given, and an empty one stays empty.
  std::string resolve(const std::string& relative) const;

  // Where a card's recording actually is; `resolve` applied to its column.
  std::string audio_path(const Flashcard& card) const {
    return resolve(card.audio);
  }

  // Likewise for its picture, so a deck and the images beside it move as one.
  std::string image_path(const Flashcard& card) const {
    return resolve(card.image);
  }

  // The review log lives beside the deck file and is loaded along with it: it
  // is part of the deck's representation on disk, not a separate thing the
  // caller has to know about. Appends go straight to the file, so unlike the
  // cards it needs no saving.
  EventLog& log() { return log_; }
  const EventLog& log() const { return log_; }

  std::vector<Flashcard>& cards() { return cards_; }
  const std::vector<Flashcard>& cards() const { return cards_; }
  std::size_t size() const { return cards_.size(); }
  bool empty() const { return cards_.empty(); }

  // Mints an id for the card if it does not have one, so that every card in a
  // deck can be named by the log.
  void add(const Flashcard& card);
  bool remove(std::size_t index);

  // Positions of the cards matching `query` as a case-insensitive substring of
  // the question, the answer or any tag, in deck order. An empty query matches
  // every card, so callers can treat "no search" as an unfiltered search.
  //
  // Positions, not copies: they stay valid for editing and deleting, and they
  // are what gets shown to the user, so the number typed to pick a card means
  // the same thing whether or not a search narrowed the list.
  std::vector<std::size_t> find(const std::string& query) const;

  // Tags as first seen, de-duplicated case-insensitively, sorted A-Z.
  std::vector<std::string> unique_tags() const;
  int count_with_tag(const std::string& tag) const;

  int due_count(int today_days) const;

  DeckStats stats(int today_days) const;

  // Gives every card a unique id, minting fresh ones for cards that have none
  // and for duplicates. Cards arrive without ids from decks written before the
  // log existed, and duplicated ids arrive from re-importing an export into
  // the deck it came from — two cards sharing one history would make the log
  // ambiguous, so the newcomer is renamed rather than the incumbent.
  //
  // Deliberately not called by load(): see ensure_id.
  void ensure_ids();

  // Gives one card an id if it has none, unique against the rest of the deck,
  // and returns it.
  //
  // This exists so that load() does not have to mint ids for a whole deck just
  // to open it. An id is what a log event names a card by, so a card needs one
  // at the moment something is about to be recorded against it and not before
  // — and a deck that is only read has nothing recorded against it at all.
  // Minting on load meant that opening a deck changed it, which made studying
  // a deck checked into a repository show up as a source change.
  const std::string& ensure_id(Flashcard& card);

 private:
  std::string path_;
  std::vector<Flashcard> cards_;
  EventLog log_;

  // What the file holds as of the last successful read or write, so that
  // save() can tell a real change from a no-op. Mutable because save() is
  // const to its callers and stays that way: what it records here is a fact
  // about the file rather than about the deck.
  //
  // The flag is not the same question as an empty string: a deck file that
  // does not exist yet still has to be created the first time it is saved,
  // and an empty deck serialises to nothing.
  mutable std::string on_disk_;
  mutable bool on_disk_known_ = false;
};

// Appends the cards in `path` to `deck`, preserving review statistics when
// the source file carries them.
ImportResult import_into(Deck& deck, const std::string& path);

// Writes every field, so an exported file can be re-imported without losing
// review history.
bool export_deck(const Deck& deck, const std::string& path, std::string* error);
}  // namespace FlashTerm
