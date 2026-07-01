# FlashTerm 🚀

A feature-rich, interactive C++ command-line application for creating, managing, and studying flashcards directly in your terminal. Designed for active recall, FlashTerm requires you to type answers manually to maximize memorization, and packages standard spaced repetition principles into a sleek terminal interface.

---

## Key Features

* 🧠 **Spaced Repetition (Leitner System)**: Flashcards are sorted into 5 boxes. Answering correctly promotes a card to the next box (up to Box 5); answering incorrectly demotes it back to Box 1. You can choose to review specific boxes or prioritize weaker boxes first.
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
* 🌀 **EOF and Pipe Safety**: Handles closed input streams gracefully. If you run the program non-interactively or pipe commands, it will auto-save your deck and exit cleanly rather than looping.

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

This creates an executable file named `FlashTerm`.

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
   - *ALL cards* (shuffled)
   - *By TAGS* (select tags interactively)
   - *DIFFICULT cards* (where Incorrect count > Correct count)
   - *By LEITNER BOX* (focus on weaker cards first)
3. **Manage flashcards**: List all cards with their current Leitner box, edit questions/answers/tags, or delete cards.
4. **Display progress**: View overall deck statistics, Leitner box distribution, hardest card, and card-by-card correctness rates.
5. **Import flashcards**: Append flashcards from a `.csv` file.
6. **Export flashcards**: Save the current deck to a clean `.csv` file.
7. **List unique Tags**: View all unique tags across your deck sorted alphabetically, along with card counts for each.
8. **Save and exit**: Saves all changes and exits safely.

---

## Contributing

Contributions are welcome! Please feel free to open issues or submit pull requests.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
