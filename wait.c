/*
TODO:
- Escape to close
- Just beep -> also beep, maybe?

- Valgrind me!
- Winelib testing via Travis CI.
- OpenWatcom on Linux
- MinGW on macOS
- Async system()
- Windows 3.0, 2.0, 1.0.
- HXDOS (again)
- Use a SysLink in the about dialog, when possible. Use LinkWindow_RegisterClass on 2000; manifest takes care of things on XP.
- Wine and Cygwin maybe need an option to spawn a console...or, you know, (x-terminal-emulator|xterm) -e works too.
- Recheck STACKSIZE in .def; maybe 4K is enough now?
- Don't use dialog.c on Winelib.
*/

#define STRICT 1

#if UNICODE
#	define _UNICODE 1
#endif

#include "wait.h"

#include "dialog.h"
#include "resource.h"

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

#if WAIT_WIN16
#	include <shellapi.h>
#	include <ver.h>
#endif

#if defined __WINE__ || defined __CYGWIN__
#	include <errno.h>
#	include <iconv.h>
#	include <locale.h>
#endif

#if WAIT_WIN16
#	define GET_WM_COMMAND_ID(wparam, lparam)    wparam
#	define GET_WM_COMMAND_CMD(wparam, lparam)   HIWORD(lparam)
#else
#	include <windowsx.h>
#endif

BYTE global_win_ver;

HINSTANCE global_instance;

#if !WAIT_WIN16
HANDLE global_heap;
#endif

static TCHAR _title[32];

#if !WAIT_WIN16
BOOL (STDAPICALLTYPE *_ShellExecuteEx)(SHELLEXECUTEINFO *);
#endif

#define LOAD_STRING(id, buffer) (LoadString(global_instance, id, (buffer), arraysize(buffer)))

