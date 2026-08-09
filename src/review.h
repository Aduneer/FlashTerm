#pragma once
#include "deck.h"

namespace FlashTerm {
// The deck is saved after every answered card, so an interrupted session
// never loses the progress it already earned.
void review_flashcards(Deck& deck);
}  // namespace FlashTerm
