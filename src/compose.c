/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2006 Hiroyuki Yamamoto and the Sylpheed-Claws team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "defs.h"

#ifndef PANGO_ENABLE_ENGINE
#  define PANGO_ENABLE_ENGINE
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkmenu.h>
#include <gtk/gtkmenuitem.h>
#include <gtk/gtkitemfactory.h>
#include <gtk/gtkcheckmenuitem.h>
#include <gtk/gtkoptionmenu.h>
#include <gtk/gtkwidget.h>
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
#include <gtk/gtktreeview.h>
#include <gtk/gtkliststore.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtktreemodel.h>

#include <gtk/gtkdnd.h>
#include <gtk/gtkclipboard.h>
#include <pango/pango-break.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#if HAVE_SYS_WAIT_H
#  include <sys/wait.h>
#endif
#include <signal.h>
#include <errno.h>
#ifndef G_OS_WIN32  /* fixme we should have a configure test. */
#include <libgen.h>
#endif

#if (HAVE_WCTYPE_H && HAVE_WCHAR_H)
#  include <wchar.h>
#  include <wctype.h>
#endif

#include "sylpheed.h"
#include "main.h"
#include "mainwindow.h"
#include "compose.h"
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
#include "undo.h"
#include "foldersel.h"
#include "toolbar.h"
#include "inc.h"
#include "message_search.h"
#include "combobox.h"

enum
{
	COL_MIMETYPE = 0,
	COL_SIZE     = 1,
	COL_NAME     = 2,
	COL_DATA     = 3,
	COL_AUTODATA = 4,
	N_COL_COLUMNS
};

#define N_ATTACH_COLS	(N_COL_COLUMNS)

typedef enum
{
	COMPOSE_CALL_ADVANCED_ACTION_MOVE_BEGINNING_OF_LINE,
	COMPOSE_CALL_ADVANCED_ACTION_MOVE_FORWARD_CHARACTER,
	COMPOSE_CALL_ADVANCED_ACTION_MOVE_BACKWARD_CHARACTER,
	COMPOSE_CALL_ADVANCED_ACTION_MOVE_FORWARD_WORD,
	COMPOSE_CALL_ADVANCED_ACTION_MOVE_BACKWARD_WORD,
	COMPOSE_CALL_ADVANCED_ACTION_MOVE_END_OF_LINE,
	COMPOSE_CALL_ADVANCED_ACTION_MOVE_NEXT_LINE,
	COMPOSE_CALL_ADVANCED_ACTION_MOVE_PREVIOUS_LINE,
	COMPOSE_CALL_ADVANCED_ACTION_DELETE_FORWARD_CHARACTER,
	COMPOSE_CALL_ADVANCED_ACTION_DELETE_BACKWARD_CHARACTER,
	COMPOSE_CALL_ADVANCED_ACTION_DELETE_FORWARD_WORD,
	COMPOSE_CALL_ADVANCED_ACTION_DELETE_BACKWARD_WORD,
	COMPOSE_CALL_ADVANCED_ACTION_DELETE_LINE,
	COMPOSE_CALL_ADVANCED_ACTION_DELETE_LINE_N,
	COMPOSE_CALL_ADVANCED_ACTION_DELETE_TO_LINE_END
} ComposeCallAdvancedAction;

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

static GList *compose_list = NULL;

Compose *compose_generic_new			(PrefsAccount	*account,
						 const gchar	*to,
						 FolderItem	*item,
						 GPtrArray 	*attach_files,
						 GList          *listAddress );

static Compose *compose_create			(PrefsAccount	*account,
						 ComposeMode	 mode,
						 gboolean batch);

static GtkWidget *compose_account_option_menu_create
						(Compose	*compose);
static void compose_set_out_encoding		(Compose	*compose);
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
						 const gchar	*body,
						 gboolean	 rewrap);

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

static void compose_beautify_paragraph		(Compose	*compose,
						 GtkTextIter	*par_iter,
						 gboolean	 force);
static void compose_wrap_all			(Compose	*compose);
static void compose_wrap_all_full		(Compose	*compose,
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
						 gint 		 action,
						 gboolean	 attach_parts);
static gint compose_write_body_to_file		(Compose	*compose,
						 const gchar	*file);
static gint compose_remove_reedit_target	(Compose	*compose,
						 gboolean	 force);
void compose_remove_draft			(Compose	*compose);
static gint compose_queue_sub			(Compose	*compose,
						 gint		*msgnum,
						 FolderItem	**item,
						 gchar		**msgpath,
						 gboolean	check_subject,
						 gboolean 	remove_reedit_target);
static void compose_add_attachments		(Compose	*compose,
						 MimeInfo	*parent);
static gchar *compose_get_header		(Compose	*compose);

static void compose_convert_header		(Compose	*compose,
						 gchar		*dest,
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
static gboolean attach_property_key_pressed	(GtkWidget	*widget,
						 GdkEventKey	*event,
						 gboolean	*cancelled);

static void compose_exec_ext_editor		(Compose	*compose);
#ifdef G_OS_UNIX
static gint compose_exec_ext_editor_real	(const gchar	*file);
static gboolean compose_ext_editor_kill		(Compose	*compose);
static gboolean compose_input_cb		(GIOChannel	*source,
						 GIOCondition	 condition,
						 gpointer	 data);
static void compose_set_ext_editor_sensitive	(Compose	*compose,
						 gboolean	 sensitive);
#endif /* G_OS_UNIX */

static void compose_undo_state_changed		(UndoMain	*undostruct,
						 gint		 undo_state,
						 gint		 redo_state,
						 gpointer	 data);

static void compose_create_header_entry	(Compose *compose);
static void compose_add_header_entry	(Compose *compose, gchar *header, gchar *text);
static void compose_update_priority_menu_item(Compose * compose);
#if USE_ASPELL
static void compose_spell_menu_changed	(void *data);
#endif
static void compose_add_field_list	( Compose *compose,
					  GList *listAddress );

/* callback functions */

static gboolean compose_edit_size_alloc (GtkEditable	*widget,
					 GtkAllocation	*allocation,
					 GtkSHRuler	*shruler);
static void account_activated		(GtkComboBox *optmenu,
					 gpointer	 data);
static void attach_selected		(GtkTreeView	*tree_view, 
					 GtkTreePath	*tree_path,
					 GtkTreeViewColumn *column, 
					 Compose *compose);
static gboolean attach_button_pressed	(GtkWidget	*widget,
					 GdkEventButton	*event,
					 gpointer	 data);
static gboolean attach_key_pressed	(GtkWidget	*widget,
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

static void compose_set_encoding_cb	(gpointer	 data,
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

static void compose_undo_cb		(Compose	*compose);
static void compose_redo_cb		(Compose	*compose);
static void compose_cut_cb		(Compose	*compose);
static void compose_copy_cb		(Compose	*compose);
static void compose_paste_cb		(Compose	*compose);
static void compose_paste_as_quote_cb	(Compose	*compose);
static void compose_paste_no_wrap_cb	(Compose	*compose);
static void compose_paste_wrap_cb	(Compose	*compose);
static void compose_allsel_cb		(Compose	*compose);

static void compose_advanced_action_cb	(Compose		   *compose,
					 ComposeCallAdvancedAction  action);

static void compose_grab_focus_cb	(GtkWidget	*widget,
					 Compose	*compose);

static void compose_changed_cb		(GtkTextBuffer	*textbuf,
					 Compose	*compose);

static void compose_wrap_cb		(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void compose_find_cb		(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);
static void compose_toggle_autowrap_cb	(gpointer	 data,
					 guint		 action,
					 GtkWidget	*widget);

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
static void compose_update_privacy_system_menu_item(Compose * compose, gboolean warn);
static void activate_privacy_system     (Compose *compose, 
                                         PrefsAccount *account,
					 gboolean warn);
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
static void compose_header_drag_received_cb (GtkWidget		*widget,
					     GdkDragContext	*drag_context,
					     gint		 x,
					     gint		 y,
					     GtkSelectionData	*data,
					     guint		 info,
					     guint		 time,
					     gpointer		 user_data);

static gboolean compose_drag_drop	    (GtkWidget *widget,
					     GdkDragContext *drag_context,
					     gint x, gint y,
					     guint time, gpointer user_data);

static void text_inserted		(GtkTextBuffer	*buffer,
					 GtkTextIter	*iter,
					 const gchar	*text,
					 gint		 len,
					 Compose	*compose);
static Compose *compose_generic_reply(MsgInfo *msginfo, gboolean quote,
				  gboolean to_all, gboolean to_ml,
				  gboolean to_sender,
				  gboolean followup_and_reply_to,
				  const gchar *body);

gboolean compose_headerentry_changed_cb	   (GtkWidget	       *entry,
					    ComposeHeaderEntry *headerentry);
gboolean compose_headerentry_key_press_event_cb(GtkWidget	       *entry,
					    GdkEventKey        *event,
					    ComposeHeaderEntry *headerentry);

static void compose_show_first_last_header (Compose *compose, gboolean show_first);

static void compose_allow_user_actions (Compose *compose, gboolean allow);

#if USE_ASPELL
static void compose_check_all		   (Compose *compose);
static void compose_highlight_all	   (Compose *compose);
static void compose_check_backwards	   (Compose *compose);
static void compose_check_forwards_go	   (Compose *compose);
#endif

static gint compose_defer_auto_save_draft	(Compose	*compose);
static PrefsAccount *compose_guess_forward_account_from_msginfo	(MsgInfo *msginfo);

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
	{N_("/_Edit/Special paste"),	NULL, NULL, 0, "<Branch>"},
	{N_("/_Edit/Special paste/as _quotation"),
					NULL, compose_paste_as_quote_cb, 0, NULL},
	{N_("/_Edit/Special paste/_wrapped"),
					NULL, compose_paste_wrap_cb, 0, NULL},
	{N_("/_Edit/Special paste/_unwrapped"),
					NULL, compose_paste_no_wrap_cb, 0, NULL},
	{N_("/_Edit/Select _all"),	"<control>A", compose_allsel_cb, 0, NULL},
	{N_("/_Edit/A_dvanced"),	NULL, NULL, 0, "<Branch>"},
	{N_("/_Edit/A_dvanced/Move a character backward"),
					"<shift><control>B",
					compose_advanced_action_cb,
					COMPOSE_CALL_ADVANCED_ACTION_MOVE_BACKWARD_CHARACTER,
					NULL},
	{N_("/_Edit/A_dvanced/Move a character forward"),
					"<shift><control>F",
					compose_advanced_action_cb,
					COMPOSE_CALL_ADVANCED_ACTION_MOVE_FORWARD_CHARACTER,
					NULL},
	{N_("/_Edit/A_dvanced/Move a word backward"),
					NULL, /* "<alt>B" */
					compose_advanced_action_cb,
					COMPOSE_CALL_ADVANCED_ACTION_MOVE_BACKWARD_WORD,
					NULL},
	{N_("/_Edit/A_dvanced/Move a word forward"),
					NULL, /* "<alt>F" */
					compose_advanced_action_cb,
					COMPOSE_CALL_ADVANCED_ACTION_MOVE_FORWARD_WORD,
					NULL},
	{N_("/_Edit/A_dvanced/Move to beginning of line"),
					NULL, /* "<control>A" */
					compose_advanced_action_cb,
					COMPOSE_CALL_ADVANCED_ACTION_MOVE_BEGINNING_OF_LINE,
					NULL},
	{N_("/_Edit/A_dvanced/Move to end of line"),
					"<control>E",
					compose_advanced_action_cb,
					COMPOSE_CALL_ADVANCED_ACTION_MOVE_END_OF_LINE,
					NULL},
	{N_("/_Edit/A_dvanced/Move to previous line"),
					"<control>P",
					compose_advanced_action_cb,
					COMPOSE_CALL_ADVANCED_ACTION_MOVE_PREVIOUS_LINE,
					NULL},
	{N_("/_Edit/A_dvanced/Move to next line"),
					"<control>N",
					compose_advanced_action_cb,
					COMPOSE_CALL_ADVANCED_ACTION_MOVE_NEXT_LINE,
					NULL},
	{N_("/_Edit/A_dvanced/Delete a character backward"),
					"<control>H",
					compose_advanced_action_cb,
					COMPOSE_CALL_ADVANCED_ACTION_DELETE_BACKWARD_CHARACTER,
					NULL},
	{N_("/_Edit/A_dvanced/Delete a character forward"),
					"<control>D",
					compose_advanced_action_cb,
					COMPOSE_CALL_ADVANCED_ACTION_DELETE_FORWARD_CHARACTER,
					NULL},
	{N_("/_Edit/A_dvanced/Delete a word backward"),
					NULL, /* "<control>W" */
					compose_advanced_action_cb,
					COMPOSE_CALL_ADVANCED_ACTION_DELETE_BACKWARD_WORD,
					NULL},
	{N_("/_Edit/A_dvanced/Delete a word forward"),
					NULL, /* "<alt>D", */
					compose_advanced_action_cb,
					COMPOSE_CALL_ADVANCED_ACTION_DELETE_FORWARD_WORD,
					NULL},
	{N_("/_Edit/A_dvanced/Delete line"),
					"<control>U",
					compose_advanced_action_cb,
					COMPOSE_CALL_ADVANCED_ACTION_DELETE_LINE,
					NULL},
	{N_("/_Edit/A_dvanced/Delete entire line"),
					NULL,
					compose_advanced_action_cb,
					COMPOSE_CALL_ADVANCED_ACTION_DELETE_LINE_N,
					NULL},
	{N_("/_Edit/A_dvanced/Delete to end of line"),
					"<control>K",
					compose_advanced_action_cb,
					COMPOSE_CALL_ADVANCED_ACTION_DELETE_TO_LINE_END,
					NULL},
	{N_("/_Edit/---"),		NULL, NULL, 0, "<Separator>"},
	{N_("/_Edit/_Find"),
					"<control>F", compose_find_cb, 0, NULL},
	{N_("/_Edit/---"),			NULL, NULL, 0, "<Separator>"},
	{N_("/_Edit/_Wrap current paragraph"),
					"<control>L", compose_wrap_cb, 0, NULL},
	{N_("/_Edit/Wrap all long _lines"),
					"<control><alt>L", compose_wrap_cb, 1, NULL},
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
	{N_("/_Spelling/Options"),
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
	{N_("/_Options/---"),		NULL,		NULL,	0, "<Separator>"},
	{N_("/_Options/Remo_ve references"),	NULL, compose_toggle_remove_refs_cb, 0, "<ToggleItem>"},
	{N_("/_Options/---"),		NULL,		NULL,	0, "<Separator>"},

#define ENC_ACTION(action) \
	NULL, compose_set_encoding_cb, action, \
	"/Options/Character encoding/Automatic"

	{N_("/_Options/Character _encoding"), NULL, NULL, 0, "<Branch>"},
	{N_("/_Options/Character _encoding/_Automatic"),
			NULL, compose_set_encoding_cb, C_AUTO, "<RadioItem>"},
	{N_("/_Options/Character _encoding/---"), NULL, NULL, 0, "<Separator>"},

	{N_("/_Options/Character _encoding/7bit ascii (US-ASC_II)"),
	 ENC_ACTION(C_US_ASCII)},
	{N_("/_Options/Character _encoding/Unicode (_UTF-8)"),
	 ENC_ACTION(C_UTF_8)},
	{N_("/_Options/Character _encoding/---"), NULL, NULL, 0, "<Separator>"},

	{N_("/_Options/Character _encoding/Western European (ISO-8859-_1)"),
	 ENC_ACTION(C_ISO_8859_1)},
	{N_("/_Options/Character _encoding/Western European (ISO-8859-15)"),
	 ENC_ACTION(C_ISO_8859_15)},
	{N_("/_Options/Character _encoding/Western European (Windows-1252)"),
	 ENC_ACTION(C_WINDOWS_1252)},
	{N_("/_Options/Character _encoding/---"), NULL, NULL, 0, "<Separator>"},

	{N_("/_Options/Character _encoding/Central European (ISO-8859-_2)"),
	 ENC_ACTION(C_ISO_8859_2)},
	{N_("/_Options/Character _encoding/---"), NULL, NULL, 0, "<Separator>"},

	{N_("/_Options/Character _encoding/_Baltic (ISO-8859-13)"),
	 ENC_ACTION(C_ISO_8859_13)},
	{N_("/_Options/Character _encoding/Baltic (ISO-8859-_4)"),
	 ENC_ACTION(C_ISO_8859_4)},
	{N_("/_Options/Character _encoding/---"), NULL, NULL, 0, "<Separator>"},

	{N_("/_Options/Character _encoding/Greek (ISO-8859-_7)"),
	 ENC_ACTION(C_ISO_8859_7)},
	{N_("/_Options/Character _encoding/---"), NULL, NULL, 0, "<Separator>"},

	{N_("/_Options/Character _encoding/Hebrew (ISO-8859-_8)"),
	 ENC_ACTION(C_ISO_8859_8)},
	{N_("/_Options/Character _encoding/Hebrew (Windows-1255)"),
	 ENC_ACTION(C_WINDOWS_1255)},
	{N_("/_Options/Character _encoding/---"), NULL, NULL, 0, "<Separator>"},

	{N_("/_Options/Character _encoding/Arabic (ISO-8859-_6)"),
	 ENC_ACTION(C_ISO_8859_6)},
	{N_("/_Options/Character _encoding/Arabic (Windows-1256)"),
	 ENC_ACTION(C_CP1256)},
	{N_("/_Options/Character _encoding/---"), NULL, NULL, 0, "<Separator>"},

	{N_("/_Options/Character _encoding/Turkish (ISO-8859-_9)"),
	 ENC_ACTION(C_ISO_8859_9)},
	{N_("/_Options/Character _encoding/---"), NULL, NULL, 0, "<Separator>"},

	{N_("/_Options/Character _encoding/Cyrillic (ISO-8859-_5)"),
	 ENC_ACTION(C_ISO_8859_5)},
	{N_("/_Options/Character _encoding/Cyrillic (KOI8-_R)"),
	 ENC_ACTION(C_KOI8_R)},
	{N_("/_Options/Character _encoding/Cyrillic (KOI8-U)"),
	 ENC_ACTION(C_KOI8_U)},
	{N_("/_Options/Character _encoding/Cyrillic (Windows-1251)"),
	 ENC_ACTION(C_WINDOWS_1251)},
	{N_("/_Options/Character _encoding/---"), NULL, NULL, 0, "<Separator>"},

	{N_("/_Options/Character _encoding/Japanese (ISO-2022-_JP)"),
	 ENC_ACTION(C_ISO_2022_JP)},
	{N_("/_Options/Character _encoding/---"), NULL, NULL, 0, "<Separator>"},

	{N_("/_Options/Character _encoding/Simplified Chinese (_GB2312)"),
	 ENC_ACTION(C_GB2312)},
	{N_("/_Options/Character _encoding/Simplified Chinese (GBK)"),
	 ENC_ACTION(C_GBK)},
	{N_("/_Options/Character _encoding/Traditional Chinese (_Big5)"),
	 ENC_ACTION(C_BIG5)},
	{N_("/_Options/Character _encoding/Traditional Chinese (EUC-_TW)"),
	 ENC_ACTION(C_EUC_TW)},
	{N_("/_Options/Character _encoding/---"), NULL, NULL, 0, "<Separator>"},

	{N_("/_Options/Character _encoding/Korean (EUC-_KR)"),
	 ENC_ACTION(C_EUC_KR)},
	{N_("/_Options/Character _encoding/---"), NULL, NULL, 0, "<Separator>"},

	{N_("/_Options/Character _encoding/Thai (TIS-620)"),
	 ENC_ACTION(C_TIS_620)},
	{N_("/_Options/Character _encoding/Thai (Windows-874)"),
	 ENC_ACTION(C_WINDOWS_874)},

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
	{"text/uri-list", 0, 0},
	{"UTF8_STRING", 0, 0},
	{"text/plain", 0, 0}
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

static GdkColor quote_color1 = 
	{(gulong)0, (gushort)0, (gushort)0, (gushort)0};
static GdkColor quote_color2 = 
	{(gulong)0, (gushort)0, (gushort)0, (gushort)0};
static GdkColor quote_color3 = 
	{(gulong)0, (gushort)0, (gushort)0, (gushort)0};

static GdkColor quote_bgcolor1 = 
	{(gulong)0, (gushort)0, (gushort)0, (gushort)0};
static GdkColor quote_bgcolor2 = 
	{(gulong)0, (gushort)0, (gushort)0, (gushort)0};
static GdkColor quote_bgcolor3 = 
	{(gulong)0, (gushort)0, (gushort)0, (gushort)0};

static GdkColor signature_color = {
	(gulong)0,
	(gushort)0x7fff,
	(gushort)0x7fff,
	(gushort)0x7fff
};

static GdkColor uri_color = {
	(gulong)0,
	(gushort)0,
	(gushort)0,
	(gushort)0
};

static void compose_create_tags(GtkTextView *text, Compose *compose)
{
	GtkTextBuffer *buffer;
	GdkColor black = {(gulong)0, (gushort)0, (gushort)0, (gushort)0};
	GdkColormap *cmap;
	GdkColor color[8];
	gboolean success[8];
	int i;

	buffer = gtk_text_view_get_buffer(text);

	if (prefs_common.enable_color) {
		/* grab the quote colors, converting from an int to a GdkColor */
		gtkut_convert_int_to_gdk_color(prefs_common.quote_level1_col,
					       &quote_color1);
		gtkut_convert_int_to_gdk_color(prefs_common.quote_level2_col,
					       &quote_color2);
		gtkut_convert_int_to_gdk_color(prefs_common.quote_level3_col,
					       &quote_color3);
		gtkut_convert_int_to_gdk_color(prefs_common.quote_level1_bgcol,
					       &quote_bgcolor1);
		gtkut_convert_int_to_gdk_color(prefs_common.quote_level2_bgcol,
					       &quote_bgcolor2);
		gtkut_convert_int_to_gdk_color(prefs_common.quote_level3_bgcol,
					       &quote_bgcolor3);
		gtkut_convert_int_to_gdk_color(prefs_common.signature_col,
					       &signature_color);
		gtkut_convert_int_to_gdk_color(prefs_common.uri_col,
					       &uri_color);
	} else {
		signature_color = quote_color1 = quote_color2 = quote_color3 = 
			quote_bgcolor1 = quote_bgcolor2 = quote_bgcolor3 = uri_color = black;
	}

	if (prefs_common.enable_color && prefs_common.enable_bgcolor) {
		gtk_text_buffer_create_tag(buffer, "quote0",
					   "foreground-gdk", &quote_color1,
					   "paragraph-background-gdk", &quote_bgcolor1,
					   NULL);
		gtk_text_buffer_create_tag(buffer, "quote1",
					   "foreground-gdk", &quote_color2,
					   "paragraph-background-gdk", &quote_bgcolor2,
					   NULL);
		gtk_text_buffer_create_tag(buffer, "quote2",
					   "foreground-gdk", &quote_color3,
					   "paragraph-background-gdk", &quote_bgcolor3,
					   NULL);
	} else {
		gtk_text_buffer_create_tag(buffer, "quote0",
					   "foreground-gdk", &quote_color1,
					   NULL);
		gtk_text_buffer_create_tag(buffer, "quote1",
					   "foreground-gdk", &quote_color2,
					   NULL);
		gtk_text_buffer_create_tag(buffer, "quote2",
					   "foreground-gdk", &quote_color3,
					   NULL);
	}
	
 	gtk_text_buffer_create_tag(buffer, "signature",
				   "foreground-gdk", &signature_color,
				   NULL);
 	gtk_text_buffer_create_tag(buffer, "link",
					"foreground-gdk", &uri_color,
					 NULL);
	compose->no_wrap_tag = gtk_text_buffer_create_tag(buffer, "no_wrap", NULL);
	compose->no_join_tag = gtk_text_buffer_create_tag(buffer, "no_join", NULL);

	color[0] = quote_color1;
	color[1] = quote_color2;
	color[2] = quote_color3;
	color[3] = quote_bgcolor1;
	color[4] = quote_bgcolor2;
	color[5] = quote_bgcolor3;
	color[6] = signature_color;
	color[7] = uri_color;
	cmap = gdk_drawable_get_colormap(compose->window->window);
	gdk_colormap_alloc_colors(cmap, color, 8, FALSE, TRUE, success);

	for (i = 0; i < 8; i++) {
		if (success[i] == FALSE) {
			GtkStyle *style;

			g_warning("Compose: color allocation failed.\n");
			style = gtk_widget_get_style(GTK_WIDGET(text));
			quote_color1 = quote_color2 = quote_color3 = 
				quote_bgcolor1 = quote_bgcolor2 = quote_bgcolor3 = 
				signature_color = uri_color = black;
		}
	}
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
	GtkTextView *textview;
	GtkTextBuffer *textbuf;
	GtkTextIter iter;
	GtkItemFactory *ifactory;

	if (item && item->prefs && item->prefs->enable_default_account)
		account = account_find_from_id(item->prefs->default_account);

 	if (!account) account = cur_account;
	g_return_val_if_fail(account != NULL, NULL);

	compose = compose_create(account, COMPOSE_NEW, FALSE);

	ifactory = gtk_item_factory_from_widget(compose->menubar);

	compose->replyinfo = NULL;
	compose->fwdinfo   = NULL;

	textview = GTK_TEXT_VIEW(compose->text);
	textbuf = gtk_text_view_get_buffer(textview);
	compose_create_tags(textview, compose);

	undo_block(compose->undostruct);
#ifdef USE_ASPELL
	if (item && item->prefs && item->prefs->enable_default_dictionary &&
	    compose->gtkaspell) 
		gtkaspell_change_dict(compose->gtkaspell, 
		    item->prefs->default_dictionary);
	compose_spell_menu_changed(compose);
#endif

	if (account->auto_sig)
		compose_insert_sig(compose, FALSE);
	gtk_text_buffer_get_start_iter(textbuf, &iter);
	gtk_text_buffer_place_cursor(textbuf, &iter);

	if (account->protocol != A_NNTP) {
		if (mailto && *mailto != '\0') {
			compose_entries_set(compose, mailto);

		} else if (item && item->prefs->enable_default_to) {
			compose_entry_append(compose, item->prefs->default_to, COMPOSE_TO);
			compose_entry_mark_default_to(compose, item->prefs->default_to);
		}
		if (item && item->ret_rcpt) {
			menu_set_active(ifactory, "/Options/Request Return Receipt", TRUE);
		}
	} else {
		if (mailto) {
			compose_entry_append(compose, mailto, COMPOSE_NEWSGROUPS);
		} else if (item) {
			compose_entry_append(compose, item->path, COMPOSE_NEWSGROUPS);
		}
		/*
		 * CLAWS: just don't allow return receipt request, even if the user
		 * may want to send an email. simple but foolproof.
		 */
		menu_set_sensitive(ifactory, "/Options/Request Return Receipt", FALSE); 
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

	undo_unblock(compose->undostruct);

	if (prefs_common.auto_exteditor)
		compose_exec_ext_editor(compose);

	compose->modified = FALSE;
	compose_set_title(compose);
        return compose;
}

static void compose_force_encryption(Compose *compose, PrefsAccount *account,
		gboolean override_pref)
{
	gchar *privacy = NULL;

	g_return_if_fail(compose != NULL);
	g_return_if_fail(account != NULL);

	if (override_pref == FALSE && account->default_encrypt_reply == FALSE)
		return;

	if (account->default_privacy_system
	&&  strlen(account->default_privacy_system)) {
		privacy = account->default_privacy_system;
	} else {
		GSList *privacy_avail = privacy_get_system_ids();
		if (privacy_avail && g_slist_length(privacy_avail)) {
			privacy = (gchar *)(privacy_avail->data);
		}
	}
	if (privacy != NULL) {
		compose->privacy_system = g_strdup(privacy);
		compose_update_privacy_system_menu_item(compose, FALSE);
		compose_use_encryption(compose, TRUE);
	}
}	

static void compose_force_signing(Compose *compose, PrefsAccount *account)
{
	gchar *privacy = NULL;

	if (account->default_privacy_system
	&&  strlen(account->default_privacy_system)) {
		privacy = account->default_privacy_system;
	} else {
		GSList *privacy_avail = privacy_get_system_ids();
		if (privacy_avail && g_slist_length(privacy_avail)) {
			privacy = (gchar *)(privacy_avail->data);
		}
	}
	if (privacy != NULL) {
		compose->privacy_system = g_strdup(privacy);
		compose_update_privacy_system_menu_item(compose, FALSE);
		compose_use_signing(compose, TRUE);
	}
}	

Compose *compose_reply_mode(ComposeMode mode, GSList *msginfo_list, gchar *body)
{
	MsgInfo *msginfo;
	guint list_len;
	Compose *compose = NULL;
	g_return_val_if_fail(msginfo_list != NULL, NULL);

	msginfo = (MsgInfo*)g_slist_nth_data(msginfo_list, 0);
	g_return_val_if_fail(msginfo != NULL, NULL);

	list_len = g_slist_length(msginfo_list);

	switch (mode) {
	case COMPOSE_REPLY:
		compose = compose_reply(msginfo, prefs_common.reply_with_quote,
		    	      FALSE, prefs_common.default_reply_list, FALSE, body);
		break;
	case COMPOSE_REPLY_WITH_QUOTE:
		compose = compose_reply(msginfo, TRUE, 
			FALSE, prefs_common.default_reply_list, FALSE, body);
		break;
	case COMPOSE_REPLY_WITHOUT_QUOTE:
		compose = compose_reply(msginfo, FALSE, 
			FALSE, prefs_common.default_reply_list, FALSE, NULL);
		break;
	case COMPOSE_REPLY_TO_SENDER:
		compose = compose_reply(msginfo, prefs_common.reply_with_quote,
			      FALSE, FALSE, TRUE, body);
		break;
	case COMPOSE_FOLLOWUP_AND_REPLY_TO:
		compose = compose_followup_and_reply_to(msginfo,
					      prefs_common.reply_with_quote,
					      FALSE, FALSE, body);
		break;
	case COMPOSE_REPLY_TO_SENDER_WITH_QUOTE:
		compose = compose_reply(msginfo, TRUE, 
			FALSE, FALSE, TRUE, body);
		break;
	case COMPOSE_REPLY_TO_SENDER_WITHOUT_QUOTE:
		compose = compose_reply(msginfo, FALSE, 
			FALSE, FALSE, TRUE, NULL);
		break;
	case COMPOSE_REPLY_TO_ALL:
		compose = compose_reply(msginfo, prefs_common.reply_with_quote,
			TRUE, FALSE, FALSE, body);
		break;
	case COMPOSE_REPLY_TO_ALL_WITH_QUOTE:
		compose = compose_reply(msginfo, TRUE, 
			TRUE, FALSE, FALSE, body);
		break;
	case COMPOSE_REPLY_TO_ALL_WITHOUT_QUOTE:
		compose = compose_reply(msginfo, FALSE, 
			TRUE, FALSE, FALSE, NULL);
		break;
	case COMPOSE_REPLY_TO_LIST:
		compose = compose_reply(msginfo, prefs_common.reply_with_quote,
			FALSE, TRUE, FALSE, body);
		break;
	case COMPOSE_REPLY_TO_LIST_WITH_QUOTE:
		compose = compose_reply(msginfo, TRUE, 
			FALSE, TRUE, FALSE, body);
		break;
	case COMPOSE_REPLY_TO_LIST_WITHOUT_QUOTE:
		compose = compose_reply(msginfo, FALSE, 
			FALSE, TRUE, FALSE, NULL);
		break;
	case COMPOSE_FORWARD:
		if (prefs_common.forward_as_attachment) {
			compose = compose_reply_mode(COMPOSE_FORWARD_AS_ATTACH, msginfo_list, body);
			return compose;
		} else {
			compose = compose_reply_mode(COMPOSE_FORWARD_INLINE, msginfo_list, body);
			return compose;
		}
		break;
	case COMPOSE_FORWARD_INLINE:
		/* check if we reply to more than one Message */
		if (list_len == 1) {
			compose = compose_forward(NULL, msginfo, FALSE, body, FALSE, FALSE);
			break;
		} 
		/* more messages FALL THROUGH */
	case COMPOSE_FORWARD_AS_ATTACH:
		compose = compose_forward_multiple(NULL, msginfo_list);
		break;
	case COMPOSE_REDIRECT:
		compose = compose_redirect(NULL, msginfo, FALSE);
		break;
	default:
		g_warning("compose_reply(): invalid Compose Mode: %d\n", mode);
	}
	return compose;
}

Compose *compose_reply(MsgInfo *msginfo, gboolean quote, gboolean to_all,
		   gboolean to_ml, gboolean to_sender, 
		   const gchar *body)
{
	return compose_generic_reply(msginfo, quote, to_all, to_ml, 
			      to_sender, FALSE, body);
}

Compose *compose_followup_and_reply_to(MsgInfo *msginfo, gboolean quote,
				   gboolean to_all,
				   gboolean to_sender,
				   const gchar *body)
{
	return compose_generic_reply(msginfo, quote, to_all, FALSE, 
			      to_sender, TRUE, body);
}

static void compose_extract_original_charset(Compose *compose)
{
	MsgInfo *info = NULL;
	if (compose->replyinfo) {
		info = compose->replyinfo;
	} else if (compose->fwdinfo) {
		info = compose->fwdinfo;
	} else if (compose->targetinfo) {
		info = compose->targetinfo;
	}
	if (info) {
		MimeInfo *mimeinfo = procmime_scan_message(info);
		MimeInfo *partinfo = mimeinfo;
		while (partinfo && partinfo->type != MIMETYPE_TEXT)
			partinfo = procmime_mimeinfo_next(partinfo);
		if (partinfo) {
			compose->orig_charset = 
				g_strdup(procmime_mimeinfo_get_parameter(
						partinfo, "charset"));
		}
		procmime_mimeinfo_free_all(mimeinfo);
	}
}

#define SIGNAL_BLOCK(buffer) {					\
	g_signal_handlers_block_by_func(G_OBJECT(buffer),	\
				G_CALLBACK(compose_changed_cb),	\
				compose);			\
	g_signal_handlers_block_by_func(G_OBJECT(buffer),	\
				G_CALLBACK(text_inserted),	\
				compose);			\
}

#define SIGNAL_UNBLOCK(buffer) {				\
	g_signal_handlers_unblock_by_func(G_OBJECT(buffer),	\
				G_CALLBACK(compose_changed_cb),	\
				compose);			\
	g_signal_handlers_unblock_by_func(G_OBJECT(buffer),	\
				G_CALLBACK(text_inserted),	\
				compose);			\
}

static Compose *compose_generic_reply(MsgInfo *msginfo, gboolean quote,
				  gboolean to_all, gboolean to_ml,
				  gboolean to_sender,
				  gboolean followup_and_reply_to,
				  const gchar *body)
{
	GtkItemFactory *ifactory;
	Compose *compose;
	PrefsAccount *account = NULL;
	PrefsAccount *reply_account;
	GtkTextView *textview;
	GtkTextBuffer *textbuf;

	g_return_val_if_fail(msginfo != NULL, NULL);
	g_return_val_if_fail(msginfo->folder != NULL, NULL);

	account = account_get_reply_account(msginfo, prefs_common.reply_account_autosel);
	
	g_return_val_if_fail(account != NULL, NULL);

	if (to_sender && account->protocol == A_NNTP &&
	    !followup_and_reply_to) {
		reply_account =
			account_find_from_address(account->address);
		if (!reply_account)
			reply_account = compose_current_mail_account();
		if (!reply_account)
			return NULL;
	} else
		reply_account = account;

	compose = compose_create(account, COMPOSE_REPLY, FALSE);

	compose->updating = TRUE;

	ifactory = gtk_item_factory_from_widget(compose->menubar);

	menu_set_active(ifactory, "/Options/Remove references", FALSE);
	menu_set_sensitive(ifactory, "/Options/Remove references", TRUE);

	compose->replyinfo = procmsg_msginfo_get_full_info(msginfo);
	if (!compose->replyinfo)
		compose->replyinfo = procmsg_msginfo_copy(msginfo);

	compose_extract_original_charset(compose);
	
    	if (msginfo->folder && msginfo->folder->ret_rcpt)
		menu_set_active(ifactory, "/Options/Request Return Receipt", TRUE);

	/* Set save folder */
	if (msginfo->folder && msginfo->folder->prefs && msginfo->folder->prefs->save_copy_to_folder) {
		gchar *folderidentifier;

    		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(compose->savemsg_checkbtn), TRUE);
		folderidentifier = folder_item_get_identifier(msginfo->folder);
		gtk_entry_set_text(GTK_ENTRY(compose->savemsg_entry), folderidentifier);
		g_free(folderidentifier);
	}

	if (compose_parse_header(compose, msginfo) < 0) return NULL;
	compose_reply_set_entry(compose, msginfo, to_all, to_ml, 
				to_sender, followup_and_reply_to);
	compose_show_first_last_header(compose, TRUE);

	textview = (GTK_TEXT_VIEW(compose->text));
	textbuf = gtk_text_view_get_buffer(textview);
	compose_create_tags(textview, compose);

	undo_block(compose->undostruct);
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
			          qmark, body, FALSE);
		quote_fmt_reset_vartable();
	}
	if (procmime_msginfo_is_encrypted(compose->replyinfo)) {
		compose_force_encryption(compose, account, FALSE);
	}

	SIGNAL_BLOCK(textbuf);
	
	if (account->auto_sig)
		compose_insert_sig(compose, FALSE);

	compose_wrap_all(compose);

	SIGNAL_UNBLOCK(textbuf);
	
	gtk_widget_grab_focus(compose->text);

	undo_unblock(compose->undostruct);

	if (prefs_common.auto_exteditor)
		compose_exec_ext_editor(compose);
		
	compose->modified = FALSE;
	compose_set_title(compose);

	compose->updating = FALSE;

	if (compose->deferred_destroy) {
		compose_destroy(compose);
		return NULL;
	}

	return compose;
}

#define INSERT_FW_HEADER(var, hdr) \
if (msginfo->var && *msginfo->var) { \
	gtk_stext_insert(text, NULL, NULL, NULL, hdr, -1); \
	gtk_stext_insert(text, NULL, NULL, NULL, msginfo->var, -1); \
	gtk_stext_insert(text, NULL, NULL, NULL, "\n", 1); \
}

Compose *compose_forward(PrefsAccount *account, MsgInfo *msginfo,
			 gboolean as_attach, const gchar *body,
			 gboolean no_extedit,
			 gboolean batch)
{
	Compose *compose;
	GtkTextView *textview;
	GtkTextBuffer *textbuf;
	GtkTextIter iter;

	g_return_val_if_fail(msginfo != NULL, NULL);
	g_return_val_if_fail(msginfo->folder != NULL, NULL);

	if (!account && 
	    !(account = compose_guess_forward_account_from_msginfo
				(msginfo)))
		account = cur_account;

	compose = compose_create(account, COMPOSE_FORWARD, batch);

	compose->updating = TRUE;
	compose->fwdinfo = procmsg_msginfo_get_full_info(msginfo);
	if (!compose->fwdinfo)
		compose->fwdinfo = procmsg_msginfo_copy(msginfo);

	compose_extract_original_charset(compose);

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

	textview = GTK_TEXT_VIEW(compose->text);
	textbuf = gtk_text_view_get_buffer(textview);
	compose_create_tags(textview, compose);
	
	undo_block(compose->undostruct);
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
			          qmark, body, FALSE);
		quote_fmt_reset_vartable();
		compose_attach_parts(compose, msginfo);

		procmsg_msginfo_free(full_msginfo);
	}

	SIGNAL_BLOCK(textbuf);

	if (account->auto_sig)
		compose_insert_sig(compose, FALSE);

	compose_wrap_all(compose);

	SIGNAL_UNBLOCK(textbuf);
	
	gtk_text_buffer_get_start_iter(textbuf, &iter);
	gtk_text_buffer_place_cursor(textbuf, &iter);

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

	undo_unblock(compose->undostruct);
	
	compose->modified = FALSE;
	compose_set_title(compose);

	compose->updating = FALSE;

	if (compose->deferred_destroy) {
		compose_destroy(compose);
		return NULL;
	}

        return compose;
}

