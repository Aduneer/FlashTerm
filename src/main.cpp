#include "flashcard.h"
#include "utils.h"
#include <algorithm>
#include <cctype>
#include <functional>
#include <iomanip>
#include <iostream>
#include <random>
#include <set>
#include <vector>
// removed redundant libraries

const std::string FILENAME = "flashcards.txt";

// Utility to get valid integer input
int get_int_input() {
  std::string input;
  std::getline(std::cin, input);
  try {
    return std::stoi(input);
  } catch (const std::exception &) {
    return -1; // Indicate failure
  }
}

// Helper function for smart answer validation
std::string normalize_string(const std::string &str) {
  std::string temp = str;

  // 1. Trim leading and trailing whitespace
  temp.erase(temp.begin(),
             std::find_if(temp.begin(), temp.end(),
                          [](unsigned char ch) { return !std::isspace(ch); }));
  temp.erase(std::find_if(temp.rbegin(), temp.rend(),
                          [](unsigned char ch) { return !std::isspace(ch); })
                 .base(),
             temp.end());

  // 2. Convert to lowercase
  std::transform(temp.begin(), temp.end(), temp.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  // 3. REMOVE ALL REMAINING WHITESPACE (hopefully)
  temp.erase(std::remove_if(temp.begin(), temp.end(),
                            [](unsigned char ch) { return std::isspace(ch); }),
             temp.end());

  return temp;
}

// Add a flashcard (with tags)
void add_flashcard(std::vector<Flashcard> &cards) {
  std::string q, a, tags_str;
  std::cout << "Enter question: ";
  std::getline(std::cin, q);
  std::cout << "Enter answer: ";
  std::getline(std::cin, a);
  std::cout << "Enter tags (semicolon-separated, e.g. math;science): ";
  std::getline(std::cin, tags_str);

  // Uses the utility function from utils.h/utils.cpp
  std::vector<std::string> tags = split_string_by_delimiter(tags_str, ';');

  cards.emplace_back(q, a, tags);
  std::cout << COLOR_GREEN << "Flashcard added!" << COLOR_RESET << "\n\n";
}

// Review flashcards (shuffled, progress tracked), with optional tag filtering
void review_flashcards(std::vector<Flashcard> &cards) {
  if (cards.empty()) {
    std::cout << COLOR_YELLOW << "No flashcards to review. Add some first!\n\n"
              << COLOR_RESET;
    return;
  }

  std::cout << COLOR_CYAN << "\n--- Review Options ---\n"
            << "1. Review ALL cards\n"
            << "2. Review by TAGS\n"
            << "3. Review DIFFICULT cards (incorrect > correct)\n"
            << "Choose review mode: " << COLOR_RESET;

  std::string mode_input;
  std::getline(std::cin, mode_input);
  int mode = (mode_input.empty()) ? 0 : mode_input[0] - '0';

  std::vector<std::string> filter_tags;
  bool filter_by_difficulty = false;

  if (mode == 2) {
    std::cout << "Enter tags to review (semicolon-separated): ";
    std::string filter_tags_str;
    std::getline(std::cin, filter_tags_str);

    filter_tags = split_string_by_delimiter(filter_tags_str, ';');

  } else if (mode == 3) {
    filter_by_difficulty = true;
    std::cout << COLOR_YELLOW
              << "Reviewing cards you find difficult (Incorrect > Correct).\n"
              << COLOR_RESET;
  } else if (mode != 1) {
    std::cout << COLOR_RED << "Invalid review mode. Defaulting to ALL cards.\n"
              << COLOR_RESET;
  }

  // Create references to the original Flashcard objects that match the filter
  std::vector<std::reference_wrapper<Flashcard>> shuffled_refs;

  for (auto &card : cards) {
    bool include_card = true;

    // Apply difficulty filter first (if enabled)
    if (filter_by_difficulty) {
      if (card.times_incorrect <= card.times_correct) {
        include_card = false;
      }
    }

    // Apply tag filter (if enabled and card is not already excluded)
    if (include_card && !filter_tags.empty()) {
      bool has_filter_tag = false;
      for (const auto &filter_tag : filter_tags) {
        if (std::find(card.tags.begin(), card.tags.end(), filter_tag) !=
            card.tags.end()) {
          has_filter_tag = true;
          break;
        }
      }
      if (!has_filter_tag) {
        include_card = false;
      }
    }

    if (include_card) {
      shuffled_refs.emplace_back(card);
    }
  }

  if (shuffled_refs.empty()) {
    std::cout << COLOR_YELLOW
              << "No flashcards match the current review filters.\n\n"
              << COLOR_RESET;
    return;
  }

  // Shuffle the references to the original cards
  std::random_device rd;
  std::mt19937 g(rd());
  std::shuffle(shuffled_refs.begin(), shuffled_refs.end(), g);

  int correct_total = 0, wrong_total = 0;

  for (auto ref : shuffled_refs) {
    Flashcard &card = ref.get(); // Get the reference to the original card
    std::cout << COLOR_CYAN << "Q: " << card.question
              << "\nYour answer: " << COLOR_RESET;
    std::string user_answer;
    std::getline(std::cin, user_answer);

    // --- "SMART" VALIDATION CHECK ---
    std::string normalized_user_answer = normalize_string(user_answer);
    std::string normalized_correct_answer = normalize_string(card.answer);

    if (normalized_user_answer == normalized_correct_answer) {
      std::cout << COLOR_GREEN << "✅ Correct!" << COLOR_RESET << "\n\n";
      card.times_correct++;
      correct_total++;
    } else {
      std::cout << COLOR_RED << "❌ Incorrect! Correct answer: " << card.answer
                << COLOR_RESET << "\n\n";
      card.times_incorrect++;
      wrong_total++;
    }
  }

  int total = correct_total + wrong_total;
  double percent =
      (total > 0) ? (static_cast<double>(correct_total) * 100.0 / total) : 0.0;

  std::cout << COLOR_YELLOW << "Review complete! Results:\n"
            << "Correct: " << correct_total << "\n"
            << "Incorrect: " << wrong_total << "\n"
            << "Total reviewed: " << total << "\n"
            << "Percent correct: " << std::fixed << std::setprecision(2)
            << percent << "%\n\n"
            << COLOR_RESET;
}

// List all flashcards with tags
void list_flashcards(const std::vector<Flashcard> &cards) {
  if (cards.empty()) {
    std::cout << COLOR_YELLOW << "No flashcards to display.\n\n" << COLOR_RESET;
    return;
  }
  for (size_t i = 0; i < cards.size(); ++i) {
    std::cout << i + 1 << ". " << cards[i].question << " - " << cards[i].answer;
    if (!cards[i].tags.empty()) {
      std::cout << " [" << cards[i].tags_to_string() << "]";
    }
    std::cout << "\n";
  }
  std::cout << "\n";
}

// Edit a flashcard
void edit_flashcard(std::vector<Flashcard> &cards) {
  list_flashcards(cards);
  if (cards.empty())
    return;
  std::cout << "Enter the number of the flashcard to edit: ";
  int index = get_int_input();

  if (index > 0 && index <= static_cast<int>(cards.size())) {
    Flashcard &card = cards[index - 1];
    std::string q, a, tags_str;
    std::cout << "Enter new question (current: " << card.question << "): ";
    std::getline(std::cin, q);
    std::cout << "Enter new answer (current: " << card.answer << "): ";
    std::getline(std::cin, a);
    std::cout << "Enter new tags (semicolon;separated, current: "
              << card.tags_to_string() << "): ";
    std::getline(std::cin, tags_str);

    if (!q.empty())
      card.question = q;
    if (!a.empty())
      card.answer = a;
    if (!tags_str.empty()) {
      // Uses the utility function from utils.h/utils.cpp
      card.tags = split_string_by_delimiter(tags_str, ';');
    }
    std::cout << COLOR_GREEN << "Flashcard updated!\n\n" << COLOR_RESET;
  } else {
    std::cout << COLOR_RED << "Invalid index.\n\n" << COLOR_RESET;
  }
}

// Delete a flashcard
void delete_flashcard(std::vector<Flashcard> &cards) {
  list_flashcards(cards);
  if (cards.empty())
    return;
  std::cout << "Enter the number of the flashcard to delete: ";
  int index = get_int_input();

  if (index > 0 && index <= static_cast<int>(cards.size())) {
    cards.erase(cards.begin() + index - 1);
    std::cout << COLOR_GREEN << "Flashcard deleted!\n\n" << COLOR_RESET;
  } else {
    std::cout << COLOR_RED << "Invalid index.\n\n" << COLOR_RESET;
  }
}

// Manage flashcards menu (list, edit, delete)
void manage_flashcards(std::vector<Flashcard> &cards) {
  while (true) {
    std::cout << "1. List flashcards\n"
              << "2. Edit a flashcard\n"
              << "3. Delete a flashcard\n"
              << "4. Return to main menu\n"
              << COLOR_CYAN << "Choose: " << COLOR_RESET;
    int choice = get_int_input();

    if (choice == 1) {
      list_flashcards(cards);
    } else if (choice == 2) {
      edit_flashcard(cards);
    } else if (choice == 3) {
      delete_flashcard(cards);
    } else if (choice == 4) {
      break;
    } else {
      std::cout << COLOR_RED << "Invalid choice. Please try again.\n"
                << COLOR_RESET;
    }
  }
}

void list_all_unique_tags(const std::vector<Flashcard> &cards) {
  if (cards.empty()) {
    std::cout << COLOR_YELLOW
              << "No flashcards added yet, so no tags to display.\n\n"
              << COLOR_RESET;
    return;
  }

  std::set<std::string> unique_tags;
  for (const auto &card : cards) {
    for (const auto &tag : card.tags) {
      unique_tags.insert(tag);
    }
  }

  if (unique_tags.empty()) {
    std::cout << COLOR_YELLOW << "No tags found across all flashcards.\n\n"
              << COLOR_RESET;
    return;
  }

  std::cout << COLOR_CYAN << "--- All Unique Tags (" << unique_tags.size()
            << ") ---\n"
            << COLOR_RESET;
  int i = 1;
  for (const auto &tag : unique_tags) {
    std::cout << i++ << ". " << tag << "\n";
  }
  std::cout << "\n";
}

// Show progress statistics
void display_progress(const std::vector<Flashcard> &cards) {
  if (cards.empty()) {
    std::cout << COLOR_YELLOW << "No flashcards to display progress for.\n\n"
              << COLOR_RESET;
    return;
  }
  std::cout << std::left << std::setw(30) << "Question" << std::setw(15)
            << "Correct" << std::setw(15) << "Incorrect" << std::setw(15)
            << "Success (%)"
            << "\n"
            << std::string(75, '-') << "\n";
  for (const auto &card : cards) {
    int total = card.times_correct + card.times_incorrect;
    double success_rate =
        (total == 0)
            ? 0.0
            : (static_cast<double>(card.times_correct) / total) * 100.0;
    std::cout << std::left << std::setw(30) << card.question << std::setw(15)
              << card.times_correct << std::setw(15) << card.times_incorrect
              << std::fixed << std::setprecision(2) << success_rate << "\n";
  }
  std::cout << "\n";
}

// Help screen
void print_help() {
  std::cout << COLOR_YELLOW
            << "Commands:\n"
               "1 - Add flashcard\n"
               "2 - Review flashcards (Choose All, by Tags, or by Difficulty)\n"
               "3 - Manage flashcards (list/edit/delete)\n"
               "4 - Display progress\n"
               "5 - Import flashcards (.csv/.txt)\n"
               "6 - Export flashcards (.csv)\n"
               "7 - Manage Tags (list all unique tags)\n"
               "0 - Save and exit\n"
               "h/? - Show this help screen\n"
            << COLOR_RESET << "\n";
}

int main() {
  std::vector<Flashcard> cards = load_flashcards(FILENAME);

  while (true) {
    std::cout << COLOR_CYAN << "1. Add flashcard\n"
              << "2. Review flashcards\n"
              << "3. Manage flashcards\n"
              << "4. Display progress\n"
              << "5. Import flashcards\n"
              << "6. Export flashcards\n"
              << "7. Manage Tags\n"
              << "0. Save and exit\n"
              << "h/? Help\n"
              << "Choose: " << COLOR_RESET;
    std::string input;
    std::getline(std::cin, input);

    if (input == "1") {
      add_flashcard(cards);
    } else if (input == "2") {
      review_flashcards(cards);
    } else if (input == "3") {
      manage_flashcards(cards);
    } else if (input == "4") {
      display_progress(cards);
    } else if (input == "5") {
      std::cout << "Enter import file path: ";
      std::string import_path;
      std::getline(std::cin, import_path);
      import_flashcards(cards, import_path);
    } else if (input == "6") {
      std::cout << "Enter export file path: ";
      std::string export_path;
      std::getline(std::cin, export_path);
      export_flashcards(cards, export_path);
    } else if (input == "7") {
      list_all_unique_tags(cards);
    } else if (input == "0") {
      save_flashcards(FILENAME, cards);
      std::cout << COLOR_YELLOW << "Flashcards saved. Goodbye!\n"
                << COLOR_RESET;
      break;
    } else if (input == "h" || input == "?") {
      print_help();
    } else {
      std::cout << COLOR_RED << "Invalid choice. Please try again.\n"
                << COLOR_RESET;
    }
  }
  return 0;
}
