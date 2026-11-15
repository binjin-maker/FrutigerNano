/* Frutiger Nano — rewrites text so it reads completely differently while saying the same thing.
   Offline engine (built-in thesaurus + sentence restructuring) or Claude with your own API key.
   Quotes, (parentheses), [brackets], links, code, names and numbers are never changed.
   Single file, Win32 only. */
#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <windowsx.h>
#include <richedit.h>
#include <uxtheme.h>
#include <wininet.h>
#include <bcrypt.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <wchar.h>
#include <wctype.h>

/* ---------- Nano rewrite engine (portable C, no Win32) ---------- */

static wchar_t *wdup(const wchar_t *s) { size_t n = wcslen(s) + 1; wchar_t *d = malloc(n * sizeof *d); memcpy(d, s, n * sizeof *d); return d; }
static wchar_t *wndup(const wchar_t *s, int n) { wchar_t *d = malloc((n + 1) * sizeof *d); memcpy(d, s, n * sizeof *d); d[n] = 0; return d; }

typedef struct { wchar_t *s; int n, cap; } Str;
static void sAddN(Str *b, const wchar_t *t, int n) {
    if (b->n + n + 1 > b->cap) { b->cap = (b->n + n + 1) * 2 + 64; b->s = realloc(b->s, b->cap * sizeof(wchar_t)); }
    memcpy(b->s + b->n, t, n * sizeof(wchar_t)); b->n += n; b->s[b->n] = 0;
}
static void sAdd(Str *b, const wchar_t *t) { sAddN(b, t, (int)wcslen(t)); }
static void sAddC(Str *b, wchar_t c) { sAddN(b, &c, 1); }

static unsigned long long nrs = 0x9E3779B97F4A7C15ULL;
static double nrnd(void) { nrs ^= nrs << 13; nrs ^= nrs >> 7; nrs ^= nrs << 17; return (nrs >> 11) * (1.0 / 9007199254740992.0); }
static int nri(int n) { int r = (int)(nrnd() * n); return r >= n ? n - 1 : r; }

static int isApos(wchar_t c) { return c == L'\'' || c == 0x2019; }
static void lowerInto(const wchar_t *s, int n, wchar_t *o) { for (int i = 0; i < n; i++) o[i] = isApos(s[i]) ? L'\'' : (wchar_t)towlower(s[i]); o[n] = 0; }

/* ---------- word lists ---------- */

static const wchar_t *COMMON[] = {
    L"the", L"a", L"an", L"this", L"that", L"these", L"those", L"it", L"its", L"it's", L"we", L"you", L"he", L"she", L"they",
    L"them", L"his", L"her", L"our", L"their", L"my", L"your", L"there", L"here", L"what", L"which", L"who", L"whom", L"whose",
    L"when", L"where", L"why", L"how", L"if", L"because", L"since", L"although", L"though", L"while", L"after", L"before",
    L"once", L"unless", L"until", L"as", L"so", L"but", L"and", L"or", L"nor", L"yet", L"for", L"to", L"of", L"in", L"on",
    L"at", L"by", L"with", L"from", L"about", L"into", L"over", L"under", L"between", L"through", L"during", L"without",
    L"within", L"than", L"then", L"also", L"just", L"only", L"even", L"still", L"not", L"no", L"yes", L"all", L"some", L"any",
    L"many", L"much", L"more", L"most", L"few", L"several", L"each", L"every", L"both", L"either", L"neither", L"one",
    L"other", L"another", L"such", L"same", L"own", L"too", L"quite", L"rather", L"always", L"never", L"is", L"are", L"was",
    L"were", L"be", L"been", L"being", L"am", L"do", L"does", L"did", L"done", L"have", L"has", L"had", L"having", L"can",
    L"could", L"will", L"would", L"shall", L"should", L"may", L"might", L"must", L"let", L"well", L"now", L"today",
    L"tomorrow", L"yesterday", L"everyone", L"everybody", L"someone", L"somebody", L"anyone", L"nobody", L"nothing",
    L"something", L"anything", L"everything", L"people", L"things", L"thing", L"time", L"way", L"day", L"days", L"year",
    L"years", L"life", L"world", L"man", L"woman", L"men", L"women", L"children", L"kids", L"family", L"friends", L"friend",
    L"school", L"work", L"home", L"water", L"food", L"money", L"students", L"teachers", L"dogs", L"cats", L"most", L"some",
    L"many", L"good", L"bad", L"new", L"old", L"first", L"last", L"next", L"every", L"our", L"don't", L"doesn't", L"didn't",
    L"isn't", L"aren't", L"wasn't", L"weren't", L"can't", L"won't", L"there's", L"that's", L"they're", L"we're", L"you're",
    L"he's", L"she's", L"what's", L"let's", L"getting", L"going", L"having", L"making", L"being", L"trying", L"after",
    L"living", L"working", L"playing", L"reading", L"writing", L"learning", L"studying", L"running", L"walking",
    L"then", L"yet", L"tonight", L"instead", L"soon", L"please", L"thanks", L"sure", L"okay", L"ok", L"oh", L"wow", L"hello",
    L"hi", L"hey", L"whether", L"whatever", L"whoever", L"wherever", L"besides", L"during", L"despite", L"unlike", L"like", NULL
};

static const wchar_t *DETS[] = { L"a", L"an", L"the", L"this", L"that", L"these", L"those", L"my", L"your", L"his", L"her",
    L"our", L"their", L"its", L"every", L"each", L"no", L"some", L"any", L"another", L"which", L"whose", L"what", L"one", NULL };
static const wchar_t *PREPS[] = { L"in", L"of", L"for", L"with", L"by", L"from", L"on", L"at", L"about", L"into", NULL };
static const wchar_t *VERBISH[] = { L"to", L"will", L"would", L"can", L"could", L"should", L"must", L"might", L"may", L"shall",
    L"i", L"you", L"we", L"they", L"he", L"she", L"it", L"not", L"don't", L"doesn't", L"didn't", L"won't", L"can't", L"never",
    L"who", L"also", L"often", L"always", L"usually", NULL };
static const wchar_t *NOUNPREV[] = { L"much", L"little", L"less", L"more", L"enough", NULL };
static const wchar_t *AFTERSET[] = { L"to", L"that", L"with", L"about", L"for", L"of", L"in", L"on", L"at", L"by", NULL };
static const wchar_t *PPCUE[] = { L"have", L"has", L"had", L"having", L"been", L"be", L"was", L"were", L"is", L"are", L"am",
    L"being", L"get", L"got", L"gets", L"i've", L"you've", L"we've", L"they've", L"it's", L"he's", L"she's", L"never", NULL };
static const wchar_t *ABBR[] = { L"mr", L"mrs", L"ms", L"dr", L"prof", L"st", L"jr", L"sr", L"vs", L"etc", L"inc", L"ltd",
    L"co", L"mt", L"ft", L"no", L"vol", L"fig", L"gen", L"gov", L"sen", L"rep", L"lt", L"col", L"sgt", L"capt", L"approx", L"dept", NULL };
static const wchar_t *UNITS[] = { L"percent", L"million", L"millions", L"billion", L"billions", L"trillion", L"thousand",
    L"thousands", L"hundred", L"hundreds", L"dozen", L"years", L"year", L"months", L"month", L"weeks", L"week", L"days", L"day",
    L"hours", L"hour", L"minutes", L"minute", L"seconds", L"second", L"miles", L"mile", L"km", L"kilometers", L"kilometres",
    L"meters", L"metres", L"meter", L"metre", L"feet", L"foot", L"inches", L"inch", L"pounds", L"pound", L"kg", L"kilograms",
    L"grams", L"lbs", L"degrees", L"dollars", L"dollar", L"cents", L"euros", L"people", L"times", L"am", L"pm", L"mph",
    L"acres", L"liters", L"litres", L"gallons", L"centuries", L"century", L"bc", L"ad", L"bce", L"ce", L"students",
    L"points", L"votes", L"cm", L"mm", L"mg", L"ml", L"gb", L"mb", L"kb", L"hz", L"ghz", L"watts", L"volts", NULL };
static const wchar_t *FACTWORDS[] = { L"january", L"february", L"march", L"april", L"june", L"july", L"august", L"september",
    L"october", L"november", L"december", L"monday", L"tuesday", L"wednesday", L"thursday", L"friday", L"saturday", L"sunday",
    L"born", L"died", L"founded", L"invented", L"discovered", L"located", L"established", L"population", L"percent",
    L"century", L"elected", L"capital", L"according", NULL };

static int inList(const wchar_t **l, const wchar_t *w) { for (; *l; l++) if (!wcscmp(*l, w)) return 1; return 0; }

/* ---------- thesaurus ----------
   T[flags] members   T: V verb (inflected), N noun (plural), A plain word, P phrase, O sentence opener, B block (never touch)
   members split by |; "a|b > c|d" = a,b only rewrite into c,d; +x = target only
   verb spec base:3sg:past:pp:ing (blank = regular), noun spec sing:plural; extra words after the head stay as-is
   flags: ! skip before "to"   ? skip before to/that/with/about/...   ^ skip after a determiner */
static const wchar_t *THES[] = {
    L"B as soon as|common sense|in common|by the way|on the way|as usual|in case|kind of|sort of|a little|at least|take it easy|a great deal|no longer|for good|at all|in time|of all time|the same|as well|make sense|makes sense|made sense|so far|in charge|at large|in short supply|start over|starts over|started over|starting over|in a way|all the way",

    L"V show:shows:showed:shown:showing up|arrive",
    L"V show:shows:showed:shown:showing|demonstrate|illustrate",
    L"V use|utilize|employ",
    L"V assist|aid",
    L"V begin:begins:began:begun:beginning|start",
    L"V try:tries:tried:tried:trying to|attempt to",
    L"V buy:buys:bought:bought:buying|purchase",
    L"V look:looks:looked:looked:looking for|search for",
    L"V find:finds:found:found:finding out > discover|learn",
    L"V allow|permit:permits:permitted:permitted:permitting",
    L"V improve|enhance",
    L"V get:gets:got:gotten:getting better > improve",
    L"V create|produce",
    L"V build:builds:built:built:building|construct",
    L"V destroy|ruin|wreck",
    L"V protect|shield|safeguard",
    L"V prevent > stop:stops:stopped:stopped:stopping",
    L"V change|alter",
    L"V understand:understands:understood:understood:understanding|grasp|comprehend",
    L"V realize|recognize",
    L"V discuss|talk about",
    L"V seem > appear",
    L"V! happen|occur:occurs:occurred:occurred:occurring|take:takes:took:taken:taking place",
    L"V receive > get:gets:got:gotten:getting",
    L"V obtain > get:gets:got:gotten:getting|gain|acquire",
    L"V claim that|argue that|assert that|contend that",
    L"V affect|influence",
    L"V solve|resolve",
    L"V examine|inspect",
    L"V verify > check|confirm",
    L"V assume|suppose|presume",
    L"V want to|wish to",
    L"V! love|adore",
    L"V! hate|despise|loathe",
    L"V talk|speak:speaks:spoke:spoken:speaking",
    L"V grab:grabs:grabbed:grabbed:grabbing|seize",
    L"V hurry|rush",
    L"V shout|yell",
    L"V whisper|murmur",
    L"V smile|grin:grins:grinned:grinned:grinning",
    L"V jump|leap:leaps:leapt:leapt:leaping",
    L"V gather|collect",
    L"V handle|deal:deals:dealt:dealt:dealing with",
    L"V emphasize|stress|highlight",
    L"V reveal|uncover",
    L"V hide:hides:hid:hidden:hiding|conceal",
    L"V remove|eliminate|get:gets:got:gotten:getting rid of",
    L"V explore|investigate|look:looks:looked:looked:looking into",
    L"V achieve|accomplish|attain",
    L"V encourage|motivate",
    L"V convince|persuade",
    L"V prefer:prefers:preferred:preferred:preferring|favor",
    L"V rely:relies:relied:relied:relying on|depend on|count on",
    L"V respond|reply",
    L"V connect|link",
    L"V combine|merge",
    L"V divide > split:splits:split:split:splitting|separate",
    L"V predict|forecast:forecasts:forecast:forecast:forecasting",
    L"V calculate|compute|work:works:worked:worked:working out",
    L"V repair|fix|mend",
    L"V damage|harm",
    L"V ignore|overlook:overlooks:overlooked:overlooked:overlooking|disregard",
    L"V imagine|envision",
    L"V! remember|recall",
    L"V depart > leave:leaves:left:left:leaving",
    L"V assess|evaluate",
    L"V recommend|suggest",
    L"V ensure|make:makes:made:made:making sure",
    L"V establish|set:sets:set:set:setting up",
    L"V expect|anticipate",
    L"V possess > own|have:has:had:had:having",
    L"V take:takes:took:taken:taking part in|participate in",
    L"V come:comes:came:come:coming up with|devise",
    L"V give:gives:gave:given:giving up|quit:quits:quit:quit:quitting",
    L"V figure out > work:works:worked:worked:working out|determine",
    L"V put:puts:put:put:putting off|postpone|delay",
    L"V carry out|perform|conduct",
    L"V look:looks:looked:looked:looking forward to > anticipate",
    L"V keep:keeps:kept:kept:keeping in mind|bear:bears:bore:borne:bearing in mind",
    L"V pay:pays:paid:paid:paying attention to|focus on|concentrate on",
    L"V run:runs:ran:run:running into|encounter|come:comes:came:come:coming across",
    L"V! say:says:said:said:saying|state",
    L"V mention|note",
    L"V choose:chooses:chose:chosen:choosing|select",
    L"V make:makes:made:made:making a decision|decide",
    L"V come:comes:came:come:coming to the conclusion|conclude",
    L"V take:takes:took:taken:taking a look at|look:looks:looked:looked:looking at",
    L"V pay:pays:paid:paid:paying a visit to|visit",
    L"V reduce > lower|decrease",
    L"V weep:weeps:wept:wept:weeping|sob:sobs:sobbed:sobbed:sobbing",
    L"V stare|gaze",
    L"V shiver|tremble",
    L"V wander|roam",
    L"V vanish|disappear",
    L"V depict|portray",
    L"V cherish|treasure",
    L"V clarify > explain|make:makes:made:made:making clear",

    L"N problem|issue",
    L"N idea|notion",
    L"N result|outcome",
    L"N goal|aim|objective",
    L"N opportunity:opportunities > chance",
    L"N benefit|advantage",
    L"N disadvantage|drawback|downside",
    L"N effect|impact",
    L"N example|instance",
    L"N kid > child:children",
    L"N person:people|individual",
    L"N car|vehicle|automobile",
    L"N occupation > job",
    L"N area|region",
    L"N trip|journey",
    L"N answer|response",
    L"N story:stories|tale",
    L"N mistake|error",
    L"N gift > present",
    L"N anger|rage|fury",
    L"N happiness|joy|delight",
    L"N sadness|sorrow",
    L"N skill|ability:abilities",
    L"N amount|quantity:quantities",
    L"N topic > subject|theme",
    L"N feature|trait|characteristic",
    L"N opinion > view|perspective|stance",
    L"N decision|choice",
    L"N plan|strategy:strategies",
    L"N building|structure",
    L"N teacher|instructor|educator",
    L"N student|pupil|learner",
    L"N doctor|physician",
    L"N boss|manager|supervisor",
    L"N worker|employee",
    L"N company:companies|business|firm",
    L"N customer|client",
    L"N country:countries|nation",
    L"N price > cost",
    L"N image > picture",
    L"N movie|film",
    L"N evidence|proof",
    L"N clue|hint",
    L"N crowd|throng",
    L"N enemy:enemies|foe|adversary:adversaries",
    L"N conversation|discussion|chat",
    L"N weakness|flaw|shortcoming",
    L"N success|achievement|accomplishment",
    L"N event|occasion",
    L"N beginning > start|outset",
    L"N purpose|intention",
    L"N method > approach|technique",
    L"N fear|dread",
    L"N route|path",
    L"N town > community",
    L"N trouble > difficulty:difficulties",

    L"A big|large",
    L"A huge|enormous|massive|immense|gigantic",
    L"A important|vital|crucial|essential",
    L"A difficult > hard|challenging|tough",
    L"A challenging > difficult|tough",
    L"A easy|simple|straightforward",
    L"A quick > fast|rapid|swift",
    L"A rapid > quick|swift",
    L"A quickly|rapidly|swiftly",
    L"A great|excellent|fantastic",
    L"A terrible|awful|dreadful|horrible",
    L"A beautiful|lovely|gorgeous|stunning",
    L"A ugly|unattractive",
    L"A? happy|cheerful|joyful",
    L"A sad|unhappy",
    L"A angry|furious",
    L"A scared|frightened",
    L"A? tired|exhausted|weary",
    L"A smart|intelligent|clever",
    L"A stupid|foolish",
    L"A strange|odd|weird|unusual|peculiar",
    L"A interesting|fascinating|intriguing",
    L"A boring|dull|tedious",
    L"A funny|amusing|humorous",
    L"A? rich|wealthy|affluent",
    L"A weak|feeble|frail",
    L"A strong|powerful",
    L"A obvious > clear|evident|apparent",
    L"A evident > clear|obvious",
    L"A clearly|obviously|plainly",
    L"A probably > likely|presumably",
    L"A maybe|perhaps|possibly",
    L"A^ really|truly",
    L"A^ very|extremely|incredibly",
    L"A often|frequently|regularly",
    L"A sometimes|occasionally|at times",
    L"A usually|typically|generally|normally",
    L"A mostly|mainly|largely|primarily|for the most part",
    L"A completely|totally|entirely|fully",
    L"A almost|nearly|practically",
    L"A fairly > rather|quite|reasonably",
    L"A suddenly|abruptly|all of a sudden",
    L"A immediately|instantly|right away",
    L"A eventually|ultimately|in the end",
    L"A lately > recently",
    L"A currently|presently|right now|at the moment|at present",
    L"A especially|particularly",
    L"A definitely|certainly|undoubtedly",
    L"A exactly|precisely",
    L"A simply|merely",
    L"A easily|effortlessly|readily",
    L"A happily|cheerfully",
    L"A quietly|softly",
    L"A loudly|noisily",
    L"A accurate > correct|exact",
    L"A incorrect > wrong|inaccurate",
    L"A whole|entire",
    L"A main|primary|chief|principal",
    L"A sufficient > enough",
    L"A similar|comparable",
    L"A rare|uncommon",
    L"A famous|well-known|renowned|celebrated",
    L"A tiny|minuscule",
    L"A dangerous|risky|hazardous|perilous",
    L"A hungry|starving|famished",
    L"A delicious|tasty",
    L"A chilly > cold|cool",
    L"A vacant > empty|unoccupied",
    L"A quiet|silent",
    L"A loud|noisy",
    L"A peaceful > calm|tranquil|serene",
    L"A tranquil > calm|peaceful",
    L"A brave|courageous|bold|fearless",
    L"A compassionate > caring|kind",
    L"A nice|pleasant",
    L"A honest|truthful|sincere",
    L"A careful|cautious",
    L"A feasible > possible|doable",
    L"A necessary|essential",
    L"A useful|helpful|handy",
    L"A useless|pointless",
    L"A modern|contemporary",
    L"A perfect|flawless|ideal",
    L"A excited|thrilled",
    L"A worried|anxious|nervous",
    L"A surprised|astonished|amazed",
    L"A confused|puzzled|baffled|perplexed",
    L"A friendly|welcoming",
    L"A amazing|incredible|remarkable|astonishing",
    L"A awesome > amazing|fantastic",
    L"A basic|fundamental",
    L"A complex|complicated|intricate",
    L"A unique|distinctive",
    L"A previous > prior|earlier",
    L"A subsequent > following|later",
    L"A additional > extra|further",
    L"A extra > additional",
    L"A various|diverse",
    L"A typical|usual|normal",
    L"A crucial > critical|essential",
    L"A polite|courteous|respectful",
    L"A rude|impolite|disrespectful",
    L"A lucky|fortunate",
    L"A unlucky|unfortunate",
    L"A huge|enormous",
    L"A afterwards|afterward|later on",
    L"A instantly|immediately",
    L"A constantly|continually|nonstop",

    L"P a lot of|lots of|plenty of",
    L"P because of|owing to|+due to",
    L"P in order to > so as to|to",
    L"P as a result of > because of|due to",
    L"P in addition to > along with|on top of|besides",
    L"P for example|for instance",
    L"P such as > like|including",
    L"P in my opinion|in my view|from my perspective",
    L"P on the other hand|by contrast|conversely",
    L"P as well as|along with|together with",
    L"P each other|one another",
    L"P even though > although|though",
    L"P although > even though|though",
    L"P in spite of|despite",
    L"P in order that > so that",
    L"P as long as|provided that|so long as",
    L"P of course > naturally|obviously",
    L"P in general|on the whole|by and large",
    L"P a little bit > slightly|a bit",
    L"P in fact|as a matter of fact|in reality",
    L"P at first|initially|in the beginning",
    L"P at last > finally",
    L"P is able to > can",
    L"P are able to > can",
    L"P am able to > can",
    L"P was able to > managed to|could",
    L"P were able to > managed to|could",
    L"P due to the fact that > because|since",
    L"P in spite of the fact that > even though|although",
    L"P at this point in time > now|currently",
    L"P in the event that > if|in case",
    L"P for the purpose of > for",
    L"P with regard to|regarding|with respect to|+concerning",
    L"P a number of > several|various",
    L"P the majority of > most|the bulk of",
    L"P in the near future > soon|before long",
    L"P soon > shortly",
    L"P prior to > before",
    L"P as a result > consequently",
    L"P pretty much > more or less|basically",
    L"P in the long run|over time|in the long term",
    L"P all the time > constantly",
    L"P a bunch of > a lot of|plenty of",
    L"P tons of > a lot of|plenty of",
    L"P in other words|put differently",
    L"P at the end of the day > ultimately|in the end",
    L"P whether or not > whether",

    L"O however|that said|even so|still",
    L"O nevertheless|nonetheless|all the same",
    L"O therefore|thus|as a result|consequently|for this reason|because of this|hence",
    L"O also|additionally|in addition|on top of that|furthermore|moreover|what's more|besides",
    L"O for example|for instance|as an example|to illustrate",
    L"O in conclusion|to conclude|in summary|to sum up|all in all|overall|in short",
    L"O first|first of all|to begin with|to start with|firstly",
    L"O second|secondly",
    L"O finally|lastly",
    L"O in fact|actually|as a matter of fact|in reality",
    L"O meanwhile|at the same time|in the meantime",
    L"O similarly|likewise|in the same way|by the same token",
    L"O unfortunately|sadly|regrettably|unluckily",
    L"O fortunately|luckily|thankfully",
    L"O basically|essentially|in essence",
    L"O generally|in general|usually|typically|for the most part|on the whole",
    L"O of course|naturally",
    L"O surprisingly|unexpectedly",
    L"O later|afterward|afterwards|after that|subsequently",
    L"O in other words|put differently|to put it another way|simply put",
    L"O honestly|frankly|to be honest|truthfully|to be frank",
    L"O personally|for my part|speaking for myself",
    L"O in my opinion|in my view|from my perspective|as I see it|to me",
    L"O clearly|obviously|evidently",
    L"O eventually|ultimately|in the end|in time",
    L"O suddenly|all of a sudden|without warning",
    L"O currently|right now|at the moment|at present|presently",
    L"O recently|lately|not long ago",
    L"O next|after that|then",
    L"O sometimes|at times|occasionally",
    L"O luckily|fortunately",
    NULL
};

