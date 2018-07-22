#include "dialog.h"

#include "wait.h"

#include <assert.h>
#include <stddef.h>

#if USE_DIALOG

typedef struct
{
	DWORD helpID;
	DWORD exStyle;
	DWORD style;
	short x;
	short y;
	short cx;
	short cy;
	DWORD id;
} DLGITEMTEMPLATEEX0;

#define DLG_TYPEFACE L"MS Shell Dlg"

struct dlg_template_suffix
{
    WORD pointsize;
    WORD weight;
    BYTE italic;
    BYTE charset;
    WCHAR typeface[arraysize(DLG_TYPEFACE)];
};

static size_t _size_sz(const WCHAR *s)
{
	return wcslen(s) + 1;
}

static size_t _size_sz_or_ord(const WORD *s)
{
	return *s == 0xffff ? 2 : _size_sz(s);
}

static void _copy_words(WORD **dst, const WORD **src, size_t size)
{
	memcpy(*dst, *src, size * sizeof(WORD));
	*src += size;
	*dst += size;
}

static ptrdiff_t _round_dword(ptrdiff_t p)
{
	return (p + sizeof(DWORD) - 1) & ~(sizeof(DWORD) - 1);
}

dialog dialog_create(LPTSTR template_name)
{
	HRSRC src_res_info;
	HGLOBAL src_global;
	DLGTEMPLATE FAR *src_rsrc;

	DWORD_PTR dst_size;
	DLGTEMPLATEEX0 *dst_rsrc;

	src_res_info = FindResource(global_instance, template_name, RT_DIALOG);

	/* Might as well. */
	if(!src_res_info)
		return (dialog)0;

	src_global = LoadResource(global_instance, src_res_info);
	src_rsrc = LockResource(src_global);

	assert((src_rsrc->style & 0xffff0000) != 0xffff0000);
	assert(!((ptrdiff_t)src_rsrc & (sizeof(DWORD) - 1)));

	if(!dialog_using_ex())
	{
		dst_size = SizeofResource(global_instance, src_res_info);
	}
	else
	{
		const WORD *src = (const void *)(src_rsrc + 1);
		size_t menu_size, class_size, title_size;
		WORD cdit = src_rsrc->cdit;

		menu_size = _size_sz_or_ord(src);
		src += menu_size;

		class_size = _size_sz_or_ord(src);
		src += class_size;

		title_size = _size_sz(src);
		src += title_size;

		if(src_rsrc->style & DS_SETFONT)
		{
			src += 1;
			src += _size_sz(src);
		}

		dst_size =
			sizeof(DLGTEMPLATEEX0) + (menu_size + class_size + title_size) * sizeof(WORD) + sizeof(struct dlg_template_suffix);

		while(cdit)
		{
			size_t create_size;

			src = (const void *)((const DLGITEMTEMPLATE *)_round_dword((ptrdiff_t)src) + 1);
			dst_size = _round_dword(dst_size);

			class_size = _size_sz_or_ord(src);
			src += class_size;

			title_size = _size_sz(src);
			src += title_size;

			if(!*src)
				create_size = 0;
			else
				create_size = *src - sizeof(WORD);
			++src;

			src = (void *)_round_dword((ptrdiff_t)src + create_size);
			dst_size += sizeof(DLGITEMTEMPLATEEX0) + (class_size + title_size) * sizeof(WORD) + sizeof(WORD) + create_size;

			--cdit;
		}

		_round_dword(dst_size);
	}

	dst_rsrc = HeapAlloc(global_heap, 0, dst_size);

	if(dst_rsrc)
	{
		if(!dialog_using_ex())
		{
			CopyMemory(dst_rsrc, src_rsrc, dst_size);
		}
		else
		{
			static const struct dlg_template_suffix suffix0 =
			{
				8,
				0,
				FALSE,
				DEFAULT_CHARSET,
				DLG_TYPEFACE
			};

			const WORD *src;
			struct dlg_template_suffix *dst_suffix;
			WORD *dst;
			size_t size;
			WORD cdit = src_rsrc->cdit;

			dst_rsrc->dlgVer = 1;
			dst_rsrc->signature = 0xffff;
			dst_rsrc->helpID = 0;
			dst_rsrc->exStyle = src_rsrc->dwExtendedStyle;
			dst_rsrc->style = src_rsrc->style | DS_SHELLFONT;
			dst_rsrc->cDlgItems = cdit;
			dst_rsrc->x = src_rsrc->x;
			dst_rsrc->y = src_rsrc->y;
			dst_rsrc->cx = src_rsrc->cx;
			dst_rsrc->cy = src_rsrc->cy;

			src = (const void *)(src_rsrc + 1);
			dst = (void *)(dst_rsrc + 1);

			_copy_words(&dst, &src, _size_sz_or_ord(src)); /* menu */
			_copy_words(&dst, &src, _size_sz_or_ord(src)); /* windowClass */
			_copy_words(&dst, &src, _size_sz(src)); /* title */

			dst_suffix = (struct dlg_template_suffix *)dst;
			*dst_suffix = suffix0;

			if(src_rsrc->style & DS_SETFONT)
			{
				dst_suffix->pointsize = *src;
				++src;
				src += _size_sz(src);
			}

			dst = (WORD *)(dst_suffix + 1);

			while(cdit)
			{
				DLGITEMTEMPLATEEX0 *dst_item;
				const DLGITEMTEMPLATE *src_item;

				/* DLGITEMTEMPLATEs start on a DWORD boundary; they don't necessarily end on one. */
				src = (const void *)_round_dword((ptrdiff_t)src);
				dst = (void *)_round_dword((ptrdiff_t)dst);

				src_item = (const DLGITEMTEMPLATE *)src;
				dst_item = (DLGITEMTEMPLATEEX0 *)dst;

				dst_item->helpID = 0;
				dst_item->exStyle = src_item->dwExtendedStyle;
				dst_item->style = src_item->style;
				dst_item->x = src_item->x;
				dst_item->y = src_item->y;
				dst_item->cx = src_item->cx;
				dst_item->cy = src_item->cy;
				dst_item->id = (INT)(SHORT)src_item->id;

				src = (const WORD *)(src_item + 1);
				dst = (WORD *)(dst_item + 1);

				_copy_words(&dst, &src, _size_sz_or_ord(src)); /* windowClass */
				_copy_words(&dst, &src, _size_sz_or_ord(src)); /* title */

				if(!*src)
					size = 0;
				else
					size = *src - sizeof(WORD);
				++src;
				*dst = size;
				++dst;
				memcpy(dst, src, size);

				src = (const void *)((const BYTE *)src + size);
				dst = (void *)((BYTE *)dst + size);

				--cdit;
			}

			assert((DWORD_PTR)((BYTE *)src - (BYTE *)src_rsrc) <= SizeofResource(global_instance, src_res_info));
			assert((DWORD_PTR)((BYTE *)dst - (BYTE *)dst_rsrc) <= dst_size);
		}
	}

	(void)UnlockResource(src_global);
	FreeResource(src_global);

	return dst_rsrc;
}

#endif
