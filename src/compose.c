/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999,2000 Hiroyuki Yamamoto
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "defs.h"

#include <glib.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkitemfactory.h>
#include <gtk/gtkcheckmenuitem.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkclist.h>
#include <gtk/gtkctree.h>
#include <gtk/gtkvpaned.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkeditable.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkcontainer.h>
#include <gtk/gtkhandlebox.h>
#include <gtk/gtktoolbar.h>
#include <gtk/gtktable.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkthemes.h>
#include <gtk/gtkdnd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>
/* #include <sys/utsname.h> */
#include <stdlib.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

#if (HAVE_WCTYPE_H && HAVE_WCHAR_H)
#  include <wchar.h>
#  include <wctype.h>
#endif


#include "gtkstext.h"

#include "intl.h"
#include "main.h"
#include "mainwindow.h"
#include "compose.h"
#include "addressbook.h"
#include "folderview.h"
#include "procmsg.h"
#include "menu.h"
#include "send.h"
#include "news.h"
#include "customheader.h"
#include "prefs_common.h"
#include "prefs_account.h"
#include "account.h"
#include "filesel.h"
#include "procheader.h"
#include "procmime.h"
#include "statusbar.h"
#include "about.h"
#include "base64.h"
#include "codeconv.h"
#include "utils.h"
#include "gtkutils.h"
#include "socket.h"
#include "alertpanel.h"
#include "manage_window.h"
#include "gtkshruler.h"
#include "folder.h"
#include "addr_compl.h"

#if USE_GPGME
#  include "rfc2015.h"
#endif

typedef enum
{
	COL_MIMETYPE = 0,
	COL_SIZE     = 1,
	COL_NAME     = 2
} AttachColumnPos;

#define N_ATTACH_COLS		3

#define B64_LINE_SIZE		57
#define B64_BUFFSIZE		77

#define MAX_REFERENCES_LEN	999

static GdkColor quote_color = {0, 0, 0, 0xbfff};

static GList *compose_list = NULL;

static Compose *compose_create			(PrefsAccount	*account);
static void compose_toolbar_create		(Compose	*compose,
						 GtkWidget	*container);
static GtkWidget *compose_account_option_menu_create
						(Compose	*compose);
static void compose_destroy			(Compose	*compose);

static gint compose_parse_header		(Compose	*compose,
						 MsgInfo	*msginfo);
static gchar *compose_parse_references		(const gchar	*ref,
						 const gchar	*msgid);
static void compose_quote_file			(Compose	*compose,
						 MsgInfo	*msginfo,
						 FILE		*fp);
static gchar *compose_quote_parse_fmt		(Compose	*compose,
						 MsgInfo	*msginfo,
						 const gchar	*fmt);
static void compose_reply_set_entry		(Compose	*compose,
						 MsgInfo	*msginfo,
						 gboolean	 to_all,
						 gboolean	 to_author);
static void compose_reedit_set_entry		(Compose	*compose,
						 MsgInfo	*msginfo);
static void compose_insert_sig			(Compose	*compose);
static void compose_insert_file			(Compose	*compose,
						 const gchar	*file);
static void compose_attach_append		(Compose	*compose,
						 const gchar	*file,
						 ContentType	 cnttype);
static void compose_wrap_line			(Compose	*compose);
static void compose_set_title			(Compose	*compose);

static PrefsAccount *compose_current_mail_account(void);
/* static gint compose_send			(Compose	*compose); */
static gint compose_write_to_file		(Compose	*compose,
						 const gchar	*file,
						 gboolean	 is_draft);
static gint compose_write_body_to_file		(Compose	*compose,
						 const gchar	*file);
static gint compose_save_to_outbox		(Compose	*compose,
						 const gchar	*file);
static gint compose_queue			(Compose	*compose,
						 const gchar	*file);
static void compose_write_attach		(Compose	*compose,
						 FILE		*fp);
static gint compose_write_headers		(Compose	*compose,
						 FILE		*fp,
						 const gchar	*charset,
						 EncodingType	 encoding,
						 gboolean	 is_draft);

static void compose_convert_header		(gchar		*dest,
						 gint		 len,
						 gchar		*src,
						 gint		 header_len);
static void compose_generate_msgid		(Compose	*compose,
						 gchar		*buf,
						 gint		 len);

static void compose_attach_info_free		(AttachInfo	*ainfo);
static void compose_attach_remove_selected	(Compose	*compose);

static void compose_attach_property		(Compose	*compose);
static void compose_attach_property_create	(gboolean	*cancelled);
static void attach_property_ok			(GtkWidget	*widget,
						 gboolean	*cancelled);
static void attach_property_cancel		(GtkWidget	*widget,
						 gboolean	*cancelled);
static gint attach_property_delete_event	(GtkWidget	*widget,
						 GdkEventAny	*event,
						 gboolean	*cancelled);
static void attach_property_key_pressed		(GtkWidget	*widget,
						 GdkEventKey	*event,
						 gboolean	*cancelled);

static void compose_exec_ext_editor		(Compose	   *compose);
static gint compose_exec_ext_editor_real	(const gchar	   *file);
static gboolean compose_ext_editor_kill		(Compose	   *compose);
static void compose_input_cb			(gpointer	    data,
						 gint		    source,
						 GdkInputCondition  condition);
static void compose_set_ext_editor_sensitive	(Compose	   *compose,
						 gboolean	    sensitive);

static gint calc_cursor_xpos	(GtkSText	*text,
				 gint		 extra,
				 gint		 char_width);

/* callback functions */

static gboolean compose_edit_size_alloc (GtkEditable	*widget,
					 GtkAllocation	*allocation,
					 GtkSHRuler	*shruler);

static void toolbar_send_cb		(GtkWidget	*widget,
					 gpointer	 data);
static void toolbar_send_later_cb	(GtkWidget	*widget,
					 gpointer	 data);
static void toolbar_draft_cb		(GtkWidget	*widget,
					 gpointer	 data);
static void toolbar_insert_cb		(GtkWidget	*widget,
					 gpointer	 data);
static void toolbar_attach_cb		(GtkWidget	*widget,
					 gpointer	 data);
static void toolbar_sig_cb		(GtkWidget	*widget,
					 gpointer	 data);
static void toolbar_ext_editor_cb	(GtkWidget	*widget,
					 gpointer	 data);
static void toolbar_linewrap_cb		(GtkWidget	*widget,
					 gpointer	 data);
static void toolbar_address_cb		(GtkWidget	*widget,
					 gpointer	 data);

static void account_activated		(GtkMenuItem	*menuitem,
					 gpointer	 data);

static void attach_selected		(GtkCList	*clist,
					 gint		 row,
					 gint		 column,
					 GdkEvent	*event,
					 gpointer	 data);
static void attach_button_pressed	(GtkWidget	*widget,
					 GdkEventButton	*event,
					 gpointer	 data);
static void attach_key_pressed		(GtkWidget	*widget,
					 GdkEventKey	*event,
					 gpointer	 data);

