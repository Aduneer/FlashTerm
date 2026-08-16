#include "review.h"

#include <algorithm>
#include <functional>
#include <iomanip>
#include <iostream>
#include <random>
#include <vector>

#include "answer.h"
#include "date.h"
#include "schedule.h"
#include "terminal.h"
#include "text.h"
#include "ui.h"

namespace FlashTerm {
namespace {
using CardRefs = std::vector<std::reference_wrapper<Flashcard>>;

enum Mode {
  kModeDue = 1,
  kModeAll = 2,
  kModeTags = 3,
  kModeDifficult = 4,
  kModeBox = 5,
};

struct Filters {
  std::vector<std::string> tags_lower;
  bool due_only = false;
  bool difficult_only = false;
  int leitner_box = 0;  // 0 means every box
};

// How this session was set up: which cards, and which way round they are asked.
struct Session {
  Filters filters;
  int mode = kModeAll;
  bool reversed = false;
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
bool choose_filters(const Deck& deck, int today_days, Filters* filters,
                    int* mode) {
  const DeckStats stats = deck.stats(today_days);

  std::cout << color::cyan << "\n--- Review Options ---\n"
            << "1. Review cards DUE now ("
            << count_label(stats.due_count, "card", "cards") << ")\n"
            << "2. Review ALL cards\n"
            << "3. Review by TAGS\n"
            << "4. Review DIFFICULT cards (incorrect > correct)\n"
            << "5. Review by LEITNER BOX (Focus on weaker boxes)\n"
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

  if (*mode == kModeDue) {
    filters->due_only = true;
  } else if (*mode == kModeTags) {
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
                << count_label(deck.count_with_tag(available[i]), "card",
                               "cards")
                << ")\n";
    }
    for (const auto& tag : select_tags(available)) {
      filters->tags_lower.push_back(to_lowercase(tag));
    }
  } else if (*mode == kModeDifficult) {
    filters->difficult_only = true;
    std::cout << color::yellow
              << "Reviewing cards you find difficult (Incorrect > Correct).\n"
              << color::reset;
  } else if (*mode == kModeBox) {
    std::cout << color::cyan << "\n--- Select Leitner Box ---\n" << color::reset;
    for (int box = 1; box <= kMaxBox; ++box) {
      std::cout << "  " << box << ". Box " << box << " ("
                << count_label(stats.box_counts[box], "card", "cards")
                << ", reviewed every "
                << interval_for_box(box) << " days)\n";
    }
    std::cout << "  0. Review ALL boxes (prioritizing lower boxes)\n"
              << color::yellow << "Choose Box: " << color::reset;
    filters->leitner_box = read_int();
    if (filters->leitner_box < 0 || filters->leitner_box > kMaxBox) {
      filters->leitner_box = 0;
    }
  } else if (*mode != kModeAll) {
    std::cout << color::red << "Invalid review mode. Defaulting to ALL cards.\n"
              << color::reset;
    *mode = kModeAll;
  }
  return true;
}

// Asked after the filters, so reversing composes with every review mode rather
// than being a mode of its own: "difficult cards, reversed" is a useful session.
bool choose_reversed() {
  std::cout << color::cyan << "\n--- Prompt Direction ---\n"
            << color::reset << color::yellow
            << "[Enter] normal (question, you type the answer)\n"
            << "     r  reversed (answer, you type the question)\n"
            << "Choose: " << color::reset;
  std::string input;
  read_line(input);
  return to_lowercase(trim(input)) == "r";
}

CardRefs collect_matches(Deck& deck, const Filters& filters, int today_days) {
  CardRefs matches;
  for (auto& card : deck.cards()) {
    if (filters.due_only && !is_due(card, today_days)) {
      continue;
    }
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

// Most overdue first, so the longest-neglected cards are never the ones cut
// short when a session is abandoned halfway.
void order_by_due(CardRefs* refs) {
  std::stable_sort(refs->begin(), refs->end(),
                   [](const Flashcard& a, const Flashcard& b) {
                     return a.due_date < b.due_date;
                   });
}

void print_header(const Flashcard& card, size_t position, size_t total,
                  int today_days, bool reversed) {
  const int bar_width = 20;
  const int filled = static_cast<int>((position * bar_width) / total);

  std::cout << color::cyan << "Progress: [";
  for (int i = 0; i < bar_width; ++i) {
    std::cout << (i < filled ? "█" : "░");
  }
  std::cout << "] " << (position * 100) / total << "% (" << position << "/"
            << total << " cards)\n"
            << "Box: " << card.leitner_box << " | Due: "
            << describe_due(card.due_date, today_days);
  if (!card.tags.empty()) {
    std::cout << " | Tags: " << card.tags_to_string();
  }
  if (reversed) {
    std::cout << " | reversed";
  }
  std::cout << "\n"
            << std::string(std::min(50, terminal_width()), '-') << "\n\n";
}

// `shown` is already the single answer to display; `others` is empty when there
// is nothing else to accept, which is always the case in a reversed session
// because a question carries no alternatives.
void print_correct_answer(const std::string& shown, const std::string& others) {
  std::cout << color::red << "\n❌ Incorrect! Correct answer: " << shown
            << color::reset;
  if (!others.empty()) {
    std::cout << color::yellow << "  (also accepted: " << others << ")"
              << color::reset;
  }
  std::cout << "\n";
}

enum class Action { kContinue, kUndo };

// Editing keeps the prompt open, so a card fixed on the spot can still have
// its answer taken back in the same breath.
Action prompt_next_action(Deck& deck, Flashcard& card) {
  while (true) {
    std::cout << "\n"
              << color::yellow
              << "[Enter] next  [e] edit this card  [u] undo this answer: "
              << color::reset;
    std::string input;
    read_line(input);
    const std::string action = to_lowercase(trim(input));

    if (action == "u") return Action::kUndo;
    if (action == "e") {
      edit_card_fields(card);
      autosave(deck);
      std::cout << color::green << "Card updated.\n" << color::reset;
      continue;
    }
    return Action::kContinue;
  }
}

void print_schedule(const AnswerResult& result) {
  if (result.new_box > result.old_box) {
    std::cout << color::green << "Card promoted to Box " << result.new_box
              << "!\n"
              << color::reset;
  } else if (result.new_box < result.old_box) {
    std::cout << color::red << "Card demoted back to Box 1!\n" << color::reset;
  }
  std::cout << color::cyan << "Next review in " << result.interval_days
            << (result.interval_days == 1 ? " day (" : " days (")
            << format_date(result.due_date) << ").\n"
            << color::reset;
}

void print_summary(int correct, int wrong, const Deck& deck, int today_days) {
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
            << "  Still Due:      " << deck.due_count(today_days) << "\n"
            << "==================================================\n\n"
            << "Press Enter to return to main menu..." << color::reset;
  std::string discard;
  read_line(discard);
  clear_screen();
}

void report_nothing_due(const Deck& deck, int today_days) {
  const DeckStats stats = deck.stats(today_days);
  std::cout << color::green << "Nothing is due right now — you are all caught up!\n"
            << color::reset;
  if (stats.next_due != kNoDate) {
    std::cout << "The next card is due " << describe_due(stats.next_due, today_days)
              << " (" << format_date(stats.next_due) << ").\n";
  }
  std::cout << "\n";
}
}  // namespace

std::string prompt_text(const Flashcard& card, bool reversed) {
  return reversed ? primary_answer(card.answer) : card.question;
}

std::string expected_answer(const Flashcard& card, bool reversed) {
  return reversed ? card.question : card.answer;
}

void review_flashcards(Deck& deck) {
  if (deck.empty()) {
    std::cout << color::yellow << "No flashcards to review. Add some first!\n\n"
              << color::reset;
    return;
  }

  // Fixed for the whole session, so a review running past midnight stays
  // internally consistent.
  const int today_days = today();

  Session session;
  if (!choose_filters(deck, today_days, &session.filters, &session.mode)) {
    return;
  }
  session.reversed = choose_reversed();
  const Filters& filters = session.filters;

  CardRefs matches = collect_matches(deck, filters, today_days);
  if (matches.empty()) {
    if (filters.due_only) {
      report_nothing_due(deck, today_days);
    } else {
      std::cout << color::yellow
                << "No flashcards match the current review filters.\n\n"
                << color::reset;
    }
    return;
  }

  std::shuffle(matches.begin(), matches.end(), rng());
  if (session.mode == kModeBox && filters.leitner_box == 0) {
    order_by_box(&matches);
  } else if (filters.due_only) {
    order_by_due(&matches);
  }

  int correct_total = 0;
  int wrong_total = 0;

  size_t idx = 0;
  while (idx < matches.size()) {
    Flashcard& card = matches[idx].get();
    const std::string expected = expected_answer(card, session.reversed);
    clear_screen();
    print_header(card, idx + 1, matches.size(), today_days, session.reversed);

    std::cout << color::cyan << (session.reversed ? "A: " : "Q: ")
              << prompt_text(card, session.reversed) << color::reset << "\n\n";
    const std::string typed =
        prompt(session.reversed ? "Your question: " : "Your answer: ");

    const AnswerCheck check = check_answer(typed, expected);
    bool counted_correct = check.exact;
    if (counted_correct) {
      std::cout << color::green << "\n✅ Correct!" << color::reset << "\n";
    } else {
      if (check.near_miss) {
        std::cout << color::yellow
                  << "\n⚠️  Close! The correct answer is: " << check.closest
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
        // A question has no "|" alternatives, so a reversed session shows it
        // verbatim rather than treating any pipe in it as a separator.
        print_correct_answer(
            session.reversed ? card.question : primary_answer(card.answer),
            session.reversed ? std::string() : alternatives_summary(card.answer));
      }
    }

    const CardState before = capture_state(card);
    const AnswerResult result = record_answer(&card, counted_correct, today_days);
    (counted_correct ? correct_total : wrong_total)++;
    print_schedule(result);

    autosave(deck);

    if (prompt_next_action(deck, card) == Action::kUndo) {
      restore_state(&card, before);
      (counted_correct ? correct_total : wrong_total)--;
      autosave(deck);
      continue;  // same card, asked again
    }
    ++idx;
  }

  clear_screen();
  print_summary(correct_total, wrong_total, deck, today_days);
}
}  // namespace FlashTerm
