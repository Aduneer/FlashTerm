#include "utils.h"

#include <sys/ioctl.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
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

std::string trim(const std::string& str) {
  size_t first = str.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) return "";
  size_t last = str.find_last_not_of(" \t\r\n");
  return str.substr(first, (last - first + 1));
}

std::string to_lowercase(const std::string& str) {
  std::string temp = str;
  for (char& c : temp) {
    c = std::tolower(static_cast<unsigned char>(c));
  }
  return temp;
}

int levenshtein_distance(const std::string& s1, const std::string& s2) {
  int len1 = s1.size();
  int len2 = s2.size();
  std::vector<std::vector<int>> d(len1 + 1, std::vector<int>(len2 + 1));

  for (int i = 0; i <= len1; ++i) d[i][0] = i;
  for (int j = 0; j <= len2; ++j) d[0][j] = j;

  for (int i = 1; i <= len1; ++i) {
    for (int j = 1; j <= len2; ++j) {
      int cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;
      d[i][j] = std::min({d[i - 1][j] + 1,
                          d[i][j - 1] + 1,
                          d[i - 1][j - 1] + cost});
    }
  }
  return d[len1][len2];
}

std::string escape_csv_field(const std::string& field) {
  bool needs_quotes = false;
  if (field.find(',') != std::string::npos ||
      field.find('"') != std::string::npos ||
      field.find('\n') != std::string::npos ||
      field.find('\r') != std::string::npos) {
    needs_quotes = true;
  }
  if (!needs_quotes) {
    return field;
  }
  std::string escaped = "\"";
  for (char c : field) {
    if (c == '"') {
      escaped += "\"\"";
    } else {
      escaped += c;
    }
  }
  escaped += "\"";
  return escaped;
}

std::vector<std::string> parse_csv_line(const std::string& line) {
  std::vector<std::string> fields;
  std::string current_field;
  bool in_quotes = false;
  for (size_t i = 0; i < line.length(); ++i) {
    char c = line[i];
    if (c == '"') {
      if (in_quotes && i + 1 < line.length() && line[i + 1] == '"') {
        current_field += '"';
        ++i; // skip next quote
      } else {
        in_quotes = !in_quotes;
      }
    } else if (c == ',' && !in_quotes) {
      fields.push_back(current_field);
      current_field.clear();
    } else {
      current_field += c;
    }
  }
  fields.push_back(current_field);
  return fields;
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
    parts.push_back(trim(part));
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
    if (line.empty()) continue;
    std::vector<std::string> fields = parse_csv_line(line);
    if (fields.size() < 2) continue; // Must at least have question and answer
    
    std::string question = fields[0];
    std::string answer = fields[1];
    
    std::string tags_str = (fields.size() >= 3) ? fields[2] : "";
    std::vector<std::string> tags = split_string_by_delimiter(tags_str, ';');

    int correct = 0;
    if (fields.size() >= 4 && !fields[3].empty()) {
      try {
        correct = std::stoi(fields[3]);
      } catch (...) {}
    }
    
    int incorrect = 0;
    if (fields.size() >= 5 && !fields[4].empty()) {
      try {
        incorrect = std::stoi(fields[4]);
      } catch (...) {}
    }
    
    int leitner = 1;
    if (fields.size() >= 6 && !fields[5].empty()) {
      try {
        leitner = std::stoi(fields[5]);
        if (leitner < 1) leitner = 1;
        if (leitner > 5) leitner = 5;
      } catch (...) {}
    }

    cards.emplace_back(question, answer, tags, correct, incorrect, leitner);
  }
  return cards;
}

void save_flashcards(const std::string& filename,
                     const std::vector<Flashcard>& cards) {
  std::ofstream file(filename);
  for (const auto& card : cards) {
    file << escape_csv_field(card.question) << ","
         << escape_csv_field(card.answer) << ","
         << escape_csv_field(card.tags_to_string()) << ","
         << card.times_correct << ","
         << card.times_incorrect << ","
         << card.leitner_box << "\n";
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
    if (line.empty()) continue;
    std::vector<std::string> fields = parse_csv_line(line);
    if (fields.size() >= 2) {
      std::string question = fields[0];
      std::string answer = fields[1];
      std::string tags_str = (fields.size() >= 3) ? fields[2] : "";
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
    file << escape_csv_field(card.question) << ","
         << escape_csv_field(card.answer) << ","
         << escape_csv_field(card.tags_to_string()) << "\n";
  }
  std::cout << COLOR_GREEN << "Exported " << cards.size() << " flashcards to "
            << filename << COLOR_RESET << std::endl
            << std::endl;
}
}  // namespace FlashTerm