#undef INSERT_FW_HEADER

Compose *compose_forward_multiple(PrefsAccount *account, GSList *msginfo_list)
{
	Compose *compose;
	GtkTextView *textview;
	GtkTextBuffer *textbuf;
	GtkTextIter iter;
	GSList *msginfo;
	gchar *msgfile;
	gboolean single_mail = TRUE;
	
	g_return_val_if_fail(msginfo_list != NULL, NULL);

	if (g_slist_length(msginfo_list) > 1)
		single_mail = FALSE;

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

	compose = compose_create(account, COMPOSE_FORWARD, FALSE);

	compose->updating = TRUE;

	textview = GTK_TEXT_VIEW(compose->text);
	textbuf = gtk_text_view_get_buffer(textview);
	compose_create_tags(textview, compose);
	
	undo_block(compose->undostruct);
	for (msginfo = msginfo_list; msginfo != NULL; msginfo = msginfo->next) {
		msgfile = procmsg_get_message_file_path((MsgInfo *)msginfo->data);

		if (!is_file_exist(msgfile))
			g_warning("%s: file not exist\n", msgfile);
		else
			compose_attach_append(compose, msgfile, msgfile,
				"message/rfc822");
		g_free(msgfile);
	}
	
	if (single_mail) {
		MsgInfo *info = (MsgInfo *)msginfo_list->data;
		if (info->subject && *info->subject) {
			gchar *buf, *buf2, *p;

			buf = p = g_strdup(info->subject);
			p += subject_get_prefix_length(p);
			memmove(buf, p, strlen(p) + 1);

			buf2 = g_strdup_printf("Fw: %s", buf);
			gtk_entry_set_text(GTK_ENTRY(compose->subject_entry), buf2);

			g_free(buf);
			g_free(buf2);
		}
	} else {
		gtk_entry_set_text(GTK_ENTRY(compose->subject_entry),
			_("Fw: multiple emails"));
	}

	SIGNAL_BLOCK(textbuf);
	
	if (account->auto_sig)
		compose_insert_sig(compose, FALSE);

	compose_wrap_all(compose);

	SIGNAL_UNBLOCK(textbuf);
	
	gtk_text_buffer_get_start_iter(textbuf, &iter);
	gtk_text_buffer_place_cursor(textbuf, &iter);

	gtk_widget_grab_focus(compose->header_last->entry);
	undo_unblock(compose->undostruct);
	compose->modified = FALSE;
	compose_set_title(compose);

	compose->updating = FALSE;

	if (compose->deferred_destroy) {
		compose_destroy(compose);
		return NULL;
	}

	return compose;
}

static gboolean compose_is_sig_separator(Compose *compose, GtkTextBuffer *textbuf, GtkTextIter *iter) 
{
	GtkTextIter start = *iter;
	GtkTextIter end_iter;
	int start_pos = gtk_text_iter_get_offset(&start);
	gchar *str = NULL;
	if (!compose->account->sig_sep)
		return FALSE;
	
	gtk_text_buffer_get_iter_at_offset(textbuf, &end_iter,
		start_pos+strlen(compose->account->sig_sep));

	/* check sig separator */
	str = gtk_text_iter_get_text(&start, &end_iter);
	if (!strcmp(str, compose->account->sig_sep)) {
		gchar *tmp = NULL;
		/* check end of line (\n) */
		gtk_text_buffer_get_iter_at_offset(textbuf, &start,
			start_pos+strlen(compose->account->sig_sep));
		gtk_text_buffer_get_iter_at_offset(textbuf, &end_iter,
			start_pos+strlen(compose->account->sig_sep)+1);
		tmp = gtk_text_iter_get_text(&start, &end_iter);
		if (!strcmp(tmp,"\n")) {
			g_free(str);
			g_free(tmp);
			return TRUE;
		}
		g_free(tmp);	
	}
	g_free(str);

	return FALSE;
}

static void compose_colorize_signature(Compose *compose)
{
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(compose->text));
	GtkTextIter iter;
	GtkTextIter end_iter;
	gtk_text_buffer_get_start_iter(buffer, &iter);
	while (gtk_text_iter_forward_line(&iter))
		if (compose_is_sig_separator(compose, buffer, &iter)) {
			gtk_text_buffer_get_end_iter(buffer, &end_iter);
			gtk_text_buffer_apply_tag_by_name(buffer,"signature",&iter, &end_iter);
		}
}

Compose *compose_reedit(MsgInfo *msginfo, gboolean batch)
{
	Compose *compose = NULL;
	PrefsAccount *account = NULL;
	GtkTextView *textview;
	GtkTextBuffer *textbuf;
	GtkTextMark *mark;
	GtkTextIter iter;
	FILE *fp;
	gchar buf[BUFFSIZE];
	gboolean use_signing = FALSE;
	gboolean use_encryption = FALSE;
	gchar *privacy_system = NULL;
	int priority = PRIORITY_NORMAL;

	g_return_val_if_fail(msginfo != NULL, NULL);
	g_return_val_if_fail(msginfo->folder != NULL, NULL);

	if (compose_put_existing_to_front(msginfo)) {
		return NULL;
	}

        if (folder_has_parent_of_type(msginfo->folder, F_QUEUE) ||
	    folder_has_parent_of_type(msginfo->folder, F_DRAFT)) {
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
			priority = param;
		}
	} else {
		account = msginfo->folder->folder->account;
	}

	if (!account && prefs_common.reedit_account_autosel) {
               	gchar from[BUFFSIZE];
		if (!procheader_get_header_from_msginfo(msginfo, from, sizeof(from), "FROM:")) {
		        extract_address(from);
		        account = account_find_from_address(from);
                }
	}
        if (!account) {
        	account = cur_account;
        }
	g_return_val_if_fail(account != NULL, NULL);

	compose = compose_create(account, COMPOSE_REEDIT, batch);
	
	compose->updating = TRUE;
	compose->priority = priority;

	if (privacy_system != NULL) {
		compose->privacy_system = privacy_system;
		compose_use_signing(compose, use_signing);
		compose_use_encryption(compose, use_encryption);
		compose_update_privacy_system_menu_item(compose, FALSE);
	} else {
		activate_privacy_system(compose, account, FALSE);
	}

	compose->targetinfo = procmsg_msginfo_copy(msginfo);

	compose_extract_original_charset(compose);

        if (folder_has_parent_of_type(msginfo->folder, F_QUEUE) ||
	    folder_has_parent_of_type(msginfo->folder, F_DRAFT)) {
		gchar queueheader_buf[BUFFSIZE];

		/* Set message save folder */
		if (!procheader_get_header_from_msginfo(msginfo, queueheader_buf, sizeof(queueheader_buf), "SCF:")) {
			gint startpos = 0;

			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(compose->savemsg_checkbtn), TRUE);
			gtk_editable_delete_text(GTK_EDITABLE(compose->savemsg_entry), 0, -1);
			gtk_editable_insert_text(GTK_EDITABLE(compose->savemsg_entry), &queueheader_buf[4], strlen(&queueheader_buf[4]), &startpos);
		}
		if (!procheader_get_header_from_msginfo(msginfo, queueheader_buf, sizeof(queueheader_buf), "RRCPT:")) {
			gint active = atoi(&queueheader_buf[strlen("RRCPT:")]);
			if (active) {
				GtkItemFactory *ifactory;
				ifactory = gtk_item_factory_from_widget(compose->menubar);
				menu_set_active(ifactory, "/Options/Request Return Receipt", TRUE);
			}
		}
	}
	
	if (compose_parse_header(compose, msginfo) < 0) {
		compose->updating = FALSE;
		compose_destroy(compose);
		return NULL;
	}
	compose_reedit_set_entry(compose, msginfo);

	textview = GTK_TEXT_VIEW(compose->text);
	textbuf = gtk_text_view_get_buffer(textview);
	compose_create_tags(textview, compose);

	mark = gtk_text_buffer_get_insert(textbuf);
	gtk_text_buffer_get_iter_at_mark(textbuf, &iter, mark);

	g_signal_handlers_block_by_func(G_OBJECT(textbuf),
					G_CALLBACK(compose_changed_cb),
					compose);
	
	if (procmime_msginfo_is_encrypted(msginfo)) {
		fp = procmime_get_first_encrypted_text_content(msginfo);
		if (fp) {
			compose_force_encryption(compose, account, TRUE);
		}
	} else {
		fp = procmime_get_first_text_content(msginfo);
	}
	if (fp == NULL) {
		g_warning("Can't get text part\n");
	}

	if (fp != NULL) {
		gboolean prev_autowrap = compose->autowrap;

		compose->autowrap = FALSE;
		while (fgets(buf, sizeof(buf), fp) != NULL) {
			strcrchomp(buf);
			gtk_text_buffer_insert(textbuf, &iter, buf, -1);
		}
		compose_wrap_all_full(compose, FALSE);
		compose->autowrap = prev_autowrap;
		fclose(fp);
	}
	
	compose_attach_parts(compose, msginfo);

	compose_colorize_signature(compose);

	g_signal_handlers_unblock_by_func(G_OBJECT(textbuf),
					G_CALLBACK(compose_changed_cb),
					compose);

	gtk_widget_grab_focus(compose->text);

        if (prefs_common.auto_exteditor) {
		compose_exec_ext_editor(compose);
	}
	compose->modified = FALSE;
	compose_set_title(compose);

	compose->updating = FALSE;

	if (compose->deferred_destroy) {
		compose_destroy(compose);
		return NULL;
	}
	
	compose->sig_str = compose_get_signature_str(compose);
	
	return compose;
}

Compose *compose_redirect(PrefsAccount *account, MsgInfo *msginfo,
						 gboolean batch)
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

	compose = compose_create(account, COMPOSE_REDIRECT, batch);

	compose->updating = TRUE;

	ifactory = gtk_item_factory_from_widget(compose->menubar);
	compose_create_tags(GTK_TEXT_VIEW(compose->text), compose);
	compose->replyinfo = NULL;
	compose->fwdinfo = NULL;

	compose_show_first_last_header(compose, TRUE);

	gtk_widget_grab_focus(compose->header_last->entry);

	filename = procmsg_get_message_file_path(msginfo);

	if (filename == NULL) {
		compose->updating = FALSE;
		compose_destroy(compose);

		return NULL;
	}

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

	compose_quote_fmt(compose, msginfo, "%M", NULL, NULL, FALSE);
	quote_fmt_reset_vartable();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(compose->text), FALSE);

	compose_colorize_signature(compose);

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
	
	if (compose->toolbar->draft_btn)
		gtk_widget_set_sensitive(compose->toolbar->draft_btn, FALSE);
	if (compose->toolbar->insert_btn)
		gtk_widget_set_sensitive(compose->toolbar->insert_btn, FALSE);
	if (compose->toolbar->attach_btn)
		gtk_widget_set_sensitive(compose->toolbar->attach_btn, FALSE);
	if (compose->toolbar->sig_btn)
		gtk_widget_set_sensitive(compose->toolbar->sig_btn, FALSE);
	if (compose->toolbar->exteditor_btn)
		gtk_widget_set_sensitive(compose->toolbar->exteditor_btn, FALSE);
	if (compose->toolbar->linewrap_current_btn)
		gtk_widget_set_sensitive(compose->toolbar->linewrap_current_btn, FALSE);
	if (compose->toolbar->linewrap_all_btn)
		gtk_widget_set_sensitive(compose->toolbar->linewrap_all_btn, FALSE);

	compose->modified = FALSE;
	compose_set_title(compose);
	compose->updating = FALSE;

	if (compose->deferred_destroy) {
		compose_destroy(compose);
		return NULL;
	}
	
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
	gchar *cur, *begin;
	gboolean in_quote = FALSE;
	if (!address || *address == '\0') return;

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
	
	cur = begin = (gchar *)address;
	
	/* we separate the line by commas, but not if we're inside a quoted
	 * string */
	while (*cur != '\0') {
		if (*cur == '"') 
			in_quote = !in_quote;
		if (*cur == ',' && !in_quote) {
			gchar *tmp = g_strdup(begin);
			gchar *o_tmp = tmp;
			tmp[cur-begin]='\0';
			cur++;
			begin = cur;
			while (*tmp == ' ' || *tmp == '\t')
				tmp++;
			compose_add_header_entry(compose, header, tmp);
			g_free(o_tmp);
			continue;
		}
		cur++;
	}
	if (begin < cur) {
		gchar *tmp = g_strdup(begin);
		gchar *o_tmp = tmp;
		tmp[cur-begin]='\0';
		cur++;
		begin = cur;
		while (*tmp == ' ' || *tmp == '\t')
			tmp++;
		compose_add_header_entry(compose, header, tmp);
		g_free(o_tmp);		
	}
}

