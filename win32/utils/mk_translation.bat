@echo off
rem compile translation files (*.po --> sylpheed.mo)
rem assumes msgfmt and utf8conv in apps subdir

if not %1x==x%1 goto process

echo Creating translations:
if not exist locale mkdir locale
for %%i in ( bg cs de el en_GB es fr hr it ja ko nl pl pt_BR ru sv ) do call %0 %%i
echo Done. Copy "locale" folder to your sylpheed directory.
goto end

:process
echo Processing %1 ...
if not exist locale\%1 mkdir locale\%1
if not exist locale\%1\LC_MESSAGES mkdir locale\%1\LC_MESSAGES
apps\utf8conv ..\..\po\%1.po %1-utf8.po
apps\msgfmt -o locale\%1\LC_MESSAGES\sylpheed.mo %1-utf8.po
del %1-utf8.po

:end
