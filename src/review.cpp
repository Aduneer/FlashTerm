#include "review.h"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <iostream>
#include <random>
#include <vector>

#include "answer.h"
#include "audio.h"
#include "date.h"
#include "event.h"
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

// How the session went. Which counter an outcome belongs in is asked once and
// answered here, so that the undo below cannot drift from the increment above
// -- the previous pair of hand-written conditionals was one edit away from
// crediting a card the user had just taken back.
struct SessionTally {
  int correct = 0;
  int partial = 0;
  int wrong = 0;

  int& bucket_for(Outcome outcome) {
    if (outcome == Outcome::kCorrect) return correct;
    if (outcome == Outcome::kPartial) return partial;
    return wrong;
  }
};

// FLASHTERM_SEED fixes the order cards come out in, so that the same scripted
// session produces the same session twice. It exists for tests/golden, which
// otherwise cannot review more than one card at a time -- and reviewing one
// card is exactly the case where ordering has no bugs to find.
//
// An unset or unreadable value keeps the nondeterministic seed, so no real run
// is affected, and a typo in a shell profile silently reverts to shuffling
// rather than quietly pinning every session to the same order.
std::mt19937::result_type chosen_seed() {
  const char* requested = std::getenv("FLASHTERM_SEED");
  if (requested != nullptr) {
    // errno rather than the return value: strtoul reports both "not a number"
    // and "too large" through it, and either one means fall back.
    errno = 0;
    char* end = nullptr;
    const unsigned long value = std::strtoul(requested, &end, 10);
    if (errno == 0 && end != requested && *end == '\0') {
      return static_cast<std::mt19937::result_type>(value);
    }
  }
  return std::random_device{}();
}

