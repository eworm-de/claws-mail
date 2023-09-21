/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2023 the Claws Mail team and Colin Leroy
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
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#include "claws-features.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include <ctype.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "utils.h"
#include "combobox.h"
#include "menu.h"
#include "prefs_common.h"
#include "description_window.h"
#include "matcher.h"
#include "matcher_parser.h"
#include "quicksearch.h"
#include "folderview.h"
#include "folder.h"
#include "prefs_matcher.h"
#include "claws.h"
#include "statusbar.h"
#include "advsearch.h"
#include "alertpanel.h"

struct _QuickSearchRequest
{
	AdvancedSearchType		 type;
	gchar				*matchstring;
};
typedef struct _QuickSearchRequest QuickSearchRequest;

struct _QuickSearch
{
	GtkWidget			*hbox_search;
	GtkWidget			*search_type_combo;
	GtkWidget			*search_string_entry;
	GtkWidget			*search_condition_expression;
	GtkWidget			*search_description;
	GtkWidget			*clear_search;

	gboolean			 active;
	gchar				*search_string;

	QuickSearchRequest		 request;
	QuickSearchExecuteCallback	 callback;
	gpointer			 callback_data;
	gboolean			 running;
	gboolean			 has_focus;
	gboolean			 in_typing;
	guint				 press_timeout_id;

	GList				*normal_search_strings;
	GList				*extended_search_strings;
	
	AdvancedSearch			*asearch;
	gboolean			 want_reexec;
	gboolean			 want_history;
};

static GdkColor qs_active_bgcolor = {
	(gulong)0,
	(gushort)0,
	(gushort)0,
	(gushort)0
};

static GdkColor qs_active_color = {
	(gulong)0,
	(gushort)0,
	(gushort)0,
	(gushort)0
};

static GdkColor qs_error_bgcolor = {
	(gulong)0,
	(gushort)0,
	(gushort)0,
	(gushort)0
};

static GdkColor qs_error_color = {
	(gulong)0,
	(gushort)0,
	(gushort)0,
	(gushort)0
};

typedef enum {
	SEARCH_TYPE_COL_TEXT,
	SEARCH_TYPE_COL_CHECKBOX,
	SEARCH_TYPE_COL_CHECKBOX_ACTIVE,
	SEARCH_TYPE_COL_ACTION,
	NUM_SEARCH_TYPE_COLS
} QuickSearchTypeColumn;

typedef enum {
	QS_MENU_ACTION_SUBJECT,
	QS_MENU_ACTION_FROM,
	QS_MENU_ACTION_TO,
	QS_MENU_ACTION_TAG,
	QS_MENU_ACTION_MIXED,
	QS_MENU_ACTION_EXTENDED,
	QS_MENU_ACTION_SEPARATOR,
	QS_MENU_ACTION_RECURSIVE,
	QS_MENU_ACTION_STICKY,
	QS_MENU_ACTION_TYPEAHEAD,
	QS_MENU_ACTION_RUNONSELECT,
	NUM_QS_MENU_ACTIONS
} QuickSearchMenuActions;

static void search_type_changed_cb(GtkComboBox *combobox,
		gpointer user_data);

void quicksearch_set_on_progress_cb(QuickSearch* search,
		gboolean (*cb)(gpointer data, guint at, guint matched, guint total), gpointer data)
{
	advsearch_set_on_progress_cb(search->asearch, cb, data);
}

static void quicksearch_set_running(QuickSearch *quicksearch, gboolean run);
static void quicksearch_set_matchstring(QuickSearch *quicksearch, const gchar *matchstring);
static void quicksearch_set_active(QuickSearch *quicksearch, gboolean active);
static void quicksearch_set_popdown_strings(QuickSearch *quicksearch);

static void quicksearch_add_to_history(QuickSearch* quicksearch)
{
	gchar* search_string = quicksearch->request.matchstring;

	/* add to history, for extended search add only correct matching rules */
	if (quicksearch->want_history && !quicksearch->in_typing && search_string && strlen(search_string) != 0) {
		switch (prefs_common.summary_quicksearch_type) {
			case ADVANCED_SEARCH_EXTENDED:
				if (advsearch_has_proper_predicate(quicksearch->asearch)) {
					quicksearch->extended_search_strings =
						add_history(quicksearch->extended_search_strings,
								search_string);
					prefs_common.summary_quicksearch_history =
						add_history(prefs_common.summary_quicksearch_history,
								search_string);
				}
				break;
			default:
				quicksearch->normal_search_strings =
					add_history(quicksearch->normal_search_strings,
							search_string);
				prefs_common.summary_quicksearch_history =
					add_history(prefs_common.summary_quicksearch_history,
							search_string);
				break;
		}

		quicksearch_set_popdown_strings(quicksearch);
	}

	quicksearch->want_history = FALSE;
}

static void quicksearch_invoke_execute(QuickSearch *quicksearch, gboolean run_only_if_fast)
{
	if (quicksearch->running) {
		quicksearch->want_reexec = TRUE;
		advsearch_abort(quicksearch->asearch);
		return;
	}

	do {
		gboolean active = quicksearch->request.matchstring != NULL 
				   && g_strcmp0(quicksearch->request.matchstring, "");
		advsearch_set(quicksearch->asearch, quicksearch->request.type,
				quicksearch->request.matchstring);

		if (run_only_if_fast && !advsearch_is_fast(quicksearch->asearch))
			return;

		quicksearch_add_to_history(quicksearch);

		quicksearch_set_active(quicksearch, active);

		quicksearch->want_reexec = FALSE;
		quicksearch_set_running(quicksearch, TRUE);
		if (quicksearch->callback != NULL)
			quicksearch->callback(quicksearch, quicksearch->callback_data);
		quicksearch_set_running(quicksearch, FALSE);
	} while (quicksearch->want_reexec);
}

