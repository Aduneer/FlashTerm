#pragma once
#include <string>
#include <vector>

#include "date.h"

namespace FlashTerm {
class Flashcard {
 public:
  std::string question;
  std::string answer;
  std::vector<std::string> tags;
  int times_correct;
  int times_incorrect;
  int leitner_box;

  // Scheduling state. kNoDate means the card has never been reviewed, which
  // counts as due immediately.
  int last_reviewed = kNoDate;
  int due_date = kNoDate;

  // Stable identity, so the review log can still name this card after it has
  // been edited, moved or had cards deleted around it. Empty until the card
  // joins a deck, which is what mints it; see Deck::ensure_ids.
  std::string id;

  // A recording of the question, as a path relative to the deck file -- so a
  // deck and its audio directory move together. Empty is the normal case and
  // means the question gets spoken by a synthesiser instead; see audio.h.
  std::string audio;

  // A picture for the card, resolved the same way `audio` is. Empty is the
  // normal case; a card that names one shows it inside the frame on terminals
  // that can draw it, and is an ordinary card everywhere else. See image.h.
  std::string image;

  Flashcard(const std::string& q, const std::string& a,
            const std::vector<std::string>& t = {}, int correct = 0,
            int incorrect = 0, int leitner = 1);

  std::string tags_to_string() const;
};
}  // namespace FlashTerm
