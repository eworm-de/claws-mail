@echo off
rem mk_versionrc.bat
rem   without params:
rem     create version.rc.in from version.rc.in.in
rem   with params:
rem     create a final version.rc from version.rc.in
rem     param 1..4 : version numbers (major, minor, micro, extra)
rem              5 : file description (text)
rem              6 : input file (e.g. c:\foo\version.rc.in)
rem              7 : output file (e.g. c:\bar\version.rc)

if %1x==x goto TEMPLATE
:FINAL
sed -e "s/@FILE_MAJOR_VERSION@/%1/;s/@FILE_MINOR_VERSION@/%2/;s/@FILE_MICRO_VERSION@/%3/;s/@FILE_EXTRA_VERSION@/%4/; s/@FILEDESCRIPTION@/%5/"  %6 > %7
goto END
:TEMPLATE
SET VERTMP=mkvertmp.bat
if EXIST %VERTMP% del %VERTMP%
rem create version.rc.in from configure.ac / version.rc.in.in using grep, sed and helper batchfile
set ROOT=..\..
set BUILD=..
echo @ECHO OFF > %VERTMP%
rem --- extract version info ---
grep -e "^[^ ]*VERSION=" %ROOT%\configure.ac | sed -e "s/^/SET /;s/\$/%%/g;s/\./%%./g;s/EXTRA_VERSION$/%%EXTRA_VERSION%% Win32/" >> %VERTMP%
rem ---- create version.rc.in ----
echo if %%EXTRA_VERSION%%x==x%%EXTRA_VERSION%% goto RELEASE>> %VERTMP%
echo if %%EXTRA_VERSION%%==0 goto RELEASE>> %VERTMP%
echo SET VERSION=%%MAJOR_VERSION%%.%%MINOR_VERSION%%.%%MICRO_VERSION%%claws%%EXTRA_VERSION%%>> %VERTMP%
echo goto VERSIONEND>> %VERTMP%
echo :RELEASE>> %VERTMP%
echo SET EXTRA_VERSION=0 >> %VERTMP%
echo SET VERSION=%%MAJOR_VERSION%%.%%MINOR_VERSION%%.%%MICRO_VERSION%%claws>> %VERTMP%
echo :VERSIONEND>> %VERTMP%
echo sed -e "s/@MAJOR_VERSION@/%%MAJOR_VERSION%%/;s/@MINOR_VERSION@/%%MINOR_VERSION%%/;s/@MICRO_VERSION@/%%MICRO_VERSION%%/;s/@EXTRA_VERSION@/%%EXTRA_VERSION%%/;"  %BUILD%\version.rc.in.in >> %VERTMP%
rem ---- create version.rc for Sylpheed and plugins ----
echo call %0 %%MAJOR_VERSION%% %%MINOR_VERSION%% %%MICRO_VERSION%% %%EXTRA_VERSION%% Sylpheed-Claws %BUILD%\version.rc.in %BUILD%\version.rc>> %VERTMP%
echo call %0 %%MAJOR_VERSION%% %%MINOR_VERSION%% %%MICRO_VERSION%% %%EXTRA_VERSION%% Demo-plugin %BUILD%\version.rc.in %BUILD%\demo_version.rc>> %VERTMP%
echo call %0 %%MAJOR_VERSION%% %%MINOR_VERSION%% %%MICRO_VERSION%% %%EXTRA_VERSION%% SpamAssassin-plugin %BUILD%\version.rc.in %BUILD%\spamassassin_version.rc>> %VERTMP%
type %VERTMP%
call %VERTMP% > %BUILD%\version.rc.in
if EXIST %VERTMP% del %VERTMP%
set ROOT=
set BUILD=
SET VERTMP=
:END
