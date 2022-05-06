/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2004-2022 the Claws Mail team
 * Copyright (C) 2014-2015 Charles Lehner
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#include "claws-features.h"
#endif

#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gi18n.h>

#include "defs.h"
#include "gtk/gtkutils.h"
#include "gtk/combobox.h"
#include "gtk/manage_window.h"
#include "alertpanel.h"
#include "undo.h"
#include "menu.h"
#include "utils.h"
#include "prefs.h"
#include "prefs_common.h"
#include "account.h"
#include "mainwindow.h"
#include "message_search.h"
#include "managesieve.h"
#include "sieve_manager.h"
#include "sieve_editor.h"

GSList *editors = NULL;

static void sieve_editor_destroy(SieveEditorPage *page);

void sieve_editor_set_position(void *obj, gint pos);
gboolean sieve_editor_search_string(void *obj,
	const gchar *str, gboolean case_sens);
gboolean sieve_editor_search_string_backward(void *obj,
	const gchar *str, gboolean case_sens);
static void sieve_editor_save_cb(GtkAction *action, SieveEditorPage *page);
static void sieve_editor_check_cb(GtkAction *action, SieveEditorPage *page);
static void sieve_editor_changed_cb(GtkTextBuffer *, SieveEditorPage *page);
static void sieve_editor_revert_cb(GtkAction *action, SieveEditorPage *page);
static void sieve_editor_close_cb(GtkAction *action, SieveEditorPage *page);
static void sieve_editor_undo_cb(GtkAction *action, SieveEditorPage *page);
static void sieve_editor_redo_cb(GtkAction *action, SieveEditorPage *page);
static void sieve_editor_cut_cb(GtkAction *action, SieveEditorPage *page);
static void sieve_editor_copy_cb(GtkAction *action, SieveEditorPage *page);
static void sieve_editor_paste_cb(GtkAction *action, SieveEditorPage *page);
static void sieve_editor_allsel_cb(GtkAction *action, SieveEditorPage *page);
static void sieve_editor_find_cb(GtkAction *action, SieveEditorPage *page);
static void sieve_editor_set_modified(SieveEditorPage *page,
		gboolean modified);

static SearchInterface search_interface = {
	.set_position = sieve_editor_set_position,
	.search_string_backward = sieve_editor_search_string_backward,
	.search_string = sieve_editor_search_string,
};

static GtkActionEntry sieve_editor_entries[] =
{
	{"Menu",				NULL, "Menu", NULL, NULL, NULL },
/* menus */
	{"Filter",			NULL, N_("_Filter"), NULL, NULL, NULL  },
	{"Edit",			NULL, N_("_Edit"), NULL, NULL, NULL  },
/* Filter menu */

	{"Filter/Save",		NULL, N_("_Save"), "<control>S", NULL, G_CALLBACK(sieve_editor_save_cb) },
	{"Filter/CheckSyntax",		NULL, N_("Chec_k Syntax"), "<control>K", NULL, G_CALLBACK(sieve_editor_check_cb) },
	{"Filter/Revert",		NULL, N_("Re_vert"), NULL, NULL, G_CALLBACK(sieve_editor_revert_cb) },
	{"Filter/Close",		NULL, N_("_Close"), "<control>W", NULL, G_CALLBACK(sieve_editor_close_cb) },

/* Edit menu */
	{"Edit/Undo",			NULL, N_("_Undo"), "<control>Z", NULL, G_CALLBACK(sieve_editor_undo_cb) },
	{"Edit/Redo",			NULL, N_("_Redo"), "<control>Y", NULL, G_CALLBACK(sieve_editor_redo_cb) },
	/* {"Edit/---",			NULL, "---", NULL, NULL, NULL }, */

	{"Edit/Cut",			NULL, N_("Cu_t"), "<control>X", NULL, G_CALLBACK(sieve_editor_cut_cb) },
	{"Edit/Copy",			NULL, N_("_Copy"), "<control>C", NULL, G_CALLBACK(sieve_editor_copy_cb) },
	{"Edit/Paste",			NULL, N_("_Paste"), "<control>V", NULL, G_CALLBACK(sieve_editor_paste_cb) },

	{"Edit/SelectAll",		NULL, N_("Select _all"), "<control>A", NULL, G_CALLBACK(sieve_editor_allsel_cb) },

	{"Edit/---",			NULL, "---", NULL, NULL, NULL },
	{"Edit/Find",		NULL, N_("_Find"), "<control>F", NULL, G_CALLBACK(sieve_editor_find_cb) },
};


