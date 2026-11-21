
/* ---------- AI detector + grammar checker (pure algorithms, offline) ---------- */

enum { IS_GRAM, IS_SPELL, IS_PUNCT, IS_STYLE, IS_AI };
static const wchar_t *ISNAME[] = { L"Grammar", L"Spelling", L"Punctuation", L"Style", L"AI phrase" };

typedef struct { int s, len, kind; wchar_t msg[120]; } Issue;
typedef struct {
    int score, words, sents, paras, nGram, nAiHits, ok, pModel, pAlgo;
    double burst, aiPer100, transFrac, contr100, triad, awl, casual100, err100;
    Issue *iss; int nIss, capIss;
    wchar_t verdict[64], conf[64];
    wchar_t sig[6][96]; int nSig;
} Detect;

typedef struct { int s, len, sent; wchar_t l[40]; } DW;
#define DT_NF 40
static double *dtF;
static int knownW(const wchar_t *w);
static int lowerStr(const wchar_t *s, int n, wchar_t *o, int cap);

/* tokens for the trained model: words (unknown words -> <unk>, numbers -> <num>) and punctuation */
static int dtTokens(const wchar_t *t, wchar_t ***out) {
    int n = (int)wcslen(t), cap = 256, k = 0;
    wchar_t **v = malloc(cap * sizeof *v);
    for (int i = 0; i < n;) {
        wchar_t c = t[i], tk[48]; tk[0] = 0;
        if (iswalnum(c)) {
            int j = i, dig = 0;
            while (j < n && (iswalnum(t[j]) || (isApos(t[j]) && j + 1 < n && iswalpha(t[j + 1])))) { if (iswdigit(t[j])) dig = 1; j++; }
            if (dig) wcscpy(tk, L"<num>");
            else { lowerStr(t + i, j - i, tk, 40); if (j - i >= 40 || !knownW(tk)) wcscpy(tk, L"<unk>"); }
            i = j;
        } else if (c == L'\n') {
            int j = i, nl = 0; while (j < n && iswspace(t[j])) { if (t[j] == L'\n') nl++; j++; }
            if (nl >= 2) wcscpy(tk, L"<p>");
            i = j;
        } else {
            if (c == 0x2014 || c == 0x2013 || (c == L'-' && i + 1 < n && t[i + 1] == L'-')) { wcscpy(tk, L"—"); i += (c == L'-') ? 2 : 1; }
            else if (c == 0x201C || c == 0x201D || c == L'"') { wcscpy(tk, L"\""); i++; }
            else if (wcschr(L".,!?;:()-'", c) || c == 0x2019 || c == 0x2018) { tk[0] = isApos(c) ? L'\'' : c; tk[1] = 0; i++; }
            else i++;
        }
        if (!tk[0]) continue;
        if (k == cap) { cap *= 2; v = realloc(v, cap * sizeof *v); }
        v[k++] = wdup(tk);
    }
    *out = v; return k;
}

static void dtAdd(Detect *d, int s, int len, int kind, const wchar_t *fmt, ...) {
    if (d->nIss >= 400 && kind != IS_AI) return;
    for (int i = 0; i < d->nIss; i++) if (d->iss[i].s == s && d->iss[i].kind == kind) return;
    if (d->nIss == d->capIss) { d->capIss = d->capIss ? d->capIss * 2 : 64; d->iss = realloc(d->iss, d->capIss * sizeof(Issue)); }
    Issue *k = &d->iss[d->nIss++];
    k->s = s; k->len = len; k->kind = kind;
    va_list ap; va_start(ap, fmt); vswprintf(k->msg, 120, fmt, ap); va_end(ap);
    if (kind != IS_AI) d->nGram++;
}
static void dtFree(Detect *d) { free(d->iss); memset(d, 0, sizeof *d); }

static int dwIsLetter(wchar_t c) { return iswalpha(c) != 0; }
static int wIn(const wchar_t *w, const wchar_t **L) { for (; *L; L++) if (!wcscmp(w, *L)) return 1; return 0; }

static int knownNarrow(const wchar_t *w) {
    char b[48]; int n = 0;
    for (; w[n] && n < 47; n++) { if (w[n] > 127) return 1; b[n] = (char)w[n]; }
    b[n] = 0;
    return tagLookup(tw, ntw, b) != NULL;
}
static int knownW(const wchar_t *w) {
    if (knownNarrow(w) || isKnownWord(w) || smGet(&vfIdx, w) >= 0) return 1;
    int n = (int)wcslen(w); wchar_t b[48];
    static const wchar_t *SUF[] = { L"s", L"es", L"ed", L"d", L"ing", L"ly", L"er", L"est", L"ness", L"ment", L"ful", L"less", L"able", L"ers", L"ings", NULL };
    for (int k = 0; SUF[k]; k++) {
        int sl = (int)wcslen(SUF[k]);
        if (n - sl < 3 || wcscmp(w + n - sl, SUF[k])) continue;
        wcsncpy(b, w, n - sl); b[n - sl] = 0;
        if (knownNarrow(b) || isKnownWord(b)) return 1;
        if (n - sl >= 3 && b[n - sl - 1] == b[n - sl - 2]) { b[n - sl - 1] = 0; if (knownNarrow(b)) return 1; }
        b[n - sl] = L'e'; b[n - sl + 1] = 0; if (knownNarrow(b)) return 1;
        if (b[n - sl - 1] == L'i') { b[n - sl - 1] = L'y'; b[n - sl] = 0; if (knownNarrow(b)) return 1; }
    }
    if (n > 5 && !wcscmp(w + n - 2, L"ly")) { wcscpy(b, w); b[n - 1] = L'e'; if (knownNarrow(b)) return 1; }
    if (n > 4 && !wcsncmp(w, L"un", 2) && knownNarrow(w + 2)) return 1;
    if (n > 4 && !wcsncmp(w, L"re", 2) && knownNarrow(w + 2)) return 1;
    return 0;
}
static int typoSuggest(const wchar_t *w, wchar_t *out) {
    int n = (int)wcslen(w); wchar_t c[48];
    for (int i = 0; i + 1 < n; i++) { wcscpy(c, w); wchar_t t = c[i]; c[i] = c[i + 1]; c[i + 1] = t; if (knownNarrow(c)) { wcscpy(out, c); return 1; } }
    if (n > 7) return 0;
    for (int i = 0; i < n; i++) { int k = 0; for (int j = 0; j < n; j++) if (j != i) c[k++] = w[j]; c[k] = 0; if (k >= 3 && knownNarrow(c)) { wcscpy(out, c); return 1; } }
    for (int i = 0; i <= n && n < 40; i++)
        for (wchar_t ch = L'a'; ch <= L'z'; ch++) {
            int k = 0; for (int j = 0; j < i; j++) c[k++] = w[j]; c[k++] = ch; for (int j = i; j < n; j++) c[k++] = w[j]; c[k] = 0;
            if (knownNarrow(c)) { wcscpy(out, c); return 1; }
        }
    for (int i = 1; i < n; i++)
        for (wchar_t ch = L'a'; ch <= L'z'; ch++) {
            if (ch == w[i]) continue;
            wcscpy(c, w); c[i] = ch; if (knownNarrow(c)) { wcscpy(out, c); return 1; }
        }
    return 0;
}

static const wchar_t *MISSP[] = {
    L"alot", L"a lot", L"definately", L"definitely", L"definatly", L"definitely", L"seperate", L"separate", L"recieve", L"receive",
    L"occured", L"occurred", L"untill", L"until", L"wich", L"which", L"becuase", L"because", L"becasue", L"because", L"thier", L"their",
    L"beleive", L"believe", L"goverment", L"government", L"tommorow", L"tomorrow", L"tommorrow", L"tomorrow", L"accomodate", L"accommodate",
    L"wierd", L"weird", L"truely", L"truly", L"arguement", L"argument", L"neccessary", L"necessary", L"necesary", L"necessary",
    L"publically", L"publicly", L"noticable", L"noticeable", L"existance", L"existence", L"calender", L"calendar", L"begining", L"beginning",
    L"enviroment", L"environment", L"occurence", L"occurrence", L"refered", L"referred", L"sucess", L"success", L"succesful", L"successful",
    L"teh", L"the", L"adn", L"and", L"irregardless", L"regardless", L"alright", L"all right", L"wanst", L"wasn't", L"dont", L"don't",
    L"doesnt", L"doesn't", L"didnt", L"didn't", L"cant", L"can't", L"wont", L"won't", L"isnt", L"isn't", L"wasnt", L"wasn't",
    L"shouldnt", L"shouldn't", L"couldnt", L"couldn't", L"wouldnt", L"wouldn't", L"im", L"I'm", L"ive", L"I've", L"youre", L"you're",
    L"theyre", L"they're", L"thats", L"that's", L"whats", L"what's", L"lets", NULL, L"hes", L"he's", L"shes", L"she's",
    L"recieved", L"received", L"acheive", L"achieve", L"acheived", L"achieved", L"belive", L"believe", L"basicly", L"basically",
    L"buisness", L"business", L"concious", L"conscious", L"embarass", L"embarrass", L"finaly", L"finally", L"foriegn", L"foreign",
    L"freind", L"friend", L"garantee", L"guarantee", L"gaurd", L"guard", L"harrass", L"harass", L"independant", L"independent",
    L"knowlege", L"knowledge", L"libary", L"library", L"lisence", L"license", L"maintainance", L"maintenance", L"millenium", L"millennium",
    L"mispell", L"misspell", L"occassion", L"occasion", L"persistant", L"persistent", L"posession", L"possession", L"prefered", L"preferred",
    L"realy", L"really", L"reccomend", L"recommend", L"recomend", L"recommend", L"relevent", L"relevant", L"rythm", L"rhythm",
    L"similiar", L"similar", L"speach", L"speech", L"stoped", L"stopped", L"supercede", L"supersede", L"suprise", L"surprise",
    L"tounge", L"tongue", L"truley", L"truly", L"usefull", L"useful", L"wether", L"whether", L"writting", L"writing", L"youll", L"you'll",
    L"probaly", L"probably", L"probly", L"probably", L"diffrent", L"different", L"intresting", L"interesting", L"beautifull", L"beautiful",
    L"comming", L"coming", L"exersize", L"exercise", L"excercise", L"exercise", L"familar", L"familiar", L"fourty", L"forty",
    L"grammer", L"grammar", L"heighth", L"height", L"hygene", L"hygiene", L"immediatly", L"immediately", L"judgement", NULL,
    L"lenght", L"length", L"neice", L"niece", L"peice", L"piece", L"pronounciation", L"pronunciation", L"questionaire", L"questionnaire",
    L"remeber", L"remember", L"rember", L"remember", L"sieze", L"seize", L"thru", L"through", L"tho", L"though", L"untill", L"until",
    NULL, NULL };

