CFLAGS=/MT /W3 /O2 /D NDEBUG

all: wait.exe

wait.exe: Wait.res wait.obj
	$(CC) $(CFLAGS) $** user32.lib shell32.lib /link /MACHINE:X86

clean:
	del wait.exe wait.obj Wait.res
