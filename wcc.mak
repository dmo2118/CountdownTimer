# Where .wat = (Open)Watcom.
# Use with Open Watcom Make.

!ifeq host i86-pc-win16
CC = wcc
!else ifeq host i386-pc-winnt
CC = wcc386
!endif

all: wait.exe

.EXTENSIONS: .res .rc

wait.exe: wait.res wait.obj
	wlink op quiet name wait sys windows libr shell fil wait.obj op resource=wait.res

.rc.res:
	wrc -q -bt=windows -d_WINDOWS -r -fo=$@ $<

.c.obj
	$(CC) -q -bt=windows -d_WINDOWS -i="$(%watcom)/h/win" -w4 -os -ms -fo=$@ $<

clean: .SYMBOLIC
	-rm wait.exe
	-rm wait.obj
	-rm wait.res
