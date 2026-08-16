#pragma once
#include <string>

namespace FlashTerm {
inline constexpr char kVersion[] = "1.0.0";

// What the command line asked for. Anything that is not RunDeck is handled and
// exited on immediately, before a deck is touched.
enum class CliAction {
  RunDeck,      // Study `deck_path`.
  ShowHelp,     // --help: usage to stdout, exit 0.
  ShowVersion,  // --version: version to stdout, exit 0.
  Error,        // Bad usage: `error` to stderr, exit 2.
};

struct CliOptions {
  CliAction action = CliAction::RunDeck;
  std::string deck_path;
  std::string error;
};

// Parses argv, treating a leading-dash argument as an option rather than a deck
// path: `FlashTerm --help` used to create a deck file literally named
// "--help" in the working directory. `--` ends option parsing, so a deck whose
// name really does start with a dash stays reachable as `FlashTerm -- -deck.txt`.
CliOptions parse_args(int argc, const char* const argv[],
                      const std::string& default_deck);

// Usage text for --help, also shown when usage is wrong.
std::string usage_text();
}  // namespace FlashTerm