void compose_entry_mark_default_to(Compose *compose, const gchar *mailto)
{
	static GdkColor yellow;
	static GdkColor black;
	static gboolean yellow_initialised = FALSE;
	GSList *h_list;
	GtkEntry *entry;
		
	if (!yellow_initialised) {
		gdk_color_parse("#f5f6be", &yellow);
		gdk_color_parse("#000000", &black);
		yellow_initialised = gdk_colormap_alloc_color(
			gdk_colormap_get_system(), &yellow, FALSE, TRUE);
		yellow_initialised &= gdk_colormap_alloc_color(
			gdk_colormap_get_system(), &black, FALSE, TRUE);
	}

	for (h_list = compose->header_list; h_list != NULL; h_list = h_list->next) {
		entry = GTK_ENTRY(((ComposeHeaderEntry *)h_list->data)->entry);
		if (gtk_entry_get_text(entry) && 
		    !g_utf8_collate(gtk_entry_get_text(entry), mailto)) {
			if (yellow_initialised) {
				gtk_widget_modify_base(
					GTK_WIDGET(((ComposeHeaderEntry *)h_list->data)->entry),
					GTK_STATE_NORMAL, &yellow);
				gtk_widget_modify_text(
					GTK_WIDGET(((ComposeHeaderEntry *)h_list->data)->entry),
					GTK_STATE_NORMAL, &black);
			}
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
		compose_beautify_paragraph(compose, NULL, TRUE);
		break;
	case A_LINEWRAP_ALL:
		compose_wrap_all_full(compose, TRUE);
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
	gchar *subject = NULL;
	gchar *body = NULL;
	gchar *temp = NULL;
	guint  len = 0;

	scan_mailto_url(mailto, &to, &cc, NULL, &subject, &body);

	if (to)
		compose_entry_append(compose, to, COMPOSE_TO);
	if (cc)
		compose_entry_append(compose, cc, COMPOSE_CC);
	if (subject) {
		if (!g_utf8_validate (subject, -1, NULL)) {
			temp = g_locale_to_utf8 (subject, -1, NULL, &len, NULL);
			gtk_entry_set_text(GTK_ENTRY(compose->subject_entry), temp);
			g_free(temp);
		} else {
			gtk_entry_set_text(GTK_ENTRY(compose->subject_entry), subject);
		}
	}
	if (body) {
		GtkTextView *text = GTK_TEXT_VIEW(compose->text);
		GtkTextBuffer *buffer = gtk_text_view_get_buffer(text);
		GtkTextMark *mark;
		GtkTextIter iter;
		gboolean prev_autowrap = compose->autowrap;

		compose->autowrap = FALSE;

		mark = gtk_text_buffer_get_insert(buffer);
		gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);

		if (!g_utf8_validate (body, -1, NULL)) {
			temp = g_locale_to_utf8 (body, -1, NULL, &len, NULL);
			gtk_text_buffer_insert(buffer, &iter, temp, -1);
			g_free(temp);
		} else {
			gtk_text_buffer_insert(buffer, &iter, body, -1);
		}
		gtk_text_buffer_insert(buffer, &iter, "\n", 1);

		compose->autowrap = prev_autowrap;
		if (compose->autowrap)
			compose_wrap_all(compose);
	}

	g_free(to);
	g_free(cc);
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

	g_return_val_if_fail(msginfo != NULL, -1);

	if ((fp = procmsg_open_message(msginfo)) == NULL) return -1;
	procheader_get_header_fields(fp, hentry);
	fclose(fp);

	if (hentry[H_REPLY_TO].body != NULL) {
		if (hentry[H_REPLY_TO].body[0] != '\0') {
			compose->replyto =
				conv_unmime_header(hentry[H_REPLY_TO].body,
						   NULL);
		}
		g_free(hentry[H_REPLY_TO].body);
		hentry[H_REPLY_TO].body = NULL;
	}
	if (hentry[H_CC].body != NULL) {
		compose->cc = conv_unmime_header(hentry[H_CC].body, NULL);
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
		if (compose->mode == COMPOSE_REEDIT)
			compose->bcc =
				conv_unmime_header(hentry[H_BCC].body, NULL);
		g_free(hentry[H_BCC].body);
		hentry[H_BCC].body = NULL;
	}
	if (hentry[H_NEWSGROUPS].body != NULL) {
		compose->newsgroups = hentry[H_NEWSGROUPS].body;
		hentry[H_NEWSGROUPS].body = NULL;
	}
	if (hentry[H_FOLLOWUP_TO].body != NULL) {
		if (hentry[H_FOLLOWUP_TO].body[0] != '\0') {
			compose->followup_to =
				conv_unmime_header(hentry[H_FOLLOWUP_TO].body,
						   NULL);
		}
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
		g_string_append_printf(new_ref, "<%s>", (gchar *)cur->data);
	}

	slist_free_strings(ref_id_list);
	g_slist_free(ref_id_list);

	new_ref_str = new_ref->str;
	g_string_free(new_ref, FALSE);

	return new_ref_str;
}

static gchar *compose_quote_fmt(Compose *compose, MsgInfo *msginfo,
				const gchar *fmt, const gchar *qmark,
				const gchar *body, gboolean rewrap)
{
	static MsgInfo dummyinfo;
	gchar *quote_str = NULL;
	gchar *buf;
	gboolean prev_autowrap;
	const gchar *trimmed_body = body;
	gint cursor_pos = -1;
	GtkTextView *text = GTK_TEXT_VIEW(compose->text);
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(text);
	GtkTextIter iter;
	GtkTextMark *mark;
	

	SIGNAL_BLOCK(buffer);

	if (!msginfo)
		msginfo = &dummyinfo;

	if (qmark != NULL) {
		quote_fmt_init(msginfo, NULL, NULL, FALSE);
		quote_fmt_scan_string(qmark);
		quote_fmt_parse();

		buf = quote_fmt_get_buffer();
		if (buf == NULL)
			alertpanel_error(_("Quote mark format error."));
		else
			Xstrdup_a(quote_str, buf, goto error)
	}

	if (fmt && *fmt != '\0') {
		while (trimmed_body && strlen(trimmed_body) > 1
			&& trimmed_body[0]=='\n')
			*trimmed_body++;

		quote_fmt_init(msginfo, quote_str, trimmed_body, FALSE);
		quote_fmt_scan_string(fmt);
		quote_fmt_parse();

		buf = quote_fmt_get_buffer();
		if (buf == NULL) {
			alertpanel_error(_("Message reply/forward format error."));
			goto error;
		}
	} else
		buf = "";

	prev_autowrap = compose->autowrap;
	compose->autowrap = FALSE;

	mark = gtk_text_buffer_get_insert(buffer);
	gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);
	if (g_utf8_validate(buf, -1, NULL)) { 
		gtk_text_buffer_insert(buffer, &iter, buf, -1);
	} else {
		gchar *tmpout = NULL;
		tmpout = conv_codeset_strdup
			(buf, conv_get_locale_charset_str_no_utf8(),
			 CS_INTERNAL);
		if (!tmpout || !g_utf8_validate(tmpout, -1, NULL)) {
			g_free(tmpout);
			tmpout = g_malloc(strlen(buf)*2+1);
			conv_localetodisp(tmpout, strlen(buf)*2+1, buf);
		}
		gtk_text_buffer_insert(buffer, &iter, tmpout, -1);
		g_free(tmpout);
	}

	cursor_pos = quote_fmt_get_cursor_pos();
	compose->set_cursor_pos = cursor_pos;
	if (cursor_pos == -1) {
		cursor_pos = 0;
	}
	gtk_text_buffer_get_start_iter(buffer, &iter);
	gtk_text_buffer_get_iter_at_offset(buffer, &iter, cursor_pos);
	gtk_text_buffer_place_cursor(buffer, &iter);

	compose->autowrap = prev_autowrap;
	if (compose->autowrap && rewrap)
		compose_wrap_all(compose);

	goto ok;

error:
	buf = NULL;
ok:
	SIGNAL_UNBLOCK(buffer);


	return buf;
}

/* if ml_post is of type addr@host and from is of type
 * addr-anything@host, return TRUE
 */
static gboolean is_subscription(const gchar *ml_post, const gchar *from)
{
	gchar *left_ml = NULL;
	gchar *right_ml = NULL;
	gchar *left_from = NULL;
	gchar *right_from = NULL;
	gboolean result = FALSE;
	
	if (!ml_post || !from)
		return FALSE;
	
	left_ml = g_strdup(ml_post);
	if (strstr(left_ml, "@")) {
		right_ml = strstr(left_ml, "@")+1;
		*(strstr(left_ml, "@")) = '\0';
	}
	
	left_from = g_strdup(from);
	if (strstr(left_from, "@")) {
		right_from = strstr(left_from, "@")+1;
		*(strstr(left_from, "@")) = '\0';
	}
	
	if (left_ml && left_from && right_ml && right_from
	&&  !strncmp(left_from, left_ml, strlen(left_ml))
	&&  !strcmp(right_from, right_ml)) {
		result = TRUE;
	}
	g_free(left_ml);
	g_free(left_from);
	
	return result;
}

static gboolean same_address(const gchar *addr1, const gchar *addr2)
{
	gchar *my_addr1, *my_addr2;
	
	if (!addr1 || !addr2)
		return FALSE;

	Xstrdup_a(my_addr1, addr1, return FALSE);
	Xstrdup_a(my_addr2, addr2, return FALSE);
	
	extract_address(my_addr1);
	extract_address(my_addr2);
	
	return !strcmp(my_addr1, my_addr2);
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

	gboolean reply_to_ml = FALSE;
	gboolean default_reply_to = FALSE;

	g_return_if_fail(compose->account != NULL);
	g_return_if_fail(msginfo != NULL);

	reply_to_ml = to_ml && compose->ml_post;

	default_reply_to = msginfo->folder && 
		msginfo->folder->prefs->enable_default_reply_to;

	if (compose->account->protocol != A_NNTP) {
		if (reply_to_ml && !default_reply_to) {
			
			gboolean is_subscr = is_subscription(compose->ml_post,
							     msginfo->from);
			if (!is_subscr) {
				/* normal answer to ml post with a reply-to */
				compose_entry_append(compose,
					   compose->ml_post,
					   COMPOSE_TO);
				if (compose->replyto
				&&  !same_address(compose->ml_post, compose->replyto))
					compose_entry_append(compose,
						compose->replyto,
						COMPOSE_CC);
			} else {
				/* answer to subscription confirmation */
				if (compose->replyto)
					compose_entry_append(compose,
						compose->replyto,
						COMPOSE_TO);
				else if (msginfo->from)
					compose_entry_append(compose,
						msginfo->from,
						COMPOSE_TO);
			}
		}
		else if (!(to_all || to_sender) && default_reply_to) {
			compose_entry_append(compose,
			    msginfo->folder->prefs->default_reply_to,
			    COMPOSE_TO);
			compose_entry_mark_default_to(compose,
				msginfo->folder->prefs->default_reply_to);
		} else {
			gchar *tmp1 = NULL;
			if (!msginfo->from)
				return;
			Xstrdup_a(tmp1, msginfo->from, return);
			extract_address(tmp1);
			if (to_all || to_sender ||
			    !account_find_from_address(tmp1))
				compose_entry_append(compose,
				 (compose->replyto && !to_sender)
					  ? compose->replyto :
					  msginfo->from ? msginfo->from : "",
					  COMPOSE_TO);
			else if (!to_all && !to_sender) {
				/* reply to the last list of recipients */
				compose_entry_append(compose,
					  msginfo->to ? msginfo->to : "",
					  COMPOSE_TO);
				compose_entry_append(compose,
					  msginfo->cc ? msginfo->cc : "",
					  COMPOSE_CC);
			}
		}
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
		gchar *p;

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
	SET_ENTRY(from_name, msginfo->from);
	SET_ADDRESS(COMPOSE_TO, msginfo->to);
	SET_ADDRESS(COMPOSE_CC, compose->cc);
	SET_ADDRESS(COMPOSE_BCC, compose->bcc);
	SET_ADDRESS(COMPOSE_REPLYTO, compose->replyto);
	SET_ADDRESS(COMPOSE_NEWSGROUPS, compose->newsgroups);
	SET_ADDRESS(COMPOSE_FOLLOWUPTO, compose->followup_to);

	compose_update_priority_menu_item(compose);
	compose_update_privacy_system_menu_item(compose, FALSE);
	compose_show_first_last_header(compose, TRUE);
}

#undef SET_ENTRY
#undef SET_ADDRESS

static void compose_insert_sig(Compose *compose, gboolean replace)
{
	GtkTextView *text = GTK_TEXT_VIEW(compose->text);
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(text);
	GtkTextMark *mark;
	GtkTextIter iter, iter_end;
	gint cur_pos;
	gchar *search = NULL;
	gboolean prev_autowrap;
	gboolean found = FALSE, shift = FALSE;

	
	g_return_if_fail(compose->account != NULL);

	prev_autowrap = compose->autowrap;
	compose->autowrap = FALSE;

	g_signal_handlers_block_by_func(G_OBJECT(buffer),
					G_CALLBACK(compose_changed_cb),
					compose);
	
	mark = gtk_text_buffer_get_insert(buffer);
	gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);
	cur_pos = gtk_text_iter_get_offset (&iter);

	gtk_text_buffer_get_end_iter(buffer, &iter);

	search = compose->sig_str;
again:
	if (replace && search) {
		GtkTextIter first_iter, start_iter, end_iter;

		gtk_text_buffer_get_start_iter(buffer, &first_iter);

		if (compose->sig_str[0] == '\0')
			found = FALSE;
		else
			found = gtk_text_iter_forward_search(&first_iter,
							     search,
							     GTK_TEXT_SEARCH_TEXT_ONLY,
							     &start_iter, &end_iter,
							     NULL);

		if (found) {
			gtk_text_buffer_delete(buffer, &start_iter, &end_iter);
			iter = start_iter;
		}
	} 
	if (replace && !found && search && strlen(search) > 2
	&&  search[0] == '\n' && search[1] == '\n') {
		search ++;
		shift = TRUE;
		goto again;
	}

	g_free(compose->sig_str);
	compose->sig_str = compose_get_signature_str(compose);
	if (!compose->sig_str || (replace && !compose->account->auto_sig))
		compose->sig_str = g_strdup("");

	cur_pos = gtk_text_iter_get_offset(&iter);
	if (shift && found)
		gtk_text_buffer_insert(buffer, &iter, compose->sig_str + 1, -1);
	else
		gtk_text_buffer_insert(buffer, &iter, compose->sig_str, -1);
	/* skip \n\n */
	gtk_text_buffer_get_iter_at_offset(buffer, &iter, cur_pos);
	gtk_text_iter_forward_char(&iter);
	gtk_text_iter_forward_char(&iter);
	gtk_text_buffer_get_end_iter(buffer, &iter_end);
	gtk_text_buffer_apply_tag_by_name(buffer,"signature",&iter, &iter_end);

	if (cur_pos > gtk_text_buffer_get_char_count (buffer))
		cur_pos = gtk_text_buffer_get_char_count (buffer);

	/* put the cursor where it should be 
	 * either where the quote_fmt says, either before the signature */
	if (compose->set_cursor_pos < 0)
		gtk_text_buffer_get_iter_at_offset(buffer, &iter, cur_pos);
	else
		gtk_text_buffer_get_iter_at_offset(buffer, &iter, 
			compose->set_cursor_pos);
		
	gtk_text_buffer_place_cursor(buffer, &iter);
	g_signal_handlers_unblock_by_func(G_OBJECT(buffer),
					G_CALLBACK(compose_changed_cb),
					compose);
		
	compose->autowrap = prev_autowrap;
	if (compose->autowrap)
		compose_wrap_all(compose);
}

static gchar *compose_get_signature_str(Compose *compose)
{
	gchar *sig_body = NULL;
	gchar *sig_str = NULL;
	gchar *utf8_sig_str = NULL;

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

	if (compose->account->sig_sep) {
		sig_str = g_strconcat("\n\n", compose->account->sig_sep, "\n", sig_body,
				      NULL);
		g_free(sig_body);
	} else
		sig_str = g_strconcat("\n\n", sig_body, NULL);

	if (sig_str) {
		if (g_utf8_validate(sig_str, -1, NULL) == TRUE)
			utf8_sig_str = sig_str;
		else {
			utf8_sig_str = conv_codeset_strdup
				(sig_str, conv_get_locale_charset_str_no_utf8(),
				 CS_INTERNAL);
			g_free(sig_str);
		}
	}

	return utf8_sig_str;
}

static ComposeInsertResult compose_insert_file(Compose *compose, const gchar *file)
{
	GtkTextView *text;
	GtkTextBuffer *buffer;
	GtkTextMark *mark;
	GtkTextIter iter;
	const gchar *cur_encoding;
	gchar buf[BUFFSIZE];
	gint len;
	FILE *fp;
	gboolean prev_autowrap;
	gboolean badtxt = FALSE;

	g_return_val_if_fail(file != NULL, COMPOSE_INSERT_NO_FILE);

	if ((fp = g_fopen(file, "rb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		return COMPOSE_INSERT_READ_ERROR;
	}

	prev_autowrap = compose->autowrap;
	compose->autowrap = FALSE;

	text = GTK_TEXT_VIEW(compose->text);
	buffer = gtk_text_view_get_buffer(text);
	mark = gtk_text_buffer_get_insert(buffer);
	gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);

	g_signal_handlers_block_by_func(G_OBJECT(buffer),
					G_CALLBACK(text_inserted),
					compose);

	cur_encoding = conv_get_locale_charset_str_no_utf8();

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		gchar *str;

		if (g_utf8_validate(buf, -1, NULL) == TRUE)
			str = g_strdup(buf);
		else
			str = conv_codeset_strdup
				(buf, cur_encoding, CS_INTERNAL);
		if (!str) continue;

		/* strip <CR> if DOS/Windows file,
		   replace <CR> with <LF> if Macintosh file. */
		strcrchomp(str);
		len = strlen(str);
		if (len > 0 && str[len - 1] != '\n') {
			while (--len >= 0)
				if (str[len] == '\r') str[len] = '\n';
		}

		gtk_text_buffer_insert(buffer, &iter, str, -1);
		g_free(str);
	}

	g_signal_handlers_unblock_by_func(G_OBJECT(buffer),
					  G_CALLBACK(text_inserted),
					  compose);
	compose->autowrap = prev_autowrap;
	if (compose->autowrap)
		compose_wrap_all(compose);

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
	GtkTreeIter iter;
	FILE *fp;
	off_t size;
	GAuto *auto_ainfo;
	gchar *size_text;
	GtkListStore *store;
	gchar *name;
	gboolean has_binary = FALSE;

	if (!is_file_exist(file)) {
		g_warning("File %s doesn't exist\n", filename);
		return;
	}
	if ((size = get_file_size(file)) < 0) {
		g_warning("Can't get file size of %s\n", filename);
		return;
	}
	if (size == 0) {
		alertpanel_notice(_("File %s is empty."), filename);
		return;
	}
	if ((fp = g_fopen(file, "rb")) == NULL) {
		alertpanel_error(_("Can't read %s."), filename);
		return;
	}
	fclose(fp);

	ainfo = g_new0(AttachInfo, 1);
	auto_ainfo = g_auto_pointer_new_with_free
			(ainfo, (GFreeFunc) compose_attach_info_free); 
	ainfo->file = g_strdup(file);

	if (content_type) {
		ainfo->content_type = g_strdup(content_type);
		if (!g_ascii_strcasecmp(content_type, "message/rfc822")) {
			MsgInfo *msginfo;
			MsgFlags flags = {0, 0};

			if (procmime_get_encoding_for_text_file(file, &has_binary) == ENC_7BIT)
				ainfo->encoding = ENC_7BIT;
			else
				ainfo->encoding = ENC_8BIT;

			msginfo = procheader_parse_file(file, flags, FALSE, FALSE);
			if (msginfo && msginfo->subject)
				name = g_strdup(msginfo->subject);
			else
				name = g_path_get_basename(filename ? filename : file);

			ainfo->name = g_strdup_printf(_("Message: %s"), name);

			procmsg_msginfo_free(msginfo);
		} else {
			if (!g_ascii_strncasecmp(content_type, "text", 4))
				ainfo->encoding = procmime_get_encoding_for_text_file(file, &has_binary);
			else
				ainfo->encoding = ENC_BASE64;
			name = g_path_get_basename(filename ? filename : file);
			ainfo->name = g_strdup(name);
		}
		g_free(name);
	} else {
		ainfo->content_type = procmime_get_mime_type(file);
		if (!ainfo->content_type) {
			ainfo->content_type =
				g_strdup("application/octet-stream");
			ainfo->encoding = ENC_BASE64;
		} else if (!g_ascii_strncasecmp(ainfo->content_type, "text", 4))
			ainfo->encoding =
				procmime_get_encoding_for_text_file(file, &has_binary);
		else
			ainfo->encoding = ENC_BASE64;
		name = g_path_get_basename(filename ? filename : file);
		ainfo->name = g_strdup(name);	
		g_free(name);
	}

	if (!strcmp(ainfo->content_type, "unknown") || has_binary) {
		g_free(ainfo->content_type);
		ainfo->content_type = g_strdup("application/octet-stream");
	}

	ainfo->size = size;
	size_text = to_human_readable(size);

	store = GTK_LIST_STORE(gtk_tree_view_get_model
			(GTK_TREE_VIEW(compose->attach_clist)));
		
	gtk_list_store_append(store, &iter);
	gtk_list_store_set(store, &iter, 
			   COL_MIMETYPE, ainfo->content_type,
			   COL_SIZE, size_text,
			   COL_NAME, ainfo->name,
			   COL_DATA, ainfo,
			   COL_AUTODATA, auto_ainfo,
			   -1);
	
	g_auto_pointer_free(auto_ainfo);
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
		 !g_ascii_strcasecmp(child->subtype, "pgp-encrypted")) {
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

			/* if we meet a pgp signature, we don't attach it, but
			 * we force signing. */
			if (strcmp(content_type, "application/pgp-signature")) {
				partname = procmime_mimeinfo_get_parameter(child, "filename");
				if (partname == NULL)
					partname = procmime_mimeinfo_get_parameter(child, "name");
				if (partname == NULL)
					partname = "";
				compose_attach_append(compose, outfile, 
						      partname, content_type);
			} else {
				compose_force_signing(compose, compose->account);
			}
			g_free(content_type);
		}
		g_free(outfile);
		NEXT_PART_NOT_CHILD(child);
	}
	procmime_mimeinfo_free_all(mimeinfo);
}

#undef NEXT_PART_NOT_CHILD



typedef enum {
	WAIT_FOR_INDENT_CHAR,
	WAIT_FOR_INDENT_CHAR_OR_SPACE,
} IndentState;

/* return indent length, we allow:
   indent characters followed by indent characters or spaces/tabs,
   alphabets and numbers immediately followed by indent characters,
   and the repeating sequences of the above
   If quote ends with multiple spaces, only the first one is included. */
static gchar *compose_get_quote_str(GtkTextBuffer *buffer,
				    const GtkTextIter *start, gint *len)
{
	GtkTextIter iter = *start;
	gunichar wc;
	gchar ch[6];
	gint clen;
	IndentState state = WAIT_FOR_INDENT_CHAR;
	gboolean is_space;
	gboolean is_indent;
	gint alnum_count = 0;
	gint space_count = 0;
	gint quote_len = 0;

	if (prefs_common.quote_chars == NULL) {
		return 0 ;
	}

	while (!gtk_text_iter_ends_line(&iter)) {
		wc = gtk_text_iter_get_char(&iter);
		if (g_unichar_iswide(wc))
			break;
		clen = g_unichar_to_utf8(wc, ch);
		if (clen != 1)
			break;

		is_indent = strchr(prefs_common.quote_chars, ch[0]) ? TRUE : FALSE;
		is_space = g_unichar_isspace(wc);

		if (state == WAIT_FOR_INDENT_CHAR) {
			if (!is_indent && !g_unichar_isalnum(wc))
				break;
			if (is_indent) {
				quote_len += alnum_count + space_count + 1;
				alnum_count = space_count = 0;
				state = WAIT_FOR_INDENT_CHAR_OR_SPACE;
			} else
				alnum_count++;
		} else if (state == WAIT_FOR_INDENT_CHAR_OR_SPACE) {
			if (!is_indent && !is_space && !g_unichar_isalnum(wc))
				break;
			if (is_space)
				space_count++;
			else if (is_indent) {
				quote_len += alnum_count + space_count + 1;
				alnum_count = space_count = 0;
			} else {
				alnum_count++;
				state = WAIT_FOR_INDENT_CHAR;
			}
		}

		gtk_text_iter_forward_char(&iter);
	}

	if (quote_len > 0 && space_count > 0)
		quote_len++;

	if (len)
		*len = quote_len;

	if (quote_len > 0) {
		iter = *start;
		gtk_text_iter_forward_chars(&iter, quote_len);
		return gtk_text_buffer_get_text(buffer, start, &iter, FALSE);
	}

	return NULL;
}

/* return TRUE if the line is itemized */
static gboolean compose_is_itemized(GtkTextBuffer *buffer,
				    const GtkTextIter *start)
{
	GtkTextIter iter = *start;
	gunichar wc;
	gchar ch[6];
	gint clen;

	if (gtk_text_iter_ends_line(&iter))
		return FALSE;

	while (1) {
		wc = gtk_text_iter_get_char(&iter);
		if (!g_unichar_isspace(wc))
			break;
		gtk_text_iter_forward_char(&iter);
		if (gtk_text_iter_ends_line(&iter))
			return FALSE;
	}

	clen = g_unichar_to_utf8(wc, ch);
	if (clen != 1)
		return FALSE;

	if (!strchr("*-+", ch[0]))
		return FALSE;

	gtk_text_iter_forward_char(&iter);
	if (gtk_text_iter_ends_line(&iter))
		return FALSE;
	wc = gtk_text_iter_get_char(&iter);
	if (g_unichar_isspace(wc))
		return TRUE;

	return FALSE;
}

static gboolean compose_get_line_break_pos(GtkTextBuffer *buffer,
					   const GtkTextIter *start,
					   GtkTextIter *break_pos,
					   gint max_col,
					   gint quote_len)
{
	GtkTextIter iter = *start, line_end = *start;
	PangoLogAttr *attrs;
	gchar *str;
	gchar *p;
	gint len;
	gint i;
	gint col = 0;
	gint pos = 0;
	gboolean can_break = FALSE;
	gboolean do_break = FALSE;
	gboolean was_white = FALSE;
	gboolean prev_dont_break = FALSE;

	gtk_text_iter_forward_to_line_end(&line_end);
	str = gtk_text_buffer_get_text(buffer, &iter, &line_end, FALSE);
	len = g_utf8_strlen(str, -1);
	/* g_print("breaking line: %d: %s (len = %d)\n",
		gtk_text_iter_get_line(&iter), str, len); */
	attrs = g_new(PangoLogAttr, len + 1);

	pango_default_break(str, -1, NULL, attrs, len + 1);

	p = str;

	/* skip quote and leading spaces */
	for (i = 0; *p != '\0' && i < len; i++) {
		gunichar wc;

		wc = g_utf8_get_char(p);
		if (i >= quote_len && !g_unichar_isspace(wc))
			break;
		if (g_unichar_iswide(wc))
			col += 2;
		else if (*p == '\t')
			col += 8;
		else
			col++;
		p = g_utf8_next_char(p);
	}

	for (; *p != '\0' && i < len; i++) {
		PangoLogAttr *attr = attrs + i;
		gunichar wc;
		gint uri_len;

		if (attr->is_line_break && can_break && was_white && !prev_dont_break)
			pos = i;
		
		was_white = attr->is_white;

		/* don't wrap URI */
		if ((uri_len = get_uri_len(p)) > 0) {
			col += uri_len;
			if (pos > 0 && col > max_col) {
				do_break = TRUE;
				break;
			}
			i += uri_len - 1;
			p += uri_len;
			can_break = TRUE;
			continue;
		}

		wc = g_utf8_get_char(p);
		if (g_unichar_iswide(wc)) {
			col += 2;
			if (prev_dont_break && can_break && attr->is_line_break)
				pos = i;
		} else if (*p == '\t')
			col += 8;
		else
			col++;
		if (pos > 0 && col > max_col) {
			do_break = TRUE;
			break;
		}

		if (*p == '-' || *p == '/')
			prev_dont_break = TRUE;
		else
			prev_dont_break = FALSE;

		p = g_utf8_next_char(p);
		can_break = TRUE;
	}

	debug_print("compose_get_line_break_pos(): do_break = %d, pos = %d, col = %d\n", do_break, pos, col);

	g_free(attrs);
	g_free(str);

	*break_pos = *start;
	gtk_text_iter_set_line_offset(break_pos, pos);

	return do_break;
}

static gboolean compose_join_next_line(Compose *compose,
				       GtkTextBuffer *buffer,
				       GtkTextIter *iter,
				       const gchar *quote_str)
{
	GtkTextIter iter_ = *iter, cur, prev, next, end;
	PangoLogAttr attrs[3];
	gchar *str;
	gchar *next_quote_str;
	gunichar wc1, wc2;
	gint quote_len;
	gboolean keep_cursor = FALSE;

	if (!gtk_text_iter_forward_line(&iter_) ||
	    gtk_text_iter_ends_line(&iter_))
		return FALSE;

	next_quote_str = compose_get_quote_str(buffer, &iter_, &quote_len);

	if ((quote_str || next_quote_str) &&
	    strcmp2(quote_str, next_quote_str) != 0) {
		g_free(next_quote_str);
		return FALSE;
	}
	g_free(next_quote_str);

	end = iter_;
	if (quote_len > 0) {
		gtk_text_iter_forward_chars(&end, quote_len);
		if (gtk_text_iter_ends_line(&end))
			return FALSE;
	}

	/* don't join itemized lines */
	if (compose_is_itemized(buffer, &end))
		return FALSE;

	/* don't join signature separator */
	if (compose_is_sig_separator(compose, buffer, &iter_))
		return FALSE;

	/* delete quote str */
	if (quote_len > 0)
		gtk_text_buffer_delete(buffer, &iter_, &end);

	/* don't join line breaks put by the user */
	prev = cur = iter_;
	gtk_text_iter_backward_char(&cur);
	if (gtk_text_iter_has_tag(&cur, compose->no_join_tag)) {
		gtk_text_iter_forward_char(&cur);
		*iter = cur;
		return FALSE;
	}
	gtk_text_iter_forward_char(&cur);
	/* delete linebreak and extra spaces */
	while (gtk_text_iter_backward_char(&cur)) {
		wc1 = gtk_text_iter_get_char(&cur);
		if (!g_unichar_isspace(wc1))
			break;
		prev = cur;
	}
	next = cur = iter_;
	while (!gtk_text_iter_ends_line(&cur)) {
		wc1 = gtk_text_iter_get_char(&cur);
		if (!g_unichar_isspace(wc1))
			break;
		gtk_text_iter_forward_char(&cur);
		next = cur;
	}
	if (!gtk_text_iter_equal(&prev, &next)) {
		GtkTextMark *mark;

		mark = gtk_text_buffer_get_insert(buffer);
		gtk_text_buffer_get_iter_at_mark(buffer, &cur, mark);
		if (gtk_text_iter_equal(&prev, &cur))
			keep_cursor = TRUE;
		gtk_text_buffer_delete(buffer, &prev, &next);
	}
	iter_ = prev;

	/* insert space if required */
	gtk_text_iter_backward_char(&prev);
	wc1 = gtk_text_iter_get_char(&prev);
	wc2 = gtk_text_iter_get_char(&next);
	gtk_text_iter_forward_char(&next);
	str = gtk_text_buffer_get_text(buffer, &prev, &next, FALSE);
	pango_default_break(str, -1, NULL, attrs, 3);
	if (!attrs[1].is_line_break ||
	    (!g_unichar_iswide(wc1) || !g_unichar_iswide(wc2))) {
		gtk_text_buffer_insert(buffer, &iter_, " ", 1);
		if (keep_cursor) {
			gtk_text_iter_backward_char(&iter_);
			gtk_text_buffer_place_cursor(buffer, &iter_);
		}
	}
	g_free(str);

	*iter = iter_;
	return TRUE;
}

