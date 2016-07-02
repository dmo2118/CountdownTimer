@ECHO OFF

REM TODO: Is there a way for DOS to suppress "Bad command or file name"?

REM DOS doesn't do SETLOCAL.
SETLOCAL

IF "%progname%"=="" SET progname=%0

:args
IF "%1"=="" GOTO args_end

IF "%1"=="--help" GOTO help
IF "%1"=="--host" GOTO arg_host
IF "%1"=="CC"     GOTO arg_cc
GOTO arg_bad

:args_end

ECHO checking build system type... batch file

IF "%host%"=="mingw32"            SET host=i686-pc-winnt
IF "%host%"=="mingw64"            SET host=x86_64-pc-winnt
IF "%host%"=="i686-w64-mingw32"   SET host=i686-pc-winnt
IF "%host%"=="x86_64-w64-mingw32" SET host=x86_64-pc-winnt

REM host=native doesn't make sense from DOS, so always detect.
IF "%host%"==""                   GOTO host_detect
:host_done

ECHO checking host system type... %host%

IF "%CC%"=="" GOTO cc_test
:cc_done

ECHO checking for C compiler... %CC%

IF "%MAKE%"=="" GOTO make_test
:make_done
ECHO checking for build command... %MAKE%

ECHO %progname%: generating make.bat
ECHO @%MAKE% %CC%.mak host=%host% %%* > make.bat

GOTO success

:arg_bad
ECHO %progname%: error: unrecognized option: %1
ECHO Try `%progname% --help' for more information
GOTO failure

:help
ECHO.Usage: %progname% [OPTION]... [CC=VALUE]...
ECHO.
ECHO.System types:
ECHO.  --host=HOST   cross-compile to build programs to run on a version of Windows
ECHO.                specified by HOST
ECHO.                  i86-pc-win16: 16-bit Windows
ECHO.                    Requires Windows 3.1.
ECHO.                  i386-pc-winnt or mingw32: 32-bit Windows
ECHO.                    Requires Windows NT 3.51 (or later), Windows 95 (or later),
ECHO.                    Win32s on Windows 3.1, or Wine.
ECHO.                  x86_64-pc-winnt or mingw64: 64-bit Windows
GOTO failure

:arg_host
SHIFT
SET host=%1
SHIFT
GOTO args

:arg_cc
SHIFT
SET CC=%1
SHIFT
GOTO args

:host_detect

REM Microsoft (Visual) C doesn't provide a nice environment variable to tell us what architecture CL targets.

REM Windows NT
IF "%PROCESSOR_ARCHITECTURE%"=="X86" GOTO host_i386
IF "%PROCESSOR_ARCHITECTURE%"=="AMD64" GOTO host_i386

REM Windows 9x
IF NOT "%winbootdir%"=="" GOTO host_i386

REM Probably DOS or Windows NT with an odd architecture.
REM Windows NT used to have an 8086 CPU emulator, so it's OK.
SET host=i86-pc-win16

GOTO host_done

:host_i386
SET host=i386-pc-winnt
GOTO host_done

:cc_test

IF NOT "%host%"=="i86-pc-win16" GOTO cc_wcl386
wcc -q -zs NUL 2>NUL
IF ERRORLEVEL 1 GOTO cc_wcl386
SET CC=wcc
GOTO cc_done

:cc_wcl386
IF NOT "%host%"=="i386-pc-winnt" GOTO cc_cl
wcc386 -q -zs NUL 2>NUL
IF ERRORLEVEL 1 GOTO cc_cl
SET CC=wcc386
GOTO cc_done

:cc_cl
REM There doesn't seem to be any way to differentiate 16- and 32-bit CL.
cl /Zs /TcNUL >NUL 2>NUL
IF ERRORLEVEL 1 GOTO cc_fail
SET CC=cl
GOTO cc_done

:cc_fail
ECHO CC= must be specified explicitly.
GOTO failure

GOTO cc_done

:make_test
IF "%CC%"=="cl" GOTO make_nmake
IF "%CC%"=="CL" GOTO make_nmake
IF "%CC%"=="wcc" GOTO make_wmake
IF "%CC%"=="WCC" GOTO make_nmake
IF "%CC%"=="wcc386" GOTO make_wmake
IF "%CC%"=="WCC386" GOTO make_nmake

ECHO MAKE= must be specified explicitly.
GOTO failure

:make_wmake
SET MAKE=wmake -f
GOTO make_done

:make_nmake
SET MAKE=nmake /f
GOTO make_done

:success
SET host=
SET progname=
REM SET CC=
REM SET MAKE=
EXIT /B

:failure
SET host=
SET progname=
REM SET CC=
REM SET MAKE=
EXIT /B 1
