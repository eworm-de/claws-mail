# Microsoft Developer Studio Generated NMAKE File, Based on sylpheed.dsp
!IF "$(CFG)" == ""
CFG=sylpheed - Win32 Release
!MESSAGE Keine Konfiguration angegeben. sylpheed - Win32 Release wird als Standard verwendet.
!ENDIF 

!IF "$(CFG)" != "sylpheed - Win32 Release" && "$(CFG)" != "sylpheed - Win32 Debug"
!MESSAGE UngÅltige Konfiguration "$(CFG)" angegeben.
!MESSAGE Sie kînnen beim AusfÅhren von NMAKE eine Konfiguration angeben
!MESSAGE durch Definieren des Makros CFG in der Befehlszeile. Zum Beispiel:
!MESSAGE 
!MESSAGE NMAKE /f "sylpheed.mak" CFG="sylpheed - Win32 Release"
!MESSAGE 
!MESSAGE FÅr die Konfiguration stehen zur Auswahl:
!MESSAGE 
!MESSAGE "sylpheed - Win32 Release" (basierend auf  "Win32 (x86) Application")
!MESSAGE "sylpheed - Win32 Debug" (basierend auf  "Win32 (x86) Application")
!MESSAGE 
!ERROR Eine ungÅltige Konfiguration wurde angegeben.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

!IF  "$(CFG)" == "sylpheed - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

ALL : "$(OUTDIR)\sylpheed.exe"


CLEAN :
	-@erase "$(INTDIR)\about.obj"
	-@erase "$(INTDIR)\account.obj"
	-@erase "$(INTDIR)\addr_compl.obj"
	-@erase "$(INTDIR)\addrbook.obj"
	-@erase "$(INTDIR)\addrcache.obj"
	-@erase "$(INTDIR)\addrclip.obj"
	-@erase "$(INTDIR)\addressadd.obj"
	-@erase "$(INTDIR)\addressbook.obj"
	-@erase "$(INTDIR)\addrgather.obj"
	-@erase "$(INTDIR)\addrharvest.obj"
	-@erase "$(INTDIR)\addrindex.obj"
	-@erase "$(INTDIR)\addritem.obj"
	-@erase "$(INTDIR)\addrselect.obj"
	-@erase "$(INTDIR)\alertpanel.obj"
	-@erase "$(INTDIR)\automaton.obj"
	-@erase "$(INTDIR)\base64.obj"
	-@erase "$(INTDIR)\codeconv.obj"
	-@erase "$(INTDIR)\colorlabel.obj"
	-@erase "$(INTDIR)\compat.obj"
	-@erase "$(INTDIR)\compose.obj"
	-@erase "$(INTDIR)\conv.obj"
	-@erase "$(INTDIR)\customheader.obj"
	-@erase "$(INTDIR)\displayheader.obj"
	-@erase "$(INTDIR)\editaddress.obj"
	-@erase "$(INTDIR)\editbook.obj"
	-@erase "$(INTDIR)\editgroup.obj"
	-@erase "$(INTDIR)\editjpilot.obj"
	-@erase "$(INTDIR)\editldap.obj"
	-@erase "$(INTDIR)\editldap_basedn.obj"
	-@erase "$(INTDIR)\editvcard.obj"
	-@erase "$(INTDIR)\enriched.obj"
	-@erase "$(INTDIR)\exphtmldlg.obj"
	-@erase "$(INTDIR)\export.obj"
	-@erase "$(INTDIR)\exporthtml.obj"
	-@erase "$(INTDIR)\filesel.obj"
	-@erase "$(INTDIR)\filter.obj"
	-@erase "$(INTDIR)\filtering.obj"
	-@erase "$(INTDIR)\folder.obj"
	-@erase "$(INTDIR)\foldersel.obj"
	-@erase "$(INTDIR)\folderview.obj"
	-@erase "$(INTDIR)\grouplistdialog.obj"
	-@erase "$(INTDIR)\gtksctree.obj"
	-@erase "$(INTDIR)\gtkshruler.obj"
	-@erase "$(INTDIR)\gtkstext.obj"
	-@erase "$(INTDIR)\gtkutils.obj"
	-@erase "$(INTDIR)\headerview.obj"
	-@erase "$(INTDIR)\html.obj"
	-@erase "$(INTDIR)\imageview.obj"
	-@erase "$(INTDIR)\imap.obj"
	-@erase "$(INTDIR)\import.obj"
	-@erase "$(INTDIR)\importldif.obj"
	-@erase "$(INTDIR)\importmutt.obj"
	-@erase "$(INTDIR)\importpine.obj"
	-@erase "$(INTDIR)\inc.obj"
	-@erase "$(INTDIR)\info.obj"
	-@erase "$(INTDIR)\inputdialog.obj"
	-@erase "$(INTDIR)\jpilot.obj"
	-@erase "$(INTDIR)\ldif.obj"
	-@erase "$(INTDIR)\logwindow.obj"
	-@erase "$(INTDIR)\main.obj"
	-@erase "$(INTDIR)\mainwindow.obj"
	-@erase "$(INTDIR)\manage_window.obj"
	-@erase "$(INTDIR)\manual.obj"
	-@erase "$(INTDIR)\matcher.obj"
	-@erase "$(INTDIR)\matcher_parser_lex.obj"
	-@erase "$(INTDIR)\matcher_parser_parse.obj"
	-@erase "$(INTDIR)\mbox.obj"
	-@erase "$(INTDIR)\mbox_folder.obj"
	-@erase "$(INTDIR)\md5.obj"
	-@erase "$(INTDIR)\menu.obj"
	-@erase "$(INTDIR)\message_search.obj"
	-@erase "$(INTDIR)\messageview.obj"
	-@erase "$(INTDIR)\mgutils.obj"
	-@erase "$(INTDIR)\mh.obj"
	-@erase "$(INTDIR)\mimeview.obj"
	-@erase "$(INTDIR)\msgcache.obj"
	-@erase "$(INTDIR)\mutt.obj"
	-@erase "$(INTDIR)\news.obj"
	-@erase "$(INTDIR)\nntp.obj"
	-@erase "$(INTDIR)\passphrase.obj"
	-@erase "$(INTDIR)\pine.obj"
	-@erase "$(INTDIR)\pop.obj"
	-@erase "$(INTDIR)\prefs.obj"
	-@erase "$(INTDIR)\prefs_account.obj"
	-@erase "$(INTDIR)\prefs_actions.obj"
	-@erase "$(INTDIR)\prefs_common.obj"
	-@erase "$(INTDIR)\prefs_customheader.obj"
	-@erase "$(INTDIR)\prefs_display_header.obj"
	-@erase "$(INTDIR)\prefs_filter.obj"
	-@erase "$(INTDIR)\prefs_filtering.obj"
	-@erase "$(INTDIR)\prefs_folder_item.obj"
	-@erase "$(INTDIR)\prefs_matcher.obj"
	-@erase "$(INTDIR)\prefs_scoring.obj"
	-@erase "$(INTDIR)\prefs_summary_column.obj"
	-@erase "$(INTDIR)\prefs_template.obj"
	-@erase "$(INTDIR)\procheader.obj"
	-@erase "$(INTDIR)\procmime.obj"
	-@erase "$(INTDIR)\procmsg.obj"
	-@erase "$(INTDIR)\progressdialog.obj"
	-@erase "$(INTDIR)\quote_fmt.obj"
	-@erase "$(INTDIR)\quote_fmt_lex.obj"
	-@erase "$(INTDIR)\quote_fmt_parse.obj"
	-@erase "$(INTDIR)\recv.obj"
	-@erase "$(INTDIR)\rfc2015.obj"
	-@erase "$(INTDIR)\scoring.obj"
	-@erase "$(INTDIR)\select-keys.obj"
	-@erase "$(INTDIR)\selective_download.obj"
	-@erase "$(INTDIR)\send.obj"
	-@erase "$(INTDIR)\session.obj"
	-@erase "$(INTDIR)\setup.obj"
	-@erase "$(INTDIR)\sigstatus.obj"
	-@erase "$(INTDIR)\simple-gettext.obj"
	-@erase "$(INTDIR)\smtp.obj"
	-@erase "$(INTDIR)\socket.obj"
	-@erase "$(INTDIR)\sourcewindow.obj"
	-@erase "$(INTDIR)\ssl.obj"
	-@erase "$(INTDIR)\statusbar.obj"
	-@erase "$(INTDIR)\stock_pixmap.obj"
	-@erase "$(INTDIR)\string_match.obj"
	-@erase "$(INTDIR)\stringtable.obj"
	-@erase "$(INTDIR)\summary_search.obj"
	-@erase "$(INTDIR)\summaryview.obj"
	-@erase "$(INTDIR)\syldap.obj"
	-@erase "$(INTDIR)\sylpheed.res"
	-@erase "$(INTDIR)\template.obj"
	-@erase "$(INTDIR)\textview.obj"
	-@erase "$(INTDIR)\undo.obj"
	-@erase "$(INTDIR)\unmime.obj"
	-@erase "$(INTDIR)\utils.obj"
	-@erase "$(INTDIR)\uuencode.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vcard.obj"
	-@erase "$(INTDIR)\w32_mailcap.obj"
	-@erase "$(INTDIR)\xml.obj"
	-@erase "$(INTDIR)\xmlprops.obj"
	-@erase "$(OUTDIR)\sylpheed.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /ML /W3 /GX /O2 /I "..\win32" /I "\dev\include" /I "\dev\include\glib-2.0" /I "\dev\lib\glib-2.0\include" /I "\dev\include\gdk" /I "\dev\include\gtk" /I "\dev\lib\gtk+\include" /I "\dev\proj\fnmatch\src\posix" /I "\dev\proj\libcompface\src" /I "..\libjconv" /I "\dev\proj\regex\src" /I "\dev\proj\w32lib\src" /I "\dev\proj\gpgme\gpgme" /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "HAVE_CONFIG_H" /Fp"$(INTDIR)\sylpheed.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