#define ADD_TXT_POS(bp_, ep_, pti_) \
	if ((last->next = alloca(sizeof(struct txtpos))) != NULL) { \
		last = last->next; \
		last->bp = (bp_); last->ep = (ep_); last->pti = (pti_); \
		last->next = NULL; \
	} else { \
		g_warning("alloc error scanning URIs\n"); \
	}

static gboolean automatic_break = FALSE;
static void compose_beautify_paragraph(Compose *compose, GtkTextIter *par_iter, gboolean force)
{
	GtkTextView *text = GTK_TEXT_VIEW(compose->text);
	GtkTextBuffer *buffer;
	GtkTextIter iter, break_pos, end_of_line;
	gchar *quote_str = NULL;
	gint quote_len;
	gboolean wrap_quote = prefs_common.linewrap_quote;
	gboolean prev_autowrap = compose->autowrap;
	gint startq_offset = -1, noq_offset = -1;
	gint uri_start = -1, uri_stop = -1;
	gint nouri_start = -1, nouri_stop = -1;
	gint num_blocks = 0;
	gint quotelevel = -1;

	compose->autowrap = FALSE;

	buffer = gtk_text_view_get_buffer(text);
	undo_wrapping(compose->undostruct, TRUE);
	if (par_iter) {
		iter = *par_iter;
	} else {
		GtkTextMark *mark;
		mark = gtk_text_buffer_get_insert(buffer);
		gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);
	}

	/* move to paragraph start */
	gtk_text_iter_set_line_offset(&iter, 0);
	if (gtk_text_iter_ends_line(&iter)) {
		while (gtk_text_iter_ends_line(&iter) &&
		       gtk_text_iter_forward_line(&iter))
			;
	} else {
		while (gtk_text_iter_backward_line(&iter)) {
			if (gtk_text_iter_ends_line(&iter)) {
				gtk_text_iter_forward_line(&iter);
				break;
			}
		}
	}

	/* go until paragraph end (empty line) */
	
	while (!gtk_text_iter_ends_line(&iter)) {
		gchar *scanpos = NULL;
		/* parse table - in order of priority */
		struct table {
			const gchar *needle; /* token */

			/* token search function */
			gchar    *(*search)	(const gchar *haystack,
						 const gchar *needle);
			/* part parsing function */
			gboolean  (*parse)	(const gchar *start,
						 const gchar *scanpos,
						 const gchar **bp_,
						 const gchar **ep_,
						 gboolean hdr);
			/* part to URI function */
			gchar    *(*build_uri)	(const gchar *bp,
						 const gchar *ep);
		};

		static struct table parser[] = {
			{"http://",  strcasestr, get_uri_part,   make_uri_string},
			{"https://", strcasestr, get_uri_part,   make_uri_string},
			{"ftp://",   strcasestr, get_uri_part,   make_uri_string},
			{"sftp://",  strcasestr, get_uri_part,   make_uri_string},
			{"www.",     strcasestr, get_uri_part,   make_http_string},
			{"mailto:",  strcasestr, get_uri_part,   make_uri_string},
			{"@",        strcasestr, get_email_part, make_email_string}
		};
		const gint PARSE_ELEMS = sizeof parser / sizeof parser[0];
		gint last_index = PARSE_ELEMS;
		gint  n;
		gchar *o_walk = NULL, *walk = NULL, *bp = NULL, *ep = NULL;
		gint walk_pos;
		
		if (!prev_autowrap && num_blocks == 0) {
			num_blocks++;
			g_signal_handlers_block_by_func(G_OBJECT(buffer),
					G_CALLBACK(text_inserted),
					compose);
		}
		if (gtk_text_iter_has_tag(&iter, compose->no_wrap_tag) && !force)
			goto colorize;

		uri_start = uri_stop = -1;
		quote_len = 0;
		quote_str = compose_get_quote_str(buffer, &iter, &quote_len);

		if (quote_str) {
			if (!wrap_quote) {
				if (startq_offset == -1) {
					startq_offset = gtk_text_iter_get_offset(&iter);
				}
				goto colorize;
			}
			debug_print("compose_beautify_paragraph(): quote_str = '%s'\n", quote_str);
			if (startq_offset == -1) 
				startq_offset = gtk_text_iter_get_offset(&iter);
			quotelevel = get_quote_level(quote_str, prefs_common.quote_chars);
			if (quotelevel > 2) {
				/* recycle colors */
				if (prefs_common.recycle_quote_colors)
					quotelevel %= 3;
				else
					quotelevel = 2;
			}
		} else {
			if (startq_offset == -1)
				noq_offset = gtk_text_iter_get_offset(&iter);
			quotelevel = -1;
		}

		if (prev_autowrap == FALSE && !force && !wrap_quote) {
			goto colorize;
		}
		if (compose_get_line_break_pos(buffer, &iter, &break_pos,
					       prefs_common.linewrap_len,
					       quote_len)) {
			GtkTextIter prev, next, cur;
			
			if (prev_autowrap != FALSE || force) {
				automatic_break = TRUE;
				gtk_text_buffer_insert(buffer, &break_pos, "\n", 1);
				automatic_break = FALSE;
			} else if (quote_str && wrap_quote) {
				automatic_break = TRUE;
				gtk_text_buffer_insert(buffer, &break_pos, "\n", 1);
				automatic_break = FALSE;
			} else 
				goto colorize;
			/* remove trailing spaces */
			cur = break_pos;
			gtk_text_iter_backward_char(&cur);
			prev = next = cur;
			while (!gtk_text_iter_starts_line(&cur)) {
				gunichar wc;

				gtk_text_iter_backward_char(&cur);
				wc = gtk_text_iter_get_char(&cur);
				if (!g_unichar_isspace(wc))
					break;
				prev = cur;
			}
			if (!gtk_text_iter_equal(&prev, &next)) {
				gtk_text_buffer_delete(buffer, &prev, &next);
				break_pos = next;
				gtk_text_iter_forward_char(&break_pos);
			}

			if (quote_str)
				gtk_text_buffer_insert(buffer, &break_pos,
						       quote_str, -1);

			iter = break_pos;
			compose_join_next_line(compose, buffer, &iter, quote_str);

			/* move iter to current line start */
			gtk_text_iter_set_line_offset(&iter, 0);
			if (quote_str) {
				g_free(quote_str);
				quote_str = NULL;
			}
			continue;
		} else {
			/* move iter to next line start */
			iter = break_pos;
		}

colorize:
		if (!prev_autowrap && num_blocks > 0) {
			num_blocks--;
			g_signal_handlers_unblock_by_func(G_OBJECT(buffer),
					G_CALLBACK(text_inserted),
					compose);
		}
		end_of_line = iter;
		while (!gtk_text_iter_ends_line(&end_of_line)) {
			gtk_text_iter_forward_char(&end_of_line);
		}
		o_walk = walk = gtk_text_buffer_get_text(buffer, &iter, &end_of_line, FALSE);

		nouri_start = gtk_text_iter_get_offset(&iter);
		nouri_stop = gtk_text_iter_get_offset(&end_of_line);

		walk_pos = gtk_text_iter_get_offset(&iter);
		/* FIXME: this looks phony. scanning for anything in the parse table */
		for (n = 0; n < PARSE_ELEMS; n++) {
			gchar *tmp;

			tmp = parser[n].search(walk, parser[n].needle);
			if (tmp) {
				if (scanpos == NULL || tmp < scanpos) {
					scanpos = tmp;
					last_index = n;
				}
			}					
		}

		bp = ep = 0;
		if (scanpos) {
			/* check if URI can be parsed */
			if (parser[last_index].parse(walk, scanpos, (const gchar **)&bp,
					(const gchar **)&ep, FALSE)
			    && (size_t) (ep - bp - 1) > strlen(parser[last_index].needle)) {
					walk = ep;
			} else
				walk = scanpos +
					strlen(parser[last_index].needle);
		} 
		if (bp && ep) {
			uri_start = walk_pos + (bp - o_walk);
			uri_stop  = walk_pos + (ep - o_walk);
		}
		g_free(o_walk);
		o_walk = NULL;
		gtk_text_iter_forward_line(&iter);
		g_free(quote_str);
		quote_str = NULL;
		if (startq_offset != -1) {
			GtkTextIter startquote, endquote;
			gtk_text_buffer_get_iter_at_offset(
				buffer, &startquote, startq_offset);
			endquote = iter;
			switch (quotelevel) {
			case 0:	gtk_text_buffer_apply_tag_by_name(
					buffer, "quote0", &startquote, &endquote);
				break;
			case 1:	gtk_text_buffer_apply_tag_by_name(
					buffer, "quote1", &startquote, &endquote);
				break;
			case 2:	gtk_text_buffer_apply_tag_by_name(
					buffer, "quote2", &startquote, &endquote);
				break;
			}
			startq_offset = -1;
		} else if (noq_offset != -1) {
			GtkTextIter startnoquote, endnoquote;
			gtk_text_buffer_get_iter_at_offset(
				buffer, &startnoquote, noq_offset);
			endnoquote = iter;
			gtk_text_buffer_remove_tag_by_name(
				buffer, "quote0", &startnoquote, &endnoquote);
			gtk_text_buffer_remove_tag_by_name(
				buffer, "quote1", &startnoquote, &endnoquote);
			gtk_text_buffer_remove_tag_by_name(
				buffer, "quote2", &startnoquote, &endnoquote);
			noq_offset = -1;
		}
		
		/* always */ {
			GtkTextIter nouri_start_iter, nouri_end_iter;
			gtk_text_buffer_get_iter_at_offset(
				buffer, &nouri_start_iter, nouri_start);
			gtk_text_buffer_get_iter_at_offset(
				buffer, &nouri_end_iter, nouri_stop);
			gtk_text_buffer_remove_tag_by_name(
				buffer, "link", &nouri_start_iter, &nouri_end_iter);
		}
		if (uri_start > 0 && uri_stop > 0) {
			GtkTextIter uri_start_iter, uri_end_iter;
			gtk_text_buffer_get_iter_at_offset(
				buffer, &uri_start_iter, uri_start);
			gtk_text_buffer_get_iter_at_offset(
				buffer, &uri_end_iter, uri_stop);
			gtk_text_buffer_apply_tag_by_name(
				buffer, "link", &uri_start_iter, &uri_end_iter);
		}
	}

	if (par_iter)
		*par_iter = iter;
	undo_wrapping(compose->undostruct, FALSE);
	compose->autowrap = prev_autowrap;
}

void compose_action_cb(void *data)
{
	Compose *compose = (Compose *)data;
	compose_wrap_all(compose);
}

static void compose_wrap_all(Compose *compose)
{
	compose_wrap_all_full(compose, FALSE);
}

static void compose_wrap_all_full(Compose *compose, gboolean force)
{
	GtkTextView *text = GTK_TEXT_VIEW(compose->text);
	GtkTextBuffer *buffer;
	GtkTextIter iter;

	buffer = gtk_text_view_get_buffer(text);

	gtk_text_buffer_get_start_iter(buffer, &iter);
	while (!gtk_text_iter_is_end(&iter))
		compose_beautify_paragraph(compose, &iter, force);

}

static void compose_set_title(Compose *compose)
{
	gchar *str;
	gchar *edited;
	gchar *subject;
	
	edited = compose->modified ? _(" [Edited]") : "";
	
	subject = gtk_editable_get_chars(
			GTK_EDITABLE(compose->subject_entry), 0, -1);

	if (subject && strlen(subject))
		str = g_strdup_printf(_("%s - Compose message%s"),
				      subject, edited);	
	else
		str = g_strdup_printf(_("[no subject] - Compose message%s"), edited);
	gtk_window_set_title(GTK_WINDOW(compose->window), str);
	g_free(str);
	g_free(subject);
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

#define QUOTE_IF_REQUIRED_NORMAL(out, str, errret)			\
{									\
	if (*str != '"' && strpbrk(str, ",.[]<>")) {			\
		gchar *__tmp;						\
		gint len;						\
									\
		len = strlen(str) + 3;					\
		if ((__tmp = alloca(len)) == NULL) {			\
			g_warning("can't allocate memory\n");		\
			errret;						\
		}							\
		g_snprintf(__tmp, len, "\"%s\"", str);			\
		out = __tmp;						\
	} else {							\
		gchar *__tmp;						\
									\
		if ((__tmp = alloca(strlen(str) + 1)) == NULL) {	\
			g_warning("can't allocate memory\n");		\
			errret;						\
		} else 							\
			strcpy(__tmp, str);				\
									\
		out = __tmp;						\
	}								\
}

static void compose_select_account(Compose *compose, PrefsAccount *account,
				   gboolean init)
{
	GtkItemFactory *ifactory;
	gchar *from = NULL;

	g_return_if_fail(account != NULL);

	compose->account = account;

	if (account->name && *account->name) {
		gchar *buf;
		QUOTE_IF_REQUIRED_NORMAL(buf, account->name, return);
		from = g_strdup_printf("%s <%s>",
				       buf, account->address);
		gtk_entry_set_text(GTK_ENTRY(compose->from_name), from);
	} else {
		from = g_strdup_printf("<%s>",
				       account->address);
		gtk_entry_set_text(GTK_ENTRY(compose->from_name), from);
	}

	g_free(from);

	compose_set_title(compose);

	ifactory = gtk_item_factory_from_widget(compose->menubar);

	if (account->default_sign && compose->mode != COMPOSE_REDIRECT)
		menu_set_active(ifactory, "/Options/Sign", TRUE);
	else
		menu_set_active(ifactory, "/Options/Sign", FALSE);
	if (account->default_encrypt && compose->mode != COMPOSE_REDIRECT)
		menu_set_active(ifactory, "/Options/Encrypt", TRUE);
	else
		menu_set_active(ifactory, "/Options/Encrypt", FALSE);
				       
	activate_privacy_system(compose, account, FALSE);

	if (!init && compose->mode != COMPOSE_REDIRECT) {
		undo_block(compose->undostruct);
		compose_insert_sig(compose, TRUE);
		undo_unblock(compose->undostruct);
	}
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

static gboolean compose_check_for_set_recipients(Compose *compose)
{
	if (compose->account->set_autocc && compose->account->auto_cc) {
		gboolean found_other = FALSE;
		GSList *list;
		/* search header entries for to and newsgroup entries */
		for (list = compose->header_list; list; list = list->next) {
			gchar *entry;
			gchar *header;
			entry = gtk_editable_get_chars(GTK_EDITABLE(((ComposeHeaderEntry *)list->data)->entry), 0, -1);
			header = gtk_editable_get_chars(GTK_EDITABLE(GTK_COMBO(((ComposeHeaderEntry *)list->data)->combo)->entry), 0, -1);
			g_strstrip(entry);
			if (strcmp(entry, compose->account->auto_cc)
			||  strcmp(header, (prefs_common.trans_hdr ? gettext("Cc:") : "Cc:"))) {
				found_other = TRUE;
				g_free(entry);
				break;
			}
			g_free(entry);
			g_free(header);
		}
		if (!found_other) {
			AlertValue aval;
			if (compose->batch) {
				gtk_widget_show_all(compose->window);
			}
			aval = alertpanel(_("Send"),
					  _("The only recipient is the default CC address. Send anyway?"),
					  GTK_STOCK_CANCEL, _("+_Send"), NULL);
			if (aval != G_ALERTALTERNATE)
				return FALSE;
		}
	}
	if (compose->account->set_autobcc && compose->account->auto_bcc) {
		gboolean found_other = FALSE;
		GSList *list;
		/* search header entries for to and newsgroup entries */
		for (list = compose->header_list; list; list = list->next) {
			gchar *entry;
			gchar *header;
			entry = gtk_editable_get_chars(GTK_EDITABLE(((ComposeHeaderEntry *)list->data)->entry), 0, -1);
			header = gtk_editable_get_chars(GTK_EDITABLE(GTK_COMBO(((ComposeHeaderEntry *)list->data)->combo)->entry), 0, -1);
			g_strstrip(entry);
			if (strcmp(entry, compose->account->auto_cc)
			||  strcmp(header, (prefs_common.trans_hdr ? gettext("Bcc:") : "Bcc:"))) {
				found_other = TRUE;
				g_free(entry);
				break;
			}
			g_free(entry);
			g_free(header);
		}
		if (!found_other) {
			AlertValue aval;
			if (compose->batch) {
				gtk_widget_show_all(compose->window);
			}
			aval = alertpanel(_("Send"),
					  _("The only recipient is the default BCC address. Send anyway?"),
					  GTK_STOCK_CANCEL, _("+_Send"), NULL);
			if (aval != G_ALERTALTERNATE)
				return FALSE;
		}
	}
	return TRUE;
}

static gboolean compose_check_entries(Compose *compose, gboolean check_subject)
{
	const gchar *str;

	if (compose_check_for_valid_recipient(compose) == FALSE) {
		if (compose->batch) {
			gtk_widget_show_all(compose->window);
		}
		alertpanel_error(_("Recipient is not specified."));
		return FALSE;
	}

	if (compose_check_for_set_recipients(compose) == FALSE) {
		return FALSE;
	}

	if (!compose->batch) {
		str = gtk_entry_get_text(GTK_ENTRY(compose->subject_entry));
		if (*str == '\0' && check_subject == TRUE) {
			AlertValue aval;

			aval = alertpanel(_("Send"),
					  _("Subject is empty. Send it anyway?"),
					  GTK_STOCK_CANCEL, _("+_Send"), NULL);
			if (aval != G_ALERTALTERNATE)
				return FALSE;
		}
	}

	return TRUE;
}

gint compose_send(Compose *compose)
{
	gint msgnum;
	FolderItem *folder = NULL;
	gint val = -1;
	gchar *msgpath = NULL;
	gboolean discard_window = FALSE;
	gchar *errstr = NULL;
	if (prefs_common.send_dialog_mode != SEND_DIALOG_ALWAYS
			|| compose->batch == TRUE)
		discard_window = TRUE;

	compose_allow_user_actions (compose, FALSE);
	compose->sending = TRUE;

	if (compose_check_entries(compose, TRUE) == FALSE) {
		if (compose->batch) {
			gtk_widget_show_all(compose->window);
		}
		goto bail;
	}

	val = compose_queue(compose, &msgnum, &folder, &msgpath, TRUE);

	if (val) {
		if (compose->batch) {
			gtk_widget_show_all(compose->window);
		}
		if (val == -4) {
			alertpanel_error(_("Could not queue message for sending:\n\n"
					   "Charset conversion failed."));
		} else if (val == -3) {
			if (privacy_peek_error())
			alertpanel_error(_("Could not queue message for sending:\n\n"
					   "Signature failed: %s"), privacy_get_error());
		} else if (val == -2 && errno != 0) {
			alertpanel_error(_("Could not queue message for sending:\n\n%s."), strerror(errno));
		} else {
			alertpanel_error(_("Could not queue message for sending."));
		}
		goto bail;
	}

	if (discard_window) {
		compose->sending = FALSE;
		compose_close(compose);
		/* No more compose access in the normal codepath 
		 * after this point! */
		compose = NULL;
	}

	if (msgnum == 0) {
		alertpanel_error(_("The message was queued but could not be "
				   "sent.\nUse \"Send queued messages\" from "
				   "the main window to retry."));
		if (!discard_window) {
			goto bail;
		}
		return -1;
	}
	
	if (msgpath == NULL) {
		msgpath = folder_item_fetch_msg(folder, msgnum);
		val = procmsg_send_message_queue(msgpath, &errstr);
		g_free(msgpath);
	} else {
		val = procmsg_send_message_queue(msgpath, &errstr);
		g_unlink(msgpath);
		g_free(msgpath);
	}
	if (!discard_window) {
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
		if (!discard_window)
			compose_close(compose);
	} else {
		if (errstr) {
			gchar *tmp = g_strdup_printf(_("%s\nUse \"Send queued messages\" from "
				   "the main window to retry."), errstr);
			g_free(errstr);
			alertpanel_error_log(tmp);
			g_free(tmp);
		} else {
			alertpanel_error_log(_("The message was queued but could not be "
				   "sent.\nUse \"Send queued messages\" from "
				   "the main window to retry."));
		}
		if (!discard_window) {
			goto bail;		
		}
		return -1;
 	}

	return 0;

bail:
	compose_allow_user_actions (compose, TRUE);
	compose->sending = FALSE;
	compose->modified = TRUE; 

	return -1;
}

static gboolean compose_use_attach(Compose *compose) 
{
	GtkTreeModel *model = gtk_tree_view_get_model
				(GTK_TREE_VIEW(compose->attach_clist));
	return gtk_tree_model_iter_n_children(model, NULL) > 0;
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
	const gchar *headerentryname;
	gchar *cc_hdr;
	gchar *to_hdr;

	debug_print("Writing redirect header\n");

	cc_hdr = prefs_common.trans_hdr ? _("Cc:") : "Cc:";
 	to_hdr = prefs_common.trans_hdr ? _("To:") : "To:";

	first_to_address = TRUE;
	for (list = compose->header_list; list; list = list->next) {
		headerentry = ((ComposeHeaderEntry *)list->data);
		headerentryname = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(headerentry->combo)->entry));

		if (g_utf8_collate(headerentryname, to_hdr) == 0) {
			const gchar *entstr = gtk_entry_get_text(GTK_ENTRY(headerentry->entry));
			Xstrdup_a(str, entstr, return -1);
			g_strstrip(str);
			if (str[0] != '\0') {
				compose_convert_header
					(compose, buf, sizeof(buf), str,
					strlen("Resent-To") + 2, TRUE);

				if (first_to_address) {
					fprintf(fp, "Resent-To: ");
					first_to_address = FALSE;
				} else {
					fprintf(fp, ",");
                                }
				fprintf(fp, "%s", buf);
			}
		}
	}
	if (!first_to_address) {
		fprintf(fp, "\n");
	}

	first_cc_address = TRUE;
	for (list = compose->header_list; list; list = list->next) {
		headerentry = ((ComposeHeaderEntry *)list->data);
		headerentryname = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(headerentry->combo)->entry));

		if (g_utf8_collate(headerentryname, cc_hdr) == 0) {
			const gchar *strg = gtk_entry_get_text(GTK_ENTRY(headerentry->entry));
			Xstrdup_a(str, strg, return -1);
			g_strstrip(str);
			if (str[0] != '\0') {
				compose_convert_header
					(compose, buf, sizeof(buf), str,
					strlen("Resent-Cc") + 2, TRUE);

                                if (first_cc_address) {
                                        fprintf(fp, "Resent-Cc: ");
                                        first_cc_address = FALSE;
                                } else {
                                        fprintf(fp, ",");
                                }
				fprintf(fp, "%s", buf);
			}
		}
	}
	if (!first_cc_address) {
		fprintf(fp, "\n");
        }
	
	return(0);
}