void sieve_editors_close()
{
	if (editors) {
		GSList *list = editors;
		editors = NULL;
		g_slist_free_full(list, (GDestroyNotify)sieve_editor_close);
	}
}

void sieve_editor_append_text(SieveEditorPage *page, gchar *text, gint len)
{
	GtkTextBuffer *buffer;
	GtkTextIter iter;

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(page->text));

	g_signal_handlers_block_by_func(G_OBJECT(buffer),
			 G_CALLBACK(sieve_editor_changed_cb), page);

	undo_block(page->undostruct);
	gtk_text_buffer_get_end_iter(buffer, &iter);
	gtk_text_buffer_insert(buffer, &iter, text, len);
	undo_unblock(page->undostruct);

	g_signal_handlers_unblock_by_func(G_OBJECT(buffer),
			 G_CALLBACK(sieve_editor_changed_cb), page);
}

static gint sieve_editor_get_text(SieveEditorPage *page, gchar **text)
{
	GtkTextBuffer *buffer;
	GtkTextIter start, end;

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(page->text));
	gtk_text_buffer_get_start_iter(buffer, &start);
	gtk_text_buffer_get_end_iter(buffer, &end);
	*text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
	/* return length in bytes */
	return strlen(*text);
}

static void sieve_editor_set_status(SieveEditorPage *page, const gchar *status)
{
	gtk_label_set_text(GTK_LABEL(page->status_text), status);
}

static void sieve_editor_set_status_icon(SieveEditorPage *page, const gchar *img_id)
{
	GtkImage *img = GTK_IMAGE(page->status_icon);
	if (img_id)
		gtk_image_set_from_icon_name(img, img_id, GTK_ICON_SIZE_BUTTON);
	else
		gtk_image_clear(img);
}

static void sieve_editor_append_status(SieveEditorPage *page,
		const gchar *new_status)
{
	GtkLabel *label = GTK_LABEL(page->status_text);
	const gchar *prev_status = gtk_label_get_text(label);
	const gchar *sep = prev_status && prev_status[0] ? "\n" : "";
	gchar *status = g_strconcat(prev_status ? prev_status : "", sep, new_status, NULL);
	gtk_label_set_text(label, status);
	g_free(status);
}

/* Update the status icon and text from a response. */
static void sieve_editor_update_status(SieveEditorPage *page,
		SieveResult *result)
{
	if (result->has_status) {
		/* set status icon */
		sieve_editor_set_status_icon(page,
			result->success ? "dialog-information" : "dialog-error");
		/* clear status text */
		sieve_editor_set_status(page, "");
	}
	if (result->description) {
		/* append to status */
		sieve_editor_append_status(page, result->description);
	}
}

/* Edit Menu */

static void sieve_editor_undo_cb(GtkAction *action, SieveEditorPage *page)
{
	undo_undo(page->undostruct);
}

static void sieve_editor_redo_cb(GtkAction *action, SieveEditorPage *page)
{
	undo_redo(page->undostruct);
}

static void sieve_editor_cut_cb(GtkAction *action, SieveEditorPage *page)
{
	GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(page->text));
	GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
	gtk_text_buffer_cut_clipboard(buf, clipboard, TRUE);
}

static void sieve_editor_copy_cb(GtkAction *action, SieveEditorPage *page)
{
	GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(page->text));
	GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
	gtk_text_buffer_copy_clipboard(buf, clipboard);
}

static void sieve_editor_paste_cb(GtkAction *action, SieveEditorPage *page)
{
	if (!gtk_widget_has_focus(page->text))
		return;

	GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(page->text));
	GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
	gchar *contents = gtk_clipboard_wait_for_text(clipboard);
	GtkTextMark *start_mark = gtk_text_buffer_get_insert(buf);
	GtkTextIter start_iter;

	undo_paste_clipboard(GTK_TEXT_VIEW(page->text), page->undostruct);
	gtk_text_buffer_delete_selection(buf, FALSE, TRUE);

	gtk_text_buffer_get_iter_at_mark(buf, &start_iter, start_mark);
	gtk_text_buffer_insert(buf, &start_iter, contents, strlen(contents));
}


static void sieve_editor_allsel_cb(GtkAction *action, SieveEditorPage *page)
{
	GtkTextIter start, end;
	GtkTextBuffer *buffer;

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(page->text));
	gtk_text_buffer_get_start_iter(buffer, &start);
	gtk_text_buffer_get_end_iter(buffer, &end);
	gtk_text_buffer_select_range(buffer, &start, &end);
}

