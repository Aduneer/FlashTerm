# FlashTerm 🚀

A feature-rich, interactive C++ command-line application for creating, managing, and studying flashcards directly in your terminal. Designed for active recall, FlashTerm requires you to type answers manually to maximize memorization, and packages standard spaced repetition principles into a sleek terminal interface.

---

## Key Features

* 🧠 **Spaced Repetition (Leitner System)**: Flashcards are sorted into 5 boxes. Answering correctly promotes a card to the next box (up to Box 5); answering incorrectly demotes it back to Box 1. You can choose to review specific boxes or prioritize weaker boxes first.
* 📅 **Real Scheduling**: Each box carries a review interval, so a card you know well genuinely stops appearing until it is due again. The main menu shows how many cards are due, the default review mode drills exactly those, and the most overdue cards come first.

  | Box | Next review after |
  | --- | --- |
  | 1 (weakest) | 1 day |
  | 2 | 3 days |
  | 3 | 7 days |
  | 4 | 14 days |
  | 5 (mastered) | 30 days |

  Cards you have never reviewed are due immediately. Existing decks upgrade automatically — every card simply starts out due, keeping its box and scores.
* 🏷️ **Tag System & Interactive Filtering**: Tag cards (e.g., `cpp; memory`) to filter reviews. Supports tag sorting and an interactive selection menu where you can choose tags by number or name.
* ✍️ **Typo Tolerance & Manual Override**: Uses Levenshtein Distance to check how close your typed answer is to the correct one. If you make a minor typo, FlashTerm will show a warning and let you override it (mark it as correct).
* 🗃️ **Custom Decks via CLI**: Load, edit, and save distinct flashcard decks by passing the filename as a command-line argument:
  ```bash
  ./FlashTerm vocabulary.txt
  ```
  *(Defaults to `flashcards.txt` if no argument is provided).*
* 📊 **Deck Statistics Dashboard**: View detailed metrics about your progress:
  * Overall success rates and review counts.
  * A box-by-box breakdown of card mastery with visual ASCII progress bars.
  * Automatic flagging of your "Hardest Card to Remember."
* 🛡️ **Robust CSV Parser**: Fully supports double-quotes so questions/answers can contain commas without corrupting the file database.
* 💾 **Crash-Safe Autosave**: Your deck is written to disk after every answered card and every edit, so a crash or `Ctrl+C` mid-session never costs you your progress. Saves are atomic — the deck is written to a temporary file and renamed into place, so an interrupted or failed write can never leave you with a truncated deck. If the deck cannot be written, FlashTerm says so loudly instead of failing silently.
* 🌀 **EOF and Pipe Safety**: Handles closed input streams gracefully. If you run the program non-interactively or pipe commands, it will auto-save your deck and exit cleanly rather than looping. Colour codes and screen clears are suppressed when output is not a terminal (and when `NO_COLOR` is set), so piped output stays readable.

---

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

### Running the Tests

```bash
make test
```

Covers the text utilities (trimming, answer normalisation, Levenshtein distance, CSV escaping/parsing, UTF-8 column widths), the date and scheduling logic (calendar round-trips, leap years, box intervals, due calculations), and the `Deck` persistence layer (save/load round-trips, atomic writes, write failures, lossless import/export, legacy-deck migration, tag collection, statistics).

### Deck File Format

One CSV record per card, with the last five fields optional:

```
question,answer,tags,correct,incorrect,box,last_reviewed,due_date
```

Dates are plain `YYYY-MM-DD`, blank when a card has never been reviewed. Questions and answers containing commas or quotes are quoted normally, so decks stay greppable and editable by hand.

### Running

To launch with the default deck (`flashcards.txt`):

```bash
./FlashTerm
```

To launch with a custom deck file:

```bash
./FlashTerm path/to/my_deck.txt
```

---

## Usage Guide

When running FlashTerm, you can navigate using the main menu options:

1. **Add flashcard**: Prompts for a question, answer, and semicolon-separated tags.
2. **Review flashcards**: Start a review session. You can choose to review:
   - *DUE cards* — everything scheduled for today or earlier, most overdue first
   - *ALL cards* (shuffled)
   - *By TAGS* (select tags interactively)
   - *DIFFICULT cards* (where Incorrect count > Correct count)
   - *By LEITNER BOX* (focus on weaker cards first)
3. **Manage flashcards**: List all cards with their current Leitner box, edit questions/answers/tags, or delete cards.
4. **Display progress**: View overall deck statistics, how many cards are due and when the next one arrives, Leitner box distribution, hardest card, and card-by-card correctness rates.
5. **Import flashcards**: Append flashcards from a `.csv` file.
6. **Export flashcards**: Save the current deck to a `.csv` file. Exports carry review history, so an exported file can be re-imported without resetting your Leitner boxes and scores.
7. **List unique Tags**: View all unique tags across your deck sorted alphabetically, along with card counts for each.
8. **Save and exit**: Saves all changes and exits safely.

---

## Project Layout

| File | Responsibility |
| --- | --- |
| `src/flashcard.*` | The `Flashcard` model |
| `src/date.*` | Civil-calendar arithmetic and due-date formatting |
| `src/schedule.*` | Box intervals, due checks, and the Leitner move for an answer |
| `src/text.*` | String, CSV and UTF-8 column helpers (no I/O) |
| `src/terminal.*` | Colour detection, screen clearing, terminal width |
| `src/deck.*` | The `Deck` class: load, atomic save, import/export, tags, statistics |
| `src/review.*` | Review session flow and the Leitner promotion rules |
| `src/ui.*` | Menus, prompts and the statistics screen |
| `src/main.cpp` | Argument handling and the main menu loop |
| `tests/tests.cpp` | Test suite (`make test`) |

---

## Contributing

Contributions are welcome! Please feel free to open issues or submit pull requests.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