typedef struct { const wchar_t *a, *b; const wchar_t *fix; int kind; } Bigram;
static const wchar_t *MODALS_OF[] = { L"could", L"should", L"would", L"must", L"might", L"may", NULL };
static const wchar_t *ITS_NEXT[] = { L"a", L"an", L"the", L"not", L"been", L"going", L"ok", L"okay", L"time", L"so", L"very", L"really", L"just",
                                     L"important", L"clear", L"true", L"hard", L"easy", L"about", L"fine", L"too", L"no", L"all", L"getting", NULL };
static const wchar_t *YOUR_NEXT[] = { L"welcome", L"not", L"going", L"gonna", L"being", L"kidding", L"so", L"very", L"really", L"doing", L"getting", NULL };
static const wchar_t *COMPAR[] = { L"better", L"more", L"less", L"rather", L"larger", L"smaller", L"faster", L"greater", L"higher", L"lower",
                                   L"bigger", L"older", L"younger", L"worse", L"fewer", L"longer", L"shorter", L"easier", L"harder", L"other", NULL };
static const wchar_t *DABBR[] = { L"mr", L"mrs", L"ms", L"dr", L"st", L"vs", L"etc", L"jr", L"sr", L"prof", L"inc", L"ltd", L"co", L"no", L"fig",
                                 L"approx", L"e.g", L"i.e", L"u.s", L"a.m", L"p.m", L"mt", L"ave", L"dept", L"est", L"gen", L"gov", L"lt", L"sgt", L"vol", NULL };

static const wchar_t *AIW[] = {
    L"delve", L"delves", L"delving", L"tapestry", L"testament", L"intricate", L"intricacies", L"multifaceted", L"pivotal", L"crucial",
    L"foster", L"fostering", L"fosters", L"leverage", L"leveraging", L"seamless", L"seamlessly", L"robust", L"realm", L"landscape",
    L"navigate", L"navigating", L"underscore", L"underscores", L"underscoring", L"showcase", L"showcases", L"showcasing", L"vibrant",
    L"nuanced", L"nuances", L"comprehensive", L"furthermore", L"moreover", L"additionally", L"embark", L"embarking", L"unlock", L"unlocking",
    L"elevate", L"elevating", L"harness", L"harnessing", L"resonate", L"resonates", L"profound", L"meticulous", L"meticulously", L"paramount",
    L"holistic", L"synergy", L"thrive", L"thriving", L"enhance", L"enhancing", L"insights", L"invaluable", L"commendable", L"endeavor",
    L"endeavors", L"captivating", L"bustling", L"whimsical", L"beacon", L"symphony", L"interplay", L"transformative", L"unwavering",
    L"noteworthy", L"notably", L"intriguing", L"empower", L"empowering", L"streamline", L"streamlined", L"utilize", L"utilizing",
    L"facilitate", L"facilitates", L"optimize", L"cornerstone", L"ever-evolving", L"ever-changing", L"game-changer", L"groundbreaking",
    L"innovative", L"dynamic", L"captivate", L"enigmatic", L"labyrinth", L"labyrinthine", L"journey", L"navigate", L"illuminate",
    L"illuminating", L"evocative", L"poignant", L"indelible", L"myriad", L"plethora", L"bolster", L"bolstering", L"encompass",
    L"encompasses", L"encompassing", L"overarching", L"in-depth", L"crucially", L"ultimately", L"essentially", L"arguably", NULL };
static const wchar_t *AIP[] = {
    L"it's important to note", L"it is important to note", L"it's worth noting", L"it is worth noting", L"plays a crucial role",
    L"plays a vital role", L"plays a pivotal role", L"a testament to", L"in today's", L"in conclusion", L"in summary", L"to sum up",
    L"not only", L"rich tapestry", L"stands as", L"serves as a", L"a wide range of", L"a wide array of", L"in the realm of",
    L"when it comes to", L"at the end of the day", L"navigate the", L"the world of", L"dive into", L"dive deeper", L"let's dive",
    L"whether you're", L"in this article", L"in this essay", L"overall,", L"on the other hand", L"it's essential", L"it is essential",
    L"it's crucial", L"it is crucial", L"fast-paced", L"due to the fact", L"each and every", L"first and foremost",
    L"the importance of", L"a deeper understanding", L"valuable insights", L"key takeaways", L"embark on", L"strike a balance",
    L"shed light on", L"sheds light on", L"paving the way", L"pave the way", L"evolving landscape", L"digital age", L"in essence",
    L"i hope this", L"feel free to", L"let me know if", L"as an ai", L"i'd be happy to", L"great question", NULL };
static const wchar_t *DTRANS[] = { L"furthermore", L"moreover", L"additionally", L"however", L"overall", L"ultimately", L"in conclusion",
                                  L"in summary", L"in addition", L"consequently", L"therefore", L"thus", L"firstly", L"secondly", L"lastly",
                                  L"finally", L"importantly", L"notably", L"similarly", L"conversely", L"nevertheless", L"indeed", NULL };
static const wchar_t *CASUAL[] = { L"lol", L"lmao", L"gonna", L"wanna", L"kinda", L"sorta", L"gotta", L"yeah", L"yep", L"nah", L"ok", L"okay",
                                   L"dunno", L"stuff", L"tbh", L"idk", L"btw", L"omg", L"ugh", L"huh", L"cuz", L"u", L"ur", L"bro", L"dude",
                                   L"literally", L"pretty", L"super", L"honestly", L"basically", L"like", L"guess", L"anyway", L"whatever", NULL };

static int lowerStr(const wchar_t *s, int n, wchar_t *o, int cap) {
    int k = 0; for (int i = 0; i < n && k < cap - 1; i++) o[k++] = isApos(s[i]) ? L'\'' : (wchar_t)towlower(s[i]);
    o[k] = 0; return k;
}

static int onlySpaces(const wchar_t *t, int a, int b) { for (int i = a; i < b; i++) if (t[i] != L' ' && t[i] != L'\t') return 0; return a < b; }

