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
emit_files() {
  find . -type f | LC_ALL=C sort | while read -r file; do
    printf -- '--- file %s ---\n' "${file#./}"
    cat "$file"
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

  # The host's own settings must not reach the app: FLASHTERM_DECK in the
  # developer's shell would otherwise send every case at their real deck.
  # TZ is pinned because scheduling is in local days while the log is in UTC,
  # and only a fixed zone makes the two agree on which day it is.
  (
    cd "$work_dir"
    # shellcheck disable=SC2086
    env -u FLASHTERM_DECK -u FLASHTERM_THEME TZ=UTC NO_COLOR=1 \
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
  } | awk -f "$script_dir/normalise.awk" >"$work_root/$name.transcript"

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
