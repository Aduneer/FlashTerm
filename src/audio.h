#pragma once
#include <string>
#include <vector>

namespace FlashTerm {
namespace audio {
// A program and its arguments, ready to run. Never a shell command line: every
// element is one argument, so deck text in the last of them cannot become
// anything else.
using Command = std::vector<std::string>;
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

// Runs `command` with `text` on its standard input, writing a sound file. Every
// occurrence of "{out}" in the arguments is replaced with `output_path` first.
//
// Two substitutions rather than one because rendering to a file is not shaped
// like speaking aloud: a synthesiser needs to be told where to write. Text on
// standard input because that is the one convention every candidate already
// has, whatever it calls its output flag:
//
//   piper -m fr_FR-siwis-medium -f {out}
//   espeak-ng -v fr --stdin -w {out}
bool render(const Command& command, const std::string& text,
            const std::string& output_path);

// What FLASHTERM_TTS_RENDER asks for, or empty when it is unset or names a
// program that is not on the PATH. An explicit setting always wins over the
// voice FlashTerm would otherwise work out for itself.
Command render_command_from_environment();

// Whether the command's program exists and can be executed. Checked before a
// run so that a mistyped name is reported once, up front, rather than as every
// card in the deck failing in turn.
bool runnable(const Command& command);
}  // namespace audio
}  // namespace FlashTerm