static void compose_send_cb		(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void compose_send_later_cb	(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);

static void compose_draft_cb		(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);

static void compose_attach_cb		(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void compose_insert_file_cb	(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);

static void compose_close_cb		(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void compose_address_cb		(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);

static void compose_ext_editor_cb	(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);

static gint compose_delete_cb		(GtkWidget	*widget,
					 GdkEventAny	*event,
					 gpointer	 data);
static void compose_destroy_cb		(GtkWidget	*widget,
					 Compose	*compose);

static void compose_cut_cb		(Compose	*compose);
static void compose_copy_cb		(Compose	*compose);
static void compose_paste_cb		(Compose	*compose);
static void compose_allsel_cb		(Compose	*compose);

static void compose_grab_focus_cb	(GtkWidget	*widget,
					 Compose	*compose);

static void compose_changed_cb		(GtkEditable	*editable,
					 Compose	*compose);
static void compose_button_press_cb	(GtkWidget	*widget,
					 GdkEventButton	*event,
					 Compose	*compose);
#if 0
static void compose_key_press_cb	(GtkWidget	*widget,
					 GdkEventKey	*event,
					 Compose	*compose);
#endif

static void compose_toggle_to_cb	(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void compose_toggle_cc_cb	(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void compose_toggle_bcc_cb	(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void compose_toggle_replyto_cb	(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void compose_toggle_followupto_cb(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void compose_toggle_attach_cb	(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void compose_toggle_ruler_cb	(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
#if USE_GPGME
static void compose_toggle_sign_cb	(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void compose_toggle_encrypt_cb	(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
#endif
static void compose_toggle_return_receipt_cb(gpointer data, guint action,
					     GtkWidget *widget);

static void compose_attach_drag_received_cb (GtkWidget		*widget,
					     GdkDragContext	*drag_context,
					     gint		 x,
					     gint		 y,
					     GtkSelectionData	*data,
					     guint		 info,
					     guint		 time,
					     gpointer		 user_data);
static void compose_insert_drag_received_cb (GtkWidget		*widget,
					     GdkDragContext	*drag_context,
					     gint		 x,
					     gint		 y,
					     GtkSelectionData	*data,
					     guint		 info,
					     guint		 time,
					     gpointer		 user_data);

static void to_activated		(GtkWidget	*widget,
					 Compose	*compose);
static void newsgroups_activated	(GtkWidget	*widget,
					 Compose	*compose);
static void subject_activated		(GtkWidget	*widget,
					 Compose	*compose);
static void cc_activated		(GtkWidget	*widget,
					 Compose	*compose);
static void bcc_activated		(GtkWidget	*widget,
					 Compose	*compose);
static void replyto_activated		(GtkWidget	*widget,
					 Compose	*compose);
static void followupto_activated	(GtkWidget	*widget,
					 Compose	*compose);
static void compose_attach_parts(Compose * compose,
				 MsgInfo * msginfo);

static GtkItemFactoryEntry compose_popup_entries[] =
{
	{N_("/_Add..."),	NULL, compose_attach_cb, 0, NULL},
	{N_("/_Remove"),	NULL, compose_attach_remove_selected, 0, NULL},
	{N_("/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_Property..."),	NULL, compose_attach_property, 0, NULL}
};

static GtkItemFactoryEntry compose_entries[] =
{
	{N_("/_File"),				NULL, NULL, 0, "<Branch>"},
	{N_("/_File/_Attach file"),		"<control>M", compose_attach_cb, 0, NULL},
	{N_("/_File/_Insert file"),		"<control>I", compose_insert_file_cb, 0, NULL},
	{N_("/_File/Insert si_gnature"),	"<control>G", compose_insert_sig, 0, NULL},
	{N_("/_File/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_File/_Close"),			"<alt>W", compose_close_cb, 0, NULL},

	{N_("/_Edit"),		   NULL,	 NULL,  0, "<Branch>"},
	{N_("/_Edit/_Undo"),	   "<control>Z", NULL,	0, NULL},
	{N_("/_Edit/_Redo"),	   "<control>Y", NULL,	0, NULL},
	{N_("/_Edit/---"),	   NULL,	 NULL,	0, "<Separator>"},
	{N_("/_Edit/Cu_t"),	   "<control>X", compose_cut_cb,    0, NULL},
	{N_("/_Edit/_Copy"),	   "<control>C", compose_copy_cb,   0, NULL},
	{N_("/_Edit/_Paste"),	   "<control>V", compose_paste_cb,  0, NULL},
	{N_("/_Edit/Select _all"), "<control>A", compose_allsel_cb, 0, NULL},
	{N_("/_Edit/---"),	   NULL,	 NULL,	0, "<Separator>"},
	{N_("/_Edit/Wrap long _lines"), "<alt>L", compose_wrap_line, 0, NULL},
	{N_("/_Edit/Edit with e_xternal editor"), "<alt>X",
					compose_ext_editor_cb,	0, NULL},

	{N_("/_Message"),		NULL,		NULL,	0, "<Branch>"},
	{N_("/_Message/_Send"),		"<shift><control>S",
					compose_send_cb, 0, NULL},
	{N_("/_Message/Send _later"),	"<shift><alt>S",
					compose_send_later_cb,  0, NULL},
	{N_("/_Message/Save to _draft folder"),
					"<alt>D",	compose_draft_cb, 0, NULL},
	{N_("/_Message/---"),		NULL,		NULL,	0, "<Separator>"},
	{N_("/_Message/_To"),		NULL, compose_toggle_to_cb, 0, "<ToggleItem>"},
	{N_("/_Message/_Cc"),		NULL, compose_toggle_cc_cb, 0, "<ToggleItem>"},
	{N_("/_Message/_Bcc"),		NULL, compose_toggle_bcc_cb, 0, "<ToggleItem>"},
	{N_("/_Message/_Reply to"),	NULL, compose_toggle_replyto_cb, 0, "<ToggleItem>"},
	{N_("/_Message/---"),		NULL,		NULL,	0, "<Separator>"},
	{N_("/_Message/_Followup to"),	NULL, compose_toggle_followupto_cb, 0, "<ToggleItem>"},
	{N_("/_Message/---"),		NULL,		NULL,	0, "<Separator>"},
	{N_("/_Message/_Attach"),	NULL, compose_toggle_attach_cb, 0, "<ToggleItem>"},
#if USE_GPGME
	{N_("/_Message/---"),		NULL,		NULL,	0, "<Separator>"},
	{N_("/_Message/Si_gn"),   	NULL, compose_toggle_sign_cb, 0, "<ToggleItem>"},
	{N_("/_Message/_Encrypt"),	NULL, compose_toggle_encrypt_cb, 0, "<ToggleItem>"},
#endif /* USE_GPGME */
	{N_("/_Message/---"),		NULL,		NULL,	0, "<Separator>"},
	{N_("/_Message/_Request Return Receipt"),	NULL, compose_toggle_return_receipt_cb, 0, "<ToggleItem>"},
	{N_("/_Tool"),			NULL, NULL,	0, "<Branch>"},
	{N_("/_Tool/Show _ruler"),	NULL, compose_toggle_ruler_cb,	0, "<ToggleItem>"},
	{N_("/_Tool/_Address book"),	"<alt>A",	compose_address_cb, 0, NULL},
	{N_("/_Help"),			NULL, NULL,	0, "<LastBranch>"},
	{N_("/_Help/_About"),		NULL,		about_show,	0, NULL}
};

static GtkTargetEntry compose_mime_types[] =
{
	{"text/uri-list", 0, 0}
};

Compose * compose_new(PrefsAccount *account)
{
	return compose_new_with_recipient(account, NULL);
}

Compose * compose_new_with_recipient(PrefsAccount *account, const gchar *to)
{
	Compose *compose;

	if (!account) account = cur_account;
	g_return_val_if_fail(account != NULL, NULL);

	compose = compose_create(account);
	compose->mode = COMPOSE_NEW;

	if (prefs_common.auto_sig)
		compose_insert_sig(compose);
	gtk_editable_set_position(GTK_EDITABLE(compose->text), 0);
	gtk_stext_set_point(GTK_STEXT(compose->text), 0);

	if (account->protocol != A_NNTP) {
		if (to) {
			compose_entry_append(compose, to, COMPOSE_TO);
			gtk_widget_grab_focus(compose->subject_entry);
		} else
			gtk_widget_grab_focus(compose->to_entry);
	} else
		gtk_widget_grab_focus(compose->newsgroups_entry);

	return compose;
}

#define CHANGE_FLAGS(msginfo) \
{ \
if (msginfo->folder->folder->change_flags != NULL) \
msginfo->folder->folder->change_flags(msginfo->folder->folder, \
				      msginfo->folder, \
				      msginfo); \
}

void compose_reply(MsgInfo *msginfo, gboolean quote, gboolean to_all,
		   gboolean to_author)
{
	Compose *compose;
	PrefsAccount *account;
	PrefsAccount *reply_account;
	GtkSText *text;

	g_return_if_fail(msginfo != NULL);
	g_return_if_fail(msginfo->folder != NULL);

	account = msginfo->folder->folder->account;
	if (!account) account = cur_account;
	g_return_if_fail(account != NULL);

	if (to_author && account->protocol == A_NNTP) {
		reply_account =
			account_find_mail_from_address(account->address);
		if (!reply_account)
			reply_account = compose_current_mail_account();
		if (!reply_account)
			return;
	} else
		reply_account = account;

	MSG_UNSET_FLAGS(msginfo->flags, MSG_FORWARDED);
	MSG_SET_FLAGS(msginfo->flags, MSG_REPLIED);

	CHANGE_FLAGS(msginfo);

	compose = compose_create(reply_account);
	compose->mode = COMPOSE_REPLY;

	if (compose_parse_header(compose, msginfo) < 0) return;
	compose_reply_set_entry(compose, msginfo, to_all, to_author);

	text = GTK_STEXT(compose->text);
	gtk_stext_freeze(text);

	if (quote) {
		FILE *fp;
		gchar *quote_str;

		if ((fp = procmime_get_text_part(msginfo)) == NULL)
			g_warning(_("Can't get text part\n"));
		else {
			quote_str = compose_quote_parse_fmt
				(compose, msginfo, prefs_common.quotefmt);
			gtk_stext_insert(text, NULL, NULL, NULL, quote_str, -1);
			g_free(quote_str);
			compose_quote_file(compose, msginfo, fp);
			fclose(fp);
		}
	}

	if (prefs_common.auto_sig)
		compose_insert_sig(compose);
	gtk_editable_set_position(GTK_EDITABLE(text), 0);
	gtk_stext_set_point(text, 0);

	gtk_stext_thaw(text);
	gtk_widget_grab_focus(compose->text);
}


static gchar *procmime_get_file_name(MimeInfo *mimeinfo)
{
	gchar *base;
	gchar *filename;

	g_return_val_if_fail(mimeinfo != NULL, NULL);

	base = mimeinfo->filename ? mimeinfo->filename
		: mimeinfo->name ? mimeinfo->name : NULL;

	if (MIME_TEXT_HTML == mimeinfo->mime_type && base == NULL)
		base = "mimetmp.html";
	else {
		base = base ? base : "mimetmp";
		base = g_basename(base);
		if (*base == '\0') base = "mimetmp";
	}

	filename = g_strconcat(get_mime_tmp_dir(), G_DIR_SEPARATOR_S,
			       base, NULL);

	return filename;
}

static gchar * mime_extract_file(gchar * source, MimeInfo *partinfo)
{
	gchar *filename;

	if (!partinfo) return;

	filename = procmime_get_file_name(partinfo);

	if (procmime_get_part(filename, source, partinfo) < 0)
		alertpanel_error
			(_("Can't get the part of multipart message."));

	return filename;
}

static void compose_attach_parts(Compose * compose,
				 MsgInfo * msginfo)
{

	FILE *fp;
	gchar *file;
	MimeInfo *mimeinfo;
	MsgInfo *tmpmsginfo;
	gchar *p;
	gchar *boundary;
	gint boundary_len = 0;
	gchar buf[BUFFSIZE];
	glong fpos, prev_fpos;
	gint npart;
	gchar * source;
	gchar * filename;

	g_return_if_fail(msginfo != NULL);
	
#if USE_GPGME
	for (;;) {
		if ((fp = procmsg_open_message(msginfo)) == NULL) return;
		mimeinfo = procmime_scan_mime_header(fp);
		if (!mimeinfo) break;

		if (!MSG_IS_ENCRYPTED(msginfo->flags) &&
		    rfc2015_is_encrypted(mimeinfo)) {
			MSG_SET_FLAGS(msginfo->flags, MSG_ENCRYPTED);
		}
		if (MSG_IS_ENCRYPTED(msginfo->flags) &&
		    !msginfo->plaintext_file  &&
		    !msginfo->decryption_failed) {
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
	mimeinfo = procmime_scan_mime_header(fp);
#endif /* USE_GPGME */

	fclose(fp);
	if (!mimeinfo) return;

	if ((fp = procmsg_open_message(msginfo)) == NULL) return;

	g_return_if_fail(mimeinfo != NULL);
	g_return_if_fail(mimeinfo->mime_type != MIME_TEXT);

	if (mimeinfo->mime_type == MIME_MULTIPART) {
		g_return_if_fail(mimeinfo->boundary != NULL);
		g_return_if_fail(mimeinfo->sub == NULL);
	}
	g_return_if_fail(fp != NULL);

	boundary = mimeinfo->boundary;

	if (boundary) {
		boundary_len = strlen(boundary);

		/* look for first boundary */
		while ((p = fgets(buf, sizeof(buf), fp)) != NULL)
			if (IS_BOUNDARY(buf, boundary, boundary_len)) break;
		if (!p) {
			fclose(fp);
			return;
		}
	}

	if ((fpos = ftell(fp)) < 0) {
		perror("ftell");
		fclose(fp);
		return;
	}

	for (npart = 0;; npart++) {
		MimeInfo *partinfo;
		gboolean eom = FALSE;

		prev_fpos = fpos;

		partinfo = procmime_scan_mime_header(fp);
		if (!partinfo) break;

		if (npart != 0)
			procmime_mimeinfo_insert(mimeinfo, partinfo);
		else
			procmime_mimeinfo_free(partinfo);

		/* look for next boundary */
		buf[0] = '\0';
		while ((p = fgets(buf, sizeof(buf), fp)) != NULL) {
			if (IS_BOUNDARY(buf, boundary, boundary_len)) {
				if (buf[2 + boundary_len]     == '-' &&
				    buf[2 + boundary_len + 1] == '-')
					eom = TRUE;
				break;
			}
		}
		if (p == NULL)
			eom = TRUE;	/* broken MIME message */
		fpos = ftell(fp);

		partinfo->size = fpos - prev_fpos - strlen(buf);

		if (eom) break;
	}

	source = procmsg_get_message_file_path(msginfo);

	g_return_if_fail(mimeinfo != NULL);

	if (!mimeinfo->main && mimeinfo->parent)
		{
			filename = mime_extract_file(source, mimeinfo);

			compose_attach_append(compose, filename,
					      mimeinfo->mime_type);

			g_free(filename);
		}

	if (mimeinfo->sub && mimeinfo->sub->children)
		{
			filename = mime_extract_file(source, mimeinfo->sub);

			compose_attach_append(compose, filename,
					      mimeinfo->sub->mime_type);

			g_free(filename);
		}

	if (mimeinfo->children) {
		MimeInfo *child;

		child = mimeinfo->children;
		while (child) {
			filename = mime_extract_file(source, child);

			compose_attach_append(compose, filename,
					      child->mime_type);

			g_free(filename);

			child = child->next;
		}
	}

	fclose(fp);

	procmime_mimeinfo_free_all(mimeinfo);
}


#define INSERT_FW_HEADER(var, hdr) \
if (msginfo->var && *msginfo->var) { \
	gtk_stext_insert(text, NULL, NULL, NULL, hdr, -1); \
	gtk_stext_insert(text, NULL, NULL, NULL, msginfo->var, -1); \
	gtk_stext_insert(text, NULL, NULL, NULL, "\n", 1); \
}

Compose * compose_forward(PrefsAccount * account, MsgInfo *msginfo,
			  gboolean as_attach)
{
	Compose *compose;
	/*	PrefsAccount *account; */
	GtkSText *text;
	FILE *fp;
	gchar buf[BUFFSIZE];

	g_return_val_if_fail(msginfo != NULL, NULL);
	g_return_val_if_fail(msginfo->folder != NULL, NULL);

	if (account == NULL) {
		account = msginfo->folder->folder->account;
		if (!account) account = cur_account;
	}
	g_return_val_if_fail(account != NULL, NULL);

	MSG_UNSET_FLAGS(msginfo->flags, MSG_REPLIED);
	MSG_SET_FLAGS(msginfo->flags, MSG_FORWARDED);

	CHANGE_FLAGS(msginfo);

	compose = compose_create(account);
	compose->mode = COMPOSE_FORWARD;

	if (msginfo->subject && *msginfo->subject) {
		gtk_entry_set_text(GTK_ENTRY(compose->subject_entry), "Fw: ");
		gtk_entry_append_text(GTK_ENTRY(compose->subject_entry),
				      msginfo->subject);
	}

	text = GTK_STEXT(compose->text);
	gtk_stext_freeze(text);

	if (as_attach) {
		gchar *msgfile;

		msgfile = procmsg_get_message_file_path(msginfo);
		if (!is_file_exist(msgfile))
			g_warning(_("%s: file not exist\n"), msgfile);
		else
			compose_attach_append(compose, msgfile,
					      MIME_MESSAGE_RFC822);

		g_free(msgfile);
	} else {
		if ((fp = procmime_get_text_part(msginfo)) == NULL)
			g_warning(_("Can't get text part\n"));
		else {
			/* insert header */
			gtk_stext_insert(text, NULL, NULL, NULL,
					_("\n\nBegin forwarded message:\n\n"), -1);
			INSERT_FW_HEADER(date,       "Date: ");
			INSERT_FW_HEADER(from,       "From: ");
			INSERT_FW_HEADER(to,         "To: ");
			INSERT_FW_HEADER(newsgroups, "Newsgroups: ");
			INSERT_FW_HEADER(subject,    "Subject: ");
			gtk_stext_insert(text, NULL, NULL, NULL, "\n\n", 2);

			/* forward body */
			while (fgets(buf, sizeof(buf), fp) != NULL)
				gtk_stext_insert(text, NULL, NULL, NULL,
						buf, -1);
			fclose(fp);
		}
		compose_attach_parts(compose, msginfo);
	}

	if (prefs_common.auto_sig)
		compose_insert_sig(compose);
	gtk_editable_set_position(GTK_EDITABLE(compose->text), 0);
	gtk_stext_set_point(GTK_STEXT(compose->text), 0);

	gtk_stext_thaw(text);
	if (account->protocol != A_NNTP)
		gtk_widget_grab_focus(compose->to_entry);
	else
		gtk_widget_grab_focus(compose->newsgroups_entry);

	return compose;
}

#undef INSERT_FW_HEADER

void compose_reedit(MsgInfo *msginfo)
{
	Compose *compose;
	PrefsAccount *account;
	GtkSText *text;
	FILE *fp;
	gchar buf[BUFFSIZE];

	g_return_if_fail(msginfo != NULL);
	g_return_if_fail(msginfo->folder != NULL);

	account = msginfo->folder->folder->account;
	if (!account) account = cur_account;
	g_return_if_fail(account != NULL);

	compose = compose_create(account);
	compose->mode = COMPOSE_REEDIT_DRAFT;
	compose->targetinfo = procmsg_msginfo_copy(msginfo);

	if (compose_parse_header(compose, msginfo) < 0) return;
	compose_reedit_set_entry(compose, msginfo);

	text = GTK_STEXT(compose->text);
	gtk_stext_freeze(text);

	if ((fp = procmime_get_text_part(msginfo)) == NULL)
		g_warning(_("Can't get text part\n"));
	else {
		while (fgets(buf, sizeof(buf), fp) != NULL)
			gtk_stext_insert(text, NULL, NULL, NULL, buf, -1);
		fclose(fp);
	}
	compose_attach_parts(compose, msginfo);

	gtk_stext_thaw(text);
	gtk_widget_grab_focus(compose->text);
}

GList *compose_get_compose_list(void)
{
	return compose_list;
}

void compose_entry_append(Compose *compose, const gchar *address,
			  ComposeEntryType type)
{
	GtkEntry *entry;
	const gchar *text;

	if (!address || *address == '\0') return;

	switch (type) {
	case COMPOSE_CC:
		entry = GTK_ENTRY(compose->cc_entry);
		break;
	case COMPOSE_BCC:
		entry = GTK_ENTRY(compose->bcc_entry);
		break;
	case COMPOSE_NEWSGROUPS:
		entry = GTK_ENTRY(compose->newsgroups_entry);
		break;
	case COMPOSE_TO:
	default:
		entry = GTK_ENTRY(compose->to_entry);
		break;
	}

	text = gtk_entry_get_text(entry);
	if (*text != '\0')
		gtk_entry_append_text(entry, ", ");
	gtk_entry_append_text(entry, address);
}

static gint compose_parse_header(Compose *compose, MsgInfo *msginfo)
{
	static HeaderEntry hentry[] = {{"Reply-To:",	NULL, TRUE},
				       {"Cc:",		NULL, FALSE},
				       {"References:",	NULL, FALSE},
				       {"Bcc:",		NULL, FALSE},
				       {"Newsgroups:",  NULL, FALSE},
				       {"Followup-To:", NULL, FALSE},
				       {NULL,		NULL, FALSE}};

	enum
	{
		H_REPLY_TO	= 0,
		H_CC		= 1,
		H_REFERENCES	= 2,
		H_BCC		= 3,
		H_NEWSGROUPS    = 4,
		H_FOLLOWUP_TO	= 5
	};

	FILE *fp;

	g_return_val_if_fail(msginfo != NULL, -1);

	if ((fp = procmsg_open_message(msginfo)) == NULL) return -1;
	procheader_get_header_fields(fp, hentry);
	fclose(fp);

	if (hentry[H_REPLY_TO].body != NULL) {
		conv_unmime_header_overwrite(hentry[H_REPLY_TO].body);
		compose->replyto = hentry[H_REPLY_TO].body;
		hentry[H_REPLY_TO].body = NULL;
	}
	if (hentry[H_CC].body != NULL) {
		conv_unmime_header_overwrite(hentry[H_CC].body);
		compose->cc = hentry[H_CC].body;
		hentry[H_CC].body = NULL;
	}
	if (hentry[H_REFERENCES].body != NULL) {
		if (compose->mode == COMPOSE_REEDIT_DRAFT)
			compose->references = hentry[H_REFERENCES].body;
		else {
			compose->references = compose_parse_references
				(hentry[H_REFERENCES].body, msginfo->msgid);
			g_free(hentry[H_REFERENCES].body);
		}
		hentry[H_REFERENCES].body = NULL;
	}
	if (hentry[H_BCC].body != NULL) {
		if (compose->mode == COMPOSE_REEDIT_DRAFT) {
			conv_unmime_header_overwrite(hentry[H_BCC].body);
			compose->bcc = hentry[H_BCC].body;
		} else
			g_free(hentry[H_BCC].body);
		hentry[H_BCC].body = NULL;
	}
	if (hentry[H_NEWSGROUPS].body != NULL) {
		compose->newsgroups = hentry[H_NEWSGROUPS].body;
		hentry[H_NEWSGROUPS].body = NULL;
	}
	if (hentry[H_FOLLOWUP_TO].body != NULL) {
		conv_unmime_header_overwrite(hentry[H_FOLLOWUP_TO].body);
		compose->followup_to = hentry[H_FOLLOWUP_TO].body;
		hentry[H_FOLLOWUP_TO].body = NULL;
	}

	if (compose->mode == COMPOSE_REEDIT_DRAFT && msginfo->inreplyto)
		compose->inreplyto = g_strdup(msginfo->inreplyto);
	else if (compose->mode != COMPOSE_REEDIT_DRAFT &&
		 msginfo->msgid && *msginfo->msgid) {
		compose->inreplyto = g_strdup(msginfo->msgid);

		if (!compose->references) {
			if (msginfo->inreplyto && *msginfo->inreplyto)
				compose->references =
					g_strdup_printf("<%s>\n\t<%s>",
							msginfo->inreplyto,
							msginfo->msgid);
			else
				compose->references =
					g_strconcat("<", msginfo->msgid, ">",
						    NULL);
		}
	}

	return 0;
}

static gchar *compose_parse_references(const gchar *ref, const gchar *msgid)
{
	GSList *ref_id_list, *cur;
	GString *new_ref;
	gchar *new_ref_str;

	ref_id_list = references_list_append(NULL, ref);
	if (!ref_id_list) return NULL;
	if (msgid && *msgid)
		ref_id_list = g_slist_append(ref_id_list, g_strdup(msgid));

	for (;;) {
		gint len = 0;

		for (cur = ref_id_list; cur != NULL; cur = cur->next)
			/* "<" + Message-ID + ">" + CR+LF+TAB */
			len += strlen((gchar *)cur->data) + 5;

		if (len > MAX_REFERENCES_LEN) {
			/* remove second message-ID */
			if (ref_id_list && ref_id_list->next &&
			    ref_id_list->next->next) {
				g_free(ref_id_list->next->data);
				ref_id_list = g_slist_remove
					(ref_id_list, ref_id_list->next->data);
			} else {
				slist_free_strings(ref_id_list);
				g_slist_free(ref_id_list);
				return NULL;
			}
		} else
			break;
	}

	new_ref = g_string_new("");
	for (cur = ref_id_list; cur != NULL; cur = cur->next) {
		if (new_ref->len > 0)
			g_string_append(new_ref, "\n\t");
		g_string_sprintfa(new_ref, "<%s>", (gchar *)cur->data);
	}

	slist_free_strings(ref_id_list);
	g_slist_free(ref_id_list);

	new_ref_str = new_ref->str;
	g_string_free(new_ref, FALSE);

	return new_ref_str;
}

static void compose_quote_file(Compose *compose, MsgInfo *msginfo, FILE *fp)
{
	GtkSText *text = GTK_STEXT(compose->text);
	gchar *qmark;
	gchar *quote_str;
	GdkColor *qcolor = NULL;
	gchar buf[BUFFSIZE];
	gint qlen;
	gchar *linep, *cur, *leftp;
	gint line_len, cur_len;
	gint wrap_len;
	gint str_len;
	gint ch_len;

	/* if (prefs_common.enable_color) qcolor = &quote_color; */
	if (prefs_common.quotemark && *prefs_common.quotemark)
		qmark = prefs_common.quotemark;
	else
		qmark = "> ";
	quote_str = compose_quote_parse_fmt(compose, msginfo, qmark);
	g_return_if_fail(quote_str != NULL);
	qlen = strlen(quote_str);

	if (!prefs_common.linewrap_quote ||
	    prefs_common.linewrap_len <= qlen) {
		while (fgets(buf, sizeof(buf), fp) != NULL) {
			gtk_stext_insert(text, NULL, qcolor, NULL,
					quote_str, -1);
			gtk_stext_insert(text, NULL, qcolor, NULL, buf, -1);
		}
		g_free(quote_str);
		return;
	}

	wrap_len = prefs_common.linewrap_len - qlen;

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		strretchomp(buf);
		str_len = strlen(buf);

		if (str_len <= wrap_len) {
			gtk_stext_insert(text, NULL, qcolor, NULL,
					quote_str, -1);
			gtk_stext_insert(text, NULL, qcolor, NULL, buf, -1);
			gtk_stext_insert(text, NULL, NULL, NULL, "\n", 1);
			continue;
		}

		linep = cur = leftp = buf;
		line_len = cur_len = 0;

		while (*cur != '\0') {
			ch_len = mblen(cur, MB_CUR_MAX);
			if (ch_len < 0) ch_len = 1;

			if (ch_len == 1 && isspace(*cur)) {
				linep = cur + ch_len;
				line_len = cur_len + ch_len;
			}

			if (cur_len + ch_len > wrap_len && line_len > 0) {
				gtk_stext_insert(text, NULL, qcolor, NULL,
						quote_str, -1);

				if (isspace(*(linep - 1)))
					gtk_stext_insert(text, NULL,
							qcolor, NULL,
							leftp, line_len - 1);
				else
					gtk_stext_insert(text, NULL,
							qcolor, NULL,
							leftp, line_len);
				gtk_stext_insert(text, NULL, NULL, NULL,
						"\n", 1);

				leftp = linep;
				cur_len = cur_len - line_len + ch_len;
				line_len = 0;
				cur += ch_len;
				continue;
			}

			if (ch_len > 1) {
				linep = cur + ch_len;
				line_len = cur_len + ch_len;
			}
			cur_len += ch_len;
			cur += ch_len;
		}

		if (*leftp) {
			gtk_stext_insert(text, NULL, qcolor, NULL,
					quote_str, -1);
			gtk_stext_insert(text, NULL, qcolor, NULL, leftp, -1);
			gtk_stext_insert(text, NULL, NULL, NULL, "\n", 1);
		}
	}

	g_free(quote_str);
}

static gchar *compose_quote_parse_fmt(Compose *compose, MsgInfo *msginfo,
				      const gchar *fmt)
{
	gchar *ext_str;
	size_t buf_len = 1024;
	size_t ext_len = 0;
	gchar *str;
	gchar *mbs;
	wchar_t *wcsfmt;
	wchar_t *sp;
	gchar tmp[3];

	if (!fmt || *fmt == '\0') return 0;

	Xalloca(mbs, sizeof(wchar_t) + 1, return 0);
	Xalloca(wcsfmt, (strlen(fmt) + 1) * sizeof(wchar_t), return 0);
	mbstowcs(wcsfmt, fmt, strlen(fmt) + 1);
	sp = wcsfmt;

	ext_str = g_malloc(sizeof(gchar) * buf_len);
	g_return_val_if_fail(ext_str != NULL, NULL);

	while (*sp) {
		gint len;

		len = wctomb(mbs, *sp);
		mbs[len] = '\0';

		if (*mbs == '%') {
			gchar *p;

			wctomb(mbs, *(++sp));
			str = NULL;

			switch (*mbs) {
			case 'd':
				str = msginfo->date;
				sp++;
				break;
			case 'f':
				str = msginfo->from;
				sp++;
				break;
			case 'I':	/* initial */
				if (!msginfo->fromname) {sp++; break;}
				p = msginfo->fromname;
				tmp[0] = tmp[1] = tmp[2] = '\0';

				if (*p && isalnum(*p))
					tmp[0] = toupper(*p);
				else {
					sp++;
					break;
				}

				while (*p) {
					while (*p && !isspace(*p)) p++;
					while (*p && isspace(*p)) p++;
					if (*p && isalnum(*p))
						tmp[1] = toupper(*p);
				}

				if (tmp[1]) str = tmp;
				sp++;
				break;
			case 'n':
				str = msginfo->fromname;
				sp++;
				break;
			case 'N':
				if (!msginfo->fromname) {sp++; break;}
				Xstrdup_a(str, msginfo->fromname,
					  {sp++; break;});
				p = str;
				while (*p && !isspace(*p)) p++;
				*p = '\0';
				sp++;
				break;
			case 's':
				str = msginfo->subject;
				sp++;
				break;
			case 't':
				str = msginfo->to;
				sp++;
				break;
			case 'i':
				if (!msginfo->msgid) {sp++; break;}
				Xalloca(str, strlen(msginfo->msgid) + 3,
					{sp++; break;});
				g_snprintf(str, strlen(msginfo->msgid) + 3,
					   "<%s>", msginfo->msgid);
				sp++;
				break;
			case '%':
				str = "%";
				sp++;
				break;
			default:
				break;
			}

			if (str) {
				while (ext_len + strlen(str) + 1 > buf_len)
					buf_len += 1024;
				ext_str = g_realloc(ext_str,
						    sizeof(gchar) * buf_len);
				g_return_val_if_fail(ext_str != NULL, NULL);
				strcpy(ext_str + ext_len, str);
				ext_len += strlen(str);
			}
		} else if (*mbs == '\\') {
			wctomb(mbs, *(++sp));
			str = NULL;

			switch (*mbs) {
			case 'n':
				str = "\n";
				break;
			case 't':
				str = "\t";
				break;
			case '\\':
				str = "\\";
				break;
			default:
				break;
			}

			if (str) {
				while (ext_len + strlen(str) + 1 > buf_len)
					buf_len += 1024;
				ext_str = g_realloc(ext_str,
						    sizeof(gchar) * buf_len);
				g_return_val_if_fail(ext_str != NULL, NULL);
				strcpy(ext_str + ext_len, str);
				ext_len += strlen(str);
				sp++;
			}
		} else {
			while (ext_len + len + 1 > buf_len) buf_len += 1024;
			ext_str = g_realloc(ext_str, sizeof(gchar) * buf_len);
			g_return_val_if_fail(ext_str != NULL, NULL);
			strcpy(ext_str + ext_len, mbs);
			ext_len += len;
			sp++;
		}
	}

	if (ext_str)
		ext_str = g_realloc(ext_str, strlen(ext_str) + 1);

	return ext_str;
}

static void compose_reply_set_entry(Compose *compose, MsgInfo *msginfo,
				    gboolean to_all, gboolean to_author)
{
	GSList *cc_list;
	GSList *cur;
	gchar *from;
	GHashTable *to_table;

	g_return_if_fail(compose->account != NULL);
	g_return_if_fail(msginfo != NULL);

	if (compose->account->protocol != A_NNTP)
		gtk_entry_set_text(GTK_ENTRY(compose->to_entry),
				   ( (compose->replyto && !to_author) 
				     ? compose->replyto
				     : msginfo->from ? msginfo->from : ""));
	if (compose->account->protocol == A_NNTP)
		gtk_entry_set_text(GTK_ENTRY(compose->newsgroups_entry),
				   compose->followup_to ? compose->followup_to
				   : compose->newsgroups ? compose->newsgroups
				   : "");

	if (msginfo->subject && *msginfo->subject) {
		gchar *buf, *buf2, *p;

		buf = g_strdup(msginfo->subject);
		while (!strncasecmp(buf, "Re:", 3)) {
			p = buf + 3;
			while (isspace(*p)) p++;
			memmove(buf, p, strlen(p) + 1);
		}

		buf2 = g_strdup_printf("Re: %s", buf);
		gtk_entry_set_text(GTK_ENTRY(compose->subject_entry), buf2);
		g_free(buf2);
		g_free(buf);
	} else
		gtk_entry_set_text(GTK_ENTRY(compose->subject_entry), "Re: ");

	if (!to_all || compose->account->protocol == A_NNTP) return;

	from = g_strdup(compose->replyto ? compose->replyto :
			msginfo->from ? msginfo->from : "");
	extract_address(from);

	cc_list = address_list_append(NULL, msginfo->to);
	cc_list = address_list_append(cc_list, compose->cc);

	to_table = g_hash_table_new(g_str_hash, g_str_equal);
	g_hash_table_insert(to_table, from, GINT_TO_POINTER(1));
	if (compose->account)
		g_hash_table_insert(to_table, compose->account->address,
				    GINT_TO_POINTER(1));

	/* remove address on To: and that of current account */
	for (cur = cc_list; cur != NULL; ) {
		GSList *next = cur->next;

		if (g_hash_table_lookup(to_table, cur->data) != NULL)
			cc_list = g_slist_remove(cc_list, cur->data);
		else
			g_hash_table_insert(to_table, cur->data, cur);

		cur = next;
	}
	g_hash_table_destroy(to_table);
	g_free(from);

	if (cc_list) {
		for (cur = cc_list; cur != NULL; cur = cur->next)
			compose_entry_append(compose, (gchar *)cur->data,
					     COMPOSE_CC);
		slist_free_strings(cc_list);
		g_slist_free(cc_list);
	}
}

#define SET_ENTRY(entry, str) \
{ \
	if (str && *str) \
		gtk_entry_set_text(GTK_ENTRY(compose->entry), str); \
}

static void compose_reedit_set_entry(Compose *compose, MsgInfo *msginfo)
{
	g_return_if_fail(msginfo != NULL);

	SET_ENTRY(to_entry, msginfo->to);
	SET_ENTRY(subject_entry, msginfo->subject);
	SET_ENTRY(cc_entry, compose->cc);
	SET_ENTRY(bcc_entry, compose->bcc);
	SET_ENTRY(reply_entry, compose->replyto);

	if (compose->bcc) {
		GtkItemFactory *ifactory;
		GtkWidget *menuitem;

		ifactory = gtk_item_factory_from_widget(compose->menubar);
		menuitem = gtk_item_factory_get_item(ifactory, "/Message/Bcc");
		gtk_check_menu_item_set_active
			(GTK_CHECK_MENU_ITEM(menuitem), TRUE);
	}
	if (compose->replyto) {
		GtkItemFactory *ifactory;
		GtkWidget *menuitem;

		ifactory = gtk_item_factory_from_widget(compose->menubar);
		menuitem = gtk_item_factory_get_item
			(ifactory, "/Message/Reply to");
		gtk_check_menu_item_set_active
			(GTK_CHECK_MENU_ITEM(menuitem), TRUE);
	}
}

#undef SET_ENTRY

static void compose_insert_sig(Compose *compose)
{
	gchar *sigfile;

	if (compose->account && compose->account->sig_path)
		sigfile = g_strdup(compose->account->sig_path);
	else {
		sigfile = g_strconcat(get_home_dir(), G_DIR_SEPARATOR_S,
				      DEFAULT_SIGNATURE, NULL);
	}

	if (!is_file_or_fifo_exist(sigfile)) {
		g_free(sigfile);
		return;
	}

	gtk_stext_insert(GTK_STEXT(compose->text), NULL, NULL, NULL, "\n\n", 2);
	if (prefs_common.sig_sep) {
		gtk_stext_insert(GTK_STEXT(compose->text), NULL, NULL, NULL,
				prefs_common.sig_sep, -1);
		gtk_stext_insert(GTK_STEXT(compose->text), NULL, NULL, NULL,
				"\n", 1);
	}

	compose_insert_file(compose, sigfile);
	g_free(sigfile);
}

static void compose_insert_file(Compose *compose, const gchar *file)
{
	GtkSText *text = GTK_STEXT(compose->text);
	gchar buf[BUFFSIZE];
	FILE *fp;

	g_return_if_fail(file != NULL);

	if ((fp = fopen(file, "r")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		return;
	}

	gtk_stext_freeze(text);

	while (fgets(buf, sizeof(buf), fp) != NULL)
		gtk_stext_insert(text, NULL, NULL, NULL, buf, -1);

	gtk_stext_thaw(text);

	fclose(fp);
}

static void compose_attach_append(Compose *compose, const gchar *file,
				  ContentType cnttype)
{
	AttachInfo *ainfo;
	gchar *text[N_ATTACH_COLS];
	off_t size;
	gint row;

	if (!is_file_exist(file)) {
		g_warning(_("File %s doesn't exist\n"), file);
		return;
	}
	if ((size = get_file_size(file)) < 0) {
		g_warning(_("Can't get file size of %s\n"), file);
		return;
	}
	if (size == 0) {
		alertpanel_notice(_("File %s is empty\n"), file);
		return;
	}

	if (!compose->use_attach) {
		GtkItemFactory *ifactory;
		GtkWidget *menuitem;

		ifactory = gtk_item_factory_from_widget(compose->menubar);
		menuitem = gtk_item_factory_get_item(ifactory,
						     "/Message/Attach");
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem),
					       TRUE);
	}

	ainfo = g_new0(AttachInfo, 1);
	ainfo->file = g_strdup(file);

	if (cnttype == MIME_MESSAGE_RFC822) {
		ainfo->content_type = g_strdup("message/rfc822");
		ainfo->encoding = ENC_7BIT;
		ainfo->name = g_strdup_printf(_("Message: %s"),
					      g_basename(file));
	} else {
		ainfo->content_type = procmime_get_mime_type(file);
		if (!ainfo->content_type)
			ainfo->content_type =
				g_strdup("application/octet-stream");
		ainfo->encoding = ENC_BASE64;
		ainfo->name = g_strdup(g_basename(file));
	}
	ainfo->size = size;

	text[COL_MIMETYPE] = ainfo->content_type;
	text[COL_SIZE] = to_human_readable(size);
	text[COL_NAME] = ainfo->name;

	row = gtk_clist_append(GTK_CLIST(compose->attach_clist), text);
	gtk_clist_set_row_data(GTK_CLIST(compose->attach_clist), row, ainfo);

	if (cnttype != MIME_MESSAGE_RFC822)
		compose_changed_cb(NULL, compose);
}

static void compose_wrap_line(Compose *compose)
{
	GtkSText *text = GTK_STEXT(compose->text);
	guint text_len;
	guint line_pos = 0, cur_pos = 0;
	gint line_len = 0, cur_len = 0;
	gint ch_len;
	gchar cbuf[MB_CUR_MAX];

	gtk_stext_freeze(text);

	text_len = gtk_stext_get_length(text);

	for (; cur_pos < text_len; cur_pos++) {
		if (text->use_wchar)
			ch_len = wctomb
				(cbuf, (wchar_t)GTK_STEXT_INDEX(text, cur_pos));
		else {
			cbuf[0] = GTK_STEXT_INDEX(text, cur_pos);
			ch_len = 1;
		}

		if (ch_len == 1 && *cbuf == '\n') {
			line_pos = cur_pos + 1;
			line_len = cur_len = 0;
			continue;
		}

		if (ch_len < 0) {
			cbuf[0] = '\0';
			ch_len = 1;
		}

		if (ch_len == 1 && isspace(*cbuf)) {
			line_pos = cur_pos + 1;
			line_len = cur_len + ch_len;
		}

		if (cur_len + ch_len > prefs_common.linewrap_len &&
		    line_len > 0) {
			gint tlen;

			if (text->use_wchar)
				tlen = wctomb(cbuf, (wchar_t)GTK_STEXT_INDEX(text, line_pos - 1));
			else {
				cbuf[0] = GTK_STEXT_INDEX(text, line_pos - 1);
				tlen = 1;
			}
			if (tlen == 1 && isspace(*cbuf)) {
				gtk_stext_set_point(text, line_pos);
				gtk_stext_backward_delete(text, 1);
				text_len--;
				cur_pos--;
				line_pos--;
				cur_len--;
				line_len--;
			}

			gtk_stext_set_point(text, line_pos);
			gtk_stext_insert(text, NULL, NULL, NULL, "\n", 1);
			text_len++;
			cur_pos++;
			line_pos++;
			cur_len = cur_len - line_len + ch_len;
			line_len = 0;
			continue;
		}

		if (ch_len > 1) {
			line_pos = cur_pos + 1;
			line_len = cur_len + ch_len;
		}
		cur_len += ch_len;
	}

	gtk_stext_thaw(text);
}

static void compose_set_title(Compose *compose)
{
	gchar *str;
	gchar *edited;

	edited = compose->modified ? _(" [Edited]") : "";
	if (compose->account && compose->account->address)
		str = g_strdup_printf(_("%s - Compose message%s"),
				      compose->account->address, edited);
	else
		str = g_strdup_printf(_("Compose message%s"), edited);
	gtk_window_set_title(GTK_WINDOW(compose->window), str);
	g_free(str);
}

/**
 * compose_current_mail_account:
 * 
 * Find a current mail account (the currently selected account, or the
 * default account, if a news account is currently selected).  If a
 * mail account cannot be found, display an error message.
 * 
 * Return value: Mail account, or NULL if not found.
 **/
static PrefsAccount *
compose_current_mail_account(void)
{
	PrefsAccount *ac;

	if (cur_account && cur_account->protocol != A_NNTP)
		ac = cur_account;
	else {
		ac = account_get_default();
		if (!ac || ac->protocol == A_NNTP) {
			alertpanel_error(_("Account for sending mail is not specified.\n"
					   "Please select a mail account before sending."));
			return NULL;
		}
	}
	return ac;
}

gint compose_send(Compose *compose)
{
	gchar tmp[MAXPATHLEN + 1];
	gchar *to, *newsgroups;
	gint ok = 0;
	static gboolean lock = FALSE;

	if (lock) return 1;

	g_return_val_if_fail(compose->account != NULL, -1);
	g_return_val_if_fail(compose->orig_account != NULL, -1);

	lock = TRUE;

	to = gtk_entry_get_text(GTK_ENTRY(compose->to_entry));
	newsgroups = gtk_entry_get_text(GTK_ENTRY(compose->newsgroups_entry));
	if (*to == '\0' && *newsgroups == '\0') {
		alertpanel_error(_("Recipient is not specified."));
		lock = FALSE;
		return 1;
	}

	/* write to temporary file */
	g_snprintf(tmp, sizeof(tmp), "%s%ctmpmsg%d",
		   get_rc_dir(), G_DIR_SEPARATOR, (gint)compose);

	if (prefs_common.linewrap_at_send)
		compose_wrap_line(compose);

	if (compose_write_to_file(compose, tmp, FALSE) < 0) {
		lock = FALSE;
		return -1;
	}

	if (!compose->to_list && !compose->newsgroup_list) {
		g_warning(_("can't get recipient list."));
		unlink(tmp);
		lock = FALSE;
		return -1;
	}

	if (compose->to_list) {
		PrefsAccount *ac;

		if (compose->account->protocol != A_NNTP)
			ac = compose->account;
		else if (compose->orig_account->protocol != A_NNTP)
			ac = compose->orig_account;
		else {
			ac = compose_current_mail_account();
			if (!ac) {
				unlink(tmp);
				lock = FALSE;
				return -1;
			}
		}
		ok = send_message(tmp, ac, compose->to_list);
		statusbar_pop_all();
	}

	if (ok == 0 && compose->newsgroup_list) {
		Folder *folder;

		if (compose->account->protocol == A_NNTP)
			folder = FOLDER(compose->account->folder);
		else
			folder = FOLDER(compose->orig_account->folder);

		ok = news_post(folder, tmp);
		if (ok < 0) {
			alertpanel_error(_("Error occurred while posting the message to %s ."),
					 compose->account->nntp_server);
			unlink(tmp);
			lock = FALSE;
			return -1;
		}
	}

	/* queue message if failed to send */
	if (ok < 0) {
		if (prefs_common.queue_msg) {
			AlertValue val;

			val = alertpanel
				(_("Queueing"),
				 _("Error occurred while sending the message.\n"
				   "Put this message into queue folder?"),
				 _("OK"), _("Cancel"), NULL);
			if (G_ALERTDEFAULT == val) {
				ok = compose_queue(compose, tmp);
				if (ok < 0)
					alertpanel_error(_("Can't queue the message."));
			}
		} else
			alertpanel_error(_("Error occurred while sending the message."));
	}

	/* save message to outbox */
	if (ok == 0 && prefs_common.savemsg) {
		if (compose_save_to_outbox(compose, tmp) < 0)
			alertpanel_error
				(_("Can't save the message to outbox."));
	}

	unlink(tmp);
	lock = FALSE;
	return ok;
}

static gint compose_write_to_file(Compose *compose, const gchar *file,
				  gboolean is_draft)
{
	FILE *fp;
	size_t len;
	gchar *chars;
	gchar *buf;
	const gchar *out_codeset;
	EncodingType encoding;

	if ((fp = fopen(file, "w")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		return -1;
	}

	/* chmod for security */
	if (change_file_mode_rw(fp, file) < 0) {
		FILE_OP_ERROR(file, "chmod");
		g_warning(_("can't change file mode\n"));
	}

	/* get all composed text */
	chars = gtk_editable_get_chars(GTK_EDITABLE(compose->text), 0, -1);
	len = strlen(chars);
	if (is_ascii_str(chars)) {
		buf = g_strdup(chars);
		out_codeset = "US-ASCII";
		encoding = ENC_7BIT;
	} else {
		const gchar *src_codeset;

		out_codeset = conv_get_outgoing_charset_str();
		if (!strcasecmp(out_codeset, "US-ASCII"))
			out_codeset = "ISO-8859-1";
		encoding = procmime_get_encoding_for_charset(out_codeset);
		debug_print("charset = %s, encoding = %s\n",
			    out_codeset, procmime_get_encoding_str(encoding));

		src_codeset = conv_get_current_charset_str();
		buf = conv_codeset_strdup(chars, src_codeset, out_codeset);
		if (!buf) {
			g_free(chars);
			fclose(fp);
			unlink(file);
			alertpanel_error(_("Can't convert the codeset of the message."));
			return -1;
		}
	}
	g_free(chars);

	/* write headers */
	if (compose_write_headers
		(compose, fp, out_codeset, encoding, is_draft) < 0) {
		g_warning(_("can't write headers\n"));
		fclose(fp);
		unlink(file);
		g_free(buf);
		return -1;
	}

	if (compose->use_attach) {
#if USE_GPGME
            /* This prolog message is ignored by mime software and
             * because it would make our signing/encryption task
             * tougher, we don't emit it in that case */
            if (!compose->use_signing && !compose->use_encryption)
#endif
		fputs("This is a multi-part message in MIME format.\n", fp);

		fprintf(fp, "\n--%s\n", compose->boundary);
		fprintf(fp, "Content-Type: text/plain; charset=%s\n",
			out_codeset);
		fprintf(fp, "Content-Transfer-Encoding: %s\n",
			procmime_get_encoding_str(encoding));
		fputc('\n', fp);
	}

	/* write body */
	len = strlen(buf);
	if (encoding == ENC_BASE64) {
		gchar outbuf[B64_BUFFSIZE];
		gint i, l;

		for (i = 0; i < len; i += B64_LINE_SIZE) {
			l = MIN(B64_LINE_SIZE, len - i);
			to64frombits(outbuf, buf + i, l);
			fputs(outbuf, fp);
			fputc('\n', fp);
		}
	} else if (fwrite(buf, sizeof(gchar), len, fp) != len) {
		FILE_OP_ERROR(file, "fwrite");
		fclose(fp);
		unlink(file);
		g_free(buf);
		return -1;
	}
	g_free(buf);

	if (compose->use_attach)
		compose_write_attach(compose, fp);

	if (fclose(fp) == EOF) {
		FILE_OP_ERROR(file, "fclose");
		unlink(file);
		return -1;
	}

#if USE_GPGME
	if (compose->use_signing) {
		if (rfc2015_sign(file, compose->account) < 0) {
			unlink(file);
			return -1;
		}
	}
	if (compose->use_encryption) {
		if (rfc2015_encrypt(file, compose->to_list) < 0) {
			unlink(file);
			return -1;
		}
	}
#endif /* USE_GPGME */

	return 0;
}

static gint compose_write_body_to_file(Compose *compose, const gchar *file)
{
	FILE *fp;
	size_t len;
	gchar *chars;

	if ((fp = fopen(file, "w")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		return -1;
	}

	/* chmod for security */
	if (change_file_mode_rw(fp, file) < 0) {
		FILE_OP_ERROR(file, "chmod");
		g_warning(_("can't change file mode\n"));
	}

	chars = gtk_editable_get_chars(GTK_EDITABLE(compose->text), 0, -1);

	/* write body */
	len = strlen(chars);
	if (fwrite(chars, sizeof(gchar), len, fp) != len) {
		FILE_OP_ERROR(file, "fwrite");
		g_free(chars);
		fclose(fp);
		unlink(file);
		return -1;
	}

	g_free(chars);

	if (fclose(fp) == EOF) {
		FILE_OP_ERROR(file, "fclose");
		unlink(file);
		return -1;
	}
	return 0;
}

static gint compose_save_to_outbox(Compose *compose, const gchar *file)
{
	FolderItem *outbox;
	gchar *path;
	gint num;
	FILE *fp;

	debug_print(_("saving sent message...\n"));

	outbox = folder_get_default_outbox();
	path = folder_item_get_path(outbox);
	if (!is_dir_exist(path))
		make_dir_hier(path);

	folder_item_scan(outbox);
	if ((num = folder_item_add_msg(outbox, file, FALSE)) < 0) {
		g_free(path);
		g_warning(_("can't save message\n"));
		return -1;
	}

	if ((fp = procmsg_open_mark_file(path, TRUE)) == NULL)
		g_warning(_("can't open mark file\n"));
	else {
		MsgInfo newmsginfo;

		newmsginfo.msgnum = num;
		newmsginfo.flags = 0;
		procmsg_write_flags(&newmsginfo, fp);
		fclose(fp);
	}
	g_free(path);

	return 0;
}

static gint compose_queue(Compose *compose, const gchar *file)
{
	FolderItem *queue;
	gchar *tmp, *queue_path;
	FILE *fp, *src_fp;
	GSList *cur;
	gchar buf[BUFFSIZE];
	gint num;

	debug_print(_("queueing message...\n"));
	g_return_val_if_fail(compose->to_list != NULL, -1);
	g_return_val_if_fail(compose->account != NULL, -1);

	tmp = g_strdup_printf("%s%cqueue.%d", g_get_tmp_dir(),
			      G_DIR_SEPARATOR, (gint)compose);
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
	if (compose->msgid)
		fprintf(fp, "MID:<%s>\n", compose->msgid);
	else
		fprintf(fp, "MID:\n");
	fprintf(fp, "CFG:\n");
	fprintf(fp, "PT:0\n");
	fprintf(fp, "S:%s\n", compose->account->address);
	fprintf(fp, "RQ:\n");
	if (compose->account->smtp_server)
		fprintf(fp, "SSV:%s\n", compose->account->smtp_server);
	else
		fprintf(fp, "SSV:\n");
	if (compose->account->nntp_server)
		fprintf(fp, "NSV:%s\n", compose->account->nntp_server);
	else
		fprintf(fp, "NSV:\n");
	fprintf(fp, "SSH:\n");
	fprintf(fp, "R:<%s>", (gchar *)compose->to_list->data);
	for (cur = compose->to_list->next; cur != NULL; cur = cur->next)
		fprintf(fp, ",<%s>", (gchar *)cur->data);
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
		newmsginfo.flags = 0;
		procmsg_write_flags(&newmsginfo, fp);
		fclose(fp);
	}
	g_free(queue_path);

	folder_item_scan(queue);
	folderview_update_item(queue, TRUE);

	return 0;
}

static void compose_write_attach(Compose *compose, FILE *fp)
{
	AttachInfo *ainfo;
	GtkCList *clist = GTK_CLIST(compose->attach_clist);
	gint row;
	FILE *attach_fp;
	gchar filename[BUFFSIZE];
	gint len;

	for (row = 0; (ainfo = gtk_clist_get_row_data(clist, row)) != NULL;
	     row++) {
		if ((attach_fp = fopen(ainfo->file, "r")) == NULL) {
			g_warning(_("Can't open file %s\n"), ainfo->file);
			continue;
		}

		fprintf(fp, "\n--%s\n", compose->boundary);

		if (!strcmp2(ainfo->content_type, "message/rfc822")) {
			fprintf(fp, "Content-Type: %s\n", ainfo->content_type);
			fprintf(fp, "Content-Disposition: inline\n");
		} else {
			conv_encode_header(filename, sizeof(filename),
					   ainfo->name, 12);
			fprintf(fp, "Content-Type: %s;\n"
				    " name=\"%s\"\n",
				ainfo->content_type, filename);
			fprintf(fp, "Content-Disposition: attachment;\n"
				    " filename=\"%s\"\n", filename);
		}

		fprintf(fp, "Content-Transfer-Encoding: %s\n\n",
			procmime_get_encoding_str(ainfo->encoding));

		if (ainfo->encoding == ENC_7BIT) {
			gchar buf[BUFFSIZE];

			while (fgets(buf, sizeof(buf), attach_fp) != NULL) {
				len = strlen(buf);
				if (len > 1 && buf[len - 1] == '\n' &&
				    buf[len - 2] == '\r') {
					buf[len - 2] = '\n';
					buf[len - 1] = '\0';
				}
				fputs(buf, fp);
			}
		} else {
			gchar inbuf[B64_LINE_SIZE], outbuf[B64_BUFFSIZE];

			while ((len = fread(inbuf, sizeof(gchar),
					    B64_LINE_SIZE, attach_fp))
			       == B64_LINE_SIZE) {
				to64frombits(outbuf, inbuf, B64_LINE_SIZE);
				fputs(outbuf, fp);
				fputc('\n', fp);
			}
			if (len > 0 && feof(attach_fp)) {
				to64frombits(outbuf, inbuf, len);
				fputs(outbuf, fp);
				fputc('\n', fp);
			}
		}

		fclose(attach_fp);
	}

	fprintf(fp, "\n--%s--\n", compose->boundary);
}

#define IS_IN_CUSTOM_HEADER(header) \
	(compose->account->add_customhdr && \
	 custom_header_find(compose->account->customhdr_list, header) != NULL)

static gint compose_write_headers(Compose *compose, FILE *fp,
				  const gchar *charset, EncodingType encoding,
				  gboolean is_draft)
{
	gchar buf[BUFFSIZE];
	gchar *str;
	/* struct utsname utsbuf; */

	g_return_val_if_fail(fp != NULL, -1);
	g_return_val_if_fail(charset != NULL, -1);
	g_return_val_if_fail(compose->account != NULL, -1);
	g_return_val_if_fail(compose->account->address != NULL, -1);

	/* Date */
	if (compose->account->add_date) {
		get_rfc822_date(buf, sizeof(buf));
		fprintf(fp, "Date: %s\n", buf);
	}
	
	/* From */
	if (!IS_IN_CUSTOM_HEADER("From")) {
		if (compose->account->name && *compose->account->name) {
			compose_convert_header
				(buf, sizeof(buf), compose->account->name,
				 strlen("From: "));
			fprintf(fp, "From: %s <%s>\n",
				buf, compose->account->address);
		} else
			fprintf(fp, "From: %s\n", compose->account->address);
	}
	
	slist_free_strings(compose->to_list);
	g_slist_free(compose->to_list);
	compose->to_list = NULL;

	/* To */
	if (compose->use_to) {
		str = gtk_entry_get_text(GTK_ENTRY(compose->to_entry));
		if (*str != '\0') {
			Xstrdup_a(str, str, return -1);
			g_strstrip(str);
			if (*str != '\0') {
				compose->to_list = address_list_append
					(compose->to_list, str);
				if (!IS_IN_CUSTOM_HEADER("To")) {
					compose_convert_header
						(buf, sizeof(buf), str,
						 strlen("To: "));
					fprintf(fp, "To: %s\n", buf);
				}
			}
		}
	}
	
	slist_free_strings(compose->newsgroup_list);
	g_slist_free(compose->newsgroup_list);
	compose->newsgroup_list = NULL;

	/* Newsgroups */
	str = gtk_entry_get_text(GTK_ENTRY(compose->newsgroups_entry));
	if (*str != '\0') {
		Xstrdup_a(str, str, return -1);
		g_strstrip(str);
		remove_space(str);
		if (*str != '\0') {
			compose->newsgroup_list =
				newsgroup_list_append(compose->newsgroup_list,
						      str);
			if (!IS_IN_CUSTOM_HEADER("Newsgroups")) {
				compose_convert_header(buf, sizeof(buf), str,
						       strlen("Newsgroups: "));
				fprintf(fp, "Newsgroups: %s\n", buf);
			}
		}
	}

	if (!is_draft && !compose->to_list && !compose->newsgroup_list)
		return -1;

	/* Cc */
	if (compose->use_cc) {
		str = gtk_entry_get_text(GTK_ENTRY(compose->cc_entry));
		if (*str != '\0') {
			Xstrdup_a(str, str, return -1);
			g_strstrip(str);
			if (*str != '\0') {
				compose->to_list = address_list_append
					(compose->to_list, str);
				if (!IS_IN_CUSTOM_HEADER("Cc")) {
					compose_convert_header
						(buf, sizeof(buf), str,
						 strlen("Cc: "));
					fprintf(fp, "Cc: %s\n", buf);
				}
			}
		}
	}
	
	/* Bcc */
	if (compose->use_bcc) {
		str = gtk_entry_get_text(GTK_ENTRY(compose->bcc_entry));
		if (*str != '\0') {
			Xstrdup_a(str, str, return -1);
			g_strstrip(str);
			if (*str != '\0') {
				compose->to_list = address_list_append
					(compose->to_list, str);
				if (is_draft) {
					compose_convert_header
						(buf, sizeof(buf), str,
						 strlen("Bcc: "));
					fprintf(fp, "Bcc: %s\n", buf);
				}
			}
		}
	}

	/* Subject */
	str = gtk_entry_get_text(GTK_ENTRY(compose->subject_entry));
	if (*str != '\0' && !IS_IN_CUSTOM_HEADER("Subject")) {
		Xstrdup_a(str, str, return -1);
		g_strstrip(str);
		if (*str != '\0') {
			compose_convert_header(buf, sizeof(buf), str,
					       strlen("Subject: "));
			fprintf(fp, "Subject: %s\n", buf);
		}
	}

	/* Message-ID */
	if (compose->account->gen_msgid) {
		compose_generate_msgid(compose, buf, sizeof(buf));
		fprintf(fp, "Message-Id: <%s>\n", buf);
		compose->msgid = g_strdup(buf);
	}

	/* In-Reply-To */
	if (compose->inreplyto && compose->to_list)
		fprintf(fp, "In-Reply-To: <%s>\n", compose->inreplyto);

	/* References */
	if (compose->references)
		fprintf(fp, "References: %s\n", compose->references);

	/* Followup-To */
	if (compose->use_followupto && !IS_IN_CUSTOM_HEADER("Followup-To")) {
		str = gtk_entry_get_text(GTK_ENTRY(compose->followup_entry));
		if (*str != '\0') {
			Xstrdup_a(str, str, return -1);
			g_strstrip(str);
			remove_space(str);
			if (*str != '\0') {
				compose_convert_header(buf, sizeof(buf), str,
						       strlen("Followup-To: "));
				fprintf(fp, "Followup-To: %s\n", buf);
			}
		}
	}

	/* Reply-To */
	if (compose->use_replyto && !IS_IN_CUSTOM_HEADER("Reply-To")) {
		str = gtk_entry_get_text(GTK_ENTRY(compose->reply_entry));
		if (*str != '\0') {
			Xstrdup_a(str, str, return -1);
			g_strstrip(str);
			if (*str != '\0') {
				compose_convert_header(buf, sizeof(buf), str,
						       strlen("Reply-To: "));
				fprintf(fp, "Reply-To: %s\n", buf);
			}
		}
	}

	/* Organization */
	if (compose->account->organization &&
	    !IS_IN_CUSTOM_HEADER("Organization")) {
		compose_convert_header(buf, sizeof(buf),
				       compose->account->organization,
				       strlen("Organization: "));
		fprintf(fp, "Organization: %s\n", buf);
	}

	/* Program version and system info */
	/* uname(&utsbuf); */
	str = gtk_entry_get_text(GTK_ENTRY(compose->to_entry));
	if (*str != '\0' && !IS_IN_CUSTOM_HEADER("X-Mailer")) {
		fprintf(fp, "X-Mailer: %s (GTK+ %d.%d.%d; %s)\n",
			prog_version,
			gtk_major_version, gtk_minor_version, gtk_micro_version,
			HOST_ALIAS);
			/* utsbuf.sysname, utsbuf.release, utsbuf.machine); */
	}
	str = gtk_entry_get_text(GTK_ENTRY(compose->newsgroups_entry));
	if (*str != '\0' && !IS_IN_CUSTOM_HEADER("X-Newsreader")) {
		fprintf(fp, "X-Newsreader: %s (GTK+ %d.%d.%d; %s)\n",
			prog_version,
			gtk_major_version, gtk_minor_version, gtk_micro_version,
			HOST_ALIAS);
			/* utsbuf.sysname, utsbuf.release, utsbuf.machine); */
	}

	/* custom headers */
	if (compose->account->add_customhdr) {
		GSList *cur;

		for (cur = compose->account->customhdr_list; cur != NULL;
		     cur = cur->next) {
			CustomHeader *chdr = (CustomHeader *)cur->data;

			if (strcasecmp(chdr->name, "Date")         != 0 &&
			    strcasecmp(chdr->name, "From")         != 0 &&
			    strcasecmp(chdr->name, "To")           != 0 &&
			    strcasecmp(chdr->name, "Sender")       != 0 &&
			    strcasecmp(chdr->name, "Message-Id")   != 0 &&
			    strcasecmp(chdr->name, "In-Reply-To")  != 0 &&
			    strcasecmp(chdr->name, "References")   != 0 &&
			    strcasecmp(chdr->name, "Mime-Version") != 0 &&
			    strcasecmp(chdr->name, "Content-Type") != 0 &&
			    strcasecmp(chdr->name, "Content-Transfer-Encoding")
			    != 0)
				compose_convert_header
					(buf, sizeof(buf),
					 chdr->value ? chdr->value : "",
					 strlen(chdr->name) + 2);
				fprintf(fp, "%s: %s\n", chdr->name, buf);
		}
	}
	
	/* MIME */
	fprintf(fp, "Mime-Version: 1.0\n");
	if (compose->use_attach) {
		get_rfc822_date(buf, sizeof(buf));
		subst_char(buf, ' ', '_');
		subst_char(buf, ',', '_');
		compose->boundary = g_strdup_printf("Multipart_%s_%08x",
						    buf, (guint)compose);
		fprintf(fp,
			"Content-Type: multipart/mixed;\n"
			" boundary=\"%s\"\n", compose->boundary);
	} else {
		fprintf(fp, "Content-Type: text/plain; charset=%s\n", charset);
		fprintf(fp, "Content-Transfer-Encoding: %s\n",
			procmime_get_encoding_str(encoding));
	}

	/* Request Return Receipt */
	if (!IS_IN_CUSTOM_HEADER("Disposition-Notification-To")) {
		if (compose->return_receipt) {
			if (compose->account->name
			    && *compose->account->name) {
				compose_convert_header(buf, sizeof(buf), compose->account->name, strlen("Disposition-Notification-To: "));
				fprintf(fp, "Disposition-Notification-To: %s <%s>\n", buf, compose->account->address);
			} else
				fprintf(fp, "Disposition-Notification-To: %s\n", compose->account->address);
		}
	}

	/* separator between header and body */
	fputs("\n", fp);

	return 0;
}

#undef IS_IN_CUSTOM_HEADER

static void compose_convert_header(gchar *dest, gint len, gchar *src,
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

static void compose_generate_msgid(Compose *compose, gchar *buf, gint len)
{
	struct tm *lt;
	time_t t;
	gchar *addr;

	t = time(NULL);
	lt = localtime(&t);

	if (compose->account && compose->account->address &&
	    *compose->account->address) {
		if (strchr(compose->account->address, '@'))
			addr = g_strdup(compose->account->address);
		else
			addr = g_strconcat(compose->account->address, "@",
					   get_domain_name(), NULL);
	} else
		addr = g_strconcat(g_get_user_name(), "@", get_domain_name(),
				   NULL);

	g_snprintf(buf, len, "%04d%02d%02d%02d%02d%02d.%08x.%s",
		   lt->tm_year + 1900, lt->tm_mon + 1,
		   lt->tm_mday, lt->tm_hour,
		   lt->tm_min, lt->tm_sec,
		   (guint)random(), addr);

	debug_print(_("generated Message-ID: %s\n"), buf);

	g_free(addr);
}

static void compose_add_entry_field(GtkWidget *table, GtkWidget **hbox,
				    GtkWidget **entry, gint *count,
				    const gchar *label_str,
				    gboolean is_addr_entry)
{
	GtkWidget *label;

	if (GTK_TABLE(table)->nrows < (*count) + 1)
		gtk_table_resize(GTK_TABLE(table), (*count) + 1, 2);

	*hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new
		(prefs_common.trans_hdr ? gettext(label_str) : label_str);
	gtk_box_pack_end(GTK_BOX(*hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), *hbox, 0, 1, *count, (*count) + 1,
			 GTK_FILL, 0, 2, 0);
	*entry = gtk_entry_new();
	gtk_table_attach_defaults
		(GTK_TABLE(table), *entry, 1, 2, *count, (*count) + 1);
	if (GTK_TABLE(table)->nrows > (*count) + 1)
		gtk_table_set_row_spacing(GTK_TABLE(table), *count, 4);

	if (is_addr_entry)
		address_completion_register_entry(GTK_ENTRY(*entry));

	(*count)++;
}

static Compose *compose_create(PrefsAccount *account)
{
	Compose   *compose;
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *menubar;
	GtkWidget *handlebox;

	GtkWidget *vbox2;

	GtkWidget *table_vbox;
	GtkWidget *label;
	GtkWidget *from_optmenu_hbox;
	GtkWidget *to_entry;
	GtkWidget *to_hbox;
	GtkWidget *newsgroups_entry;
	GtkWidget *newsgroups_hbox;
	GtkWidget *subject_entry;
	GtkWidget *cc_entry;
	GtkWidget *cc_hbox;
	GtkWidget *bcc_entry;
	GtkWidget *bcc_hbox;
	GtkWidget *reply_entry;
	GtkWidget *reply_hbox;
	GtkWidget *followup_entry;
	GtkWidget *followup_hbox;

	GtkWidget *paned;

	GtkWidget *attach_scrwin;
	GtkWidget *attach_clist;

	GtkWidget *edit_vbox;
	GtkWidget *ruler_hbox;
	GtkWidget *ruler;
	GtkWidget *scrolledwin;
	GtkWidget *text;

	GtkWidget *table;
	GtkWidget *hbox;

	gchar *titles[] = {_("MIME type"), _("Size"), _("Name")};
	guint n_menu_entries;
	GtkStyle  *style, *new_style;
	GdkColormap *cmap;
	GdkColor color[1];
	gboolean success[1];
	GdkFont   *font;
	GtkWidget *popupmenu;
	GtkWidget *menuitem;
	GtkItemFactory *popupfactory;
	GtkItemFactory *ifactory;
	gint n_entries;
	gint count = 0;
	gint i;

	g_return_val_if_fail(account != NULL, NULL);

	debug_print(_("Creating compose window...\n"));
	compose = g_new0(Compose, 1);

	compose->account = account;
	compose->orig_account = account;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_policy(GTK_WINDOW(window), TRUE, TRUE, FALSE);
	gtk_widget_set_usize(window, -1, prefs_common.compose_height);
	gtk_signal_connect(GTK_OBJECT(window), "delete_event",
			   GTK_SIGNAL_FUNC(compose_delete_cb), compose);
	gtk_signal_connect(GTK_OBJECT(window), "destroy",
			   GTK_SIGNAL_FUNC(compose_destroy_cb), compose);
	gtk_signal_connect(GTK_OBJECT(window), "focus_in_event",
			   GTK_SIGNAL_FUNC(manage_window_focus_in), NULL);
	gtk_signal_connect(GTK_OBJECT(window), "focus_out_event",
			   GTK_SIGNAL_FUNC(manage_window_focus_out), NULL);
	gtk_widget_realize(window);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	n_menu_entries = sizeof(compose_entries) / sizeof(compose_entries[0]);
	menubar = menubar_create(window, compose_entries,
				 n_menu_entries, "<Compose>", compose);
	gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, TRUE, 0);

	handlebox = gtk_handle_box_new();
	gtk_box_pack_start(GTK_BOX(vbox), handlebox, FALSE, FALSE, 0);

	compose_toolbar_create(compose, handlebox);

	vbox2 = gtk_vbox_new(FALSE, 2);
	gtk_box_pack_start(GTK_BOX(vbox), vbox2, TRUE, TRUE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(vbox2), BORDER_WIDTH);

	table_vbox = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox2), table_vbox, FALSE, TRUE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(table_vbox),
				       BORDER_WIDTH * 2);

	table = gtk_table_new(8, 2, FALSE);
	gtk_box_pack_start(GTK_BOX(table_vbox), table, FALSE, TRUE, 0);

	/* option menu for selecting accounts */
	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(prefs_common.trans_hdr ? _("From:") : "From:");
	gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 0, 1, count, count + 1,
			 GTK_FILL, 0, 2, 0);
	from_optmenu_hbox = compose_account_option_menu_create(compose);
	gtk_table_attach_defaults(GTK_TABLE(table), from_optmenu_hbox,
				  1, 2, count, count + 1);
	gtk_table_set_row_spacing(GTK_TABLE(table), 0, 4);
	count++;

	/* header labels and entries */
	compose_add_entry_field(table, &to_hbox, &to_entry, &count,
				"To:", TRUE); 
	compose_add_entry_field(table, &newsgroups_hbox, &newsgroups_entry,
				&count, "Newsgroups:", FALSE);
	compose_add_entry_field(table, &hbox, &subject_entry, &count,
				"Subject:", FALSE);
	compose_add_entry_field(table, &cc_hbox, &cc_entry, &count,
				"Cc:", TRUE);
	compose_add_entry_field(table, &bcc_hbox, &bcc_entry, &count,
				"Bcc:", TRUE);
	compose_add_entry_field(table, &reply_hbox, &reply_entry, &count,
				"Reply-To:", TRUE);
	compose_add_entry_field(table, &followup_hbox, &followup_entry, &count,
				"Followup-To:", FALSE);

	gtk_table_set_col_spacings(GTK_TABLE(table), 4);

	gtk_signal_connect(GTK_OBJECT(to_entry), "activate",
			   GTK_SIGNAL_FUNC(to_activated), compose);
	gtk_signal_connect(GTK_OBJECT(newsgroups_entry), "activate",
			   GTK_SIGNAL_FUNC(newsgroups_activated), compose);
	gtk_signal_connect(GTK_OBJECT(subject_entry), "activate",
			   GTK_SIGNAL_FUNC(subject_activated), compose);
	gtk_signal_connect(GTK_OBJECT(cc_entry), "activate",
			   GTK_SIGNAL_FUNC(cc_activated), compose);
	gtk_signal_connect(GTK_OBJECT(bcc_entry), "activate",
			   GTK_SIGNAL_FUNC(bcc_activated), compose);
	gtk_signal_connect(GTK_OBJECT(reply_entry), "activate",
			   GTK_SIGNAL_FUNC(replyto_activated), compose);
	gtk_signal_connect(GTK_OBJECT(followup_entry), "activate",
			   GTK_SIGNAL_FUNC(followupto_activated), compose);

	gtk_signal_connect(GTK_OBJECT(subject_entry), "grab_focus",
			   GTK_SIGNAL_FUNC(compose_grab_focus_cb), compose);
	gtk_signal_connect(GTK_OBJECT(to_entry), "grab_focus",
			   GTK_SIGNAL_FUNC(compose_grab_focus_cb), compose);
	gtk_signal_connect(GTK_OBJECT(newsgroups_entry), "grab_focus",
			   GTK_SIGNAL_FUNC(compose_grab_focus_cb), compose);
	gtk_signal_connect(GTK_OBJECT(cc_entry), "grab_focus",
			   GTK_SIGNAL_FUNC(compose_grab_focus_cb), compose);
	gtk_signal_connect(GTK_OBJECT(bcc_entry), "grab_focus",
			   GTK_SIGNAL_FUNC(compose_grab_focus_cb), compose);
	gtk_signal_connect(GTK_OBJECT(reply_entry), "grab_focus",
			   GTK_SIGNAL_FUNC(compose_grab_focus_cb), compose);
	gtk_signal_connect(GTK_OBJECT(followup_entry), "grab_focus",
			   GTK_SIGNAL_FUNC(compose_grab_focus_cb), compose);

	/* attachment list */
	attach_scrwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(attach_scrwin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_ALWAYS);
	gtk_widget_set_usize(attach_scrwin, -1, 80);

	attach_clist = gtk_clist_new_with_titles(N_ATTACH_COLS, titles);
	gtk_clist_set_column_justification(GTK_CLIST(attach_clist), COL_SIZE,
					   GTK_JUSTIFY_RIGHT);
	gtk_clist_set_column_width(GTK_CLIST(attach_clist), COL_MIMETYPE, 240);
	gtk_clist_set_column_width(GTK_CLIST(attach_clist), COL_SIZE, 64);
	gtk_clist_set_selection_mode(GTK_CLIST(attach_clist),
				     GTK_SELECTION_EXTENDED);
	for (i = 0; i < N_ATTACH_COLS; i++)
		GTK_WIDGET_UNSET_FLAGS
			(GTK_CLIST(attach_clist)->column[i].button,
			 GTK_CAN_FOCUS);
	gtk_container_add(GTK_CONTAINER(attach_scrwin), attach_clist);

	gtk_signal_connect(GTK_OBJECT(attach_clist), "select_row",
			   GTK_SIGNAL_FUNC(attach_selected), compose);
	gtk_signal_connect(GTK_OBJECT(attach_clist), "button_press_event",
			   GTK_SIGNAL_FUNC(attach_button_pressed), compose);
	gtk_signal_connect(GTK_OBJECT(attach_clist), "key_press_event",
			   GTK_SIGNAL_FUNC(attach_key_pressed), compose);

	/* drag and drop */
	gtk_drag_dest_set(attach_clist,
			  GTK_DEST_DEFAULT_ALL, compose_mime_types, 1,
			  GDK_ACTION_COPY);
	gtk_signal_connect(GTK_OBJECT(attach_clist), "drag_data_received",
			   GTK_SIGNAL_FUNC(compose_attach_drag_received_cb),
			   compose);

	/* pane between attach clist and text */
	paned = gtk_vpaned_new();
	gtk_paned_add1(GTK_PANED(paned), attach_scrwin);
	gtk_widget_ref(paned);
	gtk_widget_show_all(paned);

	edit_vbox = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox2), edit_vbox, TRUE, TRUE, 0);

	/* ruler */
	ruler_hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(edit_vbox), ruler_hbox, FALSE, FALSE, 0);

	ruler = gtk_shruler_new();
	gtk_ruler_set_range(GTK_RULER(ruler), 0.0, 100.0, 1.0, 100.0);
	gtk_box_pack_start(GTK_BOX(ruler_hbox), ruler, TRUE, TRUE,
			   BORDER_WIDTH + 1);
	gtk_widget_set_usize(ruler_hbox, 1, -1);

	/* text widget */
	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
				       GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	gtk_box_pack_start(GTK_BOX(edit_vbox), scrolledwin, TRUE, TRUE, 0);
	gtk_widget_set_usize(scrolledwin, prefs_common.compose_width, -1);

	text = gtk_stext_new(gtk_scrolled_window_get_hadjustment
			    (GTK_SCROLLED_WINDOW(scrolledwin)),
			    gtk_scrolled_window_get_vadjustment
			    (GTK_SCROLLED_WINDOW(scrolledwin)));
	gtk_stext_set_editable(GTK_STEXT(text), TRUE);
	gtk_stext_set_word_wrap(GTK_STEXT(text), TRUE);
	gtk_stext_set_wrap_rmargin(GTK_STEXT(text), prefs_common.linewrap_len);

	gtk_container_add(GTK_CONTAINER(scrolledwin), text);

	gtk_signal_connect(GTK_OBJECT(text), "changed",
			   GTK_SIGNAL_FUNC(compose_changed_cb), compose);
	gtk_signal_connect(GTK_OBJECT(text), "grab_focus",
			   GTK_SIGNAL_FUNC(compose_grab_focus_cb), compose);
	gtk_signal_connect_after(GTK_OBJECT(text), "button_press_event",
				 GTK_SIGNAL_FUNC(compose_button_press_cb),
				 compose);
#if 0
	gtk_signal_connect_after(GTK_OBJECT(text), "key_press_event",
				 GTK_SIGNAL_FUNC(compose_key_press_cb),
				 compose);
#endif
	gtk_signal_connect_after(GTK_OBJECT(text), "size_allocate",
				 GTK_SIGNAL_FUNC(compose_edit_size_alloc),
				 ruler);

	/* drag and drop */
	gtk_drag_dest_set(text, GTK_DEST_DEFAULT_ALL, compose_mime_types, 1,
			  GDK_ACTION_COPY);
	gtk_signal_connect(GTK_OBJECT(text), "drag_data_received",
			   GTK_SIGNAL_FUNC(compose_insert_drag_received_cb),
			   compose);

	gtk_widget_show_all(vbox);

	style = gtk_widget_get_style(text);

	/* workaround for the slow down of GtkSText when using Pixmap theme */
	if (style->engine) {
		GtkThemeEngine *engine;

		engine = style->engine;
		style->engine = NULL;
		new_style = gtk_style_copy(style);
		style->engine = engine;
	} else
		new_style = gtk_style_copy(style);

	if (prefs_common.textfont) {
		CharSet charset;

		charset = conv_get_current_charset();
		if (MB_CUR_MAX == 1) {
			gchar *fontstr, *p;

			Xstrdup_a(fontstr, prefs_common.textfont, );
			if (fontstr && (p = strchr(fontstr, ',')) != NULL)
				*p = '\0';
			font = gdk_font_load(fontstr);
		} else
			font = gdk_fontset_load(prefs_common.textfont);
		if (font) {
			gdk_font_unref(new_style->font);
			new_style->font = font;
		}
	}

	gtk_widget_set_style(text, new_style);

	color[0] = quote_color;
	cmap = gdk_window_get_colormap(window->window);
	gdk_colormap_alloc_colors(cmap, color, 1, FALSE, TRUE, success);
	if (success[0] == FALSE) {
		g_warning("Compose: color allocation failed.\n");
		style = gtk_widget_get_style(text);
		quote_color = style->black;
	}

	n_entries = sizeof(compose_popup_entries) /
		sizeof(compose_popup_entries[0]);
	popupmenu = menu_create_items(compose_popup_entries, n_entries,
				      "<Compose>", &popupfactory,
				      compose);

	ifactory = gtk_item_factory_from_widget(menubar);
	menu_set_sensitive(ifactory, "/Edit/Undo", FALSE);
	menu_set_sensitive(ifactory, "/Edit/Redo", FALSE);

	gtk_widget_hide(bcc_hbox);
	gtk_widget_hide(bcc_entry);
	gtk_widget_hide(reply_hbox);
	gtk_widget_hide(reply_entry);
	gtk_widget_hide(followup_hbox);
	gtk_widget_hide(followup_entry);
	gtk_widget_hide(ruler_hbox);
	gtk_table_set_row_spacing(GTK_TABLE(table), 4, 0);
	gtk_table_set_row_spacing(GTK_TABLE(table), 5, 0);
	gtk_table_set_row_spacing(GTK_TABLE(table), 6, 0);

	if (account->protocol == A_NNTP) {
		gtk_widget_hide(to_hbox);
		gtk_widget_hide(to_entry);
		gtk_widget_hide(cc_hbox);
		gtk_widget_hide(cc_entry);
		gtk_table_set_row_spacing(GTK_TABLE(table), 1, 0);
		gtk_table_set_row_spacing(GTK_TABLE(table), 3, 0);
	} else {
		gtk_widget_hide(newsgroups_hbox);
		gtk_widget_hide(newsgroups_entry);
		gtk_table_set_row_spacing(GTK_TABLE(table), 2, 0);
		menu_set_sensitive(ifactory, "/Message/Followup to", FALSE);
	}

	switch (prefs_common.toolbar_style) {
	case TOOLBAR_NONE:
		gtk_widget_hide(handlebox);
		break;
	case TOOLBAR_ICON:
		gtk_toolbar_set_style(GTK_TOOLBAR(compose->toolbar),
				      GTK_TOOLBAR_ICONS);
		break;
	case TOOLBAR_TEXT:
		gtk_toolbar_set_style(GTK_TOOLBAR(compose->toolbar),
				      GTK_TOOLBAR_TEXT);
		break;
	case TOOLBAR_BOTH:
		gtk_toolbar_set_style(GTK_TOOLBAR(compose->toolbar),
				      GTK_TOOLBAR_BOTH);
		break;
	}

	gtk_widget_show(window);

	address_completion_start(window);

	compose->window        = window;
	compose->vbox	       = vbox;
	compose->menubar       = menubar;
	compose->handlebox     = handlebox;

	compose->vbox2	       = vbox2;

	compose->table_vbox       = table_vbox;
	compose->table	          = table;
	compose->to_hbox          = to_hbox;
	compose->to_entry         = to_entry;
	compose->newsgroups_hbox  = newsgroups_hbox;
	compose->newsgroups_entry = newsgroups_entry;
	compose->subject_entry    = subject_entry;
	compose->cc_hbox          = cc_hbox;
	compose->cc_entry         = cc_entry;
	compose->bcc_hbox         = bcc_hbox;
	compose->bcc_entry        = bcc_entry;
	compose->reply_hbox       = reply_hbox;
	compose->reply_entry      = reply_entry;
	compose->followup_hbox    = followup_hbox;
	compose->followup_entry   = followup_entry;

	compose->paned = paned;

	compose->attach_scrwin = attach_scrwin;
	compose->attach_clist  = attach_clist;

	compose->edit_vbox     = edit_vbox;
	compose->ruler_hbox    = ruler_hbox;
	compose->ruler         = ruler;
	compose->scrolledwin   = scrolledwin;
	compose->text	       = text;

	compose->focused_editable = NULL;

	compose->popupmenu    = popupmenu;
	compose->popupfactory = popupfactory;

	compose->mode = COMPOSE_NEW;

	compose->replyto     = NULL;
	compose->cc	     = NULL;
	compose->bcc	     = NULL;
	compose->followup_to = NULL;
	compose->inreplyto   = NULL;
	compose->references  = NULL;
	compose->msgid       = NULL;
	compose->boundary    = NULL;

	compose->use_to         = FALSE;
	compose->use_cc         = FALSE;
	compose->use_bcc        = FALSE;
	compose->use_replyto    = FALSE;
	compose->use_followupto = FALSE;
	compose->use_attach     = FALSE;

#if USE_GPGME
	compose->use_signing    = FALSE;
	compose->use_encryption = FALSE;
#endif /* USE_GPGME */

	compose->modified = FALSE;

	compose->return_receipt = FALSE;

	compose->to_list        = NULL;
	compose->newsgroup_list = NULL;

	compose->exteditor_file    = NULL;
	compose->exteditor_pid     = -1;
	compose->exteditor_readdes = -1;
	compose->exteditor_tag     = -1;

	compose_set_title(compose);

	if (account->protocol != A_NNTP) {
		menuitem = gtk_item_factory_get_item(ifactory, "/Message/To");
		gtk_check_menu_item_set_active
			(GTK_CHECK_MENU_ITEM(menuitem), TRUE);
		gtk_widget_set_sensitive(menuitem, FALSE);
		menuitem = gtk_item_factory_get_item(ifactory, "/Message/Cc");
		gtk_check_menu_item_set_active
			(GTK_CHECK_MENU_ITEM(menuitem), TRUE);
		gtk_widget_set_sensitive(menuitem, FALSE);
	}
	if (account->set_autocc && account->auto_cc) {
		gtk_entry_set_text(GTK_ENTRY(cc_entry), account->auto_cc);
		menuitem = gtk_item_factory_get_item(ifactory, "/Message/Cc");
		gtk_check_menu_item_set_active
			(GTK_CHECK_MENU_ITEM(menuitem), TRUE);
	}
	if (account->set_autobcc) {
		menuitem = gtk_item_factory_get_item(ifactory, "/Message/Bcc");
		gtk_check_menu_item_set_active
			(GTK_CHECK_MENU_ITEM(menuitem), TRUE);
		if (account->auto_bcc)
			gtk_entry_set_text(GTK_ENTRY(bcc_entry),
					   account->auto_bcc);
	}
	if (account->set_autoreplyto) {
		menuitem = gtk_item_factory_get_item(ifactory,
						     "/Message/Reply to");
		gtk_check_menu_item_set_active
			(GTK_CHECK_MENU_ITEM(menuitem), TRUE);
		if (account->auto_replyto)
			gtk_entry_set_text(GTK_ENTRY(reply_entry),
					   account->auto_replyto);
	}

	menuitem = gtk_item_factory_get_item(ifactory, "/Tool/Show ruler");
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem),
				       prefs_common.show_ruler);

#if USE_GPGME
	menuitem = gtk_item_factory_get_item(ifactory, "/Message/Sign");
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem),
				       prefs_common.default_sign);
	menuitem = gtk_item_factory_get_item(ifactory, "/Message/Encrypt");
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem),
				       prefs_common.default_encrypt);
#endif /* USE_GPGME */

	addressbook_set_target_compose(compose);

	compose_list = g_list_append(compose_list, compose);

	return compose;
}

#include "pixmaps/stock_mail_send.xpm"
#include "pixmaps/stock_mail.xpm"
#include "pixmaps/stock_paste.xpm"
#include "pixmaps/stock_mail_attach.xpm"
#include "pixmaps/stock_mail_compose.xpm"
#include "pixmaps/linewrap.xpm"
//#include "pixmaps/tb_mail_queue_send.xpm"
#include "pixmaps/tb_address_book.xpm"

#define CREATE_TOOLBAR_ICON(xpm_d) \
{ \
	icon = gdk_pixmap_create_from_xpm_d(container->window, &mask, \
					    &container->style->white, \
					    xpm_d); \
	icon_wid = gtk_pixmap_new(icon, mask); \
}

static void compose_toolbar_create(Compose *compose, GtkWidget *container)
{
	GtkWidget *toolbar;
	GdkPixmap *icon;
	GdkBitmap *mask;
	GtkWidget *icon_wid;
	GtkWidget *send_btn;
	GtkWidget *sendl_btn;
	GtkWidget *draft_btn;
	GtkWidget *insert_btn;
	GtkWidget *attach_btn;
	GtkWidget *sig_btn;
	GtkWidget *exteditor_btn;
	GtkWidget *linewrap_btn;
	GtkWidget *addrbook_btn;

	toolbar = gtk_toolbar_new(GTK_ORIENTATION_HORIZONTAL,
				  GTK_TOOLBAR_BOTH);
	gtk_container_add(GTK_CONTAINER(container), toolbar);
	gtk_container_set_border_width(GTK_CONTAINER(container), 2);
	gtk_toolbar_set_button_relief(GTK_TOOLBAR(toolbar), GTK_RELIEF_NONE);
	gtk_toolbar_set_space_style(GTK_TOOLBAR(toolbar),
				    GTK_TOOLBAR_SPACE_LINE);

	CREATE_TOOLBAR_ICON(stock_mail_send_xpm);
	send_btn = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					   _("Send"),
					   _("Send message"),
					   "Send",
					   icon_wid, toolbar_send_cb, compose);

	CREATE_TOOLBAR_ICON(stock_mail_send_xpm);
	//CREATE_TOOLBAR_ICON(tb_mail_queue_send_xpm);
	sendl_btn = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					   _("Send later"),
					   _("Put into queue folder and send later"),
					   "Send later",
					   icon_wid, toolbar_send_later_cb,
					   compose);

	CREATE_TOOLBAR_ICON(stock_mail_xpm);
	draft_btn = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					    _("Draft"),
					    _("Save to draft folder"),
					    "Draft",
					    icon_wid, toolbar_draft_cb,
					    compose);

	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));

	CREATE_TOOLBAR_ICON(stock_paste_xpm);
	insert_btn = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					     _("Insert"),
					     _("Insert file"),
					     "Insert",
					     icon_wid, toolbar_insert_cb,
					     compose);

	CREATE_TOOLBAR_ICON(stock_mail_attach_xpm);
	attach_btn = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					     _("Attach"),
					     _("Attach file"),
					     "Attach",
					     icon_wid, toolbar_attach_cb,
					     compose);

	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));

	CREATE_TOOLBAR_ICON(stock_mail_xpm);
	sig_btn = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					  _("Signature"),
					  _("Insert signature"),
					  "Signature",
					  icon_wid, toolbar_sig_cb, compose);

	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));

	CREATE_TOOLBAR_ICON(stock_mail_compose_xpm);
	exteditor_btn = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
						_("Editor"),
						_("Edit with external editor"),
						"Editor",
						icon_wid,
						toolbar_ext_editor_cb,
						compose);

	CREATE_TOOLBAR_ICON(linewrap_xpm);
	linewrap_btn = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					       _("Linewrap"),
					       _("Wrap long lines"),
					       "Linewrap",
					       icon_wid,
					       toolbar_linewrap_cb,
					       compose);

	gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));

	CREATE_TOOLBAR_ICON(tb_address_book_xpm);
	addrbook_btn = gtk_toolbar_append_item(GTK_TOOLBAR(toolbar),
					       _("Address"),
					       _("Address book"),
					       "Address",
					       icon_wid, toolbar_address_cb,
					       compose);

	compose->toolbar       = toolbar;
	compose->send_btn      = send_btn;
	compose->sendl_btn     = sendl_btn;
	compose->draft_btn     = draft_btn;
	compose->insert_btn    = insert_btn;
	compose->attach_btn    = attach_btn;
	compose->sig_btn       = sig_btn;
	compose->exteditor_btn = exteditor_btn;
	compose->linewrap_btn  = linewrap_btn;
	compose->addrbook_btn  = addrbook_btn;

	gtk_widget_show_all(toolbar);
}

