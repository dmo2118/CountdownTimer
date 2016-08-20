/*
TODO:
- Escape to close
- Just beep -> also beep, maybe?

- Valgrind me!
- Winelib testing via Travis CI.
- OpenWatcom on Linux
- MinGW on macOS
- Async system()
- ShellExecuteEx: it's just better.
- Windows 3.0, 2.0, 1.0.
- HXDOS (again)
- Use a SysLink in the about dialog, when possible. Use LinkWindow_RegisterClass on 2000; manifest takes care of things on XP.
- Wine and Cygwin maybe need an option to spawn a console...or, you know, (x-terminal-emulator|xterm) -e works too.
- Recheck STACKSIZE in .def; maybe 4K is enough now?
*/

/* HX-DOS doesn't support DialogBoxParam. */

/*
There's basically three compilation modes here:
                | _WIN32 | _WINDOWS | __WINE__ | __CYGWIN__
- Win32 (or 64) |      * |  (maybe) |          |
- Win16         |        |        * |          |
- Winelib       |      * |          |        * |
- Cygwin        |        |          |          |          *
*/

#define STRICT 1

#if UNICODE
#	define _UNICODE 1
#endif

#include "resource.h"

#include "common.h"

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#if !defined __WINE__ && defined _WIN32
	/* Native Win32. */
#	include <tchar.h>
#else
#	ifdef _UNICODE
		/* Winelib doesn't let you use tchar.h with the system libc. */
#		define _tWinMain wWinMain
#		define _tcschr wcschr
#	else
#		define _tWinMain WinMain
#		define _tcschr strchr
#	endif
#endif

#if defined _WINDOWS && !defined _WIN32
	/* 16-bit Windows. */
#	include <shellapi.h>
#	include <ver.h>

typedef char TCHAR;
typedef char FAR *LPTSTR;

typedef int INT;
typedef long LONG;
typedef unsigned long ULONG;

typedef INT INT_PTR;
typedef UINT UINT_PTR;
typedef LONG LONG_PTR;

#		if defined _M_I86TM || defined _M_I86SM || defined _M_I86MM
typedef unsigned SIZE_T;
#		endif
#		if defined _M_I86CM || defined _M_I86LM || defined _M_I86HM
typedef unsigned long SIZE_T;
#		endif

#		define TEXT(s) s

static const UINT ERROR_NOT_ENOUGH_MEMORY = 8;

static const UINT BST_UNCHECKED = 0;
static const UINT BST_CHECKED   = 1;

static const ULONG BS_PUSHLIKE = 0x00001000;

static const UINT MB_ICONERROR  = MB_ICONSTOP;

#define GetWindowLongPtr GetWindowLong
#define SetWindowLongPtr SetWindowLong

static const int DWLP_USER = DWL_USER;

typedef struct
{
	DWORD style;
	BYTE cdit;
	WORD x;
	WORD y;
	WORD cx;
	WORD cy;
} DLGTEMPLATE, FAR *LPDLGTEMPLATE;

#	define IDHELP 9

#	define DestroyAcceleratorTable(accel)

#endif

#if defined __WINE__ || defined __CYGWIN__
#	include <errno.h>
#	include <iconv.h>
#	include <locale.h>
#endif

#if defined _WINDOWS && !defined _WIN32
#	define GET_WM_COMMAND_ID(wparam, lparam)    wparam
#	define GET_WM_COMMAND_CMD(wparam, lparam)   HIWORD(lparam)
#else
#	include <windowsx.h>
#endif

#if defined __MINGW32__ || defined __CYGWIN__ || defined __WINE__
	/* Missing from some older w32api headers. */
#	ifndef UnlockResource
#		define UnlockResource(res_data) ((void)(res_data), 0)
#	endif
#endif

#if defined _WINDOWS || defined _WIN32
BYTE _win_ver; /* May be 4 or greater even as a 16-bit app. */
#	define HAS_WINVER_4() (_win_ver >= 4)
#else
#	define HAS_WINVER_4() 1
#endif

static HINSTANCE _instance;

static const TCHAR _title[] = TEXT("Countdown Timer");

static void _stop_timer(HWND dlg, UINT_PTR *id)
{
	if(*id)
	{
		CheckDlgButton(dlg, IDC_START, BST_UNCHECKED);
		SetDlgItemText(dlg, IDC_START, TEXT("&Start"));
		KillTimer(dlg, *id);
		*id = 0;
	}
}

