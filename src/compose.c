/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2005 Hiroyuki Yamamoto
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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
/* #include <sys/utsname.h> */
#include <stdlib.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <libgen.h>

#if (HAVE_WCTYPE_H && HAVE_WCHAR_H)
#  include <wchar.h>
#  include <wctype.h>
#endif

#include "intl.h"
#include "main.h"
#include "mainwindow.h"
#include "compose.h"
#include "gtkstext.h"
#include "addressbook.h"
#include "folderview.h"
#include "procmsg.h"
#include "menu.h"
#include "stock_pixmap.h"
#include "send_message.h"
#include "imap.h"
#include "news.h"
#include "customheader.h"
#include "prefs_common.h"
#include "prefs_account.h"
#include "action.h"
#include "account.h"
#include "filesel.h"
#include "procheader.h"
#include "procmime.h"
#include "statusbar.h"
#include "about.h"
#include "base64.h"
#include "quoted-printable.h"
#include "codeconv.h"
#include "utils.h"
#include "gtkutils.h"
#include "socket.h"
#include "alertpanel.h"
#include "manage_window.h"
#include "gtkshruler.h"
#include "folder.h"
#include "addr_compl.h"
#include "quote_fmt.h"
#include "template.h"
#include "undo.h"
#include "foldersel.h"
#include "toolbar.h"

typedef enum
{
	COL_MIMETYPE = 0,
	COL_SIZE     = 1,
	COL_NAME     = 2
} AttachColumnPos;

#define N_ATTACH_COLS		3

typedef enum
{
	COMPOSE_CALL_GTK_STEXT_MOVE_BEGINNING_OF_LINE,
	COMPOSE_CALL_GTK_STEXT_MOVE_FORWARD_CHARACTER,
	COMPOSE_CALL_GTK_STEXT_MOVE_BACKWARD_CHARACTER,
	COMPOSE_CALL_GTK_STEXT_MOVE_FORWARD_WORD,
	COMPOSE_CALL_GTK_STEXT_MOVE_BACKWARD_WORD,
	COMPOSE_CALL_GTK_STEXT_MOVE_END_OF_LINE,
	COMPOSE_CALL_GTK_STEXT_MOVE_NEXT_LINE,
	COMPOSE_CALL_GTK_STEXT_MOVE_PREVIOUS_LINE,
	COMPOSE_CALL_GTK_STEXT_DELETE_FORWARD_CHARACTER,
	COMPOSE_CALL_GTK_STEXT_DELETE_BACKWARD_CHARACTER,
	COMPOSE_CALL_GTK_STEXT_DELETE_FORWARD_WORD,
	COMPOSE_CALL_GTK_STEXT_DELETE_BACKWARD_WORD,
	COMPOSE_CALL_GTK_STEXT_DELETE_LINE,
	COMPOSE_CALL_GTK_STEXT_DELETE_LINE_N,
	COMPOSE_CALL_GTK_STEXT_DELETE_TO_LINE_END
} ComposeCallGtkSTextAction;

typedef enum
{
	PRIORITY_HIGHEST = 1,
	PRIORITY_HIGH,
	PRIORITY_NORMAL,
	PRIORITY_LOW,
	PRIORITY_LOWEST
} PriorityLevel;

typedef enum
{
	COMPOSE_INSERT_SUCCESS,
	COMPOSE_INSERT_READ_ERROR,
	COMPOSE_INSERT_INVALID_CHARACTER,
	COMPOSE_INSERT_NO_FILE
} ComposeInsertResult;

typedef enum
{
	COMPOSE_QUIT_EDITING,
	COMPOSE_KEEP_EDITING,
	COMPOSE_AUTO_SAVE
} ComposeDraftAction;

typedef enum
{
	COMPOSE_WRITE_FOR_SEND,
	COMPOSE_WRITE_FOR_STORE
} ComposeWriteType;


#define B64_LINE_SIZE		57
#define B64_BUFFSIZE		77

#define MAX_REFERENCES_LEN	999

static GdkColor quote_color = {0, 0, 0, 0xbfff};

static GList *compose_list = NULL;

Compose *compose_generic_new			(PrefsAccount	*account,
						 const gchar	*to,
						 FolderItem	*item,
						 GPtrArray 	*attach_files,
						 GList          *listAddress );

static Compose *compose_create			(PrefsAccount	*account,
						 ComposeMode	 mode);

static GtkWidget *compose_account_option_menu_create
						(Compose	*compose);
static void compose_set_template_menu		(Compose	*compose);
static void compose_template_apply		(Compose	*compose,
						 Template	*tmpl,
						 gboolean	 replace);
static void compose_destroy			(Compose	*compose);

static void compose_entries_set			(Compose	*compose,
						 const gchar	*mailto);
static gint compose_parse_header		(Compose	*compose,
						 MsgInfo	*msginfo);
static gchar *compose_parse_references		(const gchar	*ref,
						 const gchar	*msgid);

static gchar *compose_quote_fmt			(Compose	*compose,
						 MsgInfo	*msginfo,
						 const gchar	*fmt,
						 const gchar	*qmark,
						 const gchar	*body);

static void compose_reply_set_entry		(Compose	*compose,
						 MsgInfo	*msginfo,
						 gboolean	 to_all,
						 gboolean	 to_ml,
						 gboolean	 to_sender,
						 gboolean
						 followup_and_reply_to);
static void compose_reedit_set_entry		(Compose	*compose,
						 MsgInfo	*msginfo);
static void compose_insert_sig			(Compose	*compose,
						 gboolean	 replace);
static gchar *compose_get_signature_str		(Compose	*compose);
static ComposeInsertResult compose_insert_file	(Compose	*compose,
						 const gchar	*file);
static void compose_attach_append		(Compose	*compose,
						 const gchar	*file,
						 const gchar	*type,
						 const gchar	*content_type);
static void compose_attach_parts		(Compose	*compose,
						 MsgInfo	*msginfo);
static void compose_wrap_line			(Compose	*compose);
static void compose_wrap_line_all		(Compose	*compose);
static void compose_wrap_line_all_full		(Compose	*compose,
						 gboolean	 autowrap);
static void compose_set_title			(Compose	*compose);
static void compose_select_account		(Compose	*compose,
						 PrefsAccount	*account,
						 gboolean	 init);

static PrefsAccount *compose_current_mail_account(void);
/* static gint compose_send			(Compose	*compose); */
static gboolean compose_check_for_valid_recipient
						(Compose	*compose);
static gboolean compose_check_entries		(Compose	*compose,
						 gboolean 	check_subject);
static gint compose_write_to_file		(Compose	*compose,
						 FILE		*fp,
						 gint		 action);
static gint compose_write_body_to_file		(Compose	*compose,
						 const gchar	*file);
static gint compose_remove_reedit_target	(Compose	*compose);
void compose_remove_draft			(Compose	*compose);
static gint compose_queue			(Compose	*compose,
						 gint		*msgnum,
						 FolderItem	**item);
static gint compose_queue_sub			(Compose	*compose,
						 gint		*msgnum,
						 FolderItem	**item,
						 gboolean	check_subject);
static void compose_add_attachments		(Compose	*compose,
						 MimeInfo	*parent);
static gchar *compose_get_header		(Compose	*compose);

static void compose_convert_header		(gchar		*dest,
						 gint		 len,
						 gchar		*src,
						 gint		 header_len,
						 gboolean	 addr_field);

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

static void compose_undo_state_changed		(UndoMain	*undostruct,
						 gint		 undo_state,
						 gint		 redo_state,
						 gpointer	 data);

static gint calc_cursor_xpos	(GtkSText	*text,
				 gint		 extra,
				 gint		 char_width);

static void compose_create_header_entry	(Compose *compose);
static void compose_add_header_entry	(Compose *compose, gchar *header, gchar *text);
static void compose_update_priority_menu_item(Compose * compose);

static void compose_add_field_list	( Compose *compose,
					  GList *listAddress );

/* callback functions */

static gboolean compose_edit_size_alloc (GtkEditable	*widget,
					 GtkAllocation	*allocation,
					 GtkSHRuler	*shruler);
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
static void compose_insert_sig_cb	(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);

static void compose_close_cb		(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);