#undef CREATE_TOOLBAR_ICON

static GtkWidget *compose_account_option_menu_create(Compose *compose)
{
	GList *accounts;
	GtkWidget *hbox;
	GtkWidget *optmenu;
	GtkWidget *menu;
	gint num = 0, def_menu = 0;

	accounts = account_get_list();
	g_return_val_if_fail(accounts != NULL, NULL);

	hbox = gtk_hbox_new(FALSE, 0);
	optmenu = gtk_option_menu_new();
	gtk_box_pack_start(GTK_BOX(hbox), optmenu, FALSE, FALSE, 0);
	menu = gtk_menu_new();

	for (; accounts != NULL; accounts = accounts->next, num++) {
		PrefsAccount *ac = (PrefsAccount *)accounts->data;
		GtkWidget *menuitem;
		gchar *name;

		if (ac == compose->account) def_menu = num;

		name = g_strdup_printf("%s <%s> (%s)",
				       ac->name, ac->address, ac->account_name);
		MENUITEM_ADD(menu, menuitem, name, ac);
		g_free(name);
		gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
				   GTK_SIGNAL_FUNC(account_activated),
				   compose);
	}

	gtk_option_menu_set_menu(GTK_OPTION_MENU(optmenu), menu);
	gtk_option_menu_set_history(GTK_OPTION_MENU(optmenu), def_menu);

	return hbox;
}

