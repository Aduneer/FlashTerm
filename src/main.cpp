#include <clocale>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

#include "cli.h"
#include "date.h"
#include "deck.h"
#include "review.h"
#include "terminal.h"
#include "ui.h"

using namespace FlashTerm;

namespace {
const char kDefaultDeck[] = "flashcards.txt";

void print_main_menu(const Deck& deck) {
  const int due = deck.due_count(today());
  std::cout << color::cyan << "1. Add flashcard\n"
            << "2. Review flashcards";
  if (due > 0) {
    std::cout << color::yellow << "  (" << due << " due)" << color::cyan;
  }
  std::cout << "\n"
            << "3. Manage flashcards\n"
            << "4. Display progress\n"
            << "5. Import flashcards\n"
            << "6. Export flashcards\n"
            << "7. List unique Tags\n"
            << color::reset << color::yellow << "0. Save and exit\n"
            << "h/? Help\n"
            << "Choose: " << color::reset;
}

void run_import(Deck& deck) {
  const std::string path = prompt("Enter import file path: ");
  const ImportResult result = import_into(deck, path);
  if (!result.ok) {
    std::cout << color::red << "Import failed: " << result.error << "\n\n"
              << color::reset;
    return;
  }
  autosave(deck);
  std::cout << color::green << "Imported " << result.imported
            << " flashcards from " << path << color::reset << "\n\n";
}

void run_export(const Deck& deck) {
  const std::string path = prompt("Enter export file path: ");
  std::string error;
  if (!export_deck(deck, path, &error)) {
    std::cout << color::red << "Export failed: " << error << "\n\n"
              << color::reset;
    return;
  }
  std::cout << color::green << "Exported " << deck.size() << " flashcards to "
            << path << color::reset << "\n\n";
}
}  // namespace

int main(int argc, char* argv[]) {
  const CliOptions options = parse_args(
      argc, argv, deck_from_env(std::getenv("FLASHTERM_DECK"), kDefaultDeck));
  switch (options.action) {
    case CliAction::ShowHelp:
      std::cout << usage_text();
      return 0;
    case CliAction::ShowVersion:
      std::cout << "FlashTerm " << kVersion << "\n";
      return 0;
    case CliAction::Error:
      std::cerr << "FlashTerm: " << options.error << "\n\n" << usage_text();
      return 2;
    case CliAction::RunDeck:
      break;
  }

  // Required before text widths can account for multi-byte characters.
  std::setlocale(LC_ALL, "");
  color::detect();

  Deck deck(options.deck_path);
  if (!deck.load()) {
    std::cout << color::yellow << "Creating a new flashcard deck: "
              << deck.path() << color::reset << "\n\n";
  } else {
    std::cout << color::green << "Loaded " << deck.size()
              << " flashcards from " << deck.path() << color::reset << "\n\n";
  }

  try {
    while (true) {
      print_main_menu(deck);
      std::string input;
      read_line(input);
      clear_screen();

      if (input == "1") {
        add_flashcard(deck);
      } else if (input == "2") {
        review_flashcards(deck);
      } else if (input == "3") {
        manage_flashcards(deck);
      } else if (input == "4") {
        display_progress(deck);
      } else if (input == "5") {
        run_import(deck);
      } else if (input == "6") {
        run_export(deck);
      } else if (input == "7") {
        list_unique_tags(deck);
      } else if (input == "0") {
        std::string error;
        if (!deck.save(&error)) {
          std::cout << color::red << "Could not save deck: " << error << "\n"
                    << color::reset;
          return 1;
        }
        std::cout << color::green << "Flashcards saved. Goodbye!\n"
                  << color::reset;
        break;
      } else if (input == "h" || input == "?") {
        print_help();
      } else {
        std::cout << color::red << "Invalid choice. Please try again.\n"
                  << color::reset;
      }
    }
  } catch (const std::exception&) {
    // End of input: the deck is already saved after every change, but save
    // once more so a partially entered card is not the only thing lost.
    std::string error;
    if (!deck.save(&error)) {
      std::cout << color::red << "\nCould not save deck: " << error << "\n"
                << color::reset;
      return 1;
    }
    std::cout << color::green
              << "\nInput stream closed/EOF. Flashcards saved. Goodbye!\n"
              << color::reset;
  }
  return 0;
}
