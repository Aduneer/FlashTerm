#include "cli.h"

#include <string>

namespace FlashTerm {
namespace {
CliOptions error(const std::string& message) {
  CliOptions options;
  options.action = CliAction::Error;
  options.error = message;
  return options;
}
}  // namespace

std::string usage_text() {
  return
      R"(FlashTerm - spaced-repetition flashcards in your terminal.

Usage:
  FlashTerm [deck]        Study `deck` (default: flashcards.txt).
  FlashTerm -- <deck>     Study a deck whose name starts with a dash.
  FlashTerm --help        Show this help and exit.
  FlashTerm --version     Show the version and exit.

The deck is created if it does not exist, and saved after every answer and
edit, so interrupting a session costs nothing. Import a starter deck from
examples/ with menu option 5.

Every answer is also appended to a review log beside the deck, named after it
with ".log" added, which is where the streak and review-count statistics on the
progress screen come from.

Menus take a single keypress when run in a terminal, and fall back to reading
whole lines when input is piped, so scripting still works.

Environment:
  FLASHTERM_DECK   Deck to study when none is named on the command line. Handy
                   when the deck lives in a synced directory. A deck given as
                   an argument still wins.
  FLASHTERM_THEME  Colour palette: default, ocean or sunset.
  NO_COLOR         Set to any value to disable coloured output, whatever the
                   theme says.
)";
}

std::string deck_from_env(const char* env_value, const std::string& fallback) {
  if (env_value == nullptr || *env_value == '\0') return fallback;
  return env_value;
}

CliOptions parse_args(int argc, const char* const argv[],
                      const std::string& default_deck) {
  CliOptions options;
  bool options_ended = false;
  bool have_deck = false;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];

    if (!options_ended) {
      if (arg == "--") {
        options_ended = true;
        continue;
      }
      if (arg == "-h" || arg == "--help") {
        options.action = CliAction::ShowHelp;
        return options;
      }
      if (arg == "-v" || arg == "--version") {
        options.action = CliAction::ShowVersion;
        return options;
      }
      // A bare "-" is not a deck name either, and silently creating a file
      // called "-x" is worse than refusing to start.
      if (arg.size() > 1 && arg[0] == '-') {
        return error("unknown option: " + arg);
      }
      if (arg == "-") {
        return error("reading a deck from standard input is not supported");
      }
    }

    if (have_deck) {
      return error("unexpected extra argument: " + arg);
    }
    if (arg.empty()) {
      return error("deck path is empty");
    }
    options.deck_path = arg;
    have_deck = true;
  }

  if (!have_deck) options.deck_path = default_deck;
  return options;
}
}  // namespace FlashTerm
