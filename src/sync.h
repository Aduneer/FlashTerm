#pragma once
#include <iosfwd>
#include <string>
#include <vector>

#include "deck.h"

namespace FlashTerm {

// Absorbing the copies a file-sync client leaves behind when two machines both
// appended to the review log.
//
// This is the first thing that actually calls merge_events() and replay(), and
// it is deliberately the smallest thing that could: no --sync command, no git,
// no network. Syncthing and friends already move the files; when they cannot
// decide which version won they park the loser beside the winner and leave it
// for a human. For a deck that is exactly the wrong outcome and exactly the
// easy one to fix, because the log is append-only -- neither copy is wrong,
// they are two halves of one history.

// True when `candidate` is a sync client's copy of `log_name`, both plain file
// names with no directory. Clients disagree about where the marker goes:
// Syncthing and Dropbox insert it before the final extension, so a copy of
// "spanish.txt.log" arrives as "spanish.txt.sync-conflict-20260817-101112-K3J.log"
// or "spanish.txt (laptop's conflicted copy 2026-08-17).log", while others
// simply append. Both shapes are accepted, and the inserted text has to
// contain the word "conflict" -- which is what keeps "spanish.txt.log.bak" and
// the ".tmp" of an interrupted rewrite out of it.
bool is_conflict_copy(const std::string& log_name, const std::string& candidate);

// Conflict copies of `log_path` in the directory it lives in, as full paths in
// name order. Name order rather than discovery order because a directory
// listing has none worth relying on, and the run has to be reproducible.
std::vector<std::string> find_conflict_copies(const std::string& log_path);

struct AbsorbResult {
  int copies = 0;          // conflict files read
  int absorbed = 0;        // events the log did not already have
  int cards_updated = 0;   // cards whose counters or schedule moved
  int unknown_cards = 0;   // card ids the absorbed events name and the deck has not
  bool failed = false;

  int exit_code() const { return failed ? 1 : 0; }
};

// Merges every conflict copy beside the deck's log into the log, rewrites it,
// and brings the deck's counters up to date with what the other machine did.
//
// How replayed state meets counters that predate the log -- the question that
// kept replay() unwired -- is answered by never trusting replay absolutely.
// The log is replayed twice, before the merge and after it, and only the
// *difference* is applied: counters gain what the other machine recorded, and
// a card's box and due date follow the merged log only when its last event is
// no older than what the deck already says. A deck whose history began before
// the log keeps it.
//
// The conflict copies are read and left alone. Deleting them is the user's
// call, and absorbing is idempotent -- a second run finds nothing new -- so
// leaving them costs only disk.
AbsorbResult absorb_conflicts(Deck& deck, std::ostream& out,
                              std::ostream& errors);
}  // namespace FlashTerm
