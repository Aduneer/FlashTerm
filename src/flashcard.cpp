#include "flashcard.h"

namespace FlashTerm {
Flashcard::Flashcard(const std::string& q, const std::string& a,
                     const std::vector<std::string>& t, int correct,
                     int incorrect)
    : question(q),
      answer(a),
      tags(t),
      times_correct(correct),
      times_incorrect(incorrect) {}

std::string Flashcard::tags_to_string() const {
  std::string result;
  for (size_t i = 0; i < tags.size(); ++i) {
    result += tags[i];
    if (i < tags.size() - 1) {
      result += ";";
    }
  }
  return result;
}
}  // namespace FlashTerm
