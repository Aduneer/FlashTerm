#pragma once
#include <string>

namespace FlashTerm {
// Pre-1.0 on purpose: the deck format and review flow are still gaining
// features, and 1.0.0 would promise a stability that is not being offered yet.
inline constexpr char kVersion[] = "0.3.0";

// What the command line asked for. Anything that is not RunDeck is handled and
// exited on immediately, before a deck is touched.
enum class CliAction {
  RunDeck,        // Study `deck_path`.
  GenerateAudio,  // --generate-audio: render the deck's audio, then exit.
  AbsorbConflicts,  // --absorb-conflicts: merge the log's conflict copies in.
  ShowHelp,       // --help: usage to stdout, exit 0.
  ShowVersion,    // --version: version to stdout, exit 0.
  Error,          // Bad usage: `error` to stderr, exit 2.
};

struct CliOptions {
  CliAction action = CliAction::RunDeck;
  std::string deck_path;
  std::string error;

  // --force, which only means anything alongside --generate-audio: re-render
  // cards that already have a recording, so a deck can pick up a better voice.
  bool force = false;

  // --voice, likewise: the name of a piper voice to render with, such as
  // "fr_FR-siwis-medium". Empty means none was given.
  std::string voice;
};

// Parses argv, treating a leading-dash argument as an option rather than a deck
// path: `FlashTerm --help` used to create a deck file literally named
// "--help" in the working directory. `--` ends option parsing, so a deck whose
// name really does start with a dash stays reachable as `FlashTerm -- -deck.txt`.
CliOptions parse_args(int argc, const char* const argv[],
                      const std::string& default_deck);

// The deck to study when the command line does not name one: `env_value` if it
// is set and not empty, otherwise `fallback`. This is what $FLASHTERM_DECK is
// for — a deck kept in a synced directory can be studied as plain `FlashTerm`
// from anywhere, without retyping the path or cd-ing to it.
//
// Takes the value rather than reading the environment itself, so the rule is
// testable without a process-wide setenv. An unset and an empty variable mean
// the same thing; the value is otherwise used verbatim, because a path is
// allowed to contain surprising characters and second-guessing it would make
// some legitimate filenames unreachable.
std::string deck_from_env(const char* env_value, const std::string& fallback);

// Usage text for --help, also shown when usage is wrong.
std::string usage_text();
}  // namespace FlashTerm
