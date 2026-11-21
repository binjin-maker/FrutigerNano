/* Frutiger Nano — rewrites text so it reads completely differently while saying the same thing.
   Offline engine: built-in thesaurus + sentence rearranging.
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

