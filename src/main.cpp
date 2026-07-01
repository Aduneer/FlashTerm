#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <random>
#include <set>
#include <vector>

#include "flashcard.h"
#include "utils.h"

using namespace FlashTerm;

const std::string FILENAME = "flashcards.txt";

void clear_screen() {
#ifdef _WIN32
  system("cls");
#else
  system("clear");
#endif
}

void get_input(std::string& str) {
  if (!std::getline(std::cin, str)) {
    throw std::runtime_error("EOF");
  }
}

int get_int_input() {
  std::string input;
  get_input(input);
  try {
    return std::stoi(input);
  } catch (const std::exception&) {
    return -1;
  }
}

std::string normalize_string(const std::string& str) {
  std::string temp = str;

  temp.erase(temp.begin(),
             std::find_if(temp.begin(), temp.end(),
                          [](unsigned char ch) { return !std::isspace(ch); }));
  temp.erase(std::find_if(temp.rbegin(), temp.rend(),
                          [](unsigned char ch) { return !std::isspace(ch); })
                 .base(),
             temp.end());

  std::transform(temp.begin(), temp.end(), temp.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  temp.erase(std::remove_if(temp.begin(), temp.end(),
                            [](unsigned char ch) { return std::isspace(ch); }),
             temp.end());

  return temp;
}

void add_flashcard(std::vector<Flashcard>& cards) {
  std::string q, a, tags_str;
  std::cout << COLOR_YELLOW
            << "Enter question (or 'q' to go back): " << COLOR_RESET;
  get_input(q);
  if (q == "q" || q == "Q") {
    return;
  }

  std::cout << "Enter answer: ";
  get_input(a);
  std::cout << "Enter tags (semicolon-separated, e.g. math;science): ";
  get_input(tags_str);

  std::vector<std::string> tags = split_string_by_delimiter(tags_str, ';');

  cards.emplace_back(q, a, tags);
  std::cout << COLOR_GREEN << "Flashcard added!" << COLOR_RESET << "\n\n";
}

void review_flashcards(std::vector<Flashcard>& cards) {
  if (cards.empty()) {
    std::cout << COLOR_YELLOW << "No flashcards to review. Add some first!\n\n"
              << COLOR_RESET;
    return;
  }

  std::cout << COLOR_CYAN << "\n--- Review Options ---\n"
            << "1. Review ALL cards\n"
            << "2. Review by TAGS\n"
            << "3. Review DIFFICULT cards (incorrect > correct)\n"
            << "4. Review by LEITNER BOX (Focus on weaker boxes)\n"
            << COLOR_YELLOW << "q. go back\n"
            << "Choose review mode: " << COLOR_RESET;

  std::string mode_input;
  get_input(mode_input);
  if (mode_input == "q" || mode_input == "Q") {
    clear_screen();
    return;
  }
  int mode = (mode_input.empty()) ? 0 : mode_input[0] - '0';

  std::vector<std::string> filter_tags;
  bool filter_by_difficulty = false;
  int filter_leitner_box = 0; // 0 means no filter, 1-5 means filter by box

  if (mode == 2) {
    std::vector<std::string> unique_tags;
    std::set<std::string> seen_tags;
    for (const auto& card : cards) {
      for (const auto& tag : card.tags) {
        if (!tag.empty() && seen_tags.find(to_lowercase(tag)) == seen_tags.end()) {
          seen_tags.insert(to_lowercase(tag));
          unique_tags.push_back(tag);
        }
      }
    }

    std::sort(unique_tags.begin(), unique_tags.end(), [](const std::string& a, const std::string& b) {
      return to_lowercase(a) < to_lowercase(b);
    });

    if (unique_tags.empty()) {
      std::cout << COLOR_YELLOW << "No tags found across all flashcards. Defaulting to ALL cards.\n" << COLOR_RESET;
    } else {
      std::cout << COLOR_CYAN << "\n--- Select Tags to Review ---\n" << COLOR_RESET;
      for (size_t i = 0; i < unique_tags.size(); ++i) {
        int count = 0;
        for (const auto& card : cards) {
          for (const auto& tag : card.tags) {
            if (to_lowercase(tag) == to_lowercase(unique_tags[i])) {
              count++;
              break;
            }
          }
        }
        std::cout << "  " << i + 1 << ". " << unique_tags[i] << " (" << count << " cards)\n";
      }
      std::cout << COLOR_YELLOW << "Enter numbers (e.g. 1, 3) or names of tags: " << COLOR_RESET;
      std::string tag_input;
      get_input(tag_input);
      
      std::vector<std::string> inputs = split_string_by_delimiter(tag_input, ',');
      if (inputs.size() == 1 && inputs[0].find(' ') != std::string::npos) {
        inputs = split_string_by_delimiter(tag_input, ' ');
      }
      
      for (const auto& in : inputs) {
        std::string trimmed_in = trim(in);
        if (trimmed_in.empty()) continue;
        
        bool is_num = true;
        for (char c : trimmed_in) {
          if (!std::isdigit(c)) {
            is_num = false;
            break;
          }
        }
        
        if (is_num) {
          int idx = std::stoi(trimmed_in) - 1;
          if (idx >= 0 && idx < static_cast<int>(unique_tags.size())) {
            filter_tags.push_back(unique_tags[idx]);
          }
        } else {
          filter_tags.push_back(trimmed_in);
        }
      }
    }
  } else if (mode == 3) {
    filter_by_difficulty = true;
    std::cout << COLOR_YELLOW
              << "Reviewing cards you find difficult (Incorrect > Correct).\n"
              << COLOR_RESET;
  } else if (mode == 4) {
    int box_counts[6] = {0};
    for (const auto& card : cards) {
      if (card.leitner_box >= 1 && card.leitner_box <= 5) {
        box_counts[card.leitner_box]++;
      } else {
        box_counts[1]++;
      }
    }
    std::cout << COLOR_CYAN << "\n--- Select Leitner Box ---\n" << COLOR_RESET;
    for (int b = 1; b <= 5; ++b) {
      std::cout << "  " << b << ". Box " << b << " (" << box_counts[b] << " cards)\n";
    }
    std::cout << "  0. Review ALL boxes (prioritizing lower boxes)\n";
    std::cout << COLOR_YELLOW << "Choose Box: " << COLOR_RESET;
    std::string box_input_str;
    get_input(box_input_str);
    try {
      filter_leitner_box = std::stoi(box_input_str);
    } catch (...) {
      filter_leitner_box = 0;
    }
    if (filter_leitner_box < 0 || filter_leitner_box > 5) {
      filter_leitner_box = 0;
    }
  } else if (mode != 1) {
    std::cout << COLOR_RED << "Invalid review mode. Defaulting to ALL cards.\n"
              << COLOR_RESET;
  }

  std::vector<std::reference_wrapper<Flashcard>> matched_refs;

  for (auto& card : cards) {
    bool include_card = true;

    if (filter_by_difficulty) {
      if (card.times_incorrect <= card.times_correct) {
        include_card = false;
      }
    }

    if (include_card && filter_leitner_box > 0) {
      if (card.leitner_box != filter_leitner_box) {
        include_card = false;
      }
    }

    if (include_card && !filter_tags.empty()) {
      bool has_filter_tag = false;
      std::vector<std::string> filter_tags_lower;
      for (const auto& ft : filter_tags) {
        filter_tags_lower.push_back(to_lowercase(ft));
      }
      for (const auto& card_tag : card.tags) {
        if (std::find(filter_tags_lower.begin(), filter_tags_lower.end(), to_lowercase(card_tag)) != filter_tags_lower.end()) {
          has_filter_tag = true;
          break;
        }
      }
      if (!has_filter_tag) {
        include_card = false;
      }
    }

    if (include_card) {
      matched_refs.emplace_back(card);
    }
  }

  if (matched_refs.empty()) {
    std::cout << COLOR_YELLOW
              << "No flashcards match the current review filters.\n\n"
              << COLOR_RESET;
    return;
  }

  if (mode == 4 && filter_leitner_box == 0) {
    std::vector<std::vector<std::reference_wrapper<Flashcard>>> boxes(6);
    for (auto ref : matched_refs) {
      int b = ref.get().leitner_box;
      if (b < 1) b = 1;
      if (b > 5) b = 5;
      boxes[b].push_back(ref);
    }
    
    std::random_device rd;
    std::mt19937 g(rd());
    matched_refs.clear();
    for (int b = 1; b <= 5; ++b) {
      std::shuffle(boxes[b].begin(), boxes[b].end(), g);
      for (auto ref : boxes[b]) {
        matched_refs.push_back(ref);
      }
    }
  } else {
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(matched_refs.begin(), matched_refs.end(), g);
  }

  int correct_total = 0, wrong_total = 0;
  int term_width = get_terminal_width();

  for (size_t idx = 0; idx < matched_refs.size(); ++idx) {
    Flashcard& card = matched_refs[idx].get();
    clear_screen();
    
    int current_card_num = idx + 1;
    int total_cards_num = matched_refs.size();
    int percent_done = (current_card_num * 100) / total_cards_num;
    
    int bar_width = 20;
    int pos = (current_card_num * bar_width) / total_cards_num;
    
    std::cout << COLOR_CYAN << "Progress: [";
    for (int i = 0; i < bar_width; ++i) {
      if (i < pos) std::cout << "█";
      else std::cout << "░";
    }
    std::cout << "] " << percent_done << "% (" << current_card_num << "/" << total_cards_num << " cards)\n";
    std::cout << "Box: " << card.leitner_box;
    if (!card.tags.empty()) {
      std::cout << " | Tags: " << card.tags_to_string();
    }
    std::cout << "\n";
    std::cout << std::string(std::min(50, term_width), '-') << "\n\n";

    std::cout << COLOR_CYAN << "Q: " << card.question << COLOR_RESET << "\n\n";
    std::cout << "Your answer: ";
    std::string user_answer;
    get_input(user_answer);

    std::string normalized_user_answer = normalize_string(user_answer);
    std::string normalized_correct_answer = normalize_string(card.answer);

    if (normalized_user_answer == normalized_correct_answer) {
      std::cout << COLOR_GREEN << "\n✅ Correct!" << COLOR_RESET << "\n";
      card.times_correct++;
      correct_total++;
      
      int old_box = card.leitner_box;
      card.leitner_box = std::min(5, card.leitner_box + 1);
      if (card.leitner_box > old_box) {
        std::cout << COLOR_GREEN << "Card promoted to Box " << card.leitner_box << "!\n" << COLOR_RESET;
      }
      std::cout << "\nPress Enter to continue...";
      std::string temp;
      get_input(temp);
    } else {
      std::string clean_user = to_lowercase(trim(user_answer));
      std::string clean_correct = to_lowercase(trim(card.answer));
      int dist = levenshtein_distance(clean_user, clean_correct);
      
      bool is_close = false;
      if (clean_correct.length() >= 4 && clean_correct.length() <= 8 && dist <= 1) {
        is_close = true;
      } else if (clean_correct.length() > 8 && dist <= 2) {
        is_close = true;
      }
      
      bool marked_correct = false;
      if (is_close) {
        std::cout << COLOR_YELLOW << "\n⚠️  Close! The correct answer is: " << card.answer << "\n"
                  << "   (You typed: " << user_answer << ")\n"
                  << "Mark as correct anyway? [y/N]: " << COLOR_RESET;
        std::string override_input;
        get_input(override_input);
        if (to_lowercase(trim(override_input)) == "y") {
          std::cout << COLOR_GREEN << "✅ Marked as correct!" << COLOR_RESET << "\n";
          card.times_correct++;
          correct_total++;
          
          int old_box = card.leitner_box;
          card.leitner_box = std::min(5, card.leitner_box + 1);
          if (card.leitner_box > old_box) {
            std::cout << COLOR_GREEN << "Card promoted to Box " << card.leitner_box << "!\n" << COLOR_RESET;
          }
          marked_correct = true;
        }
      }
      
      if (!marked_correct) {
        std::cout << COLOR_RED << "\n❌ Incorrect! Correct answer: " << card.answer << COLOR_RESET << "\n";
        card.times_incorrect++;
        wrong_total++;
        
        int old_box = card.leitner_box;
        card.leitner_box = 1;
        if (old_box > 1) {
          std::cout << COLOR_RED << "Card demoted back to Box 1!\n" << COLOR_RESET;
        }
      }
      std::cout << "\nPress Enter to continue...";
      std::string temp;
      get_input(temp);
    }
  }

  clear_screen();
  int total = correct_total + wrong_total;
  double percent =
      (total > 0) ? (static_cast<double>(correct_total) * 100.0 / total) : 0.0;

  std::cout << COLOR_YELLOW << "==================================================\n"
            << "                 REVIEW COMPLETE                  \n"
            << "==================================================\n"
            << "  Correct:        " << correct_total << "\n"
            << "  Incorrect:      " << wrong_total << "\n"
            << "  Total Reviewed: " << total << "\n"
            << "  Success Rate:   " << std::fixed << std::setprecision(2)
            << percent << "%\n"
            << "==================================================\n\n"
            << "Press Enter to return to main menu...";
  std::string temp;
  get_input(temp);
  clear_screen();
}

void list_flashcards(const std::vector<Flashcard>& cards) {
  if (cards.empty()) {
    std::cout << COLOR_YELLOW << "No flashcards to display.\n\n" << COLOR_RESET;
    return;
  }
  for (size_t i = 0; i < cards.size(); ++i) {
    std::cout << i + 1 << ". " << cards[i].question << " - " << cards[i].answer;
    if (!cards[i].tags.empty()) {
      std::cout << " [Tags: " << cards[i].tags_to_string() << "]";
    }
    std::cout << " (Box " << cards[i].leitner_box << ")";
    std::cout << "\n";
  }
  std::cout << "\n";
}

void edit_flashcard(std::vector<Flashcard>& cards) {
  list_flashcards(cards);
  if (cards.empty()) return;
  std::cout << "Enter the number of the flashcard to edit: ";
  int index = get_int_input();

  if (index > 0 && index <= static_cast<int>(cards.size())) {
    Flashcard& card = cards[index - 1];
    std::string q, a, tags_str;
    std::cout << "Enter new question (current: " << card.question << "): ";
    get_input(q);
    std::cout << "Enter new answer (current: " << card.answer << "): ";
    get_input(a);
    std::cout << "Enter new tags (semicolon;separated, current: "
              << card.tags_to_string() << "): ";
    get_input(tags_str);

    if (!q.empty()) card.question = q;
    if (!a.empty()) card.answer = a;
    if (!tags_str.empty()) {
      card.tags = split_string_by_delimiter(tags_str, ';');
    }
    std::cout << COLOR_GREEN << "Flashcard updated!\n\n" << COLOR_RESET;
  } else {
    std::cout << COLOR_RED << "Invalid index.\n\n" << COLOR_RESET;
  }
}

void delete_flashcard(std::vector<Flashcard>& cards) {
  list_flashcards(cards);
  if (cards.empty()) return;
  std::cout << "Enter the number of the flashcard to delete: ";
  int index = get_int_input();

  if (index > 0 && index <= static_cast<int>(cards.size())) {
    cards.erase(cards.begin() + index - 1);
    std::cout << COLOR_GREEN << "Flashcard deleted!\n\n" << COLOR_RESET;
  } else {
    std::cout << COLOR_RED << "Invalid index.\n\n" << COLOR_RESET;
  }
}

void manage_flashcards(std::vector<Flashcard>& cards) {
  while (true) {
    std::cout << COLOR_CYAN << "--- Managing Options ---\n"
              << "1. List flashcards\n"
              << "2. Edit a flashcard\n"
              << "3. Delete a flashcard\n"
              << COLOR_YELLOW << "q. Return to main menu\n"
              << COLOR_RESET << COLOR_YELLOW << "Choose: " << COLOR_RESET;
    std::string choice;
    get_input(choice);
    clear_screen();

    if (choice == "1") {
      list_flashcards(cards);
    } else if (choice == "2") {
      edit_flashcard(cards);
    } else if (choice == "3") {
      delete_flashcard(cards);
    } else if (choice == "q" || choice == "Q") {
      break;
    } else {
      std::cout << COLOR_RED << "Invalid choice. Please try again.\n"
                << COLOR_RESET;
    }
  }
}

void list_all_unique_tags(const std::vector<Flashcard>& cards) {
  if (cards.empty()) {
    std::cout << COLOR_YELLOW
              << "No flashcards added yet, so no tags to display.\n\n"
              << COLOR_RESET;
    return;
  }

  std::vector<std::string> unique_tags;
  std::set<std::string> seen_tags_lower;
  for (const auto& card : cards) {
    for (const auto& tag : card.tags) {
      if (!tag.empty() && seen_tags_lower.find(to_lowercase(tag)) == seen_tags_lower.end()) {
        seen_tags_lower.insert(to_lowercase(tag));
        unique_tags.push_back(tag);
      }
    }
  }

  if (unique_tags.empty()) {
    std::cout << COLOR_YELLOW << "No tags found across all flashcards.\n\n"
              << COLOR_RESET;
    return;
  }

  std::sort(unique_tags.begin(), unique_tags.end(), [](const std::string& a, const std::string& b) {
    return to_lowercase(a) < to_lowercase(b);
  });

  std::cout << COLOR_CYAN << "--- All Unique Tags (" << unique_tags.size()
            << ") ---\n"
            << COLOR_RESET;
  int i = 1;
  for (const auto& tag : unique_tags) {
    int count = 0;
    for (const auto& card : cards) {
      for (const auto& t : card.tags) {
        if (to_lowercase(t) == to_lowercase(tag)) {
          count++;
          break;
        }
      }
    }
    std::cout << i++ << ". " << tag << " (" << count << " cards)\n";
  }
  std::cout << "\n";
}

void display_progress(const std::vector<Flashcard>& cards) {
  if (cards.empty()) {
    std::cout << COLOR_YELLOW << "No flashcards to display progress for.\n\n"
              << COLOR_RESET;
    return;
  }

  int total_cards = cards.size();
  int total_correct = 0;
  int total_incorrect = 0;
  int box_counts[6] = {0};
  
  const Flashcard* hardest_card = nullptr;
  double lowest_success = 101.0;
  int max_incorrect = -1;

  for (const auto& card : cards) {
    total_correct += card.times_correct;
    total_incorrect += card.times_incorrect;
    
    int b = card.leitner_box;
    if (b < 1) b = 1;
    if (b > 5) b = 5;
    box_counts[b]++;
    
    int card_total = card.times_correct + card.times_incorrect;
    if (card_total > 0) {
      double rate = (static_cast<double>(card.times_correct) / card_total) * 100.0;
      if (rate < lowest_success) {
        lowest_success = rate;
        hardest_card = &card;
        max_incorrect = card.times_incorrect;
      } else if (rate == lowest_success && card.times_incorrect > max_incorrect) {
        hardest_card = &card;
        max_incorrect = card.times_incorrect;
      }
    }
  }

  int total_reviews = total_correct + total_incorrect;
  double overall_success = (total_reviews == 0) ? 0.0 : (static_cast<double>(total_correct) / total_reviews) * 100.0;

  std::cout << COLOR_CYAN << "==================================================\n"
            << "                 DECK STATISTICS                  \n"
            << "==================================================\n" << COLOR_RESET
            << "  Total Cards:          " << total_cards << "\n"
            << "  Overall Success Rate: " << std::fixed << std::setprecision(2) << overall_success << "%\n"
            << "  Total Reviews:        " << total_reviews << " (" << total_correct << " Correct, " << total_incorrect << " Incorrect)\n\n";

  std::cout << COLOR_CYAN << "--- Leitner Box Distribution ---\n" << COLOR_RESET;
  for (int b = 1; b <= 5; ++b) {
    int count = box_counts[b];
    int bar_width = 10;
    int pos = (total_cards > 0) ? (count * bar_width) / total_cards : 0;
    std::string box_label = "  Box " + std::to_string(b) + ": ";
    if (b == 1) box_label = "  Box 1 (Weakest): ";
    if (b == 5) box_label = "  Box 5 (Mastered):";
    
    std::cout << std::left << std::setw(19) << box_label << "[";
    for (int i = 0; i < bar_width; ++i) {
      if (i < pos) std::cout << "█";
      else std::cout << "░";
    }
    std::cout << "] " << count << " cards (" << (total_cards > 0 ? (count * 100 / total_cards) : 0) << "%)\n";
  }
  std::cout << "\n";

  if (hardest_card != nullptr) {
    std::cout << COLOR_RED << "  Hardest Card to Remember:\n" << COLOR_RESET
              << "    Q: \"" << hardest_card->question << "\"\n"
              << "    Success rate: " << std::fixed << std::setprecision(2) << lowest_success << "% "
              << "(" << hardest_card->times_correct << " Correct, " << hardest_card->times_incorrect << " Incorrect)\n\n";
  }

  int term_width = get_terminal_width();
  int q_width = std::max(10, term_width - 55);
  const int num_width = 10;
  const int box_width = 8;

  std::cout << COLOR_CYAN << "--------------------------------------------------------------------------------\n"
            << "                                 CARD PROGRESS                                  \n"
            << "--------------------------------------------------------------------------------\n" << COLOR_RESET
            << std::left << std::setw(q_width) << "Question" << std::right
            << std::setw(num_width) << "Correct" << std::setw(num_width) << "Incorrect" 
            << std::setw(num_width) << "Success (%)" << std::setw(box_width) << "Box" << "\n"
            << std::string(term_width, '-') << "\n";

  for (const auto& card : cards) {
    int total = card.times_correct + card.times_incorrect;
    double success_rate =
        (total == 0)
            ? 0.0
            : (static_cast<double>(card.times_correct) / total) * 100.0;

    std::string q_truncated = card.question;
    if (q_truncated.length() > static_cast<size_t>(q_width)) {
      q_truncated = q_truncated.substr(0, q_width - 3) + "...";
    }

    std::cout << std::left << std::setw(q_width) << q_truncated << std::right
              << std::setw(num_width) << card.times_correct
              << std::setw(num_width) << card.times_incorrect << std::fixed
              << std::setprecision(2) << std::setw(num_width) << success_rate
              << std::setw(box_width) << card.leitner_box
              << "\n";
  }
  std::cout << "\nPress Enter to return to main menu...";
  std::string temp;
  get_input(temp);
  clear_screen();
}

void print_help() {
  std::cout << COLOR_YELLOW
            << "Commands:\n"
               "1 - Add flashcard\n"
               "2 - Review flashcards (Choose All, by Tags, by Difficulty, or by Leitner Box)\n"
               "3 - Manage flashcards (list/edit/delete)\n"
               "4 - Display progress & statistics\n"
               "5 - Import flashcards (.csv/.txt)\n"
               "6 - Export flashcards (.csv)\n"
               "7 - Manage Tags (list all unique tags)\n"
               "0 - Save and exit\n"
               "h/? - Show this help screen\n"
            << COLOR_RESET << "\n";
}

int main(int argc, char* argv[]) {
  std::string filename = FILENAME;
  if (argc > 1) {
    filename = argv[1];
  }

  std::vector<Flashcard> cards = load_flashcards(filename);
  if (cards.empty()) {
    std::ifstream check_file(filename);
    if (!check_file.is_open()) {
      std::cout << COLOR_YELLOW << "Creating a new flashcard deck: " << filename << COLOR_RESET << "\n\n";
    }
  } else {
    std::cout << COLOR_GREEN << "Loaded " << cards.size() << " flashcards from " << filename << COLOR_RESET << "\n\n";
  }

  try {
    while (true) {
      std::cout << COLOR_CYAN << "1. Add flashcard\n"
                << "2. Review flashcards\n"
                << "3. Manage flashcards\n"
                << "4. Display progress\n"
                << "5. Import flashcards\n"
                << "6. Export flashcards\n"
                << "7. List unique Tags\n"
                << COLOR_RESET COLOR_YELLOW << "0. Save and exit\n"
                << "h/? Help\n"
                << "Choose: " << COLOR_RESET;
      std::string input;
      get_input(input);
      clear_screen();

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
        get_input(import_path);
        import_flashcards(cards, import_path);
      } else if (input == "6") {
        std::cout << "Enter export file path: ";
        std::string export_path;
        get_input(export_path);
        export_flashcards(cards, export_path);
      } else if (input == "7") {
        list_all_unique_tags(cards);
      } else if (input == "0") {
        save_flashcards(filename, cards);
        std::cout << COLOR_GREEN << "Flashcards saved. Goodbye!\n" << COLOR_RESET;
        break;
      } else if (input == "h" || input == "?") {
        print_help();
      } else {
        std::cout << COLOR_RED << "Invalid choice. Please try again.\n"
                  << COLOR_RESET;
      }
    }
  } catch (const std::exception& e) {
    save_flashcards(filename, cards);
    std::cout << COLOR_GREEN << "\nInput stream closed/EOF. Flashcards saved. Goodbye!\n" << COLOR_RESET;
  }
  return 0;
}
