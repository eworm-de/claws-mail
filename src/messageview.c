/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2002 Hiroyuki Yamamoto
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
#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "intl.h"
#include "main.h"
#include "messageview.h"
#include "headerview.h"
#include "summaryview.h"
#include "textview.h"
#include "imageview.h"
#include "mimeview.h"
#include "procmsg.h"
#include "procheader.h"
#include "procmime.h"
#include "prefs_common.h"
#include "gtkutils.h"
#include "utils.h"
#include "rfc2015.h"
#include "about.h"
#include "account.h"
#include "alertpanel.h"
#include "send_message.h"
#include "pgptext.h"
#include "menu.h"
#include "stock_pixmap.h"


static void messageview_change_view_type(MessageView	*messageview,
					 MessageType	 type);
static void messageview_destroy_cb	(GtkWidget	*widget,
					 MessageView	*messageview);
static void messageview_size_allocate_cb(GtkWidget	*widget,
					 GtkAllocation	*allocation);
static void key_pressed			(GtkWidget	*widget,
					 GdkEventKey	*event,
					 MessageView	*messageview);

static void return_receipt_show		(NoticeView     *noticeview, 
				         MsgInfo        *msginfo);	
static void return_receipt_send_clicked (NoticeView	*noticeview, 
                                         MsgInfo        *msginfo);

static PrefsAccount *select_account_from_list
					(GList		*ac_list);

static void messageview_menubar_cb	(MessageView 	*msgview,
					 guint 		 action, 
					 GtkWidget 	*widget);
static void messageview_delete_cb	(MessageView 	*msgview, 
					 guint 		 action, 
					 GtkWidget 	*widget);					 
static void messageview_close_cb	(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void messageview_update		(MessageView *msgview);
static void messageview_update_all	(MessageView *msgview);

static GList *msgview_list = NULL;

MessageView *messageview_create(MainWindow *mainwin)
{
	MessageView *messageview;
	GtkWidget *vbox;
	HeaderView *headerview;
	TextView *textview;
	ImageView *imageview;
	MimeView *mimeview;
	NoticeView *noticeview;

	debug_print("Creating message view...\n");
	messageview = g_new0(MessageView, 1);

	messageview->type = MVIEW_TEXT;

	headerview = headerview_create();

	noticeview = noticeview_create(mainwin);

	textview = textview_create();
	textview->messageview = messageview;

	imageview = imageview_create();
	imageview->messageview = messageview;

	mimeview = mimeview_create();
	mimeview->textview = textview_create();
	mimeview->textview->messageview = messageview;
	mimeview->imageview = imageview;
	mimeview->messageview = messageview;

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET_PTR(headerview),
			   FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET_PTR(noticeview),
			   FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET_PTR(textview),
			   TRUE, TRUE, 0);

	/* to remove without destroyed */
	gtk_widget_ref(GTK_WIDGET_PTR(textview));
	gtk_widget_ref(GTK_WIDGET_PTR(imageview));
	gtk_widget_ref(GTK_WIDGET_PTR(mimeview));
	gtk_widget_ref(GTK_WIDGET_PTR(mimeview->textview));

	messageview->vbox       = vbox;
	messageview->new_window = FALSE;
	messageview->window     = NULL;
	messageview->headerview = headerview;
	messageview->textview   = textview;
	messageview->imageview  = imageview;
	messageview->mimeview   = mimeview;
	messageview->noticeview = noticeview;
	messageview->mainwin    = mainwin;

	return messageview;
}

