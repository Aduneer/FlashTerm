#include "ui.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "date.h"
#include "event.h"
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

// `number` is the card's position in the deck, not its position in whatever
// list is being shown, so the number stays the same after a search narrows it.
void print_card_row(const Flashcard& card, std::size_t number, int today_days) {
  std::cout << number << ". " << card.question << " - " << card.answer;
  if (!card.tags.empty()) {
    std::cout << " [Tags: " << card.tags_to_string() << "]";
  }
  std::cout << " (Box " << card.leitner_box << ", due "
            << describe_due(card.due_date, today_days) << ")\n";
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

std::string read_choice() {
  if (!interactive()) {
    std::string line;
    read_line(line);
    return trim(line);
  }

  // std::cout is line-buffered to a terminal, and read_key() reads the file
  // descriptor directly rather than through the stream layer, so nothing
  // flushes on its behalf the way std::cin does for read_line. Without this,
  // every prompt that does not end in a newline — which is all of them, since
  // the cursor is meant to sit after "Choose: " — stays invisible in the
  // buffer while the app blocks waiting for the key it just asked for.
  std::cout << std::flush;

  const int key = read_key();
  if (key == kEndOfInput) {
    throw std::runtime_error("end of input");
  }
  // Raw mode suppressed the echo, so the keystroke is printed here instead.
  // Without this the menu would silently jump and leave no record of what was
  // pressed in the scrollback.
  if (key == '\r' || key == '\n') {
    std::cout << "\n" << std::flush;
    return "";
  }
  std::cout << static_cast<char>(key) << "\n" << std::flush;
  return std::string(1, static_cast<char>(key));
}

std::string legend(const std::vector<KeyHint>& hints, std::size_t width) {
  // Broken between hints rather than left to the terminal, which breaks words:
  // the prompt after an answer reaches 103 columns once audio is offered, and
  // an 80-column terminal turned that into "[u] undo this a / nswer". It was
  // 79 columns before audio existed, which is to say it fit by one column and
  // nobody had noticed how close it was.
  //
  // Not wrap() from text.h, because these strings carry colour escapes and
  // those are bytes with no width. Measuring each hint before it is painted
  // sidesteps the question entirely.
  if (width == 0) width = terminal_width();
  const std::string separator = "   ";

  std::string out;
  std::size_t column = 0;
  for (const auto& hint : hints) {
    const std::size_t size =
        display_width("[" + std::string(hint.key) + "] " + hint.action);
    if (column == 0) {
      // Nothing to do: the first hint on a line goes at the margin however
      // wide it is, since moving it elsewhere would not help.
    } else if (column + separator.size() + size > width) {
      out += "\n";
      column = 0;
    } else {
      out += separator;
      column += separator.size();
    }
    out += std::string(color::cyan) + "[" + hint.key + "]" + color::reset + " " +
           hint.action;
    column += size;
  }
  return out;
}

void print_menu(const std::string& title, const std::vector<MenuItem>& items) {
  std::cout << color::cyan << "\n--- " << title << " ---\n" << color::reset;
  for (const auto& item : items) {
    std::cout << color::cyan << "[" << item.key << "]" << color::reset << " "
              << item.label << "\n";
  }
}

void print_prompt() { std::cout << color::yellow << "> " << color::reset; }

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
  std::cout << color::cyan << "\n--- Add a Flashcard ---\n"
            << color::reset << "Question, or Enter to cancel.\n";
  print_prompt();
  std::string question;
  read_line(question);
  // Enter on its own backs out, matching every other prompt that takes a whole
  // line. "q" still works, because it used to be the only way out and someone
  // will have it in their fingers.
  if (trim(question).empty() || to_lowercase(trim(question)) == "q") {
    std::cout << color::yellow << "Cancelled.\n\n" << color::reset;
    return;
  }

  const std::string answer = prompt(
      "Enter answer (separate alternatives with |, e.g. std::vector|vector): ");
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
    print_card_row(cards[i], i + 1, today_days);
  }
  std::cout << "\n";
}

void edit_card_fields(Flashcard& card) {
  const std::string question =
      prompt("Enter new question (current: " + card.question + "): ");
  const std::string answer =
      prompt("Enter new answer (current: " + card.answer +
             ") [use | to accept alternatives]: ");
  const std::string tags_str =
      prompt("Enter new tags (semicolon;separated, current: " +
             card.tags_to_string() + "): ");
  const std::string audio =
      prompt("Enter audio path, relative to the deck (current: " +
             (card.audio.empty() ? std::string("none") : card.audio) +
             ") [\"none\" to clear]: ");
  const std::string picture =
      prompt("Enter image path, relative to the deck (current: " +
             (card.image.empty() ? std::string("none") : card.image) +
             ") [\"none\" to clear]: ");

  if (!question.empty()) card.question = question;
  if (!answer.empty()) card.answer = answer;
  if (!tags_str.empty()) card.tags = split(tags_str, ';');
  // Every other field here keeps its value on a bare Enter, which leaves no
  // way to take a path back off a card; "none" is that way.
  if (!audio.empty()) {
    card.audio = (to_lowercase(audio) == "none") ? std::string() : audio;
  }
  if (!picture.empty()) {
    card.image = (to_lowercase(picture) == "none") ? std::string() : picture;
  }
}