static void compose_destroy(Compose *compose)
{
	gint row;
	GtkCList *clist = GTK_CLIST(compose->attach_clist);
	AttachInfo *ainfo;

	/* NOTE: address_completion_end() does nothing with the window
	 * however this may change. */
	address_completion_end(compose->window);

	slist_free_strings(compose->to_list);
	g_slist_free(compose->to_list);
	slist_free_strings(compose->newsgroup_list);
	g_slist_free(compose->newsgroup_list);

	procmsg_msginfo_free(compose->targetinfo);

	g_free(compose->replyto);
	g_free(compose->cc);
	g_free(compose->bcc);
	g_free(compose->newsgroups);
	g_free(compose->followup_to);

	g_free(compose->inreplyto);
	g_free(compose->references);
	g_free(compose->msgid);
	g_free(compose->boundary);

	g_free(compose->exteditor_file);

	for (row = 0; (ainfo = gtk_clist_get_row_data(clist, row)) != NULL;
	     row++)
		compose_attach_info_free(ainfo);

	if (addressbook_get_target_compose() == compose)
		addressbook_set_target_compose(NULL);

	prefs_common.compose_width = compose->scrolledwin->allocation.width;
	prefs_common.compose_height = compose->window->allocation.height;

	gtk_widget_destroy(compose->paned);

	g_free(compose);

	compose_list = g_list_remove(compose_list, compose);
}

