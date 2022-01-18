# Copyright 1999-2021 the Claws Mail team.
# This file is part of Claws Mail package, and distributed under the
# terms of the General Public License version 3 (or later).
# See COPYING file for license details.

if CLAWS_LIBETPAN
etpan_dir = etpan
etpan_library = etpan/libclawsetpan.la
else
etpan_dir =
etpan_library = 
endif

SUBDIRS = common gtk $(etpan_dir) . plugins
if BUILD_TESTS
include $(top_srcdir)/tests.mk
SUBDIRS += tests
endif

bin_PROGRAMS = claws-mail

if PLATFORM_WIN32
%.o : %.rc
	$(LIBTOOL) --mode=compile --tag=RC $(RC) -I$(srcdir) -i $< -o $@

claws_mail_platform_ldflags = -Wl,w32-resource.o \
	-Wl,--export-all-symbols,--out-implib=libclaws.a
claws_mail_deps = w32-resource.o
else
claws_mail_platform_ldflags =
claws_mail_deps =
endif

alt_abook_source = \
	addressbook-dbus.c
orig_abook_source = \
	addrbook.c \
	addrclip.c \
	addrcustomattr.c \
	addressbook.c \
	addressbook_foldersel.c \
	addrindex.c \
	addrquery.c \
	addrselect.c \
	addrduplicates.c \
	addrmerge.c \
	browseldap.c \
	editaddress.c \
	editaddress_other_attributes_ldap.c \
	editbook.c \
	editgroup.c \
	editjpilot.c \
	editldap_basedn.c \
	editldap.c \
	editvcard.c \
	exphtmldlg.c \
	expldifdlg.c \
	exporthtml.c \
	exportldif.c \
	importldif.c \
	importmutt.c \
	importpine.c \
	jpilot.c \
	ldapctrl.c \
	ldaplocate.c \
	ldapquery.c \
	ldapserver.c \
	ldapupdate.c \
	ldaputil.c \
	ldif.c \
	mutt.c \
	pine.c \
	vcard.c

if BUILD_ALTADDRBOOK
abook_source = $(alt_abook_source)
abook_extra = $(orig_abook_source)
else
abook_source = $(orig_abook_source)
abook_extra = $(alt_abook_source)
endif

abook_headers = \
	addressbook-dbus.h \
	adbookbase.h \
	addrbook.h \
	addrclip.h \
	addrcustomattr.h \
	addressbook.h \
	addressbook_foldersel.h \
	addrindex.h \
	addrquery.h \
	addrselect.h \
	addrduplicates.h \
	addrmerge.h \
	browseldap.h \
	editaddress.h \
	editaddress_other_attributes_ldap.h \
	editbook.h \
	editgroup.h \
	editjpilot.h \
	editldap_basedn.h \
	editldap.h \
	editvcard.h \
	exphtmldlg.h \
	expldifdlg.h \
	exporthtml.h \
	exportldif.h \
	file_checker.h \
	importldif.h \
	importmutt.h \
	importpine.h \
	jpilot.h \
	ldapctrl.h \
	ldaplocate.h \
	ldapquery.h \
	ldapserver.h \
	ldapupdate.h \
	ldaputil.h \
	ldif.h \
	mutt.h \
	pine.h \
	vcard.h

