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

std::vector<std::string> split(const std::string& str, char delimiter);
int levenshtein_distance(const std::string& s1, const std::string& s2);

std::string escape_csv_field(const std::string& field);
std::vector<std::string> parse_csv_line(const std::string& line);

// Column helpers measure terminal columns rather than bytes, so a card
// containing accents or CJK still lines up in the statistics table. Wide
// glyphs count as two columns; this needs setlocale(LC_ALL, "") at startup.
std::size_t display_width(const std::string& str);
std::string truncate(const std::string& str, std::size_t max_width);
std::string pad_right(const std::string& str, std::size_t width);
}  // namespace FlashTerm
