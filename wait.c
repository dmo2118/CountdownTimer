/*
TODO:
- OpenWatcom
- Win16
- HXDOS
- Test with Winelib, not just Cygwin.
- MinGW-w64 64-bit
- MSVC 64-bit
- Async system()
- Restrict countdown time edit control numerically.
- About button with copyright.
- ShellExecuteEx
*/

#include "resource.h"

#include "common.h"

#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <process.h>
#include <windows.h>

#if defined __MINGW32__
#	define HAVE_WINDOWS_3 1
#	define OMIT_CRT 1
#endif

#ifdef _WIN32
#	include <tchar.h>
#else
#	define _tcschr strchr
#endif

#if HAVE_WINDOWS_3
DWORD _win_ver;
#endif

static const TCHAR _title[] = TEXT("Countdown Timer");

static void _stop_timer(HWND dlg, UINT_PTR *id)
{
	if(*id)
	{
		CheckDlgButton(dlg, IDC_START, BST_UNCHECKED);
		SetDlgItemText(dlg, IDC_START, TEXT("Start"));
		KillTimer(dlg, *id);
		*id = 0;
	}
}

static void _show_time(HWND dlg, LPTSTR time, DWORD seconds)
{
	if(seconds < 60)
		wsprintf(time, TEXT(":%.2d"), seconds);
	else if(seconds < 3600)
		wsprintf(time, TEXT("%d:%.2d"), seconds / 60, seconds % 60);
	else
		wsprintf(time, TEXT("%d:%.2d:%.2d"), seconds / 3600, (seconds / 60) % 60, seconds % 60);

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
	MessageBox(dlg, TEXT("Time elapsed."), _title, MB_OK | MB_ICONINFORMATION);
}

static void _run_prog(HWND dlg)
{
	if(IsDlgButtonChecked(dlg, IDC_BEEP) != BST_CHECKED)
	{
		TCHAR cmd_line[1024];
		LPTSTR param, file = cmd_line;
		UINT_PTR result;

		GetDlgItemText(dlg, IDC_COMMAND, cmd_line, arraysize(cmd_line));

#ifndef _WIN32
		if(system(cmd_line) == -1)
			MessageBox(dlg, "Error issuing a command.", _title, MB_ICONERROR | MB_OK);
#else
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
			return;
		}

		result = (UINT_PTR)ShellExecute(dlg, NULL, file, param, NULL, SW_SHOWNORMAL);
		if(result <= 32)
		{
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

			TCHAR *message;

			if(result >= 26)
				result = errors26[result - 26];

			if(FormatMessage(
				FORMAT_MESSAGE_ALLOCATE_BUFFER |
					FORMAT_MESSAGE_IGNORE_INSERTS |
					FORMAT_MESSAGE_FROM_SYSTEM |
					FORMAT_MESSAGE_MAX_WIDTH_MASK,
				NULL,
				result,
				0,
				(LPTSTR)&message,
				0,
				NULL))
			{
				MessageBox(dlg, message, _title, MB_ICONERROR | MB_OK);
				LocalFree(message);
			}
			else
			{
				TCHAR text[12];
				wsprintfA(text, "%d", result);
				MessageBox(dlg, text, _title, MB_ICONERROR | MB_OK);
			}
		}
#endif
	}
	else
	{
		_just_beep(dlg);
	}
}

#define MAX_TIME 10

struct _dialog
{
	DWORD seconds;
	TCHAR time[MAX_TIME];
	UINT_PTR timer_id;
};

static const struct _dialog _dialog_init = {0, TEXT(""), 0};

static INT_PTR CALLBACK _dialog_proc(HWND dlg, UINT msg, WPARAM wparam, LPARAM lparam)
{
	struct _dialog *self = (struct _dialog *)GetWindowLongPtr(dlg, DWLP_USER);

	switch(msg)
	{
	case WM_INITDIALOG:
		self = (struct _dialog *)lparam;
		SetWindowLongPtr(dlg, DWLP_USER, (LONG_PTR)self);

		SendDlgItemMessage(dlg, IDC_TIME, EM_LIMITTEXT, MAX_TIME - 1, 0);
#if HAVE_WINDOWS_3
		if(LOBYTE(_win_ver) < 4)
		{
			HWND start = GetDlgItem(dlg, IDC_START);
			SetWindowLongPtr(start, GWL_STYLE, GetWindowLongPtr(start, GWL_STYLE) & ~(LONG_PTR)(BS_AUTOCHECKBOX | BS_PUSHLIKE));
		}
#endif
		break;
	case WM_CLOSE:
		EndDialog(dlg, 0);
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
	case WM_COMMAND:
		switch(LOWORD(wparam))
		{
		case IDC_BEEP:
			EnableWindow(GetDlgItem(dlg, IDC_COMMAND), IsDlgButtonChecked(dlg, IDC_BEEP) != BST_CHECKED);
			break;
		case IDC_TIME:
			switch(HIWORD(wparam))
			{
			case EN_UPDATE:
				{
					TCHAR new_time[MAX_TIME];
					DWORD start, end;

					GetDlgItemText(dlg, IDC_TIME, new_time, arraysize(new_time));
					SendDlgItemMessage(dlg, IDC_TIME, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
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
						SendDlgItemMessage(dlg, IDC_TIME, EM_SETSEL, start, end);
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
					SetDlgItemText(dlg, IDC_START, TEXT("Stop"));
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
			EndDialog(dlg, 0);
			break;
		}
		break;
	}
	return FALSE;
}

#if OMIT_CRT
int entry_point()
#elif !defined _WIN32
int main()
#else
int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR cmd_line, int show_cmd)
#endif
{
#if OMIT_CRT || !defined _WIN32
	/* The linker-defined symbol __ImageBase is the same as hInstance under Windows NT and Windows 95, but not Win32s. */
	HINSTANCE instance = GetModuleHandle(NULL);
#endif

	int exit_code;
	struct _dialog self = _dialog_init;

#if HAVE_WINDOWS_3
	_win_ver = GetVersion();

	{
		HRSRC res_info = FindResource(instance, MAKEINTRESOURCE(IDD_MAIN), RT_DIALOG);
		// if(!res_info)
		//	return -1;

		HGLOBAL dlg_global = LoadResource(instance, res_info);
		DLGTEMPLATE *dlg_rsrc = LockResource(dlg_global);

		assert(dlg_rsrc->style & 0xffff0000 != 0xffff0000);

		if(LOBYTE(_win_ver) < 4)
		{
			// Windows 3.x requires maximize with minimize. Also, Win32s doesn't draw the minimize button correctly; NT 3.51
			// does, FWIW.
			dlg_rsrc->style &= ~(DWORD)WS_MINIMIZEBOX;
		}

		exit_code = (int)DialogBoxIndirectParam(instance, dlg_rsrc, NULL, _dialog_proc, (LPARAM)&self);
	}
#else
	exit_code = (int)DialogBoxParam(instance, MAKEINTRESOURCE(IDD_MAIN), NULL, _dialog_proc, (LPARAM)&self);
#endif

#if OMIT_CRT
	ExitProcess(exit_code);
#endif
	return exit_code;
}
