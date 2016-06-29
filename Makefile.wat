# Where .wat = (Open)Watcom.
# Use with WMAKE.

all: wait.exe

.EXTENSIONS: .res .rc

wait.exe: wait.res wait.obj
	wlink name wait sys windows libr shell fil wait.obj op resource=wait.res

.rc.res:
	wrc -bt=windows -d_WINDOWS -r -fo=$@ $<

.c.obj
	wcc -bt=windows -d_WINDOWS -i="$(%watcom)/h/win" -w4 -os -ms -fo=$@ $<

clean: .SYMBOLIC
	del wait.exe wait.obj wait.res