static gint compose_redirect_write_headers(Compose *compose, FILE *fp)
{
	gchar buf[BUFFSIZE];
	gchar *str;
	const gchar *entstr;
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
			(compose, buf, sizeof(buf), compose->account->name,
			 strlen("From: "), TRUE);
		fprintf(fp, "Resent-From: %s <%s>\n",
			buf, compose->account->address);
	} else
		fprintf(fp, "Resent-From: %s\n", compose->account->address);

	/* Subject */
	entstr = gtk_entry_get_text(GTK_ENTRY(compose->subject_entry));
	if (*entstr != '\0') {
		Xstrdup_a(str, entstr, return -1);
		g_strstrip(str);
		if (*str != '\0') {
			compose_convert_header(compose, buf, sizeof(buf), str,
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

	if ((fp = g_fopen(compose->redirect_filename, "rb")) == NULL) {
		FILE_OP_ERROR(compose->redirect_filename, "fopen");
		return -1;
	}

	while (procheader_get_one_field_asis(buf, sizeof(buf), fp) != -1) {
		/* should filter returnpath, delivered-to */
		if (g_ascii_strncasecmp(buf, "Return-Path:",
				   	strlen("Return-Path:")) == 0 ||
		    g_ascii_strncasecmp(buf, "Delivered-To:",
				  	strlen("Delivered-To:")) == 0 ||
		    g_ascii_strncasecmp(buf, "Received:",
				  	strlen("Received:")) == 0 ||
		    g_ascii_strncasecmp(buf, "Subject:",
				  	strlen("Subject:")) == 0 ||
		    g_ascii_strncasecmp(buf, "X-UIDL:",
				  	strlen("X-UIDL:")) == 0)
			continue;

		if (fputs(buf, fdest) == -1)
			goto error;

		if (!prefs_common.redirect_keep_from) {
			if (g_ascii_strncasecmp(buf, "From:",
					  strlen("From:")) == 0) {
				fputs(" (by way of ", fdest);
				if (compose->account->name
				    && *compose->account->name) {
					compose_convert_header
						(compose, buf, sizeof(buf),
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

static gint compose_write_to_file(Compose *compose, FILE *fp, gint action, gboolean attach_parts)
{
	GtkTextBuffer *buffer;
	GtkTextIter start, end;
	gchar *chars;
	gchar *buf;
	const gchar *out_codeset;
	EncodingType encoding;
	MimeInfo *mimemsg, *mimetext;
	gint line;

	if (action == COMPOSE_WRITE_FOR_SEND)
		attach_parts = TRUE;

	/* create message MimeInfo */
	mimemsg = procmime_mimeinfo_new();
        mimemsg->type = MIMETYPE_MESSAGE;
        mimemsg->subtype = g_strdup("rfc822");
	mimemsg->content = MIMECONTENT_MEM;
	mimemsg->tmp = TRUE; /* must free content later */
	mimemsg->data.mem = compose_get_header(compose);

	/* Create text part MimeInfo */
	/* get all composed text */
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(compose->text));
	gtk_text_buffer_get_start_iter(buffer, &start);
	gtk_text_buffer_get_end_iter(buffer, &end);
	chars = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
	if (is_ascii_str(chars)) {
		buf = chars;
		chars = NULL;
		out_codeset = CS_US_ASCII;
		encoding = ENC_7BIT;
	} else {
		const gchar *src_codeset = CS_INTERNAL;

		out_codeset = conv_get_charset_str(compose->out_encoding);

		if (!out_codeset) {
			gchar *test_conv_global_out = NULL;
			gchar *test_conv_reply = NULL;

			/* automatic mode. be automatic. */
			codeconv_set_strict(TRUE);
			
			out_codeset = conv_get_outgoing_charset_str();
			if (out_codeset) {
				debug_print("trying to convert to %s\n", out_codeset);
				test_conv_global_out = conv_codeset_strdup(chars, src_codeset, out_codeset);
			}
			
			if (!test_conv_global_out && compose->orig_charset
			&&  strcmp(compose->orig_charset, CS_US_ASCII)) {
				out_codeset = compose->orig_charset;
				debug_print("failure; trying to convert to %s\n", out_codeset);
				test_conv_reply = conv_codeset_strdup(chars, src_codeset, out_codeset);
			}
			
			if (!test_conv_global_out && !test_conv_reply) {
				/* we're lost */
				out_codeset = CS_INTERNAL;
				debug_print("failure; finally using %s\n", out_codeset);
			}
			g_free(test_conv_global_out);
			g_free(test_conv_reply);
			codeconv_set_strict(FALSE);
		}

		if (!g_ascii_strcasecmp(out_codeset, CS_US_ASCII))
			out_codeset = CS_ISO_8859_1;

		if (prefs_common.encoding_method == CTE_BASE64)
			encoding = ENC_BASE64;
		else if (prefs_common.encoding_method == CTE_QUOTED_PRINTABLE)
			encoding = ENC_QUOTED_PRINTABLE;
		else if (prefs_common.encoding_method == CTE_8BIT)
			encoding = ENC_8BIT;
		else
			encoding = procmime_get_encoding_for_charset(out_codeset);

		debug_print("src encoding = %s, out encoding = %s, transfer encoding = %s\n",
			    src_codeset, out_codeset, procmime_get_encoding_str(encoding));

		if (action == COMPOSE_WRITE_FOR_SEND) {
			codeconv_set_strict(TRUE);
			buf = conv_codeset_strdup(chars, src_codeset, out_codeset);
			codeconv_set_strict(FALSE);

			if (!buf) {
				AlertValue aval;
				gchar *msg;

				msg = g_strdup_printf(_("Can't convert the character encoding of the message \n"
							"to the specified %s charset.\n"
							"Send it as %s?"), out_codeset, src_codeset);
				aval = alertpanel_full(_("Error"), msg, GTK_STOCK_CANCEL, _("+_Send"), NULL, FALSE,
						      NULL, ALERT_ERROR, G_ALERTDEFAULT);
				g_free(msg);

				if (aval != G_ALERTALTERNATE) {
					g_free(chars);
					return -3;
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
	mimetext->tmp = TRUE; /* must free content later */
	mimetext->data.mem = buf;
	mimetext->type = MIMETYPE_TEXT;
	mimetext->subtype = g_strdup("plain");
	g_hash_table_insert(mimetext->typeparameters, g_strdup("charset"),
			    g_strdup(out_codeset));
			    
	/* protect trailing spaces when signing message */
	if (action == COMPOSE_WRITE_FOR_SEND && compose->use_signing && 
	    privacy_system_can_sign(compose->privacy_system)) {
		encoding = ENC_QUOTED_PRINTABLE;
	}
	
	debug_print("main text: %d bytes encoded as %s in %d\n",
		strlen(buf), out_codeset, encoding);

	/* check for line length limit */
	if (action == COMPOSE_WRITE_FOR_SEND &&
	    encoding != ENC_QUOTED_PRINTABLE && encoding != ENC_BASE64 &&
	    check_line_length(buf, 1000, &line) < 0) {
		AlertValue aval;
		gchar *msg;

		msg = g_strdup_printf
			(_("Line %d exceeds the line length limit (998 bytes).\n"
			   "The contents of the message might be broken on the way to the delivery.\n"
			   "\n"
			   "Send it anyway?"), line + 1);
		aval = alertpanel(_("Warning"), msg, GTK_STOCK_CANCEL, GTK_STOCK_OK, NULL);
		g_free(msg);
		if (aval != G_ALERTALTERNATE) {
			return -1;
		}
	}
	
	if (encoding != ENC_UNKNOWN)
		procmime_encode_content(mimetext, encoding);

	/* append attachment parts */
	if (compose_use_attach(compose) && attach_parts) {
		MimeInfo *mimempart;
		gchar *boundary = NULL;
		mimempart = procmime_mimeinfo_new();
    		mimempart->content = MIMECONTENT_EMPTY;
    		mimempart->type = MIMETYPE_MULTIPART;
	        mimempart->subtype = g_strdup("mixed");

		do {
			g_free(boundary);
			boundary = generate_mime_boundary(NULL);
		} while (strstr(buf, boundary) != NULL);

    		g_hash_table_insert(mimempart->typeparameters, g_strdup("boundary"),
				    boundary);

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
			return -2;

	procmime_write_mimeinfo(mimemsg, fp);
	
	procmime_mimeinfo_free_all(mimemsg);

	return 0;
}

static gint compose_write_body_to_file(Compose *compose, const gchar *file)
{
	GtkTextBuffer *buffer;
	GtkTextIter start, end;
	FILE *fp;
	size_t len;
	gchar *chars, *tmp;

	if ((fp = g_fopen(file, "wb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		return -1;
	}

	/* chmod for security */
	if (change_file_mode_rw(fp, file) < 0) {
		FILE_OP_ERROR(file, "chmod");
		g_warning("can't change file mode\n");
	}

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(compose->text));
	gtk_text_buffer_get_start_iter(buffer, &start);
	gtk_text_buffer_get_end_iter(buffer, &end);
	tmp = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);

	chars = conv_codeset_strdup
		(tmp, CS_INTERNAL, conv_get_locale_charset_str());

	g_free(tmp);
	if (!chars) return -1;

	/* write body */
	len = strlen(chars);
	if (fwrite(chars, sizeof(gchar), len, fp) != len) {
		FILE_OP_ERROR(file, "fwrite");
		g_free(chars);
		fclose(fp);
		g_unlink(file);
		return -1;
	}

	g_free(chars);

	if (fclose(fp) == EOF) {
		FILE_OP_ERROR(file, "fclose");
		g_unlink(file);
		return -1;
	}
	return 0;
}

static gint compose_remove_reedit_target(Compose *compose, gboolean force)
{
	FolderItem *item;
	MsgInfo *msginfo = compose->targetinfo;

	g_return_val_if_fail(compose->mode == COMPOSE_REEDIT, -1);
	if (!msginfo) return -1;

	if (!force && MSG_IS_LOCKED(msginfo->flags))
		return 0;

	item = msginfo->folder;
	g_return_val_if_fail(item != NULL, -1);

	if (procmsg_msg_exist(msginfo) &&
	    (folder_has_parent_of_type(item, F_QUEUE) ||
	     folder_has_parent_of_type(item, F_DRAFT) 
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

gint compose_queue(Compose *compose, gint *msgnum, FolderItem **item, gchar **msgpath,
		   gboolean remove_reedit_target)
{
	return compose_queue_sub (compose, msgnum, item, msgpath, FALSE, remove_reedit_target);
}
static gint compose_queue_sub(Compose *compose, gint *msgnum, FolderItem **item, 
			      gchar **msgpath, gboolean check_subject,
			      gboolean remove_reedit_target)
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
		if (compose->batch) {
			gtk_widget_show_all(compose->window);
		}
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

	/* write queue header */
	tmp = g_strdup_printf("%s%cqueue.%p", get_tmp_dir(),
			      G_DIR_SEPARATOR, compose);
	if ((fp = g_fopen(tmp, "wb")) == NULL) {
		FILE_OP_ERROR(tmp, "fopen");
		g_free(tmp);
		lock = FALSE;
		return -2;
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
		if (compose->use_encryption) {
			gchar *encdata;
			if (mailac && mailac->encrypt_to_self) {
				GSList *tmp_list = g_slist_copy(compose->to_list);
				tmp_list = g_slist_append(tmp_list, compose->account->address);
				encdata = privacy_get_encrypt_data(compose->privacy_system, tmp_list);
				g_slist_free(tmp_list);
			} else {
				encdata = privacy_get_encrypt_data(compose->privacy_system, compose->to_list);
			}
			if (encdata != NULL) {
				if (strcmp(encdata, "_DONT_ENCRYPT_")) {
					fprintf(fp, "X-Sylpheed-Encrypt:%d\n", compose->use_encryption);
					fprintf(fp, "X-Sylpheed-Encrypt-Data:%s\n", 
						encdata);
				} /* else we finally dont want to encrypt */
			} else {
				fprintf(fp, "X-Sylpheed-Encrypt:%d\n", compose->use_encryption);
				/* and if encdata was null, it means there's been a problem in 
				 * key selection */
			}
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
	/* Save copy folder */
	if (compose->return_receipt) {
		fprintf(fp, "RRCPT:1\n");
	}
	/* Message-ID of message replying to */
	if ((compose->replyinfo != NULL) && (compose->replyinfo->msgid != NULL)) {
		gchar *folderid;
		
		folderid = folder_item_get_identifier(compose->replyinfo->folder);
		fprintf(fp, "RMID:%s\t%d\t%s\n", folderid, compose->replyinfo->msgnum, compose->replyinfo->msgid);
		g_free(folderid);
	}
	/* Message-ID of message forwarding to */
	if ((compose->fwdinfo != NULL) && (compose->fwdinfo->msgid != NULL)) {
		gchar *folderid;
		
		folderid = folder_item_get_identifier(compose->fwdinfo->folder);
		fprintf(fp, "FMID:%s\t%d\t%s\n", folderid, compose->fwdinfo->msgnum, compose->fwdinfo->msgid);
		g_free(folderid);
	}

	/* end of headers */
	fprintf(fp, "X-Sylpheed-End-Special-Headers: 1\n");

	if (compose->redirect_filename != NULL) {
		if (compose_redirect_write_to_file(compose, fp) < 0) {
			lock = FALSE;
			fclose(fp);
			g_unlink(tmp);
			g_free(tmp);
			return -2;
		}
	} else {
		gint result = 0;
		if ((result = compose_write_to_file(compose, fp, COMPOSE_WRITE_FOR_SEND, TRUE)) < 0) {
			lock = FALSE;
			fclose(fp);
			g_unlink(tmp);
			g_free(tmp);
			return result - 1; /* -2 for a generic error, -3 for signing error, -4 for encoding */
		}
	}

	if (fclose(fp) == EOF) {
		FILE_OP_ERROR(tmp, "fclose");
		g_unlink(tmp);
		g_free(tmp);
		lock = FALSE;
		return -2;
	}

	if (item && *item) {
		queue = *item;
	} else {
		queue = account_get_special_folder(compose->account, F_QUEUE);
	}
	if (!queue) {
		g_warning("can't find queue folder\n");
		g_unlink(tmp);
		g_free(tmp);
		lock = FALSE;
		return -1;
	}
	folder_item_scan(queue);
	if ((num = folder_item_add_msg(queue, tmp, NULL, FALSE)) < 0) {
		g_warning("can't queue the message\n");
		g_unlink(tmp);
		g_free(tmp);
		lock = FALSE;
		return -1;
	}
	
	if (msgpath == NULL) {
		g_unlink(tmp);
		g_free(tmp);
	} else
		*msgpath = tmp;

	if (compose->mode == COMPOSE_REEDIT && remove_reedit_target) {
		compose_remove_reedit_target(compose, FALSE);
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
	GtkTreeView *tree_view = GTK_TREE_VIEW(compose->attach_clist);
	MimeInfo *mimepart;
	struct stat statbuf;
	gchar *type, *subtype;
	GtkTreeModel *model;
	GtkTreeIter iter;

	model = gtk_tree_view_get_model(tree_view);
	
	if (!gtk_tree_model_get_iter_first(model, &iter))
		return;
	do {
		gtk_tree_model_get(model, &iter,
				   COL_DATA, &ainfo,
				   -1);
							   
		mimepart = procmime_mimeinfo_new();
		mimepart->content = MIMECONTENT_FILE;
		mimepart->data.filename = g_strdup(ainfo->file);
		mimepart->tmp = FALSE; /* or we destroy our attachment */
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
		    !g_ascii_strcasecmp(mimepart->subtype, "rfc822")) {
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
	} while (gtk_tree_model_iter_next(model, &iter));
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
	const gchar * headerentryname;
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

		if (!g_utf8_collate(trans_fieldname, headerentryname)) {
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
			(compose, buf, fieldstr->len * 4  + 256, fieldstr->str,
			strlen(fieldname) + 2, TRUE);
		g_string_append_printf(header, "%s: %s\n", fieldname, buf);
		g_free(buf);
	}

	g_free(fieldname_w_colon);
	g_string_free(fieldstr, TRUE);

	return;
}

static gchar *compose_get_header(Compose *compose)
{
	gchar buf[BUFFSIZE];
	const gchar *entry_str;
	gchar *str;
	gchar *name;
	GSList *list;
	gchar *std_headers[] = {"To:", "Cc:", "Bcc:", "Newsgroups:", "Reply-To:", "Followup-To:", NULL};
	GString *header;
	gchar *from_name = NULL, *from_address = NULL;
	gchar *tmp;

	g_return_val_if_fail(compose->account != NULL, NULL);
	g_return_val_if_fail(compose->account->address != NULL, NULL);

	header = g_string_sized_new(64);

	/* Date */
	get_rfc822_date(buf, sizeof(buf));
	g_string_append_printf(header, "Date: %s\n", buf);

	/* From */
	
	if (compose->account->name && *compose->account->name) {
		gchar *buf;
		QUOTE_IF_REQUIRED(buf, compose->account->name);
		tmp = g_strdup_printf("%s <%s>",
			buf, compose->account->address);
	} else {
		tmp = g_strdup_printf("%s",
			compose->account->address);
	}
	if (!strcmp(gtk_entry_get_text(GTK_ENTRY(compose->from_name)), tmp)
	||  strlen(gtk_entry_get_text(GTK_ENTRY(compose->from_name))) == 0) {
		/* use default */
		from_name = compose->account->name ? g_strdup(compose->account->name):NULL;
		from_address = g_strdup(compose->account->address);
	} else {
		gchar *spec = gtk_editable_get_chars(GTK_EDITABLE(compose->from_name), 0, -1);
		/* extract name and address */
		if (strstr(spec, " <") && strstr(spec, ">")) {
			from_address = g_strdup(strrchr(spec, '<')+1);
			*(strrchr(from_address, '>')) = '\0';
			from_name = g_strdup(spec);
			*(strrchr(from_name, '<')) = '\0';
		} else {
			from_name = NULL;
			from_address = g_strdup(spec);
		}
		g_free(spec);
	}
	g_free(tmp);
	
	
	if (from_name && *from_name) {
		compose_convert_header
			(compose, buf, sizeof(buf), from_name,
			 strlen("From: "), TRUE);
		QUOTE_IF_REQUIRED(name, buf);
		
		g_string_append_printf(header, "From: %s <%s>\n",
			name, from_address);
	} else
		g_string_append_printf(header, "From: %s\n", from_address);
	
	g_free(from_name);
	g_free(from_address);

	/* To */
	compose_add_headerfield_from_headerlist(compose, header, "To", ", ");

	/* Newsgroups */
	compose_add_headerfield_from_headerlist(compose, header, "Newsgroups", ",");

	/* Cc */
	compose_add_headerfield_from_headerlist(compose, header, "Cc", ", ");

	/* Bcc */
	compose_add_headerfield_from_headerlist(compose, header, "Bcc", ", ");

	/* Subject */
	str = gtk_editable_get_chars(GTK_EDITABLE(compose->subject_entry), 0, -1);

	if (*str != '\0' && !IS_IN_CUSTOM_HEADER("Subject")) {
		g_strstrip(str);
		if (*str != '\0') {
			compose_convert_header(compose, buf, sizeof(buf), str,
					       strlen("Subject: "), FALSE);
			g_string_append_printf(header, "Subject: %s\n", buf);
		}
	}
	g_free(str);

	/* Message-ID */
	if (compose->account->gen_msgid) {
		generate_msgid(buf, sizeof(buf));
		g_string_append_printf(header, "Message-ID: <%s>\n", buf);
		compose->msgid = g_strdup(buf);
	}

	if (compose->remove_references == FALSE) {
		/* In-Reply-To */
		if (compose->inreplyto && compose->to_list)
			g_string_append_printf(header, "In-Reply-To: <%s>\n", compose->inreplyto);
	
		/* References */
		if (compose->references)
			g_string_append_printf(header, "References: %s\n", compose->references);
	}

	/* Followup-To */
	compose_add_headerfield_from_headerlist(compose, header, "Followup-To", ",");

	/* Reply-To */
	compose_add_headerfield_from_headerlist(compose, header, "Reply-To", ", ");

	/* Organization */
	if (compose->account->organization &&
	    strlen(compose->account->organization) &&
	    !IS_IN_CUSTOM_HEADER("Organization")) {
		compose_convert_header(compose, buf, sizeof(buf),
				       compose->account->organization,
				       strlen("Organization: "), FALSE);
		g_string_append_printf(header, "Organization: %s\n", buf);
	}

	/* Program version and system info */
	if (g_slist_length(compose->to_list) && !IS_IN_CUSTOM_HEADER("X-Mailer") &&
	    !compose->newsgroup_list) {
		g_string_append_printf(header, "X-Mailer: %s (GTK+ %d.%d.%d; %s)\n",
			prog_version,
			gtk_major_version, gtk_minor_version, gtk_micro_version,
			TARGET_ALIAS);
	}
	if (g_slist_length(compose->newsgroup_list) && !IS_IN_CUSTOM_HEADER("X-Newsreader")) {
		g_string_append_printf(header, "X-Newsreader: %s (GTK+ %d.%d.%d; %s)\n",
			prog_version,
			gtk_major_version, gtk_minor_version, gtk_micro_version,
			TARGET_ALIAS);
	}

	/* custom headers */
	if (compose->account->add_customhdr) {
		GSList *cur;

		for (cur = compose->account->customhdr_list; cur != NULL;
		     cur = cur->next) {
			CustomHeader *chdr = (CustomHeader *)cur->data;

			if (custom_header_is_allowed(chdr->name)) {
				compose_convert_header
					(compose, buf, sizeof(buf),
					 chdr->value ? chdr->value : "",
					 strlen(chdr->name) + 2, FALSE);
				g_string_append_printf(header, "%s: %s\n", chdr->name, buf);
			}
		}
	}

	/* PRIORITY */
	switch (compose->priority) {
		case PRIORITY_HIGHEST: g_string_append_printf(header, "Importance: high\n"
						   "X-Priority: 1 (Highest)\n");
			break;
		case PRIORITY_HIGH: g_string_append_printf(header, "Importance: high\n"
						"X-Priority: 2 (High)\n");
			break;
		case PRIORITY_NORMAL: break;
		case PRIORITY_LOW: g_string_append_printf(header, "Importance: low\n"
					       "X-Priority: 4 (Low)\n");
			break;
		case PRIORITY_LOWEST: g_string_append_printf(header, "Importance: low\n"
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
				compose_convert_header(compose, buf, sizeof(buf), 
						       compose->account->name, 
						       strlen("Disposition-Notification-To: "),
						       TRUE);
				g_string_append_printf(header, "Disposition-Notification-To: %s <%s>\n", buf, compose->account->address);
			} else
				g_string_append_printf(header, "Disposition-Notification-To: %s\n", compose->account->address);
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
		
		entry_str = gtk_entry_get_text(GTK_ENTRY(headerentry->entry));
		Xstrdup_a(headervalue, entry_str, return NULL);
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
			g_string_append_printf(header, "%s %s\n", headername_wcolon, headervalue);
				
		g_free(headername);
		g_free(headername_wcolon);		
	}

	str = header->str;
	g_string_free(header, FALSE);

	return str;
}

#undef IS_IN_CUSTOM_HEADER

static void compose_convert_header(Compose *compose, gchar *dest, gint len, gchar *src,
				   gint header_len, gboolean addr_field)
{
	gchar *tmpstr = NULL;
	const gchar *out_codeset = NULL;

	g_return_if_fail(src != NULL);
	g_return_if_fail(dest != NULL);

	if (len < 1) return;

	tmpstr = g_strdup(src);

	subst_char(tmpstr, '\n', ' ');
	subst_char(tmpstr, '\r', ' ');
	g_strchomp(tmpstr);

	if (!g_utf8_validate(tmpstr, -1, NULL)) {
		gchar *mybuf = g_malloc(strlen(tmpstr)*2 +1);
		conv_localetodisp(mybuf, strlen(tmpstr)*2 +1, tmpstr);
		g_free(tmpstr);
		tmpstr = mybuf;
	}

	codeconv_set_strict(TRUE);
	conv_encode_header_full(dest, len, tmpstr, header_len, addr_field, 
		conv_get_charset_str(compose->out_encoding));
	codeconv_set_strict(FALSE);
	
	if (!dest || *dest == '\0') {
		gchar *test_conv_global_out = NULL;
		gchar *test_conv_reply = NULL;

		/* automatic mode. be automatic. */
		codeconv_set_strict(TRUE);

		out_codeset = conv_get_outgoing_charset_str();
		if (out_codeset) {
			debug_print("trying to convert to %s\n", out_codeset);
			test_conv_global_out = conv_codeset_strdup(src, CS_INTERNAL, out_codeset);
		}

		if (!test_conv_global_out && compose->orig_charset
		&&  strcmp(compose->orig_charset, CS_US_ASCII)) {
			out_codeset = compose->orig_charset;
			debug_print("failure; trying to convert to %s\n", out_codeset);
			test_conv_reply = conv_codeset_strdup(src, CS_INTERNAL, out_codeset);
		}

		if (!test_conv_global_out && !test_conv_reply) {
			/* we're lost */
			out_codeset = CS_INTERNAL;
			debug_print("finally using %s\n", out_codeset);
		}
		g_free(test_conv_global_out);
		g_free(test_conv_reply);
		conv_encode_header_full(dest, len, tmpstr, header_len, addr_field, 
					out_codeset);
		codeconv_set_strict(FALSE);
	}
	g_free(tmpstr);
}

static void compose_create_header_entry(Compose *compose) 
{
	gchar *headers[] = {"To:", "Cc:", "Bcc:", "Newsgroups:", "Reply-To:", "Followup-To:", NULL};

	GtkWidget *combo;
	GtkWidget *entry;
	GList *combo_list = NULL;
	gchar **string;
	const gchar *header = NULL;
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
	g_signal_connect(G_OBJECT(GTK_COMBO(combo)->entry), "grab_focus",
			 G_CALLBACK(compose_grab_focus_cb), compose);
	gtk_widget_show(combo);
	gtk_table_attach(GTK_TABLE(compose->header_table), combo, 0, 1, compose->header_nextrow, compose->header_nextrow+1, GTK_SHRINK, GTK_FILL, 0, 0);
	if (compose->header_last) {	
		const gchar *last_header_entry = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(compose->header_last->combo)->entry));
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

	g_signal_connect_after(G_OBJECT(GTK_COMBO(combo)->entry), "grab_focus",
			 G_CALLBACK(compose_grab_focus_cb), compose);

	/* Entry field */
	entry = gtk_entry_new(); 
	gtk_widget_show(entry);
	gtk_tooltips_set_tip(compose->tooltips, entry,
		_("Use <tab> to autocomplete from addressbook"), NULL);
	gtk_table_attach(GTK_TABLE(compose->header_table), entry, 1, 2, compose->header_nextrow, compose->header_nextrow+1, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);

        g_signal_connect(G_OBJECT(entry), "key-press-event", 
			 G_CALLBACK(compose_headerentry_key_press_event_cb), 
			 headerentry);
    	g_signal_connect(G_OBJECT(entry), "changed", 
			 G_CALLBACK(compose_headerentry_changed_cb), 
			 headerentry);
	g_signal_connect_after(G_OBJECT(entry), "grab_focus",
			 G_CALLBACK(compose_grab_focus_cb), compose);
			 
	/* email dnd */
	gtk_drag_dest_set(entry, GTK_DEST_DEFAULT_ALL, compose_mime_types, 
			  sizeof(compose_mime_types)/sizeof(compose_mime_types[0]),
			  GDK_ACTION_COPY | GDK_ACTION_MOVE);
	g_signal_connect(G_OBJECT(entry), "drag_data_received",
			 G_CALLBACK(compose_header_drag_received_cb),
			 entry);
	g_signal_connect(G_OBJECT(entry), "drag-drop",
			 G_CALLBACK(compose_drag_drop),
			 compose);
	
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
	GtkWidget *from_optmenu_hbox;
	GtkWidget *header_scrolledwin;
	GtkWidget *header_table;

	gint count = 0;

	/* header labels and entries */
	header_scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_show(header_scrolledwin);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(header_scrolledwin), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	header_table = gtk_table_new(2, 2, FALSE);
	gtk_widget_show(header_table);
	gtk_container_set_border_width(GTK_CONTAINER(header_table), BORDER_WIDTH);
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(header_scrolledwin), header_table);
	gtk_viewport_set_shadow_type(GTK_VIEWPORT(GTK_BIN(header_scrolledwin)->child), GTK_SHADOW_ETCHED_IN);
	count = 0;

	/* option menu for selecting accounts */
	from_optmenu_hbox = compose_account_option_menu_create(compose);
	gtk_table_attach(GTK_TABLE(header_table), from_optmenu_hbox,
				  0, 2, count, count + 1, GTK_EXPAND | GTK_FILL, GTK_SHRINK, 0, 0);
	count++;

	compose->header_table = header_table;
	compose->header_list = NULL;
	compose->header_nextrow = count;

	compose_create_header_entry(compose);

	compose->table	          = NULL;

	return header_scrolledwin ;
}

GtkWidget *compose_create_attach(Compose *compose)
{
	GtkWidget *attach_scrwin;
	GtkWidget *attach_clist;

	GtkListStore *store;
	GtkCellRenderer *renderer;
	GtkTreeViewColumn *column;
	GtkTreeSelection *selection;

	/* attachment list */
	attach_scrwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(attach_scrwin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);
	gtk_widget_set_size_request(attach_scrwin, -1, 80);

	store = gtk_list_store_new(N_ATTACH_COLS, 
				   G_TYPE_STRING,
				   G_TYPE_STRING,
				   G_TYPE_STRING,
				   G_TYPE_POINTER,
				   G_TYPE_AUTO_POINTER,
				   -1);
	attach_clist = GTK_WIDGET(gtk_tree_view_new_with_model
					(GTK_TREE_MODEL(store)));
	gtk_container_add(GTK_CONTAINER(attach_scrwin), attach_clist);
	g_object_unref(store);
	
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes
			(_("Mime type"), renderer, "text", 
			 COL_MIMETYPE, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(attach_clist), column);			 
	
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes
			(_("Size"), renderer, "text", 
			 COL_SIZE, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(attach_clist), column);			 
	
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes
			(_("Name"), renderer, "text", 
			 COL_NAME, NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(attach_clist), column);

	gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(attach_clist),
				     prefs_common.enable_rules_hint);
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(attach_clist));
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_MULTIPLE);

	g_signal_connect(G_OBJECT(attach_clist), "row_activated",
			 G_CALLBACK(attach_selected), compose);
	g_signal_connect(G_OBJECT(attach_clist), "button_press_event",
			 G_CALLBACK(attach_button_pressed), compose);
	g_signal_connect(G_OBJECT(attach_clist), "key_press_event",
			 G_CALLBACK(attach_key_pressed), compose);

	/* drag and drop */
	gtk_drag_dest_set(attach_clist,
			  GTK_DEST_DEFAULT_ALL, compose_mime_types, 
			  sizeof(compose_mime_types)/sizeof(compose_mime_types[0]),
			  GDK_ACTION_COPY | GDK_ACTION_MOVE);
	g_signal_connect(G_OBJECT(attach_clist), "drag_data_received",
			 G_CALLBACK(compose_attach_drag_received_cb),
			 compose);
	g_signal_connect(G_OBJECT(attach_clist), "drag-drop",
			 G_CALLBACK(compose_drag_drop),
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
	gtk_container_set_border_width(GTK_CONTAINER(table), BORDER_WIDTH);
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
	g_signal_connect(G_OBJECT(savemsg_checkbtn), "toggled",
			 G_CALLBACK(compose_savemsg_checkbtn_cb), compose);

	savemsg_entry = gtk_entry_new();
	gtk_widget_show(savemsg_entry);
	gtk_table_attach_defaults(GTK_TABLE(table), savemsg_entry, 1, 2, rowcount, rowcount + 1);
	gtk_editable_set_editable(GTK_EDITABLE(savemsg_entry), prefs_common.savemsg);
	g_signal_connect_after(G_OBJECT(savemsg_entry), "grab_focus",
			 G_CALLBACK(compose_grab_focus_cb), compose);
	if (account_get_special_folder(compose->account, F_OUTBOX)) {
		folderidentifier = folder_item_get_identifier(account_get_special_folder
				  (compose->account, F_OUTBOX));
		gtk_entry_set_text(GTK_ENTRY(savemsg_entry), folderidentifier);
		g_free(folderidentifier);
	}

	savemsg_select = gtkut_get_browse_file_btn(_("_Browse"));
	gtk_widget_show(savemsg_select);
	gtk_table_attach(GTK_TABLE(table), savemsg_select, 2, 3, rowcount, rowcount + 1, GTK_SHRINK | GTK_FILL, GTK_SHRINK, 0, 0);
	g_signal_connect(G_OBJECT(savemsg_select), "clicked",
			 G_CALLBACK(compose_savemsg_select_cb),
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

static void entry_paste_clipboard(Compose *compose, GtkWidget *entry, gboolean wrap,
				  GdkAtom clip, GtkTextIter *insert_place);

#define BLOCK_WRAP() {							\
	prev_autowrap = compose->autowrap;				\
	buffer = gtk_text_view_get_buffer(				\
					GTK_TEXT_VIEW(compose->text));	\
	compose->autowrap = FALSE;					\
									\
	g_signal_handlers_block_by_func(G_OBJECT(buffer),		\
				G_CALLBACK(compose_changed_cb),		\
				compose);				\
	g_signal_handlers_block_by_func(G_OBJECT(buffer),		\
				G_CALLBACK(text_inserted),		\
				compose);				\
}
#define UNBLOCK_WRAP() {						\
	compose->autowrap = prev_autowrap;				\
	if (compose->autowrap)						\
		compose_wrap_all(compose);				\
									\
	g_signal_handlers_unblock_by_func(G_OBJECT(buffer),		\
				G_CALLBACK(compose_changed_cb),		\
				compose);				\
	g_signal_handlers_unblock_by_func(G_OBJECT(buffer),		\
				G_CALLBACK(text_inserted),		\
				compose);				\
}


static gboolean text_clicked(GtkWidget *text, GdkEventButton *event,
                                       Compose *compose)
{
	gint prev_autowrap;
	GtkTextBuffer *buffer;
#if USE_ASPELL
	if (event->button == 3) {
		GtkTextIter iter;
		GtkTextIter sel_start, sel_end;
		gboolean stuff_selected;
		gint x, y;
		/* move the cursor to allow GtkAspell to check the word
		 * under the mouse */
		gtk_text_view_window_to_buffer_coords(GTK_TEXT_VIEW(text),
			GTK_TEXT_WINDOW_TEXT, event->x, event->y,
			&x, &y);
		gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW(text),
			&iter, x, y);
		/* get selection */
		stuff_selected = gtk_text_buffer_get_selection_bounds(
				GTK_TEXT_VIEW(text)->buffer,
				&sel_start, &sel_end);

		gtk_text_buffer_place_cursor (GTK_TEXT_VIEW(text)->buffer, &iter);
		/* reselect stuff */
		if (stuff_selected 
		&& gtk_text_iter_in_range(&iter, &sel_start, &sel_end)) {
			gtk_text_buffer_select_range(GTK_TEXT_VIEW(text)->buffer,
				&sel_start, &sel_end);
		}
		return FALSE; /* pass the event so that the right-click goes through */
	}
#endif
	if (event->button == 2) {
		GtkTextIter iter;
		gint x, y;
		BLOCK_WRAP();
		
		/* get the middle-click position to paste at the correct place */
		gtk_text_view_window_to_buffer_coords(GTK_TEXT_VIEW(text),
			GTK_TEXT_WINDOW_TEXT, event->x, event->y,
			&x, &y);
		gtk_text_view_get_iter_at_location (GTK_TEXT_VIEW(text),
			&iter, x, y);
		
		entry_paste_clipboard(compose, text, 
				prefs_common.linewrap_pastes,
				GDK_SELECTION_PRIMARY, &iter);
		UNBLOCK_WRAP();
		return TRUE;
	}
	return FALSE;
}

#if USE_ASPELL
static void compose_spell_menu_changed(void *data)
{
	Compose *compose = (Compose *)data;
	GSList *items;
	GtkWidget *menuitem;
	GtkWidget *parent_item;
	GtkMenu *menu = GTK_MENU(gtk_menu_new());
	GtkItemFactory *ifactory = gtk_item_factory_from_widget(compose->menubar);
	GSList *spell_menu;

	if (compose->gtkaspell == NULL)
		return;

	parent_item = gtk_item_factory_get_item(ifactory, 
			"/Spelling/Options");

	/* setting the submenu removes /Spelling/Options from the factory 
	 * so we need to save it */

	if (parent_item == NULL) {
		parent_item = compose->aspell_options_menu;
		gtk_menu_item_remove_submenu(GTK_MENU_ITEM(parent_item));
	} else
		compose->aspell_options_menu = parent_item;

	spell_menu = gtkaspell_make_config_menu(compose->gtkaspell);

	spell_menu = g_slist_reverse(spell_menu);
	for (items = spell_menu;
	     items; items = items->next) {
		menuitem = GTK_WIDGET(GTK_MENU_ITEM(items->data));
		gtk_menu_shell_prepend(GTK_MENU_SHELL(menu), GTK_WIDGET(menuitem));
		gtk_widget_show(GTK_WIDGET(menuitem));
	}
	g_slist_free(spell_menu);

	gtk_menu_item_set_submenu(GTK_MENU_ITEM(parent_item), GTK_WIDGET(menu));
	
}
#endif

static gboolean compose_force_window_origin = TRUE;
static Compose *compose_create(PrefsAccount *account, ComposeMode mode,
						 gboolean batch)
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
	GtkTextBuffer *buffer;
	GtkClipboard *clipboard;

	UndoMain *undostruct;

	gchar *titles[N_ATTACH_COLS];
	guint n_menu_entries;
	GtkWidget *popupmenu;
	GtkItemFactory *popupfactory;
	GtkItemFactory *ifactory;
	GtkWidget *tmpl_menu;
	gint n_entries;
	GtkWidget *menuitem;

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

	compose->batch = batch;
	compose->account = account;
	
	compose->mutex = g_mutex_new();
	compose->set_cursor_pos = -1;

	compose->tooltips = gtk_tooltips_new();

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_resizable(GTK_WINDOW(window), TRUE);
	gtk_widget_set_size_request(window, -1, prefs_common.compose_height);

	if (!geometry.max_width) {
		geometry.max_width = gdk_screen_width();
		geometry.max_height = gdk_screen_height();
	}

	gtk_window_set_geometry_hints(GTK_WINDOW(window), NULL,
				      &geometry, GDK_HINT_MAX_SIZE);
	if (!geometry.min_width) {
		geometry.min_width = 600;
		geometry.min_height = 480;
	}
	gtk_window_set_geometry_hints(GTK_WINDOW(window), NULL,
				      &geometry, GDK_HINT_MIN_SIZE);
	
	if (compose_force_window_origin)
		gtk_widget_set_uposition(window, prefs_common.compose_x, 
				 prefs_common.compose_y);

	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(compose_delete_cb), compose);
	MANAGE_WINDOW_SIGNALS_CONNECT(window);
	gtk_widget_realize(window);

	gtkut_widget_set_composer_icon(window);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	n_menu_entries = sizeof(compose_entries) / sizeof(compose_entries[0]);
	menubar = menubar_create(window, compose_entries,
				 n_menu_entries, "<Compose>", compose);
	gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, TRUE, 0);

	if (prefs_common.toolbar_detachable) {
		handlebox = gtk_handle_box_new();
	} else {
		handlebox = gtk_hbox_new(FALSE, 0);
	}
	gtk_box_pack_start(GTK_BOX(vbox), handlebox, FALSE, FALSE, 0);

	gtk_widget_realize(handlebox);
	compose->toolbar = toolbar_create(TOOLBAR_COMPOSE, handlebox,
					  (gpointer)compose);

	vbox2 = gtk_vbox_new(FALSE, 2);
	gtk_box_pack_start(GTK_BOX(vbox), vbox2, TRUE, TRUE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(vbox2), 0);
	
	/* Notebook */
	notebook = gtk_notebook_new();
	gtk_widget_set_size_request(notebook, -1, 130);
	gtk_widget_show(notebook);

	/* header labels and entries */
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
			compose_create_header(compose),
			gtk_label_new_with_mnemonic(_("Hea_der")));
	/* attachment list */
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
			compose_create_attach(compose),
			gtk_label_new_with_mnemonic(_("_Attachments")));
	/* Others Tab */
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
			compose_create_others(compose),
			gtk_label_new_with_mnemonic(_("Othe_rs")));

	/* Subject */
	subject_hbox = gtk_hbox_new(FALSE, 0);
	gtk_widget_show(subject_hbox);

	subject_frame = gtk_frame_new(NULL);
	gtk_frame_set_shadow_type(GTK_FRAME(subject_frame), GTK_SHADOW_NONE);
	gtk_box_pack_start(GTK_BOX(subject_hbox), subject_frame, TRUE, TRUE, 0);
	gtk_widget_show(subject_frame);

	subject = gtk_hbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(subject), 0);
	gtk_widget_show(subject);

	label = gtk_label_new(_("Subject:"));
	gtk_box_pack_start(GTK_BOX(subject), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	subject_entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(subject), subject_entry, TRUE, TRUE, 0);
	g_signal_connect_after(G_OBJECT(subject_entry), "grab_focus",
			 G_CALLBACK(compose_grab_focus_cb), compose);
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
			   BORDER_WIDTH);

	/* text widget */
	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledwin),
					    GTK_SHADOW_IN);
	gtk_box_pack_start(GTK_BOX(edit_vbox), scrolledwin, TRUE, TRUE, 0);
	gtk_widget_set_size_request(scrolledwin, prefs_common.compose_width, -1);

	text = gtk_text_view_new();
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text));
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text), GTK_WRAP_WORD_CHAR);
	gtk_text_view_set_editable(GTK_TEXT_VIEW(text), TRUE);
	clipboard = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
	gtk_text_buffer_add_selection_clipboard(buffer, clipboard);
	
	gtk_container_add(GTK_CONTAINER(scrolledwin), text);

	g_signal_connect_after(G_OBJECT(text), "size_allocate",
			       G_CALLBACK(compose_edit_size_alloc),
			       ruler);
	g_signal_connect(G_OBJECT(buffer), "changed",
			 G_CALLBACK(compose_changed_cb), compose);
	g_signal_connect(G_OBJECT(text), "grab_focus",
			 G_CALLBACK(compose_grab_focus_cb), compose);
	g_signal_connect(G_OBJECT(buffer), "insert_text",
			 G_CALLBACK(text_inserted), compose);
	g_signal_connect(G_OBJECT(text), "button_press_event",
			 G_CALLBACK(text_clicked), compose);
	g_signal_connect(G_OBJECT(subject_entry), "changed",
			 G_CALLBACK(compose_changed_cb), compose);

	/* drag and drop */
	gtk_drag_dest_set(text, GTK_DEST_DEFAULT_ALL, compose_mime_types, 
			  sizeof(compose_mime_types)/sizeof(compose_mime_types[0]),
			  GDK_ACTION_COPY | GDK_ACTION_MOVE);
	g_signal_connect(G_OBJECT(text), "drag_data_received",
			 G_CALLBACK(compose_insert_drag_received_cb),
			 compose);
	g_signal_connect(G_OBJECT(text), "drag-drop",
			 G_CALLBACK(compose_drag_drop),
			 compose);
	gtk_widget_show_all(vbox);

	/* pane between attach clist and text */
	paned = gtk_vpaned_new();
	gtk_paned_set_gutter_size(GTK_PANED(paned), 12);
	gtk_container_add(GTK_CONTAINER(vbox2), paned);
	gtk_paned_add1(GTK_PANED(paned), notebook);
	gtk_paned_add2(GTK_PANED(paned), edit_vbox);
	gtk_widget_show_all(paned);


	if (prefs_common.textfont) {
		PangoFontDescription *font_desc;

		font_desc = pango_font_description_from_string
			(prefs_common.textfont);
		if (font_desc) {
			gtk_widget_modify_font(text, font_desc);
			pango_font_description_free(font_desc);
		}
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
	compose->exteditor_tag     = -1;
	compose->draft_timeout_tag = -1;

#if USE_ASPELL
	menu_set_sensitive(ifactory, "/Spelling", FALSE);
	if (mode != COMPOSE_REDIRECT) {
        	if (prefs_common.enable_aspell && prefs_common.dictionary &&
	    	    strcmp(prefs_common.dictionary, "")) {
			gtkaspell = gtkaspell_new(prefs_common.aspell_path,
						  prefs_common.dictionary,
						  conv_get_locale_charset_str(),
						  prefs_common.misspelled_col,
						  prefs_common.check_while_typing,
						  prefs_common.recheck_when_changing_dict,
						  prefs_common.use_alternate,
						  GTK_TEXT_VIEW(text),
						  GTK_WINDOW(compose->window),
						  compose_spell_menu_changed,
						  compose);
			if (!gtkaspell) {
				alertpanel_error(_("Spell checker could not "
						"be started.\n%s"),
						gtkaspell_checkers_strerror());
				gtkaspell_checkers_reset_error();
			} else {
				if (!gtkaspell_set_sug_mode(gtkaspell,
						prefs_common.aspell_sugmode)) {
					debug_print("Aspell: could not set "
						    "suggestion mode %s\n",
						    gtkaspell_checkers_strerror());
					gtkaspell_checkers_reset_error();
				}

				menu_set_sensitive(ifactory, "/Spelling", TRUE);
			}
        	}
	}
        compose->gtkaspell = gtkaspell;
	compose_spell_menu_changed(compose);
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
		menuitem = gtk_item_factory_get_item(ifactory, "/Tools/Template");
		menu_set_sensitive(ifactory, "/Tools/Template", FALSE);
	}

	compose_list = g_list_append(compose_list, compose);

	if (!prefs_common.show_ruler)
		gtk_widget_hide(ruler_hbox);
		
	menuitem = gtk_item_factory_get_item(ifactory, "/Tools/Show ruler");
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem),
				       prefs_common.show_ruler);

	/* Priority */
	compose->priority = PRIORITY_NORMAL;
	compose_update_priority_menu_item(compose);

	compose_set_out_encoding(compose);
	
	/* Actions menu */
	compose_update_actions_menu(compose);

	/* Privacy Systems menu */
	compose_update_privacy_systems_menu(compose);

	activate_privacy_system(compose, account, TRUE);
	toolbar_set_style(compose->toolbar->toolbar, compose->handlebox, prefs_common.toolbar_style);
	if (batch) {
		gtk_widget_realize(window);
	} else {
		gtk_widget_show(window);
	}
	
	return compose;
}

