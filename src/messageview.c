/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2001 Hiroyuki Yamamoto
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
#include "account.h"
#include "alertpanel.h"
#include "send.h"
#include "pgptext.h"

static void messageview_change_view_type(MessageView	*messageview,
					 MessageType	 type);
static void messageview_destroy_cb	(GtkWidget	*widget,
					 MessageView	*messageview);
static void messageview_size_allocate_cb(GtkWidget	*widget,
					 GtkAllocation	*allocation);
static void key_pressed			(GtkWidget	*widget,
					 GdkEventKey	*event,
					 MessageView	*messageview);
static void messageview_toggle_view(MessageView *messageview);

MessageView *messageview_create(void)
{
	MessageView *messageview;
	GtkWidget *vbox;
	HeaderView *headerview;
	TextView *textview;
	ImageView *imageview;
	MimeView *mimeview;

	debug_print(_("Creating message view...\n"));
	messageview = g_new0(MessageView, 1);

	messageview->type = MVIEW_TEXT;

	headerview = headerview_create();

	textview = textview_create();
	textview->messageview = messageview;

	imageview = imageview_create();
	imageview->messageview = messageview;

	mimeview = mimeview_create();
	mimeview->textview = textview;
	mimeview->imageview = imageview;
	mimeview->messageview = messageview;

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET_PTR(headerview),
			   FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET_PTR(textview),
			   TRUE, TRUE, 0);

	/* to remove without destroyed */
	gtk_widget_ref(GTK_WIDGET_PTR(textview));
	gtk_widget_ref(GTK_WIDGET_PTR(imageview));
	gtk_widget_ref(GTK_WIDGET_PTR(mimeview));

	messageview->vbox       = vbox;
	messageview->new_window = FALSE;
	messageview->window     = NULL;
	messageview->headerview = headerview;
	messageview->textview   = textview;
	messageview->imageview  = imageview;
	messageview->mimeview   = mimeview;

	return messageview;
}

MessageView *messageview_create_with_new_window(void)
{
	GtkWidget *window;
	MessageView *msgview;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), _("Sylpheed - Message View"));
	gtk_window_set_wmclass(GTK_WINDOW(window), "message_view", "Sylpheed");
	gtk_window_set_policy(GTK_WINDOW(window), TRUE, TRUE, FALSE);
	gtk_widget_set_usize(window, prefs_common.msgwin_width,
			     prefs_common.msgwin_height);

	msgview = messageview_create();

	gtk_signal_connect(GTK_OBJECT(window), "size_allocate",
			   GTK_SIGNAL_FUNC(messageview_size_allocate_cb),
			   msgview);
	gtk_signal_connect(GTK_OBJECT(window), "destroy",
			   GTK_SIGNAL_FUNC(messageview_destroy_cb), msgview);
	gtk_signal_connect(GTK_OBJECT(window), "key_press_event",
			   GTK_SIGNAL_FUNC(key_pressed), msgview);

	gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET_PTR(msgview));
	gtk_widget_grab_focus(msgview->textview->text);
	gtk_widget_show_all(window);

	msgview->new_window = TRUE;
	msgview->window = window;

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
}