/* Search */

void sieve_editor_set_position(void *obj, gint pos)
{
	SieveEditorPage *page = (SieveEditorPage *)obj;
	GtkTextView *text = GTK_TEXT_VIEW(page->text);

	gtkut_text_view_set_position(text, pos);
}

gboolean sieve_editor_search_string(void *obj,
	const gchar *str, gboolean case_sens)
{
	SieveEditorPage *page = (SieveEditorPage *)obj;
	GtkTextView *text = GTK_TEXT_VIEW(page->text);

	return gtkut_text_view_search_string(text, str, case_sens);
}

gboolean sieve_editor_search_string_backward(void *obj,
	const gchar *str, gboolean case_sens)
{
	SieveEditorPage *page = (SieveEditorPage *)obj;
	GtkTextView *text = GTK_TEXT_VIEW(page->text);

	return gtkut_text_view_search_string_backward(text, str, case_sens);
}

static void sieve_editor_search(SieveEditorPage *page)
{
	message_search_other(&search_interface, page);
}

/* Actions */

static void got_data_reverting(SieveSession *session, gboolean abort,
		gchar *contents,
		SieveEditorPage *page)
{
	if (abort)
		return;
	if (contents == NULL) {
		/* end of data */
		undo_unblock(page->undostruct);
		gtk_widget_set_sensitive(page->text, TRUE);
		sieve_editor_set_status(page, "");
		sieve_editor_set_modified(page, FALSE);
		return;
	}
	if (contents == (void *)-1) {
		/* error */
		sieve_editor_set_status(page, _("Unable to get script contents"));
		sieve_editor_set_status_icon(page, "dialog-error");
		return;
	}

	if (page->first_line) {
		GtkTextIter start, end;
		GtkTextBuffer *buffer;

		page->first_line = FALSE;

		/* delete previous data */
		buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(page->text));
		gtk_text_buffer_get_start_iter(buffer, &start);
		gtk_text_buffer_get_end_iter(buffer, &end);
		gtk_text_buffer_delete(buffer, &start, &end);

		/* append data */
		gtk_text_buffer_insert(buffer, &end, contents, strlen(contents));
	} else {
		sieve_editor_append_text(page, contents, strlen(contents));
	}
}

static void sieve_editor_revert(SieveEditorPage *page)
{
	undo_block(page->undostruct);
	page->first_line = TRUE;
	gtk_widget_set_sensitive(page->text, FALSE);
	sieve_editor_set_status(page, _("Reverting..."));
	sieve_editor_set_status_icon(page, NULL);
	sieve_session_get_script(page->session, page->script_name,
			(sieve_session_data_cb_fn)got_data_reverting, page);
}

static void sieve_editor_revert_cb(GtkAction *action, SieveEditorPage *page)
{
	if (!page->modified ||
			alertpanel(_("Revert script"),
				_("This script has been modified. Revert the unsaved changes?"),
				NULL, _("_Revert"), NULL, NULL, NULL, _("_Cancel"),
				ALERTFOCUS_FIRST) == G_ALERTDEFAULT)
		sieve_editor_revert(page);
}

static void got_data_saved(SieveSession *session, gboolean abort,
		SieveResult *result, SieveEditorPage *page)
{
	if (abort)
		return;
	if (result->has_status && result->success) {
		sieve_editor_set_modified(page, FALSE);
		if (page->closing) {
			sieve_editor_close(page);
			return;
		}
		/* use nice status message if there are no warnings */
		if (result->code == SIEVE_CODE_NONE) {
			result->description = _("Script saved successfully.");
		}

		if (page->is_new) {
			/* notify manager windows of newly created script */
			page->is_new = FALSE;
			sieve_manager_script_created(session,
					page->script_name);
		}
	}
	sieve_editor_update_status(page, result);
}

static void sieve_editor_save(SieveEditorPage *page)
{
	gchar *text;
	gint len = sieve_editor_get_text(page, &text);
	sieve_editor_set_status(page, _("Saving..."));
	sieve_editor_set_status_icon(page, NULL);
	sieve_session_put_script(page->session, page->script_name, len, text,
			(sieve_session_data_cb_fn)got_data_saved, page);
	g_free(text);
}

static void sieve_editor_save_cb(GtkAction *action, SieveEditorPage *page)
{
	sieve_editor_save(page);
}

