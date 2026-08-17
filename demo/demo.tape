# Regenerate the README demo with:
#
#     vhs demo/demo.tape
#
# Needs vhs and ttyd on PATH (ffmpeg does the encoding). Run it from the repo
# root, not from demo/.
#
# The recording is scripted rather than hand-performed so it can be re-rendered
# whenever the UI changes. That only works because it is deterministic:
# demo-deck.txt carries fixed past due dates, so the two overdue cards always
# come up in the same order, and "la ciudad" is dated 2027 so it stays out of a
# due-only review and demonstrates that scheduling really does hold cards back.

Output demo/demo.gif

Set Shell "bash"
# 900x795 at FontSize 16 fits the statistics dashboard, which is the tallest
# screen at 32 lines. Anything shorter clips its title off the top.
#
# Re-measure whenever a line is added to that screen: it has already grown
# twice, once for the log's "Reviewed Today" and "Current Streak", and once
# for the key legend that replaced its "press any key" line. A row is roughly
# 23.5px at this font size, which is what to add per new line.
#
# The README displays this at width=640 to keep it from eating a whole screen of
# scroll, so the extra native resolution is not wasted: it supersamples down
# cleanly, and stays sharp on high-DPI displays where 640 CSS px is 1280 real
# ones. Checked at 640: even the dashboard's numbers stay legible.
# Note: Set Framerate has no effect on GIF output in vhs 0.11 — the file lands at
# 25fps regardless. Not worth working around: halving the frame count externally
# was measured at only 5% off the file size, because the cost is the dithered
# progress bars and the sheer volume of distinct text, not the frame count.
Set FontSize 16
Set Width 900
Set Height 795
Set Padding 20
Set Theme "Dracula"
Set TypingSpeed 75ms

# Build, then work on a throwaway copy in a temp directory: the recording must
# never touch a real deck, and every run has to start from the same state.
Hide
Type "make >/dev/null 2>&1" Enter
Type "DEMO=$(mktemp -d) && cp FlashTerm $DEMO/ && cp demo/demo-deck.txt $DEMO/spanish.txt && cd $DEMO" Enter
Type "export PS1='$ ' && clear" Enter
Show

Sleep 1s
Type "./FlashTerm spanish.txt"
Sleep 600ms
Enter

# Menus take a single keypress, so there is deliberately no Enter after a menu
# digit below: typing one is what selects it. Only the prompts that accept a
# whole line -- typed answers, and the tag filter -- still need Enter, and
# those are the ones that keep it. Get this wrong and the stray Return falls
# through to whatever screen comes next.

# The main menu reports how many cards are actually due.
Sleep 2s
Type "2"
Sleep 2s

# Drill exactly what is due, most overdue first.
Type "1"
Sleep 2s

# Enter keeps the normal direction: question shown, answer typed.
Enter
Sleep 2.5s

# A typo is offered as an override instead of being marked wrong outright.
Type "libary"
Sleep 700ms
Enter
Sleep 2.5s
Type "y"
Sleep 2.5s
Enter
Sleep 1.2s

# A clean answer, and the card is promoted a box.
Type "tree"
Sleep 400ms
Enter
Sleep 2.5s
Enter

# Session summary. "Still Due: 0" while the deck holds three cards, because the
# third is not due yet.
Sleep 3s
Enter
Sleep 1.2s

# The statistics dashboard.
Type "4"
Sleep 4.5s
Enter
Sleep 1.5s

# Reversed review, scoped with a tag filter: shown the English, produce the
# Spanish. "places" matches only "la ciudad", so this stays a single card.
Type "2"
Sleep 1.5s
Type "3"
Sleep 2.5s
# The tag filter takes names or numbers, so it reads a whole line and keeps its
# Enter.
Type "1"
Enter
Sleep 1.5s
Type "r"
Sleep 3s

# Stuck on the reversed prompt, so ask for a hint: the first letter and the
# shape of the rest. It counts as a partial, which holds the card's box rather
# than advancing it.
Type "?"
Enter
Sleep 3s
Type "la ciudad"
Sleep 500ms
Enter
Sleep 3.5s
