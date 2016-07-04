# /GA means different things for 16- and 32-bit Windows.
CFLAGS=/nologo /GA /W3 /O2 /DNDEBUG

!IFNDEF host
host=i386-pc-winnt
!ENDIF

!IF "$(UNICODE)"=="1"
CFLAGS=$(CFLAGS) /DUNICODE
!ENDIF

all: wait.exe

!IF "$(host)"=="i86-pc-win16"
wait.exe: wait.rc wait.obj wait.def
	$(CC) $(CFLAGS) wait.def wait.obj shell.lib
	$(RC) -D_WINDOWS wait.rc wait.exe
!ELSE
wait.exe: wait.res wait.obj
	$(CC) $(CFLAGS) $** user32.lib shell32.lib /link /MACHINE:X86
!ENDIF

clean:
	del wait.exe
	del wait.obj
	del wait.res
