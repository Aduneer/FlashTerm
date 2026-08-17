// Minimal assert-style harness: no framework, just `make test`.
#include <clocale>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "answer.h"
#include "cli.h"
#include "date.h"
#include "deck.h"
#include "event.h"
#include "review.h"
#include "schedule.h"
#include "text.h"

using namespace FlashTerm;

namespace {
int g_checks = 0;
int g_failures = 0;

template <typename A, typename B>
void expect_eq(const A& actual, const B& expected, const char* expr, int line) {
  ++g_checks;
  if (actual == expected) return;
  ++g_failures;
  std::cout << "FAIL (line " << line << "): " << expr << "\n  expected: "
            << expected << "\n  actual:   " << actual << "\n";
}

void expect_true(bool value, const char* expr, int line) {
  ++g_checks;
  if (value) return;
  ++g_failures;
  std::cout << "FAIL (line " << line << "): " << expr << " was false\n";
}

#define EXPECT_EQ(actual, expected) \
  expect_eq((actual), (expected), #actual, __LINE__)
#define EXPECT_TRUE(value) expect_true((value), #value, __LINE__)

std::string temp_path(const std::string& name) {
  return "build/test-" + name;
}

void test_trim_and_case() {
  EXPECT_EQ(trim("  hello  "), std::string("hello"));
  EXPECT_EQ(trim("\t\r\n"), std::string(""));
  EXPECT_EQ(trim(""), std::string(""));
  EXPECT_EQ(to_lowercase("MiXeD"), std::string("mixed"));
}

void test_normalize_answer() {
  EXPECT_EQ(normalize_answer("  Std::Unique_Ptr "),
            std::string("std::unique_ptr"));
  // Internal whitespace is dropped, so spacing never fails an answer.
  EXPECT_EQ(normalize_answer("Albert  Einstein"),
            normalize_answer("albert einstein"));
  EXPECT_EQ(normalize_answer(""), std::string(""));
}

void test_count_label() {
  EXPECT_EQ(count_label(1, "card", "cards"), std::string("1 card"));
  EXPECT_EQ(count_label(0, "card", "cards"), std::string("0 cards"));
  EXPECT_EQ(count_label(2, "card", "cards"), std::string("2 cards"));
  EXPECT_EQ(count_label(142, "day", "days"), std::string("142 days"));
  // Only exactly one is singular; a negative count is not a special case.
  EXPECT_EQ(count_label(-1, "card", "cards"), std::string("-1 cards"));
}

void test_split() {
  const std::vector<std::string> tags = split("cpp; memory ;pointers", ';');
  EXPECT_EQ(tags.size(), size_t{3});
  EXPECT_EQ(tags[0], std::string("cpp"));
  EXPECT_EQ(tags[1], std::string("memory"));
  EXPECT_EQ(tags[2], std::string("pointers"));
  EXPECT_EQ(split("", ';').size(), size_t{0});
}

void test_levenshtein() {
  EXPECT_EQ(levenshtein_distance("kitten", "sitting"), 3);
  EXPECT_EQ(levenshtein_distance("same", "same"), 0);
  EXPECT_EQ(levenshtein_distance("", "abc"), 3);
  EXPECT_EQ(levenshtein_distance("abc", ""), 3);
}

void test_csv_roundtrip() {
  EXPECT_EQ(escape_csv_field("plain"), std::string("plain"));
  EXPECT_EQ(escape_csv_field("a,b"), std::string("\"a,b\""));
  EXPECT_EQ(escape_csv_field("say \"hi\""), std::string("\"say \"\"hi\"\"\""));

  const std::vector<std::string> fields =
      parse_csv_line("\"a,b\",\"say \"\"hi\"\"\",tag");
  EXPECT_EQ(fields.size(), size_t{3});
  EXPECT_EQ(fields[0], std::string("a,b"));
  EXPECT_EQ(fields[1], std::string("say \"hi\""));
  EXPECT_EQ(fields[2], std::string("tag"));

  // Trailing empty field must survive the round trip.
  EXPECT_EQ(parse_csv_line("a,b,").size(), size_t{3});
}

void test_utf8_columns() {
  EXPECT_EQ(display_width("abc"), size_t{3});
  EXPECT_EQ(display_width("café"), size_t{4});
  EXPECT_EQ(display_width(""), size_t{0});

  // Truncation must land on a character boundary, never mid-sequence.
  EXPECT_EQ(truncate("short", 10), std::string("short"));
  EXPECT_EQ(truncate("caféfé", 5), std::string("ca..."));
  EXPECT_EQ(truncate("ééééé", 4), std::string("é..."));
  EXPECT_EQ(display_width(truncate("ééééé", 4)), size_t{4});

  // Padding counts columns, which is what std::setw gets wrong.
  EXPECT_EQ(display_width(pad_right("café", 8)), size_t{8});
  EXPECT_EQ(pad_right("toolong", 3), std::string("toolong"));

  // Wide glyphs need a UTF-8 locale; without one, widths degrade to 1 per
  // character rather than being wrong in a way that breaks the table.
  if (display_width("日") == 2) {
    EXPECT_EQ(display_width("日本語"), size_t{6});
    EXPECT_EQ(truncate("日本語です", 7), std::string("日本..."));
    EXPECT_EQ(display_width(truncate("日本語です", 7)), size_t{7});
  } else {
    std::cout << "note: no UTF-8 locale, skipping wide-glyph width checks\n";
  }
}

void test_accepted_answers() {
  const std::vector<std::string> single = accepted_answers("Paris");
  EXPECT_EQ(single.size(), size_t{1});
  EXPECT_EQ(single[0], std::string("Paris"));

  const std::vector<std::string> many =
      accepted_answers("std::unique_ptr | unique_ptr|smart pointer");
  EXPECT_EQ(many.size(), size_t{3});
  EXPECT_EQ(many[0], std::string("std::unique_ptr"));
  EXPECT_EQ(many[1], std::string("unique_ptr"));
  EXPECT_EQ(many[2], std::string("smart pointer"));

  EXPECT_EQ(primary_answer("a|b|c"), std::string("a"));
  EXPECT_EQ(alternatives_summary("a|b|c"), std::string("b, c"));
  EXPECT_EQ(alternatives_summary("only"), std::string(""));

  // Degenerate input must still yield something to compare against.
  EXPECT_EQ(accepted_answers("").size(), size_t{1});
  EXPECT_EQ(accepted_answers("||").size(), size_t{1});
}

void test_check_answer_exact() {
  // Any listed alternative counts, and reports which one matched.
  EXPECT_TRUE(check_answer("unique_ptr", "std::unique_ptr|unique_ptr").exact);
  EXPECT_EQ(check_answer("unique_ptr", "std::unique_ptr|unique_ptr").closest,
            std::string("unique_ptr"));
  EXPECT_TRUE(check_answer("std::unique_ptr", "std::unique_ptr|unique_ptr").exact);

  // Normalisation still applies to every alternative.
  EXPECT_TRUE(check_answer("  ALBERT einstein ", "Albert Einstein|Einstein").exact);
  EXPECT_TRUE(check_answer("smartpointer", "smart pointer|other").exact);

  EXPECT_TRUE(!check_answer("Berlin", "Paris|Lyon").exact);
  EXPECT_TRUE(!check_answer("", "Paris").exact);

  // A single-answer card behaves exactly as before.
  EXPECT_TRUE(check_answer("paris", "Paris").exact);
}

void test_check_answer_near_miss() {
  // One typo in a mid-length answer is offered as an override.
  const AnswerCheck typo = check_answer("Pariss", "Paris");
  EXPECT_TRUE(!typo.exact);
  EXPECT_TRUE(typo.near_miss);
  EXPECT_EQ(typo.closest, std::string("Paris"));

  // Two typos are too many at that length.
  EXPECT_TRUE(!check_answer("Parsss", "Paris").near_miss);
  // Longer answers get more slack.
  EXPECT_TRUE(check_answer("Albert Einstien", "Albert Einstein").near_miss);
  // Short answers must be exact, since one edit is a different answer.
  EXPECT_TRUE(!check_answer("4", "3").near_miss);
  EXPECT_TRUE(!check_answer("cat", "car").near_miss);

  // The nearest alternative is the one reported back.
  const AnswerCheck nearest = check_answer("Lyons", "Paris|Lyon|Marseille");
  EXPECT_TRUE(nearest.near_miss);
  EXPECT_EQ(nearest.closest, std::string("Lyon"));

  // A wrong answer that is close to nothing stays a plain miss.
  EXPECT_TRUE(!check_answer("Tokyo", "Paris|Lyon").near_miss);
}

void test_undo_restores_card_exactly() {
  const int now = days_from_civil(2026, 8, 9);

  Flashcard card("Q", "A", {}, 4, 1, 3);
  card.last_reviewed = days_from_civil(2026, 8, 2);
  card.due_date = days_from_civil(2026, 8, 9);

  // A wrong answer, then undo, must leave no trace at all.
  const CardState before = capture_state(card);
  record_answer(&card, false, now);
  EXPECT_EQ(card.leitner_box, 1);
  EXPECT_EQ(card.times_incorrect, 2);

  restore_state(&card, before);
  EXPECT_EQ(card.times_correct, 4);
  EXPECT_EQ(card.times_incorrect, 1);
  EXPECT_EQ(card.leitner_box, 3);
  EXPECT_EQ(card.last_reviewed, days_from_civil(2026, 8, 2));
  EXPECT_EQ(card.due_date, days_from_civil(2026, 8, 9));

  // The same holds for a correct answer on a never-reviewed card, where the
  // dates have to go back to "never" rather than to some default.
  Flashcard fresh("Q", "A");
  const CardState fresh_before = capture_state(fresh);
  record_answer(&fresh, true, now);
  EXPECT_TRUE(fresh.due_date != kNoDate);

  restore_state(&fresh, fresh_before);
  EXPECT_EQ(fresh.times_correct, 0);
  EXPECT_EQ(fresh.leitner_box, 1);
  EXPECT_EQ(fresh.last_reviewed, kNoDate);
  EXPECT_EQ(fresh.due_date, kNoDate);
  EXPECT_TRUE(is_due(fresh, now));
}

void test_alternatives_survive_saving() {
  const std::string path = temp_path("alternatives.txt");
  std::remove(path.c_str());

  Deck deck(path);
  // Commas and pipes together, to be sure the CSV layer leaves them alone.
  deck.add(Flashcard("Name a smart pointer, any one",
                     "std::unique_ptr|unique_ptr|shared_ptr", {"cpp"}));
  EXPECT_TRUE(deck.save());

  Deck reloaded(path);
  EXPECT_TRUE(reloaded.load());
  EXPECT_EQ(reloaded.cards()[0].answer,
            std::string("std::unique_ptr|unique_ptr|shared_ptr"));
  EXPECT_TRUE(check_answer("shared_ptr", reloaded.cards()[0].answer).exact);

  std::remove(path.c_str());
}

void test_civil_dates() {
  EXPECT_EQ(days_from_civil(1970, 1, 1), 0);
  EXPECT_EQ(days_from_civil(1970, 1, 2), 1);
  EXPECT_EQ(days_from_civil(1969, 12, 31), -1);
  EXPECT_EQ(days_from_civil(2026, 8, 9), 20674);

  // Leap-year handling must be exact or intervals drift.
  EXPECT_EQ(days_from_civil(2024, 3, 1) - days_from_civil(2024, 2, 28), 2);
  EXPECT_EQ(days_from_civil(2023, 3, 1) - days_from_civil(2023, 2, 28), 1);
  EXPECT_EQ(days_from_civil(2000, 3, 1) - days_from_civil(2000, 2, 28), 2);
  EXPECT_EQ(days_from_civil(1900, 3, 1) - days_from_civil(1900, 2, 28), 1);

  // Round-trip a long span of days through both directions.
  bool round_trips = true;
  for (int day = -40000; day <= 40000; day += 7) {
    int year = 0;
    unsigned month = 0;
    unsigned dom = 0;
    civil_from_days(day, &year, &month, &dom);
    if (days_from_civil(year, month, dom) != day) {
      round_trips = false;
      break;
    }
  }
  EXPECT_TRUE(round_trips);
}

void test_date_formatting() {
  EXPECT_EQ(format_date(0), std::string("1970-01-01"));
  EXPECT_EQ(format_date(days_from_civil(2026, 8, 9)), std::string("2026-08-09"));
  EXPECT_EQ(format_date(kNoDate), std::string(""));

  EXPECT_EQ(parse_date("2026-08-09"), days_from_civil(2026, 8, 9));
  EXPECT_EQ(parse_date(""), kNoDate);
  EXPECT_EQ(parse_date("not-a-date"), kNoDate);
  EXPECT_EQ(parse_date("2026-8-9"), kNoDate);      // must be zero padded
  EXPECT_EQ(parse_date("2026-13-01"), kNoDate);    // no thirteenth month
  EXPECT_EQ(parse_date("2026-02-30"), kNoDate);    // date does not exist
  EXPECT_EQ(parse_date("2024-02-29"), days_from_civil(2024, 2, 29));
}

void test_describe_due() {
  const int now = days_from_civil(2026, 8, 9);
  EXPECT_EQ(describe_due(kNoDate, now), std::string("new"));
  EXPECT_EQ(describe_due(now, now), std::string("today"));
  EXPECT_EQ(describe_due(now + 1, now), std::string("tomorrow"));
  EXPECT_EQ(describe_due(now + 5, now), std::string("in 5 days"));
  EXPECT_EQ(describe_due(now - 1, now), std::string("overdue by 1 day"));
  EXPECT_EQ(describe_due(now - 3, now), std::string("overdue by 3 days"));

  EXPECT_EQ(describe_due_short(kNoDate, now), std::string("new"));
  EXPECT_EQ(describe_due_short(now, now), std::string("due"));
  EXPECT_EQ(describe_due_short(now - 9, now), std::string("due"));
  EXPECT_EQ(describe_due_short(now + 5, now), std::string("5d"));
}

void test_is_due() {
  const int now = days_from_civil(2026, 8, 9);
  Flashcard card("Q", "A");

  // A card that has never been scheduled is due immediately.
  EXPECT_TRUE(is_due(card, now));

  card.due_date = now;
  EXPECT_TRUE(is_due(card, now));
  card.due_date = now - 1;
  EXPECT_TRUE(is_due(card, now));
  card.due_date = now + 1;
  EXPECT_TRUE(!is_due(card, now));
}

void test_record_answer_schedules() {
  const int now = days_from_civil(2026, 8, 9);

  // Correct answers walk the card up the boxes with widening intervals.
  Flashcard card("Q", "A");
  int expected_boxes[] = {2, 3, 4, 5, 5};
  bool schedule_matches = true;
  for (int step = 0; step < 5; ++step) {
    const AnswerResult result = record_answer(&card, true, now + step);
    if (card.leitner_box != expected_boxes[step]) schedule_matches = false;
    if (result.interval_days != interval_for_box(expected_boxes[step])) {
      schedule_matches = false;
    }
    if (card.due_date != now + step + result.interval_days) {
      schedule_matches = false;
    }
    if (card.last_reviewed != now + step) schedule_matches = false;
  }
  EXPECT_TRUE(schedule_matches);
  EXPECT_EQ(card.leitner_box, 5);
  EXPECT_EQ(card.times_correct, 5);

  // Box 5 stays at the longest interval rather than running off the array.
  EXPECT_EQ(interval_for_box(5), 30);
  EXPECT_EQ(interval_for_box(99), 30);
  EXPECT_EQ(interval_for_box(0), 1);

  // One wrong answer drops the card to box 1 and back to a one-day interval.
  const AnswerResult missed = record_answer(&card, false, now + 10);
  EXPECT_EQ(missed.old_box, 5);
  EXPECT_EQ(missed.new_box, 1);
  EXPECT_EQ(card.leitner_box, 1);
  EXPECT_EQ(card.times_incorrect, 1);
  EXPECT_EQ(card.due_date, now + 11);
  EXPECT_EQ(missed.interval_days, 1);
}

void test_due_counts_and_next_due() {
  const int now = days_from_civil(2026, 8, 9);
  Deck deck(temp_path("due.txt"));

  Flashcard fresh("New", "A");  // never reviewed, so due
  Flashcard overdue("Late", "B");
  overdue.due_date = now - 4;
  Flashcard soon("Soon", "C");
  soon.due_date = now + 2;
  Flashcard later("Later", "D");
  later.due_date = now + 9;

  deck.add(fresh);
  deck.add(overdue);
  deck.add(soon);
  deck.add(later);

  EXPECT_EQ(deck.due_count(now), 2);

  const DeckStats stats = deck.stats(now);
  EXPECT_EQ(stats.due_count, 2);
  EXPECT_EQ(stats.next_due, now + 2);

  // Far enough in the future and everything has come due.
  EXPECT_EQ(deck.due_count(now + 30), 4);
  EXPECT_EQ(deck.stats(now + 30).next_due, kNoDate);
}

void test_card_csv() {
  Flashcard card("What is 2+2?", "4", {"math", "basics"}, 3, 1, 4);
  card.last_reviewed = days_from_civil(2026, 8, 1);
  card.due_date = days_from_civil(2026, 8, 15);
  const std::string line = card_to_csv(card);

  Flashcard parsed("", "");
  EXPECT_TRUE(card_from_csv(line, &parsed));
  EXPECT_EQ(parsed.question, card.question);
  EXPECT_EQ(parsed.answer, card.answer);
  EXPECT_EQ(parsed.tags_to_string(), std::string("math;basics"));
  EXPECT_EQ(parsed.times_correct, 3);
  EXPECT_EQ(parsed.times_incorrect, 1);
  EXPECT_EQ(parsed.leitner_box, 4);
  EXPECT_EQ(parsed.last_reviewed, card.last_reviewed);
  EXPECT_EQ(parsed.due_date, card.due_date);
  // Dates are stored readably rather than as opaque numbers.
  EXPECT_TRUE(line.find("2026-08-01,2026-08-15") != std::string::npos);

  // A bare question/answer pair is still a valid card.
  Flashcard minimal("", "");
  EXPECT_TRUE(card_from_csv("Q,A", &minimal));
  EXPECT_EQ(minimal.leitner_box, 1);
  EXPECT_EQ(minimal.due_date, kNoDate);

  // Six-field decks written before scheduling existed load as due now,
  // keeping their boxes and scores.
  Flashcard legacy("", "");
  EXPECT_TRUE(card_from_csv("Q,A,tag,4,2,3", &legacy));
  EXPECT_EQ(legacy.leitner_box, 3);
  EXPECT_EQ(legacy.times_correct, 4);
  EXPECT_EQ(legacy.last_reviewed, kNoDate);
  EXPECT_EQ(legacy.due_date, kNoDate);
  EXPECT_TRUE(is_due(legacy, today()));

  // A corrupt date is treated as unscheduled rather than trusted.
  Flashcard bad_date("", "");
  EXPECT_TRUE(card_from_csv("Q,A,,0,0,1,garbage,2026-99-99", &bad_date));
  EXPECT_EQ(bad_date.last_reviewed, kNoDate);
  EXPECT_EQ(bad_date.due_date, kNoDate);

  // Out-of-range and malformed boxes are clamped rather than trusted.
  Flashcard clamped("", "");
  EXPECT_TRUE(card_from_csv("Q,A,,0,0,99", &clamped));
  EXPECT_EQ(clamped.leitner_box, 5);
  EXPECT_TRUE(card_from_csv("Q,A,,0,0,0", &clamped));
  EXPECT_EQ(clamped.leitner_box, 1);
  EXPECT_TRUE(card_from_csv("Q,A,,x,y,z", &clamped));
  EXPECT_EQ(clamped.times_correct, 0);

  Flashcard rejected("", "");
  EXPECT_TRUE(!card_from_csv("just-a-question", &rejected));
}

void test_save_load_roundtrip() {
  const std::string path = temp_path("roundtrip.txt");
  std::remove(path.c_str());

  Deck deck(path);
  Flashcard scheduled("Contains, a comma", "and \"quotes\"", {"csv"}, 2, 5, 3);
  scheduled.due_date = days_from_civil(2026, 12, 25);
  deck.add(scheduled);
  deck.add(Flashcard("Plain", "Answer", {}, 0, 0, 1));
  EXPECT_TRUE(deck.save());

  Deck reloaded(path);
  EXPECT_TRUE(reloaded.load());
  EXPECT_EQ(reloaded.size(), size_t{2});
  EXPECT_EQ(reloaded.cards()[0].question, std::string("Contains, a comma"));
  EXPECT_EQ(reloaded.cards()[0].answer, std::string("and \"quotes\""));
  EXPECT_EQ(reloaded.cards()[0].times_incorrect, 5);
  EXPECT_EQ(reloaded.cards()[0].leitner_box, 3);
  EXPECT_EQ(reloaded.cards()[0].due_date, days_from_civil(2026, 12, 25));
  EXPECT_EQ(reloaded.cards()[1].due_date, kNoDate);

  std::remove(path.c_str());
}

void test_legacy_deck_migrates() {
  const std::string path = temp_path("legacy.txt");
  {
    // Exactly the format written before scheduling was added.
    std::ofstream file(path);
    file << "What does RAII stand for?,Resource Acquisition Is "
            "Initialization,cpp;basics,3,1,4\n"
         << "Plain question,Answer,,0,0,1\n";
  }

  Deck deck(path);
  EXPECT_TRUE(deck.load());
  EXPECT_EQ(deck.size(), size_t{2});
  // Boxes and scores survive; every card simply comes back due.
  EXPECT_EQ(deck.cards()[0].leitner_box, 4);
  EXPECT_EQ(deck.cards()[0].times_correct, 3);
  EXPECT_EQ(deck.due_count(today()), 2);

  // Saving upgrades the file in place, and the upgrade round-trips.
  EXPECT_TRUE(deck.save());
  Deck reloaded(path);
  EXPECT_TRUE(reloaded.load());
  EXPECT_EQ(reloaded.size(), size_t{2});
  EXPECT_EQ(reloaded.cards()[0].leitner_box, 4);

  std::remove(path.c_str());
}

void test_load_missing_file() {
  Deck deck(temp_path("does-not-exist.txt"));
  EXPECT_TRUE(!deck.load());
  EXPECT_TRUE(deck.empty());
}

void test_save_failure_preserves_deck() {
  // An unwritable path must fail loudly and leave the original file intact.
  const std::string path = temp_path("readonly-dir/deck.txt");
  Deck deck(path);
  deck.add(Flashcard("Q", "A"));

  std::string error;
  EXPECT_TRUE(!deck.save(&error));
  EXPECT_TRUE(!error.empty());

  // No stray temporary file left behind.
  std::ifstream leftover(path + ".tmp");
  EXPECT_TRUE(!leftover.is_open());
}

void test_save_is_atomic() {
  const std::string path = temp_path("atomic.txt");
  std::remove(path.c_str());

  Deck original(path);
  original.add(Flashcard("Original", "Content", {}, 7, 7, 5));
  EXPECT_TRUE(original.save());

  // The temp file is renamed into place, so no .tmp survives a good save.
  std::ifstream leftover(path + ".tmp");
  EXPECT_TRUE(!leftover.is_open());

  Deck reloaded(path);
  EXPECT_TRUE(reloaded.load());
  EXPECT_EQ(reloaded.size(), size_t{1});
  EXPECT_EQ(reloaded.cards()[0].times_correct, 7);

  std::remove(path.c_str());
}

void test_export_is_lossless() {
  const std::string source = temp_path("export-src.txt");
  const std::string exported = temp_path("export-dst.csv");
  std::remove(source.c_str());
  std::remove(exported.c_str());

  Deck deck(source);
  Flashcard card("Q1", "A1", {"tag"}, 9, 2, 5);
  card.due_date = days_from_civil(2027, 1, 31);
  deck.add(card);

  std::string error;
  EXPECT_TRUE(export_deck(deck, exported, &error));

  // Re-importing an export must not reset review history or the schedule.
  Deck fresh(temp_path("export-target.txt"));
  const ImportResult result = import_into(fresh, exported);
  EXPECT_TRUE(result.ok);
  EXPECT_EQ(result.imported, 1);
  EXPECT_EQ(fresh.cards()[0].times_correct, 9);
  EXPECT_EQ(fresh.cards()[0].times_incorrect, 2);
  EXPECT_EQ(fresh.cards()[0].leitner_box, 5);
  EXPECT_EQ(fresh.cards()[0].due_date, days_from_civil(2027, 1, 31));

  std::remove(source.c_str());
  std::remove(exported.c_str());
}

void test_import_missing_file() {
  Deck deck(temp_path("import-target.txt"));
  const ImportResult result = import_into(deck, temp_path("no-such-file.csv"));
  EXPECT_TRUE(!result.ok);
  EXPECT_TRUE(!result.error.empty());
  EXPECT_EQ(result.imported, 0);
}

void test_import_appends_plain_csv() {
  const std::string path = temp_path("plain.csv");
  {
    std::ofstream file(path);
    file << "What is the capital of France?,Paris,geography;europe\n"
         << "\n"  // blank lines are skipped
         << "Largest planet?,Jupiter,science\n";
  }

  Deck deck(temp_path("import-plain.txt"));
  deck.add(Flashcard("Existing", "Card"));
  const ImportResult result = import_into(deck, path);
  EXPECT_TRUE(result.ok);
  EXPECT_EQ(result.imported, 2);
  EXPECT_EQ(deck.size(), size_t{3});
  EXPECT_EQ(deck.cards()[1].leitner_box, 1);

  std::remove(path.c_str());
}

void test_unique_tags() {
  Deck deck(temp_path("tags.txt"));
  deck.add(Flashcard("Q1", "A1", {"Zebra", "cpp"}));
  deck.add(Flashcard("Q2", "A2", {"CPP", "apple"}));
  deck.add(Flashcard("Q3", "A3", {}));

  const std::vector<std::string> tags = deck.unique_tags();
  EXPECT_EQ(tags.size(), size_t{3});
  EXPECT_EQ(tags[0], std::string("apple"));
  EXPECT_EQ(tags[1], std::string("cpp"));
  EXPECT_EQ(tags[2], std::string("Zebra"));

  // Counting is case-insensitive, matching how reviews filter.
  EXPECT_EQ(deck.count_with_tag("cpp"), 2);
  EXPECT_EQ(deck.count_with_tag("CPP"), 2);
  EXPECT_EQ(deck.count_with_tag("missing"), 0);
}

void test_stats() {
  Deck deck(temp_path("stats.txt"));
  deck.add(Flashcard("Easy", "A", {}, 10, 0, 5));
  deck.add(Flashcard("Hard", "B", {}, 1, 9, 1));
  deck.add(Flashcard("Unseen", "C", {}, 0, 0, 1));

  const DeckStats stats = deck.stats(today());
  EXPECT_EQ(stats.total_cards, 3);
  EXPECT_EQ(stats.total_correct, 11);
  EXPECT_EQ(stats.total_incorrect, 9);
  EXPECT_EQ(stats.box_counts[1], 2);
  EXPECT_EQ(stats.box_counts[5], 1);
  EXPECT_TRUE(stats.hardest_card != nullptr);
  EXPECT_EQ(stats.hardest_card->question, std::string("Hard"));

  // A deck with no reviews yet has no "hardest" card to report.
  Deck fresh(temp_path("stats-empty.txt"));
  fresh.add(Flashcard("Q", "A"));
  EXPECT_TRUE(fresh.stats(today()).hardest_card == nullptr);
  EXPECT_EQ(fresh.stats(today()).success_rate, 0.0);
}

void test_remove() {
  Deck deck(temp_path("remove.txt"));
  deck.add(Flashcard("First", "1"));
  deck.add(Flashcard("Second", "2"));

  EXPECT_TRUE(!deck.remove(5));
  EXPECT_EQ(deck.size(), size_t{2});
  EXPECT_TRUE(deck.remove(0));
  EXPECT_EQ(deck.size(), size_t{1});
  EXPECT_EQ(deck.cards()[0].question, std::string("Second"));
}

void test_find() {
  Deck deck(temp_path("find.txt"));
  deck.add(Flashcard("la biblioteca", "library", {"spanish", "nouns"}));
  deck.add(Flashcard("Which command stages a file?", "git add|add", {"git"}));
  deck.add(Flashcard("el árbol", "tree", {"spanish"}));

  // An empty query matches everything, in deck order.
  EXPECT_EQ(deck.find("").size(), size_t{3});
  EXPECT_EQ(deck.find("   ").size(), size_t{3});
  EXPECT_EQ(deck.find("")[0], size_t{0});
  EXPECT_EQ(deck.find("")[2], size_t{2});

  // Question, answer and tag are all searched.
  EXPECT_EQ(deck.find("biblioteca").size(), size_t{1});
  EXPECT_EQ(deck.find("biblioteca")[0], size_t{0});
  EXPECT_EQ(deck.find("library")[0], size_t{0});
  EXPECT_EQ(deck.find("git").size(), size_t{1});
  EXPECT_EQ(deck.find("git")[0], size_t{1});

  // A tag shared by two cards returns both, still in deck order.
  const std::vector<size_t> spanish = deck.find("spanish");
  EXPECT_EQ(spanish.size(), size_t{2});
  EXPECT_EQ(spanish[0], size_t{0});
  EXPECT_EQ(spanish[1], size_t{2});

  // Case-insensitive and substring, not whole-word.
  EXPECT_EQ(deck.find("LIBRARY").size(), size_t{1});
  EXPECT_EQ(deck.find("StAgEs").size(), size_t{1});
  EXPECT_EQ(deck.find("libr").size(), size_t{1});

  // Alternatives are searchable, since the answer is matched as stored.
  EXPECT_EQ(deck.find("git add").size(), size_t{1});

  // No match is empty rather than everything.
  EXPECT_EQ(deck.find("python").size(), size_t{0});

  // Non-ASCII matches by exact bytes, which is enough to find the card.
  EXPECT_EQ(deck.find("árbol").size(), size_t{1});
  EXPECT_EQ(deck.find("árbol")[0], size_t{2});

  // Positions must survive a delete, which is why find returns indices the
  // caller re-derives rather than caching.
  EXPECT_TRUE(deck.remove(0));
  EXPECT_EQ(deck.find("spanish").size(), size_t{1});
  EXPECT_EQ(deck.find("spanish")[0], size_t{1});
}

void test_reverse_prompting() {
  Flashcard vocab("la biblioteca", "library");

  // Forward: ask the question, expect the answer.
  EXPECT_EQ(prompt_text(vocab, false), std::string("la biblioteca"));
  EXPECT_EQ(expected_answer(vocab, false), std::string("library"));

  // Reversed: ask the answer, expect the question.
  EXPECT_EQ(prompt_text(vocab, true), std::string("library"));
  EXPECT_EQ(expected_answer(vocab, true), std::string("la biblioteca"));

  // A reversed prompt shows only the first accepted answer, because
  // "git add|add" is not a sensible thing to display.
  Flashcard alts("Which command stages a file?", "git add|add");
  EXPECT_EQ(prompt_text(alts, true), std::string("git add"));
  EXPECT_EQ(expected_answer(alts, true),
            std::string("Which command stages a file?"));

  // Forward prompting is unaffected by alternatives.
  EXPECT_EQ(prompt_text(alts, false), std::string("Which command stages a file?"));
  EXPECT_EQ(expected_answer(alts, false), std::string("git add|add"));

  // Reversing twice is the identity, so no direction loses information.
  EXPECT_EQ(prompt_text(vocab, false), expected_answer(vocab, true));
  EXPECT_EQ(prompt_text(vocab, true), expected_answer(vocab, false));

  // The reversed answer is still matched with the usual typo tolerance.
  EXPECT_TRUE(check_answer("la biblioteca", expected_answer(vocab, true)).exact);
  EXPECT_TRUE(check_answer("La Biblioteca", expected_answer(vocab, true)).exact);
  EXPECT_TRUE(check_answer("la bibliotecca", expected_answer(vocab, true)).near_miss);
}

CliOptions parse(std::vector<const char*> args) {
  args.insert(args.begin(), "FlashTerm");
  return parse_args(static_cast<int>(args.size()), args.data(), "default.txt");
}

void test_cli_parsing() {
  // No arguments: the default deck.
  EXPECT_TRUE(parse({}).action == CliAction::RunDeck);
  EXPECT_EQ(parse({}).deck_path, std::string("default.txt"));

  // A plain path is a deck.
  EXPECT_TRUE(parse({"spanish.csv"}).action == CliAction::RunDeck);
  EXPECT_EQ(parse({"spanish.csv"}).deck_path, std::string("spanish.csv"));

  EXPECT_TRUE(parse({"-h"}).action == CliAction::ShowHelp);
  EXPECT_TRUE(parse({"--help"}).action == CliAction::ShowHelp);
  EXPECT_TRUE(parse({"-v"}).action == CliAction::ShowVersion);
  EXPECT_TRUE(parse({"--version"}).action == CliAction::ShowVersion);

  // The regression this module exists for: an unknown option must not become
  // a deck path, which silently created a file named "--help".
  EXPECT_TRUE(parse({"--nope"}).action == CliAction::Error);
  EXPECT_TRUE(parse({"-x"}).action == CliAction::Error);
  EXPECT_TRUE(parse({"-"}).action == CliAction::Error);
  EXPECT_TRUE(parse({"--nope"}).deck_path.empty());

  // "--" ends option parsing, so a dashed deck name is still reachable.
  EXPECT_TRUE(parse({"--", "-deck.txt"}).action == CliAction::RunDeck);
  EXPECT_EQ(parse({"--", "-deck.txt"}).deck_path, std::string("-deck.txt"));
  EXPECT_TRUE(parse({"--", "--help"}).action == CliAction::RunDeck);

  // One deck at a time, and no empty paths.
  EXPECT_TRUE(parse({"a.txt", "b.txt"}).action == CliAction::Error);
  EXPECT_TRUE(parse({""}).action == CliAction::Error);

  // Errors and help both carry text worth printing.
  EXPECT_TRUE(!parse({"--nope"}).error.empty());
  EXPECT_TRUE(usage_text().find("--version") != std::string::npos);
}

// --- Review log ---

// A UTC stamp landing on `day` in local time, whatever the machine's timezone
// is: noon local can never be pushed onto a neighbouring local day. Streaks
// are a question about the user's calendar, so the tests have to be able to
// name a local day without assuming UTC.
std::string stamp_on_day(int day, int hour = 12) {
  int year = 0;
  unsigned month = 0;
  unsigned mday = 0;
  civil_from_days(day, &year, &month, &mday);

  std::tm local{};
  local.tm_year = year - 1900;
  local.tm_mon = static_cast<int>(month) - 1;
  local.tm_mday = static_cast<int>(mday);
  local.tm_hour = hour;
  local.tm_isdst = -1;  // let the library work out whether DST applies
  const std::time_t when = std::mktime(&local);

  const std::tm* utc = std::gmtime(&when);
  char buffer[32];
  std::snprintf(buffer, sizeof(buffer), "%04d-%02u-%02uT%02u:%02u:%02uZ",
                utc->tm_year + 1900, static_cast<unsigned>(utc->tm_mon + 1),
                static_cast<unsigned>(utc->tm_mday),
                static_cast<unsigned>(utc->tm_hour),
                static_cast<unsigned>(utc->tm_min),
                static_cast<unsigned>(utc->tm_sec));
  return buffer;
}

ReviewEvent answer_event(const std::string& card_id, const std::string& stamp,
                         bool correct, int box_before, int box_after) {
  ReviewEvent event;
  event.id = generate_id();
  event.card_id = card_id;
  event.timestamp = stamp;
  event.correct = correct;
  event.box_before = box_before;
  event.box_after = box_after;
  return event;
}

void test_event_ids() {
  const std::string id = generate_id();
  EXPECT_EQ(id.size(), size_t{16});
  EXPECT_TRUE(id.find_first_not_of("0123456789abcdef") == std::string::npos);

  // Ids have to be unique without coordinating with anything, because two
  // machines mint them independently.
  std::set<std::string> seen;
  for (int i = 0; i < 1000; ++i) seen.insert(generate_id());
  EXPECT_EQ(seen.size(), size_t{1000});
}

void test_timestamps() {
  // Unix time is UTC by definition, so this is exact and timezone-free.
  EXPECT_EQ(parse_timestamp("1970-01-01T00:00:00Z"), std::time_t{0});
  EXPECT_EQ(parse_timestamp("2026-08-17T14:23:05Z"),
            static_cast<std::time_t>(days_from_civil(2026, 8, 17)) * 86400 +
                14 * 3600 + 23 * 60 + 5);

  // Malformed stamps are rejected rather than guessed at.
  EXPECT_EQ(parse_timestamp(""), std::time_t{-1});
  EXPECT_EQ(parse_timestamp("2026-08-17"), std::time_t{-1});
  EXPECT_EQ(parse_timestamp("2026-08-17 14:23:05Z"), std::time_t{-1});
  EXPECT_EQ(parse_timestamp("2026-08-17T14:23:05"), std::time_t{-1});
  EXPECT_EQ(parse_timestamp("2026-02-30T00:00:00Z"), std::time_t{-1});
  EXPECT_EQ(parse_timestamp("2026-08-17T25:00:00Z"), std::time_t{-1});

  // The stamp written now is one that can be read back.
  const std::string now = now_timestamp();
  EXPECT_EQ(now.size(), size_t{20});
  EXPECT_TRUE(parse_timestamp(now) > 0);
  EXPECT_EQ(local_day_of(now), today());

  // A UTC stamp maps back onto the local day it actually fell on.
  const int day = days_from_civil(2026, 3, 14);
  EXPECT_EQ(local_day_of(stamp_on_day(day)), day);
  EXPECT_EQ(local_day_of(stamp_on_day(day, 1)), day);
  EXPECT_EQ(local_day_of(stamp_on_day(day, 23)), day);
  EXPECT_EQ(local_day_of("nonsense"), kNoDate);
}

void test_event_csv() {
  ReviewEvent event;
  event.id = "0123456789abcdef";
  event.card_id = "fedcba9876543210";
  event.timestamp = "2026-08-17T14:23:05Z";
  event.direction = 'r';
  event.correct = true;
  event.box_before = 2;
  event.box_after = 3;

  EXPECT_EQ(event_to_csv(event),
            std::string("0123456789abcdef,fedcba9876543210,"
                        "2026-08-17T14:23:05Z,r,correct,2,3,"));

  ReviewEvent parsed;
  EXPECT_TRUE(event_from_csv(event_to_csv(event), &parsed));
  EXPECT_EQ(parsed.id, event.id);
  EXPECT_EQ(parsed.card_id, event.card_id);
  EXPECT_EQ(parsed.timestamp, event.timestamp);
  EXPECT_EQ(parsed.direction, 'r');
  EXPECT_TRUE(parsed.correct);
  EXPECT_EQ(parsed.box_before, 2);
  EXPECT_EQ(parsed.box_after, 3);
  EXPECT_TRUE(!parsed.is_undo());

  // An undo names the answer it takes back and reports no result of its own.
  ReviewEvent undo;
  undo.id = "1111111111111111";
  undo.card_id = event.card_id;
  undo.timestamp = "2026-08-17T14:23:40Z";
  undo.undoes = event.id;
  ReviewEvent undo_parsed;
  EXPECT_TRUE(event_from_csv(event_to_csv(undo), &undo_parsed));
  EXPECT_TRUE(undo_parsed.is_undo());
  EXPECT_EQ(undo_parsed.undoes, event.id);

  // Damaged lines are skipped, never taken as reviews that did not happen.
  ReviewEvent rejected;
  EXPECT_TRUE(!event_from_csv("", &rejected));
  EXPECT_TRUE(!event_from_csv("id,card", &rejected));
  EXPECT_TRUE(!event_from_csv("id,card,not-a-time,n,correct,1,2,", &rejected));
  EXPECT_TRUE(
      !event_from_csv(",card,2026-08-17T14:23:05Z,n,correct,1,2,", &rejected));
  EXPECT_TRUE(
      !event_from_csv("id,,2026-08-17T14:23:05Z,n,correct,1,2,", &rejected));
  // A result and an undo target contradict each other in both directions.
  EXPECT_TRUE(!event_from_csv(
      "id,card,2026-08-17T14:23:05Z,n,correct,1,2,target", &rejected));
  EXPECT_TRUE(
      !event_from_csv("id,card,2026-08-17T14:23:05Z,n,undo,1,2,", &rejected));

  // Boxes are clamped rather than trusted, as everywhere else.
  EXPECT_TRUE(
      event_from_csv("id,card,2026-08-17T14:23:05Z,n,incorrect,0,99,", &parsed));
  EXPECT_EQ(parsed.box_before, 1);
  EXPECT_EQ(parsed.box_after, kMaxBox);
  EXPECT_TRUE(!parsed.correct);
  // An unknown direction falls back to a normal prompt.
  EXPECT_EQ(parsed.direction, 'n');
}

void test_event_log_roundtrip() {
  const std::string path = temp_path("review.log");
  std::remove(path.c_str());

  EventLog log(path);
  // A log that does not exist yet is an empty log, not a failure.
  EXPECT_TRUE(!log.load());
  EXPECT_TRUE(log.empty());

  const std::string card = generate_id();
  EXPECT_TRUE(log.append(answer_event(card, "2026-08-17T09:00:00Z", true, 1, 2)));
  EXPECT_TRUE(
      log.append(answer_event(card, "2026-08-17T09:01:00Z", false, 2, 1)));
  EXPECT_EQ(log.events().size(), size_t{2});

  EventLog reloaded(path);
  EXPECT_TRUE(reloaded.load());
  EXPECT_EQ(reloaded.events().size(), size_t{2});
  EXPECT_EQ(reloaded.events()[0].card_id, card);
  EXPECT_TRUE(reloaded.events()[0].correct);
  EXPECT_TRUE(!reloaded.events()[1].correct);

  // Appending is additive: a second session extends the file rather than
  // replacing it, which is what makes the log mergeable at all.
  EXPECT_TRUE(
      reloaded.append(answer_event(card, "2026-08-17T09:02:00Z", true, 1, 2)));
  EventLog again(path);
  EXPECT_TRUE(again.load());
  EXPECT_EQ(again.events().size(), size_t{3});

  std::remove(path.c_str());
}

void test_event_log_survives_damage() {
  const std::string path = temp_path("damaged.log");
  {
    std::ofstream file(path);
    file << "aaaaaaaaaaaaaaaa,card1,2026-08-17T09:00:00Z,n,correct,1,2,\n"
         << "\n"
         << "this line is not an event\n"
         << "bbbbbbbbbbbbbbbb,card1,garbage,n,correct,1,2,\n"
         << "cccccccccccccccc,card1,2026-08-17T09:05:00Z,r,incorrect,2,1,\n";
  }

  EventLog log(path);
  EXPECT_TRUE(log.load());
  // The two readable events survive; the blank, the junk and the one with an
  // unusable timestamp are dropped.
  EXPECT_EQ(log.events().size(), size_t{2});
  EXPECT_EQ(log.events()[1].direction, 'r');

  std::remove(path.c_str());
}

void test_event_log_write_failure() {
  // An unwritable path has to be reported, but the event still happened, so
  // it stays in memory and the review it belongs to is not lost.
  EventLog log(temp_path("no-such-dir/review.log"));
  std::string error;
  EXPECT_TRUE(!log.append(answer_event("card1", "2026-08-17T09:00:00Z", true, 1, 2),
                          &error));
  EXPECT_TRUE(!error.empty());
  EXPECT_EQ(log.events().size(), size_t{1});
}

void test_merge_events() {
  const std::string card = "card1";
  const ReviewEvent first = answer_event(card, "2026-08-17T09:00:00Z", true, 1, 2);
  const ReviewEvent second = answer_event(card, "2026-08-17T21:00:00Z", false, 2, 1);
  const ReviewEvent third = answer_event(card, "2026-08-18T08:00:00Z", true, 1, 2);

  // Two machines that both saw `first` and then diverged.
  const std::vector<ReviewEvent> left{first, second};
  const std::vector<ReviewEvent> right{first, third};

  const std::vector<ReviewEvent> merged = merge_events(left, right);
  // The shared event appears once: a merge is a union, not a concatenation.
  EXPECT_EQ(merged.size(), size_t{3});
  EXPECT_EQ(merged[0].id, first.id);
  EXPECT_EQ(merged[1].id, second.id);
  EXPECT_EQ(merged[2].id, third.id);

  // Merging the other way round gives exactly the same log, which is the
  // property that makes syncing order-independent.
  const std::vector<ReviewEvent> reversed = merge_events(right, left);
  EXPECT_EQ(reversed.size(), merged.size());
  for (size_t i = 0; i < merged.size(); ++i) {
    EXPECT_EQ(event_to_csv(reversed[i]), event_to_csv(merged[i]));
  }

  // Merging a log with itself changes nothing.
  EXPECT_EQ(merge_events(merged, merged).size(), size_t{3});
  EXPECT_EQ(merge_events(merged, {}).size(), size_t{3});
}

void test_replay() {
  const int day = days_from_civil(2026, 8, 17);
  std::vector<ReviewEvent> events;
  events.push_back(answer_event("card1", stamp_on_day(day - 2), true, 1, 2));
  events.push_back(answer_event("card1", stamp_on_day(day - 1), true, 2, 3));
  events.push_back(answer_event("card1", stamp_on_day(day), false, 3, 1));
  events.push_back(answer_event("card2", stamp_on_day(day), true, 1, 2));

  const std::map<std::string, CardState> states = replay(events);
  EXPECT_EQ(states.size(), size_t{2});

  const CardState& one = states.at("card1");
  EXPECT_EQ(one.times_correct, 2);
  EXPECT_EQ(one.times_incorrect, 1);
  EXPECT_EQ(one.leitner_box, 1);
  EXPECT_EQ(one.last_reviewed, day);
  // The schedule is recomputed from the box, not remembered.
  EXPECT_EQ(one.due_date, day + interval_for_box(1));

  const CardState& two = states.at("card2");
  EXPECT_EQ(two.times_correct, 1);
  EXPECT_EQ(two.leitner_box, 2);
  EXPECT_EQ(two.due_date, day + interval_for_box(2));

  // Replaying a shuffled log gives the same answer: order comes from the
  // timestamps, not from the order the events happen to be stored in.
  std::vector<ReviewEvent> shuffled{events[3], events[1], events[0], events[2]};
  const std::map<std::string, CardState> again = replay(shuffled);
  EXPECT_EQ(again.at("card1").leitner_box, one.leitner_box);
  EXPECT_EQ(again.at("card1").times_correct, one.times_correct);
  EXPECT_EQ(again.at("card1").due_date, one.due_date);

  // An answer that was taken back leaves no trace in the replayed state, even
  // though it is still there in the log.
  ReviewEvent undo;
  undo.id = generate_id();
  undo.card_id = "card1";
  undo.timestamp = stamp_on_day(day, 13);
  undo.undoes = events[2].id;
  events.push_back(undo);

  const std::map<std::string, CardState> undone = replay(events);
  EXPECT_EQ(undone.at("card1").times_incorrect, 0);
  EXPECT_EQ(undone.at("card1").times_correct, 2);
  EXPECT_EQ(undone.at("card1").leitner_box, 3);

  EXPECT_TRUE(replay({}).empty());
}

void test_summarize() {
  const int today_days = today();

  std::vector<ReviewEvent> events;
  events.push_back(answer_event("card1", stamp_on_day(today_days), true, 1, 2));
  events.push_back(answer_event("card1", stamp_on_day(today_days, 14), false, 2, 1));
  events.push_back(answer_event("card2", stamp_on_day(today_days), true, 1, 2));
  events.push_back(answer_event("card2", stamp_on_day(today_days - 1), true, 1, 2));
  events.push_back(answer_event("card2", stamp_on_day(today_days - 2), true, 1, 2));

  LogStats stats = summarize(events, today_days);
  EXPECT_EQ(stats.reviewed_today, 3);
  EXPECT_EQ(stats.correct_today, 2);
  EXPECT_EQ(stats.current_streak, 3);

  // A gap ends the streak: the run before it is history, not part of today's.
  events.push_back(answer_event("card2", stamp_on_day(today_days - 4), true, 1, 2));
  events.push_back(answer_event("card2", stamp_on_day(today_days - 5), true, 1, 2));
  stats = summarize(events, today_days);
  EXPECT_EQ(stats.current_streak, 3);

  // Today has not finished yet, so a streak that ran through yesterday is
  // still alive at breakfast.
  const std::vector<ReviewEvent> yesterday{
      answer_event("card1", stamp_on_day(today_days - 1), true, 1, 2),
      answer_event("card1", stamp_on_day(today_days - 2), true, 1, 2)};
  stats = summarize(yesterday, today_days);
  EXPECT_EQ(stats.reviewed_today, 0);
  EXPECT_EQ(stats.current_streak, 2);

  // A whole empty day does end it.
  const std::vector<ReviewEvent> stale{
      answer_event("card1", stamp_on_day(today_days - 2), true, 1, 2)};
  EXPECT_EQ(summarize(stale, today_days).current_streak, 0);

  // Undone answers are not reviews, in the summary as in the replay.
  ReviewEvent undo;
  undo.id = generate_id();
  undo.card_id = "card1";
  undo.timestamp = stamp_on_day(today_days, 15);
  undo.undoes = events[0].id;
  events.push_back(undo);
  stats = summarize(events, today_days);
  EXPECT_EQ(stats.reviewed_today, 2);
  EXPECT_EQ(stats.correct_today, 1);

  stats = summarize({}, today_days);
  EXPECT_EQ(stats.reviewed_today, 0);
  EXPECT_EQ(stats.current_streak, 0);
}

void test_card_ids() {
  const std::string path = temp_path("ids.txt");
  const std::string log = log_path_for(path);
  std::remove(path.c_str());
  std::remove(log.c_str());

  EXPECT_EQ(log, path + ".log");

  // A deck written before ids existed gets them on load, and they stick.
  {
    std::ofstream file(path);
    file << "Q1,A1,tag,3,1,4,2026-08-01,2026-08-15\n"
         << "Q2,A2,,0,0,1\n";
  }
  Deck deck(path);
  EXPECT_TRUE(deck.load());
  EXPECT_EQ(deck.size(), size_t{2});
  EXPECT_TRUE(!deck.cards()[0].id.empty());
  EXPECT_TRUE(!deck.cards()[1].id.empty());
  EXPECT_TRUE(deck.cards()[0].id != deck.cards()[1].id);

  const std::string first_id = deck.cards()[0].id;
  EXPECT_TRUE(deck.save());
  Deck reloaded(path);
  EXPECT_TRUE(reloaded.load());
  EXPECT_EQ(reloaded.cards()[0].id, first_id);
  // The rest of the card is untouched by gaining an identity.
  EXPECT_EQ(reloaded.cards()[0].leitner_box, 4);
  EXPECT_EQ(reloaded.cards()[0].due_date, days_from_civil(2026, 8, 15));

  // A card added at runtime is minted one immediately.
  reloaded.add(Flashcard("Q3", "A3"));
  EXPECT_TRUE(!reloaded.cards()[2].id.empty());

  // Re-importing an export of this deck must not leave two cards claiming one
  // history: the newcomers are renamed, the originals keep their ids.
  const std::string exported = temp_path("ids-export.txt");
  std::string error;
  EXPECT_TRUE(export_deck(reloaded, exported, &error));
  const ImportResult result = import_into(reloaded, exported);
  EXPECT_TRUE(result.ok);
  EXPECT_EQ(result.imported, 3);
  EXPECT_EQ(reloaded.size(), size_t{6});
  EXPECT_EQ(reloaded.cards()[0].id, first_id);

  std::set<std::string> ids;
  for (const auto& card : reloaded.cards()) {
    EXPECT_TRUE(!card.id.empty());
    ids.insert(card.id);
  }
  EXPECT_EQ(ids.size(), size_t{6});

  std::remove(path.c_str());
  std::remove(log.c_str());
  std::remove(exported.c_str());
}

void test_deck_carries_its_log() {
  const std::string path = temp_path("logged.txt");
  const std::string log = log_path_for(path);
  std::remove(path.c_str());
  std::remove(log.c_str());

  {
    Deck deck(path);
    deck.add(Flashcard("Q", "A"));
    EXPECT_TRUE(deck.save());
    EXPECT_EQ(deck.log().path(), log);
    EXPECT_TRUE(deck.log().append(answer_event(deck.cards()[0].id,
                                               now_timestamp(), true, 1, 2)));
  }

  // Loading the deck brings its log back with it, without being asked.
  Deck reloaded(path);
  EXPECT_TRUE(reloaded.load());
  EXPECT_EQ(reloaded.log().events().size(), size_t{1});
  EXPECT_EQ(reloaded.log().events()[0].card_id, reloaded.cards()[0].id);
  EXPECT_EQ(summarize(reloaded.log().events(), today()).reviewed_today, 1);

  std::remove(path.c_str());
  std::remove(log.c_str());
}
}  // namespace

int main() {
  std::setlocale(LC_ALL, "");
  test_trim_and_case();
  test_normalize_answer();
  test_count_label();
  test_split();
  test_levenshtein();
  test_csv_roundtrip();
  test_utf8_columns();
  test_accepted_answers();
  test_check_answer_exact();
  test_check_answer_near_miss();
  test_undo_restores_card_exactly();
  test_alternatives_survive_saving();
  test_civil_dates();
  test_date_formatting();
  test_describe_due();
  test_is_due();
  test_record_answer_schedules();
  test_due_counts_and_next_due();
  test_card_csv();
  test_save_load_roundtrip();
  test_legacy_deck_migrates();
  test_load_missing_file();
  test_save_failure_preserves_deck();
  test_save_is_atomic();
  test_export_is_lossless();
  test_import_missing_file();
  test_import_appends_plain_csv();
  test_unique_tags();
  test_stats();
  test_remove();
  test_cli_parsing();
  test_find();
  test_reverse_prompting();
  test_event_ids();
  test_timestamps();
  test_event_csv();
  test_event_log_roundtrip();
  test_event_log_survives_damage();
  test_event_log_write_failure();
  test_merge_events();
  test_replay();
  test_summarize();
  test_card_ids();
  test_deck_carries_its_log();

  std::cout << "\n" << (g_checks - g_failures) << "/" << g_checks
            << " checks passed\n";
  if (g_failures > 0) {
    std::cout << g_failures << " FAILED\n";
    return 1;
  }
  std::cout << "All tests passed.\n";
  return 0;
}
