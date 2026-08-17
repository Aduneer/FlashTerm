# FlashTerm 🚀

[![CI](https://github.com/Aduneer/FlashTerm/actions/workflows/ci.yml/badge.svg)](https://github.com/Aduneer/FlashTerm/actions/workflows/ci.yml)

FlashTerm is a terminal flashcard app that makes you type the answer. No multiple choice and no self-grading: you either produce it or you don't, which is the whole point of active recall. Cards move through five Leitner boxes, each with a real review interval, so a card you know stops appearing until it is due again.

<img src="demo/demo.gif" width="640" alt="FlashTerm review session: a due card is answered with a typo, which is offered as an override rather than marked wrong; the card is promoted a Leitner box; then the statistics dashboard and a reversed review">

<sub>Recorded with <a href="https://github.com/charmbracelet/vhs">vhs</a> from <a href="demo/demo.tape">demo/demo.tape</a> — scripted, so it can be re-rendered whenever the UI changes.</sub>

---

## Quick Start

Needs a C++17 compiler (`g++` or `clang++`) and `make`.

```bash
git clone https://github.com/Aduneer/FlashTerm
cd FlashTerm
make
./FlashTerm
```

That builds an executable named `FlashTerm`, with object files in `build/`, and
drops you at the main menu with a new empty deck.

### Installing (optional)

To run `FlashTerm` from anywhere instead of `./FlashTerm`:

```bash
sudo make install              # installs to /usr/local/bin
make install PREFIX=~/.local   # or somewhere on your PATH, no sudo needed
```

`make uninstall` removes it again, and honours the same `PREFIX`. `DESTDIR` is
supported for staged installs if you are packaging FlashTerm.

### Running

```bash
./FlashTerm                    # default deck, created on first run
./FlashTerm path/to/deck.txt   # any other deck
./FlashTerm --help             # usage
./FlashTerm --version          # version

export FLASHTERM_DECK=~/Sync/spanish.txt   # the deck to use when none is named
export FLASHTERM_THEME=ocean               # default, ocean or sunset
```

Your decks are yours: `flashcards.txt` and every `.log` are deliberately not
tracked by git, so studying never shows up as a source change.

### Starter Decks

A new deck starts empty. To fill it, choose **5. Import flashcards** and give it
any file from `examples/` — programming languages, tooling and human languages:

```bash
ls examples/
```

Imported cards arrive in Box 1 and are due immediately. The examples are plain
`question,answer,tags` records, the shortest form of the deck format below, so
they double as a template for writing your own. Several use `|` to accept more
than one answer, which is worth copying: `mañana` really does mean both
*tomorrow* and *morning*, and `git init` should not be marked wrong because you
typed `init`.

## Key Features

* **Spaced repetition (Leitner system)** — Flashcards are sorted into 5 boxes. Answering correctly promotes a card to the next box (up to Box 5); answering incorrectly demotes it back to Box 1. You can choose to review specific boxes or prioritize weaker boxes first.
* **Real scheduling** — Each box carries a review interval, so a card you know well genuinely stops appearing until it is due again. The main menu shows how many cards are due, the default review mode drills exactly those, and the most overdue cards come first.

  | Box | Next review after |
  | --- | --- |
  | 1 (weakest) | 1 day |
  | 2 | 3 days |
  | 3 | 7 days |
  | 4 | 14 days |
  | 5 (mastered) | 30 days |

  Cards you have never reviewed are due immediately. Existing decks upgrade automatically — every card simply starts out due, keeping its box and scores.
* **Tag system and interactive filtering** — Tag cards (e.g., `cpp; memory`) to filter reviews, picking tags by number or name.
* **Reversed review** — Any review mode can be flipped, so you are shown the answer and have to produce the question. `la biblioteca` → `library` tests whether you can *read* Spanish; `library` → `la biblioteca` tests whether you can *speak* it, which is a harder and genuinely different skill. Both directions share one box and due date. Best combined with a tag filter: reversing `git init` into "which command creates a new repository?" is not a useful exercise.
* **Search** — Find cards by any substring of the question, answer or tags. Editing and deleting search first rather than dumping the whole deck, and the numbers shown are always deck positions, so the number you type means the same thing whether or not you searched.
* **Typo tolerance** — Levenshtein distance catches near misses. A minor typo prompts you to override it rather than counting it wrong.
* **Multiple accepted answers** — Separate alternatives with `|` — `std::unique_ptr|unique_ptr` — and any of them counts. The first is shown back to you when you miss the card, the rest as also accepted.
* **Undo and fix in place** — After each answer, `u` takes it back — box, scores and due date restored exactly — and `e` edits the card on the spot, which is when you actually notice a bad question. Editing keeps the prompt open, so you can fix a card and *then* undo the answer it cost you.
* **Custom decks via CLI** — `./FlashTerm vocabulary.txt` loads any deck file; the default is `flashcards.txt`, or whatever `FLASHTERM_DECK` points at.
* **Works with the sync tool you already have** — Decks are plain text and saves are atomic, so Syncthing, Dropbox, `rsync` or git sync a deck between machines with no support needed from FlashTerm. See [Syncing Between Machines](#syncing-between-machines).
* **Deck statistics** — Success rates, review counts, a box-by-box mastery breakdown with ASCII bars, automatic flagging of your hardest card, and how much you reviewed today alongside your current daily streak.
* **Review log** — Every answer is appended to a `deck.txt.log` beside the deck: what was asked, which way round, whether you got it, and when, to the second. The card counters say what a card's state *is*; the log says what actually happened, which is what streaks, retention over time and merging two machines' reviews all need. It is append-only, so it never rewrites history and never conflicts.
* **Single-keypress menus** — `2` enters review; no Enter, no waiting. Every screen that takes a key shows a legend of what the keys do. Guarded on `isatty`, so piped input still reads whole lines and every script, pipeline and recording keeps working unchanged. `Ctrl+C` at a menu saves and exits cleanly rather than killing the process.
* **Framed cards** — The card under review is drawn in a box, centred in the window, with long questions wrapped to fit. Widths are measured in terminal columns rather than bytes, so the border still lines up on Japanese or accented cards — which is exactly where most tools get it wrong.
* **Hints** — Stuck at a blank prompt, press `?` to reveal the first letter and the shape of the rest: `l·  ··········`. It counts as a *partial* — the card holds its box instead of advancing, since you produced the answer but not unaided.
* **No dead ends** — `q` ends a review session from either prompt; leaving a card unanswered records nothing rather than counting it wrong. Every line prompt cancels on a bare Enter. Review keys are withheld on cards that accept them as answers, so a deck of vim keys or regex metacharacters stays reviewable.
* **Themes** — `FLASHTERM_THEME=ocean` or `sunset` repaints the palette; `NO_COLOR` still wins over both.
* **CSV parser** — Full double-quote support, so questions and answers can contain commas. Column layout is UTF-8 aware, so accented, CJK and emoji cards still line up.
* **Crash-safe autosave** — The deck is written after every answered card and every edit, so `Ctrl+C` mid-session costs you nothing. Saves are atomic — written to a temporary file and renamed into place — and a deck that cannot be written says so loudly instead of failing silently.
* **EOF and pipe safety** — Piped or non-interactive input auto-saves and exits cleanly rather than looping. Colour and screen clears are suppressed when output is not a terminal, or when `NO_COLOR` is set.

## Usage Guide

| Menu | What it does |
| --- | --- |
| `1` Add flashcard | Question, answer and semicolon-separated tags. Use `\|` for alternative answers. |
| `2` Review flashcards | Pick a mode: **due** cards (most overdue first), **all** (shuffled), by **tag**, **difficult** only (incorrect > correct), or by **box**. Then pick a direction: Enter for normal, `r` to be shown the answer and type the question. |
| `3` Manage flashcards | List, edit, delete or **find** cards. Editing and deleting ask for a search term first, so you never scroll a 200-card list to reach one card. Numbers shown are deck positions, and a card the search did not list cannot be edited or deleted by number. |
| `4` Display progress | Deck statistics, due counts, reviews today, daily streak, box distribution, hardest card, per-card rates. |
| `5` Import flashcards | Append cards from a `.csv` file. |
| `6` Export flashcards | Write the deck to `.csv`, review history included, so it re-imports without losing progress. |
| `7` List unique tags | Every tag in the deck, sorted, with card counts. |
| `h` Help | The full key reference, including the review keys below. |
| `0` Save and exit | Saves and exits. (The deck is already saved after every change.) |

The menu is titled with the deck you are actually in, which matters once
`FLASHTERM_DECK` means you might have several:

```
--- FlashTerm · spanish.txt ---
[1] Add flashcard
[2] Review flashcards  (2 due)
...
```

Menu choices are a single keypress in a terminal — pressing `2` enters review
immediately. When input is piped, menus read a whole line instead, so scripts
are unaffected.

Every screen that takes a key shows a legend of what those keys do, so nothing
has to be memorised:

```
[Enter] submit   [?] hint   [q] end session
Your answer: libary

⚠  Close! The correct answer is: library
   (You typed: libary)
[y] mark it correct   [any other key] count it wrong
> y
✅ Marked as correct!
Card promoted to Box 3!

[Enter] next card   [e] edit this card   [u] undo this answer   [q] end session
>
```

`q` leaves a session at any point — from the answer prompt, which leaves the
card unanswered rather than marking it wrong, or after answering. Either way
you get the summary for how far you got, with a count of what you did not
reach. Every prompt that takes a whole line cancels on a bare Enter, so no
screen is a dead end.

Menu option `h` prints the same keys as a reference, including `Ctrl+C` to save
and exit.

## Deck File Format

One CSV record per card, with the last six fields optional:

```
question,answer,tags,correct,incorrect,box,last_reviewed,due_date,id
```

Dates are plain `YYYY-MM-DD`, blank when a card has never been reviewed. Answers may list alternatives separated by `|`. Questions and answers containing commas or quotes are quoted normally, so decks stay greppable and editable by hand.

`id` is 16 hex characters identifying the card for the review log. It is filled
in automatically the first time a deck is loaded, so hand-written decks can
leave it off, and it stays the same when you edit the card — a fixed typo does
not orphan the card's history.

### Review Log Format

Alongside `mydeck.txt`, FlashTerm keeps `mydeck.txt.log`: one CSV record per
answer, appended and never rewritten.

```
id,card_id,timestamp,direction,result,box_before,box_after,undoes
```

```
c9072b87405e0369,2fb76783d6b65f93,2026-08-17T09:50:49Z,n,correct,1,2,
818d2231293bb6d7,2fb76783d6b65f93,2026-08-17T09:51:02Z,,undo,1,1,c9072b87405e0369
```

* `timestamp` is UTC to the second, so events from two machines sort into one
  order regardless of timezone. Due dates stay whole days; only the log is
  finer-grained than that.
* `direction` is `n` for a normal prompt and `r` for a reversed one.
* `result` is `correct`, `partial`, `incorrect`, or `undo`. A `partial` is an
  answer that needed the hint. An answer you take back with `u` is *recorded*
  as undone rather than erased — a line that may already have been synced
  elsewhere cannot be unwritten — and undone answers are excluded from every
  statistic.

Losing or deleting the log costs you the history, not the deck: cards keep
their own counters and schedule.

## Syncing Between Machines

There is no sync server and no account. A deck is a text file, so the tool you
already use for files works: **Syncthing, Dropbox, iCloud Drive, `rsync`, or a
git repo**. Saves are atomic — written to a temporary file and renamed into
place — so a sync client watching the directory can never catch a half-written
deck and never has to guess whether a file is finished.

Put the deck somewhere synced and point `FLASHTERM_DECK` at it, so you can just
run `FlashTerm` from anywhere on either machine:

```bash
# ~/.bashrc or ~/.zshrc, on every machine
export FLASHTERM_DECK="$HOME/Sync/decks/spanish.txt"
```

A deck named on the command line still wins, so `FlashTerm other.txt` keeps
working. Sync **both** files — the deck and its `.log` — since the log is where
your streak and review history live:

```
~/Sync/decks/spanish.txt
~/Sync/decks/spanish.txt.log
```

### The one thing to watch out for

**Reviewing on two machines before they have synced loses one side's
scheduling.** The deck is saved as a whole file, so whichever machine writes
last replaces the other's copy entirely, and the box moves and due dates the
losing machine recorded go with it. Nothing is corrupted and no cards are lost
— it is ordinary last-writer-wins — but the reviews are.

The rule that avoids it completely: **finish a session, let it sync, then start
on the other machine.** For most people that is simply how it already works.

The review log behaves better, because appending is not overwriting. Each
machine's log stays complete on its own, and since the file only ever grows,
sync clients handle it far more gracefully than the deck — git in particular
merges append-only files cleanly, where deck lines conflict. So even when a
deck write is lost, the record of *what you actually answered* usually is not.

Reconstructing the deck from merged logs — the thing that would make concurrent
review on two machines genuinely safe — is deliberately not wired up yet.
`merge_events()` and `replay()` in `src/event.h` are the working, tested halves
of it, waiting on the command that will call them.

## Development

### Running the Tests

```bash
make test
```

Covers the text and answer-matching utilities (normalisation, Levenshtein
distance, CSV escaping, UTF-8 column widths, alternatives, undo round-trips),
the scheduling logic (calendar arithmetic, leap years, box intervals, due
dates), the `Deck` persistence layer (atomic writes, write failures, lossless
import/export, legacy-deck migration, statistics), the review log (event
round-trips, damaged lines, card ids, streaks, and merging and replaying two
machines' logs), text layout (column-accurate word wrapping), and command-line
and environment handling.

### Golden End-to-End Tests

```bash
make golden          # run them
make check           # unit tests and golden tests together
```

Where `make test` links the library and calls functions, these drive the real
binary: each case in `tests/golden/cases/` feeds it a scripted stdin and
compares the whole transcript — output, exit status, and every file the run
left behind — against a checked-in copy. That covers `ui.cpp` and `review.cpp`,
which are hard to reach any other way.

Adding a case means creating a directory under `tests/golden/cases/` with an
`input` file, optionally a starting `deck.txt` and an `args` file, and then:

```bash
tests/golden/run.sh --update    # write the expected transcripts
tests/golden/run.sh some-case   # run named cases only
```

`--update` rewrites every expected transcript, so read the resulting diff
before committing it — that diff is the only thing separating a deliberate
change from a silently accepted regression.

Dates, and the random ids minted for cards and events, are replaced with
numbered placeholders (`<DATE1>`, `<ID2>`) so that transcripts are stable while
still recording which id is which. Timestamps become a plain `<TIME>`, because
whether two events land in the same second depends only on how fast the machine
ran.

These tests work because every input path falls back to reading whole lines
when stdin is not a terminal, and colour, screen clearing and vertical centring
all switch off when stdout is not a terminal. Keep it that way: a golden test
cannot drive an app that insists on a tty.

### Project Layout

| File | Responsibility |
| --- | --- |
| `src/flashcard.*` | The `Flashcard` model |
| `src/answer.*` | Accepted-answer alternatives and typo-tolerant matching |
| `src/date.*` | Civil-calendar arithmetic and due-date formatting |
| `src/schedule.*` | Box intervals, due checks, and the Leitner move for an answer |
| `src/text.*` | String, CSV and UTF-8 column helpers (no I/O) |
| `src/terminal.*` | Colour detection, screen clearing, terminal width |
| `src/event.*` | The append-only review log: events, ids, timestamps, merge and replay |
| `src/deck.*` | The `Deck` class: load, atomic save, import/export, tags, statistics |
| `src/review.*` | Review session flow and the Leitner promotion rules |
| `src/ui.*` | Menus, prompts and the statistics screen |
| `src/cli.*` | Command-line parsing (`--help`, `--version`, deck path) |
| `src/main.cpp` | Argument handling and the main menu loop |
| `tests/tests.cpp` | Unit test suite (`make test`) |
| `tests/golden/` | End-to-end transcript tests (`make golden`) |
| `demo/demo.tape` | Scripted [vhs](https://github.com/charmbracelet/vhs) recording behind the demo GIF: `vhs demo/demo.tape` |

---

## Contributing

Issues and pull requests are welcome. `make check` should pass before you open one; CI runs both suites on gcc and clang.

## License

MIT — see [LICENSE](LICENSE).