gboolean quicksearch_run_on_folder(QuickSearch* quicksearch, FolderItem *folderItem, MsgInfoList **result)
{
	if (quicksearch_has_sat_predicate(quicksearch)) {
		gboolean was_running = quicksearch_is_running(quicksearch);
		gboolean searchres;

		if (!was_running)
			quicksearch_set_running(quicksearch, TRUE);

		main_window_cursor_wait(mainwindow_get_mainwindow());
		searchres = advsearch_search_msgs_in_folders(quicksearch->asearch, result, folderItem, FALSE);
		main_window_cursor_normal(mainwindow_get_mainwindow());

		if (!was_running)
			quicksearch_set_running(quicksearch, FALSE);

		if (quicksearch->want_reexec) {
			advsearch_set(quicksearch->asearch, quicksearch->request.type, "");
		}
		return searchres;
	} else
		return FALSE;
}

gboolean quicksearch_is_fast(QuickSearch *quicksearch)
{
	return advsearch_is_fast(quicksearch->asearch);
}

static void quicksearch_set_type(QuickSearch *quicksearch, gint type)
{
	prefs_common_get_prefs()->summary_quicksearch_type = type;
	quicksearch->request.type = type;
}

static gchar *quicksearch_get_text(QuickSearch * quicksearch)
{
	gchar *search_string = gtk_editable_get_chars(GTK_EDITABLE(gtk_bin_get_child(GTK_BIN((quicksearch->search_string_entry)))), 0, -1);

	return search_string;
}

static void quicksearch_set_popdown_strings(QuickSearch *quicksearch)
{
	GtkWidget *search_string_entry = quicksearch->search_string_entry;

	combobox_unset_popdown_strings(GTK_COMBO_BOX_TEXT(search_string_entry));	
	if (prefs_common.summary_quicksearch_type == ADVANCED_SEARCH_EXTENDED)
		combobox_set_popdown_strings(GTK_COMBO_BOX_TEXT(search_string_entry),
			quicksearch->extended_search_strings);	
	else
		combobox_set_popdown_strings(GTK_COMBO_BOX_TEXT(search_string_entry),
			quicksearch->normal_search_strings);
}

static void update_extended_buttons (QuickSearch *quicksearch)
{
	GtkWidget *expr_btn = quicksearch->search_condition_expression;
	GtkWidget *ext_btn = quicksearch->search_description;

	cm_return_if_fail(expr_btn != NULL);
	cm_return_if_fail(ext_btn != NULL);

	if (prefs_common.summary_quicksearch_type == ADVANCED_SEARCH_EXTENDED) {
		gtk_widget_show(expr_btn);
		gtk_widget_show(ext_btn);
	} else {
		gtk_widget_hide(expr_btn);
		gtk_widget_hide(ext_btn);
	}
}

static gboolean searchbar_focus_evt_in(GtkWidget *widget, GdkEventFocus *event,
			      	  QuickSearch *qs)
{
	qs->has_focus = TRUE;
	return FALSE;
}

static gboolean searchbar_focus_evt_out(GtkWidget *widget, GdkEventFocus *event,
			      	  QuickSearch *qs)
{
	qs->has_focus = FALSE;
	qs->in_typing = FALSE;
	return FALSE;
}

gboolean quicksearch_has_focus(QuickSearch *quicksearch)
{
	return quicksearch->has_focus;
}

static void searchbar_run(QuickSearch *quicksearch, gboolean run_only_if_fast)
{
	gchar *search_string = quicksearch_get_text(quicksearch);
	quicksearch_set_matchstring(quicksearch, search_string);
	g_free(search_string);

	quicksearch->want_history = TRUE;

	quicksearch_invoke_execute(quicksearch, run_only_if_fast);
}

static int searchbar_changed_timeout(void *data)
{
	QuickSearch *qs = (QuickSearch *)data;
	if (qs && prefs_common.summary_quicksearch_dynamic) {
		qs->in_typing = TRUE;
		searchbar_run(qs, TRUE);
	}
	return FALSE;
}

static void searchbar_changed_cb(GtkWidget *widget, QuickSearch *qs)
{
	if (!qs->has_focus && prefs_common.summary_quicksearch_autorun) {
		gtk_widget_grab_focus(qs->search_string_entry);
		searchbar_run(qs, TRUE);
		return;
	}

	if (prefs_common.summary_quicksearch_dynamic) {
		if (qs->press_timeout_id != 0) {
			g_source_remove(qs->press_timeout_id);
		}
		qs->press_timeout_id = g_timeout_add(prefs_common.qs_press_timeout,
				searchbar_changed_timeout, qs);
	}

	if (!qs->has_focus)
		gtk_widget_grab_focus(qs->search_string_entry);
}