static void compose_address_cb		(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void compose_template_activate_cb(GtkWidget	*widget,
					 gpointer	 data);

static void compose_ext_editor_cb	(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);

static gint compose_delete_cb		(GtkWidget	*widget,
					 GdkEventAny	*event,
					 gpointer	 data);
static void compose_destroy_cb		(GtkWidget	*widget,
					 Compose	*compose);

static void compose_undo_cb		(Compose	*compose);
static void compose_redo_cb		(Compose	*compose);
static void compose_cut_cb		(Compose	*compose);
static void compose_copy_cb		(Compose	*compose);
static void compose_paste_cb		(Compose	*compose);
static void compose_paste_as_quote_cb	(Compose	*compose);
static void compose_allsel_cb		(Compose	*compose);

static void compose_gtk_stext_action_cb	(Compose		   *compose,
					 ComposeCallGtkSTextAction  action);

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

static void compose_toggle_autowrap_cb	(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);

#if 0
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
#endif
static void compose_toggle_ruler_cb	(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void compose_toggle_sign_cb	(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void compose_toggle_encrypt_cb	(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void compose_set_privacy_system_cb(GtkWidget      *widget,
					  gpointer        data);
static void compose_update_privacy_system_menu_item(Compose * compose);
static void activate_privacy_system     (Compose *compose, 
                                         PrefsAccount *account);
static void compose_use_signing(Compose *compose, gboolean use_signing);
static void compose_use_encryption(Compose *compose, gboolean use_encryption);
static void compose_toggle_return_receipt_cb(gpointer data, guint action,
					     GtkWidget *widget);
static void compose_toggle_remove_refs_cb(gpointer data, guint action,
					     GtkWidget *widget);
static void compose_set_priority_cb	(gpointer 	 data,
					 guint 		 action,
					 GtkWidget 	*widget);

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

#if 0
static void to_activated		(GtkWidget	*widget,
					 Compose	*compose);
static void newsgroups_activated	(GtkWidget	*widget,
					 Compose	*compose);
static void cc_activated		(GtkWidget	*widget,
					 Compose	*compose);
static void bcc_activated		(GtkWidget	*widget,
					 Compose	*compose);
static void replyto_activated		(GtkWidget	*widget,
					 Compose	*compose);
static void followupto_activated	(GtkWidget	*widget,
					 Compose	*compose);
static void subject_activated		(GtkWidget	*widget,
					 Compose	*compose);
#endif

static void text_activated		(GtkWidget	*widget,
					 Compose	*compose);
static void text_inserted		(GtkWidget	*widget,
					 const gchar	*text,
					 gint		 length,
					 gint		*position,
					 Compose	*compose);
static void compose_generic_reply(MsgInfo *msginfo, gboolean quote,
				  gboolean to_all, gboolean to_ml,
				  gboolean to_sender,
				  gboolean followup_and_reply_to,
				  const gchar *body);

void compose_headerentry_changed_cb	   (GtkWidget	       *entry,
					    ComposeHeaderEntry *headerentry);
void compose_headerentry_key_press_event_cb(GtkWidget	       *entry,
					    GdkEventKey        *event,
					    ComposeHeaderEntry *headerentry);
static gboolean compose_headerentry_button_pressed (GtkWidget *entry, 
					            GdkEventButton *event,
						    gpointer data);

static void compose_show_first_last_header (Compose *compose, gboolean show_first);

static void compose_allow_user_actions (Compose *compose, gboolean allow);

#if USE_ASPELL
static void compose_check_all		   (Compose *compose);
static void compose_highlight_all	   (Compose *compose);
static void compose_check_backwards	   (Compose *compose);
static void compose_check_forwards_go	   (Compose *compose);
#endif

static gboolean compose_send_control_enter	(Compose	*compose);
static gint compose_defer_auto_save_draft	(Compose	*compose);
static PrefsAccount *compose_guess_forward_account_from_msginfo	(MsgInfo *msginfo);

static void compose_close	(Compose *compose);

static GtkItemFactoryEntry compose_popup_entries[] =
{
	{N_("/_Add..."),	NULL, compose_attach_cb, 0, NULL},
	{N_("/_Remove"),	NULL, compose_attach_remove_selected, 0, NULL},
	{N_("/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_Properties..."),	NULL, compose_attach_property, 0, NULL}
};

static GtkItemFactoryEntry compose_entries[] =
{
	{N_("/_Message"),				NULL, NULL, 0, "<Branch>"},
	{N_("/_Message/_Send"),		"<control>Return",
					compose_send_cb, 0, NULL},
	{N_("/_Message/Send _later"),	"<shift><control>S",
					compose_send_later_cb,  0, NULL},
	{N_("/_Message/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Message/_Attach file"),		"<control>M", compose_attach_cb,      0, NULL},
	{N_("/_Message/_Insert file"),		"<control>I", compose_insert_file_cb, 0, NULL},
	{N_("/_Message/Insert si_gnature"),	"<control>G", compose_insert_sig_cb,  0, NULL},
	{N_("/_Message/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Message/_Save"),
						"<control>S", compose_draft_cb, COMPOSE_KEEP_EDITING, NULL},
	{N_("/_Message/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Message/_Close"),			"<control>W", compose_close_cb, 0, NULL},

	{N_("/_Edit"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_Edit/_Undo"),		"<control>Z", compose_undo_cb, 0, NULL},
	{N_("/_Edit/_Redo"),		"<control>Y", compose_redo_cb, 0, NULL},
	{N_("/_Edit/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_Edit/Cu_t"),		"<control>X", compose_cut_cb,    0, NULL},
	{N_("/_Edit/_Copy"),		"<control>C", compose_copy_cb,   0, NULL},
	{N_("/_Edit/_Paste"),		"<control>V", compose_paste_cb,  0, NULL},
	{N_("/_Edit/Paste as _quotation"),
					NULL, compose_paste_as_quote_cb, 0, NULL},
	{N_("/_Edit/Select _all"),	"<control>A", compose_allsel_cb, 0, NULL},
	{N_("/_Edit/A_dvanced"),	NULL, NULL, 0, "<Branch>"},
	{N_("/_Edit/A_dvanced/Move a character backward"),
					"<control>B",
					compose_gtk_stext_action_cb,
					COMPOSE_CALL_GTK_STEXT_MOVE_BACKWARD_CHARACTER,
					NULL},
	{N_("/_Edit/A_dvanced/Move a character forward"),
					"<control>F",
					compose_gtk_stext_action_cb,
					COMPOSE_CALL_GTK_STEXT_MOVE_FORWARD_CHARACTER,
					NULL},
	{N_("/_Edit/A_dvanced/Move a word backward"),
					NULL, /* "<alt>B" */
					compose_gtk_stext_action_cb,
					COMPOSE_CALL_GTK_STEXT_MOVE_BACKWARD_WORD,
					NULL},
	{N_("/_Edit/A_dvanced/Move a word forward"),
					NULL, /* "<alt>F" */
					compose_gtk_stext_action_cb,
					COMPOSE_CALL_GTK_STEXT_MOVE_FORWARD_WORD,
					NULL},
	{N_("/_Edit/A_dvanced/Move to beginning of line"),
					NULL, /* "<control>A" */
					compose_gtk_stext_action_cb,
					COMPOSE_CALL_GTK_STEXT_MOVE_BEGINNING_OF_LINE,
					NULL},
	{N_("/_Edit/A_dvanced/Move to end of line"),
					"<control>E",
					compose_gtk_stext_action_cb,
					COMPOSE_CALL_GTK_STEXT_MOVE_END_OF_LINE,
					NULL},
	{N_("/_Edit/A_dvanced/Move to previous line"),
					"<control>P",
					compose_gtk_stext_action_cb,
					COMPOSE_CALL_GTK_STEXT_MOVE_PREVIOUS_LINE,
					NULL},
	{N_("/_Edit/A_dvanced/Move to next line"),
					"<control>N",
					compose_gtk_stext_action_cb,
					COMPOSE_CALL_GTK_STEXT_MOVE_NEXT_LINE,
					NULL},
	{N_("/_Edit/A_dvanced/Delete a character backward"),
					"<control>H",
					compose_gtk_stext_action_cb,
					COMPOSE_CALL_GTK_STEXT_DELETE_BACKWARD_CHARACTER,
					NULL},
	{N_("/_Edit/A_dvanced/Delete a character forward"),
					"<control>D",
					compose_gtk_stext_action_cb,
					COMPOSE_CALL_GTK_STEXT_DELETE_FORWARD_CHARACTER,
					NULL},
	{N_("/_Edit/A_dvanced/Delete a word backward"),
					NULL, /* "<control>W" */
					compose_gtk_stext_action_cb,
					COMPOSE_CALL_GTK_STEXT_DELETE_BACKWARD_WORD,
					NULL},
	{N_("/_Edit/A_dvanced/Delete a word forward"),
					NULL, /* "<alt>D", */
					compose_gtk_stext_action_cb,
					COMPOSE_CALL_GTK_STEXT_DELETE_FORWARD_WORD,
					NULL},
	{N_("/_Edit/A_dvanced/Delete line"),
					"<control>U",
					compose_gtk_stext_action_cb,
					COMPOSE_CALL_GTK_STEXT_DELETE_LINE,
					NULL},
	{N_("/_Edit/A_dvanced/Delete entire line"),
					NULL,
					compose_gtk_stext_action_cb,
					COMPOSE_CALL_GTK_STEXT_DELETE_LINE_N,
					NULL},
	{N_("/_Edit/A_dvanced/Delete to end of line"),
					"<control>K",
					compose_gtk_stext_action_cb,
					COMPOSE_CALL_GTK_STEXT_DELETE_TO_LINE_END,
					NULL},
	{N_("/_Edit/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_Edit/_Wrap current paragraph"),
					"<control>L", compose_wrap_line, 0, NULL},
	{N_("/_Edit/Wrap all long _lines"),
					"<control><alt>L", compose_wrap_line_all, 0, NULL},
	{N_("/_Edit/Aut_o wrapping"),	"<shift><control>L", compose_toggle_autowrap_cb, 0, "<ToggleItem>"},
	{N_("/_Edit/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_Edit/Edit with e_xternal editor"),
					"<shift><control>X", compose_ext_editor_cb, 0, NULL},
#if USE_ASPELL
	{N_("/_Spelling"),		NULL, NULL, 0, "<Branch>"},
	{N_("/_Spelling/_Check all or check selection"),
					NULL, compose_check_all, 0, NULL},
	{N_("/_Spelling/_Highlight all misspelled words"),
					NULL, compose_highlight_all, 0, NULL},
	{N_("/_Spelling/Check _backwards misspelled word"),
					NULL, compose_check_backwards , 0, NULL},
	{N_("/_Spelling/_Forward to next misspelled word"),
					NULL, compose_check_forwards_go, 0, NULL},
	{N_("/_Spelling/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_Spelling/_Spelling Configuration"),
					NULL, NULL, 0, "<Branch>"},
#endif
	{N_("/_Options"),		NULL, NULL, 0, "<Branch>"},
	{N_("/_Options/Privacy System"),		NULL, NULL,   0, "<Branch>"},
	{N_("/_Options/Privacy System/None"),	NULL, NULL,   0, "<RadioItem>"},
	{N_("/_Options/Si_gn"),   	NULL, compose_toggle_sign_cb   , 0, "<ToggleItem>"},
	{N_("/_Options/_Encrypt"),	NULL, compose_toggle_encrypt_cb, 0, "<ToggleItem>"},
	{N_("/_Options/---"),		NULL,		NULL,	0, "<Separator>"},
	{N_("/_Options/_Priority"),	NULL,		NULL,   0, "<Branch>"},
	{N_("/_Options/Priority/_Highest"), NULL, compose_set_priority_cb, PRIORITY_HIGHEST, "<RadioItem>"},
	{N_("/_Options/Priority/Hi_gh"),    NULL, compose_set_priority_cb, PRIORITY_HIGH, "/Options/Priority/Highest"},
	{N_("/_Options/Priority/_Normal"),  NULL, compose_set_priority_cb, PRIORITY_NORMAL, "/Options/Priority/Highest"},
	{N_("/_Options/Priority/Lo_w"),	   NULL, compose_set_priority_cb, PRIORITY_LOW, "/Options/Priority/Highest"},
	{N_("/_Options/Priority/_Lowest"),  NULL, compose_set_priority_cb, PRIORITY_LOWEST, "/Options/Priority/Highest"},
	{N_("/_Options/---"),		NULL,		NULL,	0, "<Separator>"},
	{N_("/_Options/_Request Return Receipt"),	NULL, compose_toggle_return_receipt_cb, 0, "<ToggleItem>"},
	{N_("/_Options/Remo_ve references"),	NULL, compose_toggle_remove_refs_cb, 0, "<ToggleItem>"},
	{N_("/_Tools"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_Tools/Show _ruler"),	NULL, compose_toggle_ruler_cb, 0, "<ToggleItem>"},
	{N_("/_Tools/_Address book"),	"<shift><control>A", compose_address_cb , 0, NULL},
	{N_("/_Tools/_Template"),	NULL, NULL, 0, "<Branch>"},
	{N_("/_Tools/Actio_ns"),	NULL, NULL, 0, "<Branch>"},
	{N_("/_Help"),			NULL, NULL, 0, "<Branch>"},
	{N_("/_Help/_About"),		NULL, about_show, 0, NULL}
};

static GtkTargetEntry compose_mime_types[] =
{
	{"text/uri-list", 0, 0}
};

static gboolean compose_put_existing_to_front(MsgInfo *info)
{
	GList *compose_list = compose_get_compose_list();
	GList *elem = NULL;
	
	if (compose_list) {
		for (elem = compose_list; elem != NULL && elem->data != NULL; 
		     elem = elem->next) {
			Compose *c = (Compose*)elem->data;

			if (!c->targetinfo || !c->targetinfo->msgid ||
			    !info->msgid)
			    	continue;

			if (!strcmp(c->targetinfo->msgid, info->msgid)) {
				gtkut_window_popup(c->window);
				return TRUE;
			}
		}
	}
	return FALSE;
}

Compose *compose_new(PrefsAccount *account, const gchar *mailto,
		     GPtrArray *attach_files)
{
	return compose_generic_new(account, mailto, NULL, attach_files, NULL);
}

Compose *compose_new_with_folderitem(PrefsAccount *account, FolderItem *item)
{
	return compose_generic_new(account, NULL, item, NULL, NULL);
}

Compose *compose_new_with_list( PrefsAccount *account, GList *listAddress )
{
	return compose_generic_new( account, NULL, NULL, NULL, listAddress );
}

Compose *compose_generic_new(PrefsAccount *account, const gchar *mailto, FolderItem *item,
			     GPtrArray *attach_files, GList *listAddress )
{
	Compose *compose;
	GtkSText *text;
	GtkItemFactory *ifactory;

	if (item && item->prefs && item->prefs->enable_default_account)
		account = account_find_from_id(item->prefs->default_account);

 	if (!account) account = cur_account;
	g_return_val_if_fail(account != NULL, NULL);

	compose = compose_create(account, COMPOSE_NEW);
	ifactory = gtk_item_factory_from_widget(compose->menubar);

	compose->replyinfo = NULL;
	compose->fwdinfo   = NULL;

	text = GTK_STEXT(compose->text);
	gtk_stext_freeze(text);

#ifdef USE_ASPELL
	if (item && item->prefs && item->prefs->enable_default_dictionary &&
	    compose->gtkaspell) 
		gtkaspell_change_dict(compose->gtkaspell, 
		    item->prefs->default_dictionary);
#endif

	if (account->auto_sig)
		compose_insert_sig(compose, FALSE);
	gtk_editable_set_position(GTK_EDITABLE(text), 0);
	gtk_stext_set_point(text, 0);

	gtk_stext_thaw(text);

	if (account->protocol != A_NNTP) {
		if (mailto && *mailto != '\0') {
			compose_entries_set(compose, mailto);

		} else if (item && item->prefs->enable_default_to) {
			compose_entry_append(compose, item->prefs->default_to, COMPOSE_TO);
			compose_entry_mark_default_to(compose, item->prefs->default_to);
		}
		if (item && item->ret_rcpt) {
			menu_set_active(ifactory, "/Message/Request Return Receipt", TRUE);
		}
	} else {
		if (mailto) {
			compose_entry_append(compose, mailto, COMPOSE_NEWSGROUPS);
		}
		/*
		 * CLAWS: just don't allow return receipt request, even if the user
		 * may want to send an email. simple but foolproof.
		 */
		menu_set_sensitive(ifactory, "/Message/Request Return Receipt", FALSE); 
	}
	compose_add_field_list( compose, listAddress );

	if (attach_files) {
		gint i;
		gchar *file;

		for (i = 0; i < attach_files->len; i++) {
			file = g_ptr_array_index(attach_files, i);
			compose_attach_append(compose, file, file, NULL);
		}
	}

	compose_show_first_last_header(compose, TRUE);

	/* Set save folder */
	if (item && item->prefs && item->prefs->save_copy_to_folder) {
		gchar *folderidentifier;

    		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(compose->savemsg_checkbtn), prefs_common.savemsg);
		folderidentifier = folder_item_get_identifier(item);
		gtk_entry_set_text(GTK_ENTRY(compose->savemsg_entry), folderidentifier);
		g_free(folderidentifier);
	}
	
	gtk_widget_grab_focus(compose->header_last->entry);

	if (prefs_common.auto_exteditor)
		compose_exec_ext_editor(compose);

        return compose;
}

/*
Compose *compose_new_followup_and_replyto(PrefsAccount *account,
					   const gchar *followupto, gchar * to)
{
	Compose *compose;

	if (!account) account = cur_account;
	g_return_val_if_fail(account != NULL, NULL);
	g_return_val_if_fail(account->protocol != A_NNTP, NULL);

	compose = compose_create(account, COMPOSE_NEW);

	if (prefs_common.auto_sig)
		compose_insert_sig(compose);
	gtk_editable_set_position(GTK_EDITABLE(compose->text), 0);
	gtk_stext_set_point(GTK_STEXT(compose->text), 0);

	compose_entry_append(compose, to, COMPOSE_TO);
	compose_entry_append(compose, followupto, COMPOSE_NEWSGROUPS);
	gtk_widget_grab_focus(compose->subject_entry);

	return compose;
}
*/

void compose_reply_mode(ComposeMode mode, GSList *msginfo_list, gchar *body)
{
	MsgInfo *msginfo;
	guint list_len;

	g_return_if_fail(msginfo_list != NULL);

	msginfo = (MsgInfo*)g_slist_nth_data(msginfo_list, 0);
	g_return_if_fail(msginfo != NULL);

	list_len = g_slist_length(msginfo_list);

	switch (mode) {
	case COMPOSE_REPLY:
		compose_reply(msginfo, prefs_common.reply_with_quote,
		    	      FALSE, prefs_common.default_reply_list, FALSE, body);
		break;
	case COMPOSE_REPLY_WITH_QUOTE:
		compose_reply(msginfo, TRUE, FALSE, prefs_common.default_reply_list, FALSE, body);
		break;
	case COMPOSE_REPLY_WITHOUT_QUOTE:
		compose_reply(msginfo, FALSE, FALSE, prefs_common.default_reply_list, FALSE, NULL);
		break;
	case COMPOSE_REPLY_TO_SENDER:
		compose_reply(msginfo, prefs_common.reply_with_quote,
			      FALSE, FALSE, TRUE, body);
		break;
	case COMPOSE_FOLLOWUP_AND_REPLY_TO:
		compose_followup_and_reply_to(msginfo,
					      prefs_common.reply_with_quote,
					      FALSE, FALSE, body);
		break;
	case COMPOSE_REPLY_TO_SENDER_WITH_QUOTE:
		compose_reply(msginfo, TRUE, FALSE, FALSE, TRUE, body);
		break;
	case COMPOSE_REPLY_TO_SENDER_WITHOUT_QUOTE:
		compose_reply(msginfo, FALSE, FALSE, FALSE, TRUE, NULL);
		break;
	case COMPOSE_REPLY_TO_ALL:
		compose_reply(msginfo, prefs_common.reply_with_quote,
			      TRUE, FALSE, FALSE, body);
		break;
	case COMPOSE_REPLY_TO_ALL_WITH_QUOTE:
		compose_reply(msginfo, TRUE, TRUE, FALSE, FALSE, body);
		break;
	case COMPOSE_REPLY_TO_ALL_WITHOUT_QUOTE:
		compose_reply(msginfo, FALSE, TRUE, FALSE, FALSE, NULL);
		break;
	case COMPOSE_REPLY_TO_LIST:
		compose_reply(msginfo, prefs_common.reply_with_quote,
			      FALSE, TRUE, FALSE, body);
		break;
	case COMPOSE_REPLY_TO_LIST_WITH_QUOTE:
		compose_reply(msginfo, TRUE, FALSE, TRUE, FALSE, body);
		break;
	case COMPOSE_REPLY_TO_LIST_WITHOUT_QUOTE:
		compose_reply(msginfo, FALSE, FALSE, TRUE, FALSE, NULL);
		break;
	case COMPOSE_FORWARD:
		if (prefs_common.forward_as_attachment) {
			compose_reply_mode(COMPOSE_FORWARD_AS_ATTACH, msginfo_list, body);
			return;
		} else {
			compose_reply_mode(COMPOSE_FORWARD_INLINE, msginfo_list, body);
			return;
		}
		break;
	case COMPOSE_FORWARD_INLINE:
		/* check if we reply to more than one Message */
		if (list_len == 1) {
			compose_forward(NULL, msginfo, FALSE, body, FALSE);
			break;
		} 
		/* more messages FALL THROUGH */
	case COMPOSE_FORWARD_AS_ATTACH:
		compose_forward_multiple(NULL, msginfo_list);
		break;
	case COMPOSE_REDIRECT:
		compose_redirect(NULL, msginfo);
		break;
	default:
		g_warning("compose_reply(): invalid Compose Mode: %d\n", mode);
	}
}

void compose_reply(MsgInfo *msginfo, gboolean quote, gboolean to_all,
		   gboolean to_ml, gboolean to_sender, 
		   const gchar *body)
{
	compose_generic_reply(msginfo, quote, to_all, to_ml, 
			      to_sender, FALSE, body);
}

void compose_followup_and_reply_to(MsgInfo *msginfo, gboolean quote,
				   gboolean to_all,
				   gboolean to_sender,
				   const gchar *body)
{
	compose_generic_reply(msginfo, quote, to_all, FALSE, 
			      to_sender, TRUE, body);
}

static void compose_generic_reply(MsgInfo *msginfo, gboolean quote,
				  gboolean to_all, gboolean to_ml,
				  gboolean to_sender,
				  gboolean followup_and_reply_to,
				  const gchar *body)
{
	GtkItemFactory *ifactory;
	Compose *compose;
	PrefsAccount *account = NULL;
	PrefsAccount *reply_account;
	GtkSText *text;

	g_return_if_fail(msginfo != NULL);
	g_return_if_fail(msginfo->folder != NULL);

	account = account_get_reply_account(msginfo, prefs_common.reply_account_autosel);
	
	g_return_if_fail(account != NULL);

	if (to_sender && account->protocol == A_NNTP &&
	    !followup_and_reply_to) {
		reply_account =
			account_find_from_address(account->address);
		if (!reply_account)
			reply_account = compose_current_mail_account();
		if (!reply_account)
			return;
	} else
		reply_account = account;

	compose = compose_create(account, COMPOSE_REPLY);
	ifactory = gtk_item_factory_from_widget(compose->menubar);

	menu_set_active(ifactory, "/Options/Remove references", FALSE);
	menu_set_sensitive(ifactory, "/Options/Remove references", TRUE);

	compose->replyinfo = procmsg_msginfo_get_full_info(msginfo);
	if (!compose->replyinfo)
		compose->replyinfo = procmsg_msginfo_copy(msginfo);

    	if (msginfo->folder && msginfo->folder->ret_rcpt)
		menu_set_active(ifactory, "/Message/Request Return Receipt", TRUE);

	/* Set save folder */
	if (msginfo->folder && msginfo->folder->prefs && msginfo->folder->prefs->save_copy_to_folder) {
		gchar *folderidentifier;

    		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(compose->savemsg_checkbtn), TRUE);
		folderidentifier = folder_item_get_identifier(msginfo->folder);
		gtk_entry_set_text(GTK_ENTRY(compose->savemsg_entry), folderidentifier);
		g_free(folderidentifier);
	}

	if (compose_parse_header(compose, msginfo) < 0) return;
	compose_reply_set_entry(compose, msginfo, to_all, to_ml, 
				to_sender, followup_and_reply_to);
	compose_show_first_last_header(compose, TRUE);

	text = GTK_STEXT(compose->text);
	gtk_stext_freeze(text);

#ifdef USE_ASPELL
	if (msginfo->folder && msginfo->folder->prefs && 
	    msginfo->folder->prefs && 
	    msginfo->folder->prefs->enable_default_dictionary &&
	    compose->gtkaspell)
		gtkaspell_change_dict(compose->gtkaspell, 
		    msginfo->folder->prefs->default_dictionary);
#endif

	if (quote) {
		gchar *qmark;

		if (prefs_common.quotemark && *prefs_common.quotemark)
			qmark = prefs_common.quotemark;
		else
			qmark = "> ";

		compose_quote_fmt(compose, compose->replyinfo,
			          prefs_common.quotefmt,
			          qmark, body);
	}

	if (account->auto_sig)
		compose_insert_sig(compose, FALSE);

	/* Must thaw here, otherwise the GtkSTextEdit will become confused 
	 * when positioning cursor */
	gtk_stext_thaw(text);

	if (quote) {
		gtk_editable_set_position
			(GTK_EDITABLE(text), quote_fmt_get_cursor_pos());
		gtk_stext_set_point(text, quote_fmt_get_cursor_pos());
	}

	if (quote && prefs_common.linewrap_quote) {
		gtk_stext_freeze(text);
		compose_wrap_line_all(compose);
		gtk_stext_thaw(text);
	}

	gtk_widget_grab_focus(compose->text);

	if (prefs_common.auto_exteditor)
		compose_exec_ext_editor(compose);
}

#define INSERT_FW_HEADER(var, hdr) \
if (msginfo->var && *msginfo->var) { \
	gtk_stext_insert(text, NULL, NULL, NULL, hdr, -1); \
	gtk_stext_insert(text, NULL, NULL, NULL, msginfo->var, -1); \
	gtk_stext_insert(text, NULL, NULL, NULL, "\n", 1); \
}

Compose *compose_forward(PrefsAccount *account, MsgInfo *msginfo,
			 gboolean as_attach, const gchar *body,
			 gboolean no_extedit)
{
	Compose *compose;
	GtkSText *text;

	g_return_val_if_fail(msginfo != NULL, NULL);
	g_return_val_if_fail(msginfo->folder != NULL, NULL);

	if (!account && 
	    !(account = compose_guess_forward_account_from_msginfo
				(msginfo)))
		account = cur_account;

	compose = compose_create(account, COMPOSE_FORWARD);

	compose->fwdinfo = procmsg_msginfo_get_full_info(msginfo);
	if (!compose->fwdinfo)
		compose->fwdinfo = procmsg_msginfo_copy(msginfo);

	if (msginfo->subject && *msginfo->subject) {
		gchar *buf, *buf2, *p;

		buf = p = g_strdup(msginfo->subject);
		p += subject_get_prefix_length(p);
		memmove(buf, p, strlen(p) + 1);

		buf2 = g_strdup_printf("Fw: %s", buf);
		gtk_entry_set_text(GTK_ENTRY(compose->subject_entry), buf2);
		
		g_free(buf);
		g_free(buf2);
	}

	text = GTK_STEXT(compose->text);
	gtk_stext_freeze(text);

	if (as_attach) {
		gchar *msgfile;

		msgfile = procmsg_get_message_file_path(msginfo);
		if (!is_file_exist(msgfile))
			g_warning("%s: file not exist\n", msgfile);
		else
			compose_attach_append(compose, msgfile, msgfile,
					      "message/rfc822");

		g_free(msgfile);
	} else {
		gchar *qmark;
		MsgInfo *full_msginfo;

		full_msginfo = procmsg_msginfo_get_full_info(msginfo);
		if (!full_msginfo)
			full_msginfo = procmsg_msginfo_copy(msginfo);

		if (prefs_common.fw_quotemark &&
		    *prefs_common.fw_quotemark)
			qmark = prefs_common.fw_quotemark;
		else
			qmark = "> ";

		compose_quote_fmt(compose, full_msginfo,
			          prefs_common.fw_quotefmt,
			          qmark, body);
		compose_attach_parts(compose, msginfo);

		procmsg_msginfo_free(full_msginfo);
	}

	if (account->auto_sig)
		compose_insert_sig(compose, FALSE);

	if (prefs_common.linewrap_quote)
		compose_wrap_line_all(compose);

	gtk_editable_set_position(GTK_EDITABLE(compose->text), 0);
	gtk_stext_set_point(GTK_STEXT(compose->text), 0);

	gtk_stext_thaw(text);
#if 0 /* NEW COMPOSE GUI */
	if (account->protocol != A_NNTP)
		gtk_widget_grab_focus(compose->to_entry);
	else
		gtk_widget_grab_focus(compose->newsgroups_entry);
#endif
	gtk_widget_grab_focus(compose->header_last->entry);

	if (!no_extedit && prefs_common.auto_exteditor)
		compose_exec_ext_editor(compose);
	
	/*save folder*/
	if (msginfo->folder && msginfo->folder->prefs && msginfo->folder->prefs->save_copy_to_folder) {
		gchar *folderidentifier;

    		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(compose->savemsg_checkbtn), TRUE);
		folderidentifier = folder_item_get_identifier(msginfo->folder);
		gtk_entry_set_text(GTK_ENTRY(compose->savemsg_entry), folderidentifier);
		g_free(folderidentifier);
	}

        return compose;
}

#undef INSERT_FW_HEADER

Compose *compose_forward_multiple(PrefsAccount *account, GSList *msginfo_list)
{
	Compose *compose;
	GtkSText *text;
	GSList *msginfo;
	gchar *msgfile;

	g_return_val_if_fail(msginfo_list != NULL, NULL);

	for (msginfo = msginfo_list; msginfo != NULL; msginfo = msginfo->next)
		if (((MsgInfo *)msginfo->data)->folder == NULL)
			return NULL;

	/* guess account from first selected message */
	if (!account && 
	    !(account = compose_guess_forward_account_from_msginfo
				(msginfo_list->data)))
		account = cur_account;

	g_return_val_if_fail(account != NULL, NULL);

	for (msginfo = msginfo_list; msginfo != NULL; msginfo = msginfo->next) {
		MSG_UNSET_PERM_FLAGS(((MsgInfo *)msginfo->data)->flags, MSG_REPLIED);
		MSG_SET_PERM_FLAGS(((MsgInfo *)msginfo->data)->flags, MSG_FORWARDED);
	}

	compose = compose_create(account, COMPOSE_FORWARD);

	text = GTK_STEXT(compose->text);
	gtk_stext_freeze(text);
		
	for (msginfo = msginfo_list; msginfo != NULL; msginfo = msginfo->next) {
		msgfile = procmsg_get_message_file_path((MsgInfo *)msginfo->data);
		if (!is_file_exist(msgfile))
			g_warning("%s: file not exist\n", msgfile);
		else
			compose_attach_append(compose, msgfile, msgfile,
				"message/rfc822");
		g_free(msgfile);
	}

	if (account->auto_sig)
		compose_insert_sig(compose, FALSE);

	if (prefs_common.linewrap_quote)
		compose_wrap_line_all(compose);

	gtk_editable_set_position(GTK_EDITABLE(compose->text), 0);
	gtk_stext_set_point(GTK_STEXT(compose->text), 0);

	gtk_stext_thaw(text);
	gtk_widget_grab_focus(compose->header_last->entry);
	
#if 0 /* NEW COMPOSE GUI */
	if (account->protocol != A_NNTP)
		gtk_widget_grab_focus(compose->to_entry);
	else
		gtk_widget_grab_focus(compose->newsgroups_entry);
#endif

	return compose;
}

void compose_reedit(MsgInfo *msginfo)
{
	Compose *compose;
	PrefsAccount *account = NULL;
	GtkSText *text;
	FILE *fp;
	gchar buf[BUFFSIZE];
	gboolean use_signing = FALSE;
	gboolean use_encryption = FALSE;
	gchar *privacy_system = NULL;

	g_return_if_fail(msginfo != NULL);
	g_return_if_fail(msginfo->folder != NULL);

	if (compose_put_existing_to_front(msginfo)) 
		return;

        if (msginfo->folder->stype == F_QUEUE || msginfo->folder->stype == F_DRAFT) {
		gchar queueheader_buf[BUFFSIZE];
		gint id, param;

		/* Select Account from queue headers */
		if (!procheader_get_header_from_msginfo(msginfo, queueheader_buf, 
					     sizeof(queueheader_buf), "X-Sylpheed-Account-Id:")) {
			id = atoi(&queueheader_buf[strlen("X-Sylpheed-Account-Id:")]);
			account = account_find_from_id(id);
		}
		if (!account && !procheader_get_header_from_msginfo(msginfo, queueheader_buf, 
					     sizeof(queueheader_buf), "NAID:")) {
			id = atoi(&queueheader_buf[strlen("NAID:")]);
			account = account_find_from_id(id);
		}
		if (!account && !procheader_get_header_from_msginfo(msginfo, queueheader_buf, 
		                                    sizeof(queueheader_buf), "MAID:")) {
			id = atoi(&queueheader_buf[strlen("MAID:")]);
			account = account_find_from_id(id);
		}
		if (!account && !procheader_get_header_from_msginfo(msginfo, queueheader_buf, 
		                                                sizeof(queueheader_buf), "S:")) {
			account = account_find_from_address(queueheader_buf);
		}
		if (!procheader_get_header_from_msginfo(msginfo, queueheader_buf, 
					     sizeof(queueheader_buf), "X-Sylpheed-Sign:")) {
			param = atoi(&queueheader_buf[strlen("X-Sylpheed-Sign:")]);
			use_signing = param;
			
		}
		if (!procheader_get_header_from_msginfo(msginfo, queueheader_buf, 
					     sizeof(queueheader_buf), "X-Sylpheed-Encrypt:")) {
			param = atoi(&queueheader_buf[strlen("X-Sylpheed-Encrypt:")]);
			use_encryption = param;
		}
                if (!procheader_get_header_from_msginfo(msginfo, queueheader_buf, 
                                            sizeof(queueheader_buf), "X-Sylpheed-Privacy-System:")) {
                        privacy_system = g_strdup(&queueheader_buf[strlen("X-Sylpheed-Privacy-System:")]);
                }
		if (!procheader_get_header_from_msginfo(msginfo, queueheader_buf, 
					     sizeof(queueheader_buf), "X-Priority: ")) {
			param = atoi(&queueheader_buf[strlen("X-Priority: ")]); /* mind the space */
			compose->priority = param;
		}
	} else 
		account = msginfo->folder->folder->account;

	if (!account && prefs_common.reedit_account_autosel) {
               	gchar from[BUFFSIZE];
		if (!procheader_get_header_from_msginfo(msginfo, from, sizeof(from), "FROM:")){
		        extract_address(from);
		        account = account_find_from_address(from);
                }
	}
        if (!account) account = cur_account;
	g_return_if_fail(account != NULL);

	compose = compose_create(account, COMPOSE_REEDIT);
	compose->privacy_system = privacy_system;
	compose_use_signing(compose, use_signing);
	compose_use_encryption(compose, use_encryption);
	compose->targetinfo = procmsg_msginfo_copy(msginfo);

        if (msginfo->folder->stype == F_QUEUE
	||  msginfo->folder->stype == F_DRAFT) {
		gchar queueheader_buf[BUFFSIZE];

		/* Set message save folder */
		if (!procheader_get_header_from_msginfo(msginfo, queueheader_buf, sizeof(queueheader_buf), "SCF:")) {
			gint startpos = 0;

			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(compose->savemsg_checkbtn), TRUE);
			gtk_editable_delete_text(GTK_EDITABLE(compose->savemsg_entry), 0, -1);
			gtk_editable_insert_text(GTK_EDITABLE(compose->savemsg_entry), &queueheader_buf[4], strlen(&queueheader_buf[4]), &startpos);
		}
	}
	
	if (compose_parse_header(compose, msginfo) < 0) return;
	compose_reedit_set_entry(compose, msginfo);

	text = GTK_STEXT(compose->text);
	gtk_stext_freeze(text);

	if ((fp = procmime_get_first_text_content(msginfo)) == NULL)
		g_warning("Can't get text part\n");
	else {
		while (fgets(buf, sizeof(buf), fp) != NULL) {
			strcrchomp(buf);
			gtk_stext_insert(text, NULL, NULL, NULL, buf, -1);
		}
		fclose(fp);
	}
	compose_attach_parts(compose, msginfo);

	gtk_stext_thaw(text);
	gtk_widget_grab_focus(compose->text);

        if (prefs_common.auto_exteditor)
		compose_exec_ext_editor(compose);
}

Compose *compose_redirect(PrefsAccount *account, MsgInfo *msginfo)
{
	Compose *compose;
	gchar *filename;
	GtkItemFactory *ifactory;
	FolderItem *item;

	g_return_val_if_fail(msginfo != NULL, NULL);

	if (!account)
		account = account_get_reply_account(msginfo,
					prefs_common.reply_account_autosel);
	g_return_val_if_fail(account != NULL, NULL);

	compose = compose_create(account, COMPOSE_REDIRECT);
	ifactory = gtk_item_factory_from_widget(compose->menubar);

	compose->replyinfo = NULL;
	compose->fwdinfo = NULL;

	compose_show_first_last_header(compose, TRUE);

	gtk_widget_grab_focus(compose->header_last->entry);

	filename = procmsg_get_message_file_path(msginfo);
	if (filename == NULL)
		return NULL;

	compose->redirect_filename = filename;
	
	/* Set save folder */
	item = msginfo->folder;
	if (item && item->prefs && item->prefs->save_copy_to_folder) {
		gchar *folderidentifier;

    		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(compose->savemsg_checkbtn), prefs_common.savemsg);
		folderidentifier = folder_item_get_identifier(item);
		gtk_entry_set_text(GTK_ENTRY(compose->savemsg_entry), folderidentifier);
		g_free(folderidentifier);
	}

	compose_attach_parts(compose, msginfo);

	if (msginfo->subject)
		gtk_entry_set_text(GTK_ENTRY(compose->subject_entry),
				   msginfo->subject);
	gtk_editable_set_editable(GTK_EDITABLE(compose->subject_entry), FALSE);

	gtk_stext_freeze(GTK_STEXT(compose->text));
	compose_quote_fmt(compose, msginfo, "%M", NULL, NULL);
	gtk_editable_set_editable(GTK_EDITABLE(compose->text), FALSE);
	gtk_stext_thaw(GTK_STEXT(compose->text));

	ifactory = gtk_item_factory_from_widget(compose->popupmenu);
	menu_set_sensitive(ifactory, "/Add...", FALSE);
	menu_set_sensitive(ifactory, "/Remove", FALSE);
	menu_set_sensitive(ifactory, "/Properties...", FALSE);

	ifactory = gtk_item_factory_from_widget(compose->menubar);
	menu_set_sensitive(ifactory, "/Message/Save", FALSE);
	menu_set_sensitive(ifactory, "/Message/Insert file", FALSE);
	menu_set_sensitive(ifactory, "/Message/Attach file", FALSE);
	menu_set_sensitive(ifactory, "/Message/Insert signature", FALSE);
	menu_set_sensitive(ifactory, "/Edit", FALSE);
	menu_set_sensitive(ifactory, "/Options", FALSE);
	menu_set_sensitive(ifactory, "/Tools/Show ruler", FALSE);
	menu_set_sensitive(ifactory, "/Tools/Actions", FALSE);
	
	gtk_widget_set_sensitive(compose->toolbar->draft_btn, FALSE);
	gtk_widget_set_sensitive(compose->toolbar->insert_btn, FALSE);
	gtk_widget_set_sensitive(compose->toolbar->attach_btn, FALSE);
	gtk_widget_set_sensitive(compose->toolbar->sig_btn, FALSE);
	gtk_widget_set_sensitive(compose->toolbar->exteditor_btn, FALSE);
	gtk_widget_set_sensitive(compose->toolbar->linewrap_current_btn, FALSE);
	gtk_widget_set_sensitive(compose->toolbar->linewrap_all_btn, FALSE);

        return compose;
}

GList *compose_get_compose_list(void)
{
	return compose_list;
}

void compose_entry_append(Compose *compose, const gchar *address,
			  ComposeEntryType type)
{
	gchar *header;

	if (!address || *address == '\0') return;

#if 0 /* NEW COMPOSE GUI */
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
#endif

	switch (type) {
	case COMPOSE_CC:
		header = N_("Cc:");
		break;
	case COMPOSE_BCC:
		header = N_("Bcc:");
		break;
	case COMPOSE_REPLYTO:
		header = N_("Reply-To:");
		break;
	case COMPOSE_NEWSGROUPS:
		header = N_("Newsgroups:");
		break;
	case COMPOSE_FOLLOWUPTO:
		header = N_( "Followup-To:");
		break;
	case COMPOSE_TO:
	default:
		header = N_("To:");
		break;
	}
	header = prefs_common.trans_hdr ? gettext(header) : header;

	compose_add_header_entry(compose, header, (gchar *)address);
}

void compose_entry_mark_default_to(Compose *compose, const gchar *mailto)
{
	static GtkStyle *bold_style = NULL;
	static GdkColor bold_color;
	static GdkFont *bold_font = NULL;
	GSList *h_list;
	GtkEntry *entry;
		
	for (h_list = compose->header_list; h_list != NULL; h_list = h_list->next) {
		entry = GTK_ENTRY(((ComposeHeaderEntry *)h_list->data)->entry);
		if (gtk_entry_get_text(entry) && 
		    !g_strcasecmp(gtk_entry_get_text(entry), mailto)) {
			gtk_widget_ensure_style(GTK_WIDGET(entry));
			if (!bold_style) {
				gtkut_convert_int_to_gdk_color
					(prefs_common.color_new, &bold_color);
				bold_style = gtk_style_copy(gtk_widget_get_style
					(GTK_WIDGET(entry)));
				if (!bold_font)
					bold_font = gtkut_font_load
						(prefs_common.boldfont);
				if (bold_font)
					bold_style->font = bold_font;
				bold_style->fg[GTK_STATE_NORMAL] = bold_color;
			}
			gtk_widget_set_style(GTK_WIDGET(entry), bold_style);
		}
	}
}

void compose_toolbar_cb(gint action, gpointer data)
{
	ToolbarItem *toolbar_item = (ToolbarItem*)data;
	Compose *compose = (Compose*)toolbar_item->parent;
	
	g_return_if_fail(compose != NULL);

	switch(action) {
	case A_SEND:
		compose_send_cb(compose, 0, NULL);
		break;
	case A_SENDL:
		compose_send_later_cb(compose, 0, NULL);
		break;
	case A_DRAFT:
		compose_draft_cb(compose, COMPOSE_QUIT_EDITING, NULL);
		break;
	case A_INSERT:
		compose_insert_file_cb(compose, 0, NULL);
		break;
	case A_ATTACH:
		compose_attach_cb(compose, 0, NULL);
		break;
	case A_SIG:
		compose_insert_sig(compose, FALSE);
		break;
	case A_EXTEDITOR:
		compose_ext_editor_cb(compose, 0, NULL);
		break;
	case A_LINEWRAP_CURRENT:
		compose_wrap_line(compose);
		break;
	case A_LINEWRAP_ALL:
		compose_wrap_line_all(compose);
		break;
	case A_ADDRBOOK:
		compose_address_cb(compose, 0, NULL);
		break;
#ifdef USE_ASPELL
	case A_CHECK_SPELLING:
		compose_check_all(compose);
		break;
#endif
	default:
		break;
	}
}

static void compose_entries_set(Compose *compose, const gchar *mailto)
{
	gchar *to = NULL;
	gchar *cc = NULL;
	gchar *bcc = NULL;
	gchar *subject = NULL;
	gchar *body = NULL;

	scan_mailto_url(mailto, &to, &cc, &bcc, &subject, &body);

	if (to)
		compose_entry_append(compose, to, COMPOSE_TO);
	if (cc)
		compose_entry_append(compose, cc, COMPOSE_CC);
	if (bcc)
		compose_entry_append(compose, bcc, COMPOSE_BCC);
	if (subject)
		gtk_entry_set_text(GTK_ENTRY(compose->subject_entry), subject);
	if (body) {
		gtk_stext_insert(GTK_STEXT(compose->text),
				NULL, NULL, NULL, body, -1);
		gtk_stext_insert(GTK_STEXT(compose->text),
				NULL, NULL, NULL, "\n", 1);
	}

	g_free(to);
	g_free(cc);
	g_free(bcc);
	g_free(subject);
	g_free(body);
}

static gint compose_parse_header(Compose *compose, MsgInfo *msginfo)
{
	static HeaderEntry hentry[] = {{"Reply-To:",	NULL, TRUE},
				       {"Cc:",		NULL, TRUE},
				       {"References:",	NULL, FALSE},
				       {"Bcc:",		NULL, TRUE},
				       {"Newsgroups:",  NULL, TRUE},
				       {"Followup-To:", NULL, TRUE},
				       {"List-Post:",	NULL, FALSE},
				       {"X-Priority:",	NULL, FALSE},
				       {NULL,		NULL, FALSE}};

	enum
	{
		H_REPLY_TO	= 0,
		H_CC		= 1,
		H_REFERENCES	= 2,
		H_BCC		= 3,
		H_NEWSGROUPS	= 4,
		H_FOLLOWUP_TO	= 5,
		H_LIST_POST	= 6,
 		H_X_PRIORITY	= 7
	};

	FILE *fp;
	gchar buf[BUFFSIZE];

	g_return_val_if_fail(msginfo != NULL, -1);

	if ((fp = procmsg_open_message(msginfo)) == NULL) return -1;
	procheader_get_header_fields(fp, hentry);
	fclose(fp);

	if (hentry[H_REPLY_TO].body != NULL) {
		conv_unmime_header(buf, sizeof(buf), hentry[H_REPLY_TO].body,
				   NULL);
		compose->replyto = g_strdup(buf);
		g_free(hentry[H_REPLY_TO].body);
		hentry[H_REPLY_TO].body = NULL;
               /* if empty replyto, let sylpheed figure it out, i.e.
                 * neglect the reply-to header. */
                if (*(compose->replyto) == 0) {
                        g_free(compose->replyto);
                        compose->replyto = NULL;
                }
	}
	if (hentry[H_CC].body != NULL) {
		conv_unmime_header(buf, sizeof(buf), hentry[H_CC].body, NULL);
		compose->cc = g_strdup(buf);
		g_free(hentry[H_CC].body);
		hentry[H_CC].body = NULL;
	}
	if (hentry[H_REFERENCES].body != NULL) {
		if (compose->mode == COMPOSE_REEDIT)
			compose->references = hentry[H_REFERENCES].body;
		else {
			compose->references = compose_parse_references
				(hentry[H_REFERENCES].body, msginfo->msgid);
			g_free(hentry[H_REFERENCES].body);
		}
		hentry[H_REFERENCES].body = NULL;
	}
	if (hentry[H_BCC].body != NULL) {
		if (compose->mode == COMPOSE_REEDIT) {
			conv_unmime_header
				(buf, sizeof(buf), hentry[H_BCC].body, NULL);
			compose->bcc = g_strdup(buf);
		}
		g_free(hentry[H_BCC].body);
		hentry[H_BCC].body = NULL;
	}
	if (hentry[H_NEWSGROUPS].body != NULL) {
		compose->newsgroups = hentry[H_NEWSGROUPS].body;
		hentry[H_NEWSGROUPS].body = NULL;
	}
	if (hentry[H_FOLLOWUP_TO].body != NULL) {
		conv_unmime_header
			(buf, sizeof(buf), hentry[H_FOLLOWUP_TO].body, NULL);
		compose->followup_to = g_strdup(buf);
		g_free(hentry[H_FOLLOWUP_TO].body);
		hentry[H_FOLLOWUP_TO].body = NULL;
	}
	if (hentry[H_LIST_POST].body != NULL) {
		gchar *to = NULL;

		extract_address(hentry[H_LIST_POST].body);
		if (hentry[H_LIST_POST].body[0] != '\0') {
			scan_mailto_url(hentry[H_LIST_POST].body,
					&to, NULL, NULL, NULL, NULL);
			if (to) {
				g_free(compose->ml_post);
				compose->ml_post = to;
			}
		}
		g_free(hentry[H_LIST_POST].body);
		hentry[H_LIST_POST].body = NULL;
	}

	/* CLAWS - X-Priority */
	if (compose->mode == COMPOSE_REEDIT)
		if (hentry[H_X_PRIORITY].body != NULL) {
			gint priority;
			
			priority = atoi(hentry[H_X_PRIORITY].body);
			g_free(hentry[H_X_PRIORITY].body);
			
			hentry[H_X_PRIORITY].body = NULL;
			
			if (priority < PRIORITY_HIGHEST || 
			    priority > PRIORITY_LOWEST)
				priority = PRIORITY_NORMAL;
			
			compose->priority =  priority;
		}
 
	if (compose->mode == COMPOSE_REEDIT) {
		if (msginfo->inreplyto && *msginfo->inreplyto)
			compose->inreplyto = g_strdup(msginfo->inreplyto);
		return 0;
	}

	if (msginfo->msgid && *msginfo->msgid)
		compose->inreplyto = g_strdup(msginfo->msgid);

	if (!compose->references) {
		if (msginfo->msgid && *msginfo->msgid) {
			if (msginfo->inreplyto && *msginfo->inreplyto)
				compose->references =
					g_strdup_printf("<%s>\n\t<%s>",
							msginfo->inreplyto,
							msginfo->msgid);
			else
				compose->references =
					g_strconcat("<", msginfo->msgid, ">",
						    NULL);
		} else if (msginfo->inreplyto && *msginfo->inreplyto) {
			compose->references =
				g_strconcat("<", msginfo->inreplyto, ">",
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

static gchar *compose_quote_fmt(Compose *compose, MsgInfo *msginfo,
				const gchar *fmt, const gchar *qmark,
				const gchar *body)
{
	GtkSText *text = GTK_STEXT(compose->text);
	static MsgInfo dummyinfo;
	gchar *quote_str = NULL;
	gchar *buf;
	gchar *p, *lastp;
	gint len;
	const gchar *trimmed_body = body;
	
	if (!msginfo)
		msginfo = &dummyinfo;

	if (qmark != NULL) {
		quote_fmt_init(msginfo, NULL, NULL);
		quote_fmt_scan_string(qmark);
		quote_fmt_parse();

		buf = quote_fmt_get_buffer();
		if (buf == NULL)
			alertpanel_error(_("Quote mark format error."));
		else
			Xstrdup_a(quote_str, buf, return NULL)
	}

	if (fmt && *fmt != '\0') {
		while (trimmed_body && strlen(trimmed_body) > 1
			&& trimmed_body[0]=='\n')
			*trimmed_body++;

		quote_fmt_init(msginfo, quote_str, trimmed_body);
		quote_fmt_scan_string(fmt);
		quote_fmt_parse();

		buf = quote_fmt_get_buffer();
		if (buf == NULL) {
			alertpanel_error(_("Message reply/forward format error."));
			return NULL;
		}
	} else
		buf = "";

	gtk_stext_freeze(text);

	for (p = buf; *p != '\0'; ) {
		lastp = strchr(p, '\n');
		len = lastp ? lastp - p + 1 : -1;
		gtk_stext_insert(text, NULL, NULL, NULL, p, len);
		if (lastp)
			p = lastp + 1;
		else
			break;
	}

	gtk_stext_thaw(text);

	return buf;
}

static void compose_reply_set_entry(Compose *compose, MsgInfo *msginfo,
				    gboolean to_all, gboolean to_ml,
				    gboolean to_sender,
				    gboolean followup_and_reply_to)
{
	GSList *cc_list = NULL;
	GSList *cur;
	gchar *from = NULL;
	gchar *replyto = NULL;
	GHashTable *to_table;

	g_return_if_fail(compose->account != NULL);
	g_return_if_fail(msginfo != NULL);

	if (compose->account->protocol != A_NNTP) {
		if (!compose->replyto && to_ml && compose->ml_post
		    && !(msginfo->folder && msginfo->folder->prefs->enable_default_reply_to))
			compose_entry_append(compose,
					   compose->ml_post,
					   COMPOSE_TO);
		else if (!(to_all || to_sender)
			 && msginfo->folder
			 && msginfo->folder->prefs->enable_default_reply_to) {
			compose_entry_append(compose,
			    msginfo->folder->prefs->default_reply_to,
			    COMPOSE_TO);
		} else
			compose_entry_append(compose,
				 (compose->replyto && !to_sender)
				  ? compose->replyto :
				  msginfo->from ? msginfo->from : "",
				  COMPOSE_TO);
	} else {
		if (to_sender || (compose->followup_to && 
			!strncmp(compose->followup_to, "poster", 6)))
			compose_entry_append
				(compose, 
				 (compose->replyto ? compose->replyto :
		    		 	msginfo->from ? msginfo->from : ""),
				 COMPOSE_TO);
				 
		else if (followup_and_reply_to || to_all) {
			compose_entry_append
		    		(compose,
		    		 (compose->replyto ? compose->replyto :
		    		 msginfo->from ? msginfo->from : ""),
		    		 COMPOSE_TO);				
		
			compose_entry_append
				(compose,
			 	 compose->followup_to ? compose->followup_to :
			 	 compose->newsgroups ? compose->newsgroups : "",
			 	 COMPOSE_NEWSGROUPS);
		} 
		else 
			compose_entry_append
				(compose,
			 	 compose->followup_to ? compose->followup_to :
			 	 compose->newsgroups ? compose->newsgroups : "",
			 	 COMPOSE_NEWSGROUPS);
	}

	if (msginfo->subject && *msginfo->subject) {
		gchar *buf, *buf2;
		guchar *p;

		buf = p = g_strdup(msginfo->subject);
		p += subject_get_prefix_length(p);
		memmove(buf, p, strlen(p) + 1);

		buf2 = g_strdup_printf("Re: %s", buf);
		gtk_entry_set_text(GTK_ENTRY(compose->subject_entry), buf2);

		g_free(buf2);
		g_free(buf);
	} else
		gtk_entry_set_text(GTK_ENTRY(compose->subject_entry), "Re: ");

	if (to_ml && compose->ml_post) return;
	if (!to_all || compose->account->protocol == A_NNTP) return;

	if (compose->replyto) {
		Xstrdup_a(replyto, compose->replyto, return);
		extract_address(replyto);
	}
	if (msginfo->from) {
		Xstrdup_a(from, msginfo->from, return);
		extract_address(from);
	}

	if (replyto && from)
		cc_list = address_list_append_with_comments(cc_list, from);
	if (to_all && msginfo->folder && 
	    msginfo->folder->prefs->enable_default_reply_to)
	    	cc_list = address_list_append_with_comments(cc_list,
				msginfo->folder->prefs->default_reply_to);
	cc_list = address_list_append_with_comments(cc_list, msginfo->to);
	cc_list = address_list_append_with_comments(cc_list, compose->cc);

	to_table = g_hash_table_new(g_str_hash, g_str_equal);
	if (replyto)
		g_hash_table_insert(to_table, g_strdup(replyto), GINT_TO_POINTER(1));
	if (compose->account)
		g_hash_table_insert(to_table, g_strdup(compose->account->address),
				    GINT_TO_POINTER(1));

	/* remove address on To: and that of current account */
	for (cur = cc_list; cur != NULL; ) {
		GSList *next = cur->next;
		gchar *addr;

		addr = g_strdup(cur->data);
		extract_address(addr);

		if (GPOINTER_TO_INT(g_hash_table_lookup(to_table, addr)) == 1)
			cc_list = g_slist_remove(cc_list, cur->data);
		else
			g_hash_table_insert(to_table, addr, GINT_TO_POINTER(1));

		cur = next;
	}
	hash_free_strings(to_table);
	g_hash_table_destroy(to_table);

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

#define SET_ADDRESS(type, str) \
{ \
	if (str && *str) \
		compose_entry_append(compose, str, type); \
}

static void compose_reedit_set_entry(Compose *compose, MsgInfo *msginfo)
{
	g_return_if_fail(msginfo != NULL);

	SET_ENTRY(subject_entry, msginfo->subject);
	SET_ADDRESS(COMPOSE_TO, msginfo->to);
	SET_ADDRESS(COMPOSE_CC, compose->cc);
	SET_ADDRESS(COMPOSE_BCC, compose->bcc);
	SET_ADDRESS(COMPOSE_REPLYTO, compose->replyto);
	SET_ADDRESS(COMPOSE_NEWSGROUPS, compose->newsgroups);
	SET_ADDRESS(COMPOSE_FOLLOWUPTO, compose->followup_to);

	compose_update_priority_menu_item(compose);
	compose_update_privacy_system_menu_item(compose);
	compose_show_first_last_header(compose, TRUE);
}

#undef SET_ENTRY
#undef SET_ADDRESS

static void compose_insert_sig(Compose *compose, gboolean replace)
{
	GtkSText *text = GTK_STEXT(compose->text);
	gint cur_pos;
	gint len;

	g_return_if_fail(compose->account != NULL);

	cur_pos = gtk_editable_get_position(GTK_EDITABLE(text));

	gtk_stext_freeze(text);

	if (replace) {
		len = gtk_stext_get_length(text);
		gtk_stext_set_point(text, len);
	}

	if (replace && compose->sig_str) {
		gint pos;

		if (compose->sig_str[0] == '\0')
			pos = -1;
		else
			pos = gtkut_stext_find(text, 0, compose->sig_str, TRUE);

		if (pos != -1) {
			len = get_mbs_len(compose->sig_str);
			if (len >= 0) {
				gtk_stext_set_point(text, pos);
				gtk_stext_forward_delete(text, len);
			}
		}
	}

	g_free(compose->sig_str);
	compose->sig_str = compose_get_signature_str(compose);
	if (!compose->sig_str || (replace && !compose->account->auto_sig))
		compose->sig_str = g_strdup("");

	gtk_stext_insert(text, NULL, NULL, NULL, compose->sig_str, -1);

	gtk_stext_thaw(text);

	if (cur_pos > gtk_stext_get_length(text))
		cur_pos = gtk_stext_get_length(text);

	gtk_editable_set_position(GTK_EDITABLE(text), cur_pos);
	gtk_stext_set_point(text, cur_pos);
}

static gchar *compose_get_signature_str(Compose *compose)
{
	gchar *sig_body = NULL;
	gchar *sig_str = NULL;

	g_return_val_if_fail(compose->account != NULL, NULL);

	if (!compose->account->sig_path)
		return NULL;

	if (compose->account->sig_type == SIG_FILE) {
		if (!is_file_or_fifo_exist(compose->account->sig_path)) {
			g_warning("can't open signature file: %s\n",
				  compose->account->sig_path);
			return NULL;
		}
	}

	if (compose->account->sig_type == SIG_COMMAND)
		sig_body = get_command_output(compose->account->sig_path);
	else {
		gchar *tmp;

		tmp = file_read_to_str(compose->account->sig_path);
		if (!tmp)
			return NULL;
		sig_body = normalize_newlines(tmp);
		g_free(tmp);
	}

	if (compose->account->sig_sep)
		sig_str = g_strconcat("\n\n", compose->account->sig_sep, "\n", sig_body,
				      NULL);
	else
		sig_str = g_strconcat("\n\n", sig_body, NULL);

	g_free(sig_body);
	
	return sig_str;
}

static ComposeInsertResult compose_insert_file(Compose *compose, const gchar *file)
{
	GtkSText *text = GTK_STEXT(compose->text);
	gchar buf[BUFFSIZE];
	gint len;
	FILE *fp;
	gboolean badtxt = FALSE;

	g_return_val_if_fail(file != NULL, COMPOSE_INSERT_NO_FILE);

	if ((fp = fopen(file, "rb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		return COMPOSE_INSERT_READ_ERROR;
	}

	gtk_stext_freeze(text);

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		/* strip <CR> if DOS/Windows file,
		   replace <CR> with <LF> if Macintosh file. */
		strcrchomp(buf);
		len = strlen(buf);
		if (len > 0 && buf[len - 1] != '\n') {
			while (--len >= 0)
				if (buf[len] == '\r') buf[len] = '\n';
		}
		if (mbstowcs(NULL, buf, 0) == -1)
			badtxt = TRUE;
		gtk_stext_insert(text, NULL, NULL, NULL, buf, -1);
	}

	gtk_stext_thaw(text);

	fclose(fp);

	if (badtxt)
		return COMPOSE_INSERT_INVALID_CHARACTER;
	else 
		return COMPOSE_INSERT_SUCCESS;
}

static void compose_attach_append(Compose *compose, const gchar *file,
				  const gchar *filename,
				  const gchar *content_type)
{
	AttachInfo *ainfo;
	gchar *text[N_ATTACH_COLS];
	FILE *fp;
	off_t size;
	gint row;

	if (!is_file_exist(file)) {
		g_warning("File %s doesn't exist\n", file);
		return;
	}
	if ((size = get_file_size(file)) < 0) {
		g_warning("Can't get file size of %s\n", file);
		return;
	}
	if (size == 0) {
		alertpanel_notice(_("File %s is empty."), file);
		return;
	}
	if ((fp = fopen(file, "rb")) == NULL) {
		alertpanel_error(_("Can't read %s."), file);
		return;
	}
	fclose(fp);

#if 0 /* NEW COMPOSE GUI */
	if (!compose->use_attach) {
		GtkItemFactory *ifactory;

		ifactory = gtk_item_factory_from_widget(compose->menubar);
		menu_set_active(ifactory, "/View/Attachment", TRUE);
	}
#endif
	ainfo = g_new0(AttachInfo, 1);
	ainfo->file = g_strdup(file);

	if (content_type) {
		ainfo->content_type = g_strdup(content_type);
		if (!g_strcasecmp(content_type, "message/rfc822")) {
			MsgInfo *msginfo;
			MsgFlags flags = {0, 0};
			gchar *name;

			if (procmime_get_encoding_for_text_file(file) == ENC_7BIT)
				ainfo->encoding = ENC_7BIT;
			else
				ainfo->encoding = ENC_8BIT;

			msginfo = procheader_parse_file(file, flags, FALSE, FALSE);
			if (msginfo && msginfo->subject)
				name = msginfo->subject;
			else
				name = g_basename(filename ? filename : file);

			ainfo->name = g_strdup_printf(_("Message: %s"), name);

			procmsg_msginfo_free(msginfo);
		} else {
			if (!g_strncasecmp(content_type, "text", 4))
				ainfo->encoding = procmime_get_encoding_for_text_file(file);
			else
				ainfo->encoding = ENC_BASE64;
			ainfo->name = g_strdup
				(g_basename(filename ? filename : file));
		}
	} else {
		ainfo->content_type = procmime_get_mime_type(file);
		if (!ainfo->content_type) {
			ainfo->content_type =
				g_strdup("application/octet-stream");
			ainfo->encoding = ENC_BASE64;
		} else if (!g_strncasecmp(ainfo->content_type, "text", 4))
			ainfo->encoding =
				procmime_get_encoding_for_text_file(file);
		else
			ainfo->encoding = ENC_BASE64;
		ainfo->name = g_strdup(g_basename(filename ? filename : file));	
	}

	if (!strcmp(ainfo->content_type, "unknown")) {
		g_free(ainfo->content_type);
		ainfo->content_type = g_strdup("application/octet-stream");
	}

	ainfo->size = size;

	text[COL_MIMETYPE] = ainfo->content_type;
	text[COL_SIZE] = to_human_readable(size);
	text[COL_NAME] = ainfo->name;

	row = gtk_clist_append(GTK_CLIST(compose->attach_clist), text);
	gtk_clist_set_row_data(GTK_CLIST(compose->attach_clist), row, ainfo);
}

static void compose_use_signing(Compose *compose, gboolean use_signing)
{
	GtkItemFactory *ifactory;
	GtkWidget *menuitem = NULL;

	compose->use_signing = use_signing;
	ifactory = gtk_item_factory_from_widget(compose->menubar);
	menuitem = gtk_item_factory_get_item
		(ifactory, "/Options/Sign");
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem), 
				       use_signing);
}

static void compose_use_encryption(Compose *compose, gboolean use_encryption)
{
	GtkItemFactory *ifactory;
	GtkWidget *menuitem = NULL;

	compose->use_encryption = use_encryption;
	ifactory = gtk_item_factory_from_widget(compose->menubar);
	menuitem = gtk_item_factory_get_item
		(ifactory, "/Options/Encrypt");

	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem), 
				       use_encryption);
}

#define NEXT_PART_NOT_CHILD(info)  \
{  \
	node = info->node;  \
	while (node->children)  \
		node = g_node_last_child(node);  \
	info = procmime_mimeinfo_next((MimeInfo *)node->data);  \
}

static void compose_attach_parts(Compose *compose, MsgInfo *msginfo)
{
	MimeInfo *mimeinfo;
	MimeInfo *child;
	MimeInfo *firsttext = NULL;
	MimeInfo *encrypted = NULL;
	GNode    *node;
	gchar *outfile;
	const gchar *partname = NULL;

	mimeinfo = procmime_scan_message(msginfo);
	if (!mimeinfo) return;

	if (mimeinfo->node->children == NULL) {
		procmime_mimeinfo_free_all(mimeinfo);
		return;
	}

	/* find first content part */
	child = (MimeInfo *) mimeinfo->node->children->data;
	while (child && child->node->children && (child->type == MIMETYPE_MULTIPART))
		child = (MimeInfo *)child->node->children->data;

	if (child->type == MIMETYPE_TEXT) {
		firsttext = child;
		debug_print("First text part found\n");
	} else if (compose->mode == COMPOSE_REEDIT &&
		 child->type == MIMETYPE_APPLICATION &&
		 !g_strcasecmp(child->subtype, "pgp-encrypted")) {
		AlertValue val;
		val = alertpanel(_("Encrypted message"),
				 _("Cannot re-edit an encrypted message. \n"
				   "Discard encrypted part?"),
				 _("Yes"), _("No"), NULL);
		if (val == G_ALERTDEFAULT) 
			encrypted = (MimeInfo *)child->node->parent->data;
	}
     
	child = (MimeInfo *) mimeinfo->node->children->data;
	while (child != NULL) {
		if (child == encrypted) {
			/* skip this part of tree */
			NEXT_PART_NOT_CHILD(child);
			continue;
		}

		if (child->type == MIMETYPE_MULTIPART) {
			/* get the actual content */
			child = procmime_mimeinfo_next(child);
			continue;
		}
		    
		if (child == firsttext) {
			child = procmime_mimeinfo_next(child);
			continue;
		}

		outfile = procmime_get_tmp_file_name(child);
		if (procmime_get_part(outfile, child) < 0)
			g_warning("Can't get the part of multipart message.");
		else {
			gchar *content_type;

			content_type = procmime_get_content_type_str(child->type, child->subtype);
			partname = procmime_mimeinfo_get_parameter(child, "name");
			if (partname == NULL)
				partname = "";
			compose_attach_append(compose, outfile, 
					      partname, content_type);
			g_free(content_type);
		}
		g_free(outfile);
		NEXT_PART_NOT_CHILD(child);
	}
	procmime_mimeinfo_free_all(mimeinfo);
}

#undef NEXT_PART_NOT_CHILD

#define GET_CHAR(pos, buf, len)						     \
{									     \
	if (text->use_wchar)						     \
		len = wctomb(buf, (wchar_t)GTK_STEXT_INDEX(text, (pos)));    \
	else {								     \
		buf[0] = GTK_STEXT_INDEX(text, (pos));			     \
		len = 1;						     \
	}								     \
}

#define DISP_WIDTH(len) \
	((len > 2 && conv_get_current_charset() == C_UTF_8) ? 2 : \
	 (len == 2 && conv_get_current_charset() == C_UTF_8) ? 1 : len)

#define SPACE_CHARS	" \t"

static void compose_wrap_line(Compose *compose)
{
	GtkSText *text = GTK_STEXT(compose->text);
	gint ch_len, last_ch_len;
	gchar cbuf[MB_LEN_MAX], last_ch;
	guint text_len;
	guint line_end;
	guint quoted;
	gint p_start, p_end;
	gint line_pos, cur_pos;
	gint line_len, cur_len;

	gtk_stext_freeze(text);

	text_len = gtk_stext_get_length(text);

	/* check to see if the point is on the paragraph mark (empty line). */
	cur_pos = gtk_stext_get_point(text);
	GET_CHAR(cur_pos, cbuf, ch_len);
	if ((ch_len == 1 && *cbuf == '\n') || cur_pos == text_len) {
		if (cur_pos == 0)
			goto compose_end; /* on the paragraph mark */
		GET_CHAR(cur_pos - 1, cbuf, ch_len);
		if (ch_len == 1 && *cbuf == '\n')
			goto compose_end; /* on the paragraph mark */
	}

	/* find paragraph start. */
	line_end = quoted = 0;
	for (p_start = cur_pos; p_start >= 0; --p_start) {
		GET_CHAR(p_start, cbuf, ch_len);
		if (ch_len == 1 && *cbuf == '\n') {
			if (quoted)
				goto compose_end; /* quoted part */
			if (line_end) {
				p_start += 2;
				break;
			}
			line_end = 1;
		} else {
			if (ch_len == 1 
			    && strchr(prefs_common.quote_chars, *cbuf))
				quoted = 1;
			else if (ch_len != 1 || !isspace(*(guchar *)cbuf))
				quoted = 0;

			line_end = 0;
		}
	}
	if (p_start < 0)
		p_start = 0;

	/* find paragraph end. */
	line_end = 0;
	for (p_end = cur_pos; p_end < text_len; p_end++) {
		GET_CHAR(p_end, cbuf, ch_len);
		if (ch_len == 1 && *cbuf == '\n') {
			if (line_end) {
				p_end -= 1;
				break;
			}
			line_end = 1;
		} else {
			if (line_end && ch_len == 1 &&
			    strchr(prefs_common.quote_chars, *cbuf))
				goto compose_end; /* quoted part */

			line_end = 0;
		}
	}
	if (p_end >= text_len)
		p_end = text_len;

	if (p_start >= p_end)
		goto compose_end;

	line_len = cur_len = 0;
	last_ch_len = 0;
	last_ch = '\0';
	line_pos = p_start;
	for (cur_pos = p_start; cur_pos < p_end; cur_pos++) {
		guint space = 0;

		GET_CHAR(cur_pos, cbuf, ch_len);

		if (ch_len < 0) {
			cbuf[0] = '\0';
			ch_len = 1;
		}

		if (ch_len == 1 && isspace(*(guchar *)cbuf))
			space = 1;

		if (ch_len == 1 && *cbuf == '\n') {
			guint replace = 0;
			if (last_ch_len == 1 && !isspace((guchar)last_ch)) {
				if (cur_pos + 1 < p_end) {
					GET_CHAR(cur_pos + 1, cbuf, ch_len);
					if (ch_len == 1 &&
					    !isspace(*(guchar *)cbuf))
						replace = 1;
				}
			}
			gtk_stext_set_point(text, cur_pos + 1);
			gtk_stext_backward_delete(text, 1);
			if (replace) {
				gtk_stext_set_point(text, cur_pos);
				gtk_stext_insert(text, NULL, NULL, NULL, " ", 1);
				space = 1;
			}
			else {
				p_end--;
				cur_pos--;
				continue;
			}
		}

		last_ch_len = ch_len;
		last_ch = *cbuf;

		if (space) {
			line_pos = cur_pos + 1;
			line_len = cur_len + ch_len;
		}

		if (cur_len + DISP_WIDTH(ch_len) > prefs_common.linewrap_len &&
		    line_len > 0) {
			gint tlen = ch_len;

			GET_CHAR(line_pos - 1, cbuf, ch_len);
			if (ch_len == 1 && isspace(*(guchar *)cbuf)) {
				gtk_stext_set_point(text, line_pos);
				gtk_stext_backward_delete(text, 1);
				p_end--;
				cur_pos--;
				line_pos--;
				cur_len--;
				line_len--;
			}
			ch_len = tlen;

			gtk_stext_set_point(text, line_pos);
			gtk_stext_insert(text, NULL, NULL, NULL, "\n", 1);
			p_end++;
			cur_pos++;
			line_pos++;
			cur_len = cur_len - line_len + DISP_WIDTH(ch_len);
			line_len = 0;
			continue;
		}

		if (ch_len > 1) {
			line_pos = cur_pos + 1;
			line_len = cur_len + DISP_WIDTH(ch_len);
		}
		cur_len += DISP_WIDTH(ch_len);
	}

compose_end:
	gtk_stext_thaw(text);
}

#undef WRAP_DEBUG
#ifdef WRAP_DEBUG
/* Darko: used when I debug wrapping */
void dump_text(GtkSText *text, int pos, int tlen, int breakoncr)
{
	gint i, clen;
	gchar cbuf[MB_LEN_MAX];

	printf("%d [", pos);
	for (i = pos; i < tlen; i++) {
		GET_CHAR(i, cbuf, clen);
		if (clen < 0) break;
		if (breakoncr && clen == 1 && cbuf[0] == '\n')
			break;
		fwrite(cbuf, clen, 1, stdout);
	}
	printf("]\n");
}
#endif

typedef enum {
	WAIT_FOR_SPACE,
	WAIT_FOR_INDENT_CHAR,
	WAIT_FOR_INDENT_CHAR_OR_SPACE
} IndentState;

/* return indent length, we allow:
   > followed by spaces/tabs
   | followed by spaces/tabs
   uppercase characters immediately followed by >,
   and the repeating sequences of the above */
/* return indent length */
static guint get_indent_length(GtkSText *text, guint start_pos, guint text_len)
{
	guint i_len = 0;
	guint i, ch_len, alnum_cnt = 0;
	IndentState state = WAIT_FOR_INDENT_CHAR;
	gchar cbuf[MB_LEN_MAX];
	gboolean is_space;
	gboolean is_indent;

	if (prefs_common.quote_chars == NULL) {
		return 0 ;
	}

	for (i = start_pos; i < text_len; i++) {
		GET_CHAR(i, cbuf, ch_len);
		if (ch_len > 1)
			break;

		if (cbuf[0] == '\n')
			break;

		is_indent = strchr(prefs_common.quote_chars, cbuf[0]) ? TRUE : FALSE;
		is_space = strchr(SPACE_CHARS, cbuf[0]) ? TRUE : FALSE;

		switch (state) {
		case WAIT_FOR_SPACE:
			if (is_space == FALSE)
				goto out;
			state = WAIT_FOR_INDENT_CHAR_OR_SPACE;
			break;
		case WAIT_FOR_INDENT_CHAR_OR_SPACE:
			if (is_indent == FALSE && is_space == FALSE &&
			    !isupper((guchar)cbuf[0]))
				goto out;
			if (is_space == TRUE) {
				alnum_cnt = 0;
				state = WAIT_FOR_INDENT_CHAR_OR_SPACE;
			} else if (is_indent == TRUE) {
				alnum_cnt = 0;
				state = WAIT_FOR_SPACE;
			} else {
				alnum_cnt++;
				state = WAIT_FOR_INDENT_CHAR;
			}
			break;
		case WAIT_FOR_INDENT_CHAR:
			if (is_indent == FALSE && !isupper((guchar)cbuf[0]))
				goto out;
			if (is_indent == TRUE) {
				if (alnum_cnt > 0 
				    && !strchr(prefs_common.quote_chars, cbuf[0]))
					goto out;
				alnum_cnt = 0;
				state = WAIT_FOR_SPACE;
			} else {
				alnum_cnt++;
			}
			break;
		}

		i_len++;
	}

out:
	if ((i_len > 0) && (state == WAIT_FOR_INDENT_CHAR))
		i_len -= alnum_cnt;

	return i_len;
}

/* insert quotation string when line was wrapped */
static guint ins_quote(GtkSText *text, guint indent_len,
		       guint prev_line_pos, guint text_len,
		       gchar *quote_fmt)
{
	guint i, ins_len = 0;
	gchar ch;

	if (indent_len) {
		for (i = 0; i < indent_len; i++) {
			ch = GTK_STEXT_INDEX(text, prev_line_pos + i);
			gtk_stext_insert(text, NULL, NULL, NULL, &ch, 1);
		}
		ins_len = indent_len;
	}

	return ins_len;
}

/* check if we should join the next line */
static gboolean join_next_line(GtkSText *text, guint start_pos, guint tlen,
			       guint prev_ilen, gboolean autowrap)
{
	guint indent_len, ch_len;
	gboolean do_join = FALSE;
	gchar cbuf[MB_LEN_MAX];

	indent_len = get_indent_length(text, start_pos, tlen);

	if ((autowrap || indent_len > 0) && indent_len == prev_ilen) {
		GET_CHAR(start_pos + indent_len, cbuf, ch_len);
		if (ch_len > 0 && (cbuf[0] != '\n'))
			do_join = TRUE;
	}

	return do_join;
}

static void compose_wrap_line_all(Compose *compose)
{
	compose_wrap_line_all_full(compose, FALSE);
}

#define STEXT_FREEZE() \
	if (!frozen) { gtk_stext_freeze(text); frozen = TRUE; }

static void compose_wrap_line_all_full(Compose *compose, gboolean autowrap)
{
	GtkSText *text = GTK_STEXT(compose->text);
	guint tlen;
	guint line_pos = 0, cur_pos = 0, p_pos = 0;
	gint line_len = 0, cur_len = 0;
	gint ch_len;
	gboolean is_new_line = TRUE, do_delete = FALSE;
	guint i_len = 0;
	gboolean linewrap_quote = prefs_common.linewrap_quote;
	gboolean set_editable_pos = FALSE;
	gint editable_pos = 0;
	gboolean frozen = FALSE;
	guint linewrap_len = prefs_common.linewrap_len;
	gchar *qfmt = prefs_common.quotemark;
	gchar cbuf[MB_LEN_MAX];

	tlen = gtk_stext_get_length(text);

	for (; cur_pos < tlen; cur_pos++) {
		/* mark position of new line - needed for quotation wrap */
		if (is_new_line) {
			if (linewrap_quote)
				i_len = get_indent_length(text, cur_pos, tlen);

			is_new_line = FALSE;
			p_pos = cur_pos;
#ifdef WRAP_DEBUG
			g_print("new line i_len=%d p_pos=", i_len);
			dump_text(text, p_pos, tlen, 1);
#endif
		}

		GET_CHAR(cur_pos, cbuf, ch_len);

		/* fix line length for tabs */
		if (ch_len == 1 && *cbuf == '\t') {
			guint tab_width = text->default_tab_width;
			guint tab_offset = line_len % tab_width;

#ifdef WRAP_DEBUG
			g_print("found tab at pos=%d line_len=%d ", cur_pos,
				line_len);
#endif
			if (tab_offset) {
				line_len += tab_width - tab_offset - 1;
				cur_len = line_len;
			}
#ifdef WRAP_DEBUG
			printf("new_len=%d\n", line_len);
#endif
		}

		/* we have encountered line break */
		if (ch_len == 1 && *cbuf == '\n') {
			gint clen;
			gchar cb[MB_LEN_MAX];

			/* should we join the next line */
			if ((autowrap || i_len != cur_len) && do_delete &&
			    join_next_line
				(text, cur_pos + 1, tlen, i_len, autowrap))
				do_delete = TRUE;
			else
				do_delete = FALSE;

#ifdef WRAP_DEBUG
			g_print("found CR at %d do_del is %d next line is ",
			       cur_pos, do_delete);
			dump_text(text, cur_pos + 1, tlen, 1);
#endif

			/* skip delete if it is continuous URL */
			if (do_delete && (line_pos - p_pos <= i_len) &&
			    gtk_stext_is_uri_string(text, line_pos, tlen))
				do_delete = FALSE;

#ifdef WRAP_DEBUG
			g_print("l_len=%d wrap_len=%d do_del=%d\n",
				line_len, linewrap_len, do_delete);
#endif
			if (line_len < linewrap_len && do_delete) {
				STEXT_FREEZE();
				/* get rid of newline */
				gtk_stext_set_point(text, cur_pos);
				gtk_stext_forward_delete(text, 1);
				tlen--;

				/* if text starts with quote fmt or with
				   indent string, delete them */
				if (i_len) {
					guint ilen;
					ilen =  gtk_stext_str_compare_n
						(text, cur_pos, p_pos, i_len,
						 tlen);
					if (ilen) {
						gtk_stext_forward_delete
							(text, ilen);
						tlen -= ilen;
					}
				}

				GET_CHAR(cur_pos, cb, clen);
				/* insert space between the next line */
				if (cur_pos > 0) {
					gint clen_prev;
					gchar cb_prev[MB_LEN_MAX];

					GET_CHAR(cur_pos - 1, cb_prev,
						 clen_prev);
					if ((clen_prev != clen && clen > 1) ||
					    (clen == 1 &&
					     !isspace((guchar)cb[0]))) {
						gtk_stext_insert
							(text, NULL, NULL,
							 NULL, " ", 1);
						tlen++;
					}
				}

				/* and start over with current line */
				cur_pos = p_pos - 1;
				line_pos = cur_pos;
				line_len = cur_len = 0;
				do_delete = FALSE;
				is_new_line = TRUE;
#ifdef WRAP_DEBUG
				g_print("after delete l_pos=");
				dump_text(text, line_pos, tlen, 1);
#endif
				/* move beginning of line if we are on LF */
				GET_CHAR(line_pos, cb, clen);
				if (clen == 1 && *cb == '\n')
					line_pos++;
#ifdef WRAP_DEBUG
				g_print("new line_pos=%d\n", line_pos);
#endif

				continue;
			}

			/* mark new line beginning */
			line_pos = cur_pos + 1;
			line_len = cur_len = 0;
			do_delete = FALSE;
			is_new_line = TRUE;
			continue;
		}

		if (ch_len < 0) {
			cbuf[0] = '\0';
			ch_len = 1;
		}

		/* possible line break */
		if (ch_len == 1 && isspace(*(guchar *)cbuf)) {
			line_pos = cur_pos + 1;
			line_len = cur_len + ch_len;
		}

		/* are we over wrapping length set in preferences ? */
		if (cur_len + DISP_WIDTH(ch_len) > linewrap_len) {
			gint clen;

#ifdef WRAP_DEBUG
			g_print("should wrap cur_pos=%d ", cur_pos);
			dump_text(text, p_pos, tlen, 1);
			dump_text(text, line_pos, tlen, 1);
#endif
			/* force wrapping if it is one long word but not URL */
			if (line_pos - p_pos <= i_len)
                        	if (!gtk_stext_is_uri_string
				    (text, line_pos, tlen))
					line_pos = cur_pos - 1;
#ifdef WRAP_DEBUG
			g_print("new line_pos=%d\n", line_pos);
#endif

			GET_CHAR(line_pos - 1, cbuf, clen);

			/* if next character is space delete it */
			if (clen == 1 && isspace(*(guchar *)cbuf)) {
				if (p_pos + i_len != line_pos ||
                            	    !gtk_stext_is_uri_string
					(text, line_pos, tlen)) {
					STEXT_FREEZE();
					/* workaround for correct cursor
					   position */
					if (set_editable_pos == FALSE) {
						editable_pos = gtk_editable_get_position(GTK_EDITABLE(text));
						if (editable_pos == line_pos)
							set_editable_pos = TRUE;
					}
					gtk_stext_set_point(text, line_pos);
					gtk_stext_backward_delete(text, 1);
					tlen--;
					cur_pos--;
					line_pos--;
					cur_len--;
					line_len--;
				}
			}

			/* if it is URL at beginning of line don't wrap */
			if (p_pos + i_len == line_pos &&
			    gtk_stext_is_uri_string(text, line_pos, tlen)) {
#ifdef WRAP_DEBUG
				g_print("found URL at ");
				dump_text(text, line_pos, tlen, 1);
#endif
				continue;
			}

			/* insert CR */
			STEXT_FREEZE();
			gtk_stext_set_point(text, line_pos);
			gtk_stext_insert(text, NULL, NULL, NULL, "\n", 1);
			gtk_stext_compact_buffer(text);
			tlen++;
			line_pos++;
			/* for loop will increase it */
			cur_pos = line_pos - 1;
			/* start over with current line */
			is_new_line = TRUE;
			line_len = cur_len = 0;
			if (autowrap || i_len > 0)
				do_delete = TRUE;
			else
				do_delete = FALSE;
#ifdef WRAP_DEBUG
			g_print("after CR insert ");
			dump_text(text, line_pos, tlen, 1);
			dump_text(text, cur_pos, tlen, 1);
#endif

			/* should we insert quotation ? */
			if (linewrap_quote && i_len) {
				/* only if line is not already quoted  */
				if (!gtk_stext_str_compare
					(text, line_pos, tlen, qfmt)) {
					guint ins_len;

					if (line_pos - p_pos > i_len) {
						ins_len = ins_quote
							(text, i_len, p_pos,
							 tlen, qfmt);
						tlen += ins_len;
					}
#ifdef WRAP_DEBUG
					g_print("after quote insert ");
					dump_text(text, line_pos, tlen, 1);
#endif
				}
			}
			continue;
		}

		if (ch_len > 1) {
			line_pos = cur_pos + 1;
			line_len = cur_len + DISP_WIDTH(ch_len);
		}
		/* advance to next character in buffer */
		cur_len += DISP_WIDTH(ch_len);
	}

	if (frozen)
		gtk_stext_thaw(text);

	if (set_editable_pos && editable_pos <= tlen)
		gtk_editable_set_position(GTK_EDITABLE(text), editable_pos);
}

#undef STEXT_FREEZE
#undef GET_CHAR

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

static void compose_select_account(Compose *compose, PrefsAccount *account,
				   gboolean init)
{
	GtkItemFactory *ifactory;

	g_return_if_fail(account != NULL);

	compose->account = account;

	compose_set_title(compose);

	ifactory = gtk_item_factory_from_widget(compose->menubar);
#if 0
	if (account->protocol == A_NNTP) {
		gtk_widget_show(compose->newsgroups_hbox);
		gtk_widget_show(compose->newsgroups_entry);
		gtk_table_set_row_spacing(GTK_TABLE(compose->table), 2, 4);
		compose->use_newsgroups = TRUE;

		menu_set_active(ifactory, "/View/To", FALSE);
		menu_set_sensitive(ifactory, "/View/To", TRUE);
		menu_set_active(ifactory, "/View/Cc", FALSE);
		menu_set_sensitive(ifactory, "/View/Cc", TRUE);
		menu_set_sensitive(ifactory, "/View/Followup to", TRUE);
	} else {
		gtk_widget_hide(compose->newsgroups_hbox);
		gtk_widget_hide(compose->newsgroups_entry);
		gtk_table_set_row_spacing(GTK_TABLE(compose->table), 2, 0);
		gtk_widget_queue_resize(compose->table_vbox);
		compose->use_newsgroups = FALSE;

		menu_set_active(ifactory, "/View/To", TRUE);
		menu_set_sensitive(ifactory, "/View/To", FALSE);
		menu_set_active(ifactory, "/View/Cc", TRUE);
		menu_set_sensitive(ifactory, "/View/Cc", FALSE);
		menu_set_active(ifactory, "/View/Followup to", FALSE);
		menu_set_sensitive(ifactory, "/View/Followup to", FALSE);
	}

	if (account->set_autocc) {
		compose_entry_show(compose, COMPOSE_ENTRY_CC);
		if (account->auto_cc && compose->mode != COMPOSE_REEDIT)
			compose_entry_set(compose, account->auto_cc,
					  COMPOSE_ENTRY_CC);
	}
	if (account->set_autobcc) {
		compose_entry_show(compose, COMPOSE_ENTRY_BCC);
		if (account->auto_bcc && compose->mode != COMPOSE_REEDIT)
			compose_entry_set(compose, account->auto_bcc,
					  COMPOSE_ENTRY_BCC);
	}
	if (account->set_autoreplyto) {
		compose_entry_show(compose, COMPOSE_ENTRY_REPLY_TO);
		if (account->auto_replyto && compose->mode != COMPOSE_REEDIT)
			compose_entry_set(compose, account->auto_replyto,
					  COMPOSE_ENTRY_REPLY_TO);
	}

#endif

	if (account->default_sign && compose->mode != COMPOSE_REDIRECT)
		menu_set_active(ifactory, "/Options/Sign", TRUE);
	else
		menu_set_active(ifactory, "/Options/Sign", FALSE);
	if (account->default_encrypt && compose->mode != COMPOSE_REDIRECT)
		menu_set_active(ifactory, "/Options/Encrypt", TRUE);
	else
		menu_set_active(ifactory, "/Options/Encrypt", FALSE);
				       
	activate_privacy_system(compose, account);

	if (!init && compose->mode != COMPOSE_REDIRECT)
		compose_insert_sig(compose, TRUE);
}

gboolean compose_check_for_valid_recipient(Compose *compose) {
	gchar *recipient_headers_mail[] = {"To:", "Cc:", "Bcc:", NULL};
	gchar *recipient_headers_news[] = {"Newsgroups:", NULL};
	gboolean recipient_found = FALSE;
	GSList *list;
	gchar **strptr;

	/* free to and newsgroup list */
        slist_free_strings(compose->to_list);
	g_slist_free(compose->to_list);
	compose->to_list = NULL;
			
	slist_free_strings(compose->newsgroup_list);
        g_slist_free(compose->newsgroup_list);
        compose->newsgroup_list = NULL;

	/* search header entries for to and newsgroup entries */
	for (list = compose->header_list; list; list = list->next) {
		gchar *header;
		gchar *entry;
		header = gtk_editable_get_chars(GTK_EDITABLE(GTK_COMBO(((ComposeHeaderEntry *)list->data)->combo)->entry), 0, -1);
		entry = gtk_editable_get_chars(GTK_EDITABLE(((ComposeHeaderEntry *)list->data)->entry), 0, -1);
		g_strstrip(entry);
		if (entry[0] != '\0') {
			for (strptr = recipient_headers_mail; *strptr != NULL; strptr++) {
				if (!strcmp(header, (prefs_common.trans_hdr ? gettext(*strptr) : *strptr))) {
					compose->to_list = address_list_append(compose->to_list, entry);
					recipient_found = TRUE;
				}
			}
			for (strptr = recipient_headers_news; *strptr != NULL; strptr++) {
				if (!strcmp(header, (prefs_common.trans_hdr ? gettext(*strptr) : *strptr))) {
					compose->newsgroup_list = newsgroup_list_append(compose->newsgroup_list, entry);
					recipient_found = TRUE;
				}
			}
		}
		g_free(header);
		g_free(entry);
	}
	return recipient_found;
}

static gboolean compose_check_entries(Compose *compose, gboolean check_subject)
{
	gchar *str;

	if (compose_check_for_valid_recipient(compose) == FALSE) {
		alertpanel_error(_("Recipient is not specified."));
		return FALSE;
	}

	str = gtk_entry_get_text(GTK_ENTRY(compose->subject_entry));
	if (*str == '\0' && check_subject == TRUE) {
		AlertValue aval;

		aval = alertpanel(_("Send"),
				  _("Subject is empty. Send it anyway?"),
				  _("Yes"), _("No"), NULL);
		if (aval != G_ALERTDEFAULT)
			return FALSE;
	}

	return TRUE;
}

gint compose_send(Compose *compose)
{
	gint msgnum;
	FolderItem *folder;
	gint val = -1;
	gchar *msgpath;

	compose_allow_user_actions (compose, FALSE);
	compose->sending = TRUE;

	if (compose_check_entries(compose, TRUE) == FALSE)
		goto bail;

	val = compose_queue(compose, &msgnum, &folder);

	if (val) {
		alertpanel_error(_("Could not queue message for sending"));
		goto bail;
	}


	if (prefs_common.send_dialog_mode != SEND_DIALOG_ALWAYS) {
		compose->sending = FALSE;
		compose_close(compose);
		/* No more compose access in the normal codepath 
		 * after this point! */
	}

	if (msgnum == 0) {
		alertpanel_error(_("The message was queued but could not be "
				   "sent.\nUse \"Send queued messages\" from "
				   "the main window to retry."));
		if (prefs_common.send_dialog_mode == SEND_DIALOG_ALWAYS) {
			compose->sending = FALSE;
			compose_allow_user_actions (compose, TRUE);
		}
		return 0;
	}
	
	msgpath = folder_item_fetch_msg(folder, msgnum);
	val = procmsg_send_message_queue(msgpath);
	g_free(msgpath);

	if (prefs_common.send_dialog_mode == SEND_DIALOG_ALWAYS) {
		compose->sending = FALSE;
		compose_allow_user_actions (compose, TRUE);
		if (val != 0) {
			folder_item_remove_msg(folder, msgnum);
			folder_item_scan(folder);
		}
	}

	if (val == 0) {
		folder_item_remove_msg(folder, msgnum);
		folder_item_scan(folder);
		if (prefs_common.send_dialog_mode == SEND_DIALOG_ALWAYS)
			compose_close(compose);
	} else {
		alertpanel_error(_("The message was queued but could not be "
				   "sent.\nUse \"Send queued messages\" from "
				   "the main window to retry."));
		if (prefs_common.send_dialog_mode == SEND_DIALOG_ALWAYS) {
			compose_allow_user_actions (compose, TRUE);
			compose->sending = FALSE;		
		}
		return -1;
 	}

	return 0;

bail:
	compose_allow_user_actions (compose, TRUE);
	compose->sending = FALSE;

	return -1;
}

static gboolean compose_use_attach(Compose *compose) 
{
	return gtk_clist_get_row_data(GTK_CLIST(compose->attach_clist), 0) != NULL;
}

static gint compose_redirect_write_headers_from_headerlist(Compose *compose, 
							   FILE *fp)
{
	gchar buf[BUFFSIZE];
	gchar *str;
	gboolean first_to_address;
	gboolean first_cc_address;
	GSList *list;
	ComposeHeaderEntry *headerentry;
	gchar *headerentryname;
	gchar *cc_hdr;
	gchar *to_hdr;

	debug_print("Writing redirect header\n");

	cc_hdr = prefs_common.trans_hdr ? _("Cc:") : "Cc:";
 	to_hdr = prefs_common.trans_hdr ? _("To:") : "To:";

	first_to_address = TRUE;
	first_cc_address = TRUE;
	for (list = compose->header_list; list; list = list->next) {
		headerentry = ((ComposeHeaderEntry *)list->data);
		headerentryname = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(headerentry->combo)->entry));

		if (g_strcasecmp(headerentryname, cc_hdr) == 0 
		   || g_strcasecmp(headerentryname, to_hdr) == 0) {
			str = gtk_entry_get_text(GTK_ENTRY(headerentry->entry));
			Xstrdup_a(str, str, return -1);
			g_strstrip(str);
			if (str[0] != '\0') {
				compose_convert_header
					(buf, sizeof(buf), str,
					strlen("Resent-To") + 2, TRUE);
				if (g_strcasecmp(headerentryname, to_hdr) == 0) {
					if (first_to_address) {
						fprintf(fp, "Resent-To: ");
						first_to_address = FALSE;
					} else {
						fprintf(fp, ",");
					}
				}
				if (g_strcasecmp(headerentryname, cc_hdr) == 0) {
					if (first_cc_address) {
						fprintf(fp, "\n");
						fprintf(fp, "Resent-Cc: ");
						first_cc_address = FALSE;
					} else {
						fprintf(fp, ",");
					}
				}
				
				fprintf(fp, "%s", buf);
			}
		}
	}
	/* if (!first_address) { */
	fprintf(fp, "\n");
	/* } */

	return(0);
}

static gint compose_redirect_write_headers(Compose *compose, FILE *fp)
{
	gchar buf[BUFFSIZE];
	gchar *str;
	/* struct utsname utsbuf; */

	g_return_val_if_fail(fp != NULL, -1);
	g_return_val_if_fail(compose->account != NULL, -1);
	g_return_val_if_fail(compose->account->address != NULL, -1);

	/* Resent-Date */
	get_rfc822_date(buf, sizeof(buf));
	fprintf(fp, "Resent-Date: %s\n", buf);

	/* Resent-From */
	if (compose->account->name && *compose->account->name) {
		compose_convert_header
			(buf, sizeof(buf), compose->account->name,
			 strlen("From: "), TRUE);
		fprintf(fp, "Resent-From: %s <%s>\n",
			buf, compose->account->address);
	} else
		fprintf(fp, "Resent-From: %s\n", compose->account->address);

	/* Subject */
	str = gtk_entry_get_text(GTK_ENTRY(compose->subject_entry));
	if (*str != '\0') {
		Xstrdup_a(str, str, return -1);
		g_strstrip(str);
		if (*str != '\0') {
			compose_convert_header(buf, sizeof(buf), str,
					       strlen("Subject: "), FALSE);
			fprintf(fp, "Subject: %s\n", buf);
		}
	}

	/* Resent-Message-ID */
	if (compose->account->gen_msgid) {
		generate_msgid(buf, sizeof(buf));
		fprintf(fp, "Resent-Message-ID: <%s>\n", buf);
		compose->msgid = g_strdup(buf);
	}

	compose_redirect_write_headers_from_headerlist(compose, fp);

	/* separator between header and body */
	fputs("\n", fp);

	return 0;
}

static gint compose_redirect_write_to_file(Compose *compose, FILE *fdest)
{
	FILE *fp;
	size_t len;
	gchar buf[BUFFSIZE];

	if ((fp = fopen(compose->redirect_filename, "rb")) == NULL) {
		FILE_OP_ERROR(compose->redirect_filename, "fopen");
		return -1;
	}

	while (procheader_get_one_field_asis(buf, sizeof(buf), fp) != -1) {
		/* should filter returnpath, delivered-to */
		if (g_strncasecmp(buf, "Return-Path:",
				   strlen("Return-Path:")) == 0 ||
		    g_strncasecmp(buf, "Delivered-To:",
				  strlen("Delivered-To:")) == 0 ||
		    g_strncasecmp(buf, "Received:",
				  strlen("Received:")) == 0 ||
		    g_strncasecmp(buf, "Subject:",
				  strlen("Subject:")) == 0 ||
		    g_strncasecmp(buf, "X-UIDL:",
				  strlen("X-UIDL:")) == 0)
			continue;

		if (fputs(buf, fdest) == -1)
			goto error;

		if (!prefs_common.redirect_keep_from) {
			if (g_strncasecmp(buf, "From:",
					  strlen("From:")) == 0) {
				fputs(" (by way of ", fdest);
				if (compose->account->name
				    && *compose->account->name) {
					compose_convert_header
						(buf, sizeof(buf),
						 compose->account->name,
						 strlen("From: "),
						 FALSE);
					fprintf(fdest, "%s <%s>",
						buf,
						compose->account->address);
				} else
					fprintf(fdest, "%s",
						compose->account->address);
				fputs(")", fdest);
			}
		}

		if (fputs("\n", fdest) == -1)
			goto error;
	}

	compose_redirect_write_headers(compose, fdest);

	while ((len = fread(buf, sizeof(gchar), sizeof(buf), fp)) > 0) {
		if (fwrite(buf, sizeof(gchar), len, fdest) != len)
			goto error;
	}

	fclose(fp);

	return 0;
error:
	fclose(fp);

	return -1;
}

static gint compose_write_to_file(Compose *compose, FILE *fp, gint action)
{
	gchar *chars;
	gchar *buf;
	const gchar *out_codeset;
	EncodingType encoding;
	MimeInfo *mimemsg, *mimetext;

	/* create message MimeInfo */
	mimemsg = procmime_mimeinfo_new();
        mimemsg->type = MIMETYPE_MESSAGE;
        mimemsg->subtype = g_strdup("rfc822");
	mimemsg->content = MIMECONTENT_MEM;
	mimemsg->data.mem = compose_get_header(compose);

	/* Create text part MimeInfo */
	/* get all composed text */
	chars = gtk_editable_get_chars(GTK_EDITABLE(compose->text), 0, -1);
	if (is_ascii_str(chars)) {
		buf = chars;
		chars = NULL;
		out_codeset = CS_US_ASCII;
		encoding = ENC_7BIT;
	} else {
		const gchar *src_codeset;

		out_codeset = conv_get_outgoing_charset_str();
		if (!g_strcasecmp(out_codeset, CS_US_ASCII))
			out_codeset = CS_ISO_8859_1;

		if (prefs_common.encoding_method == CTE_BASE64)
			encoding = ENC_BASE64;
		else if (prefs_common.encoding_method == CTE_QUOTED_PRINTABLE)
			encoding = ENC_QUOTED_PRINTABLE;
		else if (prefs_common.encoding_method == CTE_8BIT)
			encoding = ENC_8BIT;
		else
			encoding = procmime_get_encoding_for_charset(out_codeset);

		src_codeset = conv_get_current_charset_str();
		/* if current encoding is US-ASCII, set it the same as
		   outgoing one to prevent code conversion failure */
		if (!g_strcasecmp(src_codeset, CS_US_ASCII))
			src_codeset = out_codeset;

		debug_print("src encoding = %s, out encoding = %s, transfer encoding = %s\n",
			    src_codeset, out_codeset, procmime_get_encoding_str(encoding));

		if (action == COMPOSE_WRITE_FOR_SEND) {
			buf = conv_codeset_strdup(chars, src_codeset, out_codeset);
			
			if (!buf) {
				AlertValue aval;
				gchar *msg;

				msg = g_strdup_printf(_("Can't convert the character encoding of the message from\n"
							"%s to %s.\n"
							"Send it anyway?"), src_codeset, out_codeset);
				aval = alertpanel_with_type
					(_("Error"), msg, _("Yes"), _("+No"), NULL, NULL, ALERT_ERROR);
				g_free(msg);

				if (aval != G_ALERTDEFAULT) {
					g_free(chars);
					return -1;
				} else {
					buf = chars;
					out_codeset = src_codeset;
					chars = NULL;
				}
			}
		} else {
			buf = chars;
			out_codeset = src_codeset;
			chars = NULL;
 		}
	}
	g_free(chars);

	if (encoding == ENC_8BIT || encoding == ENC_7BIT) {
		if (!strncmp(buf, "From ", sizeof("From ")-1) ||
		    strstr(buf, "\nFrom ") != NULL) {
			encoding = ENC_QUOTED_PRINTABLE;
		}
	}

	mimetext = procmime_mimeinfo_new();
	mimetext->content = MIMECONTENT_MEM;
	mimetext->data.mem = buf;
	mimetext->type = MIMETYPE_TEXT;
	mimetext->subtype = g_strdup("plain");
	g_hash_table_insert(mimetext->typeparameters, g_strdup("charset"),
			    g_strdup(out_codeset));
	/* protect trailing spaces when signing message */
	if (action == COMPOSE_WRITE_FOR_SEND && compose->use_signing && 
	    privacy_system_can_sign(compose->privacy_system))
	    	if (encoding == ENC_7BIT)
			encoding = ENC_QUOTED_PRINTABLE;
		else if (encoding == ENC_8BIT)
			encoding = ENC_BASE64;
	if (encoding != ENC_UNKNOWN)
		procmime_encode_content(mimetext, encoding);

	/* append attachment parts */
	if (compose_use_attach(compose)) {
		MimeInfo *mimempart;

		mimempart = procmime_mimeinfo_new();
    		mimempart->content = MIMECONTENT_EMPTY;
    		mimempart->type = MIMETYPE_MULTIPART;
	        mimempart->subtype = g_strdup("mixed");
    		g_hash_table_insert(mimempart->typeparameters, g_strdup("boundary"),
				    generate_mime_boundary(NULL));

		mimetext->disposition = DISPOSITIONTYPE_INLINE;

		g_node_append(mimempart->node, mimetext->node);
		g_node_append(mimemsg->node, mimempart->node);

		compose_add_attachments(compose, mimempart);
	} else
		g_node_append(mimemsg->node, mimetext->node);

	/* sign message if sending */
	if (action == COMPOSE_WRITE_FOR_SEND && compose->use_signing && 
	    privacy_system_can_sign(compose->privacy_system))
		if (!privacy_sign(compose->privacy_system, mimemsg, compose->account))
			return -1;

	procmime_write_mimeinfo(mimemsg, fp);
	
	procmime_mimeinfo_free_all(mimemsg);

	return 0;
}

static gint compose_write_body_to_file(Compose *compose, const gchar *file)
{
	FILE *fp;
	size_t len;
	gchar *chars;

	if ((fp = fopen(file, "wb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		return -1;
	}

	/* chmod for security */
	if (change_file_mode_rw(fp, file) < 0) {
		FILE_OP_ERROR(file, "chmod");
		g_warning("can't change file mode\n");
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

static gint compose_remove_reedit_target(Compose *compose)
{
	FolderItem *item;
	MsgInfo *msginfo = compose->targetinfo;

	g_return_val_if_fail(compose->mode == COMPOSE_REEDIT, -1);
	if (!msginfo) return -1;

	item = msginfo->folder;
	g_return_val_if_fail(item != NULL, -1);

	if (procmsg_msg_exist(msginfo) &&
	    (item->stype == F_DRAFT || item->stype == F_QUEUE 
	     || msginfo == compose->autosaved_draft)) {
		if (folder_item_remove_msg(item, msginfo->msgnum) < 0) {
			g_warning("can't remove the old message\n");
			return -1;
		}
	}

	return 0;
}

void compose_remove_draft(Compose *compose)
{
	FolderItem *drafts;
	MsgInfo *msginfo = compose->targetinfo;
	drafts = account_get_special_folder(compose->account, F_DRAFT);

	if (procmsg_msg_exist(msginfo)) {
		folder_item_remove_msg(drafts, msginfo->msgnum);
	}

}

static gint compose_queue(Compose *compose, gint *msgnum, FolderItem **item)
{
	return compose_queue_sub (compose, msgnum, item, FALSE);
}
static gint compose_queue_sub(Compose *compose, gint *msgnum, FolderItem **item, gboolean check_subject)
{
	FolderItem *queue;
	gchar *tmp;
	FILE *fp;
	GSList *cur;
	gint num;
        static gboolean lock = FALSE;
	PrefsAccount *mailac = NULL, *newsac = NULL;
	
	debug_print("queueing message...\n");
	g_return_val_if_fail(compose->account != NULL, -1);

        lock = TRUE;
	
	if (compose_check_entries(compose, check_subject) == FALSE) {
                lock = FALSE;
                return -1;
	}

	if (!compose->to_list && !compose->newsgroup_list) {
	        g_warning("can't get recipient list.");
	        lock = FALSE;
                return -1;
        }

	if (compose->to_list) {
    		if (compose->account->protocol != A_NNTP)
            		mailac = compose->account;
		else if (cur_account && cur_account->protocol != A_NNTP)
	    		mailac = cur_account;
		else if (!(mailac = compose_current_mail_account())) {
			lock = FALSE;
			alertpanel_error(_("No account for sending mails available!"));
			return -1;
		}
	}

	if (compose->newsgroup_list) {
                if (compose->account->protocol == A_NNTP)
                        newsac = compose->account;
                else if (!newsac->protocol != A_NNTP) {
			lock = FALSE;
			alertpanel_error(_("No account for posting news available!"));
			return -1;
		}			
	}

        if (prefs_common.linewrap_at_send)
    		compose_wrap_line_all(compose);

	/* write queue header */
	tmp = g_strdup_printf("%s%cqueue.%p", get_tmp_dir(),
			      G_DIR_SEPARATOR, compose);
	if ((fp = fopen(tmp, "wb")) == NULL) {
		FILE_OP_ERROR(tmp, "fopen");
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
	if (compose->msgid)
		fprintf(fp, "MID:<%s>\n", compose->msgid);
	else
		fprintf(fp, "MID:\n");
	fprintf(fp, "CFG:\n");
	fprintf(fp, "PT:0\n");
	fprintf(fp, "S:%s\n", compose->account->address);
	fprintf(fp, "RQ:\n");
	if (mailac)
		fprintf(fp, "SSV:%s\n", mailac->smtp_server);
	else
		fprintf(fp, "SSV:\n");
	if (newsac)
		fprintf(fp, "NSV:%s\n", newsac->nntp_server);
	else
		fprintf(fp, "NSV:\n");
	fprintf(fp, "SSH:\n");
	/* write recepient list */
	if (compose->to_list) {
		fprintf(fp, "R:<%s>", (gchar *)compose->to_list->data);
		for (cur = compose->to_list->next; cur != NULL;
		     cur = cur->next)
			fprintf(fp, ",<%s>", (gchar *)cur->data);
		fprintf(fp, "\n");
	}
	/* write newsgroup list */
	if (compose->newsgroup_list) {
		fprintf(fp, "NG:");
		fprintf(fp, "%s", (gchar *)compose->newsgroup_list->data);
		for (cur = compose->newsgroup_list->next; cur != NULL; cur = cur->next)
			fprintf(fp, ",%s", (gchar *)cur->data);
		fprintf(fp, "\n");
	}
	/* Sylpheed account IDs */
	if (mailac)
		fprintf(fp, "MAID:%d\n", mailac->account_id);
	if (newsac)
		fprintf(fp, "NAID:%d\n", newsac->account_id);

	if (compose->privacy_system != NULL) {
		fprintf(fp, "X-Sylpheed-Privacy-System:%s\n", compose->privacy_system);
		fprintf(fp, "X-Sylpheed-Sign:%d\n", compose->use_signing);
		fprintf(fp, "X-Sylpheed-Encrypt:%d\n", compose->use_encryption);
		if (compose->use_encryption) {
			gchar *encdata;

			encdata = privacy_get_encrypt_data(compose->privacy_system, compose->to_list);
			if (encdata != NULL)
				fprintf(fp, "X-Sylpheed-Encrypt-Data:%s\n", 
					encdata);
			g_free(encdata);
		}
	}

	/* Save copy folder */
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(compose->savemsg_checkbtn))) {
		gchar *savefolderid;
		
		savefolderid = gtk_editable_get_chars(GTK_EDITABLE(compose->savemsg_entry), 0, -1);
		fprintf(fp, "SCF:%s\n", savefolderid);
		g_free(savefolderid);
	}
	/* Message-ID of message replying to */
	if ((compose->replyinfo != NULL) && (compose->replyinfo->msgid != NULL)) {
		gchar *folderid;
		
		folderid = folder_item_get_identifier(compose->replyinfo->folder);
		fprintf(fp, "RMID:%s\x7f%d\x7f%s\n", folderid, compose->replyinfo->msgnum, compose->replyinfo->msgid);
		g_free(folderid);
	}
	/* Message-ID of message forwarding to */
	if ((compose->fwdinfo != NULL) && (compose->fwdinfo->msgid != NULL)) {
		gchar *folderid;
		
		folderid = folder_item_get_identifier(compose->fwdinfo->folder);
		fprintf(fp, "FMID:%s\x7f%d\x7f%s\n", folderid, compose->fwdinfo->msgnum, compose->fwdinfo->msgid);
		g_free(folderid);
	}
	fprintf(fp, "\n");

	if (compose->redirect_filename != NULL) {
		if (compose_redirect_write_to_file(compose, fp) < 0) {
			lock = FALSE;
			fclose(fp);
			unlink(tmp);
			g_free(tmp);
			return -1;
		}
	} else {
		if (compose_write_to_file(compose, fp, COMPOSE_WRITE_FOR_SEND) < 0) {
			lock = FALSE;
			fclose(fp);
			unlink(tmp);
			g_free(tmp);
			return -1;
		}
	}

	if (fclose(fp) == EOF) {
		FILE_OP_ERROR(tmp, "fclose");
		unlink(tmp);
		g_free(tmp);
		return -1;
	}

	queue = account_get_special_folder(compose->account, F_QUEUE);
	if (!queue) {
		g_warning("can't find queue folder\n");
		unlink(tmp);
		g_free(tmp);
		return -1;
	}
	folder_item_scan(queue);
	if ((num = folder_item_add_msg(queue, tmp, NULL, TRUE)) < 0) {
		g_warning("can't queue the message\n");
		unlink(tmp);
		g_free(tmp);
		return -1;
	}
	unlink(tmp);
	g_free(tmp);

	if (compose->mode == COMPOSE_REEDIT) {
		compose_remove_reedit_target(compose);
	}

	if ((msgnum != NULL) && (item != NULL)) {
		*msgnum = num;
		*item = queue;
	}

	return 0;
}

static void compose_add_attachments(Compose *compose, MimeInfo *parent)
{
	AttachInfo *ainfo;
	GtkCList *clist = GTK_CLIST(compose->attach_clist);
	gint row;
	MimeInfo *mimepart;
	struct stat statbuf;
	gchar *type, *subtype;

	for (row = 0; (ainfo = gtk_clist_get_row_data(clist, row)) != NULL;
	     row++) {
		mimepart = procmime_mimeinfo_new();
		mimepart->content = MIMECONTENT_FILE;
		mimepart->data.filename = g_strdup(ainfo->file);
		mimepart->offset = 0;

		stat(ainfo->file, &statbuf);
		mimepart->length = statbuf.st_size;

    		type = g_strdup(ainfo->content_type);

		if (!strchr(type, '/')) {
			g_free(type);
			type = g_strdup("application/octet-stream");
		}

    		subtype = strchr(type, '/') + 1;
	        *(subtype - 1) = '\0';
    		mimepart->type = procmime_get_media_type(type);
    		mimepart->subtype = g_strdup(subtype);
    		g_free(type);

		if (mimepart->type == MIMETYPE_MESSAGE && 
		    !g_strcasecmp(mimepart->subtype, "rfc822")) {
			mimepart->disposition = DISPOSITIONTYPE_INLINE;
		} else {
			g_hash_table_insert(mimepart->typeparameters,
					    g_strdup("name"), g_strdup(ainfo->name));
			g_hash_table_insert(mimepart->dispositionparameters,
					    g_strdup("filename"), g_strdup(ainfo->name));
			mimepart->disposition = DISPOSITIONTYPE_ATTACHMENT;
		}

		if (compose->use_signing) {
			if (ainfo->encoding == ENC_7BIT)
				ainfo->encoding = ENC_QUOTED_PRINTABLE;
			else if (ainfo->encoding == ENC_8BIT)
				ainfo->encoding = ENC_BASE64;
		}
		
		procmime_encode_content(mimepart, ainfo->encoding);

		g_node_append(parent->node, mimepart->node);
	}
}

#define QUOTE_IF_REQUIRED(out, str)					\
{									\
	if (*str != '"' && strpbrk(str, ",.[]<>")) {			\
		gchar *__tmp;						\
		gint len;						\
									\
		len = strlen(str) + 3;					\
		if ((__tmp = alloca(len)) == NULL) {			\
			g_warning("can't allocate memory\n");		\
			g_string_free(header, TRUE);			\
			return NULL;					\
		}							\
		g_snprintf(__tmp, len, "\"%s\"", str);			\
		out = __tmp;						\
	} else {							\
		gchar *__tmp;						\
									\
		if ((__tmp = alloca(strlen(str) + 1)) == NULL) {	\
			g_warning("can't allocate memory\n");		\
			g_string_free(header, TRUE);			\
			return NULL;					\
		} else 							\
			strcpy(__tmp, str);				\
									\
		out = __tmp;						\
	}								\
}

#define IS_IN_CUSTOM_HEADER(header) \
	(compose->account->add_customhdr && \
	 custom_header_find(compose->account->customhdr_list, header) != NULL)

static void compose_add_headerfield_from_headerlist(Compose *compose, 
						    GString *header, 
					            const gchar *fieldname,
					            const gchar *seperator)
{
	gchar *str, *fieldname_w_colon, *trans_fieldname;
	gboolean add_field = FALSE;
	GSList *list;
	ComposeHeaderEntry *headerentry;
	gchar * headerentryname;
	GString *fieldstr;

	if (IS_IN_CUSTOM_HEADER(fieldname))
		return;

	debug_print("Adding %s-fields\n", fieldname);

	fieldstr = g_string_sized_new(64);

	fieldname_w_colon = g_strconcat(fieldname, ":", NULL);
	trans_fieldname = (prefs_common.trans_hdr ? gettext(fieldname_w_colon) : fieldname_w_colon);

	for (list = compose->header_list; list; list = list->next) {
    		headerentry = ((ComposeHeaderEntry *)list->data);
		headerentryname = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(headerentry->combo)->entry));

		if (!g_strcasecmp(trans_fieldname, headerentryname)) {
			str = gtk_editable_get_chars(GTK_EDITABLE(headerentry->entry), 0, -1);
			g_strstrip(str);
			if (str[0] != '\0') {
				if (add_field)
					g_string_append(fieldstr, seperator);
				g_string_append(fieldstr, str);
				add_field = TRUE;
			}
			g_free(str);
		}
	}
	if (add_field) {
		gchar *buf;

		buf = g_new0(gchar, fieldstr->len * 4 + 256);
		compose_convert_header
			(buf, fieldstr->len * 4  + 256, fieldstr->str,
			strlen(fieldname) + 2, TRUE);
		g_string_sprintfa(header, "%s: %s\n", fieldname, buf);
		g_free(buf);
	}

	g_free(fieldname_w_colon);
	g_string_free(fieldstr, TRUE);

	return;
}

static gchar *compose_get_header(Compose *compose)
{
	gchar buf[BUFFSIZE];
	gchar *str;
	gchar *name;
	GSList *list;
	gchar *std_headers[] = {"To:", "Cc:", "Bcc:", "Newsgroups:", "Reply-To:", "Followup-To:", NULL};
	GString *header;

	/* struct utsname utsbuf; */

	g_return_val_if_fail(compose->account != NULL, NULL);
	g_return_val_if_fail(compose->account->address != NULL, NULL);

	header = g_string_sized_new(64);

	/* Date */
	if (compose->account->add_date) {
		get_rfc822_date(buf, sizeof(buf));
		g_string_sprintfa(header, "Date: %s\n", buf);
	}

	/* From */
	if (compose->account->name && *compose->account->name) {
		compose_convert_header
			(buf, sizeof(buf), compose->account->name,
			 strlen("From: "), TRUE);
		QUOTE_IF_REQUIRED(name, buf);
		g_string_sprintfa(header, "From: %s <%s>\n",
			name, compose->account->address);
	} else
		g_string_sprintfa(header, "From: %s\n", compose->account->address);
	
	/* To */
	compose_add_headerfield_from_headerlist(compose, header, "To", ", ");

	/* Newsgroups */
	compose_add_headerfield_from_headerlist(compose, header, "Newsgroups", ",");

	/* Cc */
	compose_add_headerfield_from_headerlist(compose, header, "Cc", ", ");

	/* Bcc */
	compose_add_headerfield_from_headerlist(compose, header, "Bcc", ", ");

	/* Subject */
	str = gtk_entry_get_text(GTK_ENTRY(compose->subject_entry));
	if (*str != '\0' && !IS_IN_CUSTOM_HEADER("Subject")) {
		gchar *tmpstr;

		tmpstr = g_strdup(str);
		if (tmpstr == NULL) {
			g_string_free(header, TRUE);
			return NULL;
		}
		g_strstrip(str);
		if (*str != '\0') {
			compose_convert_header(buf, sizeof(buf), str,
					       strlen("Subject: "), FALSE);
			g_string_sprintfa(header, "Subject: %s\n", buf);
		}
		g_free(tmpstr);
	}

	/* Message-ID */
	if (compose->account->gen_msgid) {
		generate_msgid(buf, sizeof(buf));
		g_string_sprintfa(header, "Message-ID: <%s>\n", buf);
		compose->msgid = g_strdup(buf);
	}

	if (compose->remove_references == FALSE) {
		/* In-Reply-To */
		if (compose->inreplyto && compose->to_list)
			g_string_sprintfa(header, "In-Reply-To: <%s>\n", compose->inreplyto);
	
		/* References */
		if (compose->references)
			g_string_sprintfa(header, "References: %s\n", compose->references);
	}

	/* Followup-To */
	compose_add_headerfield_from_headerlist(compose, header, "Followup-To", ",");

	/* Reply-To */
	compose_add_headerfield_from_headerlist(compose, header, "Reply-To", ", ");

	/* Organization */
	if (compose->account->organization &&
	    !IS_IN_CUSTOM_HEADER("Organization")) {
		compose_convert_header(buf, sizeof(buf),
				       compose->account->organization,
				       strlen("Organization: "), FALSE);
		g_string_sprintfa(header, "Organization: %s\n", buf);
	}

	/* Program version and system info */
	/* uname(&utsbuf); */
	if (g_slist_length(compose->to_list) && !IS_IN_CUSTOM_HEADER("X-Mailer") &&
	    !compose->newsgroup_list) {
		g_string_sprintfa(header, "X-Mailer: %s (GTK+ %d.%d.%d; %s)\n",
			prog_version,
			gtk_major_version, gtk_minor_version, gtk_micro_version,
			TARGET_ALIAS);
			/* utsbuf.sysname, utsbuf.release, utsbuf.machine); */
	}
	if (g_slist_length(compose->newsgroup_list) && !IS_IN_CUSTOM_HEADER("X-Newsreader")) {
		g_string_sprintfa(header, "X-Newsreader: %s (GTK+ %d.%d.%d; %s)\n",
			prog_version,
			gtk_major_version, gtk_minor_version, gtk_micro_version,
			TARGET_ALIAS);
			/* utsbuf.sysname, utsbuf.release, utsbuf.machine); */
	}

	/* custom headers */
	if (compose->account->add_customhdr) {
		GSList *cur;

		for (cur = compose->account->customhdr_list; cur != NULL;
		     cur = cur->next) {
			CustomHeader *chdr = (CustomHeader *)cur->data;

			if (custom_header_is_allowed(chdr->name)) {
				compose_convert_header
					(buf, sizeof(buf),
					 chdr->value ? chdr->value : "",
					 strlen(chdr->name) + 2, FALSE);
				g_string_sprintfa(header, "%s: %s\n", chdr->name, buf);
			}
		}
	}

	/* PRIORITY */
	switch (compose->priority) {
		case PRIORITY_HIGHEST: g_string_sprintfa(header, "Importance: high\n"
						   "X-Priority: 1 (Highest)\n");
			break;
		case PRIORITY_HIGH: g_string_sprintfa(header, "Importance: high\n"
						"X-Priority: 2 (High)\n");
			break;
		case PRIORITY_NORMAL: break;
		case PRIORITY_LOW: g_string_sprintfa(header, "Importance: low\n"
					       "X-Priority: 4 (Low)\n");
			break;
		case PRIORITY_LOWEST: g_string_sprintfa(header, "Importance: low\n"
						  "X-Priority: 5 (Lowest)\n");
			break;
		default: debug_print("compose: priority unknown : %d\n",
				     compose->priority);
	}

	/* Request Return Receipt */
	if (!IS_IN_CUSTOM_HEADER("Disposition-Notification-To")) {
		if (compose->return_receipt) {
			if (compose->account->name
			    && *compose->account->name) {
				compose_convert_header(buf, sizeof(buf), 
						       compose->account->name, 
						       strlen("Disposition-Notification-To: "),
						       TRUE);
				g_string_sprintfa(header, "Disposition-Notification-To: %s <%s>\n", buf, compose->account->address);
			} else
				g_string_sprintfa(header, "Disposition-Notification-To: %s\n", compose->account->address);
		}
	}

	/* get special headers */
	for (list = compose->header_list; list; list = list->next) {
    		ComposeHeaderEntry *headerentry;
		gchar *tmp;
		gchar *headername;
		gchar *headername_wcolon;
		gchar *headername_trans;
		gchar *headervalue;
		gchar **string;
		gboolean standard_header = FALSE;

		headerentry = ((ComposeHeaderEntry *)list->data);
		
		tmp = g_strdup(gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(headerentry->combo)->entry)));
		if (strchr(tmp, ' ') != NULL || strchr(tmp, '\r') != NULL || strchr(tmp, '\n') != NULL) {
			g_free(tmp);
			continue;
		}

		if (!strstr(tmp, ":")) {
			headername_wcolon = g_strconcat(tmp, ":", NULL);
			headername = g_strdup(tmp);
		} else {
			headername_wcolon = g_strdup(tmp);
			headername = g_strdup(strtok(tmp, ":"));
		}
		g_free(tmp);
		
		headervalue = gtk_entry_get_text(GTK_ENTRY(headerentry->entry));
		subst_char(headervalue, '\r', ' ');
		subst_char(headervalue, '\n', ' ');
		string = std_headers;
		while (*string != NULL) {
			headername_trans = prefs_common.trans_hdr ? gettext(*string) : *string;
			if (!strcmp(headername_trans,headername_wcolon))
				standard_header = TRUE;
			string++;
		}
		if (!standard_header && !IS_IN_CUSTOM_HEADER(headername))
			g_string_sprintfa(header, "%s %s\n", headername_wcolon, headervalue);
				
		g_free(headername);
		g_free(headername_wcolon);		
	}

	str = header->str;
	g_string_free(header, FALSE);

	return str;
}

#undef IS_IN_CUSTOM_HEADER

static void compose_convert_header(gchar *dest, gint len, gchar *src,
				   gint header_len, gboolean addr_field)
{
	g_return_if_fail(src != NULL);
	g_return_if_fail(dest != NULL);

	if (len < 1) return;

	subst_char(src, '\n', ' ');
	subst_char(src, '\r', ' ');
	g_strchomp(src);

	conv_encode_header(dest, len, src, header_len, addr_field);
}

static void compose_create_header_entry(Compose *compose) 
{
	gchar *headers[] = {"To:", "Cc:", "Bcc:", "Newsgroups:", "Reply-To:", "Followup-To:", NULL};

	GtkWidget *combo;
	GtkWidget *entry;
	GList *combo_list = NULL;
	gchar **string, *header = NULL;
	ComposeHeaderEntry *headerentry;
	gboolean standard_header = FALSE;

	headerentry = g_new0(ComposeHeaderEntry, 1);

	/* Combo box */
	combo = gtk_combo_new();
	string = headers; 
	while(*string != NULL) {
		combo_list = g_list_append(combo_list, (prefs_common.trans_hdr ? gettext(*string) : *string));
	        string++;
	}
	gtk_combo_set_popdown_strings(GTK_COMBO(combo), combo_list);
	g_list_free(combo_list);
	gtk_editable_set_editable(GTK_EDITABLE(GTK_COMBO(combo)->entry), TRUE);
	gtk_widget_show(combo);
	gtk_table_attach(GTK_TABLE(compose->header_table), combo, 0, 1, compose->header_nextrow, compose->header_nextrow+1, GTK_SHRINK, GTK_FILL, 0, 0);
	if (compose->header_last) {	
		gchar *last_header_entry = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(compose->header_last->combo)->entry));
		string = headers;
		while (*string != NULL) {
			if (!strcmp(*string, last_header_entry))
				standard_header = TRUE;
			string++;
		}
		if (standard_header)
			header = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(compose->header_last->combo)->entry));
	}
	if (!compose->header_last || !standard_header) {
		switch(compose->account->protocol) {
			case A_NNTP:
				header = prefs_common.trans_hdr ? _("Newsgroups:") : "Newsgroups:";
				break;
			default:
				header = prefs_common.trans_hdr ? _("To:") : "To:";
				break;
		}								    
	}
	if (header)
		gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(combo)->entry), header);

	/* Entry field */
	entry = gtk_entry_new(); 
	gtk_widget_show(entry);
	gtk_table_attach(GTK_TABLE(compose->header_table), entry, 1, 2, compose->header_nextrow, compose->header_nextrow+1, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

        gtk_signal_connect(GTK_OBJECT(entry), "key-press-event", GTK_SIGNAL_FUNC(compose_headerentry_key_press_event_cb), headerentry);
    	gtk_signal_connect(GTK_OBJECT(entry), "changed", GTK_SIGNAL_FUNC(compose_headerentry_changed_cb), headerentry);
    	gtk_signal_connect(GTK_OBJECT(entry), "activate", GTK_SIGNAL_FUNC(text_activated), compose);
	gtk_signal_connect(GTK_OBJECT(entry), "button-press-event", 
			   GTK_SIGNAL_FUNC(compose_headerentry_button_pressed),
			   NULL);

	address_completion_register_entry(GTK_ENTRY(entry));

        headerentry->compose = compose;
        headerentry->combo = combo;
        headerentry->entry = entry;
        headerentry->headernum = compose->header_nextrow;

        compose->header_nextrow++;
	compose->header_last = headerentry;
}