static void dtAnalyze(const wchar_t *t, Detect *d) {
    nanoInit();
    memset(d, 0, sizeof *d);
    int n = (int)wcslen(t);
    unsigned char *url = calloc(n + 1, 1);
    for (int i = 0; i < n; i++) {
        if (!wcsncmp(t + i, L"http", 4) || !wcsncmp(t + i, L"www.", 4) || (t[i] == L'@' && i > 0 && iswalnum(t[i - 1]))) {
            int a = i; while (a > 0 && !iswspace(t[a - 1])) a--;
            int b = i; while (b < n && !iswspace(t[b])) b++;
            memset(url + a, 1, b - a); i = b;
        }
    }
    DW *w = NULL; int nw = 0, cw = 0;
    int *sStart = NULL, *sLen = NULL, ns = 0, cs = 0;
    int sent = 0, sentWordsN = 0, paraWords = 0, nPara = 0; double paraSum = 0, paraSq = 0;
    int curSentStartChar = -1;
    #define ENDSENT() do { if (sentWordsN) { if (ns == cs) { cs = cs ? cs * 2 : 64; sStart = realloc(sStart, cs * sizeof(int)); sLen = realloc(sLen, cs * sizeof(int)); } \
        sStart[ns] = nw - sentWordsN; sLen[ns] = sentWordsN; ns++; sent++; sentWordsN = 0; curSentStartChar = -1; } } while (0)
    #define ENDPARA() do { ENDSENT(); if (paraWords) { nPara++; paraSum += paraWords; paraSq += (double)paraWords * paraWords; paraWords = 0; } } while (0)
    for (int i = 0; i < n;) {
        wchar_t c = t[i];
        if (dwIsLetter(c) || (iswdigit(c))) {
            int j = i;
            while (j < n && (iswalnum(t[j]) || ((isApos(t[j]) || t[j] == L'-') && j + 1 < n && iswalnum(t[j + 1]) && j > i))) j++;
            if (nw == cw) { cw = cw ? cw * 2 : 256; w = realloc(w, cw * sizeof(DW)); }
            DW *k = &w[nw++]; k->s = i; k->len = j - i; k->sent = sent; lowerStr(t + i, j - i, k->l, 40);
            if (curSentStartChar < 0) curSentStartChar = i;
            sentWordsN++; paraWords++;
            i = j; continue;
        }
        if (c == L'\n') {
            int j = i; while (j < n && (t[j] == L'\n' || t[j] == L' ' || t[j] == L'\t')) j++;
            int nlc = 0; for (int k = i; k < j; k++) if (t[k] == L'\n') nlc++;
            if (nlc >= 2) ENDPARA(); else ENDSENT();
            i = j; continue;
        }
        if ((c == L'.' || c == L'!' || c == L'?') && !url[i]) {
            int j = i; while (j < n && (t[j] == L'.' || t[j] == L'!' || t[j] == L'?')) j++;
            int k = j; while (k < n && (t[k] == L'"' || t[k] == L'\'' || t[k] == 0x201D || t[k] == 0x2019 || t[k] == L')')) k++;
            int isEnd = k >= n || iswspace(t[k]);
            if (isEnd && c == L'.' && j - i == 1 && nw && w[nw - 1].s + w[nw - 1].len == i) {
                if (wIn(w[nw - 1].l, DABBR) || (w[nw - 1].len == 1 && iswupper(t[w[nw - 1].s]))) isEnd = 0;
                if (i > 2 && t[i - 2] == L'.') isEnd = 0;
            }
            if (isEnd) ENDSENT();
            i = j; continue;
        }
        i++;
    }
    ENDPARA();
    #undef ENDSENT
    #undef ENDPARA
    d->words = nw; d->sents = ns; d->paras = nPara;

    /* ---- grammar / spelling / punctuation ---- */
    for (int i = 0; i < nw; i++) {
        DW *a = &w[i];
        if (url[a->s]) continue;
        const wchar_t *src = t + a->s;
        int adjPrev = i > 0 && w[i - 1].sent == a->sent && onlySpaces(t, w[i - 1].s + w[i - 1].len, a->s);
        if (adjPrev && !wcscmp(a->l, w[i - 1].l) && !iswdigit(a->l[0]) && !(iswupper(src[0]) && iswupper(t[w[i - 1].s]) && i > 1 && w[i - 2].sent == a->sent)) {
            static const wchar_t *OKREP[] = { L"had", L"that", L"ha", L"bye", L"no", L"so", L"very", L"really", L"ho", L"la", L"yeah", L"knock", L"boo", L"is", L"bla", L"blah", L"tsk", L"choo", L"go", L"well", NULL };
            if (!wIn(a->l, OKREP)) dtAdd(d, w[i - 1].s, a->s + a->len - w[i - 1].s, IS_GRAM, L"Repeated word: \"%ls %ls\"", a->l, a->l);
        }
        if ((!wcscmp(a->l, L"a") || !wcscmp(a->l, L"an")) && i + 1 < nw && w[i + 1].sent == a->sent && onlySpaces(t, a->s + a->len, w[i + 1].s)) {
            DW *b = &w[i + 1]; const wchar_t *x = b->l;
            int allCaps = 1; for (int k = 0; k < b->len; k++) if (iswlower(t[b->s + k])) allCaps = 0;
            if (iswalpha(x[0]) && !(allCaps && b->len > 1) && b->len > 1) {
                int wantAn = -1;
                if (!wcsncmp(x, L"one", 3) || !wcsncmp(x, L"once", 4) || !wcsncmp(x, L"uni", 3) || !wcsncmp(x, L"use", 3) || !wcsncmp(x, L"usu", 3) ||
                    !wcsncmp(x, L"uti", 3) || !wcsncmp(x, L"ure", 3) || !wcsncmp(x, L"eu", 2) || !wcsncmp(x, L"ewe", 3)) wantAn = 0;
                else if (!wcsncmp(x, L"hour", 4) || !wcsncmp(x, L"honest", 6) || !wcsncmp(x, L"honor", 5) || !wcsncmp(x, L"honour", 6) || !wcsncmp(x, L"heir", 4)) wantAn = 1;
                else if (wcschr(L"aeiou", x[0])) wantAn = 1;
                else if (x[0] != L'h') wantAn = 0;
                int isAn = a->len == 2;
                if (wantAn >= 0 && wantAn != isAn)
                    dtAdd(d, a->s, b->s + b->len - a->s, IS_GRAM, L"\"%ls %ls\" should be \"%ls %ls\"", a->l, x, wantAn ? L"an" : L"a", x);
            }
        }
        if (a->len == 1 && src[0] == L'i' && (a->s + 1 >= n || !iswalpha(t[a->s + 1])) && (a->s == 0 || t[a->s - 1] != L'.') &&
            !(i > 0 && (!wcscmp(w[i - 1].l, L"the") || !wcscmp(w[i - 1].l, L"an") || !wcscmp(w[i - 1].l, L"letter"))) && !(a->s > 0 && (t[a->s - 1] == L'"' || t[a->s - 1] == L'(')))
            dtAdd(d, a->s, 1, IS_GRAM, L"Lowercase \"i\" should be \"I\"");
        if (!wcsncmp(src, L"i'", 2) || !wcsncmp(src, L"i\x2019", 2)) dtAdd(d, a->s, a->len, IS_GRAM, L"\"%ls\" should start with a capital I", a->l);
        if (i > 0 && w[i - 1].sent != a->sent && iswlower(src[0])) {
            int hasUpper = 0; for (int k = 1; k < a->len; k++) if (iswupper(src[k])) hasUpper = 1;
            int p = a->s - 1; while (p >= 0 && (iswspace(t[p]) || t[p] == L'"' || t[p] == 0x201C || t[p] == L'(')) p--;
            int quoted = p >= 0 && p + 1 < a->s && (t[p + 1] == L'"' || t[p + 1] == 0x201D || t[p + 1] == L'\'' || t[p + 1] == 0x2019);
            if (!hasUpper && !quoted && p >= 0 && (t[p] == L'.' || t[p] == L'!' || t[p] == L'?') && !(p >= 2 && t[p - 1] == L'.' && t[p - 2] == L'.'))
                dtAdd(d, a->s, a->len, IS_GRAM, L"Sentence should start with a capital letter (\"%lc%ls\")", towupper(src[0]), a->l + 1);
        }
        if (i == 0 && iswlower(src[0]) && a->len > 1) {
            int hasUpper = 0; for (int k = 1; k < a->len; k++) if (iswupper(src[k])) hasUpper = 1;
            if (!hasUpper && nw > 3) dtAdd(d, a->s, a->len, IS_GRAM, L"First word should start with a capital letter");
        }
        for (int m = 0; MISSP[m] || MISSP[m + 1]; m += 2) {
            if (!MISSP[m] || !MISSP[m + 1] || wcscmp(a->l, MISSP[m])) continue;
            if (!wcscmp(a->l, L"im") || !wcscmp(a->l, L"ive")) { if (src[0] != L'i' && src[0] != L'I') continue; }
            if (!wcscmp(a->l, L"cant") || !wcscmp(a->l, L"wont") || !wcscmp(a->l, L"hes") || !wcscmp(a->l, L"shes")) { if (i == 0) continue; }
            dtAdd(d, a->s, a->len, IS_SPELL, L"\"%ls\" → \"%ls\"", a->l, MISSP[m + 1]);
            break;
        }
        if (adjPrev) {
            const wchar_t *p = w[i - 1].l, *x = a->l;
            int s0 = w[i - 1].s, e1 = a->s + a->len;
            if (!wcscmp(x, L"of") && wIn(p, MODALS_OF)) dtAdd(d, s0, e1 - s0, IS_GRAM, L"\"%ls of\" should be \"%ls have\"", p, p);
            else if (!wcscmp(p, L"its") && wIn(x, ITS_NEXT)) dtAdd(d, s0, e1 - s0, IS_GRAM, L"\"its %ls\" should probably be \"it's %ls\"", x, x);
            else if ((!wcscmp(p, L"it's") || !wcscmp(p, L"it’s")) && !wcscmp(x, L"own")) dtAdd(d, s0, e1 - s0, IS_GRAM, L"\"it's own\" should be \"its own\"");
            else if (!wcscmp(p, L"your") && wIn(x, YOUR_NEXT)) dtAdd(d, s0, e1 - s0, IS_GRAM, L"\"your %ls\" should be \"you're %ls\"", x, x);
            else if (!wcscmp(x, L"then") && wIn(p, COMPAR)) dtAdd(d, s0, e1 - s0, IS_GRAM, L"\"%ls then\" should be \"%ls than\"", p, p);
            else if (!wcscmp(x, L"affect") && (!wcscmp(p, L"the") || !wcscmp(p, L"an") || !wcscmp(p, L"positive") || !wcscmp(p, L"negative")))
                dtAdd(d, s0, e1 - s0, IS_GRAM, L"\"%ls affect\" should be \"%ls effect\" (noun)", p, p);
            else if (!wcscmp(x, L"was") && (!wcscmp(p, L"they") || !wcscmp(p, L"we") || !wcscmp(p, L"you")))
                dtAdd(d, s0, e1 - s0, IS_GRAM, L"\"%ls was\" should be \"%ls were\"", p, p);
            else if ((!wcscmp(x, L"don't") || !wcscmp(x, L"dont")) && (!wcscmp(p, L"he") || !wcscmp(p, L"she") || !wcscmp(p, L"it")))
                dtAdd(d, s0, e1 - s0, IS_GRAM, L"\"%ls don't\" should be \"%ls doesn't\"", p, p);
            else if ((!wcscmp(x, L"are") && (!wcscmp(p, L"he") || !wcscmp(p, L"she") || !wcscmp(p, L"it"))) ||
                     (!wcscmp(x, L"is") && (!wcscmp(p, L"they") || !wcscmp(p, L"we") || !wcscmp(p, L"i"))))
                dtAdd(d, s0, e1 - s0, IS_GRAM, L"\"%ls %ls\": the verb doesn't match the subject", p, x);
            else if (!wcscmp(p, L"their") && (!wcscmp(x, L"is") || !wcscmp(x, L"are") || !wcscmp(x, L"was") || !wcscmp(x, L"were")))
                dtAdd(d, s0, e1 - s0, IS_GRAM, L"\"their %ls\" should be \"there %ls\"", x, x);
            else if (!wcscmp(p, L"there") && (!wcscmp(x, L"own") || !wcscmp(x, L"house") || !wcscmp(x, L"car") || !wcscmp(x, L"friends")))
                dtAdd(d, s0, e1 - s0, IS_GRAM, L"\"there %ls\" should be \"their %ls\"", x, x);
            else if (!wcscmp(p, L"could") && !wcscmp(x, L"care") && i + 1 < nw && !wcscmp(w[i + 1].l, L"less"))
                dtAdd(d, s0, w[i + 1].s + w[i + 1].len - s0, IS_GRAM, L"\"could care less\" means the opposite; use \"couldn't care less\"");
        }
        int isCap = iswupper(src[0]);
        if (!isCap && a->len >= 5 && a->len < 30) {
            int alpha = 1; for (int k = 0; k < a->len; k++) if (!iswalpha(src[k]) || src[k] > 127) alpha = 0;
            int listed = 0; for (int m = 0; MISSP[m] || MISSP[m + 1]; m += 2) if (MISSP[m] && !wcscmp(a->l, MISSP[m])) listed = 1;
            wchar_t sug[48];
            if (alpha && !listed && !knownW(a->l) && typoSuggest(a->l, sug) && !(wcslen(sug) == wcslen(a->l) + 1 && !wcsncmp(sug, a->l, wcslen(a->l)))) dtAdd(d, a->s, a->len, IS_SPELL, L"Possible typo: \"%ls\" → \"%ls\"?", a->l, sug);
        }
    }
    /* punctuation scan */
    int par = 0, lastOpen = -1, dq = 0, lastDq = -1, cOpen = 0, cClose = 0, lastC = -1;
    for (int i = 0; i < n; i++) {
        wchar_t c = t[i];
        if (url[i]) continue;
        if (c == L'(') { par++; lastOpen = i; }
        else if (c == L')') {
            if (par > 0) par--;
            else {
                int smile = i > 0 && (t[i - 1] == L':' || t[i - 1] == L';' || t[i - 1] == L'-');
                int marker = i > 0 && iswalnum(t[i - 1]) && (i == 1 || iswspace(t[i - 2]));
                if (!smile && !marker) dtAdd(d, i, 1, IS_PUNCT, L"Closing \")\" without an opening \"(\"");
            }
        }
        else if (c == L'"') { dq++; lastDq = i; }
        else if (c == 0x201C) { cOpen++; lastC = i; } else if (c == 0x201D) cClose++;
        if ((c == L',' || c == L'.' || c == L';' || c == L'!' || c == L'?') && i > 0 && t[i - 1] == L' ' && i > 1 && t[i - 2] != L' ' && t[i - 2] != L'\n') {
            int ell = c == L'.' && i + 1 < n && t[i + 1] == L'.';
            if (!ell && !(c == L'.' && i + 1 < n && iswdigit(t[i + 1]))) dtAdd(d, i - 1, 2, IS_PUNCT, L"Extra space before \"%lc\"", c);
        }
        if ((c == L',' || c == L';') && i > 0 && i + 1 < n && iswalpha(t[i - 1]) && iswalpha(t[i + 1])) dtAdd(d, i, 2, IS_PUNCT, L"Missing space after \"%lc\"", c);
        if ((c == L'.' || c == L'!' || c == L'?') && i > 2 && i + 2 < n && iswlower(t[i - 1]) && iswlower(t[i - 2]) && iswlower(t[i - 3]) && iswupper(t[i + 1]) && iswlower(t[i + 2]))
            dtAdd(d, i, 2, IS_PUNCT, L"Missing space after \"%lc\"", c);
        if (i + 1 < n && ((c == L',' && t[i + 1] == L',') || (c == L';' && t[i + 1] == L';') || (c == L'.' && t[i + 1] == L',') || (c == L',' && t[i + 1] == L'.')))
            dtAdd(d, i, 2, IS_PUNCT, L"Doubled punctuation \"%lc%lc\"", c, t[i + 1]);
        if (c == L'.' && i + 1 < n && t[i + 1] == L'.' && (i + 2 >= n || t[i + 2] != L'.') && (i == 0 || t[i - 1] != L'.'))
            dtAdd(d, i, 2, IS_PUNCT, L"Two periods; use one (or \"...\" for a pause)");
        if (c == L' ' && i + 1 < n && t[i + 1] == L' ' && i > 0 && iswalnum(t[i - 1])) {
            int j = i; while (j < n && t[j] == L' ') j++;
            if (j < n && iswalnum(t[j])) dtAdd(d, i, j - i, IS_PUNCT, L"Double space between words");
            i = j - 1;
        }
    }
    if (par > 0 && lastOpen >= 0) dtAdd(d, lastOpen, 1, IS_PUNCT, L"\"(\" is never closed");
    if (dq % 2 && lastDq >= 0) dtAdd(d, lastDq, 1, IS_PUNCT, L"Unmatched quotation mark");
    if (cOpen != cClose && lastC >= 0) dtAdd(d, lastC, 1, IS_PUNCT, L"Unmatched curly quotes");
    /* sentence-level style */
    for (int s = 0; s < ns; s++) {
        if (sLen[s] > 45) {
            DW *a = &w[sStart[s]], *b = &w[sStart[s] + sLen[s] - 1];
            dtAdd(d, a->s, b->s + b->len - a->s > 0 ? w[sStart[s] + 3].s + w[sStart[s] + 3].len - a->s : a->len, IS_STYLE, L"Very long sentence (%d words); consider splitting it", sLen[s]);
        }
        if (s >= 2 && sLen[s] > 2 && sLen[s - 1] > 2 && sLen[s - 2] > 2) {
            const wchar_t *x = w[sStart[s]].l;
            if (!wcscmp(x, w[sStart[s - 1]].l) && !wcscmp(x, w[sStart[s - 2]].l) && wcscmp(x, L"the"))
                dtAdd(d, w[sStart[s]].s, w[sStart[s]].len, IS_STYLE, L"Three sentences in a row start with \"%ls\"", x);
        }
        int caps = 0, run = 0;
        for (int k = sStart[s]; k < sStart[s] + sLen[s]; k++) {
            DW *a = &w[k]; int up = a->len > 1; for (int q = 0; q < a->len; q++) if (iswlower(t[a->s + q])) up = 0;
            if (up && iswalpha(t[a->s])) { if (++run >= 4 && !caps) { caps = 1; dtAdd(d, w[k - 3].s, a->s + a->len - w[k - 3].s, IS_STYLE, L"Long run of ALL CAPS reads like shouting"); } }
            else run = 0;
        }
    }
    {   /* paragraphs missing an ending */
        int ls = 0;
        for (int i = 0; i <= n; i++) {
            if (i == n || t[i] == L'\n') {
                int e = i - 1; while (e >= ls && iswspace(t[e])) e--;
                int wc = 0; for (int k = 0; k < nw; k++) if (w[k].s >= ls && w[k].s < i) wc++;
                if (e >= ls && wc >= 8 && !wcschr(L".!?:;\"')]\x201D\x2019…*", t[e]) && !url[e] && !iswdigit(t[e]))
                    dtAdd(d, e, 1, IS_PUNCT, L"Paragraph is missing its final punctuation");
                ls = i + 1;
            }
        }
    }

    /* ---- AI signals ---- */
    wchar_t *low = malloc((n + 1) * sizeof(wchar_t));
    for (int i = 0; i < n; i++) low[i] = isApos(t[i]) ? L'\'' : (wchar_t)towlower(t[i]);
    low[n] = 0;
    int aiHits = 0;
    for (int i = 0; i < nw; i++) if (wIn(w[i].l, AIW)) { aiHits++; dtAdd(d, w[i].s, w[i].len, IS_AI, L""); }
    for (int p = 0; AIP[p]; p++) {
        int pl = (int)wcslen(AIP[p]);
        for (wchar_t *q = low; (q = wcsstr(q, AIP[p])); q += pl) {
            int at = (int)(q - low);
            if ((at > 0 && iswalpha(low[at - 1])) || (at + pl < n && iswalpha(low[at + pl]) && iswalpha(AIP[p][pl - 1]))) continue;
            aiHits += 2; dtAdd(d, at, pl, IS_AI, L"");
        }
    }
    d->nAiHits = aiHits;
    int contr = 0, casual = 0, emd = 0, quest = 0, excl = 0, firstP = 0, triads = 0, trans = 0, letters = 0;
    for (int i = 0; i < nw; i++) {
        if (wcschr(w[i].l, L'\'')) contr++;
        if (wIn(w[i].l, CASUAL)) casual++;
        if (!wcscmp(w[i].l, L"i") || !wcscmp(w[i].l, L"me") || !wcscmp(w[i].l, L"my") || !wcscmp(w[i].l, L"i'm")) firstP++;
        letters += w[i].len;
    }
    for (int i = 0; i < n; i++) {
        if (t[i] == 0x2014 || (t[i] == L'-' && i + 1 < n && t[i + 1] == L'-')) emd++;
        if (t[i] == L'?') quest++;
        if (t[i] == L'!') excl++;
    }
    for (int i = 0; i + 3 < nw; i++) {
        /* "X, Y, and Z" */
        if (!wcscmp(w[i + 2].l, L"and") || !wcscmp(w[i + 2].l, L"or")) {
            int c1 = 0; for (int k = w[i].s + w[i].len; k < w[i + 1].s; k++) if (t[k] == L',') c1 = 1;
            int c2 = 0; for (int k = w[i + 1].s + w[i + 1].len; k < w[i + 2].s; k++) if (t[k] == L',') c2 = 1;
            if (c1 && c2 && w[i].sent == w[i + 3].sent) triads++;
        }
    }
    for (int s = 0; s < ns; s++) {
        int a = sStart[s];
        wchar_t two[80]; swprintf(two, 80, L"%ls %ls", w[a].l, sLen[s] > 1 ? w[a + 1].l : L"");
        if (wIn(w[a].l, DTRANS) || wIn(two, DTRANS)) trans++;
    }
    double mean = 0, var = 0;
    int sc = 0;
    for (int s = 0; s < ns; s++) if (sLen[s] >= 2) { mean += sLen[s]; sc++; }
    if (sc) mean /= sc;
    for (int s = 0; s < ns; s++) if (sLen[s] >= 2) var += (sLen[s] - mean) * (sLen[s] - mean);
    if (sc > 1) var /= sc - 1;
    double cv = mean > 0 ? sqrt(var) / mean : 0;
    double per = nw ? 100.0 / nw : 0;
    d->burst = cv; d->aiPer100 = aiHits * per; d->transFrac = ns ? (double)trans / ns : 0; d->contr100 = contr * per;
    d->triad = ns ? (double)triads / ns : 0; d->awl = nw ? (double)letters / nw : 0;
    d->casual100 = (casual + quest * 0.5 + excl * 0.5 + firstP * 0.3) * per;
    d->err100 = d->nGram * per;

    double z = -1.2, cb[8]; memset(cb, 0, sizeof cb);
    if (sc >= 4) { cb[0] = (0.50 - cv) / 0.18; if (cb[0] > 1.6) cb[0] = 1.6; if (cb[0] < -1.6) cb[0] = -1.6; cb[0] *= 1.1; }
    cb[1] = d->aiPer100 / 1.2; if (cb[1] > 3.2) cb[1] = 3.2;
    cb[2] = (d->transFrac - 0.08) * 6; if (cb[2] > 1.6) cb[2] = 1.6; if (cb[2] < -0.4) cb[2] = -0.4;
    cb[3] = d->contr100 < 0.4 ? 0.25 : -fmin(d->contr100 / 2.0, 1.2);
    cb[4] = (d->triad - 0.08) * 5; if (cb[4] > 1.2) cb[4] = 1.2; if (cb[4] < -0.3) cb[4] = -0.3;
    cb[5] = -fmin(d->casual100 / 2.5, 2.0);
    cb[6] = -fmin(d->err100 / 1.5, 2.2);
    cb[7] = (d->awl - 4.7) * 1.1; if (cb[7] > 1.0) cb[7] = 1.0; if (cb[7] < -1.0) cb[7] = -1.0;
    for (int k = 0; k < 8; k++) z += cb[k];
    if (emd && nw && emd * per > 0.4) z += 0.35;
    if (nPara >= 3) { double pm = paraSum / nPara, pv = paraSq / nPara - pm * pm; double pcv = pm > 0 ? sqrt(pv > 0 ? pv : 0) / pm : 1; if (pcv < 0.25) z += 0.4; }
    d->score = (int)(100.0 / (1.0 + exp(-z)) + 0.5);
    if (d->score > 99) d->score = 99; if (d->score < 1) d->score = 1;
    d->ok = nw >= 1;
    wcscpy(d->verdict, d->score < 25 ? L"Likely human" : d->score < 50 ? L"Leans human" : d->score < 75 ? L"Possibly AI" : L"Likely AI");
    wcscpy(d->conf, nw < 60 ? L"low confidence: short text" : nw < 200 ? L"medium confidence" : L"higher confidence");
    /* explain the strongest signals */
    struct { double v; wchar_t s[96]; } sg[10]; int nsg = 0;
    #define SIG(val, ...) do { sg[nsg].v = (val); swprintf(sg[nsg].s, 96, __VA_ARGS__); nsg++; } while (0)
    if (sc >= 4) SIG(cb[0], cb[0] > 0 ? L"▲ Sentences are very even in length" : L"▼ Sentence lengths vary like a person's");
    if (aiHits) SIG(cb[1], L"▲ %d AI-favorite word%ls/phrase%ls (purple)", aiHits, aiHits == 1 ? L"" : L"s", aiHits == 1 ? L"" : L"s");
    else SIG(-0.3, L"▼ No AI-favorite words or phrases");
    if (trans) SIG(cb[2], L"%lc %d sentence%ls open with a transition word", cb[2] > 0 ? 0x25B2 : 0x25BC, trans, trans == 1 ? L"" : L"s");
    SIG(cb[3], cb[3] > 0 ? L"▲ No contractions (stiff, formal tone)" : L"▼ Uses contractions like people do");
    if (triads) SIG(cb[4], L"%lc %d \"X, Y, and Z\" list%ls", cb[4] > 0 ? 0x25B2 : 0x25BC, triads, triads == 1 ? L"" : L"s");
    if (cb[5] < -0.2) SIG(cb[5], L"▼ Casual, personal voice");
    if (cb[6] < -0.2) SIG(cb[6], L"▼ Has mistakes (AI rarely makes them)");
    if (fabs(cb[7]) > 0.3) SIG(cb[7], cb[7] > 0 ? L"▲ Long, fancy words on average" : L"▼ Short, plain words");
    #undef SIG
    for (int a = 0; a < nsg; a++) for (int b = a + 1; b < nsg; b++) if (fabs(sg[b].v) > fabs(sg[a].v)) { sg[9] = sg[a]; sg[a] = sg[b]; sg[b] = sg[9]; }
    d->nSig = nsg < 5 ? nsg : 5;
    for (int k = 0; k < d->nSig; k++) wcscpy(d->sig[k], sg[k].s);
    /* sort issues by position */
    for (int a = 1; a < d->nIss; a++) { Issue k = d->iss[a]; int b = a - 1; while (b >= 0 && d->iss[b].s > k.s) { d->iss[b + 1] = d->iss[b]; b--; } d->iss[b + 1] = k; }
    if (dtF) {
        static const wchar_t *STOP[] = { L"the", L"a", L"an", L"and", L"or", L"but", L"of", L"to", L"in", L"on", L"at", L"for", L"with", L"by", L"from",
            L"is", L"are", L"was", L"were", L"be", L"been", L"it", L"its", L"this", L"that", L"these", L"those", L"as", L"if", L"not", L"no", L"so",
            L"he", L"she", L"they", L"we", L"you", L"i", L"his", L"her", L"their", L"our", L"your", L"my", L"me", L"him", L"them", L"us", L"do",
            L"does", L"did", L"have", L"has", L"had", L"will", L"would", L"can", L"could", L"there", L"which", L"who", L"what", L"when", NULL };
        double *F = dtF; memset(F, 0, DT_NF * sizeof(double));
        double wl = 0, wl2 = 0; int stop = 0, you = 0, digits = 0, midCap = 0;
        for (int i = 0; i < nw; i++) {
            wl += w[i].len; wl2 += (double)w[i].len * w[i].len;
            if (wIn(w[i].l, STOP)) stop++;
            if (!wcscmp(w[i].l, L"you") || !wcscmp(w[i].l, L"your") || !wcscmp(w[i].l, L"yours")) you++;
            if (iswdigit(w[i].l[0])) digits++;
            if (i > 0 && w[i - 1].sent == w[i].sent && iswupper(t[w[i].s]) && wcscmp(w[i].l, L"i") && wcsncmp(w[i].l, L"i'", 2)) midCap++;
        }
        double mattr = 0; int nwin = 0;
        for (int s0 = 0; s0 + 50 <= nw; s0 += 25) {
            int uniq = 0;
            for (int i = s0; i < s0 + 50; i++) { int dup = 0; for (int j = s0; j < i; j++) if (!wcscmp(w[i].l, w[j].l)) { dup = 1; break; } if (!dup) uniq++; }
            mattr += uniq / 50.0; nwin++;
        }
        int comma = 0, semi = 0, colon = 0, dash = 0, paren = 0, quote = 0;
        for (int i = 0; i < n; i++) {
            wchar_t c = t[i];
            if (c == L',') comma++; else if (c == L';') semi++; else if (c == L':') colon++;
            else if (c == 0x2014 || c == 0x2013 || (c == L'-' && i + 1 < n && t[i + 1] == L'-') || (c == L'-' && i > 0 && i + 1 < n && t[i - 1] == L' ' && t[i + 1] == L' ')) dash++;
            else if (c == L'(') paren++; else if (c == L'"' || c == 0x201C || c == 0x201D) quote++;
        }
        int mid = 0, lowStart = 0, andStart = 0, itStart = 0, mx = 0;
        for (int s = 0; s < ns; s++) {
            if (sLen[s] >= 12 && sLen[s] <= 28) mid++;
            if (sLen[s] > mx) mx = sLen[s];
            DW *a = &w[sStart[s]];
            if (iswlower(t[a->s])) lowStart++;
            if (!wcscmp(a->l, L"and") || !wcscmp(a->l, L"but") || !wcscmp(a->l, L"so") || !wcscmp(a->l, L"or")) andStart++;
            if (!wcscmp(a->l, L"this") || !wcscmp(a->l, L"these") || !wcscmp(a->l, L"it") || !wcscmp(a->l, L"they")) itStart++;
        }
        double nsD = ns ? ns : 1, nwD = nw ? nw : 1;
        F[0] = log(nw + 1.0); F[1] = mean; F[2] = sqrt(var); F[3] = cv; F[4] = wl / nwD; F[5] = sqrt(fmax(0, wl2 / nwD - (wl / nwD) * (wl / nwD)));
        F[6] = nwin ? mattr / nwin : 0.7; F[7] = d->contr100; F[8] = comma * per; F[9] = semi * per; F[10] = colon * per; F[11] = dash * per;
        F[12] = paren * per; F[13] = quote * per; F[14] = quest * per; F[15] = excl * per; F[16] = digits * per; F[17] = midCap * per;
        F[18] = firstP * per; F[19] = you * per; F[20] = stop / nwD; F[21] = nPara * per; F[22] = d->aiPer100; F[23] = d->transFrac;
        F[24] = d->triad; F[25] = d->casual100; F[26] = d->err100; F[27] = mid / nsD; F[28] = mean > 0 ? mx / mean : 1; F[29] = itStart / nsD;
        F[30] = lowStart / nsD; F[31] = andStart / nsD;
        int shortS = 0, ana = 0, isStart = 0, someStart = 0, comma3 = 0;
        for (int s = 0; s < ns; s++) {
            if (sLen[s] <= 5) shortS++;
            DW *a = &w[sStart[s]];
            if (s > 0 && sLen[s] > 1 && sLen[s - 1] > 1 && !wcscmp(a->l, w[sStart[s - 1]].l) && !wcscmp(w[sStart[s] + 1].l, w[sStart[s - 1] + 1].l)) ana++;
            if (sLen[s] > 1 && (!wcscmp(a->l, L"it") || !wcscmp(a->l, L"this") || !wcscmp(a->l, L"that") || !wcscmp(a->l, L"there")) &&
                (!wcscmp(w[sStart[s] + 1].l, L"is") || !wcscmp(w[sStart[s] + 1].l, L"was"))) isStart++;
            if (!wcscmp(a->l, L"some") || !wcscmp(a->l, L"others") || !wcscmp(a->l, L"many") || !wcscmp(a->l, L"most")) someStart++;
            int sa = a->s, se = w[sStart[s] + sLen[s] - 1].s, cc = 0;
            for (int k = sa; k < se; k++) if (t[k] == L',') cc++;
            if (cc >= 3) comma3++;
        }
        static const wchar_t *CONTR[] = { L"not just", L"not only", L"not merely", L"isn't just", L"it's not", L"this is not", L"is not about", L"isn't about",
                                          L"rather than", L"less about", L"more about", L"not because", L"but because", L"in other words", L"put another way", NULL };
        int contrast = 0;
        for (int p = 0; CONTR[p]; p++) { int pl = (int)wcslen(CONTR[p]); for (wchar_t *q = low; (q = wcsstr(q, CONTR[p])); q += pl) contrast++; }
        int heads = 0, pfShort = 0, paraN = 0;
        for (int i = 0, ls = 0; i <= n; i++) {
            if (i < n && t[i] != L'\n') continue;
            int e = i - 1; while (e >= ls && iswspace(t[e])) e--;
            int wc = 0, firstW = -1; for (int k = 0; k < nw; k++) if (w[k].s >= ls && w[k].s < i) { if (firstW < 0) firstW = k; wc++; }
            if (wc >= 1 && wc <= 6 && e >= ls && !wcschr(L".!?,;:\"'", t[e]) && iswupper(t[w[firstW].s])) {
                int nx = i + 1; while (nx < n && iswspace(t[nx])) nx++;
                int wc2 = 0; for (int k = 0; k < nw; k++) if (w[k].s >= nx) { int le = nx; while (le < n && t[le] != L'\n') le++; if (w[k].s < le) wc2++; else break; }
                if (wc2 >= 10) heads++;
            }
            if (wc >= 15) { paraN++; for (int s2 = ns - 1; s2 >= 0; s2--) { if (w[sStart[s2]].s < i && w[sStart[s2]].s >= ls) { if (sLen[s2] <= 6) pfShort++; break; } } }
            ls = i + 1;
        }
        F[32] = shortS / nsD; F[33] = ana / nsD; F[34] = contrast * per; F[35] = heads * per; F[36] = paraN ? (double)pfShort / paraN : 0;
        F[37] = comma3 / nsD; F[38] = isStart / nsD; F[39] = someStart / nsD;
    }
    free(low); free(url); free(w); free(sStart); free(sLen);
}

