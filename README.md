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

**Rhythm & mood**: on Medium and up, long sentences get split and runs of similar short ones get joined so sentence length varies. Each rewrite also picks a slight mood (a bit more emphatic, hedged or relaxed) and adds a word or two to match, scaled to the tone.

**Typos** (dropdown next to the toggles): Off, Rare, Some, Many or Lots. After rewriting, Nano slips in small human-looking mistakes (its/it's, then/than, dont, could of, a dropped comma, a common misspelling or two swapped letters), at most one per sentence and never inside quotes, names or numbers.

**Tone & Type** (dropdowns above the Rewritten box): Tone = Natural, Casual, Formal, Academic or Simple; Type = Any, Essay, Email, Story, Report or Social post. They change the word register (plain vs formal words, contractions, "But/So" vs "However/Therefore"), which sentence rebuilds are allowed (no stiff passives in casual writing, shorter sentences for Simple/Social), and slang cleanup.

**Strength**: Light / Medium / Heavy / MAX controls how much changes. Rough success rates: Light 20%, Medium 45%, Heavy 50–75%, MAX 85–100%.

**MAX** rebuilds whole sentences with ~30 grammatical transforms (passives incl. perfect/modal, "According to…", "What X wants is…", "There was a … who…", "After doing X, …", moving side-clauses and asides to the front, splitting verb phrases, "not X but Y" merges, negation rephrasing, measurements, imperatives, list reordering, cause/effect and concession flips). Every rebuilt sentence is checked by the built-in grammar checker and rejected if it introduces a problem. It always does at least 10 passes (escalating to bigger rebuilds when it gets stuck), and once it hits 10 it keeps adding passes as long as the run is still under 5 seconds (up to 1000). A progress bar shows where it's at. Still 100% algorithmic, no AI.

**Passes** (1-1000): feeds each result back in for another round. Nano remembers every word it swapped out and every sentence shape it produced, so it never undoes or repeats a change, and it stops early once there's nothing new left. Click the arrow again to stop.

**AI detector tab**: paste any text and Nano scores how AI-like it reads (0–100%). Two parts run locally, offline: a small **trained model** (logistic regression on ~2,900 word/punctuation patterns + 40 style features, trained on ~75,000 human and AI samples: ChatGPT, Claude, Gemini, DeepSeek, Grok, Llama, Mistral and more, including "humanized" AI text) and a **pattern algorithm** (sentence rhythm, AI-favorite phrases highlighted purple, transitions, lists, contractions, mistakes). The score blends both. It's an estimate, not proof. The same tab checks grammar, spelling and punctuation (highlighted yellow). For a stronger second opinion there's an **Open Pangram** button.

Note: running the rewriter again on its own output (outside of Passes) may give unwanted results. Use Passes for multiple rounds.

## Shortcuts

- **Ctrl+Enter** in the Original box: rewrite
- **Ctrl+Alt+N** anywhere: rewrite whatever's on your clipboard and put the result back on the clipboard
- **Use as input**: moves the result up for a second pass

## Activation

First launch asks for a key (`NANO-XXXX-XXXX-XXXX`). Keys are checked offline against a secret that isn't in this repo, so builds from this source use a dev secret. The Update tab shows the copy's ID (the middle two groups); add an ID to `update/revoked.txt` to revoke that key on its next update check.

## Download

Grab [`update/FrutigerNano.ex_`](https://github.com/binjin-maker/FrutigerNano/raw/main/update/FrutigerNano.ex_) and rename it to `FrutigerNano.exe`. It isn't code-signed, so Smart App Control has to be off. Updates install from the **Update** tab (checks `update/latest.json`, verifies SHA-256).

## Building

Single C file, Win32 only. Cross-compile from Linux/macOS with Zig:

```bash
pip install ziglang
./build.sh
```

Settings (strength, passes, window scale) live in `FrutigerNano.ini` next to the EXE.
