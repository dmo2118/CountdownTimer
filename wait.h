#ifndef WAIT_H
#define WAIT_H

/*
There's basically three compilation modes here:
                | _WIN32 | _WINDOWS | __WINE__ | __CYGWIN__
- Win32 (or 64) |      * |  (maybe) |          |
- Win16         |        |        * |          |
- Winelib       |      * |          |        * |
- Cygwin        |        |          |          |          *
*/

#if defined _WINDOWS && !defined _WIN32
#	define WAIT_WIN16 1
#endif

#ifndef RC_INVOKED

#define arraysize(a) (sizeof(a) / sizeof(*(a)))

#endif

#endif
