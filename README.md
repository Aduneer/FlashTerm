# FlashTerm 🚀

[![CI](https://github.com/Aduneer/FlashTerm/actions/workflows/ci.yml/badge.svg)](https://github.com/Aduneer/FlashTerm/actions/workflows/ci.yml)

FlashTerm is a terminal flashcard app that makes you type the answer. No multiple choice and no self-grading: you either produce it or you don't, which is the whole point of active recall. Cards move through five Leitner boxes, each with a real review interval, so a card you know stops appearing until it is due again.

---

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
* **Custom decks via CLI** — `./FlashTerm vocabulary.txt` loads any deck file; the default is `flashcards.txt`.
* **Deck statistics** — Success rates, review counts, a box-by-box mastery breakdown with ASCII bars, and automatic flagging of your hardest card.
* **CSV parser** — Full double-quote support, so questions and answers can contain commas. Column layout is UTF-8 aware, so accented, CJK and emoji cards still line up.
* **Crash-safe autosave** — The deck is written after every answered card and every edit, so `Ctrl+C` mid-session costs you nothing. Saves are atomic — written to a temporary file and renamed into place — and a deck that cannot be written says so loudly instead of failing silently.
* **EOF and pipe safety** — Piped or non-interactive input auto-saves and exits cleanly rather than looping. Colour and screen clears are suppressed when output is not a terminal, or when `NO_COLOR` is set.

## Getting Started

### Prerequisites

* A C++ compiler supporting C++17 (e.g., `g++` or `clang++`)
* `make` build utility

### Building the Project

Compile the application using the provided `Makefile`:

```bash
make
```

This creates an executable file named `FlashTerm`. Object files land in `build/`.

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
```

Your decks are yours: `flashcards.txt` is deliberately not tracked by git, so
studying never shows up as a source change.

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

### Running the Tests

```bash
make test
```

Covers the text and answer-matching utilities (normalisation, Levenshtein
distance, CSV escaping, UTF-8 column widths, alternatives, undo round-trips),
the scheduling logic (calendar arithmetic, leap years, box intervals, due
dates), and the `Deck` persistence layer (atomic writes, write failures,
lossless import/export, legacy-deck migration, statistics).

### Deck File Format

One CSV record per card, with the last five fields optional:

```
question,answer,tags,correct,incorrect,box,last_reviewed,due_date
```

Dates are plain `YYYY-MM-DD`, blank when a card has never been reviewed. Answers may list alternatives separated by `|`. Questions and answers containing commas or quotes are quoted normally, so decks stay greppable and editable by hand.

## Usage Guide

| Menu | What it does |
| --- | --- |
| 1. Add flashcard | Question, answer and semicolon-separated tags. Use `\|` for alternative answers. |
| 2. Review flashcards | Pick a mode: **due** cards (most overdue first), **all** (shuffled), by **tag**, **difficult** only (incorrect > correct), or by **box**. Then pick a direction: Enter for normal, `r` to be shown the answer and type the question. |
| 3. Manage flashcards | List, edit, delete or **find** cards. Editing and deleting ask for a search term first, so you never scroll a 200-card list to reach one card. Numbers shown are deck positions, and a card the search did not list cannot be edited or deleted by number. |
| 4. Display progress | Deck statistics, due counts, box distribution, hardest card, per-card rates. |
| 5. Import flashcards | Append cards from a `.csv` file. |
| 6. Export flashcards | Write the deck to `.csv`, review history included, so it re-imports without losing progress. |
| 7. List unique Tags | Every tag in the deck, sorted, with card counts. |
| 0. Save and exit | Saves and exits. (The deck is already saved after every change.) |

During a review, `u` undoes the last answer and `e` edits the current card.

## Project Layout

| File | Responsibility |
| --- | --- |
| `src/flashcard.*` | The `Flashcard` model |
| `src/answer.*` | Accepted-answer alternatives and typo-tolerant matching |
| `src/date.*` | Civil-calendar arithmetic and due-date formatting |
| `src/schedule.*` | Box intervals, due checks, and the Leitner move for an answer |
| `src/text.*` | String, CSV and UTF-8 column helpers (no I/O) |
| `src/terminal.*` | Colour detection, screen clearing, terminal width |
| `src/deck.*` | The `Deck` class: load, atomic save, import/export, tags, statistics |
| `src/review.*` | Review session flow and the Leitner promotion rules |
| `src/ui.*` | Menus, prompts and the statistics screen |
| `src/cli.*` | Command-line parsing (`--help`, `--version`, deck path) |
| `src/main.cpp` | Argument handling and the main menu loop |
| `tests/tests.cpp` | Test suite (`make test`) |

---

## Contributing

Issues and pull requests are welcome. `make test` should pass before you open one; CI runs it on gcc and clang.

## License

MIT — see [LICENSE](LICENSE).
