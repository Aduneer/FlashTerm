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
# starting `deck.txt`, an `args` file, an `env` file of KEY=VALUE lines, and an
# `audio/` directory of recordings for the deck's audio column to point at.
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

  # The host's own settings must not reach the app: FLASHTERM_DECK in the
  # developer's shell would otherwise send every case at their real deck.
  # TZ is pinned because scheduling is in local days while the log is in UTC,
  # and only a fixed zone makes the two agree on which day it is.
  # FLASHTERM_SEED pins the review shuffle, which is what lets a case use more
  # than one card: the input script has to know which card it is answering.
  # The audio commands are pinned to a stub for the same kind of reason: which
  # keys review offers depends on what the machine can play, and a transcript
  # recorded on a developer's laptop would then not match one recorded on CI,
  # which has neither a synthesiser nor a player. The case env comes last so a
  # case can override any of it.
  (
    cd "$work_dir"
    # shellcheck disable=SC2086
    env -u FLASHTERM_DECK -u FLASHTERM_THEME TZ=UTC NO_COLOR=1 FLASHTERM_SEED=1 \
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
  } | awk -v root="$root" -f "$script_dir/normalise.awk" \
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
