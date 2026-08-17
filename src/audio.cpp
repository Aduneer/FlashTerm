#include "audio.h"

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <sstream>
#include <vector>

namespace FlashTerm {
namespace audio {
namespace {
using Command = std::vector<std::string>;

// The candidates, in the order they are tried. Nothing here is a build
// dependency: the binary links against none of it, asks the PATH at runtime,
// and does without if the answer is nothing. A deck that points at no files
// and a machine with no synthesiser simply never offers the key.
const std::vector<Command>& speakers() {
  static const std::vector<Command> candidates = {
      {"espeak-ng"}, {"espeak"}, {"say"},  // say is macOS
      {"flite", "-t"},
  };
  return candidates;
}

// Quiet flags throughout: a player that draws a progress meter would draw it
// over the card. The child's output is sent to /dev/null as well, since not
// every one of these can be talked out of printing something.
const std::vector<Command>& players() {
  static const std::vector<Command> candidates = {
      {"mpv", "--no-video", "--really-quiet"},
      {"ffplay", "-nodisp", "-autoexit", "-loglevel", "quiet"},
      {"paplay"},
      {"pw-play"},
      {"mpg123", "-q"},
      {"afplay"},  // macOS
      {"aplay", "-q"},
  };
  return candidates;
}

bool is_executable(const std::string& path) {
  return access(path.c_str(), X_OK) == 0;
}

std::string find_in_path(const std::string& program) {
  if (program.find('/') != std::string::npos) {
    return is_executable(program) ? program : std::string();
  }
  const char* path = std::getenv("PATH");
  if (path == nullptr) return {};

  std::stringstream directories(path);
  std::string directory;
  while (std::getline(directories, directory, ':')) {
    if (directory.empty()) continue;
    const std::string candidate = directory + "/" + program;
    if (is_executable(candidate)) return candidate;
  }
  return {};
}

// An override is split on whitespace and used as-is, so FLASHTERM_TTS="espeak-ng
// -v fr" picks a voice. Splitting rather than handing it to a shell is the
// point: see run() for why nothing here ever reaches one. An argument
// containing a space cannot be expressed, which is the price.
Command command_from_environment(const char* variable) {
  const char* value = std::getenv(variable);
  if (value == nullptr) return {};

  Command command;
  std::stringstream words(value);
  std::string word;
  while (words >> word) command.push_back(word);
  return command;
}

// The override wins if it names something runnable; a typo in it falls through
// to the built-in list rather than silently turning audio off.
Command choose(const char* variable, const std::vector<Command>& candidates) {
  Command override = command_from_environment(variable);
  if (!override.empty() && !find_in_path(override[0]).empty()) {
    return override;
  }
  for (const Command& candidate : candidates) {
    if (!find_in_path(candidate[0]).empty()) return candidate;
  }
  return {};
}

Command speaker() { return choose("FLASHTERM_TTS", speakers()); }
Command player() { return choose("FLASHTERM_PLAYER", players()); }

// fork and exec rather than system(), because the last argument is deck
// content. A card whose question is `x; rm -rf ~` is a card about shell
// quoting, and it has to stay one: passed as an argv element it can never be
// anything but a string, whereas pasted into a command line for /bin/sh it is
// exactly what it looks like.
bool run(const Command& command, const std::string& final_argument) {
  if (command.empty()) return false;

  std::vector<char*> argv;
  argv.reserve(command.size() + 2);
  for (const std::string& word : command) {
    argv.push_back(const_cast<char*>(word.c_str()));
  }
  argv.push_back(const_cast<char*>(final_argument.c_str()));
  argv.push_back(nullptr);

  const pid_t pid = fork();
  if (pid < 0) return false;
  if (pid == 0) {
    // Detached from the terminal in both directions: the child must not read
    // the keystrokes meant for the review, and must not write over the card.
    const int null = open("/dev/null", O_RDWR);
    if (null >= 0) {
      dup2(null, STDIN_FILENO);
      dup2(null, STDOUT_FILENO);
      dup2(null, STDERR_FILENO);
      if (null > STDERR_FILENO) close(null);
    }
    execvp(argv[0], argv.data());
    _exit(127);  // reached only if exec failed; the parent reads it as failure
  }

  int status = 0;
  while (waitpid(pid, &status, 0) < 0) {
    // A window resize during playback is a signal, not a reason to stop
    // waiting and leave the child running into the next card.
    if (errno != EINTR) return false;
  }
  return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

// Same as run(), but the text goes to the child on standard input and the
// arguments are used as given rather than having one appended. Rendering needs
// both: where to write is an argument, what to say is not.
bool run_with_input(const Command& command, const std::string& input) {
  if (command.empty()) return false;

  int pipe_fds[2];
  if (pipe(pipe_fds) != 0) return false;

  std::vector<char*> argv;
  argv.reserve(command.size() + 1);
  for (const std::string& word : command) {
    argv.push_back(const_cast<char*>(word.c_str()));
  }
  argv.push_back(nullptr);

  const pid_t pid = fork();
  if (pid < 0) {
    close(pipe_fds[0]);
    close(pipe_fds[1]);
    return false;
  }
  if (pid == 0) {
    close(pipe_fds[1]);
    dup2(pipe_fds[0], STDIN_FILENO);
    close(pipe_fds[0]);
    // Rendering is a batch operation with its own progress output, so the
    // synthesiser's chatter is suppressed the same way playback's is.
    const int null = open("/dev/null", O_WRONLY);
    if (null >= 0) {
      dup2(null, STDOUT_FILENO);
      dup2(null, STDERR_FILENO);
      if (null > STDERR_FILENO) close(null);
    }
    execvp(argv[0], argv.data());
    _exit(127);
  }

  close(pipe_fds[0]);
  // A child that exits before reading turns the write below into SIGPIPE,
  // which would kill a batch run partway through for what is only a failed
  // card. Ignored here and restored after, so the failure arrives as a return
  // value like every other.
  void (*previous)(int) = signal(SIGPIPE, SIG_IGN);
  std::size_t written = 0;
  while (written < input.size()) {
    const ssize_t chunk =
        write(pipe_fds[1], input.data() + written, input.size() - written);
    if (chunk < 0) {
      if (errno == EINTR) continue;
      break;
    }
    written += static_cast<std::size_t>(chunk);
  }
  close(pipe_fds[1]);
  signal(SIGPIPE, previous);

  int status = 0;
  while (waitpid(pid, &status, 0) < 0) {
    if (errno != EINTR) return false;
  }
  return WIFEXITED(status) && WEXITSTATUS(status) == 0 &&
         written == input.size();
}

// "{out}" is replaced wherever it appears, so the placeholder can sit inside a
// larger argument ("--output={out}") as well as be one on its own.
Command substitute(const Command& command, const std::string& output_path) {
  Command result;
  result.reserve(command.size());
  for (std::string word : command) {
    const std::string token = "{out}";
    for (std::size_t at = word.find(token); at != std::string::npos;
         at = word.find(token, at + output_path.size())) {
      word.replace(at, token.size(), output_path);
    }
    result.push_back(word);
  }
  return result;
}

}  // namespace

bool available() { return !speaker().empty() || !player().empty(); }

std::string speaker_name() {
  const Command command = speaker();
  return command.empty() ? std::string() : command[0];
}

std::string player_name() {
  const Command command = player();
  return command.empty() ? std::string() : command[0];
}

Command render_command_from_environment() {
  const Command command = command_from_environment("FLASHTERM_TTS_RENDER");
  return runnable(command) ? command : Command();
}

bool runnable(const Command& command) {
  return !command.empty() && !find_in_path(command[0]).empty();
}

bool render(const Command& command, const std::string& text,
            const std::string& output_path) {
  if (command.empty() || text.empty()) return false;
  return run_with_input(substitute(command, output_path), text);
}

bool play(const std::string& file, const std::string& text) {
  // A recording beats a synthesiser, but only if it is really there: a deck
  // that has been copied without its audio directory should still speak rather
  // than fall silent with no explanation.
  if (!file.empty() && access(file.c_str(), R_OK) == 0) {
    if (run(player(), file)) return true;
  }
  return !text.empty() && run(speaker(), text);
}
}  // namespace audio
}  // namespace FlashTerm