static void compose_attach_info_free(AttachInfo *ainfo)
{
	g_free(ainfo->file);
	g_free(ainfo->content_type);
	g_free(ainfo->name);
	g_free(ainfo);
}

static void compose_attach_remove_selected(Compose *compose)
{
	GtkCList *clist = GTK_CLIST(compose->attach_clist);
	AttachInfo *ainfo;
	gint row;

	while (clist->selection != NULL) {
		row = GPOINTER_TO_INT(clist->selection->data);
		ainfo = gtk_clist_get_row_data(clist, row);
		compose_attach_info_free(ainfo);
		gtk_clist_remove(clist, row);
	}
}

static struct _AttachProperty
{
	GtkWidget *window;
	GtkWidget *mimetype_entry;
	GtkWidget *encoding_optmenu;
	GtkWidget *path_entry;
	GtkWidget *filename_entry;
	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;
} attach_prop;

static void compose_attach_property(Compose *compose)
{
	GtkCList *clist = GTK_CLIST(compose->attach_clist);
	AttachInfo *ainfo;
	gint row;
	gboolean cancelled;
	GtkOptionMenu *optmenu;

	if (!clist->selection) return;
	row = GPOINTER_TO_INT(clist->selection->data);

	ainfo = gtk_clist_get_row_data(clist, row);
	if (!ainfo) return;

	if (!attach_prop.window)
		compose_attach_property_create(&cancelled);
	gtk_widget_grab_focus(attach_prop.ok_btn);
	gtk_widget_show(attach_prop.window);
	manage_window_set_transient(GTK_WINDOW(attach_prop.window));

	optmenu = GTK_OPTION_MENU(attach_prop.encoding_optmenu);
	if (ainfo->encoding == ENC_UNKNOWN)
		gtk_option_menu_set_history(optmenu, ENC_BASE64);
	else
		gtk_option_menu_set_history(optmenu, ainfo->encoding);

	gtk_entry_set_text(GTK_ENTRY(attach_prop.mimetype_entry),
			   ainfo->content_type ? ainfo->content_type : "");
	gtk_entry_set_text(GTK_ENTRY(attach_prop.path_entry),
			   ainfo->file ? ainfo->file : "");
	gtk_entry_set_text(GTK_ENTRY(attach_prop.filename_entry),
			   ainfo->name ? ainfo->name : "");

	for (;;) {
		gchar *text;
		gchar *cnttype = NULL;
		gchar *file = NULL;
		off_t size = 0;
		GtkWidget *menu;
		GtkWidget *menuitem;

		gtk_main();

		if (cancelled == TRUE) {
			gtk_widget_hide(attach_prop.window);
			break;
		}

		text = gtk_entry_get_text(GTK_ENTRY(attach_prop.mimetype_entry));
		if (*text != '\0') {
			gchar *p;

			text = g_strstrip(g_strdup(text));
			if ((p = strchr(text, '/')) && !strchr(p + 1, '/')) {
				cnttype = g_strdup(text);
				g_free(text);
			} else {
				alertpanel_error(_("Invalid MIME type."));
				g_free(text);
				continue;
			}
		}

		menu = gtk_option_menu_get_menu(optmenu);
		menuitem = gtk_menu_get_active(GTK_MENU(menu));
		ainfo->encoding = GPOINTER_TO_INT
			(gtk_object_get_user_data(GTK_OBJECT(menuitem)));

		text = gtk_entry_get_text(GTK_ENTRY(attach_prop.path_entry));
		if (*text != '\0') {
			if (is_file_exist(text) &&
			    (size = get_file_size(text)) > 0)
				file = g_strdup(text);
			else {
				alertpanel_error
					(_("File doesn't exist or is empty."));
				g_free(cnttype);
				continue;
			}
		}

		text = gtk_entry_get_text(GTK_ENTRY(attach_prop.filename_entry));
		if (*text != '\0') {
			g_free(ainfo->name);
			ainfo->name = g_strdup(text);
		}

		if (cnttype) {
			g_free(ainfo->content_type);
			ainfo->content_type = cnttype;
		}
		if (file) {
			g_free(ainfo->file);
			ainfo->file = file;
		}
		if (size)
			ainfo->size = size;

		gtk_clist_set_text(clist, row, COL_MIMETYPE,
				   ainfo->content_type);
		gtk_clist_set_text(clist, row, COL_SIZE,
				   to_human_readable(ainfo->size));
		gtk_clist_set_text(clist, row, COL_NAME, ainfo->name);

		gtk_widget_hide(attach_prop.window);
		break;
	}
}

