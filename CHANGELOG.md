# Changelog

Notable changes per release. Dates are the release date; the PR numbers link the
detail, which is where the reasoning lives.

## 0.2.0 — 2026-08-17

Audio, a second test suite, and a review screen that fits in a terminal.

### Added

- **Audio for cards.** Press `a` during a review to hear one: a recording if the
  card names one in the new tenth deck column, otherwise spoken by whichever of
  `espeak-ng`, `espeak`, `say` or `flite` is installed. Playback uses `mpv`,
  `ffplay`, `paplay`, `pw-play`, `mpg123`, `afplay` or `aplay`. Nothing is linked
  against and nothing is required — with none of them present, the key is simply
  never offered. (#14)

  `a` plays what is on screen, which is what keeps a reversed session honest: the
  question's recording waits until after the card is graded, rather than handing
  over the answer.

- **`--generate-audio`,** which renders a sound file per card ahead of time and
  fills in the audio column, so a good voice does not cost a second a card
  mid-review. Skips what already exists; `--force` re-renders. (#15)

- **`--voice <name>`,** taking a [Piper](https://github.com/OHF-voice/piper1-gpl)
  voice by name instead of a command line. With no voice, no piper, or a name
  that is not installed, it says what to do about it — including the exact
  download command, worked out from where piper actually is, because pipx hides
  its Python somewhere the documented command does not look. (#16)

- **`FLASHTERM_TTS`, `FLASHTERM_PLAYER`, `FLASHTERM_TTS_RENDER`, `FLASHTERM_VOICES`**
  for using something other than the defaults. Commands are run as argument
  lists, never through a shell, so a card reading `hello; rm -rf ~` stays a card
  about shell quoting. (#14, #15, #16)

- **`FLASHTERM_SEED`,** which fixes the review shuffle when set. Real runs are
  unaffected; it exists so a scripted session is reproducible. (#13)

- **A golden end-to-end test suite** (`make golden`, `make check`): 38 cases that
  drive the real binary with scripted input and compare the whole transcript —
  output, exit status, and every file the run left behind. CI runs it alongside
  the unit tests on gcc and clang. (#11)

- **A `CHANGELOG.md`,** which you are reading.

### Fixed

- A hinted answer printed `✅ Correct!` and was then counted as **Incorrect** in
  the session summary. Partials now have their own row. The scoring is unchanged
  and deliberately so — a partial still earns no point — but the label no longer
  contradicts the screen above it. (#12)

- The key legend ran to 103 columns once audio was offered, so an 80-column
  terminal broke it mid-word. It wraps between hints now. It had been 79 columns
  before audio existed, which is to say it had been fitting by one column. (#18)

- A failed render reported only `FAILED`, having sent the synthesiser's
  explanation to `/dev/null`. The reason is now shown — once, not once per card —
  and a run that fails three times without a success stops rather than grinding
  through the deck. (#17)

- Piper's Japanese voices need `pipx inject piper-tts pyopenjtalk`, which nothing
  said. FlashTerm recognises that failure and prints the command. (#17)

### Changed

- Decks may carry a tenth `audio` column. It is written only when a card has a
  recording, so a deck without audio is byte-for-byte what earlier versions
  wrote — syncing between an updated machine and one that has not updated does
  not put the whole file in conflict. Every earlier deck loads unchanged. (#14)

## 0.1.0

First tagged release: Leitner scheduling, typo-tolerant answers, tags, reversed
review, import/export, an append-only review log, single-keypress menus, framed
cards, themes, and a unit test suite.