namespace {
// Asks for a search term and lists what it matched, returning the deck
// positions shown. Empty means there is nothing to pick from and the caller
// should give up: either the deck is empty or nothing matched.
std::vector<std::size_t> list_matching(const Deck& deck) {
  if (deck.empty()) {
    std::cout << color::yellow << "No flashcards to display.\n\n"
              << color::reset;
    return {};
  }

  const std::string query = trim(prompt(
      std::string(color::yellow) +
      "Search question, answer or tags (Enter to list all): " + color::reset));
  const std::vector<std::size_t> matches = deck.find(query);
  // The prompt above leaves the cursor mid-line, which only the terminal's echo
  // of your Enter hides. Piped input has no echo, so break the line here.
  std::cout << "\n";

  if (matches.empty()) {
    std::cout << color::yellow << "Nothing matches \"" << query << "\".\n\n"
              << color::reset;
    return {};
  }

  const int today_days = today();
  for (const std::size_t i : matches) {
    print_card_row(deck.cards()[i], i + 1, today_days);
  }
  if (!query.empty()) {
    std::cout << color::cyan << "Showing " << matches.size() << " of "
              << count_label(static_cast<int>(deck.size()), "card", "cards")
              << ". Numbers are deck positions.\n"
              << color::reset;
  }
  std::cout << "\n";
  return matches;
}

// Reads a card number and checks it against what was actually listed, so a
// narrowed list can never be used to edit or delete a card it did not show.
bool pick_listed(const std::vector<std::size_t>& listed, const char* verb,
                 std::size_t* out) {
  std::cout << "Enter the number of the flashcard to " << verb << ": ";
  const int index = read_int();
  if (index <= 0) {
    std::cout << color::red << "Invalid index.\n\n" << color::reset;
    return false;
  }

  const std::size_t position = static_cast<std::size_t>(index - 1);
  if (std::find(listed.begin(), listed.end(), position) == listed.end()) {
    std::cout << color::red << "Card " << index
              << " is not in the list above.\n\n"
              << color::reset;
    return false;
  }
  *out = position;
  return true;
}

void edit_flashcard(Deck& deck) {
  const std::vector<std::size_t> listed = list_matching(deck);
  if (listed.empty()) return;

  std::size_t position = 0;
  if (!pick_listed(listed, "edit", &position)) return;

  edit_card_fields(deck.cards()[position]);
  autosave(deck);
  std::cout << color::green << "Flashcard updated!\n\n" << color::reset;
}

void delete_flashcard(Deck& deck) {
  const std::vector<std::size_t> listed = list_matching(deck);
  if (listed.empty()) return;

  std::size_t position = 0;
  if (!pick_listed(listed, "delete", &position)) return;

  deck.remove(position);
  autosave(deck);
  std::cout << color::green << "Flashcard deleted!\n\n" << color::reset;
}

void find_flashcards(const Deck& deck) { list_matching(deck); }
}  // namespace

void manage_flashcards(Deck& deck) {
  while (true) {
    print_menu("Manage Flashcards", {{"1", "List flashcards"},
                                     {"2", "Edit a flashcard"},
                                     {"3", "Delete a flashcard"},
                                     {"4", "Find flashcards"},
                                     {"q", "Back to the main menu"}});
    print_prompt();
    const std::string choice = read_choice();
    clear_screen();

    if (choice == "1") {
      list_flashcards(deck);
    } else if (choice == "2") {
      edit_flashcard(deck);
    } else if (choice == "3") {
      delete_flashcard(deck);
    } else if (choice == "4") {
      find_flashcards(deck);
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
    std::cout << i + 1 << ". " << tags[i] << " ("
              << count_label(deck.count_with_tag(tags[i]), "card", "cards")
              << ")\n";
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

  // From the log rather than the counters: these are questions about *when*
  // reviews happened, which the counters cannot answer at all.
  const LogStats log_stats = summarize(deck.log().events(), today_days);
  std::cout << "  Reviewed Today:       " << log_stats.reviewed_today << " ("
            << log_stats.correct_today << " Correct, "
            << (log_stats.reviewed_today - log_stats.correct_today)
            << " Incorrect)\n  Current Streak:       "
            << count_label(log_stats.current_streak, "day", "days") << "\n";

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
    std::cout << " " << count_label(count, "card", "cards") << " ("
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

  std::cout << "\n" << legend({{"any key", "back to the main menu"}}) << "\n";
  print_prompt();
  read_choice();
  clear_screen();
}

void print_help() {
  std::cout
      << color::cyan << "Menu\n"
      << color::reset
      << "  Menu choices take a single keypress — no Enter. Typed answers,\n"
         "  searches and file paths still read a whole line.\n"
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
      << color::cyan << "\nAt the answer prompt\n"
      << color::reset << "  "
      << legend({{"?", "hint — reveals the first letter; counts as a partial"}})
      << "\n  " << legend({{"q", "end the session and go back to the menu"}})
      << "\n  Both are ignored on a card that accepts them as answers, so a\n"
         "  deck of vim keys or regex metacharacters still works.\n"
      << color::cyan << "\nAfter answering\n"
      << color::reset << "  " << legend({{"Enter", "next card"}})
      << "\n  " << legend({{"e", "edit the card you are looking at"}})
      << "\n  " << legend({{"u", "undo the answer you just gave"}})
      << "\n  " << legend({{"q", "end the session and go back to the menu"}})
      << color::cyan << "\n\nAnywhere\n"
      << color::reset << "  " << legend({{"Ctrl+C", "save and exit"}})
      << "\n  Nothing is ever lost by leaving: the deck is saved after every\n"
         "  answer and every edit.\n\n";
}
}  // namespace FlashTerm
