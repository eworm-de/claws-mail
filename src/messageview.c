/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2007 Hiroyuki Yamamoto and the Claws Mail team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 * 
 */

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkcontainer.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtktext.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

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
#include "mainwindow.h"
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
#include "send_message.h"
#include "stock_pixmap.h"
#include "hooks.h"
#include "filtering.h"
#include "partial_download.h"
#include "gedit-print.h"
#include "uri_opener.h"
#include "inc.h"
#include "log.h"
#include "combobox.h"

static GList *messageview_list = NULL;

static gint messageview_delete_cb	(GtkWidget		*widget,
					 GdkEventAny		*event,
					 MessageView		*messageview);
static void messageview_size_allocate_cb(GtkWidget	*widget,
					 GtkAllocation	*allocation);
static gboolean key_pressed		(GtkWidget	*widget,
					 GdkEventKey	*event,
					 MessageView	*messageview);

static void return_receipt_show		(NoticeView     *noticeview, 
				         MsgInfo        *msginfo);	
static void return_receipt_send_clicked (NoticeView	*noticeview, 
                                         MsgInfo        *msginfo);
static void partial_recv_show		(NoticeView     *noticeview, 
				         MsgInfo        *msginfo);	
static void partial_recv_dload_clicked 	(NoticeView	*noticeview, 
                                         MsgInfo        *msginfo);
static void partial_recv_del_clicked 	(NoticeView	*noticeview, 
                                         MsgInfo        *msginfo);
