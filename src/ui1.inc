
/* ---------- app globals ---------- */

#define APP_VERSION   "1.13.0"
#define APP_VERSION_W L"1.13.0"

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

enum { ID_IN = 101, ID_OUT, ID_GO, ID_COPY, ID_USE, ID_CLR, ID_PASSES, ID_TONE, ID_TYPE, ID_TYPOS,
       ID_UCHECK = 301, ID_UINSTALL, ID_UPAGE, ID_UNOTES,
       ID_ACTKEY = 401, ID_ACT,
       ID_DIN = 501, ID_DLIST, ID_DCHECK, ID_DPASTE, ID_DFROM, ID_DCLR, ID_DPANG };
#define HK_CLIP 1
#define TIMER_HL 7
#define TIMER_DET 8

static HWND hMain, hFrame;
static wchar_t iniPath[MAX_PATH], exePath[MAX_PATH];
static float dpi = 1.0f, uiScale = 1.0f;
static volatile LONG nanoBusy;
static int activated;
static wchar_t curKey[24];
static volatile int updPct = -1, runPct = -1;
static int savedPasses = 1;

static int S(int v) { return (int)(v * dpi * uiScale + 0.5f); }

static void status(const wchar_t *fmt, ...) {
    wchar_t *b = malloc(512 * sizeof(wchar_t));
    va_list ap; va_start(ap, fmt); _vsnwprintf(b, 511, fmt, ap); va_end(ap); b[511] = 0;
    PostMessageW(hMain, WM_STATUS, 0, (LPARAM)b);
}

