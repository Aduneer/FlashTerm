#include "answer.h"

#include "text.h"

namespace FlashTerm {
namespace {
// A short answer tolerates one typo, a longer one tolerates two. Answers
// under four characters must be exact, since at that length a single edit
// is usually a different answer rather than a slip.
bool within_typo_budget(int distance, std::size_t length) {
  if (length >= 4 && length <= 8) return distance <= 1;
  if (length > 8) return distance <= 2;
  return false;
}
}  // namespace

std::vector<std::string> accepted_answers(const std::string& answer) {
  std::vector<std::string> options;
  for (const auto& part : split(answer, kAlternativeSeparator)) {
    if (!part.empty()) options.push_back(part);
  }
  // An answer that is blank or only separators still has to yield something
  // to compare against.
  if (options.empty()) options.push_back(trim(answer));
  return options;
}

std::string primary_answer(const std::string& answer) {
  return accepted_answers(answer).front();
}

std::string alternatives_summary(const std::string& answer) {
  const std::vector<std::string> options = accepted_answers(answer);
  std::string summary;
  for (std::size_t i = 1; i < options.size(); ++i) {
    if (!summary.empty()) summary += ", ";
    summary += options[i];
  }
  return summary;
}

AnswerCheck check_answer(const std::string& typed, const std::string& answer) {
  AnswerCheck check;
  const std::vector<std::string> options = accepted_answers(answer);
  check.closest = options.front();

  const std::string normalized_typed = normalize_answer(typed);
  for (const auto& option : options) {
    if (normalize_answer(option) == normalized_typed) {
      check.exact = true;
      check.closest = option;
      return check;
    }
  }

  // No exact match, so report against whichever alternative was closest.
  const std::string clean_typed = to_lowercase(trim(typed));
  int best_distance = -1;
  for (const auto& option : options) {
    const std::string clean_option = to_lowercase(trim(option));
    const int distance = levenshtein_distance(clean_typed, clean_option);
    if (best_distance < 0 || distance < best_distance) {
      best_distance = distance;
      check.closest = option;
      check.near_miss = within_typo_budget(distance, clean_option.length());
    }
  }
  return check;
}
}  // namespace FlashTerm