claws_mail_SOURCES = \
	account.c \
	action.c \
	addrcache.c \
	addr_compl.c \
	addressadd.c \
	addrgather.c \
	addrharvest.c \
	addritem.c \
	advsearch.c \
	alertpanel.c \
	autofaces.c \
	avatars.c \
	compose.c \
	crash.c \
	customheader.c \
	displayheader.c \
	edittags.c \
	enriched.c \
	entity.c \
	export.c \
	file_checker.c \
	filtering.c \
	folder.c \
	folder_item_prefs.c \
	foldersel.c \
	folderutils.c \
	folderview.c \
	grouplistdialog.c \
	headerview.c \
	html.c \
	image_viewer.c \
	imap.c \
	imap_gtk.c \
	import.c \
	inc.c \
	localfolder.c \
	main.c \
	mainwindow.c \
	manual.c \
	matcher.c \
	matcher_parser_lex.l \
	matcher_parser_parse.y \
	mbox.c \
	message_search.c \
	messageview.c \
	mh.c \
	mh_gtk.c \
	mimeview.c \
	msgcache.c \
	news.c \
	news_gtk.c \
	noticeview.c \
	oauth2.c \
	partial_download.c \
	password.c \
	password_gtk.c \
	passwordstore.c \
	pop.c \
	prefs_account.c \
	prefs_actions.c \
	prefs_common.c \
	prefs_compose_writing.c \
	prefs_customheader.c \
	prefs_display_header.c \
	prefs_ext_prog.c \
	prefs_filtering_action.c \
	prefs_filtering.c \
	prefs_folder_item.c \
	prefs_fonts.c \
	prefs_gtk.c \
	prefs_image_viewer.c \
	prefs_logging.c \
	prefs_matcher.c \
	prefs_message.c \
	prefs_migration.c \
	prefs_msg_colors.c \
	prefs_other.c \
	prefs_proxy.c \
	prefs_quote.c \
	prefs_receive.c \
	prefs_send.c \
	prefs_spelling.c \
	prefs_summaries.c \
	prefs_summary_column.c \
	prefs_summary_open.c \
	prefs_folder_column.c \
	prefs_template.c \
	prefs_themes.c \
	prefs_toolbar.c \
	prefs_wrapping.c \
	printing.c \
	privacy.c \
	procheader.c \
	procmime.c \
	procmsg.c \
	quote_fmt.c \
	quote_fmt_lex.l \
	quote_fmt_parse.y \
	recv.c \
	remotefolder.c \
	send_message.c \
	setup.c \
	sourcewindow.c \
	ssl_manager.c \
	statusbar.c \
	stock_pixmap.c \
	summary_search.c \
	summaryview.c \
	textview.c \
	toolbar.c \
	undo.c \
	uri_opener.c \
	wizard.c \
	$(abook_source)

claws_mailincludedir = $(pkgincludedir)
claws_mailinclude_HEADERS = \
	account.h \
	action.h \
	addrcache.h \
	addr_compl.h \
	addrdefs.h \
	addressadd.h \
	addritem.h \
	addressitem.h \
	addrgather.h \
	addrharvest.h \
	advsearch.h \
	alertpanel.h \
	autofaces.h \
	avatars.h \
	compose.h \
	crash.h \
	customheader.h \
	displayheader.h \
	edittags.h \
	enriched.h \
	entity.h \
	export.h \
	filtering.h \
	folder.h \
	folder_item_prefs.h \
	foldersel.h \
	folderutils.h \
	folderview.h \
	grouplistdialog.h \
	headerview.h \
	html.h \
	image_viewer.h \
	imap.h \
	imap_gtk.h \
	import.h \
	inc.h \
	localfolder.h \
	main.h \
	mainwindow.h \
	manual.h \
	matcher.h \
	matchertypes.h \
	matcher_parser.h \
	matcher_parser_lex.h \
	matcher_parser_parse.h \
	mbox.h \
	message_search.h \
	messageview.h \
	mh.h \
	mh_gtk.h \
	mimeview.h \
	msgcache.h \
	news.h \
	news_gtk.h \
	noticeview.h \
	oauth2.h \
	partial_download.h \
	password.h \
	password_gtk.h \
	passwordstore.h \
	pop.h \
	prefs_account.h \
	prefs_actions.h \
	prefs_common.h \
	prefs_compose_writing.h \
	prefs_customheader.h \
	prefs_display_header.h \
	prefs_ext_prog.h \
	prefs_filtering_action.h \
	prefs_filtering.h \
	prefs_folder_item.h \
	prefs_fonts.h \
	prefs_gtk.h \
	prefs_image_viewer.h \
	prefs_logging.h \
	prefs_matcher.h \
	prefs_message.h \
	prefs_migration.h \
	prefs_msg_colors.h \
	prefs_other.h \
	prefs_proxy.h \
	prefs_quote.h \
	prefs_receive.h \
	prefs_send.h \
	prefs_spelling.h \
	prefs_summaries.h \
	prefs_summary_column.h \
	prefs_summary_open.h \
	prefs_folder_column.h \
	prefs_template.h \
	prefs_themes.h \
	prefs_toolbar.h \
	prefs_wrapping.h \
	printing.h \
	privacy.h \
	procheader.h \
	procmime.h \
	procmsg.h \
	proctypes.h \
	quote_fmt.h \
	quote_fmt_lex.h \
	quote_fmt_parse.h \
	recv.h \
	remotefolder.h \
	send_message.h \
	setup.h \
	sourcewindow.h \
	ssl_manager.h \
	statusbar.h \
	stock_pixmap.h \
	summary_search.h \
	summaryview.h \
	textview.h \
	toolbar.h \
	undo.h \
	uri_opener.h \
	viewtypes.h \
	wizard.h \
	$(abook_headers)

