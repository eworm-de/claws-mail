@echo off
set PATH=\dev\proj\sylpheed-claws\win32\utils\apps;%PATH%
cd ..\win32\utils
echo --- creating bison/flex files ---
call mk_bison.bat
echo --- creating version.h ---
call mk_version.bat
echo --- creating passcrypt.h ---
call mk_passcrypt.bat
echo --- creating translations ---
call mk_translation.bat
cd ..