MTL=midl.exe
MTL_PROJ=/nologo /D "NDEBUG" /mktyplib203 /win32 
RSC=rc.exe
RSC_PROJ=/l 0x411 /fo"$(INTDIR)\sylpheed.res" /d "NDEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\sylpheed.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /incremental:no /pdb:"$(OUTDIR)\sylpheed.pdb" /machine:I386 /out:"$(OUTDIR)\sylpheed.exe" 
LINK32_OBJS= \
	"$(INTDIR)\about.obj" \
	"$(INTDIR)\account.obj" \
	"$(INTDIR)\addr_compl.obj" \
	"$(INTDIR)\addrbook.obj" \
	"$(INTDIR)\addrcache.obj" \
	"$(INTDIR)\addrclip.obj" \
	"$(INTDIR)\addressadd.obj" \
	"$(INTDIR)\addressbook.obj" \
	"$(INTDIR)\addrgather.obj" \
	"$(INTDIR)\addrharvest.obj" \
	"$(INTDIR)\addrindex.obj" \
	"$(INTDIR)\addritem.obj" \
	"$(INTDIR)\addrselect.obj" \
	"$(INTDIR)\alertpanel.obj" \
	"$(INTDIR)\automaton.obj" \
	"$(INTDIR)\base64.obj" \
	"$(INTDIR)\codeconv.obj" \
	"$(INTDIR)\colorlabel.obj" \
	"$(INTDIR)\compat.obj" \
	"$(INTDIR)\compose.obj" \
	"$(INTDIR)\conv.obj" \
	"$(INTDIR)\customheader.obj" \
	"$(INTDIR)\displayheader.obj" \
	"$(INTDIR)\editaddress.obj" \
	"$(INTDIR)\editbook.obj" \
	"$(INTDIR)\editgroup.obj" \
	"$(INTDIR)\editjpilot.obj" \
	"$(INTDIR)\editldap.obj" \
	"$(INTDIR)\editldap_basedn.obj" \
	"$(INTDIR)\editvcard.obj" \
	"$(INTDIR)\enriched.obj" \
	"$(INTDIR)\exphtmldlg.obj" \
	"$(INTDIR)\export.obj" \
	"$(INTDIR)\exporthtml.obj" \
	"$(INTDIR)\filesel.obj" \
	"$(INTDIR)\filter.obj" \
	"$(INTDIR)\filtering.obj" \
	"$(INTDIR)\folder.obj" \
	"$(INTDIR)\foldersel.obj" \
	"$(INTDIR)\folderview.obj" \
	"$(INTDIR)\grouplistdialog.obj" \
	"$(INTDIR)\gtksctree.obj" \
	"$(INTDIR)\gtkshruler.obj" \
	"$(INTDIR)\gtkstext.obj" \
	"$(INTDIR)\gtkutils.obj" \
	"$(INTDIR)\headerview.obj" \
	"$(INTDIR)\html.obj" \
	"$(INTDIR)\imageview.obj" \
	"$(INTDIR)\imap.obj" \
	"$(INTDIR)\import.obj" \
	"$(INTDIR)\importldif.obj" \
	"$(INTDIR)\importmutt.obj" \
	"$(INTDIR)\importpine.obj" \
	"$(INTDIR)\inc.obj" \
	"$(INTDIR)\info.obj" \
	"$(INTDIR)\inputdialog.obj" \
	"$(INTDIR)\jpilot.obj" \
	"$(INTDIR)\ldif.obj" \
	"$(INTDIR)\logwindow.obj" \
	"$(INTDIR)\main.obj" \
	"$(INTDIR)\mainwindow.obj" \
	"$(INTDIR)\manage_window.obj" \
	"$(INTDIR)\manual.obj" \
	"$(INTDIR)\matcher.obj" \
	"$(INTDIR)\matcher_parser_lex.obj" \
	"$(INTDIR)\matcher_parser_parse.obj" \
	"$(INTDIR)\mbox.obj" \
	"$(INTDIR)\mbox_folder.obj" \
	"$(INTDIR)\md5.obj" \
	"$(INTDIR)\menu.obj" \
	"$(INTDIR)\message_search.obj" \
	"$(INTDIR)\messageview.obj" \
	"$(INTDIR)\mgutils.obj" \
	"$(INTDIR)\mh.obj" \
	"$(INTDIR)\mimeview.obj" \
	"$(INTDIR)\msgcache.obj" \
	"$(INTDIR)\mutt.obj" \
	"$(INTDIR)\news.obj" \
	"$(INTDIR)\nntp.obj" \
	"$(INTDIR)\passphrase.obj" \
	"$(INTDIR)\pine.obj" \
	"$(INTDIR)\pop.obj" \
	"$(INTDIR)\prefs.obj" \
	"$(INTDIR)\prefs_account.obj" \
	"$(INTDIR)\prefs_actions.obj" \
	"$(INTDIR)\prefs_common.obj" \
	"$(INTDIR)\prefs_customheader.obj" \
	"$(INTDIR)\prefs_display_header.obj" \
	"$(INTDIR)\prefs_filter.obj" \
	"$(INTDIR)\prefs_filtering.obj" \
	"$(INTDIR)\prefs_folder_item.obj" \
	"$(INTDIR)\prefs_matcher.obj" \
	"$(INTDIR)\prefs_scoring.obj" \
	"$(INTDIR)\prefs_summary_column.obj" \
	"$(INTDIR)\prefs_template.obj" \
	"$(INTDIR)\procheader.obj" \
	"$(INTDIR)\procmime.obj" \
	"$(INTDIR)\procmsg.obj" \
	"$(INTDIR)\progressdialog.obj" \
	"$(INTDIR)\quote_fmt.obj" \
	"$(INTDIR)\quote_fmt_lex.obj" \
	"$(INTDIR)\quote_fmt_parse.obj" \
	"$(INTDIR)\recv.obj" \
	"$(INTDIR)\rfc2015.obj" \
	"$(INTDIR)\scoring.obj" \
	"$(INTDIR)\select-keys.obj" \
	"$(INTDIR)\selective_download.obj" \
	"$(INTDIR)\send.obj" \
	"$(INTDIR)\session.obj" \
	"$(INTDIR)\setup.obj" \
	"$(INTDIR)\sigstatus.obj" \
	"$(INTDIR)\simple-gettext.obj" \
	"$(INTDIR)\smtp.obj" \
	"$(INTDIR)\socket.obj" \
	"$(INTDIR)\sourcewindow.obj" \
	"$(INTDIR)\ssl.obj" \
	"$(INTDIR)\statusbar.obj" \
	"$(INTDIR)\stock_pixmap.obj" \
	"$(INTDIR)\string_match.obj" \
	"$(INTDIR)\stringtable.obj" \
	"$(INTDIR)\summary_search.obj" \
	"$(INTDIR)\summaryview.obj" \
	"$(INTDIR)\syldap.obj" \
	"$(INTDIR)\template.obj" \
	"$(INTDIR)\textview.obj" \
	"$(INTDIR)\undo.obj" \
	"$(INTDIR)\unmime.obj" \
	"$(INTDIR)\utils.obj" \
	"$(INTDIR)\uuencode.obj" \
	"$(INTDIR)\vcard.obj" \
	"$(INTDIR)\w32_mailcap.obj" \
	"$(INTDIR)\xml.obj" \
	"$(INTDIR)\xmlprops.obj" \
	"$(INTDIR)\sylpheed.res" \
	"..\..\..\lib\glib-2.0.lib" \
	"..\..\..\lib\gtk.lib" \
	"..\..\..\lib\gdk.lib" \
	"..\..\..\lib\iconv.lib" \
	"..\..\..\lib\intl.lib" \
	"..\..\w32lib\w32lib.lib" \
	"..\..\gpgme\gpgme.lib" \
	"..\..\libcompface\libcompface.lib" \
	"..\..\regex\regex.lib" \
	"..\..\fnmatch\fnmatch.lib" \
	"..\..\..\lib\libeay32.lib" \
	"..\..\..\lib\ssleay32.lib"