static gboolean searchbar_pressed(GtkWidget *widget, GdkEventKey *event,
			      	  QuickSearch *quicksearch)
{
	if (event != NULL && (event->keyval == GDK_KEY_ISO_Left_Tab)) {
		/* Shift+Tab moves focus "back" */
		gtk_widget_grab_focus(quicksearch->search_type_combo);
		return TRUE;
	}
	if (event != NULL && (event->keyval == GDK_KEY_Tab)) {
		/* Just Tab moves focus "forwards" */
		gtk_widget_grab_focus(quicksearch->clear_search);
		return TRUE;
	}

	if (event != NULL && (event->keyval == GDK_KEY_Escape)) {
		gchar *str;

		quicksearch->in_typing = FALSE;

		str = quicksearch_get_text(quicksearch);
		cm_return_val_if_fail(str != NULL, TRUE);

		/* If the string entry is empty -> hide quicksearch bar. If not -> empty it */
		if (!*str) {
			summaryview_activate_quicksearch(
				mainwindow_get_mainwindow()->summaryview, 
				FALSE);
		} else {
			quicksearch_set(quicksearch, prefs_common.summary_quicksearch_type, "");
			gtk_widget_grab_focus(
					mainwindow_get_mainwindow()->summaryview->ctree);
		}
		g_free(str);
		return TRUE;
	}

	if (event != NULL && (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter)) {
		if (quicksearch->press_timeout_id != 0) {
			g_source_remove(quicksearch->press_timeout_id);
			quicksearch->press_timeout_id = 0;
		}
		quicksearch->in_typing = FALSE;
		/* add expression to history list and exec quicksearch */
		searchbar_run(quicksearch, FALSE);

		g_signal_stop_emission_by_name(G_OBJECT(widget), "key_press_event");
		return TRUE;
	}

	if (event && (event->keyval == GDK_KEY_Down || event->keyval == GDK_KEY_Up)) {
		combobox_set_value_from_arrow_key(
				GTK_COMBO_BOX(quicksearch->search_string_entry),
				event->keyval);
		return TRUE;
	}

	return FALSE;
}

/*
 * Strings describing how to use Extended Search
 *
 * When adding new lines, remember to put 2 strings for each line
 */
static gchar *search_descr_strings[] = {
	"a",	 N_("all messages"),
	"ag #",  N_("messages whose age is greater than # days"),
	"al #",  N_("messages whose age is less than # days"),
	"agh #",  N_("messages whose age is greater than # hours"),
	"alh #",  N_("messages whose age is less than # hours"),
	"b S",	 N_("messages which contain S in the message body"),
	"B S",	 N_("messages which contain S in the whole message"),
	"c S",	 N_("messages carbon-copied to S"),
	"C S",	 N_("message is either To: or Cc: to S"),
	"D",	 N_("deleted messages"), /** how I can filter deleted messages **/
	"da \"YYYY-MM-dd HH:mm:ss\"",  N_("messages whose date is after requested date "
					  "(time is optional)"),
	"db \"YYYY-MM-dd HH:mm:ss\"",  N_("messages whose date is before requested date "
					  "(time is optional)"),
	"e S",	 N_("messages which contain S in the Sender field"),
	"E S",	 N_("true if execute \"S\" succeeds"),
	"f S",	 N_("messages originating from user S"),
	"F",	 N_("forwarded messages"),
	"h S",	 N_("messages which contain S in any header name or value"),
	"H S",	 N_("messages which contain S in the value of any header"),
	"ha",	 N_("messages which have attachments"),
	"i S",	 N_("messages which contain S in Message-ID header"),
	"I S",	 N_("messages which contain S in In-Reply-To header"),
	"k #",	 N_("messages which are marked with color #"),
	"L",	 N_("locked messages"),
	"n S",	 N_("messages which are in newsgroup S"),
	"N",	 N_("new messages"),
	"O",	 N_("old messages"),
	"p",	 N_("incomplete messages (not entirely downloaded)"),
	"r",	 N_("messages which you have replied to"),
	"R",	 N_("read messages"),
	"s S",	 N_("messages which contain S in subject"),
	"se #",  N_("messages whose score is equal to # points"),
	"sg #",  N_("messages whose score is greater than # points"),
	"sl #",  N_("messages whose score is lower than # points"),
	"Se #",  N_("messages whose size is equal to # bytes"),
	"Sg #",  N_("messages whose size is greater than # bytes"),
	"Ss #",  N_("messages whose size is smaller than # bytes"),
	"t S",	 N_("messages which have been sent to S"),
	"tg S",  N_("messages which tags contain S"),
	"tagged",N_("messages which have tag(s)"),
	"T",	 N_("marked messages"),
	"U",	 N_("unread messages"),
	"v H V", N_("messages which contain V in header H"),
	"x S",	 N_("messages which contain S in References header"),
	"X \"cmd args\"", N_("messages returning 0 when passed to command - %F is message file"),
	"",	 "" ,
	"&amp;",	 N_("logical AND operator"),
	"|",	 N_("logical OR operator"),
	"! or ~",	N_("logical NOT operator"),
	"%",	 N_("case sensitive search"),
	"&#x00023;", N_("match using regular expressions instead of substring search"),
	"",	 "" ,
	" ",	 N_("all filtering expressions are allowed, but cannot be mixed "
	            "through logical operators with the expressions above"),
	NULL,	 NULL
};

static DescriptionWindow search_descr = {
	NULL,
	NULL,
	FALSE,
	2,
	N_("Extended Search"),
	N_("Extended Search allows the user to define criteria that messages must "
           "have in order to match and be displayed in the message list.\n"
	   "The following symbols can be used:"),
	search_descr_strings
};

static void search_description_cb(GtkWidget *widget)
{
	search_descr.parent = gtkut_window_new(GTK_WINDOW_TOPLEVEL, "description_window");
	description_window_create(&search_descr);
};

static gboolean clear_search_cb(GtkMenuItem *widget, gpointer data)
{
	QuickSearch *quicksearch = (QuickSearch *)data;

	quicksearch_set(quicksearch, prefs_common.summary_quicksearch_type, "");

	return TRUE;
};

static void search_condition_expr_done(MatcherList * matchers)
{
	gchar *str;

	cm_return_if_fail(
			mainwindow_get_mainwindow()->summaryview->quicksearch != NULL);

	if (matchers == NULL)
		return;

	str = matcherlist_to_string(matchers);

	if (str != NULL) {
		quicksearch_set(mainwindow_get_mainwindow()->summaryview->quicksearch,
				prefs_common.summary_quicksearch_type, str);
		g_free(str);

		/* add expression to history list and exec quicksearch */
		searchbar_run(mainwindow_get_mainwindow()->summaryview->quicksearch, FALSE);
	}
}

