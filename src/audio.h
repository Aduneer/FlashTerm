#pragma once
#include <string>

namespace FlashTerm {
namespace audio {
// True when this machine can make a sound at all: either a player for the
// recorded files a deck may point at, or a speech synthesiser to fall back on.
// Review asks before offering the key, the same way it withholds "?" on a card
// that has no hint to give.
bool available();

// Plays `file` if it is a readable path, and otherwise speaks `text`. Returns
// false when nothing could be played, which is not an error worth interrupting
// a review over -- the caller says so quietly and carries on.
//
// Blocks until the sound finishes. Flashcard audio is a word or a sentence, and
// a review that carried on underneath its own audio would be answering the next
// card while still hearing the last one.
bool play(const std::string& file, const std::string& text);

// What `play` would use, as a human-readable command name ("espeak-ng", "mpv"),
// or empty if nothing is available. For --help and for saying why the key is
// missing.
std::string speaker_name();
std::string player_name();
}  // namespace audio
}  // namespace FlashTerm