BUILT_SOURCES = \
	matcher_parser_parse.h \
	quote_fmt_parse.h \
	client-bindings.h

client-bindings.h: claws-contacts.xml
if BUILD_ALTADDRBOOK
	dbus-binding-tool --prefix=abook --mode=glib-client --output=$@ claws-contacts.xml
endif

CLEANFILES = \
	     $(BUILT_SOURCES)

EXTRA_DIST = \
	claws-contacts.xml \
	w32-logo.ico \
	pixmaps/addr_one.xpm \
	pixmaps/addr_two.xpm \
	pixmaps/address_book.xpm \
	pixmaps/address_search.xpm \
	pixmaps/address.xpm \
	pixmaps/anonymous.xpm \
	pixmaps/book.xpm \
	pixmaps/cancel.xpm \
	pixmaps/category.xpm \
	pixmaps/checkbox_off.xpm \
	pixmaps/checkbox_on.xpm \
	pixmaps/check_spelling.xpm \
	pixmaps/clipkey.xpm \
	pixmaps/claws_mail_icon.xpm \
	pixmaps/claws_mail_icon_64.xpm \
	pixmaps/claws_mail_compose_logo.xpm \
	pixmaps/claws_mail_logo.xpm \
	pixmaps/claws_mail_logo_small.xpm \
	pixmaps/clip.xpm \
	pixmaps/clip_gpg_signed.xpm \
	pixmaps/close.xpm \
	pixmaps/complete.xpm \
	pixmaps/continue.xpm \
	pixmaps/copied.xpm \
	pixmaps/delete_btn.xpm \
	pixmaps/delete_dup_btn.xpm \
	pixmaps/deleted.xpm \
	pixmaps/dir_close.xpm \
	pixmaps/dir_close_hrm.xpm \
	pixmaps/dir_close_hrm_mark.xpm \
	pixmaps/dir_close_mark.xpm \
	pixmaps/dir_open.xpm \
	pixmaps/dir_open_hrm.xpm \
	pixmaps/dir_open_hrm_mark.xpm \
	pixmaps/dir_open_mark.xpm \
	pixmaps/dir_noselect_close.xpm \
	pixmaps/dir_noselect_close_mark.xpm \
	pixmaps/dir_noselect_open.xpm \
	pixmaps/dir_subs_close_mark.xpm \
	pixmaps/dir_subs_close.xpm \
	pixmaps/dir_subs_open.xpm \
	pixmaps/doc_index.xpm \
	pixmaps/doc_index_close.xpm \
	pixmaps/doc_info.xpm \
	pixmaps/down_arrow.xpm \
	pixmaps/drafts_close.xpm \
	pixmaps/drafts_close_mark.xpm \
	pixmaps/drafts_open.xpm \
	pixmaps/drafts_open_mark.xpm \
	pixmaps/edit_extern.xpm \
	pixmaps/empty.xpm \
	pixmaps/error.xpm \
	pixmaps/exec.xpm \
	pixmaps/first_arrow.xpm \
	pixmaps/forwarded.xpm \
	pixmaps/gpg_signed.xpm \
	pixmaps/go_folders.xpm \
	pixmaps/group.xpm \
	pixmaps/ham_btn.xpm \
	pixmaps/ignorethread.xpm \
	pixmaps/inbox_open.xpm \
	pixmaps/inbox_open_hrm.xpm \
	pixmaps/inbox_open_hrm_mark.xpm \
	pixmaps/inbox_open_mark.xpm \
	pixmaps/inbox_close.xpm \
	pixmaps/inbox_close_hrm.xpm \
	pixmaps/inbox_close_hrm_mark.xpm \
	pixmaps/inbox_close_mark.xpm \
	pixmaps/interface.xpm \
	pixmaps/insert_file.xpm \
	pixmaps/jpilot.xpm \
	pixmaps/key.xpm \
	pixmaps/key_gpg_signed.xpm \
	pixmaps/last_arrow.xpm \
	pixmaps/ldap.xpm \
	pixmaps/left_arrow.xpm \
	pixmaps/linewrap.xpm \
	pixmaps/linewrapcurrent.xpm \
	pixmaps/locked.xpm \
	pixmaps/mail_attach.xpm \
	pixmaps/mail_compose.xpm \
	pixmaps/mail_forward.xpm \
	pixmaps/mail_privacy_encrypted.xpm \
	pixmaps/mail_privacy_signed.xpm \
	pixmaps/mail_receive_all.xpm \
	pixmaps/mail_receive.xpm \
	pixmaps/mail_reply_to_all.xpm \
	pixmaps/mail_reply_to_author.xpm \
	pixmaps/mail_reply_to_list.xpm \
	pixmaps/mail_reply.xpm \
	pixmaps/mail_send_queue.xpm \
	pixmaps/mail_send.xpm \
	pixmaps/mail_sign.xpm \
	pixmaps/mail_draft.xpm \
	pixmaps/mark.xpm \
	pixmaps/mark_allread.xpm \
	pixmaps/mark_allunread.xpm \
	pixmaps/mark_ignorethread.xpm \
	pixmaps/mark_locked.xpm \
	pixmaps/mark_mark.xpm \
	pixmaps/mark_read.xpm \
	pixmaps/mark_unlocked.xpm \
	pixmaps/mark_unmark.xpm \
	pixmaps/mark_unread.xpm \
	pixmaps/mark_watchthread.xpm \
	pixmaps/mime_application.xpm \
	pixmaps/mime_audio.xpm \
	pixmaps/mime_image.xpm \
	pixmaps/mime_message.xpm \
	pixmaps/mime_pdf.xpm \
	pixmaps/mime_ps.xpm \
	pixmaps/mime_calendar.xpm \
	pixmaps/mime_pgpsig.xpm \
	pixmaps/mime_text_enriched.xpm \
	pixmaps/mime_text_html.xpm \
	pixmaps/mime_text_patch.xpm \
	pixmaps/mime_text_plain.xpm \
	pixmaps/mime_unknown.xpm \
	pixmaps/moved.xpm \
	pixmaps/news_compose.xpm \
	pixmaps/new.xpm \
	pixmaps/notice_error.xpm \
	pixmaps/notice_note.xpm \
	pixmaps/notice_warn.xpm \
	pixmaps/offline.xpm \
	pixmaps/online.xpm \
	pixmaps/open_mail.xpm \
	pixmaps/outbox_close.xpm \
	pixmaps/outbox_close_hrm.xpm \
	pixmaps/outbox_open.xpm \
	pixmaps/outbox_open_hrm.xpm \
	pixmaps/outbox_close_mark.xpm \
	pixmaps/outbox_close_hrm_mark.xpm \
	pixmaps/outbox_open_mark.xpm \
	pixmaps/outbox_open_hrm_mark.xpm \
	pixmaps/paste.xpm \
	pixmaps/preferences.xpm \
	pixmaps/printer.xpm \
	pixmaps/printer_btn.xpm \
	pixmaps/privacy_expired.xpm \
	pixmaps/privacy_failed.xpm \
	pixmaps/privacy_passed.xpm \
	pixmaps/privacy_signed.xpm \
	pixmaps/privacy_unknown.xpm \
	pixmaps/privacy_warn.xpm \
	pixmaps/privacy_emblem_encrypted.xpm \
	pixmaps/privacy_emblem_failed.xpm \
	pixmaps/privacy_emblem_passed.xpm \
	pixmaps/privacy_emblem_signed.xpm \
	pixmaps/privacy_emblem_warn.xpm \
	pixmaps/properties.xpm \
	pixmaps/queue_close.xpm \
	pixmaps/queue_close_hrm.xpm \
	pixmaps/queue_close_hrm_mark.xpm \
	pixmaps/queue_close_mark.xpm \
	pixmaps/queue_open.xpm \
	pixmaps/queue_open_hrm.xpm \
	pixmaps/queue_open_hrm_mark.xpm \
	pixmaps/queue_open_mark.xpm \
	pixmaps/quicksearch.xpm \
	pixmaps/replied.xpm \
	pixmaps/replied_and_forwarded.xpm \
	pixmaps/right_arrow.xpm \
	pixmaps/rotate_left.xpm \
	pixmaps/rotate_right.xpm \
	pixmaps/selection.xpm \
	pixmaps/spam.xpm \
	pixmaps/spam_btn.xpm \
	pixmaps/trash_btn.xpm \
	pixmaps/trash_close.xpm \
	pixmaps/trash_close_hrm.xpm \
	pixmaps/trash_close_hrm_mark.xpm \
	pixmaps/trash_close_mark.xpm \
	pixmaps/trash_open.xpm \
	pixmaps/trash_open_hrm.xpm \
	pixmaps/trash_open_hrm_mark.xpm \
	pixmaps/trash_open_mark.xpm \
	pixmaps/tray_newmail_offline.xpm \
	pixmaps/tray_newmail.xpm \
	pixmaps/tray_newmarkedmail_offline.xpm \
	pixmaps/tray_newmarkedmail.xpm \
	pixmaps/tray_nomail_offline.xpm \
	pixmaps/tray_nomail.xpm \
	pixmaps/tray_unreadmail_offline.xpm \
	pixmaps/tray_unreadmail.xpm \
	pixmaps/tray_unreadmarkedmail_offline.xpm \
	pixmaps/tray_unreadmarkedmail.xpm \
	pixmaps/unread.xpm \
	pixmaps/read.xpm \
	pixmaps/up_arrow.xpm \
	pixmaps/vcard.xpm \
	pixmaps/watchthread.xpm \
	pixmaps/zoom_fit.xpm \
	pixmaps/zoom_in.xpm \
	pixmaps/zoom_out.xpm \
	pixmaps/zoom_width.xpm \
	$(abook_extra)


