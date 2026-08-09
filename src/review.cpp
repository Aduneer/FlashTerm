#include "review.h"

#include <algorithm>
#include <functional>
#include <iomanip>
#include <iostream>
#include <random>
#include <vector>

#include "terminal.h"
#include "text.h"
#include "ui.h"

namespace FlashTerm {
namespace {
constexpr int kMaxBox = 5;

using CardRefs = std::vector<std::reference_wrapper<Flashcard>>;

struct Filters {
  std::vector<std::string> tags_lower;
  bool difficult_only = false;
  int leitner_box = 0;  // 0 means every box
};

std::mt19937& rng() {
  static std::mt19937 generator(std::random_device{}());
  return generator;
}

int clamp_box(int box) { return std::min(kMaxBox, std::max(1, box)); }

bool card_has_any_tag(const Flashcard& card,
                      const std::vector<std::string>& tags_lower) {
  for (const auto& card_tag : card.tags) {
    if (std::find(tags_lower.begin(), tags_lower.end(),
                  to_lowercase(card_tag)) != tags_lower.end()) {
      return true;
    }
  }
  return false;
}

// Accepts either list positions ("1, 3") or tag names, comma or space separated.
std::vector<std::string> select_tags(const std::vector<std::string>& available) {
  std::cout << color::cyan << "\n--- Select Tags to Review ---\n"
            << color::reset;
  std::string tag_input =
      prompt(std::string(color::yellow) +
             "Enter numbers (e.g. 1, 3) or names of tags: " + color::reset);

  std::vector<std::string> inputs = split(tag_input, ',');
  if (inputs.size() == 1 && inputs[0].find(' ') != std::string::npos) {
    inputs = split(tag_input, ' ');
  }

  std::vector<std::string> chosen;
  for (const auto& raw : inputs) {
    const std::string entry = trim(raw);
    if (entry.empty()) continue;

    const bool is_number =
        std::all_of(entry.begin(), entry.end(),
                    [](unsigned char c) { return std::isdigit(c); });
    if (is_number) {
      const int idx = std::stoi(entry) - 1;
      if (idx >= 0 && idx < static_cast<int>(available.size())) {
        chosen.push_back(available[idx]);
      }
    } else {
      chosen.push_back(entry);
    }
  }
  return chosen;
}

// Returns false when the user backs out of the review menu.
bool choose_filters(const Deck& deck, Filters* filters, int* mode) {
  std::cout << color::cyan << "\n--- Review Options ---\n"
            << "1. Review ALL cards\n"
            << "2. Review by TAGS\n"
            << "3. Review DIFFICULT cards (incorrect > correct)\n"
            << "4. Review by LEITNER BOX (Focus on weaker boxes)\n"
            << color::yellow << "q. go back\n"
            << "Choose review mode: " << color::reset;

  std::string mode_input;
  read_line(mode_input);
  if (mode_input == "q" || mode_input == "Q") {
    clear_screen();
    return false;
  }

  try {
    *mode = std::stoi(trim(mode_input));
  } catch (const std::exception&) {
    *mode = 0;
  }

  if (*mode == 2) {
    const std::vector<std::string> available = deck.unique_tags();
    if (available.empty()) {
      std::cout << color::yellow
                << "No tags found across all flashcards. Defaulting to ALL "
                   "cards.\n"
                << color::reset;
      return true;
    }
    for (size_t i = 0; i < available.size(); ++i) {
      std::cout << "  " << i + 1 << ". " << available[i] << " ("
                << deck.count_with_tag(available[i]) << " cards)\n";
    }
    for (const auto& tag : select_tags(available)) {
      filters->tags_lower.push_back(to_lowercase(tag));
    }
  } else if (*mode == 3) {
    filters->difficult_only = true;
    std::cout << color::yellow
              << "Reviewing cards you find difficult (Incorrect > Correct).\n"
              << color::reset;
  } else if (*mode == 4) {
    const DeckStats stats = deck.stats();
    std::cout << color::cyan << "\n--- Select Leitner Box ---\n" << color::reset;
    for (int box = 1; box <= kMaxBox; ++box) {
      std::cout << "  " << box << ". Box " << box << " (" << stats.box_counts[box]
                << " cards)\n";
    }
    std::cout << "  0. Review ALL boxes (prioritizing lower boxes)\n"
              << color::yellow << "Choose Box: " << color::reset;
    filters->leitner_box = read_int();
    if (filters->leitner_box < 0 || filters->leitner_box > kMaxBox) {
      filters->leitner_box = 0;
    }
  } else if (*mode != 1) {
    std::cout << color::red << "Invalid review mode. Defaulting to ALL cards.\n"
              << color::reset;
    *mode = 1;
  }
  return true;
}

CardRefs collect_matches(Deck& deck, const Filters& filters) {
  CardRefs matches;
  for (auto& card : deck.cards()) {
    if (filters.difficult_only && card.times_incorrect <= card.times_correct) {
      continue;
    }
    if (filters.leitner_box > 0 && card.leitner_box != filters.leitner_box) {
      continue;
    }
    if (!filters.tags_lower.empty() &&
        !card_has_any_tag(card, filters.tags_lower)) {
      continue;
    }
    matches.emplace_back(card);
  }
  return matches;
}

// Shuffles within each box, then concatenates so weaker boxes come first.
void order_by_box(CardRefs* refs) {
  std::vector<CardRefs> boxes(kMaxBox + 1);
  for (auto ref : *refs) {
    boxes[clamp_box(ref.get().leitner_box)].push_back(ref);
  }
  refs->clear();
  for (int box = 1; box <= kMaxBox; ++box) {
    std::shuffle(boxes[box].begin(), boxes[box].end(), rng());
    refs->insert(refs->end(), boxes[box].begin(), boxes[box].end());
  }
}

void print_header(const Flashcard& card, size_t position, size_t total) {
  const int bar_width = 20;
  const int filled = static_cast<int>((position * bar_width) / total);

  std::cout << color::cyan << "Progress: [";
  for (int i = 0; i < bar_width; ++i) {
    std::cout << (i < filled ? "█" : "░");
  }
  std::cout << "] " << (position * 100) / total << "% (" << position << "/"
            << total << " cards)\n"
            << "Box: " << card.leitner_box;
  if (!card.tags.empty()) {
    std::cout << " | Tags: " << card.tags_to_string();
  }
  std::cout << "\n"
            << std::string(std::min(50, terminal_width()), '-') << "\n\n";
}

// A short answer tolerates one typo, a longer one tolerates two.
bool is_near_miss(const std::string& typed, const std::string& expected) {
  const int distance = levenshtein_distance(typed, expected);
  if (expected.length() >= 4 && expected.length() <= 8) return distance <= 1;
  if (expected.length() > 8) return distance <= 2;
  return false;
}

void promote(Flashcard& card) {
  const int old_box = card.leitner_box;
  card.leitner_box = std::min(kMaxBox, card.leitner_box + 1);
  if (card.leitner_box > old_box) {
    std::cout << color::green << "Card promoted to Box " << card.leitner_box
              << "!\n"
              << color::reset;
  }
}

void demote(Flashcard& card) {
  const int old_box = card.leitner_box;
  card.leitner_box = 1;
  if (old_box > 1) {
    std::cout << color::red << "Card demoted back to Box 1!\n" << color::reset;
  }
}

void print_summary(int correct, int wrong) {
  const int total = correct + wrong;
  const double percent =
      (total > 0) ? static_cast<double>(correct) * 100.0 / total : 0.0;

  std::cout << color::yellow
            << "==================================================\n"
            << "                 REVIEW COMPLETE                  \n"
            << "==================================================\n"
            << "  Correct:        " << correct << "\n"
            << "  Incorrect:      " << wrong << "\n"
            << "  Total Reviewed: " << total << "\n"
            << "  Success Rate:   " << std::fixed << std::setprecision(2)
            << percent << "%\n"
            << "==================================================\n\n"
            << "Press Enter to return to main menu..." << color::reset;
  std::string discard;
  read_line(discard);
  clear_screen();
}
}  // namespace

void review_flashcards(Deck& deck) {
  if (deck.empty()) {
    std::cout << color::yellow << "No flashcards to review. Add some first!\n\n"
              << color::reset;
    return;
  }

  Filters filters;
  int mode = 1;
  if (!choose_filters(deck, &filters, &mode)) {
    return;
  }

  CardRefs matches = collect_matches(deck, filters);
  if (matches.empty()) {
    std::cout << color::yellow
              << "No flashcards match the current review filters.\n\n"
              << color::reset;
    return;
  }

  if (mode == 4 && filters.leitner_box == 0) {
    order_by_box(&matches);
  } else {
    std::shuffle(matches.begin(), matches.end(), rng());
  }

  int correct_total = 0;
  int wrong_total = 0;

  for (size_t idx = 0; idx < matches.size(); ++idx) {
    Flashcard& card = matches[idx].get();
    clear_screen();
    print_header(card, idx + 1, matches.size());

    std::cout << color::cyan << "Q: " << card.question << color::reset << "\n\n";
    std::string typed = prompt("Your answer: ");

    bool counted_correct =
        normalize_answer(typed) == normalize_answer(card.answer);
    if (counted_correct) {
      std::cout << color::green << "\n✅ Correct!" << color::reset << "\n";
    } else {
      const std::string clean_typed = to_lowercase(trim(typed));
      const std::string clean_answer = to_lowercase(trim(card.answer));

      if (is_near_miss(clean_typed, clean_answer)) {
        std::cout << color::yellow
                  << "\n⚠️  Close! The correct answer is: " << card.answer
                  << "\n   (You typed: " << typed << ")\n"
                  << "Mark as correct anyway? [y/N]: " << color::reset;
        std::string override_input;
        read_line(override_input);
        if (to_lowercase(trim(override_input)) == "y") {
          std::cout << color::green << "✅ Marked as correct!" << color::reset
                    << "\n";
          counted_correct = true;
        }
      }
      if (!counted_correct) {
        std::cout << color::red << "\n❌ Incorrect! Correct answer: "
                  << card.answer << color::reset << "\n";
      }
    }

    if (counted_correct) {
      card.times_correct++;
      correct_total++;
      promote(card);
    } else {
      card.times_incorrect++;
      wrong_total++;
      demote(card);
    }

    autosave(deck);

    std::cout << "\nPress Enter to continue...";
    std::string discard;
    read_line(discard);
  }

  clear_screen();
  print_summary(correct_total, wrong_total);
}
}  // namespace FlashTerm