static void _show_time(HWND dlg, LPTSTR time, DWORD seconds)
{
	if(seconds < 60)
		wsprintf(time, TEXT(":%.2u"), (UINT)seconds);
	else if(seconds < 3600)
		wsprintf(time, TEXT("%lu:%.2u"), (ULONG)(seconds / 60), (UINT)(seconds % 60));
	else
		wsprintf(time, TEXT("%lu:%.2u:%.2u"), (ULONG)(seconds / 3600), (UINT)((seconds / 60) % 60), (UINT)(seconds % 60));

	SetDlgItemText(dlg, IDC_TIME, time);
}

static BOOL _valid_time(LPTSTR time)
{
	DWORD dig = 0, field = 0;

	while(*time)
	{
		if(isdigit(*time))
		{
			if(field && dig >= 2)
				return FALSE;

			dig++;
		}
		else if(*time == ':')
		{
			if((!dig && field) || field == 2)
				return FALSE;

			dig = 0;
			field++;
		}
		else
		{
			return FALSE;
		}
		time++;
	}

	return TRUE;
}

static void _just_beep(HWND dlg)
{
	/* Windows 3.1 doesn't make sound with message boxes. */
	if(!HAS_WINVER_4())
		MessageBeep(MB_OK);
	MessageBox(dlg, TEXT("Time elapsed."), _title, MB_OK | MB_ICONINFORMATION);
}

#if defined __CYGWIN__ || defined __WINE__

static void _iconv_realloc(char **cmd_line_ptr, char **out, size_t *out_left)
{
	size_t out_pos = *out - *cmd_line_ptr;
	size_t cmd_line_ptr_size = *out_left + out_pos;
	size_t new_size = cmd_line_ptr_size < 63 ? 63 : cmd_line_ptr_size * 2 + 1;

	char *new_ptr = realloc(*cmd_line_ptr, new_size);
	if(!new_ptr)
	{
		free(*cmd_line_ptr);
		*cmd_line_ptr = NULL;
		return;
	}

	*cmd_line_ptr = new_ptr;
	*out = *cmd_line_ptr + out_pos;
	assert(*out_left + new_size - cmd_line_ptr_size == new_size - out_pos);
	*out_left = new_size - out_pos;
}

#endif

static void _error_message(HWND dlg, UINT result)
{
	/* WinExec/ShellExecute errors on Win16, system errors on Win32. These overlap somewhat. */
#	if defined _WINDOWS && !defined _WIN32

	static const char *errors[] =
	{
		"System was out of memory, executable file was corrupt, or relocations were invalid. ",
		NULL,
		"File was not found. ",
		"Path was not found. ",
		NULL,
		"Attempt was made to dynamically link to a task, or there was a sharing or network-protection error. ",
		"Library required separate data segments for each task. ",
		NULL,
		"There was insufficient memory to start the application. ",
		NULL,
		"Windows version was incorrect. ",
		"Executable file was invalid. Either it was not a Windows application or there was an error in the .EXE image. ",
		"Application was designed for a different operating system. ",
		"Application was designed for MS-DOS 4.0. ",
		"Type of executable file was unknown. ",
		"Attempt was made to load a real-mode application (developed for an earlier version of Windows). ",
		"Attempt was made to load a second instance of an executable file containing multiple data segments that were not marked read-only. ",
		NULL,
		NULL,
		"Attempt was made to load a compressed executable file. The file must be decompressed before it can be loaded. ",
		"Dynamic-link library (DLL) file was invalid. One of the DLLs required to run this application was corrupt. ",
		"Application requires Microsoft Windows 32-bit extensions. ",
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
	};

	char code_str[6];
	const char *error = errors[result];
	if(!error)
	{
		wsprintf(code_str, "%u", result);
		error = code_str;
	}
	MessageBox(dlg, error, _title, MB_ICONERROR | MB_OK);

#	else

	TCHAR *message;

	if(FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_IGNORE_INSERTS |
			FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_MAX_WIDTH_MASK,
		NULL,
		(DWORD)result,
		0,
		(LPTSTR)&message, /* Older GCC: likely spurious type-punning warning here. */
		0,
		NULL))
	{
		MessageBox(dlg, message, _title, MB_ICONERROR | MB_OK);
		LocalFree(message);
	}
	else
	{
		TCHAR text[12];
		wsprintf(text, TEXT("%d"), result);
		MessageBox(dlg, text, _title, MB_ICONERROR | MB_OK);
	}

