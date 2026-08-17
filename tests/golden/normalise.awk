# Replaces the tokens that differ on every run -- log timestamps, calendar
# dates, and the random ids minted for cards and events -- with placeholders,
# so that a transcript can be compared against a checked-in copy at all.
#
# Numbered rather than uniform, and numbered across the whole transcript rather
# than per file, because which id is which is exactly what these tests are for.
# An undo event names the answer it takes back, and that shows up here as the
# same <ID3> appearing in both records. A card's last-review date and its due
# date come out as <DATE1> and <DATE2>, so scheduling a card to be due today
# instead of tomorrow fails the test instead of hiding behind a shared
# placeholder.
#
# Numbering is by order of first appearance, which does mean inserting a new
# card near the top of a case renumbers what follows. That noise is worth it
# for what the numbers catch.
#
# The version is blanked rather than numbered, so that releasing does not have
# to touch every transcript; tests/tests.cpp is where the version string itself
# belongs.
#
# Repetition counts are spelled out longhand because mawk -- which is
# /usr/bin/awk on Debian and Ubuntu, so on CI -- does not support {n}.

BEGIN {
  d = "[0-9]"
  h = "[0-9a-f]"
  date = d d d d "-" d d "-" d d
  id = h h h h h h h h h h h h h h h h
  # A log timestamp is a date with a time stuck on the end, so it is written
  # here as exactly that, with the time part optional. Spelling it as a second
  # alternative would work only on an engine that picks the longest of two
  # matches starting in the same place; a greedy optional group is the same
  # thing asked for in a way every awk agrees on.
  pattern = date "(T" d d ":" d d ":" d d "Z)?|" id
}

function placeholder(token,   kind, key) {
  # The three lengths do not collide: 20 for a timestamp, 10 for a date, 16
  # for an id.
  kind = (length(token) == 16) ? "ID" : (length(token) == 10 ? "DATE" : "TIME")

  # Timestamps are deliberately not numbered. Whether two events in one case
  # land on the same second depends on how fast the machine ran, so numbering
  # them would turn a slow CI runner into a failing test.
  if (kind == "TIME") return "<TIME>"

  key = kind SUBSEP token
  if (!(key in seen)) seen[key] = ++count[kind]
  return "<" kind seen[key] ">"
}

{
  line = $0
  gsub(/FlashTerm [0-9]+\.[0-9]+\.[0-9]+/, "FlashTerm <VERSION>", line)

  out = ""
  while (match(line, pattern)) {
    out = out substr(line, 1, RSTART - 1) placeholder(substr(line, RSTART, RLENGTH))
    line = substr(line, RSTART + RLENGTH)
  }
  print out line
}