static gboolean search_condition_expr(GtkMenuItem *widget, gpointer data)
{
	gchar * cond_str;
	MatcherList * matchers = NULL;
	
	cm_return_val_if_fail(
			mainwindow_get_mainwindow()->summaryview->quicksearch != NULL,
			FALSE);

	/* re-use the current quicksearch value, expanding it so it also works
	 * with extended symbols */
	cond_str = quicksearch_get_text(mainwindow_get_mainwindow()->summaryview->quicksearch);

	if (*cond_str != '\0') {
		gchar *newstr = advsearch_expand_search_string(cond_str);

		if (newstr && newstr[0] != '\0')
			matchers = matcher_parser_get_cond(newstr, FALSE);
		g_free(newstr);
	}

	prefs_matcher_open(matchers, search_condition_expr_done);

	if (matchers != NULL)
		matcherlist_free(matchers);

	g_free(cond_str);

	return TRUE;
};

static void quicksearch_set_button(GtkButton *button, const gchar *icon, const gchar *text)
{
	GList *children = gtk_container_get_children(GTK_CONTAINER(button));
	GList *cur;
	GtkWidget *box;
	gboolean icon_visible;

	g_object_get(gtk_settings_get_default(), 
					 "gtk-button-images", &icon_visible, 
					 NULL);

	for (cur = children; cur; cur = cur->next)
		gtk_container_remove(GTK_CONTAINER(button), GTK_WIDGET(cur->data));
	
	g_list_free(children);
	box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	
	gtk_container_add(GTK_CONTAINER(button), box);
	if (icon_visible || !text || !*text || icon != NULL)
		gtk_box_pack_start(GTK_BOX(box), gtk_image_new_from_icon_name(icon, 
			GTK_ICON_SIZE_BUTTON), FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box), gtk_label_new_with_mnemonic(text), FALSE, FALSE, 0);
	gtk_widget_show_all(box);
}

static gboolean search_type_combo_separator_func(GtkTreeModel *model,
		GtkTreeIter *iter, gpointer data)
{
	gchar *txt = NULL;

	cm_return_val_if_fail(model != NULL, FALSE);

	gtk_tree_model_get(model, iter, 0, &txt, -1);

	if (txt == NULL)
		return TRUE;

	g_free(txt);
	return FALSE;
}

static void quicksearch_error(gpointer data)
{
	alertpanel_error(_("Something went wrong during search. Please check your logs."));
}

static void select_correct_combobox_menuitem(QuickSearch *quicksearch)
{
	gint active_menuitem = 0;
	gint active_type;
	GtkWidget *combobox = quicksearch->search_type_combo;

	/* Figure out which menuitem to set as active. QS_MENU_ACTION_ aliases
	 * are in the same order as order of their menu items, so we can
	 * use them as index for gtk_combo_box_set_active().
	 *
	 * However, ADVANCED_SEARCH_ aliases are in a different order,
	 * and even if they weren't, we do not want to depend on them
	 * always being in correct order, so we have to map them to
	 * our QS_MENU_ACTION_ aliases manually. */
	active_type = prefs_common_get_prefs()->summary_quicksearch_type;
	switch (active_type) {
		case ADVANCED_SEARCH_SUBJECT:
			active_menuitem = QS_MENU_ACTION_SUBJECT;
			break;
		case ADVANCED_SEARCH_FROM:
			active_menuitem = QS_MENU_ACTION_FROM;
			break;
		case ADVANCED_SEARCH_TO:
			active_menuitem = QS_MENU_ACTION_TO;
			break;
		case ADVANCED_SEARCH_TAG:
			active_menuitem = QS_MENU_ACTION_TAG;
			break;
		case ADVANCED_SEARCH_MIXED:
			active_menuitem = QS_MENU_ACTION_MIXED;
			break;
		case ADVANCED_SEARCH_EXTENDED:
			active_menuitem = QS_MENU_ACTION_EXTENDED;
			break;
		default:
			g_warning("unhandled advsearch type %d in quicksearch", active_type);
			break;
	}

	/* We just want to quietly make the correct menuitem active
	 * without triggering handlers of the combobox "changed" signal,
	 * so we temporarily block handling of that signal. */
	g_signal_handlers_block_matched(G_OBJECT(combobox),
			G_SIGNAL_MATCH_FUNC, 0, 0, 0,
			(gpointer)search_type_changed_cb,
			(gpointer)quicksearch);
	gtk_combo_box_set_active(GTK_COMBO_BOX(combobox), active_menuitem);
	g_signal_handlers_unblock_matched(G_OBJECT(combobox),
			G_SIGNAL_MATCH_FUNC, 0, 0, 0,
			(gpointer)search_type_changed_cb,
			(gpointer)quicksearch);
}

