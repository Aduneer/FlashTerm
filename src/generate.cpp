#include "generate.h"

#include <sys/stat.h>
#include <unistd.h>

#include <ostream>
#include <string>

#include "audio.h"
#include "text.h"
#include "voice.h"

namespace FlashTerm {
namespace {
// Recordings go in a directory beside the deck rather than next to it, so that
// a deck of a few hundred cards does not bury the deck file itself.
constexpr char kAudioDirectory[] = "audio";

// Named after the card's id rather than its question. An id is already unique
// and already ASCII, which a question is neither: two cards may legitimately
// ask the same thing with different tags, and "¿Dónde está la estación?" is a
// filename only on a filesystem that is feeling generous. The cost is a
// directory that cannot be read at a glance, which is what the deck's audio
// column is for.
std::string audio_file_for(const Flashcard& card) {
  return std::string(kAudioDirectory) + "/" + card.id + ".wav";
}

// mkdir rather than a check-then-create, so two runs racing each other cannot
// both decide the directory is missing. Anything other than "it is already
// there" is left to the render itself to report, since it will fail too and
// its message names the actual file.
void ensure_directory(const std::string& path) {
  mkdir(path.c_str(), 0755);
}

bool file_exists(const std::string& path) {
  return access(path.c_str(), R_OK) == 0;
}

// Failures that are worth translating, because the message is accurate and
// still leaves someone stuck. Piper's Japanese voices phonemize through
// pyopenjtalk, which `pipx install piper-tts` does not bring with it, so a
// Japanese deck fails on its first card with a bare ModuleNotFoundError.
std::string hint_for(const std::string& diagnostics) {
  if (diagnostics.find("pyopenjtalk") != std::string::npos) {
    return
        "That voice needs a phonemizer that piper does not install by "
        "default:\n\n"
        "  pipx inject piper-tts pyopenjtalk\n\n"
        "It downloads a dictionary the first time it runs, so the first card "
        "is slow.\n";
  }
  return {};
}
}  // namespace

GenerateResult generate_audio(Deck& deck, const std::string& voice, bool force,
                              std::ostream& out, std::ostream& errors) {
  GenerateResult result;

  // An explicit command is a deliberate one, so it beats a named voice rather
  // than the other way round.
  audio::Command renderer = audio::render_command_from_environment();
  if (renderer.empty() && !voice.empty()) {
    renderer = voice::render_command(voice);
    if (renderer.empty()) {
      errors << "FlashTerm: no piper voice called \"" << voice << "\".\n\n"
             << voice::setup_instructions(voice, deck.path());
      result.failed = 1;
      return result;
    }
  }
  if (renderer.empty()) {
    errors << "FlashTerm: --generate-audio needs a voice.\n\n"
              "Name one with --voice, for example:\n\n"
              "  FlashTerm "
           << deck.path()
           << " --generate-audio --voice fr_FR-siwis-medium\n\n"
              "Or name the synthesiser yourself with FLASHTERM_TTS_RENDER, "
              "where {out} is\nthe file to write and the text arrives on "
              "standard input:\n\n"
              "  FLASHTERM_TTS_RENDER=\"espeak-ng -v fr --stdin -w {out}\"\n\n"
              "There is no default because a voice implies a language, and "
              "guessing it would\nrender a French deck in English.\n\n"
           << voice::setup_instructions("", deck.path());
    result.failed = 1;
    return result;
  }

  // A card is named by its id, so every card needs one before anything can be
  // written. Loading a deck normally mints them; a deck loaded and immediately
  // generated from may not have been through that yet.
  deck.ensure_ids();

  const std::string directory = deck.resolve(kAudioDirectory);
  bool changed = false;
  bool reported = false;

  for (Flashcard& card : deck.cards()) {
    const std::string relative =
        card.audio.empty() ? audio_file_for(card) : card.audio;
    const std::string absolute = deck.resolve(relative);

    if (!force && !card.audio.empty() && file_exists(absolute)) {
      out << "  skipped   " << relative << "  (exists)\n";
      ++result.skipped;
      continue;
    }

    ensure_directory(directory);
    std::string diagnostics;
    if (audio::render(renderer, card.question, absolute, &diagnostics)) {
      // Only recorded in the deck once the file is really there, so a run that
      // dies partway through leaves no card pointing at nothing.
      if (card.audio != relative) {
        card.audio = relative;
        changed = true;
      }
      out << "  rendered  " << relative << "  " << card.question << "\n";
      ++result.rendered;
    } else {
      errors << "  FAILED    " << relative << "  " << card.question << "\n";
      ++result.failed;

      // Said once. The reason is nearly always the same for every card, and a
      // traceback repeated five hundred times buries the one thing to read.
      if (!reported) {
        reported = true;
        const std::string reason = last_nonempty_line(diagnostics);
        if (!reason.empty()) errors << "            " << reason << "\n";
        const std::string hint = hint_for(diagnostics);
        if (!hint.empty()) errors << "\n" << hint << "\n";
      }

      // Nothing has worked yet and three cards have now failed, which is a
      // misconfiguration rather than a difficult card. Stopping beats spawning
      // a synthesiser once per card for a deck of five hundred to watch every
      // one of them fail the same way.
      if (result.rendered == 0 && result.failed >= 3) {
        errors << "\nStopped after three failures without a success.\n";
        break;
      }
    }
  }

  if (changed) {
    std::string error;
    if (!deck.save(&error)) {
      errors << "FlashTerm: could not save " << deck.path() << ": " << error
             << "\n";
      ++result.failed;
    }
  }

  out << "\n"
      << result.rendered << " rendered, " << result.skipped << " skipped, "
      << result.failed << " failed\n";
  return result;
}
}  // namespace FlashTerm
