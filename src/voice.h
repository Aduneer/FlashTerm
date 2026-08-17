#pragma once
#include <string>
#include <vector>

#include "audio.h"

namespace FlashTerm {
// Finding piper voices on disk, so that --generate-audio can be given a name
// rather than a command line.
//
// This is the one place FlashTerm knows anything about a specific synthesiser,
// and it is a convenience rather than a coupling: FLASHTERM_TTS_RENDER still
// takes any command at all and still wins when it is set. What is bought for
// that is the difference between
//
//   FLASHTERM_TTS_RENDER="piper -m ~/.local/share/piper-voices/fr_FR-siwis-medium.onnx -f {out}"
//
// and `--voice fr_FR-siwis-medium`, which is roughly the difference between a
// tool someone will use and one they will not.
namespace voice {
// Directories searched, in order: $FLASHTERM_VOICES first if it is set (a
// colon-separated list, like PATH), then the places piper's own downloader
// writes to.
std::vector<std::string> search_paths();

// Every voice found, by name and without the .onnx, sorted and deduplicated.
// Empty is the normal state before anything has been downloaded.
std::vector<std::string> installed();

// Full path to a named voice's model, or empty if it is not downloaded.
std::string model_path(const std::string& name);

// The command that would render with a named voice, or empty when the voice is
// not installed or piper is not on the PATH. The model is passed by full path
// rather than by name, so it does not matter which directory it was found in.
audio::Command render_command(const std::string& name);

// What to tell someone who has no voice yet, worked out rather than guessed:
// whether piper is installed at all, and if it is, the interpreter that can
// actually run its downloader. That last part matters because pipx puts piper
// in its own virtual environment, so the obvious `python3 -m
// piper.download_voices` fails with a bare ModuleNotFoundError -- which is a
// miserable thing to hand somebody who only wanted to hear a word.
// `deck` appears in the example command so that it can be copied and run as
// printed, rather than being a template to fill in.
std::string setup_instructions(const std::string& wanted_voice,
                               const std::string& deck);
}  // namespace voice
}  // namespace FlashTerm
