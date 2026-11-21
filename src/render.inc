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