static gboolean set_search_type_checkboxes_func(GtkTreeModel *model,
		GtkTreePath *path,
		GtkTreeIter *iter,
		gpointer data)
{
	gboolean has_checkbox;
	gint action;
	gboolean cur_active;
	gboolean active = -1;
	PrefsCommon *prefs = prefs_common_get_prefs();

	gtk_tree_model_get(model, iter,
			SEARCH_TYPE_COL_CHECKBOX, &has_checkbox,
			SEARCH_TYPE_COL_CHECKBOX_ACTIVE, &cur_active,
			SEARCH_TYPE_COL_ACTION, &action,
			-1);

	if (!has_checkbox)
		return FALSE;

	prefs = prefs_common_get_prefs();
	switch (action) {
		case QS_MENU_ACTION_RECURSIVE:
			active = prefs->summary_quicksearch_recurse;
			break;
		case QS_MENU_ACTION_STICKY:
			active = prefs->summary_quicksearch_sticky;
			break;
		case QS_MENU_ACTION_TYPEAHEAD:
			active = prefs->summary_quicksearch_dynamic;
			break;
		case QS_MENU_ACTION_RUNONSELECT:
			active = prefs->summary_quicksearch_autorun;
			break;
	}

	if (active != cur_active) {
		gtk_list_store_set(GTK_LIST_STORE(model), iter,
				SEARCH_TYPE_COL_CHECKBOX_ACTIVE, active,
				-1);
	}

	return FALSE;
}

/* A function to make status of the toggleable menu items
 * in search type combobox reflect configured prefs */
static void set_search_type_checkboxes(GtkComboBox *combobox)
{
	GtkTreeModel *model;

	cm_return_if_fail(combobox != NULL);

	model = gtk_combo_box_get_model(GTK_COMBO_BOX(combobox));

	if (model == NULL)
		return;

	gtk_tree_model_foreach(model, set_search_type_checkboxes_func, NULL);
}

static void search_type_changed_cb(GtkComboBox *combobox,
		gpointer user_data)
{
	QuickSearch *quicksearch = (QuickSearch *)user_data;
	GtkTreeIter iter;
	GtkTreeModel *model;
	gboolean has_checkbox, checkbox_active;
	gint action;
	PrefsCommon *prefs = prefs_common_get_prefs();

	cm_return_if_fail(quicksearch != NULL);
	cm_return_if_fail(gtk_combo_box_get_active_iter(combobox, &iter));

	model = gtk_combo_box_get_model(combobox);

	gtk_tree_model_get(model, &iter,
			SEARCH_TYPE_COL_CHECKBOX, &has_checkbox,
			SEARCH_TYPE_COL_CHECKBOX_ACTIVE, &checkbox_active,
			SEARCH_TYPE_COL_ACTION, &action,
			-1);

	/* React based on which menuitem was selected */
	switch (action) {
		case QS_MENU_ACTION_SUBJECT:
			quicksearch_set_type(quicksearch, ADVANCED_SEARCH_SUBJECT);
			break;
		case QS_MENU_ACTION_FROM:
			quicksearch_set_type(quicksearch, ADVANCED_SEARCH_FROM);
			break;
		case QS_MENU_ACTION_TO:
			quicksearch_set_type(quicksearch, ADVANCED_SEARCH_TO);
			break;
		case QS_MENU_ACTION_TAG:
			quicksearch_set_type(quicksearch, ADVANCED_SEARCH_TAG);
			break;
		case QS_MENU_ACTION_MIXED:
			quicksearch_set_type(quicksearch, ADVANCED_SEARCH_MIXED);
			break;
		case QS_MENU_ACTION_EXTENDED:
			quicksearch_set_type(quicksearch, ADVANCED_SEARCH_EXTENDED);
			break;
		case QS_MENU_ACTION_RECURSIVE:
			prefs->summary_quicksearch_recurse = !checkbox_active;
			break;
		case QS_MENU_ACTION_STICKY:
			prefs->summary_quicksearch_sticky = !checkbox_active;
			break;
		case QS_MENU_ACTION_TYPEAHEAD:
			prefs->summary_quicksearch_dynamic = !checkbox_active;
			break;
		case QS_MENU_ACTION_RUNONSELECT:
			prefs->summary_quicksearch_autorun = !checkbox_active;
			break;
	}
	update_extended_buttons(quicksearch);
	/* If one of the toggleable items has been activated, there's
	 * work to be done */
	if (has_checkbox) {
		/* TYPEAHEAD is mutually exclusive with RUNONSELECT */
		if (action == QS_MENU_ACTION_TYPEAHEAD && !checkbox_active)
			prefs->summary_quicksearch_autorun = FALSE;

		/* RUNONSELECT is mutually exclusive with TYPEAHEAD */
		if (action == QS_MENU_ACTION_RUNONSELECT && !checkbox_active)
			prefs->summary_quicksearch_dynamic = FALSE;

		/* Update state of the toggleable menu items */
		set_search_type_checkboxes(combobox);

		/* Activate one of the search types' menu entries, so that
		 * the current search type is visible on a closed combobox */
		select_correct_combobox_menuitem(quicksearch);
	}

	/* update history list */
	quicksearch_set_popdown_strings(quicksearch);

	/* Update search results */
	quicksearch_invoke_execute(quicksearch, FALSE);

	/* Give focus to the text entry */
	gtk_widget_grab_focus(quicksearch->search_string_entry);
}