static void partial_recv_unmark_clicked (NoticeView	*noticeview, 
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
static void set_decode_cb		(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void view_source_cb		(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void show_all_header_cb		(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void msg_hide_quotes_cb		(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);

static void compose_cb			(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void reply_cb			(gpointer	 data,
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
static void open_urls_cb		(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);

static void about_cb			(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void messageview_update		(MessageView	*msgview,
					 MsgInfo	*old_msginfo);
static gboolean messageview_update_msg	(gpointer source, gpointer data);

static GList *msgview_list = NULL;
static GtkItemFactoryEntry msgview_entries[] =
{
	{N_("/_File"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_File/_Save as..."),	"<control>S", save_as_cb, 0, NULL},
	{N_("/_File/_Print..."),	"<control>P", print_cb, 0, NULL},
	{N_("/_File/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_File/_Close"),		"<control>W", close_cb, 0, NULL},

	{N_("/_Edit"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_Edit/_Copy"),		"<control>C", copy_cb, 0, NULL},
	{N_("/_Edit/Select _all"),	"<control>A", allsel_cb, 0, NULL},
	{N_("/_Edit/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_Edit/_Find in current message..."),
					"<control>F", search_cb, 0, NULL},

	{N_("/_View"),			NULL, NULL, 0, "<Branch>"},

#define ENC_SEPARATOR \
	{N_("/_View/Character _encoding/---"),	NULL, NULL, 0, "<Separator>"}
#define ENC_ACTION(action) \
	NULL, set_charset_cb, action, "/View/Character encoding/Auto detect"

	{N_("/_View/Character _encoding"),	NULL, NULL, 0, "<Branch>"},
	{N_("/_View/Character _encoding/_Auto detect"),
					NULL, set_charset_cb, C_AUTO, "<RadioItem>"},
	ENC_SEPARATOR,
	{N_("/_View/Character _encoding/7bit ascii (US-ASC_II)"),
	 ENC_ACTION(C_US_ASCII)},

	{N_("/_View/Character _encoding/Unicode (_UTF-8)"),
	 ENC_ACTION(C_UTF_8)},
	ENC_SEPARATOR,
	{N_("/_View/Character _encoding/Western European (ISO-8859-_1)"),
	 ENC_ACTION(C_ISO_8859_1)},
	{N_("/_View/Character _encoding/Western European (ISO-8859-15)"),
	 ENC_ACTION(C_ISO_8859_15)},
	{N_("/_View/Character _encoding/Western European (Windows-1252)"),
	 ENC_ACTION(C_WINDOWS_1252)},
	ENC_SEPARATOR,
	{N_("/_View/Character _encoding/Central European (ISO-8859-_2)"),
	 ENC_ACTION(C_ISO_8859_2)},
	ENC_SEPARATOR,
	{N_("/_View/Character _encoding/_Baltic (ISO-8859-13)"),
	 ENC_ACTION(C_ISO_8859_13)},
	{N_("/_View/Character _encoding/Baltic (ISO-8859-_4)"),
	 ENC_ACTION(C_ISO_8859_4)},
	ENC_SEPARATOR,
	{N_("/_View/Character _encoding/Greek (ISO-8859-_7)"),
	 ENC_ACTION(C_ISO_8859_7)},
	ENC_SEPARATOR,
	{N_("/_View/Character _encoding/Hebrew (ISO-8859-_8)"),
	 ENC_ACTION(C_ISO_8859_8)},
	{N_("/_View/Character _encoding/Hebrew (Windows-1255)"),
	 ENC_ACTION(C_CP1255)},
	ENC_SEPARATOR,
	{N_("/_View/Character _encoding/Arabic (ISO-8859-_6)"),
	 ENC_ACTION(C_ISO_8859_6)},
	{N_("/_View/Character _encoding/Arabic (Windows-1256)"),
	 ENC_ACTION(C_CP1256)},
	ENC_SEPARATOR,
	{N_("/_View/Character _encoding/Turkish (ISO-8859-_9)"),
	 ENC_ACTION(C_ISO_8859_9)},
	ENC_SEPARATOR,
	{N_("/_View/Character _encoding/Cyrillic (ISO-8859-_5)"),
	 ENC_ACTION(C_ISO_8859_5)},
	{N_("/_View/Character _encoding/Cyrillic (KOI8-_R)"),
	 ENC_ACTION(C_KOI8_R)},
	{N_("/_View/Character _encoding/Cyrillic (KOI8-U)"),
	 ENC_ACTION(C_KOI8_U)},
	{N_("/_View/Character _encoding/Cyrillic (Windows-1251)"),
	 ENC_ACTION(C_CP1251)},
	ENC_SEPARATOR,
	{N_("/_View/Character _encoding/Japanese (ISO-2022-_JP)"),
	 ENC_ACTION(C_ISO_2022_JP)},
	{N_("/_View/Character _encoding/Japanese (ISO-2022-JP-2)"),
	 ENC_ACTION(C_ISO_2022_JP_2)},
	{N_("/_View/Character _encoding/Japanese (_EUC-JP)"),
	 ENC_ACTION(C_EUC_JP)},
	{N_("/_View/Character _encoding/Japanese (_Shift__JIS)"),
	 ENC_ACTION(C_SHIFT_JIS)},
	ENC_SEPARATOR,
	{N_("/_View/Character _encoding/Simplified Chinese (_GB2312)"),
	 ENC_ACTION(C_GB2312)},
	{N_("/_View/Character _encoding/Simplified Chinese (GBK)"),
	 ENC_ACTION(C_GBK)},
	{N_("/_View/Character _encoding/Traditional Chinese (_Big5)"),
	 ENC_ACTION(C_BIG5)},
	{N_("/_View/Character _encoding/Traditional Chinese (EUC-_TW)"),
	 ENC_ACTION(C_EUC_TW)},
	{N_("/_View/Character _encoding/Chinese (ISO-2022-_CN)"),
	 ENC_ACTION(C_ISO_2022_CN)},
	ENC_SEPARATOR,
	{N_("/_View/Character _encoding/Korean (EUC-_KR)"),
	 ENC_ACTION(C_EUC_KR)},
	{N_("/_View/Character _encoding/Korean (ISO-2022-KR)"),
	 ENC_ACTION(C_ISO_2022_KR)},
	ENC_SEPARATOR,
	{N_("/_View/Character _encoding/Thai (TIS-620)"),
	 ENC_ACTION(C_TIS_620)},
	{N_("/_View/Character _encoding/Thai (Windows-874)"),
	 ENC_ACTION(C_WINDOWS_874)},

#undef ENC_SEPARATOR
#undef ENC_ACTION

#define DEC_SEPARATOR \
	{N_("/_View/Decode/---"),		NULL, NULL, 0, "<Separator>"}
#define DEC_ACTION(action) \
	 NULL, set_decode_cb, action, "/View/Decode/Auto detect"
	{N_("/_View/Decode"),		NULL, NULL, 0, "<Branch>"},
	{N_("/_View/Decode/_Auto detect"),
	 NULL, set_decode_cb, 0, "<RadioItem>"},
	{N_("/_View/Decode/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_View/Decode/_8bit"), 		DEC_ACTION(ENC_8BIT)},
	{N_("/_View/Decode/_Quoted printable"),	DEC_ACTION(ENC_QUOTED_PRINTABLE)},
	{N_("/_View/Decode/_Base64"), 		DEC_ACTION(ENC_BASE64)},
	{N_("/_View/Decode/_Uuencode"),		DEC_ACTION(ENC_X_UUENCODE)},

#undef DEC_SEPARATOR
#undef DEC_ACTION

	{N_("/_View/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_View/Mess_age source"),	"<control>U", view_source_cb, 0, NULL},
	{N_("/_View/Show all _headers"),"<control>H", show_all_header_cb, 0, "<ToggleItem>"},
	{N_("/_View/Quotes"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_View/Quotes/_Fold all"),		"<control><shift>Q", msg_hide_quotes_cb, 1, "<ToggleItem>"},
	{N_("/_View/Quotes/Fold from level _2"),NULL, msg_hide_quotes_cb, 2, "<ToggleItem>"},
	{N_("/_View/Quotes/Fold from level _3"),NULL, msg_hide_quotes_cb, 3, "<ToggleItem>"},

	{N_("/_Message"),		NULL, NULL, 0, "<Branch>"},
	{N_("/_Message/Compose _new message"),
					"<control>M", compose_cb, 0, NULL},
	{N_("/_Message/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_Message/_Reply"),	"<control>R", reply_cb, COMPOSE_REPLY, NULL},
	{N_("/_Message/Repl_y to/_all"),
					"<control><shift>R", reply_cb, COMPOSE_REPLY_TO_ALL, NULL},
	{N_("/_Message/Repl_y to/_sender"),
					NULL, reply_cb, COMPOSE_REPLY_TO_SENDER, NULL},
	{N_("/_Message/Repl_y to/mailing _list"),
					"<control>L", reply_cb, COMPOSE_REPLY_TO_LIST, NULL},
	{N_("/_Message/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_Message/_Forward"),	"<control><alt>F", reply_cb, COMPOSE_FORWARD_INLINE, NULL},
	{N_("/_Message/For_ward as attachment"),
					NULL, reply_cb, COMPOSE_FORWARD_AS_ATTACH, NULL},
	{N_("/_Message/Redirec_t"),	NULL, reply_cb, COMPOSE_REDIRECT, NULL},

	{N_("/_Tools"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_Tools/_Address book"),	"<control><shift>A", addressbook_open_cb, 0, NULL},
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
	{N_("/_Tools/Create processing rule"),
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
	{N_("/_Tools/List _URLs..."),	"<shift><control>U", open_urls_cb, 0, NULL},
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

	messageview->vbox        = vbox;
	messageview->new_window  = FALSE;
	messageview->window      = NULL;
	messageview->headerview  = headerview;
	messageview->mimeview    = mimeview;
	messageview->noticeview = noticeview;
	messageview->mainwin    = mainwin;

	messageview->statusbar     = NULL;
	messageview->statusbar_cid = 0;

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
	GtkWidget *statusbar = NULL;
	guint n_menu_entries;

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(vbox);
	gtk_container_add(GTK_CONTAINER(window), vbox);	

	n_menu_entries = sizeof(msgview_entries) / sizeof(msgview_entries[0]);
	menubar = menubar_create(window, msgview_entries,
				 n_menu_entries, "<MessageView>", msgview);
	gtk_widget_show(menubar);
	gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, TRUE, 0);

	if (prefs_common.toolbar_detachable) {
		handlebox = gtk_handle_box_new();
	} else {
		handlebox = gtk_hbox_new(FALSE, 0);
	}
	gtk_box_pack_start(GTK_BOX(vbox), handlebox, FALSE, FALSE, 0);
	gtk_widget_realize(handlebox);
#ifdef MAEMO
	msgview->toolbar = toolbar_create(TOOLBAR_MSGVIEW, window,
					  (gpointer)msgview);
	msgview->statusbar = NULL;
	msgview->statusbar_cid = 0;
#else
	msgview->toolbar = toolbar_create(TOOLBAR_MSGVIEW, handlebox,
					  (gpointer)msgview);
	statusbar = gtk_statusbar_new();
	gtk_widget_show(statusbar);
	gtk_box_pack_end(GTK_BOX(vbox), statusbar, FALSE, FALSE, 0);
	msgview->statusbar = statusbar;
	msgview->statusbar_cid = gtk_statusbar_get_context_id
		(GTK_STATUSBAR(statusbar), "Message View");
#endif


	msgview->handlebox = handlebox;
	msgview->menubar   = menubar;

	gtk_container_add(GTK_CONTAINER(vbox),
			  GTK_WIDGET_PTR(msgview));

	messageview_update_actions_menu(msgview);

	msgview_list = g_list_append(msgview_list, msgview);
}

static MessageView *messageview_create_with_new_window_visible(MainWindow *mainwin, gboolean show)
{
	MessageView *msgview;
	GtkWidget *window;
	static GdkGeometry geometry;

	window = gtkut_window_new(GTK_WINDOW_TOPLEVEL, "messageview");
	gtk_window_set_title(GTK_WINDOW(window), _("Claws Mail - Message View"));
	gtk_window_set_resizable(GTK_WINDOW(window), TRUE);

	if (!geometry.min_height) {
		geometry.min_width = 320;
		geometry.min_height = 200;
	}
	gtk_window_set_geometry_hints(GTK_WINDOW(window), NULL, &geometry,
				      GDK_HINT_MIN_SIZE);

	gtk_widget_set_size_request(window, prefs_common.msgwin_width,
				    prefs_common.msgwin_height);

	msgview = messageview_create(mainwin);

	g_signal_connect(G_OBJECT(window), "size_allocate",
			 G_CALLBACK(messageview_size_allocate_cb),
			 msgview);
	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(messageview_delete_cb), msgview);
#ifdef MAEMO
	maemo_connect_key_press_to_mainwindow(GTK_WINDOW(window));
#else
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(key_pressed), msgview);
#endif
	messageview_add_toolbar(msgview, window);

	if (show) {
		gtk_widget_grab_focus(msgview->mimeview->textview->text);
		gtk_widget_show(window);
	} else {
		gtk_widget_realize(window);
	}

	msgview->new_window = TRUE;
	msgview->window = window;
	msgview->visible = TRUE;

	toolbar_set_style(msgview->toolbar->toolbar, msgview->handlebox, 
			  prefs_common.toolbar_style);
	messageview_init(msgview);

	return msgview;
}

MessageView *messageview_create_with_new_window(MainWindow *mainwin)
{
	return messageview_create_with_new_window_visible(mainwin, TRUE);
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
	PrefsAccount *account = NULL;
	gint ok;
	gchar *to;
	FolderItem *queue, *outbox;
	gint num;
	gchar *path;
        gchar *addr;
        gchar *addrp;
	gchar *foo = NULL;
	gboolean queued_removed = FALSE;
	
	if (!msginfo->extradata)
		return -1;
	if (!msginfo->extradata->returnreceiptto && 
	    !msginfo->extradata->dispositionnotificationto) 
		return -1;

	/* RFC2298: Test for Return-Path */
	if (msginfo->extradata->dispositionnotificationto)
		to = msginfo->extradata->dispositionnotificationto;
	else
		to = msginfo->extradata->returnreceiptto;

	ok = procheader_get_header_from_msginfo(msginfo, buf, sizeof(buf),
				"Return-Path:");
	if (ok == 0) {
		gchar *to_addr = g_strdup(to);
		extract_address(to_addr);
		extract_address(buf);
		ok = strcasecmp(to_addr, buf);
		g_free(to_addr);
	} else {
		strncpy(buf, _("<No Return-Path found>"), 
				sizeof(buf));
	}
	
	if (ok != 0) {
		AlertValue val;
		gchar *message;
		message = g_markup_printf_escaped(
		  _("The notification address to which the return receipt is\n"
		    "to be sent does not correspond to the return path:\n"
		    "Notification address: %s\n"
		    "Return path: %s\n"
		    "It is advised to not to send the return receipt."),
		  to, buf);
		val = alertpanel_full(_("Warning"), message,
				_("_Don't Send"), _("_Send"), NULL, FALSE,
				NULL, ALERT_WARNING, G_ALERTDEFAULT);
		g_free(message);				
		if (val != G_ALERTALTERNATE)
			return -1;
	}

	ac_list = account_find_all_from_address(NULL, msginfo->to);
	ac_list = account_find_all_from_address(ac_list, msginfo->cc);

	if (ac_list == NULL) {
		AlertValue val = 
		alertpanel_full(_("Warning"),
		  _("This message is asking for a return receipt notification\n"
		    "but according to its 'To:' and 'CC:' headers it was not\n"
		    "officially addressed to you.\n"
		    "It is advised to not to send the return receipt."),
		  _("_Don't Send"), _("_Send"), NULL, FALSE,
		  NULL, ALERT_WARNING, G_ALERTDEFAULT);
		if (val != G_ALERTALTERNATE)
			return -1;
	}

	if (g_list_length(ac_list) > 1) {
		if ((account = select_account_from_list(ac_list)) == NULL)
			return -1;
	}
	else if (ac_list != NULL)
		account = (PrefsAccount *) ac_list->data;
	g_list_free(ac_list);

	if (account == NULL)
		account = account_get_default();
	if (!account || account->protocol == A_NNTP) {
		alertpanel_error(_("Account for sending mail is not specified.\n"
				   "Please select a mail account before sending."));
		return -1;
	}

	/* write to temporary file */
	g_snprintf(tmp, sizeof(tmp), "%s%ctmpmsg%p",
		   get_rc_dir(), G_DIR_SEPARATOR, msginfo);

	if ((fp = g_fopen(tmp, "wb")) == NULL) {
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

	fprintf(fp, "X-Claws-End-Special-Headers: 1\n");
	
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
	generate_msgid(buf, sizeof(buf));
	fprintf(fp, "Message-ID: <%s>\n", buf);

	if (fclose(fp) == EOF) {
		FILE_OP_ERROR(tmp, "fclose");
		g_unlink(tmp);
		return -1;
	}

	/* put it in queue */
	queue = account_get_special_folder(account, F_QUEUE);
	if (!queue) queue = folder_get_default_queue();
	if (!queue) {
		g_warning("can't find queue folder\n");
		g_unlink(tmp);
		return -1;
	}
	folder_item_scan(queue);
	if ((num = folder_item_add_msg(queue, tmp, NULL, TRUE)) < 0) {
		g_warning("can't queue the message\n");
		g_unlink(tmp);
		return -1;
	}
		
	if (prefs_common.work_offline && 
	    !inc_offline_should_override(TRUE,
		_("Claws Mail needs network access in order "
		  "to send this email.")))
		return 0;

	/* send it */
	path = folder_item_fetch_msg(queue, num);
	ok = procmsg_send_message_queue(path, &foo, queue, num, &queued_removed);
	g_free(path);
	g_free(foo);
	if (ok == 0 && !queued_removed)
		folder_item_remove_msg(queue, num);

	return ok;
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
	gchar *subject = NULL;
	g_return_val_if_fail(msginfo != NULL, -1);

	if (messageview->mimeview->textview &&
	    messageview->mimeview->textview->loading) {
		messageview->mimeview->textview->stop_loading = TRUE;
		return 0;
	}

	if (messageview->toolbar)
		toolbar_set_learn_button
			(messageview->toolbar,
			 MSG_IS_SPAM(msginfo->flags)?LEARN_HAM:LEARN_SPAM);
	else
		toolbar_set_learn_button
			(messageview->mainwin->toolbar,
			 MSG_IS_SPAM(msginfo->flags)?LEARN_HAM:LEARN_SPAM);

	if (messageview->toolbar) {
		if (messageview->toolbar->learn_spam_btn)
			gtk_widget_set_sensitive(
				messageview->toolbar->learn_spam_btn, 
				procmsg_spam_can_learn());
	}
	messageview->updating = TRUE;
	mimeinfo = procmime_scan_message(msginfo);
	messageview->updating = FALSE;
	
	if (messageview->deferred_destroy) {
		messageview_destroy(messageview);
		return 0;
	}

	if (!mimeinfo) {
		textview_show_error(messageview->mimeview->textview);
		return -1;
	}

	while ((encinfo = find_encrypted_part(mimeinfo)) != NULL) {
		debug_print("decrypting message part\n");
		if (privacy_mimeinfo_decrypt(encinfo) < 0) {
			alertpanel_error(_("Couldn't decrypt: %s"),
				privacy_get_error());
			break;
		}
	}
	
	messageview->updating = TRUE;
	file = procmsg_get_message_file_path(msginfo);
	messageview->updating = FALSE;
	
	if (messageview->deferred_destroy) {
		g_free(file);
		messageview_destroy(messageview);
		return 0;
	}

	if (!file) {
		g_warning("can't get message file path.\n");
		procmime_mimeinfo_free_all(mimeinfo);
		textview_show_error(messageview->mimeview->textview);
		return -1;
	}
	
	if (messageview->msginfo != msginfo) {
		procmsg_msginfo_free(messageview->msginfo);
		messageview->msginfo = NULL;
		messageview_set_menu_sensitive(messageview);
		messageview->msginfo = procmsg_msginfo_get_full_info(msginfo);
		if (!messageview->msginfo)
			messageview->msginfo = procmsg_msginfo_copy(msginfo);
	} else {
		messageview->msginfo = NULL;
		messageview_set_menu_sensitive(messageview);
		messageview->msginfo = msginfo;
	}
	headerview_show(messageview->headerview, messageview->msginfo);

	messageview_set_position(messageview, 0);

	textview_set_all_headers(messageview->mimeview->textview, 
			messageview->all_headers);

#ifdef MAEMO
	maemo_window_full_screen_if_needed(GTK_WINDOW(messageview->window));
#endif
	if (messageview->window) {
		gtk_window_set_title(GTK_WINDOW(messageview->window), 
				_("Claws Mail - Message View"));
		GTK_EVENTS_FLUSH();
	}
	mimeview_show_message(messageview->mimeview, mimeinfo, file);
	
#ifndef MAEMO
	messageview_set_position(messageview, 0);
#endif

	if (messageview->window && msginfo->subject) {
		subject = g_strdup(msginfo->subject);
		if (!g_utf8_validate(subject, -1, NULL)) {
			g_free(subject);
			subject = g_malloc(strlen(msginfo->subject)*2 +1);
			conv_localetodisp(subject, strlen(msginfo->subject)*2 +1, 
				msginfo->subject);
		}
		if (g_utf8_validate(subject, -1, NULL))
			gtk_window_set_title(GTK_WINDOW(messageview->window), 
				subject);
		g_free(subject);
	}

	if (msginfo && msginfo->folder) {
		msginfo->folder->last_seen = msginfo->msgnum;	
	}

	main_create_mailing_list_menu(messageview->mainwin, messageview->msginfo);

	if (messageview->msginfo && messageview->msginfo->extradata
	    && messageview->msginfo->extradata->partial_recv)
		partial_recv_show(messageview->noticeview, 
				  messageview->msginfo);
	else if (messageview->msginfo && messageview->msginfo->extradata &&
	    (messageview->msginfo->extradata->dispositionnotificationto || 
	     messageview->msginfo->extradata->returnreceiptto) &&
	    !MSG_IS_RETRCPT_SENT(messageview->msginfo->flags) &&
	    !prefs_common.never_send_retrcpt)
		return_receipt_show(messageview->noticeview, 
				    messageview->msginfo);
	else 
		noticeview_hide(messageview->noticeview);

	mimeinfo = procmime_mimeinfo_next(mimeinfo);
	if (!all_headers && mimeinfo 
			&& (mimeinfo->type != MIMETYPE_TEXT || 
	    strcasecmp(mimeinfo->subtype, "plain")) 
			&& (mimeinfo->type != MIMETYPE_MULTIPART || 
	    strcasecmp(mimeinfo->subtype, "signed"))) {
	    	if (strcasecmp(mimeinfo->subtype, "html"))
			mimeview_show_part(messageview->mimeview,mimeinfo);
		else if (prefs_common.invoke_plugin_on_html)
			mimeview_select_mimepart_icon(messageview->mimeview,mimeinfo);
	}

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
	if (!messageview)
		return;
	procmsg_msginfo_free(messageview->msginfo);
	messageview->msginfo = NULL;
	messageview->filtered = FALSE;
	mimeview_clear(messageview->mimeview);
	headerview_clear(messageview->headerview);
	noticeview_hide(messageview->noticeview);
}

void messageview_destroy(MessageView *messageview)
{
	debug_print("destroy messageview\n");
	messageview_list = g_list_remove(messageview_list, messageview);

	if (messageview->mainwin->summaryview->messageview == messageview) {
		messageview->mainwin->summaryview->displayed = NULL;
		messageview->mainwin->summaryview->messageview = NULL;
	}
	if (messageview->mainwin->summaryview->ext_messageview == messageview) {
		messageview->mainwin->summaryview->displayed = NULL;
		messageview->mainwin->summaryview->ext_messageview = NULL;
	}
	if (!messageview->deferred_destroy) {
		hooks_unregister_hook(MSGINFO_UPDATE_HOOKLIST,
			      messageview->msginfo_update_callback_id);
	}

	if (messageview->updating) {
		debug_print("uh oh, better not touch that now (fetching)\n");
		messageview->deferred_destroy = TRUE;
		gtk_widget_hide(messageview->window);
		return;
	}
	
	if (messageview->mimeview->textview
	&&  messageview->mimeview->textview->loading) {
		debug_print("uh oh, better not touch that now (loading text)\n");
		messageview->deferred_destroy = TRUE;
		messageview->mimeview->textview->stop_loading = TRUE;
		gtk_widget_hide(messageview->window);
		return;
	}

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

	if (messageview->window)
		gtk_widget_destroy(messageview->window);
	g_free(messageview);
}

void messageview_delete(MessageView *msgview)
{
	MsgInfo *msginfo = NULL;
	FolderItem *trash = NULL;
	PrefsAccount *ac = NULL;

	if (msgview->msginfo && msgview->mainwin && msgview->mainwin->summaryview)
		msginfo = summary_get_selected_msg(msgview->mainwin->summaryview);
	
	/* need a procmsg_msginfo_equal() */
	if (msginfo && msgview->msginfo && 
	    msginfo->msgnum == msgview->msginfo->msgnum && 
	    msginfo->folder == msgview->msginfo->folder) {
		summary_delete_trash(msgview->mainwin->summaryview);
	} else {		
		msginfo = msgview->msginfo;

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

		g_return_if_fail(trash != NULL);

		if (prefs_common.immediate_exec)
			/* TODO: Delete from trash */
			folder_item_move_msg(trash, msginfo);
		else {
			procmsg_msginfo_set_to_folder(msginfo, trash);
			procmsg_msginfo_set_flags(msginfo, MSG_DELETED, 0);
			/* NOTE: does not update to next message in summaryview */
		}
	}
#ifdef MAEMO
	if (msgview->window) {
		messageview_destroy(msgview);
	}
#endif
}

/* 
 * \brief update messageview with currently selected message in summaryview
 *        leave unchanged if summaryview is empty
 * \param pointer to MessageView
 */	
static void messageview_update(MessageView *msgview, MsgInfo *old_msginfo)
{
	SummaryView *summaryview = (SummaryView*)msgview->mainwin->summaryview;

	g_return_if_fail(summaryview != NULL);
	
	if (summaryview->selected) {
		MsgInfo *msginfo = summary_get_selected_msg(summaryview);
		if (msginfo == NULL || msginfo == old_msginfo)
			return;

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
	gchar *text = messageview_get_selection(messageview);
	if (text) {
		gtk_clipboard_set_text(
			gtk_clipboard_get(GDK_SELECTION_CLIPBOARD),
			text, -1);
	}
	g_free(text);
}

void messageview_select_all(MessageView *messageview)
{
	TextView *text;

	text = messageview_get_current_textview(messageview);
	if (text) {
		GtkTextView *textview = GTK_TEXT_VIEW(text->text);
		GtkTextBuffer *buffer;
		GtkTextIter start, end;

		buffer = gtk_text_view_get_buffer(textview);
		gtk_text_buffer_get_bounds(buffer, &start, &end);
		gtk_text_buffer_select_range(buffer, &start, &end);
	}
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

	if (messageview->mimeview->type == MIMEVIEW_VIEWER) {
		MimeViewer *viewer = messageview->mimeview->mimeviewer;
		if (viewer && viewer->text_search) {
			return viewer->text_search(viewer, FALSE, str, case_sens);
		}
	}

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

	if (messageview->mimeview->type == MIMEVIEW_VIEWER) {
		MimeViewer *viewer = messageview->mimeview->mimeviewer;
		if (viewer && viewer->text_search) {
			return viewer->text_search(viewer, TRUE, str, case_sens);
		}
	}

	text = messageview_get_current_textview(messageview);
	if (text)	
		return textview_search_string_backward(text,
						       str, case_sens);
	return FALSE;
}

gboolean messageview_is_visible(MessageView *messageview)
{
	if (messageview == NULL)
		return FALSE;
	return messageview->visible;
}

static void messageview_save_as(MessageView *messageview)
{
	gchar *filename = NULL;
	MsgInfo *msginfo;
	gchar *src, *dest, *tmp;

	if (!messageview->msginfo) return;
	msginfo = messageview->msginfo;

	if (msginfo->subject) {
		Xstrdup_a(filename, msginfo->subject, return);
		subst_for_filename(filename);
	}
	if (filename && !g_utf8_validate(filename, -1, NULL)) {
		gchar *oldstr = filename;
		filename = conv_codeset_strdup(filename,
					       conv_get_locale_charset_str(),
					       CS_UTF_8);
		if (!filename) {
			g_warning("messageview_save_as(): failed to convert character set.");
			filename = g_strdup(oldstr);
		}
		dest = filesel_select_file_save(_("Save as"), filename);
		g_free(filename);
	} else
		dest = filesel_select_file_save(_("Save as"), filename);
	if (!dest) return;
	if (is_file_exist(dest)) {
		AlertValue aval;

		aval = alertpanel(_("Overwrite"),
				  _("Overwrite existing file?"),
				  GTK_STOCK_CANCEL, GTK_STOCK_OK, NULL);
		if (G_ALERTALTERNATE != aval) return;
	}

	src = procmsg_get_message_file(msginfo);
	if (copy_file(src, dest, TRUE) < 0) {
		tmp =  g_path_get_basename(dest);
		alertpanel_error(_("Couldn't save the file '%s'."), tmp);
		g_free(tmp);
	}
	g_free(dest);
	g_free(src);
}

static gint messageview_delete_cb(GtkWidget *widget, GdkEventAny *event,
				  MessageView *messageview)
{
	messageview_destroy(messageview);
	return TRUE;
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
	if (event && event->keyval == GDK_Escape && messageview->window) {
		messageview_destroy(messageview);
		return TRUE;
	}

	if ((event->state & (GDK_MOD1_MASK|GDK_CONTROL_MASK)) != 0)
		return FALSE;

	g_signal_stop_emission_by_name(G_OBJECT(widget),
					"key_press_event");
	mimeview_pass_key_press_event(messageview->mimeview, event);
	return FALSE;
}

static void return_receipt_show(NoticeView *noticeview, MsgInfo *msginfo)
{
	gchar *addr = NULL;
	gboolean from_me = FALSE;
	if (msginfo->folder 
		&& (folder_has_parent_of_type(msginfo->folder, F_QUEUE)
		 || folder_has_parent_of_type(msginfo->folder, F_DRAFT)))
		return;

	addr = g_strdup(msginfo->from);
	if (addr) {
		extract_address(addr);
		if (account_find_from_address(addr)) {
			from_me = TRUE;
		}
		g_free(addr);
	}

	if (from_me) {
		noticeview_set_icon(noticeview, STOCK_PIXMAP_NOTICE_WARN);
		noticeview_set_text(noticeview, _("You asked for a return receipt in this message."));
		noticeview_set_button_text(noticeview, NULL);
		noticeview_set_button_press_callback(noticeview, NULL, NULL);
	} else {
		noticeview_set_icon(noticeview, STOCK_PIXMAP_NOTICE_WARN);
		noticeview_set_text(noticeview, _("This message asks for a return receipt."));
		noticeview_set_button_text(noticeview, _("Send receipt"));
		noticeview_set_button_press_callback(noticeview,
						     G_CALLBACK(return_receipt_send_clicked),
						     (gpointer) msginfo);
	}
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

static void partial_recv_show(NoticeView *noticeview, MsgInfo *msginfo)
{
	gchar *text = NULL;
	gchar *button1 = NULL;
	gchar *button2 = NULL;
	void  *button1_cb = NULL;
	void  *button2_cb = NULL;

	if (!msginfo->extradata)
		return;
	if (!partial_msg_in_uidl_list(msginfo)) {
		text = g_strdup_printf(_("This message has been partially "
				"retrieved,\nand has been deleted from the "
				"server."));
	} else {
		switch (msginfo->planned_download) {
		case POP3_PARTIAL_DLOAD_UNKN:
			text = g_strdup_printf(_("This message has been "
					"partially retrieved;\nit is %s."),
					to_human_readable(
						(off_t)(msginfo->total_size)));
			button1 = _("Mark for download");
			button2 = _("Mark for deletion");
			button1_cb = partial_recv_dload_clicked;
			button2_cb = partial_recv_del_clicked;
			break;
		case POP3_PARTIAL_DLOAD_DLOAD:
			text = g_strdup_printf(_("This message has been "
					"partially retrieved;\nit is %s and "
					"will be downloaded."),
					to_human_readable(
						(off_t)(msginfo->total_size)));
			button1 = _("Unmark");
			button1_cb = partial_recv_unmark_clicked;
			button2 = _("Mark for deletion");
			button2_cb = partial_recv_del_clicked;
			break;
		case POP3_PARTIAL_DLOAD_DELE:
			text = g_strdup_printf(_("This message has been "
					"partially retrieved;\nit is %s and "
					"will be deleted."),
					to_human_readable(
						(off_t)(msginfo->total_size)));
			button1 = _("Mark for download");
			button1_cb = partial_recv_dload_clicked;
			button2 = _("Unmark");
			button2_cb = partial_recv_unmark_clicked;
			break;
		default:
			return;
		}
	}
	
	noticeview_set_icon(noticeview, STOCK_PIXMAP_NOTICE_WARN);
	noticeview_set_text(noticeview, text);
	g_free(text);
	noticeview_set_button_text(noticeview, button1);
	noticeview_set_button_press_callback(noticeview,
		     G_CALLBACK(button1_cb), (gpointer) msginfo);

	noticeview_set_2ndbutton_text(noticeview, button2);
	noticeview_set_2ndbutton_press_callback(noticeview,
		     G_CALLBACK(button2_cb), (gpointer) msginfo);

	noticeview_show(noticeview);
}

static void partial_recv_dload_clicked(NoticeView *noticeview, 
				       MsgInfo *msginfo)
{
	if (partial_mark_for_download(msginfo) == 0) {
		partial_recv_show(noticeview, msginfo);
	}
}

static void partial_recv_del_clicked(NoticeView *noticeview, 
				       MsgInfo *msginfo)
{
	if (partial_mark_for_delete(msginfo) == 0) {
		partial_recv_show(noticeview, msginfo);
	}
}

static void partial_recv_unmark_clicked(NoticeView *noticeview, 
				       MsgInfo *msginfo)
{
	if (partial_unmark(msginfo) == 0) {
		partial_recv_show(noticeview, msginfo);
	}
}

static void select_account_cb(GtkWidget *w, gpointer data)
{
	*(gint*)data = combobox_get_active_data(GTK_COMBO_BOX(w));
}

static PrefsAccount *select_account_from_list(GList *ac_list)
{
	GtkWidget *optmenu;
	gint account_id;

	g_return_val_if_fail(ac_list != NULL, NULL);
	g_return_val_if_fail(ac_list->data != NULL, NULL);
	
	optmenu = gtkut_account_menu_new(ac_list,
			G_CALLBACK(select_account_cb),
			&account_id);
	if (!optmenu)
		return NULL;
	account_id = ((PrefsAccount *) ac_list->data)->account_id;
	if (alertpanel_with_widget(
				_("Return Receipt Notification"),
				_("The message was sent to several of your "
				  "accounts.\n"
				  "Please choose which account do you want to "
				  "use for sending the receipt notification:"),
			        _("_Cancel"), _("_Send Notification"), NULL,
			        FALSE, G_ALERTDEFAULT, optmenu) != G_ALERTALTERNATE)
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

	if (msgview->mimeview->type == MIMEVIEW_VIEWER) {
		MimeViewer *viewer = msgview->mimeview->mimeviewer;
		if (viewer && viewer->get_selection) {
			text = viewer->get_selection(viewer);
			if (text)
				return text;
		}
	}

	textview = messageview_get_current_textview(msgview);
	g_return_val_if_fail(textview != NULL, NULL);

	edit = GTK_TEXT_VIEW(textview->text);
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

#ifdef USE_GNOMEPRINT
static void print_mimeview(MimeView *mimeview, gint sel_start, gint sel_end, gint partnum) 
{
	if (!mimeview 
	||  !mimeview->textview
	||  !mimeview->textview->text)
		alertpanel_warning(_("Cannot print: the message doesn't "
				     "contain text."));
	else {
		gtk_widget_realize(mimeview->textview->text);
		if (partnum > 0) {
			mimeview_select_part_num(mimeview, partnum);
		}
		if (mimeview->type == MIMEVIEW_VIEWER) {
			MimeViewer *viewer = mimeview->mimeviewer;
			if (viewer && viewer->print) {
				viewer->print(viewer);
				return;
			}
		}
		if (sel_start != -1 && sel_end != -1) {
			GtkTextIter start, end;
			GtkTextView *text = GTK_TEXT_VIEW(mimeview->textview->text);
			GtkTextBuffer *buffer = gtk_text_view_get_buffer(text);

			gtk_text_buffer_get_iter_at_offset(buffer, &start, sel_start);
			gtk_text_buffer_get_iter_at_offset(buffer, &end, sel_end);
			gtk_text_buffer_select_range(buffer, &start, &end);
		}

		gedit_print(GTK_TEXT_VIEW(mimeview->textview->text));
	}
}

void messageview_print(MsgInfo *msginfo, gboolean all_headers, 
			gint sel_start, gint sel_end, gint partnum) 
{
	PangoFontDescription *font_desc = NULL;
	MessageView *tmpview = messageview_create_with_new_window_visible(
				mainwindow_get_mainwindow(), FALSE);

	if (prefs_common.use_different_print_font) {
		font_desc = pango_font_description_from_string
						(prefs_common.printfont);
	} else {
		font_desc = pango_font_description_from_string
						(prefs_common.textfont);
	}
	if (font_desc) {
		gtk_widget_modify_font(tmpview->mimeview->textview->text, 
			font_desc);
		pango_font_description_free(font_desc);
	}

	tmpview->all_headers = all_headers;
	if (msginfo && messageview_show(tmpview, msginfo, 
		tmpview->all_headers) >= 0) {
			print_mimeview(tmpview->mimeview, 
				sel_start, sel_end, partnum);
	}
	messageview_destroy(tmpview);
}
#endif

static void print_cb(gpointer data, guint action, GtkWidget *widget)
{
	MessageView *messageview = (MessageView *)data;
#ifndef USE_GNOMEPRINT
	gchar *cmdline = NULL;
	gchar *p;
#else
	gint sel_start = -1, sel_end = -1, partnum = 0;
#endif

	if (!messageview->msginfo) return;
#ifndef USE_GNOMEPRINT
	cmdline = input_dialog(_("Print"),
			       _("Enter the print command line:\n"
				 "('%s' will be replaced with file name)"),
			       prefs_common.print_cmd);
	if (!cmdline) return;
	if (!(p = strchr(cmdline, '%')) || *(p + 1) != 's' ||
	    strchr(p + 2, '%')) {
		alertpanel_error(_("Print command line is invalid:\n'%s'"),
				 cmdline);
		g_free(cmdline);
		return;
	}
	procmsg_print_message(messageview->msginfo, cmdline);
	g_free(cmdline);
#else
	partnum = mimeview_get_selected_part_num(messageview->mimeview);
	textview_get_selection_offsets(messageview->mimeview->textview,
		&sel_start, &sel_end);
	messageview_print(messageview->msginfo, messageview->all_headers, 
		sel_start, sel_end, partnum);
#endif
}

static void close_cb(gpointer data, guint action, GtkWidget *widget)
{
	MessageView *messageview = (MessageView *)data;
	messageview_destroy(messageview);
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
		procmime_force_charset(charset);
		
		messageview_show(messageview, messageview->msginfo, FALSE);
	}
}

static void set_decode_cb(gpointer data, guint action, GtkWidget *widget)
{
	MessageView *messageview = (MessageView *)data;
	if (GTK_CHECK_MENU_ITEM(widget)->active) {
		messageview->forced_encoding = (EncodingType)action;

		messageview_show(messageview, messageview->msginfo, FALSE);
		
		debug_print("forced encoding: %d\n", action);
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

	if (messageview->mimeview->textview &&
	    messageview->mimeview->textview->loading) {
		return;
	}
	if (messageview->updating)
		return;

	messageview->all_headers = 
			GTK_CHECK_MENU_ITEM(widget)->active;
	if (!msginfo) return;
	messageview->msginfo = NULL;
	messageview_show(messageview, msginfo,
			 GTK_CHECK_MENU_ITEM(widget)->active);
	procmsg_msginfo_free(msginfo);
	main_window_set_menu_sensitive(messageview->mainwin);
}

#define SET_CHECK_MENU_ACTIVE(path, active) \
{ \
	menuitem = gtk_item_factory_get_widget(ifactory, path); \
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem), active); \
}

static void msg_hide_quotes_cb(gpointer data, guint action, GtkWidget *widget)
{
	MessageView *messageview = (MessageView *)data;
	MsgInfo *msginfo = messageview->msginfo;
	static gboolean updating_menu = FALSE;
	GtkItemFactory *ifactory = gtk_item_factory_from_widget(messageview->menubar);
	GtkWidget *menuitem;
	if (updating_menu)
		return;

	prefs_common.hide_quotes = 
			GTK_CHECK_MENU_ITEM(widget)->active ? action : 0;
	
	updating_menu=TRUE;
	SET_CHECK_MENU_ACTIVE("/View/Quotes/Fold all", FALSE);
	SET_CHECK_MENU_ACTIVE("/View/Quotes/Fold from level 2", FALSE);
	SET_CHECK_MENU_ACTIVE("/View/Quotes/Fold from level 3", FALSE);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(widget), prefs_common.hide_quotes > 0);	
	updating_menu=FALSE;
	if (!msginfo) return;
	messageview->msginfo = NULL;
	messageview_show(messageview, msginfo,
			 messageview->all_headers);
	procmsg_msginfo_free(msginfo);
	
	/* update main window */
	main_window_set_menu_sensitive(messageview->mainwin);
	summary_redisplay_msg(messageview->mainwin->summaryview);
}
#undef SET_CHECK_MENU_ACTIVE

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
	GSList *msginfo_list = NULL;

	g_return_if_fail(messageview->msginfo);

	msginfo_list = g_slist_append(msginfo_list, messageview->msginfo);
	compose_reply_from_messageview(messageview, msginfo_list, action);
	g_slist_free(msginfo_list);
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

static void open_urls_cb(gpointer data, guint action, GtkWidget *widget)
{
	MessageView *messageview = (MessageView *)data;
	messageview_list_urls(messageview);
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
		MsgInfo *old_msginfo = messageview->msginfo;
		messageview_clear(messageview);
		messageview_update(messageview, old_msginfo);
	}

	return FALSE;
}

void messageview_set_menu_sensitive(MessageView *messageview)
{
	GtkItemFactory *ifactory;
	GtkWidget *menuitem = NULL;

	if (!messageview || !messageview->new_window) 
		return;
	/* do some smart things */
	if (!messageview->menubar) return;
	ifactory = gtk_item_factory_from_widget(messageview->menubar);
	if (!ifactory) return;

	if (prefs_common.hide_quotes) {
		menuitem = NULL;
		if (prefs_common.hide_quotes == 1)
			menuitem = gtk_item_factory_get_widget(ifactory, 
					"/View/Quotes/Fold all");
		if (prefs_common.hide_quotes == 2)
			menuitem = gtk_item_factory_get_widget(ifactory, 
					"/View/Quotes/Fold from level 2");
		if (prefs_common.hide_quotes == 3)
			menuitem = gtk_item_factory_get_widget(ifactory, 
					"/View/Quotes/Fold from level 3");
		gtk_check_menu_item_set_active
			(GTK_CHECK_MENU_ITEM(menuitem),
			 TRUE);
	}
}

void messageview_learn (MessageView *msgview, gboolean is_spam)
{
	if (is_spam) {
		if (procmsg_spam_learner_learn(msgview->msginfo, NULL, TRUE) == 0)
			procmsg_msginfo_set_flags(msgview->msginfo, MSG_SPAM, 0);
		else
			log_error(LOG_PROTOCOL, _("An error happened while learning.\n"));
		
	} else {
		if (procmsg_spam_learner_learn(msgview->msginfo, NULL, FALSE) == 0)
			procmsg_msginfo_unset_flags(msgview->msginfo, MSG_SPAM, 0);
		else
			log_error(LOG_PROTOCOL, _("An error happened while learning.\n"));
	}
	if (msgview->toolbar)
		toolbar_set_learn_button
			(msgview->toolbar,
			 MSG_IS_SPAM(msgview->msginfo->flags)?LEARN_HAM:LEARN_SPAM);
	else
		toolbar_set_learn_button
			(msgview->mainwin->toolbar,
			 MSG_IS_SPAM(msgview->msginfo->flags)?LEARN_HAM:LEARN_SPAM);
}

void messageview_list_urls (MessageView	*msgview)
{
	GSList *cur = msgview->mimeview->textview->uri_list;
	GSList *newlist = NULL;
	for (; cur; cur = cur->next) {
		ClickableText *uri = (ClickableText *)cur->data;
		if (uri->uri &&
		    (!g_ascii_strncasecmp(uri->uri, "ftp.", 4) ||
		     !g_ascii_strncasecmp(uri->uri, "ftp:", 4) ||
		     !g_ascii_strncasecmp(uri->uri, "www.", 4) ||
		     !g_ascii_strncasecmp(uri->uri, "http:", 5) ||
		     !g_ascii_strncasecmp(uri->uri, "https:", 6)))
			newlist = g_slist_prepend(newlist, uri);
	}
	newlist = g_slist_reverse(newlist);
	uri_opener_open(msgview, newlist);
	g_slist_free(newlist);
}