#	endif
}

static void _run_prog(HWND dlg)
{
#if !defined _WINDOWS || defined _WIN32
	SetForegroundWindow(dlg); /* For flashing taskbar buttons. Not necessary before Windows 98. */
#endif

	if(IsDlgButtonChecked(dlg, IDC_BEEP) != BST_CHECKED)
	{
		HWND command = GetDlgItem(dlg, IDC_COMMAND);
		UINT cmd_line_size = GetWindowTextLength(command) + 1;
		TCHAR *cmd_line = (TCHAR *)LocalAlloc(LMEM_FIXED, cmd_line_size * sizeof(TCHAR));

		if(!cmd_line)
		{
			_error_message(dlg, ERROR_NOT_ENOUGH_MEMORY);
			return;
		}

		cmd_line_size = GetDlgItemText(dlg, IDC_COMMAND, cmd_line, cmd_line_size);

#if defined __CYGWIN__ || defined __WINE__

		{
			char *cmd_line_ptr = NULL;

#	ifdef UNICODE
			const WCHAR *const cmd_line_w = cmd_line;
			const DWORD cmd_line_w_size = cmd_line_size;
#	else

			WCHAR *cmd_line_w = NULL;
			DWORD cmd_line_w_size = MultiByteToWideChar(
				CP_ACP,
				MB_ERR_INVALID_CHARS,
				cmd_line,
				cmd_line_size + 1,
				NULL,
				0);

			if(cmd_line_w_size)
			{
				cmd_line_w = malloc(cmd_line_w_size * sizeof(WCHAR));
				if(cmd_line_w)
				{
					MultiByteToWideChar(
						CP_ACP,
						0,
						cmd_line,
						cmd_line_size + 1,
						cmd_line_w,
						cmd_line_w_size);
				}
			}
#	endif

			if(cmd_line_w)
			{
				/*
				The situation on Winelib:
				wcstombs on... | accepts...     | and gives us... | which...
				glibc          | 32-bit wchar_t | $LC_CTYPE       | won't work
				Wine MSVCRT    | 16-bit WCHAR   | Windows-1252    | won't work
				*/

				iconv_t cd = iconv_open("", "UTF-16LE");
				if(cd != (iconv_t)-1)
				{
					char *in = (char *)cmd_line_w;
					size_t in_left = (cmd_line_w_size + 1) * sizeof(WCHAR);
					char *out = cmd_line_ptr;
					size_t out_left = 0;

					while(in_left)
					{
						assert(!out_left && !out || out);
						if(!out)
						{
							_iconv_realloc(&cmd_line_ptr, &out, &out_left);
							if(!cmd_line_ptr)
								break;
						}
						else if(iconv(cd, &in, &in_left, &out, &out_left) == (size_t)-1)
						{
							int last_error = errno;
							if(last_error == E2BIG)
							{
								_iconv_realloc(&cmd_line_ptr, &out, &out_left);
								if(!cmd_line_ptr)
									break;
							}
							else
							{
								free(cmd_line_ptr);
								cmd_line_ptr = NULL;
								break;
							}
						}
					}

					iconv_close(cd);
				}
			}

#	ifndef UNICODE
			free(cmd_line_w);
#	endif

			if(!cmd_line_ptr || system(cmd_line_ptr) == -1)
				MessageBox(dlg, TEXT("Error issuing a command."), _title, MB_ICONERROR | MB_OK);

			free(cmd_line_ptr);
		}

#else

		{
			TCHAR *param;
			TCHAR *file = cmd_line;
			UINT_PTR result;

			while(isspace(*file))
				file++;

			if(*file == '"')
			{
				file++;
				/* GCC translates __builtin_strchr to simply strchr. */
				param = _tcschr(file, '"');

				if(param)
				{
					*param = 0;
					param++;
				}
			}
			else
			{
				param = file;

				while(*param && !isspace(*param))
					param++;

				if(*param)
				{
					*param = 0;
					param++;
				}
			}

			if(!*file)
			{
				_just_beep(dlg);
			}
			else
			{
				result = (UINT_PTR)ShellExecute(dlg, NULL, file, param, NULL, SW_SHOWNORMAL);
				if(result <= 32)
				{
#	if !defined _WINDOWS || defined _WIN32
					static const DWORD errors26[] =
					{
						ERROR_SHARING_VIOLATION,
						ERROR_NO_ASSOCIATION,
						ERROR_DDE_FAIL,
						ERROR_DDE_FAIL,
						ERROR_DDE_FAIL,
						ERROR_NO_ASSOCIATION,
						ERROR_MOD_NOT_FOUND
					};

					if(result >= 26)
						result = errors26[result - 26];
#	endif

					_error_message(dlg, result);
				}
			}
		}
#endif
		LocalFree(cmd_line);
	}
	else
	{
		_just_beep(dlg);
	}
}

