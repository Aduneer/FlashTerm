#include "cli.h"

#include <string>

namespace FlashTerm {
namespace {
constexpr char kOneModeOnly[] =
    "--generate-audio and --absorb-conflicts cannot be combined";

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

  FlashTerm [deck] --absorb-conflicts
                          Merge the sync-conflict copies your file-sync tool
                          left beside the deck's review log back into it, and
                          bring the deck's counters and due dates up to date
                          with the reviews they contain. Reads the copies and
                          leaves them alone; running it twice finds nothing the
                          second time.

The deck is created if it does not exist, and saved after every answer and
edit, so interrupting a session costs nothing. Import a starter deck from
examples/ with menu option 5.

Every answer is also appended to a review log beside the deck, named after it
with ".log" added, which is where the streak and review-count statistics on the
progress screen come from. Because it is only ever appended to, two machines
reviewing before they sync produce two complete halves of one history rather
than a lost one -- which is what --absorb-conflicts puts back together.

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
  FLASHTERM_IMAGE  How to draw a card's picture: "kitty" for terminals that
                   speak the kitty graphics protocol, "chafa" to draw it as
                   coloured text blocks instead, or "none" to draw nothing.
                   Worked out from $TERM otherwise. Worth setting to "kitty"
                   on a terminal that can manage it but does not say so in
                   $TERM, WezTerm being the common one. A deck full of
                   pictures still reviews perfectly well as text, so this is
                   never required.
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

Pictures work the same way. A card's eleventh column names an image beside the
deck, drawn inside the card frame: as a real graphic on terminals that speak
the kitty protocol, and as coloured text blocks through chafa if it is
installed, which needs no graphics support at all. Everywhere else the same
deck is an ordinary deck of text.
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
  // Each of these does one thing to a deck and exits, so two of them is not a
  // request that can be honoured -- and picking the last one silently would
  // skip whichever the user typed first.
  bool mode_given = false;

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
        if (mode_given) return error(kOneModeOnly);
        options.action = CliAction::GenerateAudio;
        mode_given = true;
        continue;
      }
      if (arg == "--absorb-conflicts") {
        if (mode_given) return error(kOneModeOnly);
        options.action = CliAction::AbsorbConflicts;
        mode_given = true;
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
