/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2003 Hiroyuki Yamamoto
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "defs.h"

#include <glib.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkcontainer.h>
#include <gtk/gtkeditable.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtktext.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "intl.h"
#include "main.h"
#include "messageview.h"
#include "message_search.h"
#include "headerview.h"
#include "summaryview.h"
#include "textview.h"
#include "mimeview.h"
#include "menu.h"
#include "about.h"
#include "filesel.h"
#include "sourcewindow.h"
#include "addressbook.h"
#include "alertpanel.h"
#include "inputdialog.h"
#include "manage_window.h"
#include "procmsg.h"
#include "procheader.h"
#include "procmime.h"
#include "account.h"
#include "action.h"
#include "prefs_common.h"
#include "prefs_account.h"
#include "gtkutils.h"
#include "utils.h"
#include "rfc2015.h"
#include "send_message.h"
#include "stock_pixmap.h"
#include "hooks.h"
#include "filtering.h"

static GList *messageview_list = NULL;

static void messageview_destroy_cb	(GtkWidget	*widget,
					 MessageView	*messageview);
static void messageview_size_allocate_cb(GtkWidget	*widget,
					 GtkAllocation	*allocation);
static gboolean key_pressed		(GtkWidget	*widget,
					 GdkEventKey	*event,
					 MessageView	*messageview);

static void return_receipt_show		(NoticeView     *noticeview, 
				         MsgInfo        *msginfo);	
static void return_receipt_send_clicked (NoticeView	*noticeview, 
                                         MsgInfo        *msginfo);