std::mt19937& rng() {
  static std::mt19937 generator(chosen_seed());
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
  // The one prompt in the review flow that takes more than a key, because a
  // tag filter is a list — so it says so rather than leaving the user waiting
  // for a keypress to be enough.
  std::cout << "Numbers or names, separated by commas or spaces. "
            << "Enter to confirm.\n";
  std::string tag_input =
      prompt(std::string(color::yellow) + "> " + color::reset);

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

  print_menu("Review", {{"1", "Due now  (" +
                                  count_label(stats.due_count, "card", "cards") +
                                  ")"},
                        {"2", "All cards"},
                        {"3", "By tag"},
                        {"4", "Difficult only  (incorrect > correct)"},
                        {"5", "By Leitner box, weakest first"},
                        {"q", "Back to the main menu"}});
  print_prompt();

  const std::string mode_input = read_choice();
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
    std::vector<MenuItem> tags;
    for (size_t i = 0; i < available.size(); ++i) {
      tags.push_back({std::to_string(i + 1),
                      available[i] + "  (" +
                          count_label(deck.count_with_tag(available[i]), "card",
                                      "cards") +
                          ")"});
    }
    print_menu("Select Tags", tags);
    for (const auto& tag : select_tags(available)) {
      filters->tags_lower.push_back(to_lowercase(tag));
    }
  } else if (*mode == kModeDifficult) {
    filters->difficult_only = true;
    std::cout << color::yellow
              << "Reviewing cards you find difficult (Incorrect > Correct).\n"
              << color::reset;
  } else if (*mode == kModeBox) {
    std::vector<MenuItem> boxes;
    for (int box = 1; box <= kMaxBox; ++box) {
      boxes.push_back(
          {std::to_string(box),
           "Box " + std::to_string(box) + "  (" +
               count_label(stats.box_counts[box], "card", "cards") +
               ", reviewed every " +
               count_label(interval_for_box(box), "day", "days") + ")"});
    }
    boxes.push_back({"0", "All boxes, weakest first"});
    print_menu("Select a Box", boxes);
    print_prompt();
    try {
      filters->leitner_box = std::stoi(read_choice());
    } catch (const std::invalid_argument&) {
      filters->leitner_box = 0;
    } catch (const std::out_of_range&) {
      filters->leitner_box = 0;
    }
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
  print_menu("Prompt Direction",
             {{"Enter", "Normal — question shown, you type the answer"},
              {"r", "Reversed — answer shown, you type the question"}});
  print_prompt();
  return to_lowercase(read_choice()) == "r";
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

// Plays whatever the card is currently showing, and says whether anything came
// out. In a reversed session what is showing is the answer, and the recording
// is deliberately skipped there: the audio column holds a reading of the
// question, which is the very thing being asked for.
bool play_prompt(const Deck& deck, const Flashcard& card, bool reversed) {
  const std::string file = reversed ? std::string() : deck.audio_path(card);
  const std::string text =
      reversed ? primary_answer(card.answer) : card.question;
  return audio::play(file, text);
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

void print_progress(size_t position, size_t total) {
  const int bar_width = 20;
  const int filled = static_cast<int>((position * bar_width) / total);

  std::cout << color::cyan << "Progress: [";
  for (int i = 0; i < bar_width; ++i) {
    std::cout << (i < filled ? "█" : "░");
  }
  std::cout << "] " << (position * 100) / total << "% (" << position << "/"
            << total << " cards)\n"
            << color::reset;
}

// "Box 2  ·  due today  ·  spanish  ·  reversed"
std::string card_summary(const Flashcard& card, int today_days, bool reversed) {
  std::string summary = "Box " + std::to_string(card.leitner_box) + "  ·  " +
                        describe_due(card.due_date, today_days);
  if (!card.tags.empty()) {
    summary += "  ·  " + card.tags_to_string();
  }
  if (reversed) {
    summary += "  ·  reversed";
  }
  return summary;
}

// The card itself, framed. Widths are measured in columns rather than bytes,
// so the right-hand border stays put on a Japanese or accented card, which is
// the whole reason this is worth drawing at all.
// Columns available inside the frame, once the "│ " and " │" are taken out.
// Capped so a very wide terminal does not stretch a three-word card across the
// whole screen, and floored so a narrow one still has something to write in.
std::size_t frame_inner_width() {
  const std::size_t frame_width = static_cast<std::size_t>(
      std::max(30, std::min(64, terminal_width() - 4)));
  return frame_width - 4;
}

// How tall the frame will be, worked out before anything is printed so the
// caller can centre it. Kept next to print_card because the two have to agree.
int count_frame_lines(const std::string& summary,
                      const std::string& prompt_line) {
  const std::size_t inner = frame_inner_width();
  const std::size_t lines =
      wrap(summary, inner).size() + wrap(prompt_line, inner - 2).size();
  // Four border and blank rows, plus the separator.
  return static_cast<int>(lines) + 5;
}

void print_card(const std::string& summary, const std::string& prompt_line) {
  const std::size_t inner = frame_inner_width();

  std::string rule;
  for (std::size_t i = 0; i < inner + 2; ++i) rule += "─";

  auto edge = [&](const char* left, const char* right) {
    std::cout << color::cyan << left << rule << right << "\n" << color::reset;
  };
  auto row = [&](const std::string& text, std::size_t indent,
                 const char* text_color) {
    std::cout << color::cyan << "│ " << color::reset << std::string(indent, ' ')
              << text_color << pad_right(text, inner - indent) << color::reset
              << color::cyan << " │\n" << color::reset;
  };

  const std::vector<std::string> summary_lines = wrap(summary, inner);
  const std::vector<std::string> prompt_lines = wrap(prompt_line, inner - 2);

  edge("┌", "┐");
  for (const auto& line : summary_lines) row(line, 0, color::reset);
  edge("├", "┤");
  row("", 0, color::reset);
  // The prompt is the one thing on screen actually worth reading, so it is
  // indented within the frame and given the emphasis colour.
  for (const auto& line : prompt_lines) row(line, 2, color::yellow);
  row("", 0, color::reset);
  edge("└", "┘");
}

// Pads the top of the screen so the card sits in the middle of the window
// rather than hugging the top. Does nothing when the card already fills the
// terminal, and nothing at all when output is not a screen, so piped output
// stays free of stray blank lines.
void centre_vertically(int content_lines) {
  if (!interactive()) return;

  // Room left below for the verdict, the new schedule and the next-action
  // prompt. Without it the card is centred beautifully right up until it is
  // answered, at which point the screen scrolls and pushes it off the top —
  // so the space that the answer is about to need is reserved in advance.
  const int room_for_the_answer = 8;
  const int padding =
      (terminal_height() - content_lines - room_for_the_answer) / 2;
  for (int i = 0; i < padding; ++i) {
    std::cout << "\n";
  }
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

// What `?` reveals: the first character, with the shape of the rest. Spaces
// are kept, so "la biblioteca" comes back as "l·  ··········" — enough to jog
// the memory and to show how long the answer is, without giving it away.
std::string hint_for(const std::string& expected) {
  const std::string answer = primary_answer(expected);
  std::string hint;
  bool first = true;
  for (std::size_t i = 0; i < answer.size();) {
    const std::size_t bytes = utf8_char_bytes(answer, i);
    const std::string glyph = answer.substr(i, bytes);
    if (glyph == " ") {
      hint += "  ";
    } else if (first) {
      hint += glyph;
      first = false;
    } else {
      hint += "·";
    }
    i += bytes;
  }
  return hint;
}

enum class Action { kContinue, kUndo, kQuit };

// Editing keeps the prompt open, so a card fixed on the spot can still have
// its answer taken back in the same breath.
Action prompt_next_action(Deck& deck, Flashcard& card) {
  const bool audio_available = audio::available();
  while (true) {
    std::vector<KeyHint> hints = {{"Enter", "next card"}};
    // The one place a reversed session can hear the question: it has just been
    // revealed, so there is nothing left to give away. This is also the moment
    // the pronunciation is worth hearing, right after finding out what the
    // word was.
    if (audio_available) hints.push_back({"a", "hear the question"});
    hints.push_back({"e", "edit this card"});
    hints.push_back({"u", "undo this answer"});
    hints.push_back({"q", "end session"});
    std::cout << "\n" << legend(hints) << "\n";
    print_prompt();
    const std::string action = to_lowercase(read_choice());

    if (action == "q") return Action::kQuit;
    if (action == "u") return Action::kUndo;
    if (audio_available && action == "a") {
      if (!audio::play(deck.audio_path(card), card.question)) {
        std::cout << color::yellow << "No audio available for this card.\n"
                  << color::reset;
      }
      continue;  // this prompt does not redraw, so the message survives
    }
    if (action == "e") {
      edit_card_fields(card);
      autosave(deck);
      std::cout << color::green << "Card updated.\n" << color::reset;
      continue;
    }
    return Action::kContinue;
  }
}

// The log is a side record, never a gate: a review that cannot be written to
// it still counts, and the user hears about it once rather than after every
// card. Returns the event id so an undo can name what it takes back.
std::string log_event(Deck& deck, const ReviewEvent& event, bool* warned) {
  std::string error;
  if (!deck.log().append(event, &error) && !*warned) {
    *warned = true;
    std::cout << color::yellow << "Review log unavailable: " << error << "\n"
              << color::reset;
  }
  return event.id;
}

std::string log_answer(Deck& deck, const Flashcard& card, bool reversed,
                       Outcome outcome, const AnswerResult& result,
                       bool* warned) {
  ReviewEvent event;
  event.id = generate_id();
  event.card_id = card.id;
  event.timestamp = now_timestamp();
  event.direction = reversed ? 'r' : 'n';
  event.outcome = outcome;
  event.box_before = result.old_box;
  event.box_after = result.new_box;
  return log_event(deck, event, warned);
}

void log_undo(Deck& deck, const Flashcard& card, const std::string& answer_id,
              bool* warned) {
  ReviewEvent event;
  event.id = generate_id();
  event.card_id = card.id;
  event.timestamp = now_timestamp();
  event.undoes = answer_id;
  log_event(deck, event, warned);
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

// `skipped` is what was left on the table by ending the session early, which
// is worth naming: leaving is a normal thing to do, and the summary should
// still tell you where you got to rather than looking like a finished session.
// Partials are named rather than folded into either neighbour. They still do
// not earn a point -- the score they get here is the score record_answer gives
// the card -- but filing them under "Incorrect" contradicted the "✅ Correct!"
// the session had just printed, and hid the one number that says how much of
// the session leant on the hint.
void print_summary(int correct, int partial, int wrong, int skipped,
                   const Deck& deck, int today_days) {
  const int total = correct + partial + wrong;
  const double percent =
      (total > 0) ? static_cast<double>(correct) * 100.0 / total : 0.0;

  std::cout << color::yellow
            << "==================================================\n"
            << (skipped > 0 ? "                  SESSION ENDED                   \n"
                            : "                 REVIEW COMPLETE                  \n")
            << "==================================================\n"
            << "  Correct:        " << correct << "\n";
  // Shown only when it happened, like "Not Reviewed" below: a session with no
  // hints in it should not have to read a row of zeroes to learn that.
  if (partial > 0) {
    std::cout << "  Partial (hint): " << partial << "\n";
  }
  std::cout << "  Incorrect:      " << wrong << "\n"
            << "  Total Reviewed: " << total << "\n"
            << "  Success Rate:   " << std::fixed << std::setprecision(2)
            << percent << "%\n";
  if (skipped > 0) {
    std::cout << "  Not Reviewed:   " << skipped << "\n";
  }
  std::cout << "  Still Due:      " << deck.due_count(today_days) << "\n"
            << "==================================================\n\n"
            << color::reset
            << legend({{"any key", "back to the main menu"}}) << "\n";
  print_prompt();
  read_choice();
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

  SessionTally tally;
  bool log_warned = false;

  size_t idx = 0;
  while (idx < matches.size()) {
    Flashcard& card = matches[idx].get();
    const std::string expected = expected_answer(card, session.reversed);
    const std::string question_label =
        session.reversed ? "Your question: " : "Your answer: ";

    // "?" asks for a hint and "q" leaves the session. Both are unambiguous
    // except on a card that actually accepts them as answers, and there the
    // answer wins — asked through the real matcher rather than a string
    // compare, so "?|question mark" is graded rather than hinted, and a vim
    // deck can still be asked what `q` does. On such a card the key simply is
    // not offered, and the legend says so; the session can still be ended from
    // the prompt after the answer, which is never ambiguous.
    const bool hint_available = !check_answer("?", expected).exact;
    const bool quit_available = !check_answer("q", expected).exact;
    // Audio is offered on the same terms, plus one more: there has to be
    // something on this machine that can make a sound. What it plays is
    // whatever is on screen, which is what keeps it from giving the answer
    // away in a reversed session.
    const bool audio_available =
        audio::available() && !check_answer("a", expected).exact;
    bool hinted = false;
    bool audio_failed = false;
    bool quit_requested = false;
    std::string typed;
    while (true) {
      clear_screen();
      const std::string summary =
          card_summary(card, today_days, session.reversed);
      const std::string shown = prompt_text(card, session.reversed);
      // Two for the progress bar and its blank line, two for the prompt line
      // and the breathing room above it.
      centre_vertically(count_frame_lines(summary, shown) + 4);

      print_progress(idx + 1, matches.size());
      std::cout << "\n";
      print_card(summary, shown);
      std::cout << "\n";
      if (hinted) {
        std::cout << color::yellow << "Hint: " << hint_for(expected) << "\n"
                  << color::reset;
      }
      // Said on the redraw rather than at the moment of failure, because the
      // redraw is what would have wiped it. A missing recording is not worth
      // interrupting a review over; it is worth not leaving the user pressing
      // a key that appears to do nothing.
      if (audio_failed) {
        std::cout << color::yellow << "No audio available for this card.\n"
                  << color::reset;
      }

      std::vector<KeyHint> hints = {{"Enter", "submit"}};
      if (audio_available) hints.push_back({"a", "play audio"});
      if (hint_available && !hinted) hints.push_back({"?", "hint"});
      if (quit_available) hints.push_back({"q", "end session"});
      std::cout << legend(hints) << "\n";

      typed = prompt(question_label);
      const std::string command = to_lowercase(trim(typed));
      if (audio_available && command == "a") {
        audio_failed = !play_prompt(deck, card, session.reversed);
        continue;  // same card, unanswered; playing is not an attempt
      }
      if (hint_available && !hinted && command == "?") {
        hinted = true;
        continue;  // same card, now with the hint on screen
      }
      if (quit_available && command == "q") {
        // Left unanswered on purpose: walking away from a card must not be
        // recorded as getting it wrong.
        quit_requested = true;
      }
      break;
    }
    if (quit_requested) break;

    const AnswerCheck check = check_answer(typed, expected);
    bool counted_correct = check.exact;
    if (counted_correct) {
      std::cout << color::green << "\n✅ Correct!" << color::reset << "\n";
    } else {
      if (check.near_miss) {
        std::cout << color::yellow
                  << "\n⚠️  Close! The correct answer is: " << check.closest
                  << "\n   (You typed: " << typed << ")\n"
                  << color::reset
                  << legend({{"y", "mark it correct"},
                             {"any other key", "count it wrong"}})
                  << "\n"
                  << color::yellow << "> " << color::reset;
        if (to_lowercase(read_choice()) == "y") {
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

    // Producing the answer only after being shown its first letter is a
    // partial, not a clean recall: it holds the box rather than advancing it.
    const Outcome outcome = !counted_correct ? Outcome::kIncorrect
                            : hinted         ? Outcome::kPartial
                                             : Outcome::kCorrect;
    if (outcome == Outcome::kPartial) {
      std::cout << color::yellow
                << "Counted as a partial — the hint means this card stays "
                   "where it is.\n"
                << color::reset;
    }

    const CardState before = capture_state(card);
    const AnswerResult result = record_answer(&card, outcome, today_days);
    ++tally.bucket_for(outcome);
    print_schedule(result);

    autosave(deck);
    // Logged as soon as it happens rather than once the user moves on, so
    // that closing the terminal at the prompt below cannot leave an answer
    // that the counters kept but the log never saw.
    const std::string answer_id =
        log_answer(deck, card, session.reversed, outcome, result, &log_warned);

    const Action action = prompt_next_action(deck, card);
    if (action == Action::kUndo) {
      restore_state(&card, before);
      --tally.bucket_for(outcome);
      autosave(deck);
      log_undo(deck, card, answer_id, &log_warned);
      continue;  // same card, asked again
    }
    ++idx;  // this card is done either way; quitting does not un-answer it
    if (action == Action::kQuit) break;
  }

  clear_screen();
  print_summary(tally.correct, tally.partial, tally.wrong,
                static_cast<int>(matches.size() - idx), deck, today_days);
}
}  // namespace FlashTerm