static void notification_convert_header(gchar *dest, gint len, gchar *src,
					gint header_len)
{
	g_return_if_fail(src != NULL);
	g_return_if_fail(dest != NULL);

	if (len < 1) return;

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
	gchar *tmp, *queue_path;
	FILE *fp, *src_fp;
	GSList *cur;
	gchar buf[BUFFSIZE];
	gint num;

	debug_print(_("queueing message...\n"));
	g_return_val_if_fail(account != NULL, -1);

	tmp = g_strdup_printf("%s%cqueue.%d", g_get_tmp_dir(),
			      G_DIR_SEPARATOR, (gint)file);
	if ((fp = fopen(tmp, "w")) == NULL) {
		FILE_OP_ERROR(tmp, "fopen");
		g_free(tmp);
		return -1;
	}
	if ((src_fp = fopen(file, "r")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		fclose(fp);
		unlink(tmp);
		g_free(tmp);
		return -1;
	}
	if (change_file_mode_rw(fp, tmp) < 0) {
		FILE_OP_ERROR(tmp, "chmod");
		g_warning(_("can't change file mode\n"));
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
	folder_item_scan(queue);
	queue_path = folder_item_get_path(queue);
	if (!is_dir_exist(queue_path))
		make_dir_hier(queue_path);
	if ((num = folder_item_add_msg(queue, tmp, TRUE)) < 0) {
		g_warning(_("can't queue the message\n"));
		unlink(tmp);
		g_free(tmp);
		g_free(queue_path);
		return -1;
	}
	g_free(tmp);

	if ((fp = procmsg_open_mark_file(queue_path, TRUE)) == NULL)
		g_warning(_("can't open mark file\n"));
	else {
		MsgInfo newmsginfo;

		newmsginfo.msgnum = num;
		newmsginfo.flags.perm_flags = newmsginfo.flags.tmp_flags = 0;
		procmsg_write_flags(&newmsginfo, fp);
		fclose(fp);
	}
	g_free(queue_path);

	folder_item_scan(queue);
	folderview_update_item(queue, TRUE);

	return 0;
}

static gint disposition_notification_send(MsgInfo * msginfo)
{
	gchar buf[BUFFSIZE];
	gchar tmp[MAXPATHLEN + 1];
	FILE *fp;
	GSList * to_list;
	gint ok;
	gchar * to;

	if ((!msginfo->returnreceiptto) && 
	    (!msginfo->dispositionnotificationto))
		return -1;

	/* write to temporary file */
	g_snprintf(tmp, sizeof(tmp), "%s%ctmpmsg%d",
		   get_rc_dir(), G_DIR_SEPARATOR, (gint)msginfo);

	if ((fp = fopen(tmp, "w")) == NULL) {
		FILE_OP_ERROR(tmp, "fopen");
		return -1;
	}

	/* chmod for security */
	if (change_file_mode_rw(fp, tmp) < 0) {
		FILE_OP_ERROR(tmp, "chmod");
		g_warning(_("can't change file mode\n"));
	}

	/* Date */
	get_rfc822_date(buf, sizeof(buf));
	fprintf(fp, "Date: %s\n", buf);

	/* From */
	if (cur_account->name && *cur_account->name) {
		notification_convert_header
			(buf, sizeof(buf), cur_account->name,
			 strlen("From: "));
		fprintf(fp, "From: %s <%s>\n", buf, cur_account->address);
	} else
		fprintf(fp, "From: %s\n", cur_account->address);

	/* To */
	if (msginfo->dispositionnotificationto)
		to = msginfo->dispositionnotificationto;
	else
		to = msginfo->returnreceiptto;
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

	to_list = address_list_append(NULL, msginfo->dispositionnotificationto);
	ok = send_message(tmp, cur_account, to_list);
	
	if (ok < 0) {
		if (prefs_common.queue_msg) {
			AlertValue val;
			
			val = alertpanel
				(_("Queueing"),
				 _("Error occurred while sending the notification.\n"
				   "Put this notification into queue folder?"),
				 _("OK"), _("Cancel"), NULL);
			if (G_ALERTDEFAULT == val) {
				ok = disposition_notification_queue(cur_account, to, tmp);
				if (ok < 0)
					alertpanel_error(_("Can't queue the notification."));
			}
		} else
			alertpanel_error(_("Error occurred while sending the notification."));
	}

	if (unlink(tmp) < 0) FILE_OP_ERROR(tmp, "unlink");

	return ok;
}

void messageview_show(MessageView *messageview, MsgInfo *msginfo)
{
	FILE *fp;
	gchar *file;
	MimeInfo *mimeinfo;
	MsgInfo *tmpmsginfo;

	g_return_if_fail(msginfo != NULL);

#if USE_GPGME
	for (;;) {
		if ((fp = procmsg_open_message(msginfo)) == NULL) return;
		mimeinfo = procmime_scan_mime_header(fp, MIME_TEXT);
		if (!mimeinfo) break;

		if (!MSG_IS_ENCRYPTED(msginfo->flags) &&
		    rfc2015_is_encrypted(mimeinfo)) {
			MSG_SET_TMP_FLAGS(msginfo->flags, MSG_ENCRYPTED);
		}
		if (!MSG_IS_ENCRYPTED(msginfo->flags) &&
		    pgptext_is_encrypted(mimeinfo, msginfo)) {
			MSG_SET_TMP_FLAGS(msginfo->flags, MSG_ENCRYPTED);
			/* To avoid trouble with the rfc2015 stuff we go for encryption 
			 * right here. */
			if (MSG_IS_ENCRYPTED(msginfo->flags) &&
			    !msginfo->plaintext_file  &&
			    !msginfo->decryption_failed) {
				/* This is an encrypted message but it has not yet
				 * been decrypted and there was no unsuccessful
				 * decryption attempt */
				pgptext_decrypt_message(msginfo, mimeinfo, fp);
				if (msginfo->plaintext_file &&
				    !msginfo->decryption_failed) {
					fclose(fp);
					continue;
				}
			}
		}
		
		if (MSG_IS_ENCRYPTED(msginfo->flags) &&
		    !msginfo->plaintext_file  &&
		    !msginfo->decryption_failed) {
			/* This is an encrypted message but it has not yet
			 * been decrypted and there was no unsuccessful
			 * decryption attempt */
			rfc2015_decrypt_message(msginfo, mimeinfo, fp);
			if (msginfo->plaintext_file &&
			    !msginfo->decryption_failed) {
				fclose(fp);
				continue;
			}
		}

		break;
	}
#else /* !USE_GPGME */
	if ((fp = procmsg_open_message(msginfo)) == NULL) return;
	mimeinfo = procmime_scan_mime_header(fp, MIME_TEXT);
#endif /* USE_GPGME */
	fclose(fp);
	if (!mimeinfo) return;

	file = procmsg_get_message_file_path(msginfo);
	g_return_if_fail(file != NULL);

	/* FIXME - doesn't tmpmsginfo->flags have the value
	 * of msginfo->flags after procheader_parse()???
	 * in any case, checking tmpmsginfo->flags for MSG_UNREAD
	 * fixes the return-receipt-request bug */

	tmpmsginfo = procheader_parse(file, msginfo->flags, TRUE, TRUE);
	if (MSG_IS_MIME(tmpmsginfo->flags))
		MSG_SET_TMP_FLAGS(msginfo->flags, MSG_MIME);

	if (prefs_common.return_receipt
	    && (tmpmsginfo->dispositionnotificationto
		|| tmpmsginfo->returnreceiptto)
	    && (MSG_IS_UNREAD(msginfo->flags))) {
		gint ok;
		
		if (alertpanel(_("Return Receipt"), _("Send return receipt ?"),
			       _("Yes"), _("No"), NULL) == G_ALERTDEFAULT) {
			ok = disposition_notification_send(tmpmsginfo);
			if (ok < 0)
				alertpanel_error(_("Error occurred while sending notification."));
		}
	}

	headerview_show(messageview->headerview, tmpmsginfo);
	procmsg_msginfo_free(tmpmsginfo);

	if (mimeinfo->mime_type != MIME_TEXT) {
		messageview_change_view_type(messageview, MVIEW_MIME);
		mimeview_show_message(messageview->mimeview, mimeinfo, file);
	} else {
		messageview_change_view_type(messageview, MVIEW_TEXT);
		textview_show_message(messageview->textview, mimeinfo, file);
		procmime_mimeinfo_free(mimeinfo);
	}

	g_free(file);
}

static void messageview_change_view_type(MessageView *messageview,
					 MessageType type)
{
	TextView *textview = messageview->textview;
	ImageView *imageview = messageview->imageview;
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
		mimeview->type = MIMEVIEW_TEXT;
	} else if (type == MVIEW_TEXT) {
		gtkut_container_remove
			(GTK_CONTAINER(GTK_WIDGET_PTR(messageview)),
			 GTK_WIDGET_PTR(mimeview));

		if (mimeview->vbox == GTK_WIDGET_PTR(textview)->parent) {
			gtkut_container_remove(GTK_CONTAINER(mimeview->vbox),
			 		       GTK_WIDGET_PTR(textview));
		} else {
			gtkut_container_remove(GTK_CONTAINER(mimeview->vbox),
			  		       GTK_WIDGET_PTR(imageview));
		}

		gtk_box_pack_start(GTK_BOX(messageview->vbox),
				   GTK_WIDGET_PTR(textview), TRUE, TRUE, 0);
	} else
		return;

	messageview->type = type;
}

void messageview_clear(MessageView *messageview)
{
	messageview_change_view_type(messageview, MVIEW_TEXT);
	headerview_clear(messageview->headerview);
	textview_clear(messageview->textview);
}

void messageview_destroy(MessageView *messageview)
{
	GtkWidget *textview  = GTK_WIDGET_PTR(messageview->textview);
	GtkWidget *imageview = GTK_WIDGET_PTR(messageview->imageview);
	GtkWidget *mimeview  = GTK_WIDGET_PTR(messageview->mimeview);

	headerview_destroy(messageview->headerview);
	textview_destroy(messageview->textview);
	imageview_destroy(messageview->imageview);
	mimeview_destroy(messageview->mimeview);

	g_free(messageview);

	gtk_widget_unref(textview);
	gtk_widget_unref(imageview);
	gtk_widget_unref(mimeview);
}

void messageview_quote_color_set(void)
{
}

void messageview_set_font(MessageView *messageview)
{
	textview_set_font(messageview->textview, NULL);
}

void messageview_copy_clipboard(MessageView *messageview)
{
	switch (messageview->type) {
	case MVIEW_TEXT:
		gtk_editable_copy_clipboard(GTK_EDITABLE(messageview->textview->text));
		break;
	case MVIEW_MIME:
		if (messageview->mimeview->type == MIMEVIEW_TEXT)
			gtk_editable_copy_clipboard(GTK_EDITABLE(messageview->mimeview->textview->text));
	default:
		break;
	}
}

void messageview_select_all(MessageView *messageview)
{
	switch (messageview->type) {
	case MVIEW_TEXT:
		gtk_editable_select_region(GTK_EDITABLE(messageview->textview->text), 0, -1);
		break;
	case MVIEW_MIME:
		if (messageview->mimeview->type == MIMEVIEW_TEXT)
			gtk_editable_select_region(GTK_EDITABLE(messageview->mimeview->textview->text), 0, -1);
	default:
		break;
	}
}

void messageview_set_position(MessageView *messageview, gint pos)
{
	switch (messageview->type) {
	case MVIEW_TEXT:
		textview_set_position(messageview->textview, pos);
		break;
	case MVIEW_MIME:
		if (messageview->mimeview->type == MIMEVIEW_TEXT)
			textview_set_position(messageview->mimeview->textview, pos);
		break;
	default:
		break;
	}
}

gboolean messageview_search_string(MessageView *messageview, const gchar *str,
				   gboolean case_sens)
{
	switch (messageview->type) {
	case MVIEW_TEXT:
		return textview_search_string(messageview->textview,
					str, case_sens);
	case MVIEW_MIME:
		if (messageview->mimeview->type == MIMEVIEW_TEXT)
			return textview_search_string(messageview->mimeview->textview,
						str, case_sens);
		else
			return FALSE;
	default:
		return FALSE;
	}
		
	return FALSE;
}

gboolean messageview_search_string_backward(MessageView *messageview,
					    const gchar *str,
					    gboolean case_sens)
{
	switch (messageview->type) {
	case MVIEW_TEXT:
		return textview_search_string_backward(messageview->textview,
					str, case_sens);
	case MVIEW_MIME:
		if (messageview->mimeview->type == MIMEVIEW_TEXT)
			return textview_search_string_backward(messageview->mimeview->textview,
						str, case_sens);
		else
			return FALSE;
	default:
		return FALSE;
	}
		
	return FALSE;

}

GtkWidget *messageview_get_text_widget(MessageView *messageview)
{
	return messageview->textview->text;
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

static void messageview_toggle_view(MessageView *messageview)
{
	MainWindow *mainwin = messageview->mainwin;
	GtkItemFactory *ifactory;
	
	if (!mainwin) return;
	
	ifactory = gtk_item_factory_from_widget(mainwin->menubar);
	menu_toggle_toggle(ifactory, "/View/Expand Summary View");
}

void messageview_toggle_view_real(MessageView *messageview)
{
	MainWindow *mainwin = messageview->mainwin;
	union CompositeWin *cwin = &mainwin->win;
	GtkWidget *vpaned = NULL;
	GtkWidget *container = NULL;
	GtkItemFactory *ifactory =gtk_item_factory_from_widget(mainwin->menubar);
	
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
