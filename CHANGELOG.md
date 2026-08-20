# Changelog

Notable changes per release. Dates are the release date; the PR numbers link the
detail, which is where the reasoning lives.

## 0.3.1 — 2026-08-20

Two fixes and a second platform. Nothing here changes what FlashTerm does or
what a deck can hold; both fixes are things that were quietly already wrong,
and one of them only a second standard library could show.

### Fixed

- **Opening a deck no longer rewrites it.** Studying any deck in `examples/`
  straight from a clone left it modified in `git status` without a single card
  having been answered, which looks like the app corrupting its own sample data
  and is an easy way to get surprise diffs.

  Two separate causes, both of which had to go:

  - Every card was given an `id` when the deck was *loaded*. An id is what a
    log event names a card by, so a card needs one at the moment something is
    recorded against it — and a deck that is only read has nothing recorded
    against it. Ids are now minted at that moment instead. Answered cards are
    unaffected: the id still exists before the event that names it is written,
    which is the invariant that matters.
  - Saving expanded every row to its full width, so a hand-written
    `question,answer,tags` deck came back as `question,answer,tags,0,0,1,,,id`.
    Cards are now written only as far as the last column they actually use.
    That rule already existed for the `audio` and `image` columns, for exactly
    this reason; it now covers the whole row rather than the last two.

  A save that would reproduce the file byte for byte is also skipped outright.
  Saving is unconditional at every call site — after every answer, after every
  edit, and on the way out — which is deliberate and is what makes an
  interrupted session cost nothing; it just should not mean that reading a deck
  counts as writing it.

  Nothing about what a deck can contain has changed, and every deck written by
  an earlier version still loads and still saves identically once a card in it
  has been reviewed. (#25)

- **`FLASHTERM_SEED` now fixes the review order on every platform, not just the
  one you built on.** The shuffle went through `std::shuffle`, whose output the
  standard does not specify — only that the permutation is uniformly random —
  so libstdc++ and libc++ deal the same seeded deck in different orders. It is
  now an explicit Fisher–Yates over `std::mt19937`, whose own output *is*
  specified exactly, so the seed alone decides the order.

  Found by the macOS runner below, on its first run, which is the entire
  argument for having added it: every multi-card golden case failed there,
  because a scripted session answers cards in the order it expects to meet
  them and the transcripts were recorded against libstdc++. Nothing was wrong
  with the shuffle's randomness, and no real session is affected — the bug was
  in what the seed promised. (#24)

### Changed

- **CI builds and tests on macOS as well as Linux.** Everything FlashTerm does
  outside the standard library is POSIX rather than Linux — `termios`, `ioctl`,
  `dirent`, `wcwidth` — and the time functions have had their `gmtime_r` and
  `localtime_r` branches since the log landed, so this was expected to pass.
  Expected is not tested, and "runs on a Mac" is the kind of claim a README
  should not make on the strength of reading the source.

  The macOS runner is Apple Silicon, so it also builds for arm64. That is the
  half worth having beyond the platform itself: `char` is unsigned there and
  signed on x86, which is a real difference that no amount of running on one
  architecture can show. `g++` on macOS is a shim for Apple clang rather than
  GCC, so that pair is excluded rather than run as a second clang under another
  name — six jobs, not eight.

  Two golden cases turned out to be able to observe the machine, which is the
  same class as the bug that made `run.sh` pin the environment for every case
  rather than per case. `wc` pads its count with leading spaces on a BSD
  userland, so a binary fixture was described with the padding baked in. And
  `review-audio-unplayable` reached the "no audio available" path by naming
  `/bin/false`, which does not exist on macOS — `false` is in `/usr/bin` there
  — so FlashTerm rejected an override it could not execute, fell back to the
  built-in candidate list, and was answered by macOS's own `say`. The case had
  stopped testing a failed playback at all; `fake-audio.sh` now fails on demand
  via `FAKE_AUDIO_FAIL`. (#24)

## 0.3.0 — 2026-08-19

Pictures on cards, sync that puts two machines' reviews back together, and a CI
matrix that checks what used to be checked by hand.

### Added

- **`--absorb-conflicts`,** which merges the sync-conflict copies a file-sync
  client leaves beside the review log back into it, and brings the deck's
  counters and due dates up to date with the reviews they contain. Two machines
  reviewing before they sync no longer costs one side's scheduling: the log is
  append-only, so neither copy is wrong, and the two are simply unioned by event
  id. Syncthing's, Dropbox's and Nextcloud's naming schemes are recognised, and
  the copies are read and left in place rather than deleted.

  This is what `merge_events()` and `replay()` have been waiting for since #8.
  The question that kept `replay()` unwired — how replayed state should meet
  counters that predate the log — is answered by replaying the log twice, before
  the merge and after it, and applying only the difference. A deck whose history
  began before the log keeps it. (#20)

- **Images on cards.** A new eleventh deck column names a picture beside the
  deck, drawn inside the card frame above the prompt. Terminals that speak the
  kitty graphics protocol — kitty, Ghostty, WezTerm — need nothing installed:
  the escape sequence names the *file*, so it stays about sixty bytes however
  large the picture, which matters on a screen that redraws after every
  keypress. Everything else draws the picture as coloured text blocks through
  [chafa](https://hpjansson.org/chafa/) when it is installed, which needs no
  graphics support at all and so works over `ssh` and inside `tmux`.
  `FLASHTERM_IMAGE` overrides the guess with `kitty`, `chafa` or `none`.

  Detection reads `$TERM` alone. `$TERM_PROGRAM` and `$KITTY_WINDOW_ID` are
  inherited rather than set per session, so they outlive the terminal that set
  them and are still present under `tmux`, over `ssh`, or in a screen recorder
  — believing them reserved room for a picture that then could not be drawn,
  leaving a hole in the card. Terminals that can draw but do not say so in
  `$TERM`, such as WezTerm, want `FLASHTERM_IMAGE=kitty`.

  PNG, GIF and JPEG are understood, header only, so a large photograph costs
  no more to display than a thumbnail. Aspect ratio is preserved and the
  picture is fitted to the frame — the terminal stretches to fill whatever box
  it is handed, so a panorama would otherwise come out looking twice as wide
  as it is.

  A deck of pictures is still a deck: on a terminal that cannot draw them, in a
  pipe, or with `FLASHTERM_IMAGE=none`, it reviews as ordinary text. A missing
  file, or one that is not really an image, quietly becomes a card without a
  picture rather than an error. (#22)

- **Four more example decks, and a fifth rewritten.** `http-status.csv` (18
  codes, tagged by class), `elements.csv` (19 symbols, including the ones from
  Latin names that make the deck worth having), `nato-phonetic.csv` (all 26),
  and `colores.csv` with pictures. `general-knowledge.csv` grew from 5 cards to
  12 real ones.

  `nato-phonetic.csv` is also the first shipped deck that exercises the
  single-key guard: reviewed reversed, its answers are single letters, so `Q`
  and `A` would otherwise collide with the quit and audio keys. (#23)

### Changed

- **Piper setup moved to `docs/audio.md`,** leaving the README with what the
  feature *is* rather than how to install a text-to-speech engine. (#23)

- **CI builds under the sanitizers and with `-Werror`,** as a four-way matrix of
  both compilers against an optimised and a sanitised build. Every pull request
  so far was checked under `-fsanitize=address,undefined` by hand and came back
  clean, which is the argument for making it a job rather than a habit.

  `-Werror` is deliberately not in the Makefile: a warning should stop a change
  being merged, not stop a contributor building the project at all. The
  sanitised entry also compiles with `-fno-sanitize-recover=undefined`, without
  which UBSan prints its diagnostic and exits 0 — so a real finding would have
  been a green build that nobody reads the log of. (#21)

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