QuickSearch *quicksearch_new()
{
	QuickSearch *quicksearch;

	GtkWidget *hbox_search;
	GtkWidget *search_type_combo;
	GtkWidget *search_string_entry;
	GtkWidget *search_hbox;
	GtkWidget *search_description;
	GtkWidget *clear_search;
	GtkWidget *search_condition_expression;
	GtkWidget *vbox;
	GtkListStore *menu;
	GtkCellRenderer *renderer;
	GtkTreeIter iter;

	quicksearch = g_new0(QuickSearch, 1);

	quicksearch->asearch = advsearch_new();
	advsearch_set_on_error_cb(quicksearch->asearch, quicksearch_error, NULL);

	/* init. values initally found in quicksearch_new().
	   There's no need to init. all pointers to NULL since we use g_new0
	 */
	quicksearch->active = FALSE;
	quicksearch->running = FALSE;
	quicksearch->in_typing = FALSE;
	quicksearch->press_timeout_id = 0;
	quicksearch->normal_search_strings = NULL;
	quicksearch->extended_search_strings = NULL;

	/* quick search */
	hbox_search = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

  menu = gtk_list_store_new(4,
      G_TYPE_STRING,
      G_TYPE_BOOLEAN,
			G_TYPE_BOOLEAN,
      G_TYPE_INT);

  search_type_combo = gtk_combo_box_new_with_model(GTK_TREE_MODEL(menu));
	gtk_widget_set_focus_on_click(GTK_WIDGET(search_type_combo), FALSE);
	gtk_combo_box_set_row_separator_func(GTK_COMBO_BOX(search_type_combo),
			(GtkTreeViewRowSeparatorFunc)search_type_combo_separator_func,
			NULL, NULL);

  renderer = gtk_cell_renderer_toggle_new();
  gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(search_type_combo), renderer, TRUE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(search_type_combo),
			renderer,
			"visible", SEARCH_TYPE_COL_CHECKBOX,
			"active", SEARCH_TYPE_COL_CHECKBOX_ACTIVE,
			NULL);
  renderer = gtk_cell_renderer_text_new();
  gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(search_type_combo), renderer, TRUE);
	gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(search_type_combo),
			renderer, "text", SEARCH_TYPE_COL_TEXT, NULL);

	gtk_box_pack_start(GTK_BOX(hbox_search), search_type_combo, FALSE, FALSE, 0);

	gtk_list_store_append(menu, &iter);
	gtk_list_store_set(menu, &iter,
			SEARCH_TYPE_COL_TEXT, _("Subject"),
			SEARCH_TYPE_COL_CHECKBOX, FALSE,
			SEARCH_TYPE_COL_ACTION, QS_MENU_ACTION_SUBJECT,
			-1);
	gtk_list_store_append(menu, &iter);
	gtk_list_store_set(menu, &iter,
			SEARCH_TYPE_COL_TEXT, _("From"),
			SEARCH_TYPE_COL_CHECKBOX, FALSE,
			SEARCH_TYPE_COL_ACTION, QS_MENU_ACTION_FROM,
			-1);
	gtk_list_store_append(menu, &iter);
	gtk_list_store_set(menu, &iter,
			SEARCH_TYPE_COL_TEXT, _("To"),
			SEARCH_TYPE_COL_CHECKBOX, FALSE,
			SEARCH_TYPE_COL_ACTION, QS_MENU_ACTION_TO,
			-1);
	gtk_list_store_append(menu, &iter);
	gtk_list_store_set(menu, &iter,
			SEARCH_TYPE_COL_TEXT, _("Tag"),
			SEARCH_TYPE_COL_CHECKBOX, FALSE,
			SEARCH_TYPE_COL_ACTION, QS_MENU_ACTION_TAG,
			-1);
	gtk_list_store_append(menu, &iter);
	gtk_list_store_set(menu, &iter,
			SEARCH_TYPE_COL_TEXT, _("From/To/Cc/Subject/Tag"),
			SEARCH_TYPE_COL_CHECKBOX, FALSE,
			SEARCH_TYPE_COL_ACTION, QS_MENU_ACTION_MIXED,
			-1);
	gtk_list_store_append(menu, &iter);
	gtk_list_store_set(menu, &iter,
			SEARCH_TYPE_COL_TEXT, _("Extended"),
			SEARCH_TYPE_COL_CHECKBOX, FALSE,
			SEARCH_TYPE_COL_ACTION, QS_MENU_ACTION_EXTENDED,
			-1);
	gtk_list_store_append(menu, &iter); /* Separator */
	gtk_list_store_set(menu, &iter,
			SEARCH_TYPE_COL_TEXT, NULL,
			SEARCH_TYPE_COL_CHECKBOX, FALSE,
			SEARCH_TYPE_COL_ACTION, QS_MENU_ACTION_SEPARATOR,
			-1);
	gtk_list_store_append(menu, &iter);
	gtk_list_store_set(menu, &iter,
			SEARCH_TYPE_COL_TEXT, _("Recursive"),
			SEARCH_TYPE_COL_CHECKBOX, TRUE,
			SEARCH_TYPE_COL_ACTION, QS_MENU_ACTION_RECURSIVE,
			-1);
	gtk_list_store_append(menu, &iter);
	gtk_list_store_set(menu, &iter,
			SEARCH_TYPE_COL_TEXT, _("Sticky"),
			SEARCH_TYPE_COL_CHECKBOX, TRUE,
			SEARCH_TYPE_COL_ACTION, QS_MENU_ACTION_STICKY,
			-1);
	gtk_list_store_append(menu, &iter);
	gtk_list_store_set(menu, &iter,
			SEARCH_TYPE_COL_TEXT, _("Type-ahead"),
			SEARCH_TYPE_COL_CHECKBOX, TRUE,
			SEARCH_TYPE_COL_ACTION, QS_MENU_ACTION_TYPEAHEAD,
			-1);
	gtk_list_store_append(menu, &iter);
	gtk_list_store_set(menu, &iter,
			SEARCH_TYPE_COL_TEXT, _("Run on select"),
			SEARCH_TYPE_COL_CHECKBOX, TRUE,
			SEARCH_TYPE_COL_ACTION, QS_MENU_ACTION_RUNONSELECT,
			-1);

	g_signal_connect(G_OBJECT(search_type_combo), "changed",
			G_CALLBACK(search_type_changed_cb), quicksearch);

	gtk_widget_show(search_type_combo);

	search_string_entry = gtk_combo_box_text_new_with_entry ();
	gtk_combo_box_set_active(GTK_COMBO_BOX(search_string_entry), -1);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_pack_start(GTK_BOX(vbox), search_string_entry, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox_search), vbox, TRUE, TRUE, 4);

	gtk_widget_show(vbox);
	gtk_widget_show(search_string_entry);

	search_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
	clear_search = gtkut_stock_button("edit-clear", _("C_lear"));
	gtk_box_pack_start(GTK_BOX(search_hbox), clear_search,
			   FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(clear_search), "clicked",
			 G_CALLBACK(clear_search_cb), quicksearch);
	CLAWS_SET_TIP(clear_search,
			     _("Clear the current search"));
	gtk_widget_show(clear_search);

	search_condition_expression = gtk_button_new_with_mnemonic("_Edit");
	gtk_box_pack_start(GTK_BOX(search_hbox), search_condition_expression,
			   FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT (search_condition_expression), "clicked",
			 G_CALLBACK(search_condition_expr),
			 quicksearch);
	CLAWS_SET_TIP(search_condition_expression,
			     _("Edit search criteria"));
	gtk_widget_show(search_condition_expression);

	search_description = gtkut_stock_button("dialog-information", _("_Information"));
	gtk_box_pack_start(GTK_BOX(search_hbox), search_description,
			   FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(search_description), "clicked",
			 G_CALLBACK(search_description_cb), NULL);
	CLAWS_SET_TIP(search_description,
			     _("Information about extended symbols"));
	gtk_widget_show(search_description);

	gtk_box_pack_start(GTK_BOX(hbox_search), search_hbox, FALSE, FALSE, 2);
	gtk_widget_show(search_hbox);

	g_signal_connect(G_OBJECT(gtk_bin_get_child(GTK_BIN((search_string_entry)))),
			   "key_press_event",
			   G_CALLBACK(searchbar_pressed),
			   quicksearch);

	g_signal_connect(G_OBJECT(gtk_bin_get_child(GTK_BIN((search_string_entry)))),
			 "changed",
			 G_CALLBACK(searchbar_changed_cb),
			 quicksearch);

	g_signal_connect(G_OBJECT(gtk_bin_get_child(GTK_BIN((search_string_entry)))),
			 "focus_in_event",
			 G_CALLBACK(searchbar_focus_evt_in),
			 quicksearch);
	g_signal_connect(G_OBJECT(gtk_bin_get_child(GTK_BIN((search_string_entry)))),
			 "focus_out_event",
			 G_CALLBACK(searchbar_focus_evt_out),
			 quicksearch);

	quicksearch->hbox_search = hbox_search;
	quicksearch->search_type_combo = search_type_combo;
	quicksearch->search_string_entry = search_string_entry;
	quicksearch->search_condition_expression = search_condition_expression;
	quicksearch->search_description = search_description;
	quicksearch->active = FALSE;
	quicksearch->running = FALSE;
	quicksearch->clear_search = clear_search;
	quicksearch->in_typing = FALSE;
	quicksearch->press_timeout_id = 0;
	quicksearch->normal_search_strings = NULL;
	quicksearch->extended_search_strings = NULL;

	quicksearch_set_button(GTK_BUTTON(quicksearch->search_description), "dialog-information", _("_Information"));
	quicksearch_set_button(GTK_BUTTON(quicksearch->search_condition_expression), NULL, _("E_dit"));
	quicksearch_set_button(GTK_BUTTON(quicksearch->clear_search), "edit-clear", _("C_lear"));
	
	update_extended_buttons(quicksearch);

	GTKUT_GDKRGBA_TO_GDKCOLOR(prefs_common.color[COL_QS_ACTIVE_BG],
					   qs_active_bgcolor);
	GTKUT_GDKRGBA_TO_GDKCOLOR(prefs_common.color[COL_QS_ACTIVE],
					   qs_active_color);
	GTKUT_GDKRGBA_TO_GDKCOLOR(prefs_common.color[COL_QS_ERROR_BG],
					   qs_error_bgcolor);
	GTKUT_GDKRGBA_TO_GDKCOLOR(prefs_common.color[COL_QS_ERROR],
					   qs_error_color);

	/* Update state of the search type combobox to reflect
	 * current quicksearch prefs */
	set_search_type_checkboxes(GTK_COMBO_BOX(search_type_combo));
	select_correct_combobox_menuitem(quicksearch);

	return quicksearch;
}