static void compose_add_header_entry(Compose *compose, gchar *header, gchar *text) 
{
	ComposeHeaderEntry *last_header;
	
	last_header = compose->header_last;
	
	gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(last_header->combo)->entry), header);
	gtk_entry_set_text(GTK_ENTRY(last_header->entry), text);
}

static GtkWidget *compose_create_header(Compose *compose) 
{
	GtkWidget *label;
	GtkWidget *hbox;
	GtkWidget *from_optmenu_hbox;
#if 0 /* NEW COMPOSE GUI */
	GtkWidget *to_entry;
	GtkWidget *to_hbox;
	GtkWidget *newsgroups_entry;
	GtkWidget *newsgroups_hbox;
#endif
	GtkWidget *header_scrolledwin;
	GtkWidget *header_table;
#if 0 /* NEW COMPOSE GUI */
	GtkWidget *cc_entry;
	GtkWidget *cc_hbox;
	GtkWidget *bcc_entry;
	GtkWidget *bcc_hbox;
	GtkWidget *reply_entry;
	GtkWidget *reply_hbox;
	GtkWidget *followup_entry;
	GtkWidget *followup_hbox;
#endif

	gint count = 0;

	/* header labels and entries */
	header_scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_show(header_scrolledwin);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(header_scrolledwin), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	header_table = gtk_table_new(2, 2, FALSE);
	gtk_widget_show(header_table);
	gtk_container_set_border_width(GTK_CONTAINER(header_table), 2);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(header_scrolledwin), header_table);
	gtk_viewport_set_shadow_type(GTK_VIEWPORT(GTK_BIN(header_scrolledwin)->child), GTK_SHADOW_ETCHED_IN);
	count = 0;

	/* option menu for selecting accounts */
	hbox = gtk_hbox_new(FALSE, 0);
	label = gtk_label_new(prefs_common.trans_hdr ? _("From:") : "From:");
	gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_table_attach(GTK_TABLE(header_table), hbox, 0, 1, count, count + 1,
			 GTK_FILL, 0, 2, 0);
	from_optmenu_hbox = compose_account_option_menu_create(compose);
	gtk_table_attach(GTK_TABLE(header_table), from_optmenu_hbox,
				  1, 2, count, count + 1, GTK_EXPAND | GTK_FILL, GTK_SHRINK, 0, 0);
