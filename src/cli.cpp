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

  FlashTerm [deck] --generate-audio --voice <name> [--force]
                          Render a sound file for every card's question into an
                          audio/ directory beside the deck, and record each one
                          in the deck's audio column, so that reviewing plays a
                          real voice instead of a robotic one. Cards that
                          already have a recording are skipped unless --force
                          is given.

                          <name> is a piper voice, such as fr_FR-siwis-medium.
                          Run it without --voice to be shown which ones are
                          installed and how to get more.

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
  FLASHTERM_VOICES Extra directories to search for piper voices, separated by
                   colons like PATH. The usual places are searched anyway.
  FLASHTERM_TTS_RENDER
                   A synthesiser other than piper for --generate-audio, which
                   it then uses instead of --voice: "{out}" is replaced with
                   the path to write and the text arrives on standard input.
                     espeak-ng -v fr --stdin -w {out}
  FLASHTERM_TTS    Command that speaks the text given as its last argument, for
                   cards with no recording of their own. Defaults to whichever
                   of espeak-ng, espeak, say or flite is installed. Add
                   arguments to pick a voice: "espeak-ng -v fr".
  FLASHTERM_PLAYER Command that plays the audio file given as its last
                   argument. Defaults to whichever of mpv, ffplay, paplay,
                   pw-play, mpg123, afplay or aplay is installed.

Audio is optional in every sense: nothing is linked against, nothing is
installed, and if none of the above is on the PATH then review simply does not
offer the key. Press "a" during a review to hear the card.
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
      // Unlike --help and --version these do not return immediately: both are
      // about a deck, so the rest of the command line still has to be read.
      if (arg == "--generate-audio") {
        options.action = CliAction::GenerateAudio;
        continue;
      }
      if (arg == "--force") {
        options.force = true;
        continue;
      }
      // Accepts both spellings, because a user who has just been shown
      // `--voice fr_FR-siwis-medium` will type it either way.
      if (arg == "--voice") {
        if (i + 1 >= argc) return error("--voice needs the name of a voice");
        options.voice = argv[++i];
        continue;
      }
      if (arg.rfind("--voice=", 0) == 0) {
        options.voice = arg.substr(std::string("--voice=").size());
        if (options.voice.empty()) {
          return error("--voice needs the name of a voice");
        }
        continue;
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

  // Either on its own would silently do nothing, which is worse than saying so:
  // the likely reading of --force is that it forces something about a review,
  // and of --voice that it changes the voice used when reviewing.
  if (options.action != CliAction::GenerateAudio) {
    if (options.force) return error("--force only applies to --generate-audio");
    if (!options.voice.empty()) {
      return error("--voice only applies to --generate-audio");
    }
  }
  if (!have_deck) options.deck_path = default_deck;
  return options;
}
}  // namespace FlashTerm
