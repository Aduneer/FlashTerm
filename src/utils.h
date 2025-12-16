// utils.h

#pragma once
#include "flashcard.h"
#include <iostream>
#include <string>
#include <vector>

// ANSI color codes
#define COLOR_GREEN "\033[32m"
#define COLOR_RED "\033[31m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_CYAN "\033[36m"
#define COLOR_RESET "\033[0m"

std::vector<Flashcard> load_flashcards(const std::string &filename);
void save_flashcards(const std::string &filename,
                     const std::vector<Flashcard> &cards);

void import_flashcards(std::vector<Flashcard> &cards,
                       const std::string &filename);

void export_flashcards(const std::vector<Flashcard> &cards,
                       const std::string &filename);

// NEW: Centralized utility function for splitting strings
std::vector<std::string> split_string_by_delimiter(const std::string &str,
                                                   char delimiter);