/* contracted|expanded (expanded side may be several words) */
static const wchar_t *CONTR[] = {
    L"don't|do not", L"doesn't|does not", L"didn't|did not", L"isn't|is not", L"aren't|are not", L"wasn't|was not",
    L"weren't|were not", L"can't|cannot", L"won't|will not", L"wouldn't|would not", L"couldn't|could not",
    L"shouldn't|should not", L"haven't|have not", L"hasn't|has not", L"hadn't|had not", L"mustn't|must not",
    L"it's|it is", L"that's|that is", L"there's|there is", L"what's|what is", L"I'm|I am", L"you're|you are",
    L"we're|we are", L"they're|they are", L"he's|he is", L"she's|she is", L"I'll|I will", L"you'll|you will",
    L"we'll|we will", L"they'll|they will", NULL
};

typedef struct { wchar_t *f[5]; signed char src, tgt; } Mem;
typedef struct { wchar_t type, flags; int n; Mem *m; } Grp;
typedef struct { wchar_t *key; int g, m, f; } Ent;

static Grp *grp; static int ngrp, gcap;
#define TCAP 16384
static Ent tab[TCAP], otab[TCAP];

static unsigned hashW(const wchar_t *s) { unsigned h = 2166136261u; for (; *s; s++) h = (h ^ (unsigned)*s) * 16777619u; return h; }
static Ent *tFind(Ent *t, const wchar_t *k) {
    for (unsigned h = hashW(k) & (TCAP - 1);; h = (h + 1) & (TCAP - 1)) {
        if (!t[h].key) return NULL;
        if (!wcscmp(t[h].key, k)) return &t[h];
    }
}
static void tAdd(Ent *t, const wchar_t *k, int g, int m, int f) {
    unsigned h = hashW(k) & (TCAP - 1);
    while (t[h].key) { if (!wcscmp(t[h].key, k)) return; h = (h + 1) & (TCAP - 1); }
    t[h].key = wdup(k); t[h].g = g; t[h].m = m; t[h].f = f;
}

static int isVowelCh(wchar_t c) { return c && wcschr(L"aeiou", c) != NULL; }
static wchar_t *sfx(const wchar_t *b, const wchar_t *s, int cut) {
    int n = (int)wcslen(b) - cut; Str o = {0}; sAddN(&o, b, n); sAdd(&o, s); return o.s;
}
static wchar_t *mk3sg(const wchar_t *b) {
    int n = (int)wcslen(b);
    if (n > 1 && b[n - 1] == L'y' && !isVowelCh(b[n - 2])) return sfx(b, L"ies", 1);
    if (b[n - 1] == L's' || b[n - 1] == L'x' || b[n - 1] == L'z' || b[n - 1] == L'o' ||
        (n > 1 && (b[n - 1] == L'h') && (b[n - 2] == L'c' || b[n - 2] == L's'))) return sfx(b, L"es", 0);
    return sfx(b, L"s", 0);
}
static wchar_t *mkPast(const wchar_t *b) {
    int n = (int)wcslen(b);
    if (b[n - 1] == L'e') return sfx(b, L"d", 0);
    if (n > 1 && b[n - 1] == L'y' && !isVowelCh(b[n - 2])) return sfx(b, L"ied", 1);
    return sfx(b, L"ed", 0);
}
static wchar_t *mkIng(const wchar_t *b) {
    int n = (int)wcslen(b);
    if (n > 2 && b[n - 2] == L'i' && b[n - 1] == L'e') return sfx(b, L"ying", 2);
    if (n > 2 && b[n - 1] == L'e' && b[n - 2] != L'e' && b[n - 2] != L'o' && b[n - 2] != L'y') return sfx(b, L"ing", 1);
    return sfx(b, L"ing", 0);
}

static void parseMember(wchar_t type, const wchar_t *spec, Mem *m) {
    memset(m->f, 0, sizeof m->f);
    const wchar_t *sp = wcschr(spec, L' ');
    int hn = sp ? (int)(sp - spec) : (int)wcslen(spec);
    const wchar_t *tail = sp ? sp : L"";
    wchar_t head[96]; if (hn > 95) hn = 95; wmemcpy(head, spec, hn); head[hn] = 0;
    if (type != L'V' && type != L'N') { m->f[0] = wdup(spec); return; }
    wchar_t *part[5] = {0}; int np = 0;
    wchar_t *p = head;
    while (np < 5) { wchar_t *c = wcschr(p, L':'); if (c) *c = 0; part[np++] = p; if (!c) break; p = c + 1; }
    int nf = type == L'V' ? 5 : 2;
    wchar_t *base = part[0];
    for (int i = 0; i < nf; i++) {
        wchar_t *w;
        if (i < np && part[i] && part[i][0]) w = wdup(part[i]);
        else if (type == L'N') w = mk3sg(base);
        else if (i == 1) w = mk3sg(base);
        else if (i == 2) w = mkPast(base);
        else if (i == 3) w = (np > 2 && part[2][0]) ? wdup(part[2]) : mkPast(base);
        else w = mkIng(base);
        Str o = {0}; sAdd(&o, w); sAdd(&o, tail); free(w);
        m->f[i] = o.s;
    }
}

static int nForms(wchar_t type) { return type == L'V' ? 5 : type == L'N' ? 2 : 1; }

static void addGroup(wchar_t type, wchar_t flags, const wchar_t *body) {
    if (ngrp == gcap) { gcap = gcap ? gcap * 2 : 512; grp = realloc(grp, gcap * sizeof(Grp)); }
    Grp *g = &grp[ngrp]; g->type = type; g->flags = flags; g->n = 0; g->m = NULL;
    const wchar_t *gt = wcsstr(body, L" > ");
    int dir = gt != NULL;
    const wchar_t *p = body;
    while (*p) {
        const wchar_t *e = p; while (*e && *e != L'|' && !(dir && e == gt)) e++;
        int right = dir && p > gt;
        wchar_t tmp[128]; int n = (int)(e - p); if (n > 127) n = 127; wmemcpy(tmp, p, n); tmp[n] = 0;
        wchar_t *s = tmp; int tgtOnly = 0;
        while (*s == L' ') s++;
        if (*s == L'+') { tgtOnly = 1; s++; }
        int L2 = (int)wcslen(s); while (L2 && s[L2 - 1] == L' ') s[--L2] = 0;
        if (*s) {
            g->m = realloc(g->m, (g->n + 1) * sizeof(Mem));
            Mem *m = &g->m[g->n++];
            parseMember(type, s, m);
            if (type == L'B') { m->src = 1; m->tgt = 0; }
            else if (dir) { m->src = !right; m->tgt = right; }
            else { m->src = !tgtOnly; m->tgt = 1; }
        }
        if (!*e) break;
        p = (dir && e == gt) ? e + 3 : e + 1;
    }
    int gi = ngrp++;
    Ent *t = type == L'O' ? otab : tab;
    for (int i = 0; i < g->n; i++) {
        if (!g->m[i].src) continue;
        for (int f = 0; f < nForms(type); f++) {
            wchar_t k[160]; int n = (int)wcslen(g->m[i].f[f]); if (n > 159) n = 159;
            lowerInto(g->m[i].f[f], n, k);
            tAdd(t, k, gi, i, f);
        }
    }
}

static int nanoReady;
static void nanoInit(void) {
    if (nanoReady) return;
    nanoReady = 1;
    for (int i = 0; THES[i]; i++) {
        const wchar_t *l = THES[i];
        wchar_t type = l[0], flags = 0; int k = 1;
        while (l[k] && l[k] != L' ') { flags = l[k]; k++; }
        addGroup(type, flags, l + k + 1);
    }
    /* contractions: C = expand (contracted -> long), K = contract (long -> contracted) */
    for (int i = 0; CONTR[i]; i++) {
        const wchar_t *bar = wcschr(CONTR[i], L'|');
        wchar_t a[40], b[40]; int na = (int)(bar - CONTR[i]);
        wmemcpy(a, CONTR[i], na); a[na] = 0; wcscpy(b, bar + 1);
        wchar_t body[96];
        swprintf(body, 96, L"%ls > %ls", a, b); addGroup(L'C', 0, body);
        swprintf(body, 96, L"%ls > %ls", b, a); addGroup(L'K', 0, body);
    }
}

static int isKnownWord(const wchar_t *w) {
    if (inList(COMMON, w)) return 1;
    if (tFind(otab, w)) return 1;
    Ent *e = tFind(tab, w);
    return e && grp[e->g].type != L'C' && grp[e->g].type != L'K';
}

/* ---------- locks ----------
   1 = quote / brackets / link / code (exact)   2 = name   3 = number or fact token   4 = whole fact sentence */

static int findCh(const wchar_t *t, int n, int from, wchar_t c, int lim) {
    for (int k = from; k < n && k < from + lim; k++) if (t[k] == c) return k;
    return -1;
}
static int matchBr(const wchar_t *t, int n, int i) {
    wchar_t o = t[i], c = o == L'(' ? L')' : o == L'[' ? L']' : L'}';
    int d = 0;
    for (int k = i; k < n && k < i + 6000; k++) {
        if (t[k] == o) d++;
        else if (t[k] == c && --d == 0) return k;
    }
    return -1;
}
static int findSingleClose(const wchar_t *t, int n, int from, wchar_t c) {
    for (int k = from; k < n && k < from + 800; k++) {
        if (t[k] == L'\n') return -1;
        if (t[k] == c && k > from && !iswspace(t[k - 1]) && (k + 1 >= n || !iswalpha(t[k + 1]))) return k;
    }
    return -1;
}

static int isAbbrevBefore(const wchar_t *t, int dot) {
    int e = dot, s = dot;
    while (s > 0 && iswalpha(t[s - 1])) s--;
    if (e - s == 0 || (s > 0 && iswalnum(t[s - 1]))) return 0;
    if (e - s == 1) return 1;
    if (e - s > 6) return 0;
    wchar_t w[8]; lowerInto(t + s, e - s, w);
    return inList(ABBR, w);
}

/* is position p (start of a word) the first word of a sentence? */
static int sentInitial(const wchar_t *t, int p) {
    int k = p - 1, sawNl = 0;
    while (k >= 0 && iswspace(t[k])) { if (t[k] == L'\n') sawNl = 1; k--; }
    while (k >= 0 && t[k] && wcschr(L"\"“‘'([", t[k])) { k--; while (k >= 0 && iswspace(t[k])) { if (t[k] == L'\n') sawNl = 1; k--; } }
    if (k < 0 || sawNl) return 1;
    wchar_t c = t[k];
    if (c == L'.' || c == L'!' || c == L'?' || c == 0x2026) return c == L'.' ? !isAbbrevBefore(t, k) : 1;
    if (c == L'-' || c == L'*' || c == 0x2022) {
        int j = k - 1; while (j >= 0 && (t[j] == L' ' || t[j] == L'\t')) j--;
        return j < 0 || t[j] == L'\n';
    }
    if ((c == L'"' || c == 0x201D || c == 0x2019 || c == L')') && k > 0 && wcschr(L".!?", t[k - 1])) return 1;
    return 0;
}

static int wordEnd(const wchar_t *t, int n, int s) {
    int j = s;
    while (j < n && (iswalpha(t[j]) || ((isApos(t[j]) || t[j] == L'-') && j + 1 < n && iswalpha(t[j + 1])))) j++;
    return j;
}

static unsigned char *computeLocks(const wchar_t *t, int n, int strict) {
    nanoInit();
    unsigned char *L = calloc(n + 1, 1);
    for (int i = 0; i < n; i++) {
        wchar_t c = t[i]; int j = -1;
        if (c == L'"') j = findCh(t, n, i + 1, L'"', 6000);
        else if (c == 0x201C) j = findCh(t, n, i + 1, 0x201D, 6000);
        else if (c == 0xAB) j = findCh(t, n, i + 1, 0xBB, 6000);
        else if (c == 0x300C) j = findCh(t, n, i + 1, 0x300D, 6000);
        else if (c == L'`') j = findCh(t, n, i + 1, L'`', 6000);
        else if (c == L'(' || c == L'[' || c == L'{') j = matchBr(t, n, i);
        else if (c == 0x2018) j = findSingleClose(t, n, i + 1, 0x2019);
        else if (c == L'\'' && (i == 0 || !iswalnum(t[i - 1])) && i + 1 < n && iswalnum(t[i + 1])) j = findSingleClose(t, n, i + 1, L'\'');
        if (j > i) { memset(L + i, 1, j - i + 1); i = j; }
    }
    /* pass 2: links, handles, numbers, mid-sentence capitals */
    wchar_t **caps = NULL; int ncaps = 0;
    for (int pass = 0; pass < 2; pass++) {
        int i = 0;
        while (i < n) {
            if (L[i] || iswspace(t[i])) { i++; continue; }
            int s = i; while (i < n && !L[i] && !iswspace(t[i])) i++;
            int e = i, cs = s, ce = e;
            while (cs < ce && !iswalnum(t[cs]) && !wcschr(L"$€£¥#@", t[cs])) cs++;
            while (ce > cs && !iswalnum(t[ce - 1]) && t[ce - 1] != L'%' && t[ce - 1] != 0xB0) ce--;
            if (cs >= ce) continue;
            int digit = 0, at = -1;
            for (int k = cs; k < ce; k++) { if (iswdigit(t[k])) digit = 1; if (t[k] == L'@') at = k; }
            if (pass == 0) {
                if (!wcsncmp(t + cs, L"http://", 7) || !wcsncmp(t + cs, L"https://", 8) || !wcsncmp(t + cs, L"www.", 4) ||
                    (at > cs && at + 1 < ce && iswalnum(t[at - 1]) && iswalnum(t[at + 1])) ||
                    ((t[cs] == L'@' || t[cs] == L'#') && cs + 1 < ce && iswalnum(t[cs + 1]))) {
                    memset(L + cs, 1, ce - cs); continue;
                }
                if (digit) {
                    memset(L + cs, 3, ce - cs);
                    int k = e; while (k < n && (t[k] == L' ' || t[k] == L'\t')) k++;
                    int we = wordEnd(t, n, k);
                    if (we > k && we - k < 16 && !L[k]) {
                        wchar_t w[16]; lowerInto(t + k, we - k, w);
                        if (inList(UNITS, w)) memset(L + k, 3, we - k);
                    }
                    continue;
                }
            }
            /* words inside the raw token */
            for (int k = cs; k < ce;) {
                if (!iswalpha(t[k])) { k++; continue; }
                int we = wordEnd(t, n, k); if (we > ce) we = ce;
                int letters = 0, up = 0, innerUp = 0;
                for (int q = k; q < we; q++) if (iswalpha(t[q])) { letters++; if (iswupper(t[q])) { up++; if (q > k && t[q - 1] != L'-' && !isApos(t[q - 1])) innerUp = 1; } }
                int init = sentInitial(t, k);
                int isI = t[k] == L'I' && (we - k == 1 || isApos(t[k + 1]));
                if (pass == 0) {
                    if (!isI && ((up == letters && letters > 1) || innerUp || (iswupper(t[k]) && !init))) {
                        memset(L + k, 2, we - k);
                        if (we - k < 40) { caps = realloc(caps, (ncaps + 1) * sizeof *caps); caps[ncaps++] = wndup(t + k, we - k); }
                    }
                } else if (iswupper(t[k]) && init && !isI && !L[k]) {
                    int lock = 0;
                    wchar_t lw[40]; int wl = we - k < 39 ? we - k : 39; lowerInto(t + k, wl, lw);
                    if (isKnownWord(lw)) { k = we; continue; }
                    int q = we; while (q < n && (t[q] == L' ' || t[q] == L'\t')) q++;
                    if (q < n && iswupper(t[q]) && !(t[q] == L'I' && (q + 1 >= n || !iswalpha(t[q + 1])))) {
                        int qe = wordEnd(t, n, q);
                        if (!(we < n && wcschr(L",;:", t[we]))) lock = qe > q + 1 || L[q];
                    }
                    if (!lock && we - k > 1) {
                        int poss = we - k > 2 && isApos(t[we - 2]) && towlower(t[we - 1]) == L's';
                        static const wchar_t *SUF[] = { L"s", L"ing", L"ed", L"ly", L"tion", L"sion", L"ness", L"ment", L"est", L"ity", L"ies", L"ful", L"ous", L"ive", L"able", L"ism", L"ist", NULL };
                        int suf = 0;
                        for (int q = 0; SUF[q] && !suf; q++) { int sl = (int)wcslen(SUF[q]); if (wl > sl + 1 && !wcscmp(lw + wl - sl, SUF[q])) suf = 1; }
                        if (poss || !suf) lock = 1;
                    }
                    for (int c2 = 0; c2 < ncaps && !lock; c2++)
                        if ((int)wcslen(caps[c2]) == we - k && !wcsncmp(caps[c2], t + k, we - k)) lock = 1;
                    if (lock) memset(L + k, 2, we - k);
                }
                k = we;
            }
        }
    }
    for (int i = 0; i < ncaps; i++) free(caps[i]);
    free(caps);
    if (strict) {
        int s = 0;
        for (int i = 0; i <= n; i++) {
            int end = i == n || t[i] == L'\n' ||
                ((t[i] == L'.' || t[i] == L'!' || t[i] == L'?') && !L[i] && (i + 1 >= n || iswspace(t[i + 1])) && !(t[i] == L'.' && isAbbrevBefore(t, i)));
            if (!end) continue;
            int e = (i < n && t[i] != L'\n') ? i + 1 : i;
            int fact = 0;
            for (int k = s; k < e && !fact; k++) {
                if (L[k] == 3) fact = 1;
                else if (iswalpha(t[k]) && (k == 0 || !iswalpha(t[k - 1]))) {
                    int we = wordEnd(t, n, k);
                    if (we - k < 16) { wchar_t w[16]; lowerInto(t + k, we - k, w); if (inList(FACTWORDS, w)) fact = 1; }
                }
            }
            if (fact) {
                int a = s; while (a < e && iswspace(t[a])) a++;
                for (int k = a; k < e; k++) if (!L[k] || L[k] == 2 || L[k] == 3) L[k] = 4;
            }
            s = e;
        }
    }
    return L;
}

/* ---------- tokens ---------- */

enum { TK_WORD, TK_SPACE, TK_NL, TK_PUNCT, TK_LOCK };
typedef struct { wchar_t *s; int type, chg; } Tok;
typedef struct { Tok *t; int n, cap; } Toks;

static void tPush(Toks *v, int type, const wchar_t *s, int n, int chg) {
    if (v->n == v->cap) { v->cap = v->cap ? v->cap * 2 : 256; v->t = realloc(v->t, v->cap * sizeof(Tok)); }
    Tok k = { wndup(s, n), type, chg }; v->t[v->n++] = k;
}
static void tCopy(Toks *v, const Tok *k) { tPush(v, k->type, k->s, (int)wcslen(k->s), k->chg); }
static void tFree(Toks *v) { for (int i = 0; i < v->n; i++) free(v->t[i].s); free(v->t); v->t = NULL; v->n = v->cap = 0; }

static void tokenize(const wchar_t *t, int n, const unsigned char *L, Toks *v) {
    int i = 0;
    while (i < n) {
        int j = i;
        if (L[i]) { while (j < n && L[j]) j++; tPush(v, TK_LOCK, t + i, j - i, 0); }
        else if (t[i] == L'\n' || t[i] == L'\r') { while (j < n && !L[j] && (t[j] == L'\n' || t[j] == L'\r')) j++; tPush(v, TK_NL, t + i, j - i, 0); }
        else if (iswspace(t[i])) { while (j < n && !L[j] && iswspace(t[j]) && t[j] != L'\n' && t[j] != L'\r') j++; tPush(v, TK_SPACE, t + i, j - i, 0); }
        else if (iswalpha(t[i])) {
            j++;
            while (j < n && !L[j] && (iswalpha(t[j]) || ((isApos(t[j]) || t[j] == L'-') && j + 1 < n && !L[j + 1] && iswalpha(t[j + 1])))) j++;
            tPush(v, TK_WORD, t + i, j - i, 0);
        } else { j++; tPush(v, TK_PUNCT, t + i, 1, 0); }
        i = j;
    }
}

