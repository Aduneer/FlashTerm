#include "utils.h"

#include <sys/ioctl.h>
#include <unistd.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "flashcard.h"

namespace FlashTerm {
int get_terminal_width() {
  if (isatty(STDOUT_FILENO)) {
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    return w.ws_col;
  }
  return 80;  // Default width if not a TTY
}

std::vector<std::string> split_string_by_delimiter(const std::string& str,
                                                   char delimiter) {
  std::vector<std::string> parts;
  if (str.empty()) {
    return parts;
  }
  std::stringstream ss(str);
  std::string part;
  while (std::getline(ss, part, delimiter)) {
    parts.push_back(part);
  }
  return parts;
}

std::vector<Flashcard> load_flashcards(const std::string& filename) {
  std::vector<Flashcard> cards;
  std::ifstream file(filename);
  if (!file.is_open()) {
    return cards;
  }
  std::string line;
  while (std::getline(file, line)) {
    std::stringstream ss(line);
    std::string question, answer, tags_str, correct_str, incorrect_str;
    std::getline(ss, question, ',');
    std::getline(ss, answer, ',');
    std::getline(ss, tags_str, ',');
    std::getline(ss, correct_str, ',');
    std::getline(ss, incorrect_str, ',');

    std::vector<std::string> tags = split_string_by_delimiter(tags_str, ';');

    int correct = 0;
    if (!correct_str.empty()) {
      try {
        correct = std::stoi(correct_str);
      } catch (const std::invalid_argument&) {
        // keep default
      }
    }
    int incorrect = 0;
    if (!incorrect_str.empty()) {
      try {
        incorrect = std::stoi(incorrect_str);
      } catch (const std::invalid_argument&) {
        // keep default
      }
    }

    cards.emplace_back(question, answer, tags, correct, incorrect);
  }
  return cards;
}

void save_flashcards(const std::string& filename,
                     const std::vector<Flashcard>& cards) {
  std::ofstream file(filename);
  for (const auto& card : cards) {
    file << card.question << "," << card.answer << "," << card.tags_to_string()
         << "," << card.times_correct << "," << card.times_incorrect << "\n";
  }
}

void import_flashcards(std::vector<Flashcard>& cards,
                       const std::string& filename) {
  std::ifstream file(filename);
  if (!file) {
    std::cout << COLOR_RED << "Failed to open file: " << filename << COLOR_RESET
              << std::endl;
    return;
  }
  int imported = 0;
  std::string line;
  while (std::getline(file, line)) {
    std::istringstream iss(line);
    std::string question, answer, tags_str;
    if (std::getline(iss, question, ',') && std::getline(iss, answer, ',') &&
        std::getline(iss, tags_str)) {
      std::vector<std::string> tags = split_string_by_delimiter(tags_str, ';');

      cards.emplace_back(question, answer, tags);
      ++imported;
    }
  }
  std::cout << COLOR_GREEN << "Imported " << imported << " flashcards from "
            << filename << COLOR_RESET << std::endl
            << std::endl;
}

void export_flashcards(const std::vector<Flashcard>& cards,
                       const std::string& filename) {
  std::ofstream file(filename);
  if (!file) {
    std::cout << COLOR_RED << "Failed to write to: " << filename << COLOR_RESET
              << std::endl;
    return;
  }
  for (const auto& card : cards) {
    file << card.question << "," << card.answer << "," << card.tags_to_string()
         << "\n";
  }
  std::cout << COLOR_GREEN << "Exported " << cards.size() << " flashcards to "
            << filename << COLOR_RESET << std::endl
            << std::endl;
}
}  // namespace FlashTerm
