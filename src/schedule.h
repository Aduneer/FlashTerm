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

// How well the card was answered.
//
// kPartial is what a hinted answer earns: you did produce the answer, so it is
// not the same as failing outright, but you did not produce it unaided, so it
// has not earned a longer interval either. It therefore holds the box still
// rather than promoting or demoting, and counts against the success rate.
enum class Outcome {
  kCorrect,
  kPartial,
  kIncorrect,
};

// Applies the Leitner move and the resulting schedule: a correct answer
// promotes one box, a wrong answer drops the card straight back to box 1, and
// a partial one leaves it where it is. Either way the card is rescheduled from
// today by whatever box it ends up in.
AnswerResult record_answer(Flashcard* card, Outcome outcome, int today_days);

// Everything record_answer touches, so an answer can be taken back exactly.
struct CardState {
  int times_correct = 0;
  int times_incorrect = 0;
  int leitner_box = 1;
  int last_reviewed = kNoDate;
  int due_date = kNoDate;
};

CardState capture_state(const Flashcard& card);
void restore_state(Flashcard* card, const CardState& state);
}  // namespace FlashTerm