static void _stop_timer(HWND dlg, UINT_PTR *id)
{
	if(*id)
	{
		TCHAR start[32];
		LOAD_STRING(IDS_START, start);
		CheckDlgButton(dlg, IDC_START, BST_UNCHECKED);
		SetDlgItemText(dlg, IDC_START, start);
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
	TCHAR time_elapsed[32];

	/* Windows 3.1 doesn't make sound with message boxes. */
	if(!HAS_WINVER_4())
		MessageBeep(MB_OK);
	LOAD_STRING(IDS_TIME_ELAPSED, time_elapsed);
	MessageBox(dlg, time_elapsed, _title, MB_OK | MB_ICONINFORMATION);
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
#	if WAIT_WIN16

	char error[256];
	static const BYTE error_map[] =
	{
		8, 1, 2, 3, 4, 3, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 29, 29, 29, 31, 32
	};

	assert(result < arraysize(error_map));
	if(!LOAD_STRING(IDS_ERRORMSG + error_map[result], error))
	{
		char unknown_msg[64];
		LOAD_STRING(IDS_UNKNOWNMSG, unknown_msg);
		wsprintf(error, unknown_msg, result);
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
		TCHAR text[64], unknown_msg[64];
		LOAD_STRING(IDS_UNKNOWNMSG, unknown_msg);
		wsprintf(text, unknown_msg, result);
		MessageBox(dlg, text, _title, MB_ICONERROR | MB_OK);
	}

#	endif
}

#if !defined __CYGWIN__ && !defined __WINE__

static void _run_prog_shell_execute(HWND dlg, const TCHAR *file, const TCHAR *param)
{
	UINT_PTR result = (UINT_PTR)ShellExecute(dlg, NULL, file, param, NULL, SW_SHOWNORMAL);
	if(result <= 32)
	{
#if !WAIT_WIN16
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
#endif

		_error_message(dlg, (UINT)result);
	}
}

static BOOL _check_file(const TCHAR *file)
{
#if WAIT_WIN16
	OFSTRUCT of;
	int hnd;

	of.cBytes = sizeof(of);
	hnd = OpenFile(file, &of, OF_EXIST);
	if(hnd != HFILE_ERROR)
		return TRUE;

	if(of.nErrCode != ERROR_FILE_NOT_FOUND)
		return TRUE;

	return FALSE;
#else
	HANDLE hnd = CreateFile(file, 0, 0, NULL, OPEN_EXISTING, 0, NULL);
	if(hnd != INVALID_HANDLE_VALUE)
	{
		CloseHandle(hnd);
		return TRUE;
	}
	else
	{
		DWORD last_error = GetLastError();
		if(last_error != ERROR_FILE_NOT_FOUND)
			return TRUE;
	}

	return FALSE;
#endif
}

static TCHAR *_find_exe_with_spaces(TCHAR *file)
{
	/*
	ShellExecute needs a separate file and parameters. In the absense of quotes, make an educated guess. This works a bit like
	CreateProcess does when lpApplicationName is NULL.
	*/

	TCHAR *param = file;
	BOOL has_slash = FALSE;

	while(*param)
	{
		has_slash |= *param == '/' || *param == '\\';

		if(isspace(*param))
		{
			TCHAR old_chars[5];
			BOOL got_file;

			if(!has_slash)
			{
				/* pbrush File.bmp */
				*param = 0;
				++param;
				break;
			}

			memcpy(old_chars, param, sizeof(old_chars));
			*param = 0;

			if(_check_file(file))
			{
				++param;
				break;
			}

			memcpy(param, TEXT(".exe"), sizeof(old_chars));
			got_file = _check_file(file);
			memcpy(param, old_chars, sizeof(old_chars));

			if(got_file)
			{
				*param = 0;
				++param;
				break;
			}
		}

		++param;
	}

	return param;
}

#endif

static void _run_prog(HWND dlg)
{
#if !WAIT_WIN16
	SetForegroundWindow(dlg); /* For flashing taskbar buttons. Not necessary before Windows 98. */
#endif

	if(IsDlgButtonChecked(dlg, IDC_BEEP) != BST_CHECKED)
	{
		HWND command = GetDlgItem(dlg, IDC_COMMAND);
		UINT cmd_line_size = GetWindowTextLength(command) + 1;
		TCHAR *cmd_line = (TCHAR *)LOCAL_ALLOC_PTR(cmd_line_size * sizeof(TCHAR) + 3); /* +3 for _find_exe_with_spaces. */

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
						assert((!out_left && !out) || out);
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
			{
				TCHAR command_error[64];
				LOAD_STRING(IDS_COMMAND_ERROR, command_error);
				MessageBox(dlg, command_error, _title, MB_ICONERROR | MB_OK);
			}

			free(cmd_line_ptr);
		}

#else

		{
			TCHAR *param;
			TCHAR *file = cmd_line;
			while(isspace(*file))
				++file;

			if(*file != '"')
			{
				param = _find_exe_with_spaces(file);
			}
			else
			{
				++file;
				param = file;

				for(;;)
				{
					if(!param[0])
					{
						file = cmd_line;
						param = _find_exe_with_spaces(file);
						break;
					}

					if(param[0] == '"' && (!param[1] || isspace(param[1])))
					{
						param[0] = 0;
						++param;
						break;
					}

					++param;
				}
			}

			while(isspace(*param))
				++param;
			if(!*param)
				param = NULL;

			if(!*file)
			{
				_just_beep(dlg);
			}
#if !WAIT_WIN16
			else if(_ShellExecuteEx)
			{
				SHELLEXECUTEINFO exec_info = {sizeof(exec_info)};
				exec_info.hwnd = dlg;
				exec_info.lpFile = file;
				exec_info.lpParameters = param;
				exec_info.nShow = SW_SHOWNORMAL;
				if(!_ShellExecuteEx(&exec_info))
				{
					if(GetLastError() == ERROR_CALL_NOT_IMPLEMENTED)
						_run_prog_shell_execute(dlg, file, param);
				}
			}
#endif
			else
			{
				_run_prog_shell_execute(dlg, file, param);
			}
		}
#endif
		LOCAL_FREE(cmd_line);
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
				TEXT("Copyright (c) 2000-2018, Dave Odell <dmo2118@gmail.com>\n")
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
	dialog about = dialog_create(MAKEINTRESOURCE(IDD_ABOUT));
	if(about)
	{
		dialog_box(about, dlg, _about_dialog_proc);
		dialog_destroy(about);
	}
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

static INT_PTR CALLBACK _main_dialog_proc(HWND dlg, UINT msg, WPARAM wparam, LPARAM lparam)
{
	HLOCAL h_self = (HLOCAL)GetWindowLongPtr(dlg, DWLP_USER);
	struct _dialog *self = LOCAL_LOCK(h_self);

	switch(msg)
	{
	case WM_INITDIALOG:
		{
			static const struct _dialog dialog_init = {0, TEXT(""), 0};
			h_self = LOCAL_ALLOC_HND(sizeof(struct _dialog));
			if(!h_self)
			{
				/* TODO: Is this okay for Windows 1.0? */
				_error_message(dlg, ERROR_NOT_ENOUGH_MEMORY);
				DestroyWindow(dlg);
				PostQuitMessage(1);
				return FALSE;
			}
			self = LOCAL_LOCK(h_self);
			*self = dialog_init;
		}

		SetWindowLongPtr(dlg, DWLP_USER, (SIZE_T)h_self);

		SendDlgItemMessage(dlg, IDC_TIME, EM_LIMITTEXT, MAX_TIME - 1, 0);

		if(HAS_WINVER_4())
		{
			HWND start = GetDlgItem(dlg, IDC_START);
			SetWindowLongPtr(start, GWL_STYLE, GetWindowLongPtr(start, GWL_STYLE) | (BS_AUTOCHECKBOX | BS_PUSHLIKE));
		}

		{
			HMENU sys_menu = GetSystemMenu(dlg, FALSE);
			TCHAR about[32];
			LOAD_STRING(IDS_ABOUT, about);
			AppendMenu(sys_menu, MF_SEPARATOR, 0, NULL);
			AppendMenu(sys_menu, MF_STRING, IDM_ABOUT, about);
		}

		break;

	case WM_DESTROY:
		SetWindowLongPtr(dlg, DWLP_USER, 0);
		LOCAL_UNLOCK(h_self);
		LOCAL_FREE(h_self);
		h_self = NULL;
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

#if WAIT_WIN16
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
#if WAIT_WIN16
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
					TCHAR stop[16];
					LOAD_STRING(IDS_STOP, stop);
					CheckDlgButton(dlg, IDC_START, BST_CHECKED);
					SetDlgItemText(dlg, IDC_START, stop);
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

	LOCAL_UNLOCK(h_self);
	return FALSE;
}

static int _no_lang()
{
#ifdef _WIN32
	MessageBoxA(NULL, "Not for Windows 3.1. Use wait16en.exe instead.", "Countdown Timer", MB_OK | MB_ICONERROR);
#endif
	return EXIT_FAILURE;
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

	INT exit_code;

#if defined __CYGWIN__ || defined __WINE__
	/* iconv() needs this. */
	setlocale(LC_ALL, "");
#endif

	global_instance = instance;

#if !WAIT_WIN16
	global_heap = GetProcessHeap();
#endif

	{
#if defined _WIN32
		DWORD win_ver = GetVersion();
#	ifdef UNICODE
		if(win_ver & 0x80000000)
		{
			CHAR title[32], requires_nt[64];
			if(!LoadStringA(global_instance, IDS_TITLE, title, arraysize(title)))
				return _no_lang();
			LoadStringA(global_instance, IDS_REQUIRES_NT, requires_nt, arraysize(requires_nt));
			MessageBoxA(NULL, requires_nt, title, MB_ICONERROR | MB_OK);
			return EXIT_FAILURE;
		}
#	endif
		global_win_ver = LOBYTE(win_ver);
#elif WAIT_WIN16
		/* Recommended by KB113892. */
		const TCHAR _user_exe[] = TEXT("user.exe");
		DWORD handle;
		DWORD len = GetFileVersionInfoSize(_user_exe, &handle);
		global_win_ver = 3; /* (Cheating) */
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
							global_win_ver = LOBYTE(HIWORD(file_info->dwFileVersionMS));
						};
					}

					GlobalUnlock(hblock);
				}

				GlobalFree(hblock);
			}
		}
