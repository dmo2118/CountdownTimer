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

#include <windows.h>

#define arraysize(a) (sizeof(a) / sizeof(*(a)))

#if WAIT_WIN16
#	define LOCAL_ALLOC_PTR(bytes) (LocalAlloc(LMEM_FIXED, (bytes)))
#	define LOCAL_FREE(mem) (LocalFree(mem))
#else
extern HANDLE global_heap;
#	define LOCAL_ALLOC_PTR(bytes) (HeapAlloc(global_heap, 0, bytes))
#	define LOCAL_FREE(mem) (HeapFree(global_heap, 0, mem))
#endif

#endif

#endif