static int isEndP(const Tok *k) { return k->type == TK_PUNCT && (k->s[0] == L'.' || k->s[0] == L'!' || k->s[0] == L'?' || k->s[0] == 0x2026); }
static int isWordish(const Tok *k) { return k->type == TK_WORD || k->type == TK_LOCK; }

/* lowercase text of first / last word of a token */
static void firstWordLower(const wchar_t *s, wchar_t *o, int cap) {
    int n = 0; while (s[n] && s[n] != L' ' && n < cap - 1) n++;
    lowerInto(s, n, o);
}
static void lastWordLower(const wchar_t *s, wchar_t *o, int cap) {
    int e = (int)wcslen(s), b = e; while (b > 0 && s[b - 1] != L' ') b--;
    int n = e - b; if (n > cap - 1) n = cap - 1;
    lowerInto(s + b, n, o);
}

static int abbrevTok(const Tok *k) {
    if (k->type != TK_WORD) return 0;
    int n = (int)wcslen(k->s);
    if (n == 1) return 1;
    if (n > 6) return 0;
    wchar_t w[8]; lowerInto(k->s, n, w);
    return inList(ABBR, w);
}

static int tokSentStart(Toks *v, int i) {
    int k = i - 1;
    while (k >= 0 && v->t[k].type == TK_SPACE) k--;
    if (k < 0 || v->t[k].type == TK_NL) return 1;
    Tok *p = &v->t[k];
    if (isEndP(p)) return !(p->s[0] == L'.' && k > 0 && abbrevTok(&v->t[k - 1]));
    if (p->type == TK_PUNCT && wcschr(L"-*•", p->s[0])) {
        int j = k - 1; while (j >= 0 && v->t[j].type == TK_SPACE) j--;
        return j < 0 || v->t[j].type == TK_NL;
    }
    if (p->type == TK_LOCK) { int L2 = (int)wcslen(p->s); return L2 > 1 && wcschr(L".!?", p->s[L2 - 2]) && wcschr(L"\"”)", p->s[L2 - 1]); }
    return 0;
}

static void prevWordL(Toks *v, int i, wchar_t *o) {
    o[0] = 0;
    int k = i - 1; while (k >= 0 && v->t[k].type == TK_SPACE) k--;
    if (k >= 0 && isWordish(&v->t[k])) lastWordLower(v->t[k].s, o, 40);
}
static void nextWordL(Toks *v, int j, wchar_t *o) {
    o[0] = 0;
    int k = j; while (k < v->n && v->t[k].type == TK_SPACE) k++;
    if (k < v->n && isWordish(&v->t[k])) firstWordLower(v->t[k].s, o, 40);
}
static int nextIsWord(Toks *v, int j) {
    int k = j; while (k < v->n && v->t[k].type == TK_SPACE) k++;
    return k < v->n && isWordish(&v->t[k]);
}

/* try matching up to 6 words at token i; returns end token (exclusive) or -1 */
static Ent *matchAt(Toks *v, int i, Ent *table, int *endTok, int *nWords) {
    int idx[6], nw = 0, k = i;
    while (nw < 6 && k < v->n && v->t[k].type == TK_WORD && !v->t[k].chg) {
        idx[nw++] = k;
        if (k + 2 < v->n && v->t[k + 1].type == TK_SPACE && v->t[k + 2].type == TK_WORD && !v->t[k + 2].chg) k += 2; else break;
    }
    for (int w = nw; w >= 1; w--) {
        wchar_t key[200]; int p = 0;
        for (int q = 0; q < w; q++) {
            int L2 = (int)wcslen(v->t[idx[q]].s);
            if (p + L2 + 2 >= 200) { p = -1; break; }
            if (q) key[p++] = L' ';
            lowerInto(v->t[idx[q]].s, L2, key + p); p += L2;
        }
        if (p < 0) continue;
        key[p] = 0;
        Ent *e = tFind(table, key);
        if (e) { *endTok = idx[w - 1] + 1; *nWords = w; return e; }
    }
    return NULL;
}

static wchar_t aposCh = L'\'';

static wchar_t *casedCopy(const wchar_t *src, const wchar_t *tgt) {
    wchar_t *o = wdup(tgt);
    int letters = 0, up = 0;
    for (const wchar_t *p = src; *p; p++) if (iswalpha(*p)) { letters++; if (iswupper(*p)) up++; }
    if (letters > 1 && up == letters) { for (wchar_t *p = o; *p; p++) *p = (wchar_t)towupper(*p); }
    else if (iswupper(src[0])) o[0] = (wchar_t)towupper(o[0]);
    for (int i = 0; o[i]; i++) {
        if (o[i] == L'i' && (i == 0 || o[i - 1] == L' ') && (!o[i + 1] || o[i + 1] == L' ' || isApos(o[i + 1]))) o[i] = L'I';
        if (o[i] == L'\'') o[i] = aposCh;
    }
    return o;
}

static void replaceRange(Toks *v, int i, int j, wchar_t *s) {
    for (int k = i; k < j; k++) free(v->t[k].s);
    v->t[i].s = s; v->t[i].type = TK_WORD; v->t[i].chg = 1;
    memmove(v->t + i + 1, v->t + j, (v->n - j) * sizeof(Tok));
    v->n -= j - i - 1;
}

typedef struct { int strength, strict; } NanoOpts;
typedef struct { int words, changed, restructured, locks; } NanoStats;

static int pickTarget(Grp *g, int not) {
    int c[64], nc = 0;
    for (int i = 0; i < g->n && nc < 64; i++) if (g->m[i].tgt && i != not) c[nc++] = i;
    return nc ? c[nri(nc)] : -1;
}

static void swapPass(Toks *v, const NanoOpts *o, NanoStats *st, int contrMode) {
    static const double PW[3] = { 0.35, 0.7, 0.95 }, PP[3] = { 0.55, 0.85, 1.0 }, PO[3] = { 0.5, 0.8, 1.0 }, PC[3] = { 0.25, 0.7, 1.0 };
    int s = o->strength;
    for (int i = 0; i < v->n; i++) {
        Tok *t = &v->t[i];
        if (t->type != TK_WORD || t->chg) continue;
        int endTok, nw;
        if (tokSentStart(v, i)) {
            Ent *e = matchAt(v, i, otab, &endTok, &nw);
            if (e && endTok < v->n && v->t[endTok].type == TK_PUNCT && v->t[endTok].s[0] == L',') {
                if (nrnd() < PO[s]) {
                    Grp *g = &grp[e->g]; int m = pickTarget(g, e->m);
                    if (m >= 0) { replaceRange(v, i, endTok, casedCopy(v->t[i].s, g->m[m].f[0])); st->changed += nw; }
                }
                continue;
            }
        }
        Ent *e = matchAt(v, i, tab, &endTok, &nw);
        if (!e) continue;
        Grp *g = &grp[e->g];
        wchar_t prev[40], next[40];
        prevWordL(v, i, prev); nextWordL(v, endTok, next);
        double p;
        switch (g->type) {
        case L'B': i = endTok - 1; continue;
        case L'C': if (contrMode != 1) continue; p = PC[s]; break;
        case L'K': if (contrMode != 2 || !nextIsWord(v, endTok)) continue; p = PC[s]; break;
        case L'P': p = PP[s]; break;
        case L'V':
            if (inList(DETS, prev) || (e->f != 4 && inList(PREPS, prev))) continue;
            p = PW[s]; break;
        case L'N':
            if (inList(VERBISH, prev) || inList(NOUNPREV, prev) || !wcscmp(next, L"to")) continue;
            p = PW[s]; break;
        default: p = PW[s];
        }
        if (g->flags == L'!' && !wcscmp(next, L"to")) continue;
        if (g->flags == L'?' && inList(AFTERSET, next)) continue;
        if (g->flags == L'^' && inList(DETS, prev)) continue;
        if (nrnd() >= p) { i = endTok - 1; continue; }
        int m = pickTarget(g, e->m);
        if (m < 0) continue;
        int f = e->f;
        if (g->type == L'V' && f == 2 && !wcscmp(g->m[e->m].f[2], g->m[e->m].f[3])) {
            wchar_t pp[40]; prevWordL(v, i, pp);
            int k = i - 1; while (k >= 0 && v->t[k].type == TK_SPACE) k--;
            if (inList(PPCUE, pp)) f = 3;
            else if (k >= 0 && isWordish(&v->t[k])) {
                int k2 = k - 1; while (k2 >= 0 && v->t[k2].type == TK_SPACE) k2--;
                if (k2 >= 0 && isWordish(&v->t[k2])) { lastWordLower(v->t[k2].s, pp, 40); if (inList(PPCUE, pp)) f = 3; }
            }
        }
        const wchar_t *tgt = g->m[m].f[f < nForms(g->type) ? f : 0];
        wchar_t buf[128];
        if (g->type == L'C' && (!wcscmp(next, L"been") || !wcscmp(next, L"got") || !wcscmp(next, L"gotten"))) {
            int L2 = (int)wcslen(tgt);
            if (L2 > 3 && !wcscmp(tgt + L2 - 3, L" is") && L2 < 120) { wcscpy(buf, tgt); wcscpy(buf + L2 - 2, L"has"); tgt = buf; }
        }
        replaceRange(v, i, endTok, casedCopy(v->t[i].s, tgt));
        st->changed += nw;
    }
}

/* ---------- sentence restructuring ---------- */

static const wchar_t *FRONT[] = { L"even though", L"as soon as", L"because", L"since", L"although", L"though", L"if", L"when",
    L"while", L"whenever", L"unless", L"once", L"after", L"before", NULL };
static const wchar_t *TRAIL[] = { L"even though", L"as soon as", L"because", L"although", L"unless", L"whenever", L"since", NULL };
static const wchar_t *NOTBEFORE[] = { L"just", L"only", L"not", L"partly", L"simply", L"mainly", L"largely", L"and", L"or", L"but",
    L"even", L"ever", L"long", L"is", L"was", L"it's", L"that's", L"probably", NULL };

typedef struct { Tok *t; int n; } Span;

static int spanWords(Span s) { int w = 0; for (int i = 0; i < s.n; i++) if (isWordish(&s.t[i])) w += 1 + (s.t[i].type == TK_WORD ? (int)(wcschr(s.t[i].s, L' ') != NULL) : 0); return w; }
static Span trimSpan(Tok *t, int a, int b) {
    while (a < b && t[a].type == TK_SPACE) a++;
    while (b > a && t[b - 1].type == TK_SPACE) b--;
    Span s = { t + a, b - a }; return s;
}

static int lowerable(const Tok *k) {
    if (k->type == TK_LOCK) return 1;
    if (k->type != TK_WORD) return 0;
    wchar_t w[40]; firstWordLower(k->s, w, 40);
    if (!wcscmp(w, L"i") || !wcsncmp(w, L"i'", 2)) return 1;
    return isKnownWord(w);
}

static void pushSpan(Toks *o, Span s, int caseMode) {
    for (int i = 0; i < s.n; i++) {
        tCopy(o, &s.t[i]);
        if (i == 0 && o->t[o->n - 1].type == TK_WORD) {
            wchar_t *w = o->t[o->n - 1].s;
            if (caseMode > 0) w[0] = (wchar_t)towupper(w[0]);
            else if (caseMode < 0 && !(w[0] == L'I' && (!w[1] || w[1] == L' ' || isApos(w[1])))) w[0] = (wchar_t)towlower(w[0]);
        }
    }
}
static void pushW(Toks *o, const wchar_t *s, int chg) { tPush(o, TK_WORD, s, (int)wcslen(s), chg); }
static void pushSp(Toks *o) { tPush(o, TK_SPACE, L" ", 1, 0); }
static void pushP(Toks *o, wchar_t c) { tPush(o, TK_PUNCT, &c, 1, 0); }

static const wchar_t *conjVariant(const wchar_t *c) {
    if (!wcscmp(c, L"although") || !wcscmp(c, L"even though") || !wcscmp(c, L"though"))
        return nri(2) ? L"although" : L"even though";
    return c;
}

/* items = indexes of non-space tokens; returns count of tokens consumed if conj matches at token index ti */
static int conjAt(Tok *t, int n, int ti, const wchar_t **list, const wchar_t **which) {
    for (int c = 0; list[c]; c++) {
        const wchar_t *cj = list[c]; int k = ti; const wchar_t *p = cj;
        while (k < n) {
            if (t[k].type != TK_WORD) break;
            wchar_t w[60]; int L2 = (int)wcslen(t[k].s); if (L2 > 59) break;
            lowerInto(t[k].s, L2, w);
            if (wcsncmp(p, w, L2) || (p[L2] && p[L2] != L' ')) break;
            p += L2; k++;
            if (!*p) { *which = cj; return k - ti; }
            p++;
            if (k < n && t[k].type == TK_SPACE) k++; else break;
        }
    }
    return 0;
}

static int restructure(Tok *t, int n, Toks *o) {
    if (n < 6) return 0;
    Tok *last = &t[n - 1];
    if (!(last->type == TK_PUNCT && (last->s[0] == L'.' || last->s[0] == L'!'))) return 0;
    int commas = 0, comma = -1;
    for (int i = 0; i < n - 1; i++) {
        if (t[i].type == TK_PUNCT) {
            if (t[i].s[0] == L',') { commas++; comma = i; }
            else return 0;
        }
        if (t[i].type == TK_NL) return 0;
    }
    const wchar_t *cj = NULL;
    /* 1: fronted clause  "Because X, Y."  ->  "Y because X." */
    int cn = conjAt(t, n, 0, FRONT, &cj);
    if (cn && commas == 1) {
        Span clause = trimSpan(t, cn, comma), mainS = trimSpan(t, comma + 1, n - 1);
        if (spanWords(clause) < 2 || spanWords(mainS) < 2) return 0;
        wchar_t fw[40]; if (mainS.t[0].type == TK_WORD) { firstWordLower(mainS.t[0].s, fw, 40); if (!wcscmp(fw, L"then")) return 0; }
        pushSpan(o, mainS, 1); pushSp(o);
        pushW(o, conjVariant(cj), 1); pushSp(o);
        pushSpan(o, clause, 0);
        tCopy(o, last);
        return 1;
    }
    if (cn) return 0;
    /* 2: trailing clause  "X because Y."  ->  "Because Y, x." */
    if (commas == 0) {
        for (int i = 2; i < n - 3; i++) {
            if (t[i].type != TK_WORD || t[i - 1].type != TK_SPACE) continue;
            int k = conjAt(t, n, i, TRAIL, &cj);
            if (!k) continue;
            Span mainS = trimSpan(t, 0, i), clause = trimSpan(t, i + k, n - 1);
            if (spanWords(mainS) < 3 || spanWords(clause) < 3) return 0;
            wchar_t pv[40]; int pk = i - 2; while (pk >= 0 && t[pk].type == TK_SPACE) pk--;
            if (pk >= 0 && t[pk].type == TK_WORD) { lastWordLower(t[pk].s, pv, 40); if (inList(NOTBEFORE, pv)) return 0; }
            for (int q = 0; q < clause.n; q++) { const wchar_t *x; if (clause.t[q].type == TK_WORD && conjAt(clause.t, clause.n, q, FRONT, &x)) return 0; }
            if (!lowerable(&mainS.t[0])) return 0;
            wchar_t *c2 = wdup(conjVariant(cj)); c2[0] = (wchar_t)towupper(c2[0]);
            pushW(o, c2, 1); free(c2); pushSp(o);
            pushSpan(o, clause, 0);
            pushP(o, L','); pushSp(o);
            pushSpan(o, mainS, -1);
            tCopy(o, last);
            return 1;
        }
        return 0;
    }
    /* 3: "X, but Y." / "X, so Y." */
    if (commas == 1) {
        int k = comma + 1; while (k < n && t[k].type == TK_SPACE) k++;
        if (k >= n || t[k].type != TK_WORD) return 0;
        wchar_t w[40]; firstWordLower(t[k].s, w, 40);
        int isBut = !wcscmp(w, L"but"), isSo = !wcscmp(w, L"so");
        if (!isBut && !isSo) return 0;
        Span x = trimSpan(t, 0, comma), y = trimSpan(t, k + 1, n - 1);
        if (spanWords(x) < 3 || spanWords(y) < 3) return 0;
        if (y.t[0].type == TK_WORD) { wchar_t yw[40]; firstWordLower(y.t[0].s, yw, 40); if (!wcscmp(yw, L"that") || !wcscmp(yw, L"far") || !wcscmp(yw, L"much")) return 0; }
        int canFront = lowerable(&x.t[0]);
        int choice = nri(isSo ? 3 : 2);
        if (choice == 0 && !canFront) choice = 1;
        if (choice == 0) {
            pushW(o, isBut ? (nri(2) ? L"Although" : L"Even though") : (nri(2) ? L"Because" : L"Since"), 1); pushSp(o);
            pushSpan(o, x, -1); pushP(o, L','); pushSp(o); pushSpan(o, y, 0);
        } else if (choice == 1) {
            pushSpan(o, x, 0); pushP(o, L'.'); pushSp(o);
            static const wchar_t *BUTS[] = { L"However", L"Still", L"Even so" }, *SOS[] = { L"As a result", L"Because of this", L"For that reason" };
            pushW(o, isBut ? BUTS[nri(3)] : SOS[nri(3)], 1); pushP(o, L','); pushSp(o);
            pushSpan(o, y, 0);
        } else {
            pushSpan(o, x, 0); pushP(o, L','); pushSp(o); pushW(o, L"which is why", 1); pushSp(o); pushSpan(o, y, 0);
        }
        tCopy(o, last);
        return 1;
    }
    return 0;
}

static void restructurePass(Toks *v, const NanoOpts *op, NanoStats *st) {
    static const double PR[3] = { 0.15, 0.5, 0.9 };
    Toks o = {0};
    int i = 0;
    while (i < v->n) {
        while (i < v->n && (v->t[i].type == TK_SPACE || v->t[i].type == TK_NL)) tCopy(&o, &v->t[i++]);
        int s = i;
        while (i < v->n && v->t[i].type != TK_NL) {
            if (isEndP(&v->t[i]) && !(v->t[i].s[0] == L'.' && i > 0 && abbrevTok(&v->t[i - 1])) &&
                (i + 1 >= v->n || v->t[i + 1].type != TK_WORD)) {
                while (i + 1 < v->n && isEndP(&v->t[i + 1])) i++;
                i++; break;
            }
            i++;
        }
        if (i > s) {
            if (nrnd() < PR[op->strength] && restructure(v->t + s, i - s, &o)) st->restructured++;
            else for (int k = s; k < i; k++) tCopy(&o, &v->t[k]);
        }
    }
    tFree(v); *v = o;
}

static void articlePass(Toks *v) {
    for (int i = 0; i + 2 < v->n; i++) {
        Tok *a = &v->t[i];
        if (a->type != TK_WORD) continue;
        wchar_t al[4]; int L2 = (int)wcslen(a->s); if (L2 > 2) continue;
        lowerInto(a->s, L2, al);
        int isA = !wcscmp(al, L"a"), isAn = !wcscmp(al, L"an");
        if (!isA && !isAn) continue;
        if (v->t[i + 1].type != TK_SPACE) continue;
        Tok *nx = &v->t[i + 2];
        if (!isWordish(nx) || (!nx->chg && !a->chg)) continue;
        wchar_t w[12]; int nl = 0; while (nx->s[nl] && nl < 11) nl++; lowerInto(nx->s, nl, w);
        if (!iswalpha(w[0])) continue;
        int wantAn = -1;
        if (!wcsncmp(w, L"one", 3) || !wcsncmp(w, L"once", 4) || !wcsncmp(w, L"uni", 3) || !wcsncmp(w, L"use", 3) ||
            !wcsncmp(w, L"usu", 3) || !wcsncmp(w, L"eu", 2)) wantAn = 0;
        else if (!wcsncmp(w, L"hour", 4) || !wcsncmp(w, L"honest", 6) || !wcsncmp(w, L"honor", 5) || !wcsncmp(w, L"heir", 4)) wantAn = 1;
        else if (wcschr(L"aeio", w[0])) wantAn = 1;
        else if (w[0] != L'u' && w[0] != L'h') wantAn = 0;
        if (wantAn < 0 || wantAn == isAn) continue;
        int cap = iswupper(a->s[0]);
        free(a->s);
        a->s = wdup(wantAn ? (cap ? L"An" : L"an") : (cap ? L"A" : L"a"));
        a->chg = 1;
    }
}

static int countContractions(Toks *v) {
    int c = 0;
    for (int i = 0; i < v->n; i++) if (v->t[i].type == TK_WORD) for (wchar_t *p = v->t[i].s; *p; p++) if (isApos(*p)) { c++; break; }
    return c;
}

static wchar_t *nanoRewrite(const wchar_t *text, const NanoOpts *op, NanoStats *st, unsigned long long seed) {
    nanoInit();
    memset(st, 0, sizeof *st);
    nrs = seed ? seed : 0x9E3779B97F4A7C15ULL;
    for (int i = 0; i < 4; i++) nrnd();
    int n = (int)wcslen(text);
    unsigned char *L = computeLocks(text, n, op->strict);
    int curly = 0, straight = 0;
    for (int i = 0; i < n; i++) { if (text[i] == 0x2019) curly++; else if (text[i] == L'\'') straight++; }
    aposCh = curly > straight ? 0x2019 : L'\'';
    Toks v = {0};
    tokenize(text, n, L, &v);
    for (int i = 0; i < v.n; i++) {
        if (v.t[i].type == TK_WORD) st->words++;
        else if (v.t[i].type == TK_LOCK) {
            st->locks++;
            for (wchar_t *p = v.t[i].s; *p; p++) if (iswalnum(*p) && (p == v.t[i].s || !iswalnum(p[-1]))) st->words++;
        }
    }
    int contrMode = countContractions(&v) ? 1 : 2;
    swapPass(&v, op, st, contrMode);
    restructurePass(&v, op, st);
    articlePass(&v);
    Str out = {0}; sAdd(&out, L"");
    for (int i = 0; i < v.n; i++) sAdd(&out, v.t[i].s);
    tFree(&v); free(L);
    return out.s;
}