static GtkWidget *compose_account_option_menu_create(Compose *compose)
{
	GList *accounts;
	GtkWidget *hbox;
	GtkWidget *optmenu;
	GtkWidget *optmenubox;
	GtkListStore *menu;
	GtkTreeIter iter;
	GtkWidget *from_name = NULL;

	gint num = 0, def_menu = 0;
	
	accounts = account_get_list();
	g_return_val_if_fail(accounts != NULL, NULL);

	optmenubox = gtk_event_box_new();
	optmenu = gtkut_sc_combobox_create(optmenubox, FALSE);
	menu = GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(optmenu)));

	hbox = gtk_hbox_new(FALSE, 6);
	from_name = gtk_entry_new();
	
	g_signal_connect_after(G_OBJECT(from_name), "grab_focus",
			 G_CALLBACK(compose_grab_focus_cb), compose);

	for (; accounts != NULL; accounts = accounts->next, num++) {
		PrefsAccount *ac = (PrefsAccount *)accounts->data;
		gchar *name, *from = NULL;

		if (ac == compose->account) def_menu = num;

		name = g_markup_printf_escaped(_("From: <i>%s</i>"),
				       ac->account_name);
		
		if (ac == compose->account) {
			if (ac->name && *ac->name) {
				gchar *buf;
				QUOTE_IF_REQUIRED_NORMAL(buf, ac->name, return NULL);
				from = g_strdup_printf("%s <%s>",
						       buf, ac->address);
				gtk_entry_set_text(GTK_ENTRY(from_name), from);
			} else {
				from = g_strdup_printf("%s",
						       ac->address);
				gtk_entry_set_text(GTK_ENTRY(from_name), from);
			}
		}
		COMBOBOX_ADD(menu, name, ac->account_id);
		g_free(name);
		g_free(from);
	}

	gtk_combo_box_set_active(GTK_COMBO_BOX(optmenu), def_menu);

	g_signal_connect(G_OBJECT(optmenu), "changed",
			G_CALLBACK(account_activated),
			compose);

	gtk_box_pack_start(GTK_BOX(hbox), optmenubox, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), from_name, TRUE, TRUE, 0);
	
	gtk_tooltips_set_tip(compose->tooltips, optmenubox,
		_("Account to use for this email"), NULL);
	gtk_tooltips_set_tip(compose->tooltips, from_name,
		_("Sender address to be used"), NULL);

	compose->from_name = from_name;
	
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

	systemid = g_object_get_data(G_OBJECT(widget), "privacy_system");
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

static void compose_update_privacy_system_menu_item(Compose * compose, gboolean warn)
{
	static gchar *branch_path = "/Options/Privacy System";
	GtkItemFactory *ifactory;
	GtkWidget *menuitem = NULL;
	GList *amenu;
	gboolean can_sign = FALSE, can_encrypt = FALSE;
	gboolean found = FALSE;

	ifactory = gtk_item_factory_from_widget(compose->menubar);

	if (compose->privacy_system != NULL) {
		gchar *systemid;

		menuitem = gtk_item_factory_get_widget(ifactory, branch_path);
		g_return_if_fail(menuitem != NULL);

		amenu = GTK_MENU_SHELL(menuitem)->children;
		menuitem = NULL;
		while (amenu != NULL) {
		        GList *alist = amenu->next;

			systemid = g_object_get_data(G_OBJECT(amenu->data), "privacy_system");
			if (systemid != NULL) {
				if (strcmp(systemid, compose->privacy_system) == 0) {
					menuitem = GTK_WIDGET(amenu->data);

					can_sign = privacy_system_can_sign(systemid);
					can_encrypt = privacy_system_can_encrypt(systemid);
					found = TRUE;
					break;
				} 
			} else if (strlen(compose->privacy_system) == 0) {
					menuitem = GTK_WIDGET(amenu->data);

					can_sign = FALSE;
					can_encrypt = FALSE;
					found = TRUE;
					break;
			}

			amenu = alist;
		}
		if (menuitem != NULL)
			gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem), TRUE);
		
		if (warn && !found && strlen(compose->privacy_system)) {
			gchar *tmp = g_strdup_printf(
				_("The privacy system '%s' cannot be loaded. You "
				  "will not be able to sign or encrypt this message."),
				  compose->privacy_system);
			alertpanel_warning(tmp);
			g_free(tmp);
		}
	} 

	menu_set_sensitive(ifactory, "/Options/Sign", can_sign);
	menu_set_sensitive(ifactory, "/Options/Encrypt", can_encrypt);
}	
 
static void compose_set_out_encoding(Compose *compose)
{
	GtkItemFactoryEntry *entry;
	GtkItemFactory *ifactory;
	CharSet out_encoding;
	gchar *path, *p, *q;
	GtkWidget *item;

	out_encoding = conv_get_charset_from_str(prefs_common.outgoing_charset);
	ifactory = gtk_item_factory_from_widget(compose->menubar);

	for (entry = compose_entries; entry->callback != compose_address_cb;
	     entry++) {
		if (entry->callback == compose_set_encoding_cb &&
		    (CharSet)entry->callback_action == out_encoding) {
			p = q = path = g_strdup(entry->path);
			while (*p) {
				if (*p == '_') {
					if (p[1] == '_') {
						p++;
						*q++ = '_';
					}
				} else
					*q++ = *p;
				p++;
			}
			*q = '\0';
			item = gtk_item_factory_get_item(ifactory, path);
			gtk_widget_activate(item);
			g_free(path);
			break;
		}
	}
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
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
		g_signal_connect(G_OBJECT(item), "activate",
				 G_CALLBACK(compose_template_activate_cb),
				 compose);
		g_object_set_data(G_OBJECT(item), "template", tmpl);
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

	g_signal_connect(G_OBJECT(system_none), "activate",
		G_CALLBACK(compose_set_privacy_system_cb), compose);

	systems = privacy_get_system_ids();
	for (cur = systems; cur != NULL; cur = g_slist_next(cur)) {
		gchar *systemid = cur->data;

		group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(system_none));
		widget = gtk_radio_menu_item_new_with_label(group,
			privacy_system_get_name(systemid));
		g_object_set_data_full(G_OBJECT(widget), "privacy_system",
				       g_strdup(systemid), g_free);
		g_signal_connect(G_OBJECT(widget), "activate",
			G_CALLBACK(compose_set_privacy_system_cb), compose);

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
	GtkTextView *text;
	GtkTextBuffer *buffer;
	GtkTextMark *mark;
	GtkTextIter iter;
	gchar *qmark;
	gchar *parsed_str = NULL;
	gint cursor_pos = 0;
	if (!tmpl) return;

	/* process the body */

	text = GTK_TEXT_VIEW(compose->text);
	buffer = gtk_text_view_get_buffer(text);

	if (replace)
		gtk_text_buffer_set_text(buffer, "", -1);

	mark = gtk_text_buffer_get_insert(buffer);
	gtk_text_buffer_get_iter_at_mark(buffer, &iter, mark);

	if (tmpl->value) {
		if ((compose->replyinfo == NULL) && (compose->fwdinfo == NULL)) {
			parsed_str = compose_quote_fmt(compose, NULL, tmpl->value,
						       NULL, NULL, FALSE);
		} else {
			if (prefs_common.quotemark && *prefs_common.quotemark)
				qmark = prefs_common.quotemark;
			else
				qmark = "> ";

			if (compose->replyinfo != NULL)
				parsed_str = compose_quote_fmt(compose, compose->replyinfo,
							       tmpl->value, qmark, NULL, FALSE);
			else if (compose->fwdinfo != NULL)
				parsed_str = compose_quote_fmt(compose, compose->fwdinfo,
							       tmpl->value, qmark, NULL, FALSE);
			else
				parsed_str = NULL;
		}
	}
	if (replace && parsed_str && compose->account->auto_sig)
		compose_insert_sig(compose, FALSE);

	if (replace && parsed_str) {
		gtk_text_buffer_get_start_iter(buffer, &iter);
		gtk_text_buffer_place_cursor(buffer, &iter);
	}
	
	if (parsed_str) {
		cursor_pos = quote_fmt_get_cursor_pos();
		compose->set_cursor_pos = cursor_pos;
		if (cursor_pos == -1)
			cursor_pos = 0;
		gtk_text_buffer_get_start_iter(buffer, &iter);
		gtk_text_buffer_get_iter_at_offset(buffer, &iter, cursor_pos);
		gtk_text_buffer_place_cursor(buffer, &iter);
	}

	/* process the other fields */
	compose_template_apply_fields(compose, tmpl);
	quote_fmt_reset_vartable();
	compose_changed_cb(NULL, compose);
}

void compose_template_apply_fields(Compose *compose, Template *tmpl)
{
	static MsgInfo dummyinfo;
	MsgInfo *msginfo = NULL;
	gchar *buf = NULL;

	if (compose->replyinfo != NULL)
		msginfo = compose->replyinfo;
	else if (compose->fwdinfo != NULL)
		msginfo = compose->fwdinfo;
	else
		msginfo = &dummyinfo;

	if (tmpl->to && *tmpl->to != '\0') {
		quote_fmt_init(msginfo, NULL, NULL, FALSE);
		quote_fmt_scan_string(tmpl->to);
		quote_fmt_parse();

		buf = quote_fmt_get_buffer();
		if (buf == NULL) {
			alertpanel_error(_("Message To format error."));
		} else {
			compose_entry_append(compose, buf, COMPOSE_TO);
		}
	}

	if (tmpl->cc && *tmpl->cc != '\0') {
		quote_fmt_init(msginfo, NULL, NULL, FALSE);
		quote_fmt_scan_string(tmpl->cc);
		quote_fmt_parse();

		buf = quote_fmt_get_buffer();
		if (buf == NULL) {
			alertpanel_error(_("Message Cc format error."));
		} else {
			compose_entry_append(compose, buf, COMPOSE_CC);
		}
	}

	if (tmpl->bcc && *tmpl->bcc != '\0') {
		quote_fmt_init(msginfo, NULL, NULL, FALSE);
		quote_fmt_scan_string(tmpl->bcc);
		quote_fmt_parse();

		buf = quote_fmt_get_buffer();
		if (buf == NULL) {
			alertpanel_error(_("Message Bcc format error."));
		} else {
			compose_entry_append(compose, buf, COMPOSE_BCC);
		}
	}

	/* process the subject */
	if (tmpl->subject && *tmpl->subject != '\0') {
		quote_fmt_init(msginfo, NULL, NULL, FALSE);
		quote_fmt_scan_string(tmpl->subject);
		quote_fmt_parse();

		buf = quote_fmt_get_buffer();
		if (buf == NULL) {
			alertpanel_error(_("Message subject format error."));
		} else {
			gtk_entry_set_text(GTK_ENTRY(compose->subject_entry), buf);
		}
	}
}

