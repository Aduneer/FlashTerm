#pragma once
#include "flashcard.h"

namespace FlashTerm {
constexpr int kMaxBox = 5;

// Days a card waits after landing in each box. Doubling-ish Leitner spacing:
// a card must survive five correct answers to reach the longest interval.
// Index 0 is unused so the array is indexed by box number directly.
constexpr int kBoxIntervals[kMaxBox + 1] = {0, 1, 3, 7, 14, 30};

int interval_for_box(int box);

// A card with no due date has never been scheduled, so it is due now.
bool is_due(const Flashcard& card, int today_days);

struct AnswerResult {
  int old_box = 1;
  int new_box = 1;
  int interval_days = 1;
  int due_date = 0;
};

// Applies the Leitner move and the resulting schedule: a correct answer
// promotes one box, a wrong answer drops the card straight back to box 1.
AnswerResult record_answer(Flashcard* card, bool correct, int today_days);
}  // namespace FlashTerm