static GtkItemFactoryEntry messageview_entries[] =
{
	{N_("/_File"),				NULL, NULL, 0, "<Branch>"},
	{N_("/_File/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_File/_Close"),			"<control>W", messageview_close_cb, 0, NULL},

	{N_("/_Message"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_Message/_Reply"),		"<control>R", 	messageview_menubar_cb, COMPOSE_REPLY, NULL},
	{N_("/_Message/Repl_y to"),		NULL, NULL, 0, "<Branch>"},
	{N_("/_Message/Repl_y to/_all"),	"<shift><control>R", messageview_menubar_cb, COMPOSE_REPLY_TO_ALL, NULL},
	{N_("/_Message/Repl_y to/_sender"),	NULL, messageview_menubar_cb, COMPOSE_REPLY_TO_SENDER, NULL},
	{N_("/_Message/Repl_y to/mailing _list"),
						"<control>L", messageview_menubar_cb, COMPOSE_REPLY_TO_LIST, NULL},
/*	{N_("/_Message/Follow-up and reply to"),NULL, messageview_menubar_cb, COMPOSE_FOLLOWUP_AND_REPLY_TO, NULL}, */
	{N_("/_Message/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Message/_Forward"),		"<control><alt>F", messageview_menubar_cb, COMPOSE_FORWARD, NULL},
	{N_("/_Message/Redirect"),		NULL, messageview_menubar_cb, COMPOSE_REDIRECT, NULL},
	{N_("/_Message/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Message/_Delete"),		"<control>D", messageview_delete_cb,  0, NULL},
	
	{N_("/_Help"),				NULL, NULL, 0, "<Branch>"},
	{N_("/_Help/_About"),			NULL, about_show, 0, NULL}
};


GList *messageview_get_msgview_list(void)
{
	return msgview_list;
}

void messageview_add_toolbar(MessageView *msgview, GtkWidget *window) 
{
 	GtkWidget *handlebox;
	GtkWidget *vbox;
	GtkWidget *menubar;
	guint n_menu_entries;

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(vbox);
	gtk_container_add(GTK_CONTAINER(window), vbox);	
	
	n_menu_entries = sizeof(messageview_entries) / sizeof(messageview_entries[0]);
	menubar = menubar_create(window, messageview_entries,
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
	gtk_widget_set_usize(window, prefs_common.msgwin_width,
			     prefs_common.msgwin_height);

	msgview = messageview_create(mainwin);

	gtk_signal_connect(GTK_OBJECT(window), "size_allocate",
			   GTK_SIGNAL_FUNC(messageview_size_allocate_cb),
			   msgview);
	gtk_signal_connect(GTK_OBJECT(window), "destroy",
			   GTK_SIGNAL_FUNC(messageview_destroy_cb), msgview);
	gtk_signal_connect(GTK_OBJECT(window), "key_press_event",
			   GTK_SIGNAL_FUNC(key_pressed), msgview);

	messageview_add_toolbar(msgview, window);

	gtk_widget_grab_focus(msgview->textview->text);
	gtk_widget_show_all(window);

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
	textview_init(messageview->textview);
	imageview_init(messageview->imageview);
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
		conv_encode_header(dest, len, src, header_len);
}

static gint disposition_notification_queue(PrefsAccount * account,
					   gchar * to, const gchar *file)
{
	FolderItem *queue;
	gchar *tmp;
	FILE *fp, *src_fp;
	gchar buf[BUFFSIZE];
	gint num;

	debug_print("queueing message...\n");
	g_return_val_if_fail(account != NULL, -1);

	tmp = g_strdup_printf("%s%cqueue.%d", g_get_tmp_dir(),
			      G_DIR_SEPARATOR, (gint)file);
	if ((fp = fopen(tmp, "wb")) == NULL) {
		FILE_OP_ERROR(tmp, "fopen");
		g_free(tmp);
		return -1;
	}
	if ((src_fp = fopen(file, "rb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		fclose(fp);
		unlink(tmp);
		g_free(tmp);
		return -1;
	}
	if (change_file_mode_rw(fp, tmp) < 0) {
		FILE_OP_ERROR(tmp, "chmod");
		g_warning("can't change file mode\n");
	}

	/* queueing variables */
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
	if (account->nntp_server)
		fprintf(fp, "NSV:%s\n", account->nntp_server);
	else
		fprintf(fp, "NSV:\n");
	fprintf(fp, "SSH:\n");
	fprintf(fp, "R:<%s>", to);
	fprintf(fp, "\n");
	fprintf(fp, "\n");

	while (fgets(buf, sizeof(buf), src_fp) != NULL) {
		if (fputs(buf, fp) == EOF) {
			FILE_OP_ERROR(tmp, "fputs");
			fclose(fp);
			fclose(src_fp);
			unlink(tmp);
			g_free(tmp);
			return -1;
		}
	}

	fclose(src_fp);
	if (fclose(fp) == EOF) {
		FILE_OP_ERROR(tmp, "fclose");
		unlink(tmp);
		g_free(tmp);
		return -1;
	}

	queue = folder_get_default_queue();
	if ((num = folder_item_add_msg(queue, tmp, TRUE)) < 0) {
		g_warning("can't queue the message\n");
		unlink(tmp);
		g_free(tmp);
		return -1;
	}
	g_free(tmp);

	return 0;
}

static gint disposition_notification_send(MsgInfo *msginfo)
{
	gchar buf[BUFFSIZE];
	gchar tmp[MAXPATHLEN + 1];
	FILE *fp;
	GSList *to_list;
	GList *ac_list;
	PrefsAccount *account;
	gint ok;
	gchar *to;

	if ((!msginfo->returnreceiptto) && 
	    (!msginfo->dispositionnotificationto)) 
		return -1;

	/* RFC2298: Test for Return-Path */
	if (msginfo->dispositionnotificationto)
		to = msginfo->dispositionnotificationto;
	else
		to = msginfo->returnreceiptto;

	ok = get_header_from_msginfo(msginfo, buf, sizeof(buf),
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

	if (fclose(fp) == EOF) {
		FILE_OP_ERROR(tmp, "fclose");
		unlink(tmp);
		return -1;
	}

	to_list = address_list_append(NULL, to);
	ok = send_message(tmp, account, to_list);
	
	if (ok < 0) {
		if (prefs_common.queue_msg) {
			AlertValue val;
			
			val = alertpanel
				(_("Queueing"),
				 _("Error occurred while sending the notification.\n"
				   "Put this notification into queue folder?"),
				 _("OK"), _("Cancel"), NULL);
			if (G_ALERTDEFAULT == val) {
				ok = disposition_notification_queue(account, to, tmp);
				if (ok < 0)
					alertpanel_error(_("Can't queue the notification."));
			}
		} else
			alertpanel_error_log(_("Error occurred while sending the notification."));
	}

	if (unlink(tmp) < 0) FILE_OP_ERROR(tmp, "unlink");

	return ok;
}

void messageview_show(MessageView *messageview, MsgInfo *msginfo,
		      gboolean all_headers)
{
	FILE *fp;
	gchar *file;
	MimeInfo *mimeinfo;
	MsgInfo *tmpmsginfo;

	g_return_if_fail(msginfo != NULL);
	messageview->msginfo = msginfo;

#if USE_GPGME
	if ((fp = procmsg_open_message_decrypted(msginfo, &mimeinfo)) == NULL)
		return;
#else /* !USE_GPGME */
	if ((fp = procmsg_open_message(msginfo)) == NULL) return;
	mimeinfo = procmime_scan_mime_header(fp);
#endif /* USE_GPGME */
	fclose(fp);
	if (!mimeinfo) return;

	file = procmsg_get_message_file_path(msginfo);
	if (!file) {
		g_warning("can't get message file path.\n");
		procmime_mimeinfo_free_all(mimeinfo);
		return;
	}

	tmpmsginfo = procheader_parse_file(file, msginfo->flags, TRUE, TRUE);

	headerview_show(messageview->headerview, tmpmsginfo);
	procmsg_msginfo_free(tmpmsginfo);

	messageview->all_headers = all_headers;
	textview_set_all_headers(messageview->textview, all_headers);
	textview_set_all_headers(messageview->mimeview->textview, all_headers);

	if (mimeinfo->mime_type != MIME_TEXT &&
	    mimeinfo->mime_type != MIME_TEXT_HTML) {
		messageview_change_view_type(messageview, MVIEW_MIME);
		mimeview_show_message(messageview->mimeview, mimeinfo, file);
	} else {
		messageview_change_view_type(messageview, MVIEW_TEXT);
		textview_show_message(messageview->textview, mimeinfo, file);
		procmime_mimeinfo_free_all(mimeinfo);
	}

	if (MSG_IS_RETRCPT_PENDING(msginfo->flags))
		return_receipt_show(messageview->noticeview, msginfo);
	else 
		noticeview_hide(messageview->noticeview);

	g_free(file);
}

static void messageview_change_view_type(MessageView *messageview,
					 MessageType type)
{
	TextView *textview = messageview->textview;
	MimeView *mimeview = messageview->mimeview;

	if (messageview->type == type) return;

	if (type == MVIEW_MIME) {
		gtkut_container_remove
			(GTK_CONTAINER(GTK_WIDGET_PTR(messageview)),
			 GTK_WIDGET_PTR(textview));
		gtk_box_pack_start(GTK_BOX(messageview->vbox),
				   GTK_WIDGET_PTR(mimeview), TRUE, TRUE, 0);
		gtk_container_add(GTK_CONTAINER(mimeview->vbox),
				  GTK_WIDGET_PTR(textview));
	} else if (type == MVIEW_TEXT) {
		gtkut_container_remove
			(GTK_CONTAINER(GTK_WIDGET_PTR(messageview)),
			 GTK_WIDGET_PTR(mimeview));

		if (mimeview->vbox == GTK_WIDGET_PTR(textview)->parent)
			gtkut_container_remove(GTK_CONTAINER(mimeview->vbox),
			 		       GTK_WIDGET_PTR(textview));

		gtk_box_pack_start(GTK_BOX(messageview->vbox),
				   GTK_WIDGET_PTR(textview), TRUE, TRUE, 0);
	} else
		return;

	messageview->type = type;
}

void messageview_reflect_prefs_pixmap_theme(void)
{
	GList *cur;
	MessageView *msgview;

	for (cur = msgview_list; cur != NULL; cur = cur->next) {
		msgview = (MessageView*)cur->data;
		toolbar_update(TOOLBAR_MSGVIEW, msgview);
	}
}

void messageview_clear(MessageView *messageview)
{
	messageview_change_view_type(messageview, MVIEW_TEXT);
	headerview_clear(messageview->headerview);
	textview_clear(messageview->textview);
	imageview_clear(messageview->imageview);
	noticeview_hide(messageview->noticeview);
}

void messageview_destroy(MessageView *messageview)
{
	GtkWidget *textview  = GTK_WIDGET_PTR(messageview->textview);
	GtkWidget *imageview = GTK_WIDGET_PTR(messageview->imageview);
	GtkWidget *mimeview  = GTK_WIDGET_PTR(messageview->mimeview);

	debug_print("destroy messageview\n");
	headerview_destroy(messageview->headerview);
	textview_destroy(messageview->textview);
	imageview_destroy(messageview->imageview);
	mimeview_destroy(messageview->mimeview);
	noticeview_destroy(messageview->noticeview);

	toolbar_clear_list(TOOLBAR_MSGVIEW);
	if (messageview->toolbar) {
		toolbar_destroy(messageview->toolbar);
		g_free(messageview->toolbar);
	}
	
	msgview_list = g_list_remove(msgview_list, messageview); 

	g_free(messageview);

	gtk_widget_unref(textview);
	gtk_widget_unref(imageview);
	gtk_widget_unref(mimeview);
}

void messageview_delete(MessageView *msgview)
{
	MsgInfo *msginfo = (MsgInfo*)msgview->msginfo;
	SummaryView *summaryview = (SummaryView*)msgview->mainwin->summaryview;
	FolderItem *trash = folder_get_default_trash();
	GSList *msg_list;

	g_return_if_fail(msginfo != NULL);
	g_return_if_fail(trash   != NULL);
	
	msg_list = folder_item_get_msg_list(msginfo->folder);
	
	if (msg_list == NULL) {
		alertpanel_error(_("Message already removed from folder."));
		return;
	}
	
	for (; msg_list != NULL; msg_list = msg_list->next) {
		MsgInfo *msginfo_list = (MsgInfo*)msg_list->data;
		
		if (msginfo->msgnum == msginfo_list->msgnum) {

			if (prefs_common.immediate_exec)
				folder_item_move_msg(trash, msginfo);
			else {
				procmsg_msginfo_set_to_folder(msginfo, trash);
				procmsg_msginfo_set_flags(msginfo, MSG_DELETED, 0);
				/* NOTE: does not update to next message in summaryview
				 */
			}
				
			messageview_update_all(msgview);
			break;
		}
	}
}

/*	
 * scan List of MessageViews checking whether there are any Views holding messages 
 * which need to be updated (another view might have deleted the one this MessagView holds)
 */
static void messageview_update_all(MessageView *msgview)
{
	MsgInfo *msginfo = (MsgInfo*)msgview->msginfo;
	GList *cur;
	
	g_return_if_fail(msginfo != NULL);

	for (cur = msgview_list; cur != NULL; cur = cur->next) {
		MessageView *msgview = (MessageView*)cur->data;
		MsgInfo *msginfo_list = (MsgInfo*)msgview->msginfo;
		
		g_return_if_fail(msginfo != NULL);

		if (msginfo->msgnum == msginfo_list->msgnum)
			messageview_update(msgview);
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
	textview_set_font(messageview->textview, NULL);
}

TextView *messageview_get_current_textview(MessageView *messageview)
{
	TextView *text = NULL;

	if (messageview->type == MVIEW_TEXT)
		text = messageview->textview;
	else if (messageview->type == MVIEW_MIME) {
		if (gtk_notebook_get_current_page
			(GTK_NOTEBOOK(messageview->mimeview->notebook)) == 0)
			text = messageview->textview;
		else if (messageview->mimeview->type == MIMEVIEW_TEXT)
			text = messageview->mimeview->textview;
	}

	return text;
}

void messageview_copy_clipboard(MessageView *messageview)
{
	TextView *text;

	text = messageview_get_current_textview(messageview);
	if (text)
		gtk_editable_copy_clipboard(GTK_EDITABLE(text->text));
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
	textview_set_position(messageview->textview, pos);
}

gboolean messageview_search_string(MessageView *messageview, const gchar *str,
				   gboolean case_sens)
{
	return textview_search_string(messageview->textview, str, case_sens);
	return FALSE;
}

gboolean messageview_search_string_backward(MessageView *messageview,
					    const gchar *str,
					    gboolean case_sens)
{
	return textview_search_string_backward(messageview->textview,
					       str, case_sens);
	return FALSE;
}

gboolean messageview_is_visible(MessageView *messageview)
{
	return messageview->visible;
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

static void key_pressed(GtkWidget *widget, GdkEventKey *event,
			MessageView *messageview)
{
	if (event && event->keyval == GDK_Escape && messageview->window)
		gtk_widget_destroy(messageview->window);
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
		gtk_widget_grab_focus(GTK_WIDGET(messageview->textview->text));
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
	noticeview_set_text(noticeview, _("This messages asks for a return receipt."));
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
		procmsg_msginfo_unset_flags(msginfo, MSG_RETRCPT_PENDING, 0);
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
 * \brief return selected messageview text used by composing 
 * 	  to reply to selected text only
 *
 * \param  pointer to Messageview 
 *
 * \return pointer to text (needs to be free'd by calling func)
 */
gchar *messageview_get_selection(MessageView *msgview)
{
	gchar *text = NULL;
	
	g_return_val_if_fail(msgview != NULL, NULL);

	text = gtkut_editable_get_selection
		(GTK_EDITABLE(msgview->textview->text));
	
	if (!text && msgview->type == MVIEW_MIME
	    && msgview->mimeview->type == MIMEVIEW_TEXT
	    && msgview->mimeview->textview
	    && !msgview->mimeview->textview->default_text) {
		text = gtkut_editable_get_selection 
			(GTK_EDITABLE(msgview->mimeview->textview->text));   
	}

	return text;
}

static void messageview_delete_cb(MessageView *msgview, guint action, GtkWidget *widget)
{
	messageview_delete(msgview);
}

static void messageview_menubar_cb(MessageView *msgview, guint action, GtkWidget *widget)
{
	GSList *msginfo_list = NULL;
	gchar *body;
	MsgInfo *msginfo;

	g_return_if_fail(msgview != NULL);

	msginfo = (MsgInfo*)msgview->msginfo;
	g_return_if_fail(msginfo != NULL);

	msginfo_list = g_slist_append(msginfo_list, msginfo);
	g_return_if_fail(msginfo_list);

	body =  messageview_get_selection(msgview);
	compose_reply_mode((ComposeMode)action, msginfo_list, body);
	g_free(body);
	g_slist_free(msginfo_list);
}

static void messageview_close_cb(gpointer data, guint action, GtkWidget *widget)
{
	MessageView *messageview = (MessageView *)data;
	
	gtk_widget_destroy(messageview->window);
}
