@echo off
rem compile translation files (*.po --> sylpheed.mo)
rem assumes msgfmt and utf8conv in apps subdir

if not %1x==x%1 goto process

echo Creating translations:
if not exist locale mkdir locale
for %%i in ( bg cs de el en_GB es fr hr hu it ja ko nl pl pt_BR ru sr sv zh_TW.Big5 ) do call %0 %%i
echo Done. Copy "locale" folder to your sylpheed directory.
goto end

:process
echo Processing %1 ...
if not exist locale\%1 mkdir locale\%1
if not exist locale\%1\LC_MESSAGES mkdir locale\%1\LC_MESSAGES
rem *** utf8 translation ***
SET CONTENTTYPE=^.Content-Type: text\/plain; charset=
copy ..\..\po\%1.po
echo @echo off > mk_%1.bat
echo set PATH=apps;%%PATH%% >> mk_%1.bat
sed -n -e "/%CONTENTTYPE%/ {s/.*=/iconv -f /;s/\\\\.*/ -t utf-8 %1.po/;p;} " %1.po >> mk_%1.bat
call mk_%1.bat > %1-utf8.po
rem *** make .mo ***
apps\msgfmt -o locale\%1\LC_MESSAGES\sylpheed.mo %1-utf8.po
del %1.po %1-utf8.po mk_%1.bat

:end
