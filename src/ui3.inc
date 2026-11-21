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

enum { PG_REWRITE, PG_DETECT, PG_UPDATE, PG_LOCK, PG_N };
#define NTABS 3
static const wchar_t *TABNAME[NTABS] = { L"Rewrite", L"AI detector", L"Update" };
static const int TABICON[NTABS] = { 0, 5, 2 };
static Detect det; static int detHave;
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
    DrawTextW(dc, t, -1, &r, fmt | DT_NOPREFIX);
}

enum { TG_LIGHT, TG_MED, TG_HEAVY, TG_MAX, TG_STRICT, TG_HILITE, TG_UCHECK, TG_UAUTO, TG_N };
typedef struct { const wchar_t *t; int page, x, y, w, radio, on; } Tog;
static Tog tog[TG_N] = {
    { L"Light",                  PG_REWRITE, 296,  12,  58, 2, 0 },
    { L"Medium",                 PG_REWRITE, 356,  12,  76, 2, 0 },
    { L"Heavy",                  PG_REWRITE, 434,  12,  66, 2, 1 },
    { L"MAX",                    PG_REWRITE, 502,  12,  56, 2, 0 },
    { L"Lock facts",             PG_REWRITE, 106, 397, 100, 0, 0 },
    { L"Highlight locks",        PG_REWRITE, 210, 397, 132, 0, 1 },
    { L"Check on launch",        PG_UPDATE,  108, 358, 180, 0, 1 },
    { L"Auto-install updates",   PG_UPDATE,  300, 358, 200, 0, 0 },
};

static int strength(void) { return tog[TG_LIGHT].on ? 0 : tog[TG_HEAVY].on ? 2 : tog[TG_MAX].on ? 3 : 1; }

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

static void drawAppIcon(HDC dc, int cx, int cy, int sz) {
    HICON ic = (HICON)LoadImageW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(1), IMAGE_ICON, sz, sz, 0);
    if (!ic) return;
    DrawIconEx(dc, cx - sz / 2, cy - sz / 2, ic, sz, sz, 0, NULL, DI_NORMAL);
    DestroyIcon(ic);
}

