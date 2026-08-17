#include <clocale>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>

#include "cli.h"
#include "date.h"
#include "deck.h"
#include "generate.h"
#include "review.h"
#include "terminal.h"
#include "text.h"
#include "ui.h"

using namespace FlashTerm;

namespace {
const char kDefaultDeck[] = "flashcards.txt";

// The deck's own name in the title, because studying more than one deck is the
// normal case once FLASHTERM_DECK exists and "which deck am I in" should not
// need a guess.
std::string deck_title(const Deck& deck) {
  const std::string& path = deck.path();
  const std::size_t slash = path.find_last_of('/');
  const std::string name =
      (slash == std::string::npos) ? path : path.substr(slash + 1);
  return "FlashTerm · " + name;
}

void print_main_menu(const Deck& deck) {
  const int due = deck.due_count(today());
  std::string review = "Review flashcards";
  if (due > 0) {
    review += std::string(color::yellow) + "  (" + std::to_string(due) +
              " due)" + color::reset;
  }

  print_menu(deck_title(deck), {{"1", "Add flashcard"},
                                {"2", review},
                                {"3", "Manage flashcards"},
                                {"4", "Display progress"},
                                {"5", "Import flashcards"},
                                {"6", "Export flashcards"},
                                {"7", "List unique tags"},
                                {"h", "Help"},
                                {"0", "Save and exit"}});
  print_prompt();
}

// Empty means "changed my mind": a menu you cannot back out of is a trap, and
// trying to open a file called "" only produces a confusing error.
std::string ask_for_path(const char* purpose) {
  std::cout << color::cyan << "\n--- " << purpose << " ---\n"
            << color::reset << "Path to the file, or Enter to cancel.\n";
  print_prompt();
  std::string path;
  read_line(path);
  return trim(path);
}

void run_import(Deck& deck) {
  const std::string path = ask_for_path("Import Flashcards");
  if (path.empty()) {
    std::cout << color::yellow << "Import cancelled.\n\n" << color::reset;
    return;
  }

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
  const std::string path = ask_for_path("Export Flashcards");
  if (path.empty()) {
    std::cout << color::yellow << "Export cancelled.\n\n" << color::reset;
    return;
  }

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
    case CliAction::GenerateAudio:
    case CliAction::RunDeck:
      break;
  }

  // Required before text widths can account for multi-byte characters.
  std::setlocale(LC_ALL, "");
  color::detect();

  if (options.action == CliAction::GenerateAudio) {
    Deck deck(options.deck_path);
    // Refusing beats creating an empty deck and reporting nothing to do: the
    // likely cause is a mistyped path, and generating is not how a deck is
    // meant to come into existence.
    if (!deck.load()) {
      std::cerr << "FlashTerm: no deck at " << deck.path() << "\n";
      return 2;
    }
    std::cout << "Rendering audio for "
              << count_label(static_cast<int>(deck.size()), "card", "cards")
              << " in " << deck.path() << "\n";
    return generate_audio(deck, options.force, std::cout, std::cerr)
        .exit_code();
  }

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
      const std::string input = read_choice();
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
