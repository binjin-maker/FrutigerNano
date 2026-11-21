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
    L"hi", L"hey", L"throughout", L"within", L"across", L"among", L"beyond", L"near", L"around", L"until", L"under",
    L"whether", L"whatever", L"whoever", L"wherever", L"besides", L"during", L"despite", L"unlike", L"like", NULL
};

static const wchar_t *DETS[] = { L"a", L"an", L"the", L"this", L"that", L"these", L"those", L"my", L"your", L"his", L"her",
    L"our", L"their", L"its", L"every", L"each", L"no", L"some", L"any", L"another", L"which", L"whose", L"what", L"one", NULL };
static const wchar_t *PREPS[] = { L"in", L"of", L"for", L"with", L"by", L"from", L"on", L"at", L"about", L"into", NULL };
static const wchar_t *VERBISH[] = { L"to", L"will", L"would", L"can", L"could", L"should", L"must", L"might", L"may", L"shall",
    L"i", L"you", L"we", L"they", L"he", L"she", L"it", L"not", L"don't", L"doesn't", L"didn't", L"won't", L"can't", L"never",
    L"who", L"also", L"often", L"always", L"usually", NULL };
static const wchar_t *NOUNPREV[] = { L"much", L"little", L"less", L"more", L"enough", NULL };
static const wchar_t *PARTICLES[] = { L"up", L"out", L"off", L"away", L"over", L"down", L"back", L"on", L"in", L"for", L"around", L"through", NULL };
static const wchar_t *BEFORMS[] = { L"is", L"are", L"was", L"were", L"be", L"been", L"being", L"am", L"seems", L"seem", L"seemed", L"looks", L"look",
    L"became", L"become", L"it's", L"that's", L"isn't", L"wasn't", L"not", NULL };
static const wchar_t *PERSONAL[] = { L"i", L"we", L"you", L"he", L"she", L"they", L"would", L"could", L"might", L"can", L"may", L"to", L"will",
    L"should", L"must", L"me", L"us", L"i'd", L"we'd", NULL };
static const wchar_t *QUANTPREV[] = { L"how", L"so", L"too", L"as", L"very", L"the", L"of", L"nothing", L"all", L"anything",
    L"everything", L"none", L"anyone", L"anybody", L"everyone", L"no", L"not", L"many", NULL };
static const wchar_t *AFTERSET[] = { L"to", L"that", L"with", L"about", L"for", L"of", L"in", L"on", L"at", L"by", NULL };
static const wchar_t *PPCUE[] = { L"have", L"has", L"had", L"having", L"been", L"be", L"was", L"were", L"is", L"are", L"am",
    L"being", L"get", L"got", L"gets", L"i've", L"you've", L"we've", L"they've", L"it's", L"he's", L"she's", NULL };
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
    L"B can't believe|cannot believe|can not believe|couldn't believe|could not believe|as it happens|as it turns out|truth be told|in truth|as soon as|common sense|in common|by the way|on the way|as usual|in case|kind of|sort of|a little|at least|take it easy|a great deal|no longer|for good|at all|in time|of all time|the same|as well|make sense|makes sense|made sense|so far|in charge|at large|in short supply|start over|starts over|started over|starting over|in a way|all the way",

    L"V show:shows:showed:shown:showing up|arrive",
    L"V show:shows:showed:shown:showing|demonstrate|illustrate|reveal",
    L"V suggest:suggests:suggested:suggested:suggesting|imply",
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
    L"V recommend > suggest",
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
    L"V mention > note",
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
    L"N? benefit|advantage",
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
    L"A easy > simple|straightforward",
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
    L"A completely|totally|entirely",
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
    L"A main > primary|chief",
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
    L"P at first glance|at a glance|on the surface",
    L"P for example|for instance",
    L"P such as > like|including",
    L"P in my opinion|in my view|from my perspective",
    L"P on the other hand|by contrast|conversely",
    L"P along with|together with|+as well as",
    L"P each other|one another",
    L"P even though > although|though",
    L"P although > even though|though",
    L"P in spite of|despite",
    L"P in order that > so that",
    L"P as long as|so long as",
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
    L"O also|in addition|on top of that|plus",
    L"O additionally|furthermore|moreover|what's more > also|plus|on top of that",
    L"O for example|for instance|as an example|to illustrate",
    L"O in conclusion|to conclude|in summary|to sum up|all in all|overall|in short",
    L"O first|first of all|to begin with|to start with|firstly",
    L"O second|secondly",
    L"O finally|lastly",
    L"O in fact|actually",
    L"O as a matter of fact|in reality > in fact|actually",
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
    L"O eventually|ultimately|in the end",
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
typedef struct { wchar_t type, flags[8]; wchar_t *skip; int n; Mem *m; } Grp;
typedef struct { wchar_t *key; int g, m, f; } Ent;

static Grp *grp; static int ngrp, gcap;
#define TCAP 65536
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

