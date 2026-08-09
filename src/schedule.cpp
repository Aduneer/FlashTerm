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

CardState capture_state(const Flashcard& card) {
  CardState state;
  state.times_correct = card.times_correct;
  state.times_incorrect = card.times_incorrect;
  state.leitner_box = card.leitner_box;
  state.last_reviewed = card.last_reviewed;
  state.due_date = card.due_date;
  return state;
}

void restore_state(Flashcard* card, const CardState& state) {
  card->times_correct = state.times_correct;
  card->times_incorrect = state.times_incorrect;
  card->leitner_box = state.leitner_box;
  card->last_reviewed = state.last_reviewed;
  card->due_date = state.due_date;
}
}  // namespace FlashTerm
