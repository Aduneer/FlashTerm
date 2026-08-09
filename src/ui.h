#pragma once
#include <string>

#include "deck.h"

namespace FlashTerm {
// Throws std::runtime_error at end of input so callers can save and exit.
void read_line(std::string& out);
std::string prompt(const std::string& message);
// Returns -1 when the input is not a number.
int read_int();

// Persists the deck immediately, reporting any failure rather than losing it.
void autosave(const Deck& deck);

void add_flashcard(Deck& deck);
void manage_flashcards(Deck& deck);
void list_flashcards(const Deck& deck);
void list_unique_tags(const Deck& deck);
void display_progress(const Deck& deck);
void print_help();
}  // namespace FlashTerm