static void compose_destroy(Compose *compose)
{
	compose_list = g_list_remove(compose_list, compose);

	if (compose->updating) {
		debug_print("danger, not destroying anything now\n");
		compose->deferred_destroy = TRUE;
		return;
	}
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

	g_free(compose->redirect_filename);
	if (compose->undostruct)
		undo_destroy(compose->undostruct);

	g_free(compose->sig_str);

	g_free(compose->exteditor_file);

	g_free(compose->orig_charset);

	g_free(compose->privacy_system);

	if (addressbook_get_target_compose() == compose)
		addressbook_set_target_compose(NULL);

#if USE_ASPELL
        if (compose->gtkaspell) {
	        gtkaspell_delete(compose->gtkaspell);
		compose->gtkaspell = NULL;
        }
#endif

	prefs_common.compose_width = compose->scrolledwin->allocation.width;
	prefs_common.compose_height = compose->window->allocation.height;

	if (!gtk_widget_get_parent(compose->paned))
		gtk_widget_destroy(compose->paned);
	gtk_widget_destroy(compose->popupmenu);

	gtk_widget_destroy(compose->window);
	toolbar_destroy(compose->toolbar);
	g_free(compose->toolbar);
	g_mutex_free(compose->mutex);
	g_free(compose);
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
	GtkTreeView *tree_view = GTK_TREE_VIEW(compose->attach_clist);
	GtkTreeSelection *selection;
	GList *sel, *cur;
	GtkTreeModel *model;

	selection = gtk_tree_view_get_selection(tree_view);
	sel = gtk_tree_selection_get_selected_rows(selection, &model);

	if (!sel) 
		return;

	for (cur = sel; cur != NULL; cur = cur->next) {
		GtkTreePath *path = cur->data;
		GtkTreeRowReference *ref = gtk_tree_row_reference_new
						(model, cur->data);
		cur->data = ref;
		gtk_tree_path_free(path);
	}

	for (cur = sel; cur != NULL; cur = cur->next) {
		GtkTreeRowReference *ref = cur->data;
		GtkTreePath *path = gtk_tree_row_reference_get_path(ref);
		GtkTreeIter iter;

		if (gtk_tree_model_get_iter(model, &iter, path))
			gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
		
		gtk_tree_path_free(path);
		gtk_tree_row_reference_free(ref);
	}

	g_list_free(sel);
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

static void gtk_tree_path_free_(gpointer ptr, gpointer data)
{	
	gtk_tree_path_free((GtkTreePath *)ptr);
}

static void compose_attach_property(Compose *compose)
{
	GtkTreeView *tree_view = GTK_TREE_VIEW(compose->attach_clist);
	AttachInfo *ainfo;
	GtkComboBox *optmenu;
	GtkTreeSelection *selection;
	GList *sel;
	GtkTreeModel *model;
	GtkTreeIter iter;
	GtkTreePath *path;
	static gboolean cancelled;

	/* only if one selected */
	selection = gtk_tree_view_get_selection(tree_view);
	if (gtk_tree_selection_count_selected_rows(selection) != 1) 
		return;

	sel = gtk_tree_selection_get_selected_rows(selection, &model);
	if (!sel)
		return;

	path = (GtkTreePath *) sel->data;
	gtk_tree_model_get_iter(model, &iter, path);
	gtk_tree_model_get(model, &iter, COL_DATA, &ainfo, -1); 
	
	if (!ainfo) {
		g_list_foreach(sel, gtk_tree_path_free_, NULL);
		g_list_free(sel);
		return;
	}		
	g_list_free(sel);

	if (!attach_prop.window)
		compose_attach_property_create(&cancelled);
	gtk_widget_grab_focus(attach_prop.ok_btn);
	gtk_widget_show(attach_prop.window);
	manage_window_set_transient(GTK_WINDOW(attach_prop.window));

	optmenu = GTK_COMBO_BOX(attach_prop.encoding_optmenu);
	if (ainfo->encoding == ENC_UNKNOWN)
		combobox_select_by_data(optmenu, ENC_BASE64);
	else
		combobox_select_by_data(optmenu, ainfo->encoding);

	gtk_entry_set_text(GTK_ENTRY(attach_prop.mimetype_entry),
			   ainfo->content_type ? ainfo->content_type : "");
	gtk_entry_set_text(GTK_ENTRY(attach_prop.path_entry),
			   ainfo->file ? ainfo->file : "");
	gtk_entry_set_text(GTK_ENTRY(attach_prop.filename_entry),
			   ainfo->name ? ainfo->name : "");

	for (;;) {
		const gchar *entry_text;
		gchar *text;
		gchar *cnttype = NULL;
		gchar *file = NULL;
		off_t size = 0;

		cancelled = FALSE;
		gtk_main();

		gtk_widget_hide(attach_prop.window);
		
		if (cancelled) 
			break;

		entry_text = gtk_entry_get_text(GTK_ENTRY(attach_prop.mimetype_entry));
		if (*entry_text != '\0') {
			gchar *p;

			text = g_strstrip(g_strdup(entry_text));
			if ((p = strchr(text, '/')) && !strchr(p + 1, '/')) {
				cnttype = g_strdup(text);
				g_free(text);
			} else {
				alertpanel_error(_("Invalid MIME type."));
				g_free(text);
				continue;
			}
		}

		ainfo->encoding = combobox_get_active_data(optmenu);

		entry_text = gtk_entry_get_text(GTK_ENTRY(attach_prop.path_entry));
		if (*entry_text != '\0') {
			if (is_file_exist(entry_text) &&
			    (size = get_file_size(entry_text)) > 0)
				file = g_strdup(entry_text);
			else {
				alertpanel_error
					(_("File doesn't exist or is empty."));
				g_free(cnttype);
				continue;
			}
		}

		entry_text = gtk_entry_get_text(GTK_ENTRY(attach_prop.filename_entry));
		if (*entry_text != '\0') {
			g_free(ainfo->name);
			ainfo->name = g_strdup(entry_text);
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

		/* update tree store */
		text = to_human_readable(ainfo->size);
		gtk_tree_model_get_iter(model, &iter, path);
		gtk_list_store_set(GTK_LIST_STORE(model), &iter,
				   COL_MIMETYPE, ainfo->content_type,
				   COL_SIZE, text,
				   COL_NAME, ainfo->name,
				   -1);
		
		break;
	}

	gtk_tree_path_free(path);
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
	GtkListStore *optmenu_menu;
	GtkWidget *path_entry;
	GtkWidget *filename_entry;
	GtkWidget *hbbox;
	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;
	GList     *mime_type_list, *strlist;
	GtkTreeIter iter;

	debug_print("Creating attach_property window...\n");

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_widget_set_size_request(window, 480, -1);
	gtk_container_set_border_width(GTK_CONTAINER(window), 8);
	gtk_window_set_title(GTK_WINDOW(window), _("Properties"));
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(window), TRUE);
	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(attach_property_delete_event),
			 cancelled);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(attach_property_key_pressed),
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
		gchar *tmp;

		tmp = g_strdup_printf("%s/%s", type->type, type->sub_type);

		if (g_list_find_custom(strlist, tmp, (GCompareFunc)strcmp2))
			g_free(tmp);
		else
			strlist = g_list_insert_sorted(strlist, (gpointer)tmp,
					(GCompareFunc)strcmp2);
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

	optmenu = gtkut_sc_combobox_create(NULL, TRUE);
	optmenu_menu = GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(optmenu)));

	COMBOBOX_ADD(optmenu_menu, "7bit", ENC_7BIT);
	COMBOBOX_ADD(optmenu_menu, "8bit", ENC_8BIT);
	COMBOBOX_ADD(optmenu_menu, "quoted-printable",	ENC_QUOTED_PRINTABLE);
	COMBOBOX_ADD(optmenu_menu, "base64", ENC_BASE64);
	gtk_combo_box_set_active(GTK_COMBO_BOX(optmenu), 0);

	gtk_box_pack_start(GTK_BOX(hbox), optmenu, TRUE, TRUE, 0);

	SET_LABEL_AND_ENTRY(_("Path"),      path_entry,     2);
	SET_LABEL_AND_ENTRY(_("File name"), filename_entry, 3);

	gtkut_stock_button_set_create(&hbbox, &cancel_btn, GTK_STOCK_CANCEL,
				      &ok_btn, GTK_STOCK_OK,
				      NULL, NULL);
	gtk_box_pack_end(GTK_BOX(vbox), hbbox, FALSE, FALSE, 0);
	gtk_widget_grab_default(ok_btn);

	g_signal_connect(G_OBJECT(ok_btn), "clicked",
			 G_CALLBACK(attach_property_ok),
			 cancelled);
	g_signal_connect(G_OBJECT(cancel_btn), "clicked",
			 G_CALLBACK(attach_property_cancel),
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

static gboolean attach_property_key_pressed(GtkWidget *widget,
					    GdkEventKey *event,
					    gboolean *cancelled)
{
	if (event && event->keyval == GDK_Escape) {
		*cancelled = TRUE;
		gtk_main_quit();
	}
	return FALSE;
}

static void compose_exec_ext_editor(Compose *compose)
{
#ifdef G_OS_UNIX
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

		compose_set_ext_editor_sensitive(compose, FALSE);

		compose->exteditor_ch = g_io_channel_unix_new(pipe_fds[0]);
		compose->exteditor_tag = g_io_add_watch(compose->exteditor_ch,
							G_IO_IN,
							compose_input_cb,
							compose);
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
#endif /* G_OS_UNIX */
}

#ifdef G_OS_UNIX
static gint compose_exec_ext_editor_real(const gchar *file)
{
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
			g_warning("External editor command line is invalid: '%s'\n",
				  prefs_common.ext_editor_cmd);
		g_snprintf(buf, sizeof(buf), DEFAULT_EDITOR_CMD, file);
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
		val = alertpanel_full(_("Notice"), msg, GTK_STOCK_NO, GTK_STOCK_YES,
		      		      NULL, FALSE, NULL, ALERT_WARNING, G_ALERTDEFAULT);
			
		g_free(msg);

		if (val == G_ALERTALTERNATE) {
			g_source_remove(compose->exteditor_tag);
			g_io_channel_shutdown(compose->exteditor_ch,
					      FALSE, NULL);
			g_io_channel_unref(compose->exteditor_ch);

			if (kill(pgid, SIGTERM) < 0) perror("kill");
			waitpid(compose->exteditor_pid, NULL, 0);

			g_warning("Terminated process group id: %d", -pgid);
			g_warning("Temporary file: %s",
				  compose->exteditor_file);

			compose_set_ext_editor_sensitive(compose, TRUE);

			g_free(compose->exteditor_file);
			compose->exteditor_file    = NULL;
			compose->exteditor_pid     = -1;
			compose->exteditor_ch      = NULL;
			compose->exteditor_tag     = -1;
		} else
			return FALSE;
	}

	return TRUE;
}

static gboolean compose_input_cb(GIOChannel *source, GIOCondition condition,
				 gpointer data)
{
	gchar buf[3] = "3";
	Compose *compose = (Compose *)data;
	gsize bytes_read;

	debug_print(_("Compose: input from monitoring process\n"));

	g_io_channel_read_chars(source, buf, sizeof(buf), &bytes_read, NULL);

	g_io_channel_shutdown(source, FALSE, NULL);
	g_io_channel_unref(source);

	waitpid(compose->exteditor_pid, NULL, 0);

	if (buf[0] == '0') {		/* success */
		GtkTextView *text = GTK_TEXT_VIEW(compose->text);
		GtkTextBuffer *buffer = gtk_text_view_get_buffer(text);

		gtk_text_buffer_set_text(buffer, "", -1);
		compose_insert_file(compose, compose->exteditor_file);
		compose_changed_cb(NULL, compose);

		if (g_unlink(compose->exteditor_file) < 0)
			FILE_OP_ERROR(compose->exteditor_file, "unlink");
	} else if (buf[0] == '1') {	/* failed */
		g_warning("Couldn't exec external editor\n");
		if (g_unlink(compose->exteditor_file) < 0)
			FILE_OP_ERROR(compose->exteditor_file, "unlink");
	} else if (buf[0] == '2') {
		g_warning("Couldn't write to file\n");
	} else if (buf[0] == '3') {
		g_warning("Pipe read failed\n");
	}

	compose_set_ext_editor_sensitive(compose, TRUE);

	g_free(compose->exteditor_file);
	compose->exteditor_file    = NULL;
	compose->exteditor_pid     = -1;
	compose->exteditor_ch      = NULL;
	compose->exteditor_tag     = -1;

	return FALSE;
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

	gtk_widget_set_sensitive(compose->text,                       sensitive);
	if (compose->toolbar->send_btn)
		gtk_widget_set_sensitive(compose->toolbar->send_btn,      sensitive);
	if (compose->toolbar->sendl_btn)
		gtk_widget_set_sensitive(compose->toolbar->sendl_btn,     sensitive);
	if (compose->toolbar->draft_btn)
		gtk_widget_set_sensitive(compose->toolbar->draft_btn,     sensitive);
	if (compose->toolbar->insert_btn)
		gtk_widget_set_sensitive(compose->toolbar->insert_btn,    sensitive);
	if (compose->toolbar->sig_btn)
		gtk_widget_set_sensitive(compose->toolbar->sig_btn,       sensitive);
	if (compose->toolbar->exteditor_btn)
		gtk_widget_set_sensitive(compose->toolbar->exteditor_btn, sensitive);
	if (compose->toolbar->linewrap_current_btn)
		gtk_widget_set_sensitive(compose->toolbar->linewrap_current_btn, sensitive);
	if (compose->toolbar->linewrap_all_btn)
		gtk_widget_set_sensitive(compose->toolbar->linewrap_all_btn, sensitive);
}
#endif /* G_OS_UNIX */

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
		gint char_width = 0, char_height = 0;
		gint line_width_in_chars;

		gtkut_get_font_size(GTK_WIDGET(widget),
				    &char_width, &char_height);
		line_width_in_chars =
			(allocation->width - allocation->x) / char_width;

		/* got the maximum */
		gtk_ruler_set_range(GTK_RULER(shruler),
				    0.0, line_width_in_chars, 0,
				    /*line_width_in_chars*/ char_width);
	}

	return TRUE;
}

static void account_activated(GtkComboBox *optmenu, gpointer data)
{
	Compose *compose = (Compose *)data;

	PrefsAccount *ac;
	gchar *folderidentifier;
	gint account_id = 0;
	GtkTreeModel *menu;
	GtkTreeIter iter;

	/* Get ID of active account in the combo box */
	menu = gtk_combo_box_get_model(optmenu);
	gtk_combo_box_get_active_iter(optmenu, &iter);
	gtk_tree_model_get(menu, &iter, 1, &account_id, -1);

	ac = account_find_from_id(account_id);
	g_return_if_fail(ac != NULL);

	if (ac != compose->account)
		compose_select_account(compose, ac, FALSE);

	/* Set message save folder */
	if (account_get_special_folder(compose->account, F_OUTBOX)) {
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(compose->savemsg_checkbtn), prefs_common.savemsg);
	}
	g_signal_connect(G_OBJECT(compose->savemsg_checkbtn), "toggled",
			 G_CALLBACK(compose_savemsg_checkbtn_cb), compose);
			   
	gtk_editable_delete_text(GTK_EDITABLE(compose->savemsg_entry), 0, -1);
	if (account_get_special_folder(compose->account, F_OUTBOX)) {
		folderidentifier = folder_item_get_identifier(account_get_special_folder
				  (compose->account, F_OUTBOX));
		gtk_entry_set_text(GTK_ENTRY(compose->savemsg_entry), folderidentifier);
		g_free(folderidentifier);
	}
}

static void attach_selected(GtkTreeView *tree_view, GtkTreePath *tree_path,
			    GtkTreeViewColumn *column, Compose *compose)
{
	compose_attach_property(compose);
}

static gboolean attach_button_pressed(GtkWidget *widget, GdkEventButton *event,
				      gpointer data)
{
	Compose *compose = (Compose *)data;
	GtkTreeSelection *attach_selection;
	gint attach_nr_selected;
	GtkItemFactory *ifactory;
	
	if (!event) return FALSE;

	if (event->button == 3) {
		attach_selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));
		attach_nr_selected = gtk_tree_selection_count_selected_rows(attach_selection);
		ifactory = gtk_item_factory_from_widget(compose->popupmenu);
			
		if (attach_nr_selected > 0)
		{
			menu_set_sensitive(ifactory, "/Remove", TRUE);
			menu_set_sensitive(ifactory, "/Properties...", TRUE);
		} else {
			menu_set_sensitive(ifactory, "/Remove", FALSE);
			menu_set_sensitive(ifactory, "/Properties...", FALSE);
		}
			
		gtk_menu_popup(GTK_MENU(compose->popupmenu), NULL, NULL,
			       NULL, NULL, event->button, event->time);
		return TRUE;			       
	}

	return FALSE;
}

static gboolean attach_key_pressed(GtkWidget *widget, GdkEventKey *event,
				   gpointer data)
{
	Compose *compose = (Compose *)data;

	if (!event) return FALSE;

	switch (event->keyval) {
	case GDK_Delete:
		compose_attach_remove_selected(compose);
		break;
	}
	return FALSE;
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
	
	gtk_text_view_set_editable(GTK_TEXT_VIEW(compose->text), allow);

}

static void compose_send_cb(gpointer data, guint action, GtkWidget *widget)
{
	Compose *compose = (Compose *)data;
	
	if (prefs_common.work_offline && 
	    !inc_offline_should_override(
		_("Sylpheed-Claws needs network access in order "
		  "to send this email.")))
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

	val = compose_queue_sub(compose, NULL, NULL, NULL, TRUE, TRUE);
	if (!val) 
		compose_close(compose);
	else if (val == -1) {
		alertpanel_error(_("Could not queue message."));
	} else if (val == -2) {
		alertpanel_error(_("Could not queue message:\n\n%s."), strerror(errno));
	} else if (val == -3) {
		if (privacy_peek_error())
		alertpanel_error(_("Could not queue message for sending:\n\n"
				   "Signature failed: %s"), privacy_get_error());
	} else if (val == -4) {
		alertpanel_error(_("Could not queue message for sending:\n\n"
				   "Charset conversion failed."));
	}
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
	gboolean target_locked = FALSE;
	
	if (lock) return;

	draft = account_get_special_folder(compose->account, F_DRAFT);
	g_return_if_fail(draft != NULL);
	
	if (!g_mutex_trylock(compose->mutex)) {
		/* we don't want to lock the mutex once it's available,
		 * because as the only other part of compose.c locking
		 * it is compose_close - which means once unlocked,
		 * the compose struct will be freed */
		debug_print("couldn't lock mutex, probably sending\n");
		return;
	}
	
	lock = TRUE;

	tmp = g_strdup_printf("%s%cdraft.%p", get_tmp_dir(),
			      G_DIR_SEPARATOR, compose);
	if ((fp = g_fopen(tmp, "wb")) == NULL) {
		FILE_OP_ERROR(tmp, "fopen");
		goto unlock;
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
	if (compose->return_receipt) {
		fprintf(fp, "RRCPT:1\n");
	}
	if (compose->privacy_system) {
		fprintf(fp, "X-Sylpheed-Sign:%d\n", compose->use_signing);
		fprintf(fp, "X-Sylpheed-Encrypt:%d\n", compose->use_encryption);
		fprintf(fp, "X-Sylpheed-Privacy-System:%s\n", compose->privacy_system);
	}

	/* Message-ID of message replying to */
	if ((compose->replyinfo != NULL) && (compose->replyinfo->msgid != NULL)) {
		gchar *folderid;
		
		folderid = folder_item_get_identifier(compose->replyinfo->folder);
		fprintf(fp, "RMID:%s\t%d\t%s\n", folderid, compose->replyinfo->msgnum, compose->replyinfo->msgid);
		g_free(folderid);
	}
	/* Message-ID of message forwarding to */
	if ((compose->fwdinfo != NULL) && (compose->fwdinfo->msgid != NULL)) {
		gchar *folderid;
		
		folderid = folder_item_get_identifier(compose->fwdinfo->folder);
		fprintf(fp, "FMID:%s\t%d\t%s\n", folderid, compose->fwdinfo->msgnum, compose->fwdinfo->msgid);
		g_free(folderid);
	}

	/* end of headers */
	fprintf(fp, "X-Sylpheed-End-Special-Headers: 1\n");

	if (compose_write_to_file(compose, fp, COMPOSE_WRITE_FOR_STORE, action != COMPOSE_AUTO_SAVE) < 0) {
		fclose(fp);
		g_unlink(tmp);
		g_free(tmp);
		goto unlock;
	}
	fclose(fp);
	
	if (compose->targetinfo) {
		target_locked = MSG_IS_LOCKED(compose->targetinfo->flags);
		flag.perm_flags = target_locked?MSG_LOCKED:0;
	}
	flag.tmp_flags = MSG_DRAFT;

	folder_item_scan(draft);
	if ((msgnum = folder_item_add_msg(draft, tmp, &flag, TRUE)) < 0) {
		g_unlink(tmp);
		g_free(tmp);
		if (action != COMPOSE_AUTO_SAVE)
			alertpanel_error(_("Could not save draft."));
		goto unlock;
	}
	g_free(tmp);

	if (compose->mode == COMPOSE_REEDIT) {
		compose_remove_reedit_target(compose, TRUE);
	}

	newmsginfo = folder_item_get_msginfo(draft, msgnum);
	if (newmsginfo) {
		procmsg_msginfo_unset_flags(newmsginfo, ~0, ~0);
		if (target_locked)
			procmsg_msginfo_set_flags(newmsginfo, MSG_LOCKED, MSG_DRAFT);
		else
			procmsg_msginfo_set_flags(newmsginfo, 0, MSG_DRAFT);
		if (compose_use_attach(compose) && action != COMPOSE_AUTO_SAVE)
			procmsg_msginfo_set_flags(newmsginfo, 0,
						  MSG_HAS_ATTACHMENT);

		procmsg_msginfo_free(newmsginfo);
	}
	
	folder_item_scan(draft);
	
	if (action == COMPOSE_QUIT_EDITING) {
		lock = FALSE;
		g_mutex_unlock(compose->mutex); /* must be done before closing */
		compose_close(compose);
		return;
	} else {
		struct stat s;
		gchar *path;

		path = folder_item_fetch_msg(draft, msgnum);
		if (path == NULL) {
			debug_print("can't fetch %s:%d\n",draft->path, msgnum);
			goto unlock;
		}
		if (g_stat(path, &s) < 0) {
			FILE_OP_ERROR(path, "stat");
			g_free(path);
			goto unlock;
		}
		g_free(path);

		procmsg_msginfo_free(compose->targetinfo);
		compose->targetinfo = procmsg_msginfo_new();
		compose->targetinfo->msgnum = msgnum;
		compose->targetinfo->size = s.st_size;
		compose->targetinfo->mtime = s.st_mtime;
		compose->targetinfo->folder = draft;
		if (target_locked)
			procmsg_msginfo_set_flags(compose->targetinfo, MSG_LOCKED, 0);
		compose->mode = COMPOSE_REEDIT;
		
		if (action == COMPOSE_AUTO_SAVE) {
			compose->autosaved_draft = compose->targetinfo;
		}
		compose->modified = FALSE;
		compose_set_title(compose);
	}
unlock:
	lock = FALSE;
	g_mutex_unlock(compose->mutex);
}

static void compose_attach_cb(gpointer data, guint action, GtkWidget *widget)
{
	Compose *compose = (Compose *)data;
	GList *file_list;

	if (compose->redirect_filename != NULL)
		return;

	file_list = filesel_select_multiple_files_open(_("Select file"));

	if (file_list) {
		GList *tmp;

		for ( tmp = file_list; tmp; tmp = tmp->next) {
			gchar *file = (gchar *) tmp->data;
			gchar *utf8_filename = conv_filename_to_utf8(file);
			compose_attach_append(compose, file, utf8_filename, NULL);
			compose_changed_cb(NULL, compose);
			g_free(file);
			g_free(utf8_filename);
		}
		g_list_free(file_list);
	}		
}

static void compose_insert_file_cb(gpointer data, guint action,
				   GtkWidget *widget)
{
	Compose *compose = (Compose *)data;
	GList *file_list;

	file_list = filesel_select_multiple_files_open(_("Select file"));

	if (file_list) {
		GList *tmp;

		for ( tmp = file_list; tmp; tmp = tmp->next) {
			gchar *file = (gchar *) tmp->data;
			gchar *filedup = g_strdup(file);
			gchar *shortfile = g_path_get_basename(filedup);
			ComposeInsertResult res;

			res = compose_insert_file(compose, file);
			if (res == COMPOSE_INSERT_READ_ERROR) {
				alertpanel_error(_("File '%s' could not be read."), shortfile);
			} else if (res == COMPOSE_INSERT_INVALID_CHARACTER) {
				alertpanel_error(_("File '%s' contained invalid characters\n"
						   "for the current encoding, insertion may be incorrect."), shortfile);
			}
			g_free(shortfile);
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

	if (compose->sending || compose->updating)
		return TRUE;
	compose_close_cb(compose, 0, NULL);
	return TRUE;
}

static void compose_close_cb(gpointer data, guint action, GtkWidget *widget)
{
	Compose *compose = (Compose *)data;
	AlertValue val;

#ifdef G_OS_UNIX
	if (compose->exteditor_tag != -1) {
		if (!compose_ext_editor_kill(compose))
			return;
	}
#endif

	if (compose->modified) {
		val = alertpanel(_("Discard message"),
				 _("This message has been modified. Discard it?"),
				 _("_Discard"), _("_Save to Drafts"), GTK_STOCK_CANCEL);

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

static void compose_set_encoding_cb(gpointer data, guint action,
				    GtkWidget *widget)
{
	Compose *compose = (Compose *)data;

	if (GTK_CHECK_MENU_ITEM(widget)->active)
		compose->out_encoding = (CharSet)action;
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

	tmpl = g_object_get_data(G_OBJECT(widget), "template");
	g_return_if_fail(tmpl != NULL);

	msg = g_strdup_printf(_("Do you want to apply the template '%s' ?"),
			      tmpl->name);
	val = alertpanel(_("Apply template"), msg,
			 _("_Replace"), _("_Insert"), GTK_STOCK_CANCEL);
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

static void compose_undo_cb(Compose *compose)
{
	gboolean prev_autowrap = compose->autowrap;

	compose->autowrap = FALSE;
	undo_undo(compose->undostruct);
	compose->autowrap = prev_autowrap;
}

static void compose_redo_cb(Compose *compose)
{
	gboolean prev_autowrap = compose->autowrap;
	
	compose->autowrap = FALSE;
	undo_redo(compose->undostruct);
	compose->autowrap = prev_autowrap;
}

static void entry_cut_clipboard(GtkWidget *entry)
{
	if (GTK_IS_EDITABLE(entry))
		gtk_editable_cut_clipboard (GTK_EDITABLE(entry));
	else if (GTK_IS_TEXT_VIEW(entry))
		gtk_text_buffer_cut_clipboard(
			gtk_text_view_get_buffer(GTK_TEXT_VIEW(entry)),
			gtk_clipboard_get(GDK_SELECTION_CLIPBOARD),
			TRUE);
}

static void entry_copy_clipboard(GtkWidget *entry)
{
	if (GTK_IS_EDITABLE(entry))
		gtk_editable_copy_clipboard (GTK_EDITABLE(entry));
	else if (GTK_IS_TEXT_VIEW(entry))
		gtk_text_buffer_copy_clipboard(
			gtk_text_view_get_buffer(GTK_TEXT_VIEW(entry)),
			gtk_clipboard_get(GDK_SELECTION_CLIPBOARD));
}

static void entry_paste_clipboard(Compose *compose, GtkWidget *entry, 
				  gboolean wrap, GdkAtom clip, GtkTextIter *insert_place)
{
	if (GTK_IS_TEXT_VIEW(entry)) {
		GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(entry));
		GtkTextMark *mark_start = gtk_text_buffer_get_insert(buffer);
		GtkTextIter start_iter, end_iter;
		gint start, end;
		gchar *contents = gtk_clipboard_wait_for_text(gtk_clipboard_get(clip));

		if (contents == NULL)
			return;

		undo_paste_clipboard(GTK_TEXT_VIEW(compose->text), compose->undostruct);

		/* we shouldn't delete the selection when middle-click-pasting, or we
		 * can't mid-click-paste our own selection */
		if (clip != GDK_SELECTION_PRIMARY) {
			gtk_text_buffer_delete_selection(buffer, FALSE, TRUE);
		}
		
		if (insert_place == NULL) {
			/* if insert_place isn't specified, insert at the cursor.
			 * used for Ctrl-V pasting */
			gtk_text_buffer_get_iter_at_mark(buffer, &start_iter, mark_start);
			start = gtk_text_iter_get_offset(&start_iter);
			gtk_text_buffer_insert(buffer, &start_iter, contents, strlen(contents));
		} else {
			/* if insert_place is specified, paste here.
			 * used for mid-click-pasting */
			start = gtk_text_iter_get_offset(insert_place);
			gtk_text_buffer_insert(buffer, insert_place, contents, strlen(contents));
		}
		
		if (!wrap) {
			/* paste unwrapped: mark the paste so it's not wrapped later */
			end = start + strlen(contents);
			gtk_text_buffer_get_iter_at_offset(buffer, &start_iter, start);
			gtk_text_buffer_get_iter_at_offset(buffer, &end_iter, end);
			gtk_text_buffer_apply_tag_by_name(buffer, "no_wrap", &start_iter, &end_iter);
		} else if (wrap && clip == GDK_SELECTION_PRIMARY) {
			/* rewrap paragraph now (after a mid-click-paste) */
			mark_start = gtk_text_buffer_get_insert(buffer);
			gtk_text_buffer_get_iter_at_mark(buffer, &start_iter, mark_start);
			gtk_text_iter_backward_char(&start_iter);
			compose_beautify_paragraph(compose, &start_iter, TRUE);
		}
	} else if (GTK_IS_EDITABLE(entry))
		gtk_editable_paste_clipboard (GTK_EDITABLE(entry));
	
}

static void entry_allsel(GtkWidget *entry)
{
	if (GTK_IS_EDITABLE(entry))
		gtk_editable_select_region(GTK_EDITABLE(entry), 0, -1);
	else if (GTK_IS_TEXT_VIEW(entry)) {
		GtkTextIter startiter, enditer;
		GtkTextBuffer *textbuf;

		textbuf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(entry));
		gtk_text_buffer_get_start_iter(textbuf, &startiter);
		gtk_text_buffer_get_end_iter(textbuf, &enditer);

		gtk_text_buffer_move_mark_by_name(textbuf, 
			"selection_bound", &startiter);
		gtk_text_buffer_move_mark_by_name(textbuf, 
			"insert", &enditer);
	}
}

static void compose_cut_cb(Compose *compose)
{
	if (compose->focused_editable &&
	    GTK_WIDGET_HAS_FOCUS(compose->focused_editable))
		entry_cut_clipboard(compose->focused_editable);
}

static void compose_copy_cb(Compose *compose)
{
	if (compose->focused_editable &&
	    GTK_WIDGET_HAS_FOCUS(compose->focused_editable))
		entry_copy_clipboard(compose->focused_editable);
}

static void compose_paste_cb(Compose *compose)
{
	gint prev_autowrap;
	GtkTextBuffer *buffer;
	BLOCK_WRAP();
	if (compose->focused_editable &&
	    GTK_WIDGET_HAS_FOCUS(compose->focused_editable))
		entry_paste_clipboard(compose, compose->focused_editable, 
				prefs_common.linewrap_pastes,
				GDK_SELECTION_CLIPBOARD, NULL);
	UNBLOCK_WRAP();
}

static void compose_paste_as_quote_cb(Compose *compose)
{
	gint wrap_quote = prefs_common.linewrap_quote;
	if (compose->focused_editable &&
	    GTK_WIDGET_HAS_FOCUS(compose->focused_editable)) {
		/* let text_insert() (called directly or at a later time
		 * after the gtk_editable_paste_clipboard) know that 
		 * text is to be inserted as a quotation. implemented
		 * by using a simple refcount... */
		gint paste_as_quotation = GPOINTER_TO_INT(g_object_get_data(
						G_OBJECT(compose->focused_editable),
						"paste_as_quotation"));
		g_object_set_data(G_OBJECT(compose->focused_editable),
				    "paste_as_quotation",
				    GINT_TO_POINTER(paste_as_quotation + 1));
		prefs_common.linewrap_quote = prefs_common.linewrap_pastes;
		entry_paste_clipboard(compose, compose->focused_editable, 
				prefs_common.linewrap_pastes,
				GDK_SELECTION_CLIPBOARD, NULL);
		prefs_common.linewrap_quote = wrap_quote;
	}
}

static void compose_paste_no_wrap_cb(Compose *compose)
{
	gint prev_autowrap;
	GtkTextBuffer *buffer;
	BLOCK_WRAP();
	if (compose->focused_editable &&
	    GTK_WIDGET_HAS_FOCUS(compose->focused_editable))
		entry_paste_clipboard(compose, compose->focused_editable, FALSE,
			GDK_SELECTION_CLIPBOARD, NULL);
	UNBLOCK_WRAP();
}

static void compose_paste_wrap_cb(Compose *compose)
{
	gint prev_autowrap;
	GtkTextBuffer *buffer;
	BLOCK_WRAP();
	if (compose->focused_editable &&
	    GTK_WIDGET_HAS_FOCUS(compose->focused_editable))
		entry_paste_clipboard(compose, compose->focused_editable, TRUE,
			GDK_SELECTION_CLIPBOARD, NULL);
	UNBLOCK_WRAP();
}

static void compose_allsel_cb(Compose *compose)
{
	if (compose->focused_editable &&
	    GTK_WIDGET_HAS_FOCUS(compose->focused_editable))
		entry_allsel(compose->focused_editable);
}

static void textview_move_beginning_of_line (GtkTextView *text)
{
	GtkTextBuffer *buffer;
	GtkTextMark *mark;
	GtkTextIter ins;

	g_return_if_fail(GTK_IS_TEXT_VIEW(text));

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text));
	mark = gtk_text_buffer_get_insert(buffer);
	gtk_text_buffer_get_iter_at_mark(buffer, &ins, mark);
	gtk_text_iter_set_line_offset(&ins, 0);
	gtk_text_buffer_place_cursor(buffer, &ins);
}

static void textview_move_forward_character (GtkTextView *text)
{
	GtkTextBuffer *buffer;
	GtkTextMark *mark;
	GtkTextIter ins;

	g_return_if_fail(GTK_IS_TEXT_VIEW(text));

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text));
	mark = gtk_text_buffer_get_insert(buffer);
	gtk_text_buffer_get_iter_at_mark(buffer, &ins, mark);
	if (gtk_text_iter_forward_cursor_position(&ins))
		gtk_text_buffer_place_cursor(buffer, &ins);
}

