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

// Renders `text` to a sound file at `output_path`, for --generate-audio.
//
// The command comes from FLASHTERM_TTS_RENDER, where "{out}" stands for the
// output path and the text arrives on standard input. Two substitutions rather
// than one because rendering to a file is not shaped like speaking aloud: a
// synthesiser needs to be told where to write. Text on stdin rather than as an
// argument because that is the one convention both candidates already have:
//
//   piper -m fr_FR-siwis-medium -f {out}
//   espeak-ng -v fr --stdin -w {out}
//
// Returns false if the variable is unset, the command is not runnable, or it
// exits non-zero. There is no default: piper cannot be run without naming a
// voice, and a guessed language would quietly render a French deck in English.
bool render(const std::string& text, const std::string& output_path);

// The configured render command's program name, or empty when
// FLASHTERM_TTS_RENDER is unset or names something that is not runnable. Lets
// the caller explain which of the two it is rather than only that it failed.
std::string renderer_name();
}  // namespace audio
}  // namespace FlashTerm
