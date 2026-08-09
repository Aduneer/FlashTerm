#pragma once

namespace FlashTerm {
namespace color {
// Empty strings when colour is disabled, so they stay safe to stream anywhere.
extern const char* reset;
extern const char* red;
extern const char* green;
extern const char* yellow;
extern const char* cyan;

// Disables colour when stdout is not a terminal or NO_COLOR is set.
void detect();
}  // namespace color

// No-op when stdout is not a terminal, so piped output stays readable.
void clear_screen();

// Always returns a usable width, falling back to 80 when it cannot be queried.
int terminal_width();
}  // namespace FlashTerm
