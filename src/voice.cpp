#include "voice.h"

#include <dirent.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>

#include <algorithm>
#include <cstdlib>
#include <sstream>

namespace FlashTerm {
namespace voice {
namespace {
constexpr char kModelSuffix[] = ".onnx";

std::string home_directory() {
  const char* home = std::getenv("HOME");
  return (home == nullptr) ? std::string() : home;
}

// Resolves a program on the PATH to its real location, following symlinks. What
// this is for: pipx puts `piper` in ~/.local/bin as a link into the virtual
// environment it built, and that environment is where the downloader lives.
std::string resolve_program(const std::string& program) {
  const char* path = std::getenv("PATH");
  if (path == nullptr) return {};

  std::stringstream directories(path);
  std::string directory;
  while (std::getline(directories, directory, ':')) {
    if (directory.empty()) continue;
    const std::string candidate = directory + "/" + program;
    if (access(candidate.c_str(), X_OK) != 0) continue;

    char resolved[PATH_MAX];
    if (realpath(candidate.c_str(), resolved) != nullptr) return resolved;
    return candidate;
  }
  return {};
}

// The interpreter that can import piper, which is the one sitting beside the
// piper executable once its symlinks have been followed. Empty when piper is
// not installed, or is installed somewhere without a Python next to it.
std::string piper_python() {
  const std::string program = resolve_program("piper");
  if (program.empty()) return {};

  const std::size_t slash = program.find_last_of('/');
  if (slash == std::string::npos) return {};
  const std::string directory = program.substr(0, slash);

  for (const char* name : {"/python3", "/python"}) {
    const std::string candidate = directory + name;
    if (access(candidate.c_str(), X_OK) == 0) return candidate;
  }
  return {};
}

std::vector<std::string> models_in(const std::string& directory) {
  std::vector<std::string> names;
  DIR* handle = opendir(directory.c_str());
  if (handle == nullptr) return names;

  const std::size_t suffix_length = sizeof(kModelSuffix) - 1;
  while (const dirent* entry = readdir(handle)) {
    const std::string name = entry->d_name;
    if (name.size() <= suffix_length) continue;
    if (name.compare(name.size() - suffix_length, suffix_length,
                     kModelSuffix) != 0) {
      continue;
    }
    names.push_back(name.substr(0, name.size() - suffix_length));
  }
  closedir(handle);
  return names;
}
}  // namespace

std::vector<std::string> search_paths() {
  std::vector<std::string> paths;

  // First, and separately, because it is how the tests pin this down and how
  // someone with voices in an unusual place says so.
  const char* configured = std::getenv("FLASHTERM_VOICES");
  if (configured != nullptr && *configured != '\0') {
    std::stringstream directories(configured);
    std::string directory;
    while (std::getline(directories, directory, ':')) {
      if (!directory.empty()) paths.push_back(directory);
    }
  }

  const std::string home = home_directory();
  if (!home.empty()) {
    paths.push_back(home + "/.local/share/piper-voices");
    paths.push_back(home + "/.local/share/piper/voices");
    paths.push_back(home + "/.cache/piper");
  }
  return paths;
}

std::vector<std::string> installed() {
  std::vector<std::string> names;
  for (const std::string& directory : search_paths()) {
    const std::vector<std::string> found = models_in(directory);
    names.insert(names.end(), found.begin(), found.end());
  }
  std::sort(names.begin(), names.end());
  names.erase(std::unique(names.begin(), names.end()), names.end());
  return names;
}

std::string model_path(const std::string& name) {
  if (name.empty()) return {};

  // A path is used as given, so a voice kept outside the usual places is still
  // reachable without moving it.
  if (name.find('/') != std::string::npos) {
    return (access(name.c_str(), R_OK) == 0) ? name : std::string();
  }

  for (const std::string& directory : search_paths()) {
    const std::string candidate = directory + "/" + name + kModelSuffix;
    if (access(candidate.c_str(), R_OK) == 0) return candidate;
  }
  return {};
}

audio::Command render_command(const std::string& name) {
  const std::string model = model_path(name);
  if (model.empty()) return {};

  const audio::Command command = {"piper", "-m", model, "-f", "{out}"};
  return audio::runnable(command) ? command : audio::Command();
}

std::string setup_instructions(const std::string& wanted_voice,
                               const std::string& deck) {
  const std::string voice =
      wanted_voice.empty() ? "fr_FR-siwis-medium" : wanted_voice;
  const std::string python = piper_python();
  std::string text;

  if (python.empty()) {
    text +=
        "Piper is not installed. It is a neural text-to-speech engine that runs\n"
        "offline, and it is what makes a language deck sound like a person:\n"
        "\n"
        "  pipx install piper-tts\n"
        "\n"
        "Then download a voice and try again. Run --generate-audio once more\n"
        "afterwards and it will print the exact download command for you.\n";
    return text;
  }

  const std::vector<std::string> have = installed();
  if (have.empty()) {
    text += "Piper is installed but has no voices yet. To download one:\n\n  ";
    text += python + " -m piper.download_voices " + voice + " \\\n";
    text += "    --download-dir " + (home_directory().empty()
                                         ? std::string("piper-voices")
                                         : home_directory() +
                                               "/.local/share/piper-voices");
    text +=
        "\n\nVoices are named language_REGION-speaker-quality, so es_ES, de_DE,\n"
        "it_IT and about thirty more exist. List them all with:\n\n  ";
    text += python + " -m piper.download_voices --help\n";
    return text;
  }

  text += "Installed voices:\n\n";
  for (const std::string& name : have) text += "  " + name + "\n";
  text += "\nUse one with:\n\n  FlashTerm " + deck +
          " --generate-audio --voice " + have.front() + "\n";
  if (!wanted_voice.empty()) {
    text += "\nTo download " + wanted_voice + " as well:\n\n  ";
    text += python + " -m piper.download_voices " + wanted_voice + " \\\n";
    text += "    --download-dir " + (home_directory().empty()
                                         ? std::string("piper-voices")
                                         : home_directory() +
                                               "/.local/share/piper-voices");
    text += "\n";
  }
  return text;
}
}  // namespace voice
}  // namespace FlashTerm