"$(OUTDIR)\sylpheed.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

ALL : "$(OUTDIR)\sylpheed_d.exe" "$(OUTDIR)\sylpheed.bsc"


CLEAN :
	-@erase "$(INTDIR)\about.obj"
	-@erase "$(INTDIR)\about.sbr"
	-@erase "$(INTDIR)\account.obj"
	-@erase "$(INTDIR)\account.sbr"
	-@erase "$(INTDIR)\addr_compl.obj"
	-@erase "$(INTDIR)\addr_compl.sbr"
	-@erase "$(INTDIR)\addrbook.obj"
	-@erase "$(INTDIR)\addrbook.sbr"
	-@erase "$(INTDIR)\addrcache.obj"
	-@erase "$(INTDIR)\addrcache.sbr"
	-@erase "$(INTDIR)\addrclip.obj"
	-@erase "$(INTDIR)\addrclip.sbr"
	-@erase "$(INTDIR)\addressadd.obj"
	-@erase "$(INTDIR)\addressadd.sbr"
	-@erase "$(INTDIR)\addressbook.obj"
	-@erase "$(INTDIR)\addressbook.sbr"
	-@erase "$(INTDIR)\addrgather.obj"
	-@erase "$(INTDIR)\addrgather.sbr"
	-@erase "$(INTDIR)\addrharvest.obj"
	-@erase "$(INTDIR)\addrharvest.sbr"
	-@erase "$(INTDIR)\addrindex.obj"
	-@erase "$(INTDIR)\addrindex.sbr"
	-@erase "$(INTDIR)\addritem.obj"
	-@erase "$(INTDIR)\addritem.sbr"
	-@erase "$(INTDIR)\addrselect.obj"
	-@erase "$(INTDIR)\addrselect.sbr"
	-@erase "$(INTDIR)\alertpanel.obj"
	-@erase "$(INTDIR)\alertpanel.sbr"
	-@erase "$(INTDIR)\automaton.obj"
	-@erase "$(INTDIR)\automaton.sbr"
	-@erase "$(INTDIR)\base64.obj"
	-@erase "$(INTDIR)\base64.sbr"
	-@erase "$(INTDIR)\codeconv.obj"
	-@erase "$(INTDIR)\codeconv.sbr"
	-@erase "$(INTDIR)\colorlabel.obj"
	-@erase "$(INTDIR)\colorlabel.sbr"
	-@erase "$(INTDIR)\compat.obj"
	-@erase "$(INTDIR)\compat.sbr"
	-@erase "$(INTDIR)\compose.obj"
	-@erase "$(INTDIR)\compose.sbr"
	-@erase "$(INTDIR)\conv.obj"
	-@erase "$(INTDIR)\conv.sbr"
	-@erase "$(INTDIR)\customheader.obj"
	-@erase "$(INTDIR)\customheader.sbr"
	-@erase "$(INTDIR)\displayheader.obj"
	-@erase "$(INTDIR)\displayheader.sbr"
	-@erase "$(INTDIR)\editaddress.obj"
	-@erase "$(INTDIR)\editaddress.sbr"
	-@erase "$(INTDIR)\editbook.obj"
	-@erase "$(INTDIR)\editbook.sbr"
	-@erase "$(INTDIR)\editgroup.obj"
	-@erase "$(INTDIR)\editgroup.sbr"
	-@erase "$(INTDIR)\editjpilot.obj"
	-@erase "$(INTDIR)\editjpilot.sbr"
	-@erase "$(INTDIR)\editldap.obj"
	-@erase "$(INTDIR)\editldap.sbr"
	-@erase "$(INTDIR)\editldap_basedn.obj"
	-@erase "$(INTDIR)\editldap_basedn.sbr"
	-@erase "$(INTDIR)\editvcard.obj"
	-@erase "$(INTDIR)\editvcard.sbr"
	-@erase "$(INTDIR)\enriched.obj"
	-@erase "$(INTDIR)\enriched.sbr"
	-@erase "$(INTDIR)\exphtmldlg.obj"
	-@erase "$(INTDIR)\exphtmldlg.sbr"
	-@erase "$(INTDIR)\export.obj"
	-@erase "$(INTDIR)\export.sbr"
	-@erase "$(INTDIR)\exporthtml.obj"
	-@erase "$(INTDIR)\exporthtml.sbr"
	-@erase "$(INTDIR)\filesel.obj"
	-@erase "$(INTDIR)\filesel.sbr"
	-@erase "$(INTDIR)\filter.obj"
	-@erase "$(INTDIR)\filter.sbr"
	-@erase "$(INTDIR)\filtering.obj"
	-@erase "$(INTDIR)\filtering.sbr"
	-@erase "$(INTDIR)\folder.obj"
	-@erase "$(INTDIR)\folder.sbr"
	-@erase "$(INTDIR)\foldersel.obj"
	-@erase "$(INTDIR)\foldersel.sbr"
	-@erase "$(INTDIR)\folderview.obj"
	-@erase "$(INTDIR)\folderview.sbr"
	-@erase "$(INTDIR)\grouplistdialog.obj"
	-@erase "$(INTDIR)\grouplistdialog.sbr"
	-@erase "$(INTDIR)\gtksctree.obj"
	-@erase "$(INTDIR)\gtksctree.sbr"
	-@erase "$(INTDIR)\gtkshruler.obj"
	-@erase "$(INTDIR)\gtkshruler.sbr"
	-@erase "$(INTDIR)\gtkstext.obj"
	-@erase "$(INTDIR)\gtkstext.sbr"
	-@erase "$(INTDIR)\gtkutils.obj"
	-@erase "$(INTDIR)\gtkutils.sbr"
	-@erase "$(INTDIR)\headerview.obj"
	-@erase "$(INTDIR)\headerview.sbr"
	-@erase "$(INTDIR)\html.obj"
	-@erase "$(INTDIR)\html.sbr"
	-@erase "$(INTDIR)\imageview.obj"
	-@erase "$(INTDIR)\imageview.sbr"
	-@erase "$(INTDIR)\imap.obj"
	-@erase "$(INTDIR)\imap.sbr"
	-@erase "$(INTDIR)\import.obj"
	-@erase "$(INTDIR)\import.sbr"
	-@erase "$(INTDIR)\importldif.obj"
	-@erase "$(INTDIR)\importldif.sbr"
	-@erase "$(INTDIR)\importmutt.obj"
	-@erase "$(INTDIR)\importmutt.sbr"
	-@erase "$(INTDIR)\importpine.obj"
	-@erase "$(INTDIR)\importpine.sbr"
	-@erase "$(INTDIR)\inc.obj"
	-@erase "$(INTDIR)\inc.sbr"
	-@erase "$(INTDIR)\info.obj"
	-@erase "$(INTDIR)\info.sbr"
	-@erase "$(INTDIR)\inputdialog.obj"
	-@erase "$(INTDIR)\inputdialog.sbr"
	-@erase "$(INTDIR)\jpilot.obj"
	-@erase "$(INTDIR)\jpilot.sbr"
	-@erase "$(INTDIR)\ldif.obj"
	-@erase "$(INTDIR)\ldif.sbr"
	-@erase "$(INTDIR)\logwindow.obj"
	-@erase "$(INTDIR)\logwindow.sbr"
	-@erase "$(INTDIR)\main.obj"
	-@erase "$(INTDIR)\main.sbr"
	-@erase "$(INTDIR)\mainwindow.obj"
	-@erase "$(INTDIR)\mainwindow.sbr"
	-@erase "$(INTDIR)\manage_window.obj"
	-@erase "$(INTDIR)\manage_window.sbr"
	-@erase "$(INTDIR)\manual.obj"
	-@erase "$(INTDIR)\manual.sbr"
	-@erase "$(INTDIR)\matcher.obj"
	-@erase "$(INTDIR)\matcher.sbr"
	-@erase "$(INTDIR)\matcher_parser_lex.obj"
	-@erase "$(INTDIR)\matcher_parser_lex.sbr"
	-@erase "$(INTDIR)\matcher_parser_parse.obj"
	-@erase "$(INTDIR)\matcher_parser_parse.sbr"
	-@erase "$(INTDIR)\mbox.obj"
	-@erase "$(INTDIR)\mbox.sbr"
	-@erase "$(INTDIR)\mbox_folder.obj"
	-@erase "$(INTDIR)\mbox_folder.sbr"
	-@erase "$(INTDIR)\md5.obj"
	-@erase "$(INTDIR)\md5.sbr"
	-@erase "$(INTDIR)\menu.obj"
	-@erase "$(INTDIR)\menu.sbr"
	-@erase "$(INTDIR)\message_search.obj"
	-@erase "$(INTDIR)\message_search.sbr"
	-@erase "$(INTDIR)\messageview.obj"
	-@erase "$(INTDIR)\messageview.sbr"
	-@erase "$(INTDIR)\mgutils.obj"
	-@erase "$(INTDIR)\mgutils.sbr"
	-@erase "$(INTDIR)\mh.obj"
	-@erase "$(INTDIR)\mh.sbr"
	-@erase "$(INTDIR)\mimeview.obj"
	-@erase "$(INTDIR)\mimeview.sbr"
	-@erase "$(INTDIR)\msgcache.obj"
	-@erase "$(INTDIR)\msgcache.sbr"
	-@erase "$(INTDIR)\mutt.obj"
	-@erase "$(INTDIR)\mutt.sbr"
	-@erase "$(INTDIR)\news.obj"
	-@erase "$(INTDIR)\news.sbr"
	-@erase "$(INTDIR)\nntp.obj"
	-@erase "$(INTDIR)\nntp.sbr"
	-@erase "$(INTDIR)\passphrase.obj"
	-@erase "$(INTDIR)\passphrase.sbr"
	-@erase "$(INTDIR)\pine.obj"
	-@erase "$(INTDIR)\pine.sbr"
	-@erase "$(INTDIR)\pop.obj"
	-@erase "$(INTDIR)\pop.sbr"
	-@erase "$(INTDIR)\prefs.obj"
	-@erase "$(INTDIR)\prefs.sbr"
	-@erase "$(INTDIR)\prefs_account.obj"
	-@erase "$(INTDIR)\prefs_account.sbr"
	-@erase "$(INTDIR)\prefs_actions.obj"
	-@erase "$(INTDIR)\prefs_actions.sbr"
	-@erase "$(INTDIR)\prefs_common.obj"
	-@erase "$(INTDIR)\prefs_common.sbr"
	-@erase "$(INTDIR)\prefs_customheader.obj"
	-@erase "$(INTDIR)\prefs_customheader.sbr"
	-@erase "$(INTDIR)\prefs_display_header.obj"
	-@erase "$(INTDIR)\prefs_display_header.sbr"
	-@erase "$(INTDIR)\prefs_filter.obj"
	-@erase "$(INTDIR)\prefs_filter.sbr"
	-@erase "$(INTDIR)\prefs_filtering.obj"
	-@erase "$(INTDIR)\prefs_filtering.sbr"
	-@erase "$(INTDIR)\prefs_folder_item.obj"
	-@erase "$(INTDIR)\prefs_folder_item.sbr"
	-@erase "$(INTDIR)\prefs_matcher.obj"
	-@erase "$(INTDIR)\prefs_matcher.sbr"
	-@erase "$(INTDIR)\prefs_scoring.obj"
	-@erase "$(INTDIR)\prefs_scoring.sbr"
	-@erase "$(INTDIR)\prefs_summary_column.obj"
	-@erase "$(INTDIR)\prefs_summary_column.sbr"
	-@erase "$(INTDIR)\prefs_template.obj"
	-@erase "$(INTDIR)\prefs_template.sbr"
	-@erase "$(INTDIR)\procheader.obj"
	-@erase "$(INTDIR)\procheader.sbr"
	-@erase "$(INTDIR)\procmime.obj"
	-@erase "$(INTDIR)\procmime.sbr"
	-@erase "$(INTDIR)\procmsg.obj"
	-@erase "$(INTDIR)\procmsg.sbr"
	-@erase "$(INTDIR)\progressdialog.obj"
	-@erase "$(INTDIR)\progressdialog.sbr"
	-@erase "$(INTDIR)\quote_fmt.obj"
	-@erase "$(INTDIR)\quote_fmt.sbr"
	-@erase "$(INTDIR)\quote_fmt_lex.obj"
	-@erase "$(INTDIR)\quote_fmt_lex.sbr"
	-@erase "$(INTDIR)\quote_fmt_parse.obj"
	-@erase "$(INTDIR)\quote_fmt_parse.sbr"
	-@erase "$(INTDIR)\recv.obj"
	-@erase "$(INTDIR)\recv.sbr"
	-@erase "$(INTDIR)\rfc2015.obj"
	-@erase "$(INTDIR)\rfc2015.sbr"
	-@erase "$(INTDIR)\scoring.obj"
	-@erase "$(INTDIR)\scoring.sbr"
	-@erase "$(INTDIR)\select-keys.obj"
	-@erase "$(INTDIR)\select-keys.sbr"
	-@erase "$(INTDIR)\selective_download.obj"
	-@erase "$(INTDIR)\selective_download.sbr"
	-@erase "$(INTDIR)\send.obj"
	-@erase "$(INTDIR)\send.sbr"
	-@erase "$(INTDIR)\session.obj"
	-@erase "$(INTDIR)\session.sbr"
	-@erase "$(INTDIR)\setup.obj"
	-@erase "$(INTDIR)\setup.sbr"
	-@erase "$(INTDIR)\sigstatus.obj"
	-@erase "$(INTDIR)\sigstatus.sbr"
	-@erase "$(INTDIR)\simple-gettext.obj"
	-@erase "$(INTDIR)\simple-gettext.sbr"
	-@erase "$(INTDIR)\smtp.obj"
	-@erase "$(INTDIR)\smtp.sbr"
	-@erase "$(INTDIR)\socket.obj"
	-@erase "$(INTDIR)\socket.sbr"
	-@erase "$(INTDIR)\sourcewindow.obj"
	-@erase "$(INTDIR)\sourcewindow.sbr"
	-@erase "$(INTDIR)\ssl.obj"
	-@erase "$(INTDIR)\ssl.sbr"
	-@erase "$(INTDIR)\statusbar.obj"
	-@erase "$(INTDIR)\statusbar.sbr"
	-@erase "$(INTDIR)\stock_pixmap.obj"
	-@erase "$(INTDIR)\stock_pixmap.sbr"
	-@erase "$(INTDIR)\string_match.obj"
	-@erase "$(INTDIR)\string_match.sbr"
	-@erase "$(INTDIR)\stringtable.obj"
	-@erase "$(INTDIR)\stringtable.sbr"
	-@erase "$(INTDIR)\summary_search.obj"
	-@erase "$(INTDIR)\summary_search.sbr"
	-@erase "$(INTDIR)\summaryview.obj"
	-@erase "$(INTDIR)\summaryview.sbr"
	-@erase "$(INTDIR)\syldap.obj"
	-@erase "$(INTDIR)\syldap.sbr"
	-@erase "$(INTDIR)\sylpheed.res"
	-@erase "$(INTDIR)\template.obj"
	-@erase "$(INTDIR)\template.sbr"
	-@erase "$(INTDIR)\textview.obj"
	-@erase "$(INTDIR)\textview.sbr"
	-@erase "$(INTDIR)\undo.obj"
	-@erase "$(INTDIR)\undo.sbr"
	-@erase "$(INTDIR)\unmime.obj"
	-@erase "$(INTDIR)\unmime.sbr"
	-@erase "$(INTDIR)\utils.obj"
	-@erase "$(INTDIR)\utils.sbr"
	-@erase "$(INTDIR)\uuencode.obj"
	-@erase "$(INTDIR)\uuencode.sbr"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(INTDIR)\vcard.obj"
	-@erase "$(INTDIR)\vcard.sbr"
	-@erase "$(INTDIR)\w32_mailcap.obj"
	-@erase "$(INTDIR)\w32_mailcap.sbr"
	-@erase "$(INTDIR)\xml.obj"
	-@erase "$(INTDIR)\xml.sbr"
	-@erase "$(INTDIR)\xmlprops.obj"
	-@erase "$(INTDIR)\xmlprops.sbr"
	-@erase "$(OUTDIR)\sylpheed.bsc"
	-@erase "$(OUTDIR)\sylpheed_d.exe"
	-@erase "$(OUTDIR)\sylpheed_d.ilk"
	-@erase "$(OUTDIR)\sylpheed_d.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP=cl.exe
