#!/bin/sh -e

for lang in $(echo wait-*.rc | sed 's/wait-//g ; s/\.rc//g')
do
	wmake -f wcc.mak clean all lang=$lang
	mv wait.exe wait16$lang.exe
done

./configure --host=mingw32 UNICODE=1
make clean all
strip wait.exe

ls -l --color wait.exe wait16*.exe
