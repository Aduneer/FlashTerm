# FlashTerm 🚀

[![CI](https://github.com/Aduneer/FlashTerm/actions/workflows/ci.yml/badge.svg)](https://github.com/Aduneer/FlashTerm/actions/workflows/ci.yml)

FlashTerm is a terminal flashcard app that makes you type the answer. No multiple choice and no self-grading: you either produce it or you don't, which is the whole point of active recall. Cards move through five Leitner boxes, each with a real review interval, so a card you know stops appearing until it is due again.

<img src="demo/demo.gif" width="640" alt="FlashTerm review session: a due card is answered with a typo, which is offered as an override rather than marked wrong; the card is promoted a Leitner box; then the statistics dashboard and a reversed review">

<sub>Recorded with <a href="https://github.com/charmbracelet/vhs">vhs</a> from <a href="demo/demo.tape">demo/demo.tape</a> — scripted, so it can be re-rendered whenever the UI changes.</sub>

---

## Quick Start

Needs a C++17 compiler (`g++` or `clang++`) and `make`. Linux and macOS are
both built and tested on every change; anything else POSIX will very likely
work, since nothing outside the standard library is used that POSIX does not
define.

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

./FlashTerm deck.txt --absorb-conflicts   # merge the sync-conflict copies of
                                          # deck.txt.log back into it

export FLASHTERM_DECK=~/Sync/spanish.txt   # the deck to use when none is named
export FLASHTERM_THEME=ocean               # default, ocean or sunset
```

Your decks are yours: `flashcards.txt` and every `.log` are deliberately not
tracked by git, so studying never shows up as a source change.

### Starter Decks

A new deck starts empty. To fill it, choose **5. Import flashcards** and give it
any file from `examples/`:

| Deck | |
| --- | --- |
| `cpp.csv`, `python.csv` | Language features and syntax |
| `git.csv`, `linux-cli.csv` | Commands worth having in your fingers |
| `http-status.csv` | Status codes, tagged by class |
| `elements.csv` | Chemical symbols, including the ones from Latin names |
| `nato-phonetic.csv` | The phonetic alphabet, all 26 |
| `spanish.csv`, `japanese.csv` | Vocabulary, foreign → English |
| `colores.csv` | Colours, English → Spanish, **with pictures** |
| `general-knowledge.csv` | A bit of everything |

Imported cards arrive in Box 1 and are due immediately. Most of the examples are
plain `question,answer,tags` records, the shortest form of the deck format below,
so they double as a template for writing your own. Several use `|` to accept more
than one answer, which is worth copying: `mañana` really does mean both
*tomorrow* and *morning*, and `git init` should not be marked wrong because you
typed `init`.

`nato-phonetic.csv` is worth reviewing **reversed** (`r`) at least once: the
answers become single letters, so `Q` and `A` would collide with the quit and
audio keys. They are withheld exactly when a card could accept them, which is
easier to believe once you have watched it happen.

**Adding audio to a language deck** takes one command — recordings are rendered
locally rather than shipped, so nothing large lives in this repo:

```bash
cp examples/spanish.csv ~/spanish.txt            # copy first: studying writes to the deck
FlashTerm ~/spanish.txt --generate-audio         # lists the voices you have
FlashTerm ~/spanish.txt --generate-audio --voice es_ES-davefx-medium
```

`ja_JA-hi_fi_captain-medium` does the same for `japanese.csv`, though Japanese
needs one extra package first — see [docs/audio.md](docs/audio.md).

`colores.csv` is the exception, and the one to copy if you want [pictures](#images):
each card carries a colour swatch from `examples/images/`. It also runs
**English → Spanish**, against the direction of every other language deck here,
and that is the point rather than an oversight. A picture shows you the
*meaning*, so on a `rojo → red` card it hands you the answer; on `red → rojo` it
cannot, because no picture spells a Spanish word. Put the picture on the side
that asks, not the side that answers.

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
* **Works with the sync tool you already have** — Decks are plain text and saves are atomic, so Syncthing, Dropbox, `rsync` or git sync a deck between machines with no support needed from FlashTerm. And when two machines review before they sync, `--absorb-conflicts` merges the conflict copy your sync tool left behind back into the review log and puts the scheduling it recorded back on the cards. See [Syncing Between Machines](#syncing-between-machines).
* **Images** — A card can name a picture in the deck's eleventh column, drawn inside the card frame. Terminals that speak the kitty graphics protocol (kitty, Ghostty) need nothing installed at all; everything else draws it as coloured text blocks via [chafa](https://hpjansson.org/chafa/), which works even over `ssh` and inside `tmux`. Aspect ratio is preserved and the picture is fitted to the frame, so a panorama and a portrait both land inside the borders. A deck of pictures still reviews as plain text anywhere that cannot draw them. See [Images](#images).
* **Deck statistics** — Success rates, review counts, a box-by-box mastery breakdown with ASCII bars, automatic flagging of your hardest card, and how much you reviewed today alongside your current daily streak.
* **Review log** — Every answer is appended to a `deck.txt.log` beside the deck: what was asked, which way round, whether you got it, and when, to the second. The card counters say what a card's state *is*; the log says what actually happened, which is what streaks, retention over time and merging two machines' reviews all need. It is append-only, so it never rewrites history and never conflicts.
* **Single-keypress menus** — `2` enters review; no Enter, no waiting. Every screen that takes a key shows a legend of what the keys do. Guarded on `isatty`, so piped input still reads whole lines and every script, pipeline and recording keeps working unchanged. `Ctrl+C` at a menu saves and exits cleanly rather than killing the process.
* **Framed cards** — The card under review is drawn in a box, centred in the window, with long questions wrapped to fit. Widths are measured in terminal columns rather than bytes, so the border still lines up on Japanese or accented cards — which is exactly where most tools get it wrong.
* **Hints** — Stuck at a blank prompt, press `?` to reveal the first letter and the shape of the rest: `l·  ··········`. It counts as a *partial* — the card holds its box instead of advancing, since you produced the answer but not unaided. The session summary gives partials their own line rather than filing them under either neighbour, so you can see how much of a session leant on the hint.
* **No dead ends** — `q` ends a review session from either prompt; leaving a card unanswered records nothing rather than counting it wrong. Every line prompt cancels on a bare Enter. Review keys are withheld on cards that accept them as answers, so a deck of vim keys or regex metacharacters stays reviewable.
* **Audio** — Press `a` during a review to hear the card: a recording if it has one, spoken by `espeak-ng` or whatever else is installed if it does not. `--generate-audio` renders a whole deck ahead of time with a good neural voice such as [Piper](https://github.com/OHF-voice/piper1-gpl). Nothing is linked against and nothing is required — with no player and no synthesiser on the machine, the key is simply never offered. See [Audio](#audio).
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

One CSV record per card, with the last eight fields optional:

```
question,answer,tags,correct,incorrect,box,last_reviewed,due_date,id,audio,image
```

Dates are plain `YYYY-MM-DD`, blank when a card has never been reviewed. Answers may list alternatives separated by `|`. Questions and answers containing commas or quotes are quoted normally, so decks stay greppable and editable by hand.

`id` is 16 hex characters identifying the card for the review log. It is filled
in automatically the first time the card is answered, so hand-written decks can
leave it off, and it stays the same when you edit the card — a fixed typo does
not orphan the card's history. Not on load, deliberately: opening a deck does
not change it, so a deck you only read is a deck your sync client and your
version control never see move.

`audio` is a recording of the *question*, as a path relative to the deck file —
so a deck and the audio directory beside it can be moved or synced as one thing.

`image` is a picture for the card, resolved the same way. See [Images](#images).

**Every card is written only as far as the last column it actually uses**, so a
deck of plain `question,answer,tags` rows — which is what the examples are, and
what you get writing one by hand — is saved back in that form rather than
expanded. A card that has never been reviewed says nothing in the six columns
after its tags, and so writes none of them.

That is also what keeps a deck byte for byte as earlier versions wrote it, so
syncing between a machine that has updated and one that has not does not put
the whole file in conflict. Trailing columns only: a card with a picture and no
recording still writes the empty audio column, because position is what names a
field in a CSV.

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

## Audio

Press `a` while a card is on screen to hear it. Nothing needs setting up, and
nothing needs installing — if the machine has neither a player nor a speech
synthesiser, review just never offers the key.

A card can point at a recording, in the deck's tenth column:

```
Bonjour,Hello,french,0,0,1,,,,audio/bonjour.mp3
```

The path is relative to the deck file, so `mydeck.txt` and the `audio/`
directory beside it travel together. Set it from inside the app by pressing `e`
on a card during review, or write it into the deck by hand. A card with no
recording is spoken instead, so a plain text deck has working audio from the
moment it is created.

**What plays, and when.** `a` plays what is on screen. In a normal review that
is the question, so you hear the recording. In a reversed review the question is
the thing you are being asked to produce, and playing it would hand you the
answer — so `a` speaks the *answer* on screen instead, and the question's
recording becomes available at the prompt after you have answered. That prompt
is where `a` is most useful anyway: it is the moment you find out what the word
was, and want to know how it sounds.

### Generating recordings

Speaking a card live is instant but robotic. Better voices are too slow to run
mid-review — around a second a card — so they are rendered once, ahead of time.

Start here, and let FlashTerm tell you the rest:

```bash
FlashTerm french.txt --generate-audio
```

With nothing set up, that prints what to install; once piper is installed it
prints the exact download command for a voice, including the right interpreter
— pipx hides piper's Python inside its own virtual environment, and the obvious
`python3 -m piper.download_voices` fails with a bare `ModuleNotFoundError`. Once
a voice is downloaded, it lists what you have. The whole path from nothing to
audio is three commands, and you are told each one at the point you need it.

```bash
pipx install piper-tts                            # it will tell you this
~/.local/share/pipx/venvs/piper-tts/bin/python3 \
  -m piper.download_voices fr_FR-siwis-medium     # and this, worked out for you
FlashTerm french.txt --generate-audio --voice fr_FR-siwis-medium
```

```
Rendering audio for 3 cards in french.txt
  rendered  audio/37e2df4b5396755a.wav  Bonjour
  rendered  audio/6cb389b96a2611d1.wav  Merci beaucoup
  rendered  audio/6476afccfb900dea.wav  Au revoir

3 rendered, 0 skipped, 0 failed
```

Files land in an `audio/` directory beside the deck and the paths are written
into the audio column. It is safe to re-run — cards that already have a
recording are skipped, so adding ten cards to a deck of five hundred renders
ten. `--force` re-renders everything, which is how a deck picks up a better
voice or a fixed typo.

Files are named after the card's id rather than its question, because an id is
already unique and already ASCII — `¿Dónde está la estación?` is a filename only
on a generous filesystem. The deck's audio column is what maps one to the other.

`--voice` takes a piper voice name. Voices are looked for in
`~/.local/share/piper-voices`, `~/.local/share/piper/voices` and `~/.cache/piper`
— set `FLASHTERM_VOICES` (colon-separated, like `PATH`) to add more, or pass a
path to a `.onnx` file directly. There is deliberately no default voice: a voice
implies a language, and guessing would render a French deck in English without
saying so.

To use something other than piper, `FLASHTERM_TTS_RENDER` takes any command at
all and wins over `--voice` when set. `{out}` is replaced with the file to write
and the text arrives on standard input — the one convention every candidate
already has, whatever it calls its output flag:

```bash
FLASHTERM_TTS_RENDER="espeak-ng -v fr --stdin -w {out}"
```

### Setting up piper

Piper is a neural text-to-speech system that runs offline and sounds
dramatically better than `espeak-ng` — which is the whole point of hearing a
card. FlashTerm does not bundle it, link against it, or require it.

**[docs/audio.md](docs/audio.md)** covers installing it, what the voices are,
where they live, why there is no default voice, and the one extra package
Japanese needs.

One general caveat: a recording only ever matches the question it was rendered
from. Edit a card's question and its audio is stale until you `--force`.

**Choosing a voice for live speech.** The defaults are whatever is installed, tried in order:
`espeak-ng`, `espeak`, `say`, `flite` for speech, and `mpv`, `ffplay`, `paplay`,
`pw-play`, `mpg123`, `afplay`, `aplay` for files. Override either one, including
arguments:

```bash
FLASHTERM_TTS="espeak-ng -v fr" FlashTerm french.txt
FLASHTERM_PLAYER="mpv --no-video --volume=70" FlashTerm french.txt
```

The command is split on whitespace and run directly — never through a shell —
and the text or path is appended as its last argument. A card whose question is
`rm -rf ~` is a card about shell quoting and stays one.

## Images

A card can carry a picture, in the deck's eleventh column:

```
el perro,the dog,animals,0,0,1,,,a1b2c3d4e5f60718,,images/perro.png
```

The path is relative to the deck file, so a deck and the `images/` directory
beside it move, sync and back up as one thing. It is drawn inside the card
frame, above the prompt:

```
┌──────────────────────────────────────────┐
│ Box 1  ·  new  ·  animals                │
├──────────────────────────────────────────┤
│                                          │
│              ▄▄▄▄▄▄▄▄▄▄▄▄                │
│              █ a picture █                │
│              ▀▀▀▀▀▀▀▀▀▀▀▀                │
│                                          │
│   el perro                               │
│                                          │
└──────────────────────────────────────────┘
```

PNG, GIF and JPEG are understood. Only the header is read — enough to learn the
dimensions — so a large photograph costs no more to display than a thumbnail.

### What your terminal needs

Nothing, on a terminal that speaks the **kitty graphics protocol**: kitty,
Ghostty and WezTerm are drawn to directly, with no library linked and no tool
installed. FlashTerm sends the *path* rather than the picture, so the escape
sequence is about sixty bytes however big the file is — which matters on a
screen that redraws after every keypress.

For anything else, install [chafa](https://hpjansson.org/chafa/) and the picture
is drawn as **coloured text blocks** — which need no graphics support of any
kind, and so work in any terminal at all, including inside `tmux` and over
`ssh`. That is the whole demo GIF above: those trees are text. chafa is optional
in exactly the way `espeak-ng` is — present, pictures; absent, no pictures.

`FLASHTERM_IMAGE` overrides the guess: `kitty`, `chafa`, or `none` to turn
pictures off entirely.

**Set it to `kitty` if your terminal can draw graphics but does not say so in
`$TERM`** — WezTerm is the common case. Detection reads `$TERM` and nothing
else, on purpose. `$TERM_PROGRAM` and `$KITTY_WINDOW_ID` look tempting and are
traps: they are ordinary environment variables, so they are *inherited* and
outlive the terminal that set them. A shell opened inside `tmux`, over `ssh`, or
in a screen recorder still carries them, and believing them means reserving room
for a picture in a terminal that cannot draw one — leaving a hole in the card,
which is worse than showing no picture at all. `$TERM` is replaced per session,
so it tells the truth.

**A deck full of pictures is still a deck.** On a terminal that cannot draw
them, in a pipe, or with `FLASHTERM_IMAGE=none`, the same deck reviews as
ordinary text — the picture is simply not shown, and nothing about the card,
its scheduling or its log changes. The same is true of a card whose image file
has been deleted or is not really an image: it quietly becomes a card without a
picture rather than an error.

**One thing to decide for yourself:** the picture is shown with the *question*,
not held back until the answer. For a visual-vocabulary deck that is the whole
point. For a deck where the picture *is* the answer, do not add one to that
card — there is no per-card setting for which side it belongs to.

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
deck write is lost, the record of *what you actually answered* usually is not
— and the next section is how you get it back.

### Absorbing sync-conflict copies

Two machines appending to the same log is the one case a sync client cannot
resolve on its own. It keeps one version, parks the other beside it under a
name like `spanish.txt.sync-conflict-20260818-101112-K3JQ7ZP.log`, and leaves
it for a human. But neither copy is wrong: they are two halves of one history,
and taking both is simply correct. So FlashTerm does:

```bash
FlashTerm ~/Sync/decks/spanish.txt --absorb-conflicts
```

Every conflict copy beside the log is merged into it by event id — a union,
not a concatenation, so the events both machines already had appear once — the
log is rewritten in timestamp order, and the deck's counters and due dates are
brought up to date with the reviews it just gained:

```
Reading 1 sync-conflict copy of spanish.txt.log:
  spanish.txt.sync-conflict-20260818-101112-K3JQ7ZP.log: 4 events
Absorbed 3 new events into spanish.txt.log, now 12 events.
  el perro                            +1 correct, box 2 -> 3, due 2026-08-25
  la casa                             +1 incorrect, due 2026-08-19
Updated 2 cards in spanish.txt.
The copy was left in place. Delete when you are happy with the result:
  rm spanish.txt.sync-conflict-20260818-101112-K3JQ7ZP.log
```

Syncthing, Dropbox and Nextcloud all name their copies differently and all
three are recognised, along with anything else whose inserted name contains
"conflict". A `.bak` or a `.tmp` sitting beside the log is not touched.

Two things it deliberately does not do:

* **It does not delete the conflict copies.** That is your call. Absorbing is
  idempotent — running it again finds nothing new — so leaving them costs only
  disk, while deleting the wrong file costs a history that exists nowhere else.
* **It does not rebuild your counters from the log.** A deck that predates the
  log has counters with no events behind them, and replaying it wholesale would
  report every such card as brand new. Instead the log is replayed *twice*,
  before the merge and after it, and only the difference is applied: your
  counters gain exactly what the other machine recorded, and a card's box and
  due date follow the merged log only when its newest event is no older than
  what the deck already says. History from before the log survives untouched.

If the deck file and the log arrive out of step — they are two files, and
nothing makes a sync client deliver them together — events naming cards this
deck has not received yet are kept in the log and counted the next time you
absorb, once the cards are there.

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
machines' logs), absorbing sync-conflict copies (which names count as one,
finding them, atomic log rewrites, and the differential replay that leaves
pre-log counters alone), images (PNG, GIF and JPEG header parsing including a
JPEG segment walk, aspect-preserving fitting, and protocol selection), text
layout (column-accurate word wrapping), and command-line and environment
handling.

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
`input` file, optionally a starting `deck.txt`, an `args` file, an `env` file of
`KEY=VALUE` lines, an `audio/` directory for the deck's audio column to point
at, and a `files/` directory whose contents are copied in as they are — which is
how a case ships a review log and the conflict copies beside it, whose names the
sync client invents, or an `images/` directory for the image column — and then:

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

The other source of run-to-run difference is the review shuffle, and
`FLASHTERM_SEED` settles it: set to a number, it fixes the order cards are asked
in, which is what lets a case script answers for a deck of more than one card.
The harness sets it, so a case need not. Leave it unset — as every real run
does — and the shuffle stays random; an unparseable value falls back to random
too, so a typo in a shell profile cannot quietly pin every session to one order.

Audio is pinned the same way and for the same reason. Which keys review offers
depends on what the machine can play, so a transcript recorded on a laptop with
`espeak-ng` would not match one recorded on CI, which has nothing. The harness
points `FLASHTERM_TTS` and `FLASHTERM_PLAYER` at `tests/golden/fake-audio.sh`,
which appends what it was asked to play to `played.txt` in the working directory
— and since the harness emits every file the run left behind, the transcript
ends up recording exactly what was handed to the player:

```
--- file played.txt ---
speak: Hello
play: audio/bonjour.wav
```

A case that needs to test the *absence* of audio sets the two variables empty
and `PATH=/nonexistent` in its `env` file; see `review-no-audio`.

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
| `src/audio.*` | Finding a player or synthesiser on the PATH, and running it |
| `src/generate.*` | `--generate-audio`: rendering a deck's recordings in bulk |
| `src/voice.*` | Finding piper voices on disk, and explaining how to get one |
| `src/image.*` | Drawing a card's picture: image headers, aspect fitting, terminal protocols |
| `src/event.*` | The append-only review log: events, ids, timestamps, merge and replay |
| `src/sync.*` | `--absorb-conflicts`: finding a sync client's conflict copies and folding them back in |
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

Issues and pull requests are welcome. `make check` should pass before you open one; CI runs both suites on Linux and macOS, on gcc and clang, in an optimised build and again under `-fsanitize=address,undefined`, all of them with `-Werror`. Warnings are deliberately not errors in the Makefile itself, so a warning never stops you building — it stops the change being merged.

Released versions and what changed in them are in [CHANGELOG.md](CHANGELOG.md).

## Credits

FlashTerm has no dependencies and bundles no third-party code. It can, however,
be pointed at other people's work, and that work deserves naming:

* **[Piper](https://github.com/OHF-voice/piper1-gpl)** by the Home Assistant
  authors — the neural text-to-speech behind `--generate-audio`, and the reason
  a language deck can sound like a person rather than a modem. GPL-3.0-or-later.
* **[eSpeak NG](https://github.com/espeak-ng/espeak-ng)** — the fallback voice,
  and what makes audio work on a machine where nothing has been set up.
  GPL-3.0-or-later.

Both are invoked as separate programs, never linked or redistributed, so their
licences apply to them and not to FlashTerm.

## License

MIT — see [LICENSE](LICENSE).