/* ---------- app globals ---------- */

#define APP_VERSION   "1.0.1"
#define APP_VERSION_W L"1.0.1"

#if defined(__has_include)
#if __has_include("secret.h")
#include "secret.h"
#endif
#endif
#ifndef NANO_SECRET
#define NANO_SECRET "DEV-BUILD"
#endif

#define BW 760
#define BH 470

#define WM_STATUS     (WM_APP + 1)
#define WM_NANODONE   (WM_APP + 2)
#define WM_UPDCHECKED (WM_APP + 5)
#define WM_UPDFAIL    (WM_APP + 6)
#define WM_UPDRESTART (WM_APP + 7)

enum { ID_IN = 101, ID_OUT, ID_GO, ID_COPY, ID_USE, ID_CLR,
       ID_KEY = 201, ID_MODEL, ID_EXTRA,
       ID_UCHECK = 301, ID_UINSTALL, ID_UPAGE, ID_UNOTES,
       ID_ACTKEY = 401, ID_ACT };
#define HK_CLIP 1
#define TIMER_HL 7

static HWND hMain, hFrame;
static wchar_t iniPath[MAX_PATH], exePath[MAX_PATH];
static float dpi = 1.0f, uiScale = 1.0f;
static volatile LONG nanoBusy;
static int activated;

static int S(int v) { return (int)(v * dpi * uiScale + 0.5f); }

static void status(const wchar_t *fmt, ...) {
    wchar_t *b = malloc(512 * sizeof(wchar_t));
    va_list ap; va_start(ap, fmt); _vsnwprintf(b, 511, fmt, ap); va_end(ap); b[511] = 0;
    PostMessageW(hMain, WM_STATUS, 0, (LPARAM)b);
}

/* ---------- Frutiger Aero renderer ---------- */

typedef struct { int w, h; DWORD *px; HDC dc; HBITMAP bm, old; } Canvas;

static Canvas mkCanvas(int w, int h) {
    Canvas c = { w, h, NULL, NULL, NULL, NULL };
    BITMAPINFO bi = {0};
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = w; bi.bmiHeader.biHeight = -h;
    bi.bmiHeader.biPlanes = 1; bi.bmiHeader.biBitCount = 32; bi.bmiHeader.biCompression = BI_RGB;
    c.dc = CreateCompatibleDC(NULL);
    c.bm = CreateDIBSection(c.dc, &bi, DIB_RGB_COLORS, (void **)&c.px, NULL, 0);
    c.old = SelectObject(c.dc, c.bm);
    return c;
}
static void freeCanvas(Canvas *c) { SelectObject(c->dc, c->old); DeleteObject(c->bm); DeleteDC(c->dc); }

