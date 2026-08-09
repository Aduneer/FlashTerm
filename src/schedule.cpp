#include "schedule.h"

#include <algorithm>

namespace FlashTerm {
int interval_for_box(int box) {
  return kBoxIntervals[std::min(kMaxBox, std::max(1, box))];
}

bool is_due(const Flashcard& card, int today_days) {
  return card.due_date == kNoDate || card.due_date <= today_days;
}

AnswerResult record_answer(Flashcard* card, bool correct, int today_days) {
  AnswerResult result;
  result.old_box = card->leitner_box;

  if (correct) {
    card->times_correct++;
    card->leitner_box = std::min(kMaxBox, card->leitner_box + 1);
  } else {
    card->times_incorrect++;
    card->leitner_box = 1;
  }

  result.new_box = card->leitner_box;
  result.interval_days = interval_for_box(result.new_box);

  card->last_reviewed = today_days;
  card->due_date = today_days + result.interval_days;
  result.due_date = card->due_date;
  return result;
}
}  // namespace FlashTerm
