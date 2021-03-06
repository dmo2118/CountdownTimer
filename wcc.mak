# Use with Open Watcom wmake.

CRFLAGS=-q -DNDEBUG

!ifndef host
!	define host i86-pc-win16
!endif

!ifeq host i86-pc-win16
CC=wcc
CRFLAGS += -bt=windows -d_WINDOWS -i="$(%watcom)/h" -i="$(%watcom)/h/win"
CFLAGS += -ms
LDLIBS=libr shell libr ver
LDFLAGS=sys windows
!	ifndef lang
lang=en
!	endif
!else ifeq host i386-pc-winnt
CC=wcc386
CRFLAGS += -bt=nt -i="$(%watcom)/h" -i="$(%watcom)/h/nt"
LDLIBS=libr kernel32 libr user32
LDFLAGS=sys nt_win

!	ifeq UNICODE 1
CFLAGS += -DUNICODE
!	endif
!endif

!ifdef lang
wait_rc="wait-$(lang).rc"
!else
wait_rc="wait.rc"
!endif

all: wait.exe

.EXTENSIONS: .res .rc

objs=wait.obj dialog.obj

wait.exe: wait.res $(objs)
	wlink op quiet name wait $(LDFLAGS) fil wait.obj fil dialog.obj $(LDLIBS) op resource=wait.res

wait.res: $(wait_rc)
	wrc $(CRFLAGS) -r -fo=$@ $<

.c.obj
	$(CC) $(CRFLAGS) $(CFLAGS) -w4 -os -fo=$@ $<

clean: .SYMBOLIC
	-rm wait.exe
	-rm $(objs)
	-rm wait.res