CPP_PROJ=/nologo /MLd /W3 /Gm /GX /ZI /Od /I "..\win32" /I "\dev\include" /I "\dev\include\glib-2.0" /I "\dev\lib\glib-2.0\include" /I "\dev\include\gdk" /I "\dev\include\gtk" /I "\dev\lib\gtk+\include" /I "\dev\proj\fnmatch\src\posix" /I "\dev\proj\libcompface\src" /I "..\libjconv" /I "\dev\proj\regex\src" /I "\dev\proj\w32lib\src" /I "\dev\proj\gpgme\gpgme" /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "_MBCS" /D "HAVE_CONFIG_H" /FR"$(INTDIR)\\" /Fp"$(INTDIR)\sylpheed.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

MTL=midl.exe
MTL_PROJ=/nologo /D "_DEBUG" /mktyplib203 /win32 
RSC=rc.exe
RSC_PROJ=/l 0x411 /fo"$(INTDIR)\sylpheed.res" /d "_DEBUG" 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\sylpheed.bsc" 
BSC32_SBRS= \
	"$(INTDIR)\about.sbr" \
	"$(INTDIR)\account.sbr" \
	"$(INTDIR)\addr_compl.sbr" \
	"$(INTDIR)\addrbook.sbr" \
	"$(INTDIR)\addrcache.sbr" \
	"$(INTDIR)\addrclip.sbr" \
	"$(INTDIR)\addressadd.sbr" \
	"$(INTDIR)\addressbook.sbr" \
	"$(INTDIR)\addrgather.sbr" \
	"$(INTDIR)\addrharvest.sbr" \
	"$(INTDIR)\addrindex.sbr" \
	"$(INTDIR)\addritem.sbr" \
	"$(INTDIR)\addrselect.sbr" \
	"$(INTDIR)\alertpanel.sbr" \
	"$(INTDIR)\automaton.sbr" \
	"$(INTDIR)\base64.sbr" \
	"$(INTDIR)\codeconv.sbr" \
	"$(INTDIR)\colorlabel.sbr" \
	"$(INTDIR)\compat.sbr" \
	"$(INTDIR)\compose.sbr" \
	"$(INTDIR)\conv.sbr" \
	"$(INTDIR)\customheader.sbr" \
	"$(INTDIR)\displayheader.sbr" \
	"$(INTDIR)\editaddress.sbr" \
	"$(INTDIR)\editbook.sbr" \
	"$(INTDIR)\editgroup.sbr" \
	"$(INTDIR)\editjpilot.sbr" \
	"$(INTDIR)\editldap.sbr" \
	"$(INTDIR)\editldap_basedn.sbr" \
	"$(INTDIR)\editvcard.sbr" \
	"$(INTDIR)\enriched.sbr" \
	"$(INTDIR)\exphtmldlg.sbr" \
	"$(INTDIR)\export.sbr" \
	"$(INTDIR)\exporthtml.sbr" \
	"$(INTDIR)\filesel.sbr" \
	"$(INTDIR)\filter.sbr" \
	"$(INTDIR)\filtering.sbr" \
	"$(INTDIR)\folder.sbr" \
	"$(INTDIR)\foldersel.sbr" \
	"$(INTDIR)\folderview.sbr" \
	"$(INTDIR)\grouplistdialog.sbr" \
	"$(INTDIR)\gtksctree.sbr" \
	"$(INTDIR)\gtkshruler.sbr" \
	"$(INTDIR)\gtkstext.sbr" \
	"$(INTDIR)\gtkutils.sbr" \
	"$(INTDIR)\headerview.sbr" \
	"$(INTDIR)\html.sbr" \
	"$(INTDIR)\imageview.sbr" \
	"$(INTDIR)\imap.sbr" \
	"$(INTDIR)\import.sbr" \
	"$(INTDIR)\importldif.sbr" \
	"$(INTDIR)\importmutt.sbr" \
	"$(INTDIR)\importpine.sbr" \
	"$(INTDIR)\inc.sbr" \
	"$(INTDIR)\info.sbr" \
	"$(INTDIR)\inputdialog.sbr" \
	"$(INTDIR)\jpilot.sbr" \
	"$(INTDIR)\ldif.sbr" \
	"$(INTDIR)\logwindow.sbr" \
	"$(INTDIR)\main.sbr" \
	"$(INTDIR)\mainwindow.sbr" \
	"$(INTDIR)\manage_window.sbr" \
	"$(INTDIR)\manual.sbr" \
	"$(INTDIR)\matcher.sbr" \
	"$(INTDIR)\matcher_parser_lex.sbr" \
	"$(INTDIR)\matcher_parser_parse.sbr" \
	"$(INTDIR)\mbox.sbr" \
	"$(INTDIR)\mbox_folder.sbr" \
	"$(INTDIR)\md5.sbr" \
	"$(INTDIR)\menu.sbr" \
	"$(INTDIR)\message_search.sbr" \
	"$(INTDIR)\messageview.sbr" \
	"$(INTDIR)\mgutils.sbr" \
	"$(INTDIR)\mh.sbr" \
	"$(INTDIR)\mimeview.sbr" \
	"$(INTDIR)\msgcache.sbr" \
	"$(INTDIR)\mutt.sbr" \
	"$(INTDIR)\news.sbr" \
	"$(INTDIR)\nntp.sbr" \
	"$(INTDIR)\passphrase.sbr" \
	"$(INTDIR)\pine.sbr" \
	"$(INTDIR)\pop.sbr" \
	"$(INTDIR)\prefs.sbr" \
	"$(INTDIR)\prefs_account.sbr" \
	"$(INTDIR)\prefs_actions.sbr" \
	"$(INTDIR)\prefs_common.sbr" \
	"$(INTDIR)\prefs_customheader.sbr" \
	"$(INTDIR)\prefs_display_header.sbr" \
	"$(INTDIR)\prefs_filter.sbr" \
	"$(INTDIR)\prefs_filtering.sbr" \
	"$(INTDIR)\prefs_folder_item.sbr" \
	"$(INTDIR)\prefs_matcher.sbr" \
	"$(INTDIR)\prefs_scoring.sbr" \
	"$(INTDIR)\prefs_summary_column.sbr" \
	"$(INTDIR)\prefs_template.sbr" \
	"$(INTDIR)\procheader.sbr" \
	"$(INTDIR)\procmime.sbr" \
	"$(INTDIR)\procmsg.sbr" \
	"$(INTDIR)\progressdialog.sbr" \
	"$(INTDIR)\quote_fmt.sbr" \
	"$(INTDIR)\quote_fmt_lex.sbr" \
	"$(INTDIR)\quote_fmt_parse.sbr" \
	"$(INTDIR)\recv.sbr" \
	"$(INTDIR)\rfc2015.sbr" \
	"$(INTDIR)\scoring.sbr" \
	"$(INTDIR)\select-keys.sbr" \
	"$(INTDIR)\selective_download.sbr" \
	"$(INTDIR)\send.sbr" \
	"$(INTDIR)\session.sbr" \
	"$(INTDIR)\setup.sbr" \
	"$(INTDIR)\sigstatus.sbr" \
	"$(INTDIR)\simple-gettext.sbr" \
	"$(INTDIR)\smtp.sbr" \
	"$(INTDIR)\socket.sbr" \
	"$(INTDIR)\sourcewindow.sbr" \
	"$(INTDIR)\ssl.sbr" \
	"$(INTDIR)\statusbar.sbr" \
	"$(INTDIR)\stock_pixmap.sbr" \
	"$(INTDIR)\string_match.sbr" \
	"$(INTDIR)\stringtable.sbr" \
	"$(INTDIR)\summary_search.sbr" \
	"$(INTDIR)\summaryview.sbr" \
	"$(INTDIR)\syldap.sbr" \
	"$(INTDIR)\template.sbr" \
	"$(INTDIR)\textview.sbr" \
	"$(INTDIR)\undo.sbr" \
	"$(INTDIR)\unmime.sbr" \
	"$(INTDIR)\utils.sbr" \
	"$(INTDIR)\uuencode.sbr" \
	"$(INTDIR)\vcard.sbr" \
	"$(INTDIR)\w32_mailcap.sbr" \
	"$(INTDIR)\xml.sbr" \
	"$(INTDIR)\xmlprops.sbr"

