bin_PROGRAMS = sylpheed

sylpheed_SOURCES = \
	intl.h \
	defs.h \
	main.c main.h \
	mainwindow.c mainwindow.h \
	folderview.c folderview.h \
	summaryview.c summaryview.h \
	messageview.c messageview.h \
	headerview.c headerview.h \
	textview.c textview.h \
	imageview.c imageview.h \
	mimeview.c mimeview.h \
	summary_search.c summary_search.h \
	folder.c folder.h \
	procmsg.c procmsg.h \
	procheader.c procheader.h \
	filter.c filter.h \
	compose.c compose.h \
	gtkshruler.c gtkshruler.h \
	gtksctree.c gtksctree.h \
	menu.c menu.h \
	prefs.c prefs.h \
	prefs_common.c prefs_common.h \
	prefs_filter.c prefs_filter.h \
	prefs_account.c prefs_account.h \
	account.c account.h \
	addressbook.c addressbook.h \
	filesel.c filesel.h \
	foldersel.c foldersel.h \
	statusbar.c statusbar.h \
	headerwindow.c headerwindow.h \
	logwindow.c logwindow.h \
	sourcewindow.c sourcewindow.h \
	manage_window.c manage_window.h \
	alertpanel.c alertpanel.h \
	inputdialog.c inputdialog.h \
	progressdialog.c progressdialog.h \
	about.c about.h \
	setup.c setup.h \
	utils.c utils.h \
	gtkutils.c gtkutils.h \
	codeconv.c codeconv.h \
	unmime.c unmime.h \
	base64.c base64.h \
	uuencode.c uuencode.h \
	md5c.c md5.h \
	rfc822.c rfc822.h \
	socket.c socket.h \
	automaton.c automaton.h \
	session.c session.h \
	smtp.c smtp.h \
	esmtp.c esmtp.h \
	pop.c pop.h \
	mh.c mh.h \
	mbox.c mbox.h \
	send.c send.h \
	recv.c recv.h \
	inc.c inc.h \
	import.c import.h \
	export.c export.h \
	nntp.c nntp.h \
	news.c news.h \
	imap.c imap.h \
	xml.c xml.h \
	html.c html.h \
	procmime.c procmime.h \
	rfc2015.c rfc2015.h \
	passphrase.c passphrase.h \
	select-keys.c select-keys.h \
	sigstatus.c sigstatus.h \
	simple-gettext.c \
	manual.c manual.h \
	addr_compl.c addr_compl.h \
	gtkstext.c gtkstext.h \
	grouplist_dialog.c grouplist_dialog.h \
	customheader.c customheader.h \
	prefs_headers.c prefs_headers.h \
	headers_display.c headers_display.h \
	prefs_display_headers.c prefs_display_headers.h

EXTRA_DIST = \
	pixmaps/clip.xpm \
	pixmaps/deleted.xpm \
	pixmaps/dir-close.xpm \
	pixmaps/dir-open.xpm \
	pixmaps/folder.xpm \
	pixmaps/forwarded.xpm \
	pixmaps/group.xpm \
	pixmaps/inbox.xpm \
	pixmaps/mark.xpm \
	pixmaps/new.xpm \
	pixmaps/outbox.xpm \
	pixmaps/regular.xpm \
	pixmaps/replied.xpm \
	pixmaps/trash.xpm \
	pixmaps/unread.xpm \
	pixmaps/linewrap.xpm \
	pixmaps/stock_mail.xpm \
	pixmaps/stock_mail_attach.xpm \
	pixmaps/stock_mail_receive.xpm \
	pixmaps/stock_mail_receive_all.xpm \
	pixmaps/stock_mail_send.xpm \
	pixmaps/stock_mail_compose.xpm \
	pixmaps/stock_mail_reply.xpm \
	pixmaps/stock_mail_reply_to_all.xpm \
	pixmaps/stock_mail_forward.xpm \
	pixmaps/stock_preferences.xpm \
	pixmaps/stock_properties.xpm \
	pixmaps/stock_search.xpm \
	pixmaps/stock_close.xpm \
	pixmaps/stock_exec.xpm \
	pixmaps/stock_trash.xpm \
	pixmaps/stock_up_arrow.xpm \
	pixmaps/stock_down_arrow.xpm \
	pixmaps/stock_paste.xpm \
	pixmaps/tb_address_book.xpm \
	pixmaps/sylpheed-logo.xpm

INCLUDES = \
	-DG_LOG_DOMAIN=\"Sylpheed\" \
	-I$(top_srcdir)/libkcc \
	-I$(top_srcdir)/intl \
	$(GTK_CFLAGS) \
	$(GDK_IMLIB_CFLAGS) \
	$(GDK_PIXBUF_CFLAGS) \
	-I$(includedir)

sylpheed_LDADD = \
	$(top_builddir)/libkcc/libkcc.a	\
	$(INTLLIBS) \
	$(GTK_LIBS) \
	$(GDK_IMLIB_LIBS) \
	$(GDK_PIXBUF_LIBS) \
	$(GPGME_LIBS)

CPPFLAGS = \
	-DLOCALEDIR=\""$(localedir)"\" \
	-DMANUALDIR=\""$(manualdir)"\" \
	-DHOST_ALIAS=\""$(host_alias)"\"
