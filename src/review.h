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
std::string prompt_text(const Flashcard& card, bool reversed);
std::string expected_answer(const Flashcard& card, bool reversed);
}  // namespace FlashTerm