#endif
	}

	/* Windows NT 3.1 and Win32s don't support multilingual resources, even though the spec allows it. */
	if(!LOAD_STRING(IDS_TITLE, _title))
		return _no_lang();

#if !WAIT_WIN16
	_ShellExecuteEx = (BOOL (STDAPICALLTYPE *)(SHELLEXECUTEINFO *))GetProcAddress(
		GetModuleHandle(TEXT("shell32")),
		"ShellExecuteEx"
#ifdef UNICODE
		"W"
#else
		"A"
#endif
		);
#endif

	{
		dialog dlg_rsrc = dialog_create(MAKEINTRESOURCE(IDD_MAIN));

		if(!dlg_rsrc)
		{
			_error_message(NULL, ERROR_NOT_ENOUGH_MEMORY);
			return 1;
		}

#if USE_DIALOG
		if(HAS_WINVER_4() && dialog_using_ex())
		{
			DLGTEMPLATEEX0 *dlg_rsrc_fixed = dlg_rsrc;
			assert(dlg_rsrc_fixed->signature == 0xffff);
			assert(dlg_rsrc_fixed->dlgVer == 1);

			/*
			Windows 3.x requires maximize with minimize. Also, 3.1 doesn't draw the minimize button correctly; NT 3.51 does,
			FWIW.

			Sometimes Windows lets us edit resource sections in place; sometimes that gives us an access violation. It depends.
			Better to play it safe.
			*/

			dlg_rsrc_fixed->style |= WS_MINIMIZEBOX;
		}
#endif

		{
			/* 16-bit: CreateDialogIndirect needs a pointer; DialogBoxIndirect needs an HGLOBAL. */
			MSG msg;
			HACCEL accel = LoadAccelerators(instance, MAKEINTRESOURCE(IDR_ACCELERATOR));
			HWND dlg = dialog_create_dialog(dlg_rsrc, NULL, _main_dialog_proc);
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

			exit_code = (INT)msg.wParam;

			DestroyAcceleratorTable(accel);
		}

		dialog_destroy(dlg_rsrc);
	}

#if OMIT_CRT
	ExitProcess(exit_code);
#endif
	return exit_code;
}
