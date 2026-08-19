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
  # Escape sequences are matched as bytes rather than by regex metacharacters,
  # so that this behaves the same under mawk and gawk. A picture reaches the
  # transcript as a kitty graphics escape wrapped around base64, and the frame
  # positions itself around it with cursor moves; left raw, both are unreadable
  # in a diff and are exactly the "invalid multibyte data" awk complains about.
  esc = sprintf("%c", 27)
  graphics_start = esc "_G"
  graphics_end = esc "\\"
  cursor_move = esc "\\[[0-9]+[ABCD]"

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

# The picture itself never reaches a transcript -- only the instruction to draw
# it -- so what is checked is the box it was given and the file it named. Both
# are the things that break.
function collapse_graphics(line,   at, rest, stop) {
  while ((at = index(line, graphics_start)) > 0) {
    rest = substr(line, at + length(graphics_start))
    stop = index(rest, graphics_end)
    if (stop == 0) break
    line = substr(line, 1, at - 1) "<IMAGE " substr(rest, 1, stop - 1) ">" \
           substr(rest, stop + length(graphics_end))
  }
  return line
}

# Cursor moves are kept rather than dropped: the frame draws itself, walks back
# up into the space it reserved, and walks back down. Those two counts have to
# match, and a transcript that shows them is what catches it when they stop
# matching.
function collapse_cursor(line,   out, token, n) {
  out = ""
  while (match(line, cursor_move)) {
    token = substr(line, RSTART, RLENGTH)
    n = substr(token, 3, length(token) - 3)
    out = out substr(line, 1, RSTART - 1) "<CURSOR " substr(token, length(token), 1) n ">"
    line = substr(line, RSTART + RLENGTH)
  }
  return out line
}

{
  line = $0
  line = collapse_graphics(line)
  line = collapse_cursor(line)
  gsub(/\r/, "<CR>", line)
  gsub(/FlashTerm [0-9]+\.[0-9]+\.[0-9]+/, "FlashTerm <VERSION>", line)

  # Where the checkout happens to live is not a property of the program. A
  # message that names a tool's location -- and --generate-audio prints one, so
  # that nobody has to work out where pipx hid its Python -- would otherwise
  # pass here and fail on any other machine. Done literally rather than as a
  # pattern, since a path may contain regex metacharacters.
  if (root != "") {
    while ((at = index(line, root)) > 0) {
      line = substr(line, 1, at - 1) "<ROOT>" substr(line, at + length(root))
    }
  }

  out = ""
  while (match(line, pattern)) {
    out = out substr(line, 1, RSTART - 1) placeholder(substr(line, RSTART, RLENGTH))
    line = substr(line, RSTART + RLENGTH)
  }
  print out line
}
