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
# 900x770 at FontSize 16 fits the statistics dashboard, which is the tallest
# screen at 31 lines. Anything shorter clips its title off the top. It grew by
# two when the review log added "Reviewed Today" and "Current Streak" to it, so
# re-measure this if anything else is ever added to that screen.
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
Set Height 770
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

# The main menu reports how many cards are actually due.
Sleep 2s
Type "2"
Enter
Sleep 2s

# Drill exactly what is due, most overdue first.
Type "1"
Enter
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
Enter
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
Enter
Sleep 4.5s
Enter
Sleep 1.5s

# Reversed review, scoped with a tag filter: shown the English, produce the
# Spanish. "places" matches only "la ciudad", so this stays a single card.
Type "2"
Enter
Sleep 1.5s
Type "3"
Enter
Sleep 2.5s
Type "1"
Enter
Sleep 1.5s
Type "r"
Enter
Sleep 3s
Type "la ciudad"
Sleep 500ms
Enter
Sleep 3s
