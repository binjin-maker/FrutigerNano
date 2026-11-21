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