#define SET_LABEL_AND_ENTRY(str, entry, top) \
{ \
	label = gtk_label_new(str); \
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, top, (top + 1), \
			 GTK_FILL, 0, 0, 0); \
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5); \
 \
	entry = gtk_entry_new(); \
	gtk_table_attach(GTK_TABLE(table), entry, 1, 2, top, (top + 1), \
			 GTK_EXPAND|GTK_SHRINK|GTK_FILL, 0, 0, 0); \
}

static void compose_attach_property_create(gboolean *cancelled)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *table;
	GtkWidget *label;
	GtkWidget *mimetype_entry;
	GtkWidget *hbox;
	GtkWidget *optmenu;
	GtkWidget *optmenu_menu;
	GtkWidget *menuitem;
	GtkWidget *path_entry;
	GtkWidget *filename_entry;
	GtkWidget *hbbox;
	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;

	debug_print("Creating attach_property window...\n");

	window = gtk_window_new(GTK_WINDOW_DIALOG);
	gtk_widget_set_usize(window, 480, -1);
	gtk_container_set_border_width(GTK_CONTAINER(window), 8);
	gtk_window_set_title(GTK_WINDOW(window), _("Property"));
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(window), TRUE);
	gtk_signal_connect(GTK_OBJECT(window), "delete_event",
			   GTK_SIGNAL_FUNC(attach_property_delete_event),
			   cancelled);
	gtk_signal_connect(GTK_OBJECT(window), "key_press_event",
			   GTK_SIGNAL_FUNC(attach_property_key_pressed),
			   cancelled);

	vbox = gtk_vbox_new(FALSE, 8);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	table = gtk_table_new(4, 2, FALSE);
	gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);
	gtk_table_set_row_spacings(GTK_TABLE(table), 8);
	gtk_table_set_col_spacings(GTK_TABLE(table), 8);

	SET_LABEL_AND_ENTRY(_("MIME type"), mimetype_entry, 0);

	label = gtk_label_new(_("Encoding"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2,
			 GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 1, 2, 1, 2,
			 GTK_EXPAND|GTK_SHRINK|GTK_FILL, 0, 0, 0);

	optmenu = gtk_option_menu_new();
	gtk_box_pack_start(GTK_BOX(hbox), optmenu, FALSE, FALSE, 0);

	optmenu_menu = gtk_menu_new();
	MENUITEM_ADD(optmenu_menu, menuitem, "7bit", ENC_7BIT);
	gtk_widget_set_sensitive(menuitem, FALSE);
	MENUITEM_ADD(optmenu_menu, menuitem, "8bit", ENC_8BIT);
	gtk_widget_set_sensitive(menuitem, FALSE);
	MENUITEM_ADD(optmenu_menu, menuitem, "quoted-printable", ENC_QUOTED_PRINTABLE);
	gtk_widget_set_sensitive(menuitem, FALSE);
	MENUITEM_ADD(optmenu_menu, menuitem, "base64", ENC_BASE64);

	gtk_option_menu_set_menu(GTK_OPTION_MENU(optmenu), optmenu_menu);

	SET_LABEL_AND_ENTRY(_("Path"),      path_entry,     2);
	SET_LABEL_AND_ENTRY(_("File name"), filename_entry, 3);

	gtkut_button_set_create(&hbbox, &ok_btn, _("OK"),
				&cancel_btn, _("Cancel"), NULL, NULL);
	gtk_box_pack_end(GTK_BOX(vbox), hbbox, FALSE, FALSE, 0);
	gtk_widget_grab_default(ok_btn);

	gtk_signal_connect(GTK_OBJECT(ok_btn), "clicked",
			   GTK_SIGNAL_FUNC(attach_property_ok),
			   cancelled);
	gtk_signal_connect(GTK_OBJECT(cancel_btn), "clicked",
			   GTK_SIGNAL_FUNC(attach_property_cancel),
			   cancelled);

	gtk_widget_show_all(vbox);

	attach_prop.window           = window;
	attach_prop.mimetype_entry   = mimetype_entry;
	attach_prop.encoding_optmenu = optmenu;
	attach_prop.path_entry       = path_entry;
	attach_prop.filename_entry   = filename_entry;
	attach_prop.ok_btn           = ok_btn;
	attach_prop.cancel_btn       = cancel_btn;
}

