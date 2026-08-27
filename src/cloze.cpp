#include "cloze.h"

#include <algorithm>
#include <set>

#include "answer.h"
#include "text.h"

namespace FlashTerm {
namespace cloze {
namespace {
constexpr char kOpen[] = "{{";
constexpr char kClose[] = "}}";
constexpr char kSeparator[] = "::";
// A blank is numbered "c1", not "c99999999". Capped so that a nonsense number
// cannot overflow the parse; anything above it is simply not a group prefix,
// which leaves the text to be read as an answer like any other.
constexpr int kMaxGroup = 999;

// Where one deletion sits in the text, and what it holds. Kept per occurrence
// rather than per group because rendering has to put something back at every
// one of them, and two occurrences of one group show their own text when the
// group is not the one being asked.
struct Occurrence {
  std::size_t start = 0;
  std::size_t length = 0;
  int group = 0;  // 0 until the second pass numbers an unnumbered deletion
  std::string answer;
  std::string hint;
};

std::vector<std::string> split_on_separator(const std::string& body) {
  std::vector<std::string> parts;
  std::size_t at = 0;
  while (true) {
    const std::size_t next = body.find(kSeparator, at);
    if (next == std::string::npos) {
      parts.push_back(body.substr(at));
      return parts;
    }
    parts.push_back(body.substr(at, next - at));
    at = next + 2;
  }
}

std::string join_on_separator(const std::vector<std::string>& parts,
                              std::size_t count) {
  std::string joined;
  for (std::size_t i = 0; i < count; ++i) {
    if (i > 0) joined += kSeparator;
    joined += parts[i];
  }
  return joined;
}

// The number in a "c12" prefix, or 0 when the segment is not one. Zero rather
// than a flag because a group is numbered from 1, so 0 already means "none".
int group_prefix(const std::string& segment) {
  if (segment.size() < 2 || segment[0] != 'c') return 0;
  int value = 0;
  for (std::size_t i = 1; i < segment.size(); ++i) {
    if (segment[i] < '0' || segment[i] > '9') return 0;
    value = value * 10 + (segment[i] - '0');
    if (value > kMaxGroup) return 0;
  }
  return value;
}

// Splits what is between the braces into a group, an answer and a hint.
// Returns false when there is no answer left to give, which is what makes
// "{{}}" and "{{c1::}}" ordinary text rather than an unanswerable blank.
//
// The hint is only looked for once a "cN::" prefix has been taken off. That is
// what lets "{{std::vector}}" be an answer containing a "::" rather than an
// answer of "std" hinted with "vector" -- a distinction a C++ deck needs and
// which the separator alone cannot make. Written the long way,
// "{{c1::std::vector::}}" says the same thing with an empty hint.
bool parse_body(const std::string& body, Occurrence* out) {
  std::vector<std::string> parts = split_on_separator(body);
  const int numbered = group_prefix(parts.front());
  if (numbered != 0) {
    out->group = numbered;
    parts.erase(parts.begin());
    if (parts.size() >= 2) {
      out->hint = trim(parts.back());
      parts.pop_back();
    }
  }
  out->answer = trim(join_on_separator(parts, parts.size()));
  return !out->answer.empty();
}

std::vector<Occurrence> scan(const std::string& text) {
  std::vector<Occurrence> found;
  std::size_t at = 0;
  while (true) {
    const std::size_t open = text.find(kOpen, at);
    if (open == std::string::npos) break;
    const std::size_t close = text.find(kClose, open + 2);
    if (close == std::string::npos) break;

    Occurrence occurrence;
    occurrence.start = open;
    occurrence.length = close + 2 - open;
    if (parse_body(text.substr(open + 2, close - open - 2), &occurrence)) {
      found.push_back(occurrence);
    }
    at = close + 2;
  }

  // Second pass: an unnumbered deletion takes the lowest number no numbered
  // one has claimed, in the order it appears. Done after the whole text has
  // been read because a "{{c1::...}}" further along still owns 1.
  std::set<int> taken;
  for (const auto& occurrence : found) {
    if (occurrence.group != 0) taken.insert(occurrence.group);
  }
  int next = 1;
  for (auto& occurrence : found) {
    if (occurrence.group != 0) continue;
    while (taken.count(next) != 0) ++next;
    occurrence.group = next;
    taken.insert(next);
  }
  return found;
}

std::string blank_text(const Occurrence& occurrence, Blank style) {
  if (style == Blank::kSpoken) return "blank";
  if (occurrence.hint.empty()) return "[...]";
  return "[" + occurrence.hint + "]";
}
}  // namespace

bool contains(const std::string& text) { return !scan(text).empty(); }

std::vector<Deletion> deletions(const std::string& text) {
  std::vector<Occurrence> found = scan(text);
  // By group, and by where it appears within a group, so that a group written
  // in two places is described by the first of them.
  std::stable_sort(found.begin(), found.end(),
                   [](const Occurrence& a, const Occurrence& b) {
                     return a.group < b.group;
                   });

  std::vector<Deletion> blanks;
  for (const auto& occurrence : found) {
    if (!blanks.empty() && blanks.back().group == occurrence.group) continue;
    Deletion blank;
    blank.group = occurrence.group;
    blank.answer = occurrence.answer;
    blank.hint = occurrence.hint;
    blanks.push_back(blank);
  }
  return blanks;
}

std::string render(const std::string& text, int group, Blank style) {
  const std::vector<Occurrence> found = scan(text);
  std::string out;
  std::size_t at = 0;
  for (const auto& occurrence : found) {
    out += text.substr(at, occurrence.start - at);
    const bool open = (group == kAllGroups) || (occurrence.group == group);
    out += open ? blank_text(occurrence, style)
                : primary_answer(occurrence.answer);
    at = occurrence.start + occurrence.length;
  }
  out += text.substr(at);
  return out;
}
}  // namespace cloze
}  // namespace FlashTerm
