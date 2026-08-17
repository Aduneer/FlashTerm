#pragma once

namespace FlashTerm {
namespace color {
// Empty strings when colour is disabled, so they stay safe to stream anywhere.
// The names are the roles they are used for, not promises about the exact hue:
// a theme repaints all four, so `cyan` is "the heading colour" and `red` is
// "the something-went-wrong colour".
extern const char* reset;
extern const char* red;
extern const char* green;
extern const char* yellow;
extern const char* cyan;

// Chooses the palette from $FLASHTERM_THEME and disables colour entirely when
// stdout is not a terminal or NO_COLOR is set. NO_COLOR wins over any theme.
void detect();

// Theme names accepted by $FLASHTERM_THEME, for --help and error messages.
const char* theme_names();
}  // namespace color

// No-op when stdout is not a terminal, so piped output stays readable.
void clear_screen();

// Always return usable sizes, falling back to 80x24 when they cannot be
// queried, which is also what piped output gets.
int terminal_width();
int terminal_height();

// True when stdin and stdout are both terminals. Single-keypress input needs a
// stdin it can put in raw mode, and there is no point echoing a menu to
// something that is not a screen. Everything falls back to line input when
// this is false, so piping the app keeps working exactly as it always has.
bool interactive();

// What read_key() returns when there is nothing more to read: real end of
// input, Ctrl+D, or Ctrl+C. Callers treat all three the same way, which is to
// save and exit.
constexpr int kEndOfInput = -1;

// One keypress, without waiting for Enter. Enter itself comes back as '\n'.
//
// Raw mode is entered and left around this single read rather than held for
// the whole session, so there is no window in which an unexpected exit could
// leave the user's shell without echo. Ctrl+C is read as a character rather
// than raised as a signal for the same reason: it can then take the ordinary
// save-and-exit path instead of killing the process with the terminal
// half-configured.
//
// Reads through C stdin, which std::cin shares while sync_with_stdio is on --
// so a key typed ahead during a line prompt is still found here rather than
// stranded in a buffer nobody else looks at. Do not turn sync_with_stdio off
// without revisiting this.
//
// Only call this when interactive() is true, and flush std::cout first: this
// bypasses the stream layer, so nothing else will push a pending prompt to the
// screen before it blocks.
int read_key();
}  // namespace FlashTerm