static void textview_move_backward_character (GtkTextView *text)
{
	GtkTextBuffer *buffer;
	GtkTextMark *mark;
	GtkTextIter ins;

	g_return_if_fail(GTK_IS_TEXT_VIEW(text));

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text));
	mark = gtk_text_buffer_get_insert(buffer);
	gtk_text_buffer_get_iter_at_mark(buffer, &ins, mark);
	if (gtk_text_iter_backward_cursor_position(&ins))
		gtk_text_buffer_place_cursor(buffer, &ins);
}

static void textview_move_forward_word (GtkTextView *text)
{
	GtkTextBuffer *buffer;
	GtkTextMark *mark;
	GtkTextIter ins;
	gint count;

	g_return_if_fail(GTK_IS_TEXT_VIEW(text));

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text));
	mark = gtk_text_buffer_get_insert(buffer);
	gtk_text_buffer_get_iter_at_mark(buffer, &ins, mark);
	count = gtk_text_iter_inside_word (&ins) ? 2 : 1;
	if (gtk_text_iter_forward_word_ends(&ins, count)) {
		gtk_text_iter_backward_word_start(&ins);
		gtk_text_buffer_place_cursor(buffer, &ins);
	}
}

static void textview_move_backward_word (GtkTextView *text)
{
	GtkTextBuffer *buffer;
	GtkTextMark *mark;
	GtkTextIter ins;
	gint count;

	g_return_if_fail(GTK_IS_TEXT_VIEW(text));

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text));
	mark = gtk_text_buffer_get_insert(buffer);
	gtk_text_buffer_get_iter_at_mark(buffer, &ins, mark);
	count = gtk_text_iter_inside_word (&ins) ? 2 : 1;
	if (gtk_text_iter_backward_word_starts(&ins, 1))
		gtk_text_buffer_place_cursor(buffer, &ins);
}

static void textview_move_end_of_line (GtkTextView *text)
{
	GtkTextBuffer *buffer;
	GtkTextMark *mark;
	GtkTextIter ins;

	g_return_if_fail(GTK_IS_TEXT_VIEW(text));

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text));
	mark = gtk_text_buffer_get_insert(buffer);
	gtk_text_buffer_get_iter_at_mark(buffer, &ins, mark);
	if (gtk_text_iter_forward_to_line_end(&ins))
		gtk_text_buffer_place_cursor(buffer, &ins);
}

static void textview_move_next_line (GtkTextView *text)
{
	GtkTextBuffer *buffer;
	GtkTextMark *mark;
	GtkTextIter ins;
	gint offset;

	g_return_if_fail(GTK_IS_TEXT_VIEW(text));

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text));
	mark = gtk_text_buffer_get_insert(buffer);
	gtk_text_buffer_get_iter_at_mark(buffer, &ins, mark);
	offset = gtk_text_iter_get_line_offset(&ins);
	if (gtk_text_iter_forward_line(&ins)) {
		gtk_text_iter_set_line_offset(&ins, offset);
		gtk_text_buffer_place_cursor(buffer, &ins);
	}
}

static void textview_move_previous_line (GtkTextView *text)
{
	GtkTextBuffer *buffer;
	GtkTextMark *mark;
	GtkTextIter ins;
	gint offset;

	g_return_if_fail(GTK_IS_TEXT_VIEW(text));

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text));
	mark = gtk_text_buffer_get_insert(buffer);
	gtk_text_buffer_get_iter_at_mark(buffer, &ins, mark);
	offset = gtk_text_iter_get_line_offset(&ins);
	if (gtk_text_iter_backward_line(&ins)) {
		gtk_text_iter_set_line_offset(&ins, offset);
		gtk_text_buffer_place_cursor(buffer, &ins);
	}
}

static void textview_delete_forward_character (GtkTextView *text)
{
	GtkTextBuffer *buffer;
	GtkTextMark *mark;
	GtkTextIter ins, end_iter;

	g_return_if_fail(GTK_IS_TEXT_VIEW(text));

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text));
	mark = gtk_text_buffer_get_insert(buffer);
	gtk_text_buffer_get_iter_at_mark(buffer, &ins, mark);
	end_iter = ins;
	if (gtk_text_iter_forward_char(&end_iter)) {
		gtk_text_buffer_delete(buffer, &ins, &end_iter);
	}
}

static void textview_delete_backward_character (GtkTextView *text)
{
	GtkTextBuffer *buffer;
	GtkTextMark *mark;
	GtkTextIter ins, end_iter;

	g_return_if_fail(GTK_IS_TEXT_VIEW(text));

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text));
	mark = gtk_text_buffer_get_insert(buffer);
	gtk_text_buffer_get_iter_at_mark(buffer, &ins, mark);
	end_iter = ins;
	if (gtk_text_iter_backward_char(&end_iter)) {
		gtk_text_buffer_delete(buffer, &end_iter, &ins);
	}
}

static void textview_delete_forward_word (GtkTextView *text)
{
	GtkTextBuffer *buffer;
	GtkTextMark *mark;
	GtkTextIter ins, end_iter;

	g_return_if_fail(GTK_IS_TEXT_VIEW(text));

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text));
	mark = gtk_text_buffer_get_insert(buffer);
	gtk_text_buffer_get_iter_at_mark(buffer, &ins, mark);
	end_iter = ins;
	if (gtk_text_iter_forward_word_end(&end_iter)) {
		gtk_text_buffer_delete(buffer, &ins, &end_iter);
	}
}

static void textview_delete_backward_word (GtkTextView *text)
{
	GtkTextBuffer *buffer;
	GtkTextMark *mark;
	GtkTextIter ins, end_iter;

	g_return_if_fail(GTK_IS_TEXT_VIEW(text));

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text));
	mark = gtk_text_buffer_get_insert(buffer);
	gtk_text_buffer_get_iter_at_mark(buffer, &ins, mark);
	end_iter = ins;
	if (gtk_text_iter_backward_word_start(&end_iter)) {
		gtk_text_buffer_delete(buffer, &end_iter, &ins);
	}
}

static void textview_delete_line (GtkTextView *text)
{
	GtkTextBuffer *buffer;
	GtkTextMark *mark;
	GtkTextIter ins, start_iter, end_iter;
	gboolean found;

	g_return_if_fail(GTK_IS_TEXT_VIEW(text));

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text));
	mark = gtk_text_buffer_get_insert(buffer);
	gtk_text_buffer_get_iter_at_mark(buffer, &ins, mark);

	start_iter = ins;
	gtk_text_iter_set_line_offset(&start_iter, 0);

	end_iter = ins;
	if (gtk_text_iter_ends_line(&end_iter))
		found = gtk_text_iter_forward_char(&end_iter);
	else
		found = gtk_text_iter_forward_to_line_end(&end_iter);

	if (found)
		gtk_text_buffer_delete(buffer, &start_iter, &end_iter);
}

static void textview_delete_to_line_end (GtkTextView *text)
{
	GtkTextBuffer *buffer;
	GtkTextMark *mark;
	GtkTextIter ins, end_iter;
	gboolean found;

	g_return_if_fail(GTK_IS_TEXT_VIEW(text));

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text));
	mark = gtk_text_buffer_get_insert(buffer);
	gtk_text_buffer_get_iter_at_mark(buffer, &ins, mark);
	end_iter = ins;
	if (gtk_text_iter_ends_line(&end_iter))
		found = gtk_text_iter_forward_char(&end_iter);
	else
		found = gtk_text_iter_forward_to_line_end(&end_iter);
	if (found)
		gtk_text_buffer_delete(buffer, &ins, &end_iter);
}

static void compose_advanced_action_cb(Compose *compose,
					ComposeCallAdvancedAction action)
{
	GtkTextView *text = GTK_TEXT_VIEW(compose->text);
	static struct {
		void (*do_action) (GtkTextView *text);
	} action_table[] = {
		{textview_move_beginning_of_line},
		{textview_move_forward_character},
		{textview_move_backward_character},
		{textview_move_forward_word},
		{textview_move_backward_word},
		{textview_move_end_of_line},
		{textview_move_next_line},
		{textview_move_previous_line},
		{textview_delete_forward_character},
		{textview_delete_backward_character},
		{textview_delete_forward_word},
		{textview_delete_backward_word},
		{textview_delete_line},
		{NULL}, /* gtk_stext_delete_line_n */
		{textview_delete_to_line_end}
	};

	if (!GTK_WIDGET_HAS_FOCUS(text)) return;

	if (action >= COMPOSE_CALL_ADVANCED_ACTION_MOVE_BEGINNING_OF_LINE &&
	    action <= COMPOSE_CALL_ADVANCED_ACTION_DELETE_TO_LINE_END) {
		if (action_table[action].do_action)
			action_table[action].do_action(text);
		else
			g_warning("Not implemented yet.");
	}
}

static void compose_grab_focus_cb(GtkWidget *widget, Compose *compose)
{
	gchar *str = NULL;
	
	if (GTK_IS_EDITABLE(widget)) {
		str = gtk_editable_get_chars(GTK_EDITABLE(widget), 0, -1);
		gtk_editable_set_position(GTK_EDITABLE(widget), 
			strlen(str));
		g_free(str);
		if (widget->parent && widget->parent->parent
		 && widget->parent->parent->parent) {
			if (GTK_IS_SCROLLED_WINDOW(widget->parent->parent->parent)) {
				gint y = widget->allocation.y;
				gint height = widget->allocation.height;
				GtkAdjustment *shown = gtk_scrolled_window_get_vadjustment
					(GTK_SCROLLED_WINDOW(widget->parent->parent->parent));

				if (y < (int)shown->value) {
					gtk_adjustment_set_value(GTK_ADJUSTMENT(shown), y - 1);
				}
				if (y + height > (int)shown->value + (int)shown->page_size) {
					if (y - height - 1 < (int)shown->upper - (int)shown->page_size) {
						gtk_adjustment_set_value(GTK_ADJUSTMENT(shown), 
							y + height - (int)shown->page_size - 1);
					} else {
						gtk_adjustment_set_value(GTK_ADJUSTMENT(shown), 
							(int)shown->upper - (int)shown->page_size - 1);
					}
				}
			}
		}
	}

	if (GTK_IS_EDITABLE(widget) || GTK_IS_TEXT_VIEW(widget))
		compose->focused_editable = widget;
}

static void compose_changed_cb(GtkTextBuffer *textbuf, Compose *compose)
{
	compose->modified = TRUE;
	compose_set_title(compose);
}

static void compose_wrap_cb(gpointer data, guint action, GtkWidget *widget)
{
	Compose *compose = (Compose *)data;

	if (action == 1)
		compose_wrap_all_full(compose, TRUE);
	else
		compose_beautify_paragraph(compose, NULL, TRUE);
}

static void compose_find_cb(gpointer data, guint action, GtkWidget *widget)
{
	Compose *compose = (Compose *)data;

	message_search_compose(compose);
}

static void compose_toggle_autowrap_cb(gpointer data, guint action,
				       GtkWidget *widget)
{
	Compose *compose = (Compose *)data;
	compose->autowrap = GTK_CHECK_MENU_ITEM(widget)->active;
	if (compose->autowrap)
		compose_wrap_all_full(compose, TRUE);
	compose->autowrap = GTK_CHECK_MENU_ITEM(widget)->active;
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

static void activate_privacy_system(Compose *compose, PrefsAccount *account, gboolean warn) 
{
	g_free(compose->privacy_system);

	compose->privacy_system = g_strdup(account->default_privacy_system);
	compose_update_privacy_system_menu_item(compose, warn);
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
					     GdkDragContext	*context,
					     gint		 x,
					     gint		 y,
					     GtkSelectionData	*data,
					     guint		 info,
					     guint		 time,
					     gpointer		 user_data)
{
	Compose *compose = (Compose *)user_data;
	GList *list, *tmp;

	if (gdk_atom_name(data->type) && 
	    !strcmp(gdk_atom_name(data->type), "text/uri-list")
	    && gtk_drag_get_source_widget(context) != 
	        mainwindow_get_mainwindow()->summaryview->ctree) {
		list = uri_list_extract_filenames((const gchar *)data->data);
		for (tmp = list; tmp != NULL; tmp = tmp->next)
			compose_attach_append
				(compose, (const gchar *)tmp->data,
				 (const gchar *)tmp->data, NULL);
		if (list) compose_changed_cb(NULL, compose);
		list_free_strings(list);
		g_list_free(list);
	} else if (gtk_drag_get_source_widget(context) 
		   == mainwindow_get_mainwindow()->summaryview->ctree) {
		/* comes from our summaryview */
		SummaryView * summaryview = NULL;
		GSList * list = NULL, *cur = NULL;
		
		if (mainwindow_get_mainwindow())
			summaryview = mainwindow_get_mainwindow()->summaryview;
		
		if (summaryview)
			list = summary_get_selected_msg_list(summaryview);
		
		for (cur = list; cur; cur = cur->next) {
			MsgInfo *msginfo = (MsgInfo *)cur->data;
			gchar *file = NULL;
			if (msginfo)
				file = procmsg_get_message_file_full(msginfo, 
					TRUE, TRUE);
			if (file) {
				compose_attach_append(compose, (const gchar *)file, 
					(const gchar *)file, "message/rfc822");
				g_free(file);
			}
		}
		g_slist_free(list);
	}
}

static gboolean compose_drag_drop(GtkWidget *widget,
				  GdkDragContext *drag_context,
				  gint x, gint y,
				  guint time, gpointer user_data)
{
	/* not handling this signal makes compose_insert_drag_received_cb
	 * called twice */
	return TRUE;					 
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

	/* strangely, testing data->type == gdk_atom_intern("text/uri-list", TRUE)
	 * does not work */
	if (gdk_atom_name(data->type) && !strcmp(gdk_atom_name(data->type), "text/uri-list")) {
		AlertValue val = G_ALERTDEFAULT;

		switch (prefs_common.compose_dnd_mode) {
			case COMPOSE_DND_ASK:
				val = alertpanel_full(_("Insert or attach?"),
					 _("Do you want to insert the contents of the file(s) "
					   "into the message body, or attach it to the email?"),
					  GTK_STOCK_CANCEL, _("+_Insert"), _("_Attach"),
					  TRUE, NULL, ALERT_QUESTION, G_ALERTALTERNATE);
				break;
			case COMPOSE_DND_INSERT:
				val = G_ALERTALTERNATE;
				break;
			case COMPOSE_DND_ATTACH:
				val = G_ALERTOTHER;
				break;
			default:
				/* unexpected case */
				g_warning("error: unexpected compose_dnd_mode option value in compose_insert_drag_received_cb()");
		}

		if (val & G_ALERTDISABLE) {
			val &= ~G_ALERTDISABLE;
			/* remember what action to perform by default, only if we don't click Cancel */
			if (val == G_ALERTALTERNATE)
				prefs_common.compose_dnd_mode = COMPOSE_DND_INSERT;
			else if (val == G_ALERTOTHER)
					prefs_common.compose_dnd_mode = COMPOSE_DND_ATTACH;
		}

		if (val == G_ALERTDEFAULT || val == G_ALERTCANCEL) {
			gtk_drag_finish(drag_context, FALSE, FALSE, time);
			return;
		} else if (val == G_ALERTOTHER) {
			compose_attach_drag_received_cb(widget, drag_context, x, y, data, info, time, user_data);
			return;
		} 
		list = uri_list_extract_filenames((const gchar *)data->data);
		for (tmp = list; tmp != NULL; tmp = tmp->next) {
			compose_insert_file(compose, (const gchar *)tmp->data);
		}
		list_free_strings(list);
		g_list_free(list);
		gtk_drag_finish(drag_context, TRUE, FALSE, time);
		return;
	} else {
#if GTK_CHECK_VERSION(2, 8, 0)
		/* do nothing, handled by GTK */
#else
		gchar *tmpfile = get_tmp_file();
		str_write_to_file((const gchar *)data->data, tmpfile);
		compose_insert_file(compose, tmpfile);
		g_unlink(tmpfile);
		g_free(tmpfile);
		gtk_drag_finish(drag_context, TRUE, FALSE, time);
#endif
		return;
	}
	gtk_drag_finish(drag_context, TRUE, FALSE, time);
}

static void compose_header_drag_received_cb (GtkWidget		*widget,
					     GdkDragContext	*drag_context,
					     gint		 x,
					     gint		 y,
					     GtkSelectionData	*data,
					     guint		 info,
					     guint		 time,
					     gpointer		 user_data)
{
	GtkEditable *entry = (GtkEditable *)user_data;
	gchar *email = (gchar *)data->data;

	/* strangely, testing data->type == gdk_atom_intern("text/plain", TRUE)
	 * does not work */

	if (!strncmp(email, "mailto:", strlen("mailto:"))) {
		gchar *decoded=g_new(gchar, strlen(email));
		int start = 0;

		email += strlen("mailto:");
		decode_uri(decoded, email); /* will fit */
		gtk_editable_delete_text(entry, 0, -1);
		gtk_editable_insert_text(entry, decoded, strlen(decoded), &start);
		gtk_drag_finish(drag_context, TRUE, FALSE, time);
		g_free(decoded);
		return;
	}
	gtk_drag_finish(drag_context, TRUE, FALSE, time);
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

static void compose_toggle_remove_refs_cb(gpointer data, guint action,
					     GtkWidget *widget)
{
	Compose *compose = (Compose *)data;

	if (GTK_CHECK_MENU_ITEM(widget)->active)
		compose->remove_references = TRUE;
	else
		compose->remove_references = FALSE;
}

gboolean compose_headerentry_key_press_event_cb(GtkWidget *entry,
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
			g_signal_stop_emission_by_name(G_OBJECT(entry), "key-press-event"); 
			gtk_widget_grab_focus(headerentry->compose->subject_entry);
			return TRUE;
		}
	}
	return FALSE;
}

gboolean compose_headerentry_changed_cb(GtkWidget *entry,
				    ComposeHeaderEntry *headerentry)
{
	if (strlen(gtk_entry_get_text(GTK_ENTRY(entry))) != 0) {
		headerentry->compose->header_list =
			g_slist_append(headerentry->compose->header_list,
				       headerentry);
		
		compose_create_header_entry(headerentry->compose);
		g_signal_handlers_disconnect_matched
			(G_OBJECT(entry), G_SIGNAL_MATCH_DATA,
			 0, 0, NULL, NULL, headerentry);
		
		/* Automatically scroll down */
		compose_show_first_last_header(headerentry->compose, FALSE);
		
	}
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

static void text_inserted(GtkTextBuffer *buffer, GtkTextIter *iter,
			  const gchar *text, gint len, Compose *compose)
{
	gint paste_as_quotation = GPOINTER_TO_INT(g_object_get_data
				(G_OBJECT(compose->text), "paste_as_quotation"));
	GtkTextMark *mark;

	g_return_if_fail(text != NULL);

	g_signal_handlers_block_by_func(G_OBJECT(buffer),
					G_CALLBACK(text_inserted),
					compose);
	if (paste_as_quotation) {
		gchar *new_text;
		gchar *qmark;

		if (len < 0)
			len = strlen(text);

		new_text = g_strndup(text, len);
		if (prefs_common.quotemark && *prefs_common.quotemark)
			qmark = prefs_common.quotemark;
		else
			qmark = "> ";

		mark = gtk_text_buffer_create_mark(buffer, NULL, iter, FALSE);
		gtk_text_buffer_place_cursor(buffer, iter);

		compose_quote_fmt(compose, NULL, "%Q", qmark, new_text, TRUE);
		quote_fmt_reset_vartable();
		g_free(new_text);
		g_object_set_data(G_OBJECT(compose->text), "paste_as_quotation",
				  GINT_TO_POINTER(paste_as_quotation - 1));
				  
		gtk_text_buffer_get_iter_at_mark(buffer, iter, mark);
		gtk_text_buffer_place_cursor(buffer, iter);
	} else {
		if (strcmp(text, "\n") || automatic_break
		|| gtk_text_iter_starts_line(iter))
			gtk_text_buffer_insert(buffer, iter, text, len);
		else {
			debug_print("insert nowrap \\n\n");
			gtk_text_buffer_insert_with_tags_by_name(buffer, 
				iter, text, len, "no_join", NULL);
		}
	}
	
	mark = gtk_text_buffer_create_mark(buffer, NULL, iter, FALSE);
	
	compose_beautify_paragraph(compose, iter, FALSE);

	gtk_text_buffer_get_iter_at_mark(buffer, iter, mark);
	gtk_text_buffer_delete_mark(buffer, mark);

	g_signal_handlers_unblock_by_func(G_OBJECT(buffer),
					  G_CALLBACK(text_inserted),
					  compose);
	g_signal_stop_emission_by_name(G_OBJECT(buffer), "insert-text");

	if (prefs_common.autosave && 
	    gtk_text_buffer_get_char_count(buffer) % prefs_common.autosave_length == 0)
		compose->draft_timeout_tag = gtk_timeout_add
			(500, (GtkFunction) compose_defer_auto_save_draft, compose);
}
static gint compose_defer_auto_save_draft(Compose *compose)
{
	compose->draft_timeout_tag = -1;
	compose_draft_cb((gpointer)compose, COMPOSE_AUTO_SAVE, NULL);
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

gboolean compose_close(Compose *compose)
{
	gint x, y;

	if (!g_mutex_trylock(compose->mutex)) {
		/* we have to wait for the (possibly deferred by auto-save)
		 * drafting to be done, before destroying the compose under
		 * it. */
		debug_print("waiting for drafting to finish...\n");
		g_timeout_add (500, (GSourceFunc) compose_close, compose);
		return FALSE;
	}
	g_return_val_if_fail(compose, FALSE);
	gtkut_widget_get_uposition(compose->window, &x, &y);
	prefs_common.compose_x = x;
	prefs_common.compose_y = y;
	g_mutex_unlock(compose->mutex);
	compose_destroy(compose);
	return FALSE;
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

static void compose_reply_from_messageview_real(MessageView *msgview, GSList *msginfo_list, 
				    guint action, gboolean opening_multiple)
{
	gchar *body = NULL;
	GSList *new_msglist = NULL;
	MsgInfo *tmp_msginfo = NULL;
	gboolean originally_enc = FALSE;
	Compose *compose = NULL;

	g_return_if_fail(msgview != NULL);

	g_return_if_fail(msginfo_list != NULL);

	if (g_slist_length(msginfo_list) == 1 && !opening_multiple) {
		MimeInfo *mimeinfo = messageview_get_selected_mime_part(msgview);
		MsgInfo *orig_msginfo = (MsgInfo *)msginfo_list->data;
		
		if (mimeinfo != NULL && mimeinfo->type == MIMETYPE_MESSAGE && 
		    !g_ascii_strcasecmp(mimeinfo->subtype, "rfc822")) {
			tmp_msginfo = procmsg_msginfo_new_from_mimeinfo(
						orig_msginfo, mimeinfo);
			if (tmp_msginfo != NULL) {
				new_msglist = g_slist_append(NULL, tmp_msginfo);
				if (procmime_msginfo_is_encrypted(orig_msginfo)) {
					originally_enc = TRUE;
				}
			} 
		}
	}

	if (!opening_multiple)
		body = messageview_get_selection(msgview);

	if (new_msglist) {
		compose = compose_reply_mode((ComposeMode)action, new_msglist, body);
		procmsg_msginfo_free(tmp_msginfo);
		g_slist_free(new_msglist);
	} else
		compose = compose_reply_mode((ComposeMode)action, msginfo_list, body);

	if (originally_enc) {
		compose_force_encryption(compose, compose->account, FALSE);
	}

	g_free(body);
}

void compose_reply_from_messageview(MessageView *msgview, GSList *msginfo_list, 
				    guint action)
{
	if ((!prefs_common.forward_as_attachment || action != COMPOSE_FORWARD) 
	&&  action != COMPOSE_FORWARD_AS_ATTACH && g_slist_length(msginfo_list) > 1) {
		GSList *cur = msginfo_list;
		gchar *msg = g_strdup_printf(_("You are about to reply to %d "
					       "messages. Opening the windows "
					       "could take some time. Do you "
					       "want to continue?"), 
					       g_slist_length(msginfo_list));
		if (g_slist_length(msginfo_list) > 9
		&&  alertpanel(_("Warning"), msg, GTK_STOCK_CANCEL, "+" GTK_STOCK_YES, NULL)
		    != G_ALERTALTERNATE) {
		    	g_free(msg);
			return;
		}
		g_free(msg);
		/* We'll open multiple compose windows */
		/* let the WM place the next windows */
		compose_force_window_origin = FALSE;
		for (; cur; cur = cur->next) {
			GSList tmplist;
			tmplist.data = cur->data;
			tmplist.next = NULL;
			compose_reply_from_messageview_real(msgview, &tmplist, action, TRUE);
		}
		compose_force_window_origin = TRUE;
	} else {
		/* forwarding multiple mails as attachments is done via a
		 * single compose window */
		compose_reply_from_messageview_real(msgview, msginfo_list, action, FALSE);
	}
}

void compose_set_position(Compose *compose, gint pos)
{
	GtkTextView *text = GTK_TEXT_VIEW(compose->text);

	gtkut_text_view_set_position(text, pos);
}

gboolean compose_search_string(Compose *compose,
				const gchar *str, gboolean case_sens)
{
	GtkTextView *text = GTK_TEXT_VIEW(compose->text);

	return gtkut_text_view_search_string(text, str, case_sens);
}

gboolean compose_search_string_backward(Compose *compose,
				const gchar *str, gboolean case_sens)
{
	GtkTextView *text = GTK_TEXT_VIEW(compose->text);

	return gtkut_text_view_search_string_backward(text, str, case_sens);
}

/*
 * End of Source.
 */
