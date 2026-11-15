# Frutiger Nano

Rewrites your text so it reads completely differently while saying exactly the same thing. One small EXE, no installer.

Made by **DOOG unincorporated**.

## What stays locked

Nano never touches:
- anything in "quotes", 'single quotes', “curly quotes”, (parentheses), [brackets], {braces} or `code`
- links, emails, @handles and #tags
- names (capitalized words mid-sentence, acronyms like NASA, possessives like Emma's)
- numbers, dates, money, percentages, and the unit right after a number ("12 percent", "3 weeks")
- with **Lock fact sentences** on: any whole sentence with a number, a month/weekday, or words like *born*, *founded*, *located*

Turn on **Highlight locked parts** to see exactly what's protected (blue = locked bits, green = locked fact sentences).

## Engines

**Offline** (free, instant, no internet): swaps words and phrases from a built-in thesaurus (with verb tenses and plurals handled), flips contractions, swaps sentence openers, and restructures sentences ("X because Y." → "Because Y, X." / "X, but Y." → "Although X, Y."). Each press gives a different version.

**Claude** (your own Anthropic API key, paid per use even with a Claude subscription): full rewrites. Locked parts are swapped for placeholders before the text is sent and restored exactly afterward, so Claude can't change them. If any placeholder goes missing, the result is thrown out.

**Strength**: Light / Medium / Heavy controls how much changes in both engines.

## Shortcuts

- **Ctrl+Enter** in the Original box: rewrite
- **Ctrl+Alt+N** anywhere: rewrite whatever's on your clipboard and put the result back on the clipboard
- **Use as input**: moves the result up for a second pass

## Activation

First launch asks for a key (`NANO-XXXX-XXXX-XXXX`). Keys are checked offline against a secret that isn't in this repo, so builds from this source use a dev secret.

## Download

Grab [`update/FrutigerNano.exe`](https://github.com/binjin-maker/FrutigerNano/raw/main/update/FrutigerNano.exe). It isn't code-signed, so Smart App Control has to be off. Updates install from the **Update** tab (checks `update/latest.json`, verifies SHA-256).

## Building

Single C file, Win32 only. Cross-compile from Linux/macOS with Zig:

```bash
pip install ziglang
./build.sh
```

Settings (engine, strength, API key, window scale) live in `FrutigerNano.ini` next to the EXE.
