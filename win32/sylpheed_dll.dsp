# Microsoft Developer Studio Project File - Name="sylpheed_dll" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** NICHT BEARBEITEN **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102

CFG=sylpheed_dll - Win32 Debug
!MESSAGE Dies ist kein gültiges Makefile. Zum Erstellen dieses Projekts mit NMAKE
!MESSAGE verwenden Sie den Befehl "Makefile exportieren" und führen Sie den Befehl
!MESSAGE 
!MESSAGE NMAKE /f "sylpheed_dll.mak".
!MESSAGE 
!MESSAGE Sie können beim Ausführen von NMAKE eine Konfiguration angeben
!MESSAGE durch Definieren des Makros CFG in der Befehlszeile. Zum Beispiel:
!MESSAGE 
!MESSAGE NMAKE /f "sylpheed_dll.mak" CFG="sylpheed_dll - Win32 Debug"
!MESSAGE 
!MESSAGE Für die Konfiguration stehen zur Auswahl:
!MESSAGE 
!MESSAGE "sylpheed_dll - Win32 Release" (basierend auf  "Win32 (x86) Dynamic-Link Library")
!MESSAGE "sylpheed_dll - Win32 Debug" (basierend auf  "Win32 (x86) Dynamic-Link Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "sylpheed_dll - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "sylpheed_dll___Win32_Release"
# PROP BASE Intermediate_Dir "sylpheed_dll___Win32_Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "sylpheed_dll___Win32_Release"
# PROP Intermediate_Dir "sylpheed_dll___Win32_Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "sylpheed_dll_EXPORTS" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /I "." /I "\dev\proj\aspell\interfaces\cc" /I "..\src" /I "..\src\common" /I "..\src\gtk" /I "..\win32" /I "\dev\include" /I "\dev\include\glib-2.0" /I "\dev\lib\glib-2.0\include" /I "\dev\include\gdk" /I "\dev\include\gtk" /I "\dev\lib\gtk+\include" /I "\dev\proj\fnmatch\src\posix" /I "\dev\proj\libcompface\src" /I "..\libjconv" /I "\dev\proj\regex\src" /I "\dev\proj\w32lib\src" /I "\dev\proj\gpgme\gpgme" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "sylpheed_dll_EXPORTS" /D "HAVE_CONFIG_H" /YX /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x407 /d "NDEBUG"
# ADD RSC /l 0x407 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib wsock32.lib /nologo /dll /machine:I386 /out:"sylpheed.dll"

!ELSEIF  "$(CFG)" == "sylpheed_dll - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "sylpheed_dll___Win32_Debug"
# PROP BASE Intermediate_Dir "sylpheed_dll___Win32_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "sylpheed_dll___Win32_Debug"
# PROP Intermediate_Dir "sylpheed_dll___Win32_Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "sylpheed_dll_EXPORTS" /YX /FD /GZ /c
# ADD CPP /nologo /MD /W3 /Gm /GX /ZI /Od /I "." /I "..\src" /I "..\src\common" /I "..\src\gtk" /I "..\win32" /I "\dev\include" /I "\dev\include\glib-2.0" /I "\dev\lib\glib-2.0\include" /I "\dev\include\gdk" /I "\dev\include\gtk" /I "\dev\lib\gtk+\include" /I "\dev\proj\fnmatch\src\posix" /I "\dev\proj\libcompface\src" /I "..\libjconv" /I "\dev\proj\regex\src" /I "\dev\proj\w32lib\src" /I "\dev\proj\gpgme\gpgme" /I "\dev\proj\aspell\interfaces\cc" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /D "_USRDLL" /D "sylpheed_dll_EXPORTS" /D "HAVE_CONFIG_H" /YX /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x407 /d "_DEBUG"
# ADD RSC /l 0x407 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /dll /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib wsock32.lib /nologo /dll /debug /machine:I386 /out:"sylpheed_d.dll" /pdbtype:sept

!ENDIF 

# Begin Target

# Name "sylpheed_dll - Win32 Release"
# Name "sylpheed_dll - Win32 Debug"
# Begin Group "Quellcodedateien"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\src\about.c
# End Source File
# Begin Source File

SOURCE=..\src\account.c
# End Source File
# Begin Source File

SOURCE=..\src\addr_compl.c
# End Source File
# Begin Source File

SOURCE=..\src\addrbook.c
# End Source File
# Begin Source File

SOURCE=..\src\addrcache.c
# End Source File
# Begin Source File

SOURCE=..\src\addrclip.c
# End Source File
# Begin Source File

SOURCE=..\src\addressadd.c
# End Source File
# Begin Source File

SOURCE=..\src\addressbook.c
# End Source File
# Begin Source File

SOURCE=..\src\addrgather.c
# End Source File
# Begin Source File

SOURCE=..\src\addrharvest.c
# End Source File
# Begin Source File

SOURCE=..\src\addrindex.c
# End Source File
# Begin Source File

SOURCE=..\src\addritem.c
# End Source File
# Begin Source File

SOURCE=..\src\addrselect.c
# End Source File
# Begin Source File

SOURCE=..\src\alertpanel.c
# End Source File
# Begin Source File

SOURCE=..\src\automaton.c
# End Source File
# Begin Source File

SOURCE=..\src\common\base64.c
# End Source File
# Begin Source File

SOURCE=..\src\codeconv.c
# End Source File
# Begin Source File

SOURCE=..\src\colorlabel.c
# End Source File
# Begin Source File

SOURCE=..\libjconv\compat.c
# End Source File
# Begin Source File

SOURCE=..\src\compose.c
# End Source File
# Begin Source File

SOURCE=..\libjconv\conv.c
# End Source File
# Begin Source File

SOURCE=..\src\customheader.c
# End Source File
# Begin Source File

SOURCE=..\src\gtk\description_window.c
# End Source File
# Begin Source File

SOURCE=..\src\displayheader.c
# End Source File
# Begin Source File

SOURCE=..\src\editaddress.c
# End Source File
# Begin Source File

SOURCE=..\src\editbook.c
# End Source File
# Begin Source File

SOURCE=..\src\editgroup.c
# End Source File
# Begin Source File

SOURCE=..\src\editjpilot.c
# End Source File
# Begin Source File

SOURCE=..\src\editldap.c
# End Source File
# Begin Source File

SOURCE=..\src\editldap_basedn.c
# End Source File
# Begin Source File

SOURCE=..\src\editvcard.c
# End Source File
# Begin Source File

SOURCE=..\src\enriched.c
# End Source File
# Begin Source File

SOURCE=..\src\exphtmldlg.c
# End Source File
# Begin Source File

SOURCE=..\src\export.c
# End Source File
# Begin Source File

SOURCE=..\src\exporthtml.c
# End Source File
# Begin Source File

SOURCE=..\src\filesel.c
# End Source File
# Begin Source File

SOURCE=..\src\filtering.c
# End Source File
# Begin Source File

SOURCE=..\src\folder.c
# End Source File
# Begin Source File

SOURCE=..\src\foldersel.c
# End Source File
# Begin Source File

SOURCE=..\src\folderview.c
# End Source File
# Begin Source File

SOURCE=..\src\grouplistdialog.c
# End Source File
# Begin Source File

SOURCE=..\src\gtkaspell.c
# End Source File
# Begin Source File

SOURCE=..\src\gtk\gtksctree.c
# End Source File
# Begin Source File

SOURCE=..\src\gtk\gtkshruler.c
# End Source File
# Begin Source File

SOURCE=..\src\gtk\gtkstext.c
# End Source File
# Begin Source File

SOURCE=..\src\gtkutils.c
# End Source File
# Begin Source File

SOURCE=..\src\headerview.c
# End Source File
# Begin Source File

SOURCE=..\src\common\hooks.c
# End Source File
# Begin Source File

SOURCE=..\src\html.c
# End Source File
# Begin Source File

SOURCE=..\src\imageview.c
# End Source File
# Begin Source File

SOURCE=..\src\imap.c
# End Source File
# Begin Source File

SOURCE=..\src\import.c
# End Source File
# Begin Source File

SOURCE=..\src\importldif.c
# End Source File
# Begin Source File

SOURCE=..\src\importmutt.c
# End Source File
# Begin Source File

SOURCE=..\src\importpine.c
# End Source File
# Begin Source File

SOURCE=..\src\inc.c
# End Source File
# Begin Source File

SOURCE=..\libjconv\info.c
# End Source File
# Begin Source File

SOURCE=..\src\inputdialog.c
# End Source File
# Begin Source File

SOURCE=..\src\jpilot.c
# End Source File
# Begin Source File

SOURCE=..\src\ldif.c
# End Source File
# Begin Source File

SOURCE=..\src\common\log.c
# End Source File
# Begin Source File

SOURCE=..\src\logwindow.c
# End Source File
# Begin Source File

SOURCE=..\src\main.c
# End Source File
# Begin Source File

SOURCE=..\src\mainwindow.c
# End Source File
# Begin Source File

SOURCE=..\src\gtk\manage_window.c
# End Source File
# Begin Source File

SOURCE=..\src\manual.c
# End Source File
# Begin Source File

SOURCE=..\src\matcher.c
# End Source File
# Begin Source File

SOURCE=..\src\matcher_parser_lex.c
# End Source File
# Begin Source File

SOURCE=..\src\matcher_parser_parse.c
# End Source File
# Begin Source File

SOURCE=..\src\mbox.c
# End Source File
# Begin Source File

SOURCE=..\src\mbox_folder.c
# End Source File
# Begin Source File

SOURCE=..\src\common\md5.c
# End Source File
# Begin Source File

SOURCE=..\src\gtk\menu.c
# End Source File
# Begin Source File

SOURCE=..\src\message_search.c
# End Source File
# Begin Source File

SOURCE=..\src\messageview.c
# End Source File
# Begin Source File

SOURCE=..\src\mgutils.c
# End Source File
# Begin Source File

SOURCE=..\src\mh.c
# End Source File
# Begin Source File

SOURCE=..\src\mimeview.c
# End Source File
# Begin Source File

SOURCE=..\src\msgcache.c
# End Source File
# Begin Source File

SOURCE=..\src\mutt.c
# End Source File
# Begin Source File

SOURCE=..\src\news.c
# End Source File
# Begin Source File

SOURCE=..\src\common\nntp.c
# End Source File
# Begin Source File

SOURCE=..\src\noticeview.c
# End Source File
# Begin Source File

SOURCE=..\src\passphrase.c
# End Source File
# Begin Source File

SOURCE=..\src\pine.c
# End Source File
# Begin Source File

SOURCE=..\src\common\plugin.c
# End Source File
# Begin Source File

SOURCE=..\src\gtk\pluginwindow.c
# End Source File
# Begin Source File

SOURCE=..\src\pop.c
# End Source File
# Begin Source File

SOURCE=..\src\common\prefs.c
# End Source File
# Begin Source File

SOURCE=..\src\prefs_account.c
# End Source File
# Begin Source File

SOURCE=..\src\prefs_actions.c
# End Source File
# Begin Source File

SOURCE=..\src\prefs_common.c
# End Source File
# Begin Source File

SOURCE=..\src\prefs_customheader.c
# End Source File
# Begin Source File

SOURCE=..\src\prefs_display_header.c
# End Source File
# Begin Source File

SOURCE=..\src\prefs_filtering.c
# End Source File
# Begin Source File

SOURCE=..\src\prefs_folder_item.c
# End Source File
# Begin Source File

SOURCE=..\src\prefs_gtk.c
# End Source File
# Begin Source File

SOURCE=..\src\prefs_matcher.c
# End Source File
# Begin Source File

SOURCE=..\src\prefs_scoring.c
# End Source File
# Begin Source File

SOURCE=..\src\prefs_summary_column.c
# End Source File
# Begin Source File

SOURCE=..\src\prefs_template.c
# End Source File
# Begin Source File

SOURCE=..\src\prefs_toolbar.c
# End Source File
# Begin Source File

SOURCE=..\src\gtk\prefswindow.c
# End Source File
# Begin Source File

SOURCE=..\src\procheader.c
# End Source File
# Begin Source File

SOURCE=..\src\procmime.c
# End Source File
# Begin Source File

SOURCE=..\src\procmsg.c
# End Source File
# Begin Source File

SOURCE=..\src\progressdialog.c
# End Source File
# Begin Source File

SOURCE=..\src\quote_fmt.c
# End Source File
# Begin Source File

SOURCE=..\src\quote_fmt_lex.c
# End Source File
# Begin Source File

SOURCE=..\src\quote_fmt_parse.c
# End Source File
# Begin Source File

SOURCE="..\src\common\quoted-printable.c"
# End Source File
# Begin Source File

SOURCE=..\src\recv.c
# End Source File
# Begin Source File

SOURCE=..\src\rfc2015.c
# End Source File
# Begin Source File

SOURCE=..\src\scoring.c
# End Source File
# Begin Source File

SOURCE="..\src\select-keys.c"
# End Source File
# Begin Source File

SOURCE=..\src\selective_download.c
# End Source File
# Begin Source File

SOURCE=..\src\send.c
# End Source File
# Begin Source File

SOURCE=..\src\common\session.c
# End Source File
# Begin Source File

SOURCE=..\src\setup.c
# End Source File
# Begin Source File

SOURCE=..\src\sigstatus.c
# End Source File
# Begin Source File

SOURCE="..\src\simple-gettext.c"
# End Source File
# Begin Source File

SOURCE=..\src\common\smtp.c
# End Source File
# Begin Source File

SOURCE=..\src\common\socket.c
# End Source File
# Begin Source File

SOURCE=..\src\sourcewindow.c
# End Source File
# Begin Source File

SOURCE=..\src\common\ssl.c
# End Source File
# Begin Source File

SOURCE=..\src\common\ssl_certificate.c
# End Source File
# Begin Source File

SOURCE=..\src\ssl_manager.c
# End Source File
# Begin Source File

SOURCE=..\src\gtk\sslcertwindow.c
# End Source File
# Begin Source File

SOURCE=..\src\statusbar.c
# End Source File
# Begin Source File

SOURCE=..\src\stock_pixmap.c
# End Source File
# Begin Source File

SOURCE=..\src\string_match.c
# End Source File
# Begin Source File

SOURCE=..\src\common\stringtable.c
# End Source File
# Begin Source File

SOURCE=..\src\summary_search.c
# End Source File
# Begin Source File

SOURCE=..\src\summaryview.c
# End Source File
# Begin Source File

SOURCE=..\src\syldap.c
# End Source File
# Begin Source File

SOURCE=..\src\common\sylpheed.c
# End Source File
# Begin Source File

SOURCE=.\sylpheed.dll.def
# End Source File
# Begin Source File

SOURCE=.\sylpheed.rc
# End Source File
# Begin Source File

SOURCE=..\src\common\template.c
# End Source File
# Begin Source File

SOURCE=..\src\textview.c
# End Source File
# Begin Source File

SOURCE=..\src\toolbar.c
# End Source File
# Begin Source File

SOURCE=..\src\undo.c
# End Source File
# Begin Source File

SOURCE=..\src\unmime.c
# End Source File
# Begin Source File

SOURCE=..\src\common\utils.c
# End Source File
# Begin Source File

SOURCE=..\src\common\uuencode.c
# End Source File
# Begin Source File

SOURCE=..\src\vcard.c
# End Source File
# Begin Source File

SOURCE=..\src\w32_aspell_init.c
# End Source File
# Begin Source File

SOURCE=..\src\w32_mailcap.c
# End Source File
# Begin Source File

SOURCE=..\src\xml.c
# End Source File
# Begin Source File

SOURCE=..\src\xmlprops.c
# End Source File
# End Group
# Begin Group "Header-Dateien"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\src\about.h
# End Source File
# Begin Source File

SOURCE=..\src\account.h
# End Source File
# Begin Source File

SOURCE=..\src\addr_compl.h
# End Source File
# Begin Source File

SOURCE=..\src\addrbook.h
# End Source File
# Begin Source File

SOURCE=..\src\addrcache.h
# End Source File
# Begin Source File

SOURCE=..\src\addressadd.h
# End Source File
# Begin Source File

SOURCE=..\src\addressbook.h
# End Source File
# Begin Source File

SOURCE=..\src\addressitem.h
# End Source File
# Begin Source File

SOURCE=..\src\addrindex.h
# End Source File
# Begin Source File

SOURCE=..\src\addritem.h
# End Source File
# Begin Source File

SOURCE=..\src\alertpanel.h
# End Source File
# Begin Source File

SOURCE=..\src\automaton.h
# End Source File
# Begin Source File

SOURCE=..\src\common\base64.h
# End Source File
# Begin Source File

SOURCE=..\src\codeconv.h
# End Source File
# Begin Source File

SOURCE=..\src\colorlabel.h
# End Source File
# Begin Source File

SOURCE=..\src\compose.h
# End Source File
# Begin Source File

SOURCE=..\win32\config.h
# End Source File
# Begin Source File

SOURCE=..\src\customheader.h
# End Source File
# Begin Source File

SOURCE=..\src\common\defs.h
# End Source File
# Begin Source File

SOURCE=..\src\gtk\description_window.h
# End Source File
# Begin Source File

SOURCE=..\src\displayheader.h
# End Source File
# Begin Source File

SOURCE=..\src\editaddress.h
# End Source File
# Begin Source File

SOURCE=..\src\editbook.h
# End Source File
# Begin Source File

SOURCE=..\src\editgroup.h
# End Source File
# Begin Source File

SOURCE=..\src\editjpilot.h
# End Source File
# Begin Source File

SOURCE=..\src\editldap.h
# End Source File
# Begin Source File

SOURCE=..\src\editldap_basedn.h
# End Source File
# Begin Source File

SOURCE=..\src\editvcard.h
# End Source File
# Begin Source File

SOURCE=..\src\exphtmldlg.h
# End Source File
# Begin Source File

SOURCE=..\src\export.h
# End Source File
# Begin Source File

SOURCE=..\src\exporthtml.h
# End Source File
# Begin Source File

SOURCE=..\src\filesel.h
# End Source File
# Begin Source File

SOURCE=..\src\folder.h
# End Source File
# Begin Source File

SOURCE=..\src\foldersel.h
# End Source File
# Begin Source File

SOURCE=..\src\folderview.h
# End Source File
# Begin Source File

SOURCE=..\src\grouplistdialog.h
# End Source File
# Begin Source File

SOURCE=..\src\gtkaspell.h
# End Source File
# Begin Source File

SOURCE=..\src\gtk\gtksctree.h
# End Source File
# Begin Source File

SOURCE=..\src\gtk\gtkshruler.h
# End Source File
# Begin Source File

SOURCE=..\src\gtk\gtkstext.h
# End Source File
# Begin Source File

SOURCE=..\src\gtkutils.h
# End Source File
# Begin Source File

SOURCE=..\src\headerview.h
# End Source File
# Begin Source File

SOURCE=..\src\common\hooks.h
# End Source File
# Begin Source File

SOURCE=..\src\html.h
# End Source File
# Begin Source File

SOURCE=..\src\imageview.h
# End Source File
# Begin Source File

SOURCE=..\src\imap.h
# End Source File
# Begin Source File

SOURCE=..\src\import.h
# End Source File
# Begin Source File

SOURCE=..\src\importldif.h
# End Source File
# Begin Source File

SOURCE=..\src\inc.h
# End Source File
# Begin Source File

SOURCE=..\src\inputdialog.h
# End Source File
# Begin Source File

SOURCE=..\src\common\intl.h
# End Source File
# Begin Source File

SOURCE=..\libjconv\jconv.h
# End Source File
# Begin Source File

SOURCE=..\src\jpilot.h
# End Source File
# Begin Source File

SOURCE=..\src\ldif.h
# End Source File
# Begin Source File

SOURCE=..\src\common\log.h
# End Source File
# Begin Source File

SOURCE=..\src\logwindow.h
# End Source File
# Begin Source File

SOURCE=..\src\main.h
# End Source File
# Begin Source File

SOURCE=..\src\mainwindow.h
# End Source File
# Begin Source File

SOURCE=..\src\gtk\manage_window.h
# End Source File
# Begin Source File

SOURCE=..\src\manual.h
# End Source File
# Begin Source File

SOURCE=..\src\matcher.h
# End Source File
# Begin Source File

SOURCE=..\src\mbox.h
# End Source File
# Begin Source File

SOURCE=..\src\common\md5.h
# End Source File
# Begin Source File

SOURCE=..\src\gtk\menu.h
# End Source File
# Begin Source File

SOURCE=..\src\message_search.h
# End Source File
# Begin Source File

SOURCE=..\src\messageview.h
# End Source File
# Begin Source File

SOURCE=..\src\mgutils.h
# End Source File
# Begin Source File

SOURCE=..\src\mh.h
# End Source File
# Begin Source File

SOURCE=..\src\mimeview.h
# End Source File
# Begin Source File

SOURCE=..\src\msgcache.h
# End Source File
# Begin Source File

SOURCE=..\src\news.h
# End Source File
# Begin Source File

SOURCE=..\src\common\nntp.h
# End Source File
# Begin Source File

SOURCE=..\src\noticeview.h
# End Source File
# Begin Source File

SOURCE=..\src\passphrase.h
# End Source File
# Begin Source File

SOURCE=..\src\common\plugin.h
# End Source File
# Begin Source File

SOURCE=..\src\gtk\pluginwindow.h
# End Source File
# Begin Source File

SOURCE=..\src\pop.h
# End Source File
# Begin Source File

SOURCE=..\src\common\prefs.h
# End Source File
# Begin Source File

SOURCE=..\src\prefs_account.h
# End Source File
# Begin Source File

SOURCE=..\src\prefs_common.h
# End Source File
# Begin Source File

SOURCE=..\src\prefs_customheader.h
# End Source File
# Begin Source File

SOURCE=..\src\prefs_display_header.h
# End Source File
# Begin Source File

SOURCE=..\src\prefs_folder_item.h
# End Source File
# Begin Source File

SOURCE=..\src\prefs_gtk.h
# End Source File
# Begin Source File

SOURCE=..\src\prefs_summary_column.h
# End Source File
# Begin Source File

SOURCE=..\src\prefs_template.h
# End Source File
# Begin Source File

SOURCE=..\src\prefs_toolbar.h
# End Source File
# Begin Source File

SOURCE=..\src\gtk\prefswindow.h
# End Source File
# Begin Source File

SOURCE=..\src\procheader.h
# End Source File
# Begin Source File

SOURCE=..\src\procmime.h
# End Source File
# Begin Source File

SOURCE=..\src\procmsg.h
# End Source File
# Begin Source File

SOURCE=..\src\progressdialog.h
# End Source File
# Begin Source File

SOURCE=..\src\quote_fmt.h
# End Source File
# Begin Source File

SOURCE=..\src\quote_fmt_lex.h
# End Source File
# Begin Source File

SOURCE=..\src\quote_fmt_parse.h
# End Source File
# Begin Source File

SOURCE="..\src\common\quoted-printable.h"
# End Source File
# Begin Source File

SOURCE=..\src\recv.h
# End Source File
# Begin Source File

SOURCE=..\src\rfc2015.h
# End Source File
# Begin Source File

SOURCE="..\src\select-keys.h"
# End Source File
# Begin Source File

SOURCE=..\src\send.h
# End Source File
# Begin Source File

SOURCE=..\src\common\session.h
# End Source File
# Begin Source File

SOURCE=..\src\setup.h
# End Source File
# Begin Source File

SOURCE=..\src\sigstatus.h
# End Source File
# Begin Source File

SOURCE=..\src\common\smtp.h
# End Source File
# Begin Source File

SOURCE=..\src\common\socket.h
# End Source File
# Begin Source File

SOURCE=..\src\sourcewindow.h
# End Source File
# Begin Source File

SOURCE=..\src\common\ssl.h
# End Source File
# Begin Source File

SOURCE=..\src\common\ssl_certificate.h
# End Source File
# Begin Source File

SOURCE=..\src\ssl_manager.h
# End Source File
# Begin Source File

SOURCE=..\src\gtk\sslcertwindow.h
# End Source File
# Begin Source File

SOURCE=..\src\statusbar.h
# End Source File
# Begin Source File

SOURCE=..\src\stock_pixmap.h
# End Source File
# Begin Source File

SOURCE=..\src\common\stringtable.h
# End Source File
# Begin Source File

SOURCE=..\src\summary_search.h
# End Source File
# Begin Source File

SOURCE=..\src\summaryview.h
# End Source File
# Begin Source File

SOURCE=..\src\syldap.h
# End Source File
# Begin Source File

SOURCE=..\src\common\sylpheed.h
# End Source File
# Begin Source File

SOURCE=..\src\common\template.h
# End Source File
# Begin Source File

SOURCE=..\src\textview.h
# End Source File
# Begin Source File

SOURCE=..\src\toolbar.h
# End Source File
# Begin Source File

SOURCE=..\src\undo.h
# End Source File
# Begin Source File

SOURCE=..\src\unmime.h
# End Source File
# Begin Source File

SOURCE=..\src\common\utils.h
# End Source File
# Begin Source File

SOURCE=..\src\common\uuencode.h
# End Source File
# Begin Source File

SOURCE=..\src\vcard.h
# End Source File
# Begin Source File

SOURCE=..\src\version.h
# End Source File
# Begin Source File

SOURCE=..\src\w32_aspell_init.h
# End Source File
# Begin Source File

SOURCE=..\src\w32_mailcap.h
# End Source File
# Begin Source File

SOURCE=..\src\xml.h
# End Source File
# Begin Source File

SOURCE=..\src\xmlprops.h
# End Source File
# End Group
# Begin Group "Ressourcendateien"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=.\sylpheed.ico
# End Source File
# End Group
# Begin Source File

SOURCE="..\..\..\lib\glib-2.0.lib"
# End Source File
# Begin Source File

SOURCE="..\..\..\lib\gmodule-2.0.lib"
# End Source File
# Begin Source File

SOURCE=..\..\..\lib\gtk.lib
# End Source File
# Begin Source File

SOURCE=..\..\..\lib\gdk.lib
# End Source File
# Begin Source File

SOURCE=..\..\..\lib\iconv.lib
# End Source File
# Begin Source File

SOURCE=..\..\..\lib\intl.lib
# End Source File
# Begin Source File

SOURCE=..\..\w32lib\w32lib.lib
# End Source File
# Begin Source File

SOURCE=..\..\gpgme\gpgme.lib
# End Source File
# Begin Source File

SOURCE=..\..\libcompface\libcompface.lib
# End Source File
# Begin Source File

SOURCE=..\..\regex\regex.lib
# End Source File
# Begin Source File

SOURCE=..\..\fnmatch\fnmatch.lib
# End Source File
# Begin Source File

SOURCE=..\..\..\lib\libeay32.lib
# End Source File
# Begin Source File

SOURCE=..\..\..\lib\ssleay32.lib
# End Source File
# End Target
# End Project
