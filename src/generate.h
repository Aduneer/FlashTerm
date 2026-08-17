#pragma once
#include <iosfwd>
#include <string>

#include "deck.h"

namespace FlashTerm {
struct GenerateResult {
  int rendered = 0;
  int skipped = 0;
  int failed = 0;

  // What the process should exit with: a run that could not render everything
  // it was asked to has to be noticeable from a script.
  int exit_code() const { return failed > 0 ? 1 : 0; }
};

// Renders a sound file for every card's question and records it in the deck's
// audio column, for cards that do not already have one. `force` re-renders the
// ones that do, which is how a deck picks up a better voice or a fixed typo.
//
// `voice` names a piper voice, as --voice gives it. FLASHTERM_TTS_RENDER wins
// when it is set, since an explicit command is a deliberate one; with neither,
// nothing is rendered and the caller is told how to get a voice rather than
// simply that it has none.
//
// Non-interactive on purpose: it is the one thing FlashTerm does that is worth
// putting in a script or a Makefile, and prompting would ruin that. Progress
// goes to `out` a line at a time rather than in a summary at the end, because
// a good voice takes about a second a card and a silent minute looks hung.
//
// The deck is saved once, at the end, and only if something changed.
GenerateResult generate_audio(Deck& deck, const std::string& voice, bool force,
                              std::ostream& out, std::ostream& errors);
}  // namespace FlashTerm
