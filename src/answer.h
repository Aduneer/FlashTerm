#pragma once
#include <string>
#include <vector>

namespace FlashTerm {
// An answer may list alternatives separated by '|', any of which counts as
// correct: "std::unique_ptr|unique_ptr". The first one is what gets shown
// back to you when you miss the card.
constexpr char kAlternativeSeparator = '|';

std::vector<std::string> accepted_answers(const std::string& answer);
std::string primary_answer(const std::string& answer);
// The alternatives after the first, joined for display; "" when there is
// only one accepted answer.
std::string alternatives_summary(const std::string& answer);

struct AnswerCheck {
  // Matched an accepted answer once whitespace and case were normalised.
  bool exact = false;
  // Close enough to one accepted answer to be worth offering an override.
  bool near_miss = false;
  // The accepted answer the typed text matched, or came closest to.
  std::string closest;
};

AnswerCheck check_answer(const std::string& typed, const std::string& answer);
}  // namespace FlashTerm
