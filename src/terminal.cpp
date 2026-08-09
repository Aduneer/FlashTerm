#include "terminal.h"

#include <sys/ioctl.h>
#include <unistd.h>

#include <algorithm>
#include <cstdlib>
#include <iostream>

namespace FlashTerm {
namespace {
bool stdout_is_tty() { return isatty(STDOUT_FILENO) != 0; }
}  // namespace

namespace color {
const char* reset = "\033[0m";
const char* red = "\033[31m";
const char* green = "\033[32m";
const char* yellow = "\033[33m";
const char* cyan = "\033[36m";

void detect() {
  const bool enabled = stdout_is_tty() && std::getenv("NO_COLOR") == nullptr;
  if (enabled) return;
  reset = red = green = yellow = cyan = "";
}
}  // namespace color

void clear_screen() {
  if (!stdout_is_tty()) return;
  // Cursor home, erase screen, erase scrollback. Avoids forking a shell.
  std::cout << "\033[H\033[2J\033[3J" << std::flush;
}

int terminal_width() {
  const int kFallback = 80;
  const int kMinimum = 40;

  if (!stdout_is_tty()) return kFallback;

  struct winsize w;
  // ioctl leaves w untouched on failure, so its result must be checked
  // before ws_col is read.
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) != 0 || w.ws_col == 0) {
    return kFallback;
  }
  return std::max(kMinimum, static_cast<int>(w.ws_col));
}
}  // namespace FlashTerm