IFLAGS = \
	-I$(srcdir)/common \
	-Icommon \
	-I$(srcdir)/gtk \
	-I$(srcdir)/etpan

claws_mail_LDFLAGS = \
	$(claws_mail_platform_ldflags)

claws_mail_DEPENDENCIES = $(claws_mail_deps) \
	$(etpan_library) \
	gtk/libclawsgtk.la

claws_mail_LDADD = \
	$(etpan_library) \
	gtk/libclawsgtk.la \
	$(ENCHANT_LIBS) \
	$(INTLLIBS) \
	$(GTK_LIBS) \
	$(LDAP_LIBS) \
	$(GNUTLS_LIBS) \
	$(NETTLE_LIBS) \
	$(COMPFACE_LIBS) \
	$(JPILOT_LIBS) \
	$(PTHREAD_LIBS) \
	$(SM_LIBS) \
	$(LIBICONV) \
	$(STARTUP_NOTIFICATION_LIBS) \
	$(LIBETPAN_LIBS) \
	$(DBUS_LIBS) \
	$(SVG_LIBS) \
	$(CONTACTS_LIBS)

pixmapdir=$(datadir)/icons/hicolor/48x48/apps

AM_CPPFLAGS = \
	-DG_LOG_DOMAIN=\"Claws-Mail\" \
	-DLOCALEDIR=\""$(localedir)"\" \
	-DPIXMAP_DIR=\""$(pixmapdir)"\" \
	-DMANUALDIR=\""$(manualdir)"\" \
	-DTARGET_ALIAS=\""$(target_triplet)"\" \
	-DSYSCONFDIR=\""$(sysconfdir)"\" \
	-DDATAROOTDIR=\""$(datarootdir)"\" \
	$(IFLAGS) \
	$(ENCHANT_CFLAGS) \
	$(GTK_CFLAGS) \
	$(GNUTLS_CFLAGS) \
	$(GPGME_CFLAGS) \
	$(LIBETPAN_CPPFLAGS) \
	$(STARTUP_NOTIFICATION_CFLAGS) \
	$(DBUS_CFLAGS) \
	$(SVG_CFLAGS) \
	$(NETWORKMANAGER_SUPPORT_CFLAGS) \
	$(VALGRIND_CFLAGS) \
	$(CONTACTS_CFLAGS)

