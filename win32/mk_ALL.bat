call vcvars32
set PATH=apps;utils\apps;%PATH%

:generated
cd utils
call mk_versionrc.bat
call mk_version.bat
call mk_passcrypt.bat
call mk_bison.bat
call mk_translation.bat
cd ..

:debug
msdev sylpheed_dll_d.dsp /make "sylpheed_dll_d - Win32 Debug" /rebuild
msdev sylpheed_exe_d.dsp /make "sylpheed_exe_d - Win32 Debug" /rebuild
msdev demo_d.dsp /make "demo_d - Win32 Debug" /rebuild
msdev spamassassin_d.dsp /make "spamassassin_d - Win32 Debug" /rebuild
msdev spamassassin_gtk_d.dsp /make "spamassassin_gtk_d - Win32 Debug" /rebuild

:release
msdev sylpheed_dll.dsp /make "sylpheed_dll - Win32 Release" /rebuild
msdev sylpheed_exe.dsp /make "sylpheed_exe - Win32 Release" /rebuild
msdev demo.dsp /make "demo - Win32 Release" /rebuild
msdev spamassassin.dsp /make "spamassassin - Win32 Release" /rebuild
msdev spamassassin_gtk.dsp /make "spamassassin_gtk - Win32 Release" /rebuild

:end
pause
