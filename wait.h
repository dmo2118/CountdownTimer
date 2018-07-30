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
#	define LOCAL_ALLOC_HND(bytes) (LocalAlloc(LMEM_MOVEABLE, (bytes)))
#	define LOCAL_LOCK(mem) (LocalLock(mem))
#	define LOCAL_UNLOCK(mem) (LocalUnlock(mem))
#	define LOCAL_FREE(mem) (LocalFree(mem))
#else
extern HANDLE global_heap;
#	define LOCAL_ALLOC_PTR(bytes) (HeapAlloc(global_heap, 0, bytes))
#	define LOCAL_ALLOC_HND(bytes) (LOCAL_ALLOC_PTR(bytes))
#	define LOCAL_LOCK(mem) (mem)
#	define LOCAL_UNLOCK(mem) ((void)(mem))
#	define LOCAL_FREE(mem) (HeapFree(global_heap, 0, mem))
#endif

#if WAIT_WIN16

typedef char TCHAR;
typedef char FAR *LPTSTR;
typedef const char FAR *LPCTSTR;

typedef int INT;
typedef unsigned long ULONG;

typedef INT INT_PTR;
typedef UINT UINT_PTR;
typedef LONG LONG_PTR;

#	if defined _M_I86TM || defined _M_I86SM || defined _M_I86MM
typedef unsigned SIZE_T;
#	endif
#	if defined _M_I86CM || defined _M_I86LM || defined _M_I86HM
typedef unsigned long SIZE_T;
#	endif

#	define TEXT(s) s

#	define ERROR_NOT_ENOUGH_MEMORY 8

#	define BST_UNCHECKED   0
#	define BST_CHECKED     1

#	define BS_PUSHLIKE 0x00001000

#	define MB_ICONERROR MB_ICONSTOP

#	define GetWindowLongPtr GetWindowLong
#	define SetWindowLongPtr SetWindowLong

#	define DWLP_USER DWL_USER

#	define IDHELP 9

#	define DestroyAcceleratorTable(accel)

#endif

#if defined __MINGW32__ || defined __CYGWIN__ || defined __WINE__
	/* Missing from some older w32api headers. */
#	ifndef UnlockResource
#		define UnlockResource(res_data) ((void)(res_data), 0)
#	endif
#endif

#if defined _WINDOWS || defined _WIN32
extern BYTE global_win_ver; /* May be 4 or greater even as a 16-bit app. */
#	define HAS_WINVER_4() (global_win_ver >= 4)
#else
#	define HAS_WINVER_4() 1
#endif

extern HINSTANCE global_instance;

#endif

#endif
