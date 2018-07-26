#ifndef WAIT_RC_H
#define WAIT_RC_H

#define WINVER 0x0400 /* For old w32api. */

#include "resource.h"
#include "wait.h"

#if !WAIT_WIN16
#	include "winresrc.h"
#	define DEFAULT_FONT "MS Shell Dlg"
#else
#	include "windows.h"
#	define DS_3DLOOK    0x0004L
#	define IDHELP       9
#	define DEFAULT_FONT "Helv" /* For Windows 3.0 and earlier. Aliased to MS Sans Serif on 3.1. */
#endif

#define IDC_STATIC -1

/*
16-color icon from Windows 95 PowerToys: Round Clock. Used without permission.
256+ color icon uses art assets from Windows 7: timedate.cpl. Used without permission.

To make a 32-bit subimage:
- Load icon-32bit.xcfgz
- Remove non-visible layers
- On the 'Hands' layer: Select All, Clear
- For each desired size:
  - Duplicate the image
  - Resample to desired size. Use Cubic interpolation.
  - Do Alpha to Selection on 'Red Halo', then invert selection.
  - On '-135 deg back' layer, Unsharp Mask: Radius = 1.5, Amount = 0.5, Threshold = 0
  - On 'Hands' layer, stroke paths in RGB(0, 0, 128), according to size:
    | Size       | HH:MM  | SS      |
    | 16px       | 0.5 px | 0.25 px |
    | 32px, 48px | 1   px | 0.5  px |
    | 64px       | 1.5 px | 0.75 px |

To make an 8-bit subimage:
- Continue from a 32-bit subimage
- Copy and paste the image onto itself repeatedly until the alpha channel is either 0 or 1.
- Copy and paste from the 32-bit subimage, then do Alpha to Selection.
- Delete the 32-bit layer, then Add Layer Mask, initialized to Selection.
- Clear the selection, then on the layer mask, do Threshold: (240-255). (16px: do 96-255 instead, 32px: 212-255.)
- Convert the image to indexed color, with a 236 color palette.
*/
IDI_MAIN ICON "main.ico"

#if defined _WIN32 || !defined _WINDOWS
LANGUAGE LANG_ENGLISH, SUBLANG_ENGLISH_US
CREATEPROCESS_MANIFEST_RESOURCE_ID RT_MANIFEST "wait.exe.manifest"
#endif

#endif
