# Where .wat = (Open)Watcom.
# Use with Open Watcom Make.

CRFLAGS=-q -DNDEBUG

!ifeq host i86-pc-win16
CC=wcc
CRFLAGS += -bt=windows -d_WINDOWS -i="$(%watcom)/h/win"
CFLAGS += -ms
LDLIBS=libr shell
LDFLAGS=sys windows
!else ifeq host i386-pc-winnt
CC=wcc386
CRFLAGS += -bt=nt
LDLIBS=libr kernel32 libr user32
LDFLAGS=sys nt_win
!endif

all: wait.exe

.EXTENSIONS: .res .rc

wait.exe: wait.res wait.obj
	wlink op quiet name wait $(LDFLAGS) fil wait.obj $(LDLIBS) op resource=wait.res

.rc.res:
	wrc $(CRFLAGS) $(RFLAGS) -r -fo=$@ $<

.c.obj
	$(CC) $(CRFLAGS) $(CFLAGS) -w4 -os -fo=$@ $<

clean: .SYMBOLIC
	-rm wait.exe
	-rm wait.obj
	-rm wait.res
