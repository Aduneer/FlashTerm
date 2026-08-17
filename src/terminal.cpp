#include "terminal.h"

#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

namespace FlashTerm {
namespace {
bool stdout_is_tty() { return isatty(STDOUT_FILENO) != 0; }

// Queries one field of the window size. Returns 0 when it cannot be read, so
// callers can apply their own fallback.
int window_size(bool want_rows) {
  if (!stdout_is_tty()) return 0;

  struct winsize w;
  // ioctl leaves w untouched on failure, so its result must be checked before
  // ws_col or ws_row is read.
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) != 0) return 0;
  return want_rows ? w.ws_row : w.ws_col;
}
}  // namespace

namespace color {
namespace {
struct Palette {
  const char* name;
  const char* red;
  const char* green;
  const char* yellow;
  const char* cyan;
};

// `default` stays plain ANSI so that terminals with only eight colours, and
// anyone who liked the old look, are unaffected. The other two are 256-colour,
// which every terminal worth theming has supported for years.
constexpr Palette kPalettes[] = {
    {"default", "\033[31m", "\033[32m", "\033[33m", "\033[36m"},
    // Cool: coral for errors so they still read as errors, against seafoam,
    // sky and deep teal.
    {"ocean", "\033[38;5;210m", "\033[38;5;79m", "\033[38;5;117m",
     "\033[38;5;38m"},
    // Warm: salmon, gold, peach and a dusty rose for headings.
    {"sunset", "\033[38;5;203m", "\033[38;5;179m", "\033[38;5;215m",
     "\033[38;5;211m"},
};

const Palette& palette_for(const char* requested) {
  if (requested != nullptr) {
    for (const Palette& palette : kPalettes) {
      if (std::strcmp(palette.name, requested) == 0) return palette;
    }
  }
  // An unrecognised theme falls back rather than complaining: a typo in a
  // shell profile should not put a message in front of every single run.
  return kPalettes[0];
}
}  // namespace

const char* reset = "\033[0m";
const char* red = "\033[31m";
const char* green = "\033[32m";
const char* yellow = "\033[33m";
const char* cyan = "\033[36m";

void detect() {
  if (!stdout_is_tty() || std::getenv("NO_COLOR") != nullptr) {
    reset = red = green = yellow = cyan = "";
    return;
  }

  const Palette& palette = palette_for(std::getenv("FLASHTERM_THEME"));
  red = palette.red;
  green = palette.green;
  yellow = palette.yellow;
  cyan = palette.cyan;
}

const char* theme_names() { return "default, ocean, sunset"; }
}  // namespace color

void clear_screen() {
  if (!stdout_is_tty()) return;
  // Cursor home, erase screen, erase scrollback. Avoids forking a shell.
  std::cout << "\033[H\033[2J\033[3J" << std::flush;
}

int terminal_width() {
  const int columns = window_size(false);
  if (columns <= 0) return 80;
  return std::max(40, columns);
}

int terminal_height() {
  const int rows = window_size(true);
  if (rows <= 0) return 24;
  // Below this there is nothing sensible to centre within, and callers simply
  // stop padding.
  return std::max(10, rows);
}

bool interactive() {
  return isatty(STDIN_FILENO) != 0 && isatty(STDOUT_FILENO) != 0;
}

int read_key() {
  termios original{};
  if (tcgetattr(STDIN_FILENO, &original) != 0) return kEndOfInput;

  termios raw = original;
  // ICANON off delivers the byte without waiting for Enter, ECHO off keeps the
  // keystroke from being printed, and ISIG off means Ctrl+C arrives as data
  // instead of as a signal that could kill us mid-raw-mode.
  raw.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO | ISIG));
  raw.c_cc[VMIN] = 1;   // block until exactly one byte is available
  raw.c_cc[VTIME] = 0;  // with no timeout
  if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) return kEndOfInput;

  // Read through C stdin rather than the file descriptor. std::cin is tied to
  // the same stdin buffer while sync_with_stdio is on, which it is by default
  // and this depends on: keys typed ahead during a line prompt are already
  // sitting in that buffer, and a raw read() on the descriptor would never
  // find them. Going through getchar means one buffer and no lost keystrokes.
  int byte = EOF;
  do {
    std::clearerr(stdin);
    errno = 0;
    byte = std::getchar();
    // A window resize interrupts the read. That is not the user typing
    // anything, and certainly not end of input, so wait again rather than
    // quitting the session because someone dragged a window edge.
  } while (byte == EOF && errno == EINTR);

  tcsetattr(STDIN_FILENO, TCSANOW, &original);

  if (byte == EOF) return kEndOfInput;
  if (byte == 0x03 || byte == 0x04) return kEndOfInput;  // Ctrl+C, Ctrl+D
  return byte;
}
}  // namespace FlashTerm
