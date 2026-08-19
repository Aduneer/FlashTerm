#!/bin/sh
#
# End-to-end golden tests.
#
# Each case drives the real binary with a scripted stdin and records everything
# the run produced -- the output, the exit status, and every file left in the
# working directory -- as one transcript, which is compared against a
# checked-in copy. Unlike tests/tests.cpp, which links the library and calls
# functions, nothing here knows the internals: if the menus, the framing or the
# scheduling text change, these fail, which is the point. ui.cpp and review.cpp
# are otherwise the least-tested files in the project.
#
# This works because every input path falls back to reading whole lines when
# stdin is not a terminal, and because colour, screen clearing and vertical
# centring all switch themselves off when stdout is not a terminal. Keep it
# that way: a golden test cannot drive an app that insists on a tty.
#
# A case is a directory under cases/ holding an `input` file and, optionally, a
# starting `deck.txt`, an `args` file, an `env` file of KEY=VALUE lines, an
# `audio/` directory of recordings for the deck's audio column to point at, and
# a `files/` directory whose contents are copied in as they are.
#
# Usage:
#   tests/golden/run.sh                 run every case
#   tests/golden/run.sh review-correct  run named cases only
#   tests/golden/run.sh --update        rewrite the expected transcripts
#
# Review the diff after --update. It is the only thing standing between a
# deliberate change and a silently accepted regression.

set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
root=$(CDPATH= cd -- "$script_dir/../.." && pwd)
bin=${FLASHTERM_BIN:-$root/FlashTerm}
work_root=$root/build/golden

update=0
if [ "${1:-}" = "--update" ]; then
  update=1
  shift
fi

if [ ! -x "$bin" ]; then
  echo "golden: no binary at $bin -- run 'make' first" >&2
  exit 1
fi

# Everything the run left behind, in a stable order, so a case that writes an
# export or a log covers it without having to say so.
#
# Sorted by the real filename, before normalisation, which matters for a case
# whose files are named after card ids: those are random, so two of them sort
# differently from one run to the next and the transcript shuffles. Such a case
# has to give its cards fixed ids in its deck fixture. See cases/generate-audio.
emit_files() {
  find . -type f | LC_ALL=C sort | while read -r file; do
    printf -- '--- file %s ---\n' "${file#./}"

    # A binary file is described rather than embedded. A picture fixture is
    # the case that forced this: its bytes are not what any test is about,
    # they make the transcript undiffable, and they are exactly the "invalid
    # multibyte data" that makes one awk behave differently from another.
    # Detected by looking for a NUL, which is what every "is this text" check
    # has always come down to.
    size=$(wc -c <"$file")
    stripped=$(LC_ALL=C tr -d '\000' <"$file" | wc -c)
    if [ "$size" -ne "$stripped" ]; then
      printf -- '<binary, %s bytes>\n' "$size"
      continue
    fi

    cat "$file"
    # A file that does not end in a newline would otherwise run into the next
    # header and hide its own last line inside it. Command substitution strips
    # trailing newlines, so a non-empty result means the last byte was not one.
    if [ -n "$(tail -c 1 "$file")" ]; then echo; fi
  done
}

