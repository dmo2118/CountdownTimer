# /GA means different things for 16- and 32-bit Windows.
CFLAGS=/nologo /GA /W3 /O2 /DNDEBUG

!IFNDEF host
host=i386-pc-winnt
!ENDIF

# Not strictly necessary, this bit.
!IF "$(host)"=="x86"
host=i386-pc-winnt
!ELSE IF "$(host)"=="Win32"
host=i386-pc-winnt
!ELSE IF "$(host)"=="x64"
host=x86_64-pc-winnt
!ENDIF

!IF "$(UNICODE)"=="1"
CFLAGS=$(CFLAGS) /DUNICODE
!ENDIF

all: wait.exe

objs=wait.obj dialog.obj

clean:
	del wait.exe
	del wait.obj
	del dialog.obj
	del wait.res

!IF "$(host)"=="i86-pc-win16"
wait.exe: wait.rc $(objs) wait.def
	$(CC) $(CFLAGS) wait.def $(objs) shell.lib ver.lib
	$(RC) -D_WINDOWS wait.rc wait.exe
!ELSE
wait.exe: $(objs) wait.res
	$(CC) $(CFLAGS) $** user32.lib shell32.lib
!ENDIF
