@echo off
SET VERTMP=mkvertmp.bat
del %VERTMP%
rem create version.h from configure.ac / version.h.in using grep, sed and helper batchfile
set ROOT=..\..
echo @ECHO OFF> %VERTMP%
grep -e "^[^ ]*VERSION=" %ROOT%\configure.ac | sed -e "s/^/SET /;">> %VERTMP%
echo if %%EXTRA_VERSION%%x==x%%EXTRA_VERSION%% goto RELEASE>> %VERTMP%
echo if %%EXTRA_VERSION%%==0 goto RELEASE>> %VERTMP%
echo SET VERSION=%%MAJOR_VERSION%%.%%MINOR_VERSION%%.%%MICRO_VERSION%%cvs%%EXTRA_VERSION%%%%EXTRA_WIN32_VERSION%% Win32>> %VERTMP%
echo goto VERSIONEND>> %VERTMP%
echo :RELEASE>> %VERTMP%
echo SET EXTRA_VERSION=0>> %VERTMP%
echo SET VERSION=%%MAJOR_VERSION%%.%%MINOR_VERSION%%.%%MICRO_VERSION%%%%EXTRA_WIN32_VERSION%% Win32>> %VERTMP%
echo :VERSIONEND>> %VERTMP%
echo sed -e "s/@PACKAGE@/sylpheed-claws/;s/@VERSION@/%%VERSION%%/;s/@MAJOR_VERSION@/%%MAJOR_VERSION%%/;s/@MINOR_VERSION@/%%MINOR_VERSION%%/;s/@MICRO_VERSION@/%%MICRO_VERSION%%/;s/@EXTRA_VERSION@/%%EXTRA_VERSION%%/;s/@EXTRA_WIN32_VERSION@/%%EXTRA_WIN32_VERSION%%/;"  %ROOT%\src\common\version.h.in>> %VERTMP%
type %VERTMP%
call %VERTMP% > %ROOT%\src\common\version.h
rem del %VERTMP%
set ROOT=
SET VERTMP=
