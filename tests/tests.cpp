// Minimal assert-style harness: no framework, just `make test`.
#include <clocale>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "deck.h"
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

void test_card_csv() {
  Flashcard card("What is 2+2?", "4", {"math", "basics"}, 3, 1, 4);
  const std::string line = card_to_csv(card);

  Flashcard parsed("", "");
  EXPECT_TRUE(card_from_csv(line, &parsed));
  EXPECT_EQ(parsed.question, card.question);
  EXPECT_EQ(parsed.answer, card.answer);
  EXPECT_EQ(parsed.tags_to_string(), std::string("math;basics"));
  EXPECT_EQ(parsed.times_correct, 3);
  EXPECT_EQ(parsed.times_incorrect, 1);
  EXPECT_EQ(parsed.leitner_box, 4);

  // A bare question/answer pair is still a valid card.
  Flashcard minimal("", "");
  EXPECT_TRUE(card_from_csv("Q,A", &minimal));
  EXPECT_EQ(minimal.leitner_box, 1);

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
  deck.add(Flashcard("Contains, a comma", "and \"quotes\"", {"csv"}, 2, 5, 3));
  deck.add(Flashcard("Plain", "Answer", {}, 0, 0, 1));
  EXPECT_TRUE(deck.save());

  Deck reloaded(path);
  EXPECT_TRUE(reloaded.load());
  EXPECT_EQ(reloaded.size(), size_t{2});
  EXPECT_EQ(reloaded.cards()[0].question, std::string("Contains, a comma"));
  EXPECT_EQ(reloaded.cards()[0].answer, std::string("and \"quotes\""));
  EXPECT_EQ(reloaded.cards()[0].times_incorrect, 5);
  EXPECT_EQ(reloaded.cards()[0].leitner_box, 3);

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
  deck.add(Flashcard("Q1", "A1", {"tag"}, 9, 2, 5));

  std::string error;
  EXPECT_TRUE(export_deck(deck, exported, &error));

  // Re-importing an export must not reset review history.
  Deck fresh(temp_path("export-target.txt"));
  const ImportResult result = import_into(fresh, exported);
  EXPECT_TRUE(result.ok);
  EXPECT_EQ(result.imported, 1);
  EXPECT_EQ(fresh.cards()[0].times_correct, 9);
  EXPECT_EQ(fresh.cards()[0].times_incorrect, 2);
  EXPECT_EQ(fresh.cards()[0].leitner_box, 5);

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

  const DeckStats stats = deck.stats();
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
  EXPECT_TRUE(fresh.stats().hardest_card == nullptr);
  EXPECT_EQ(fresh.stats().success_rate, 0.0);
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
}  // namespace

int main() {
  std::setlocale(LC_ALL, "");
  test_trim_and_case();
  test_normalize_answer();
  test_split();
  test_levenshtein();
  test_csv_roundtrip();
  test_utf8_columns();
  test_card_csv();
  test_save_load_roundtrip();
  test_load_missing_file();
  test_save_failure_preserves_deck();
  test_save_is_atomic();
  test_export_is_lossless();
  test_import_missing_file();
  test_import_appends_plain_csv();
  test_unique_tags();
  test_stats();
  test_remove();

  std::cout << "\n" << (g_checks - g_failures) << "/" << g_checks
            << " checks passed\n";
  if (g_failures > 0) {
    std::cout << g_failures << " FAILED\n";
    return 1;
  }
  std::cout << "All tests passed.\n";
  return 0;
}