static INT_PTR CALLBACK _about_dialog_proc(HWND dlg, UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch(msg)
	{
	case WM_INITDIALOG:
		{
			static const TCHAR license[] =
				TEXT("Copyright (c) 2000-2016, Dave Odell <dmo2118@gmail.com>\n")
				TEXT("\n")
				TEXT("Permission to use, copy, modify, and/or distribute this software for ")
				TEXT("any purpose with or without fee is hereby granted, provided that the ")
				TEXT("above copyright notice and this permission notice appear in all copies.\n")
				TEXT("\n")
				TEXT("THE SOFTWARE IS PROVIDED \"AS IS\" AND THE AUTHOR DISCLAIMS ALL ")
				TEXT("WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED ")
				TEXT("WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE ")
				TEXT("AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL ")
				TEXT("DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR ")
				TEXT("PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER ")
				TEXT("TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR ")
				TEXT("PERFORMANCE OF THIS SOFTWARE.\n");
			SetDlgItemText(dlg, IDC_LICENSE, license);
		}
		break;
	case WM_COMMAND:
		switch(GET_WM_COMMAND_ID(wparam, lparam))
		{
		case IDCANCEL:
			EndDialog(dlg, 0);
			break;
		}
		break;
	}

	return FALSE;
}

static void _about(HWND dlg)
{
	DialogBox(_instance, MAKEINTRESOURCE(IDD_ABOUT), dlg, _about_dialog_proc);
}

static void _end_dialog(HWND dlg, int result)
{
	/* EndDialog(dlg, result); */
	PostQuitMessage(result);
}

#define MAX_TIME 10

struct _dialog
{
	DWORD seconds;
	TCHAR time[MAX_TIME];
	UINT_PTR timer_id;
};

static const struct _dialog _dialog_init = {0, TEXT(""), 0};