"$(OUTDIR)\sylpheed.bsc" : "$(OUTDIR)" $(BSC32_SBRS)
    $(BSC32) @<<
  $(BSC32_FLAGS) $(BSC32_SBRS)
<<

LINK32=link.exe
LINK32_FLAGS=wsock32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:windows /incremental:yes /pdb:"$(OUTDIR)\sylpheed_d.pdb" /debug /machine:I386 /out:"$(OUTDIR)\sylpheed_d.exe" /pdbtype:sept 
LINK32_OBJS= \
	"$(INTDIR)\about.obj" \
	"$(INTDIR)\account.obj" \
	"$(INTDIR)\addr_compl.obj" \
	"$(INTDIR)\addrbook.obj" \
	"$(INTDIR)\addrcache.obj" \
	"$(INTDIR)\addrclip.obj" \
	"$(INTDIR)\addressadd.obj" \
	"$(INTDIR)\addressbook.obj" \
	"$(INTDIR)\addrgather.obj" \
	"$(INTDIR)\addrharvest.obj" \
	"$(INTDIR)\addrindex.obj" \
	"$(INTDIR)\addritem.obj" \
	"$(INTDIR)\addrselect.obj" \
	"$(INTDIR)\alertpanel.obj" \
	"$(INTDIR)\automaton.obj" \
	"$(INTDIR)\base64.obj" \
	"$(INTDIR)\codeconv.obj" \
	"$(INTDIR)\colorlabel.obj" \
	"$(INTDIR)\compat.obj" \
	"$(INTDIR)\compose.obj" \
	"$(INTDIR)\conv.obj" \
	"$(INTDIR)\customheader.obj" \
	"$(INTDIR)\displayheader.obj" \
	"$(INTDIR)\editaddress.obj" \
	"$(INTDIR)\editbook.obj" \
	"$(INTDIR)\editgroup.obj" \
	"$(INTDIR)\editjpilot.obj" \
	"$(INTDIR)\editldap.obj" \
	"$(INTDIR)\editldap_basedn.obj" \
	"$(INTDIR)\editvcard.obj" \
	"$(INTDIR)\enriched.obj" \
	"$(INTDIR)\exphtmldlg.obj" \
	"$(INTDIR)\export.obj" \
	"$(INTDIR)\exporthtml.obj" \
	"$(INTDIR)\filesel.obj" \
	"$(INTDIR)\filter.obj" \
	"$(INTDIR)\filtering.obj" \
	"$(INTDIR)\folder.obj" \
	"$(INTDIR)\foldersel.obj" \
	"$(INTDIR)\folderview.obj" \
	"$(INTDIR)\grouplistdialog.obj" \
	"$(INTDIR)\gtksctree.obj" \
	"$(INTDIR)\gtkshruler.obj" \
	"$(INTDIR)\gtkstext.obj" \
	"$(INTDIR)\gtkutils.obj" \
	"$(INTDIR)\headerview.obj" \
	"$(INTDIR)\html.obj" \
	"$(INTDIR)\imageview.obj" \
	"$(INTDIR)\imap.obj" \
	"$(INTDIR)\import.obj" \
	"$(INTDIR)\importldif.obj" \
	"$(INTDIR)\importmutt.obj" \
	"$(INTDIR)\importpine.obj" \
	"$(INTDIR)\inc.obj" \
	"$(INTDIR)\info.obj" \
	"$(INTDIR)\inputdialog.obj" \
	"$(INTDIR)\jpilot.obj" \
	"$(INTDIR)\ldif.obj" \
	"$(INTDIR)\logwindow.obj" \
	"$(INTDIR)\main.obj" \
	"$(INTDIR)\mainwindow.obj" \
	"$(INTDIR)\manage_window.obj" \
	"$(INTDIR)\manual.obj" \
	"$(INTDIR)\matcher.obj" \
	"$(INTDIR)\matcher_parser_lex.obj" \
	"$(INTDIR)\matcher_parser_parse.obj" \
	"$(INTDIR)\mbox.obj" \
	"$(INTDIR)\mbox_folder.obj" \
	"$(INTDIR)\md5.obj" \
	"$(INTDIR)\menu.obj" \
	"$(INTDIR)\message_search.obj" \
	"$(INTDIR)\messageview.obj" \
	"$(INTDIR)\mgutils.obj" \
	"$(INTDIR)\mh.obj" \
	"$(INTDIR)\mimeview.obj" \
	"$(INTDIR)\msgcache.obj" \
	"$(INTDIR)\mutt.obj" \
	"$(INTDIR)\news.obj" \
	"$(INTDIR)\nntp.obj" \
	"$(INTDIR)\passphrase.obj" \
	"$(INTDIR)\pine.obj" \
	"$(INTDIR)\pop.obj" \
	"$(INTDIR)\prefs.obj" \
	"$(INTDIR)\prefs_account.obj" \
	"$(INTDIR)\prefs_actions.obj" \
	"$(INTDIR)\prefs_common.obj" \
	"$(INTDIR)\prefs_customheader.obj" \
	"$(INTDIR)\prefs_display_header.obj" \
	"$(INTDIR)\prefs_filter.obj" \
	"$(INTDIR)\prefs_filtering.obj" \
	"$(INTDIR)\prefs_folder_item.obj" \
	"$(INTDIR)\prefs_matcher.obj" \
	"$(INTDIR)\prefs_scoring.obj" \
	"$(INTDIR)\prefs_summary_column.obj" \
	"$(INTDIR)\prefs_template.obj" \
	"$(INTDIR)\procheader.obj" \
	"$(INTDIR)\procmime.obj" \
	"$(INTDIR)\procmsg.obj" \
	"$(INTDIR)\progressdialog.obj" \
	"$(INTDIR)\quote_fmt.obj" \
	"$(INTDIR)\quote_fmt_lex.obj" \
	"$(INTDIR)\quote_fmt_parse.obj" \
	"$(INTDIR)\recv.obj" \
	"$(INTDIR)\rfc2015.obj" \
	"$(INTDIR)\scoring.obj" \
	"$(INTDIR)\select-keys.obj" \
	"$(INTDIR)\selective_download.obj" \
	"$(INTDIR)\send.obj" \
	"$(INTDIR)\session.obj" \
	"$(INTDIR)\setup.obj" \
	"$(INTDIR)\sigstatus.obj" \
	"$(INTDIR)\simple-gettext.obj" \
	"$(INTDIR)\smtp.obj" \
	"$(INTDIR)\socket.obj" \
	"$(INTDIR)\sourcewindow.obj" \
	"$(INTDIR)\ssl.obj" \
	"$(INTDIR)\statusbar.obj" \
	"$(INTDIR)\stock_pixmap.obj" \
	"$(INTDIR)\string_match.obj" \
	"$(INTDIR)\stringtable.obj" \
	"$(INTDIR)\summary_search.obj" \
	"$(INTDIR)\summaryview.obj" \
	"$(INTDIR)\syldap.obj" \
	"$(INTDIR)\template.obj" \
	"$(INTDIR)\textview.obj" \
	"$(INTDIR)\undo.obj" \
	"$(INTDIR)\unmime.obj" \
	"$(INTDIR)\utils.obj" \
	"$(INTDIR)\uuencode.obj" \
	"$(INTDIR)\vcard.obj" \
	"$(INTDIR)\w32_mailcap.obj" \
	"$(INTDIR)\xml.obj" \
	"$(INTDIR)\xmlprops.obj" \
	"$(INTDIR)\sylpheed.res" \
	"..\..\..\lib\glib-2.0.lib" \
	"..\..\..\lib\gtk.lib" \
	"..\..\..\lib\gdk.lib" \
	"..\..\..\lib\iconv.lib" \
	"..\..\..\lib\intl.lib" \
	"..\..\w32lib\w32lib.lib" \
	"..\..\gpgme\gpgme.lib" \
	"..\..\libcompface\libcompface.lib" \
	"..\..\regex\regex.lib" \
	"..\..\fnmatch\fnmatch.lib" \
	"..\..\..\lib\libeay32.lib" \
	"..\..\..\lib\ssleay32.lib"

