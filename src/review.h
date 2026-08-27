#pragma once
#include <string>

#include "deck.h"

namespace FlashTerm {
// The deck is saved after every answered card, so an interrupted session
// never loses the progress it already earned.
void review_flashcards(Deck& deck);

// A reversed session shows the card back-to-front: the answer becomes the
// prompt and the question is what you have to produce. That trains recall
// ("library" -> "la biblioteca") rather than recognition, which is a harder and
// genuinely different skill. Both directions share one Leitner box and due
// date: reversing is a way of asking, not a second card.
//
// Only the first accepted answer is used as the prompt, since "git add|add" is
// not a sensible thing to show.
//
// A cloze card ignores `reversed` and is described whole: the prompt is its
// sentence with every hole open and the expected answer is the same sentence
// with every hole filled in. Reversing it would mean showing the finished
// sentence and asking for the one with holes in it, which is not a question.
// The review loop asks such a card one hole at a time instead; see cloze.h.
std::string prompt_text(const Flashcard& card, bool reversed);
std::string expected_answer(const Flashcard& card, bool reversed);
}  // namespace FlashTerm