static INT_PTR CALLBACK _main_dialog_proc(HWND dlg, UINT msg, WPARAM wparam, LPARAM lparam)
{
	struct _dialog *self = (struct _dialog *)GetWindowLongPtr(dlg, DWLP_USER);

	switch(msg)
	{
	case WM_INITDIALOG:
		self = (struct _dialog *)lparam;
		SetWindowLongPtr(dlg, DWLP_USER, (SIZE_T)self);

		SendDlgItemMessage(dlg, IDC_TIME, EM_LIMITTEXT, MAX_TIME - 1, 0);

		if(HAS_WINVER_4())
		{
			HWND start = GetDlgItem(dlg, IDC_START);
			SetWindowLongPtr(start, GWL_STYLE, GetWindowLongPtr(start, GWL_STYLE) | (BS_AUTOCHECKBOX | BS_PUSHLIKE));
		}

		{
			HMENU sys_menu = GetSystemMenu(dlg, FALSE);
			AppendMenu(sys_menu, MF_SEPARATOR, 0, NULL);
			AppendMenu(sys_menu, MF_STRING, IDM_ABOUT, TEXT("&About...\tF1"));
		}

		break;

	case WM_CLOSE:
		_end_dialog(dlg, 0);
		break;

	case WM_COMMAND:
		switch(GET_WM_COMMAND_ID(wparam, lparam))
		{
		case IDHELP:
			_about(dlg);
			break;
		case IDC_BEEP:
			EnableWindow(GetDlgItem(dlg, IDC_COMMAND), IsDlgButtonChecked(dlg, IDC_BEEP) != BST_CHECKED);
			break;
		case IDC_TIME:
			switch(GET_WM_COMMAND_CMD(wparam, lparam))
			{
			case EN_UPDATE:
				{
					TCHAR new_time[MAX_TIME];
					UINT_PTR start, end;

					GetDlgItemText(dlg, IDC_TIME, new_time, arraysize(new_time));

#if defined _WINDOWS && !defined _WIN32
					{
						DWORD result = SendDlgItemMessage(dlg, IDC_TIME, EM_GETSEL, 0, 0);
						start = LOWORD(result);
						end   = HIWORD(result);
					}
#else
					SendDlgItemMessage(dlg, IDC_TIME, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
#endif
/*					start--;
					end--; */

					if(_valid_time(new_time))
					{
						lstrcpy(self->time, new_time);
					}
					else
					{
						/* Set old time. */
						MessageBeep(MB_OK);
						SetDlgItemText(dlg, IDC_TIME, self->time);
						SendDlgItemMessage(
							dlg,
							IDC_TIME,
							EM_SETSEL,
#if defined _WINDOWS && !defined _WIN32
							0,
							MAKELPARAM(0, 0)
#else
							start,
							end
#endif
							);
					}
				}

				break;
			case EN_SETFOCUS:
				_stop_timer(dlg, &self->timer_id);
				break;
			case EN_KILLFOCUS:
				{
					TCHAR new_time[MAX_TIME];
					LPTSTR ptr = new_time;
					DWORD dig = 0, field = 0;

					GetDlgItemText(dlg, IDC_TIME, new_time, arraysize(new_time));

					self->seconds = 0;

					while(*ptr)
					{
						if(isdigit(*ptr))
						{
							dig = dig * 10 + *ptr - '0';
						}
						else if(*ptr == ':')
						{
							if(field < 3)
							{
								self->seconds = self->seconds * 60 + dig;
								dig = 0;
								field++;
							}
							else
							{
								break;
							}
						}
						else
						{
							break;
						}
						ptr++;
					}

					self->seconds = self->seconds * 60 + dig;

					{
						const DWORD max_seconds = (999ul * 60 + 59) * 60 + 59;
						if(self->seconds > max_seconds)
							self->seconds = max_seconds;
					}

					_show_time(dlg, self->time, self->seconds);
				}

				break;
			}

			break;

		case IDC_START:
			if(!self->timer_id)
			{
				/* Start timer. */
				self->timer_id = SetTimer(dlg, 1, 1000, NULL);
				if(self->timer_id)
				{
					CheckDlgButton(dlg, IDC_START, BST_CHECKED);
					SetDlgItemText(dlg, IDC_START, TEXT("&Stop"));
				}
			}
			else
			{
				_stop_timer(dlg, &self->timer_id);
			}
			break;

		case IDC_RUNNOW:
			_run_prog(dlg);
			break;

		case IDC_CLOSE:
			_end_dialog(dlg, 0);
			break;
		}
		break;

	case WM_SYSCOMMAND:
		switch(wparam)
		{
		case IDM_ABOUT:
			_about(dlg);
			break;
		}
		break;

	case WM_TIMER:
		if(self->seconds)
		{
			self->seconds--;
			_show_time(dlg, self->time, self->seconds);
		}
		else
		{
			_stop_timer(dlg, &self->timer_id);
			_run_prog(dlg);
		}
		break;
	}
	return FALSE;
}

#if OMIT_CRT
int entry_point()
#elif defined __CYGWIN__
int main()
#else
/* WINAPI is FAR PASCAL, which is incorrect on 16-bit. */
int PASCAL _tWinMain(HINSTANCE instance, HINSTANCE prev_instance, LPTSTR cmd_line, int show_cmd)
#endif
{
#if OMIT_CRT || defined __CYGWIN__
	/* The linker-defined symbol __ImageBase is the same as hInstance under Windows NT and Windows 95, but not Win32s. */
	HINSTANCE instance = GetModuleHandle(NULL);
#endif

	int exit_code;
	struct _dialog self = _dialog_init;

#if defined __CYGWIN__ || defined __WINE__
	/* iconv() needs this. */
	setlocale(LC_ALL, "");
#endif

	{
#if defined _WIN32
		DWORD win_ver = GetVersion();
#	ifdef UNICODE
		if(win_ver & 0x80000000)
		{
			MessageBoxA(NULL, "This program requires Windows NT.", "Countdown Timer", MB_ICONERROR | MB_OK);
			return EXIT_FAILURE;
		}
#	endif
		_win_ver = LOBYTE(win_ver);
#elif defined _WINDOWS
		/* Recommended by KB113892. */
		const TCHAR _user_exe[] = TEXT("user.exe");
		DWORD handle;
		DWORD len = GetFileVersionInfoSize(_user_exe, &handle);
		_win_ver = 3; /* (Cheating) */
		if(len)
		{
			HGLOBAL hblock = GlobalAlloc(GMEM_FIXED, len);
			if(hblock)
			{
				LPVOID block = GlobalLock(hblock);
				if(block)
				{
					if(GetFileVersionInfo(_user_exe, handle, len, block))
					{
						VS_FIXEDFILEINFO FAR *file_info;
						UINT file_info_len;
						if(
							VerQueryValue(block, TEXT("\\"), (LPVOID FAR *)&file_info, &file_info_len) &&
							file_info_len >= sizeof(*file_info) &&
							file_info->dwSignature == 0xFEEF04BD &&
							file_info->dwStrucVersion > 0x00000029)
						{
							_win_ver = LOBYTE(HIWORD(file_info->dwFileVersionMS));
						};
					}

					GlobalUnlock(hblock);
				}

				GlobalFree(hblock);
			}
		}
#endif
	}

	_instance = instance;

	{
		HRSRC res_info = FindResource(instance, MAKEINTRESOURCE(IDD_MAIN), RT_DIALOG);
		HGLOBAL dlg_global;
		DLGTEMPLATE FAR *dlg_rsrc;
#if !defined _WINDOWS || defined _WIN32
		HANDLE heap = GetProcessHeap();
		DLGTEMPLATE *dlg_rsrc_fixed = NULL;
#endif

		/* Might as well. */
		if(!res_info)
			return 1;

		dlg_global = LoadResource(instance, res_info);
		dlg_rsrc = LockResource(dlg_global);

		assert((dlg_rsrc->style & 0xffff0000) != 0xffff0000);

		if(HAS_WINVER_4())
		{
			/*
			Windows 3.x requires maximize with minimize. Also, 3.1 doesn't draw the minimize button correctly; NT 3.51 does,
			FWIW.

			Sometimes Windows lets us edit resource sections in place; sometimes that gives us an access violation. It depends.
			Better to play it safe.
			*/

#if !defined _WINDOWS || defined _WIN32
			DWORD dlg_size = SizeofResource(instance, res_info);
			dlg_rsrc_fixed = HeapAlloc(heap, 0, dlg_size);
			if(dlg_rsrc_fixed)
			{
				CopyMemory(dlg_rsrc_fixed, dlg_rsrc, dlg_size);
				dlg_rsrc = dlg_rsrc_fixed; /* UnlockResource is a no-op on Win32. */
			}
#else
			LPDLGTEMPLATE dlg_rsrc_fixed = dlg_rsrc;
#endif
			if(dlg_rsrc_fixed)
				dlg_rsrc_fixed->style |= WS_MINIMIZEBOX;
		}

		{
			/* 16-bit: CreateDialogIndirect needs a pointer; DialogBoxIndirect needs an HGLOBAL. */
			MSG msg;
			HACCEL accel = LoadAccelerators(instance, MAKEINTRESOURCE(IDR_ACCELERATOR));
			HWND dlg = CreateDialogIndirectParam(instance, dlg_rsrc, NULL, _main_dialog_proc, (SIZE_T)&self);
			ShowWindow(dlg, SW_SHOW);

			while(GetMessage(&msg, NULL, 0, 0)) /* MSDN says this is wrong. */
			{
				if(!TranslateAccelerator(dlg, accel, &msg))
				{
					if(!IsDialogMessage(dlg, &msg))
					{
						TranslateMessage(&msg);
						DispatchMessage(&msg);
					}
				}
			}

			exit_code = msg.wParam;

			DestroyAcceleratorTable(accel);
		}

		(void)UnlockResource(dlg_global);
		FreeResource(dlg_global);

#if !defined _WINDOWS || defined _WIN32
		if (dlg_rsrc_fixed)
			HeapFree(heap, 0, dlg_rsrc_fixed);
#endif
	}

#if OMIT_CRT
	ExitProcess(exit_code);
#endif
	return exit_code;
}
