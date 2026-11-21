
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
            if (pct != lastPct) { lastPct = pct; status(L"Downloading update… %d%%", pct); updPct = pct; InvalidateRect(hMain, NULL, FALSE); }
        }
        if (len > (64u << 20)) { err = -2; break; }
    }
    InternetCloseHandle(h); InternetCloseHandle(in);
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

static void u8w(const char *s, wchar_t *out, int n) { MultiByteToWideChar(CP_UTF8, 0, s ? s : "", -1, out, n); out[n - 1] = 0; }

/* ---------- updater ---------- */

enum { CONN_UNKNOWN, CONN_CHECKING, CONN_ONLINE, CONN_OFFLINE, CONN_ERROR };
static int updConn = CONN_UNKNOWN, updAvail, justUpdated;
static volatile LONG updBusy;
static wchar_t updRepo[128] = L"binjin-maker/FrutigerNano";
static wchar_t updLatest[32], updUrl[1024], updPage[512], updDigest[80], updConnMsg[160];
static wchar_t *updNotes, *updRevoked;

static int updForce;
static wchar_t updMinRun[32];
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
        char *hist = jsonStr(b, "history", NULL, NULL), *mr = jsonStr(b, "minRun", NULL, NULL);
        if (mr) { u8w(mr, updMinRun, 32); updForce = verNewer(updMinRun, APP_VERSION_W); free(mr); }
        if (hist && hist[0]) {
            size_t a = strlen(nt ? nt : ""), h = strlen(hist);
            char *both = malloc(a + h + 40);
            sprintf(both, "%s\n\nEarlier versions\n%s", nt ? nt : "", hist);
            free(nt); nt = both;
        }
        free(hist);
        if (ver) { latest[0] = L'v'; u8w(ver, latest + 1, 31); }
        u8w(u8, dl, 1024); u8w(sha, dig, 80);
        notes = cleanNotes(nt ? nt : "");
        if (latest[0] && verNewer(latest, APP_VERSION_W)) avail = dl[0] ? 1 : 2;
        free(ver); free(u8); free(sha); free(nt);
    }
    free(b);
    if (conn == CONN_ONLINE) {
        swprintf(url, 300, L"https://raw.githubusercontent.com/%ls/main/update/revoked.txt?t=%llu", updRepo, (unsigned long long)GetTickCount64());
        char *rv; size_t rl; DWORD rs;
        if (!httpGet(url, L"Cache-Control: no-cache\r\n", &rv, &rl, &rs, 0) && rs == 200) {
            int n = MultiByteToWideChar(CP_UTF8, 0, rv, -1, NULL, 0);
            wchar_t *w = malloc(n * sizeof(wchar_t)); MultiByteToWideChar(CP_UTF8, 0, rv, -1, w, n);
            wchar_t *old = updRevoked; updRevoked = w; free(old);
        }
        free(rv);
    }
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
    updPct = -1;
    wchar_t *c = wdup(m);
    InterlockedExchange(&updBusy, 0);
    PostMessageW(hMain, WM_UPDFAIL, 0, (LPARAM)c);
}

static DWORD WINAPI InstallThread(LPVOID unused) {
    (void)unused;
    status(L"Downloading update %ls…", updLatest);
    updPct = 0; InvalidateRect(hMain, NULL, FALSE);
    char *b; size_t len; DWORD st;
    wchar_t fresh[1100];
    swprintf(fresh, 1100, L"%ls%lsv=%ls&t=%llu", updUrl, wcschr(updUrl, L'?') ? L"&" : L"?", updLatest, (unsigned long long)GetTickCount64());
    int e = httpGet(fresh, L"Cache-Control: no-cache\r\n", &b, &len, &st, 1);
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