void quicksearch_relayout(QuickSearch *quicksearch)
{
	switch (prefs_common.layout_mode) {
	case NORMAL_LAYOUT:
	case WIDE_LAYOUT:
	case WIDE_MSGLIST_LAYOUT:
		quicksearch_set_button(GTK_BUTTON(quicksearch->search_description), "dialog-information", _("_Information"));
		quicksearch_set_button(GTK_BUTTON(quicksearch->search_condition_expression), NULL, _("E_dit"));
		quicksearch_set_button(GTK_BUTTON(quicksearch->clear_search), "edit-clear", _("C_lear"));
		break;
	case SMALL_LAYOUT:
	case VERTICAL_LAYOUT:
		quicksearch_set_button(GTK_BUTTON(quicksearch->search_description), "dialog-information", "");
		quicksearch_set_button(GTK_BUTTON(quicksearch->search_condition_expression), NULL, _("E_dit"));
		quicksearch_set_button(GTK_BUTTON(quicksearch->clear_search), "edit-clear", "");
		break;
	}
}

GtkWidget *quicksearch_get_widget(QuickSearch *quicksearch)
{
	return quicksearch->hbox_search;
}

GtkWidget *quicksearch_get_entry(QuickSearch *quicksearch)
{
	return gtk_bin_get_child(GTK_BIN(quicksearch->search_string_entry));
}