static int dtGramCount(const wchar_t *s) {
    Detect d; dtAnalyze(s, &d);
    int c = 0;
    for (int i = 0; i < d.nIss; i++) if (d.iss[i].kind == IS_GRAM || d.iss[i].kind == IS_PUNCT) c++;
    dtFree(&d);
    return c;
}

/* ---------- human-style slips (optional, after rewriting) ----------
   weights follow what real writers get wrong most (Connors & Lunsford's error counts in student essays:
   missing intro comma, missing compound comma, wrong word, missing possessive apostrophe, comma splice,
   missing series comma) plus keyboard slips (deletion > insertion > adjacent-key substitution > transposition) */
static int wordAt(const wchar_t *t, int n, int s, int *e) { int j = s; while (j < n && (iswalpha(t[j]) || (isApos(t[j]) && j + 1 < n && iswalpha(t[j + 1])))) j++; *e = j; return j > s; }
static const char *KEYROWS[] = { "qwertyuiop", "asdfghjkl", "zxcvbnm" };
static wchar_t adjKey(wchar_t c) {
    char lc = (char)towlower(c);
    for (int r = 0; r < 3; r++) { const char *p = strchr(KEYROWS[r], lc); if (!p) continue;
        int i = (int)(p - KEYROWS[r]), L2 = (int)strlen(KEYROWS[r]);
        int d = nri(2) ? -1 : 1; if (i + d < 0 || i + d >= L2) d = -d;
        return (wchar_t)KEYROWS[r][i + d]; }
    return c;
}
enum { SL_INTRO, SL_COMPOUND, SL_WORD, SL_POSS, SL_SPLICE, SL_SERIES, SL_SPELL, SL_KEY, SL_N };
static const int SLW[SL_N] = { 20, 15, 15, 8, 8, 6, 12, 16 };
static wchar_t *addTypos(const wchar_t *t, int perWords, unsigned long long seed) {
    int n = (int)wcslen(t);
    if (perWords <= 0 || !n) return wdup(t);
    nanoInit();
    nrs = seed ? seed : 0x5DEECE66DULL; for (int i = 0; i < 4; i++) nrnd();
    unsigned char *L = computeLocks(t, n, 0);
    typedef struct { int s, e, sent, cat; wchar_t rep[48]; } Ed;
    int cap = 256, nc = 0, words = 0, sent = 0, sentStart = 1; Ed *c = malloc(cap * sizeof(Ed));
    #define ADD(S, E, CAT, REP) do { if (nc == cap) { cap *= 2; c = realloc(c, cap * sizeof(Ed)); } c[nc].s = (S); c[nc].e = (E); c[nc].sent = sent; c[nc].cat = (CAT); wcsncpy(c[nc].rep, (REP), 47); c[nc].rep[47] = 0; nc++; } while (0)
    static const wchar_t *SWAP[] = { L"it's", L"its", L"their", L"there", L"than", L"then", L"you're", L"your", L"they're", L"their",
        L"don't", L"dont", L"doesn't", L"doesnt", L"didn't", L"didnt", L"can't", L"cant", L"isn't", L"isnt", L"wasn't", L"wasnt",
        L"couldn't", L"couldnt", L"wouldn't", L"wouldnt", L"that's", L"thats", L"who's", L"whos",
        L"a lot", L"alot", L"could have", L"could of", L"should have", L"should of", L"would have", L"would of", L"affect", L"effect",
        L"lose", L"loose", L"too", L"to", L"whose", L"who's", NULL, NULL };
    static const wchar_t *INTRO[] = { L"in", L"on", L"at", L"after", L"before", L"during", L"however", L"later", L"then", L"eventually", L"finally",
        L"instead", L"still", L"also", L"today", L"by", L"for", L"when", L"as", L"since", L"once", L"while", L"although", L"because", L"if", NULL };
    for (int i = 0; i < n;) {
        int e;
        wchar_t ch = t[i];
        if (ch == L'.' || ch == L'!' || ch == L'?' || ch == L'\n') {
            /* comma splice: "short sentence. It ..." -> "short sentence, it ..." */
            if (ch == L'.' && !L[i] && i + 2 < n && t[i + 1] == L' ' && wordAt(t, n, i + 2, &e)) {
                wchar_t w[12]; int L2 = e - i - 2 < 11 ? e - i - 2 : 11; lowerInto(t + i + 2, L2, w);
                static const wchar_t *SP[] = { L"it", L"this", L"he", L"she", L"they", L"that", L"there", NULL };
                if (inList(SP, w) && !L[i + 2]) { wchar_t r[16]; swprintf(r, 16, L", %lc", towlower(t[i + 2])); ADD(i, i + 3, SL_SPLICE, r); }
            }
            sent++; sentStart = 1; i++; continue;
        }
        if (!iswalpha(ch) || (i > 0 && iswalpha(t[i - 1])) || !wordAt(t, n, i, &e)) {
            if (ch == L',' && !L[i]) {
                int k = i + 1; while (k < n && t[k] == L' ') k++;
                int ke; wordAt(t, n, k, &ke);
                wchar_t nw[12] = L""; if (ke > k && ke - k < 11) lowerInto(t + k, ke - k, nw);
                if (!wcscmp(nw, L"and") || !wcscmp(nw, L"but") || !wcscmp(nw, L"so")) {
                    /* series comma vs compound comma: look back for another comma in this sentence */
                    int back = i - 1, series = 0; while (back >= 0 && !wcschr(L".!?\n", t[back]) && i - back < 80) { if (t[back] == L',') { series = 1; break; } back--; }
                    ADD(i, i + 1, series && !wcscmp(nw, L"and") ? SL_SERIES : SL_COMPOUND, L"");
                }
            }
            i++; continue;
        }
        words++;
        int locked = 0; for (int q = i; q < e; q++) if (L[q]) locked = 1;
        wchar_t w[48]; int L2 = e - i < 47 ? e - i : 47; lowerInto(t + i, L2, w);
        if (sentStart) {
            sentStart = 0;
            /* missing comma after a short introductory element */
            if (inList(INTRO, w)) {
                int k = e, wc = 1; while (k < n && t[k] != L',' && !wcschr(L".!?\n", t[k]) && wc <= 5) { if (t[k] == L' ') wc++; k++; }
                if (k < n && t[k] == L',' && wc <= 5 && !L[k]) ADD(k, k + 1, SL_INTRO, L"");
            }
        }
        if (locked) { i = e; continue; }
        int ee = e; const wchar_t *rep = NULL;
        for (int m = 0; SWAP[m]; m += 2) {
            const wchar_t *k = SWAP[m]; int kl = (int)wcslen(k);
            if (wcschr(k, L' ')) { if (i + kl <= n && !wcsncmp(t + i, k, kl) && (i + kl >= n || !iswalpha(t[i + kl]))) { rep = SWAP[m + 1]; ee = i + kl; break; } continue; }
            if (!wcscmp(w, k)) { rep = SWAP[m + 1]; break; }
        }
        if (rep) { wchar_t r[48]; wcsncpy(r, rep, 47); r[47] = 0; if (iswupper(t[i])) r[0] = towupper(r[0]); ADD(i, ee, SL_WORD, r); i = ee; continue; }
        /* possessive apostrophe left out: "the story's end" -> "the storys end" */
        if (L2 > 3 && isApos(t[e - 2]) && towlower(t[e - 1]) == L's' && iswlower(t[i])) { wchar_t r[48]; wcsncpy(r, t + i, L2 - 2); r[L2 - 2] = L's'; r[L2 - 1] = 0; ADD(i, e, SL_POSS, r); }
        for (int m = 0; MISSP[m] || MISSP[m + 1]; m += 2)
            if (MISSP[m] && MISSP[m + 1] && !wcscmp(w, MISSP[m + 1]) && !wcschr(MISSP[m], L' ') && wcscmp(MISSP[m], L"lets") && wcscmp(MISSP[m], L"teh") && wcscmp(MISSP[m], L"adn")) {
                wchar_t r[48]; wcsncpy(r, MISSP[m], 47); r[47] = 0; if (iswupper(t[i])) r[0] = towupper(r[0]); ADD(i, e, SL_SPELL, r); break; }
        if (L2 >= 5) {
            /* keyboard slip, by real frequency: deletion 34, insertion 27, adjacent-key 19, transposition 12 */
            wchar_t r[48]; wcsncpy(r, t + i, L2); r[L2] = 0;
            int p = 1 + nri(L2 - 2), roll = nri(92), ok = 1;
            if (!iswlower(r[p])) ok = 0;
            else if (roll < 34) { if (r[p] == r[p - 1] || r[p] == r[p + 1]) wmemmove(r + p, r + p + 1, L2 - p); else wmemmove(r + p, r + p + 1, L2 - p); }
            else if (roll < 61) { wmemmove(r + p + 1, r + p, L2 - p + 1); r[p] = nri(2) ? r[p + 1] : adjKey(r[p + 1]); }
            else if (roll < 80) { r[p] = adjKey(r[p]); }
            else { if (iswlower(r[p + 1]) && r[p] != r[p + 1]) { wchar_t x = r[p]; r[p] = r[p + 1]; r[p + 1] = x; } else ok = 0; }
            if (ok && wcscmp(r, t + i) && wcsncmp(r, t + i, L2)) ADD(i, e, SL_KEY, r);
            else if (ok && (int)wcslen(r) != L2) ADD(i, e, SL_KEY, r);
        }
        i = e;
    }
    #undef ADD
    int want = words / perWords; if (!want && words >= perWords / 2) want = 1;
    /* weighted pick by category, one per sentence, no repeats */
    char *usedSent = calloc(sent + 2, 1), *taken = calloc(nc + 1, 1);
    Ed *pick = malloc((nc + 1) * sizeof(Ed)); int np = 0;
    for (int tries = 0; np < want && tries < want * 40 + 40; tries++) {
        int tot = 0, have[SL_N] = {0};
        for (int k = 0; k < nc; k++) if (!taken[k] && !usedSent[c[k].sent]) have[c[k].cat] = 1;
        for (int q = 0; q < SL_N; q++) if (have[q]) tot += SLW[q];
        if (!tot) break;
        int r = nri(tot), cat = 0; for (; cat < SL_N; cat++) { if (!have[cat]) continue; if (r < SLW[cat]) break; r -= SLW[cat]; }
        int cand[512], ncand = 0;
        for (int k = 0; k < nc && ncand < 512; k++) if (!taken[k] && !usedSent[c[k].sent] && c[k].cat == cat) cand[ncand++] = k;
        if (!ncand) continue;
        int k = cand[nri(ncand)];
        int clash = 0; for (int q = 0; q < np; q++) if ((c[k].s < pick[q].e && pick[q].s < c[k].e) || (c[k].rep[0] && !wcscmp(pick[q].rep, c[k].rep))) clash = 1;
        taken[k] = 1;
        if (clash) continue;
        usedSent[c[k].sent] = 1; pick[np++] = c[k];
    }
    for (int a = 1; a < np; a++) { Ed x = pick[a]; int b = a - 1; while (b >= 0 && pick[b].s > x.s) { pick[b + 1] = pick[b]; b--; } pick[b + 1] = x; }
    Str o = {0}; sAdd(&o, L"");
    int pos = 0;
    for (int k = 0; k < np; k++) { if (pick[k].s < pos) continue; sAddN(&o, t + pos, pick[k].s - pos); sAdd(&o, pick[k].rep); pos = pick[k].e; }
    sAdd(&o, t + pos);
    free(c); free(pick); free(usedSent); free(taken); free(L);
    return o.s;
}