"$(OUTDIR)\sylpheed_d.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("sylpheed.dep")
!INCLUDE "sylpheed.dep"
!ELSE 
!MESSAGE Warning: cannot find "sylpheed.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "sylpheed - Win32 Release" || "$(CFG)" == "sylpheed - Win32 Debug"
SOURCE=..\src\about.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\about.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\about.obj"	"$(INTDIR)\about.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\account.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\account.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\account.obj"	"$(INTDIR)\account.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\addr_compl.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\addr_compl.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\addr_compl.obj"	"$(INTDIR)\addr_compl.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\addrbook.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\addrbook.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\addrbook.obj"	"$(INTDIR)\addrbook.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\addrcache.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\addrcache.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\addrcache.obj"	"$(INTDIR)\addrcache.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\addrclip.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\addrclip.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\addrclip.obj"	"$(INTDIR)\addrclip.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\addressadd.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\addressadd.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\addressadd.obj"	"$(INTDIR)\addressadd.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\addressbook.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\addressbook.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\addressbook.obj"	"$(INTDIR)\addressbook.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\addrgather.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\addrgather.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\addrgather.obj"	"$(INTDIR)\addrgather.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\addrharvest.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\addrharvest.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\addrharvest.obj"	"$(INTDIR)\addrharvest.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\addrindex.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\addrindex.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\addrindex.obj"	"$(INTDIR)\addrindex.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\addritem.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\addritem.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\addritem.obj"	"$(INTDIR)\addritem.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\addrselect.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\addrselect.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\addrselect.obj"	"$(INTDIR)\addrselect.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\alertpanel.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\alertpanel.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\alertpanel.obj"	"$(INTDIR)\alertpanel.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\automaton.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\automaton.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\automaton.obj"	"$(INTDIR)\automaton.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\base64.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\base64.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\base64.obj"	"$(INTDIR)\base64.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\codeconv.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\codeconv.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\codeconv.obj"	"$(INTDIR)\codeconv.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\colorlabel.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\colorlabel.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\colorlabel.obj"	"$(INTDIR)\colorlabel.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\libjconv\compat.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\compat.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\compat.obj"	"$(INTDIR)\compat.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\compose.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\compose.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\compose.obj"	"$(INTDIR)\compose.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\libjconv\conv.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\conv.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\conv.obj"	"$(INTDIR)\conv.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\customheader.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\customheader.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\customheader.obj"	"$(INTDIR)\customheader.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\displayheader.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\displayheader.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\displayheader.obj"	"$(INTDIR)\displayheader.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\editaddress.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\editaddress.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\editaddress.obj"	"$(INTDIR)\editaddress.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\editbook.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\editbook.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\editbook.obj"	"$(INTDIR)\editbook.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\editgroup.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\editgroup.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\editgroup.obj"	"$(INTDIR)\editgroup.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\editjpilot.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\editjpilot.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\editjpilot.obj"	"$(INTDIR)\editjpilot.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\editldap.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\editldap.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\editldap.obj"	"$(INTDIR)\editldap.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\editldap_basedn.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\editldap_basedn.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\editldap_basedn.obj"	"$(INTDIR)\editldap_basedn.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\editvcard.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\editvcard.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\editvcard.obj"	"$(INTDIR)\editvcard.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\enriched.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\enriched.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\enriched.obj"	"$(INTDIR)\enriched.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\exphtmldlg.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\exphtmldlg.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\exphtmldlg.obj"	"$(INTDIR)\exphtmldlg.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\export.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\export.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\export.obj"	"$(INTDIR)\export.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\exporthtml.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\exporthtml.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\exporthtml.obj"	"$(INTDIR)\exporthtml.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\filesel.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\filesel.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\filesel.obj"	"$(INTDIR)\filesel.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\filter.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\filter.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\filter.obj"	"$(INTDIR)\filter.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\filtering.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\filtering.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\filtering.obj"	"$(INTDIR)\filtering.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\folder.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\folder.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\folder.obj"	"$(INTDIR)\folder.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\foldersel.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\foldersel.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\foldersel.obj"	"$(INTDIR)\foldersel.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\folderview.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\folderview.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\folderview.obj"	"$(INTDIR)\folderview.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\grouplistdialog.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\grouplistdialog.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\grouplistdialog.obj"	"$(INTDIR)\grouplistdialog.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\gtksctree.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\gtksctree.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\gtksctree.obj"	"$(INTDIR)\gtksctree.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\gtkshruler.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\gtkshruler.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\gtkshruler.obj"	"$(INTDIR)\gtkshruler.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\gtkstext.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\gtkstext.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\gtkstext.obj"	"$(INTDIR)\gtkstext.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\gtkutils.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\gtkutils.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\gtkutils.obj"	"$(INTDIR)\gtkutils.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\headerview.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\headerview.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\headerview.obj"	"$(INTDIR)\headerview.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\html.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\html.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\html.obj"	"$(INTDIR)\html.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\imageview.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\imageview.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\imageview.obj"	"$(INTDIR)\imageview.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\imap.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\imap.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\imap.obj"	"$(INTDIR)\imap.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\import.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\import.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\import.obj"	"$(INTDIR)\import.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\importldif.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\importldif.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\importldif.obj"	"$(INTDIR)\importldif.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\importmutt.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\importmutt.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\importmutt.obj"	"$(INTDIR)\importmutt.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\importpine.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\importpine.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\importpine.obj"	"$(INTDIR)\importpine.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\inc.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\inc.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\inc.obj"	"$(INTDIR)\inc.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\libjconv\info.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\info.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\info.obj"	"$(INTDIR)\info.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\inputdialog.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\inputdialog.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\inputdialog.obj"	"$(INTDIR)\inputdialog.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\jpilot.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\jpilot.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\jpilot.obj"	"$(INTDIR)\jpilot.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\ldif.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\ldif.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\ldif.obj"	"$(INTDIR)\ldif.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\logwindow.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\logwindow.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\logwindow.obj"	"$(INTDIR)\logwindow.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\main.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\main.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\main.obj"	"$(INTDIR)\main.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\mainwindow.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\mainwindow.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\mainwindow.obj"	"$(INTDIR)\mainwindow.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\manage_window.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\manage_window.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\manage_window.obj"	"$(INTDIR)\manage_window.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\manual.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\manual.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\manual.obj"	"$(INTDIR)\manual.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\matcher.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\matcher.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\matcher.obj"	"$(INTDIR)\matcher.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\matcher_parser_lex.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\matcher_parser_lex.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\matcher_parser_lex.obj"	"$(INTDIR)\matcher_parser_lex.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\matcher_parser_parse.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\matcher_parser_parse.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\matcher_parser_parse.obj"	"$(INTDIR)\matcher_parser_parse.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\mbox.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\mbox.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\mbox.obj"	"$(INTDIR)\mbox.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\mbox_folder.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\mbox_folder.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\mbox_folder.obj"	"$(INTDIR)\mbox_folder.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\md5.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\md5.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\md5.obj"	"$(INTDIR)\md5.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\menu.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\menu.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\menu.obj"	"$(INTDIR)\menu.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\message_search.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\message_search.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\message_search.obj"	"$(INTDIR)\message_search.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\messageview.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\messageview.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\messageview.obj"	"$(INTDIR)\messageview.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\mgutils.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\mgutils.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\mgutils.obj"	"$(INTDIR)\mgutils.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\mh.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\mh.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\mh.obj"	"$(INTDIR)\mh.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\mimeview.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\mimeview.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\mimeview.obj"	"$(INTDIR)\mimeview.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\msgcache.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\msgcache.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\msgcache.obj"	"$(INTDIR)\msgcache.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\mutt.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\mutt.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\mutt.obj"	"$(INTDIR)\mutt.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\news.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\news.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\news.obj"	"$(INTDIR)\news.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\nntp.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\nntp.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\nntp.obj"	"$(INTDIR)\nntp.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\passphrase.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\passphrase.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\passphrase.obj"	"$(INTDIR)\passphrase.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\pine.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\pine.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\pine.obj"	"$(INTDIR)\pine.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\pop.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\pop.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\pop.obj"	"$(INTDIR)\pop.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\prefs.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\prefs.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\prefs.obj"	"$(INTDIR)\prefs.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\prefs_account.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\prefs_account.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\prefs_account.obj"	"$(INTDIR)\prefs_account.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\prefs_actions.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\prefs_actions.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\prefs_actions.obj"	"$(INTDIR)\prefs_actions.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\prefs_common.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\prefs_common.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\prefs_common.obj"	"$(INTDIR)\prefs_common.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\prefs_customheader.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\prefs_customheader.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\prefs_customheader.obj"	"$(INTDIR)\prefs_customheader.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\prefs_display_header.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\prefs_display_header.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\prefs_display_header.obj"	"$(INTDIR)\prefs_display_header.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\prefs_filter.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\prefs_filter.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\prefs_filter.obj"	"$(INTDIR)\prefs_filter.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\prefs_filtering.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\prefs_filtering.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\prefs_filtering.obj"	"$(INTDIR)\prefs_filtering.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\prefs_folder_item.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\prefs_folder_item.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\prefs_folder_item.obj"	"$(INTDIR)\prefs_folder_item.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\prefs_matcher.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\prefs_matcher.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\prefs_matcher.obj"	"$(INTDIR)\prefs_matcher.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\prefs_scoring.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\prefs_scoring.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\prefs_scoring.obj"	"$(INTDIR)\prefs_scoring.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\prefs_summary_column.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\prefs_summary_column.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\prefs_summary_column.obj"	"$(INTDIR)\prefs_summary_column.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\prefs_template.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\prefs_template.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\prefs_template.obj"	"$(INTDIR)\prefs_template.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\procheader.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\procheader.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\procheader.obj"	"$(INTDIR)\procheader.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\procmime.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\procmime.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\procmime.obj"	"$(INTDIR)\procmime.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\procmsg.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\procmsg.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\procmsg.obj"	"$(INTDIR)\procmsg.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\progressdialog.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\progressdialog.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\progressdialog.obj"	"$(INTDIR)\progressdialog.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\quote_fmt.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\quote_fmt.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\quote_fmt.obj"	"$(INTDIR)\quote_fmt.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\quote_fmt_lex.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\quote_fmt_lex.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\quote_fmt_lex.obj"	"$(INTDIR)\quote_fmt_lex.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\quote_fmt_parse.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\quote_fmt_parse.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\quote_fmt_parse.obj"	"$(INTDIR)\quote_fmt_parse.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\recv.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\recv.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\recv.obj"	"$(INTDIR)\recv.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\rfc2015.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\rfc2015.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\rfc2015.obj"	"$(INTDIR)\rfc2015.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\scoring.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\scoring.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\scoring.obj"	"$(INTDIR)\scoring.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\select-keys.c"

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\select-keys.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\select-keys.obj"	"$(INTDIR)\select-keys.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\selective_download.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\selective_download.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\selective_download.obj"	"$(INTDIR)\selective_download.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\send.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\send.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\send.obj"	"$(INTDIR)\send.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\session.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\session.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\session.obj"	"$(INTDIR)\session.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\setup.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\setup.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\setup.obj"	"$(INTDIR)\setup.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\sigstatus.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\sigstatus.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\sigstatus.obj"	"$(INTDIR)\sigstatus.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE="..\src\simple-gettext.c"

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\simple-gettext.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\simple-gettext.obj"	"$(INTDIR)\simple-gettext.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\smtp.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\smtp.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\smtp.obj"	"$(INTDIR)\smtp.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\socket.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\socket.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\socket.obj"	"$(INTDIR)\socket.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\sourcewindow.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\sourcewindow.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\sourcewindow.obj"	"$(INTDIR)\sourcewindow.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\ssl.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\ssl.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\ssl.obj"	"$(INTDIR)\ssl.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\statusbar.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\statusbar.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\statusbar.obj"	"$(INTDIR)\statusbar.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\stock_pixmap.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\stock_pixmap.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\stock_pixmap.obj"	"$(INTDIR)\stock_pixmap.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\string_match.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\string_match.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\string_match.obj"	"$(INTDIR)\string_match.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\stringtable.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\stringtable.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\stringtable.obj"	"$(INTDIR)\stringtable.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\summary_search.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\summary_search.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\summary_search.obj"	"$(INTDIR)\summary_search.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\summaryview.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\summaryview.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\summaryview.obj"	"$(INTDIR)\summaryview.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\syldap.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\syldap.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\syldap.obj"	"$(INTDIR)\syldap.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=.\sylpheed.rc

