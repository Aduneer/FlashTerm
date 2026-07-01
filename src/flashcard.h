#pragma once
#include <string>
#include <vector>

namespace FlashTerm {
class Flashcard {
 public:
  std::string question;
  std::string answer;
  std::vector<std::string> tags;
  int times_correct;
  int times_incorrect;
  int leitner_box;

  Flashcard(const std::string& q, const std::string& a,
            const std::vector<std::string>& t = {}, int correct = 0,
            int incorrect = 0, int leitner = 1);

  std::string tags_to_string() const;
};
}  // namespace FlashTerm