#if 0 /* NEW COMPOSE GUI */
	gtk_table_set_row_spacing(GTK_TABLE(table), 0, 4);
#endif
	count++;

	compose->header_table = header_table;
	compose->header_list = NULL;
	compose->header_nextrow = count;

	compose_create_header_entry(compose);

#if 0 /* NEW COMPOSE GUI */
	compose_add_entry_field(table, &to_hbox, &to_entry, &count,
				"To:", TRUE); 
	gtk_table_set_row_spacing(GTK_TABLE(table), 0, 4);
	compose_add_entry_field(table, &newsgroups_hbox, &newsgroups_entry,
				&count, "Newsgroups:", FALSE);
	gtk_table_set_row_spacing(GTK_TABLE(table), 1, 4);

	gtk_table_set_row_spacing(GTK_TABLE(table), 2, 4);

	compose_add_entry_field(table, &cc_hbox, &cc_entry, &count,
				"Cc:", TRUE);
	gtk_table_set_row_spacing(GTK_TABLE(table), 3, 4);
	compose_add_entry_field(table, &bcc_hbox, &bcc_entry, &count,
				"Bcc:", TRUE);
	gtk_table_set_row_spacing(GTK_TABLE(table), 4, 4);
	compose_add_entry_field(table, &reply_hbox, &reply_entry, &count,
				"Reply-To:", TRUE);
	gtk_table_set_row_spacing(GTK_TABLE(table), 5, 4);
	compose_add_entry_field(table, &followup_hbox, &followup_entry, &count,
				"Followup-To:", FALSE);
	gtk_table_set_row_spacing(GTK_TABLE(table), 6, 4);

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
#endif

	compose->table	          = NULL;
