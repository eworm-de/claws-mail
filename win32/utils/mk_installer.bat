@echo off
rem create setup.iss from setup.iss.in and configure.ac
set ROOT=../..
set SETUP_ISS=sylpheed-setup.iss
set SETUP_ISS_IN=%SETUP_ISS%.in
set MKINST=mkinstaller.bat
rem @ECHO OFF > mkvertmp.bat
grep VERSION= %ROOT%/configure.ac | sed -e "s/^/SET /;s/claws/Claws/g;s/\$/%%/g;s/\./%%./g;s/EXTRA_VERSION$/-%%EXTRA_VERSION%%/" >> %MKINST%
echo SET SHORTVER=%%MAJOR_VERSION%%%%MINOR_VERSION%%%%MICRO_VERSION%%>> %MKINST%
echo SET PAKDIR=d:\\\\_pak\\\\sylpheed.%%SHORTVER%%.%%EXTRA_VERSION%%>> %MKINST%
echo sed -e "s/@VERSION@/%%VERSION%%/;s/@MAJOR_VERSION@/%%MAJOR_VERSION%%/;s/@MINOR_VERSION@/%%MINOR_VERSION%%/;s/@MICRO_VERSION@/%%MICRO_VERSION%%/;s/@EXTRA_VERSION@/%%EXTRA_VERSION%%/;s/@PAKDIR@/%%PAKDIR%%/" %SETUP_ISS_IN% >> %MKINST%
type %MKINST%
call %MKINST% > %SETUP_ISS%
del %MKINST%
set ROOT=
start %SETUP_ISS%
