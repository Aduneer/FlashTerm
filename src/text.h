#pragma once
#include <cstddef>
#include <string>
#include <vector>

namespace FlashTerm {
std::string trim(const std::string& str);
std::string to_lowercase(const std::string& str);

// Trims, lowercases and strips all whitespace: the form answers are compared in.
std::string normalize_answer(const std::string& str);

// "1 card" / "2 cards". Counts are printed in enough places that "(1 cards)"
// shows up on screen regularly, so the agreement is done in one place.
std::string count_label(int count, const std::string& singular,
                        const std::string& plural);

// The last line with anything on it, trailing blank lines ignored. What it is
// for: a command that fails writes its explanation to standard error at
// length, and the last line is the one worth repeating -- a Python traceback
// says which file and which function on every line but the one that names the
// error.
std::string last_nonempty_line(const std::string& text);

std::vector<std::string> split(const std::string& str, char delimiter);
int levenshtein_distance(const std::string& s1, const std::string& s2);

std::string escape_csv_field(const std::string& field);
std::vector<std::string> parse_csv_line(const std::string& line);

// Column helpers measure terminal columns rather than bytes, so a card
// containing accents or CJK still lines up in the statistics table. Wide
// glyphs count as two columns; this needs setlocale(LC_ALL, "") at startup.
// Bytes in the UTF-8 code point starting at `index`, so callers can walk a
// string one character at a time instead of one byte at a time.
std::size_t utf8_char_bytes(const std::string& str, std::size_t index);

std::size_t display_width(const std::string& str);
std::string truncate(const std::string& str, std::size_t max_width);
std::string pad_right(const std::string& str, std::size_t width);

// Breaks text into lines no wider than `width` columns, preferring spaces.
// Measured in columns rather than bytes, so a framed card built from these
// lines still lines up when the text is accented or CJK.
//
// A word longer than the whole width — a URL, or unspaced Japanese — is split
// at the column limit rather than allowed to overflow, because a box that a
// long word breaks out of is worse than one that hyphenlessly wraps it.
// Returns a single empty line for empty input, so callers always have
// something to draw.
std::vector<std::string> wrap(const std::string& str, std::size_t width);
}  // namespace FlashTerm
