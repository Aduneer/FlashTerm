#pragma once
#include <string>
#include <vector>

#include "deck.h"

namespace FlashTerm {
// Throws std::runtime_error at end of input so callers can save and exit.
void read_line(std::string& out);
std::string prompt(const std::string& message);
// Returns -1 when the input is not a number.
int read_int();

// A menu choice: one keypress when the session is interactive, one whole line
// when input is piped. Enter comes back as "", so `""` means "the default".
//
// Falling back to line input is what keeps the app scriptable — a piped
// session, the demo recording and the test suite all still drive it exactly as
// before. Only use this where every choice is a single character; anything the
// user might type more than one character of stays on read_line.
//
// Throws std::runtime_error at end of input, like read_line.
std::string read_choice();

struct KeyHint {
  const char* key;
  const char* action;
};

// "[Enter] next card   [e] edit   [u] undo", with the keys picked out in
// colour. Every screen that accepts keys builds its footer from this, so the
// same key is taught the same way wherever it appears — which matters more now
// that a keypress acts immediately and there is no Enter to take it back.
// Renders "[Enter] submit   [?] hint" and wraps between hints when they will
// not fit, rather than leaving the terminal to break a word in half.
//
// `width` of 0 means ask the terminal, which is what every caller wants. It is
// a parameter at all so that the wrapping can be tested at a width the test
// chooses: reading the real terminal would make the test pass in a terminal
// and fail in a pipe.
std::string legend(const std::vector<KeyHint>& hints, std::size_t width = 0);

struct MenuItem {
  std::string key;
  std::string label;
};

// A titled menu of single keypresses, one per line, in the same "[k] label"
// shape the review legends use. Menus render through here rather than each
// building its own list, so a key looks the same wherever the user meets it.
//
// Labels are built by the caller and may carry colour of their own, which is
// how the main menu highlights its due count.
void print_menu(const std::string& title, const std::vector<MenuItem>& items);

// The "> " every menu and key prompt ends with. One cursor position to learn:
// wherever this appears, a single keypress is what moves things along.
void print_prompt();

// Persists the deck immediately, reporting any failure rather than losing it.
void autosave(const Deck& deck);

// Prompts for question, answer and tags, keeping the current value whenever
// the reply is empty. Shared by the manage menu and mid-review editing.
void edit_card_fields(Flashcard& card);

void add_flashcard(Deck& deck);
void manage_flashcards(Deck& deck);
void list_flashcards(const Deck& deck);
void list_unique_tags(const Deck& deck);
void display_progress(const Deck& deck);
void print_help();
}  // namespace FlashTerm
