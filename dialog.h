#ifndef DIALOG_H
#define DIALOG_H

/*
Windows 3 doesn't support DIALOGEX.
Windows 5+ requires DIALOGEX + DS_SHELLFONT for Tahoma.
This fixes that for both.

16-bit Windows uses DialogBoxHeader instead of DLGTEMPLATE, so none of this applies there.
*/

#include "wait.h" /* For WAIT_WIN16 and HAS_WINVER_4(). */

#include <windows.h>

#if !WAIT_WIN16
#	define USE_DIALOG 1
#endif

#if !USE_DIALOG /* Usually Windows 3 or lower. Use a dialog ID. */

typedef LPCTSTR dialog; /* template_name */

#	define dialog_create(template_name) (template_name)
#	define dialog_create_dialog(self, parent, dialog_func) (CreateDialog(global_instance, (self), (parent), (dialog_func)))
#	define dialog_box(self, parent, dialog_func) (DialogBox(global_instance, (self), (parent), (dialog_func)))
#	define dialog_destroy(self)

#else /* Usually Windows 4 or higher. Use *Dialog*Indirect. */

#	if defined _MSC_VER || defined __WATCOMC__
#		define DIALOG_PRAGMA_PACK 1
#	endif

#	if DIALOG_PRAGMA_PACK
#		pragma pack(push, 2)
#	endif

typedef struct
#	if !DIALOG_PRAGMA_PACK
	__attribute__((packed))
#	endif
{
	WORD dlgVer;
	WORD signature;
	DWORD helpID;
	DWORD exStyle;
	DWORD style;
	WORD cDlgItems;
	short x;
	short y;
	short cx;
	short cy;
} DLGTEMPLATEEX0;

#	if DIALOG_PRAGMA_PACK
#		pragma pack(pop)
#	endif

typedef void *dialog;

dialog dialog_create(LPTSTR template_name);
#	define dialog_create_dialog(self, parent, dialog_func) (CreateDialogIndirect(global_instance, (self), (parent), (dialog_func)))
#	define dialog_box(self, parent, dialog_func) (DialogBoxIndirect(global_instance, (self), (parent), (dialog_func)))
#	define dialog_destroy(self) (HeapFree(global_heap, 0, self))

#	define dialog_using_ex() HAS_WINVER_4()

#endif

#endif
