#!/bin/sh
#
# Stands in for a speech synthesiser and for an audio player.
#
# Two problems it solves at once. A transcript that depended on whether the
# machine happened to have espeak or mpv installed would pass here and fail on
# CI, which has neither -- so the suite pins both to this instead of asking the
# PATH. And a test suite that made noise would be intolerable to run.
#
# What was played is appended to played.txt in the working directory, which the
# harness emits like any other file the run left behind. So the transcript
# records not merely that audio happened but exactly what was handed to the
# player, which is the part worth testing: the right side of the card, the
# resolved path, and the fall back to speech when a recording is missing.
#
# Called as `fake-audio.sh speak <text>`, `fake-audio.sh play <file>`, or
# `fake-audio.sh render <file>` with the text on standard input.
#
# FAKE_AUDIO_FAIL makes it report that it could not play, which is the only way
# a case can reach the "No audio available for this card" path. It exists
# because the alternative -- pointing FLASHTERM_TTS at a command that always
# fails -- is a fact about the machine rather than about FlashTerm, and it was
# wrong: the case named /bin/false, which does not exist on macOS, where
# `false` lives in /usr/bin. FlashTerm rejects an override it cannot execute
# and falls back to the built-in list, so the case quietly stopped testing a
# failure at all and was answered by macOS's own `say` instead. Nothing is
# recorded here, because nothing played.

if [ -n "${FAKE_AUDIO_FAIL:-}" ]; then
  exit 1
fi

kind=$1
shift

case $kind in
  render)
    # Writes the text it was asked to synthesise into the file it was told to
    # write. A real voice would put a WAV there and the transcript would show
    # nothing useful; this way the transcript shows which text reached which
    # file, which is the thing worth checking. It also makes the file really
    # exist, so the skip-if-present path is exercised for real.
    cat >"$1"
    printf 'render: %s\n' "$1" >>played.txt
    ;;
  *)
    printf '%s: %s\n' "$kind" "$*" >>played.txt
    ;;
esac