static void sieve_editor_find_cb(GtkAction *action, SieveEditorPage *page)
{
	sieve_editor_search(page);
}

static void got_data_checked(SieveSession *session, gboolean abort,
		SieveResult *result, SieveEditorPage *page)
{
	if (abort)
		return;
	sieve_editor_update_status(page, result);
}

static void sieve_editor_check_cb(GtkAction *action, SieveEditorPage *page)
{
	gchar *text;
	gint len = sieve_editor_get_text(page, &text);
	sieve_editor_set_status(page, _("Checking syntax..."));
	sieve_editor_set_status_icon(page, NULL);
	sieve_session_check_script(page->session, len, text,
			(sieve_session_data_cb_fn)got_data_checked, page);
	g_free(text);
}

static void sieve_editor_changed_cb(GtkTextBuffer *textbuf,
		SieveEditorPage *page)
{
	if (!page->modified) {
		sieve_editor_set_modified(page, TRUE);
	}
}

static void sieve_editor_destroy(SieveEditorPage *page)
{
	gtk_widget_destroy(page->window);
	undo_destroy(page->undostruct);
	g_free(page->script_name);
	g_free(page);
}

void sieve_editor_close(SieveEditorPage *page)
{
	editors = g_slist_remove(editors, (gconstpointer)page);
	sieve_sessions_discard_callbacks(page);
	sieve_editor_destroy(page);
}

static gboolean sieve_editor_confirm_close(SieveEditorPage *page)
{
	if (page->modified) {
		switch (alertpanel(_("Save changes"),
				_("This script has been modified. Save the latest changes?"),
				NULL, _("_Discard"), "document-save", _("_Save"), NULL, _("_Cancel"),
				ALERTFOCUS_SECOND)) {
			case G_ALERTDEFAULT:
				return TRUE;
			case G_ALERTALTERNATE:
				page->closing = TRUE;
				sieve_editor_save(page);
				return FALSE;
			default:
				return FALSE;
		}
	}
	return TRUE;
}

static void sieve_editor_close_cb(GtkAction *action, SieveEditorPage *page)
{
	if (sieve_editor_confirm_close(page)) {
		sieve_editor_close(page);
	}
}

static gint sieve_editor_delete_cb(GtkWidget *widget, GdkEventAny *event,
		SieveEditorPage *page)
{
	sieve_editor_close_cb(NULL, page);
	return TRUE;
}

/**
 * sieve_editor_undo_state_changed:
 *
 * Change the sensivity of the menuentries undo and redo
 **/
static void sieve_editor_undo_state_changed(UndoMain *undostruct,
		gint undo_state, gint redo_state, gpointer data)
{
	SieveEditorPage *page = (SieveEditorPage *)data;

	switch (undo_state) {
	case UNDO_STATE_TRUE:
		if (!undostruct->undo_state) {
			undostruct->undo_state = TRUE;
			cm_menu_set_sensitive_full(page->ui_manager, "Menu/Edit/Undo", TRUE);
		}
		break;
	case UNDO_STATE_FALSE:
		if (undostruct->undo_state) {
			undostruct->undo_state = FALSE;
			cm_menu_set_sensitive_full(page->ui_manager, "Menu/Edit/Undo", FALSE);
		}
		break;
	case UNDO_STATE_UNCHANGED:
		break;
	case UNDO_STATE_REFRESH:
		cm_menu_set_sensitive_full(page->ui_manager, "Menu/Edit/Undo", undostruct->undo_state);
		break;
	default:
		g_warning("undo state not recognized");
		break;
	}

	switch (redo_state) {
	case UNDO_STATE_TRUE:
		if (!undostruct->redo_state) {
			undostruct->redo_state = TRUE;
			cm_menu_set_sensitive_full(page->ui_manager, "Menu/Edit/Redo", TRUE);
		}
		break;
	case UNDO_STATE_FALSE:
		if (undostruct->redo_state) {
			undostruct->redo_state = FALSE;
			cm_menu_set_sensitive_full(page->ui_manager, "Menu/Edit/Redo", FALSE);
		}
		break;
	case UNDO_STATE_UNCHANGED:
		break;
	case UNDO_STATE_REFRESH:
		cm_menu_set_sensitive_full(page->ui_manager, "Menu/Edit/Redo", undostruct->redo_state);
		break;
	default:
		g_warning("redo state not recognized");
		break;
	}
}


