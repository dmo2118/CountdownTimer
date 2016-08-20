Countdown Timer [![Build status](https://ci.appveyor.com/api/projects/status/gl99ww7kogeija31?svg=true)](https://ci.appveyor.com/project/dmo2118/countdowntimer)
===============

It's an egg timer. Set the time, optionally give it a program to run, and it will count the seconds down until it's time to do
whatever.

Requires [Windows 3.1](https://support.microsoft.com/en-us/kb/83245) or [later](https://www.microsoft.com/en-us/windows), or
[compatible](https://www.winehq.org/).
Works with 32- and 64-bit Windows.

Windows RT, Windows Phone, and Windows Embedded are not supported.

Downloads
---------

Two builds are available for [download](https://github.com/dmo2118/CountdownTimer/releases):

* `wait.exe`: 32-bit Unicode, for currently supported (NT-based) versions of Windows.
* `wait16.exe`: 16-bit, for Windows 3.1, Windows NT 3.x, and Windows 9x.

Building from source
--------------------

### Windows command prompt

	configure
	make

### POSIX: MSYS, Cygwin, Linux, and OS X

	./configure
	make

For both platforms, type `configure --help` for more options.

Not all builds are exactly equivalent. In particular, MSYS/MinGW builds are smaller and work on much older systems than builds
produced by other toolchains.

### Alternate makefile-only builds

Several makefiles are included for specific build toolchains. Their names are in the form of `{compiler name}.mak`. Each of
these optionally takes the following parameters:

Parameter     | Description
--------------|---------------------------------------------------------------
host=*{host}* | Same as the --host=*{host}* parameter in the configure script.
UNICODE=1     | Enables a Unicode build.

For example, `nmake /f cl.mak UNICODE=1` creates a Unicode build using Microsoft Visual Studio.

License
-------

Countdown Timer is distributed under the terms of the
[ISC License](https://www.isc.org/downloads/software-support-policy/isc-license/);
see [`LICENSE.md`](LICENSE.md), or press F1 in the dialog.
