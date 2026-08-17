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
# Called as `fake-audio.sh speak <text>` or `fake-audio.sh play <file>`.

kind=$1
shift
printf '%s: %s\n' "$kind" "$*" >>played.txt