void quicksearch_show(QuickSearch *quicksearch)
{
	gint active_type;
	MainWindow *mainwin = mainwindow_get_mainwindow();
	gtk_widget_show(quicksearch->hbox_search);
	update_extended_buttons(quicksearch);
	gtk_widget_grab_focus(quicksearch->search_string_entry);

	if (!mainwin || !mainwin->summaryview) {
		return;
	}
	
	active_type = prefs_common_get_prefs()->summary_quicksearch_type;
	quicksearch_set_type(quicksearch, active_type);
}

void quicksearch_hide(QuickSearch *quicksearch)
{
	if (quicksearch_has_sat_predicate(quicksearch)) {
		quicksearch_set(quicksearch, prefs_common.summary_quicksearch_type, "");
		quicksearch_set_active(quicksearch, FALSE);
	}
	gtk_widget_hide(quicksearch->hbox_search);
}

/*
 *\brief	Sets the matchstring.
 *
 *\param	quicksearch quicksearch to set
 *\param	matchstring the match string; it is duplicated, not stored
 */
static void quicksearch_set_matchstring(QuickSearch *quicksearch,
					const gchar *matchstring)
{
	g_free(quicksearch->request.matchstring);
	quicksearch->request.matchstring = g_strdup(matchstring);
}

void quicksearch_set(QuickSearch *quicksearch, AdvancedSearchType type, const gchar *matchstring)
{
	quicksearch_set_type(quicksearch, type);
	select_correct_combobox_menuitem(quicksearch);

	if (!matchstring || !(*matchstring))
		quicksearch->in_typing = FALSE;

	quicksearch_set_matchstring(quicksearch, matchstring);
		
	g_signal_handlers_block_by_func(G_OBJECT(gtk_bin_get_child(GTK_BIN((quicksearch->search_string_entry)))),
					G_CALLBACK(searchbar_changed_cb), quicksearch);
	gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN((quicksearch->search_string_entry)))),
			   matchstring);
	g_signal_handlers_unblock_by_func(G_OBJECT(gtk_bin_get_child(GTK_BIN((quicksearch->search_string_entry)))),
					  G_CALLBACK(searchbar_changed_cb), quicksearch);

	prefs_common.summary_quicksearch_type = type;

	quicksearch_invoke_execute(quicksearch, FALSE);
}

gboolean quicksearch_has_sat_predicate(QuickSearch *quicksearch)
{
	return quicksearch->active && advsearch_has_proper_predicate(quicksearch->asearch);
}

static void quicksearch_set_active(QuickSearch *quicksearch, gboolean active)
{
	gboolean error = FALSE;

	quicksearch->active = active;

	if (active && 
		(prefs_common.summary_quicksearch_type == ADVANCED_SEARCH_EXTENDED
		 && !advsearch_has_proper_predicate(quicksearch->asearch)))
		error = TRUE;

	if (active) {
		gtk_widget_modify_base(
			gtk_bin_get_child(GTK_BIN((quicksearch->search_string_entry))),
			GTK_STATE_NORMAL, error ? &qs_error_bgcolor : &qs_active_bgcolor);
		gtk_widget_modify_text(
			gtk_bin_get_child(GTK_BIN((quicksearch->search_string_entry))),
			GTK_STATE_NORMAL, error ? &qs_error_color : &qs_active_color);
	} else {
		gtk_widget_modify_base(
			gtk_bin_get_child(GTK_BIN((quicksearch->search_string_entry))),
			GTK_STATE_NORMAL, NULL);
		gtk_widget_modify_text(
			gtk_bin_get_child(GTK_BIN((quicksearch->search_string_entry))),
			GTK_STATE_NORMAL, NULL);
	}

	if (!active) {
		advsearch_abort(quicksearch->asearch);
	}
}

void quicksearch_set_execute_callback(QuickSearch *quicksearch,
				      QuickSearchExecuteCallback callback,
				      gpointer data)
{
	quicksearch->callback = callback;
	quicksearch->callback_data = data;
}

static void quicksearch_set_running(QuickSearch *quicksearch, gboolean run)
{
	quicksearch->running = run;
}

gboolean quicksearch_is_running(QuickSearch *quicksearch)
{
	return quicksearch->running;
}

gboolean quicksearch_is_in_typing(QuickSearch *quicksearch)
{
	return quicksearch->in_typing;
}

void quicksearch_set_search_strings(QuickSearch *quicksearch)
{
	GList *strings = prefs_common.summary_quicksearch_history;
	gchar *newstr = NULL;
	MatcherList *matcher_list = NULL;

	if (!strings)
		return;

	matcher_parser_disable_warnings(TRUE);
	
	do {
		newstr = advsearch_expand_search_string((gchar *) strings->data);
		if (newstr && newstr[0] != '\0') {
			if (!strchr(newstr, ' ')) {
				quicksearch->normal_search_strings =
					g_list_append(
						quicksearch->normal_search_strings,
						g_strdup(strings->data));
			} else {
				matcher_list = matcher_parser_get_cond(newstr, FALSE);
			
				if (matcher_list) {
					quicksearch->extended_search_strings =
						g_list_prepend(
							quicksearch->extended_search_strings,
							g_strdup(strings->data));
					matcherlist_free(matcher_list);
				} else
					quicksearch->normal_search_strings =
						g_list_prepend(
							quicksearch->normal_search_strings,
							g_strdup(strings->data));
			}
		}
		g_free(newstr);
	
	} while ((strings = g_list_next(strings)) != NULL);

	matcher_parser_disable_warnings(FALSE);	

	quicksearch->normal_search_strings = g_list_reverse(quicksearch->normal_search_strings);
	quicksearch->extended_search_strings = g_list_reverse(quicksearch->extended_search_strings);

	quicksearch_set_popdown_strings(quicksearch);
}