"$(INTDIR)\sylpheed.res" : $(SOURCE) "$(INTDIR)"
	$(RSC) $(RSC_PROJ) $(SOURCE)


SOURCE=..\src\template.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\template.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\template.obj"	"$(INTDIR)\template.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\textview.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\textview.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\textview.obj"	"$(INTDIR)\textview.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\undo.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\undo.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\undo.obj"	"$(INTDIR)\undo.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\unmime.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\unmime.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\unmime.obj"	"$(INTDIR)\unmime.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\utils.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\utils.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\utils.obj"	"$(INTDIR)\utils.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\uuencode.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\uuencode.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\uuencode.obj"	"$(INTDIR)\uuencode.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\vcard.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\vcard.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\vcard.obj"	"$(INTDIR)\vcard.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\w32_mailcap.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\w32_mailcap.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\w32_mailcap.obj"	"$(INTDIR)\w32_mailcap.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\xml.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\xml.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\xml.obj"	"$(INTDIR)\xml.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 

SOURCE=..\src\xmlprops.c

!IF  "$(CFG)" == "sylpheed - Win32 Release"


"$(INTDIR)\xmlprops.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ELSEIF  "$(CFG)" == "sylpheed - Win32 Debug"


"$(INTDIR)\xmlprops.obj"	"$(INTDIR)\xmlprops.sbr" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


!ENDIF 


!ENDIF 