static void addGroup(wchar_t type, const wchar_t *flags, const wchar_t *skip, const wchar_t *body) {
    if (ngrp == gcap) { gcap = gcap ? gcap * 2 : 512; grp = realloc(grp, gcap * sizeof(Grp)); }
    Grp *g = &grp[ngrp]; g->type = type; wcsncpy(g->flags, flags ? flags : L"", 7); g->flags[7] = 0;
    g->skip = skip && *skip ? wdup(skip) : NULL; g->n = 0; g->m = NULL;
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

static void taggerInit(void);
static int nanoReady;
static void nanoInit(void) {
    if (nanoReady) return;
    nanoReady = 1;
    const wchar_t **LISTS[3] = { THES, THES2, THES3 };
    for (int pass = 0; pass < 3; pass++)
    for (int i = 0; LISTS[pass][i]; i++) {
        const wchar_t *l = LISTS[pass][i];
        wchar_t type = l[0], flags[8] = L"", skip[160] = L""; int k = 1, nf = 0;
        while (l[k] && l[k] != L' ' && l[k] != L'[') { if (nf < 7) flags[nf++] = l[k]; k++; }
        flags[nf] = 0;
        if (l[k] == L'[') {
            int ns = 0; k++;
            skip[ns++] = L',';
            while (l[k] && l[k] != L']' && ns < 156) skip[ns++] = l[k++];
            skip[ns++] = L','; skip[ns] = 0;
            if (l[k] == L']') k++;
        }
        addGroup(type, flags, skip, l + k + 1);
    }
    /* contractions: C = expand (contracted -> long), K = contract (long -> contracted) */
    for (int i = 0; CONTR[i]; i++) {
        const wchar_t *bar = wcschr(CONTR[i], L'|');
        wchar_t a[40], b[40]; int na = (int)(bar - CONTR[i]);
        wmemcpy(a, CONTR[i], na); a[na] = 0; wcscpy(b, bar + 1);
        wchar_t body[96];
        swprintf(body, 96, L"%ls > %ls", a, b); addGroup(L'C', NULL, NULL, body);
        swprintf(body, 96, L"%ls > %ls", b, a); addGroup(L'K', NULL, NULL, body);
    }
    taggerInit();
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
    /* plural possessive: "students' books" */
    if (j < n && isApos(t[j]) && j > s && towlower(t[j - 1]) == L's' && j + 2 < n && t[j + 1] == L' ' && iswalpha(t[j + 2])) {
        int open = 0; for (int q = j - 1; q >= 0 && t[q] != L'\n'; q--) { if (isApos(t[q]) && (q == 0 || !iswalpha(t[q - 1]))) { open = 1; break; } if (t[q] == L'.') break; }
        if (!open) j++;
    }
    return j;
}

static int commonLowerWord(const wchar_t *lw);
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
                    if (commonLowerWord(lw)) { k = we; continue; }
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
typedef struct { wchar_t *s; int type, chg, tag; } Tok;
typedef struct { Tok *t; int n, cap; } Toks;

static void tPush(Toks *v, int type, const wchar_t *s, int n, int chg) {
    if (v->n == v->cap) { v->cap = v->cap ? v->cap * 2 : 256; v->t = realloc(v->t, v->cap * sizeof(Tok)); }
    Tok k = { wndup(s, n), type, chg, -1 }; v->t[v->n++] = k;
}
static void tCopy(Toks *v, const Tok *k) { tPush(v, k->type, k->s, (int)wcslen(k->s), k->chg); v->t[v->n - 1].tag = k->tag; }
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
    if (k->type != TK_WORD && k->type != TK_LOCK) return 0;
    for (const wchar_t *p = k->s; *p; p++) if (!iswalpha(*p)) return 0;
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
static int lockHasDigit(const Tok *k);

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

typedef struct { int strength, strict, tone, type; const wchar_t *style; } NanoOpts;
/* tones: 0 natural, 1 casual, 2 formal, 3 academic, 4 simple, 5 friendly
   types: 0 any, 1 essay, 2 email, 3 story, 4 report, 5 social post */
enum { TN_NATURAL, TN_CASUAL, TN_FORMAL, TN_ACADEMIC, TN_SIMPLE, TN_FRIENDLY };
enum { WT_ANY, WT_ESSAY, WT_EMAIL, WT_STORY, WT_REPORT, WT_SOCIAL };
static int gTone, gType;
/* effective register: -1 plain/casual, 0 neutral, +1 formal */
static int toneReg(void) {
    if (gTone == TN_CASUAL || gTone == TN_SIMPLE || gTone == TN_FRIENDLY) return -1;
    if (gTone == TN_FORMAL || gTone == TN_ACADEMIC) return 1;
    if (gType == WT_SOCIAL || gType == WT_STORY || gType == WT_EMAIL) return -1;
    if (gType == WT_ESSAY || gType == WT_REPORT) return 1;
    return 0;
}

/* memory shared by all passes of one run, so nothing gets undone or redone */
typedef struct { wchar_t **k; int cap, n; } StrSet;
static int ssFind(StrSet *s, const wchar_t *k) {
    if (!s->cap) return -1;
    for (unsigned h = hashW(k) & (s->cap - 1);; h = (h + 1) & (s->cap - 1)) {
        if (!s->k[h]) return -(int)h - 2;
        if (!wcscmp(s->k[h], k)) return (int)h;
    }
}
static int ssHas(StrSet *s, const wchar_t *k) { return ssFind(s, k) >= 0; }
static void ssAdd(StrSet *s, const wchar_t *k) {
    if ((s->n + 1) * 2 > s->cap) {
        StrSet t = { calloc(s->cap ? s->cap * 2 : 1024, sizeof(wchar_t *)), s->cap ? s->cap * 2 : 1024, 0 };
        for (int i = 0; i < s->cap; i++) if (s->k[i]) { int r = ssFind(&t, s->k[i]); t.k[-r - 2] = s->k[i]; t.n++; }
        free(s->k); *s = t;
    }
    int r = ssFind(s, k);
    if (r >= 0) return;
    s->k[-r - 2] = wdup(k); s->n++;
}
static void ssFree(StrSet *s) { for (int i = 0; i < s->cap; i++) free(s->k[i]); free(s->k); memset(s, 0, sizeof *s); }

typedef struct { StrSet keys; int *val; } StrMap;
static void smSet(StrMap *m, const wchar_t *k, int v) {
    int oldCap = m->keys.cap;
    if ((m->keys.n + 1) * 2 > m->keys.cap) {
        StrSet t = { calloc(oldCap ? oldCap * 2 : 1024, sizeof(wchar_t *)), oldCap ? oldCap * 2 : 1024, 0 };
        int *tv = calloc(t.cap, sizeof(int));
        for (int i = 0; i < oldCap; i++) if (m->keys.k[i]) { int r = -ssFind(&t, m->keys.k[i]) - 2; t.k[r] = m->keys.k[i]; tv[r] = m->val[i]; t.n++; }
        free(m->keys.k); free(m->val); m->keys = t; m->val = tv;
    }
    int r = ssFind(&m->keys, k);
    if (r >= 0) return;
    r = -r - 2; m->keys.k[r] = wdup(k); m->val[r] = v; m->keys.n++;
}
static int smGet(StrMap *m, const wchar_t *k) { int r = ssFind(&m->keys, k); return r >= 0 ? m->val[r] : -1; }
static void smPut(StrMap *m, const wchar_t *k, int v) { int r = ssFind(&m->keys, k); if (r >= 0) m->val[r] = v; else smSet(m, k, v); }

typedef struct { StrSet retired, seen; StrMap origin; int contrMode, pass; } NanoCtx;
static NanoCtx *curCtx;

static void retire(const wchar_t *w) {
    if (!curCtx) return;
    int n = (int)wcslen(w); if (n > 150) return;
    wchar_t k[160]; lowerInto(w, n, k); ssAdd(&curCtx->retired, k);
}
static int isRetired(const wchar_t *w) {
    if (!curCtx) return 0;
    int n = (int)wcslen(w); if (n > 150) return 0;
    wchar_t k[160]; lowerInto(w, n, k); return ssHas(&curCtx->retired, k);
}
typedef struct { int words, changed, restructured, locks; } NanoStats;

/* ---------- part-of-speech tagger (bigram HMM + Viterbi) ---------- */

enum { T_N, T_NS, T_NP, T_PRON, T_DET, T_ADJ, T_ADV, T_VB, T_VBZ, T_VBD, T_VBN, T_VBG, T_MD, T_BE, T_HV, T_DO, T_ADP,
       T_CONJ, T_TO, T_NUM, T_PUNCT, T_PRT, T_EX, T_X };

typedef struct { char *w; unsigned char nt; unsigned char tag[6]; short sc[6]; } TagEnt;
static TagEnt *tw; static int ntw;
static TagEnt *tsx; static int ntsx;
typedef struct { char *f[4]; } VForm;
static VForm *vf; static int nvf;
static StrMap vfIdx;

static int tagCmp(const void *a, const void *b) { return strcmp(((const TagEnt *)a)->w, ((const TagEnt *)b)->w); }

static void parseTagLines(const char *src, TagEnt **arr, int *n) {
    const char *p = src;
    while (*p) {
        const char *tab = strchr(p, '\t'), *nl = strchr(p, '\n');
        if (!tab || !nl) break;
        TagEnt e; memset(&e, 0, sizeof e);
        e.w = malloc(tab - p + 1); memcpy(e.w, p, tab - p); e.w[tab - p] = 0;
        const char *q = tab + 1;
        while (q < nl && e.nt < 6) {
            int t = 0, s = 0, used = 0;
            if (sscanf(q, "%d:%d%n", &t, &s, &used) != 2) break;
            e.tag[e.nt] = (unsigned char)t; e.sc[e.nt] = (short)s; e.nt++;
            q += used; if (*q == ',') q++;
        }
        *arr = realloc(*arr, (*n + 1) * sizeof(TagEnt)); (*arr)[(*n)++] = e;
        p = nl + 1;
    }
}

static void taggerInit(void) {
    for (int i = 0; TAGWORDS[i]; i++) parseTagLines(TAGWORDS[i], &tw, &ntw);
    qsort(tw, ntw, sizeof(TagEnt), tagCmp);
    parseTagLines(TAGSUFFIX, &tsx, &ntsx);
    qsort(tsx, ntsx, sizeof(TagEnt), tagCmp);
    const char *p = VERBFORMS;
    while (*p) {
        const char *nl = strchr(p, '\n'); if (!nl) break;
        VForm v; int k = 0; const char *s = p;
        while (k < 4 && s <= nl) {
            const char *c = s; while (c < nl && *c != ':') c++;
            v.f[k] = malloc(c - s + 1); memcpy(v.f[k], s, c - s); v.f[k][c - s] = 0; k++;
            s = c + 1;
        }
        if (k == 4) {
            vf = realloc(vf, (nvf + 1) * sizeof(VForm)); vf[nvf] = v;
            for (int j = 3; j >= 0; j--) {
                wchar_t w[64]; mbstowcs(w, v.f[j], 63); w[63] = 0;
                smSet(&vfIdx, w, nvf);
            }
            nvf++;
        }
        p = nl + 1;
    }
}

static TagEnt *tagLookup(TagEnt *arr, int n, const char *w) {
    TagEnt key; key.w = (char *)w;
    return bsearch(&key, arr, n, sizeof(TagEnt), tagCmp);
}

/* emission scores (x100 log) for one token; returns count written into tags/sc */
static int emitFor(const Tok *k, int first, unsigned char *tags, short *sc) {
    if (k->type == TK_PUNCT) { tags[0] = T_PUNCT; sc[0] = 0; return 1; }
    if (k->type == TK_LOCK) {
        if (lockHasDigit(k)) { tags[0] = T_NUM; sc[0] = 0; return 1; }
        if (iswupper(k->s[0])) { tags[0] = T_NP; sc[0] = 0; return 1; }
        tags[0] = T_N; sc[0] = 0; return 1;
    }
    char w[48]; int n = 0;
    for (const wchar_t *p = k->s; *p && *p != L' ' && n < 46; p++) w[n++] = (char)(isApos(*p) ? '\'' : (*p < 128 ? towlower(*p) : 'x'));
    w[n] = 0;
    TagEnt *e = tagLookup(tw, ntw, w);
    if (e) { for (int i = 0; i < e->nt; i++) { tags[i] = e->tag[i]; sc[i] = e->sc[i]; } return e->nt; }
    int cnt = 0;
    for (int L = 3; L >= 1 && !cnt; L--) {
        if (n <= L) continue;
        TagEnt *s = tagLookup(tsx, ntsx, w + n - L);
        if (s) for (int i = 0; i < s->nt; i++) { tags[cnt] = s->tag[i]; sc[cnt] = s->sc[i]; cnt++; }
    }
    if (!cnt) { tags[0] = T_N; sc[0] = -900; cnt = 1; }
    if (iswupper(k->s[0]) && !first) {
        int f = -1; for (int i = 0; i < cnt; i++) if (tags[i] == T_NP) f = i;
        if (f < 0 && cnt < 6) { tags[cnt] = T_NP; sc[cnt] = -500; f = cnt++; }
        sc[f] += 200;
    }
    return cnt;
}

static void tagRange(Tok *t, int a, int b) {
    int idx[512], m = 0;
    for (int i = a; i < b && m < 512; i++) { t[i].tag = -1; if (t[i].type != TK_SPACE && t[i].type != TK_NL) idx[m++] = i; }
    if (!m) return;
    int (*V)[NTAG] = malloc(m * sizeof *V);
    signed char (*B)[NTAG] = malloc(m * sizeof *B);
    for (int i = 0; i < m; i++) for (int j = 0; j < NTAG; j++) { V[i][j] = -1000000000; B[i][j] = -1; }
    for (int i = 0; i < m; i++) {
        unsigned char tg[6]; short sc[6];
        int c = emitFor(&t[idx[i]], i == 0, tg, sc);
        for (int q = 0; q < c; q++) {
            int to = tg[q];
            if (i == 0) { int s = TRANS[0][to] + sc[q]; if (s > V[0][to]) { V[0][to] = s; B[0][to] = -1; } continue; }
            for (int fr = 0; fr < NTAG; fr++) {
                if (V[i - 1][fr] <= -1000000000) continue;
                int s = V[i - 1][fr] + TRANS[fr + 1][to] + sc[q];
                if (s > V[i][to]) { V[i][to] = s; B[i][to] = (signed char)fr; }
            }
        }
    }
    int best = 0, bs = -2000000000;
    for (int j = 0; j < NTAG; j++) if (V[m - 1][j] > -1000000000 && V[m - 1][j] + TRANS[j + 1][NTAG] > bs) { bs = V[m - 1][j] + TRANS[j + 1][NTAG]; best = j; }
    for (int i = m - 1; i >= 0; i--) { t[idx[i]].tag = best; if (i) best = B[i][best] < 0 ? 0 : B[i][best]; }
    free(V); free(B);
}

static void tagAll(Toks *v) {
    int s = 0;
    for (int i = 0; i <= v->n; i++) {
        int end = i == v->n || v->t[i].type == TK_NL ||
                  (isEndP(&v->t[i]) && (i + 1 >= v->n || v->t[i + 1].type != TK_WORD) && !(v->t[i].s[0] == L'.' && i > 0 && abbrevTok(&v->t[i - 1])));
        if (!end) continue;
        int e = i < v->n && v->t[i].type != TK_NL ? i + 1 : i;
        tagRange(v->t, s, e);
        s = e;
    }
}

static int isVerbTag(int t) { return t >= T_VB && t <= T_VBG; }

/* verb forms by any inflection: 0 base, 1 3sg, 2 past, 3 participle */
static VForm *verbOf(const wchar_t *w) {
    wchar_t k[64]; int n = (int)wcslen(w); if (n > 60) return NULL;
    lowerInto(w, n, k);
    int r = smGet(&vfIdx, k);
    return r >= 0 ? &vf[r] : NULL;
}

/* across presses: remember which swaps were picked, so the next press wanders somewhere else */
static StrMap pickHist;
static double moodW = 1, moodR = 1;
static int pickCount(const wchar_t *src, const wchar_t *tgt) {
    wchar_t k[200]; swprintf(k, 200, L"%ls\x01%ls", src, tgt);
    int r = ssFind(&pickHist.keys, k); return r >= 0 ? pickHist.val[r] : 0;
}
static void pickBump(const wchar_t *src, const wchar_t *tgt) {
    wchar_t k[200]; swprintf(k, 200, L"%ls\x01%ls", src, tgt);
    int r = ssFind(&pickHist.keys, k);
    if (r >= 0) pickHist.val[r]++; else smSet(&pickHist, k, 1);
}

static StrMap zipfMap;
/* how common a word or phrase is (zipf x10, ~30 = rare, ~60 = very common); a phrase counts as its rarest word */
static int zipfOf(const wchar_t *s) {
    static int loaded;
    if (!loaded) {
        loaded = 1;
    for (int b = 0; ZIPF[b]; b++)
        for (const char *p = ZIPF[b]; *p;) {
            const char *c = strchr(p, ':'), *nl = strchr(p, '\n'); if (!c || !nl) break;
            wchar_t w[40]; int L2 = (int)(c - p); if (L2 > 38) L2 = 38;
            for (int i = 0; i < L2; i++) w[i] = (wchar_t)p[i]; w[L2] = 0;
            smSet(&zipfMap, w, atoi(c + 1));
            p = nl + 1;
        }
    }
    int mn = 99, any = 0;
    const wchar_t *p = s;
    while (*p) {
        while (*p == L' ') p++;
        const wchar_t *e = p; while (*e && *e != L' ') e++;
        if (e > p) {
            wchar_t w[40]; int L2 = (int)(e - p); if (L2 > 38) L2 = 38;
            for (int i = 0; i < L2; i++) w[i] = isApos(p[i]) ? L'\'' : (wchar_t)towlower(p[i]); w[L2] = 0;
            int z = smGet(&zipfMap, w); if (z < 0) z = L2 <= 2 ? 70 : 25;
            if (z < mn) mn = z; any = 1;
        }
        p = e;
    }
    return any ? mn : 0;
}
static const wchar_t *AIVOCAB[] = { L"delve", L"tapestry", L"testament", L"intricate", L"multifaceted", L"pivotal", L"crucial", L"foster",
    L"leverage", L"seamless", L"robust", L"realm", L"navigate", L"underscore", L"showcase", L"vibrant", L"nuanced", L"comprehensive",
    L"embark", L"elevate", L"harness", L"resonate", L"profound", L"meticulous", L"paramount", L"holistic", L"synergy", L"enhance",
    L"invaluable", L"commendable", L"endeavor", L"captivating", L"bustling", L"beacon", L"interplay", L"transformative", L"utilize",
    L"facilitate", L"optimize", L"cornerstone", L"myriad", L"plethora", L"bolster", L"encompass", L"furthermore", L"moreover",
    L"additionally", L"notably", L"ultimately", L"essentially", L"innovative", L"dynamic", L"illuminate", L"poignant", NULL };
static int aiVocab(const wchar_t *s) {
    wchar_t w[48]; int k = 0; while (s[k] && s[k] != L' ' && k < 47) { w[k] = towlower(s[k]); k++; } w[k] = 0;
    for (int i = 0; AIVOCAB[i]; i++) { int L2 = (int)wcslen(AIVOCAB[i]); if (!wcsncmp(w, AIVOCAB[i], L2) && (int)wcslen(w) <= L2 + 3) return 1; }
    return 0;
}
/* ---- writing-style sample: the rewrite leans toward the sample's words, sentence length and contractions ---- */
typedef struct { int on, words, sents; double meanLen, contr100; StrMap vocab; } StyleProf;
static StyleProf gStyle;
static unsigned long long gStyleHash;
static void styleBuild(const wchar_t *t) {
    unsigned long long h = 1469598103934665603ULL; for (const wchar_t *p = t; *p; p++) h = (h ^ (unsigned)*p) * 1099511628211ULL;
    if (gStyle.on && h == gStyleHash) return;
    ssFree(&gStyle.vocab.keys); free(gStyle.vocab.val); memset(&gStyle, 0, sizeof gStyle);
    gStyleHash = h;
    int n = (int)wcslen(t), inQ = 0, sw = 0, contr = 0;
    for (int i = 0; i < n;) {
        wchar_t c = t[i];
        if (c == 0x201C || (c == L'"' && !inQ)) { inQ = 1; i++; continue; }
        if (c == 0x201D || (c == L'"' && inQ)) { inQ = 0; i++; continue; }
        if ((c == L'.' || c == L'!' || c == L'?') && !inQ) { if (sw > 2) { gStyle.sents++; } sw = 0; i++; continue; }
        if (!iswalpha(c)) { i++; continue; }
        int j = i; while (j < n && (iswalpha(t[j]) || (isApos(t[j]) && j + 1 < n && iswalpha(t[j + 1])))) j++;
        if (!inQ) {
            gStyle.words++; sw++;
            wchar_t w[40]; int L2 = j - i < 39 ? j - i : 39; lowerInto(t + i, L2, w);
            for (int q = 0; q < L2; q++) if (w[q] == L'\'') { contr++; break; }
            if (!(iswupper(t[i]) && i > 0 && t[i - 2] != L'.')) { int c0 = smGet(&gStyle.vocab, w); smPut(&gStyle.vocab, w, c0 < 0 ? 1 : c0 + 1); }
        }
        i = j;
    }
    if (gStyle.words < 40) return;
    gStyle.on = 1;
    gStyle.meanLen = gStyle.sents ? (double)gStyle.words / gStyle.sents : 15;
    gStyle.contr100 = 100.0 * contr / gStyle.words;
}
static int styleHas(const wchar_t *w) {
    if (!gStyle.on) return 0;
    wchar_t b[48]; int k = 0; while (w[k] && k < 47) { b[k] = isApos(w[k]) ? L'\'' : (wchar_t)towlower(w[k]); k++; } b[k] = 0;
    return smGet(&gStyle.vocab, b) > 0;
}
/* a person picks one word for a thing and sticks with it: the same source maps to the same choice for the whole run */
static StrMap gChoice;
/* collocations: how often two words sit side by side in ~11M words of human writing (counts below 4 are dropped) */
static StrMap bigMap;
static wchar_t gCtxPrev[40], gCtxNext[40];
static int bigramCount(const wchar_t *a, const wchar_t *b) {
    static int loaded;
    if (!loaded) {
        loaded = 1;
        for (int k = 0; BIGRAM[k]; k++)
            for (const char *p = BIGRAM[k]; *p;) {
                const char *c = strrchr(p, ':'), *nl = strchr(p, '\n'); if (!nl) break;
                c = p; const char *lc = NULL; while (c < nl) { if (*c == ':') lc = c; c++; }
                if (!lc) { p = nl + 1; continue; }
                wchar_t w[64]; int L2 = (int)(lc - p); if (L2 > 62) L2 = 62;
                for (int i = 0; i < L2; i++) w[i] = (wchar_t)(unsigned char)p[i]; w[L2] = 0;
                smSet(&bigMap, w, atoi(lc + 1));
                p = nl + 1;
            }
    }
    if (!a[0] || !b[0]) return -1;
    wchar_t k[96]; swprintf(k, 96, L"%ls %ls", a, b);
    int r = smGet(&bigMap, k); return r < 0 ? 0 : r;
}
static void edgeWords(const wchar_t *s, wchar_t *first, wchar_t *last) {
    int n = (int)wcslen(s), a = 0;
    while (a < n && s[a] != L' ') a++;
    int L2 = a < 39 ? a : 39; lowerInto(s, L2, first);
    int b = n; while (b > 0 && s[b - 1] != L' ') b--;
    L2 = n - b < 39 ? n - b : 39; lowerInto(s + b, L2, last);
}
/* how well a word fits between its neighbours (0 = no evidence) */
static double ctxFit(const wchar_t *w, int *left, int *right) {
    wchar_t f[40], l[40]; edgeWords(w, f, l);
    *left = gCtxPrev[0] ? bigramCount(gCtxPrev, f) : -1;
    *right = gCtxNext[0] ? bigramCount(l, gCtxNext) : -1;
    return log(1.0 + (*left > 0 ? *left : 0)) + log(1.0 + (*right > 0 ? *right : 0));
}

static int pickTarget(Grp *g, int not, int f) {
    int c[64], nc = 0, ff = f < nForms(g->type) ? f : 0;
    int zs = not >= 0 ? zipfOf(g->m[not].f[ff] ? g->m[not].f[ff] : g->m[not].f[0]) : 45;
    for (int i = 0; i < g->n && nc < 64; i++) {
        if (!g->m[i].tgt || i == not || isRetired(g->m[i].f[ff])) continue;
        const wchar_t *tw = g->m[i].f[ff] ? g->m[i].f[ff] : g->m[i].f[0];
        if (aiVocab(tw)) continue;
        int zt = zipfOf(tw);
        if (zt < 30 || zt + 5 < zs) continue;
        if (toneReg() < 0 && zt + 1 < zs) continue;
        if (gTone == TN_SIMPLE && zt < 45) continue;
        c[nc++] = i;
    }
    if (!nc) return -1;
    if (not >= 0 && (gCtxPrev[0] || gCtxNext[0])) {
        /* keep only substitutes that fit the neighbouring words about as well as the original */
        const wchar_t *sw = g->m[not].f[ff] ? g->m[not].f[ff] : g->m[not].f[0];
        int oL, oR; double of = ctxFit(sw, &oL, &oR);
        double fit[64], best = -1; int keep[64], nk = 0;
        for (int i = 0; i < nc; i++) {
            const wchar_t *tw = g->m[c[i]].f[ff] ? g->m[c[i]].f[ff] : g->m[c[i]].f[0];
            int cL, cR; fit[i] = ctxFit(tw, &cL, &cR);
            if ((oL >= 6 && cL == 0) || (oR >= 6 && cR == 0)) { fit[i] = -1; continue; }
            if (of >= 3.0 && fit[i] < of * 0.35) { fit[i] = -1; continue; }
            if (fit[i] > best) best = fit[i];
        }
        for (int i = 0; i < nc; i++) if (fit[i] >= 0 && fit[i] >= best - 1.2) keep[nk++] = c[i];
        if (!nk) return -1;
        for (int i = 0; i < nk; i++) c[i] = keep[i];
        nc = nk;
    }
    wchar_t ck[32]; swprintf(ck, 32, L"%d:%d:%d", (int)(g - grp), not, ff);
    { int prev = smGet(&gChoice, ck); if (prev >= 0) for (int i = 0; i < nc; i++) if (c[i] == prev) return prev; }
    if (gStyle.on) {
        int in[64], ni = 0;
        for (int i = 0; i < nc; i++) if (styleHas(g->m[c[i]].f[ff] ? g->m[c[i]].f[ff] : g->m[c[i]].f[0])) in[ni++] = c[i];
        if (ni) { int m = in[nri(ni)]; smSet(&gChoice, ck, m); return m; }
        if (not >= 0 && styleHas(g->m[not].f[ff] ? g->m[not].f[ff] : g->m[not].f[0]) && nrnd() < 0.7) return -1;
    }
    if (not < 0 || nrnd() < 0.15) { int m = c[nri(nc)]; smSet(&gChoice, ck, m); return m; }
    const wchar_t *src = g->m[not].f[0];
    int best = 1 << 30, cand[64], nb = 0;
    for (int i = 0; i < nc; i++) {
        int k = pickCount(src, g->m[c[i]].f[0]);
        if (k < best) { best = k; nb = 0; }
        if (k == best) cand[nb++] = c[i];
    }
    int m = cand[nri(nb)];
    pickBump(src, g->m[m].f[0]);
    smSet(&gChoice, ck, m);
    return m;
}

static int maxMode, escLevel;
/* text-level budget for "decorations" (framing openers, clefts, rare connectors) so many passes can't pile them up */
static int gSents, gOpeners, gClefts, gLconn, gWords, gFillUsed, gPrevOpener, gWhat, gThere, gAccord, gLets, gAfter, gPassBy;
static const wchar_t *OPENLIST[] = { L"in fact", L"indeed", L"as it turns out", L"in truth", L"as it happens", L"truth be told", L"in addition",
    L"additionally", L"moreover", L"furthermore", L"as a matter of fact", L"in reality", L"actually", L"also", L"besides", L"what's more",
    L"on top of that", L"in short", L"in other words", L"that said", L"still", L"even so", L"of course", L"naturally", NULL };
static int startsWithList(const wchar_t *p, const wchar_t **L) {
    for (int k = 0; L[k]; k++) {
        int n = (int)wcslen(L[k]), i = 0;
        for (; i < n; i++) if (towlower(p[i]) != L[k][i] && !(isApos(p[i]) && L[k][i] == L'\'')) break;
        if (i == n && !iswalpha(p[n])) return 1;
    }
    return 0;
}
static void textBudget(const wchar_t *t) {
    gSents = gOpeners = gClefts = gLconn = gWords = gFillUsed = gWhat = gThere = gAccord = gLets = gAfter = gPassBy = 0;
    int n = (int)wcslen(t), atStart = 1;
    for (int i = 0; i < n; i++) {
        if (atStart && iswalpha(t[i])) {
            atStart = 0;
            int e = i; while (e < n && t[e] != L'\n' && t[e] != L'.' && t[e] != L'!' && t[e] != L'?') e++;
            if (e < n && t[e] != L'\n') gSents++;
            if (startsWithList(t + i, OPENLIST)) gOpeners++;
            if (!wcsncmp(t + i, L"What ", 5)) gWhat++;
            if (!wcsncmp(t + i, L"There was ", 10) || !wcsncmp(t + i, L"There were ", 11) || !wcsncmp(t + i, L"There is ", 9) || !wcsncmp(t + i, L"There are ", 10)) gThere++;
            if (!wcsncmp(t + i, L"According to", 12)) gAccord++;
            if (!wcsncmp(t + i, L"Let's", 5) || !wcsncmp(t + i, L"Let\x2019s", 5)) gLets++;
            if (!wcsncmp(t + i, L"After ", 6)) gAfter++;
            { static const wchar_t *FL[] = { L"in fact", L"indeed", L"as it turns out", L"in truth", L"as it happens", L"truth be told", NULL };
              for (int q = 0; FL[q]; q++) { const wchar_t *one[2] = { FL[q], NULL }; if (startsWithList(t + i, one)) gFillUsed |= 1 << q; } }

        }
        if (iswalpha(t[i]) && (i == 0 || !iswalpha(t[i - 1]))) gWords++;
        if ((i == 0 || !iswalpha(t[i - 1])) && (!wcsncmp(t + i, L"It was ", 7) || !wcsncmp(t + i, L"It is ", 6) || !wcsncmp(t + i, L"it was ", 7) || !wcsncmp(t + i, L"it is ", 6))) {
            int e = i; while (e < n && t[e] != L'.' && t[e] != L',' && t[e] != L'\n') e++;
            for (int k = i + 6; k + 5 < e && k < i + 60; k++) if (!wcsncmp(t + k, L" that ", 6) || !wcsncmp(t + k, L" who ", 5)) { gClefts++; break; }
        }
        if (t[i] == L'.' || t[i] == L'!' || t[i] == L'?' || t[i] == L'\n') atStart = 1;
        else if (!iswspace(t[i]) && t[i] != L'"' && t[i] != 0x201C && t[i] != L'(' && !iswalpha(t[i])) {}
        if ((i == 0 || !iswalpha(t[i - 1])) && !wcsncmp(t + i, L"as well as", 10)) gLconn++;
        if (i > 1 && !wcsncmp(t + i, L" by ", 4) && (!wcsncmp(t + i - 2, L"ed", 2) || !wcsncmp(t + i - 2, L"en", 2) || !wcsncmp(t + i - 2, L"wn", 2) || !wcsncmp(t + i - 2, L"ht", 2))) gPassBy++;
    }
}

static int npEnd(Tok *t, int n, int k, int *plural, int *pron);
static void swapPass(Toks *v, const NanoOpts *o, NanoStats *st, int contrMode) {
    static const double PW[4] = { 0.35, 0.7, 0.95, 1.0 }, PP[4] = { 0.55, 0.85, 1.0, 1.0 }, PO[4] = { 0.5, 0.8, 1.0, 1.0 },
                        PC[4] = { 0.25, 0.7, 1.0, 1.0 }, PL[4] = { 0.08, 0.15, 0.25, 0.15 };
    int s = o->strength;
    tagAll(v);
    StrMap kc = {0};
    for (int i = 0; i < v->n; i++) if (v->t[i].type == TK_WORD && !wcschr(v->t[i].s, L' ')) {
        int L2 = (int)wcslen(v->t[i].s); if (L2 < 4 || L2 > 38) continue;
        wchar_t w[40]; lowerInto(v->t[i].s, L2, w); int c0 = smGet(&kc, w); smPut(&kc, w, c0 < 0 ? 1 : c0 + 1);
        if (L2 > 4 && w[L2 - 1] == L's') { w[L2 - 1] = 0; c0 = smGet(&kc, w); smPut(&kc, w, c0 < 0 ? 1 : c0 + 1); }
    }
    for (int i = 0; i < v->n; i++) {
        Tok *t = &v->t[i];
        if (t->type != TK_WORD || t->chg) continue;
        int endTok, nw;
        if (tokSentStart(v, i)) {
            Ent *e = matchAt(v, i, otab, &endTok, &nw);
            if (e && endTok < v->n && v->t[endTok].type == TK_PUNCT && v->t[endTok].s[0] == L',') {
                if (nrnd() < PO[s]) {
                    Grp *g = &grp[e->g]; int m = pickTarget(g, e->m, 0);
                    if (m >= 0) { retire(g->m[e->m].f[0]); replaceRange(v, i, endTok, casedCopy(v->t[i].s, g->m[m].f[0])); st->changed += nw; continue; }
                }
                i = endTok - 1;
                continue;
            }
        }
        Ent *e = matchAt(v, i, tab, &endTok, &nw);
        if (!e) continue;
        Ent eo = *e;
        int jump = nw > 1 ? endTok - 1 : i;
        if (nw == 1 && v->t[i].type == TK_WORD && !wcschr(v->t[i].s, L' ')) {
            int L2 = (int)wcslen(v->t[i].s);
            if (L2 >= 4 && L2 <= 38) { wchar_t w[40]; lowerInto(v->t[i].s, L2, w); if (smGet(&kc, w) >= 3) { i = jump; continue; }
                /* plurals and verb forms of a repeated word count too */
                if (L2 > 4 && w[L2 - 1] == L's') { w[L2 - 1] = 0; if (smGet(&kc, w) >= 2) { i = jump; continue; } } }
        }
        wchar_t mk[200] = L"";
        if (curCtx) {
            int p2 = 0;
            for (int q = i; q < endTok && p2 < 190; q++) {
                int L2 = (int)wcslen(v->t[q].s); if (p2 + L2 >= 199) break;
                if (v->t[q].type == TK_SPACE) mk[p2++] = L' '; else { lowerInto(v->t[q].s, L2, mk + p2); p2 += L2; }
            }
            mk[p2] = 0;
            int og = smGet(&curCtx->origin, mk);
            if (og >= 0 && og != e->g) {
                Grp *G = &grp[og]; int found = -1;
                for (int mm = 0; mm < G->n && found < 0; mm++)
                    for (int ff = 0; ff < nForms(G->type) && found < 0; ff++) {
                        wchar_t lk[160]; int L3 = (int)wcslen(G->m[mm].f[ff]); if (L3 > 150) { i = jump; continue; }
                        lowerInto(G->m[mm].f[ff], L3, lk);
                        if (!wcscmp(lk, mk)) { found = mm; eo.f = ff; }
                    }
                if (found < 0) { i = jump; continue; }
                eo.g = og; eo.m = found;
            }
        }
        e = &eo;
        Grp *g = &grp[e->g];
        wchar_t prev[40], next[40];
        prevWordL(v, i, prev); nextWordL(v, endTok, next);
        double p;
        switch (g->type) {
        case L'B': i = endTok - 1; continue;
        case L'C': if (contrMode != 1) { i = jump; continue; } p = PC[s]; break;
        case L'K': if (contrMode != 2 || !nextIsWord(v, endTok)) { i = jump; continue; } p = PC[s]; break;
        case L'P': p = PP[s]; break;
        case L'L': {
            int k = i - 1; while (k >= 0 && v->t[k].type == TK_SPACE) k--;
            int prevComma = k >= 0 && v->t[k].type == TK_PUNCT && v->t[k].s[0] == L',';
            int nk = endTok; while (nk < v->n && v->t[nk].type == TK_SPACE) nk++;
            if (wcschr(g->flags, L',') && !prevComma) { i = jump; continue; }
            if (wcschr(g->flags, L'A') && !prevComma && wcscmp(prev, L"and")) { i = jump; continue; }
            if (wcschr(g->flags, L'N') && !(nk < v->n && lockHasDigit(&v->t[nk]))) { i = jump; continue; }
            if (wcschr(g->flags, L'd') && !(nk < v->n && (v->t[nk].type == TK_LOCK || inList(DETS, next)))) { i = jump; continue; }
            if (wcschr(g->flags, L'd')) {
                int c2 = nk, words = 0, clause = 0;
                while (c2 < v->n && words < 4) {
                    if (v->t[c2].type == TK_PUNCT) break;
                    if (v->t[c2].type == TK_WORD) { words++; int tg = v->t[c2].tag; if (words > 1 && (isVerbTag(tg) || tg == T_BE || tg == T_HV || tg == T_MD)) { clause = 1; break; } }
                    c2++;
                }
                if (clause) { i = jump; continue; }
                /* the right side must be a plain noun phrase, not the start of a new clause */
                int pp2, qq2, ne = npEnd(v->t, v->n, nk, &pp2, &qq2);
                if (ne < 0) { i = jump; continue; }
                int af = ne; while (af < v->n && v->t[af].type == TK_SPACE) af++;
                if (af < v->n && (v->t[af].type == TK_LOCK || isVerbTag(v->t[af].tag) || v->t[af].tag == T_BE || v->t[af].tag == T_HV || v->t[af].tag == T_MD || v->t[af].tag == T_DO)) { i = jump; continue; }
                for (int z = af; z < v->n && !(v->t[z].type == TK_PUNCT && wcschr(L".!?;,", v->t[z].s[0])); z++) if (v->t[z].tag == T_BE || v->t[z].tag == T_VBZ || v->t[z].tag == T_VBD || v->t[z].tag == T_MD) { clause = 1; break; }
                if (clause) { i = jump; continue; }
            }
            if (wcschr(g->flags, L'X') && inList(QUANTPREV, prev)) { i = jump; continue; }
            if (wcschr(g->flags, L'w') && !nextIsWord(v, endTok)) { i = jump; continue; }
            p = gLconn * 150 >= gWords + 150 ? 0 : PL[s];
        } break;
        case L'V':
            if (inList(DETS, prev) || (e->f != 4 && inList(PREPS, prev))) { i = jump; continue; }
            p = PW[s]; break;
        case L'N':
            if (inList(VERBISH, prev) || inList(NOUNPREV, prev) || !wcscmp(next, L"to")) { i = jump; continue; }
            p = PW[s]; break;
        default: p = PW[s];
        }
        p *= moodW; if (p > 1) p = 1;
        if (wcschr(g->flags, L'!') && !wcscmp(next, L"to")) { i = jump; continue; }
        if (wcschr(g->flags, L'?') && inList(AFTERSET, next)) { i = jump; continue; }
        if (wcschr(g->flags, L'^') && inList(DETS, prev)) { i = jump; continue; }
        if (wcschr(g->flags, L'#') && inList(PARTICLES, next)) { i = jump; continue; }
        if (wcschr(g->flags, L'=') && inList(BEFORMS, prev)) { i = jump; continue; }
        if (wcschr(g->flags, L'P') && inList(PERSONAL, prev)) { i = jump; continue; }
        if (wcschr(g->flags, L'D') && !inList(DETS, prev)) { i = jump; continue; }
        if (g->skip && next[0]) { wchar_t pat[48]; swprintf(pat, 48, L",%ls,", next); if (wcsstr(g->skip, pat)) { i = jump; continue; } }
        if (nrnd() >= p) { i = jump; continue; }
        int f = e->f, tg = v->t[i].tag;
        if (tg >= 0) {
            if (g->type == L'V') {
                if (!isVerbTag(tg)) { i = jump; continue; }
                f = tg - T_VB;
                if (f == 3 || f == 2) {
                    int cue = 0, k = i - 1;
                    for (int q = 0; q < 3 && k >= 0; k--) {
                        if (v->t[k].type == TK_SPACE) continue;
                        if (v->t[k].type != TK_WORD) break;
                        wchar_t w[40]; lastWordLower(v->t[k].s, w, 40);
                        if (inList(PPCUE, w)) { cue = 1; break; }
                        q++;
                    }
                    int nk = endTok; while (nk < v->n && v->t[nk].type == TK_SPACE) nk++;
                    int obj = nk < v->n && (v->t[nk].tag == T_DET || v->t[nk].tag == T_PRON || v->t[nk].tag == T_ADJ || v->t[nk].tag == T_NUM);
                    if (f == 3 && !cue && obj) f = 2;
                    if (f == 2 && cue) f = 3;
                }
            }
            else if (g->type == L'N') { if (tg != T_N && tg != T_NS) { i = jump; continue; } f = tg == T_NS ? 1 : 0; }
            else if (g->type == L'J' && tg != T_ADJ) { i = jump; continue; }
            else if (g->type == L'R' && tg != T_ADV) { i = jump; continue; }
        } else if (g->type == L'J' || g->type == L'R') { i = jump; continue; }
        if (tg < 0 && g->type == L'V' && f == 2 && !wcscmp(g->m[e->m].f[2], g->m[e->m].f[3])) {
            wchar_t pp[40]; prevWordL(v, i, pp);
            int k = i - 1; while (k >= 0 && v->t[k].type == TK_SPACE) k--;
            if (inList(PPCUE, pp)) f = 3;
            else if (k >= 0 && isWordish(&v->t[k])) {
                int k2 = k - 1; while (k2 >= 0 && v->t[k2].type == TK_SPACE) k2--;
                if (k2 >= 0 && isWordish(&v->t[k2])) { lastWordLower(v->t[k2].s, pp, 40); if (inList(PPCUE, pp)) f = 3; }
            }
        }
        if (v->t[i].type == TK_WORD && !wcschr(v->t[i].s, L' ')) { int L2 = (int)wcslen(v->t[i].s); if (L2 >= 4 && L2 <= 38) { wchar_t w[40]; lowerInto(v->t[i].s, L2, w); if (smGet(&kc, w) >= 3) { i = jump; continue; } } }
        if ((g->type == L'J' || g->type == L'A') && nw == 1) {
            /* an adjective swap needs an adjective slot: "calm always returns" / "welcoming others" are not */
            int nq = endTok; while (nq < v->n && v->t[nq].type == TK_SPACE) nq++;
            if (v->t[i].tag == T_VBG || v->t[i].tag == T_N || v->t[i].tag == T_NS) { i = jump; continue; }
            if (nq < v->n && v->t[nq].type == TK_WORD && (v->t[nq].tag == T_ADV || v->t[nq].tag == T_VBZ || v->t[nq].tag == T_VBD || v->t[nq].tag == T_MD || v->t[nq].tag == T_PRON)) {
                int pq = i - 1; while (pq >= 0 && v->t[pq].type == TK_SPACE) pq--;
                if (!(pq >= 0 && (v->t[pq].tag == T_BE || v->t[pq].tag == T_ADV))) { i = jump; continue; }
            }
        }
        gCtxPrev[0] = gCtxNext[0] = 0;
        { int pq = i - 1; while (pq >= 0 && v->t[pq].type == TK_SPACE) pq--;
          if (pq >= 0 && v->t[pq].type == TK_WORD) { wchar_t f0[40]; edgeWords(v->t[pq].s, f0, gCtxPrev); }
          int nq = endTok; while (nq < v->n && v->t[nq].type == TK_SPACE) nq++;
          if (nq < v->n && v->t[nq].type == TK_WORD) { wchar_t l0[40]; edgeWords(v->t[nq].s, gCtxNext, l0); } }
        int m = pickTarget(g, e->m, f);
        gCtxPrev[0] = gCtxNext[0] = 0;
        if (m < 0) { i = jump; continue; }
        retire(g->m[e->m].f[f < nForms(g->type) ? f : 0]);
        const wchar_t *tgt = g->m[m].f[f < nForms(g->type) ? f : 0];
        wchar_t buf[128];
        if (g->type == L'C' && (!wcscmp(next, L"been") || !wcscmp(next, L"got") || !wcscmp(next, L"gotten"))) {
            int L2 = (int)wcslen(tgt);
            if (L2 > 3 && !wcscmp(tgt + L2 - 3, L" is") && L2 < 120) { wcscpy(buf, tgt); wcscpy(buf + L2 - 2, L"has"); tgt = buf; }
        }
        if (curCtx) {
            int og = smGet(&curCtx->origin, mk);
            int L3 = (int)wcslen(tgt);
            if (L3 < 150) { wchar_t tk[160]; lowerInto(tgt, L3, tk); smSet(&curCtx->origin, tk, og >= 0 ? og : e->g); }
        }
        replaceRange(v, i, endTok, casedCopy(v->t[i].s, tgt));
        st->changed += nw;
    }
    ssFree(&kc.keys); free(kc.val);
}

/* ---------- sentence restructuring ---------- */

static const wchar_t *FRONT[] = { L"even though", L"as soon as", L"as long as", L"so long as", L"because", L"since", L"although", L"though", L"if", L"when",
    L"while", L"whenever", L"unless", L"once", L"after", L"before", NULL };
static const wchar_t *TRAIL[] = { L"even though", L"as soon as", L"as long as", L"so long as", L"because", L"although", L"unless", L"whenever", L"since", NULL };
static const wchar_t *NOTBEFORE[] = { L"just", L"only", L"not", L"partly", L"simply", L"mainly", L"largely", L"and", L"or", L"but",
    L"even", L"ever", L"long", L"is", L"was", L"it's", L"that's", L"probably", NULL };
static const wchar_t *PPSTART[] = { L"in spite of", L"thanks to", L"because of", L"for", L"in", L"during", L"over", L"throughout",
    L"within", L"at", L"on", L"by", L"across", L"among", L"despite", L"under", L"through", L"around", L"beyond", L"until", L"without", NULL };
static const wchar_t *TIMEPREP[] = { L"during", L"throughout", L"until", L"within", L"after", L"before", NULL };
static const wchar_t *LOOSEPREP[] = { L"in", L"on", L"at", L"for", L"over", L"by", NULL };
static const wchar_t *TIMEWORDS[] = { L"week", L"weeks", L"year", L"years", L"month", L"months", L"day", L"days", L"morning", L"night",
    L"evening", L"afternoon", L"weekend", L"past", L"future", L"century", L"centuries", L"decade", L"decades", L"moment", L"summer",
    L"winter", L"spring", L"fall", L"autumn", L"time", L"times", L"hours", L"hour", L"minutes", L"end", L"beginning", L"start",
    L"history", L"ages", L"millennia", L"meantime", L"school", L"class", L"practice", L"lunch", L"breakfast", L"dinner", NULL };
static const wchar_t *REPORT[] = { L"suggest", L"suggests", L"suggested", L"show", L"shows", L"showed", L"say", L"says", L"said",
    L"believe", L"believes", L"believed", L"think", L"thinks", L"thought", L"claim", L"claims", L"claimed", L"argue", L"argues",
    L"argued", L"found", L"find", L"finds", L"reveal", L"reveals", L"revealed", L"indicate", L"indicates", L"indicated", L"prove",
    L"proves", L"proved", L"state", L"states", L"stated", L"report", L"reports", L"reported", L"insist", L"insists", L"insisted",
    L"admit", L"admits", L"admitted", L"feel", L"feels", L"felt", L"realize", L"realizes", L"realized", L"assume", L"assumes",
    L"assumed", L"predict", L"predicts", L"predicted", L"conclude", L"concludes", L"concluded", L"guess", L"bet", L"hope",
    L"hopes", L"hoped", L"noticed", L"notice", L"notices", L"explained", L"explains", L"agree", L"agrees", L"agreed", NULL };
static const wchar_t *SUBJ[] = { L"i", L"we", L"you", L"he", L"she", L"they", L"it", L"this", L"that", L"there", L"these", L"those", NULL };
static const wchar_t *CLAUSEDET[] = { L"the", L"a", L"an", L"our", L"their", L"his", L"her", L"my", L"your", L"its", L"some", L"many",
    L"most", L"all", L"every", L"no", L"each", L"both", L"people", L"nobody", L"everyone", L"someone", L"nothing", L"everything",
    L"scientists", L"researchers", L"experts", L"studies", L"students", L"teachers", L"humans", L"kids", L"adults", NULL };
static const wchar_t *AUXV[] = { L"is", L"are", L"was", L"were", L"has", L"have", L"had", L"will", L"would", L"can", L"could",
    L"should", L"may", L"might", L"must", L"do", L"does", L"did", L"isn't", L"aren't", L"wasn't", L"weren't", L"hasn't",
    L"haven't", L"won't", L"can't", L"doesn't", L"don't", L"didn't", L"it's", L"that's", L"there's", L"they're", L"we're",
    L"he's", L"she's", L"i'm", L"you're", L"cannot", L"becomes", L"became", L"seems", L"seemed", L"needs", L"makes", L"made",
    L"gets", L"got", L"keeps", L"kept", L"goes", L"went", L"takes", L"took", L"helps", L"causes", L"caused", NULL };
static const wchar_t *HOISTADV[] = { L"finally", L"eventually", L"suddenly", L"recently", L"gradually", L"quickly", L"slowly",
    L"quietly", L"carefully", L"immediately", L"ultimately", L"initially", L"originally", L"clearly", L"apparently",
    L"obviously", L"certainly", L"probably", L"surely", L"honestly", L"actually", L"happily", L"sadly", L"luckily",
    L"fortunately", L"unfortunately", L"secretly", L"gently", L"instantly", L"rapidly", L"soon", L"later", L"usually",
    L"sometimes", L"often", L"frequently", L"typically", L"generally", L"basically", L"essentially", NULL };
static const wchar_t *AUXH[] = { L"has", L"have", L"had", L"will", L"would", L"can", L"could", L"should", L"is", L"are", L"was",
    L"were", L"did", L"do", L"does", L"might", L"must", L"may", L"i've", L"we've", L"they've", L"you've", L"i'll", L"we'll",
    L"they'll", L"it's", L"he's", L"she's", L"they're", L"we're", L"i'm", L"you're", NULL };

typedef struct { Tok *t; int n; } Span;

static int spanWords(Span s) {
    int w = 0;
    for (int i = 0; i < s.n; i++) if (isWordish(&s.t[i])) {
        w++;
        for (wchar_t *p = s.t[i].s; *p; p++) if (*p == L' ') w++;
    }
    return w;
}
static Span trimSpan(Tok *t, int a, int b) {
    while (a < b && t[a].type == TK_SPACE) a++;
    while (b > a && t[b - 1].type == TK_SPACE) b--;
    Span s = { t + a, b - a }; return s;
}

static TagEnt *tagLookupFwd(const char *w);
static int lowerable(const Tok *k) {
    if (k->type == TK_LOCK) return 1;
    if (k->type != TK_WORD) return 0;
    wchar_t w[40]; firstWordLower(k->s, w, 40);
    if (!wcscmp(w, L"i") || !wcsncmp(w, L"i'", 2)) return 1;
    if (isKnownWord(w)) return 1;
    char nb[48]; int kk = 0; for (; w[kk] && kk < 47; kk++) { if (w[kk] > 127) return 0; nb[kk] = (char)w[kk]; } nb[kk] = 0;
    TagEnt *e = tagLookupFwd(nb);
    if (!e) return 0;
    int best = 0; for (int q = 1; q < e->nt; q++) if (e->sc[q] > e->sc[best]) best = q;
    return e->tag[best] != T_NP;
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
static void pushCap(Toks *o, const wchar_t *s) { wchar_t *c = wdup(s); c[0] = (wchar_t)towupper(c[0]); pushW(o, c, 1); free(c); }

static const wchar_t *conjVariant(const wchar_t *c) {
    if (!wcscmp(c, L"although") || !wcscmp(c, L"even though") || !wcscmp(c, L"though"))
        return nri(2) ? L"although" : L"even though";
    return c;
}

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

static void wordL(const Tok *k, wchar_t *o) { o[0] = 0; if (k->type == TK_WORD) firstWordLower(k->s, o, 40); }
static int isWordIn(const Tok *k, const wchar_t **list) { wchar_t w[40]; wordL(k, w); return w[0] && inList(list, w); }

/* single sentence, ends in . or !, only commas inside; returns comma count or -1 */
static int sentShape(Tok *t, int n) {
    if (n < 5) return -1;
    Tok *last = &t[n - 1];
    if (!(last->type == TK_PUNCT && (last->s[0] == L'.' || last->s[0] == L'!'))) return -1;
    int commas = 0;
    for (int i = 0; i < n - 1; i++) {
        if (t[i].type == TK_NL) return -1;
        if (t[i].type == TK_PUNCT) { if (t[i].s[0] == L',') commas++; else return -1; }
    }
    return commas;
}
static int firstComma(Tok *t, int n) { for (int i = 0; i < n; i++) if (t[i].type == TK_PUNCT && t[i].s[0] == L',') return i; return -1; }
static int lockHasDigit(const Tok *k) { if (k->type != TK_LOCK) return 0; for (wchar_t *p = k->s; *p; p++) if (iswdigit(*p)) return 1; return 0; }

/* "Because X, Y." -> "Y because X." */
static const wchar_t *PERSONN[] = { L"critic", L"critics", L"man", L"men", L"woman", L"women", L"person", L"people", L"student", L"students",
    L"teacher", L"teachers", L"scholar", L"scholars", L"friend", L"friends", L"worker", L"workers", L"boy", L"girl", L"child", L"children",
    L"kid", L"kids", L"doctor", L"doctors", L"writer", L"writers", L"reader", L"readers", L"guy", L"player", L"players", L"team", L"family",
    L"neighbor", L"stranger", L"officer", L"scientist", L"scientists", L"researcher", L"researchers", L"author", L"artist", L"poet",
    L"individual", L"individuals", L"persons", L"humans", L"human", L"employee", L"employees", L"pupil", L"pupils", L"learner", L"learners",
    L"citizen", L"citizens", L"customer", L"customers", L"user", L"users", L"parent", L"parents", L"member", L"members", L"expert", L"experts",
    L"educator", L"educators", L"staff", L"adult", L"adults", L"teen", L"teens", L"teenager", L"teenagers", L"folks", L"group", NULL };
static void pushWs(Toks *o, const wchar_t *s, int chg) {
    const wchar_t *p = s;
    while (*p) {
        const wchar_t *e = p; while (*e && *e != L' ') e++;
        tPush(o, TK_WORD, p, (int)(e - p), chg);
        if (*e) { pushSp(o); p = e + 1; } else p = e;
    }
}
static int nextNS(Tok *t, int n, int i);
static int wordIs(const Tok *k, const wchar_t *w) { wchar_t a[40]; if (k->type != TK_WORD || wcschr(k->s, L' ')) return 0; wordL(k, a); return !wcscmp(a, w); }
static int isFiniteTok(Tok *t, int i) {
    int tg = t[i].tag;
    if (t[i].type != TK_WORD) return 0;
    if (tg == T_VBD || tg == T_VBZ || tg == T_MD || tg == T_DO) return 1;
    if (tg == T_HV) return !wordIs(&t[i], L"having");
    if (tg == T_BE) return !(wordIs(&t[i], L"being") || wordIs(&t[i], L"been") || wordIs(&t[i], L"be"));
    if (tg == T_VB) { int p = i - 1; while (p >= 0 && t[p].type == TK_SPACE) p--; return !(p >= 0 && (t[p].tag == T_TO || t[p].tag == T_MD || wordIs(&t[p], L"to"))); }
    return 0;
}

static TagEnt *tagLookupFwd(const char *w);
static int isFiniteN(Tok *t, int n, int i) {
    if (isFiniteTok(t, i)) return 1;
    int tg = t[i].tag;
    if (t[i].type != TK_WORD) return 0;
    if (tg == T_NS || tg == T_N) {
        /* tagger slips like "Nye ends with", "no one outranks another": a noun-tagged word that can be a verb, right after the subject */
        int p = i - 1; while (p >= 0 && t[p].type == TK_SPACE) p--;
        int q = i + 1; while (q < n && t[q].type == TK_SPACE) q++; if (q >= n) return 0;
        if (p < 0 || !(t[p].tag == T_NP || t[p].tag == T_PRON || t[p].type == TK_LOCK || (t[p].tag == T_N && iswupper(t[p].s[0])) || wordIs(&t[p], L"one"))) return 0;
        if (t[q].type != TK_WORD || !(t[q].tag == T_ADP || t[q].tag == T_DET || t[q].tag == T_ADV || t[q].tag == T_PRON || t[q].tag == T_ADJ)) return 0;
        char nb[48]; int kk = 0; for (const wchar_t *c = t[i].s; *c && *c != L' ' && kk < 46; c++) { if (*c > 127) return 0; nb[kk++] = (char)towlower(*c); } nb[kk] = 0;
        TagEnt *e = tagLookupFwd(nb); if (!e) return tg == T_NS && kk > 3 && nb[kk - 1] == 's' && nb[kk - 2] != 's';
        for (int z = 0; z < e->nt; z++) if (e->tag[z] == (tg == T_NS ? T_VBZ : T_VBD)) return 1;
        if (tg == T_NS && kk > 3 && nb[kk - 1] == 's') {
            nb[kk - 1] = 0; TagEnt *b2 = tagLookupFwd(nb);
            if (!b2 && kk > 4 && nb[kk - 2] == 'e') { nb[kk - 2] = 0; b2 = tagLookupFwd(nb); }
            if (b2) for (int z = 0; z < b2->nt; z++) if (b2->tag[z] == T_VB) return 1;
        }
    }
    return 0;
}

static int patFrontSub(Tok *t, int n, Toks *o) {
    const wchar_t *cj;
    int commas = sentShape(t, n), cn = conjAt(t, n, 0, FRONT, &cj);
    if (commas != 1 || !cn) return 0;
    int comma = firstComma(t, n);
    Span clause = trimSpan(t, cn, comma), mainS = trimSpan(t, comma + 1, n - 1);
    if (spanWords(clause) < 2 || spanWords(mainS) < 2) return 0;
    wchar_t fw[40]; wordL(&mainS.t[0], fw); if (!wcscmp(fw, L"then")) return 0;
    pushSpan(o, mainS, 1); pushSp(o);
    pushW(o, conjVariant(cj), 1); pushSp(o);
    pushSpan(o, clause, 0);
    tCopy(o, &t[n - 1]);
    return 1;
}

/* "X because Y." -> "Because Y, x." */
static int patTrailSub(Tok *t, int n, Toks *o) {
    if (sentShape(t, n) != 0) return 0;
    const wchar_t *cj, *x;
    for (int i = 2; i < n - 3; i++) {
        if (t[i].type != TK_WORD || t[i - 1].type != TK_SPACE) continue;
        int k = conjAt(t, n, i, TRAIL, &cj);
        if (!k) continue;
        Span mainS = trimSpan(t, 0, i), clause = trimSpan(t, i + k, n - 1);
        if (spanWords(mainS) < 3 || spanWords(clause) < 3) return 0;
        int pk = i - 2; while (pk >= 0 && t[pk].type == TK_SPACE) pk--;
        if (pk >= 0 && t[pk].type == TK_WORD) { wchar_t pv[40]; lastWordLower(t[pk].s, pv, 40); if (inList(NOTBEFORE, pv)) return 0; }
        for (int q = 0; q < clause.n; q++) if (clause.t[q].type == TK_WORD && conjAt(clause.t, clause.n, q, FRONT, &x)) return 0;
        if (!lowerable(&mainS.t[0])) return 0;
        pushCap(o, conjVariant(cj)); pushSp(o);
        pushSpan(o, clause, 0);
        pushP(o, L','); pushSp(o);
        pushSpan(o, mainS, -1);
        tCopy(o, &t[n - 1]);
        return 1;
    }
    return 0;
}

/* "X, but Y." / "X, so Y." */
static int startsClause(Tok *k) { return k->type == TK_LOCK || isWordIn(k, SUBJ) || isWordIn(k, CLAUSEDET) || k->tag == T_NP; }
static int patButSo(Tok *t, int n, Toks *o) {
    if (sentShape(t, n) != 1) return 0;
    int comma = firstComma(t, n);
    int k = comma + 1; while (k < n && t[k].type == TK_SPACE) k++;
    if (k >= n || t[k].type != TK_WORD) return 0;
    wchar_t w[40]; wordL(&t[k], w);
    int isBut = !wcscmp(w, L"but"), isSo = !wcscmp(w, L"so");
    if (!isBut && !isSo) return 0;
    Span x = trimSpan(t, 0, comma), y = trimSpan(t, k + 1, n - 1);
    if (spanWords(x) < 3 || spanWords(y) < 3) return 0;
    wchar_t yw[40]; wordL(&y.t[0], yw);
    if (!wcscmp(yw, L"that") || !wcscmp(yw, L"far") || !wcscmp(yw, L"much") || !wcscmp(yw, L"not")) return 0;
    if (!startsClause(&y.t[0])) return 0;
    { int p3, q3, ye = npEnd(y.t, y.n, 0, &p3, &q3); if (ye < 0) return 0; int yv = nextNS(y.t, y.n, ye); if (yv >= y.n || !isFiniteTok(y.t, yv)) return 0; }
    int choice = nri(isSo ? 3 : 2);
    if (choice == 0 && !lowerable(&x.t[0])) choice = 1;
    if (choice == 0) {
        pushW(o, isBut ? (nri(2) ? L"Although" : L"Even though") : (nri(2) ? L"Because" : L"Since"), 1); pushSp(o);
        pushSpan(o, x, -1); pushP(o, L','); pushSp(o); pushSpan(o, y, 0);
    } else if (choice == 1) {
        static const wchar_t *BUTS[] = { L"However", L"Still", L"Even so" }, *SOS[] = { L"As a result", L"Because of this", L"For that reason" };
        pushSpan(o, x, 0); pushP(o, L'.'); pushSp(o);
        pushW(o, isBut ? BUTS[nri(3)] : SOS[nri(3)], 1); pushP(o, L','); pushSp(o);
        pushSpan(o, y, 0);
    } else {
        pushSpan(o, x, 0); pushP(o, L','); pushSp(o); pushW(o, L"which is why", 1); pushSp(o); pushSpan(o, y, 0);
    }
    tCopy(o, &t[n - 1]);
    return 1;
}

/* "For millennia, humans did X." -> "Humans did X for millennia." */
static int patPPBack(Tok *t, int n, Toks *o) {
    const wchar_t *cj;
    if (sentShape(t, n) < 1 || !conjAt(t, n, 0, PPSTART, &cj)) return 0;
    int comma = firstComma(t, n);
    Span head = trimSpan(t, 0, comma), rest = trimSpan(t, comma + 1, n - 1);
    int hw = spanWords(head);
    if (hw < 2 || hw > 8 || spanWords(rest) < 3) return 0;
    if (rest.t[0].type == TK_WORD) { wchar_t w[40]; wordL(&rest.t[0], w); if (!wcscmp(w, L"then") || !wcscmp(w, L"and") || !wcscmp(w, L"but")) return 0; }
    static const wchar_t *SUBWORDS[] = { L"what", L"that", L"which", L"who", L"because", L"when", L"where", L"how", L"why", L"if",
        L"while", L"whether", L"although", L"though", L"since", L"until", L"unless", NULL };
    for (int i = 0; i < rest.n; i++) if (rest.t[i].type == TK_PUNCT || isWordIn(&rest.t[i], SUBWORDS)) return 0;
    static const wchar_t *DISC[] = { L"for example", L"for instance", L"in my opinion", L"in my view", L"from my perspective", NULL };
    wchar_t hk[80] = L""; int hp = 0;
    for (int i = 0; i < head.n && hp < 70; i++) {
        if (head.t[i].type == TK_SPACE) { hk[hp++] = L' '; continue; }
        int L2 = (int)wcslen(head.t[i].s); if (hp + L2 >= 79) break;
        lowerInto(head.t[i].s, L2, hk + hp); hp += L2;
    }
    hk[hp] = 0;
    static const wchar_t *MOVEOK[] = { L"in the end", L"at first", L"in the beginning", L"at the time", L"at that point", L"in time", NULL };
    if (tFind(otab, hk) && !inList(DISC, hk) && !inList(MOVEOK, hk)) return 0;
    if (startsWithList(hk, OPENLIST) || startsWithList(hk, (const wchar_t *[]){ L"at last", L"by the way", NULL })) return 0;
    { wchar_t lw[40]; lastWordLower(head.t[head.n - 1].type == TK_WORD ? head.t[head.n - 1].s : L"", lw, 40);
      static const wchar_t *DEM[] = { L"this", L"that", L"these", L"those", L"it", NULL }; if (inList(DEM, lw)) return 0; }
    pushSpan(o, rest, 1);
    if (inList(DISC, hk)) pushP(o, L',');
    pushSp(o);
    pushSpan(o, head, -1);
    tCopy(o, &t[n - 1]);
    return 1;
}

/* "They kept working during the storm." -> "During the storm, they kept working." */
static int patPPFront(Tok *t, int n, Toks *o) {
    if (sentShape(t, n) != 0) return 0;
    const wchar_t *cj;
    for (int i = n - 3; i >= 3; i--) {
        if (t[i].type != TK_WORD || t[i - 1].type != TK_SPACE) continue;
        int tight = conjAt(t, n, i, TIMEPREP, &cj) != 0;
        int loose = !tight && conjAt(t, n, i, LOOSEPREP, &cj) != 0;
        if (!tight && !loose) continue;
        Span pp = trimSpan(t, i, n - 1), mainS = trimSpan(t, 0, i);
        int pw = spanWords(pp);
        if (pw < 2 || pw > 7) return 0;
        if (loose) {
            int ok = 0;
            for (int q = 1; q < pp.n && !ok; q++) {
                if (lockHasDigit(&pp.t[q])) ok = 1;
                else if (pp.t[q].type == TK_WORD) { wchar_t w[40]; lastWordLower(pp.t[q].s, w, 40); if (inList(TIMEWORDS, w)) ok = 1; }
            }
            if (!ok) continue;
        }
        if (spanWords(mainS) < 3 || !lowerable(&mainS.t[0])) return 0;
        pushSpan(o, pp, 1); pushP(o, L','); pushSp(o);
        pushSpan(o, mainS, -1);
        tCopy(o, &t[n - 1]);
        return 1;
    }
    return 0;
}

/* "Our findings suggest the Hi is X." -> "The Hi is X, our findings suggest." */
static int patReport(Tok *t, int n, Toks *o) {
    if (sentShape(t, n) < 0) return 0;
    for (int v = 1; v < n - 4; v++) {
        if (t[v].type == TK_PUNCT) return 0;
        if (!isWordIn(&t[v], REPORT) || t[v].chg) continue;
        Span subj = trimSpan(t, 0, v);
        int sw = spanWords(subj);
        if (sw < 1 || sw > 5) return 0;
        static const wchar_t *NEGMOD[] = { L"can't", L"cannot", L"can", L"could", L"couldn't", L"would", L"wouldn't", L"should",
            L"shouldn't", L"will", L"won't", L"might", L"may", L"must", L"don't", L"doesn't", L"didn't", L"not", L"never",
            L"do", L"does", L"did", L"to", L"had", L"has", L"have", L"is", L"was", L"are", L"were", L"be", L"been", NULL };
        for (int q = 0; q < subj.n; q++) if (isWordIn(&subj.t[q], NEGMOD)) return 0;
        { int q = v - 1; while (q >= 0 && t[q].type == TK_SPACE) q--; if (q >= 0 && t[q].type == TK_WORD) { wchar_t pw[40]; lastWordLower(t[q].s, pw, 40); if (inList(NEGMOD, pw)) return 0; } }
        Tok *s0 = &subj.t[0];
        if (!(s0->type == TK_LOCK || isWordIn(s0, SUBJ) || isWordIn(s0, CLAUSEDET))) return 0;
        int c = v + 1; while (c < n && t[c].type == TK_SPACE) c++;
        int hadThat = 0;
        if (c < n && t[c].type == TK_WORD) { wchar_t w[40]; wordL(&t[c], w); if (!wcscmp(w, L"that")) { hadThat = 1; c++; } }
        Span cl = trimSpan(t, c, n - 1);
        if (spanWords(cl) < 4) return 0;
        Tok *c0 = &cl.t[0];
        if (!(c0->type == TK_LOCK || isWordIn(c0, SUBJ) || isWordIn(c0, CLAUSEDET))) return 0;
        int aux = 0, fin = 0;
        for (int q = 1; q < cl.n; q++) {
            if (cl.t[q].type == TK_PUNCT) break;
            if (isWordIn(&cl.t[q], AUXV)) aux = 1;
            if (isFiniteTok(cl.t, q)) fin = 1;
        }
        if (!(fin && (aux || hadThat))) return 0;
        for (int q = 0; q + 2 < cl.n; q++) if (wordIs(&cl.t[q], L"and") || wordIs(&cl.t[q], L"but")) { int k3 = nextNS(cl.t, cl.n, q + 1); if (k3 < cl.n && isFiniteTok(cl.t, k3) && cl.t[k3].tag != T_VB) return 0; }
        for (int q = 0; q + 2 < cl.n; q++) if (wordIs(&cl.t[q], L"and") && wordIs(&cl.t[nextNS(cl.t, cl.n, q + 1)], L"that")) return 0;
        pushSpan(o, cl, 1); pushP(o, L','); pushSp(o);
        pushSpan(o, subj, -1); pushSp(o);
        tCopy(o, &t[v]);
        tCopy(o, &t[n - 1]);
        return 1;
    }
    return 0;
}

/* "They have finally found it." -> "Finally, they have found it." */
static int patHoist(Tok *t, int n, Toks *o) {
    if (sentShape(t, n) < 0) return 0;
    int k = 1; while (k < n && t[k].type == TK_SPACE) k++;
    if (k < n && t[k].type == TK_PUNCT) return 0;
    if (!lowerable(&t[0])) return 0;
    static const wchar_t *STOPAT[] = { L"that", L"which", L"who", L"because", L"although", L"though", L"when", L"if", L"while",
        L"since", L"where", L"and", L"but", L"or", L"so", L"until", L"unless", NULL };
    for (int a = 2; a < n - 2; a++) {
        if (t[a].type == TK_PUNCT || isWordIn(&t[a], STOPAT)) return 0;
        if (!isWordIn(&t[a], HOISTADV) || t[a].chg || t[a - 1].type != TK_SPACE || t[a + 1].type != TK_SPACE) continue;
        int p = a - 2; while (p >= 0 && t[p].type == TK_SPACE) p--;
        if (p < 0 || !isWordIn(&t[p], AUXH)) continue;
        if (t[a + 2].type != TK_WORD) continue;
        pushCap(o, t[a].s); pushP(o, L','); pushSp(o);
        Span before = { t, a - 1 }, after = { t + a + 1, n - a - 1 };
        pushSpan(o, before, -1);
        pushSpan(o, after, 0);
        return 1;
    }
    return 0;
}

/* "X, and they Y." -> "X. They Y." */
static int patSplit(Tok *t, int n, Toks *o) {
    if (sentShape(t, n) < 1) return 0;
    for (int c = 2; c < n - 4; c++) {
        if (!(t[c].type == TK_PUNCT && t[c].s[0] == L',')) continue;
        int k = c + 1; while (k < n && t[k].type == TK_SPACE) k++;
        wchar_t w[40]; if (k >= n) return 0; wordL(&t[k], w);
        if (wcscmp(w, L"and")) continue;
        int s = k + 1; while (s < n && t[s].type == TK_SPACE) s++;
        if (s >= n || !(isWordIn(&t[s], SUBJ) || t[s].type == TK_LOCK)) continue;
        if (isWordIn(&t[s], (const wchar_t *[]){ L"that", L"this", NULL })) { int s2 = nextNS(t, n, s + 1); if (s2 < n && (t[s2].tag == T_PRON || t[s2].tag == T_DET || t[s2].type == TK_LOCK || t[s2].tag == T_NP)) continue; }
        Span x = trimSpan(t, 0, c), y = trimSpan(t, s, n - 1);
        if (spanWords(x) < 3 || spanWords(y) < 3) continue;
        pushSpan(o, x, 0); pushP(o, L'.'); pushSp(o);
        int r = nri(3);
        (void)r; pushSpan(o, y, 1);
        tCopy(o, &t[n - 1]);
        return 1;
    }
    return 0;
}


/* ---------- active <-> passive (needs tags) ---------- */

static const wchar_t *POSSPRON[] = { L"his", L"her", L"my", L"your", L"our", L"their", L"its", NULL };
static const wchar_t *NOPASSIVE[] = { L"cost", L"weigh", L"last", L"become", L"seem", L"have", L"get", L"lack", L"resemble", L"fit",
    L"suit", L"mean", L"equal", L"total", L"measure", L"number", L"want", L"need", L"like", L"love", L"hate", L"say", L"tell",
    L"ask", L"go", L"come", L"take", L"make", L"let", L"help", L"feel", L"keep", L"hold", L"marry", L"reach", L"leave", L"enter",
    L"join", L"face", L"meet", L"own", L"contain", L"include", L"involve", L"consist", L"belong", L"remain", L"stay", L"do", L"be", NULL };

/* noun phrase starting at token k (non-space); returns index just past it, or -1 */
static int npNoOf;
static int npEnd(Tok *t, int n, int k, int *plural, int *pron) {
    *plural = 0; *pron = 0;
    if (k >= n) return -1;
    if (t[k].tag == T_PRON && !isWordIn(&t[k], POSSPRON)) {
        wchar_t w[40]; wordL(&t[k], w);
        static const wchar_t *OKP[] = { L"i", L"me", L"he", L"him", L"she", L"they", L"them", L"we", L"us", L"it", L"you", NULL };
        if (!inList(OKP, w)) return -1;
        *pron = 1; *plural = !wcscmp(w, L"they") || !wcscmp(w, L"them") || !wcscmp(w, L"we") || !wcscmp(w, L"us");
        return k + 1;
    }
    int i = k, last = -1, cnt = 0;
    if (t[i].tag == T_DET || isWordIn(&t[i], POSSPRON)) { i++; while (i < n && t[i].type == TK_SPACE) i++; }
    while (i < n && cnt < 7) {
        int tg = t[i].tag;
        int premod = 0;
        if ((tg == T_VBG || tg == T_VBN) && t[i].type == TK_WORD) {
            int j2 = i + 1; while (j2 < n && t[j2].type == TK_SPACE) j2++;
            premod = j2 < n && (t[j2].tag == T_N || t[j2].tag == T_NS || t[j2].tag == T_ADJ) && (i > k || t[k].tag == T_DET);
        }
        if (premod || tg == T_ADJ || tg == T_NUM || tg == T_N || tg == T_NS || tg == T_NP || (t[i].type == TK_LOCK && tg != T_PUNCT)) {
            if (tg == T_N || tg == T_NS || tg == T_NP || t[i].type == TK_LOCK) { last = i; *plural = tg == T_NS; }
            cnt++; i++;
            while (i < n && t[i].type == TK_SPACE) i++;
            continue;
        }
        break;
    }
    if (last < 0) return -1;
    int e = last + 1;
    int j = e; while (j < n && t[j].type == TK_SPACE) j++;
    if (!npNoOf && j < n && t[j].type == TK_WORD) {
        wchar_t w[40]; wordL(&t[j], w);
        if (!wcscmp(w, L"of")) {
            int k2 = j + 1; while (k2 < n && t[k2].type == TK_SPACE) k2++;
            int p2, q2, e2 = npEnd(t, n, k2, &p2, &q2);
            if (e2 > 0 && !q2) e = e2;
        }
    }
    return e;
}

static const wchar_t *swapPron(const wchar_t *w, int toObj) {
    static const wchar_t *S[] = { L"i", L"he", L"she", L"we", L"they", NULL }, *O[] = { L"me", L"him", L"her", L"us", L"them", NULL };
    const wchar_t **from = toObj ? S : O, **to = toObj ? O : S;
    for (int i = 0; from[i]; i++) if (!wcscmp(w, from[i])) return to[i];
    return NULL;
}

static void pushNP(Toks *o, Span np, int toObj, int caseMode) {
    if (np.n == 1 && np.t[0].type == TK_WORD) {
        wchar_t w[40]; wordL(&np.t[0], w);
        const wchar_t *r = swapPron(w, toObj);
        if (r) {
            wchar_t b[16]; wcscpy(b, r);
            if (!wcscmp(b, L"i")) b[0] = L'I';
            else if (caseMode > 0) b[0] = (wchar_t)towupper(b[0]);
            pushW(o, b, 1); return;
        }
    }
    pushSpan(o, np, caseMode);
}

static int nextNS(Tok *t, int n, int i) { while (i < n && t[i].type == TK_SPACE) i++; return i; }

/* "The factory built 500 cars in 2019." -> "500 cars were built by the factory in 2019." */
static int patPassive(Tok *t, int n, Toks *o) {
    if (maxMode && gPassBy >= 1 + gSents / 12) return 0;
    if (sentShape(t, n) != 0) return 0;
    int p1, q1, e1 = npEnd(t, n, 0, &p1, &q1);
    if (e1 < 0) return 0;
    int k = nextNS(t, n, e1), adv = -1;
    if (k < n && t[k].tag == T_ADV && t[k].type == TK_WORD) { adv = k; k = nextNS(t, n, k + 1); if (maxMode) return 0; }
    if (k >= n || t[k].type != TK_WORD) return 0;
    int vt = t[k].tag;
    if (!(vt == T_VBD || vt == T_VBZ || (vt == T_VB && (p1 || q1)))) return 0;
    VForm *vfm = verbOf(t[k].s);
    if (!vfm) return 0;
    wchar_t base[64]; mbstowcs(base, vfm->f[0], 63); base[63] = 0;
    if (inList(NOPASSIVE, base)) return 0;
    if (q1) { wchar_t w[40]; wordL(&t[0], w); if (!wcscmp(w, L"it") || !wcscmp(w, L"i") || !wcscmp(w, L"we") || !wcscmp(w, L"you")) return 0; }
    int s2 = nextNS(t, n, k + 1), p2, q2, e2 = npEnd(t, n, s2, &p2, &q2);
    if (e2 < 0) return 0;
    int r = nextNS(t, n, e2);
    if (r < n - 1 && !(t[r].tag == T_ADP && t[r].type == TK_WORD)) return 0;
    if (r < n - 1) { wchar_t w[40]; wordL(&t[r], w); if (!wcscmp(w, L"of") || !wcscmp(w, L"to") || !wcscmp(w, L"by")) return 0; }
    if (!lowerable(&t[0])) return 0;
    if (!(q2 || t[s2].type == TK_LOCK || isWordIn(&t[s2], (const wchar_t *[]){ L"the", L"this", L"that", L"these", L"those", L"his", L"her", L"their", L"its", L"my", L"our", L"your", NULL }))) return 0;
    Span np1 = trimSpan(t, 0, adv >= 0 ? adv : k), np2 = trimSpan(t, s2, e2), rest = trimSpan(t, e2, n - 1);
    if (spanWords(np1) > 7 || spanWords(np2) > 8) return 0;
    wchar_t pp[64]; mbstowcs(pp, vfm->f[3], 63); pp[63] = 0;
    pushNP(o, np2, 0, 1); pushSp(o);
    gPassBy++;
    pushW(o, vt == T_VBD ? (p2 ? L"were" : L"was") : (p2 ? L"are" : L"is"), 1); pushSp(o);
    if (adv >= 0) { tCopy(o, &t[adv]); pushSp(o); }
    pushW(o, pp, 1); pushSp(o); pushW(o, L"by", 1); pushSp(o);
    pushNP(o, np1, 1, -1);
    if (rest.n) { pushSp(o); pushSpan(o, rest, 0); }
    tCopy(o, &t[n - 1]);
    return 1;
}

/* "500 cars were built by the factory." -> "The factory built 500 cars." */
static int patActive(Tok *t, int n, Toks *o) {
    if (sentShape(t, n) != 0) return 0;
    int p2, q2, e2 = npEnd(t, n, 0, &p2, &q2);
    if (e2 < 0) return 0;
    int b = nextNS(t, n, e2);
    if (b >= n || t[b].type != TK_WORD) return 0;
    wchar_t bw[40]; wordL(&t[b], bw);
    int past = !wcscmp(bw, L"was") || !wcscmp(bw, L"were"), pres = !wcscmp(bw, L"is") || !wcscmp(bw, L"are");
    if (!past && !pres) return 0;
    int k = nextNS(t, n, b + 1), adv = -1;
    if (k < n && t[k].tag == T_ADV && t[k].type == TK_WORD) { adv = k; k = nextNS(t, n, k + 1); }
    if (k >= n || t[k].type != TK_WORD || t[k].tag != T_VBN) return 0;
    VForm *vfm = verbOf(t[k].s);
    if (!vfm) return 0;
    int by = nextNS(t, n, k + 1);
    if (by >= n || t[by].type != TK_WORD) return 0;
    wchar_t w[40]; wordL(&t[by], w); if (wcscmp(w, L"by")) return 0;
    int s1 = nextNS(t, n, by + 1), p1, q1, e1 = npEnd(t, n, s1, &p1, &q1);
    if (e1 < 0) return 0;
    int r = nextNS(t, n, e1);
    if (r < n - 1 && !(t[r].tag == T_ADP && t[r].type == TK_WORD)) return 0;
    if (!lowerable(&t[0])) return 0;
    Span np2 = trimSpan(t, 0, e2), np1 = trimSpan(t, s1, e1), rest = trimSpan(t, e1, n - 1);
    if (spanWords(np1) > 7 || spanWords(np2) > 8) return 0;
    int plural1 = p1;
    if (q1) { wchar_t x[40]; wordL(&np1.t[0], x); plural1 = !wcscmp(x, L"them") || !wcscmp(x, L"us") || !wcscmp(x, L"me") || !wcscmp(x, L"you"); }
    wchar_t vb[64]; mbstowcs(vb, vfm->f[past ? 2 : (plural1 ? 0 : 1)], 63); vb[63] = 0;
    pushNP(o, np1, 0, 1); pushSp(o);
    if (adv >= 0) { tCopy(o, &t[adv]); pushSp(o); }
    pushW(o, vb, 1); pushSp(o);
    pushNP(o, np2, 1, -1);
    if (rest.n) { pushSp(o); pushSpan(o, rest, 0); }
    tCopy(o, &t[n - 1]);
    return 1;
}

/* ---------- MAX-mode rebuilds ---------- */


/* "Because X, Y." / "Y because X." -> "X, so y." */
static int patCauseFlip(Tok *t, int n, Toks *o) {
    const wchar_t *cj;
    static const wchar_t *BEC[] = { L"because", NULL };
    int commas = sentShape(t, n);
    if (commas < 0) return 0;
    int cn = conjAt(t, n, 0, BEC, &cj);
    if (cn && commas == 1) {
        int comma = firstComma(t, n);
        Span x = trimSpan(t, cn, comma), y = trimSpan(t, comma + 1, n - 1);
        if (spanWords(x) < 3 || spanWords(y) < 3) return 0;
        pushSpan(o, x, 1); pushP(o, L','); pushSp(o); pushW(o, L"so", 1); pushSp(o); pushSpan(o, y, -1);
        tCopy(o, &t[n - 1]);
        return 1;
    }
    if (commas != 0) return 0;
    for (int i = 2; i < n - 3; i++) {
        if (t[i].type != TK_WORD || t[i - 1].type != TK_SPACE || !conjAt(t, n, i, BEC, &cj)) continue;
        int k = nextNS(t, n, i + 1);
        if (k < n && t[k].type == TK_WORD) { wchar_t w[40]; wordL(&t[k], w); if (!wcscmp(w, L"of")) return 0; }
        Span y = trimSpan(t, 0, i), x = trimSpan(t, i + 1, n - 1);
        if (spanWords(x) < 3 || spanWords(y) < 3 || !lowerable(&y.t[0])) return 0;
        Tok *x0 = &x.t[0];
        if (!(x0->type == TK_LOCK || isWordIn(x0, SUBJ) || isWordIn(x0, CLAUSEDET))) return 0;
        pushSpan(o, x, 1); pushP(o, L','); pushSp(o); pushW(o, L"so", 1); pushSp(o); pushSpan(o, y, -1);
        tCopy(o, &t[n - 1]);
        return 1;
    }
    return 0;
}

/* "Although X, Y." -> "X, but y." */
static int patConcession(Tok *t, int n, Toks *o) {
    const wchar_t *cj;
    static const wchar_t *ALT[] = { L"even though", L"although", L"though", NULL };
    if (sentShape(t, n) != 1) return 0;
    int cn = conjAt(t, n, 0, ALT, &cj);
    if (!cn) return 0;
    int comma = firstComma(t, n);
    Span x = trimSpan(t, cn, comma), y = trimSpan(t, comma + 1, n - 1);
    if (spanWords(x) < 3 || spanWords(y) < 3) return 0;
    pushSpan(o, x, 1); pushP(o, L','); pushSp(o); pushW(o, nri(3) ? L"but" : L"yet", 1); pushSp(o); pushSpan(o, y, -1);
    tCopy(o, &t[n - 1]);
    return 1;
}

/* "The storm destroyed the bridge." -> "It was the storm that destroyed the bridge." */
static int patCleft(Tok *t, int n, Toks *o) {
    if (!maxMode || sentShape(t, n) != 0 || gClefts >= 1 + gSents / 40) return 0;
    int p1, q1, e1 = npEnd(t, n, 0, &p1, &q1);
    if (e1 < 0 || q1) return 0;
    int k = nextNS(t, n, e1);
    if (k >= n || t[k].type != TK_WORD || !(t[k].tag == T_VBD || t[k].tag == T_VBZ)) return 0;
    Span np = trimSpan(t, 0, e1), rest = trimSpan(t, k, n - 1);
    if (spanWords(np) > 6 || spanWords(rest) < 3 || spanWords(rest) > 14 || !lowerable(&t[0]) || q1) return 0;
    for (int q = 0; q < rest.n; q++) if (isWordIn(&rest.t[q], (const wchar_t *[]){ L"because", L"since", L"although", L"though", L"when", L"if", L"while", L"so", NULL })) return 0;
    { int k2 = nextNS(t, n, k + 1); if (k2 < n && wordIs(&t[k2], L"that")) return 0; }
    for (int q = 0; q < rest.n; q++) if (rest.t[q].type == TK_LOCK && wcspbrk(rest.t[q].s, L".!?") && q + 1 < rest.n) return 0;
    pushWs(o, t[k].tag == T_VBD ? L"It was" : L"It is", 1); pushSp(o);
    gClefts++;
    int who = 0; for (int q = 0; q < np.n; q++) if (isWordIn(&np.t[q], PERSONN)) who = 1;
    if (np.n == 1 && np.t[0].type == TK_LOCK && iswupper(np.t[0].s[0]) && !wcschr(np.t[0].s, L' ') && !wcschr(np.t[0].s, L'"')) who = 1;
    pushSpan(o, np, -1); pushSp(o); pushW(o, who ? L"who" : L"that", 1); pushSp(o);
    pushSpan(o, rest, 0);
    tCopy(o, &t[n - 1]);
    return 1;
}

/* "They quickly left the room." -> "They left the room quickly." */
static int patAdvEnd(Tok *t, int n, Toks *o) {
    if (sentShape(t, n) != 0) return 0;
    Span all = { t, n };
    if (spanWords(all) > 14) return 0;
    for (int a = 2; a < n - 3; a++) {
        if (t[a].type != TK_WORD || t[a].tag != T_ADV || t[a].chg) continue;
        int L2 = (int)wcslen(t[a].s);
        if (L2 < 5 || wcscmp(t[a].s + L2 - 2, L"ly")) continue;
        int nx = nextNS(t, n, a + 1);
        if (nx >= n - 1 || !isVerbTag(t[nx].tag) || t[nx].tag == T_VBN) continue;
        { int pv = a - 1; while (pv >= 0 && t[pv].type == TK_SPACE) pv--; if (pv >= 0 && (t[pv].tag == T_BE || t[pv].tag == T_HV)) continue; }
        if (isWordIn(&t[a], (const wchar_t *[]){ L"really", L"truly", L"extremely", L"fully", L"completely", L"totally", L"highly", L"fairly", L"nearly", L"barely", L"hardly", L"only", L"simply", L"merely", NULL })) continue;
        { int sub = 0; for (int q = 0; q < n; q++) if (isWordIn(&t[q], (const wchar_t *[]){ L"because", L"although", L"when", L"while", L"if", L"since", L"that", L"which", L"who", NULL })) sub = 1; if (sub) return 0; }
        Span before = { t, a - 1 }, after = trimSpan(t, a + 1, n - 1);
        if (spanWords(after) < 2) return 0;
        pushSpan(o, before, 0); pushSp(o); pushSpan(o, after, 0); pushSp(o);
        tCopy(o, &t[a]); tCopy(o, &t[n - 1]);
        return 1;
    }
    return 0;
}

/* escalation 1: "X, which made it Y." -> "X. This made it Y." ; "X; Y." -> "X. Y." ; "X, and the Y." -> "X. The Y." */
static int patSplitMore(Tok *t, int n, Toks *o) {
    if ((escLevel < 1 && !(gType == WT_SOCIAL || gTone == TN_SIMPLE)) || n < 6) return 0;
    Tok *last = &t[n - 1];
    if (!(last->type == TK_PUNCT && (last->s[0] == L'.' || last->s[0] == L'!'))) return 0;
    for (int i = 0; i < n - 1; i++) {
        if (t[i].type == TK_NL) return 0;
        if (t[i].type != TK_PUNCT) continue;
        wchar_t c = t[i].s[0];
        if (c == L';') {
            Span x = trimSpan(t, 0, i), y = trimSpan(t, i + 1, n - 1);
            if (spanWords(x) < 3 || spanWords(y) < 3) return 0;
            pushSpan(o, x, 0); pushP(o, L'.'); pushSp(o); pushSpan(o, y, 1); tCopy(o, last);
            return 1;
        }
        if (c != L',') continue;
        int k = nextNS(t, n, i + 1);
        if (k >= n - 1 || t[k].type != TK_WORD) continue;
        wchar_t w[40]; wordL(&t[k], w);
        int k2 = nextNS(t, n, k + 1);
        if (!wcscmp(w, L"which") && k2 < n - 1 && (isVerbTag(t[k2].tag) || t[k2].tag == T_BE || t[k2].tag == T_HV || t[k2].tag == T_MD)) {
            Span x = trimSpan(t, 0, i), y = trimSpan(t, k2, n - 1);
            if (spanWords(x) < 3 || spanWords(y) < 2) continue;
            pushSpan(o, x, 0); pushP(o, L'.'); pushSp(o); pushW(o, L"This", 1); pushSp(o); pushSpan(o, y, 0); tCopy(o, last);
            return 1;
        }
        if ((!wcscmp(w, L"and") || !wcscmp(w, L"but")) && k2 < n - 1 && (t[k2].tag == T_DET || t[k2].tag == T_PRON || t[k2].type == TK_LOCK)) {
            Span x = trimSpan(t, 0, i), y = trimSpan(t, k2, n - 1);
            if (spanWords(x) < 3 || spanWords(y) < 3) continue;
            int hasVerb = 0, hasVerbX = 0;
            for (int q = 0; q < y.n; q++) { int tg = y.t[q].tag; if ((tg == T_VBD || tg == T_VBZ || tg == T_BE || tg == T_HV || tg == T_MD) && !isWordIn(&y.t[q], (const wchar_t *[]){ L"being", L"having", NULL })) hasVerb = 1; }
            for (int q = 0; q < x.n; q++) { int tg = x.t[q].tag; if (tg == T_VBD || tg == T_VBZ || tg == T_VB || tg == T_BE || tg == T_HV || tg == T_MD) hasVerbX = 1; }
            if (!hasVerb || !hasVerbX) continue;
            { int cc = 0; for (int q = 0; q < x.n; q++) if (x.t[q].type == TK_PUNCT && x.t[q].s[0] == L',') cc++; if (cc >= 2) continue; }
            if (isWordIn(&y.t[0], (const wchar_t *[]){ L"that", L"which", L"who", L"what", L"whether", L"how", NULL })) continue;
            { int p3, q3, ye = npEnd(y.t, y.n, 0, &p3, &q3); if (ye < 0) continue; int yv = nextNS(y.t, y.n, ye); if (yv >= y.n || !isFiniteTok(y.t, yv)) continue; }
            pushSpan(o, x, 0); pushP(o, L'.'); pushSp(o);
            if (!wcscmp(w, L"but")) { pushW(o, L"However", 1); pushP(o, L','); pushSp(o); pushSpan(o, y, -1); }
            else pushSpan(o, y, 1);
            tCopy(o, last);
            return 1;
        }
    }
    return 0;
}

/* escalation 2: one neutral framing opener per sentence */
static int patFiller(Tok *t, int n, Toks *o) {
    if (escLevel < 2 || sentShape(t, n) < 0 || !lowerable(&t[0])) return 0;
    return 0;
    static const wchar_t *FW[] = { L"indeed", L"truth", L"fact", L"happens", L"turns", L"occurs", NULL };
    for (int q = 0, seen = 0; q < n && seen < 6; q++) {
        if (t[q].type == TK_SPACE) continue;
        if (t[q].type == TK_PUNCT && t[q].s[0] == L',') return 0;
        seen++;
    }
    for (int q = 0; q < n; q++) if (t[q].type == TK_WORD) { wchar_t w[40]; lastWordLower(t[q].s, w, 40); if (inList(FW, w)) return 0; }
    int k = nextNS(t, n, 1);
    if (k < n && t[k].type == TK_PUNCT) return 0;
    for (int w = 1; w <= 4; w++) {
        wchar_t key[80] = L""; int p = 0, c = 0, q;
        for (q = 0; q < n && c < w; q++) {
            if (t[q].type == TK_SPACE) { if (p) key[p++] = L' '; continue; }
            if (t[q].type != TK_WORD) break;
            int L2 = (int)wcslen(t[q].s); if (p + L2 > 70) break;
            lowerInto(t[q].s, L2, key + p); p += L2; c++;
        }
        key[p] = 0;
        int nq = nextNS(t, n, q);
        if (c == w && tFind(otab, key) && nq < n && t[nq].type == TK_PUNCT) return 0;
    }
    static const wchar_t *FILL[] = { L"In fact", L"Indeed", L"In fact", L"Indeed", L"In fact", L"Indeed" };
    int pick = -1, cnt = 0;
    for (int q = 0; q < 2; q++) if (!(gFillUsed & (1 << q)) && nri(++cnt) == 0) pick = q;
    if (pick < 0) return 0;
    gFillUsed |= 1 << pick;
    pushW(o, FILL[pick], 1); pushP(o, L','); pushSp(o);
    gOpeners++;
    Span all = { t, n };
    pushSpan(o, all, -1);
    return 1;
}

/* escalation 3: reorder a plain noun list that ends a clause ("cats, dogs, and birds") */
static int patListShuffle(Tok *t, int n, Toks *o) {
    if (escLevel < 3 || sentShape(t, n) < 0) return 0;
    npNoOf = 1;
    for (int st = 0; st < n - 3; st++) {
        if (t[st].type == TK_SPACE) continue;
        if (st > 0) {
            int pv = st - 1; while (pv >= 0 && t[pv].type == TK_SPACE) pv--;
            if (pv < 0) continue;
            int ok = isVerbTag(t[pv].tag) || t[pv].tag == T_BE || t[pv].tag == T_HV || (t[pv].tag == T_ADP && !isWordIn(&t[pv], (const wchar_t *[]){ L"of", NULL }));
            if (!ok) continue;
        }
        int is[8], ie[8], ni = 0, k = st, oxford = 0, pl, pr;
        while (ni < 8) {
            int e = npEnd(t, n, k, &pl, &pr);
            if (e < 0 || pr) break;
            is[ni] = k; ie[ni] = e; ni++;
            int c = nextNS(t, n, e);
            if (c < n && t[c].type == TK_PUNCT && t[c].s[0] == L',') {
                int a = nextNS(t, n, c + 1);
                if (a < n && t[a].type == TK_WORD) { wchar_t w[40]; wordL(&t[a], w); if (!wcscmp(w, L"and")) { oxford = 1; k = nextNS(t, n, a + 1); continue; } }
                k = a; continue;
            }
            if (c < n && t[c].type == TK_WORD) { wchar_t w[40]; wordL(&t[c], w); if (!wcscmp(w, L"and")) { k = nextNS(t, n, c + 1); continue; } }
            break;
        }
        if (ni < 2) continue;
        { int lk = 0, hasAnd = 0;
          for (int q = is[0]; q < ie[ni - 1]; q++) { if (t[q].type == TK_LOCK) lk = 1; if (wordIs(&t[q], L"and") || wordIs(&t[q], L"or")) hasAnd = 1; }
          if (lk || !hasAnd) continue; }
        int after = nextNS(t, n, ie[ni - 1]);
        if (!(after == n - 1 || (after < n && t[after].type == TK_PUNCT && t[after].s[0] == L','))) continue;
        if (after < n - 1) { int a2 = nextNS(t, n, after + 1); if (a2 < n && (wordIs(&t[a2], L"and") || wordIs(&t[a2], L"or"))) continue; }
        int perm[8]; for (int i = 0; i < ni; i++) perm[i] = i;
        for (int tries = 0; tries < 10; tries++) {
            for (int i = ni - 1; i > 0; i--) { int j = nri(i + 1), x = perm[i]; perm[i] = perm[j]; perm[j] = x; }
            int same = 1; for (int i = 0; i < ni; i++) if (perm[i] != i) same = 0;
            if (!same) break;
        }
        int same = 1; for (int i = 0; i < ni; i++) if (perm[i] != i) same = 0;
        if (same) continue;
        if (st == 0 && !lowerable(&t[0])) continue;
        for (int i = 0; i < st; i++) tCopy(o, &t[i]);
        for (int i = 0; i < ni; i++) {
            Span it = { t + is[perm[i]], ie[perm[i]] - is[perm[i]] };
            pushSpan(o, it, st == 0 ? (i == 0 ? 1 : -1) : 0);
            if (i < ni - 1) {
                if (ni > 2 || oxford) pushP(o, L',');
                pushSp(o);
                if (i == ni - 2) { pushW(o, L"and", 0); pushSp(o); }
            }
        }
        for (int i = ie[ni - 1]; i < n; i++) tCopy(o, &t[i]);
        npNoOf = 0;
        return 1;
    }
    npNoOf = 0;
    return 0;
}


/* ---------- MAX rebuild patterns: whole-sentence reconstructions ---------- */


static void mkIngB(const char *base, wchar_t *out) {
    wchar_t b[64]; mbstowcs(b, base, 60); b[60] = 0;
    int n = (int)wcslen(b);
    if (n > 2 && !wcscmp(b + n - 2, L"ie")) { b[n - 2] = 0; swprintf(out, 64, L"%lsying", b); return; }
    if (n > 2 && b[n - 1] == L'e' && !wcschr(L"eoy", b[n - 2])) { b[n - 1] = 0; swprintf(out, 64, L"%lsing", b); return; }
    int V = wcschr(L"aeiou", b[n - 1]) != NULL;
    if (n >= 3 && n <= 4 && !V && !wcschr(L"wxy", b[n - 1]) && wcschr(L"aeiou", b[n - 2]) && !wcschr(L"aeiou", b[n - 3]))
        { swprintf(out, 64, L"%ls%lcing", b, b[n - 1]); return; }
    swprintf(out, 64, L"%lsing", b);
}

/* "Note the absence." -> "Let's note the absence." / "It is worth noting the absence." */
static int budgetOK(int used, int per) { return used < 1 + gSents / per; }
static int patImperative(Tok *t, int n, Toks *o) {
    if (!budgetOK(gLets, 15)) return 0;
    if (sentShape(t, n) != 0 || t[0].type != TK_WORD || wcschr(t[0].s, L' ') || !lowerable(&t[0])) return 0;
    VForm *vf = verbOf(t[0].s); if (!vf) return 0;
    wchar_t b[64], w[40]; mbstowcs(b, vf->f[0], 63); b[63] = 0; wordL(&t[0], w);
    if (wcscmp(b, w) || t[0].tag != T_VB) return 0;
    static const wchar_t *NO[] = { L"be", L"let", L"please", L"thank", L"have", L"do", L"go", L"get", L"come", L"see", NULL };
    if (inList(NO, w)) return 0;
    int k = nextNS(t, n, 1); if (k >= n - 1 || isWordIn(&t[k], SUBJ) || t[k].tag == T_VBD || t[k].tag == T_VBZ) return 0;
    Span all = { t, n - 1 };
    pushW(o, L"Let's", 1); pushSp(o); pushSpan(o, all, -1); gLets++;
    tCopy(o, &t[n - 1]);
    return 1;
}

/* "I would like an extension." -> "What I would like is an extension." ; "Some say he wept." -> "What some say is that he wept." */
static int patPseudoCleft(Tok *t, int n, Toks *o) {
    if (!budgetOK(gWhat, 40)) return 0;
    if (sentShape(t, n) < 0 || !lowerable(&t[0])) return 0;
    static const wchar_t *OBJV[] = { L"like", L"want", L"need", L"love", L"hate", L"prefer", L"enjoy", NULL };
    static const wchar_t *CLV[] = { L"believe", L"think", L"say", L"know", L"suggest", L"mean", L"argue", L"claim", L"feel", L"hope", L"expect", L"remember", L"realize", NULL };
    int p1, q1, e = npEnd(t, n, 0, &p1, &q1);
    if (e < 0 && (isWordIn(&t[0], (const wchar_t *[]){ L"some", L"others", L"many", L"most", NULL }))) e = 1;
    if (e < 0) return 0;
    if (isWordIn(&t[0], (const wchar_t *[]){ L"it", L"there", L"this", L"that", L"what", NULL })) return 0;
    Span subj = trimSpan(t, 0, e); if (spanWords(subj) > 8) return 0;
    for (int q = 0; q < subj.n; q++) if (subj.t[q].type == TK_PUNCT) return 0;
    int k = nextNS(t, n, e), md = -1;
    if (k < n && (t[k].tag == T_MD || t[k].tag == T_DO)) { if (isWordIn(&t[k], (const wchar_t *[]){ L"don't", L"doesn't", L"didn't", L"won't", L"can't", L"wouldn't", NULL })) return 0; md = k; k = nextNS(t, n, k + 1); }
    if (k >= n || t[k].type != TK_WORD || wcschr(t[k].s, L' ')) return 0;
    VForm *vf = verbOf(t[k].s); if (!vf) return 0;
    wchar_t b[64]; mbstowcs(b, vf->f[0], 63); b[63] = 0;
    int ob = inList(OBJV, b), cl = inList(CLV, b);
    if (!ob && !cl) return 0;
    if (!(t[k].tag == T_VB || t[k].tag == T_VBZ || t[k].tag == T_VBD)) return 0;
    int r = nextNS(t, n, k + 1);
    if (r >= n - 1) return 0;
    Span rest = trimSpan(t, r, n - 1);
    if (spanWords(rest) < 2) return 0;
    int hasThat = wordIs(&t[r], L"that");
    if (cl && !hasThat && !startsClause(&t[r]) && !wordIs(&t[r], L"to")) return 0;
    if (ob && (wordIs(&t[r], L"it") || wordIs(&t[r], L"them") || wordIs(&t[r], L"him") || wordIs(&t[r], L"her"))) return 0;
    for (int q = 0; q < rest.n; q++) if (rest.t[q].type == TK_PUNCT && rest.t[q].s[0] != L',') return 0;
    int past = t[k].tag == T_VBD || (md >= 0 && wordIs(&t[md], L"did"));
    pushW(o, L"What", 1); pushSp(o); pushSpan(o, subj, -1); pushSp(o); gWhat++;
    if (md >= 0) { tCopy(o, &t[md]); pushSp(o); }
    tCopy(o, &t[k]); pushSp(o);
    pushW(o, past ? L"was" : L"is", 1); pushSp(o);
    if (cl && !hasThat && !wordIs(&t[r], L"to")) { pushW(o, L"that", 1); pushSp(o); }
    pushSpan(o, rest, 0);
    tCopy(o, &t[n - 1]);
    return 1;
}

/* "A critic in New York wrote that ..." -> "There was a critic in New York who wrote that ..." */
static int patThereWas(Tok *t, int n, Toks *o) {
    if (!budgetOK(gThere, 15)) return 0;
    if (sentShape(t, n) != 0) return 0;
    static const wchar_t *INDEF[] = { L"a", L"an", L"some", L"several", L"many", L"two", L"three", L"four", L"few", NULL };
    if (!isWordIn(&t[0], INDEF)) return 0;
    int p1, q1, e = npEnd(t, n, 0, &p1, &q1);
    if (e < 0) return 0;
    int j = e; while (j < n && t[j].type == TK_SPACE) j++;
    /* allow a short trailing prepositional phrase on the subject: "a critic in New York" */
    if (j < n && t[j].tag == T_ADP && !wordIs(&t[j], L"of")) { int s2 = nextNS(t, n, j + 1), p2, q2, e2 = npEnd(t, n, s2, &p2, &q2); if (e2 > 0) e = e2; }
    int k = nextNS(t, n, e);
    if (k >= n - 1 || !(t[k].tag == T_VBD || t[k].tag == T_VBZ || (t[k].tag == T_VB && p1))) return 0;
    if (t[k].tag == T_BE || t[k].tag == T_HV) return 0;
    Span np = trimSpan(t, 0, e), rest = trimSpan(t, k, n - 1);
    if (spanWords(np) > 9 || spanWords(rest) < 2) return 0;
    for (int q = 0; q < np.n; q++) if (np.t[q].type == TK_PUNCT) return 0;
    int person = 0; for (int q = 0; q < np.n; q++) if (isWordIn(&np.t[q], PERSONN)) person = 1;
    const wchar_t *be = t[k].tag == T_VBD ? (p1 ? L"were" : L"was") : (p1 ? L"are" : L"is");
    gThere++;
    pushW(o, L"There", 1); pushSp(o); pushW(o, be, 1); pushSp(o); pushSpan(o, np, -1); pushSp(o);
    pushW(o, person ? L"who" : L"that", 1); pushSp(o); pushSpan(o, rest, 0);
    tCopy(o, &t[n - 1]);
    return 1;
}

/* "The poem also follows a meter." -> "The poem follows a meter as well." */
static int patAlsoEnd(Tok *t, int n, Toks *o) {
    if (sentShape(t, n) < 0) return 0;
    Span all = { t, n }; if (spanWords(all) > 22) return 0;
    for (int a = 2; a < n - 3; a++) {
        if (!wordIs(&t[a], L"also")) continue;
        int nx = nextNS(t, n, a + 1);
        if (nx >= n - 1 || !(isVerbTag(t[nx].tag) || t[nx].tag == T_BE || t[nx].tag == T_HV)) return 0;
        int pv = a - 1; while (pv >= 0 && t[pv].type == TK_SPACE) pv--;
        if (pv < 0 || t[pv].type == TK_PUNCT) return 0;
        for (int i = 0; i < a; i++) tCopy(o, &t[i]);
        int i = a + 1; while (i < n && t[i].type == TK_SPACE) i++;
        for (; i < n - 1; i++) tCopy(o, &t[i]);
        if (nri(2)) { pushSp(o); pushWs(o, L"as well", 1); } else { pushP(o, L','); pushSp(o); pushW(o, L"too", 1); }
        tCopy(o, &t[n - 1]);
        return 1;
    }
    return 0;
}

/* perfect / modal passives: "Some scholars have spent careers on this." -> "Careers have been spent on this by some scholars."
   "You will feel it." -> "It will be felt." */
static int patAuxPassive(Tok *t, int n, Toks *o) {
    if (!budgetOK(gPassBy, 12)) return 0;
    if (sentShape(t, n) < 0) return 0;
    int p1, q1, e1 = npEnd(t, n, 0, &p1, &q1);
    if (e1 < 0 || !lowerable(&t[0])) return 0;
    int k = nextNS(t, n, e1);
    if (k >= n || !(t[k].tag == T_HV || t[k].tag == T_MD)) return 0;
    if (isWordIn(&t[k], (const wchar_t *[]){ L"won't", L"can't", L"wouldn't", L"couldn't", L"shouldn't", L"hasn't", L"haven't", L"hadn't", L"having", NULL })) return 0;
    int v = nextNS(t, n, k + 1);
    if (v >= n || t[v].type != TK_WORD || wcschr(t[v].s, L' ')) return 0;
    int modal = t[k].tag == T_MD;
    if (modal ? t[v].tag != T_VB : t[v].tag != T_VBN) return 0;
    VForm *vf = verbOf(t[v].s); if (!vf) return 0;
    wchar_t b[64]; mbstowcs(b, vf->f[0], 63); b[63] = 0;
    wchar_t sw0[40]; wordL(&t[0], sw0);
    int genericYou = q1 && !wcscmp(sw0, L"you") && modal;
    if ((inList(NOPASSIVE, b) && !(genericYou && wcscmp(b, L"have") && wcscmp(b, L"get") && wcscmp(b, L"go") && wcscmp(b, L"come") && wcscmp(b, L"be")))
        || !wcscmp(b, L"be") || !wcscmp(b, L"say") || !wcscmp(b, L"like")) return 0;
    int s2 = nextNS(t, n, v + 1), p2, q2, e2 = npEnd(t, n, s2, &p2, &q2);
    if (e2 < 0) return 0;
    int r = nextNS(t, n, e2);
    if (r < n - 1 && !(t[r].tag == T_ADP && t[r].type == TK_WORD) && !(t[r].tag == T_ADV)) return 0;
    if (r < n - 1 && (wordIs(&t[r], L"of") || wordIs(&t[r], L"to") || wordIs(&t[r], L"by"))) return 0;
    for (int q = r; q < n - 1; q++) if (t[q].type == TK_PUNCT) return 0;
    wchar_t sw[40]; wordL(&t[0], sw);
    int generic = q1 && !wcscmp(sw, L"you") && modal;
    if (q1 && !generic && (!wcscmp(sw, L"i") || !wcscmp(sw, L"we") || !wcscmp(sw, L"you") || !wcscmp(sw, L"it"))) return 0;
    Span np1 = trimSpan(t, 0, e1), np2 = trimSpan(t, s2, e2), rest = trimSpan(t, e2, n - 1);
    if (spanWords(np1) > 7 || spanWords(np2) > 8) return 0;
    if (!(q2 || t[s2].type == TK_LOCK || isWordIn(&t[s2], (const wchar_t *[]){ L"the", L"this", L"that", L"these", L"those", L"his", L"her", L"their", L"its", L"my", L"our", L"your", L"entire", L"whole", L"all", NULL }))) return 0;
    wchar_t pp[64]; mbstowcs(pp, vf->f[3], 63); pp[63] = 0;
    pushNP(o, np2, 0, 1); pushSp(o);
    if (modal) { tCopy(o, &t[k]); pushSp(o); pushW(o, L"be", 1); }
    else {
        wchar_t hv[40]; wordL(&t[k], hv);
        pushW(o, !wcscmp(hv, L"had") ? L"had" : (p2 || (q2 && !wcscmp(np2.t[0].s, L"them"))) ? L"have" : L"has", 1); pushSp(o); pushW(o, L"been", 1);
    }
    pushSp(o); pushW(o, pp, 1); gPassBy++;
    if (rest.n) { pushSp(o); pushSpan(o, rest, 0); }
    if (!generic) { pushSp(o); pushW(o, L"by", 1); pushSp(o); pushNP(o, np1, 1, -1); }
    tCopy(o, &t[n - 1]);
    return 1;
}

/* negation rephrasing: "not well received" -> "poorly received", "did not have much X" -> "had little X",
   "never wrote another X" -> "wrote no other X", "is not happy" -> "is unhappy" */
static int patNegation(Tok *t, int n, Toks *o) {
    if (sentShape(t, n) < 0) return 0;
    for (int i = 0; i < n - 3; i++) {
        if (t[i].type != TK_WORD || wcschr(t[i].s, L' ')) continue;
        int a = nextNS(t, n, i + 1), b = a < n ? nextNS(t, n, a + 1) : n, c = b < n ? nextNS(t, n, b + 1) : n;
        if (wordIs(&t[i], L"not") && a < n && wordIs(&t[a], L"well") && b < n && t[b].tag == T_VBN) {
            for (int q = 0; q < i; q++) tCopy(o, &t[q]);
            pushW(o, L"poorly", 1);
            for (int q = a + 1; q < n; q++) tCopy(o, &t[q]);
            return 1;
        }
        if ((wordIs(&t[i], L"did") && a < n && wordIs(&t[a], L"not")) || wordIs(&t[i], L"didn't")) {
            int h = wordIs(&t[i], L"didn't") ? a : b, m = h < n ? nextNS(t, n, h + 1) : n;
            if (h < n && wordIs(&t[h], L"have") && m < n && (wordIs(&t[m], L"much") || wordIs(&t[m], L"many"))) {
                for (int q = 0; q < i; q++) tCopy(o, &t[q]);
                pushW(o, L"had", 1); pushSp(o); pushW(o, wordIs(&t[m], L"much") ? L"little" : L"few", 1);
                for (int q = m + 1; q < n; q++) tCopy(o, &t[q]);
                return 1;
            }
        }
        if (wordIs(&t[i], L"never") && a < n && t[a].tag == T_VBD && b < n && wordIs(&t[b], L"another")) {
            for (int q = 0; q < i; q++) tCopy(o, &t[q]);
            tCopy(o, &t[a]); pushSp(o); pushWs(o, L"no other", 1);
            for (int q = b + 1; q < n; q++) tCopy(o, &t[q]);
            return 1;
        }
        if (wordIs(&t[i], L"not") && a < n && t[a].tag == T_ADJ && !wcschr(t[a].s, L' ') && (c >= n || b == n - 1 || t[b].type == TK_PUNCT)) {
            wchar_t adj[48], un[52]; wordL(&t[a], adj); swprintf(un, 52, L"un%ls", adj);
            char nb[56]; wcstombs(nb, un, 55); nb[55] = 0;
            if (!tagLookup(tw, ntw, nb)) continue;
            int pv = i - 1; while (pv >= 0 && t[pv].type == TK_SPACE) pv--;
            if (pv < 0 || t[pv].tag != T_BE) continue;
            for (int q = 0; q < i; q++) tCopy(o, &t[q]);
            pushW(o, un, 1);
            for (int q = a + 1; q < n; q++) tCopy(o, &t[q]);
            return 1;
        }
    }
    return 0;
}

/* "That poem was 40,000 lines long." -> "That poem had a length of 40,000 lines." */
static int patMeasure(Tok *t, int n, Toks *o) {
    return 0;
    if (sentShape(t, n) < 0) return 0;
    static const wchar_t *DIM[] = { L"long", L"tall", L"wide", L"deep", L"high", NULL };
    static const wchar_t *NOUN[] = { L"length", L"height", L"width", L"depth", L"height" };
    for (int i = 0; i < n - 4; i++) {
        if (t[i].tag != T_BE || !isFiniteTok(t, i)) continue;
        int a = nextNS(t, n, i + 1); if (a >= n || !(t[a].type == TK_LOCK || t[a].tag == T_NUM)) continue;
        int u = nextNS(t, n, a + 1); if (u >= n || t[u].type != TK_WORD || !(t[u].tag == T_NS || t[u].tag == T_N)) continue;
        int d = nextNS(t, n, u + 1); if (d >= n) continue;
        int di = -1; for (int q = 0; DIM[q]; q++) if (wordIs(&t[d], DIM[q])) di = q;
        if (di < 0) continue;
        wchar_t be[16]; wordL(&t[i], be);
        const wchar_t *hv = !wcscmp(be, L"was") || !wcscmp(be, L"were") ? L"had" : !wcscmp(be, L"are") ? L"have" : L"has";
        for (int q = 0; q < i; q++) tCopy(o, &t[q]);
        pushW(o, hv, 1); pushSp(o); pushW(o, L"a", 1); pushSp(o); pushW(o, NOUN[di], 1); pushSp(o); pushW(o, L"of", 1); pushSp(o);
        tCopy(o, &t[a]); pushSp(o); tCopy(o, &t[u]);
        for (int q = d + 1; q < n; q++) tCopy(o, &t[q]);
        return 1;
    }
    return 0;
}

/* "Every friendship begins here." -> "Here is where every friendship begins." */
static int patHereFront(Tok *t, int n, Toks *o) {
    if (sentShape(t, n) < 0 || !lowerable(&t[0])) return 0;
    int h = n - 2; while (h >= 0 && t[h].type == TK_SPACE) h--;
    if (h < 2 || !(wordIs(&t[h], L"here") || wordIs(&t[h], L"there"))) return 0;
    int v = h - 1; while (v >= 0 && t[v].type == TK_SPACE) v--;
    if (v < 1 || !(t[v].tag == T_VBZ || t[v].tag == T_VBD || t[v].tag == T_VB)) return 0;
    if (isWordIn(&t[0], (const wchar_t *[]){ L"there", L"here", L"it", NULL })) return 0;
    Span body = trimSpan(t, 0, h);
    if (nri(2)) { pushWs(o, wordIs(&t[h], L"here") ? L"Here is where" : L"There is where", 1); pushSp(o); pushSpan(o, body, -1); }
    else { pushWs(o, t[v].tag == T_VBD ? L"It was" : L"It is", 1); pushSp(o); tCopy(o, &t[h]); pushSp(o); pushW(o, L"that", 1); pushSp(o); pushSpan(o, body, -1); }
    tCopy(o, &t[n - 1]);
    return 1;
}

/* "The H is, if you squint, a ladder." -> "If you squint, the H is a ladder."
   "Fennimore, alone after losing his work, reaches out." -> "Alone after losing his work, Fennimore reaches out." */
static int patParenFront(Tok *t, int n, Toks *o) {
    if (sentShape(t, n) != 2 || !lowerable(&t[0])) return 0;
    int c1 = firstComma(t, n), c2 = -1;
    for (int i = c1 + 1; i < n; i++) if (t[i].type == TK_PUNCT && t[i].s[0] == L',') { c2 = i; break; }
    if (c1 < 1 || c2 < 0) return 0;
    Span a = trimSpan(t, 0, c1), p = trimSpan(t, c1 + 1, c2), b = trimSpan(t, c2 + 1, n - 1);
    int pw = spanWords(p);
    if (spanWords(a) < 1 || spanWords(a) > 6 || pw < 1 || pw > 9 || spanWords(b) < 2) return 0;
    static const wchar_t *PSTART[] = { L"if", L"when", L"as", L"in", L"for", L"at", L"on", L"by", L"after", L"before", L"during", L"despite",
        L"alone", L"like", L"unlike", L"once", L"though", L"although", L"while", L"of", NULL };
    Tok *p0 = &p.t[0];
    if (!(isWordIn(p0, PSTART) || p0->tag == T_ADJ || p0->tag == T_VBN || p0->tag == T_ADV)) return 0;
    if (wordIs(p0, L"of") && pw < 3) return 0;
    if (isWordIn(p0, (const wchar_t *[]){ L"however", L"too", L"though", NULL }) && pw == 1) return 0;
    /* B must continue the sentence: a verb, or A must already contain one */
    int finA = 0, finB = 0;
    for (int q = 0; q < a.n; q++) if (isFiniteTok(a.t, q)) finA = 1;
    for (int q = 0; q < b.n; q++) if (isFiniteTok(b.t, q)) finB = 1;
    if (!finA && !(b.t[0].type == TK_WORD && isFiniteTok(b.t, 0))) return 0;
    if (!finA && !finB) return 0;
    if (isWordIn(&b.t[0], (const wchar_t *[]){ L"and", L"but", L"or", L"which", L"who", NULL })) return 0;
    pushSpan(o, p, 1); pushP(o, L','); pushSp(o);
    pushSpan(o, a, -1); pushSp(o); pushSpan(o, b, 0);
    tCopy(o, &t[n - 1]);
    return 1;
}

/* "Devastated, Fennimore sat down." -> "Fennimore, devastated, sat down." */
static int patAdjTuck(Tok *t, int n, Toks *o) {
    if (sentShape(t, n) < 1) return 0;
    int c = firstComma(t, n);
    if (c != 1 && c != 2) return 0;
    if (!(t[0].tag == T_ADJ || t[0].tag == T_VBN) || wcschr(t[0].s, L' ')) return 0;
    int s = nextNS(t, n, c + 1), p1, q1, e = npEnd(t, n, s, &p1, &q1);
    if (e < 0) return 0;
    int v = nextNS(t, n, e);
    if (v >= n || !isFiniteTok(t, v)) return 0;
    Span np = trimSpan(t, s, e), rest = trimSpan(t, v, n - 1);
    if (spanWords(np) > 5) return 0;
    pushSpan(o, np, 1); pushP(o, L','); pushSp(o);
    Span adj = { t, 1 }; pushSpan(o, adj, -1); pushP(o, L','); pushSp(o);
    pushSpan(o, rest, 0);
    tCopy(o, &t[n - 1]);
    return 1;
}

/* "He sat down at his desk, stared at the page, and wrote one word." -> "After sitting down at his desk, he stared at the page and wrote one word." */
static int patAfterSeq(Tok *t, int n, Toks *o) {
    if (!budgetOK(gAfter, 15)) return 0;
    if (sentShape(t, n) < 0) return 0;
    int p1, q1, e = npEnd(t, n, 0, &p1, &q1);
    if (e < 0 || !lowerable(&t[0])) return 0;
    int v1 = nextNS(t, n, e);
    if (v1 >= n || t[v1].tag != T_VBD || wcschr(t[v1].s, L' ')) return 0;
    VForm *vf = verbOf(t[v1].s); if (!vf) return 0;
    wchar_t b[64]; mbstowcs(b, vf->f[0], 63); b[63] = 0;
    if (!wcscmp(b, L"be") || !wcscmp(b, L"have") || !wcscmp(b, L"do") || !wcscmp(b, L"say")) return 0;
    /* find the end of the first verb phrase: ", VBD" or "and VBD" */
    int cut = -1, next = -1, commaList = 0;
    for (int i = v1 + 1; i < n - 1; i++) {
        if (t[i].type == TK_PUNCT && t[i].s[0] != L',') return 0;
        if (t[i].type == TK_PUNCT && t[i].s[0] == L',') {
            int k = nextNS(t, n, i + 1);
            if (k < n && t[k].tag == T_VBD) { cut = i; next = k; commaList = 1; break; }
            if (k < n && wordIs(&t[k], L"and")) { int k2 = nextNS(t, n, k + 1); if (k2 < n && t[k2].tag == T_VBD) { cut = i; next = k2; break; } }
            return 0;
        }
        if (wordIs(&t[i], L"and")) { int k = nextNS(t, n, i + 1); if (k < n && t[k].tag == T_VBD) { cut = i; next = k; break; } return 0; }
    }
    if (cut < 0) return 0;
    static const wchar_t *SUBORD[] = { L"how", L"what", L"why", L"who", L"that", L"which", L"where", L"when", L"if", L"whether", L"because", NULL };
    for (int q = v1 + 1; q < cut; q++) if (isWordIn(&t[q], SUBORD)) return 0;
    Span x = trimSpan(t, v1 + 1, cut), subj = trimSpan(t, 0, e);
    if (spanWords(x) < 1 || spanWords(subj) > 6) return 0;
    wchar_t ing[64]; mkIngB(vf->f[0], ing);
    gAfter++;
    pushW(o, L"After", 1); pushSp(o); pushW(o, ing, 1); pushSp(o); pushSpan(o, x, 0); pushP(o, L','); pushSp(o);
    pushNP(o, subj, 0, -1); pushSp(o);
    /* remaining verbs: if it was "A, B, and C" with three, drop the now-orphan comma before "and" */
    int items = 0; for (int i = next; i < n - 1; i++) if (t[i].type == TK_PUNCT && t[i].s[0] == L',') items++;
    for (int i = next; i < n - 1; i++) {
        if (commaList && items == 1 && t[i].type == TK_PUNCT && t[i].s[0] == L',') { int k = nextNS(t, n, i + 1); if (k < n && wordIs(&t[k], L"and")) continue; }
        tCopy(o, &t[i]);
    }
    tCopy(o, &t[n - 1]);
    return 1;
}

/* "Others say he went to get a sandwich." -> "According to others, he went to get a sandwich."
   "The prevailing theory is that X." -> "According to the prevailing theory, X." */
static int patAccording(Tok *t, int n, Toks *o) {
    if (!budgetOK(gAccord, 15)) return 0;
    if (sentShape(t, n) < 0 || !lowerable(&t[0])) return 0;
    static const wchar_t *SAYV[] = { L"say", L"says", L"said", L"claim", L"claims", L"claimed", L"report", L"reports", L"reported", L"argue",
        L"argues", L"argued", L"believe", L"believes", L"believed", L"suggest", L"suggests", L"suggested", L"write", L"writes", L"wrote",
        L"insist", L"insists", L"insisted", L"maintain", L"maintains", NULL };
    static const wchar_t *THEORY[] = { L"theory", L"idea", L"view", L"belief", L"consensus", L"assumption", L"argument", L"explanation", L"hypothesis", L"story", NULL };
    int e = -1, p1 = 0, q1 = 0;
    if (isWordIn(&t[0], (const wchar_t *[]){ L"some", L"others", L"many", L"most", L"experts", L"critics", L"scholars", NULL })) e = 1;
    else e = npEnd(t, n, 0, &p1, &q1);
    if (e < 0 || q1) return 0;
    Span subj = trimSpan(t, 0, e);
    if (spanWords(subj) > 8) return 0;
    for (int q = 0; q < subj.n; q++) if (subj.t[q].type == TK_PUNCT) return 0;
    int v = nextNS(t, n, e);
    if (v >= n - 1) return 0;
    int c;
    if (isWordIn(&t[v], SAYV)) { c = nextNS(t, n, v + 1); if (c < n && wordIs(&t[c], L"that")) c = nextNS(t, n, c + 1); }
    else if (t[v].tag == T_BE && isFiniteTok(t, v)) {
        int hd = 0; for (int q = 0; q < subj.n; q++) if (isWordIn(&subj.t[q], THEORY)) hd = 1;
        c = nextNS(t, n, v + 1);
        if (!hd || c >= n || !wordIs(&t[c], L"that")) return 0;
        c = nextNS(t, n, c + 1);
    } else return 0;
    if (c >= n - 1 || !startsClause(&t[c])) return 0;
    Span cl = trimSpan(t, c, n - 1);
    int fin = 0; for (int q = 0; q < cl.n; q++) if (isFiniteTok(cl.t, q)) fin = 1;
    if (!fin || spanWords(cl) < 3) return 0;
    for (int q = 0; q + 2 < cl.n; q++) if (wordIs(&cl.t[q], L"and") && wordIs(&cl.t[nextNS(cl.t, cl.n, q + 1)], L"that")) return 0;
    gAccord++;
    pushWs(o, L"According to", 1); pushSp(o); pushSpan(o, subj, -1); pushP(o, L','); pushSp(o);
    pushSpan(o, cl, wordIs(&cl.t[0], L"i") ? 0 : -1);
    tCopy(o, &t[n - 1]);
    return 1;
}

/* "What is certain is that X." -> "It is certain that X." */
static int patWhatIs(Tok *t, int n, Toks *o) {
    if (sentShape(t, n) < 0 || !wordIs(&t[0], L"what")) return 0;
    int b1 = nextNS(t, n, 1), a = nextNS(t, n, b1 + 1), b2 = nextNS(t, n, a + 1), th = nextNS(t, n, b2 + 1);
    if (th >= n - 1 || t[b1].tag != T_BE || t[a].tag != T_ADJ || t[b2].tag != T_BE || !wordIs(&t[th], L"that")) return 0;
    pushW(o, L"It", 1); pushSp(o); tCopy(o, &t[b1]); pushSp(o); tCopy(o, &t[a]); pushSp(o);
    for (int q = th; q < n; q++) tCopy(o, &t[q]);
    return 1;
}

/* "Today it is taught in 23 countries." -> "It is taught in 23 countries today." */
static int patTimeWordBack(Tok *t, int n, Toks *o) {
    if (sentShape(t, n) < 0) return 0;
    static const wchar_t *TW[] = { L"today", L"now", L"yesterday", L"tomorrow", L"eventually", L"soon", L"later", L"then", L"recently", L"nowadays", NULL };
    if (!isWordIn(&t[0], TW) || wcschr(t[0].s, L' ')) return 0;
    int k = nextNS(t, n, 1);
    if (k >= n - 1 || t[k].type == TK_PUNCT || !startsClause(&t[k])) return 0;
    Span rest = trimSpan(t, k, n - 1), tw0 = { t, 1 };
    if (spanWords(rest) < 3 || spanWords(rest) > 12) return 0;
    for (int q = 0; q < rest.n; q++) if (isWordIn(&rest.t[q], (const wchar_t *[]){ L"who", L"which", L"that", L"when", L"where", L"because", L"if", L"and", L"or", NULL })) return 0;
    for (int q = 0; q < rest.n; q++) if (rest.t[q].type == TK_PUNCT && rest.t[q].s[0] != L',') return 0;
    pushSpan(o, rest, 1); pushSp(o); pushSpan(o, tw0, -1);
    tCopy(o, &t[n - 1]);
    return 1;
}

/* "X and is Y" verb-phrase coordination -> two sentences: "Today it is taught in 23 countries and is the only poem ..." */
static int patVPSplit(Tok *t, int n, Toks *o) {
    if (sentShape(t, n) < 0) return 0;
    if (isWordIn(&t[0], (const wchar_t *[]){ L"when", L"if", L"because", L"while", L"after", L"before", L"as", L"since", L"although", L"though", L"once", L"until", L"unless", L"whenever", L"by", NULL })) return 0;
    int p1, q1, s = 0;
    while (s < n && !((t[s].type == TK_WORD && (t[s].tag == T_PRON || t[s].tag == T_DET)) || t[s].type == TK_LOCK)) { if (t[s].type == TK_PUNCT) return 0; s++; }
    int e = npEnd(t, n, s, &p1, &q1);
    if (e < 0) return 0;
    int v = nextNS(t, n, e);
    if (v >= n || !isFiniteTok(t, v)) return 0;
    for (int i = v + 1; i < n - 2; i++) {
        if (t[i].type == TK_PUNCT) return 0;
        if (isWordIn(&t[i], (const wchar_t *[]){ L"how", L"what", L"why", L"who", L"that", L"which", L"where", L"when", L"if", L"whether", L"because", NULL })) return 0;
        if (!wordIs(&t[i], L"and")) continue;
        int k = nextNS(t, n, i + 1);
        if (k >= n - 1 || !isFiniteTok(t, k) || t[k].tag == T_VB) continue;
        Span a = trimSpan(t, 0, i), b = trimSpan(t, k, n - 1), subj = trimSpan(t, s, e);
        if (spanWords(a) < 4 || spanWords(b) < 3) return 0;
        pushSpan(o, a, 0); pushP(o, L'.'); pushSp(o);
        if (q1 || spanWords(subj) <= 2) pushNP(o, subj, 0, 1);
        else pushW(o, p1 ? L"They" : L"It", 1);
        pushSp(o); pushSpan(o, b, 0);
        tCopy(o, &t[n - 1]);
        return 1;
    }
    return 0;
}

/* ---- validation: a rebuilt sentence must still read as grammatical ---- */
static int dtGramCount(const wchar_t *s);
static int rebuildOK(Toks *in, Toks *out) {
    tagAll(out);
    {   /* keep the sentence's ending, and don't strand a quote that ends in a comma */
        int li = in->n - 1; while (li >= 0 && in->t[li].type == TK_SPACE) li--;
        int lo = out->n - 1; while (lo >= 0 && out->t[lo].type == TK_SPACE) lo--;
        if (li >= 0 && in->t[li].type == TK_PUNCT && (lo < 0 || out->t[lo].type != TK_PUNCT)) return 0;
        for (int i = 0; i < out->n; i++) if (out->t[i].type == TK_LOCK) {
            int L2 = (int)wcslen(out->t[i].s);
            if (L2 >= 2 && out->t[i].s[L2 - 2] == L',' ) {
                int nx = i + 1; while (nx < out->n && out->t[nx].type == TK_SPACE) nx++;
                if (nx >= out->n || (out->t[nx].type == TK_PUNCT && wcschr(L".!?", out->t[nx].s[0]))) return 0;
            }
        }
    }
    /* every piece needs a finite verb (headings and quotes aside) */
    int segStart = 0, fin = 0, words = 0, inRel = 0, subOpen = 0, afterComma = 0, finAfter = 0;
    static const wchar_t *RELW[] = { L"that", L"which", L"who", L"whom", L"whose", L"where", L"when", NULL };
    static const wchar_t *SUBO[] = { L"through", L"by", L"because", L"although", L"though", L"if", L"when", L"while", L"since", L"after",
        L"before", L"as", L"unless", L"until", L"once", L"whenever", L"with", L"without", L"despite", L"in", L"on", L"at", L"from", L"for", L"during", NULL };
    for (int i = 0; i < out->n; i++) {
        Tok *k = &out->t[i];
        if (k->type == TK_WORD) {
            if (words == 0 && isWordIn(k, SUBO)) subOpen = 1;
            if (words > 0 && isWordIn(k, RELW)) inRel = 1;
            words++;
            if (isFiniteN(out->t, out->n, i) && !inRel) { fin = 1; if (afterComma) finAfter = 1; }
        }
        if (k->type == TK_LOCK) words++;
        if (k->type == TK_PUNCT && k->s[0] == L',') { inRel = 0; afterComma = 1; }
        if (k->type == TK_PUNCT && (k->s[0] == L'.' || k->s[0] == L'!' || k->s[0] == L'?')) {
            if (words >= 3 && !fin) return 0;
            if (words >= 5 && subOpen && afterComma && !finAfter) return 0;
            segStart = i + 1; fin = 0; words = 0; inRel = 0; subOpen = 0; afterComma = 0; finAfter = 0;
        }
        if (i > 0 && k->type == TK_WORD && out->t[i - 1].type == TK_SPACE && i > 1 && out->t[i - 2].type == TK_WORD) {
            wchar_t a[40], b[40]; lastWordLower(out->t[i - 2].s, a, 40); firstWordLower(k->s, b, 40);
            if (a[0] && !wcscmp(a, b) && wcscmp(a, L"that") && wcscmp(a, L"had")) return 0;
        }
        if (k->type == TK_PUNCT && i + 1 < out->n && out->t[i + 1].type == TK_PUNCT && k->s[0] == L',' && out->t[i + 1].s[0] != L'"') return 0;
    }
    (void)segStart;
    for (int i = 0; i < out->n; i++) if (out->t[i].type == TK_WORD) {
        static const wchar_t *BADSTART[] = { L"and", L"or", L"as well as", L"which", NULL };
        wchar_t w[40]; firstWordLower(out->t[i].s, w, 40); if (inList(BADSTART, w)) return 0; break;
    }
    Str a = {0}, b = {0}; sAdd(&a, L""); sAdd(&b, L"");
    for (int i = 0; i < in->n; i++) sAdd(&a, in->t[i].s);
    for (int i = 0; i < out->n; i++) sAdd(&b, out->t[i].s);
    {   /* an additive opener can't introduce a consequence or a contrast */
        static const wchar_t *ADD[] = { L"also, ", L"in addition, ", L"additionally, ", L"moreover, ", L"furthermore, ", L"on top of that, ", L"what's more, ", NULL };
        static const wchar_t *NEXT[] = { L"this is why", L"that is why", L"this is because", L"so ", L"therefore", L"however", L"still", L"yet ", L"but ", L"even so", NULL };
        wchar_t *lw = wdup(b.s); for (wchar_t *q = lw; *q; q++) *q = towlower(*q);
        int bad = 0;
        for (int x = 0; ADD[x] && !bad; x++) for (wchar_t *q = lw; (q = wcsstr(q, ADD[x])); q++) {
            const wchar_t *r = q + wcslen(ADD[x]);
            for (int y = 0; NEXT[y]; y++) if (!wcsncmp(r, NEXT[y], wcslen(NEXT[y]))) bad = 1;
            if (wcsstr(r, L", not ") && wcsstr(r, L", not ") - r < 60) bad = 1;
        }
        free(lw);
        if (bad) { free(a.s); free(b.s); return 0; }
    }
    {
        wchar_t *lw = wdup(b.s); for (wchar_t *q = lw; *q; q++) *q = towlower(*q);
        static const wchar_t *BADJ[] = { L", and that said", L", and however", L", and still,", L", and even so", L", and so,", L", and also,", L", and in fact", L", and indeed", L", and as a result", L", and for that reason", NULL };
        int bad = 0; for (int x = 0; BADJ[x]; x++) if (wcsstr(lw, BADJ[x])) bad = 1;
        free(lw);
        if (bad) { free(a.s); free(b.s); return 0; }
    }
    int ok = dtGramCount(b.s) <= dtGramCount(a.s);
    free(a.s); free(b.s);
    return ok;
}

typedef int (*PatFn)(Tok *, int, Toks *);
static PatFn PATS[] = { patFrontSub, patTrailSub, patButSo, patPPBack, patPPFront, patReport, patHoist, patSplit, patPassive, patActive,
                        patCauseFlip, patConcession, patCleft, patAdvEnd, patSplitMore, patFiller, patListShuffle,
                        patImperative, patPseudoCleft, patThereWas, patAlsoEnd, patAuxPassive, patNegation, patMeasure, patHereFront,
                        patParenFront, patAdjTuck, patAfterSeq, patAccording, patWhatIs, patTimeWordBack, patVPSplit };
#define FIRST_MAXPAT 17
static const int INV[] = { (1 << 1) | (1 << 4), 1 << 0, (1 << 10) | (1 << 11), 1 << 4, (1 << 3) | (1 << 0), 0, 1 << 13, 0, 1 << 9, 1 << 8,
                           1 << 2, 1 << 2, 0, 1 << 6, 0, 0, 0,
                           0, 0, 0, 0, 0, 0, 0, 0, 1 << 26, 1 << 25, 0, 1 << 5, 0, 0, 0 };
#define NPATS (int)(sizeof PATS / sizeof PATS[0])

/* shape of a sentence that word swaps can't change: locked bits, pronouns/determiners and punctuation, in order.
   if a rearrangement recreates a shape we've already had, it's an undo and gets refused */
static const wchar_t *SKEL[] = { L"the", L"a", L"an", L"he", L"she", L"they", L"it", L"we", L"i", L"you", L"his", L"her", L"their",
    L"our", L"my", L"your", L"its", L"this", L"that", L"these", L"those", L"him", L"them", L"us", L"me",
    L"let's", L"what", L"there", L"here", L"according", L"worth", L"after", L"who", L"which", L"by", L"been", L"be", L"is", L"was",
    L"are", L"were", L"has", L"have", L"had", L"not", L"no", L"too", L"well", L"of", L"to", L"if", L"so", L"and", L"but", L"because",
    L"although", L"though", L"when", L"while", L"as", NULL };
static wchar_t *sentKey(Tok *t, int n) {
    Str o = {0}; sAdd(&o, L"");
    for (int i = 0; i < n; i++) {
        if (t[i].type == TK_LOCK) { sAdd(&o, t[i].s); sAddC(&o, L'|'); continue; }
        if (t[i].type == TK_PUNCT) { sAddC(&o, t[i].s[0]); continue; }
        if (t[i].type != TK_WORD) continue;
        const wchar_t *p = t[i].s;
        while (*p) {
            const wchar_t *e = p; while (*e && *e != L' ') e++;
            int L2 = (int)(e - p), a = 0; while (a < L2 && !isApos(p[a])) a++;
            if (a < 30) { wchar_t w[32]; lowerInto(p, a, w); if (inList(SKEL, w)) { sAdd(&o, w); sAddC(&o, L'|'); } }
            p = *e ? e + 1 : e;
        }
    }
    return o.s;
}
static int sentSeen(Toks *s) {
    if (!curCtx) return 0;
    wchar_t *k = sentKey(s->t, s->n); int r = ssHas(&curCtx->seen, k); free(k); return r;
}
static void sentRemember(Tok *t, int n) {
    if (!curCtx || !n) return;
    wchar_t *k = sentKey(t, n); ssAdd(&curCtx->seen, k); free(k);
}

static StrSet recentSkel;
/* after a clause swap, "He was tired, so John left." -> "John was tired, so he left." */
static void fixCataphora(Toks *o) {
    int pron = -1, name = -1, names = 0;
    for (int i = 0; i < o->n; i++) {
        Tok *k = &o->t[i];
        if (k->type == TK_LOCK && iswupper(k->s[0])) { int alpha = 1; for (wchar_t *p = k->s; *p; p++) if (!iswalpha(*p) && *p != L' ' && *p != L'.') alpha = 0; if (alpha) { names++; if (name < 0) name = i; } }
        if (pron < 0 && name < 0 && (wordIs(k, L"he") || wordIs(k, L"she"))) pron = i;
    }
    if (pron < 0 || name < 0 || names != 1 || pron != 0) return;
    { int q = name - 1; while (q >= 0 && o->t[q].type == TK_SPACE) q--; if (q >= 0 && (wordIs(&o->t[q], L"was") || wordIs(&o->t[q], L"is"))) return; }
    int startP = pron == 0, startN = 0;
    { int q = name - 1; while (q >= 0 && o->t[q].type == TK_SPACE) q--; startN = q < 0 || (o->t[q].type == TK_PUNCT && o->t[q].s[0] != L','); }
    wchar_t pw[8]; wordL(&o->t[pron], pw);
    wchar_t *nm = wdup(o->t[name].s);
    free(o->t[pron].s); o->t[pron].s = nm; o->t[pron].type = TK_LOCK;
    free(o->t[name].s); o->t[name].s = wdup(pw); o->t[name].type = TK_WORD; o->t[name].tag = T_PRON;
    if (startN) o->t[name].s[0] = towupper(o->t[name].s[0]);
    (void)startP;
}

static int restructureOnce(Toks *s, unsigned *used) {
    int order[NPATS];
    for (int i = 0; i < NPATS; i++) order[i] = i;
    for (int i = NPATS - 1; i > 0; i--) { int j = nri(i + 1), x = order[i]; order[i] = order[j]; order[j] = x; }
    if (gType == WT_SOCIAL || gTone == TN_SIMPLE) {
        static const int SPL[] = { 31, 14, 7 };
        for (int a = 0; a < 3; a++) for (int i = 0; i < NPATS; i++) if (order[i] == SPL[a]) { int x = order[a]; order[a] = order[i]; order[i] = x; break; }
    }
    Toks fb = {0}; int fbp = -1;
    for (int i = 0; i < NPATS; i++) {
        int p = order[i];
        if (*used & (1u << p)) continue;
        if (p >= FIRST_MAXPAT && !maxMode && !(p == 31 && (gType == WT_SOCIAL || gTone == TN_SIMPLE))) continue;
        if (toneReg() < 0 && (p == 8 || p == 21 || p == 28 || p == 19 || p == 18 || p == 12)) continue;
        if ((p == 8 || p == 21) && !(gTone == TN_FORMAL || gTone == TN_ACADEMIC || gType == WT_REPORT)) continue;
        if (toneReg() > 0 && (p == 17)) continue;
        if (gType == WT_STORY && (p == 28 || p == 18 || p == 5)) continue;
        Toks o = {0};
        int made = PATS[p](s->t, s->n, &o);
        if (!made && maxMode && p >= 8 && p != 15 && p != 12) {
            int c = firstComma(s->t, s->n);
            if (c > 0 && c < s->n - 4) {
                Span head = trimSpan(s->t, 0, c);
                int hw = spanWords(head);
                Tok *h0 = &head.t[0];
                if (hw >= 1 && hw <= 4 && h0->type == TK_WORD && (h0->tag == T_ADV || h0->tag == T_ADP || h0->tag == T_CONJ || startsWithList(h0->s, OPENLIST))) {
                    Toks body = {0}; Span bs = trimSpan(s->t, c + 1, s->n);
                    pushSpan(&body, bs, 1);
                    tagAll(&body);
                    Toks bo = {0};
                    if (body.n && lowerable(&body.t[0]) && PATS[p](body.t, body.n, &bo) && bo.n) {
                        pushSpan(&o, head, 0); pushP(&o, L','); pushSp(&o);
                        Span bsp = { bo.t, bo.n }; pushSpan(&o, bsp, -1);
                        made = 1;
                    }
                    tFree(&bo); tFree(&body);
                }
            }
        }
        if (made && p != 0) fixCataphora(&o);
        if (made && !sentSeen(&o) && ((!maxMode && p < FIRST_MAXPAT) || rebuildOK(s, &o))) {
            wchar_t *sk = sentKey(o.t, o.n);
            int recent = ssHas(&recentSkel, sk);
            if (recent && fbp < 0) { fb = o; fbp = p; free(sk); continue; }
            if (recent) { tFree(&o); free(sk); continue; }
            if (recentSkel.n > 20000) ssFree(&recentSkel);
            ssAdd(&recentSkel, sk); free(sk);
            if (fbp >= 0) tFree(&fb);
            tFree(s); *s = o; *used |= (1u << p) | (unsigned)INV[p]; return 1;
        }
        tFree(&o);
    }
    if (fbp >= 0 && !maxMode && nrnd() >= 0.5) { tFree(&fb); fbp = -1; }
    if (fbp >= 0) {
        tFree(s); *s = fb; *used |= (1u << fbp) | (unsigned)INV[fbp]; return 1;
    }
    return 0;
}

static int sentWords(Toks *s) { Span sp = { s->t, s->n }; return spanWords(sp); }

/* sentence rhythm: break up runs of same-length sentences (join two similar short ones, split a long one) */
/* split one sentence at a clause boundary; mode 0 = most even halves, 1 = leave a short punchy piece */
static int rhSplit(Tok *t, int n, Toks *o, int mode) {
    if (n < 8 || t[n - 1].type != TK_PUNCT || !(t[n - 1].s[0] == L'.' || t[n - 1].s[0] == L'!')) return 0;
    int total = 0; for (int i = 0; i < n; i++) { if (t[i].type == TK_NL) return 0; if (t[i].type == TK_WORD || t[i].type == TK_LOCK) total++; }
    int best = -1, bestKind = 0, bestScore = -1000000, words = 0, inQ = 0, paren = 0;
    for (int c = 0; c < n - 3; c++) {
        Tok *k = &t[c];
        if (k->type == TK_WORD || k->type == TK_LOCK) words++;
        if (k->type == TK_PUNCT) {
            wchar_t ch = k->s[0];
            if (ch == L'\u201C') inQ = 1; else if (ch == L'\u201D') inQ = 0; else if (ch == L'"') inQ = !inQ;
            if (ch == L'(') paren++; else if (ch == L')') paren--;
        }
        int bare = !inQ && paren <= 0 && k->type == TK_SPACE && c + 1 < n && (wordIs(&t[c + 1], L"so") || wordIs(&t[c + 1], L"but")) && words >= 5;
        if (!bare && (inQ || paren > 0 || k->type != TK_PUNCT || !(k->s[0] == L',' || k->s[0] == L';'))) continue;
        int a = nextNS(t, n, c + 1); if (a >= n) continue;
        int kind = 0, y0 = a;
        if (bare) { int pv = c - 1; while (pv >= 0 && t[pv].type == TK_SPACE) pv--; if (pv < 0 || t[pv].type != TK_WORD) continue; }
        if (k->s[0] == L';') kind = 1;
        else if (wordIs(&t[a], L"and") || wordIs(&t[a], L"but") || wordIs(&t[a], L"so") || wordIs(&t[a], L"yet")) {
            y0 = nextNS(t, n, a + 1); if (y0 >= n) continue;
            if (wordIs(&t[y0], L"that") || wordIs(&t[y0], L"this")) continue;
            kind = wordIs(&t[a], L"and") ? 1 : 2;
        } else if (wordIs(&t[a], L"where") || wordIs(&t[a], L"because")) {
            y0 = nextNS(t, n, a + 1); if (y0 >= n) continue;
            kind = wordIs(&t[a], L"where") ? 4 : 5;
        } else if (wordIs(&t[a], L"just") && nextNS(t, n, a + 1) < n && wordIs(&t[nextNS(t, n, a + 1)], L"as")) {
            y0 = nextNS(t, n, nextNS(t, n, a + 1) + 1); if (y0 >= n) continue;
            kind = 6;
        } else if (!bare && (wordIs(&t[a], L"while") || wordIs(&t[a], L"when"))) {
            y0 = nextNS(t, n, a + 1); if (y0 >= n) continue;
            if (!(t[y0].type == TK_LOCK || t[y0].tag == T_DET || t[y0].tag == T_PRON || t[y0].tag == T_NP)) continue;
            int f = 0; for (int q = y0; q < n && t[q].type != TK_PUNCT; q++) if (q > y0 && isFiniteN(t, n, q)) { f = 1; break; } if (!f) continue;
            kind = wordIs(&t[a], L"while") ? 7 : 8;
        } else if (!bare && (wordIs(&t[a], L"a") || wordIs(&t[a], L"an"))) {
            /* appositive at the end: "..., a highly poisonous fish that can kill." -> ". It is a highly poisonous fish that can kill." */
            int pv = c - 1; while (pv >= 0 && t[pv].type == TK_SPACE) pv--;
            if (pv < 0 || !(t[pv].tag == T_N || t[pv].tag == T_NS || t[pv].tag == T_NP)) continue;
            int more = 0; for (int q = a; q < n - 1; q++) if (t[q].type == TK_PUNCT && t[q].s[0] != L'\u201D' && t[q].s[0] != L'"') more = 1;
            if (more) continue;
            int lf = 0; for (int q = 0; q < c; q++) if (isFiniteN(t, n, q)) lf = 1; if (!lf) continue;
            y0 = a; kind = 9;
        } else if (wordIs(&t[a], L"which")) {
            y0 = nextNS(t, n, a + 1); if (y0 >= n || t[y0].tag != T_VBZ) continue;
            kind = 3;
        } else continue;
        if (kind >= 4 && kind <= 6) { int f = 0; for (int q = y0; q < n && t[q].type != TK_PUNCT; q++) if (isFiniteN(t, n, q)) { f = 1; break; } if (!f) continue; }
        else if (kind <= 2) {
            int p3, q3, ye = npEnd(t, n, y0, &p3, &q3), okc = 0;
            if (ye >= 0) { int yv = nextNS(t, n, ye);
                for (int pp = 0; pp < 2 && yv < n && t[yv].tag == T_ADP && !wordIs(&t[yv], L"that"); pp++) { int e2 = npEnd(t, n, nextNS(t, n, yv + 1), &p3, &q3); if (e2 < 0) break; yv = nextNS(t, n, e2); }
                while (yv < n && t[yv].tag == T_ADV) yv = nextNS(t, n, yv + 1);
                okc = yv < n && isFiniteN(t, n, yv); }
            if (!okc && (t[y0].type == TK_LOCK || t[y0].tag == T_DET || t[y0].tag == T_PRON || t[y0].tag == T_NP || t[y0].tag == T_N || t[y0].tag == T_NS || t[y0].tag == T_ADJ)) { int w2 = 0; for (int q = y0; q < n && t[q].type != TK_PUNCT && w2 < 4; q++) if (t[q].type == TK_WORD) { if (q > y0 && isFiniteN(t, n, q)) { okc = 1; break; } w2++; } }
            if (!okc) continue;
        }
        int left = words, right = total - words - (kind == 1 && k->s[0] == L';' ? 0 : 1);
        if (left < 4 || right < 3) continue;
        int sc = mode == 0 ? -abs(left - right) : abs(left - right);
        if (mode == 1 && (left > 9 && right > 9)) sc -= 100;
        if (sc > bestScore) { bestScore = sc; best = c; bestKind = kind; }
    }
    if (best < 0) return 0;
    int a = nextNS(t, n, best + 1);
    Span x = t[best].type == TK_SPACE ? trimSpan(t, 0, best + 1) : trimSpan(t, 0, best);
    pushSpan(o, x, 0); pushP(o, L'.'); pushSp(o);
    if (bestKind == 1) { int y0 = t[best].s[0] == L';' ? a : nextNS(t, n, a + 1); pushSpan(o, trimSpan(t, y0, n - 1), 1); }
    else if (bestKind == 2) { pushCap(o, t[a].s); pushSp(o); int y0 = nextNS(t, n, a + 1); pushSpan(o, trimSpan(t, y0, n - 1), 0); }
    else if (bestKind == 3) { pushW(o, L"This", 1); pushSp(o); int y0 = nextNS(t, n, a + 1); pushSpan(o, trimSpan(t, y0, n - 1), 0); }
    else {
        int y0 = nextNS(t, n, a + 1);
        if (bestKind == 6) y0 = nextNS(t, n, y0 + 1);
        if (bestKind == 9) y0 = a;
        pushWs(o, bestKind == 4 ? L"There," : bestKind == 5 ? L"This is because" : bestKind == 6 ? L"In the same way," :
                  bestKind == 7 ? L"At the same time," : bestKind == 8 ? L"That is when" : L"It is", 1);
        if (bestKind == 4 || bestKind == 6 || bestKind == 7) { Tok *lt = &o->t[o->n - 1]; /* "There," came in as one word: split the comma off */
            size_t L2 = wcslen(lt->s); if (L2 && lt->s[L2 - 1] == L',') { lt->s[L2 - 1] = 0; pushP(o, L','); } }
        pushSp(o); pushSpan(o, trimSpan(t, y0, n - 1), -1);
    }
    tCopy(o, &t[n - 1]);
    return 1;
}

static void rhythmPass(Toks *units, int *kind, int nu, int str, NanoStats *st) {
    static const double PJ[4] = { 0.0, 0.1, 0.15, 0.2 }, PS[4] = { 0.0, 0.35, 0.6, 0.8 };
    if (str < 1) return;
    int splitAt = 26, joinMax = 16;
    if (gStyle.on) { splitAt = (int)(gStyle.meanLen * 1.7); if (splitAt < 18) splitAt = 18; joinMax = (int)(gStyle.meanLen * 0.75); if (joinMax < 6) joinMax = 6; }
    static const wchar_t *NOSTART[] = { L"but", L"so", L"however", L"therefore", L"yet", L"still", L"also", L"because", L"although", L"then",
        L"instead", L"meanwhile", L"and", L"or", L"for", L"thus", L"even", L"in", L"on", L"at", L"after", L"before", L"if", L"when", NULL };
    {   /* burstiness: if sentence lengths are too even, split some long ones and leave a short piece behind */
        static const double TARGET[4] = { 0.0, 0.45, 0.55, 0.65 };
        double sum = 0, sq = 0; int cnt = 0;
        for (int u = 0; u < nu; u++) if (kind[u] == 1) { int w = sentWords(&units[u]); if (w >= 2) { sum += w; sq += (double)w * w; cnt++; } }
        if (cnt >= 3) {
            double mean = sum / cnt, sd = sqrt(sq / cnt - mean * mean);
            int prev = -1, budget = cnt / 3 + 1;
            for (int u = 0; u < nu && budget > 0 && sd / mean < TARGET[str]; u++) {
                if (kind[u] != 1) continue;
                int w = sentWords(&units[u]);
                int even = prev >= 0 && abs(w - prev) <= (int)(mean * 0.35);
                if (w >= 14 && (even || w > mean * 1.15) && nrnd() < 0.8) {
                    Toks o = {0};
                    int got = rhSplit(units[u].t, units[u].n, &o, nrnd() < 0.65 ? 1 : 0);
                    if (getenv("RHDBG")) fprintf(stderr, "[rh w=%d got=%d seen=%d ok=%d]\n", w, got, got ? sentSeen(&o) : -1, got ? rebuildOK(&units[u], &o) : -1);
                    if (got && !sentSeen(&o) && rebuildOK(&units[u], &o)) {
                        tFree(&units[u]); units[u] = o; st->restructured++; budget--;
                        Toks *nw = &units[u]; double w2 = sentWords(nw);
                        sum += 0; sq += 0; (void)w2;
                        /* recompute stats */
                        sum = 0; sq = 0; cnt = 0;
                        for (int z = 0; z < nu; z++) if (kind[z] == 1) {
                            Span sp = { units[z].t, units[z].n }; int dots = 0, ww = 0;
                            for (int q = 0; q < sp.n; q++) { if (sp.t[q].type == TK_WORD || sp.t[q].type == TK_LOCK) ww++; if (sp.t[q].type == TK_PUNCT && sp.t[q].s[0] == L'.') { if (ww >= 2) { sum += ww; sq += (double)ww * ww; cnt++; } ww = 0; dots++; } }
                            if (ww >= 2) { sum += ww; sq += (double)ww * ww; cnt++; }
                        }
                        mean = sum / cnt; sd = sqrt(fmax(0, sq / cnt - mean * mean));
                        prev = 4; continue;
                    }
                    tFree(&o);
                }
                prev = w;
            }
            /* still too even: join a pair of plain mid-length neighbours into one long sentence */
            static const wchar_t *NOSTART2[] = { L"but", L"so", L"however", L"therefore", L"yet", L"still", L"also", L"because", L"although", L"then",
                L"instead", L"meanwhile", L"and", L"or", L"for", L"thus", L"even", L"this", L"that", L"these", L"it", L"finally", L"first", L"by", L"in", L"as", L"when", L"if", NULL };
            int joins = cnt / 8 + 1;
            for (int u = 0; u + 2 < nu && joins > 0 && sd / mean < TARGET[str]; u++) {
                if (kind[u] != 1 || kind[u + 2] != 1 || units[u + 1].n != 1 || units[u + 1].t[0].type != TK_SPACE) continue;
                Toks *a = &units[u], *b = &units[u + 2];
                int wa = sentWords(a), wb = sentWords(b);
                if (wa < 7 || wb < 7 || wa + wb > 44 || wa + wb < mean * 1.6 || nrnd() > 0.6) continue;
                if (a->t[a->n - 1].type != TK_PUNCT || a->t[a->n - 1].s[0] != L'.' || sentShape(a->t, a->n) < 0 || sentShape(a->t, a->n) > 1 || sentShape(b->t, b->n) < 0 || sentShape(b->t, b->n) > 1) continue;
                if (!lowerable(&b->t[0]) || isWordIn(&b->t[0], NOSTART2) || b->t[0].type == TK_LOCK || wcschr(b->t[0].s, L' ')) continue;
                int bad = 0; Toks *ab[2] = { a, b };
                for (int z = 0; z < 2; z++) for (int q = 0; q + 2 < ab[z]->n; q++) if (ab[z]->t[q].type == TK_PUNCT && ab[z]->t[q].s[0] == L',' && (wordIs(&ab[z]->t[q + 2], L"and") || wordIs(&ab[z]->t[q + 2], L"but"))) bad = 1;
                if (bad) continue;
                Toks m = {0};
                Span sa = { a->t, a->n - 1 }, sb = { b->t, b->n };
                pushSpan(&m, sa, 0); pushP(&m, L','); pushSp(&m); pushW(&m, L"and", 1); pushSp(&m); pushSpan(&m, sb, -1);
                if (sentSeen(&m) || !rebuildOK(a, &m)) { tFree(&m); continue; }
                tFree(a); *a = m; tFree(&units[u + 1]); tFree(b);
                kind[u + 1] = kind[u + 2] = 2; st->restructured++; joins--;
                u += 2;
            }
        }
    }
    for (int u = 0; u < nu; u++) {
        if (kind[u] != 1) continue;
        Toks *a = &units[u];
        int wa = sentWords(a);
        /* split a long sentence */
        if (wa > splitAt && nrnd() < PS[str]) {
            int saveE = escLevel; escLevel = escLevel < 1 ? 1 : escLevel;
            static PatFn SP[] = { patSplitMore, patSplit, patVPSplit };
            for (int k = 0; k < 3; k++) {
                Toks o = {0};
                if (SP[k](a->t, a->n, &o) && !sentSeen(&o) && rebuildOK(a, &o)) { tFree(a); *a = o; st->restructured++; break; }
                tFree(&o);
            }
            escLevel = saveE;
            continue;
        }
        /* join two similar, short neighbours */
        if (u + 2 >= nu || kind[u + 2] != 1 || units[u + 1].n != 1 || units[u + 1].t[0].type != TK_SPACE) continue;
        Toks *b = &units[u + 2];
        int wb = sentWords(b);
        double pj = PJ[str] * (gStyle.on && gStyle.meanLen > 18 ? 2.5 : 1.0);
        if (wa < 3 || wb < 3 || wa > joinMax || wb > joinMax || abs(wa - wb) > 6 || nrnd() >= pj) continue;
        if (sentShape(a->t, a->n) != 0 || sentShape(b->t, b->n) < 0 || a->t[a->n - 1].s[0] != L'.') continue;
        if (!lowerable(&b->t[0]) || isWordIn(&b->t[0], NOSTART) || b->t[0].type == TK_LOCK || wcschr(b->t[0].s, L' ')) continue;
        { int c1 = nextNS(b->t, b->n, 1); if (c1 < b->n && b->t[c1].type == TK_PUNCT && b->t[c1].s[0] == L',') continue; }
        int bad = 0; Toks *ab[2] = { a, b };
        static const wchar_t *NOJ[] = { L"and", L"but", L"so", L"or", L"not", L"never", L"no", L"because", L"although", L"however", NULL };
        for (int z = 0; z < 2; z++) for (int q = 0; q < ab[z]->n; q++) if (isWordIn(&ab[z]->t[q], NOJ) || (ab[z]->t[q].type == TK_WORD && wcsstr(ab[z]->t[q].s, L"n't"))) bad = 1;
        if (bad) continue;
        Toks m = {0};
        Span sa = { a->t, a->n - 1 }, sb = { b->t, b->n };
        pushSpan(&m, sa, 0); pushP(&m, L','); pushSp(&m); pushW(&m, L"and", 1); pushSp(&m); pushSpan(&m, sb, -1);
        if (sentSeen(&m) || !rebuildOK(a, &m)) { tFree(&m); continue; }
        tFree(a); *a = m; tFree(&units[u + 1]); tFree(b);
        kind[u + 1] = kind[u + 2] = 2; st->restructured++;
        u += 2;
    }
}

static void restructurePass(Toks *v, const NanoOpts *op, NanoStats *st) {
    static const double P1[4] = { 0.3, 0.75, 1.0, 1.0 }, P2[4] = { 0.0, 0.3, 0.75, 0.9 }, PM[4] = { 0.0, 0.3, 0.6, 0.7 };
    int str = op->strength;
    static const int MAXPASS[4] = { 1, 2, 3, 1000000 };
    maxMode = str == 3;
    int allowRe = !curCtx || curCtx->pass <= MAXPASS[str], allowMerge = !curCtx || curCtx->pass <= 1 || maxMode;
    tagAll(v);
    /* split into units: 0 = separator run, 1 = sentence */
    Toks *units = NULL; int *kind = NULL, nu = 0, i = 0;
    while (i < v->n) {
        units = realloc(units, (nu + 1) * sizeof(Toks)); kind = realloc(kind, (nu + 1) * sizeof(int));
        memset(&units[nu], 0, sizeof(Toks));
        if (v->t[i].type == TK_SPACE || v->t[i].type == TK_NL) {
            while (i < v->n && (v->t[i].type == TK_SPACE || v->t[i].type == TK_NL)) tCopy(&units[nu], &v->t[i++]);
            kind[nu++] = 0; continue;
        }
        while (i < v->n && v->t[i].type != TK_NL) {
            if (isEndP(&v->t[i]) && !(v->t[i].s[0] == L'.' && i > 0 && abbrevTok(&v->t[i - 1])) &&
                (i + 1 >= v->n || v->t[i + 1].type != TK_WORD)) {
                while (i + 1 < v->n && isEndP(&v->t[i + 1])) tCopy(&units[nu], &v->t[i++]);
                tCopy(&units[nu], &v->t[i++]);
                break;
            }
            if (v->t[i].type == TK_LOCK) {
                const wchar_t *ls = v->t[i].s; int L2 = (int)wcslen(ls);
                int endsQ = L2 >= 2 && wcschr(L"\"\x201D'\x2019)", ls[L2 - 1]) && wcschr(L".!?", ls[L2 - 2]);
                int nx = i + 1; while (nx < v->n && v->t[nx].type == TK_SPACE) nx++;
                if (endsQ && nx > i + 1 && nx < v->n && (v->t[nx].type == TK_WORD || v->t[nx].type == TK_LOCK) && iswupper(v->t[nx].s[0])) {
                    tCopy(&units[nu], &v->t[i++]);
                    break;
                }
            }
            tCopy(&units[nu], &v->t[i++]);
        }
        kind[nu++] = 1;
    }
    int *done = calloc(nu + 1, sizeof(int));
    for (int u = 0; u < nu; u++) if (kind[u] == 1) sentRemember(units[u].t, units[u].n);
    for (int u = 0; u < nu; u++) {
        if (kind[u] != 1) continue;
        unsigned used = 0;
        gPrevOpener = 0;
        for (int pu = u - 1; pu >= 0; pu--) if (kind[pu] == 1) {
            Str h = {0}; sAdd(&h, L""); for (int q = 0; q < units[pu].n && q < 12; q++) sAdd(&h, units[pu].t[q].s);
            gPrevOpener = startsWithList(h.s, OPENLIST); free(h.s); break;
        }
        { Str h = {0}; sAdd(&h, L""); for (int q = 0; q < units[u].n && q < 12; q++) sAdd(&h, units[u].t[q].s);
          int self = startsWithList(h.s, OPENLIST); free(h.s); if (self) gPrevOpener = 1; }
        if (allowRe && nrnd() < P1[str] * moodR && restructureOnce(&units[u], &used)) {
            st->restructured++; done[u] = 1;
            if (nrnd() < P2[str]) restructureOnce(&units[u], &used);
        }
    }
    /* MAX: "This is not laziness. This is compression." -> "This is compression, not laziness." */
    for (int u = 0; maxMode && u + 2 < nu; u++) {
        if (kind[u] != 1 || kind[u + 2] != 1 || units[u + 1].n != 1 || units[u + 1].t[0].type != TK_SPACE) continue;
        Toks *a = &units[u], *b = &units[u + 2];
        if (sentShape(a->t, a->n) != 0 || sentShape(b->t, b->n) != 0) continue;
        int ba = -1, bb = -1;
        for (int q = 0; q < a->n; q++) if (a->t[q].tag == T_BE && isFiniteTok(a->t, q)) { ba = q; break; }
        for (int q = 0; q < b->n; q++) if (b->t[q].tag == T_BE && isFiniteTok(b->t, q)) { bb = q; break; }
        if (ba < 1 || bb < 1 || ba != bb) continue;
        int same = 1; for (int q = 0; q <= ba; q++) { wchar_t x[40], y[40]; x[0] = y[0] = 0; if (a->t[q].type == TK_WORD) wordL(&a->t[q], x); else wcscpy(x, a->t[q].s); if (b->t[q].type == TK_WORD) wordL(&b->t[q], y); else wcscpy(y, b->t[q].s); if (wcscmp(x, y)) same = 0; }
        int nt = nextNS(a->t, a->n, ba + 1);
        if (same && nt < a->n && (wordIs(&a->t[nt], L"a") || wordIs(&a->t[nt], L"an"))) {
            int nb2 = nextNS(b->t, b->n, bb + 1);
            Span x = trimSpan(a->t, nt, a->n - 1), y = trimSpan(b->t, nb2, b->n - 1);
            if (nb2 < b->n && (wordIs(&b->t[nb2], L"a") || wordIs(&b->t[nb2], L"an")) && spanWords(x) <= 5 && spanWords(y) <= 5) {
                Toks m = {0}; Span head = { a->t, ba + 1 };
                pushSpan(&m, head, 0); pushSp(&m); pushW(&m, L"both", 1); pushSp(&m); pushSpan(&m, x, 0); pushSp(&m); pushW(&m, L"and", 1); pushSp(&m); pushSpan(&m, y, 0);
                tCopy(&m, &b->t[b->n - 1]);
                if (sentSeen(&m)) { tFree(&m); continue; }
                tFree(a); *a = m; tFree(&units[u + 1]); tFree(b);
                kind[u + 1] = kind[u + 2] = 2; st->restructured++; done[u] = 1; u += 2; continue;
            }
        }
        if (!same || nt >= a->n || !wordIs(&a->t[nt], L"not")) continue;
        Span x = trimSpan(a->t, nt + 1, a->n - 1), y = trimSpan(b->t, bb + 1, b->n - 1);
        if (spanWords(x) < 1 || spanWords(y) < 1 || spanWords(x) > 6 || spanWords(y) > 8) continue;
        Toks m = {0};
        Span head = { b->t, bb + 1 };
        pushSpan(&m, head, 0); pushSp(&m); pushSpan(&m, y, 0); pushP(&m, L','); pushSp(&m); pushW(&m, L"not", 1); pushSp(&m); pushSpan(&m, x, 0);
        tCopy(&m, &b->t[b->n - 1]);
        if (sentSeen(&m)) { tFree(&m); continue; }
        tFree(a); *a = m; tFree(&units[u + 1]); tFree(b);
        kind[u + 1] = kind[u + 2] = 2; st->restructured++; done[u] = 1;
        u += 2;
    }
    /* merge short neighbours: "X. It Y." -> "X, and it Y." */
    for (int u = 0; allowMerge && u + 2 < nu; u++) {
        if (kind[u] != 1 || kind[u + 2] != 1 || units[u + 1].n != 1 || units[u + 1].t[0].type != TK_SPACE) continue;
        Toks *a = &units[u], *b = &units[u + 2];
        if (a->n < 3 || b->n < 3 || sentShape(a->t, a->n) < 0 || sentShape(b->t, b->n) < 0) continue;
        if (a->t[a->n - 1].s[0] != L'.' || sentWords(a) > 14 || sentWords(b) > 16) continue;
        static const wchar_t *MERGE[] = { L"it", L"they", L"this", L"he", L"she", L"these", NULL };
        if (!isWordIn(&b->t[0], MERGE) || nrnd() >= PM[str] || wcschr(b->t[0].s, L' ')) continue;
        { int c1 = nextNS(b->t, b->n, 1); if (c1 < b->n && b->t[c1].type == TK_PUNCT && b->t[c1].s[0] == L',') continue; }
        { int dup = 0; Toks *ab[2] = { a, b };
          for (int z = 0; z < 2; z++) for (int q = 0; q < ab[z]->n; q++) if (wordIs(&ab[z]->t[q], L"and") || wordIs(&ab[z]->t[q], L"but") || wordIs(&ab[z]->t[q], L"so")) dup = 1;
          if (dup) continue; }
        { int neg = 0; static const wchar_t *NEG[] = { L"not", L"never", L"no", NULL };
          for (int q = 0; q < a->n; q++) if (a->t[q].type == TK_WORD && (isWordIn(&a->t[q], NEG) || wcsstr(a->t[q].s, L"n't") || wcsstr(a->t[q].s, L"n\x2019t"))) neg = 1;
          if (neg) continue; }
        Toks m = {0};
        Span sa = { a->t, a->n - 1 }, sb = { b->t, b->n };
        pushSpan(&m, sa, 0); pushP(&m, L','); pushSp(&m); pushW(&m, L"and", 1); pushSp(&m);
        pushSpan(&m, sb, -1);
        if (sentSeen(&m)) { tFree(&m); continue; }
        tFree(a); *a = m;
        tFree(&units[u + 1]); tFree(b);
        kind[u + 1] = kind[u + 2] = 2;
        if (!done[u]) st->restructured++;
        u += 2;
    }
    rhythmPass(units, kind, nu, str, st);
    for (int u = 0; u < nu; u++) if (kind[u] == 1) sentRemember(units[u].t, units[u].n);
    Toks out = {0};
    for (int u = 0; u < nu; u++) {
        if (kind[u] != 2) for (int k = 0; k < units[u].n; k++) tCopy(&out, &units[u].t[k]);
        if (kind[u] != 2) tFree(&units[u]);
    }
    free(units); free(kind); free(done);
    tFree(v); *v = out;
}

static void openerCasePass(Toks *v) {
    int sentStart = 0;
    for (int i = 0; i < v->n; i++) {
        Tok *k = &v->t[i];
        if (k->type == TK_NL || (k->type == TK_PUNCT && isEndP(k))) { sentStart = i + 1; continue; }
        if (!(k->type == TK_PUNCT && k->s[0] == L',')) continue;
        Str h = {0}; sAdd(&h, L"");
        for (int q = sentStart; q < i; q++) sAdd(&h, v->t[q].s);
        const wchar_t *hp = h.s; while (*hp == L' ') hp++;
        int isOp = 0;
        for (int q = 0; OPENLIST[q]; q++) { int L2 = (int)wcslen(OPENLIST[q]); if ((int)wcslen(hp) == L2 && startsWithList(hp, OPENLIST)) { isOp = 1; break; } }
        free(h.s);
        int nx = i + 1; while (nx < v->n && v->t[nx].type == TK_SPACE) nx++;
        if (!isOp || nx >= v->n || v->t[nx].type != TK_WORD) continue;
        Tok *w = &v->t[nx];
        if (!iswupper(w->s[0]) || (w->s[1] && iswupper(w->s[1]))) continue;
        wchar_t lw[40]; int L2 = 0; while (w->s[L2] && w->s[L2] != L' ' && L2 < 39) L2++; lowerInto(w->s, L2, lw);
        if (!wcscmp(lw, L"i") || !wcsncmp(lw, L"i'", 2)) continue;
        if (isKnownWord(lw) || verbOf(lw)) w->s[0] = towlower(w->s[0]);
    }
}

/* register swaps applied after the normal rewrite: word -> word, exact forms */
static const wchar_t *CASUALMAP[] = {
    L"however", L"but", L"therefore", L"so", L"thus", L"so", L"purchase", L"buy", L"purchases", L"buys", L"purchased", L"bought", L"purchasing", L"buying",
    L"obtain", L"get", L"obtains", L"gets", L"obtained", L"got", L"assist", L"help", L"assists", L"helps", L"assisted", L"helped", L"assisting", L"helping",
    L"numerous", L"lots of", L"approximately", L"about", L"individuals", L"people", L"utilize", L"use", L"utilizes", L"uses", L"utilized", L"used",
    L"commence", L"start", L"commenced", L"started", L"terminate", L"end", L"sufficient", L"enough", L"require", L"need", L"requires", L"needs",
    L"required", L"needed", L"attempt", L"try", L"attempted", L"tried", L"demonstrate", L"show", L"demonstrates", L"shows", L"demonstrated", L"showed",
    L"regarding", L"about", L"prior to", L"before", L"in order to", L"to", L"reside", L"live", L"resides", L"lives", L"inquire", L"ask",
    L"extremely", L"really", L"children", L"kids", L"perhaps", L"maybe", L"additionally", L"also", L"furthermore", L"plus", L"nevertheless", L"still",
    L"consequently", L"so", L"subsequently", L"later", L"currently", L"right now", L"frequently", L"often", L"possess", L"have", L"possesses", L"has",
    L"remain", L"stay", L"remains", L"stays", L"remained", L"stayed", L"commenced", L"started", L"desire", L"want", L"desires", L"wants", NULL, NULL };
static const wchar_t *FORMALMAP[] = {
    L"kids", L"children", L"kid", L"child", L"buy", L"purchase", L"buys", L"purchases", L"bought", L"purchased", L"lots of", L"many",
    L"a lot of", L"many", L"gonna", L"going to", L"wanna", L"want to", L"kinda", L"somewhat", L"sorta", L"somewhat", L"really", L"truly",
    L"maybe", L"perhaps", L"pretty", L"fairly", L"okay", L"acceptable", L"ok", L"acceptable", L"stuff", L"material", L"guys", L"people",
    L"start", L"begin", L"starts", L"begins", L"started", L"began", L"show", L"demonstrate", L"shows", L"demonstrates", L"showed", L"demonstrated",
    L"need", L"require", L"needs", L"requires", L"needed", L"required", L"try", L"attempt", L"tried", L"attempted",
    L"enough", L"sufficient", L"about", L"approximately", L"anyway", L"regardless", L"also", L"additionally", L"plus", L"in addition", L"right now", L"currently",
    L"often", L"frequently", L"big", L"large", L"huge", L"substantial", NULL, NULL };
static const wchar_t *CASUALOPEN[] = { L"that said", L"but", L"as a result", L"so", L"for that reason", L"so", L"nevertheless", L"still",
    L"nonetheless", L"still", L"in addition", L"also", L"on top of that", L"also", L"additionally", L"also", L"moreover", L"also",
    L"furthermore", L"also", L"however", L"but", L"therefore", L"so", L"thus", L"so", L"consequently", L"so", L"in conclusion", L"so",
    L"as a matter of fact", L"actually", L"in reality", L"actually", L"subsequently", L"then", NULL, NULL };
static const wchar_t *FORMALOPEN[] = { L"also", L"in addition", L"plus", L"in addition", L"on top of that", L"in addition", L"still", L"nevertheless",
    L"actually", L"in fact", L"anyway", L"regardless", L"so", L"therefore", L"but", L"however", L"then", L"subsequently", NULL, NULL };
static const wchar_t *FORMALCONTR[] = { L"it'll", L"it will", L"we'll", L"we will", L"they'll", L"they will", L"I'll", L"I will", L"you'll", L"you will",
    L"didn't", L"did not", L"don't", L"do not", L"doesn't", L"does not", L"can't", L"cannot", L"won't", L"will not", L"isn't", L"is not",
    L"aren't", L"are not", L"wasn't", L"was not", L"weren't", L"were not", L"it's", L"it is", L"we're", L"we are", L"they're", L"they are",
    L"I'm", L"I am", L"you're", L"you are", L"couldn't", L"could not", L"wouldn't", L"would not", L"shouldn't", L"should not",
    L"haven't", L"have not", L"hasn't", L"has not", L"that's", L"that is", L"there's", L"there is", L"let's", L"let us",
    L"gotta", L"have to", L"gonna", L"going to", L"wanna", L"want to", NULL, NULL };
static const wchar_t *ACADMAP[] = { L"i think", L"it can be argued that", L"i believe", L"it can be argued that", L"shows", L"indicates",
    L"showed", L"indicated", L"a lot of", L"a considerable amount of", L"lots of", L"a considerable number of", L"big", L"substantial",
    L"very", L"highly", L"really", L"genuinely", NULL, NULL };
static const wchar_t *MULTIMAP[] = { L"a lot of", L"many", L"lots of", L"many", L"in order to", L"to", L"prior to", L"before", L"right now", L"currently", NULL, NULL };

static int matchWords(Toks *v, int i, const wchar_t *phrase, int *endTok) {
    /* compare a lowercase phrase against consecutive word tokens starting at i */
    const wchar_t *p = phrase; int k = i;
    while (*p) {
        while (k < v->n && v->t[k].type == TK_SPACE) k++;
        if (k >= v->n || v->t[k].type != TK_WORD) return 0;
        wchar_t w[64]; int L2 = (int)wcslen(v->t[k].s); if (L2 > 63) return 0; lowerInto(v->t[k].s, L2, w);
        int pl = (int)wcslen(p); if (pl < L2 || wcsncmp(p, w, L2) || (p[L2] && p[L2] != L' ')) return 0;
        p += L2; if (*p == L' ') p++;
        k++;
    }
    *endTok = k; return 1;
}
static void toneReplace(Toks *v, int i, int end, const wchar_t *rep, NanoStats *st) {
    wchar_t *nw = casedCopy(v->t[i].s, rep);
    if (iswupper(rep[0])) nw[0] = rep[0];
    free(v->t[i].s); v->t[i].s = nw; v->t[i].chg = 1;
    for (int q = i + 1; q < end; q++) { free(v->t[q].s); v->t[q].s = wdup(L""); v->t[q].type = TK_SPACE; }
    st->changed++;
}
static void tonePass(Toks *v, NanoStats *st) {
    int reg = toneReg();
    if (!reg) return;
    tagAll(v);
    for (int i = 0; i < v->n; i++) {
        Tok *k = &v->t[i];
        if (k->type != TK_WORD) continue;
        int q = i - 1; while (q >= 0 && v->t[q].type == TK_SPACE) q--;
        int sentStart = q < 0 || v->t[q].type == TK_NL || (v->t[q].type == TK_PUNCT && wcschr(L".!?", v->t[q].s[0]));
        int end;
        /* sentence openers */
        if (sentStart) {
            const wchar_t **O = reg < 0 ? CASUALOPEN : FORMALOPEN; int done = 0;
            for (int m = 0; O[m]; m += 2) {
                if (!matchWords(v, i, O[m], &end)) continue;
                int c = end; while (c < v->n && v->t[c].type == TK_SPACE) c++;
                int comma = c < v->n && v->t[c].type == TK_PUNCT && v->t[c].s[0] == L',';
                const wchar_t *rep = O[m + 1];
                int repNeedsComma = reg > 0 || !wcscmp(rep, L"still") || !wcscmp(rep, L"actually") || !wcscmp(rep, L"also");
                if (!comma && !(reg > 0 && (!wcscmp(O[m], L"so") || !wcscmp(O[m], L"but")))) break;
                toneReplace(v, i, end, rep, st);
                if (comma && !repNeedsComma) { free(v->t[c].s); v->t[c].s = wdup(L""); v->t[c].type = TK_SPACE; }
                if (!comma && repNeedsComma) {
                    Tok cm = { wdup(L","), TK_PUNCT, 1, T_PUNCT };
                    if (v->n + 1 > v->cap) { v->cap = v->cap ? v->cap * 2 : 16; v->t = realloc(v->t, v->cap * sizeof(Tok)); }
                    memmove(&v->t[i + 2], &v->t[i + 1], (v->n - i - 1) * sizeof(Tok)); v->t[i + 1] = cm; v->n++;
                }
                done = 1; break;
            }
            if (done) continue;
        }
        if (reg > 0) {
            int hit = 0;
            for (int m = 0; FORMALCONTR[m] && !hit; m += 2) {
                wchar_t lc[24]; wcscpy(lc, FORMALCONTR[m]); for (wchar_t *z = lc; *z; z++) *z = towlower(*z);
                if (matchWords(v, i, lc, &end)) { toneReplace(v, i, end, FORMALCONTR[m + 1], st); hit = 1; }
            }
            if (hit) continue;
            if (gTone == TN_ACADEMIC || gType == WT_REPORT)
                for (int m = 0; ACADMAP[m] && !hit; m += 2) if (matchWords(v, i, ACADMAP[m], &end)) { toneReplace(v, i, end, ACADMAP[m + 1], st); hit = 1; }
            if (hit) continue;
            for (int m = 0; MULTIMAP[m] && !hit; m += 2) if (wcschr(MULTIMAP[m], L' ') && matchWords(v, i, MULTIMAP[m], &end)) {
                const wchar_t *rep = MULTIMAP[m + 1];
                if (!wcscmp(rep, L"many")) {
                    int plural = 0, c = end;
                    for (int z = 0; z < 4 && c < v->n; c++) { if (v->t[c].type == TK_SPACE) continue; if (v->t[c].type != TK_WORD) break; if (v->t[c].tag == T_NS) { plural = 1; break; } if (v->t[c].tag == T_N) break; z++; }
                    if (!plural) rep = L"a great deal of";
                }
                toneReplace(v, i, end, rep, st); hit = 1;
            }
            if (hit) continue;
        } else {
            int hit = 0;
            for (int m = 0; MULTIMAP[m] && !hit; m += 2) if (!wcscmp(MULTIMAP[m], L"in order to") || !wcscmp(MULTIMAP[m], L"prior to")) if (matchWords(v, i, MULTIMAP[m], &end)) { toneReplace(v, i, end, MULTIMAP[m + 1], st); hit = 1; }
            if (hit) continue;
        }
        const wchar_t **M = reg < 0 ? CASUALMAP : FORMALMAP;
        int nx = i + 1; while (nx < v->n && v->t[nx].type == TK_SPACE) nx++;
        wchar_t w[48]; int L2 = (int)wcslen(k->s); if (L2 > 47) continue; lowerInto(k->s, L2, w);
        if (reg > 0) {
            if (!wcscmp(w, L"about") && !(nx < v->n && (lockHasDigit(&v->t[nx]) || v->t[nx].tag == T_NUM))) continue;
            if ((!wcscmp(w, L"get") || !wcscmp(w, L"gets")) && !(nx < v->n && isWordIn(&v->t[nx], (const wchar_t *[]){ L"a", L"an", L"the", L"some", L"more", NULL }))) continue;
            if (!wcscmp(w, L"pretty") && !(nx < v->n && (v->t[nx].tag == T_ADJ || v->t[nx].tag == T_ADV))) continue;
            if (!wcscmp(w, L"enough") && !(nx < v->n && (v->t[nx].tag == T_N || v->t[nx].tag == T_NS))) continue;
            if (!wcscmp(w, L"really") && !(nx < v->n && (v->t[nx].tag == T_ADJ || v->t[nx].tag == T_ADV))) continue;
            if (!wcscmp(w, L"plus") || !wcscmp(w, L"also") || !wcscmp(w, L"so") || !wcscmp(w, L"but")) continue;
        } else {
            if (!wcscmp(w, L"however") || !wcscmp(w, L"therefore") || !wcscmp(w, L"thus")) continue;
            if (gTone == TN_SIMPLE && (!wcscmp(w, L"kids") || !wcscmp(w, L"extremely"))) continue;
            if (gTone == TN_SIMPLE || gType == WT_EMAIL) {
                static const wchar_t *SLANG[] = { L"gonna", L"going to", L"wanna", L"want to", L"gotta", L"have to", L"kinda", L"kind of", L"sorta", L"sort of", NULL, NULL };
                int hit = 0; for (int m = 0; SLANG[m]; m += 2) if (!wcscmp(w, SLANG[m])) { toneReplace(v, i, i + 1, SLANG[m + 1], st); hit = 1; break; }
                if (hit) continue;
            }
        }
        for (int m = 0; M[m]; m += 2) if (!wcschr(M[m], L' ') && !wcscmp(w, M[m])) {
            if (gTone == TN_SIMPLE && (!wcscmp(M[m], L"children") || !wcscmp(M[m], L"extremely"))) break;
            toneReplace(v, i, i + 1, M[m + 1], st); break;
        }
    }
}

/* mood: each run leans a little emphatic, hedged or relaxed */
static int gMood, gMoodBase;
static const wchar_t *MOODADV[] = { L"really", L"very", L"truly", L"fairly", L"somewhat", L"rather", L"pretty", L"quite", L"a bit", L"probably", L"likely", NULL };
static int countMoodAdv(const wchar_t *t) {
    int c = 0, n = (int)wcslen(t);
    for (int i = 0; i < n; i++) {
        if (i > 0 && iswalpha(t[i - 1])) continue;
        for (int k = 0; MOODADV[k]; k++) { int L2 = (int)wcslen(MOODADV[k]); if (!wcsncmp(t + i, MOODADV[k], L2) && (i + L2 >= n || !iswalpha(t[i + L2]))) { c++; break; } }
    }
    return c;
}
static void moodPass(Toks *v, int str, int curCount) {
    if (!gMood || str < 1) return;
    int sents = gSents > 0 ? gSents : 1;
    int budget = gMoodBase + 1 + sents / 8 - curCount;
    if (budget <= 0) return;
    int reg = toneReg();
    for (int i = 0; i + 2 < v->n && budget > 0; i++) {
        Tok *k = &v->t[i];
        if (k->type != TK_WORD || nrnd() > 0.35) continue;
        int a = i + 1; while (a < v->n && v->t[a].type == TK_SPACE) a++;
        if (a >= v->n || v->t[a].type != TK_WORD || wcschr(v->t[a].s, L' ')) continue;
        const wchar_t *adv = NULL;
        if (k->tag == T_BE && isFiniteTok(v->t, i) && v->t[a].tag == T_ADJ) {
            int b2 = a + 1; while (b2 < v->n && v->t[b2].type == TK_SPACE) b2++;
            if (b2 < v->n && (v->t[b2].tag == T_N || v->t[b2].tag == T_NS)) continue;
            if (isWordIn(&v->t[a], (const wchar_t *[]){ L"able", L"unable", L"sure", L"aware", L"alone", L"alive", L"dead", L"true", L"false", L"right", L"wrong", L"possible", L"impossible", L"certain", L"same", L"only", L"first", L"last", L"next", L"other", L"such", L"ready", L"done", L"finished", L"gone", L"full", L"empty", L"open", L"closed", NULL })) continue;
            if (gMood == 1) adv = reg < 0 ? L"really" : reg > 0 ? L"very" : (nri(2) ? L"really" : L"very");
            else if (gMood == 2) adv = reg > 0 ? L"somewhat" : (nri(2) ? L"fairly" : L"rather");
            else adv = reg > 0 ? L"quite" : (nri(2) ? L"pretty" : L"quite");
        } else if (gMood == 2 && wordIs(k, L"will") && isVerbTag(v->t[a].tag) && v->t[a].tag == T_VB) {
            adv = reg < 0 ? L"probably" : L"likely";
        } else continue;
        Tok nt = { wdup(adv), TK_WORD, 1, T_ADV }, sp = { wdup(L" "), TK_SPACE, 0, -1 };
        if (v->n + 2 > v->cap) { v->cap = (v->cap ? v->cap : 16) * 2 + 2; v->t = realloc(v->t, v->cap * sizeof(Tok)); }
        memmove(&v->t[a + 2], &v->t[a], (v->n - a) * sizeof(Tok));
        v->t[a] = nt; v->t[a + 1] = sp; v->n += 2;
        budget--; i = a + 2;
    }
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
    if (curCtx) curCtx->pass++;
    if (!curCtx || curCtx->pass == 1) {
        moodW = 0.8 + 0.4 * nrnd(); moodR = 0.75 + 0.5 * nrnd();
        double r = nrnd(); gMood = r < 0.2 ? 0 : r < 0.47 ? 1 : r < 0.73 ? 2 : 3;
        ssFree(&gChoice.keys); free(gChoice.val); memset(&gChoice, 0, sizeof gChoice);
        gMoodBase = countMoodAdv(text);
    }
    if (curCtx) { if (!curCtx->contrMode) curCtx->contrMode = contrMode; contrMode = curCtx->contrMode; }
    gTone = op->tone; gType = op->type;
    if (op->style && op->style[0]) styleBuild(op->style); else gStyle.on = 0;
    if (gStyle.on) { contrMode = gStyle.contr100 < 0.4 ? 1 : gStyle.contr100 > 1.5 ? 2 : contrMode; }
    if (!gStyle.on) { if (toneReg() < 0) contrMode = 2; else if (toneReg() > 0) contrMode = 1; }
    textBudget(text);
    restructurePass(&v, op, st);
    swapPass(&v, op, st, contrMode);
    tonePass(&v, st);
    moodPass(&v, op->strength, countMoodAdv(text));
    articlePass(&v);
    openerCasePass(&v);
    Str out = {0}; sAdd(&out, L"");
    for (int i = 0; i < v.n; i++) sAdd(&out, v.t[i].s);
    tFree(&v); free(L);
    return out.s;
}
static TagEnt *tagLookupFwd(const char *w) { return tagLookup(tw, ntw, w); }
static int commonLowerWord(const wchar_t *lw) {
    char nb[48]; int k = 0; for (; lw[k] && k < 47; k++) { if (lw[k] > 127) return 0; nb[k] = (char)lw[k]; } nb[k] = 0;
    TagEnt *e = tagLookup(tw, ntw, nb);
    if (!e) return 0;
    int best = 0; for (int q = 1; q < e->nt; q++) if (e->sc[q] > e->sc[best]) best = q;
    return e->tag[best] != T_NP;
}