#if 0 /* NEW COMPOSE GUI */
	compose->table	          = table;
	compose->to_hbox          = to_hbox;
	compose->to_entry         = to_entry;
	compose->newsgroups_hbox  = newsgroups_hbox;
	compose->newsgroups_entry = newsgroups_entry;
#endif
#if 0 /* NEW COMPOSE GUI */
	compose->cc_hbox          = cc_hbox;
	compose->cc_entry         = cc_entry;
	compose->bcc_hbox         = bcc_hbox;
	compose->bcc_entry        = bcc_entry;
	compose->reply_hbox       = reply_hbox;
	compose->reply_entry      = reply_entry;
	compose->followup_hbox    = followup_hbox;
	compose->followup_entry   = followup_entry;
#endif

	return header_scrolledwin ;
}

GtkWidget *compose_create_attach(Compose *compose)
{
	gchar *titles[N_ATTACH_COLS];
	gint i;

	GtkWidget *attach_scrwin;
	GtkWidget *attach_clist;

	titles[COL_MIMETYPE] = _("MIME type");
	titles[COL_SIZE]     = _("Size");
	titles[COL_NAME]     = _("Name");

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
			  GDK_ACTION_COPY | GDK_ACTION_MOVE);
	gtk_signal_connect(GTK_OBJECT(attach_clist), "drag_data_received",
			   GTK_SIGNAL_FUNC(compose_attach_drag_received_cb),
			   compose);

	compose->attach_scrwin = attach_scrwin;
	compose->attach_clist  = attach_clist;

	return attach_scrwin;
}

