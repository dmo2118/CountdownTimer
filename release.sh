#!/bin/sh -e

wmake -f wcc.mak clean all
mv wait.exe wait16.exe

./configure --host=mingw32 UNICODE=1
make clean all
strip wait.exe

ls -l --color wait.exe wait16.exe

