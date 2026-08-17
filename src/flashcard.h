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

  Flashcard(const std::string& q, const std::string& a,
            const std::vector<std::string>& t = {}, int correct = 0,
            int incorrect = 0, int leitner = 1);

  std::string tags_to_string() const;
};
}  // namespace FlashTerm
