@echo off
rem create version.h from configure.in / version.h.in using grep, sed and helper batchfile
set ROOT=..\..
rem @ECHO OFF > mkvertmp.bat
grep VERSION= %ROOT%\configure.in | sed -e "s/^/SET /;s/\$/%%/g;s/\./%%./g;s/EXTRA_VERSION$/%%EXTRA_VERSION%% Win32/" >> mkvertmp.bat
echo sed -e "s/@PACKAGE@/sylpheed/;s/@VERSION@/%%VERSION%%/" %ROOT%\src\version.h.in >> mkvertmp.bat
type mkvertmp.bat
call mkvertmp.bat > %ROOT%\src\version.h
del mkvertmp.bat
set ROOT=