static void compose_savemsg_checkbtn_cb(GtkWidget *widget, Compose *compose);
static void compose_savemsg_select_cb(GtkWidget *widget, Compose *compose);

static GtkWidget *compose_create_others(Compose *compose)
{
	GtkWidget *table;
	GtkWidget *savemsg_checkbtn;
	GtkWidget *savemsg_entry;
	GtkWidget *savemsg_select;
	
	guint rowcount = 0;
	gchar *folderidentifier;

	/* Table for settings */
	table = gtk_table_new(3, 1, FALSE);
	gtk_widget_show(table);
	gtk_table_set_row_spacings(GTK_TABLE(table), VSPACING_NARROW);
	rowcount = 0;

	/* Save Message to folder */
	savemsg_checkbtn = gtk_check_button_new_with_label(_("Save Message to "));
	gtk_widget_show(savemsg_checkbtn);
	gtk_table_attach(GTK_TABLE(table), savemsg_checkbtn, 0, 1, rowcount, rowcount + 1, GTK_SHRINK | GTK_FILL, GTK_SHRINK, 0, 0);
	if (account_get_special_folder(compose->account, F_OUTBOX)) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(savemsg_checkbtn), prefs_common.savemsg);
	}
	gtk_signal_connect(GTK_OBJECT(savemsg_checkbtn), "toggled",
			    GTK_SIGNAL_FUNC(compose_savemsg_checkbtn_cb), compose);

	savemsg_entry = gtk_entry_new();
	gtk_widget_show(savemsg_entry);
	gtk_table_attach_defaults(GTK_TABLE(table), savemsg_entry, 1, 2, rowcount, rowcount + 1);
	gtk_editable_set_editable(GTK_EDITABLE(savemsg_entry), prefs_common.savemsg);
	if (account_get_special_folder(compose->account, F_OUTBOX)) {
		folderidentifier = folder_item_get_identifier(account_get_special_folder
				  (compose->account, F_OUTBOX));
		gtk_entry_set_text(GTK_ENTRY(savemsg_entry), folderidentifier);
		g_free(folderidentifier);
	}

	savemsg_select = gtk_button_new_with_label (_("Select ..."));
	gtk_widget_show (savemsg_select);
	gtk_table_attach(GTK_TABLE(table), savemsg_select, 2, 3, rowcount, rowcount + 1, GTK_SHRINK | GTK_FILL, GTK_SHRINK, 0, 0);
	gtk_signal_connect (GTK_OBJECT (savemsg_select), "clicked",
			    GTK_SIGNAL_FUNC (compose_savemsg_select_cb),
			    compose);

	rowcount++;

	compose->savemsg_checkbtn = savemsg_checkbtn;
	compose->savemsg_entry = savemsg_entry;

	return table;	
}

static void compose_savemsg_checkbtn_cb(GtkWidget *widget, Compose *compose) 
{
	gtk_editable_set_editable(GTK_EDITABLE(compose->savemsg_entry),
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(compose->savemsg_checkbtn)));
}

static void compose_savemsg_select_cb(GtkWidget *widget, Compose *compose)
{
	FolderItem *dest;
	gchar * path;

	dest = foldersel_folder_sel(NULL, FOLDER_SEL_COPY, NULL);
	if (!dest) return;

	path = folder_item_get_identifier(dest);

	gtk_entry_set_text(GTK_ENTRY(compose->savemsg_entry), path);
	g_free(path);
}

