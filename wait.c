#include <windows.h>
#include <ctype.h>
#include <tchar.h>
#include "common.h"
#include "resource.h"

VOID SetOldTime(HWND hDlg, LPCTSTR lpTime, DWORD dwStart, DWORD dwEnd)
{
	MessageBeep(MB_OK);
	SetDlgItemText(hDlg, IDC_TIME, lpTime);
	SendDlgItemMessage(hDlg, IDC_TIME, EM_SETSEL, dwStart, dwEnd);
}

VOID StartTimer(HWND hDlg, UINT_PTR *lpID)
{
	if(!*lpID)
	{
		*lpID = SetTimer(hDlg, 1, 1000, NULL);
		if(!*lpID)
			return;

		CheckDlgButton(hDlg, IDC_START, BST_CHECKED);
		SetDlgItemText(hDlg, IDC_START, TEXT("Stop"));
	}
}

VOID StopTimer(HWND hDlg, UINT_PTR *lpID)
{
	if(*lpID)
	{
		CheckDlgButton(hDlg, IDC_START, BST_UNCHECKED);
		SetDlgItemText(hDlg, IDC_START, TEXT("Start"));
		KillTimer(hDlg, *lpID);
		*lpID = 0;
	}
}

VOID ShowTime(HWND hDlg, LPTSTR lpTime, DWORD dwSeconds)
{
	if(dwSeconds < 60)
		wsprintf(lpTime, TEXT(":%.2d"), dwSeconds);
	else if(dwSeconds < 3600)
		wsprintf(lpTime, TEXT("%d:%.2d"), dwSeconds / 60, dwSeconds % 60);
	else
		wsprintf(lpTime, TEXT("%d:%.2d:%.2d"), dwSeconds / 3600, (dwSeconds / 60) % 60, dwSeconds % 60);

	SetDlgItemText(hDlg, IDC_TIME, lpTime);
}

BOOL ValidTime(LPTSTR lpTime)
{
	DWORD dwDig = 0, dwField = 0;

	while(*lpTime)
	{
		if(isdigit(*lpTime))
		{
			if(dwField && dwDig >= 2)
				return FALSE;

			dwDig++;
		}
		else if(*lpTime == ':')
		{
			if(!dwDig && dwField || dwField == 2)
				return FALSE;

			dwDig = 0;
			dwField++;
		}
		else
		{
			return FALSE;
		}
		lpTime++;
	}

	return TRUE;
}

VOID RunProg(HWND hDlg)
{
	TCHAR cCmdLine[1024];
	LPTSTR lpParam, lpFile;

	if(IsDlgButtonChecked(hDlg, IDC_BEEP) != BST_CHECKED)
	{
		GetDlgItemText(hDlg, IDC_COMMAND, cCmdLine, arraysize(cCmdLine));
		lpFile = cCmdLine;

		while(isspace(*lpFile))
			lpFile++;

		if(*lpFile == '"')
		{
			lpFile++;
			lpParam = _tcschr(lpFile, '"');

			if(lpParam)
			{
				*lpParam = 0;
				lpParam++;
			}
		}
		else
		{
			lpParam = lpFile;

			while(*lpParam && !isspace(*lpParam))
				lpParam++;

			if(*lpParam)
			{
				*lpParam = 0;
				lpParam++;
			}
		}

		ShellExecute(NULL, NULL, lpFile, lpParam, NULL, SW_SHOWNORMAL);
	}
	else
	{
		MessageBox(hDlg, TEXT("Time Elapsed."), TEXT("Countdown Timer"), MB_OK | MB_ICONINFORMATION);
	}
}

DWORD dwSeconds = 0;
TCHAR cTime[10] = TEXT("");
UINT_PTR uTimerID = 0;

INT_PTR CALLBACK DialogProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	LPTSTR lpPtr;
	TCHAR cNewTime[10];
	DWORD dwStart, dwEnd;
	DWORD dwDig, dwField;

	switch(uMsg)
	{
	case WM_INITDIALOG:
		SendDlgItemMessage(hDlg, IDC_TIME, EM_LIMITTEXT, 9, 0);
		break;
	case WM_CLOSE:
		EndDialog(hDlg, 0);
		break;
	case WM_TIMER:
		if(dwSeconds)
		{
			dwSeconds--;
			ShowTime(hDlg, cTime, dwSeconds);
		}
		else
		{
			StopTimer(hDlg, &uTimerID);
			RunProg(hDlg);
		}
		break;
	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDC_BEEP:
			EnableWindow(GetDlgItem(hDlg, IDC_COMMAND), IsDlgButtonChecked(hDlg, IDC_BEEP) != BST_CHECKED);
			break;
		case IDC_TIME:
			switch(HIWORD(wParam))
			{
			case EN_UPDATE:
				GetDlgItemText(hDlg, IDC_TIME, cNewTime, arraysize(cNewTime));
				SendDlgItemMessage(hDlg, IDC_TIME, EM_GETSEL, (WPARAM)&dwStart, (LPARAM)&dwEnd);
/*				dwStart--;
				dwEnd--; */

				if(ValidTime(cNewTime))
				{
					lstrcpy(cTime, cNewTime);
				}
				else
				{
					MessageBeep(MB_OK);
					SetOldTime(hDlg, cTime, dwStart, dwEnd);
					SendDlgItemMessage(hDlg, IDC_TIME, EM_SETSEL, dwStart, dwEnd);
				}

				break;
			case EN_SETFOCUS:
				StopTimer(hDlg, &uTimerID);
				break;
			case EN_KILLFOCUS:
				GetDlgItemText(hDlg, IDC_TIME, cNewTime, arraysize(cNewTime));

				lpPtr = cNewTime;
				dwSeconds = 0;
				dwField = 0;
				dwDig = 0;

				while(*lpPtr)
				{
					if(isdigit(*lpPtr))
					{
						dwDig = dwDig * 10 + *lpPtr - '0';
					}
					else if(*lpPtr == ':')
					{
						if(dwField < 3)
						{
							dwSeconds = dwSeconds * 60 + dwDig;
							dwDig = 0;
							dwField++;
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
					lpPtr++;
				}

				dwSeconds = dwSeconds * 60 + dwDig;

				ShowTime(hDlg, cTime, dwSeconds);
				break;
			}

			break;

		case IDC_START:
			if(!uTimerID)
				StartTimer(hDlg, &uTimerID);
			else
				StopTimer(hDlg, &uTimerID);
			break;

		case IDC_RUNNOW:
			RunProg(hDlg);
			break;

		case IDC_CLOSE:
			EndDialog(hDlg, 0);
			break;
		}
		break;
	}
	return FALSE;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	return (int)DialogBox(hInstance, MAKEINTRESOURCE(IDD_MAIN), NULL, DialogProc);
}
