@echo off
set PATH=\dev\proj\sylpheed-claws\win32\utils\apps;%PATH%
patch -d "../src" -p1 -i "../win32/patches/generated.diff"
cd ..\win32\utils
rem cd utils
call mk_version.bat
call mk_translation.bat
cd ..