matcher_parser_lex.$(OBJEXT) : AM_CFLAGS += -Wno-unused-function
quote_fmt_lex.$(OBJEXT) : AM_CFLAGS += -Wno-unused-function

#no-unused-function is there because of bison stuff
#no-deprecated-declarations because of gpgme_data_rewind, which we have
#to call inside gpgme due to our lack of LARGEFILE support

AM_YFLAGS = -d

if CYGWIN
claws-mail$(EXEEXT): $(claws_mail_OBJECTS) $(claws_mail_DEPENDENCIES) 
	@rm -f claws-mail$(EXEEXT)
	@rm -f claws-mail.def claws-mail.base claws-mail.exp libclaws-mail.a .libs/libclaws-mail.a
	$(LINK) -shared $(claws_mailLDFLAGS) -Wl,--out-implib=libclaws-mail.a -Wl,--export-all-symbols,--exclude-symbols,main $(claws_mail_OBJECTS) -Wl,--no-whole-archive $(claws_mail_LDADD) $(LIBS)
	dlltool --output-def claws-mail.def --dllname claws-mail$(EXEEXT) --export-all-symbols --exclude-symbols main $(claws_mail_OBJECTS) gtk/.libs/libclawsgtk.a
	$(LINK) $(claws_mail_LDFLAGS) -Wl,--base-file,claws-mail.base $(claws_mail_OBJECTS) $(claws_mail_LDADD) $(LIBS)
	sleep 5
	dlltool --base-file claws-mail.base --input-def claws-mail.def --output-exp claws-mail.exp
	$(LINK) $(claws_mail_LDFLAGS) -Wl,claws-mail.exp $(claws_mail_OBJECTS) $(claws_mail_LDADD) $(LIBS)
	@cp libclaws-mail.a .libs/libclaws-mail.a
else
#claws-mail$(EXEEXT): $(claws_mail_OBJECTS) $(claws_mail_DEPENDENCIES) 
#	@rm -f claws-mail$(EXEEXT)
#	$(LINK) $(claws_mail_LDFLAGS) $(claws_mail_OBJECTS) $(claws_mail_LDADD) $(LIBS)
endif
