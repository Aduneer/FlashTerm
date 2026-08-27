#pragma once
#include <string>
#include <vector>

namespace FlashTerm {
// Cloze deletion: a card whose question is a sentence with holes in it.
//
//   The {{mitochondrion}} is the powerhouse of the cell
//
// asks "The [...] is the powerhouse of the cell" and expects "mitochondrion".
// This is a way of writing a card, not a new kind of card: a cloze line has one
// Leitner box, one id and one due date like any other, and only how it is
// *asked* differs. A sentence with several holes is asked one hole at a time
// within a single presentation, and the card is scheduled once, on the worst of
// the answers -- so a three-blank sentence cannot promote a card three boxes in
// one sitting.
//
// The syntax is Anki's, so decks paste across in both directions and the
// planned Anki import gets it for nothing:
//
//   {{text}}              one blank
//   {{c1::text}}          the same, numbered -- repeat the number to make two
//                         places in the sentence into one blank
//   {{c1::text::hint}}    with a nudge shown in place of the blank
//
// `text` may carry "|" alternatives exactly as an answer column does, so
// {{c1::powerhouse|mitochondrion}} accepts either.
namespace cloze {

// `group` values that mean something other than one numbered blank.
constexpr int kNoGroup = 0;    // nothing blanked: the sentence as it reads
constexpr int kAllGroups = -1; // every blank open at once

// One blank.
struct Deletion {
  int group = 1;
  std::string answer;  // as written, "|" alternatives and all
  std::string hint;    // "" when the deletion gave none
};

// True when `text` holds at least one well-formed deletion. A "{{" with no
// closing "}}", or one with nothing inside it to answer, is not one: such a
// line stays an ordinary card and its braces are shown as written, because
// silently turning a card into an unanswerable one is worse than printing a
// brace.
bool contains(const std::string& text);

// The blanks, one entry per distinct group, ordered by group number. Numbered
// deletions keep the number they were given; unnumbered ones take the lowest
// number not already spoken for, in the order they appear -- so an all-bare
// sentence is asked left to right, and a sentence that numbers its blanks is
// asked in the order it asked for.
std::vector<Deletion> deletions(const std::string& text);

// How an open blank is drawn.
enum class Blank {
  kBox,     // "[...]", or "[hint]" where the deletion carries one
  kSpoken,  // "blank", for a synthesiser that would otherwise read punctuation
};

// `text` with the deletions in `group` left open and every other deletion
// replaced by the first of its accepted answers. kNoGroup fills them all in,
// kAllGroups opens them all.
std::string render(const std::string& text, int group,
                   Blank style = Blank::kBox);

// The sentence as it reads once the card is done.
inline std::string reveal(const std::string& text) {
  return render(text, kNoGroup);
}
}  // namespace cloze
}  // namespace FlashTerm