static void save_as_cb			(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void print_cb			(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void close_cb			(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void copy_cb			(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void allsel_cb			(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void search_cb			(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);

static void set_charset_cb		(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void view_source_cb		(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void show_all_header_cb		(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);

static void compose_cb			(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void reply_cb			(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void reedit_cb			(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);

static PrefsAccount *select_account_from_list
					(GList		*ac_list);
static void addressbook_open_cb		(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void add_address_cb		(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void create_filter_cb		(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void create_processing_cb	(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);

static void about_cb			(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void messageview_update		(MessageView *msgview);
static gboolean messageview_update_msg	(gpointer source, gpointer data);

static GList *msgview_list = NULL;
static GtkItemFactoryEntry msgview_entries[] =
{
	{N_("/_File"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_File/_Save as..."),	NULL, save_as_cb, 0, NULL},
	{N_("/_File/_Print..."),	NULL, print_cb, 0, NULL},
	{N_("/_File/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_File/_Close"),		NULL, close_cb, 0, NULL},

	{N_("/_Edit"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_Edit/_Copy"),		NULL, copy_cb, 0, NULL},
	{N_("/_Edit/Select _all"),	NULL, allsel_cb, 0, NULL},
	{N_("/_Edit/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_Edit/_Find in current message..."),
					NULL, search_cb, 0, NULL},

	{N_("/_View"),			NULL, NULL, 0, "<Branch>"},

#define CODESET_SEPARATOR \
	{N_("/_View/_Code set/---"),	NULL, NULL, 0, "<Separator>"}
#define CODESET_ACTION(action) \
	NULL, set_charset_cb, action, "/View/Code set/Auto detect"

	{N_("/_View/_Code set"),	NULL, NULL, 0, "<Branch>"},
	{N_("/_View/_Code set/_Auto detect"),
					NULL, set_charset_cb, C_AUTO, "<RadioItem>"},
	CODESET_SEPARATOR,
	{N_("/_View/_Code set/7bit ascii (US-ASC_II)"),
	 CODESET_ACTION(C_US_ASCII)},

#if HAVE_ICONV
	{N_("/_View/_Code set/Unicode (_UTF-8)"),
	 CODESET_ACTION(C_UTF_8)},
	CODESET_SEPARATOR,
#endif
	{N_("/_View/_Code set/Western European (ISO-8859-_1)"),
	 CODESET_ACTION(C_ISO_8859_1)},
	{N_("/_View/_Code set/Western European (ISO-8859-15)"),
	 CODESET_ACTION(C_ISO_8859_15)},
	CODESET_SEPARATOR,
#if HAVE_ICONV
	{N_("/_View/_Code set/Central European (ISO-8859-_2)"),
	 CODESET_ACTION(C_ISO_8859_2)},
	CODESET_SEPARATOR,
	{N_("/_View/_Code set/_Baltic (ISO-8859-13)"),
	 CODESET_ACTION(C_ISO_8859_13)},
	{N_("/_View/_Code set/Baltic (ISO-8859-_4)"),
	 CODESET_ACTION(C_ISO_8859_4)},
	CODESET_SEPARATOR,
	{N_("/_View/_Code set/Greek (ISO-8859-_7)"),
	 CODESET_ACTION(C_ISO_8859_7)},
	CODESET_SEPARATOR,
	{N_("/_View/_Code set/Turkish (ISO-8859-_9)"),
	 CODESET_ACTION(C_ISO_8859_9)},
	CODESET_SEPARATOR,
	{N_("/_View/_Code set/Cyrillic (ISO-8859-_5)"),
	 CODESET_ACTION(C_ISO_8859_5)},
	{N_("/_View/_Code set/Cyrillic (KOI8-_R)"),
	 CODESET_ACTION(C_KOI8_R)},
	{N_("/_View/_Code set/Cyrillic (Windows-1251)"),
	 CODESET_ACTION(C_CP1251)},
	CODESET_SEPARATOR,
#endif
	{N_("/_View/_Code set/Japanese (ISO-2022-_JP)"),
	 CODESET_ACTION(C_ISO_2022_JP)},
#if HAVE_ICONV
	{N_("/_View/_Code set/Japanese (ISO-2022-JP-2)"),
	 CODESET_ACTION(C_ISO_2022_JP_2)},
#endif
	{N_("/_View/_Code set/Japanese (_EUC-JP)"),
	 CODESET_ACTION(C_EUC_JP)},
	{N_("/_View/_Code set/Japanese (_Shift__JIS)"),
	 CODESET_ACTION(C_SHIFT_JIS)},
#if HAVE_ICONV
	CODESET_SEPARATOR,
	{N_("/_View/_Code set/Simplified Chinese (_GB2312)"),
	 CODESET_ACTION(C_GB2312)},
	{N_("/_View/_Code set/Traditional Chinese (_Big5)"),
	 CODESET_ACTION(C_BIG5)},
	{N_("/_View/_Code set/Traditional Chinese (EUC-_TW)"),
	 CODESET_ACTION(C_EUC_TW)},
	{N_("/_View/_Code set/Chinese (ISO-2022-_CN)"),
	 CODESET_ACTION(C_ISO_2022_CN)},
	CODESET_SEPARATOR,
	{N_("/_View/_Code set/Korean (EUC-_KR)"),
	 CODESET_ACTION(C_EUC_KR)},
	{N_("/_View/_Code set/Korean (ISO-2022-KR)"),
	 CODESET_ACTION(C_ISO_2022_KR)},
	CODESET_SEPARATOR,
	{N_("/_View/_Code set/Thai (TIS-620)"),
	 CODESET_ACTION(C_TIS_620)},
	{N_("/_View/_Code set/Thai (Windows-874)"),
	 CODESET_ACTION(C_WINDOWS_874)},
#endif

#undef CODESET_SEPARATOR
#undef CODESET_ACTION

	{N_("/_View/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_View/Mess_age source"),	NULL, view_source_cb, 0, NULL},
	{N_("/_View/Show all _header"),	NULL, show_all_header_cb, 0, "<ToggleItem>"},

	{N_("/_Message"),		NULL, NULL, 0, "<Branch>"},
	{N_("/_Message/Compose _new message"),
					NULL, compose_cb, 0, NULL},
	{N_("/_Message/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_Message/_Reply"),	NULL, reply_cb, COMPOSE_REPLY, NULL},
	{N_("/_Message/Repl_y to/_all"),
					NULL, reply_cb, COMPOSE_REPLY_TO_ALL, NULL},
	{N_("/_Message/Repl_y to/_sender"),
					NULL, reply_cb, COMPOSE_REPLY_TO_SENDER, NULL},
	{N_("/_Message/Repl_y to/mailing _list"),
					NULL, reply_cb, COMPOSE_REPLY_TO_LIST, NULL},
	{N_("/_Message/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_Message/_Forward"),	NULL, reply_cb, COMPOSE_FORWARD, NULL},
	{N_("/_Message/For_ward as attachment"),
					NULL, reply_cb, COMPOSE_FORWARD_AS_ATTACH, NULL},
	{N_("/_Message/Redirec_t"),	NULL, reply_cb, COMPOSE_REDIRECT, NULL},
	{N_("/_Message/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_Message/Re-_edit"),	NULL, reedit_cb, 0, NULL},

	{N_("/_Tools"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_Tools/_Address book"),	NULL, addressbook_open_cb, 0, NULL},
	{N_("/_Tools/Add sender to address boo_k"),
					NULL, add_address_cb, 0, NULL},
	{N_("/_Tools/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_Tools/_Create filter rule"),
					NULL, NULL, 0, "<Branch>"},
	{N_("/_Tools/_Create filter rule/_Automatically"),
					NULL, create_filter_cb, FILTER_BY_AUTO, NULL},
	{N_("/_Tools/_Create filter rule/by _From"),
					NULL, create_filter_cb, FILTER_BY_FROM, NULL},
	{N_("/_Tools/_Create filter rule/by _To"),
					NULL, create_filter_cb, FILTER_BY_TO, NULL},
	{N_("/_Tools/_Create filter rule/by _Subject"),
					NULL, create_filter_cb, FILTER_BY_SUBJECT, NULL},
	{N_("/_Tools/Create processing rule/"),
					NULL, NULL, 0, "<Branch>"},
	{N_("/_Tools/Create processing rule/_Automatically"),
					NULL, create_processing_cb, FILTER_BY_AUTO, NULL},
	{N_("/_Tools/Create processing rule/by _From"),
					NULL, create_processing_cb, FILTER_BY_FROM, NULL},
	{N_("/_Tools/Create processing rule/by _To"),
					NULL, create_processing_cb, FILTER_BY_TO, NULL},
	{N_("/_Tools/Create processing rule/by _Subject"),
					NULL, create_processing_cb, FILTER_BY_SUBJECT, NULL},
	{N_("/_Tools/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_Tools/Actio_ns"),	NULL, NULL, 0, "<Branch>"},

	{N_("/_Help"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_Help/_About"),		NULL, about_cb, 0, NULL}
};

MessageView *messageview_create(MainWindow *mainwin)
{
	MessageView *messageview;
	GtkWidget *vbox;
	HeaderView *headerview;
	MimeView *mimeview;
	NoticeView *noticeview;

	debug_print("Creating message view...\n");
	messageview = g_new0(MessageView, 1);

	headerview = headerview_create();

	noticeview = noticeview_create(mainwin);

	mimeview = mimeview_create(mainwin);
	mimeview->textview = textview_create();
	mimeview->textview->messageview = messageview;
	mimeview->messageview = messageview;

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET_PTR(headerview),
			   FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET_PTR(noticeview),
			   FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox),
                           GTK_WIDGET_PTR(mimeview), TRUE, TRUE, 0);
	gtk_widget_show(vbox);

	messageview->vbox       = vbox;
	messageview->new_window = FALSE;
	messageview->window     = NULL;
	messageview->headerview = headerview;
	messageview->mimeview   = mimeview;
	messageview->noticeview = noticeview;
	messageview->mainwin    = mainwin;
	messageview->msginfo_update_callback_id =
		hooks_register_hook(MSGINFO_UPDATE_HOOKLIST, messageview_update_msg, (gpointer) messageview);

	return messageview;
}


GList *messageview_get_msgview_list(void)
{
	return msgview_list;
}

void messageview_update_actions_menu(MessageView *msgview)
{
	GtkItemFactory *ifactory;

	/* Messages opened in a new window do not have a menu bar */
	if (msgview->menubar == NULL)
		return;
	ifactory = gtk_item_factory_from_widget(msgview->menubar);
	action_update_msgview_menu(ifactory, "/Tools/Actions", msgview);
}

void messageview_add_toolbar(MessageView *msgview, GtkWidget *window) 
{
 	GtkWidget *handlebox;
	GtkWidget *vbox;
	GtkWidget *menubar;
	GtkItemFactory *ifactory;
	guint n_menu_entries;

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(vbox);
	gtk_container_add(GTK_CONTAINER(window), vbox);	
	
	n_menu_entries = sizeof(msgview_entries) / sizeof(msgview_entries[0]);
	menubar = menubar_create(window, msgview_entries,
				 n_menu_entries, "<MessageView>", msgview);
	gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, TRUE, 0);

	handlebox = gtk_handle_box_new();
	gtk_box_pack_start(GTK_BOX(vbox), handlebox, FALSE, FALSE, 0);
	msgview->toolbar = toolbar_create(TOOLBAR_MSGVIEW, handlebox,
					  (gpointer)msgview);
	msgview->handlebox = handlebox;
	msgview->menubar   = menubar;

	gtk_container_add(GTK_CONTAINER(vbox),
			  GTK_WIDGET_PTR(msgview));

	messageview_update_actions_menu(msgview);

	msgview_list = g_list_append(msgview_list, msgview);
}

MessageView *messageview_create_with_new_window(MainWindow *mainwin)
{
	GtkWidget *window;
	MessageView *msgview;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), _("Sylpheed - Message View"));
	gtk_window_set_wmclass(GTK_WINDOW(window), "message_view", "Sylpheed");
	gtk_window_set_policy(GTK_WINDOW(window), TRUE, TRUE, FALSE);
	gtk_widget_set_size_request(window, prefs_common.msgwin_width,
				    prefs_common.msgwin_height);

	msgview = messageview_create(mainwin);

	g_signal_connect(G_OBJECT(window), "size_allocate",
			 G_CALLBACK(messageview_size_allocate_cb),
			 msgview);
	g_signal_connect(G_OBJECT(window), "destroy",
			 G_CALLBACK(messageview_destroy_cb), msgview);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(key_pressed), msgview);

	messageview_add_toolbar(msgview, window);

	gtk_widget_grab_focus(msgview->mimeview->textview->text);
	gtk_widget_show(window);

	msgview->new_window = TRUE;
	msgview->window = window;
	msgview->visible = TRUE;

	toolbar_set_style(msgview->toolbar->toolbar, msgview->handlebox, 
			  prefs_common.toolbar_style);
	messageview_init(msgview);

	return msgview;
}

void messageview_init(MessageView *messageview)
{
	headerview_init(messageview->headerview);
	mimeview_init(messageview->mimeview);
	/*messageview_set_font(messageview);*/

	noticeview_hide(messageview->noticeview);
}

static void notification_convert_header(gchar *dest, gint len, 
					const gchar *src_,
					gint header_len)
{
	char *src;

	g_return_if_fail(src_ != NULL);
	g_return_if_fail(dest != NULL);

	if (len < 1) return;

	Xstrndup_a(src, src_, len, return);

	remove_return(src);

	if (is_ascii_str(src)) {
		strncpy2(dest, src, len);
		dest[len - 1] = '\0';
		return;
	} else
		conv_encode_header(dest, len, src, header_len, FALSE);
}

static gint disposition_notification_send(MsgInfo *msginfo)
{
	gchar buf[BUFFSIZE];
	gchar tmp[MAXPATHLEN + 1];
	FILE *fp;
	GList *ac_list;
	PrefsAccount *account;
	gint ok;
	gchar *to;
	FolderItem *queue, *outbox;
	gint num;
	gchar *path;
        gchar *addr;
        gchar *addrp;

	if ((!msginfo->returnreceiptto) && 
	    (!msginfo->dispositionnotificationto)) 
		return -1;

	/* RFC2298: Test for Return-Path */
	if (msginfo->dispositionnotificationto)
		to = msginfo->dispositionnotificationto;
	else
		to = msginfo->returnreceiptto;

	ok = procheader_get_header_from_msginfo(msginfo, buf, sizeof(buf),
				"Return-Path:");
	if (ok == 0) {
		gchar *to_addr = g_strdup(to);
		extract_address(to_addr);
		extract_address(buf);
		ok = strcmp(to_addr, buf);
		g_free(to_addr);
	} else {
		strncpy(buf, _("<No Return-Path found>"), 
				sizeof(buf));
	}
	
	if (ok != 0) {
		AlertValue val;
		gchar *message;
		message = g_strdup_printf(
				 _("The notification address to which the "
				   "return receipt is to be sent\n"
				   "does not correspond to the return path:\n"
				   "Notification address: %s\n"
				   "Return path: %s\n"
				   "It is advised to not to send the return "
				   "receipt."), to, buf);
		val = alertpanel(_("Warning"), message, _("Send"),
				_("+Don't Send"), NULL);
		g_free(message);				
		if (val != G_ALERTDEFAULT)
			return -1;
	}

	ac_list = account_find_all_from_address(NULL, msginfo->to);
	ac_list = account_find_all_from_address(ac_list, msginfo->cc);

	if (ac_list == NULL) {
		alertpanel_error(_("This message is asking for a return "
				   "receipt notification\n"
				   "but according to its 'To:' and 'CC:' "
				   "headers it was not\nofficially addressed "
				   "to you.\n"
				   "Receipt notification cancelled."));
		return -1;
	}

	if (g_list_length(ac_list) > 1)
		account = select_account_from_list(ac_list);
	else
		account = (PrefsAccount *) ac_list->data;
	g_list_free(ac_list);

	if (account == NULL)
		return -1;

	/* write to temporary file */
	g_snprintf(tmp, sizeof(tmp), "%s%ctmpmsg%d",
		   get_rc_dir(), G_DIR_SEPARATOR, (gint)msginfo);

	if ((fp = fopen(tmp, "wb")) == NULL) {
		FILE_OP_ERROR(tmp, "fopen");
		return -1;
	}

	/* chmod for security */
	if (change_file_mode_rw(fp, tmp) < 0) {
		FILE_OP_ERROR(tmp, "chmod");
		g_warning("can't change file mode\n");
	}
	
	addr = g_strdup(to);
	
	extract_address(addr);
	addrp = addr;
	
	/* write queue headers */
	fprintf(fp, "AF:\n");
	fprintf(fp, "NF:0\n");
	fprintf(fp, "PS:10\n");
	fprintf(fp, "SRH:1\n");
	fprintf(fp, "SFN:\n");
	fprintf(fp, "DSR:\n");
	fprintf(fp, "MID:\n");
	fprintf(fp, "CFG:\n");
	fprintf(fp, "PT:0\n");
	fprintf(fp, "S:%s\n", account->address);
	fprintf(fp, "RQ:\n");
	if (account->smtp_server)
		fprintf(fp, "SSV:%s\n", account->smtp_server);
	else
		fprintf(fp, "SSV:\n");
	fprintf(fp, "SSH:\n");
	fprintf(fp, "R:<%s>\n", addrp);
	
	g_free(addrp);
	
	/* check whether we need to save the message */
	outbox = account_get_special_folder(account, F_OUTBOX); 
	if (folder_get_default_outbox() == outbox && !prefs_common.savemsg)
		outbox = NULL;
	if (outbox) {
		path = folder_item_get_identifier(outbox);
		fprintf(fp, "SCF:%s\n", path);
		g_free(path);
	}		

	fprintf(fp, "\n");
	
	/* Date */
	get_rfc822_date(buf, sizeof(buf));
	fprintf(fp, "Date: %s\n", buf);

	/* From */
	if (account->name && *account->name) {
		notification_convert_header
			(buf, sizeof(buf), account->name,
			 strlen("From: "));
		fprintf(fp, "From: %s <%s>\n", buf, account->address);
	} else
		fprintf(fp, "From: %s\n", account->address);

	fprintf(fp, "To: %s\n", to);

	/* Subject */
	notification_convert_header(buf, sizeof(buf), msginfo->subject,
				    strlen("Subject: "));
	fprintf(fp, "Subject: Disposition notification: %s\n", buf);

	/* Message ID */
	generate_msgid(account->address, buf, sizeof buf);
	fprintf(fp, "Message-Id: <%s>\n", buf);

	if (fclose(fp) == EOF) {
		FILE_OP_ERROR(tmp, "fclose");
		unlink(tmp);
		return -1;
	}

	/* put it in queue */
	queue = account_get_special_folder(account, F_QUEUE);
	if (!queue) queue = folder_get_default_queue();
	if (!queue) {
		g_warning("can't find queue folder\n");
		unlink(tmp);
		return -1;
	}
	folder_item_scan(queue);
	if ((num = folder_item_add_msg(queue, tmp, NULL, TRUE)) < 0) {
		g_warning("can't queue the message\n");
		unlink(tmp);
		return -1;
	}
	
	/* send it */
	path = folder_item_fetch_msg(queue, num);
	ok = procmsg_send_message_queue(path);
	g_free(path);
	folder_item_remove_msg(queue, num);

	return ok;
}

GList *messageview_get_window_list(void)
{
	return messageview_list;
}

static gboolean find_encrypted_func(GNode *node, gpointer data)
{
	MimeInfo *mimeinfo = (MimeInfo *) node->data;
	MimeInfo **encinfo = (MimeInfo **) data;
	
	if (privacy_mimeinfo_is_encrypted(mimeinfo)) {
		*encinfo = mimeinfo;
		return TRUE;
	}
	
	return FALSE;
}

static MimeInfo *find_encrypted_part(MimeInfo *rootinfo)
{
	MimeInfo *encinfo = NULL;

	g_node_traverse(rootinfo->node, G_IN_ORDER, G_TRAVERSE_ALL, -1,
		find_encrypted_func, &encinfo);
	
	return encinfo;
}

gint messageview_show(MessageView *messageview, MsgInfo *msginfo,
		      gboolean all_headers)
{
	gchar *file;
	MimeInfo *mimeinfo, *encinfo;

	g_return_val_if_fail(msginfo != NULL, -1);

	mimeinfo = procmime_scan_message(msginfo);
	if (!mimeinfo) {
		textview_show_error(messageview->mimeview->textview);
		return -1;
	}

	while ((encinfo = find_encrypted_part(mimeinfo)) != NULL) {
		debug_print("decrypting message part\n");
		if (privacy_mimeinfo_decrypt(encinfo) < 0)
			break;
	}
	
	file = procmsg_get_message_file_path(msginfo);
	if (!file) {
		g_warning("can't get message file path.\n");
		procmime_mimeinfo_free_all(mimeinfo);
		textview_show_error(messageview->mimeview->textview);
		return -1;
	}

	if (messageview->msginfo != msginfo) {
		procmsg_msginfo_free(messageview->msginfo);
		messageview->msginfo = procmsg_msginfo_get_full_info(msginfo);
	}
	headerview_show(messageview->headerview, messageview->msginfo);

	messageview->all_headers = all_headers;
	textview_set_all_headers(messageview->mimeview->textview, all_headers);

	mimeview_show_message(messageview->mimeview, mimeinfo, file);

	if ((messageview->msginfo->dispositionnotificationto || 
	     messageview->msginfo->returnreceiptto) &&
	    !MSG_IS_RETRCPT_SENT(messageview->msginfo->flags))
		return_receipt_show(messageview->noticeview, messageview->msginfo);
	else 
		noticeview_hide(messageview->noticeview);

	g_free(file);

	return 0;
}

void messageview_reflect_prefs_pixmap_theme(void)
{
	GList *cur;
	MessageView *msgview;

	for (cur = msgview_list; cur != NULL; cur = cur->next) {
		msgview = (MessageView*)cur->data;
		toolbar_update(TOOLBAR_MSGVIEW, msgview);
		mimeview_update(msgview->mimeview);
	}
}

void messageview_clear(MessageView *messageview)
{
	procmsg_msginfo_free(messageview->msginfo);
	messageview->msginfo = NULL;
	messageview->filtered = FALSE;
	mimeview_clear(messageview->mimeview);
	headerview_clear(messageview->headerview);
	noticeview_hide(messageview->noticeview);
}

void messageview_destroy(MessageView *messageview)
{
	GtkWidget *mimeview  = GTK_WIDGET_PTR(messageview->mimeview);

	debug_print("destroy messageview\n");
	messageview_list = g_list_remove(messageview_list, messageview);

	hooks_unregister_hook(MSGINFO_UPDATE_HOOKLIST,
			      messageview->msginfo_update_callback_id);

	headerview_destroy(messageview->headerview);
	mimeview_destroy(messageview->mimeview);
	noticeview_destroy(messageview->noticeview);

	procmsg_msginfo_free(messageview->msginfo);
	toolbar_clear_list(TOOLBAR_MSGVIEW);
	if (messageview->toolbar) {
		toolbar_destroy(messageview->toolbar);
		g_free(messageview->toolbar);
	}
	
	msgview_list = g_list_remove(msgview_list, messageview); 

	g_free(messageview);
}

void messageview_delete(MessageView *msgview)
{
	MsgInfo *msginfo = (MsgInfo *) msgview->msginfo;
	FolderItem *trash = NULL;
	PrefsAccount *ac = NULL;

	g_return_if_fail(msginfo != NULL);

	/* to get the trash folder, we have to choose either
	 * the folder's or account's trash default - we prefer
	 * the one in the account prefs */
	if (msginfo->folder) {
		if (NULL != (ac = account_find_from_item(msginfo->folder)))
			trash = account_get_special_folder(ac, F_TRASH);
		if (!trash && msginfo->folder->folder)	
			trash = msginfo->folder->folder->trash;
		/* if still not found, use the default */
		if (!trash) 
			trash =	folder_get_default_trash();
	}	

	g_return_if_fail(trash   != NULL);

	if (prefs_common.immediate_exec)
		/* TODO: Delete from trash */
		folder_item_move_msg(trash, msginfo);
	else {
		procmsg_msginfo_set_to_folder(msginfo, trash);
		procmsg_msginfo_set_flags(msginfo, MSG_DELETED, 0);
		/* NOTE: does not update to next message in summaryview */
	}
}

/* 
 * \brief update messageview with currently selected message in summaryview
 *        leave unchanged if summaryview is empty
 * \param pointer to MessageView
 */	
static void messageview_update(MessageView *msgview)
{
	SummaryView *summaryview = (SummaryView*)msgview->mainwin->summaryview;

	g_return_if_fail(summaryview != NULL);
	
	if (summaryview->selected) {
		GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
		MsgInfo *msginfo = gtk_ctree_node_get_row_data(ctree, 
						      summaryview->selected);
		g_return_if_fail(msginfo != NULL);

		messageview_show(msgview, msginfo, 
				 msgview->all_headers);
	} 
}

void messageview_quote_color_set(void)
{
}

void messageview_set_font(MessageView *messageview)
{
}

TextView *messageview_get_current_textview(MessageView *messageview)
{
	TextView *text = NULL;

	text = messageview->mimeview->textview;

	return text;
}

MimeInfo *messageview_get_selected_mime_part(MessageView *messageview)
{
	return mimeview_get_selected_part(messageview->mimeview);
}

void messageview_copy_clipboard(MessageView *messageview)
{
	TextView *text;

	text = messageview_get_current_textview(messageview);
	if (text) {
		GtkTextView *textview = GTK_TEXT_VIEW(text->text);
		GtkTextBuffer *buffer = gtk_text_view_get_buffer(textview);
		GtkClipboard *clipboard
			= gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);

		gtk_text_buffer_copy_clipboard(buffer, clipboard);
	}
}

void messageview_select_all(MessageView *messageview)
{
	TextView *text;

	text = messageview_get_current_textview(messageview);
	if (text)
		gtk_editable_select_region(GTK_EDITABLE(text->text), 0, -1);
}

void messageview_set_position(MessageView *messageview, gint pos)
{
	TextView *text;

	text = messageview_get_current_textview(messageview);
	if (text)
		textview_set_position(text, pos);
}

gboolean messageview_search_string(MessageView *messageview, const gchar *str,
				   gboolean case_sens)
{
	TextView *text;

	text = messageview_get_current_textview(messageview);
	if (text)
		return textview_search_string(text, str, case_sens);
	return FALSE;
}

gboolean messageview_search_string_backward(MessageView *messageview,
					    const gchar *str,
					    gboolean case_sens)
{
	TextView *text;

	text = messageview_get_current_textview(messageview);
		return textview_search_string_backward(text,
						       str, case_sens);
	return FALSE;
}

gboolean messageview_is_visible(MessageView *messageview)
{
	return messageview->visible;
}

void messageview_save_as(MessageView *messageview)
{
	gchar *filename = NULL;
	MsgInfo *msginfo;
	gchar *src, *dest;

	if (!messageview->msginfo) return;
	msginfo = messageview->msginfo;

	if (msginfo->subject) {
		Xstrdup_a(filename, msginfo->subject, return);
		subst_for_filename(filename);
	}
	dest = filesel_select_file(_("Save as"), filename);
	if (!dest) return;
	if (is_file_exist(dest)) {
		AlertValue aval;

		aval = alertpanel(_("Overwrite"),
				  _("Overwrite existing file?"),
				  _("OK"), _("Cancel"), NULL);
		if (G_ALERTDEFAULT != aval) return;
	}

	src = procmsg_get_message_file(msginfo);
	if (copy_file(src, dest, TRUE) < 0) {
		alertpanel_error(_("Can't save the file `%s'."),
				 g_basename(dest));
	}
	g_free(src);
}

static void messageview_destroy_cb(GtkWidget *widget, MessageView *messageview)
{
	messageview_destroy(messageview);
}

static void messageview_size_allocate_cb(GtkWidget *widget,
					 GtkAllocation *allocation)
{
	g_return_if_fail(allocation != NULL);

	prefs_common.msgwin_width  = allocation->width;
	prefs_common.msgwin_height = allocation->height;
}

static gboolean key_pressed(GtkWidget *widget, GdkEventKey *event,
			MessageView *messageview)
{
	if (event && event->keyval == GDK_Escape && messageview->window)
		gtk_widget_destroy(messageview->window);
	return FALSE;
}

void messageview_toggle_view_real(MessageView *messageview)
{
	MainWindow *mainwin = messageview->mainwin;
	union CompositeWin *cwin = &mainwin->win;
	GtkWidget *vpaned = NULL;
	GtkWidget *container = NULL;
	GtkItemFactory *ifactory = gtk_item_factory_from_widget(mainwin->menubar);
	
	switch (mainwin->type) {
	case SEPARATE_NONE:
		vpaned = cwin->sep_none.vpaned;
		container = cwin->sep_none.hpaned;
		break;
	case SEPARATE_FOLDER:
		vpaned = cwin->sep_folder.vpaned;
		container = mainwin->vbox_body;
		break;
	case SEPARATE_MESSAGE:
	case SEPARATE_BOTH:
		return;
	}

	if (vpaned->parent != NULL) {
		gtk_widget_ref(vpaned);
		gtkut_container_remove(GTK_CONTAINER(container), vpaned);
		gtk_widget_reparent(GTK_WIDGET_PTR(messageview), container);
		menu_set_sensitive(ifactory, "/View/Expand Summary View", FALSE);
	} else {
		gtk_widget_reparent(GTK_WIDGET_PTR(messageview), vpaned);
		gtk_container_add(GTK_CONTAINER(container), vpaned);
		gtk_widget_unref(vpaned);
		menu_set_sensitive(ifactory, "/View/Expand Summary View", TRUE);
		gtk_widget_grab_focus(GTK_WIDGET(mainwin->summaryview->ctree));
	}
}

static void return_receipt_show(NoticeView *noticeview, MsgInfo *msginfo)
{
	noticeview_set_text(noticeview, _("This message asks for a return receipt"));
	noticeview_set_button_text(noticeview, _("Send receipt"));
	noticeview_set_button_press_callback(noticeview,
					     GTK_SIGNAL_FUNC(return_receipt_send_clicked),
					     (gpointer) msginfo);
	noticeview_show(noticeview);
}

static void return_receipt_send_clicked(NoticeView *noticeview, MsgInfo *msginfo)
{
	MsgInfo *tmpmsginfo;
	gchar *file;

	file = procmsg_get_message_file_path(msginfo);
	if (!file) {
		g_warning("can't get message file path.\n");
		return;
	}

	tmpmsginfo = procheader_parse_file(file, msginfo->flags, TRUE, TRUE);
	tmpmsginfo->folder = msginfo->folder;
	tmpmsginfo->msgnum = msginfo->msgnum;

	if (disposition_notification_send(tmpmsginfo) >= 0) {
		procmsg_msginfo_set_flags(msginfo, MSG_RETRCPT_SENT, 0);
		noticeview_hide(noticeview);
	}		

	procmsg_msginfo_free(tmpmsginfo);
	g_free(file);
}

static void select_account_cb(GtkWidget *w, gpointer data)
{
	*(gint*)data = GPOINTER_TO_INT(gtk_object_get_user_data(GTK_OBJECT(w)));
}
	
static PrefsAccount *select_account_from_list(GList *ac_list)
{
	GtkWidget *optmenu;
	GtkWidget *menu;
	gint account_id;

	g_return_val_if_fail(ac_list != NULL, NULL);
	g_return_val_if_fail(ac_list->data != NULL, NULL);
	
	optmenu = gtk_option_menu_new();
	menu = gtkut_account_menu_new(ac_list, select_account_cb, &account_id);
	if (!menu)
		return NULL;
	gtk_option_menu_set_menu(GTK_OPTION_MENU(optmenu), menu);
	gtk_option_menu_set_history(GTK_OPTION_MENU(optmenu), 0);
	account_id = ((PrefsAccount *) ac_list->data)->account_id;
	if (alertpanel_with_widget(
				_("Return Receipt Notification"),
				_("The message was sent to several of your "
				  "accounts.\n"
				  "Please choose which account do you want to "
				  "use for sending the receipt notification:"),
			        _("Send Notification"), _("+Cancel"), NULL,
			        optmenu) != G_ALERTDEFAULT)
		return NULL;
	return account_find_from_id(account_id);
}

/* 
 * \brief return selected messageview text, when nothing is 
 * 	  selected and message was filtered, return complete text
 *
 * \param  pointer to Messageview 
 *
 * \return pointer to text (needs to be free'd by calling func)
 */
gchar *messageview_get_selection(MessageView *msgview)
{
	TextView *textview;
	gchar *text = NULL;
	GtkTextView *edit = NULL;
	GtkTextBuffer *textbuf;
	gint body_pos = 0;
	
	g_return_val_if_fail(msgview != NULL, NULL);

	textview = messageview_get_current_textview(msgview);
	g_return_val_if_fail(textview != NULL, NULL);

	edit = GTK_EDITABLE(textview->text);
	g_return_val_if_fail(edit != NULL, NULL);
	body_pos = textview->body_pos;

	textbuf = gtk_text_view_get_buffer(edit);

	if (gtk_text_buffer_get_selection_bounds(textbuf, NULL, NULL))
		return gtkut_text_view_get_selection(edit);
	else if (msgview->filtered) {
		GtkTextIter start_iter, end_iter;
		gtk_text_buffer_get_iter_at_offset(textbuf, &start_iter, body_pos);
		gtk_text_buffer_get_end_iter(textbuf, &end_iter);
		gtk_text_buffer_get_text(textbuf, &start_iter, &end_iter, FALSE);
	} else
		text = NULL;

	return text;
}

static void save_as_cb(gpointer data, guint action, GtkWidget *widget)
{
	MessageView *messageview = (MessageView *)data;
	messageview_save_as(messageview);
}

static void print_cb(gpointer data, guint action, GtkWidget *widget)
{
	MessageView *messageview = (MessageView *)data;
	gchar *cmdline;
	gchar *p;

	if (!messageview->msginfo) return;

	cmdline = input_dialog(_("Print"),
			       _("Enter the print command line:\n"
				 "(`%s' will be replaced with file name)"),
			       prefs_common.print_cmd);
	if (!cmdline) return;
	if (!(p = strchr(cmdline, '%')) || *(p + 1) != 's' ||
	    strchr(p + 2, '%')) {
		alertpanel_error(_("Print command line is invalid:\n`%s'"),
				 cmdline);
		g_free(cmdline);
		return;
	}

	procmsg_print_message(messageview->msginfo, cmdline);
	g_free(cmdline);
}

static void close_cb(gpointer data, guint action, GtkWidget *widget)
{
	MessageView *messageview = (MessageView *)data;
	gtk_widget_destroy(messageview->window);
}

static void copy_cb(gpointer data, guint action, GtkWidget *widget)
{
	MessageView *messageview = (MessageView *)data;
	messageview_copy_clipboard(messageview);
}

static void allsel_cb(gpointer data, guint action, GtkWidget *widget)
{
	MessageView *messageview = (MessageView *)data;
	messageview_select_all(messageview);
}

static void search_cb(gpointer data, guint action, GtkWidget *widget)
{
	MessageView *messageview = (MessageView *)data;
	message_search(messageview);
}

static void set_charset_cb(gpointer data, guint action, GtkWidget *widget)
{
	MessageView *messageview = (MessageView *)data;
	const gchar *charset;

	if (GTK_CHECK_MENU_ITEM(widget)->active) {
		charset = conv_get_charset_str((CharSet)action);
		g_free(messageview->forced_charset);
		messageview->forced_charset = g_strdup(charset);
		messageview_show(messageview, messageview->msginfo, FALSE);
	}
}

static void view_source_cb(gpointer data, guint action, GtkWidget *widget)
{
	MessageView *messageview = (MessageView *)data;
	SourceWindow *srcwin;

	if (!messageview->msginfo) return;

	srcwin = source_window_create();
	source_window_show_msg(srcwin, messageview->msginfo);
	source_window_show(srcwin);
}

static void show_all_header_cb(gpointer data, guint action, GtkWidget *widget)
{
	MessageView *messageview = (MessageView *)data;
	MsgInfo *msginfo = messageview->msginfo;

	if (!msginfo) return;
	messageview->msginfo = NULL;
	messageview_show(messageview, msginfo,
			 GTK_CHECK_MENU_ITEM(widget)->active);
	procmsg_msginfo_free(msginfo);
}

static void compose_cb(gpointer data, guint action, GtkWidget *widget)
{
	MessageView *messageview = (MessageView *)data;
	PrefsAccount *ac = NULL;
	FolderItem *item = NULL;

	if (messageview->msginfo)
		item = messageview->msginfo->folder;

	if (item) {
		ac = account_find_from_item(item);
		if (ac && ac->protocol == A_NNTP &&
		    item->stype == F_NEWS) {
			compose_new(ac, item->path, NULL);
			return;
		}
	}

	compose_new(ac, NULL, NULL);
}

static void reply_cb(gpointer data, guint action, GtkWidget *widget)
{
	MessageView *messageview = (MessageView *)data;
	GSList *mlist = NULL;
	MsgInfo *msginfo;
	gchar *text = NULL;
	ComposeMode mode = (ComposeMode)action;
	TextView *textview;

	msginfo = messageview->msginfo;
	mlist = g_slist_append(NULL, msginfo);

	textview = messageview_get_current_textview(messageview);
	text = gtkut_editable_get_selection
		(GTK_EDITABLE(textview->text));
	if (text && *text == '\0') {
		g_free(text);
		text = NULL;
	}

	switch (mode) {
	case COMPOSE_REPLY:
		compose_reply(msginfo, prefs_common.reply_with_quote,
		    	      FALSE, prefs_common.default_reply_list, FALSE, text);
		break;
	case COMPOSE_REPLY_WITH_QUOTE:
		compose_reply(msginfo, TRUE, FALSE, prefs_common.default_reply_list, FALSE, text);
		break;
	case COMPOSE_REPLY_WITHOUT_QUOTE:
		compose_reply(msginfo, FALSE, FALSE, prefs_common.default_reply_list, FALSE, NULL);
		break;
	case COMPOSE_REPLY_TO_SENDER:
		compose_reply(msginfo, prefs_common.reply_with_quote,
			      FALSE, FALSE, TRUE, text);
		break;
	case COMPOSE_FOLLOWUP_AND_REPLY_TO:
		compose_followup_and_reply_to(msginfo,
					      prefs_common.reply_with_quote,
					      FALSE, FALSE, text);
		break;
	case COMPOSE_REPLY_TO_SENDER_WITH_QUOTE:
		compose_reply(msginfo, TRUE, FALSE, FALSE, TRUE, text);
		break;
	case COMPOSE_REPLY_TO_SENDER_WITHOUT_QUOTE:
		compose_reply(msginfo, FALSE, FALSE, FALSE, TRUE, NULL);
		break;
	case COMPOSE_REPLY_TO_ALL:
		compose_reply(msginfo, prefs_common.reply_with_quote,
			      TRUE, FALSE, FALSE, text);
		break;
	case COMPOSE_REPLY_TO_ALL_WITH_QUOTE:
		compose_reply(msginfo, TRUE, TRUE, FALSE, FALSE, text);
		break;
	case COMPOSE_REPLY_TO_ALL_WITHOUT_QUOTE:
		compose_reply(msginfo, FALSE, TRUE, FALSE, FALSE, NULL);
		break;
	case COMPOSE_REPLY_TO_LIST:
		compose_reply(msginfo, prefs_common.reply_with_quote,
			      FALSE, TRUE, FALSE, text);
		break;
	case COMPOSE_REPLY_TO_LIST_WITH_QUOTE:
		compose_reply(msginfo, TRUE, FALSE, TRUE, FALSE, text);
		break;
	case COMPOSE_REPLY_TO_LIST_WITHOUT_QUOTE:
		compose_reply(msginfo, FALSE, FALSE, TRUE, FALSE, NULL);
		break;
	case COMPOSE_FORWARD:
		if (prefs_common.forward_as_attachment) {
			compose_reply_mode(COMPOSE_FORWARD_AS_ATTACH, mlist, text);
			return;
		} else {
			compose_reply_mode(COMPOSE_FORWARD_INLINE, mlist, text);
			return;
		}
		break;
	case COMPOSE_FORWARD_INLINE:
		compose_forward(NULL, msginfo, FALSE, text, FALSE);
		break;
	case COMPOSE_FORWARD_AS_ATTACH:
		compose_forward_multiple(NULL, mlist);
		break;
	case COMPOSE_REDIRECT:
		compose_redirect(NULL, msginfo);
		break;
	default:
		g_warning("compose_reply(): invalid Compose Mode: %d\n", mode);
	}

	/* summary_set_marks_selected(summaryview); */
	g_free(text);
	g_slist_free(mlist);
}

static void reedit_cb(gpointer data, guint action, GtkWidget *widget)
{
	MessageView *messageview = (MessageView *)data;
	MsgInfo *msginfo;

	if (!messageview->msginfo) return;
	msginfo = messageview->msginfo;
	if (!msginfo->folder) return;
	if (msginfo->folder->stype != F_OUTBOX &&
	    msginfo->folder->stype != F_DRAFT &&
	    msginfo->folder->stype != F_QUEUE) return;

	compose_reedit(msginfo);
}

static void addressbook_open_cb(gpointer data, guint action, GtkWidget *widget)
{
	addressbook_open(NULL);
}

static void add_address_cb(gpointer data, guint action, GtkWidget *widget)
{
	MessageView *messageview = (MessageView *)data;
	MsgInfo *msginfo;
	gchar *from;

	if (!messageview->msginfo) return;
	msginfo = messageview->msginfo;
	Xstrdup_a(from, msginfo->from, return);
	eliminate_address_comment(from);
	extract_address(from);
	addressbook_add_contact(msginfo->fromname, from, NULL);
}

static void create_filter_cb(gpointer data, guint action, GtkWidget *widget)
{
	MessageView *messageview = (MessageView *)data;
	FolderItem * item;
	
	if (!messageview->msginfo) return;
	
	item = messageview->msginfo->folder;
	summary_msginfo_filter_open(item,  messageview->msginfo,
				    (PrefsFilterType)action, 0);
}

static void create_processing_cb(gpointer data, guint action,
				 GtkWidget *widget)
{
	MessageView *messageview = (MessageView *)data;
	FolderItem * item;
	
	if (!messageview->msginfo) return;
	
	item = messageview->msginfo->folder;
	summary_msginfo_filter_open(item,  messageview->msginfo,
				    (PrefsFilterType)action, 1);
}

static void about_cb(gpointer data, guint action, GtkWidget *widget)
{
	about_show();
}

static gboolean messageview_update_msg(gpointer source, gpointer data)
{
	MsgInfoUpdate *msginfo_update = (MsgInfoUpdate *) source;
	MessageView *messageview = (MessageView *)data;

	if (messageview->msginfo != msginfo_update->msginfo)
		return FALSE;

	if (msginfo_update->flags & MSGINFO_UPDATE_DELETED) {
		messageview_clear(messageview);
		messageview_update(messageview);
	}

	return FALSE;
}