#undef SET_LABEL_AND_ENTRY

static void attach_property_ok(GtkWidget *widget, gboolean *cancelled)
{
	*cancelled = FALSE;
	gtk_main_quit();
}

static void attach_property_cancel(GtkWidget *widget, gboolean *cancelled)
{
	*cancelled = TRUE;
	gtk_main_quit();
}

static gint attach_property_delete_event(GtkWidget *widget, GdkEventAny *event,
					 gboolean *cancelled)
{
	*cancelled = TRUE;
	gtk_main_quit();

	return TRUE;
}

static void attach_property_key_pressed(GtkWidget *widget, GdkEventKey *event,
					gboolean *cancelled)
{
	if (event && event->keyval == GDK_Escape) {
		*cancelled = TRUE;
		gtk_main_quit();
	}
}

static void compose_exec_ext_editor(Compose *compose)
{
	gchar tmp[64];
	pid_t pid;
	gint pipe_fds[2];

	g_snprintf(tmp, sizeof(tmp), "%s%ctmpmsg.%08x",
		   g_get_tmp_dir(), G_DIR_SEPARATOR, (gint)compose);

	if (pipe(pipe_fds) < 0) {
		perror("pipe");
		return;
	}

	if ((pid = fork()) < 0) {
		perror("fork");
		return;
	}

	if (pid != 0) {
		/* close the write side of the pipe */
		close(pipe_fds[1]);

		compose->exteditor_file    = g_strdup(tmp);
		compose->exteditor_pid     = pid;
		compose->exteditor_readdes = pipe_fds[0];

		compose_set_ext_editor_sensitive(compose, FALSE);

		compose->exteditor_tag =
			gdk_input_add(pipe_fds[0], GDK_INPUT_READ,
				      compose_input_cb, compose);
	} else {	/* process-monitoring process */
		pid_t pid_ed;

		if (setpgid(0, 0))
			perror("setpgid");

		/* close the read side of the pipe */
		close(pipe_fds[0]);

		if (compose_write_body_to_file(compose, tmp) < 0) {
			fd_write(pipe_fds[1], "2\n", 2);
			_exit(1);
		}

		pid_ed = compose_exec_ext_editor_real(tmp);
		if (pid_ed < 0) {
			fd_write(pipe_fds[1], "1\n", 2);
			_exit(1);
		}

		/* wait until editor is terminated */
		waitpid(pid_ed, NULL, 0);

		fd_write(pipe_fds[1], "0\n", 2);

		close(pipe_fds[1]);
		_exit(0);
	}
}

static gint compose_exec_ext_editor_real(const gchar *file)
{
	static gchar *def_cmd = "emacs %s";
	gchar buf[1024];
	gchar *p;
	gchar **cmdline;
	pid_t pid;

	g_return_val_if_fail(file != NULL, -1);

	if ((pid = fork()) < 0) {
		perror("fork");
		return -1;
	}

	if (pid != 0) return pid;

	/* grandchild process */

	if (setpgid(0, getppid()))
		perror("setpgid");

	if (prefs_common.ext_editor_cmd &&
	    (p = strchr(prefs_common.ext_editor_cmd, '%')) &&
	    *(p + 1) == 's' && !strchr(p + 2, '%')) {
		g_snprintf(buf, sizeof(buf), prefs_common.ext_editor_cmd, file);
	} else {
		if (prefs_common.ext_editor_cmd)
			g_warning(_("External editor command line is invalid: `%s'\n"),
				  prefs_common.ext_editor_cmd);
		g_snprintf(buf, sizeof(buf), def_cmd, file);
	}

	cmdline = g_strsplit(buf, " ", 1024);
	execvp(cmdline[0], cmdline);

	perror("execvp");
	g_strfreev(cmdline);

	_exit(1);
}

static gboolean compose_ext_editor_kill(Compose *compose)
{
	pid_t pgid = compose->exteditor_pid * -1;
	gint ret;

	ret = kill(pgid, 0);

	if (ret == 0 || (ret == -1 && EPERM == errno)) {
		AlertValue val;
		gchar *msg;

		msg = g_strdup_printf
			(_("The external editor is still working.\n"
			   "Force terminating the process?\n"
			   "process group id: %d"), -pgid);
		val = alertpanel(_("Notice"), msg, _("Yes"), _("+No"), NULL);
		g_free(msg);

		if (val == G_ALERTDEFAULT) {
			gdk_input_remove(compose->exteditor_tag);
			close(compose->exteditor_readdes);

			if (kill(pgid, SIGTERM) < 0) perror("kill");
			waitpid(compose->exteditor_pid, NULL, 0);

			g_warning(_("Terminated process group id: %d"), -pgid);
			g_warning(_("Temporary file: %s"),
				  compose->exteditor_file);

			compose_set_ext_editor_sensitive(compose, TRUE);

			g_free(compose->exteditor_file);
			compose->exteditor_file    = NULL;
			compose->exteditor_pid     = -1;
			compose->exteditor_readdes = -1;
			compose->exteditor_tag     = -1;
		} else
			return FALSE;
	}

	return TRUE;
}

static void compose_input_cb(gpointer data, gint source,
			     GdkInputCondition condition)
{
	gchar buf[3];
	Compose *compose = (Compose *)data;
	gint i = 0;

	debug_print(_("Compose: input from monitoring process\n"));

	gdk_input_remove(compose->exteditor_tag);

	for (;;) {
		if (read(source, &buf[i], 1) < 1) {
			buf[0] = '3';
			break;
		}
		if (buf[i] == '\n') {
			buf[i] = '\0';
			break;
		}
		i++;
		if (i == sizeof(buf) - 1)
			break;
	}

	waitpid(compose->exteditor_pid, NULL, 0);

	if (buf[0] == '0') {		/* success */
		GtkSText *text = GTK_STEXT(compose->text);

		gtk_stext_freeze(text);
		gtk_stext_set_point(text, 0);
		gtk_stext_forward_delete(text, gtk_stext_get_length(text));
		compose_insert_file(compose, compose->exteditor_file);
		compose_changed_cb(NULL, compose);
		gtk_stext_thaw(text);

		if (unlink(compose->exteditor_file) < 0)
			FILE_OP_ERROR(compose->exteditor_file, "unlink");
	} else if (buf[0] == '1') {	/* failed */
		g_warning(_("Couldn't exec external editor\n"));
		if (unlink(compose->exteditor_file) < 0)
			FILE_OP_ERROR(compose->exteditor_file, "unlink");
	} else if (buf[0] == '2') {
		g_warning(_("Couldn't write to file\n"));
	} else if (buf[0] == '3') {
		g_warning(_("Pipe read failed\n"));
	}

	close(source);

	compose_set_ext_editor_sensitive(compose, TRUE);

	g_free(compose->exteditor_file);
	compose->exteditor_file    = NULL;
	compose->exteditor_pid     = -1;
	compose->exteditor_readdes = -1;
	compose->exteditor_tag     = -1;
}

static void compose_set_ext_editor_sensitive(Compose *compose,
					     gboolean sensitive)
{
	GtkItemFactory *ifactory;

	ifactory = gtk_item_factory_from_widget(compose->menubar);

	menu_set_sensitive(ifactory, "/Message/Send", sensitive);
	menu_set_sensitive(ifactory, "/Message/Send later", sensitive);
	menu_set_sensitive(ifactory, "/Message/Save to draft folder",
			   sensitive);
	menu_set_sensitive(ifactory, "/File/Insert file", sensitive);
	menu_set_sensitive(ifactory, "/File/Insert signature", sensitive);
	menu_set_sensitive(ifactory, "/Edit/Wrap long lines", sensitive);
	menu_set_sensitive(ifactory, "/Edit/Edit with external editor",
			   sensitive);

	gtk_widget_set_sensitive(compose->text,          sensitive);
	gtk_widget_set_sensitive(compose->send_btn,      sensitive);
	gtk_widget_set_sensitive(compose->sendl_btn,     sensitive);
	gtk_widget_set_sensitive(compose->draft_btn,     sensitive);
	gtk_widget_set_sensitive(compose->insert_btn,    sensitive);
	gtk_widget_set_sensitive(compose->sig_btn,       sensitive);
	gtk_widget_set_sensitive(compose->exteditor_btn, sensitive);
	gtk_widget_set_sensitive(compose->linewrap_btn,  sensitive);
}

static gint calc_cursor_xpos(GtkSText *text, gint extra, gint char_width)
{
	gint cursor_pos;

	cursor_pos = (text->cursor_pos_x - extra) / char_width;
	cursor_pos = MAX(cursor_pos, 0);

	return cursor_pos;
}

/* callback functions */

/* compose_edit_size_alloc() - called when resized. don't know whether Gtk
 * includes "non-client" (windows-izm) in calculation, so this calculation
 * may not be accurate.
 */
static gboolean compose_edit_size_alloc(GtkEditable *widget,
					GtkAllocation *allocation,
					GtkSHRuler *shruler)
{
	if (prefs_common.show_ruler) {
		gint char_width;
		gint line_width_in_chars;

		char_width = gtkut_get_font_width
			(GTK_WIDGET(widget)->style->font);
		line_width_in_chars =
			(allocation->width - allocation->x) / char_width;

		/* got the maximum */
		gtk_ruler_set_range(GTK_RULER(shruler),
				    0.0, line_width_in_chars,
				    calc_cursor_xpos(GTK_STEXT(widget),
						     allocation->x,
						     char_width),
				    /*line_width_in_chars*/ char_width);
	}

	return TRUE;
}

static void toolbar_send_cb(GtkWidget *widget, gpointer data)
{
	compose_send_cb(data, 0, NULL);
}

static void toolbar_send_later_cb(GtkWidget *widget, gpointer data)
{
	compose_send_later_cb(data, 0, NULL);
}

static void toolbar_draft_cb(GtkWidget *widget, gpointer data)
{
	compose_draft_cb(data, 0, NULL);
}

static void toolbar_insert_cb(GtkWidget *widget, gpointer data)
{
	compose_insert_file_cb(data, 0, NULL);
}

static void toolbar_attach_cb(GtkWidget *widget, gpointer data)
{
	compose_attach_cb(data, 0, NULL);
}

static void toolbar_sig_cb(GtkWidget *widget, gpointer data)
{
	Compose *compose = (Compose *)data;

	compose_insert_sig(compose);
}

static void toolbar_ext_editor_cb(GtkWidget *widget, gpointer data)
{
	Compose *compose = (Compose *)data;

	compose_exec_ext_editor(compose);
}

static void toolbar_linewrap_cb(GtkWidget *widget, gpointer data)
{
	Compose *compose = (Compose *)data;

	compose_wrap_line(compose);
}

static void toolbar_address_cb(GtkWidget *widget, gpointer data)
{
	compose_address_cb(data, 0, NULL);
}

static void account_activated(GtkMenuItem *menuitem, gpointer data)
{
	Compose *compose = (Compose *)data;

	PrefsAccount *ac;

	ac = (PrefsAccount *)gtk_object_get_user_data(GTK_OBJECT(menuitem));
	g_return_if_fail(ac != NULL);

	if (ac != compose->account) {
		compose->account = ac;
		compose_set_title(compose);
	}
}

static void attach_selected(GtkCList *clist, gint row, gint column,
			    GdkEvent *event, gpointer data)
{
	Compose *compose = (Compose *)data;

	if (event && event->type == GDK_2BUTTON_PRESS)
		compose_attach_property(compose);
}

