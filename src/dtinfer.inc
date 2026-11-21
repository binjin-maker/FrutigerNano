
typedef struct { wchar_t *k; float w; } NgW;
static NgW *dtNg; static int dtNgN;
static int ngCmp(const void *a, const void *b) { return wcscmp(((const NgW *)a)->k, ((const NgW *)b)->k); }
static void dtModelInit(void) {
    if (dtNg) return;
    int cap = 4096; dtNg = malloc(cap * sizeof(NgW));
    for (int b = 0; DT_NG[b]; b++)
        for (const wchar_t *p = DT_NG[b]; *p;) {
            const wchar_t *tab = wcschr(p, L'\t'), *nl = wcschr(p, L'\n');
            if (!tab || !nl) break;
            if (dtNgN == cap) { cap *= 2; dtNg = realloc(dtNg, cap * sizeof(NgW)); }
            NgW e; e.k = malloc((tab - p + 1) * sizeof(wchar_t)); wmemcpy(e.k, p, tab - p); e.k[tab - p] = 0;
            e.w = (float)wcstod(tab + 1, NULL);
            dtNg[dtNgN++] = e;
            p = nl + 1;
        }
    qsort(dtNg, dtNgN, sizeof(NgW), ngCmp);
}
static float ngLook(const wchar_t *k, int *found) {
    NgW key = { (wchar_t *)k, 0 };
    NgW *r = bsearch(&key, dtNg, dtNgN, sizeof(NgW), ngCmp);
    *found = r != NULL; return r ? r->w : 0;
}

static void dtScore(const wchar_t *t, Detect *d) {
    dtModelInit();
    double F[DT_NF];
    dtF = F; dtAnalyze(t, d); dtF = NULL;
    if (!d->words) return;
    double za = DT_AB, zm = DT_B;
    for (int k = 0; k < DT_NF; k++) {
        double z = (F[k] - DT_MU[k]) / DT_SD[k]; if (z > 5) z = 5; if (z < -5) z = -5;
        za += z * DT_AW[k]; zm += z * DT_DW[k];
    }
    wchar_t **v; int n = dtTokens(t, &v);
    /* count unigrams + bigrams (with a start marker) that the model knows */
    int cap = 256, nk = 0; struct { int idx, c; } *hit = malloc(cap * sizeof *hit);
    wchar_t buf[100];
    for (int i = -1; i < n; i++)
        for (int kind = 0; kind < 2; kind++) {
            if (kind == 0) { if (i < 0) continue; wcsncpy(buf, v[i], 99); buf[99] = 0; }
            else { if (i + 1 >= n) continue; swprintf(buf, 100, L"%ls %ls", i < 0 ? L"<s>" : v[i], v[i + 1]); }
            int f; ngLook(buf, &f); if (!f) continue;
            NgW key = { buf, 0 }; NgW *r = bsearch(&key, dtNg, dtNgN, sizeof(NgW), ngCmp);
            int idx = (int)(r - dtNg), j;
            for (j = 0; j < nk; j++) if (hit[j].idx == idx) { hit[j].c++; break; }
            if (j == nk) { if (nk == cap) { cap *= 2; hit = realloc(hit, cap * sizeof *hit); } hit[nk].idx = idx; hit[nk].c = 1; nk++; }
        }
    double norm = 0;
    for (int j = 0; j < nk; j++) { double x = 1 + log((double)hit[j].c); norm += x * x; }
    norm = sqrt(norm);
    if (norm > 0) for (int j = 0; j < nk; j++) zm += 3.0 * (1 + log((double)hit[j].c)) / norm * dtNg[hit[j].idx].w;
    free(hit);
    for (int i = 0; i < n; i++) free(v[i]);
    free(v);
    double pm = 1 / (1 + exp(-zm)), pa = 1 / (1 + exp(-za)), p = 0.5 * pm + 0.5 * pa;
    d->pModel = (int)(pm * 100 + 0.5); d->pAlgo = (int)(pa * 100 + 0.5);
    d->score = (int)(p * 100 + 0.5); if (d->score > 99) d->score = 99; if (d->score < 1) d->score = 1;
    wcscpy(d->verdict, d->score < 25 ? L"Likely human" : d->score < 50 ? L"Leans human" : d->score < 75 ? L"Possibly AI" : L"Likely AI");
    wcscpy(d->conf, d->words < 60 ? L"low confidence: short text" : d->words < 200 ? L"medium confidence" : L"higher confidence");
}