static double clamp01(double v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
static int lerpCol(int a, int b, double t) {
    int r = ((a >> 16) & 255) + (int)((((b >> 16) & 255) - ((a >> 16) & 255)) * t);
    int g = ((a >> 8) & 255) + (int)((((b >> 8) & 255) - ((a >> 8) & 255)) * t);
    int l = (a & 255) + (int)(((b & 255) - (a & 255)) * t);
    return (r << 16) | (g << 8) | l;
}
static void blend(Canvas *c, int x, int y, int col, double a) {
    if (x < 0 || y < 0 || x >= c->w || y >= c->h || a <= 0) return;
    if (a > 1) a = 1;
    DWORD *p = c->px + y * c->w + x;
    *p = (DWORD)lerpCol((int)(*p & 0xffffff), col, a);
}

static double sdRR(double px, double py, double x, double y, double w, double h, double r) {
    double qx = fabs(px - (x + w / 2)) - (w / 2 - r), qy = fabs(py - (y + h / 2)) - (h / 2 - r);
    return hypot(fmax(qx, 0), fmax(qy, 0)) + fmin(fmax(qx, qy), 0) - r;
}

#define BBOX(BX, BY, BW, BH, BP) \
    int x0 = (int)floor((BX) - (BP)), y0 = (int)floor((BY) - (BP)), x1 = (int)ceil((BX) + (BW) + (BP)), y1 = (int)ceil((BY) + (BH) + (BP)); \
    if (x0 < 0) x0 = 0; if (y0 < 0) y0 = 0; if (x1 > c->w) x1 = c->w; if (y1 > c->h) y1 = c->h;

static void rrFill(Canvas *c, double x, double y, double w, double h, double r, int c1, int c2, double a1, double a2) {
    BBOX(x, y, w, h, 1)
    for (int py = y0; py < y1; py++) for (int px = x0; px < x1; px++) {
        double cov = clamp01(0.5 - sdRR(px + .5, py + .5, x, y, w, h, r));
        if (cov <= 0) continue;
        double t = clamp01((py + .5 - y) / h);
        blend(c, px, py, lerpCol(c1, c2, t), (a1 + (a2 - a1) * t) * cov);
    }
}

static void rrStroke(Canvas *c, double x, double y, double w, double h, double r, int col, double a, double width, int outside) {
    BBOX(x, y, w, h, width + 1)
    for (int py = y0; py < y1; py++) for (int px = x0; px < x1; px++) {
        double d = sdRR(px + .5, py + .5, x, y, w, h, r);
        double cov = outside ? clamp01(d + .5) * clamp01(width + .5 - d) : clamp01(.5 - d) * clamp01(d + width + .5);
        if (cov > 0) blend(c, px, py, col, a * cov);
    }
}

static void rrGloss(Canvas *c, double x, double y, double w, double h, double r, double frac, double aTop, double aBot) {
    double gh = h * frac;
    BBOX(x, y, w, gh, 1)
    for (int py = y0; py < y1; py++) for (int px = x0; px < x1; px++) {
        double cov = clamp01(0.5 - sdRR(px + .5, py + .5, x + 1, y + 1, w - 2, h - 2, r - 1));
        cov *= clamp01(y + gh - (py + .5) + .5);
        if (cov <= 0) continue;
        double t = clamp01((py + .5 - y) / gh);
        blend(c, px, py, 0xffffff, (aTop + (aBot - aTop) * t) * cov);
    }
}

static void rrGlow(Canvas *c, double x, double y, double w, double h, double r, double gx, double gy, double rad, int col, double a) {
    BBOX(x, y, w, h, 1)
    for (int py = y0; py < y1; py++) for (int px = x0; px < x1; px++) {
        double cov = clamp01(0.5 - sdRR(px + .5, py + .5, x, y, w, h, r));
        if (cov <= 0) continue;
        double f = clamp01(1 - hypot((px + .5 - gx) / rad, (py + .5 - gy) / (rad * 0.55)));
        if (f > 0) blend(c, px, py, col, a * cov * f * f);
    }
}

static void rrShadow(Canvas *c, double x, double y, double w, double h, double r, double blur, double a) {
    BBOX(x, y, w, h, blur)
    for (int py = y0; py < y1; py++) for (int px = x0; px < x1; px++) {
        double d = sdRR(px + .5, py + .5, x, y, w, h, r);
        if (d <= 0) continue;
        double f = clamp01(1 - d / blur);
        blend(c, px, py, 0x0a2a4a, a * f * f);
    }
}

static void disc(Canvas *c, double cx, double cy, double rad, int col, double a, double soft) {
    BBOX(cx - rad, cy - rad, rad * 2, rad * 2, soft + 1)
    for (int py = y0; py < y1; py++) for (int px = x0; px < x1; px++) {
        double d = hypot(px + .5 - cx, py + .5 - cy) - rad;
        double cov = clamp01(.5 - d / soft);
        if (cov > 0) blend(c, px, py, col, a * cov);
    }
}

static void ring(Canvas *c, double cx, double cy, double rad, int col, double a, double width) {
    BBOX(cx - rad, cy - rad, rad * 2, rad * 2, 2)
    for (int py = y0; py < y1; py++) for (int px = x0; px < x1; px++) {
        double d = hypot(px + .5 - cx, py + .5 - cy) - rad;
        double cov = clamp01(.5 - d) * clamp01(d + width + .5);
        if (cov > 0) blend(c, px, py, col, a * cov);
    }
}

static void segment(Canvas *c, double ax, double ay, double bx, double by, double th, int col, double a) {
    double mnx = fmin(ax, bx), mny = fmin(ay, by);
    BBOX(mnx, mny, fabs(bx - ax), fabs(by - ay), th + 1)
    double dx = bx - ax, dy = by - ay, L2 = dx * dx + dy * dy;
    for (int py = y0; py < y1; py++) for (int px = x0; px < x1; px++) {
        double qx = px + .5 - ax, qy = py + .5 - ay;
        double t = clamp01((qx * dx + qy * dy) / L2);
        double d = hypot(qx - dx * t, qy - dy * t);
        double cov = clamp01(th / 2 + .5 - d);
        if (cov > 0) blend(c, px, py, col, a * cov);
    }
}

static void bubble(Canvas *c, double cx, double cy, double r) {
    disc(c, cx, cy, r, 0xffffff, 0.10, 1);
    disc(c, cx + r * 0.25, cy + r * 0.3, r * 0.65, 0xbff4ff, 0.10, r * 0.5);
    ring(c, cx, cy, r, 0xffffff, 0.55, fmax(1.2, r * 0.05));
    disc(c, cx - r * 0.38, cy - r * 0.42, r * 0.26, 0xffffff, 0.75, r * 0.18);
    disc(c, cx + r * 0.42, cy + r * 0.48, r * 0.1, 0xffffff, 0.45, r * 0.08);
}
/* ---------- Vista glass frame ---------- */

#define FR 7
#define TT 30
static Canvas frameC, frameBack, snapC;
static int sizing, rescaling;
static void rescaleAll(void);
static int capHover = -1, capDown = -1;

static RECT capRect(int i) {
    int right = frameC.w - S(7), cw = S(43), mw = S(26);
    RECT r = { i ? right - cw : right - cw - mw, 1, i ? right : right - cw, 1 + S(19) };
    return r;
}
static void renderFrame(void) {
    Canvas *c = &frameC;
    int W = c->w, H = c->h, th = S(TT);
    double sx = W * 0.22, sy = -S(14);
    for (int y = 0; y < H; y++) {
        int base = y < th ? lerpCol(0xa8f0f2, 0x52c0cc, (double)y / th) : lerpCol(0x52c0cc, 0x1f8a9e, (double)(y - th) / (H - th));
        for (int x = 0; x < W; x++) {
            double s1 = sin((x + y * 1.7) / (double)S(38)) * 0.5 + 0.5;
            double s2 = sin((x * 0.8 + y * 1.7) / (double)S(11) + 1.3) * 0.5 + 0.5;
            double d = hypot((x - sx) / (W * 0.7), (y - sy) / (H * 0.25));
            double sheen = clamp01(1 - d); sheen *= sheen;
            double a = 0.13 * s1 * s1 * s1 + 0.05 * s2 + 0.30 * sheen;
            c->px[y * W + x] = (DWORD)lerpCol(base, 0xffffff, clamp01(a));
        }
    }
    for (int y = 0; y < th / 2; y++) for (int x = 0; x < W; x++)
        blend(c, x, y, 0xffffff, 0.22 * (1 - (double)y / (th / 2)));
    int cx = S(FR), cy = th, cw = W - 2 * S(FR), ch = H - th - S(FR);
    rrStroke(c, cx - 2, cy - 2, cw + 4, ch + 4, 2, 0xffffff, 0.45, 1, 0);
    rrStroke(c, cx - 1, cy - 1, cw + 2, ch + 2, 1, 0x0b3a44, 0.75, 1, 0);
}

static void capButton(Canvas *c, int i) {
    RECT r = capRect(i);
    int close = i == 1, hov = capHover == i, prs = capDown == i && hov;
    double rad = S(4), x = r.left, w = r.right - r.left, h = r.bottom - r.top;
    double y = r.top - rad, hh = h + rad;
    if (close) {
        int top = hov ? 0xffb9a3 : 0xe6a08c, bot = hov ? 0xe8452a : 0xc23a1f;
        if (prs) { top = lerpCol(top, 0, 0.2); bot = lerpCol(bot, 0, 0.2); }
        rrFill(c, x, y, w, hh, rad, top, bot, 1, 1);
        rrGlow(c, x, y, w, hh, rad, x + w / 2, y + hh * 1.08, w * 0.6, hov ? 0xffd27a : 0xff8a5a, hov ? 0.95 : 0.55);
    } else if (hov) {
        int top = prs ? 0x5fd4e0 : 0xa6f7ff, bot = prs ? 0x0d7f8e : 0x1fb0c4;
        rrFill(c, x, y, w, hh, rad, top, bot, 1, 1);
        rrGlow(c, x, y, w, hh, rad, x + w / 2, y + hh * 1.08, w * 0.75, 0x7ff6ff, prs ? 0.6 : 1.0);
    } else {
        rrFill(c, x, y, w, hh, rad, 0xd3f5f5, 0x8cd0d6, 0.6, 0.6);
    }
    rrGloss(c, x, r.top, w, h, rad, 0.5, prs ? 0.3 : 0.6, 0.12);
    rrStroke(c, x, y, w, hh, rad, 0x0b3036, 0.8, 1, 0);
    rrStroke(c, x + 1, y + 1, w - 2, hh - 2, rad - 1, 0xffffff, 0.45, 1, 0);
    double gx = x + w / 2, gy = r.top + h / 2;
    if (close) {
        double s = S(4);
        for (int pass = 0; pass < 2; pass++) {
            double t = pass ? S(2) : S(2) + 2.2; int col = pass ? 0xffffff : 0x5a1208; double a = pass ? 1 : 0.7;
            segment(c, gx - s, gy - s * 0.8, gx + s, gy + s * 0.8, t, col, a);
            segment(c, gx - s, gy + s * 0.8, gx + s, gy - s * 0.8, t, col, a);
        }
    } else {
        double s = S(4), yy = gy + S(3);
        segment(c, gx - s, yy, gx + s, yy, S(2) + 2.2, 0x0b3036, 0.7);
        segment(c, gx - s, yy, gx + s, yy, S(2), 0xffffff, 1);
    }
}

static void paintFrame(HDC hdc) {
    if (sizing && snapC.dc) {
        RECT r; GetClientRect(hFrame, &r);
        SetStretchBltMode(hdc, HALFTONE); SetBrushOrgEx(hdc, 0, 0, NULL);
        StretchBlt(hdc, 0, 0, r.right, r.bottom, snapC.dc, 0, 0, snapC.w, snapC.h, SRCCOPY);
        return;
    }
    Canvas *c = &frameBack;
    memcpy(c->px, frameC.px, (size_t)c->w * c->h * 4);
    capButton(c, 0); capButton(c, 1);
    rrStroke(c, 0, 0, c->w, c->h, S(8), 0x0b3036, 0.85, 1, 0);
    rrStroke(c, 1, 1, c->w - 2, c->h - 2, S(7), 0xffffff, 0.45, 1, 0);
    for (int i = 0; i < c->w * S(2); i++) c->px[i] |= 0xFF000000;
    BitBlt(hdc, 0, 0, c->w, c->h, c->dc, 0, 0, SRCCOPY);
}

static int capAt(int x, int y) {
    POINT p = { x, y };
    for (int i = 0; i < 2; i++) { RECT r = capRect(i); if (PtInRect(&r, p)) return i; }
    return -1;
}

static void capInvalidate(void) {
    RECT r = { capRect(0).left - 2, 0, frameC.w, capRect(0).bottom + 2 };
    InvalidateRect(hFrame, &r, FALSE);
}

/* ---------- net + json ---------- */

static int httpGet(const wchar_t *url, const wchar_t *hdrs, char **out, size_t *outLen, DWORD *httpSt, int progress) {
    *out = NULL; *outLen = 0; *httpSt = 0;
    HINTERNET in = InternetOpenW(L"FrutigerNano/" APP_VERSION_W, INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!in) return (int)GetLastError();
    DWORD to = 15000;
    InternetSetOptionW(in, INTERNET_OPTION_CONNECT_TIMEOUT, &to, sizeof to);
    InternetSetOptionW(in, INTERNET_OPTION_RECEIVE_TIMEOUT, &to, sizeof to);
    HINTERNET h = InternetOpenUrlW(in, url, hdrs, hdrs ? (DWORD)-1 : 0,
        INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_NO_COOKIES | INTERNET_FLAG_NO_UI, 0);
    if (!h) { int e = (int)GetLastError(); InternetCloseHandle(in); return e ? e : -1; }
    DWORD st = 0, sl = sizeof st, total = 0, tl = sizeof total;
    HttpQueryInfoW(h, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &st, &sl, NULL);
    HttpQueryInfoW(h, HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER, &total, &tl, NULL);
    *httpSt = st;
    size_t cap = 1 << 16, len = 0; char *b = malloc(cap + 1); DWORD got = 0; int err = 0, lastPct = -1;
    for (;;) {
        if (len + 65536 > cap) { cap *= 2; b = realloc(b, cap + 1); }
        if (!InternetReadFile(h, b + len, 65536, &got)) { err = (int)GetLastError(); if (!err) err = -1; break; }
        if (!got) break;
        len += got;
        if (progress && total) {
            int pct = (int)(len * 100 / total);
            if (pct != lastPct) { lastPct = pct; status(L"Downloading update… %d%%", pct); }
        }
        if (len > (64u << 20)) { err = -2; break; }
    }
    InternetCloseHandle(h); InternetCloseHandle(in);
    if (err) { free(b); return err; }
    b[len] = 0; *out = b; *outLen = len;
    return 0;
}

static int httpPost(const wchar_t *host, const wchar_t *path, const wchar_t *headers,
                    const char *body, DWORD blen, char **out, size_t *outLen, DWORD *httpSt) {
    *out = NULL; *outLen = 0; *httpSt = 0;
    HINTERNET in = InternetOpenW(L"FrutigerNano/" APP_VERSION_W, INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (!in) return (int)GetLastError();
    DWORD to = 120000;
    InternetSetOptionW(in, INTERNET_OPTION_CONNECT_TIMEOUT, &to, sizeof to);
    InternetSetOptionW(in, INTERNET_OPTION_SEND_TIMEOUT, &to, sizeof to);
    InternetSetOptionW(in, INTERNET_OPTION_RECEIVE_TIMEOUT, &to, sizeof to);
    HINTERNET con = InternetConnectW(in, host, INTERNET_DEFAULT_HTTPS_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
    if (!con) { int e = (int)GetLastError(); InternetCloseHandle(in); return e ? e : -1; }
    const wchar_t *accept[] = { L"application/json", NULL };
    HINTERNET req = HttpOpenRequestW(con, L"POST", path, NULL, NULL, accept,
        INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD | INTERNET_FLAG_NO_CACHE_WRITE | INTERNET_FLAG_NO_COOKIES | INTERNET_FLAG_NO_UI, 0);
    if (!req) { int e = (int)GetLastError(); InternetCloseHandle(con); InternetCloseHandle(in); return e ? e : -1; }
    BOOL ok = HttpSendRequestW(req, headers, (DWORD)-1, (void *)body, blen);
    if (!ok) { int e = (int)GetLastError(); InternetCloseHandle(req); InternetCloseHandle(con); InternetCloseHandle(in); return e ? e : -1; }
    DWORD st = 0, sl = sizeof st;
    HttpQueryInfoW(req, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &st, &sl, NULL);
    *httpSt = st;
    size_t cap = 1 << 16, len = 0; char *b = malloc(cap + 1); DWORD got = 0; int err = 0;
    for (;;) {
        if (len + 65536 > cap) { cap *= 2; b = realloc(b, cap + 1); }
        if (!InternetReadFile(req, b + len, 65536, &got)) { err = (int)GetLastError(); if (!err) err = -1; break; }
        if (!got) break;
        len += got;
        if (len > (16u << 20)) break;
    }
    InternetCloseHandle(req); InternetCloseHandle(con); InternetCloseHandle(in);
    if (err) { free(b); return err; }
    b[len] = 0; *out = b; *outLen = len;
    return 0;
}

static void putU8(char *o, size_t *n, unsigned u) {
    if (u < 0x80) o[(*n)++] = (char)u;
    else if (u < 0x800) { o[(*n)++] = (char)(0xC0 | (u >> 6)); o[(*n)++] = (char)(0x80 | (u & 63)); }
    else if (u < 0x10000) { o[(*n)++] = (char)(0xE0 | (u >> 12)); o[(*n)++] = (char)(0x80 | ((u >> 6) & 63)); o[(*n)++] = (char)(0x80 | (u & 63)); }
    else { o[(*n)++] = (char)(0xF0 | (u >> 18)); o[(*n)++] = (char)(0x80 | ((u >> 12) & 63)); o[(*n)++] = (char)(0x80 | ((u >> 6) & 63)); o[(*n)++] = (char)(0x80 | (u & 63)); }
}

static char *jsonStr(const char *json, const char *key, const char *from, const char *until) {
    char pat[64]; snprintf(pat, sizeof pat, "\"%s\"", key);
    const char *p = strstr(from ? from : json, pat);
    if (!p || (until && p >= until)) return NULL;
    p += strlen(pat);
    while (*p == ' ' || *p == ':' || *p == '\t' || *p == '\n' || *p == '\r') p++;
    if (*p != '"') return NULL;
    p++;
    size_t cap = 256, n = 0; char *o = malloc(cap);
    while (*p && *p != '"') {
        if (n + 8 > cap) { cap *= 2; o = realloc(o, cap); }
        if (*p == '\\' && p[1]) {
            p++;
            switch (*p) {
            case 'n': o[n++] = '\n'; break; case 'r': break; case 't': o[n++] = '\t'; break;
            case 'u': {
                unsigned u = 0; sscanf(p + 1, "%4x", &u); p += 4;
                if (u >= 0xD800 && u < 0xDC00 && p[1] == '\\' && p[2] == 'u') {
                    unsigned lo = 0; sscanf(p + 3, "%4x", &lo);
                    if (lo >= 0xDC00 && lo < 0xE000) { u = 0x10000 + ((u - 0xD800) << 10) + (lo - 0xDC00); p += 6; }
                }
                putU8(o, &n, u);
            } break;
            default: o[n++] = *p;
            }
            p++;
        } else o[n++] = *p++;
    }
    o[n] = 0;
    return o;
}

static char *jsonEsc(const wchar_t *w) {
    int n8 = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
    char *s = malloc(n8); WideCharToMultiByte(CP_UTF8, 0, w, -1, s, n8, NULL, NULL);
    size_t cap = n8 * 2 + 8, k = 0; char *o = malloc(cap);
    for (char *p = s; *p; p++) {
        unsigned char c = *p;
        if (k + 8 > cap) { cap *= 2; o = realloc(o, cap); }
        if (c == '"' || c == '\\') { o[k++] = '\\'; o[k++] = c; }
        else if (c == '\n') { o[k++] = '\\'; o[k++] = 'n'; }
        else if (c == '\r') { o[k++] = '\\'; o[k++] = 'r'; }
        else if (c == '\t') { o[k++] = '\\'; o[k++] = 't'; }
        else if (c < 0x20) { k += snprintf(o + k, 8, "\\u%04x", c); }
        else o[k++] = c;
    }
    o[k] = 0; free(s);
    return o;
}

static wchar_t *u8dup(const char *s) {
    if (!s) return wdup(L"");
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    wchar_t *w = malloc(n * sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n);
    return w;
}
static void u8w(const char *s, wchar_t *out, int n) { MultiByteToWideChar(CP_UTF8, 0, s ? s : "", -1, out, n); out[n - 1] = 0; }

/* ---------- updater ---------- */

enum { CONN_UNKNOWN, CONN_CHECKING, CONN_ONLINE, CONN_OFFLINE, CONN_ERROR };
static int updConn = CONN_UNKNOWN, updAvail, justUpdated;
static volatile LONG updBusy;
static wchar_t updRepo[128] = L"binjin-maker/FrutigerNano";
static wchar_t updLatest[32], updUrl[1024], updPage[512], updDigest[80], updConnMsg[160];
static wchar_t *updNotes;
static int updErr;
static SYSTEMTIME updWhen; static int updChecked;
static void saveSettings(void);

static void verParse(const wchar_t *s, int v[3]) {
    v[0] = v[1] = v[2] = 0;
    while (*s && !iswdigit(*s)) s++;
    swscanf(s, L"%d.%d.%d", &v[0], &v[1], &v[2]);
}
static int verNewer(const wchar_t *a, const wchar_t *b) {
    int x[3], y[3]; verParse(a, x); verParse(b, y);
    for (int i = 0; i < 3; i++) if (x[i] != y[i]) return x[i] > y[i];
    return 0;
}

static wchar_t *cleanNotes(const char *md) {
    int n = MultiByteToWideChar(CP_UTF8, 0, md, -1, NULL, 0);
    wchar_t *w = malloc(n * sizeof(wchar_t)), *o = malloc((n + 16) * sizeof(wchar_t)), *q = o;
    MultiByteToWideChar(CP_UTF8, 0, md, -1, w, n);
    int bol = 1;
    for (wchar_t *p = w; *p; p++) {
        if (bol) {
            while (*p == L'#' || *p == L'>') p++;
            while (*p == L' ') p++;
            if ((*p == L'-' || *p == L'*') && p[1] == L' ') { *q++ = L'•'; *q++ = L' '; p += 2; }
            if (!*p) break;
        }
        if (*p == L'*' || *p == L'`' || (*p == L'_' && p[1] == L'_')) { bol = 0; continue; }
        *q++ = *p;
        bol = *p == L'\n';
    }
    *q = 0; free(w);
    return o;
}

static DWORD WINAPI CheckThread(LPVOID unused) {
    (void)unused;
    wchar_t url[300];
    swprintf(url, 300, L"https://raw.githubusercontent.com/%ls/main/update/latest.json?t=%llu", updRepo, (unsigned long long)GetTickCount64());
    char *b; size_t len; DWORD st;
    int e = httpGet(url, L"Cache-Control: no-cache\r\n", &b, &len, &st, 0);
    int conn = CONN_ONLINE, avail = 0;
    wchar_t latest[32] = L"", dl[1024] = L"", page[512] = L"", dig[80] = L"", msg[160] = L"";
    wchar_t *notes = NULL;
    swprintf(page, 512, L"https://github.com/%ls", updRepo);
    if (e) {
        conn = (e == ERROR_INTERNET_NAME_NOT_RESOLVED || e == ERROR_INTERNET_CANNOT_CONNECT ||
                e == ERROR_INTERNET_TIMEOUT || e == ERROR_INTERNET_DISCONNECTED) ? CONN_OFFLINE : CONN_ERROR;
        if (conn == CONN_OFFLINE) wcscpy(msg, L"Offline — no internet (the offline engine still works)");
        else swprintf(msg, 160, L"Couldn't reach GitHub (network error %d) — maybe a school/work firewall", e);
    } else if (st == 404) {
        wcscpy(msg, L"Online — but no update has been published to the repo yet");
    } else if (st == 403 || st == 429) {
        conn = CONN_ERROR;
        wcscpy(msg, L"Online — GitHub rate-limited the check, try again in a bit");
    } else if (st != 200) {
        conn = CONN_ERROR;
        swprintf(msg, 160, L"Online — update server answered HTTP %lu", (unsigned long)st);
    } else {
        wcscpy(msg, L"Online — connected to GitHub");
        char *ver = jsonStr(b, "version", NULL, NULL), *u8 = jsonStr(b, "url", NULL, NULL);
        char *sha = jsonStr(b, "sha256", NULL, NULL), *nt = jsonStr(b, "notes", NULL, NULL);
        if (ver) { latest[0] = L'v'; u8w(ver, latest + 1, 31); }
        u8w(u8, dl, 1024); u8w(sha, dig, 80);
        notes = cleanNotes(nt ? nt : "");
        if (latest[0] && verNewer(latest, APP_VERSION_W)) avail = dl[0] ? 1 : 2;
        free(ver); free(u8); free(sha); free(nt);
    }
    free(b);
    wcscpy(updLatest, latest); wcscpy(updUrl, dl); wcscpy(updPage, page); wcscpy(updDigest, dig); wcscpy(updConnMsg, msg);
    free(updNotes); updNotes = notes;
    updConn = conn; updAvail = avail;
    GetLocalTime(&updWhen); updChecked = 1;
    InterlockedExchange(&updBusy, 0);
    PostMessageW(hMain, WM_UPDCHECKED, 0, 0);
    return 0;
}

static void fillNotes(void) {
    const wchar_t *src = (updNotes && updNotes[0]) ? updNotes :
        updConn == CONN_OFFLINE ? L"Changelog unavailable offline." :
        updConn == CONN_CHECKING ? L"Checking for updates…" :
        updLatest[0] ? L"No release notes for this version." : L"No changelog yet — press Check now.";
    size_t n = wcslen(src);
    wchar_t *o = malloc((n * 2 + 1) * sizeof(wchar_t)), *q = o;
    for (const wchar_t *p = src; *p; p++) { if (*p == L'\n') *q++ = L'\r'; *q++ = *p; }
    *q = 0;
    SetDlgItemTextW(hMain, ID_UNOTES, o);
    free(o);
}

static void startCheck(void) {
    if (InterlockedCompareExchange(&updBusy, 1, 0)) return;
    updConn = CONN_CHECKING; updErr = 0;
    wcscpy(updConnMsg, L"Checking…");
    fillNotes();
    InvalidateRect(hMain, NULL, FALSE);
    CloseHandle(CreateThread(NULL, 0, CheckThread, NULL, 0, NULL));
}

static int sha256Hex(const char *data, size_t len, wchar_t *out) {
    BCRYPT_ALG_HANDLE alg; UCHAR h[32];
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, NULL, 0)) return 0;
    BCRYPT_HASH_HANDLE hh;
    int ok = !BCryptCreateHash(alg, &hh, NULL, 0, NULL, 0, 0) &&
             !BCryptHashData(hh, (PUCHAR)data, (ULONG)len, 0) && !BCryptFinishHash(hh, h, 32, 0);
    BCryptDestroyHash(hh); BCryptCloseAlgorithmProvider(alg, 0);
    for (int i = 0; ok && i < 32; i++) swprintf(out + i * 2, 3, L"%02x", h[i]);
    return ok;
}

static void updFail(const wchar_t *m) {
    wchar_t *c = wdup(m);
    InterlockedExchange(&updBusy, 0);
    PostMessageW(hMain, WM_UPDFAIL, 0, (LPARAM)c);
}

static DWORD WINAPI InstallThread(LPVOID unused) {
    (void)unused;
    status(L"Downloading update %ls…", updLatest);
    char *b; size_t len; DWORD st;
    int e = httpGet(updUrl, NULL, &b, &len, &st, 1);
    if (e || st != 200) { free(b); updFail(L"Update download failed — check your connection and try again."); return 0; }
    if (len < 20000 || b[0] != 'M' || b[1] != 'Z') { free(b); updFail(L"The downloaded file isn't a valid EXE, so nothing was changed."); return 0; }
    if (updDigest[0]) {
        wchar_t hex[80] = L"";
        if (!sha256Hex(b, len, hex) || _wcsicmp(hex, updDigest)) { free(b); updFail(L"Update failed its checksum check, so nothing was changed."); return 0; }
    }
    wchar_t newPath[MAX_PATH + 16], oldPath[MAX_PATH + 16];
    swprintf(newPath, MAX_PATH + 16, L"%ls.new", exePath);
    HANDLE f = CreateFileW(newPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD wr = 0;
    int ok = f != INVALID_HANDLE_VALUE && WriteFile(f, b, (DWORD)len, &wr, NULL) && wr == len;
    if (f != INVALID_HANDLE_VALUE) CloseHandle(f);
    free(b);
    if (!ok) { DeleteFileW(newPath); updFail(L"Couldn't write the update next to the EXE — the folder may be write-protected."); return 0; }
    int chosen = 0;
    for (int slot = 0; slot < 64 && !chosen; slot++) {
        if (slot == 0) swprintf(oldPath, MAX_PATH + 16, L"%ls.old", exePath);
        else swprintf(oldPath, MAX_PATH + 16, L"%ls.old%d", exePath, slot);
        if (GetFileAttributesW(oldPath) == INVALID_FILE_ATTRIBUTES || DeleteFileW(oldPath)) chosen = 1;
    }
    if (!chosen) { DeleteFileW(newPath); updFail(L"Couldn't clear an old backup next to the EXE."); return 0; }
    int moved = 0;
    for (int i = 0; i < 12 && !moved; i++) { if (MoveFileExW(exePath, oldPath, MOVEFILE_REPLACE_EXISTING)) moved = 1; else Sleep(120); }
    if (!moved) { DeleteFileW(newPath); updFail(L"Couldn't move the running EXE aside — close other copies and retry."); return 0; }
    int placed = 0;
    for (int i = 0; i < 12 && !placed; i++) { if (MoveFileExW(newPath, exePath, MOVEFILE_REPLACE_EXISTING)) placed = 1; else Sleep(120); }
    if (!placed) { MoveFileExW(oldPath, exePath, MOVEFILE_REPLACE_EXISTING); DeleteFileW(newPath); updFail(L"Couldn't put the new EXE in place; kept the current version."); return 0; }
    MoveFileExW(oldPath, NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
    DeleteFileW(oldPath);
    PostMessageW(hMain, WM_UPDRESTART, 0, 0);
    return 0;
}

static void startInstall(void) {
    if (updAvail != 1) { status(updAvail == 2 ? L"The new release has no .exe attached, so it can't auto-install." : L"No update to install."); return; }
    if (nanoBusy) { status(L"Wait for the current rewrite to finish before updating."); return; }
    if (InterlockedCompareExchange(&updBusy, 1, 0)) return;
    CloseHandle(CreateThread(NULL, 0, InstallThread, NULL, 0, NULL));
}

static void restartUpdated(void) {
    saveSettings();
    wchar_t cmd[MAX_PATH * 2];
    swprintf(cmd, MAX_PATH * 2, L"\"%ls\" --updated", exePath);
    STARTUPINFOW si = { sizeof si }; PROCESS_INFORMATION pi;
    UnregisterHotKey(hMain, HK_CLIP);
    if (CreateProcessW(exePath, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
        DestroyWindow(hFrame);
    } else {
        RegisterHotKey(hMain, HK_CLIP, MOD_CONTROL | MOD_ALT | 0x4000, 'N');
        status(L"Update installed — close and reopen Frutiger Nano to finish.");
    }
}

static void updInfo(wchar_t *out, int n) {
    wchar_t when[64] = L"never";
    if (updChecked) { wchar_t t[32]; GetTimeFormatEx(NULL, TIME_NOSECONDS, &updWhen, NULL, t, 32); swprintf(when, 64, L"%ls", t); }
    const wchar_t *lat = updLatest[0] ? updLatest : (updConn == CONN_CHECKING ? L"checking…" : L"unknown");
    const wchar_t *note = updAvail == 1 ? L"  — update available!" : updAvail == 2 ? L"  — newer, but no .exe attached" :
                          (updLatest[0] ? L"  — you're up to date" : L"");
    swprintf(out, n,
        L"Connection:  %ls\n"
        L"Installed:  v%ls        Latest:  %ls%ls\n"
        L"Source:  github.com/%ls   ·   last checked %ls",
        updConnMsg[0] ? updConnMsg : L"Not checked yet", APP_VERSION_W, lat, note, updRepo, when);
}

static int connColor(void) {
    if (updErr || updConn == CONN_OFFLINE || updConn == CONN_ERROR) return 0xe0483a;
    if (updConn == CONN_ONLINE) return updAvail ? 0xf5c21a : 0x34c23a;
    return 0x9aa8b8;
}

/* ---------- activation ---------- */

static const char KEYALPH[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";

static int sha256Raw(const char *data, size_t len, unsigned char *out) {
    BCRYPT_ALG_HANDLE alg; BCRYPT_HASH_HANDLE hh;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, NULL, 0)) return 0;
    int ok = !BCryptCreateHash(alg, &hh, NULL, 0, NULL, 0, 0) &&
             !BCryptHashData(hh, (PUCHAR)data, (ULONG)len, 0) && !BCryptFinishHash(hh, out, 32, 0);
    BCryptDestroyHash(hh); BCryptCloseAlgorithmProvider(alg, 0);
    return ok;
}

/* NANO-XXXX-XXXX-CCCC: CCCC = first 4 chars of sha256(secret + XXXXXXXX) in the key alphabet */
static int keyValid(const wchar_t *in, wchar_t *norm) {
    char k[32]; int n = 0;
    for (const wchar_t *p = in; *p && n < 31; p++) if (iswalnum(*p)) k[n++] = (char)towupper(*p);
    k[n] = 0;
    if (n != 16 || strncmp(k, "NANO", 4)) return 0;
    for (int i = 4; i < 16; i++) if (!strchr(KEYALPH, k[i])) return 0;
    char buf[96]; int bl = snprintf(buf, sizeof buf, "%s%.8s", NANO_SECRET, k + 4);
    unsigned char h[32];
    if (!sha256Raw(buf, bl, h)) return 0;
    for (int i = 0; i < 4; i++) if (k[12 + i] != KEYALPH[h[i] & 31]) return 0;
    if (norm) {
        int q = 0;
        for (int i = 0; i < 16; i++) { if (i && !(i % 4)) norm[q++] = L'-'; norm[q++] = (wchar_t)k[i]; }
        norm[q] = 0;
    }
    return 1;
}

/* ---------- pages, layout, painting ---------- */

enum { PG_REWRITE, PG_CLAUDE, PG_UPDATE, PG_LOCK, PG_N };
#define NTABS 3
static const wchar_t *TABNAME[NTABS] = { L"Rewrite", L"Claude", L"Update" };
static int curPage = PG_REWRITE, tabHover = -1;

static Canvas pageC[PG_N], backC;
static HFONT hFont, hBold, hTitle, hSmall, hBtnSmall, hEdit, hBrand, hBig, hHuge, hTiny;
static HWND hoverBtn;
static wchar_t statusText[512] = L"";

#define NAVY RGB(10, 52, 70)
#define HINT RGB(40, 112, 130)

static RECT RS(int x, int y, int w, int h) { RECT r = { S(x), S(y), S(x + w), S(y + h) }; return r; }

static void glassPanel(Canvas *c, int lx, int ly, int lw, int lh) {
    double x = S(lx), y = S(ly), w = S(lw), h = S(lh), r = S(14);
    rrShadow(c, x, y + S(4), w, h, r, S(12), 0.30);
    rrFill(c, x, y, w, h, r, 0xffffff, 0xdcfaff, 0.80, 0.60);
    rrGloss(c, x, y, w, h, r, 0.28, 0.60, 0.06);
    rrStroke(c, x, y, w, h, r, 0x0b4f5e, 0.45, 1, 1);
    rrStroke(c, x, y, w, h, r, 0xffffff, 0.95, 1.3, 0);
}

static void darkGlass(Canvas *c, int lx, int ly, int lw, int lh) {
    double x = S(lx), y = S(ly), w = S(lw), h = S(lh), r = S(16);
    rrShadow(c, x, y + S(4), w, h, r, S(12), 0.35);
    rrFill(c, x, y, w, h, r, 0x0d5f78, 0x063a4f, 0.62, 0.72);
    rrGloss(c, x, y, w, h, r, 0.22, 0.30, 0.04);
    rrStroke(c, x, y, w, h, r, 0x041e28, 0.6, 1, 1);
    rrStroke(c, x, y, w, h, r, 0x9ff3ff, 0.55, 1.2, 0);
}

static void inset(Canvas *c, int lx, int ly, int lw, int lh) {
    rrFill(c, S(lx), S(ly), S(lw), S(lh), S(9), 0xffffff, 0xeefdff, 0.75, 0.55);
    rrStroke(c, S(lx), S(ly), S(lw), S(lh), S(9), 0x1a8fa3, 0.35, 1, 0);
}

static void textAt(HDC dc, HFONT f, const wchar_t *t, int lx, int ly, int lw, int lh, COLORREF col, UINT fmt) {
    RECT r = RS(lx, ly, lw, lh);
    SelectObject(dc, f);
    SetTextColor(dc, col);
    DrawTextW(dc, t, -1, &r, fmt);
}

static void orb(Canvas *c, double cx, double cy, double r) {
    disc(c, cx, cy + r * 0.12, r * 1.05, 0x03202c, 0.35, r * 0.25);
    rrFill(c, cx - r, cy - r, 2 * r, 2 * r, r, 0x8ff2ff, 0x149c7c, 1, 1);
    rrGlow(c, cx - r, cy - r, 2 * r, 2 * r, r, cx, cy + r * 0.95, r * 1.1, 0xb0ffe0, 0.9);
    disc(c, cx, cy - r * 0.42, r * 0.66, 0xffffff, 0.45, r * 0.25);
    ring(c, cx, cy, r, 0x0a5a50, 0.8, fmax(1, r * 0.05));
}

static void orbLetter(HDC dc, double cx, double cy, double r) {
    RECT rr = { (int)(cx - r), (int)(cy - r), (int)(cx + r), (int)(cy + r) };
    SelectObject(dc, r > S(20) ? hHuge : hBig);
    SetBkMode(dc, TRANSPARENT);
    OffsetRect(&rr, 1, 2); SetTextColor(dc, RGB(8, 70, 60));
    DrawTextW(dc, L"N", 1, &rr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    OffsetRect(&rr, -1, -2); SetTextColor(dc, RGB(255, 255, 255));
    DrawTextW(dc, L"N", 1, &rr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

enum { TG_OFF, TG_CLAUDE, TG_LIGHT, TG_MED, TG_HEAVY, TG_STRICT, TG_HILITE, TG_UCHECK, TG_UAUTO, TG_N };
typedef struct { const wchar_t *t; int page, x, y, w, radio, on; } Tog;
static Tog tog[TG_N] = {
    { L"Offline",                PG_REWRITE, 236,  19,  74, 1, 1 },
    { L"Claude",                 PG_REWRITE, 312,  19,  74, 1, 0 },
    { L"Light",                  PG_REWRITE, 410,  19,  62, 2, 0 },
    { L"Medium",                 PG_REWRITE, 474,  19,  78, 2, 1 },
    { L"Heavy",                  PG_REWRITE, 554,  19,  70, 2, 0 },
    { L"Lock fact sentences",    PG_REWRITE, 108, 397, 168, 0, 0 },
    { L"Highlight locked parts", PG_REWRITE, 282, 397, 186, 0, 1 },
    { L"Check on launch",        PG_UPDATE,  108, 358, 180, 0, 1 },
    { L"Auto-install updates",   PG_UPDATE,  300, 358, 200, 0, 0 },
};

static int strength(void) { return tog[TG_LIGHT].on ? 0 : tog[TG_HEAVY].on ? 2 : 1; }

static void renderOcean(Canvas *c) {
    int W = c->w, H = c->h;
    double sx = W * 0.32, sy = -H * 0.45;
    for (int y = 0; y < H; y++) {
        double t = (double)y / H;
        int col = t < 0.5 ? lerpCol(0x46dbe8, 0x0f8db4, t / 0.5) : lerpCol(0x0f8db4, 0x053f66, (t - 0.5) / 0.5);
        double fade = (1 - t) * (1 - t);
        for (int x = 0; x < W; x++) {
            double ang = atan2(x - sx, y - sy);
            double sh = sin(ang * 23 + 1.3) * 0.5 + 0.5; sh = pow(sh, 7) * 0.22 * (1 - t * 0.9);
            double cv = sin(x / (double)S(21) + sin(y / (double)S(29)) * 2.1) + sin((x * 0.73 - y) / (double)S(17) + 0.7);
            double ca = fabs(cv) < 0.12 ? (0.12 - fabs(cv)) / 0.12 * 0.20 * fade : 0;
            c->px[y * W + x] = (DWORD)lerpCol(col, 0xeaffff, clamp01(sh + ca));
        }
    }
    static const float WEED[][3] = { { 0.03f, 0.30f, 1.0f }, { 0.07f, 0.22f, 2.2f }, { 0.94f, 0.28f, 0.6f }, { 0.975f, 0.20f, 3.1f }, { 0.55f, 0.12f, 1.7f } };
    for (unsigned k = 0; k < sizeof WEED / sizeof WEED[0]; k++) {
        double bx = WEED[k][0] * W, len = WEED[k][1] * H, ph = WEED[k][2];
        double px0 = bx, py0 = H;
        for (int s = 1; s <= 24; s++) {
            double f = s / 24.0, yy = H - len * f, xx = bx + sin(f * 5 + ph) * S(9) * f;
            segment(c, px0, py0, xx, yy, S(5) * (1.1 - f * 0.7), lerpCol(0x0c6b4a, 0x37c79a, f), 0.75);
            px0 = xx; py0 = yy;
        }
    }
    for (int pass = 0; pass < 2; pass++) {
        double base = pass ? H * 0.95 : H * 0.90, amp = pass ? S(8) : S(14);
        int top = pass ? 0x5fc2b0 : 0x9fe0cf, bot = pass ? 0x1f7a7c : 0x3f9f98;
        for (int x = 0; x < W; x++) {
            double cy = base + amp * sin(x / (double)W * (pass ? 5.1 : 3.0) + (pass ? 2.4 : 0.9));
            for (int y = (int)cy - 1; y < H; y++) {
                if (y < 0) continue;
                blend(c, x, y, lerpCol(top, bot, clamp01((y - cy) / (H - cy + 1))), clamp01(y + 1 - cy) * 0.92);
                double crest = (y - cy) / S(3);
                if (crest >= 0 && crest < 1) blend(c, x, y, 0xffffff, 0.35 * (1 - crest));
            }
        }
    }
    static const float BUB[][3] = {
        { 0.125f, 0.80f, 9 }, { 0.13f, 0.66f, 6 }, { 0.12f, 0.55f, 4 }, { 0.585f, 0.70f, 7 }, { 0.59f, 0.58f, 5 },
        { 0.595f, 0.47f, 3 }, { 0.99f, 0.62f, 14 }, { 0.985f, 0.44f, 8 }, { 0.99f, 0.33f, 5 }, { 0.03f, 0.12f, 12 },
        { 0.995f, 0.05f, 18 }, { 0.53f, 0.985f, 10 }, { 0.82f, 0.99f, 7 },
    };
    for (unsigned i = 0; i < sizeof BUB / sizeof BUB[0]; i++) bubble(c, BUB[i][0] * W, BUB[i][1] * H, S((int)BUB[i][2]));
    for (int i = 0; i < 40; i++) {
        double px = fmod(i * 197.31, 1.0 * W), py = fmod(i * 83.77 + i * i * 3.1, 1.0 * H * 0.85);
        disc(c, px, py, S(1) + (i % 3) * 0.5, 0xffffff, 0.25 + (i % 4) * 0.08, 1);
    }
}

static const wchar_t *CLAUDE_INFO =
    L"How the Claude engine works\n"
    L"• Your text goes to Anthropic's API with your own key. Every rewrite is billed to your API credits, even if you pay for a Claude subscription.\n"
    L"• Quotes, (parentheses), [brackets], links and code are swapped for placeholders before sending and put back exactly, so Claude can't touch them.\n"
    L"• Names, numbers and dates are kept by instruction, and Nano tells you if any went missing.\n"
    L"• Strength and “Lock fact sentences” on the Rewrite page apply here too.  Keys: console.anthropic.com → API Keys.";

static void renderPages(void) {
    renderOcean(&pageC[0]);
    for (int p = 1; p < PG_N; p++) memcpy(pageC[p].px, pageC[0].px, (size_t)pageC[0].w * pageC[0].h * 4);
    for (int p = 0; p < PG_N; p++) {
        Canvas *c = &pageC[p];
        HDC dc = c->dc;
        SetBkMode(dc, TRANSPARENT);
        darkGlass(c, 10, 10, 76, 452);
        orb(c, S(48), S(40), S(21));
        orbLetter(dc, S(48), S(40), S(21));
        textAt(dc, hTiny, L"NANO", 10, 64, 76, 14, RGB(200, 250, 255), DT_CENTER | DT_SINGLELINE);
        textAt(dc, hTiny, L"v" APP_VERSION_W, 10, 440, 76, 14, RGB(150, 220, 235), DT_CENTER | DT_SINGLELINE);
        glassPanel(c, 96, 432, 654, 30);
        if (p == PG_REWRITE) {
            glassPanel(c, 96, 10, 654, 40);
            textAt(dc, hTitle, L"Rewrite", 108, 19, 110, 22, NAVY, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
            rrFill(c, S(397), S(22), 1.2, S(18), 0, 0x1a8fa3, 0x1a8fa3, 0.5, 0.5);
            glassPanel(c, 96, 58, 300, 324);
            glassPanel(c, 450, 58, 300, 324);
            textAt(dc, hTitle, L"Original", 108, 62, 200, 20, NAVY, DT_LEFT | DT_SINGLELINE);
            textAt(dc, hSmall, L"paste here", 250, 64, 136, 16, HINT, DT_RIGHT | DT_SINGLELINE);
            textAt(dc, hTitle, L"Rewritten", 462, 62, 200, 20, NAVY, DT_LEFT | DT_SINGLELINE);
            textAt(dc, hSmall, L"same meaning, new words", 590, 64, 150, 16, HINT, DT_RIGHT | DT_SINGLELINE);
            textAt(dc, hSmall, L"Ctrl+Enter", 392, 256, 62, 14, RGB(220, 250, 255), DT_CENTER | DT_SINGLELINE);
            glassPanel(c, 96, 390, 654, 36);
        }
        if (p == PG_CLAUDE) {
            glassPanel(c, 96, 10, 654, 414);
            textAt(dc, hTitle, L"Claude engine", 108, 20, 300, 20, NAVY, DT_LEFT | DT_SINGLELINE);
            textAt(dc, hSmall, L"every rewrite is paid, even with a subscription", 420, 22, 318, 16, RGB(198, 112, 20), DT_RIGHT | DT_SINGLELINE);
            textAt(dc, hFont, L"API key:", 108, 56, 64, 18, NAVY, DT_LEFT | DT_SINGLELINE);
            textAt(dc, hFont, L"Model:", 108, 82, 64, 18, NAVY, DT_LEFT | DT_SINGLELINE);
            textAt(dc, hFont, L"Extra instructions (optional), e.g. “keep it casual” or “sound like a 10th grader”:", 108, 110, 630, 18, NAVY, DT_LEFT | DT_SINGLELINE);
            inset(c, 106, 256, 634, 158);
            textAt(dc, hFont, CLAUDE_INFO, 118, 264, 612, 146, NAVY, DT_LEFT | DT_WORDBREAK);
        }
        if (p == PG_UPDATE) {
            glassPanel(c, 96, 10, 654, 414);
            textAt(dc, hTitle, L"Updates", 108, 20, 300, 20, NAVY, DT_LEFT | DT_SINGLELINE);
            textAt(dc, hSmall, L"from GitHub · works offline", 420, 22, 318, 16, HINT, DT_RIGHT | DT_SINGLELINE);
            inset(c, 106, 46, 634, 70);
            inset(c, 106, 124, 634, 226);
        }
        if (p == PG_LOCK) {
            glassPanel(c, 190, 54, 470, 340);
            orb(c, S(425), S(112), S(34));
            orbLetter(dc, S(425), S(112), S(34));
            textAt(dc, hBrand, L"Activate Frutiger Nano", 190, 158, 470, 26, NAVY, DT_CENTER | DT_SINGLELINE);
            textAt(dc, hFont, L"Enter your activation key to unlock the app.\nYou only have to do this once on this PC.", 210, 188, 430, 40, HINT, DT_CENTER | DT_WORDBREAK);
            textAt(dc, hSmall, L"keys look like NANO-XXXX-XXXX-XXXX", 190, 362, 470, 16, HINT, DT_CENTER | DT_SINGLELINE);
        }
        for (int i = 0; i < TG_N; i++)
            if (tog[i].page == p) textAt(dc, hFont, tog[i].t, tog[i].x + 22, tog[i].y + 3, tog[i].w - 22, 18, NAVY, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
}

static void arrowLine(Canvas *c, double ax, double ay, double bx, double by, double th, int col, double a, double head) {
    segment(c, ax, ay, bx, by, th, col, a);
    double ang = atan2(by - ay, bx - ax);
    segment(c, bx, by, bx - head * cos(ang - 0.6), by - head * sin(ang - 0.6), th, col, a);
    segment(c, bx, by, bx - head * cos(ang + 0.6), by - head * sin(ang + 0.6), th, col, a);
}

/* 0 swap arrows, 1 sparkle, 2 refresh, 3 big arrow, 4 dots */
static void drawIcon(Canvas *c, int kind, double cx, double cy, double s, int col, double a) {
    double th = fmax(1.6, s * 0.13);
    if (kind == 0) {
        arrowLine(c, cx - s * 0.45, cy - s * 0.2, cx + s * 0.45, cy - s * 0.2, th, col, a, s * 0.28);
        arrowLine(c, cx + s * 0.45, cy + s * 0.25, cx - s * 0.45, cy + s * 0.25, th, col, a, s * 0.28);
    } else if (kind == 1) {
        for (int k = 0; k < 4; k++) {
            double an = k * 1.5707963;
            for (int q = 0; q < 6; q++) {
                double f = q / 6.0;
                segment(c, cx + cos(an) * s * 0.5 * f, cy + sin(an) * s * 0.5 * f, cx + cos(an) * s * 0.5 * (f + 0.17), cy + sin(an) * s * 0.5 * (f + 0.17), th * 1.6 * (1 - f) + 1, col, a);
            }
        }
        disc(c, cx + s * 0.38, cy - s * 0.38, s * 0.08, col, a, 1);
    } else if (kind == 2) {
        double r = s * 0.38, px = cx + r * cos(-0.6), py = cy + r * sin(-0.6);
        for (int q = 1; q <= 20; q++) {
            double an = -0.6 - q * (4.9 / 20), nx = cx + r * cos(an), ny = cy + r * sin(an);
            segment(c, px, py, nx, ny, th, col, a); px = nx; py = ny;
        }
        double an = -0.6 - 4.9, tx = -sin(an), ty = cos(an);
        segment(c, px, py, px + tx * s * 0.2 + cos(an) * s * 0.14, py + ty * s * 0.2 + sin(an) * s * 0.14, th, col, a);
        segment(c, px, py, px + tx * s * 0.2 - cos(an) * s * 0.14, py + ty * s * 0.2 - sin(an) * s * 0.14, th, col, a);
    } else if (kind == 3) {
        arrowLine(c, cx - s * 0.32, cy, cx + s * 0.3, cy, th * 1.5, col, a, s * 0.32);
    } else {
        for (int k = -1; k <= 1; k++) disc(c, cx + k * s * 0.3, cy, s * 0.09, col, a, 1);
    }
}

typedef struct { int top, bot, glow, border; } BtnPal;
static const BtnPal PAL_TEAL  = { 0x9ff0f0, 0x16a2b6, 0x9ffcff, 0x0b5a66 };
static const BtnPal PAL_GREEN = { 0xa6ec7c, 0x2f9f26, 0xb4ff7a, 0x1a6514 };
static const BtnPal PAL_RED   = { 0xffa48e, 0xcf2d22, 0xffb08a, 0x821812 };

static void gel(Canvas *c, double x, double y, double bw, double bh, double r, const BtnPal *p, int hov, int pressed) {
    int top = p->top, bot = p->bot;
    if (hov && !pressed) { top = lerpCol(top, 0xffffff, 0.18); bot = lerpCol(bot, 0xffffff, 0.12); }
    if (pressed) { top = lerpCol(top, 0x000000, 0.18); bot = lerpCol(bot, 0x000000, 0.12); }
    rrShadow(c, x, y + 1.5, bw, bh, r, S(3), 0.30);
    rrFill(c, x, y, bw, bh, r, top, bot, 1, 1);
    rrGlow(c, x, y, bw, bh, r, x + bw / 2.0, y + bh * 1.05, bw * 0.55, p->glow, pressed ? 0.35 : 0.75);
    rrGloss(c, x, y, bw, bh, r, 0.5, pressed ? 0.35 : 0.75, pressed ? 0.12 : 0.28);
    rrStroke(c, x + 1, y + 1, bw - 2, bh - 2, r - 1, 0xffffff, 0.35, 1, 0);
    rrStroke(c, x, y, bw, bh, r, p->border, 0.9, 1.1, 0);
}

static void gelText(HDC dc, const wchar_t *t, RECT tr, const BtnPal *p, int pressed, HFONT f) {
    SetBkMode(dc, TRANSPARENT);
    SIZE sz; int room = tr.right - tr.left - S(10);
    SelectObject(dc, f ? f : hBold);
    GetTextExtentPoint32W(dc, t, (int)wcslen(t), &sz);
    if (!f && sz.cx > room) SelectObject(dc, hBtnSmall);
    OffsetRect(&tr, 0, pressed ? 2 : 1);
    SetTextColor(dc, RGB((p->border >> 16) & 255, (p->border >> 8) & 255, p->border & 255));
    InflateRect(&tr, -S(4), 0);
    DrawTextW(dc, t, -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    OffsetRect(&tr, 0, -1);
    SetTextColor(dc, RGB(255, 255, 255));
    DrawTextW(dc, t, -1, &tr, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

static RECT tabRect(int i) { return RS(17, 88 + i * 70, 62, 62); }

static void drawTabs(Canvas *c) {
    for (int i = 0; i < NTABS; i++) {
        RECT r = tabRect(i);
        double x = r.left, y = r.top, w = r.right - r.left, h = r.bottom - r.top, rad = S(12);
        int active = i == curPage && activated;
        if (active) gel(c, x, y, w, h, rad, &PAL_TEAL, 0, 0);
        else if (i == tabHover && activated) {
            rrFill(c, x, y, w, h, rad, 0x5fd8e8, 0x137a92, 0.45, 0.35);
            rrGloss(c, x, y, w, h, rad, 0.5, 0.35, 0.05);
            rrStroke(c, x, y, w, h, rad, 0x9ff3ff, 0.5, 1, 0);
        }
        COLORREF col = !activated ? RGB(90, 150, 165) : active ? RGB(255, 255, 255) : RGB(200, 245, 252);
        int icol = (GetRValue(col) << 16) | (GetGValue(col) << 8) | GetBValue(col);
        RECT l = { r.left, r.top + S(38), r.right, r.bottom - S(4) };
        if (active) drawIcon(c, i, x + w / 2, y + S(21) + 1, S(24), 0x0b5a66, 0.6);
        drawIcon(c, i, x + w / 2, y + S(21), S(24), icol, 1);
        SetBkMode(c->dc, TRANSPARENT);
        SetTextColor(c->dc, col);
        SelectObject(c->dc, hBtnSmall);
        DrawTextW(c->dc, TABNAME[i], -1, &l, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        if (i == PG_UPDATE && updAvail) {
            disc(c, r.right - S(9), r.top + S(9), S(5), 0xff8a1e, 1, 1);
            disc(c, r.right - S(10), r.top + S(8), S(2), 0xffffff, 0.8, 1);
        }
    }
}

static void drawToggles(Canvas *c) {
    for (int i = 0; i < TG_N; i++) {
        if (tog[i].page != curPage) continue;
        double x = S(tog[i].x), y = S(tog[i].y + 3), s = S(16);
        if (tog[i].radio) {
            double cx = x + s / 2, cy = y + s / 2, r = s / 2;
            disc(c, cx, cy, r, 0xffffff, 0.95, 1);
            disc(c, cx, cy + r * 0.3, r * 0.8, 0xcff7ff, 0.6, r * 0.5);
            ring(c, cx, cy, r, 0x1a8fa3, 0.95, 1.2);
            if (tog[i].on) {
                disc(c, cx, cy, r * 0.55, 0x14a2b6, 1, 1);
                disc(c, cx, cy + r * 0.15, r * 0.4, 0x7ff6e8, 0.8, r * 0.3);
                disc(c, cx - r * 0.15, cy - r * 0.22, r * 0.2, 0xffffff, 0.85, r * 0.12);
            }
        } else {
            double r = S(4);
            if (tog[i].on) {
                rrFill(c, x, y, s, s, r, 0x8ff2f0, 0x14a2b6, 1, 1);
                rrGloss(c, x, y, s, s, r, 0.5, 0.7, 0.2);
                rrStroke(c, x, y, s, s, r, 0x0b5a66, 0.95, 1.1, 0);
                segment(c, x + s * 0.24, y + s * 0.52, x + s * 0.43, y + s * 0.72, S(2), 0xffffff, 1);
                segment(c, x + s * 0.43, y + s * 0.72, x + s * 0.78, y + s * 0.28, S(2), 0xffffff, 1);
            } else {
                rrFill(c, x, y, s, s, r, 0xffffff, 0xdcfbff, 0.95, 0.95);
                rrStroke(c, x, y, s, s, r, 0x1a8fa3, 0.95, 1.1, 0);
            }
        }
    }
}

static void paintMain(HDC hdc) {
    Canvas *src = &pageC[curPage];
    memcpy(backC.px, src->px, (size_t)src->w * src->h * 4);
    drawTabs(&backC);
    drawToggles(&backC);
    HDC dc = backC.dc;
    SetBkMode(dc, TRANSPARENT);
    if (curPage == PG_UPDATE) {
        wchar_t info[600]; updInfo(info, 600);
        disc(&backC, S(120), S(58), S(5), connColor(), 1, 1);
        disc(&backC, S(119), S(56), S(2), 0xffffff, 0.7, 1);
        textAt(dc, hFont, info, 132, 52, 600, 60, NAVY, DT_LEFT | DT_WORDBREAK);
        wchar_t hd[96];
        if (updLatest[0]) swprintf(hd, 96, L"What's new in %ls", updLatest); else wcscpy(hd, L"What's new");
        textAt(dc, hBold, hd, 116, 128, 400, 18, NAVY, DT_LEFT | DT_SINGLELINE);
    }
    textAt(dc, hFont, statusText, 108, 432, 632, 30, NAVY, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
    BitBlt(hdc, 0, 0, src->w, src->h, dc, 0, 0, SRCCOPY);
}

static void drawButton(DRAWITEMSTRUCT *d) {
    Canvas *src = &pageC[curPage];
    RECT wr; GetWindowRect(d->hwndItem, &wr);
    MapWindowPoints(NULL, hMain, (POINT *)&wr, 2);
    int w = d->rcItem.right, h = d->rcItem.bottom;
    Canvas cv = mkCanvas(w, h), *c = &cv;
    for (int y = 0; y < h; y++) {
        int sy = wr.top + y;
        if (sy < 0 || sy >= src->h) continue;
        for (int x = 0; x < w; x++) {
            int sx = wr.left + x;
            c->px[y * w + x] = (sx >= 0 && sx < src->w) ? src->px[sy * src->w + sx] : 0;
        }
    }
    UINT id = d->CtlID;
    const BtnPal *p = id == ID_CLR ? &PAL_RED : (id == ID_GO || id == ID_UINSTALL || id == ID_ACT) ? &PAL_GREEN : &PAL_TEAL;
    int pressed = (d->itemState & ODS_SELECTED) != 0;
    if (id == ID_GO) {
        double r = (w - 4) / 2.0;
        gel(c, 2, 2, w - 4, w - 4, r, p, d->hwndItem == hoverBtn, pressed);
        double off = pressed ? 2 : 1;
        drawIcon(c, nanoBusy ? 4 : 3, w / 2.0 + 1, w / 2.0 + off + 1, w * 0.62, 0x1a6514, 0.8);
        drawIcon(c, nanoBusy ? 4 : 3, w / 2.0, w / 2.0 + off - 0.5, w * 0.62, 0xffffff, 1);
    } else {
        gel(c, 1, 1, w - 2, h - 3, fmin((h - 3) / 2.0, S(10)), p, d->hwndItem == hoverBtn, pressed);
        wchar_t t[96]; GetWindowTextW(d->hwndItem, t, 96);
        RECT tr = { 0, 0, w, h - 2 };
        gelText(c->dc, t, tr, p, pressed, NULL);
    }
    BitBlt(d->hDC, 0, 0, w, h, c->dc, 0, 0, SRCCOPY);
    freeCanvas(c);
}

static LRESULT CALLBACK BtnSub(HWND h, UINT m, WPARAM wp, LPARAM lp, UINT_PTR id, DWORD_PTR rd) {
    (void)id; (void)rd;
    if (m == WM_MOUSEMOVE && hoverBtn != h) {
        HWND old = hoverBtn; hoverBtn = h;
        TRACKMOUSEEVENT t = { sizeof t, TME_LEAVE, h, 0 };
        TrackMouseEvent(&t);
        if (old) InvalidateRect(old, NULL, FALSE);
        InvalidateRect(h, NULL, FALSE);
    } else if (m == WM_MOUSELEAVE && hoverBtn == h) {
        hoverBtn = NULL; InvalidateRect(h, NULL, FALSE);
    } else if (m == WM_ERASEBKGND) return 1;
    else if (m == WM_LBUTTONDBLCLK) m = WM_LBUTTONDOWN;
    return DefSubclassProc(h, m, wp, lp);
}

/* ---------- rich edit helpers ---------- */

static int hiliting;

static void resetTextFormat(HWND re) {
    CHARRANGE keep; SendMessageW(re, EM_EXGETSEL, 0, (LPARAM)&keep);
    CHARFORMAT2W cf; memset(&cf, 0, sizeof cf); cf.cbSize = sizeof cf;
    cf.dwMask = CFM_BACKCOLOR | CFM_COLOR | CFM_BOLD | CFM_ITALIC | CFM_UNDERLINE | CFM_STRIKEOUT;
    cf.dwEffects = CFE_AUTOBACKCOLOR; cf.crTextColor = NAVY;
    SendMessageW(re, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);
    SendMessageW(re, EM_EXSETSEL, 0, (LPARAM)&keep);
}

static LRESULT CALLBACK RichSub(HWND h, UINT m, WPARAM wp, LPARAM lp, UINT_PTR id, DWORD_PTR rd) {
    (void)id; (void)rd;
    int paste = m == WM_PASTE ||
        (m == WM_KEYDOWN && ((wp == 'V' && GetKeyState(VK_CONTROL) < 0) || (wp == VK_INSERT && GetKeyState(VK_SHIFT) < 0)));
    if (m == WM_CHAR && (wp == 0x16 || (wp == 0x0A && GetKeyState(VK_CONTROL) < 0))) return 0;
    if (paste) {
        if (SendMessageW(h, EM_GETOPTIONS, 0, 0) & ECO_READONLY) return 0;
        if (OpenClipboard(h)) {
            HANDLE d = GetClipboardData(CF_UNICODETEXT);
            wchar_t *t = d ? GlobalLock(d) : NULL;
            if (t) { SendMessageW(h, EM_REPLACESEL, TRUE, (LPARAM)t); GlobalUnlock(d); }
            CloseClipboard();
        }
        return 0;
    }
    if (m == WM_KEYDOWN && wp == VK_RETURN && GetKeyState(VK_CONTROL) < 0 && GetDlgCtrlID(h) == ID_IN) {
        PostMessageW(hMain, WM_COMMAND, ID_GO, 0);
        return 0;
    }
    return DefSubclassProc(h, m, wp, lp);
}

static LRESULT CALLBACK KeyEditSub(HWND h, UINT m, WPARAM wp, LPARAM lp, UINT_PTR id, DWORD_PTR rd) {
    (void)id; (void)rd;
    if (m == WM_KEYDOWN && wp == VK_RETURN) { PostMessageW(hMain, WM_COMMAND, ID_ACT, 0); return 0; }
    if (m == WM_CHAR && wp == L'\r') return 0;
    if (m == WM_GETDLGCODE && lp && ((MSG *)lp)->message == WM_KEYDOWN && ((MSG *)lp)->wParam == VK_RETURN) return DLGC_WANTALLKEYS;
    return DefSubclassProc(h, m, wp, lp);
}

static wchar_t *getRichText(HWND te) {
    GETTEXTLENGTHEX gl = { GTL_DEFAULT | GTL_NUMCHARS, 1200 };
    int n = (int)SendMessageW(te, EM_GETTEXTLENGTHEX, (WPARAM)&gl, 0);
    if (n < 0) n = 0;
    wchar_t *t = calloc(n + 2, sizeof(wchar_t));
    GETTEXTEX gt = { (DWORD)((n + 1) * sizeof(wchar_t)), GT_DEFAULT, 1200, NULL, NULL };
    if (n) SendMessageW(te, EM_GETTEXTEX, (WPARAM)&gt, (LPARAM)t);
    for (wchar_t *q = t; *q; q++) if (*q == L'\r') *q = L'\n';
    return t;
}

static void setRichText(HWND re, const wchar_t *s) {
    wchar_t *c = wdup(s);
    for (wchar_t *q = c; *q; q++) if (*q == L'\n') *q = L'\r';
    hiliting = 1;
    SetWindowTextW(re, c);
    hiliting = 0;
    free(c);
}

static void hiliteBox(HWND re) {
    hiliting = 1;
    SendMessageW(re, WM_SETREDRAW, FALSE, 0);
    CHARRANGE keep; SendMessageW(re, EM_EXGETSEL, 0, (LPARAM)&keep);
    POINT scroll; SendMessageW(re, EM_GETSCROLLPOS, 0, (LPARAM)&scroll);
    resetTextFormat(re);
    if (tog[TG_HILITE].on) {
        wchar_t *t = getRichText(re);
        int n = (int)wcslen(t);
        unsigned char *L = computeLocks(t, n, tog[TG_STRICT].on);
        CHARFORMAT2W cf; memset(&cf, 0, sizeof cf); cf.cbSize = sizeof cf;
        cf.dwMask = CFM_BACKCOLOR | CFM_COLOR;
        for (int i = 0; i < n;) {
            if (!L[i]) { i++; continue; }
            int j = i; while (j < n && L[j]) j++;
            cf.crBackColor = L[i] == 4 ? RGB(222, 246, 214) : RGB(200, 242, 246);
            cf.crTextColor = L[i] == 4 ? RGB(30, 96, 30) : RGB(8, 104, 120);
            CHARRANGE cr = { i, j };
            SendMessageW(re, EM_EXSETSEL, 0, (LPARAM)&cr);
            SendMessageW(re, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
            i = j;
        }
        free(L); free(t);
    }
    SendMessageW(re, EM_EXSETSEL, 0, (LPARAM)&keep);
    SendMessageW(re, EM_SETSCROLLPOS, 0, (LPARAM)&scroll);
    SendMessageW(re, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(re, NULL, TRUE);
    hiliting = 0;
}

static void hiliteAll(void) { hiliteBox(GetDlgItem(hMain, ID_IN)); hiliteBox(GetDlgItem(hMain, ID_OUT)); }

/* ---------- clipboard ---------- */

static wchar_t *clipGet(void) {
    wchar_t *r = NULL;
    if (!OpenClipboard(hMain)) return NULL;
    HANDLE d = GetClipboardData(CF_UNICODETEXT);
    wchar_t *t = d ? GlobalLock(d) : NULL;
    if (t) {
        r = wdup(t); GlobalUnlock(d);
        wchar_t *o = r;
        for (wchar_t *p = r; *p; p++) { if (*p == L'\r') { if (p[1] != L'\n') *o++ = L'\n'; } else *o++ = *p; }
        *o = 0;
    }
    CloseClipboard();
    return r;
}

static int clipSet(const wchar_t *s) {
    size_t n = wcslen(s), k = 0;
    for (size_t i = 0; i < n; i++) if (s[i] == L'\n') k++;
    if (!OpenClipboard(hMain)) return 0;
    EmptyClipboard();
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, (n + k + 1) * sizeof(wchar_t));
    wchar_t *p = GlobalLock(h);
    for (size_t i = 0; i < n; i++) { if (s[i] == L'\n') *p++ = L'\r'; *p++ = s[i]; }
    *p = 0;
    GlobalUnlock(h);
    SetClipboardData(CF_UNICODETEXT, h);
    CloseClipboard();
    return 1;
}

static int missingLocks(const wchar_t *in, const wchar_t *out, int strict, wchar_t *list, int cap) {
    int n = (int)wcslen(in), miss = 0, used = 0;
    unsigned char *L = computeLocks(in, n, strict);
    list[0] = 0;
    for (int i = 0; i < n;) {
        if (!L[i]) { i++; continue; }
        int j = i; while (j < n && L[j]) j++;
        wchar_t *piece = wndup(in + i, j - i);
        if (!wcsstr(out, piece)) {
            miss++;
            if (used + (j - i) + 8 < cap) {
                if (used) { wcscat(list, L", "); used += 2; }
                if (j - i > 40) { wcsncat(list, piece, 37); wcscat(list, L"…"); used += 38; }
                else { wcscat(list, piece); used += j - i; }
            }
        }
        free(piece);
        i = j;
    }
    free(L);
    return miss;
}

/* ---------- Claude engine ---------- */

#define PH_OPEN  0x27E6
#define PH_CLOSE 0x27E7

typedef struct { wchar_t *text, *extra; wchar_t key[300], model[80]; int strength, strict, toClip; } CReq;
typedef struct { wchar_t *in, *out, *err; int toClip, engine; NanoStats st; } NRes;

static wchar_t *maskText(const wchar_t *t, int strict, wchar_t ***pieces, int *np) {
    int n = (int)wcslen(t);
    unsigned char *L = computeLocks(t, n, strict);
    Str o = {0}; sAdd(&o, L"");
    *pieces = NULL; *np = 0;
    for (int i = 0; i < n;) {
        if (L[i] == 1 || L[i] == 4) {
            int j = i; while (j < n && (L[j] == 1 || L[j] == 4)) j++;
            *pieces = realloc(*pieces, (*np + 1) * sizeof(wchar_t *));
            (*pieces)[(*np)++] = wndup(t + i, j - i);
            wchar_t ph[24]; swprintf(ph, 24, L"%lc%d%lc", (wint_t)PH_OPEN, *np, (wint_t)PH_CLOSE);
            sAdd(&o, ph);
            i = j;
        } else sAddC(&o, t[i++]);
    }
    free(L);
    return o.s;
}

static wchar_t *unmaskText(const wchar_t *t, wchar_t **pieces, int np, int *missing) {
    Str o = {0}; sAdd(&o, L"");
    char *seen = calloc(np + 1, 1);
    for (const wchar_t *p = t; *p;) {
        if (*p == PH_OPEN) {
            int k = 0; const wchar_t *q = p + 1;
            while (iswdigit(*q)) k = k * 10 + (*q++ - L'0');
            if (*q == PH_CLOSE && k >= 1 && k <= np) { sAdd(&o, pieces[k - 1]); seen[k - 1] = 1; p = q + 1; continue; }
        }
        sAddC(&o, *p++);
    }
    *missing = 0;
    for (int i = 0; i < np; i++) if (!seen[i]) (*missing)++;
    free(seen);
    return o.s;
}

static const wchar_t *STRENGTH_TXT[3] = {
    L"Make light edits: swap in synonyms and rephrase some phrases, but keep the sentence structure mostly the same.",
    L"Reword it thoroughly: change most of the wording and vary the sentence structure.",
    L"Rewrite it aggressively: restructure every sentence, reorder clauses, merge or split sentences where natural, and change the vocabulary throughout so it shares as few phrases as possible with the original."
};

static DWORD WINAPI ClaudeThread(LPVOID arg) {
    CReq *r = arg;
    NRes *res = calloc(1, sizeof *res);
    res->in = r->text; res->toClip = r->toClip; res->engine = 1;
    wchar_t **pieces; int np;
    wchar_t *masked = maskText(r->text, r->strict, &pieces, &np);

    Str sys = {0};
    sAdd(&sys, L"You rewrite text so it looks completely different while keeping exactly the same meaning, point and claims. "
               L"Rules:\n"
               L"- Tokens like ⟦1⟧ are locked placeholders. Keep every one exactly once, unchanged, in a position that makes sense.\n"
               L"- Keep every name, number, date, quantity and factual claim exactly as accurate as the original. Never add or remove information.\n"
               L"- Keep the same language, roughly the same length, paragraph breaks, and the same tone and reading level.\n"
               L"- Output only the rewritten text: no preamble, no notes, no quotation marks around it.\n");
    sAdd(&sys, STRENGTH_TXT[r->strength]);
    if (r->extra && r->extra[0]) { sAdd(&sys, L"\nAlso: "); sAdd(&sys, r->extra); }

    char *se = jsonEsc(sys.s), *te = jsonEsc(masked), *me = jsonEsc(r->model[0] ? r->model : L"claude-sonnet-5-5");
    int maxTok = (int)wcslen(masked) / 2 + 1024; if (maxTok > 16000) maxTok = 16000;
    size_t cap = strlen(se) + strlen(te) + strlen(me) + 512;
    char *body = malloc(cap);
    int blen = snprintf(body, cap, "{\"model\":\"%s\",\"max_tokens\":%d,\"system\":\"%s\",\"messages\":[{\"role\":\"user\",\"content\":\"%s\"}]}",
                        me, maxTok, se, te);
    free(se); free(te); free(me); free(sys.s); free(masked);

    wchar_t hdr[512];
    swprintf(hdr, 512, L"x-api-key: %ls\r\nanthropic-version: 2023-06-01\r\ncontent-type: application/json\r\n", r->key);
    char *resp; size_t rlen; DWORD st;
    int e = httpPost(L"api.anthropic.com", L"/v1/messages", hdr, body, (DWORD)blen, &resp, &rlen, &st);
    free(body);

    if (e) {
        wchar_t m[256]; swprintf(m, 256, L"Couldn't reach Claude (network error %d). Check your internet, or switch to Offline.", e);
        res->err = wdup(m);
    } else if (st == 200) {
        char *tp = strstr(resp, "\"type\":\"text\"");
        char *txt = jsonStr(resp, "text", tp ? tp : resp, NULL);
        if (txt && txt[0]) {
            wchar_t *w = u8dup(txt);
            wchar_t *a = w; while (iswspace(*a)) a++;
            int L2 = (int)wcslen(a); while (L2 && iswspace(a[L2 - 1])) a[--L2] = 0;
            int missing;
            res->out = unmaskText(a, pieces, np, &missing);
            if (missing) {
                wchar_t m[200]; swprintf(m, 200, L"Claude dropped %d locked part%ls, so the result was thrown out. Hit rewrite to try again.", missing, missing == 1 ? L"" : L"s");
                free(res->out); res->out = NULL; res->err = wdup(m);
            }
            free(w);
        } else res->err = wdup(L"Claude returned no text.");
        free(txt);
    } else {
        char *msg = jsonStr(resp, "message", NULL, NULL);
        wchar_t *wm = u8dup(msg ? msg : "");
        wchar_t m[400];
        if (st == 401) swprintf(m, 400, L"Claude rejected the API key (HTTP 401). Check it on the Claude page.");
        else swprintf(m, 400, L"Claude API error (HTTP %lu): %ls", (unsigned long)st, wm);
        res->err = wdup(m);
        free(msg); free(wm);
    }
    free(resp);
    for (int i = 0; i < np; i++) free(pieces[i]);
    free(pieces);
    free(r->extra); free(r);
    PostMessageW(hMain, WM_NANODONE, 0, (LPARAM)res);
    return 0;
}

static wchar_t *getEditText(int id) {
    HWND h = GetDlgItem(hMain, id);
    int n = GetWindowTextLengthW(h);
    wchar_t *t = calloc(n + 2, sizeof(wchar_t));
    GetWindowTextW(h, t, n + 1);
    return t;
}

static void setGoLabel(void) { InvalidateRect(GetDlgItem(hMain, ID_GO), NULL, FALSE); }

static void finishRewrite(NRes *r) {
    if (r->err) {
        status(L"%ls", r->err);
    } else {
        setRichText(GetDlgItem(hMain, ID_OUT), r->out);
        hiliteBox(GetDlgItem(hMain, ID_OUT));
        wchar_t miss[200];
        int m = missingLocks(r->in, r->out, tog[TG_STRICT].on, miss, 200);
        wchar_t tail[100] = L"";
        if (r->toClip && clipSet(r->out)) wcscpy(tail, L"  Copied, paste it anywhere.");
        if (m) status(L"Heads up: %d locked bit%ls changed or moved: %ls", m, m == 1 ? L"" : L"s", miss);
        else if (r->engine == 0)
            status(L"Done. %d of %d words reworded, %d sentence%ls restructured, %d part%ls locked.%ls",
                   r->st.changed, r->st.words, r->st.restructured, r->st.restructured == 1 ? L"" : L"s",
                   r->st.locks, r->st.locks == 1 ? L"" : L"s", tail);
        else status(L"Done. Rewritten by Claude, every locked part checked and intact.%ls", tail);
    }
    free(r->in); free(r->out); free(r->err); free(r);
    InterlockedExchange(&nanoBusy, 0);
    setGoLabel();
}

static void doRewrite(int fromClip) {
    if (!activated) return;
    if (nanoBusy) { status(L"Still working on the last one…"); return; }
    wchar_t *text;
    if (fromClip) {
        text = clipGet();
        if (!text || !text[0]) { free(text); status(L"Clipboard has no text. Copy something first, then press Ctrl+Alt+N."); return; }
        setRichText(GetDlgItem(hMain, ID_IN), text);
        hiliteBox(GetDlgItem(hMain, ID_IN));
    } else text = getRichText(GetDlgItem(hMain, ID_IN));
    int any = 0; for (wchar_t *p = text; *p; p++) if (!iswspace(*p)) { any = 1; break; }
    if (!any) { free(text); status(L"Type or paste some text in the Original box first."); return; }
    InterlockedExchange(&nanoBusy, 1);
    setGoLabel();
    if (tog[TG_CLAUDE].on) {
        CReq *r = calloc(1, sizeof *r);
        GetDlgItemTextW(hMain, ID_KEY, r->key, 300);
        GetDlgItemTextW(hMain, ID_MODEL, r->model, 80);
        r->extra = getEditText(ID_EXTRA);
        for (wchar_t *p = r->extra; *p; p++) if (*p == L'\r' || *p == L'\n') *p = L' ';
        wchar_t *k = r->key; while (*k == L' ') k++;
        if (!*k) {
            free(r->extra); free(r); free(text);
            InterlockedExchange(&nanoBusy, 0); setGoLabel();
            status(L"The Claude engine needs an API key. Paste it on the Claude page, or switch to Offline.");
            return;
        }
        if (k != r->key) memmove(r->key, k, (wcslen(k) + 1) * sizeof(wchar_t));
        int L2 = (int)wcslen(r->key); while (L2 && iswspace(r->key[L2 - 1])) r->key[--L2] = 0;
        r->text = text; r->strength = strength(); r->strict = tog[TG_STRICT].on; r->toClip = fromClip;
        status(L"Asking Claude…");
        CloseHandle(CreateThread(NULL, 0, ClaudeThread, r, 0, NULL));
        return;
    }
    NanoOpts o = { strength(), tog[TG_STRICT].on };
    NRes *res = calloc(1, sizeof *res);
    LARGE_INTEGER c; QueryPerformanceCounter(&c);
    res->in = text; res->toClip = fromClip; res->engine = 0;
    res->out = nanoRewrite(text, &o, &res->st, (unsigned long long)c.QuadPart ^ GetTickCount64());
    finishRewrite(res);
}

/* ---------- frame ---------- */

static LRESULT CALLBACK FrameProc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    switch (m) {
    case WM_NCHITTEST: {
        POINT p = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
        ScreenToClient(h, &p);
        if (capAt(p.x, p.y) >= 0) return HTCLIENT;
        RECT r; GetClientRect(h, &r);
        int b = S(6), L = p.x < b, Rt = p.x >= r.right - b, T = p.y < S(3), B = p.y >= r.bottom - b;
        if (T && L) return HTTOPLEFT;
        if (T && Rt) return HTTOPRIGHT;
        if (B && L) return HTBOTTOMLEFT;
        if (B && Rt) return HTBOTTOMRIGHT;
        if (L) return HTLEFT;
        if (Rt) return HTRIGHT;
        if (B) return HTBOTTOM;
        if (T) return HTTOP;
        return HTCAPTION;
    }
    case WM_NCLBUTTONDBLCLK:
        if (wp == HTCAPTION && fabs(uiScale - 1.0f) > 0.01f) { uiScale = 1.0f; rescaleAll(); }
        return 0;
    case WM_SIZING: {
        RECT *r = (RECT *)lp;
        if (!sizing) {
            sizing = 1;
            snapC = mkCanvas(frameC.w, frameC.h);
            PrintWindow(h, snapC.dc, 2);
            ShowWindow(hMain, SW_HIDE);
        }
        double bw = (BW + 2 * FR) * dpi, bh = (BH + TT + FR) * dpi;
        int w = r->right - r->left, hg = r->bottom - r->top;
        double sc = (wp == WMSZ_TOP || wp == WMSZ_BOTTOM) ? hg / bh : w / bw;
        if (sc < 0.7) sc = 0.7;
        if (sc > 2.2) sc = 2.2;
        int nw = (int)(bw * sc + .5), nh = (int)(bh * sc + .5);
        if (wp == WMSZ_LEFT || wp == WMSZ_TOPLEFT || wp == WMSZ_BOTTOMLEFT) r->left = r->right - nw; else r->right = r->left + nw;
        if (wp == WMSZ_TOP || wp == WMSZ_TOPLEFT || wp == WMSZ_TOPRIGHT) r->top = r->bottom - nh; else r->bottom = r->top + nh;
    } return TRUE;
    case WM_SIZE: if (sizing) InvalidateRect(h, NULL, FALSE); break;
    case WM_EXITSIZEMOVE:
        if (sizing) {
            RECT wr; GetWindowRect(h, &wr);
            uiScale = (float)((wr.right - wr.left) / ((BW + 2 * FR) * dpi));
            sizing = 0;
            freeCanvas(&snapC); memset(&snapC, 0, sizeof snapC);
            rescaleAll();
        }
        break;
    case WM_NCCALCSIZE: if (wp) return 0; break;
    case WM_NCACTIVATE: return DefWindowProcW(h, m, wp, -1);
    case WM_SYSCOMMAND:
        if ((wp & 0xFFF0) == SC_MAXIMIZE) return 0;
        break;
    case WM_WINDOWPOSCHANGING: {
        WINDOWPOS *wpos = (WINDOWPOS *)lp;
        if (!IsIconic(h) && !sizing && !rescaling && !(wpos->flags & SWP_NOSIZE)) { wpos->cx = frameC.w; wpos->cy = frameC.h; }
    } break;
    case WM_MOUSEMOVE: {
        int i = capAt(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        if (i != capHover) {
            capHover = i; capInvalidate();
            TRACKMOUSEEVENT t = { sizeof t, TME_LEAVE, h, 0 };
            TrackMouseEvent(&t);
        }
    } return 0;
    case WM_MOUSELEAVE: if (capDown < 0) { capHover = -1; capInvalidate(); } return 0;
    case WM_LBUTTONDOWN: capDown = capAt(GET_X_LPARAM(lp), GET_Y_LPARAM(lp)); SetCapture(h); capInvalidate(); return 0;
    case WM_LBUTTONUP: {
        int i = capAt(GET_X_LPARAM(lp), GET_Y_LPARAM(lp)), was = capDown;
        capDown = -1; ReleaseCapture();
        capHover = i; capInvalidate();
        if (i >= 0 && i == was) {
            if (i == 1) PostMessageW(h, WM_CLOSE, 0, 0);
            else { capHover = -1; ShowWindow(h, SW_MINIMIZE); }
        }
    } return 0;
    case WM_SETFOCUS: SetFocus(GetDlgItem(hMain, curPage == PG_LOCK ? ID_ACTKEY : curPage == PG_CLAUDE ? ID_KEY : curPage == PG_UPDATE ? ID_UCHECK : ID_IN)); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps);
        paintFrame(dc);
        EndPaint(h, &ps);
    } return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(h, m, wp, lp);
}

/* ---------- controls & pages ---------- */

typedef struct { HWND h; int page, x, y, w, hh; } Ctl;
static Ctl ctls[48]; static int nctl;
static int buildPage = PG_REWRITE;

static HWND mk(DWORD ex, const wchar_t *cls, const wchar_t *txt, DWORD st, int x, int y, int w, int h, int id) {
    HWND c = CreateWindowExW(ex, cls, txt, WS_CHILD | st, S(x), S(y), S(w), S(h),
                             hMain, (HMENU)(INT_PTR)id, GetModuleHandleW(NULL), NULL);
    SendMessageW(c, WM_SETFONT, (WPARAM)hFont, TRUE);
    if (nctl < 48) { Ctl k = { c, buildPage, x, y, w, h }; ctls[nctl++] = k; }
    return c;
}
static HWND BTN(const wchar_t *t, int x, int y, int w, int h, int id) {
    HWND b = mk(0, L"BUTTON", t, WS_TABSTOP | BS_OWNERDRAW, x, y, w, h, id);
    SetWindowSubclass(b, BtnSub, 1, 0);
    return b;
}

static void showPage(int p) {
    if (!activated) p = PG_LOCK;
    curPage = p;
    hoverBtn = NULL;
    for (int i = 0; i < nctl; i++) ShowWindow(ctls[i].h, ctls[i].page == p ? SW_SHOWNA : SW_HIDE);
    InvalidateRect(hMain, NULL, FALSE);
    SetFocus(GetDlgItem(hMain, p == PG_LOCK ? ID_ACTKEY : p == PG_CLAUDE ? ID_KEY : p == PG_UPDATE ? ID_UCHECK : ID_IN));
}

static HWND mkRich(int x, int y, int w, int h, int id) {
    HWND te = mk(0, MSFTEDIT_CLASS, L"", WS_TABSTOP | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | ES_NOHIDESEL,
                 x, y, w, h, id);
    SendMessageW(te, EM_EXLIMITTEXT, 0, 0x7FFFFFFF);
    SendMessageW(te, EM_SETBKGNDCOLOR, 0, RGB(255, 255, 255));
    SendMessageW(te, EM_SETTARGETDEVICE, 0, 0);
    SendMessageW(te, WM_SETFONT, (WPARAM)hEdit, TRUE);
    resetTextFormat(te);
    SetWindowSubclass(te, RichSub, 2, 0);
    SendMessageW(te, EM_SETEVENTMASK, 0, ENM_CHANGE);
    return te;
}

static void buildUI(void) {
    buildPage = PG_REWRITE;
    mkRich(106, 86, 280, 286, ID_IN);
    mkRich(460, 86, 280, 286, ID_OUT);
    BTN(L"Rewrite", 399, 196, 48, 48, ID_GO);
    BTN(L"Copy", 480, 393, 76, 30, ID_COPY);
    BTN(L"Use as input", 560, 393, 106, 30, ID_USE);
    BTN(L"Clear", 670, 393, 70, 30, ID_CLR);

    buildPage = PG_CLAUDE;
    mk(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_TABSTOP | ES_AUTOHSCROLL | ES_PASSWORD, 176, 53, 360, 22, ID_KEY);
    mk(WS_EX_CLIENTEDGE, L"EDIT", L"claude-sonnet-5-5", WS_TABSTOP | ES_AUTOHSCROLL, 176, 79, 200, 22, ID_MODEL);
    mk(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_TABSTOP | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL, 108, 132, 632, 112, ID_EXTRA);

    buildPage = PG_UPDATE;
    mk(0, L"EDIT", L"", WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL, 114, 150, 618, 192, ID_UNOTES);
    BTN(L"Check now", 106, 386, 120, 30, ID_UCHECK);
    BTN(L"Install update", 232, 386, 150, 30, ID_UINSTALL);
    BTN(L"Open GitHub page", 388, 386, 160, 30, ID_UPAGE);

    buildPage = PG_LOCK;
    HWND ke = mk(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_TABSTOP | ES_CENTER | ES_UPPERCASE | ES_AUTOHSCROLL, 250, 246, 350, 30, ID_ACTKEY);
    SendMessageW(ke, EM_SETLIMITTEXT, 40, 0);
    SetWindowSubclass(ke, KeyEditSub, 5, 0);
    BTN(L"Activate", 330, 292, 190, 36, ID_ACT);
}

static void makeFonts(void) {
    HFONT old[10] = { hFont, hBold, hTitle, hSmall, hBtnSmall, hEdit, hBrand, hBig, hHuge, hTiny };
    hFont   = CreateFontW(-S(13), 0, 0, 0, FW_NORMAL,   0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    hBold   = CreateFontW(-S(13), 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    hTitle  = CreateFontW(-S(15), 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    hSmall  = CreateFontW(-S(11), 0, 0, 0, FW_NORMAL,   1, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    hBtnSmall = CreateFontW(-S(11), 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    hEdit   = CreateFontW(-S(14), 0, 0, 0, FW_NORMAL,   0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    hBrand  = CreateFontW(-S(20), 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    hBig    = CreateFontW(-S(22), 0, 0, 0, FW_BOLD,     0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    hHuge   = CreateFontW(-S(38), 0, 0, 0, FW_BOLD,     0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    hTiny   = CreateFontW(-S(10), 0, 0, 0, FW_SEMIBOLD, 0, 0, 0, DEFAULT_CHARSET, 0, 0, CLEARTYPE_QUALITY, 0, L"Segoe UI");
    for (int i = 0; i < 10; i++) if (old[i]) DeleteObject(old[i]);
}

static void makeCanvases(void) {
    int cw = S(BW), ch = S(BH), fw = cw + 2 * S(FR), fh = ch + S(TT) + S(FR);
    for (int p = 0; p < PG_N; p++) { if (pageC[p].dc) freeCanvas(&pageC[p]); pageC[p] = mkCanvas(cw, ch); }
    if (backC.dc) freeCanvas(&backC);
    if (frameC.dc) freeCanvas(&frameC);
    if (frameBack.dc) freeCanvas(&frameBack);
    backC = mkCanvas(cw, ch);
    frameC = mkCanvas(fw, fh);
    frameBack = mkCanvas(fw, fh);
    renderPages();
    renderFrame();
}

static HFONT ctlFont(int id) { return id == ID_IN || id == ID_OUT ? hEdit : id == ID_ACTKEY ? hBrand : hFont; }

static void rescaleAll(void) {
    makeFonts();
    makeCanvases();
    rescaling = 1;
    RECT wr; GetWindowRect(hFrame, &wr);
    SetWindowPos(hFrame, NULL, wr.left, wr.top, frameC.w, frameC.h, SWP_NOZORDER | SWP_NOACTIVATE);
    rescaling = 0;
    SetWindowPos(hMain, NULL, S(FR), S(TT), backC.w, backC.h, SWP_NOZORDER | SWP_NOACTIVATE);
    for (int i = 0; i < nctl; i++) {
        SetWindowPos(ctls[i].h, NULL, S(ctls[i].x), S(ctls[i].y), S(ctls[i].w), S(ctls[i].hh), SWP_NOZORDER | SWP_NOACTIVATE);
        SendMessageW(ctls[i].h, WM_SETFONT, (WPARAM)ctlFont(GetDlgCtrlID(ctls[i].h)), TRUE);
    }
    hiliteAll();
    ShowWindow(hMain, SW_SHOW);
    showPage(curPage);
    RedrawWindow(hFrame, NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    wchar_t b[32]; swprintf(b, 32, L"%.3f", uiScale);
    WritePrivateProfileStringW(L"Settings", L"Scale", b, iniPath);
}

static const wchar_t *TOGKEY[TG_N] = { L"EngineOffline", L"EngineClaude", L"Light", L"Medium", L"Heavy", L"LockFacts", L"Highlight", L"UpdateCheck", L"UpdateAuto" };

static void loadSettings(void) {
    for (int i = 0; i < TG_N; i++) tog[i].on = GetPrivateProfileIntW(L"Settings", TOGKEY[i], tog[i].on, iniPath) != 0;
    if (tog[TG_OFF].on == tog[TG_CLAUDE].on) { tog[TG_OFF].on = 1; tog[TG_CLAUDE].on = 0; }
    if (tog[TG_LIGHT].on + tog[TG_MED].on + tog[TG_HEAVY].on != 1) { tog[TG_LIGHT].on = tog[TG_HEAVY].on = 0; tog[TG_MED].on = 1; }
    wchar_t b[400];
    if (GetPrivateProfileStringW(L"Claude", L"ApiKey", L"", b, 400, iniPath) && b[0]) SetDlgItemTextW(hMain, ID_KEY, b);
    if (GetPrivateProfileStringW(L"Claude", L"Model", L"", b, 400, iniPath) && b[0]) SetDlgItemTextW(hMain, ID_MODEL, b);
    if (GetPrivateProfileStringW(L"Claude", L"Extra", L"", b, 400, iniPath) && b[0]) SetDlgItemTextW(hMain, ID_EXTRA, b);
    GetPrivateProfileStringW(L"Activation", L"Key", L"", b, 400, iniPath);
    activated = keyValid(b, NULL);
}

static void saveSettings(void) {
    for (int i = 0; i < TG_N; i++) WritePrivateProfileStringW(L"Settings", TOGKEY[i], tog[i].on ? L"1" : L"0", iniPath);
    wchar_t b[400];
    GetDlgItemTextW(hMain, ID_KEY, b, 400); WritePrivateProfileStringW(L"Claude", L"ApiKey", b, iniPath);
    GetDlgItemTextW(hMain, ID_MODEL, b, 400); WritePrivateProfileStringW(L"Claude", L"Model", b, iniPath);
    GetDlgItemTextW(hMain, ID_EXTRA, b, 400);
    for (wchar_t *p = b; *p; p++) if (*p == L'\r' || *p == L'\n') *p = L' ';
    WritePrivateProfileStringW(L"Claude", L"Extra", b, iniPath);
}

static void idleStatus(void) {
    status(L"Paste text on the left and hit the arrow. Quotes, (parens), names and numbers stay locked. Ctrl+Alt+N rewrites your clipboard.");
}

static void enableHotkey(void) {
    if (!RegisterHotKey(hMain, HK_CLIP, MOD_CONTROL | MOD_ALT | 0x4000, 'N'))
        status(L"Ctrl+Alt+N is taken by another app, so clipboard rewriting is off. Everything else works.");
    else idleStatus();
}

static void tryActivate(void) {
    wchar_t in[64], norm[24];
    GetDlgItemTextW(hMain, ID_ACTKEY, in, 64);
    if (!keyValid(in, norm)) {
        status(in[0] ? L"That key isn't valid. Check for typos (dashes and spaces don't matter)." : L"Type your activation key first.");
        return;
    }
    WritePrivateProfileStringW(L"Activation", L"Key", norm, iniPath);
    activated = 1;
    showPage(PG_REWRITE);
    InvalidateRect(hMain, NULL, FALSE);
    enableHotkey();
    status(L"Activated, welcome to Frutiger Nano. Paste text on the left and hit the arrow.");
}

static int tabAt(int mx, int my) {
    POINT p = { mx, my };
    for (int i = 0; i < NTABS; i++) { RECT r = tabRect(i); if (PtInRect(&r, p)) return i; }
    return -1;
}

static void clickToggle(int mx, int my) {
    for (int i = 0; i < TG_N; i++) {
        if (tog[i].page != curPage) continue;
        RECT r = RS(tog[i].x, tog[i].y, tog[i].w, 22);
        if (mx < r.left || mx >= r.right || my < r.top || my >= r.bottom) continue;
        if (tog[i].radio) { for (int k = 0; k < TG_N; k++) if (tog[k].radio == tog[i].radio) tog[k].on = 0; tog[i].on = 1; }
        else tog[i].on = !tog[i].on;
        InvalidateRect(hMain, NULL, FALSE);
        if (i == TG_HILITE || i == TG_STRICT) hiliteAll();
        if (i == TG_CLAUDE) {
            wchar_t k[8]; GetDlgItemTextW(hMain, ID_KEY, k, 8);
            status(k[0] ? L"Claude engine on. Rewrites use your API credits." : L"Claude engine on. Paste your API key on the Claude page first.");
        }
        if (i == TG_OFF) status(L"Offline engine on. Free, instant, works without internet.");
        return;
    }
}

static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    switch (m) {
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case ID_ACT: tryActivate(); break;
        case ID_GO: doRewrite(0); break;
        case ID_COPY: {
            wchar_t *t = getRichText(GetDlgItem(h, ID_OUT));
            if (t[0] && clipSet(t)) status(L"Rewritten text copied to the clipboard.");
            else status(L"Nothing to copy yet. Hit the arrow first.");
            free(t);
        } break;
        case ID_USE: {
            wchar_t *t = getRichText(GetDlgItem(h, ID_OUT));
            if (t[0]) {
                setRichText(GetDlgItem(h, ID_IN), t);
                setRichText(GetDlgItem(h, ID_OUT), L"");
                hiliteAll();
                status(L"Moved the rewrite over. Hit the arrow again for a second pass.");
            }
            free(t);
        } break;
        case ID_CLR:
            setRichText(GetDlgItem(h, ID_IN), L""); setRichText(GetDlgItem(h, ID_OUT), L"");
            idleStatus();
            SetFocus(GetDlgItem(h, ID_IN));
            break;
        case ID_IN:
            if (HIWORD(wp) == EN_CHANGE && !hiliting) SetTimer(h, TIMER_HL, 350, NULL);
            break;
        case ID_UCHECK: startCheck(); break;
        case ID_UINSTALL: startInstall(); break;
        case ID_UPAGE: {
            wchar_t url[600];
            if (updPage[0]) wcscpy(url, updPage); else swprintf(url, 600, L"https://github.com/%ls", updRepo);
            ShellExecuteW(NULL, L"open", url, NULL, NULL, SW_SHOWNORMAL);
        } break;
        }
        return 0;
    case WM_TIMER:
        if (wp == TIMER_HL) { KillTimer(h, TIMER_HL); hiliteBox(GetDlgItem(h, ID_IN)); }
        return 0;
    case WM_LBUTTONDOWN: {
        int x = GET_X_LPARAM(lp), y = GET_Y_LPARAM(lp), t = tabAt(x, y);
        if (t >= 0) { if (activated && t != curPage) showPage(t); }
        else clickToggle(x, y);
    } return 0;
    case WM_MOUSEMOVE: {
        int t = tabAt(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        if (t != tabHover) {
            tabHover = t;
            TRACKMOUSEEVENT tm = { sizeof tm, TME_LEAVE, h, 0 };
            TrackMouseEvent(&tm);
            RECT r = RS(10, 80, 76, 230); InvalidateRect(h, &r, FALSE);
        }
    } return 0;
    case WM_MOUSELEAVE: if (tabHover >= 0) { tabHover = -1; RECT r = RS(10, 80, 76, 230); InvalidateRect(h, &r, FALSE); } return 0;
    case WM_DRAWITEM: drawButton((DRAWITEMSTRUCT *)lp); return TRUE;
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORSTATIC:
        SetTextColor((HDC)wp, NAVY); SetBkColor((HDC)wp, RGB(255, 255, 255));
        return (LRESULT)GetStockObject(WHITE_BRUSH);
    case WM_ERASEBKGND: return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps; HDC dc = BeginPaint(h, &ps);
        paintMain(dc);
        EndPaint(h, &ps);
    } return 0;
    case WM_HOTKEY:
        if (wp == HK_CLIP) doRewrite(1);
        return 0;
    case WM_STATUS:
        wcsncpy(statusText, (wchar_t *)lp, 511); free((void *)lp);
        { RECT r = RS(96, 432, 654, 30); InvalidateRect(h, &r, FALSE); }
        return 0;
    case WM_NANODONE: finishRewrite((NRes *)lp); return 0;
    case WM_UPDCHECKED:
        fillNotes();
        InvalidateRect(h, NULL, FALSE);
        if (updAvail == 1) {
            if (tog[TG_UAUTO].on && !nanoBusy) startInstall();
            else if (activated) status(L"Update %ls is available. Open the Update page to install it.", updLatest);
        }
        return 0;
    case WM_UPDFAIL: {
        wchar_t *msg = (wchar_t *)lp;
        updErr = 1;
        _snwprintf(updConnMsg, 159, L"%ls", msg); updConnMsg[159] = 0;
        status(L"%ls", msg); free(msg); InvalidateRect(h, NULL, FALSE);
    } return 0;
    case WM_UPDRESTART: restartUpdated(); return 0;
    case WM_DESTROY:
        saveSettings();
        UnregisterHotKey(h, HK_CLIP);
        return 0;
    }
    return DefWindowProcW(h, m, wp, lp);
}

int WINAPI WinMain(HINSTANCE hi, HINSTANCE hp, LPSTR cmd, int show) {
    (void)hp; (void)cmd;
    SetProcessDPIAware();
    INITCOMMONCONTROLSEX icc = { sizeof icc, ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);
    LoadLibraryW(L"Msftedit.dll");
    nanoInit();

    HDC dc = GetDC(NULL); dpi = GetDeviceCaps(dc, LOGPIXELSY) / 96.0f; ReleaseDC(NULL, dc);
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    wcscpy(iniPath, exePath);
    wchar_t *sl = wcsrchr(iniPath, L'\\'); if (sl) sl[1] = 0;
    wcscat(iniPath, L"FrutigerNano.ini");
    {
        wchar_t old[MAX_PATH + 8]; swprintf(old, MAX_PATH + 8, L"%ls.old", exePath);
        for (int i = 0; i < 20 && GetFileAttributesW(old) != INVALID_FILE_ATTRIBUTES && !DeleteFileW(old); i++) Sleep(150);
        justUpdated = wcsstr(GetCommandLineW(), L"--updated") != NULL;
        wchar_t b[64];
        GetPrivateProfileStringW(L"Settings", L"Scale", L"1", b, 64, iniPath);
        uiScale = (float)_wtof(b);
        if (uiScale < 0.7f || uiScale > 2.2f) uiScale = 1.0f;
        GetPrivateProfileStringW(L"Update", L"Repo", updRepo, updRepo, 128, iniPath);
    }
    makeFonts();

    WNDCLASSW wc = {0};
    wc.lpfnWndProc = WndProc; wc.hInstance = hi; wc.lpszClassName = L"FrutigerNano";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClassW(&wc);
    WNDCLASSW fc = {0};
    fc.style = CS_HREDRAW | CS_VREDRAW;
    fc.lpfnWndProc = FrameProc; fc.hInstance = hi; fc.lpszClassName = L"FrutigerNanoFrame";
    fc.hCursor = LoadCursor(NULL, IDC_ARROW);
    fc.hIcon = LoadIconW(hi, MAKEINTRESOURCEW(1));
    RegisterClassW(&fc);

    makeCanvases();
    int cw = backC.w, ch = backC.h, fw = frameC.w, fh = frameC.h;

    RECT wa; SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
    hFrame = CreateWindowExW(WS_EX_APPWINDOW, L"FrutigerNanoFrame", L"Frutiger Nano",
                             WS_POPUP | WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX | WS_SYSMENU | WS_CLIPCHILDREN,
                             wa.left + (wa.right - wa.left - fw) / 2, wa.top + (wa.bottom - wa.top - fh) / 2, fw, fh,
                             NULL, NULL, hi, NULL);
    DWORD pref = 2;
    DwmSetWindowAttribute(hFrame, 33, &pref, sizeof pref);
    MARGINS mg = { 0, 0, 1, 0 };
    DwmExtendFrameIntoClientArea(hFrame, &mg);
    SetWindowPos(hFrame, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    hMain = CreateWindowExW(WS_EX_CONTROLPARENT, L"FrutigerNano", L"", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
                            S(FR), S(TT), cw, ch, hFrame, NULL, hi, NULL);
    buildUI();
    SendMessageW(GetDlgItem(hMain, ID_ACTKEY), WM_SETFONT, (WPARAM)hBrand, TRUE);
    loadSettings();
    fillNotes();
    showPage(PG_REWRITE);
    if (activated) enableHotkey();
    else status(L"Enter your activation key to unlock Frutiger Nano.");
    if (justUpdated && activated) status(L"Updated to v%ls, enjoy!", APP_VERSION_W);
    if (tog[TG_UCHECK].on) startCheck();
    ShowWindow(hFrame, show);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        if (!IsDialogMessageW(hMain, &msg)) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    }
    return 0;
}