/* ---------- de-AI pass for MAX: strip the patterns detectors and readers flag ----------
   em dashes (~10x more common in AI text), "not only ... but also", tacked-on ", highlighting/showing ..." analysis,
   stock AI phrases, and some Oxford commas (series commas are dropped often by people) */
static wchar_t *humanizePass2(const wchar_t *t, unsigned long long seed, int contract);
static wchar_t *humanizePass1(const wchar_t *t, int level, unsigned long long seed);
static wchar_t *humanizePass(const wchar_t *t, int level, unsigned long long seed, int contract) {
    wchar_t *a = humanizePass2(t, seed, contract && level >= 2);
    wchar_t *b = humanizePass1(a, level, seed);
    free(a); return b;
}
static wchar_t *humanizePass1(const wchar_t *t, int level, unsigned long long seed) {
    nanoInit();
    nrs = seed ? seed : 0x1234567ULL; for (int i = 0; i < 4; i++) nrnd();
    int n = (int)wcslen(t);
    unsigned char *L = computeLocks(t, n, 0);
    Str o = {0}; sAdd(&o, L"");
    static const wchar_t *PHR[] = { L"stands as a testament to", L"shows", L"is a testament to", L"shows", L"plays a crucial role in", L"matters a lot in",
        L"plays a vital role in", L"matters a lot in", L"plays a pivotal role in", L"matters a lot in", L"plays a significant role in", L"matters in",
        L"plays a key role in", L"matters in", L"it is important to note that ", L"", L"it's important to note that ", L"", L"it is worth noting that ", L"",
        L"in today's fast-paced world", L"today", L"in today's world", L"today", L"delve into", L"look at", L"delves into", L"looks at",
        L"a rich tapestry of", L"a mix of", L"serves as a reminder", L"is a reminder", L"serves as a", L"is a", L"in conclusion,", L"in the end,",
        L"in summary,", L"overall,", L"navigate the complexities of", L"deal with", L"a wide range of", L"lots of", L"a myriad of", L"lots of",
        L"embark on", L"start", L"foster", L"build", L"underscores", L"shows", L"underscore", L"show", L"highlights the importance of", L"shows why",
        L"utilize", L"use", L"utilizes", L"uses", NULL, NULL };
    static const wchar_t *ING[] = { L"showing", L"shows", L"highlighting", L"highlights", L"underscoring", L"shows", L"emphasizing", L"shows",
        L"reflecting", L"reflects", L"demonstrating", L"shows", L"revealing", L"reveals", L"illustrating", L"shows", L"suggesting", L"suggests", NULL, NULL };
    int dashOpen = 0;
    for (int i = 0; i < n;) {
        if (L[i]) { sAddC(&o, t[i]); i++; continue; }
        int boundary = i == 0 || !iswalpha(t[i - 1]);
        /* stock AI phrases */
        if (boundary && iswalpha(t[i])) {
            int hit = 0;
            for (int m = 0; PHR[m]; m += 2) {
                int pl = (int)wcslen(PHR[m]), ok = i + pl <= n;
                for (int q = 0; ok && q < pl; q++) if ((wchar_t)towlower(t[i + q]) != PHR[m][q] && !(isApos(t[i + q]) && PHR[m][q] == L'\'')) ok = 0;
                if (!ok || (i + pl < n && iswalpha(t[i + pl]) && iswalpha(PHR[m][pl - 1]))) continue;
                const wchar_t *r = PHR[m + 1];
                if (r[0]) { wchar_t b[64]; wcscpy(b, r); if (iswupper(t[i])) b[0] = towupper(b[0]); sAdd(&o, b); }
                else if (i + pl < n && iswlower(t[i + pl]) && (o.n == 0 || o.s[o.n - 1] == L' ' || o.s[o.n - 1] == L'\n')) { /* capitalize what follows if we removed a sentence opener */
                    int sStart = o.n == 0; for (int q = o.n - 1; q >= 0 && !sStart; q--) { if (o.s[q] == L' ') continue; sStart = wcschr(L".!?\n", o.s[q]) != NULL; break; }
                    if (sStart) { sAddC(&o, towupper(t[i + pl])); i += pl + 1; hit = 1; break; } }
                i += pl; hit = 1; break;
            }
            if (hit) continue;
            /* "not only X but also Y" -> "X and also Y" */
            if (!wcsncmp(t + i, L"not only ", 9) && level >= 1) {
                const wchar_t *b = wcsstr(t + i + 9, L" but also ");
                const wchar_t *stop = t + i; while (*stop && !wcschr(L".!?\n", *stop)) stop++;
                if (b && b < stop && b - (t + i) < 90) {
                    int xs = i + 9, xe = (int)(b - t);
                    int lk = 0; for (int q = xs; q < xe; q++) if (L[q]) lk = 1;
                    if (!lk) { sAddN(&o, t + xs, xe - xs); sAdd(&o, L" and also "); i = xe + 10; continue; }
                }
            }
        }
        /* ", highlighting ..." tacked-on analysis -> ". This highlights ..." */
        if (t[i] == L',' && i + 2 < n && t[i + 1] == L' ' && nrnd() < 0.6 * level) {
            int hit = 0;
            for (int m = 0; ING[m]; m += 2) {
                int pl = (int)wcslen(ING[m]);
                if (wcsncmp(t + i + 2, ING[m], pl) || iswalpha(t[i + 2 + pl])) continue;
                int k = i + 2 + pl; while (k < n && t[k] == L' ') k++;
                int ke = k; while (ke < n && iswalpha(t[ke])) ke++;
                wchar_t nx[16] = L""; if (ke - k < 15) lowerInto(t + k, ke - k, nx);
                if (!(!wcscmp(nx, L"that") || !wcscmp(nx, L"how") || !wcscmp(nx, L"the") || !wcscmp(nx, L"his") || !wcscmp(nx, L"her") || !wcscmp(nx, L"their") || !wcscmp(nx, L"its") || !wcscmp(nx, L"a") || !wcscmp(nx, L"an"))) break;
                sAdd(&o, L". This "); sAdd(&o, ING[m + 1]); i += 2 + pl; hit = 1; break;
            }
            if (hit) continue;
        }
        /* em dashes -> commas / parentheses */
        if (t[i] == 0x2014 || (t[i] == L'-' && i + 1 < n && t[i + 1] == L'-')) {
            int w = t[i] == 0x2014 ? 1 : 2;
            while (o.n && o.s[o.n - 1] == L' ') o.s[--o.n] = 0;
            int j = i + w; while (j < n && t[j] == L' ') j++;
            /* a second dash later in the sentence makes it a pair */
            int pair = 0; for (int q = j; q < n && !wcschr(L".!?\n", t[q]); q++) if (t[q] == 0x2014 || (t[q] == L'-' && q + 1 < n && t[q + 1] == L'-')) { pair = 1; break; }
            if (dashOpen) { sAdd(&o, nri(2) ? L")" : L","); sAddC(&o, L' '); dashOpen = 0; }
            else if (pair) { if (nri(2)) { sAdd(&o, L" ("); dashOpen = 1; } else { sAdd(&o, L", "); dashOpen = 0; } }
            else sAdd(&o, L", ");
            i = j; continue;
        }
        if (wcschr(L".!?\n", t[i])) dashOpen = 0;
        /* drop some series commas: "X, Y, and Z" -> "X, Y and Z" */
        if (t[i] == L',' && level >= 2 && i + 5 < n && !wcsncmp(t + i, L", and ", 6)) {
            int back = i - 1, series = 0; while (back >= 0 && !wcschr(L".!?\n", t[back]) && i - back < 60) { if (t[back] == L',') { series = 1; break; } back--; }
            if (series && nri(2)) { i++; continue; }
        }
        sAddC(&o, t[i]); i++;
    }
    free(L);
    return o.s;
}

