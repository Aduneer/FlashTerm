#include "ui.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <stdexcept>

#include "date.h"
#include "schedule.h"
#include "terminal.h"
#include "text.h"

namespace FlashTerm {
namespace {
void draw_bar(int filled, int width) {
  std::cout << "[";
  for (int i = 0; i < width; ++i) {
    std::cout << (i < filled ? "█" : "░");
  }
  std::cout << "]";
}

void edit_flashcard(Deck& deck);
void delete_flashcard(Deck& deck);
}  // namespace

void read_line(std::string& out) {
  if (!std::getline(std::cin, out)) {
    throw std::runtime_error("end of input");
  }
}

std::string prompt(const std::string& message) {
  std::cout << message;
  std::string input;
  read_line(input);
  return input;
}

int read_int() {
  std::string input;
  read_line(input);
  try {
    return std::stoi(input);
  } catch (const std::exception&) {
    return -1;
  }
}

void autosave(const Deck& deck) {
  std::string error;
  if (!deck.save(&error)) {
    std::cout << color::red << "WARNING: could not save deck (" << error
              << ").\nYour changes are still in memory but not on disk.\n"
              << color::reset;
  }
}

void add_flashcard(Deck& deck) {
  std::cout << color::yellow << "Enter question (or 'q' to go back): "
            << color::reset;
  std::string question;
  read_line(question);
  if (question == "q" || question == "Q") {
    return;
  }

  const std::string answer = prompt("Enter answer: ");
  const std::string tags_str =
      prompt("Enter tags (semicolon-separated, e.g. math;science): ");

  deck.add(Flashcard(question, answer, split(tags_str, ';')));
  autosave(deck);
  std::cout << color::green << "Flashcard added!" << color::reset << "\n\n";
}

void list_flashcards(const Deck& deck) {
  if (deck.empty()) {
    std::cout << color::yellow << "No flashcards to display.\n\n"
              << color::reset;
    return;
  }
  const int today_days = today();
  const auto& cards = deck.cards();
  for (size_t i = 0; i < cards.size(); ++i) {
    std::cout << i + 1 << ". " << cards[i].question << " - " << cards[i].answer;
    if (!cards[i].tags.empty()) {
      std::cout << " [Tags: " << cards[i].tags_to_string() << "]";
    }
    std::cout << " (Box " << cards[i].leitner_box << ", due "
              << describe_due(cards[i].due_date, today_days) << ")\n";
  }
  std::cout << "\n";
}

namespace {
void edit_flashcard(Deck& deck) {
  list_flashcards(deck);
  if (deck.empty()) return;
  std::cout << "Enter the number of the flashcard to edit: ";
  const int index = read_int();

  if (index <= 0 || index > static_cast<int>(deck.size())) {
    std::cout << color::red << "Invalid index.\n\n" << color::reset;
    return;
  }

  Flashcard& card = deck.cards()[index - 1];
  const std::string question =
      prompt("Enter new question (current: " + card.question + "): ");
  const std::string answer =
      prompt("Enter new answer (current: " + card.answer + "): ");
  const std::string tags_str =
      prompt("Enter new tags (semicolon;separated, current: " +
             card.tags_to_string() + "): ");

  if (!question.empty()) card.question = question;
  if (!answer.empty()) card.answer = answer;
  if (!tags_str.empty()) card.tags = split(tags_str, ';');

  autosave(deck);
  std::cout << color::green << "Flashcard updated!\n\n" << color::reset;
}

void delete_flashcard(Deck& deck) {
  list_flashcards(deck);
  if (deck.empty()) return;
  std::cout << "Enter the number of the flashcard to delete: ";
  const int index = read_int();

  if (index <= 0 || index > static_cast<int>(deck.size())) {
    std::cout << color::red << "Invalid index.\n\n" << color::reset;
    return;
  }

  deck.remove(static_cast<size_t>(index - 1));
  autosave(deck);
  std::cout << color::green << "Flashcard deleted!\n\n" << color::reset;
}
}  // namespace

void manage_flashcards(Deck& deck) {
  while (true) {
    std::cout << color::cyan << "--- Managing Options ---\n"
              << "1. List flashcards\n"
              << "2. Edit a flashcard\n"
              << "3. Delete a flashcard\n"
              << color::yellow << "q. Return to main menu\n"
              << "Choose: " << color::reset;
    std::string choice;
    read_line(choice);
    clear_screen();

    if (choice == "1") {
      list_flashcards(deck);
    } else if (choice == "2") {
      edit_flashcard(deck);
    } else if (choice == "3") {
      delete_flashcard(deck);
    } else if (choice == "q" || choice == "Q") {
      break;
    } else {
      std::cout << color::red << "Invalid choice. Please try again.\n"
                << color::reset;
    }
  }
}

void list_unique_tags(const Deck& deck) {
  if (deck.empty()) {
    std::cout << color::yellow
              << "No flashcards added yet, so no tags to display.\n\n"
              << color::reset;
    return;
  }

  const std::vector<std::string> tags = deck.unique_tags();
  if (tags.empty()) {
    std::cout << color::yellow << "No tags found across all flashcards.\n\n"
              << color::reset;
    return;
  }

  std::cout << color::cyan << "--- All Unique Tags (" << tags.size() << ") ---\n"
            << color::reset;
  for (size_t i = 0; i < tags.size(); ++i) {
    std::cout << i + 1 << ". " << tags[i] << " (" << deck.count_with_tag(tags[i])
              << " cards)\n";
  }
  std::cout << "\n";
}

void display_progress(const Deck& deck) {
  if (deck.empty()) {
    std::cout << color::yellow << "No flashcards to display progress for.\n\n"
              << color::reset;
    return;
  }

  const int today_days = today();
  const DeckStats stats = deck.stats(today_days);
  const int reviews = stats.total_correct + stats.total_incorrect;

  std::cout << color::cyan << "==================================================\n"
            << "                 DECK STATISTICS                  \n"
            << "==================================================\n"
            << color::reset << "  Total Cards:          " << stats.total_cards
            << "\n  Overall Success Rate: " << std::fixed << std::setprecision(2)
            << stats.success_rate << "%\n  Total Reviews:        " << reviews
            << " (" << stats.total_correct << " Correct, "
            << stats.total_incorrect << " Incorrect)\n  Due Now:              "
            << stats.due_count << "\n";
  if (stats.next_due != kNoDate) {
    std::cout << "  Next Card Due:        "
              << describe_due(stats.next_due, today_days) << " ("
              << format_date(stats.next_due) << ")\n";
  }
  std::cout << "\n";

  std::cout << color::cyan << "--- Leitner Box Distribution ---\n"
            << color::reset;
  for (int box = 1; box <= kMaxBox; ++box) {
    const int count = stats.box_counts[box];
    const int bar_width = 10;
    const int filled = (count * bar_width) / stats.total_cards;

    std::string label = "  Box " + std::to_string(box) + ": ";
    if (box == 1) label = "  Box 1 (Weakest): ";
    if (box == kMaxBox) label = "  Box 5 (Mastered):";

    std::cout << std::left << std::setw(19) << label;
    draw_bar(filled, bar_width);
    std::cout << " " << count << " cards ("
              << (count * 100 / stats.total_cards) << "%), every "
              << interval_for_box(box) << "d\n";
  }
  std::cout << "\n";

  if (stats.hardest_card != nullptr) {
    std::cout << color::red << "  Hardest Card to Remember:\n"
              << color::reset << "    Q: \"" << stats.hardest_card->question
              << "\"\n    Success rate: " << std::fixed << std::setprecision(2)
              << stats.hardest_rate << "% (" << stats.hardest_card->times_correct
              << " Correct, " << stats.hardest_card->times_incorrect
              << " Incorrect)\n\n";
  }

  const int width = terminal_width();
  const size_t q_width = static_cast<size_t>(std::max(10, width - 48));
  const int num_width = 10;
  const int box_width = 5;
  const int due_width = 8;

  std::cout << color::cyan << std::string(width, '-') << "\n"
            << pad_right("", static_cast<size_t>(std::max(0, (width - 13) / 2)))
            << "CARD PROGRESS\n"
            << std::string(width, '-') << "\n"
            << color::reset
            // setw counts bytes, so the question column is padded manually.
            << pad_right("Question", q_width) << std::right
            << std::setw(num_width) << "Correct" << std::setw(num_width)
            << "Incorrect" << std::setw(num_width) << "Success"
            << std::setw(box_width) << "Box" << std::setw(due_width) << "Due"
            << "\n"
            << std::string(width, '-') << "\n";

  for (const auto& card : deck.cards()) {
    const int total = card.times_correct + card.times_incorrect;
    const double rate =
        (total == 0) ? 0.0
                     : static_cast<double>(card.times_correct) * 100.0 / total;

    std::cout << pad_right(truncate(card.question, q_width), q_width)
              << std::right << std::setw(num_width) << card.times_correct
              << std::setw(num_width) << card.times_incorrect << std::fixed
              << std::setprecision(2) << std::setw(num_width) << rate
              << std::setw(box_width) << card.leitner_box
              << std::setw(due_width)
              << describe_due_short(card.due_date, today_days) << "\n";
  }

  std::cout << "\nPress Enter to return to main menu...";
  std::string discard;
  read_line(discard);
  clear_screen();
}

void print_help() {
  std::cout
      << color::yellow
      << "Commands:\n"
         "1 - Add flashcard\n"
         "2 - Review flashcards (Due now, All, by Tags, by Difficulty, or by "
         "Leitner Box)\n"
         "3 - Manage flashcards (list/edit/delete)\n"
         "4 - Display progress & statistics\n"
         "5 - Import flashcards (.csv/.txt)\n"
         "6 - Export flashcards (.csv)\n"
         "7 - Manage Tags (list all unique tags)\n"
         "0 - Save and exit\n"
         "h/? - Show this help screen\n"
      << color::reset << "\n";
}
}  // namespace FlashTerm