static void renderPages(void) {
    renderOcean(&pageC[0]);
    for (int p = 1; p < PG_N; p++) memcpy(pageC[p].px, pageC[0].px, (size_t)pageC[0].w * pageC[0].h * 4);
    for (int p = 0; p < PG_N; p++) {
        Canvas *c = &pageC[p];
        HDC dc = c->dc;
        SetBkMode(dc, TRANSPARENT);
        darkGlass(c, 10, 10, 76, 452);
        drawAppIcon(dc, S(48), S(40), S(48));
        textAt(dc, hTiny, L"NANO", 10, 64, 76, 14, RGB(200, 250, 255), DT_CENTER | DT_SINGLELINE);
        textAt(dc, hTiny, L"v" APP_VERSION_W, 10, 440, 76, 14, RGB(150, 220, 235), DT_CENTER | DT_SINGLELINE);
        glassPanel(c, 96, 432, 654, 30);
        if (p == PG_REWRITE) {
            glassPanel(c, 96, 10, 654, 40);
            textAt(dc, hTitle, L"Rewrite", 108, 19, 110, 22, NAVY, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
            textAt(dc, hFont, L"Strength:", 228, 15, 66, 18, HINT, DT_LEFT | DT_SINGLELINE);
            textAt(dc, hTiny, L"success rate", 228, 33, 66, 12, HINT, DT_LEFT | DT_SINGLELINE);
            static const wchar_t *RATE[4] = { L"20%", L"45%", L"50–75%", L"85–100%" };
            for (int r = 0; r < 4; r++) textAt(dc, hTiny, RATE[r], tog[TG_LIGHT + r].x + 2, 33, tog[TG_LIGHT + r].w, 12, RGB(30, 130, 90), DT_LEFT | DT_SINGLELINE);
            rrFill(c, S(564), S(20), 1.2, S(20), 0, 0x1a8fa3, 0x1a8fa3, 0.5, 0.5);
            textAt(dc, hFont, L"Passes:", 574, 21, 50, 18, HINT, DT_LEFT | DT_SINGLELINE);
            textAt(dc, hTiny, L"1 – 1000", 678, 23, 64, 14, HINT, DT_LEFT | DT_SINGLELINE);
            glassPanel(c, 96, 58, 300, 324);
            glassPanel(c, 450, 58, 300, 324);
            textAt(dc, hTitle, L"Original", 108, 62, 200, 20, NAVY, DT_LEFT | DT_SINGLELINE);
            textAt(dc, hSmall, L"paste here", 250, 64, 136, 16, HINT, DT_RIGHT | DT_SINGLELINE);
            textAt(dc, hTitle, L"Rewritten", 462, 62, 200, 20, NAVY, DT_LEFT | DT_SINGLELINE);

            textAt(dc, hSmall, L"Ctrl+Enter", 392, 256, 62, 14, RGB(220, 250, 255), DT_CENTER | DT_SINGLELINE);
            textAt(dc, hTiny, L"Quotes, (parens), names and numbers stay locked.", 108, 354, 278, 24, HINT, DT_LEFT | DT_WORDBREAK);
            textAt(dc, hTiny, L"Note: running the rewriter again on the same text (outside of Passes) may give unwanted results.", 462, 354, 280, 26, RGB(176, 92, 20), DT_LEFT | DT_WORDBREAK);
            glassPanel(c, 96, 390, 654, 36);
        }
        if (p == PG_DETECT) {
            glassPanel(c, 96, 10, 654, 40);
            textAt(dc, hTitle, L"AI Detector", 108, 19, 200, 22, NAVY, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
            textAt(dc, hSmall, L"trained model + algorithm · runs offline", 400, 22, 338, 16, HINT, DT_RIGHT | DT_SINGLELINE);
            glassPanel(c, 96, 58, 330, 324);
            textAt(dc, hTitle, L"Text to check", 108, 62, 200, 20, NAVY, DT_LEFT | DT_SINGLELINE);
            textAt(dc, hSmall, L"checks as you type", 260, 64, 156, 16, HINT, DT_RIGHT | DT_SINGLELINE);
            glassPanel(c, 434, 58, 316, 324);
            textAt(dc, hTitle, L"AI likelihood", 446, 62, 200, 20, NAVY, DT_LEFT | DT_SINGLELINE);
            glassPanel(c, 96, 390, 654, 36);
            textAt(dc, hSmall, L"Stronger check:", 456, 400, 102, 16, HINT, DT_RIGHT | DT_SINGLELINE);
        }
        if (p == PG_UPDATE) {
            glassPanel(c, 96, 10, 654, 414);
            textAt(dc, hTitle, L"Updates", 108, 20, 300, 20, NAVY, DT_LEFT | DT_SINGLELINE);
            textAt(dc, hSmall, L"from GitHub · works offline", 420, 22, 318, 16, HINT, DT_RIGHT | DT_SINGLELINE);
            inset(c, 106, 46, 634, 88);
            inset(c, 106, 160, 634, 190);
        }
        if (p == PG_LOCK) {
            glassPanel(c, 190, 54, 470, 340);
            drawAppIcon(dc, S(425), S(110), S(80));
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
    } else if (kind == 5) {
        double r = s * 0.27, mx = cx - s * 0.08, my = cy - s * 0.08;
        ring(c, mx, my, r, col, a, th);
        segment(c, mx + r * 0.72, my + r * 0.72, cx + s * 0.42, cy + s * 0.42, th * 1.5, col, a);
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
        if (active) drawIcon(c, TABICON[i], x + w / 2, y + S(21) + 1, S(24), 0x0b5a66, 0.6);
        drawIcon(c, TABICON[i], x + w / 2, y + S(21), S(24), icol, 1);
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
        textAt(dc, hFont, info, 132, 52, 600, 62, NAVY, DT_LEFT | DT_WORDBREAK);
        wchar_t idl[80];
        if (curKey[0]) swprintf(idl, 80, L"This copy:  ID %.9ls  ·  activated", curKey + 5); else wcscpy(idl, L"This copy:  not activated");
        textAt(dc, hFont, idl, 132, 114, 600, 18, HINT, DT_LEFT | DT_SINGLELINE);
        if (updPct >= 0) {
            double x = S(106), y = S(140), w = S(634), h = S(14), r = S(7), f = w * (updPct > 100 ? 100 : updPct) / 100.0;
            rrFill(&backC, x, y, w, h, r, 0xe8f6fa, 0xcfeef5, 0.95, 0.95);
            if (f > r * 2) { rrFill(&backC, x, y, f, h, r, 0x9ff0a0, 0x2f9f26, 1, 1); rrGloss(&backC, x, y, f, h, r, 0.5, 0.7, 0.15); }
            rrStroke(&backC, x, y, w, h, r, 0x1a8fa3, 0.6, 1, 0);
        } else if (updForce) {
            textAt(dc, hBold, L"This version is no longer supported. Please install the update.", 106, 138, 634, 18, RGB(200, 60, 40), DT_LEFT | DT_SINGLELINE);
        }
        wchar_t hd[96];
        if (updLatest[0]) swprintf(hd, 96, L"What's new in %ls", updLatest); else wcscpy(hd, L"What's new");
        textAt(dc, hBold, hd, 116, 164, 400, 18, NAVY, DT_LEFT | DT_SINGLELINE);
    }
    if (curPage == PG_DETECT) {
        if (detHave) {
            int sc = det.score;
            COLORREF col = sc < 25 ? RGB(30, 140, 60) : sc < 50 ? RGB(110, 150, 30) : sc < 75 ? RGB(210, 120, 20) : RGB(200, 50, 40);
            wchar_t b[16]; swprintf(b, 16, L"%d%%", sc);
            textAt(dc, hHuge, b, 444, 80, 104, 46, col, DT_LEFT | DT_SINGLELINE | DT_VCENTER);
            textAt(dc, hBold, det.verdict, 552, 86, 190, 18, col, DT_LEFT | DT_SINGLELINE);
            wchar_t cf[96]; swprintf(cf, 96, L"%d words · %ls", det.words, det.conf);
            textAt(dc, hTiny, cf, 552, 103, 190, 13, HINT, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
            wchar_t mb[96]; swprintf(mb, 96, L"model %d%%  ·  algorithm %d%%", det.pModel, det.pAlgo);
            textAt(dc, hTiny, mb, 552, 115, 190, 13, NAVY, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
            double x = S(446), y = S(130), w = S(292), h = S(10), r = S(5), f = w * sc / 100.0;
            int bc = sc < 25 ? 0x3cbf5a : sc < 50 ? 0x9ccc2a : sc < 75 ? 0xf0a030 : 0xe0483a;
            rrFill(&backC, x, y, w, h, r, 0xe8f6fa, 0xcfeef5, 0.95, 0.95);
            if (f > r * 2) { rrFill(&backC, x, y, f, h, r, bc, bc, 1, 1); rrGloss(&backC, x, y, f, h, r, 0.5, 0.7, 0.15); }
            rrStroke(&backC, x, y, w, h, r, 0x1a8fa3, 0.6, 1, 0);
            for (int k = 0; k < det.nSig && k < 4; k++)
                textAt(dc, hSmall, det.sig[k], 446, 146 + k * 15, 296, 15, det.sig[k][0] == 0x25B2 ? RGB(170, 70, 30) : RGB(30, 120, 70), DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);
            wchar_t gh[64]; swprintf(gh, 64, L"Grammar & issues (%d)", det.nGram);
            textAt(dc, hBold, gh, 446, 210, 296, 18, NAVY, DT_LEFT | DT_SINGLELINE);
        } else {
            textAt(dc, hFont, L"Paste or type text on the left. A small model trained on ~75,000 human and AI samples (ChatGPT, Claude, Gemini, Llama, and more) runs right here on your PC, alongside a writing-pattern algorithm. It also lists grammar, spelling and punctuation problems.\n\nIt's an estimate, not proof.", 446, 88, 292, 120, HINT, DT_LEFT | DT_WORDBREAK);
            textAt(dc, hBold, L"Grammar & issues", 446, 210, 296, 18, NAVY, DT_LEFT | DT_SINGLELINE);
        }
    }
    if (runPct >= 0) {
        double x = S(98), y = S(434), w = S(650), h = S(26), r = S(12), f = w * (runPct > 100 ? 100 : runPct) / 100.0;
        if (f > r * 2) { rrFill(&backC, x, y, f, h, r, 0xc6f5c0, 0x7fd88a, 0.85, 0.85); rrGloss(&backC, x, y, f, h, r, 0.5, 0.6, 0.1); }
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
    const BtnPal *p = (id == ID_CLR || id == ID_DCLR) ? &PAL_RED : (id == ID_GO || id == ID_UINSTALL || id == ID_ACT || id == ID_DCHECK) ? &PAL_GREEN : &PAL_TEAL;
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
    if (m == WM_KEYDOWN && wp == VK_RETURN && GetKeyState(VK_CONTROL) < 0 && (GetDlgCtrlID(h) == ID_IN || GetDlgCtrlID(h) == ID_DIN)) {
        PostMessageW(hMain, WM_COMMAND, GetDlgCtrlID(h) == ID_IN ? ID_GO : ID_DCHECK, 0);
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

typedef struct { wchar_t *in, *out, *err; int toClip, engine, passes, asked, stopped, converged, escalated, maxRun; NanoStats st; } NRes;

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
        else if (r->maxRun)
            status(r->passes >= 10 ? L"MAX done: %d passes, %d swaps, %d rebuilds, nothing undone or repeated.%ls" :
                   L"MAX: only %d passes were possible (%d swaps, %d rebuilds). Add more text for a full rebuild.%ls",
                   r->passes, r->st.changed, r->st.restructured, tail);
        else if (r->asked <= 1)
            status(L"Done. %d of %d words reworded, %d sentence%ls rearranged, %d part%ls locked.%ls",
                   r->st.changed, r->st.words, r->st.restructured, r->st.restructured == 1 ? L"" : L"s",
                   r->st.locks, r->st.locks == 1 ? L"" : L"s", tail);
        else status(L"%ls %d pass%ls: %d swaps, %d rearrangements, nothing undone or repeated.%ls%ls",
                   r->stopped ? L"Stopped after" : L"Done,", r->passes, r->passes == 1 ? L"" : L"es",
                   r->st.changed, r->st.restructured,
                   r->converged ? L" Ran out of new changes early." : L"", tail);
    }
    free(r->in); free(r->out); free(r->err); free(r);
    InterlockedExchange(&nanoBusy, 0);
    setGoLabel();
}

typedef struct { wchar_t *text; NanoOpts o; int passes, toClip, typos; } PJob;
static volatile LONG stopPasses;
static wchar_t *sampleText = L"";



static void setRunPct(int p) { runPct = p; RECT r = RS(96, 432, 654, 30); InvalidateRect(hMain, &r, FALSE); }

static DWORD WINAPI PassThread(LPVOID arg) {
    PJob *j = arg;
    NRes *res = calloc(1, sizeof *res);
    int maxRun = j->o.strength == 3, target = maxRun ? 1000 : j->passes;
    ULONGLONG t0 = GetTickCount64();
    res->in = j->text; res->toClip = j->toClip; res->asked = target;
    NanoCtx ctx = {0};
    curCtx = &ctx;
    StrSet outs = {0};
    LARGE_INTEGER c; QueryPerformanceCounter(&c);
    unsigned long long seed = (unsigned long long)c.QuadPart ^ GetTickCount64();
    wchar_t *cur = wdup(j->text);
    ssAdd(&outs, cur);
    DWORD lastMsg = 0;
    int esc = 0, attempts = 0, resets = 0;
    if (target > 1) setRunPct(0);
    while (res->passes < target) {
        if (stopPasses) { res->stopped = 1; break; }
        if (maxRun && res->passes >= 10 && GetTickCount64() - t0 >= 5000) break;
        NanoStats st;
        escLevel = esc;
        wchar_t *nx = nanoRewrite(cur, &j->o, &st, seed + (unsigned long long)(attempts++) * 0x9E3779B97F4A7C15ULL);
        if (!res->st.words) { res->st.words = st.words; res->st.locks = st.locks; }
        int same = !wcscmp(nx, cur) || ssHas(&outs, nx);
        if (same && !(res->passes == 0 && target == 1)) {
            free(nx);
            if (!maxRun) { res->converged = 1; break; }
            if (++esc > 4) {
                if (res->passes >= 10 && GetTickCount64() - t0 >= 5000) break;
                if (resets < 2) {
                    resets++;
                    if (resets == 1) ssFree(&ctx.seen); else ssFree(&ctx.retired);
                    esc = 2;
                    continue;
                }
                res->converged = 1; break;
            }
            if (esc > res->escalated) res->escalated = esc;
            continue;
        }
        ssAdd(&outs, nx);
        res->st.changed += st.changed; res->st.restructured += st.restructured;
        free(cur); cur = nx; res->passes++;
        esc = 0; resets = 0;
        if (target > 1 && GetTickCount() - lastMsg > 60) {
            lastMsg = GetTickCount();
            if (maxRun) {
                int el = (int)(GetTickCount64() - t0), p = (res->passes < 10 ? res->passes : 10) * 5 + (el > 5000 ? 50 : el / 100);
                setRunPct(p);
                status(L"MAX: pass %d%ls… click the arrow again to stop.", res->passes, res->passes < 10 ? L" (min 10)" : L", going until 5s");
            } else {
                setRunPct(res->passes * 100 / target);
                status(L"Pass %d of %d… click the arrow again to stop.", res->passes, target);
            }
        }
    }
    escLevel = 0;
    curCtx = NULL;
    ssFree(&outs);
    ssFree(&ctx.retired); ssFree(&ctx.seen); free(ctx.origin.val); ssFree(&ctx.origin.keys);
    if (j->o.strength >= 2 && !res->stopped) {
        int contract = (j->o.tone == TN_CASUAL || j->o.tone == TN_SIMPLE || j->o.type == WT_EMAIL || j->o.type == WT_SOCIAL || j->o.type == WT_STORY || (j->o.style && gStyle.on && gStyle.contr100 > 1.5)) && j->o.tone != TN_FORMAL && j->o.tone != TN_ACADEMIC;
        wchar_t *hz = humanizePass(cur, j->o.strength == 3 ? 2 : 1, seed ^ 0x5A5A5A5AULL, contract);
        free(cur); cur = hz;
    }
    if (j->typos > 0 && !res->stopped) {
        static const int PER[5] = { 0, 150, 80, 40, 20 };
        wchar_t *ty = addTypos(cur, PER[j->typos % 5], seed ^ 0xA5A5A5A5ULL);
        free(cur); cur = ty;
    }
    res->out = cur;
    res->maxRun = maxRun;
    free((void *)j->o.style);
    free(j);
    setRunPct(-1);
    PostMessageW(hMain, WM_NANODONE, 0, (LPARAM)res);
    return 0;
}

static void doRewrite(int fromClip) {
    if (!activated) return;
    if (nanoBusy) { InterlockedExchange(&stopPasses, 1); status(L"Stopping…"); return; }
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
    PJob *jb = calloc(1, sizeof *jb);
    jb->text = text; jb->toClip = fromClip;
    jb->typos = (int)SendDlgItemMessageW(hMain, ID_TYPOS, CB_GETCURSEL, 0, 0); if (jb->typos < 0) jb->typos = 0;
    jb->o.strength = strength(); jb->o.strict = tog[TG_STRICT].on;
    jb->o.tone = (int)SendDlgItemMessageW(hMain, ID_TONE, CB_GETCURSEL, 0, 0); if (jb->o.tone < 0 || jb->o.tone > 5) jb->o.tone = 0;
    if (jb->o.tone == 5) { jb->o.tone = 0; jb->o.style = sampleText[0] ? wdup(sampleText) : NULL; }
    jb->o.type = (int)SendDlgItemMessageW(hMain, ID_TYPE, CB_GETCURSEL, 0, 0); if (jb->o.type < 0) jb->o.type = 0;
    jb->passes = (int)GetDlgItemInt(hMain, ID_PASSES, NULL, FALSE);
    if (jb->passes < 1) jb->passes = 1;
    if (jb->passes > 1000) { jb->passes = 1000; SetDlgItemInt(hMain, ID_PASSES, 1000, FALSE); }
    InterlockedExchange(&stopPasses, 0);
    if (jb->passes > 1) status(L"Rewriting… click the arrow again to stop.");
    CloseHandle(CreateThread(NULL, 0, PassThread, jb, 0, NULL));
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
    case WM_SETFOCUS: SetFocus(GetDlgItem(hMain, curPage == PG_LOCK ? ID_ACTKEY : curPage == PG_UPDATE ? ID_UCHECK : curPage == PG_DETECT ? ID_DIN : ID_IN)); return 0;
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
    SetFocus(GetDlgItem(hMain, p == PG_LOCK ? ID_ACTKEY : p == PG_UPDATE ? ID_UCHECK : p == PG_DETECT ? ID_DIN : ID_IN));
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
    mkRich(106, 86, 280, 264, ID_IN);
    mkRich(460, 86, 280, 264, ID_OUT);
    BTN(L"Rewrite", 399, 196, 48, 48, ID_GO);
    HWND pe = mk(WS_EX_CLIENTEDGE, L"EDIT", L"1", WS_TABSTOP | ES_NUMBER | ES_CENTER, 624, 18, 50, 24, ID_PASSES);
    SendMessageW(pe, EM_SETLIMITTEXT, 4, 0);
    {
        static const wchar_t *TONES[] = { L"Tone: Natural", L"Tone: Casual", L"Tone: Formal", L"Tone: Academic", L"Tone: Simple", L"Tone: My writing", L"Edit my sample...", NULL };
        static const wchar_t *TYPES[] = { L"Type: Any", L"Type: Essay", L"Type: Email", L"Type: Story", L"Type: Report", L"Type: Social post", NULL };
        HWND c1 = mk(0, L"COMBOBOX", L"", WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, 548, 60, 94, 200, ID_TONE);
        HWND c2 = mk(0, L"COMBOBOX", L"", WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, 646, 60, 98, 200, ID_TYPE);
        for (int i = 0; TONES[i]; i++) SendMessageW(c1, CB_ADDSTRING, 0, (LPARAM)TONES[i]);
        for (int i = 0; TYPES[i]; i++) SendMessageW(c2, CB_ADDSTRING, 0, (LPARAM)TYPES[i]);
        SendMessageW(c1, CB_SETCURSEL, 0, 0); SendMessageW(c2, CB_SETCURSEL, 0, 0);
    }
    {
        static const wchar_t *TY[] = { L"Typos: Off", L"Typos: Rare", L"Typos: Some", L"Typos: Many", L"Typos: Lots", NULL };
        HWND c3 = mk(0, L"COMBOBOX", L"", WS_TABSTOP | WS_VSCROLL | CBS_DROPDOWNLIST, 348, 397, 120, 200, ID_TYPOS);
        for (int i = 0; TY[i]; i++) SendMessageW(c3, CB_ADDSTRING, 0, (LPARAM)TY[i]);
        SendMessageW(c3, CB_SETCURSEL, 0, 0);
    }
    BTN(L"Copy", 480, 393, 76, 30, ID_COPY);
    BTN(L"Use as input", 560, 393, 106, 30, ID_USE);
    BTN(L"Clear", 670, 393, 70, 30, ID_CLR);

    buildPage = PG_DETECT;
    mkRich(106, 86, 310, 286, ID_DIN);
    HWND dl = mkRich(446, 230, 294, 142, ID_DLIST);
    SendMessageW(dl, EM_SETREADONLY, TRUE, 0);
    SendMessageW(dl, EM_SETBKGNDCOLOR, 0, RGB(250, 254, 255));
    SendMessageW(dl, WM_SETFONT, (WPARAM)hSmall, TRUE);
    BTN(L"Check", 106, 393, 84, 30, ID_DCHECK);
    BTN(L"Paste", 194, 393, 70, 30, ID_DPASTE);
    BTN(L"From Rewrite", 268, 393, 112, 30, ID_DFROM);
    BTN(L"Clear", 384, 393, 64, 30, ID_DCLR);
    BTN(L"Open Pangram", 562, 393, 178, 30, ID_DPANG);

    buildPage = PG_UPDATE;
    mk(0, L"EDIT", L"", WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL, 114, 184, 618, 160, ID_UNOTES);
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

static HFONT ctlFont(int id) { return id == ID_IN || id == ID_OUT || id == ID_DIN ? hEdit : id == ID_ACTKEY ? hBrand : id == ID_DLIST ? hSmall : hFont; }
static void runDetect(int quiet);

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
    if (detHave) runDetect(1);
    ShowWindow(hMain, SW_SHOW);
    showPage(curPage);
    RedrawWindow(hFrame, NULL, NULL, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    wchar_t b[32]; swprintf(b, 32, L"%.3f", uiScale);
    WritePrivateProfileStringW(L"Settings", L"Scale", b, iniPath);
}

static const wchar_t *TOGKEY[TG_N] = { L"Light", L"Medium", L"Heavy", L"Max", L"LockFacts", L"Highlight", L"UpdateCheck", L"UpdateAuto" };

/* ---- "My writing" sample ---- */
static HWND hSample;
static void samplePath(wchar_t *p) { wcscpy(p, iniPath); wchar_t *d = wcsrchr(p, L'\\'); if (d) wcscpy(d + 1, L"FrutigerNano-sample.txt"); }
static void loadSample(void) {
    wchar_t p[MAX_PATH + 32]; samplePath(p);
    FILE *f = _wfopen(p, L"rb"); if (!f) return;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n <= 2 || n > 4000000) { fclose(f); return; }
    char *b = malloc(n + 2); fread(b, 1, n, f); fclose(f); b[n] = b[n + 1] = 0;
    int off = ((unsigned char)b[0] == 0xFF && (unsigned char)b[1] == 0xFE) ? 2 : 0;
    sampleText = wdup((wchar_t *)(b + off)); free(b);
}
static void saveSample(const wchar_t *t) {
    wchar_t p[MAX_PATH + 32]; samplePath(p);
    FILE *f = _wfopen(p, L"wb"); if (!f) return;
    unsigned char bom[2] = { 0xFF, 0xFE }; fwrite(bom, 1, 2, f); fwrite(t, sizeof(wchar_t), wcslen(t), f); fclose(f);
}
static LRESULT CALLBACK SampleProc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    switch (m) {
    case WM_COMMAND:
        if (LOWORD(wp) == 1) {
            HWND e = GetDlgItem(h, 10); int n = GetWindowTextLengthW(e);
            wchar_t *t = calloc(n + 2, sizeof(wchar_t)); GetWindowTextW(e, t, n + 1);
            if (sampleText[0]) free(sampleText);
            sampleText = t; saveSample(t);
            SendDlgItemMessageW(hMain, ID_TONE, CB_SETCURSEL, 5, 0);
            int words = 0; for (wchar_t *q = t; *q; q++) if (iswalpha(*q) && (q == t || !iswalpha(q[-1]))) words++;
            if (words < 60) status(L"Saved, but the sample is short (%d words). A few paragraphs works much better.", words);
            else status(L"Writing sample saved (%d words). Tone is set to My writing.", words);
            DestroyWindow(h); return 0;
        }
        if (LOWORD(wp) == 2) { DestroyWindow(h); return 0; }
        break;
    case WM_CLOSE: DestroyWindow(h); return 0;
    case WM_DESTROY: hSample = NULL; EnableWindow(hFrame, TRUE); SetForegroundWindow(hFrame); return 0;
    }
    return DefWindowProcW(h, m, wp, lp);
}
static void openSampleBox(void) {
    if (hSample) { SetForegroundWindow(hSample); return; }
    static int reg;
    if (!reg) { WNDCLASSW c = {0}; c.lpfnWndProc = SampleProc; c.hInstance = GetModuleHandleW(NULL); c.lpszClassName = L"NanoSample"; c.hCursor = LoadCursor(NULL, IDC_ARROW); c.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1); RegisterClassW(&c); reg = 1; }
    RECT fr; GetWindowRect(hFrame, &fr);
    int w = S(560), hh = S(420);
    hSample = CreateWindowExW(WS_EX_DLGMODALFRAME, L"NanoSample", L"My writing sample", WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
                              fr.left + (fr.right - fr.left - w) / 2, fr.top + (fr.bottom - fr.top - hh) / 2, w, hh, hFrame, NULL, GetModuleHandleW(NULL), NULL);
    HWND l = CreateWindowExW(0, L"STATIC", L"Paste a few paragraphs you (or your class) wrote. Nano will lean toward these words, sentence lengths and contractions when Tone is set to My writing.",
                             WS_CHILD | WS_VISIBLE, S(12), S(8), w - S(30), S(36), hSample, NULL, NULL, NULL);
    HWND e = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", sampleText, WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
                             S(12), S(48), w - S(30), hh - S(130), hSample, (HMENU)10, NULL, NULL);
    SendMessageW(e, EM_SETLIMITTEXT, 0, 0);
    HWND b1 = CreateWindowExW(0, L"BUTTON", L"Save", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, w - S(200), hh - S(74), S(84), S(28), hSample, (HMENU)1, NULL, NULL);
    HWND b2 = CreateWindowExW(0, L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE, w - S(106), hh - S(74), S(84), S(28), hSample, (HMENU)2, NULL, NULL);
    HWND ctl[4] = { l, e, b1, b2 }; for (int i = 0; i < 4; i++) SendMessageW(ctl[i], WM_SETFONT, (WPARAM)(i == 1 ? hEdit : hFont), TRUE);
    EnableWindow(hFrame, FALSE);
    SetFocus(e);
}

static void loadSettings(void) {
    for (int i = 0; i < TG_N; i++) tog[i].on = GetPrivateProfileIntW(L"Settings", TOGKEY[i], tog[i].on, iniPath) != 0;
    SendDlgItemMessageW(hMain, ID_TONE, CB_SETCURSEL, GetPrivateProfileIntW(L"Settings", L"Tone", 0, iniPath) % 6, 0);
    loadSample();
    SendDlgItemMessageW(hMain, ID_TYPOS, CB_SETCURSEL, GetPrivateProfileIntW(L"Settings", L"Typos", 0, iniPath) % 5, 0);
    SendDlgItemMessageW(hMain, ID_TYPE, CB_SETCURSEL, GetPrivateProfileIntW(L"Settings", L"Type", 0, iniPath) % 6, 0);
    { int ps = GetPrivateProfileIntW(L"Settings", L"Passes", 1, iniPath); SetDlgItemInt(hMain, ID_PASSES, ps < 1 ? 1 : ps > 1000 ? 1000 : ps, FALSE); savedPasses = ps < 1 ? 1 : ps; }
    if (tog[TG_LIGHT].on + tog[TG_MED].on + tog[TG_HEAVY].on + tog[TG_MAX].on != 1) { tog[TG_LIGHT].on = tog[TG_HEAVY].on = tog[TG_MAX].on = 0; tog[TG_MED].on = 1; }
    wchar_t b[400];
    GetPrivateProfileStringW(L"Activation", L"Key", L"", b, 400, iniPath);
    activated = keyValid(b, curKey);
    if (!activated) curKey[0] = 0;
}

static void saveSettings(void) {
    for (int i = 0; i < TG_N; i++) WritePrivateProfileStringW(L"Settings", TOGKEY[i], tog[i].on ? L"1" : L"0", iniPath);
    { wchar_t b[16]; swprintf(b, 16, L"%d", (int)SendDlgItemMessageW(hMain, ID_TONE, CB_GETCURSEL, 0, 0)); WritePrivateProfileStringW(L"Settings", L"Tone", b, iniPath);
      swprintf(b, 16, L"%d", (int)SendDlgItemMessageW(hMain, ID_TYPE, CB_GETCURSEL, 0, 0)); WritePrivateProfileStringW(L"Settings", L"Type", b, iniPath);
      swprintf(b, 16, L"%d", (int)SendDlgItemMessageW(hMain, ID_TYPOS, CB_GETCURSEL, 0, 0)); WritePrivateProfileStringW(L"Settings", L"Typos", b, iniPath); }
    { wchar_t b[16]; swprintf(b, 16, L"%d", tog[TG_MAX].on ? savedPasses : (int)GetDlgItemInt(hMain, ID_PASSES, NULL, FALSE)); WritePrivateProfileStringW(L"Settings", L"Passes", b, iniPath); }
}

static void idleStatus(void) {
    status(L"Paste text on the left and hit the arrow. Quotes, (parens), names and numbers stay locked. Ctrl+Alt+N rewrites your clipboard.");
}

static void enableHotkey(void) {
    if (!RegisterHotKey(hMain, HK_CLIP, MOD_CONTROL | MOD_ALT | 0x4000, 'N'))
        status(L"Ctrl+Alt+N is taken by another app, so clipboard rewriting is off. Everything else works.");
    else idleStatus();
}

static int keyRevoked(void) {
    if (!updRevoked || !curKey[0]) return 0;
    wchar_t id[16]; swprintf(id, 16, L"%.9ls", curKey + 5);
    for (const wchar_t *p = updRevoked; *p;) {
        const wchar_t *e = p; while (*e && *e != L'\n' && *e != L'\r') e++;
        while (p < e && iswspace(*p)) p++;
        if (*p != L'#' && e - p >= 9) {
            wchar_t ln[64]; int n = (int)(e - p) < 63 ? (int)(e - p) : 63; int q = 0;
            for (int i = 0; i < n; i++) if (iswalnum(p[i]) || p[i] == L'-') ln[q++] = (wchar_t)towupper(p[i]);
            ln[q] = 0;
            if (wcsstr(ln, id)) return 1;
        }
        p = *e ? e + 1 : e;
    }
    return 0;
}

static void tryActivate(void) {
    wchar_t in[64], norm[24];
    GetDlgItemTextW(hMain, ID_ACTKEY, in, 64);
    if (!keyValid(in, norm)) {
        status(in[0] ? L"That key isn't valid. Check for typos (dashes and spaces don't matter)." : L"Type your activation key first.");
        return;
    }
    WritePrivateProfileStringW(L"Activation", L"Key", norm, iniPath);
    wcscpy(curKey, norm);
    if (keyRevoked()) {
        WritePrivateProfileStringW(L"Activation", L"Key", L"", iniPath); curKey[0] = 0;
        status(L"That key has been revoked. Contact the seller for a new one.");
        return;
    }
    activated = 1;
    showPage(PG_REWRITE);
    InvalidateRect(hMain, NULL, FALSE);
    enableHotkey();
    status(L"Activated, welcome to Frutiger Nano. Paste text on the left and hit the arrow.");
}


static void syncPassesBox(void) {
    HWND pe = GetDlgItem(hMain, ID_PASSES);
    if (tog[TG_MAX].on) {
        if (IsWindowEnabled(pe)) { int v = (int)GetDlgItemInt(hMain, ID_PASSES, NULL, FALSE); savedPasses = v < 1 ? 1 : v; }
        SetWindowTextW(pe, L"10+"); EnableWindow(pe, FALSE);
    } else if (!IsWindowEnabled(pe)) { EnableWindow(pe, TRUE); SetDlgItemInt(hMain, ID_PASSES, savedPasses, FALSE); }
}

static void runDetect(int quiet) {
    HWND in = GetDlgItem(hMain, ID_DIN), ls = GetDlgItem(hMain, ID_DLIST);
    wchar_t *t = getRichText(in);
    int any = 0; for (wchar_t *p = t; *p; p++) if (!iswspace(*p)) { any = 1; break; }
    dtFree(&det); detHave = 0;
    hiliting = 1;
    SendMessageW(in, WM_SETREDRAW, FALSE, 0);
    CHARRANGE keep; SendMessageW(in, EM_EXGETSEL, 0, (LPARAM)&keep);
    POINT scroll; SendMessageW(in, EM_GETSCROLLPOS, 0, (LPARAM)&scroll);
    resetTextFormat(in);
    if (any) {
        dtScore(t, &det); detHave = 1;
        CHARFORMAT2W cf; memset(&cf, 0, sizeof cf); cf.cbSize = sizeof cf; cf.dwMask = CFM_BACKCOLOR | CFM_COLOR;
        for (int pass = 0; pass < 2; pass++)
            for (int k = 0; k < det.nIss; k++) {
                Issue *is = &det.iss[k];
                if ((is->kind == IS_AI) != (pass == 0)) continue;
                cf.crBackColor = is->kind == IS_AI ? RGB(236, 226, 255) : is->kind == IS_STYLE ? RGB(226, 240, 255) : RGB(255, 236, 168);
                cf.crTextColor = is->kind == IS_AI ? RGB(96, 52, 160) : is->kind == IS_STYLE ? RGB(30, 80, 150) : RGB(150, 70, 0);
                CHARRANGE cr = { is->s, is->s + is->len };
                SendMessageW(in, EM_EXSETSEL, 0, (LPARAM)&cr);
                SendMessageW(in, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
            }
    }
    SendMessageW(in, EM_EXSETSEL, 0, (LPARAM)&keep);
    SendMessageW(in, EM_SETSCROLLPOS, 0, (LPARAM)&scroll);
    SendMessageW(in, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(in, NULL, TRUE);
    hiliting = 0;
    Str o = {0}; sAdd(&o, L"");
    if (detHave) {
        int num = 0;
        for (int k = 0; k < det.nIss; k++) {
            Issue *is = &det.iss[k];
            if (is->kind == IS_AI) continue;
            wchar_t line[180]; swprintf(line, 180, L"%d. %ls: %ls\n", ++num, ISNAME[is->kind], is->msg);
            sAdd(&o, line);
        }
        if (!num) sAdd(&o, L"No grammar, spelling or punctuation problems found.");
    }
    setRichText(ls, o.s); free(o.s);
    InvalidateRect(hMain, NULL, FALSE);
    if (!detHave) { if (!quiet) status(L"Type or paste some text in the box first."); }
    else status(L"%d words checked: %d%% AI-likely (%ls), %d issue%ls found. Yellow = mistakes, purple = AI-style phrases.",
                det.words, det.score, det.verdict, det.nGram, det.nGram == 1 ? L"" : L"s");
    free(t);
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
        if (tog[i].radio == 2) syncPassesBox();
        if (i == TG_MAX) status(L"MAX: at least 10 passes, then keeps going as long as it's under 5 seconds. Progress bar shows how far along it is.");
        return;
    }
}

static LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM wp, LPARAM lp) {
    switch (m) {
    case WM_COMMAND:
        switch (LOWORD(wp)) {
        case ID_ACT: tryActivate(); break;
        case ID_TONE:
            if (HIWORD(wp) == CBN_SELCHANGE) {
                int sel = (int)SendDlgItemMessageW(h, ID_TONE, CB_GETCURSEL, 0, 0);
                if (sel == 6 || (sel == 5 && !sampleText[0])) { SendDlgItemMessageW(h, ID_TONE, CB_SETCURSEL, sampleText[0] ? 5 : 0, 0); openSampleBox(); }
            }
            break;
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
                status(L"Moved over. Heads up: re-running on the same text can give unwanted results; Passes is safer.");
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
        case ID_DIN:
            if (HIWORD(wp) == EN_CHANGE && !hiliting) SetTimer(h, TIMER_DET, 600, NULL);
            break;
        case ID_DCHECK: runDetect(0); break;
        case ID_DPASTE: {
            wchar_t *t = clipGet();
            if (t && t[0]) { setRichText(GetDlgItem(h, ID_DIN), t); runDetect(0); }
            else status(L"Clipboard has no text to paste.");
            free(t);
        } break;
        case ID_DFROM: {
            wchar_t *t = getRichText(GetDlgItem(h, ID_OUT));
            if (t[0]) { setRichText(GetDlgItem(h, ID_DIN), t); runDetect(0); }
            else status(L"The Rewritten box is empty. Rewrite something first.");
            free(t);
        } break;
        case ID_DCLR:
            setRichText(GetDlgItem(h, ID_DIN), L""); setRichText(GetDlgItem(h, ID_DLIST), L"");
            dtFree(&det); detHave = 0; InvalidateRect(h, NULL, FALSE); idleStatus();
            SetFocus(GetDlgItem(h, ID_DIN));
            break;
        case ID_DPANG:
            ShellExecuteW(NULL, L"open", L"https://www.pangram.com/dashboard?pendingCheck=true&pendingType=text", NULL, NULL, SW_SHOWNORMAL);
            status(L"Opened Pangram in your browser. Paste your text there for a second opinion.");
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
        if (wp == TIMER_DET) { KillTimer(h, TIMER_DET); runDetect(1); }
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
            RECT r = RS(10, 80, 76, 300); InvalidateRect(h, &r, FALSE);
        }
    } return 0;
    case WM_MOUSELEAVE: if (tabHover >= 0) { tabHover = -1; RECT r = RS(10, 80, 76, 300); InvalidateRect(h, &r, FALSE); } return 0;
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
        if (activated && keyRevoked()) {
            WritePrivateProfileStringW(L"Activation", L"Key", L"", iniPath);
            activated = 0; curKey[0] = 0;
            UnregisterHotKey(h, HK_CLIP);
            showPage(PG_LOCK);
            status(L"This copy's key has been revoked. Enter a new activation key.");
            return 0;
        }
        if (updForce && updAvail == 1 && !nanoBusy) { status(L"This version is no longer supported, installing %ls…", updLatest); startInstall(); return 0; }
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
    syncPassesBox();
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
