# C++ Flashcards

A simple yet effective C++ command-line application for creating and reviewing flashcards.

## Overview

This application allows users to manage a collection of flashcards for studying. You can add new flashcards, review existing ones in a random order, and your collection is saved to a simple `flashcards.txt` file.

This project is built with C++17 and uses a standard `Makefile` for compilation.

## Features

*   **Add Flashcards**: Easily add new flashcards with a question and an answer.
*   **Review Flashcards**: Go through your flashcards in a shuffled, random order.
*   **Save & Exit**: Your flashcards are saved to `flashcards.txt` so you can pick up where you left off.
*   **Simple Storage**: Flashcards are stored in a human-readable comma-separated `flashcards.txt` file.

## Getting Started

### Prerequisites

*   A C++ compiler (e.g., `g++`)
*   `make`

### Building

To build the application, run the `make` command:

```bash
make
```

This will create an executable file named `cpp-flashcards`.

### Running

To run the application, execute the following command:

```bash
./cpp-flashcards
```

## Usage

When you run the application, you will see a menu with the following options:

1.  **Add flashcard**: Prompts for a question and an answer to create a new flashcard.
2.  **Review flashcards**: Quizzes you on your existing flashcards in a random order.
3.  **Manage flashcards**: List, Edit or Delete flashcards.
4.  **Display progress**: Shows you an overview of Correct/Incorrect, along with %.
5.  **Import flashcards**: Allows you to import .csv flashcards.
6.  **Export flashcards**: Allows you to export your questions as .csv.
7.  **Manage Tags**: Allows you to modify question tags.
0.  **Save and exit**: Saves all flashcards to `flashcards.txt` and terminates the program.

## Contributing

Contributions are welcome! Please feel free to submit a pull request or open an issue.

## License

This project is licensed under the MIT License.