SieveEditorPage *sieve_editor_new(SieveSession *session, gchar *script_name)
{
	SieveEditorPage *page;
	GtkUIManager *ui_manager;
	UndoMain *undostruct;
	GtkWidget *window;
	GtkWidget *menubar;
	GtkWidget *vbox, *hbox, *hbox1;
	GtkWidget *scrolledwin;
	GtkWidget *text;
	GtkTextBuffer *buffer;
	GtkWidget *check_btn, *save_btn, *close_btn;
	GtkWidget *status_text;
	GtkWidget *status_icon;

	page = g_new0(SieveEditorPage, 1);

	/* window */
	window = gtkut_window_new(GTK_WINDOW_TOPLEVEL, "sieveeditor");
	gtk_window_set_resizable(GTK_WINDOW(window), TRUE);
	MANAGE_WINDOW_SIGNALS_CONNECT (window);
	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(sieve_editor_delete_cb), page);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	ui_manager = gtk_ui_manager_new();
	cm_menu_create_action_group_full(ui_manager,
			"Menu", sieve_editor_entries, G_N_ELEMENTS(sieve_editor_entries),
			page);

	MENUITEM_ADDUI_MANAGER(ui_manager, "/", "Menu", NULL, GTK_UI_MANAGER_MENUBAR)

	MENUITEM_ADDUI_MANAGER(ui_manager, "/Menu", "Filter", "Filter", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Menu", "Edit", "Edit", GTK_UI_MANAGER_MENU)

/* Filter menu */
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Menu/Filter", "Save", "Filter/Save", GTK_UI_MANAGER_MENUITEM)
MENUITEM_ADDUI_MANAGER(ui_manager, "/Menu/Filter", "CheckSyntax", "Filter/CheckSyntax", GTK_UI_MANAGER_MENUITEM)
MENUITEM_ADDUI_MANAGER(ui_manager, "/Menu/Filter", "Revert", "Filter/Revert", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Menu/Filter", "Close", "Filter/Close", GTK_UI_MANAGER_MENUITEM)

/* Edit menu */
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Menu/Edit", "Undo", "Edit/Undo", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Menu/Edit", "Redo", "Edit/Redo", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Menu/Edit", "Separator1", "Edit/---", GTK_UI_MANAGER_SEPARATOR)

	MENUITEM_ADDUI_MANAGER(ui_manager, "/Menu/Edit", "Cut", "Edit/Cut", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Menu/Edit", "Copy", "Edit/Copy", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(ui_manager, "/Menu/Edit", "Paste", "Edit/Paste", GTK_UI_MANAGER_MENUITEM)

	MENUITEM_ADDUI_MANAGER(ui_manager, "/Menu/Edit", "SelectAll", "Edit/SelectAll", GTK_UI_MANAGER_MENUITEM)

	MENUITEM_ADDUI_MANAGER(ui_manager, "/Menu/Edit", "Separator2", "Edit/---", GTK_UI_MANAGER_SEPARATOR)

	MENUITEM_ADDUI_MANAGER(ui_manager, "/Menu/Edit", "Find", "Edit/Find", GTK_UI_MANAGER_MENUITEM)

	menubar = gtk_ui_manager_get_widget(ui_manager, "/Menu");

	gtk_window_add_accel_group(GTK_WINDOW(window), gtk_ui_manager_get_accel_group(ui_manager));
	gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, TRUE, 0);

	cm_menu_set_sensitive_full(ui_manager, "Menu/Edit/Undo", FALSE);
	cm_menu_set_sensitive_full(ui_manager, "Menu/Edit/Redo", FALSE);

	/* text */
	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_set_size_request (scrolledwin, 660, 408);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledwin),
					    GTK_SHADOW_IN);
	gtk_box_pack_start(GTK_BOX(vbox), scrolledwin, TRUE, TRUE, 0);

	text = gtk_text_view_new();
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text), GTK_WRAP_WORD_CHAR);
	gtk_text_view_set_editable(GTK_TEXT_VIEW(text), TRUE);
	gtk_container_add(GTK_CONTAINER(scrolledwin), text);

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text));
	g_signal_connect(G_OBJECT(buffer), "changed",
			 G_CALLBACK(sieve_editor_changed_cb), page);

	/* set text font */
	if (prefs_common_get_prefs()->textfont) {
		PangoFontDescription *font_desc;

		font_desc = pango_font_description_from_string
			(prefs_common_get_prefs()->textfont);
		if (font_desc) {
			gtk_widget_override_font(text, font_desc);
			pango_font_description_free(font_desc);
		}
	}

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_box_pack_end (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (hbox), 8);

	/* status */
	status_icon = gtk_image_new ();
	gtk_box_pack_start (GTK_BOX (hbox), status_icon, FALSE, FALSE, 0);
	status_text = gtk_label_new ("");
	gtk_box_pack_start (GTK_BOX (hbox), status_text, FALSE, FALSE, 0);
	gtk_label_set_justify (GTK_LABEL (status_text), GTK_JUSTIFY_LEFT);

	/* buttons */
	gtkut_stock_with_text_button_set_create(&hbox1,
			&close_btn, _("_Cancel"), _("_Close"),
			&check_btn, _("_OK"), _("Chec_k Syntax"),
			&save_btn, "document-save", _("_Save"));
	gtk_box_pack_end (GTK_BOX (hbox), hbox1, FALSE, FALSE, 0);
	gtk_widget_grab_default (save_btn);
	g_signal_connect (G_OBJECT (close_btn), "clicked",
			  G_CALLBACK (sieve_editor_close_cb), page);
	g_signal_connect (G_OBJECT (check_btn), "clicked",
			  G_CALLBACK (sieve_editor_check_cb), page);
	g_signal_connect (G_OBJECT (save_btn), "clicked",
			  G_CALLBACK (sieve_editor_save_cb), page);

	undostruct = undo_init(text);
	undo_set_change_state_func(undostruct, &sieve_editor_undo_state_changed,
			page);

	page->window = window;
	page->ui_manager = ui_manager;
	page->text = text;
	page->undostruct = undostruct;
	page->session = session;
	page->script_name = script_name;
	page->status_text = status_text;
	page->status_icon = status_icon;

	editors = g_slist_prepend(editors, page);

	sieve_editor_set_modified(page, FALSE);

	return page;
}

