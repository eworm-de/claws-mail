if %1x==x goto ERR_PARAM
if %2x==x goto ERR_PARAM
if %3x==x goto CALLSELF

echo adding %3 to %1...
echo ;------------------------------------------------------------>>%1
echo ;%3>>%1
pubobj %3>>%1 2>NUL
goto END

:CALLSELF
echo EXPORTS>%1
for %%i in (%2\*.obj) do call %0 %1 %2 %%i
goto END

:ERR_PARAM
echo [Deffile] and/or [Objdir] missing
exit 1

:END
