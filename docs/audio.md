# Audio setup

Everything FlashTerm needs to speak a card, in more detail than the
[README](../README.md) wants to carry. Nothing here is required: with none of
it installed, review simply never offers the `a` key and `--generate-audio`
tells you what to do about it.

## About piper

[Piper](https://github.com/OHF-voice/piper1-gpl) is a neural text-to-speech
system from the Home Assistant authors. It runs offline, needs no GPU, and its
voices are dramatically better than `espeak-ng` for language learning — which is
the whole point of hearing a card. Voices are about 60 MB each and cover 30-odd
languages; list them all with
`python3 -m piper.download_voices --help` (using the interpreter FlashTerm
printed for you).

FlashTerm does not bundle, link against, or require piper. It runs whatever
command you put in `FLASHTERM_TTS_RENDER` as a separate process, so piper's
GPL-3.0 licence applies to piper and FlashTerm stays MIT — and swapping in a
different engine is a one-line change, not a fork.

## Japanese

Piper's Japanese voice needs one extra package, because it phonemizes through
`pyopenjtalk` rather than through espeak like the others, and `pipx install
piper-tts` does not bring it along:

```bash
pipx inject piper-tts pyopenjtalk
```

The first card is then slow — it downloads a pronunciation dictionary of about
23 MB once — and after that Japanese works like any other language, kanji
included. Without it, rendering fails with a `ModuleNotFoundError`;
FlashTerm recognises that particular failure and tells you the command above.