SieveEditorPage *sieve_editor_get(SieveSession *session, gchar *script_name)
{
	GSList *item;
	SieveEditorPage *page;
	for (item = editors; item; item = item->next) {
		page = (SieveEditorPage *)item->data;
		if (page->session == session &&
				strcmp(script_name, page->script_name) == 0)
			return page;
	}
	return NULL;
}

void sieve_editor_present(SieveEditorPage *page)
{
	gtk_window_present(GTK_WINDOW(page->window));
}

void sieve_editor_show(SieveEditorPage *page)
{
	gtk_widget_show_all(GTK_WIDGET(page->window));
}

static void sieve_editor_set_modified(SieveEditorPage *page,
		gboolean modified)
{
	gchar *title;

	page->modified = modified;
	cm_menu_set_sensitive_full(page->ui_manager, "Menu/Filter/Revert",
			modified);

	title = g_strdup_printf(_("%s - Sieve Filter%s"), page->script_name,
			modified ? _(" [Edited]") : "");
	gtk_window_set_title (GTK_WINDOW (page->window), title);
	g_free(title);

	if (modified) {
		sieve_editor_set_status(page, "");
		sieve_editor_set_status_icon(page, NULL);
	}
}

static void got_data_loading(SieveSession *session, gboolean aborted,
		gchar *contents, SieveEditorPage *page)
{
	if (aborted)
		return;
	if (contents == NULL) {
		/* end of data */
		sieve_editor_set_status(page, "");
		return;
	}
	if (contents == (void *)-1) {
		/* error */
		if (page->first_line) {
			/* no data. show error in manager window */
			if (page->on_load_error)
				page->on_load_error(session, page->on_load_error_data);
		} else {
			/* partial failure. show error in editor window */
			sieve_editor_set_status(page, _("Unable to get script contents"));
			sieve_editor_set_status_icon(page, "dialog-error");
		}
		return;
	}

	if (page->first_line) {
		page->first_line = FALSE;
		sieve_editor_show(page);
	}
	sieve_editor_append_text(page, contents, strlen(contents));
}

/* load the script for this editor */
void sieve_editor_load(SieveEditorPage *page,
		sieve_session_cb_fn on_load_error, gpointer load_error_data)
{
	page->first_line = TRUE;
	page->on_load_error = on_load_error;
	page->on_load_error_data = load_error_data;
	sieve_editor_set_status(page, _("Loading..."));
	sieve_editor_set_status_icon(page, NULL);
	sieve_session_get_script(page->session, page->script_name,
			(sieve_session_data_cb_fn)got_data_loading, page);
}