/* plain openers and everyday contractions, the way people actually write */
static wchar_t *humanizePass2(const wchar_t *t, unsigned long long seed, int contract) {
    nanoInit();
    nrs = seed ^ 0x9E3779B97F4A7C15ULL; for (int i = 0; i < 4; i++) nrnd();
    int n = (int)wcslen(t);
    unsigned char *L = computeLocks(t, n, 0);
    static const wchar_t *OPEN[] = { L"However, ", L"But ", L"Therefore, ", L"So ", L"Thus, ", L"So ", L"Additionally, ", L"Also, ", L"Moreover, ", L"Also, ",
        L"Furthermore, ", L"Also, ", L"Ultimately, ", L"In the end, ", L"Overall, ", L"In the end, ", L"Consequently, ", L"So ", L"Nevertheless, ", L"Still, ",
        L"Nonetheless, ", L"Still, ", L"In addition, ", L"Also, ", L"Subsequently, ", L"Later, ", NULL, NULL };
    static const wchar_t *CON[] = { L"do not", L"don't", L"does not", L"doesn't", L"did not", L"didn't", L"is not", L"isn't", L"are not", L"aren't",
        L"was not", L"wasn't", L"were not", L"weren't", L"cannot", L"can't", L"could not", L"couldn't", L"would not", L"wouldn't", L"will not", L"won't",
        L"should not", L"shouldn't", L"has not", L"hasn't", L"have not", L"haven't", L"it is", L"it's", L"that is", L"that's", L"there is", L"there's",
        L"they are", L"they're", L"we are", L"we're", L"you are", L"you're", L"I am", L"I'm", L"I have", L"I've", L"let us", L"let's", NULL, NULL };
    Str o = {0}; sAdd(&o, L"");
    int sentStart = 1;
    for (int i = 0; i < n;) {
        if (L[i]) { sAddC(&o, t[i]); sentStart = 0; i++; continue; }
        if (sentStart && iswalpha(t[i])) {
            sentStart = 0;
            int hit = 0;
            for (int m = 0; OPEN[m]; m += 2) {
                int pl = (int)wcslen(OPEN[m]);
                if (wcsncmp(t + i, OPEN[m], pl) || nrnd() > 0.75) continue;
                sAdd(&o, OPEN[m + 1]);
                i += pl;
                if (i < n && iswupper(t[i]) && !L[i] && !(t[i] == L'I' && (i + 1 >= n || !iswalpha(t[i + 1])))) {
                    int e = i; while (e < n && iswalpha(t[e])) e++;
                    wchar_t w[40]; int L2 = e - i < 39 ? e - i : 39; lowerInto(t + i, L2, w);
                    if (commonLowerWord(w) || isKnownWord(w)) { sAddC(&o, towlower(t[i])); i++; }
                }
                hit = 1; break;
            }
            if (hit) continue;
        }
        if (contract && iswalpha(t[i]) && (i == 0 || !iswalpha(t[i - 1]))) {
            int hit = 0;
            for (int m = 0; CON[m]; m += 2) {
                int pl = (int)wcslen(CON[m]), ok = i + pl <= n;
                for (int q = 0; ok && q < pl; q++) if (towlower(t[i + q]) != towlower(CON[m][q])) ok = 0;
                if (!ok || (i + pl < n && iswalpha(t[i + pl]))) continue;
                int lk = 0; for (int q = i; q < i + pl; q++) if (L[q]) lk = 1;
                if (lk) break;
                /* "it is." / "that is," at a clause end can't contract */
                int k = i + pl; while (k < n && t[k] == L' ') k++;
                if (k >= n || wcschr(L".,;:!?\n", t[k])) break;
                if (nrnd() > 0.6) break;
                wchar_t b[16]; wcscpy(b, CON[m + 1]); if (iswupper(t[i])) b[0] = towupper(b[0]);
                sAdd(&o, b); i += pl; hit = 1; break;
            }
            if (hit) continue;
        }
        if (wcschr(L".!?\n", t[i])) sentStart = 1;
        else if (!iswspace(t[i]) && t[i] != L'"' && t[i] != 0x201C && t[i] != L'(') sentStart = 0;
        sAddC(&o, t[i]); i++;
    }
    free(L);
    return o.s;
}
