# Microsoft Developer Studio Project File - Name="spamassassin" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** NICHT BEARBEITEN **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=spamassassin - Win32 Debug
!MESSAGE Dies ist kein gültiges Makefile. Zum Erstellen dieses Projekts mit NMAKE
!MESSAGE verwenden Sie den Befehl "Makefile exportieren" und führen Sie den Befehl
!MESSAGE 
!MESSAGE NMAKE /f "spamassassin.mak".
!MESSAGE 
!MESSAGE Sie können beim Ausführen von NMAKE eine Konfiguration angeben
!MESSAGE durch Definieren des Makros CFG in der Befehlszeile. Zum Beispiel:
!MESSAGE 
!MESSAGE NMAKE /f "spamassassin.mak" CFG="spamassassin - Win32 Debug"
!MESSAGE 
!MESSAGE Für die Konfiguration stehen zur Auswahl:
!MESSAGE 
!MESSAGE "spamassassin - Win32 Release" (basierend auf  "Win32 (x86) Dynamic-Link Library")
!MESSAGE "spamassassin - Win32 Debug" (basierend auf  "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "spamassassin - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "spamassassin___Win32_Release"
# PROP BASE Intermediate_Dir "spamassassin___Win32_Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "spamassassin___Win32_Release"
# PROP Intermediate_Dir "spamassassin___Win32_Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "spamassassin_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "." /I "\dev\proj\aspell\interfaces\cc" /I "..\src" /I "..\src\common" /I "..\src\gtk" /I "..\win32" /I "\dev\include" /I "\dev\include\glib-2.0" /I "\dev\lib\glib-2.0\include" /I "\dev\include\gdk" /I "\dev\include\gtk" /I "\dev\lib\gtk+\include" /I "\dev\proj\fnmatch\src\posix" /I "\dev\proj\libcompface\src" /I "..\libjconv" /I "\dev\proj\regex\src" /I "\dev\proj\w32lib\src" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "spamassassin_EXPORTS" /D "HAVE_CONFIG_H" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x407 /d "NDEBUG"
# ADD RSC /l 0x407 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ws2_32.lib /nologo /dll /machine:I386 /out:"spamassassin.dll"
# Begin Special Build Tool
OutDir=.\spamassassin_d___Win32_Release
SOURCE="$(InputPath)"
PreLink_Desc=creating module definition
PreLink_Cmds=call build_def.bat spamassassin.def $(OutDir)
# End Special Build Tool

!ELSEIF  "$(CFG)" == "spamassassin - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "spamassassin___Win32_Debug"
# PROP BASE Intermediate_Dir "spamassassin___Win32_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "spamassassin___Win32_Debug"
# PROP Intermediate_Dir "spamassassin___Win32_Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "spamassassin_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MD /W3 /Gm /GX /ZI /Od /I "." /I "..\src" /I "..\src\common" /I "..\src\gtk" /I "..\win32" /I "\dev\include" /I "\dev\include\glib-2.0" /I "\dev\lib\glib-2.0\include" /I "\dev\include\gdk" /I "\dev\include\gtk" /I "\dev\lib\gtk+\include" /I "\dev\proj\fnmatch\src\posix" /I "\dev\proj\libcompface\src" /I "..\libjconv" /I "\dev\proj\regex\src" /I "\dev\proj\w32lib\src" /I "\dev" /I "\dev\proj\aspell\interfaces\cc" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "spamassassin_EXPORTS" /D "HAVE_CONFIG_H" /FR /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x407 /d "_DEBUG"
# ADD RSC /l 0x407 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ws2_32.lib /nologo /dll /debug /machine:I386 /out:"spamassassin_d.dll" /pdbtype:sept
# Begin Special Build Tool
OutDir=.\spamassassin_d___Win32_Debug
SOURCE="$(InputPath)"
PreLink_Desc=creating module definition
PreLink_Cmds=call build_def.bat spamassassin_d.def $(OutDir)
# End Special Build Tool

!ENDIF 

# Begin Target

# Name "spamassassin - Win32 Release"
# Name "spamassassin - Win32 Debug"
# Begin Group "Quellcodedateien"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\src\plugins\spamassassin\libspamc.c
# End Source File
# Begin Source File

SOURCE=..\src\plugins\spamassassin\spamassassin.c
# End Source File
# Begin Source File

SOURCE=.\spamassassin.def
# End Source File
# Begin Source File

SOURCE=.\spamassassin_version.rc
# End Source File
# Begin Source File

SOURCE=..\src\plugins\spamassassin\utils.c
# End Source File
# End Group
# Begin Group "Header-Dateien"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\src\plugins\spamassassin\libspamc.h
# End Source File
# Begin Source File

SOURCE=..\src\plugins\spamassassin\spamassassin.h
# End Source File
# Begin Source File

SOURCE=..\src\plugins\spamassassin\utils.h
# End Source File
# End Group
# Begin Group "Ressourcendateien"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# End Group
# Begin Source File

SOURCE="..\..\..\lib\glib-2.0.lib"
# End Source File
# Begin Source File

SOURCE="..\..\..\lib\gmodule-2.0.lib"
# End Source File
# Begin Source File

SOURCE=..\..\..\lib\intl.lib
# End Source File
# Begin Source File

SOURCE=..\..\w32lib\w32lib.lib
# End Source File
# Begin Source File

SOURCE=.\sylpheed_dll___Win32_Release\sylpheed.lib
# End Source File
# End Target
# End Project