static Compose *compose_create(PrefsAccount *account, ComposeMode mode)
{
	Compose   *compose;
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *menubar;
	GtkWidget *handlebox;

	GtkWidget *notebook;

	GtkWidget *vbox2;

	GtkWidget *label;
	GtkWidget *subject_hbox;
	GtkWidget *subject_frame;
	GtkWidget *subject_entry;
	GtkWidget *subject;
	GtkWidget *paned;

	GtkWidget *edit_vbox;
	GtkWidget *ruler_hbox;
	GtkWidget *ruler;
	GtkWidget *scrolledwin;
	GtkWidget *text;

	UndoMain *undostruct;

	gchar *titles[N_ATTACH_COLS];
	guint n_menu_entries;
	GtkStyle  *style, *new_style;
	GdkColormap *cmap;
	GdkColor color[1];
	gboolean success[1];
	GtkWidget *popupmenu;
	GtkItemFactory *popupfactory;
	GtkItemFactory *ifactory;
	GtkWidget *tmpl_menu;
	gint n_entries;

#if USE_ASPELL
        GtkAspell * gtkaspell = NULL;
#endif

	static GdkGeometry geometry;

	g_return_val_if_fail(account != NULL, NULL);

	debug_print("Creating compose window...\n");
	compose = g_new0(Compose, 1);

	titles[COL_MIMETYPE] = _("MIME type");
	titles[COL_SIZE]     = _("Size");
	titles[COL_NAME]     = _("Name");

	compose->account = account;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_policy(GTK_WINDOW(window), TRUE, TRUE, FALSE);
	gtk_widget_set_usize(window, -1, prefs_common.compose_height);
	gtk_window_set_wmclass(GTK_WINDOW(window), "compose window", "Sylpheed");

	if (!geometry.max_width) {
		geometry.max_width = gdk_screen_width();
		geometry.max_height = gdk_screen_height();
	}
	gtk_window_set_geometry_hints(GTK_WINDOW(window), NULL,
				      &geometry, GDK_HINT_MAX_SIZE);
	gtk_widget_set_uposition(window, prefs_common.compose_x, 
		prefs_common.compose_y);
	gtk_signal_connect(GTK_OBJECT(window), "delete_event",
			   GTK_SIGNAL_FUNC(compose_delete_cb), compose);
	gtk_signal_connect(GTK_OBJECT(window), "destroy",
			   GTK_SIGNAL_FUNC(compose_destroy_cb), compose);
	MANAGE_WINDOW_SIGNALS_CONNECT(window);
	gtk_widget_realize(window);

	gtkut_widget_set_composer_icon(window);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	n_menu_entries = sizeof(compose_entries) / sizeof(compose_entries[0]);
	menubar = menubar_create(window, compose_entries,
				 n_menu_entries, "<Compose>", compose);
	gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, TRUE, 0);

	handlebox = gtk_handle_box_new();
	gtk_box_pack_start(GTK_BOX(vbox), handlebox, FALSE, FALSE, 0);

	compose->toolbar = toolbar_create(TOOLBAR_COMPOSE, handlebox,
					  (gpointer)compose);

	vbox2 = gtk_vbox_new(FALSE, 2);
	gtk_box_pack_start(GTK_BOX(vbox), vbox2, TRUE, TRUE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(vbox2), BORDER_WIDTH);
	
	/* Notebook */
	notebook = gtk_notebook_new();
	gtk_widget_set_usize(notebook, -1, 130);
	gtk_widget_show(notebook);

	/* header labels and entries */
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), compose_create_header(compose), gtk_label_new(_("Header")));
	/* attachment list */
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), compose_create_attach(compose), gtk_label_new(_("Attachments")));
	/* Others Tab */
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), compose_create_others(compose), gtk_label_new(_("Others")));

	/* Subject */
	subject_hbox = gtk_hbox_new(FALSE, 0);
	gtk_widget_show(subject_hbox);

	subject_frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(subject_frame), GTK_SHADOW_OUT);
	gtk_box_pack_start(GTK_BOX(subject_hbox), subject_frame, TRUE, TRUE, BORDER_WIDTH+1);
	gtk_widget_show(subject_frame);

	subject = gtk_hbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(subject), BORDER_WIDTH);
	gtk_widget_show(subject);

	label = gtk_label_new(_("Subject:"));
	gtk_box_pack_start(GTK_BOX(subject), label, FALSE, FALSE, 4);
	gtk_widget_show(label);

	subject_entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(subject), subject_entry, TRUE, TRUE, 2);
    	gtk_signal_connect(GTK_OBJECT(subject_entry), "activate", GTK_SIGNAL_FUNC(text_activated), compose);
	gtk_widget_show(subject_entry);
	compose->subject_entry = subject_entry;
	gtk_container_add(GTK_CONTAINER(subject_frame), subject);
	
	edit_vbox = gtk_vbox_new(FALSE, 0);

	gtk_box_pack_start(GTK_BOX(edit_vbox), subject_hbox, FALSE, FALSE, 0);

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
	GTK_STEXT(text)->default_tab_width = 8;
	gtk_stext_set_editable(GTK_STEXT(text), TRUE);

	if (prefs_common.block_cursor) {
		GTK_STEXT(text)->cursor_type = GTK_STEXT_CURSOR_BLOCK;
	}
	
	gtk_container_add(GTK_CONTAINER(scrolledwin), text);

	gtk_signal_connect(GTK_OBJECT(text), "changed",
			   GTK_SIGNAL_FUNC(compose_changed_cb), compose);
	gtk_signal_connect(GTK_OBJECT(text), "grab_focus",
			   GTK_SIGNAL_FUNC(compose_grab_focus_cb), compose);
	gtk_signal_connect(GTK_OBJECT(text), "activate",
			   GTK_SIGNAL_FUNC(text_activated), compose);
	gtk_signal_connect(GTK_OBJECT(text), "insert_text",
			   GTK_SIGNAL_FUNC(text_inserted), compose);
	gtk_signal_connect_after(GTK_OBJECT(text), "button_press_event",
				 GTK_SIGNAL_FUNC(compose_button_press_cb),
				 edit_vbox);
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
			  GDK_ACTION_COPY | GDK_ACTION_MOVE);
	gtk_signal_connect(GTK_OBJECT(text), "drag_data_received",
			   GTK_SIGNAL_FUNC(compose_insert_drag_received_cb),
			   compose);
	gtk_widget_show_all(vbox);

	/* pane between attach clist and text */
	paned = gtk_vpaned_new();
	gtk_paned_set_gutter_size(GTK_PANED(paned), 12);
	gtk_paned_set_handle_size(GTK_PANED(paned), 12);
	gtk_container_add(GTK_CONTAINER(vbox2), paned);
	gtk_paned_add1(GTK_PANED(paned), notebook);
	gtk_paned_add2(GTK_PANED(paned), edit_vbox);
	gtk_widget_show_all(paned);

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
		GdkFont *font;

		if ((font = gtkut_font_load(prefs_common.textfont)) != NULL) {
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
	menu_set_sensitive(ifactory, "/Options/Remove references", FALSE);

	tmpl_menu = gtk_item_factory_get_item(ifactory, "/Tools/Template");

	undostruct = undo_init(text);
	undo_set_change_state_func(undostruct, &compose_undo_state_changed,
				   menubar);

	address_completion_start(window);

	compose->window        = window;
	compose->vbox	       = vbox;
	compose->menubar       = menubar;
	compose->handlebox     = handlebox;

	compose->vbox2	       = vbox2;

	compose->paned = paned;

	compose->edit_vbox     = edit_vbox;
	compose->ruler_hbox    = ruler_hbox;
	compose->ruler         = ruler;
	compose->scrolledwin   = scrolledwin;
	compose->text	       = text;

	compose->focused_editable = NULL;

	compose->popupmenu    = popupmenu;
	compose->popupfactory = popupfactory;

	compose->tmpl_menu = tmpl_menu;

	compose->mode = mode;

	compose->targetinfo = NULL;
	compose->replyinfo  = NULL;
	compose->fwdinfo    = NULL;

	compose->replyto     = NULL;
	compose->cc	     = NULL;
	compose->bcc	     = NULL;
	compose->followup_to = NULL;

	compose->ml_post     = NULL;

	compose->inreplyto   = NULL;
	compose->references  = NULL;
	compose->msgid       = NULL;
	compose->boundary    = NULL;

	compose->autowrap       = prefs_common.autowrap;

	compose->use_signing    = FALSE;
	compose->use_encryption = FALSE;
	compose->privacy_system = NULL;

	compose->modified = FALSE;

	compose->return_receipt = FALSE;

	compose->to_list        = NULL;
	compose->newsgroup_list = NULL;

	compose->undostruct = undostruct;

	compose->sig_str = NULL;

	compose->exteditor_file    = NULL;
	compose->exteditor_pid     = -1;
	compose->exteditor_readdes = -1;
	compose->exteditor_tag     = -1;
	compose->draft_timeout_tag = -1;

#if USE_ASPELL
	menu_set_sensitive(ifactory, "/Spelling", FALSE);
	if (mode != COMPOSE_REDIRECT) {
        	if (prefs_common.enable_aspell && prefs_common.dictionary &&
	    	    strcmp(prefs_common.dictionary, _("None"))) {
			gtkaspell = gtkaspell_new(prefs_common.aspell_path,
						  prefs_common.dictionary,
						  conv_get_current_charset_str(),
						  prefs_common.misspelled_col,
						  prefs_common.check_while_typing,
						  prefs_common.use_alternate,
						  GTK_STEXT(text));
			if (!gtkaspell) {
				alertpanel_error(_("Spell checker could not "
						"be started.\n%s"),
						gtkaspell_checkers_strerror());
				gtkaspell_checkers_reset_error();
			} else {

				GtkWidget *menuitem;

				if (!gtkaspell_set_sug_mode(gtkaspell,
						prefs_common.aspell_sugmode)) {
					debug_print("Aspell: could not set "
						    "suggestion mode %s\n",
						    gtkaspell_checkers_strerror());
					gtkaspell_checkers_reset_error();
				}

				menuitem = gtk_item_factory_get_item(ifactory,
					"/Spelling/Spelling Configuration");
				gtkaspell_populate_submenu(gtkaspell, menuitem);
				menu_set_sensitive(ifactory, "/Spelling", TRUE);
			}
        	}
	}
        compose->gtkaspell = gtkaspell;
#endif

	compose_select_account(compose, account, TRUE);

	menu_set_active(ifactory, "/Edit/Auto wrapping", prefs_common.autowrap);
	if (account->set_autocc && account->auto_cc && mode != COMPOSE_REEDIT)
		compose_entry_append(compose, account->auto_cc, COMPOSE_CC);

	if (account->set_autobcc && account->auto_bcc && mode != COMPOSE_REEDIT) 
		compose_entry_append(compose, account->auto_bcc, COMPOSE_BCC);
	
	if (account->set_autoreplyto && account->auto_replyto && mode != COMPOSE_REEDIT)
		compose_entry_append(compose, account->auto_replyto, COMPOSE_REPLYTO);


	if (account->protocol != A_NNTP)
		gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(compose->header_last->combo)->entry), prefs_common.trans_hdr ? _("To:") : "To:");
	else
		gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(compose->header_last->combo)->entry), prefs_common.trans_hdr ? _("Newsgroups:") : "Newsgroups:");

	addressbook_set_target_compose(compose);
	
	if (mode != COMPOSE_REDIRECT)
		compose_set_template_menu(compose);
	else {
		GtkWidget *menuitem;
		menuitem = gtk_item_factory_get_item(ifactory, "/Tools/Template");
		menu_set_sensitive(ifactory, "/Tools/Template", FALSE);
	}

	compose_list = g_list_append(compose_list, compose);

	if (!prefs_common.show_ruler)
		gtk_widget_hide(ruler_hbox);

	/* Priority */
	compose->priority = PRIORITY_NORMAL;
	compose_update_priority_menu_item(compose);

	/* Actions menu */
	compose_update_actions_menu(compose);

	/* Privacy Systems menu */
	compose_update_privacy_systems_menu(compose);

	activate_privacy_system(compose, account);
	toolbar_set_style(compose->toolbar->toolbar, compose->handlebox, prefs_common.toolbar_style);
	gtk_widget_show(window);
	
	return compose;
}

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

		if (ac->name)
			name = g_strdup_printf("%s: %s <%s>",
					       ac->account_name,
					       ac->name, ac->address);
		else
			name = g_strdup_printf("%s: %s",
					       ac->account_name, ac->address);
		MENUITEM_ADD(menu, menuitem, name, ac->account_id);
		g_free(name);
		gtk_signal_connect(GTK_OBJECT(menuitem), "activate",
				   GTK_SIGNAL_FUNC(account_activated),
				   compose);
	}

	gtk_option_menu_set_menu(GTK_OPTION_MENU(optmenu), menu);
	gtk_option_menu_set_history(GTK_OPTION_MENU(optmenu), def_menu);

	return hbox;
}

static void compose_set_priority_cb(gpointer data,
				    guint action,
				    GtkWidget *widget)
{
	Compose *compose = (Compose *) data;
	compose->priority = action;
}

static void compose_update_priority_menu_item(Compose * compose)
{
	GtkItemFactory *ifactory;
	GtkWidget *menuitem = NULL;

	ifactory = gtk_item_factory_from_widget(compose->menubar);
	
	switch (compose->priority) {
		case PRIORITY_HIGHEST:
			menuitem = gtk_item_factory_get_item
				(ifactory, "/Options/Priority/Highest");
			break;
		case PRIORITY_HIGH:
			menuitem = gtk_item_factory_get_item
				(ifactory, "/Options/Priority/High");
			break;
		case PRIORITY_NORMAL:
			menuitem = gtk_item_factory_get_item
				(ifactory, "/Options/Priority/Normal");
			break;
		case PRIORITY_LOW:
			menuitem = gtk_item_factory_get_item
				(ifactory, "/Options/Priority/Low");
			break;
		case PRIORITY_LOWEST:
			menuitem = gtk_item_factory_get_item
				(ifactory, "/Options/Priority/Lowest");
			break;
	}
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem), TRUE);
}	

static void compose_set_privacy_system_cb(GtkWidget *widget, gpointer data)
{
	Compose *compose = (Compose *) data;
	gchar *systemid;
	GtkItemFactory *ifactory;
	gboolean can_sign = FALSE, can_encrypt = FALSE;

	g_return_if_fail(GTK_IS_CHECK_MENU_ITEM(widget));

	if (!GTK_CHECK_MENU_ITEM(widget)->active)
		return;

	systemid = gtk_object_get_data(GTK_OBJECT(widget), "privacy_system");
	g_free(compose->privacy_system);
	compose->privacy_system = NULL;
	if (systemid != NULL) {
		compose->privacy_system = g_strdup(systemid);

		can_sign = privacy_system_can_sign(systemid);
		can_encrypt = privacy_system_can_encrypt(systemid);
	}

	debug_print("activated privacy system: %s\n", systemid != NULL ? systemid : "None");

	ifactory = gtk_item_factory_from_widget(compose->menubar);
	menu_set_sensitive(ifactory, "/Options/Sign", can_sign);
	menu_set_sensitive(ifactory, "/Options/Encrypt", can_encrypt);
}

static void compose_update_privacy_system_menu_item(Compose * compose)
{
	static gchar *branch_path = "/Options/Privacy System";
	GtkItemFactory *ifactory;
	GtkWidget *menuitem = NULL;
	GList *amenu;
	gboolean can_sign = FALSE, can_encrypt = FALSE;

	ifactory = gtk_item_factory_from_widget(compose->menubar);

	if (compose->privacy_system != NULL) {
		gchar *systemid;

		menuitem = gtk_item_factory_get_widget(ifactory, branch_path);
		g_return_if_fail(menuitem != NULL);

		amenu = GTK_MENU_SHELL(menuitem)->children;
		menuitem = NULL;
		while (amenu != NULL) {
		        GList *alist = amenu->next;

			systemid = gtk_object_get_data(GTK_OBJECT(amenu->data), "privacy_system");
			if (systemid != NULL)
				if (strcmp(systemid, compose->privacy_system) == 0) {
					menuitem = GTK_WIDGET(amenu->data);

					can_sign = privacy_system_can_sign(systemid);
					can_encrypt = privacy_system_can_encrypt(systemid);

					break;
				}

			amenu = alist;
		}
		if (menuitem != NULL)
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem), TRUE);
	}

	menu_set_sensitive(ifactory, "/Options/Sign", can_sign);
	menu_set_sensitive(ifactory, "/Options/Encrypt", can_encrypt);
}	
 
static void compose_set_template_menu(Compose *compose)
{
	GSList *tmpl_list, *cur;
	GtkWidget *menu;
	GtkWidget *item;

	tmpl_list = template_get_config();

	menu = gtk_menu_new();

	for (cur = tmpl_list; cur != NULL; cur = cur->next) {
		Template *tmpl = (Template *)cur->data;

		item = gtk_menu_item_new_with_label(tmpl->name);
		gtk_menu_append(GTK_MENU(menu), item);
		gtk_signal_connect(GTK_OBJECT(item), "activate",
				   GTK_SIGNAL_FUNC(compose_template_activate_cb),
				   compose);
		gtk_object_set_data(GTK_OBJECT(item), "template", tmpl);
		gtk_widget_show(item);
	}

	gtk_widget_show(menu);
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(compose->tmpl_menu), menu);
}

void compose_update_actions_menu(Compose *compose)
{
	GtkItemFactory *ifactory;

	ifactory = gtk_item_factory_from_widget(compose->menubar);
	action_update_compose_menu(ifactory, "/Tools/Actions", compose);
}

void compose_update_privacy_systems_menu(Compose *compose)
{
	static gchar *branch_path = "/Options/Privacy System";
	GtkItemFactory *ifactory;
	GtkWidget *menuitem;
	GSList *systems, *cur;
	GList *amenu;
	GtkWidget *widget;
	GtkWidget *system_none;
	GSList *group;

	ifactory = gtk_item_factory_from_widget(compose->menubar);

	/* remove old entries */
	menuitem = gtk_item_factory_get_widget(ifactory, branch_path);
	g_return_if_fail(menuitem != NULL);

	amenu = GTK_MENU_SHELL(menuitem)->children->next;
	while (amenu != NULL) {
		GList *alist = amenu->next;
		gtk_widget_destroy(GTK_WIDGET(amenu->data));
		amenu = alist;
	}

	system_none = gtk_item_factory_get_widget(ifactory,
		"/Options/Privacy System/None");

	gtk_signal_connect(GTK_OBJECT(system_none), "activate",
			   GTK_SIGNAL_FUNC(compose_set_privacy_system_cb),
			   compose);

	systems = privacy_get_system_ids();
	for (cur = systems; cur != NULL; cur = g_slist_next(cur)) {
		gchar *systemid = cur->data;

		group = gtk_radio_menu_item_group(GTK_RADIO_MENU_ITEM(system_none));
		widget = gtk_radio_menu_item_new_with_label(group,
			privacy_system_get_name(systemid));
		gtk_object_set_data_full(GTK_OBJECT(widget), "privacy_system",
				         g_strdup(systemid), g_free);
		gtk_signal_connect(GTK_OBJECT(widget), "activate",
			GTK_SIGNAL_FUNC(compose_set_privacy_system_cb), compose);

		gtk_menu_append(GTK_MENU(system_none->parent), widget);
		gtk_widget_show(widget);
		g_free(systemid);
	}
	g_slist_free(systems);
}

void compose_reflect_prefs_all(void)
{
	GList *cur;
	Compose *compose;

	for (cur = compose_list; cur != NULL; cur = cur->next) {
		compose = (Compose *)cur->data;
		compose_set_template_menu(compose);
	}
}

void compose_reflect_prefs_pixmap_theme(void)
{
	GList *cur;
	Compose *compose;

	for (cur = compose_list; cur != NULL; cur = cur->next) {
		compose = (Compose *)cur->data;
		toolbar_update(TOOLBAR_COMPOSE, compose);
	}
}

static void compose_template_apply(Compose *compose, Template *tmpl,
				   gboolean replace)
{
	gchar *qmark;
	gchar *parsed_str;

	if (!tmpl || !tmpl->value) return;

	gtk_stext_freeze(GTK_STEXT(compose->text));

	if (tmpl->subject && *tmpl->subject != '\0')
		gtk_entry_set_text(GTK_ENTRY(compose->subject_entry),
				   tmpl->subject);
	if (tmpl->to && *tmpl->to != '\0')
		compose_entry_append(compose, tmpl->to, COMPOSE_TO);
	if (tmpl->cc && *tmpl->cc != '\0')
		compose_entry_append(compose, tmpl->cc, COMPOSE_CC);

	if (tmpl->bcc && *tmpl->bcc != '\0')
		compose_entry_append(compose, tmpl->bcc, COMPOSE_BCC);

	if (replace)
		gtkut_stext_clear(GTK_STEXT(compose->text));

	if ((compose->replyinfo == NULL) && (compose->fwdinfo == NULL)) {
		parsed_str = compose_quote_fmt(compose, NULL, tmpl->value,
					       NULL, NULL);
	} else {
		if (prefs_common.quotemark && *prefs_common.quotemark)
			qmark = prefs_common.quotemark;
		else
			qmark = "> ";

		if (compose->replyinfo != NULL)
			parsed_str = compose_quote_fmt(compose, compose->replyinfo,
						       tmpl->value, qmark, NULL);
		else if (compose->fwdinfo != NULL)
			parsed_str = compose_quote_fmt(compose, compose->fwdinfo,
						       tmpl->value, qmark, NULL);
		else
			parsed_str = NULL;
	}

	if (replace && parsed_str && compose->account->auto_sig)
		compose_insert_sig(compose, FALSE);

	if (replace && parsed_str) {
		gtk_editable_set_position(GTK_EDITABLE(compose->text), 0);
		gtk_stext_set_point(GTK_STEXT(compose->text), 0);
	}

	if (parsed_str)
		compose_changed_cb(NULL, compose);

	gtk_stext_thaw(GTK_STEXT(compose->text));
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
	slist_free_strings(compose->header_list);
	g_slist_free(compose->header_list);

	procmsg_msginfo_free(compose->targetinfo);
	procmsg_msginfo_free(compose->replyinfo);
	procmsg_msginfo_free(compose->fwdinfo);

	g_free(compose->replyto);
	g_free(compose->cc);
	g_free(compose->bcc);
	g_free(compose->newsgroups);
	g_free(compose->followup_to);

	g_free(compose->ml_post);

	g_free(compose->inreplyto);
	g_free(compose->references);
	g_free(compose->msgid);
	g_free(compose->boundary);

	if (compose->redirect_filename)
		g_free(compose->redirect_filename);
	if (compose->undostruct)
		undo_destroy(compose->undostruct);

	g_free(compose->sig_str);

	g_free(compose->exteditor_file);

	for (row = 0; (ainfo = gtk_clist_get_row_data(clist, row)) != NULL;
	     row++)
		compose_attach_info_free(ainfo);

	if (addressbook_get_target_compose() == compose)
		addressbook_set_target_compose(NULL);

#if USE_ASPELL
        if (compose->gtkaspell) {
	        gtkaspell_delete(compose->gtkaspell);
        }
#endif

	prefs_common.compose_width = compose->scrolledwin->allocation.width;
	prefs_common.compose_height = compose->window->allocation.height;

	if (!compose->paned->parent)
		gtk_widget_destroy(compose->paned);
	gtk_widget_destroy(compose->popupmenu);

	toolbar_destroy(compose->toolbar);
	g_free(compose->toolbar);
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
	GtkOptionMenu *optmenu;
	static gboolean cancelled;

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
		menu_select_by_data(GTK_MENU(gtk_option_menu_get_menu(optmenu)),
				    GINT_TO_POINTER(ENC_BASE64));
	else
		menu_select_by_data(GTK_MENU(gtk_option_menu_get_menu(optmenu)),
				    GINT_TO_POINTER(ainfo->encoding));

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

		cancelled = FALSE;
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
	GList     *mime_type_list, *strlist;

	debug_print("Creating attach_property window...\n");

	window = gtk_window_new(GTK_WINDOW_DIALOG);
	gtk_widget_set_usize(window, 480, -1);
	gtk_container_set_border_width(GTK_CONTAINER(window), 8);
	gtk_window_set_title(GTK_WINDOW(window), _("Properties"));
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

	label = gtk_label_new(_("MIME type")); 
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, (0 + 1), 
			 GTK_FILL, 0, 0, 0); 
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5); 
	mimetype_entry = gtk_combo_new(); 
	gtk_table_attach(GTK_TABLE(table), mimetype_entry, 1, 2, 0, (0 + 1), 
			 GTK_EXPAND|GTK_SHRINK|GTK_FILL, 0, 0, 0);
			 
	/* stuff with list */
	mime_type_list = procmime_get_mime_type_list();
	strlist = NULL;
	for (; mime_type_list != NULL; mime_type_list = mime_type_list->next) {
		MimeType *type = (MimeType *) mime_type_list->data;
		strlist = g_list_append(strlist, 
				g_strdup_printf("%s/%s",
					type->type, type->sub_type));
	}
	
	gtk_combo_set_popdown_strings(GTK_COMBO(mimetype_entry), strlist);

	for (mime_type_list = strlist; mime_type_list != NULL; 
		mime_type_list = mime_type_list->next)
		g_free(mime_type_list->data);
	g_list_free(strlist);
			 
	mimetype_entry = GTK_COMBO(mimetype_entry)->entry;			 

	label = gtk_label_new(_("Encoding"));
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2,
			 GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), hbox, 1, 2, 1, 2,
			 GTK_EXPAND|GTK_SHRINK|GTK_FILL, 0, 0, 0);

	optmenu = gtk_option_menu_new();
	gtk_box_pack_start(GTK_BOX(hbox), optmenu, TRUE, TRUE, 0);

	optmenu_menu = gtk_menu_new();
	MENUITEM_ADD(optmenu_menu, menuitem, "7bit", ENC_7BIT);
	gtk_option_menu_set_menu(GTK_OPTION_MENU(optmenu), optmenu_menu);
#if 0
	gtk_widget_set_sensitive(menuitem, FALSE);
#endif
	MENUITEM_ADD(optmenu_menu, menuitem, "8bit", ENC_8BIT);
	gtk_option_menu_set_menu(GTK_OPTION_MENU(optmenu), optmenu_menu);
#if 0
	gtk_widget_set_sensitive(menuitem, FALSE);
#endif
	MENUITEM_ADD(optmenu_menu, menuitem, "quoted-printable",
		     ENC_QUOTED_PRINTABLE);
	gtk_option_menu_set_menu(GTK_OPTION_MENU(optmenu), optmenu_menu);

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
	gchar *tmp;
	pid_t pid;
	gint pipe_fds[2];

	tmp = g_strdup_printf("%s%ctmpmsg.%p", get_tmp_dir(),
			      G_DIR_SEPARATOR, compose);

	if (pipe(pipe_fds) < 0) {
		perror("pipe");
		g_free(tmp);
		return;
	}

	if ((pid = fork()) < 0) {
		perror("fork");
		g_free(tmp);
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
			fd_write_all(pipe_fds[1], "2\n", 2);
			_exit(1);
		}

		pid_ed = compose_exec_ext_editor_real(tmp);
		if (pid_ed < 0) {
			fd_write_all(pipe_fds[1], "1\n", 2);
			_exit(1);
		}

		/* wait until editor is terminated */
		waitpid(pid_ed, NULL, 0);

		fd_write_all(pipe_fds[1], "0\n", 2);

		close(pipe_fds[1]);
		_exit(0);
	}

	g_free(tmp);
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
			g_warning("External editor command line is invalid: `%s'\n",
				  prefs_common.ext_editor_cmd);
		g_snprintf(buf, sizeof(buf), def_cmd, file);
	}

	cmdline = strsplit_with_quote(buf, " ", 1024);
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

			g_warning("Terminated process group id: %d", -pgid);
			g_warning("Temporary file: %s",
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

	debug_print("Compose: input from monitoring process\n");

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
		g_warning("Couldn't exec external editor\n");
		if (unlink(compose->exteditor_file) < 0)
			FILE_OP_ERROR(compose->exteditor_file, "unlink");
	} else if (buf[0] == '2') {
		g_warning("Couldn't write to file\n");
	} else if (buf[0] == '3') {
		g_warning("Pipe read failed\n");
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
	menu_set_sensitive(ifactory, "/Message/Insert file", sensitive);
	menu_set_sensitive(ifactory, "/Message/Insert signature", sensitive);
	menu_set_sensitive(ifactory, "/Edit/Wrap current paragraph", sensitive);
	menu_set_sensitive(ifactory, "/Edit/Wrap all long lines", sensitive);
	menu_set_sensitive(ifactory, "/Edit/Edit with external editor",
			   sensitive);

	gtk_widget_set_sensitive(compose->text,                   sensitive);
	gtk_widget_set_sensitive(compose->toolbar->send_btn,      sensitive);
	gtk_widget_set_sensitive(compose->toolbar->sendl_btn,     sensitive);
	gtk_widget_set_sensitive(compose->toolbar->draft_btn,     sensitive);
	gtk_widget_set_sensitive(compose->toolbar->insert_btn,    sensitive);
	gtk_widget_set_sensitive(compose->toolbar->sig_btn,       sensitive);
	gtk_widget_set_sensitive(compose->toolbar->exteditor_btn, sensitive);
	gtk_widget_set_sensitive(compose->toolbar->linewrap_current_btn,  sensitive);
	gtk_widget_set_sensitive(compose->toolbar->linewrap_all_btn,  sensitive);
}

/**
 * compose_undo_state_changed:
 *
 * Change the sensivity of the menuentries undo and redo
 **/
