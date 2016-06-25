#include "resource.h"

#include "common.h"

#include <windows.h>
#include <ctype.h>
#include <tchar.h>

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
			if(!dig && field || field == 2)
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

static void _run_prog(HWND dlg)
{
	TCHAR cmd_line[1024];
	LPTSTR param, file;

	if(IsDlgButtonChecked(dlg, IDC_BEEP) != BST_CHECKED)
	{
		GetDlgItemText(dlg, IDC_COMMAND, cmd_line, arraysize(cmd_line));
		file = cmd_line;

		while(isspace(*file))
			file++;

		if(*file == '"')
		{
			file++;
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

		ShellExecute(NULL, NULL, file, param, NULL, SW_SHOWNORMAL);
	}
	else
	{
		MessageBox(dlg, TEXT("Time Elapsed."), TEXT("Countdown Timer"), MB_OK | MB_ICONINFORMATION);
	}
}

#define MAX_TIME 10

static DWORD _seconds = 0;
static TCHAR _time[MAX_TIME] = TEXT("");
static UINT_PTR _timer_id = 0;

static INT_PTR CALLBACK _dialog_proc(HWND dlg, UINT msg, WPARAM wparam, LPARAM lparam)
{
	switch(msg)
	{
	case WM_INITDIALOG:
		SendDlgItemMessage(dlg, IDC_TIME, EM_LIMITTEXT, MAX_TIME - 1, 0);
		break;
	case WM_CLOSE:
		EndDialog(dlg, 0);
		break;
	case WM_TIMER:
		if(_seconds)
		{
			_seconds--;
			_show_time(dlg, _time, _seconds);
		}
		else
		{
			_stop_timer(dlg, &_timer_id);
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
						lstrcpy(_time, new_time);
					}
					else
					{
						/* Set old time. */
						MessageBeep(MB_OK);
						SetDlgItemText(dlg, IDC_TIME, _time);
						SendDlgItemMessage(dlg, IDC_TIME, EM_SETSEL, start, end);
					}
				}

				break;
			case EN_SETFOCUS:
				_stop_timer(dlg, &_timer_id);
				break;
			case EN_KILLFOCUS:
				{
					TCHAR new_time[MAX_TIME];
					LPTSTR ptr = new_time;
					DWORD dig = 0, field = 0;

					GetDlgItemText(dlg, IDC_TIME, new_time, arraysize(new_time));

					_seconds = 0;

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
								_seconds = _seconds * 60 + dig;
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

					_seconds = _seconds * 60 + dig;

					_show_time(dlg, _time, _seconds);
				}

				break;
			}

			break;

		case IDC_START:
			if(!_timer_id)
			{
				/* Start timer. */
				_timer_id = SetTimer(dlg, 1, 1000, NULL);
				if(_timer_id)
				{
					CheckDlgButton(dlg, IDC_START, BST_CHECKED);
					SetDlgItemText(dlg, IDC_START, TEXT("Stop"));
				}
			}
			else
			{
				_stop_timer(dlg, &_timer_id);
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

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev_instance, LPSTR cmd_line, int show_cmd)
{
	return (int)DialogBox(instance, MAKEINTRESOURCE(IDD_MAIN), NULL, _dialog_proc);
}