static void attach_button_pressed(GtkWidget *widget, GdkEventButton *event,
				  gpointer data)
{
	Compose *compose = (Compose *)data;
	GtkCList *clist = GTK_CLIST(compose->attach_clist);
	gint row, column;

	if (!event) return;

	if (event->button == 3) {
		if ((clist->selection && !clist->selection->next) ||
		    !clist->selection) {
			gtk_clist_unselect_all(clist);
			if (gtk_clist_get_selection_info(clist,
							 event->x, event->y,
							 &row, &column)) {
				gtk_clist_select_row(clist, row, column);
				gtkut_clist_set_focus_row(clist, row);
			}
		}
		gtk_menu_popup(GTK_MENU(compose->popupmenu), NULL, NULL,
			       NULL, NULL, event->button, event->time);
	}
}

static void attach_key_pressed(GtkWidget *widget, GdkEventKey *event,
			       gpointer data)
{
	Compose *compose = (Compose *)data;

	if (!event) return;

	switch (event->keyval) {
	case GDK_Delete:
		compose_attach_remove_selected(compose);
		break;
	}
}

static void compose_send_cb(gpointer data, guint action, GtkWidget *widget)
{
	Compose *compose = (Compose *)data;
	gint val;

	val = compose_send(compose);

	if (val == 0) gtk_widget_destroy(compose->window);
}

static void compose_send_later_cb(gpointer data, guint action,
				  GtkWidget *widget)
{
	Compose *compose = (Compose *)data;
	gchar tmp[22];
	gchar *to, *newsgroups;

	to = gtk_entry_get_text(GTK_ENTRY(compose->to_entry));
	newsgroups = gtk_entry_get_text(GTK_ENTRY(compose->newsgroups_entry));
	if (*to == '\0' && *newsgroups == '\0') {
		alertpanel_error(_("Recipient is not specified."));
		return;
	}

	g_snprintf(tmp, 22, "%s%ctmpmsg%d",
		   g_get_tmp_dir(), G_DIR_SEPARATOR, (gint)compose);

	if (prefs_common.linewrap_at_send)
		compose_wrap_line(compose);

	if (compose_write_to_file(compose, tmp, FALSE) < 0 ||
	    compose_queue(compose, tmp) < 0) {
		alertpanel_error(_("Can't queue the message."));
		return;
	}

	if (prefs_common.savemsg) {
		if (compose_save_to_outbox(compose, tmp) < 0)
			alertpanel_error
				(_("Can't save the message to outbox."));
	}

	if (unlink(tmp) < 0)
		FILE_OP_ERROR(tmp, "unlink");

	gtk_widget_destroy(compose->window);
}

static void compose_draft_cb(gpointer data, guint action, GtkWidget *widget)
{
	Compose *compose = (Compose *)data;
	FolderItem *draft;
	gchar *tmp;

	draft = folder_get_default_draft();
	folder_item_scan(draft);

	if (procmsg_msg_exist(compose->targetinfo) &&
	    compose->targetinfo->folder == draft) {
		if (folder_item_remove_msg(draft,
					   compose->targetinfo->msgnum) < 0)
			g_warning(_("can't remove the old draft message\n"));
	}
	tmp = g_strdup_printf("%s%cdraft.%d", g_get_tmp_dir(),
			      G_DIR_SEPARATOR, (gint)compose);

	if (compose_write_to_file(compose, tmp, TRUE) < 0) {
		g_free(tmp);
		return;
	}

	if (folder_item_add_msg(draft, tmp, TRUE) < 0) {
		unlink(tmp);
		g_free(tmp);
		return;
	}

	g_free(tmp);

	//folderview_scan_folder_a(DRAFT_DIR, TRUE);

	gtk_widget_destroy(compose->window);
}

static void compose_attach_cb(gpointer data, guint action, GtkWidget *widget)
{
	Compose *compose = (Compose *)data;
	gchar *file;

	file = filesel_select_file(_("Select file"), NULL);

	if (file)
		compose_attach_append(compose, file, MIME_UNKNOWN);
}

static void compose_insert_file_cb(gpointer data, guint action,
				   GtkWidget *widget)
{
	Compose *compose = (Compose *)data;
	gchar *file;

	file = filesel_select_file(_("Select file"), NULL);

	if (file)
		compose_insert_file(compose, file);
}

static gint compose_delete_cb(GtkWidget *widget, GdkEventAny *event,
			      gpointer data)
{
	compose_close_cb(data, 0, NULL);
	return TRUE;
}

static void compose_close_cb(gpointer data, guint action, GtkWidget *widget)
{
	Compose *compose = (Compose *)data;
	AlertValue val;

	if (compose->exteditor_tag != -1) {
		if (!compose_ext_editor_kill(compose))
			return;
	}

	if (compose->modified) {
		val = alertpanel(_("Discard message"),
				 _("This message has been modified. discard it?"),
				 _("Discard"), _("to Draft"), _("Cancel"));

		switch (val) {
		case G_ALERTDEFAULT:
			break;
		case G_ALERTALTERNATE:
			compose_draft_cb(data, 0, NULL);
			return;
		default:
			return;
		}
	}

	gtk_widget_destroy(compose->window);
}

static void compose_address_cb(gpointer data, guint action, GtkWidget *widget)
{
	Compose *compose = (Compose *)data;

	addressbook_open(compose);
}

static void compose_ext_editor_cb(gpointer data, guint action,
				  GtkWidget *widget)
{
	Compose *compose = (Compose *)data;

	compose_exec_ext_editor(compose);
}

static void compose_destroy_cb(GtkWidget *widget, Compose *compose)
{
	compose_destroy(compose);
}

static void compose_cut_cb(Compose *compose)
{
	if (compose->focused_editable &&
	    GTK_WIDGET_HAS_FOCUS(compose->focused_editable))
		gtk_editable_cut_clipboard
			(GTK_EDITABLE(compose->focused_editable));
}

static void compose_copy_cb(Compose *compose)
{
	if (compose->focused_editable &&
	    GTK_WIDGET_HAS_FOCUS(compose->focused_editable))
		gtk_editable_copy_clipboard
			(GTK_EDITABLE(compose->focused_editable));
}

static void compose_paste_cb(Compose *compose)
{
	if (compose->focused_editable &&
	    GTK_WIDGET_HAS_FOCUS(compose->focused_editable))
		gtk_editable_paste_clipboard
			(GTK_EDITABLE(compose->focused_editable));
}

static void compose_allsel_cb(Compose *compose)
{
	if (compose->focused_editable &&
	    GTK_WIDGET_HAS_FOCUS(compose->focused_editable))
		gtk_editable_select_region
			(GTK_EDITABLE(compose->focused_editable), 0, -1);
}

static void compose_grab_focus_cb(GtkWidget *widget, Compose *compose)
{
	if (GTK_IS_EDITABLE(widget))
		compose->focused_editable = widget;
}

static void compose_changed_cb(GtkEditable *editable, Compose *compose)
{
	if (compose->modified == FALSE) {
		compose->modified = TRUE;
		compose_set_title(compose);
	}
}

static void compose_button_press_cb(GtkWidget *widget, GdkEventButton *event,
				    Compose *compose)
{
	gtk_stext_set_point(GTK_STEXT(widget),
			   gtk_editable_get_position(GTK_EDITABLE(widget)));
}

#if 0
static void compose_key_press_cb(GtkWidget *widget, GdkEventKey *event,
				 Compose *compose)
{
	gtk_stext_set_point(GTK_STEXT(widget),
			   gtk_editable_get_position(GTK_EDITABLE(widget)));
}
#endif

static void compose_toggle_to_cb(gpointer data, guint action,
				 GtkWidget *widget)
{
	Compose *compose = (Compose *)data;

	if (GTK_CHECK_MENU_ITEM(widget)->active) {
		gtk_widget_show(compose->to_hbox);
		gtk_widget_show(compose->to_entry);
		gtk_table_set_row_spacing(GTK_TABLE(compose->table), 1, 4);
		compose->use_to = TRUE;
	} else {
		gtk_widget_hide(compose->to_hbox);
		gtk_widget_hide(compose->to_entry);
		gtk_table_set_row_spacing(GTK_TABLE(compose->table), 1, 0);
		gtk_widget_queue_resize(compose->table_vbox);
		compose->use_to = FALSE;
	}

	if (addressbook_get_target_compose() == compose)
		addressbook_set_target_compose(compose);
}

static void compose_toggle_cc_cb(gpointer data, guint action,
				 GtkWidget *widget)
{
	Compose *compose = (Compose *)data;

	if (GTK_CHECK_MENU_ITEM(widget)->active) {
		gtk_widget_show(compose->cc_hbox);
		gtk_widget_show(compose->cc_entry);
		gtk_table_set_row_spacing(GTK_TABLE(compose->table), 3, 4);
		compose->use_cc = TRUE;
	} else {
		gtk_widget_hide(compose->cc_hbox);
		gtk_widget_hide(compose->cc_entry);
		gtk_table_set_row_spacing(GTK_TABLE(compose->table), 3, 0);
		gtk_widget_queue_resize(compose->table_vbox);
		compose->use_cc = FALSE;
	}

	if (addressbook_get_target_compose() == compose)
		addressbook_set_target_compose(compose);
}

static void compose_toggle_bcc_cb(gpointer data, guint action,
				  GtkWidget *widget)
{
	Compose *compose = (Compose *)data;

	if (GTK_CHECK_MENU_ITEM(widget)->active) {
		gtk_widget_show(compose->bcc_hbox);
		gtk_widget_show(compose->bcc_entry);
		gtk_table_set_row_spacing(GTK_TABLE(compose->table), 4, 4);
		compose->use_bcc = TRUE;
	} else {
		gtk_widget_hide(compose->bcc_hbox);
		gtk_widget_hide(compose->bcc_entry);
		gtk_table_set_row_spacing(GTK_TABLE(compose->table), 4, 0);
		gtk_widget_queue_resize(compose->table_vbox);
		compose->use_bcc = FALSE;
	}

	if (addressbook_get_target_compose() == compose)
		addressbook_set_target_compose(compose);
}

static void compose_toggle_replyto_cb(gpointer data, guint action,
				      GtkWidget *widget)
{
	Compose *compose = (Compose *)data;

	if (GTK_CHECK_MENU_ITEM(widget)->active) {
		gtk_widget_show(compose->reply_hbox);
		gtk_widget_show(compose->reply_entry);
		gtk_table_set_row_spacing(GTK_TABLE(compose->table), 5, 4);
		compose->use_replyto = TRUE;
	} else {
		gtk_widget_hide(compose->reply_hbox);
		gtk_widget_hide(compose->reply_entry);
		gtk_table_set_row_spacing(GTK_TABLE(compose->table), 5, 0);
		gtk_widget_queue_resize(compose->table_vbox);
		compose->use_replyto = FALSE;
	}
}

static void compose_toggle_followupto_cb(gpointer data, guint action,
					 GtkWidget *widget)
{
	Compose *compose = (Compose *)data;

	if (GTK_CHECK_MENU_ITEM(widget)->active) {
		gtk_widget_show(compose->followup_hbox);
		gtk_widget_show(compose->followup_entry);
		gtk_table_set_row_spacing(GTK_TABLE(compose->table), 6, 4);
		compose->use_followupto = TRUE;
	} else {
		gtk_widget_hide(compose->followup_hbox);
		gtk_widget_hide(compose->followup_entry);
		gtk_table_set_row_spacing(GTK_TABLE(compose->table), 6, 0);
		gtk_widget_queue_resize(compose->table_vbox);
		compose->use_followupto = FALSE;
	}
}

static void compose_toggle_attach_cb(gpointer data, guint action,
				     GtkWidget *widget)
{
	Compose *compose = (Compose *)data;

	if (GTK_CHECK_MENU_ITEM(widget)->active) {
		gtk_widget_ref(compose->edit_vbox);

		gtk_container_remove(GTK_CONTAINER(compose->vbox2),
				     compose->edit_vbox);
		gtk_paned_add2(GTK_PANED(compose->paned), compose->edit_vbox);
		gtk_box_pack_start(GTK_BOX(compose->vbox2), compose->paned,
				   TRUE, TRUE, 0);
		gtk_widget_show(compose->paned);

		gtk_widget_unref(compose->edit_vbox);
		gtk_widget_unref(compose->paned);

		compose->use_attach = TRUE;
	} else {
		gtk_widget_ref(compose->paned);
		gtk_widget_ref(compose->edit_vbox);

		gtk_container_remove(GTK_CONTAINER(compose->vbox2),
				     compose->paned);
		gtk_container_remove(GTK_CONTAINER(compose->paned),
				     compose->edit_vbox);
		gtk_box_pack_start(GTK_BOX(compose->vbox2),
				   compose->edit_vbox, TRUE, TRUE, 0);

		gtk_widget_unref(compose->edit_vbox);

		compose->use_attach = FALSE;
	}
}

#if USE_GPGME
static void compose_toggle_sign_cb(gpointer data, guint action,
				   GtkWidget *widget)
{
	Compose *compose = (Compose *)data;

	if (GTK_CHECK_MENU_ITEM(widget)->active)
		compose->use_signing = TRUE;
	else
		compose->use_signing = FALSE;
}

static void compose_toggle_encrypt_cb(gpointer data, guint action,
				      GtkWidget *widget)
{
	Compose *compose = (Compose *)data;

	if (GTK_CHECK_MENU_ITEM(widget)->active)
		compose->use_encryption = TRUE;
	else
		compose->use_encryption = FALSE;
}
#endif /* USE_GPGME */

static void compose_toggle_ruler_cb(gpointer data, guint action,
				    GtkWidget *widget)
{
	Compose *compose = (Compose *)data;

	if (GTK_CHECK_MENU_ITEM(widget)->active) {
		gtk_widget_show(compose->ruler_hbox);
		prefs_common.show_ruler = TRUE;
	} else {
		gtk_widget_hide(compose->ruler_hbox);
		gtk_widget_queue_resize(compose->edit_vbox);
		prefs_common.show_ruler = FALSE;
	}
}

static void compose_attach_drag_received_cb (GtkWidget		*widget,
					     GdkDragContext	*drag_context,
					     gint		 x,
					     gint		 y,
					     GtkSelectionData	*data,
					     guint		 info,
					     guint		 time,
					     gpointer		 user_data)
{
	Compose *compose = (Compose *)user_data;
	GList *list, *tmp;

	list = uri_list_extract_filenames((const gchar *)data->data);
	for (tmp = list; tmp != NULL; tmp = tmp->next)
		compose_attach_append(compose, (const gchar *)tmp->data,
				      MIME_UNKNOWN);
	list_free_strings(list);
	g_list_free(list);
}

static void compose_insert_drag_received_cb (GtkWidget		*widget,
					     GdkDragContext	*drag_context,
					     gint		 x,
					     gint		 y,
					     GtkSelectionData	*data,
					     guint		 info,
					     guint		 time,
					     gpointer		 user_data)
{
	Compose *compose = (Compose *)user_data;
	GList *list, *tmp;

	list = uri_list_extract_filenames((const gchar *)data->data);
	for (tmp = list; tmp != NULL; tmp = tmp->next)
		compose_insert_file(compose, (const gchar *)tmp->data);
	list_free_strings(list);
	g_list_free(list);
}

static void to_activated(GtkWidget *widget, Compose *compose)
{
	if (GTK_WIDGET_VISIBLE(compose->newsgroups_entry))
		gtk_widget_grab_focus(compose->newsgroups_entry);
	else
		gtk_widget_grab_focus(compose->subject_entry);
}

static void newsgroups_activated(GtkWidget *widget, Compose *compose)
{
	gtk_widget_grab_focus(compose->subject_entry);
}

static void subject_activated(GtkWidget *widget, Compose *compose)
{
	if (GTK_WIDGET_VISIBLE(compose->cc_entry))
		gtk_widget_grab_focus(compose->cc_entry);
	else if (GTK_WIDGET_VISIBLE(compose->bcc_entry))
		gtk_widget_grab_focus(compose->bcc_entry);
	else if (GTK_WIDGET_VISIBLE(compose->reply_entry))
		gtk_widget_grab_focus(compose->reply_entry);
	else if (GTK_WIDGET_VISIBLE(compose->followup_entry))
		gtk_widget_grab_focus(compose->followup_entry);
	else
		gtk_widget_grab_focus(compose->text);
}

static void cc_activated(GtkWidget *widget, Compose *compose)
{
	if (GTK_WIDGET_VISIBLE(compose->bcc_entry))
		gtk_widget_grab_focus(compose->bcc_entry);
	else if (GTK_WIDGET_VISIBLE(compose->reply_entry))
		gtk_widget_grab_focus(compose->reply_entry);
	else if (GTK_WIDGET_VISIBLE(compose->followup_entry))
		gtk_widget_grab_focus(compose->followup_entry);
	else
		gtk_widget_grab_focus(compose->text);
}

static void bcc_activated(GtkWidget *widget, Compose *compose)
{
	if (GTK_WIDGET_VISIBLE(compose->reply_entry))
		gtk_widget_grab_focus(compose->reply_entry);
	else if (GTK_WIDGET_VISIBLE(compose->followup_entry))
		gtk_widget_grab_focus(compose->followup_entry);
	else
		gtk_widget_grab_focus(compose->text);
}

static void replyto_activated(GtkWidget *widget, Compose *compose)
{
	if (GTK_WIDGET_VISIBLE(compose->followup_entry))
		gtk_widget_grab_focus(compose->followup_entry);
	else
		gtk_widget_grab_focus(compose->text);
}

static void followupto_activated(GtkWidget *widget, Compose *compose)
{
	gtk_widget_grab_focus(compose->text);
}

static void compose_toggle_return_receipt_cb(gpointer data, guint action,
					     GtkWidget *widget)
{
	Compose *compose = (Compose *)data;

	if (GTK_CHECK_MENU_ITEM(widget)->active)
		compose->return_receipt = TRUE;
	else
		compose->return_receipt = FALSE;
}