run_case() {
  name=$1
  case_dir=$script_dir/cases/$name
  work_dir=$work_root/$name
  captured=$work_root/$name.output

  # A fresh directory per case, so that one case can never see what another
  # left behind and every run starts from the fixture and nothing else.
  rm -rf "$work_dir"
  mkdir -p "$work_dir"
  if [ -f "$case_dir/deck.txt" ]; then
    cp "$case_dir/deck.txt" "$work_dir/deck.txt"
  fi

  # Defaults to studying the deck the case supplied. A case that is about
  # argument handling overrides this with its own args file.
  args=deck.txt
  if [ -f "$case_dir/args" ]; then
    args=$(cat "$case_dir/args")
  fi

  # A case may add or override environment variables of its own, one KEY=VALUE
  # per line. It is how a case says "pretend this machine has no audio", which
  # cannot be expressed by input alone. "{golden}" stands for this directory,
  # so a case can put fakebin/ on the PATH without knowing where the checkout
  # lives.
  case_env=
  if [ -f "$case_dir/env" ]; then
    case_env=$(sed "s|{golden}|$script_dir|g" "$case_dir/env" | tr '\n' ' ')
  fi

  # Fixtures a case ships alongside its deck: recordings for the audio column
  # to point at, and voice models for --voice to find.
  for extra in audio voices; do
    if [ -d "$case_dir/$extra" ]; then
      cp -R "$case_dir/$extra" "$work_dir/$extra"
    fi
  done

  # Anything else the case wants in the working directory, copied in by
  # content rather than as a named directory. What needs it: a review log and
  # the sync-conflict copies beside it, whose names are the sync client's
  # invention and so cannot be a fixed fixture name like the two above.
  if [ -d "$case_dir/files" ]; then
    cp -R "$case_dir/files/." "$work_dir/"
  fi

  # The host's own settings must not reach the app: FLASHTERM_DECK in the
  # developer's shell would otherwise send every case at their real deck.
  # TZ is pinned because scheduling is in local days while the log is in UTC,
  # and only a fixed zone makes the two agree on which day it is.
  # FLASHTERM_SEED pins the review shuffle, which is what lets a case use more
  # than one card: the input script has to know which card it is answering.
  #
  # Everything else here pins what the app is allowed to notice about the
  # machine it is running on, because all of it reaches the output. Which keys
  # review offers depends on what can play sound; what --generate-audio says
  # depends on which piper voices are installed, and it will happily read the
  # developer's own home directory to find out. So the audio commands go to a
  # stub, PATH starts at a stub piper, and HOME and the voice directory point
  # inside the case's own working directory -- where a case can put voices/ if
  # it wants any to be found, and where there are none otherwise.
  #
  # FLASHTERM_IMAGE is pinned for the same reason and is the sharpest example
  # yet: whether a card shows a picture would otherwise depend on $TERM and on
  # whether chafa happens to be installed, so the same case would draw an image
  # on a developer's machine and nothing at all on CI. A case that wants
  # pictures names a protocol in its own env file.
  #
  # Pinned here rather than per case on purpose: a case that forgets passes on
  # a laptop and fails on CI, which is precisely what happened before this was
  # a default. The case env comes last, so a case can still override any of it.
  (
    cd "$work_dir"
    # shellcheck disable=SC2086
    env -u FLASHTERM_DECK -u FLASHTERM_THEME TZ=UTC NO_COLOR=1 FLASHTERM_SEED=1 \
      HOME=. FLASHTERM_VOICES=voices FLASHTERM_IMAGE=none \
      PATH="$script_dir/fakebin:/usr/bin:/bin" \
      FLASHTERM_TTS="$script_dir/fake-audio.sh speak" \
      FLASHTERM_PLAYER="$script_dir/fake-audio.sh play" \
      FLASHTERM_TTS_RENDER="$script_dir/fake-audio.sh render {out}" \
      $case_env \
      "$bin" $args <"$case_dir/input" >"$captured" 2>&1
  ) && status=0 || status=$?

  # Assembled raw and normalised in one pass at the end, so that the
  # placeholder numbering runs across the whole transcript: an id in the output
  # and the same id in the log have to come out with the same number, or the
  # transcript stops recording which record refers to which.
  {
    echo '--- output ---'
    cat "$captured"
    echo '--- exit status ---'
    echo "$status"
    cd "$work_dir" && emit_files
  } | LC_ALL=C awk -v root="$root" -f "$script_dir/normalise.awk" \
    >"$work_root/$name.transcript"

  if [ "$update" -eq 1 ]; then
    if [ -f "$case_dir/expected" ] &&
      cmp -s "$work_root/$name.transcript" "$case_dir/expected"; then
      echo "  unchanged  $name"
    else
      cp "$work_root/$name.transcript" "$case_dir/expected"
      echo "  written    $name"
    fi
    return 0
  fi

  if [ ! -f "$case_dir/expected" ]; then
    echo "  MISSING    $name (no expected transcript; run with --update)" >&2
    return 1
  fi
  if diff -u "$case_dir/expected" "$work_root/$name.transcript" \
    >"$work_root/$name.diff"; then
    echo "  ok         $name"
    return 0
  fi
  echo "  FAILED     $name" >&2
  sed -e "s|$case_dir/expected|expected|" \
    -e "s|$work_root/$name.transcript|actual|" "$work_root/$name.diff" >&2
  return 1
}

if [ "$#" -gt 0 ]; then
  cases=$*
else
  cases=$(cd "$script_dir/cases" && ls -1 | LC_ALL=C sort)
fi

mkdir -p "$work_root"
failed=0
total=0
for name in $cases; do
  if [ ! -d "$script_dir/cases/$name" ]; then
    echo "golden: no such case: $name" >&2
    exit 1
  fi
  total=$((total + 1))
  run_case "$name" || failed=$((failed + 1))
done

if [ "$failed" -gt 0 ]; then
  echo "golden: $failed of $total cases failed" >&2
  exit 1
fi
echo "golden: $total cases passed"
