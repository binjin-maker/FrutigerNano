# Frutiger Nano

Rewrites your text so it reads completely differently while saying exactly the same thing. One small EXE, no installer.

## What stays locked

Nano never touches:
- anything in "quotes", 'single quotes', “curly quotes”, (parentheses), [brackets], {braces} or `code`
- links, emails, @handles and #tags
- names (capitalized words mid-sentence, acronyms like NASA, possessives like Emma's)
- numbers, dates, money, percentages, and the unit right after a number ("12 percent", "3 weeks")
- with **Lock fact sentences** on: any whole sentence with a number, a month/weekday, or words like *born*, *founded*, *located*

Turn on **Highlight locked parts** to see exactly what's protected (blue = locked bits, green = locked fact sentences).

## How it rewrites

A built-in grammar tagger (an HMM trained on the Brown corpus) labels every word as noun, verb, adjective and so on, so swaps only happen when the part of speech and tense match. Everything runs offline, instantly. Nano rearranges sentences (moves "because/although" clauses, time phrases and adverbs to the other end, flips "X says Y" into "Y, X says", splits long sentences and merges short ones), flips active and passive voice, then swaps words and phrases from a built-in thesaurus (hand-written entries plus WordNet synonyms filtered by GloVe similarity) with verb tenses and plurals handled, flips contractions and swaps sentence openers. Each press gives a different version.

**Strength**: Light / Medium / Heavy controls how much changes.

**Passes** (1-1000): feeds each result back in for another round. Nano remembers every word it swapped out and every sentence shape it produced, so it never undoes or repeats a change, and it stops early once there's nothing new left. Click the arrow again to stop.

## Shortcuts

- **Ctrl+Enter** in the Original box: rewrite
- **Ctrl+Alt+N** anywhere: rewrite whatever's on your clipboard and put the result back on the clipboard
- **Use as input**: moves the result up for a second pass

## Activation

First launch asks for a key (`NANO-XXXX-XXXX-XXXX`). Keys are checked offline against a secret that isn't in this repo, so builds from this source use a dev secret. The Update tab shows the copy's ID (the middle two groups); add an ID to `update/revoked.txt` to revoke that key on its next update check.

## Download

Grab [`update/FrutigerNano.exe`](https://github.com/binjin-maker/FrutigerNano/raw/main/update/FrutigerNano.exe). It isn't code-signed, so Smart App Control has to be off. Updates install from the **Update** tab (checks `update/latest.json`, verifies SHA-256).

## Building

Single C file, Win32 only. Cross-compile from Linux/macOS with Zig:

```bash
pip install ziglang
./build.sh
```

Settings (engine, strength, API key, window scale) live in `FrutigerNano.ini` next to the EXE.
