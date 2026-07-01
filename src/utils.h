#pragma once
#include <iostream>
#include <string>
#include <vector>

#include "flashcard.h"

namespace FlashTerm {
// ANSI color codes
#define COLOR_GREEN "\033[32m"
#define COLOR_RED "\033[31m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_CYAN "\033[36m"
#define COLOR_RESET "\033[0m"

std::vector<Flashcard> load_flashcards(const std::string& filename);
void save_flashcards(const std::string& filename,
                     const std::vector<Flashcard>& cards);

void import_flashcards(std::vector<Flashcard>& cards,
                       const std::string& filename);

void export_flashcards(const std::vector<Flashcard>& cards,
                       const std::string& filename);

std::vector<std::string> split_string_by_delimiter(const std::string& str,
                                                   char delimiter);

std::string trim(const std::string& str);
std::string to_lowercase(const std::string& str);
int levenshtein_distance(const std::string& s1, const std::string& s2);
std::string escape_csv_field(const std::string& field);
std::vector<std::string> parse_csv_line(const std::string& line);

int get_terminal_width();
}  // namespace FlashTerm