static void compose_undo_state_changed(UndoMain *undostruct, gint undo_state,
				       gint redo_state, gpointer data)
{
	GtkWidget *widget = GTK_WIDGET(data);
	GtkItemFactory *ifactory;

	g_return_if_fail(widget != NULL);

	ifactory = gtk_item_factory_from_widget(widget);

	switch (undo_state) {
	case UNDO_STATE_TRUE:
		if (!undostruct->undo_state) {
			undostruct->undo_state = TRUE;
			menu_set_sensitive(ifactory, "/Edit/Undo", TRUE);
		}
		break;
	case UNDO_STATE_FALSE:
		if (undostruct->undo_state) {
			undostruct->undo_state = FALSE;
			menu_set_sensitive(ifactory, "/Edit/Undo", FALSE);
		}
		break;
	case UNDO_STATE_UNCHANGED:
		break;
	case UNDO_STATE_REFRESH:
		menu_set_sensitive(ifactory, "/Edit/Undo",
				   undostruct->undo_state);
		break;
	default:
		g_warning("Undo state not recognized");
		break;
	}

	switch (redo_state) {
	case UNDO_STATE_TRUE:
		if (!undostruct->redo_state) {
			undostruct->redo_state = TRUE;
			menu_set_sensitive(ifactory, "/Edit/Redo", TRUE);
		}
		break;
	case UNDO_STATE_FALSE:
		if (undostruct->redo_state) {
			undostruct->redo_state = FALSE;
			menu_set_sensitive(ifactory, "/Edit/Redo", FALSE);
		}
		break;
	case UNDO_STATE_UNCHANGED:
		break;
	case UNDO_STATE_REFRESH:
		menu_set_sensitive(ifactory, "/Edit/Redo",
				   undostruct->redo_state);
		break;
	default:
		g_warning("Redo state not recognized");
		break;
	}
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

static void account_activated(GtkMenuItem *menuitem, gpointer data)
{
	Compose *compose = (Compose *)data;

	PrefsAccount *ac;
	gchar *folderidentifier;

	ac = account_find_from_id(
		GPOINTER_TO_INT(gtk_object_get_user_data(GTK_OBJECT(menuitem))));
	g_return_if_fail(ac != NULL);

	if (ac != compose->account)
		compose_select_account(compose, ac, FALSE);

	/* Set message save folder */
	if (account_get_special_folder(compose->account, F_OUTBOX)) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(compose->savemsg_checkbtn), prefs_common.savemsg);
	}
	gtk_signal_connect(GTK_OBJECT(compose->savemsg_checkbtn), "toggled",
			   GTK_SIGNAL_FUNC(compose_savemsg_checkbtn_cb), compose);
			   
	gtk_editable_delete_text(GTK_EDITABLE(compose->savemsg_entry), 0, -1);
	if (account_get_special_folder(compose->account, F_OUTBOX)) {
		folderidentifier = folder_item_get_identifier(account_get_special_folder
				  (compose->account, F_OUTBOX));
		gtk_entry_set_text(GTK_ENTRY(compose->savemsg_entry), folderidentifier);
		g_free(folderidentifier);
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

static void compose_allow_user_actions (Compose *compose, gboolean allow)
{
	GtkItemFactory *ifactory = gtk_item_factory_from_widget(compose->menubar);
	toolbar_comp_set_sensitive(compose, allow);
	menu_set_sensitive(ifactory, "/Message", allow);
	menu_set_sensitive(ifactory, "/Edit", allow);
#if USE_ASPELL
	menu_set_sensitive(ifactory, "/Spelling", allow);
#endif	
	menu_set_sensitive(ifactory, "/Options", allow);
	menu_set_sensitive(ifactory, "/Tools", allow);
	menu_set_sensitive(ifactory, "/Help", allow);
}

static void compose_send_cb(gpointer data, guint action, GtkWidget *widget)
{
	Compose *compose = (Compose *)data;
	
	if (prefs_common.work_offline)
		if (alertpanel(_("Offline warning"), 
			       _("You're working offline. Override?"),
			       _("Yes"), _("No"), NULL) != G_ALERTDEFAULT)
			return;
	
	if (compose->draft_timeout_tag != -1) { /* CLAWS: disable draft timeout */
		gtk_timeout_remove(compose->draft_timeout_tag);
		compose->draft_timeout_tag = -1;
	}

	compose_send(compose);
}

static void compose_send_later_cb(gpointer data, guint action,
				  GtkWidget *widget)
{
	Compose *compose = (Compose *)data;
	gint val;

	val = compose_queue_sub(compose, NULL, NULL, TRUE);
	if (!val) 
		compose_close(compose);
}

void compose_draft (gpointer data) 
{
	compose_draft_cb(data, COMPOSE_QUIT_EDITING, NULL);	
}

static void compose_draft_cb(gpointer data, guint action, GtkWidget *widget)
{
	Compose *compose = (Compose *)data;
	FolderItem *draft;
	gchar *tmp;
	gint msgnum;
	MsgFlags flag = {0, 0};
	static gboolean lock = FALSE;
	MsgInfo *newmsginfo;
	FILE *fp;
	
	if (lock) return;

	draft = account_get_special_folder(compose->account, F_DRAFT);
	g_return_if_fail(draft != NULL);

	lock = TRUE;

	tmp = g_strdup_printf("%s%cdraft.%p", get_tmp_dir(),
			      G_DIR_SEPARATOR, compose);
	if ((fp = fopen(tmp, "wb")) == NULL) {
		FILE_OP_ERROR(tmp, "fopen");
		return;
	}

	/* chmod for security */
	if (change_file_mode_rw(fp, tmp) < 0) {
		FILE_OP_ERROR(tmp, "chmod");
		g_warning("can't change file mode\n");
	}

	/* Save draft infos */
	fprintf(fp, "X-Sylpheed-Account-Id:%d\n", compose->account->account_id);
	fprintf(fp, "S:%s\n", compose->account->address);
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(compose->savemsg_checkbtn))) {
		gchar *savefolderid;

		savefolderid = gtk_editable_get_chars(GTK_EDITABLE(compose->savemsg_entry), 0, -1);
		fprintf(fp, "SCF:%s\n", savefolderid);
		g_free(savefolderid);
	}
	if (compose->privacy_system) {
		fprintf(fp, "X-Sylpheed-Sign:%d\n", compose->use_signing);
		fprintf(fp, "X-Sylpheed-Encrypt:%d\n", compose->use_encryption);
		fprintf(fp, "X-Sylpheed-Privacy-System:%s\n", compose->privacy_system);
	}
	fprintf(fp, "\n");

	if (compose_write_to_file(compose, fp, COMPOSE_WRITE_FOR_STORE) < 0) {
		fclose(fp);
		unlink(tmp);
		g_free(tmp);
		lock = FALSE;
		return;
	}
	fclose(fp);

	folder_item_scan(draft);
	if ((msgnum = folder_item_add_msg(draft, tmp, &flag, TRUE)) < 0) {
		unlink(tmp);
		g_free(tmp);
		lock = FALSE;
		return;
	}
	g_free(tmp);
	draft->mtime = 0;	/* force updating */

	if (compose->mode == COMPOSE_REEDIT) {
		compose_remove_reedit_target(compose);
	}

	newmsginfo = folder_item_get_msginfo(draft, msgnum);
	if (newmsginfo) {
		procmsg_msginfo_unset_flags(newmsginfo, ~0, ~0);
		procmsg_msginfo_set_flags(newmsginfo, 0, MSG_DRAFT);
		if (compose_use_attach(compose))
			procmsg_msginfo_set_flags(newmsginfo, 0,
						  MSG_HAS_ATTACHMENT);

		procmsg_msginfo_free(newmsginfo);
	}
	
	folder_item_scan(draft);
	
	lock = FALSE;

	if (action == COMPOSE_QUIT_EDITING)
		compose_close(compose);
	else {
		struct stat s;
		gchar *path;

		path = folder_item_fetch_msg(draft, msgnum);
		g_return_if_fail(path != NULL);
		if (stat(path, &s) < 0) {
			FILE_OP_ERROR(path, "stat");
			g_free(path);
			lock = FALSE;
			return;
		}
		g_free(path);

		procmsg_msginfo_free(compose->targetinfo);
		compose->targetinfo = procmsg_msginfo_new();
		compose->targetinfo->msgnum = msgnum;
		compose->targetinfo->size = s.st_size;
		compose->targetinfo->mtime = s.st_mtime;
		compose->targetinfo->folder = draft;
		compose->mode = COMPOSE_REEDIT;
		
		if (action == COMPOSE_AUTO_SAVE) {
			compose->autosaved_draft = compose->targetinfo;
		}
	}
}

static void compose_attach_cb(gpointer data, guint action, GtkWidget *widget)
{
	Compose *compose = (Compose *)data;
	GList *file_list;

	if (compose->redirect_filename != NULL)
		return;

	file_list = filesel_select_multiple_files(_("Select file"), NULL);

	if (file_list) {
		GList *tmp;

		for ( tmp = file_list; tmp; tmp = tmp->next) {
			gchar *file = (gchar *) tmp->data;
			compose_attach_append(compose, file, file, NULL);
			compose_changed_cb(NULL, compose);
			g_free(file);
		}
		g_list_free(file_list);
	}		
}

static void compose_insert_file_cb(gpointer data, guint action,
				   GtkWidget *widget)
{
	Compose *compose = (Compose *)data;
	GList *file_list;

	file_list = filesel_select_multiple_files(_("Select file"), NULL);

	if (file_list) {
		GList *tmp;

		for ( tmp = file_list; tmp; tmp = tmp->next) {
			gchar *file = (gchar *) tmp->data;
			gchar *filedup = g_strdup(file);
			gchar *shortfile;
			ComposeInsertResult res;

			res = compose_insert_file(compose, file);
			shortfile = g_basename(filedup);
			if (res == COMPOSE_INSERT_READ_ERROR) {
				alertpanel_error(_("File '%s' could not be read."), shortfile);
			} else if (res == COMPOSE_INSERT_INVALID_CHARACTER) {
				alertpanel_error(_("File '%s' contained invalid characters\n"
						   "for the current encoding, insertion may be incorrect."), shortfile);
			}
			g_free(filedup);
			g_free(file);
		}
		g_list_free(file_list);
	}
}

static void compose_insert_sig_cb(gpointer data, guint action,
				  GtkWidget *widget)
{
	Compose *compose = (Compose *)data;

	compose_insert_sig(compose, FALSE);
}

static gint compose_delete_cb(GtkWidget *widget, GdkEventAny *event,
			      gpointer data)
{
	gint x, y;
	Compose *compose = (Compose *)data;

	gtkut_widget_get_uposition(widget, &x, &y);
	prefs_common.compose_x = x;
	prefs_common.compose_y = y;

	if (compose->sending)
		return TRUE;
	compose_close_cb(compose, 0, NULL);
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
			if (prefs_common.autosave)
				compose_remove_draft(compose);			
			break;
		case G_ALERTALTERNATE:
			compose_draft_cb(data, COMPOSE_QUIT_EDITING, NULL);
			return;
		default:
			return;
		}
	}

	compose_close(compose);
}

static void compose_address_cb(gpointer data, guint action, GtkWidget *widget)
{
	Compose *compose = (Compose *)data;

	addressbook_open(compose);
}

static void compose_template_activate_cb(GtkWidget *widget, gpointer data)
{
	Compose *compose = (Compose *)data;
	Template *tmpl;
	gchar *msg;
	AlertValue val;

	tmpl = gtk_object_get_data(GTK_OBJECT(widget), "template");
	g_return_if_fail(tmpl != NULL);

	msg = g_strdup_printf(_("Do you want to apply the template `%s' ?"),
			      tmpl->name);
	val = alertpanel(_("Apply template"), msg,
			 _("Replace"), _("Insert"), _("Cancel"));
	g_free(msg);

	if (val == G_ALERTDEFAULT)
		compose_template_apply(compose, tmpl, TRUE);
	else if (val == G_ALERTALTERNATE)
		compose_template_apply(compose, tmpl, FALSE);
}

static void compose_ext_editor_cb(gpointer data, guint action,
				  GtkWidget *widget)
{
	Compose *compose = (Compose *)data;

	compose_exec_ext_editor(compose);
}

static void compose_destroy_cb(GtkWidget *widget, Compose *compose)
{
	if (compose->sending)
		return;
	compose_destroy(compose);
}

static void compose_undo_cb(Compose *compose)
{
	undo_undo(compose->undostruct);
}

static void compose_redo_cb(Compose *compose)
{
	undo_redo(compose->undostruct);
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

static void compose_paste_as_quote_cb(Compose *compose)
{
	if (compose->focused_editable &&
	    GTK_WIDGET_HAS_FOCUS(compose->focused_editable)) {
		/* let text_insert() (called directly or at a later time
		 * after the gtk_editable_paste_clipboard) know that 
		 * text is to be inserted as a quotation. implemented
		 * by using a simple refcount... */
		gint paste_as_quotation = GPOINTER_TO_INT(gtk_object_get_data(
						GTK_OBJECT(compose->focused_editable),
						"paste_as_quotation"));
		gtk_object_set_data(GTK_OBJECT(compose->focused_editable),
				    "paste_as_quotation",
				    GINT_TO_POINTER(paste_as_quotation + 1));
		gtk_editable_paste_clipboard
			(GTK_EDITABLE(compose->focused_editable));
	}
}

static void compose_allsel_cb(Compose *compose)
{
	if (compose->focused_editable &&
	    GTK_WIDGET_HAS_FOCUS(compose->focused_editable))
		gtk_editable_select_region
			(GTK_EDITABLE(compose->focused_editable), 0, -1);
}

static void compose_gtk_stext_action_cb(Compose *compose,
					ComposeCallGtkSTextAction action)
{
	GtkSText *text = GTK_STEXT(compose->text);
	static struct {
		void (*do_action) (GtkSText *text);
	} action_table[] = {
		{gtk_stext_move_beginning_of_line},
		{gtk_stext_move_forward_character},
		{gtk_stext_move_backward_character},
		{gtk_stext_move_forward_word},
		{gtk_stext_move_backward_word},
		{gtk_stext_move_end_of_line},
		{gtk_stext_move_next_line},
		{gtk_stext_move_previous_line},
		{gtk_stext_delete_forward_character},
		{gtk_stext_delete_backward_character},
		{gtk_stext_delete_forward_word},
		{gtk_stext_delete_backward_word},
		{gtk_stext_delete_line},
		{gtk_stext_delete_line}, /* gtk_stext_delete_line_n */
		{gtk_stext_delete_to_line_end}
	};

	if (!GTK_WIDGET_HAS_FOCUS(text)) return;

	if (action >= COMPOSE_CALL_GTK_STEXT_MOVE_BEGINNING_OF_LINE &&
	    action <= COMPOSE_CALL_GTK_STEXT_DELETE_TO_LINE_END)
		action_table[action].do_action(text);
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

static void compose_toggle_autowrap_cb(gpointer data, guint action,
				       GtkWidget *widget)
{
	Compose *compose = (Compose *)data;

	compose->autowrap = GTK_CHECK_MENU_ITEM(widget)->active;
	if (compose->autowrap)
		compose_wrap_line_all_full(compose, TRUE);
}

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

static void activate_privacy_system(Compose *compose, PrefsAccount *account) 
{
	g_free(compose->privacy_system);
	compose->privacy_system = g_strdup(account->default_privacy_system);
	compose_update_privacy_system_menu_item(compose);
}

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
		compose_attach_append
			(compose, (const gchar *)tmp->data,
			 (const gchar *)tmp->data, NULL);
	if (list) compose_changed_cb(NULL, compose);
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

#if 0 /* NEW COMPOSE GUI */
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
#endif

static void compose_toggle_return_receipt_cb(gpointer data, guint action,
					     GtkWidget *widget)
{
	Compose *compose = (Compose *)data;

	if (GTK_CHECK_MENU_ITEM(widget)->active)
		compose->return_receipt = TRUE;
	else
		compose->return_receipt = FALSE;
}

static void compose_toggle_remove_refs_cb(gpointer data, guint action,
					     GtkWidget *widget)
{
	Compose *compose = (Compose *)data;

	if (GTK_CHECK_MENU_ITEM(widget)->active)
		compose->remove_references = TRUE;
	else
		compose->remove_references = FALSE;
}

void compose_headerentry_key_press_event_cb(GtkWidget *entry,
					    GdkEventKey *event,
					    ComposeHeaderEntry *headerentry)
{
	if ((g_slist_length(headerentry->compose->header_list) > 0) &&
	    ((headerentry->headernum + 1) != headerentry->compose->header_nextrow) &&
	    !(event->state & GDK_MODIFIER_MASK) &&
	    (event->keyval == GDK_BackSpace) &&
	    (strlen(gtk_entry_get_text(GTK_ENTRY(entry))) == 0)) {
		gtk_container_remove
			(GTK_CONTAINER(headerentry->compose->header_table),
			 headerentry->combo);
		gtk_container_remove
			(GTK_CONTAINER(headerentry->compose->header_table),
			 headerentry->entry);
		headerentry->compose->header_list =
			g_slist_remove(headerentry->compose->header_list,
				       headerentry);
		g_free(headerentry);
	} else 	if (event->keyval == GDK_Tab) {
		if (headerentry->compose->header_last == headerentry) {
			/* Override default next focus, and give it to subject_entry
			 * instead of notebook tabs
			 */
			gtk_signal_emit_stop_by_name(GTK_OBJECT(entry), "key-press-event"); 
			gtk_widget_grab_focus(headerentry->compose->subject_entry);
		}
	}

}

void compose_headerentry_changed_cb(GtkWidget *entry,
				    ComposeHeaderEntry *headerentry)
{
	if (strlen(gtk_entry_get_text(GTK_ENTRY(entry))) != 0) {
		headerentry->compose->header_list =
			g_slist_append(headerentry->compose->header_list,
				       headerentry);
		compose_create_header_entry(headerentry->compose);
		gtk_signal_disconnect_by_func
			(GTK_OBJECT(entry),
			 GTK_SIGNAL_FUNC(compose_headerentry_changed_cb),
			 headerentry);
		/* Automatically scroll down */
		compose_show_first_last_header(headerentry->compose, FALSE);
		
	}
}

static gboolean compose_headerentry_button_pressed
	(GtkWidget *entry, GdkEventButton *event, gpointer data)
{
	/* if this is a lclick, grab the focus */
	if (event->button == 1)
		gtk_widget_grab_focus(entry);
	return FALSE;
}

static void compose_show_first_last_header(Compose *compose, gboolean show_first)
{
	GtkAdjustment *vadj;

	g_return_if_fail(compose);
	g_return_if_fail(GTK_IS_WIDGET(compose->header_table));
	g_return_if_fail(GTK_IS_VIEWPORT(compose->header_table->parent));

	vadj = gtk_viewport_get_vadjustment(GTK_VIEWPORT(compose->header_table->parent));
	gtk_adjustment_set_value(vadj, (show_first ? vadj->lower : vadj->upper));
}

static void text_activated(GtkWidget *widget, Compose *compose)
{
	compose_send_control_enter(compose);
}

static void text_inserted(GtkWidget *widget, const gchar *text,
			  gint length, gint *position, Compose *compose)
{
	GtkEditable *editable = GTK_EDITABLE(widget);
	gint paste_as_quotation = GPOINTER_TO_INT(gtk_object_get_data
				(GTK_OBJECT(widget), "paste_as_quotation"));

	gtk_signal_handler_block_by_func(GTK_OBJECT(widget),
					 GTK_SIGNAL_FUNC(text_inserted),
					 compose);
	if (paste_as_quotation) {
		gchar *new_text;
		gchar *qmark;
		gint pos;

		new_text = g_strndup(text, length);
		if (prefs_common.quotemark && *prefs_common.quotemark)
			qmark = prefs_common.quotemark;
		else
			qmark = "> ";
		gtk_stext_set_point(GTK_STEXT(widget), *position);
		compose_quote_fmt(compose, NULL, "%Q", qmark, new_text);
		pos = gtk_stext_get_point(GTK_STEXT(widget));
		gtk_editable_set_position(editable, pos);
		*position = pos;
		g_free(new_text);
		gtk_object_set_data(GTK_OBJECT(widget), "paste_as_quotation",
				    GINT_TO_POINTER(paste_as_quotation - 1));
	} else
		gtk_editable_insert_text(editable, text, length, position);

	if (compose->autowrap)
		compose_wrap_line_all_full(compose, TRUE);

	gtk_signal_handler_unblock_by_func(GTK_OBJECT(widget),
					   GTK_SIGNAL_FUNC(text_inserted),
					   compose);
	gtk_signal_emit_stop_by_name(GTK_OBJECT(editable), "insert_text");

	if (prefs_common.autosave && 
	    gtk_stext_get_length(GTK_STEXT(widget)) % prefs_common.autosave_length == 0)
		compose->draft_timeout_tag = gtk_timeout_add
			(500, (GtkFunction) compose_defer_auto_save_draft, compose);
}

static gint compose_defer_auto_save_draft(Compose *compose)
{
	compose->draft_timeout_tag = -1;
	compose_draft_cb((gpointer)compose, COMPOSE_AUTO_SAVE, NULL);
	return FALSE;
}

static gboolean compose_send_control_enter(Compose *compose)
{
	GdkEvent *ev;
	GdkEventKey *kev;
	GtkItemFactory *ifactory;
	GtkAccelEntry *accel;
	GtkWidget *send_menu;
	GSList *list;
	GdkModifierType ignored_mods =
		(GDK_LOCK_MASK | GDK_MOD2_MASK | GDK_MOD3_MASK |
		 GDK_MOD4_MASK | GDK_MOD5_MASK);

	ev = gtk_get_current_event();
	if (ev->type != GDK_KEY_PRESS) return FALSE;

	kev = (GdkEventKey *)ev;
	if (!(kev->keyval == GDK_Return && (kev->state & GDK_CONTROL_MASK)))
		return FALSE;

	if (compose->exteditor_tag != -1)
		return FALSE;

	ifactory = gtk_item_factory_from_widget(compose->menubar);
	send_menu = gtk_item_factory_get_widget(ifactory, "/Message/Send");
	list = gtk_accel_group_entries_from_object(GTK_OBJECT(send_menu));
	if (!list)
		return FALSE;

	accel = (GtkAccelEntry *)list->data;
	if (accel && accel->accelerator_key == kev->keyval &&
	    (accel->accelerator_mods & ~ignored_mods) ==
	    (kev->state & ~ignored_mods)) {
		compose_send_cb(compose, 0, NULL);
		return TRUE;
	}

	return FALSE;
}

#if USE_ASPELL
static void compose_check_all(Compose *compose)
{
	if (compose->gtkaspell)
		gtkaspell_check_all(compose->gtkaspell);
}

static void compose_highlight_all(Compose *compose)
{
	if (compose->gtkaspell)
		gtkaspell_highlight_all(compose->gtkaspell);
}

static void compose_check_backwards(Compose *compose)
{
	if (compose->gtkaspell)	
		gtkaspell_check_backwards(compose->gtkaspell);
	else {
		GtkItemFactory *ifactory;
		ifactory = gtk_item_factory_from_widget(compose->popupmenu);
		menu_set_sensitive(ifactory, "/Edit/Check backwards misspelled word", FALSE);
		menu_set_sensitive(ifactory, "/Edit/Forward to next misspelled word", FALSE);
	}
}

static void compose_check_forwards_go(Compose *compose)
{
	if (compose->gtkaspell)	
		gtkaspell_check_forwards_go(compose->gtkaspell);
	else {
		GtkItemFactory *ifactory;
		ifactory = gtk_item_factory_from_widget(compose->popupmenu);
		menu_set_sensitive(ifactory, "/Edit/Check backwards misspelled word", FALSE);
		menu_set_sensitive(ifactory, "/Edit/Forward to next misspelled word", FALSE);
	}
}
#endif

/*!
 *\brief	Guess originating forward account from MsgInfo and several 
 *		"common preference" settings. Return NULL if no guess. 
 */
static PrefsAccount *compose_guess_forward_account_from_msginfo(MsgInfo *msginfo)
{
	PrefsAccount *account = NULL;
	
	g_return_val_if_fail(msginfo, NULL);
	g_return_val_if_fail(msginfo->folder, NULL);
	g_return_val_if_fail(msginfo->folder->prefs, NULL);

	if (msginfo->folder->prefs->enable_default_account)
		account = account_find_from_id(msginfo->folder->prefs->default_account);
		
	if (!account) 
		account = msginfo->folder->folder->account;
		
	if (!account && msginfo->to && prefs_common.forward_account_autosel) {
		gchar *to;
		Xstrdup_a(to, msginfo->to, return NULL);
		extract_address(to);
		account = account_find_from_address(to);
	}

	if (!account && prefs_common.forward_account_autosel) {
		gchar cc[BUFFSIZE];
		if (!procheader_get_header_from_msginfo
			(msginfo, cc,sizeof cc , "CC:")) { /* Found a CC header */
		        extract_address(cc);
		        account = account_find_from_address(cc);
                }
	}
	
	return account;
}

static void compose_close(Compose *compose)
{
	gint x, y;

	g_return_if_fail(compose);
	gtkut_widget_get_uposition(compose->window, &x, &y);
	prefs_common.compose_x = x;
	prefs_common.compose_y = y;
	gtk_widget_destroy(compose->window);
}

/**
 * Add entry field for each address in list.
 * \param compose     E-Mail composition object.
 * \param listAddress List of (formatted) E-Mail addresses.
 */
static void compose_add_field_list( Compose *compose, GList *listAddress ) {
	GList *node;
	gchar *addr;
	node = listAddress;
	while( node ) {
		addr = ( gchar * ) node->data;
		compose_entry_append( compose, addr, COMPOSE_TO );
		node = g_list_next( node );
	}
}

void compose_reply_from_messageview(MessageView *msgview, GSList *msginfo_list, 
				    guint action)
{
	gchar *body;
	GSList *new_msglist = NULL;
	MsgInfo *tmp_msginfo = NULL;

	g_return_if_fail(msgview != NULL);

	g_return_if_fail(msginfo_list != NULL);

 	if (g_slist_length(msginfo_list) == 1) {
 		MimeInfo *mimeinfo = messageview_get_selected_mime_part(msgview);
 		MsgInfo *orig_msginfo = (MsgInfo *)msginfo_list->data;
 		
 		if (mimeinfo != NULL && mimeinfo->type == MIMETYPE_MESSAGE && 
 		    !g_strcasecmp(mimeinfo->subtype, "rfc822")) {
 	    		
 			tmp_msginfo = procmsg_msginfo_new_from_mimeinfo(
 						orig_msginfo, mimeinfo);
 			if (tmp_msginfo != NULL) {
 				new_msglist = g_slist_append(NULL, tmp_msginfo);
 			} 
 		}
 	}

	body = messageview_get_selection(msgview);

	if (new_msglist) {
		compose_reply_mode((ComposeMode)action, new_msglist, body);
		procmsg_msginfo_free(tmp_msginfo);
		g_slist_free(new_msglist);
	} else
		compose_reply_mode((ComposeMode)action, msginfo_list, body);

	g_free(body);
}

/*
 * End of Source.
 */
