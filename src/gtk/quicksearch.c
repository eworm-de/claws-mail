/*
 * Claws Mail -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2007 Colin Leroy <colin@colino.net> 
 * and the Claws Mail team
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

#include <glib.h>
#include <glib/gi18n.h>
#include <ctype.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "utils.h"
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

struct _QuickSearch
{
	GtkWidget			*hbox_search;
	GtkWidget			*search_type;
	GtkWidget			*search_type_opt;
	GtkWidget			*search_string_entry;
	GtkWidget			*search_condition_expression;
	GtkWidget			*search_description;
	GtkWidget			*clear_search;

	gboolean			 active;
	gchar				*search_string;
	MatcherList			*matcher_list;

	QuickSearchExecuteCallback	 callback;
	gpointer			 callback_data;
	gboolean			 running;
	gboolean			 has_focus;
	gboolean			 matching;
	gboolean			 deferred_free;
	FolderItem			*root_folder_item;
	gboolean			 is_fast;
	gboolean			 in_typing;
	guint				 press_timeout_id;
};

static void quicksearch_set_running(QuickSearch *quicksearch, gboolean run);
static void quicksearch_set_active(QuickSearch *quicksearch, gboolean active);
static void quicksearch_reset_folder_items(QuickSearch *quicksearch, FolderItem *folder_item);
static gchar *expand_search_string(const gchar *str);

gboolean quicksearch_is_fast(QuickSearch *quicksearch)
{
	return quicksearch->is_fast;
}

static void prepare_matcher(QuickSearch *quicksearch)
{
	const gchar *search_string = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(quicksearch->search_string_entry)->entry));

	if (search_string == NULL || search_string[0] == '\0') {
		quicksearch_set_active(quicksearch, FALSE);
	}

	if (quicksearch->matcher_list != NULL) {
		if (quicksearch->matching) {
			quicksearch->deferred_free = TRUE;
			return;
		}
		quicksearch->deferred_free = FALSE;
		matcherlist_free(quicksearch->matcher_list);
		quicksearch->matcher_list = NULL;
	}

	if (search_string == NULL || search_string[0] == '\0') {
		return;
	}

	if (prefs_common.summary_quicksearch_type == QUICK_SEARCH_EXTENDED) {
		char *newstr = NULL;

		newstr = expand_search_string(search_string);
		if (newstr && newstr[0] != '\0') {
			quicksearch->matcher_list = matcher_parser_get_cond(newstr, &quicksearch->is_fast);
			g_free(newstr);
		} else {
			quicksearch->matcher_list = NULL;
			quicksearch_set_active(quicksearch, FALSE);

			return;
		}
	} else {
		quicksearch->is_fast = TRUE;
		g_free(quicksearch->search_string);
		quicksearch->search_string = g_strdup(search_string);
	}

	quicksearch_set_active(quicksearch, TRUE);
}

static void update_extended_buttons (QuickSearch *quicksearch)
{
	GtkWidget *expr_btn = quicksearch->search_condition_expression;
	GtkWidget *ext_btn = quicksearch->search_description;

	g_return_if_fail(expr_btn != NULL);
	g_return_if_fail(ext_btn != NULL);

	if (prefs_common.summary_quicksearch_type == QUICK_SEARCH_EXTENDED) {
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
	const gchar *search_string = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(quicksearch->search_string_entry)->entry));

	/* add to history */
	if (!quicksearch->in_typing && search_string && strlen(search_string) != 0) {
		prefs_common.summary_quicksearch_history =
			add_history(prefs_common.summary_quicksearch_history,
					search_string);
		gtk_combo_set_popdown_strings(GTK_COMBO(quicksearch->search_string_entry),
			prefs_common.summary_quicksearch_history);
	}

	prepare_matcher(quicksearch);
	if (run_only_if_fast && !quicksearch->is_fast)
		return;
	if (quicksearch->matcher_list == NULL && 
	    prefs_common.summary_quicksearch_type == QUICK_SEARCH_EXTENDED &&
	    search_string && strlen(search_string) != 0)
		return;
	quicksearch_set_running(quicksearch, TRUE);
	if (quicksearch->callback != NULL)
		quicksearch->callback(quicksearch, quicksearch->callback_data);
	quicksearch_set_running(quicksearch, FALSE);
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

static gboolean searchbar_changed_cb(GtkWidget *widget, QuickSearch *qs)
{
	if (prefs_common.summary_quicksearch_dynamic) {
		if (qs->press_timeout_id != -1) {
			g_source_remove(qs->press_timeout_id);
		}
		qs->press_timeout_id = g_timeout_add(500,
				searchbar_changed_timeout, qs);
	}

	return FALSE;
}

static gboolean searchbar_pressed(GtkWidget *widget, GdkEventKey *event,
			      	  QuickSearch *quicksearch)
{
	if (event != NULL && event->keyval == GDK_Escape) {

		const gchar *str;

		quicksearch->in_typing = FALSE;

		str = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(quicksearch->search_string_entry)->entry));
		g_return_val_if_fail(str != NULL, TRUE);

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

		return TRUE;
	}

	if (event != NULL && event->keyval == GDK_Return) {
		if (quicksearch->press_timeout_id != -1) {
			g_source_remove(quicksearch->press_timeout_id);
			quicksearch->press_timeout_id = -1;
		}
		quicksearch->in_typing = FALSE;
		/* add expression to history list and exec quicksearch */
		searchbar_run(quicksearch, FALSE);

		g_signal_stop_emission_by_name(G_OBJECT(widget), "key_press_event");
		return TRUE;
	}

	return FALSE;
}

static gboolean searchtype_changed(GtkMenuItem *widget, gpointer data)
{
	QuickSearch *quicksearch = (QuickSearch *)data;
	const gchar *search_string = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(quicksearch->search_string_entry)->entry));

	prefs_common.summary_quicksearch_type = GPOINTER_TO_INT(g_object_get_data(
				   G_OBJECT(GTK_MENU_ITEM(gtk_menu_get_active(
				   GTK_MENU(quicksearch->search_type)))), MENU_VAL_ID));

	/* Show extended search description button, only when Extended is selected */
	update_extended_buttons(quicksearch);

	if (!search_string || strlen(search_string) == 0) {
		return TRUE;
	}

	prepare_matcher(quicksearch);

	quicksearch_set_running(quicksearch, TRUE);
	if (quicksearch->callback != NULL)
		quicksearch->callback(quicksearch, quicksearch->callback_data);
	quicksearch_set_running(quicksearch, FALSE);
	return TRUE;
}

static gboolean searchtype_recursive_changed(GtkMenuItem *widget, gpointer data)
{
	QuickSearch *quicksearch = (QuickSearch *)data;
	gboolean checked = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget));
	const gchar *search_string = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(quicksearch->search_string_entry)->entry));

	prefs_common.summary_quicksearch_recurse = checked;

	/* reselect the search type */
	gtk_option_menu_set_history(GTK_OPTION_MENU(quicksearch->search_type_opt),
				    prefs_common.summary_quicksearch_type);

	if (!search_string || strlen(search_string) == 0) {
		return TRUE;
	}

	prepare_matcher(quicksearch);

	quicksearch_set_running(quicksearch, TRUE);
	if (quicksearch->callback != NULL)
		quicksearch->callback(quicksearch, quicksearch->callback_data);
	quicksearch_set_running(quicksearch, FALSE);
	return TRUE;
}

static gboolean searchtype_sticky_changed(GtkMenuItem *widget, gpointer data)
{
	QuickSearch *quicksearch = (QuickSearch *)data;
	gboolean checked = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget));

	prefs_common.summary_quicksearch_sticky = checked;

	/* reselect the search type */
	gtk_option_menu_set_history(GTK_OPTION_MENU(quicksearch->search_type_opt),
				    prefs_common.summary_quicksearch_type);

	return TRUE;
}

static gboolean searchtype_dynamic_changed(GtkMenuItem *widget, gpointer data)
{
	QuickSearch *quicksearch = (QuickSearch *)data;
	gboolean checked = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(widget));

	prefs_common.summary_quicksearch_dynamic = checked;

	/* reselect the search type */
	gtk_option_menu_set_history(GTK_OPTION_MENU(quicksearch->search_type_opt),
				    prefs_common.summary_quicksearch_type);

	return TRUE;
}

/*
 * Strings describing how to use Extended Search
 *
 * When adding new lines, remember to put 2 strings for each line
 */
static gchar *search_descr_strings[] = {
	"a",	 N_("all messages"),
	"ag #",  N_("messages whose age is greater than #"),
	"al #",  N_("messages whose age is less than #"),
	"b S",	 N_("messages which contain S in the message body"),
	"B S",	 N_("messages which contain S in the whole message"),
	"c S",	 N_("messages carbon-copied to S"),
	"C S",	 N_("message is either to: or cc: to S"),
	"D",	 N_("deleted messages"), /** how I can filter deleted messages **/
	"e S",	 N_("messages which contain S in the Sender field"),
	"E S",	 N_("true if execute \"S\" succeeds"),
	"f S",	 N_("messages originating from user S"),
	"F",	 N_("forwarded messages"),
	"h S",	 N_("messages which contain header S"),
	"i S",	 N_("messages which contain S in Message-ID header"),
	"I S",	 N_("messages which contain S in inreplyto header"),
	"k #",	 N_("messages which are marked with color #"),
	"L",	 N_("locked messages"),
	"n S",	 N_("messages which are in newsgroup S"),
	"N",	 N_("new messages"),
	"O",	 N_("old messages"),
	"p",	 N_("incomplete messages (not entirely downloaded)"),
	"r",	 N_("messages which have been replied to"),
	"R",	 N_("read messages"),
	"s S",	 N_("messages which contain S in subject"),
	"se #",  N_("messages whose score is equal to #"),
	"sg #",  N_("messages whose score is greater than #"),
	"sl #",  N_("messages whose score is lower than #"),
	"Se #",  N_("messages whose size is equal to #"),
	"Sg #",  N_("messages whose size is greater than #"),
	"Ss #",  N_("messages whose size is smaller than #"),
	"t S",	 N_("messages which have been sent to S"),
	"T",	 N_("marked messages"),
	"U",	 N_("unread messages"),
	"x S",	 N_("messages which contain S in References header"),
	"X \"cmd args\"", N_("messages returning 0 when passed to command - %F is message file"),
	"y S",	 N_("messages which contain S in X-Label header"),
	"",	 "" ,
	"&amp;",	 N_("logical AND operator"),
	"|",	 N_("logical OR operator"),
	"! or ~",	N_("logical NOT operator"),
	"%",	 N_("case sensitive search"),
	"",	 "" ,
	" ",	 N_("all filtering expressions are allowed"),
	NULL,	 NULL
};

static DescriptionWindow search_descr = {
	NULL,
	NULL,
	2,
	N_("Extended Search"),
	N_("Extended Search allows the user to define criteria that messages must "
           "have in order to match and be displayed in the message list.\n\n"
	   "The following symbols can be used:"),
	search_descr_strings
};

static void search_description_cb(GtkWidget *widget)
{
	description_window_create(&search_descr);
};

static gboolean clear_search_cb(GtkMenuItem *widget, gpointer data)
{
	QuickSearch *quicksearch = (QuickSearch *)data;

	if (!quicksearch->active)
		return TRUE;

	quicksearch_set(quicksearch, prefs_common.summary_quicksearch_type, "");

	return TRUE;
};

static void search_condition_expr_done(MatcherList * matchers)
{
	gchar *str;

	g_return_if_fail(
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
	const gchar * cond_str;
	MatcherList * matchers = NULL;
	
	g_return_val_if_fail(
			mainwindow_get_mainwindow()->summaryview->quicksearch != NULL,
			FALSE);

	/* re-use it the current quicksearch value if it's a condition expression,
	   otherwise ignore it silently */
	cond_str = gtk_entry_get_text(
			GTK_ENTRY(GTK_COMBO(mainwindow_get_mainwindow()->summaryview->quicksearch->
			search_string_entry)->entry));
	if (*cond_str != '\0') {
		matchers = matcher_parser_get_cond((gchar*)cond_str, NULL);
	}

	prefs_matcher_open(matchers, search_condition_expr_done);

	if (matchers != NULL)
		matcherlist_free(matchers);

	return TRUE;
};

QuickSearch *quicksearch_new()
{
	QuickSearch *quicksearch;

	GtkWidget *hbox_search;
	GtkWidget *search_type_opt;
	GtkWidget *search_type;
	GtkWidget *search_string_entry;
	GtkWidget *search_hbox;
	GtkWidget *search_description;
	GtkWidget *clear_search;
	GtkWidget *search_condition_expression;
	GtkWidget *menuitem;
	GtkTooltips *tips = gtk_tooltips_new();
	gint index;

	quicksearch = g_new0(QuickSearch, 1);

	/* quick search */
	hbox_search = gtk_hbox_new(FALSE, 0);

	search_type_opt = gtk_option_menu_new();
	gtk_widget_show(search_type_opt);
	gtk_box_pack_start(GTK_BOX(hbox_search), search_type_opt, FALSE, FALSE, 0);

	search_type = gtk_menu_new();
	MENUITEM_ADD (search_type, menuitem,
			prefs_common_translated_header_name("Subject"), QUICK_SEARCH_SUBJECT);
	g_signal_connect(G_OBJECT(menuitem), "activate",
			 G_CALLBACK(searchtype_changed),
			 quicksearch);
	MENUITEM_ADD (search_type, menuitem,
			prefs_common_translated_header_name("From"), QUICK_SEARCH_FROM);
	g_signal_connect(G_OBJECT(menuitem), "activate",
			 G_CALLBACK(searchtype_changed),
			 quicksearch);
	MENUITEM_ADD (search_type, menuitem,
			prefs_common_translated_header_name("To"), QUICK_SEARCH_TO);
	g_signal_connect(G_OBJECT(menuitem), "activate",
			 G_CALLBACK(searchtype_changed),
			 quicksearch);
	MENUITEM_ADD (search_type, menuitem,
			prefs_common_translated_header_name("From, To or Subject"), QUICK_SEARCH_MIXED);
	g_signal_connect(G_OBJECT(menuitem), "activate",
	                 G_CALLBACK(searchtype_changed),
			 quicksearch);
	MENUITEM_ADD (search_type, menuitem, _("Extended"), QUICK_SEARCH_EXTENDED);
	g_signal_connect(G_OBJECT(menuitem), "activate",
			 G_CALLBACK(searchtype_changed),
			 quicksearch);

	gtk_menu_shell_append(GTK_MENU_SHELL(search_type), gtk_separator_menu_item_new());

	menuitem = gtk_check_menu_item_new_with_label(_("Recursive"));
	gtk_menu_shell_append(GTK_MENU_SHELL(search_type), menuitem);

	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem),
					prefs_common.summary_quicksearch_recurse);

	g_signal_connect(G_OBJECT(menuitem), "activate",
			 G_CALLBACK(searchtype_recursive_changed),
			 quicksearch);

	menuitem = gtk_check_menu_item_new_with_label(_("Sticky"));
	gtk_menu_shell_append(GTK_MENU_SHELL(search_type), menuitem);

	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem),
					prefs_common.summary_quicksearch_sticky);

	g_signal_connect(G_OBJECT(menuitem), "activate",
			 G_CALLBACK(searchtype_sticky_changed),
			 quicksearch);

	menuitem = gtk_check_menu_item_new_with_label(_("Type-ahead"));
	gtk_menu_shell_append(GTK_MENU_SHELL(search_type), menuitem);

	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menuitem),
					prefs_common.summary_quicksearch_dynamic);

	g_signal_connect(G_OBJECT(menuitem), "activate",
			 G_CALLBACK(searchtype_dynamic_changed),
			 quicksearch);

	gtk_option_menu_set_menu(GTK_OPTION_MENU(search_type_opt), search_type);

	index = menu_find_option_menu_index(GTK_OPTION_MENU(search_type_opt), 
					GINT_TO_POINTER(prefs_common.summary_quicksearch_type),
					NULL);
	gtk_option_menu_set_history(GTK_OPTION_MENU(search_type_opt), index);

	gtk_widget_show(search_type);

	search_string_entry = gtk_combo_new();
	gtk_box_pack_start(GTK_BOX(hbox_search), search_string_entry, FALSE, FALSE, 2);
	gtk_combo_set_value_in_list(GTK_COMBO(search_string_entry), FALSE, TRUE);
	gtk_combo_set_case_sensitive(GTK_COMBO(search_string_entry), TRUE);
	if (prefs_common.summary_quicksearch_history)
		gtk_combo_set_popdown_strings(GTK_COMBO(search_string_entry),
			prefs_common.summary_quicksearch_history);
	gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(search_string_entry)->entry), "");
	gtk_widget_show(search_string_entry);

	search_hbox = gtk_hbox_new(FALSE, 5);

#if GTK_CHECK_VERSION(2, 8, 0)
	clear_search = gtk_button_new_from_stock(GTK_STOCK_CLEAR);
#else
	clear_search = gtk_button_new_with_label(_(" Clear "));
#endif
	gtk_box_pack_start(GTK_BOX(search_hbox), clear_search,
			   FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(clear_search), "clicked",
			 G_CALLBACK(clear_search_cb), quicksearch);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(tips),
			     clear_search,
			     _("Clear the current search"), NULL);
	gtk_widget_show(clear_search);

#if GTK_CHECK_VERSION(2, 8, 0)
	search_condition_expression = gtk_button_new_from_stock(GTK_STOCK_EDIT);
#else
	search_condition_expression = gtk_button_new_with_label(" ... ");
#endif
	gtk_box_pack_start(GTK_BOX(search_hbox), search_condition_expression,
			   FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT (search_condition_expression), "clicked",
			 G_CALLBACK(search_condition_expr),
			 quicksearch);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(tips),
			     search_condition_expression,
			     _("Edit search criteria"), NULL);
	gtk_widget_show(search_condition_expression);

#if GTK_CHECK_VERSION(2, 8, 0)
	search_description = gtk_button_new_from_stock(GTK_STOCK_INFO);
#else
	search_description = gtk_button_new_with_label(_(" Extended Symbols... "));
#endif
	gtk_box_pack_start(GTK_BOX(search_hbox), search_description,
			   FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(search_description), "clicked",
			 G_CALLBACK(search_description_cb), NULL);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(tips),
			     search_description,
			     _("Information about extended symbols"), NULL);
	gtk_widget_show(search_description);

	gtk_box_pack_start(GTK_BOX(hbox_search), search_hbox, FALSE, FALSE, 2);
	gtk_widget_show(search_hbox);

	g_signal_connect(G_OBJECT(GTK_COMBO(search_string_entry)->entry),
			   "key_press_event",
			   G_CALLBACK(searchbar_pressed),
			   quicksearch);

	g_signal_connect(G_OBJECT(GTK_COMBO(search_string_entry)->entry),
			 "changed",
			 G_CALLBACK(searchbar_changed_cb),
			 quicksearch);

	g_signal_connect(G_OBJECT(GTK_COMBO(search_string_entry)->entry),
			 "focus_in_event",
			 G_CALLBACK(searchbar_focus_evt_in),
			 quicksearch);
	g_signal_connect(G_OBJECT(GTK_COMBO(search_string_entry)->entry),
			 "focus_out_event",
			 G_CALLBACK(searchbar_focus_evt_out),
			 quicksearch);

	quicksearch->hbox_search = hbox_search;
	quicksearch->search_type = search_type;
	quicksearch->search_type_opt = search_type_opt;
	quicksearch->search_string_entry = search_string_entry;
	quicksearch->search_condition_expression = search_condition_expression;
	quicksearch->search_description = search_description;
	quicksearch->matcher_list = NULL;
	quicksearch->active = FALSE;
	quicksearch->running = FALSE;
	quicksearch->clear_search = clear_search;
	quicksearch->in_typing = FALSE;
	quicksearch->press_timeout_id = -1;

	update_extended_buttons(quicksearch);

	return quicksearch;
}

void quicksearch_relayout(QuickSearch *quicksearch)
{
	switch (prefs_common.layout_mode) {
	case NORMAL_LAYOUT:
	case WIDE_LAYOUT:
	case WIDE_MSGLIST_LAYOUT:
#if GTK_CHECK_VERSION(2, 8, 0)
		gtk_button_set_label(GTK_BUTTON(quicksearch->search_description), GTK_STOCK_INFO);
		gtk_button_set_label(GTK_BUTTON(quicksearch->search_condition_expression), GTK_STOCK_EDIT);
		gtk_button_set_label(GTK_BUTTON(quicksearch->clear_search), GTK_STOCK_CLEAR);
#else
		gtk_button_set_label(GTK_BUTTON(quicksearch->search_description), _(" Extended Symbols... "));
		gtk_button_set_label(GTK_BUTTON(quicksearch->search_condition_expression), " ... ");
		gtk_button_set_label(GTK_BUTTON(quicksearch->clear_search), _(" Clear "));
#endif
		break;
	case VERTICAL_LAYOUT:
#if GTK_CHECK_VERSION(2, 8, 0)
		gtk_button_set_label(GTK_BUTTON(quicksearch->search_description), "");
		gtk_button_set_label(GTK_BUTTON(quicksearch->search_condition_expression), "");
		gtk_button_set_label(GTK_BUTTON(quicksearch->clear_search), "");

		gtk_button_set_image(GTK_BUTTON(quicksearch->search_description),
			gtk_image_new_from_stock(GTK_STOCK_INFO, GTK_ICON_SIZE_BUTTON));
		gtk_button_set_image(GTK_BUTTON(quicksearch->search_condition_expression),
			gtk_image_new_from_stock(GTK_STOCK_EDIT, GTK_ICON_SIZE_BUTTON));
		gtk_button_set_image(GTK_BUTTON(quicksearch->clear_search),
			gtk_image_new_from_stock(GTK_STOCK_CLEAR, GTK_ICON_SIZE_BUTTON));
#else
		gtk_button_set_label(GTK_BUTTON(quicksearch->search_description), _("Info"));
		gtk_button_set_label(GTK_BUTTON(quicksearch->search_condition_expression), "...");
		gtk_button_set_label(GTK_BUTTON(quicksearch->clear_search), _("Clear"));
#endif
		break;
	}
}

GtkWidget *quicksearch_get_widget(QuickSearch *quicksearch)
{
	return quicksearch->hbox_search;
}

void quicksearch_show(QuickSearch *quicksearch)
{
	MainWindow *mainwin = mainwindow_get_mainwindow();
	GtkWidget *ctree = NULL;
	prepare_matcher(quicksearch);
	gtk_widget_show(quicksearch->hbox_search);
	update_extended_buttons(quicksearch);
	gtk_widget_grab_focus(
		GTK_WIDGET(GTK_COMBO(quicksearch->search_string_entry)->entry));

	GTK_EVENTS_FLUSH();

	if (!mainwin || !mainwin->summaryview) {
		return;
	}
	
	ctree = summary_get_main_widget(mainwin->summaryview);
	
	if (ctree && mainwin->summaryview->selected)
		gtk_ctree_node_moveto(GTK_CTREE(ctree), 
				mainwin->summaryview->selected, 
				0, 0.5, 0);
}

void quicksearch_hide(QuickSearch *quicksearch)
{
	if (quicksearch_is_active(quicksearch)) {
		quicksearch_set(quicksearch, prefs_common.summary_quicksearch_type, "");
		quicksearch_set_active(quicksearch, FALSE);
	}
	gtk_widget_hide(quicksearch->hbox_search);
}

void quicksearch_set(QuickSearch *quicksearch, QuickSearchType type,
		     const gchar *matchstring)
{
	gtk_option_menu_set_history(GTK_OPTION_MENU(quicksearch->search_type_opt),
				    type);

	if (!matchstring || !(*matchstring))
		quicksearch->in_typing = FALSE;

	g_signal_handlers_block_by_func(G_OBJECT(GTK_COMBO(quicksearch->search_string_entry)->entry),
			G_CALLBACK(searchbar_changed_cb), quicksearch);
	gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(quicksearch->search_string_entry)->entry),
			   matchstring);
	g_signal_handlers_unblock_by_func(G_OBJECT(GTK_COMBO(quicksearch->search_string_entry)->entry),
			G_CALLBACK(searchbar_changed_cb), quicksearch);

	prefs_common.summary_quicksearch_type = type;

	prepare_matcher(quicksearch);

	quicksearch_set_running(quicksearch, TRUE);
	if (quicksearch->callback != NULL)
		quicksearch->callback(quicksearch, quicksearch->callback_data);
	quicksearch_set_running(quicksearch, FALSE);
}

gboolean quicksearch_is_active(QuickSearch *quicksearch)
{
	return quicksearch->active && 
		(prefs_common.summary_quicksearch_type != QUICK_SEARCH_EXTENDED
		 || quicksearch->matcher_list != NULL);
}

static void quicksearch_set_active(QuickSearch *quicksearch, gboolean active)
{
	static GdkColor yellow;
	static GdkColor red;
	static GdkColor black;
	static gboolean colors_initialised = FALSE;
	gboolean error = FALSE;

	if (!colors_initialised) {
		gdk_color_parse("#f5f6be", &yellow);
		gdk_color_parse("#000000", &black);
		gdk_color_parse("#ff7070", &red);
		colors_initialised = gdk_colormap_alloc_color(
			gdk_colormap_get_system(), &yellow, FALSE, TRUE);
		colors_initialised &= gdk_colormap_alloc_color(
			gdk_colormap_get_system(), &black, FALSE, TRUE);
		colors_initialised &= gdk_colormap_alloc_color(
			gdk_colormap_get_system(), &red, FALSE, TRUE);
	}

	quicksearch->active = active;

	if (active && 
		(prefs_common.summary_quicksearch_type == QUICK_SEARCH_EXTENDED
		 && quicksearch->matcher_list == NULL))
		error = TRUE;

	if (active) {
		gtk_widget_set_sensitive(quicksearch->clear_search, TRUE);
		if (colors_initialised) {
			gtk_widget_modify_base(
				GTK_COMBO(quicksearch->search_string_entry)->entry,
				GTK_STATE_NORMAL, error ? &red : &yellow);
			gtk_widget_modify_text(
				GTK_COMBO(quicksearch->search_string_entry)->entry,
				GTK_STATE_NORMAL, &black);
		}
	} else {
		gtk_widget_set_sensitive(quicksearch->clear_search, FALSE);
		if (colors_initialised) {
			gtk_widget_modify_base(
				GTK_COMBO(quicksearch->search_string_entry)->entry,
				GTK_STATE_NORMAL, NULL);
			gtk_widget_modify_text(
				GTK_COMBO(quicksearch->search_string_entry)->entry,
				GTK_STATE_NORMAL, NULL);
		}
	}

	if (!active) {
		quicksearch_reset_cur_folder_item(quicksearch);
	}
}

void quicksearch_set_execute_callback(QuickSearch *quicksearch,
				      QuickSearchExecuteCallback callback,
				      gpointer data)
{
	quicksearch->callback = callback;
	quicksearch->callback_data = data;
}

gboolean quicksearch_match(QuickSearch *quicksearch, MsgInfo *msginfo)
{
	gchar *searched_header = NULL;
	gboolean result = FALSE;

	if (!quicksearch->active)
		return TRUE;

	switch (prefs_common.summary_quicksearch_type) {
	case QUICK_SEARCH_SUBJECT:
		searched_header = msginfo->subject;
		break;
	case QUICK_SEARCH_FROM:
		searched_header = msginfo->from;
		break;
	case QUICK_SEARCH_TO:
		searched_header = msginfo->to;
		break;
	case QUICK_SEARCH_MIXED:
		break;
	case QUICK_SEARCH_EXTENDED:
		break;
	default:
		debug_print("unknown search type (%d)\n", prefs_common.summary_quicksearch_type);
		break;
	}
	quicksearch->matching = TRUE;
	if (prefs_common.summary_quicksearch_type != QUICK_SEARCH_EXTENDED &&
	    prefs_common.summary_quicksearch_type != QUICK_SEARCH_MIXED &&
	    quicksearch->search_string &&
            searched_header && strcasestr(searched_header, quicksearch->search_string) != NULL)
		result = TRUE;
	else if (prefs_common.summary_quicksearch_type == QUICK_SEARCH_MIXED &&
		quicksearch->search_string && (
		(msginfo->to && strcasestr(msginfo->to, quicksearch->search_string) != NULL) ||
		(msginfo->from && strcasestr(msginfo->from, quicksearch->search_string) != NULL) ||
		(msginfo->subject && strcasestr(msginfo->subject, quicksearch->search_string) != NULL)  ))
		result = TRUE;
	else if ((quicksearch->matcher_list != NULL) &&
	         matcherlist_match(quicksearch->matcher_list, msginfo))
		result = TRUE;

	quicksearch->matching = FALSE;
	if (quicksearch->deferred_free) {
		prepare_matcher(quicksearch);
	}

	return result;
}

/* allow Mutt-like patterns in quick search */
static gchar *expand_search_string(const gchar *search_string)
{
	int i = 0;
	gchar term_char, save_char;
	gchar *cmd_start, *cmd_end;
	GString *matcherstr;
	gchar *returnstr = NULL;
	gchar *copy_str;
	gboolean casesens, dontmatch;
	/* list of allowed pattern abbreviations */
	struct {
		gchar		*abbreviated;	/* abbreviation */
		gchar		*command;	/* actual matcher command */
		gint		numparams;	/* number of params for cmd */
		gboolean	qualifier;	/* do we append regexpcase */
		gboolean	quotes;		/* do we need quotes */
	}
	cmds[] = {
		{ "a",	"all",				0,	FALSE,	FALSE },
		{ "ag",	"age_greater",			1,	FALSE,	FALSE },
		{ "al",	"age_lower",			1,	FALSE,	FALSE },
		{ "b",	"body_part",			1,	TRUE,	TRUE  },
		{ "B",	"message",			1,	TRUE,	TRUE  },
		{ "c",	"cc",				1,	TRUE,	TRUE  },
		{ "C",	"to_or_cc",			1,	TRUE,	TRUE  },
		{ "D",	"deleted",			0,	FALSE,	FALSE },
		{ "e",	"header \"Sender\"",		1,	TRUE,	TRUE  },
		{ "E",	"execute",			1,	FALSE,	TRUE  },
		{ "f",	"from",				1,	TRUE,	TRUE  },
		{ "F",	"forwarded",			0,	FALSE,	FALSE },
		{ "h",	"headers_part",			1,	TRUE,	TRUE  },
		{ "i",	"header \"Message-ID\"",	1,	TRUE,	TRUE  },
		{ "I",	"inreplyto",			1,	TRUE,	TRUE  },
		{ "k",	"colorlabel",			1,	FALSE,	FALSE },
		{ "L",	"locked",			0,	FALSE,	FALSE },
		{ "n",	"newsgroups",			1,	TRUE,	TRUE  },
		{ "N",	"new",				0,	FALSE,	FALSE },
		{ "O",	"~new",				0,	FALSE,	FALSE },
		{ "r",	"replied",			0,	FALSE,	FALSE },
		{ "R",	"~unread",			0,	FALSE,	FALSE },
		{ "s",	"subject",			1,	TRUE,	TRUE  },
		{ "se",	"score_equal",			1,	FALSE,	FALSE },
		{ "sg",	"score_greater",		1,	FALSE,	FALSE },
		{ "sl",	"score_lower",			1,	FALSE,	FALSE },
		{ "Se",	"size_equal",			1,	FALSE,	FALSE },
		{ "Sg",	"size_greater",			1,	FALSE,	FALSE },
		{ "Ss",	"size_smaller",			1,	FALSE,	FALSE },
		{ "t",	"to",				1,	TRUE,	TRUE  },
		{ "T",	"marked",			0,	FALSE,	FALSE },
		{ "U",	"unread",			0,	FALSE,	FALSE },
		{ "x",	"header \"References\"",	1,	TRUE,	TRUE  },
		{ "X",  "test",				1,	FALSE,  FALSE },
		{ "y",	"header \"X-Label\"",		1,	TRUE,	TRUE  },
		{ "&",	"&",				0,	FALSE,	FALSE },
		{ "|",	"|",				0,	FALSE,	FALSE },
		{ "p",	"partial",			0,	FALSE, 	FALSE },
		{ NULL,	NULL,				0,	FALSE,	FALSE }
	};

	if (search_string == NULL)
		return NULL;

	copy_str = g_strdup(search_string);

	matcherstr = g_string_sized_new(16);
	cmd_start = copy_str;
	while (cmd_start && *cmd_start) {
		/* skip all white spaces */
		while (*cmd_start && isspace((guchar)*cmd_start))
			cmd_start++;
		cmd_end = cmd_start;

		/* extract a command */
		while (*cmd_end && !isspace((guchar)*cmd_end))
			cmd_end++;

		/* save character */
		save_char = *cmd_end;
		*cmd_end = '\0';

		dontmatch = FALSE;
		casesens = FALSE;

		/* ~ and ! mean logical NOT */
		if (*cmd_start == '~' || *cmd_start == '!')
		{
			dontmatch = TRUE;
			cmd_start++;
		}
		/* % means case sensitive match */
		if (*cmd_start == '%')
		{
			casesens = TRUE;
			cmd_start++;
		}

		/* find matching abbreviation */
		for (i = 0; cmds[i].command; i++) {
			if (!strcmp(cmd_start, cmds[i].abbreviated)) {
				/* restore character */
				*cmd_end = save_char;

				/* copy command */
				if (matcherstr->len > 0) {
					g_string_append(matcherstr, " ");
				}
				if (dontmatch)
					g_string_append(matcherstr, "~");
				g_string_append(matcherstr, cmds[i].command);
				g_string_append(matcherstr, " ");

				/* stop if no params required */
				if (cmds[i].numparams == 0)
					break;

				/* extract a parameter, allow quotes */
				while (*cmd_end && isspace((guchar)*cmd_end))
					cmd_end++;

				cmd_start = cmd_end;
				if (*cmd_start == '"') {
					term_char = '"';
					cmd_end++;
				}
				else
					term_char = ' ';

				/* extract actual parameter */
				while ((*cmd_end) && (*cmd_end != term_char))
					cmd_end++;

				if (*cmd_end == '"')
					cmd_end++;

				save_char = *cmd_end;
				*cmd_end = '\0';

				if (cmds[i].qualifier) {
					if (casesens)
						g_string_append(matcherstr, "regexp ");
					else
						g_string_append(matcherstr, "regexpcase ");
				}

				/* do we need to add quotes ? */
				if (cmds[i].quotes && term_char != '"')
					g_string_append(matcherstr, "\"");

				/* copy actual parameter */
				g_string_append(matcherstr, cmd_start);

				/* do we need to add quotes ? */
				if (cmds[i].quotes && term_char != '"')
					g_string_append(matcherstr, "\"");

				/* restore original character */
				*cmd_end = save_char;

				break;
			}
		}

		if (*cmd_end)
			cmd_end++;
		cmd_start = cmd_end;
	}

	g_free(copy_str);

	/* return search string if no match is found to allow
	   all available filtering expressions in quicksearch */
	if (matcherstr->len > 0) returnstr = matcherstr->str;
	else returnstr = g_strdup(search_string);

	g_string_free(matcherstr, FALSE);
	return returnstr;
}

static void quicksearch_set_running(QuickSearch *quicksearch, gboolean run)
{
	quicksearch->running = run;
}

gboolean quicksearch_is_running(QuickSearch *quicksearch)
{
	return quicksearch->running;
}

void quicksearch_pass_key(QuickSearch *quicksearch, guint val, GdkModifierType mod)
{
	GtkEntry *entry = GTK_ENTRY(GTK_COMBO(quicksearch->search_string_entry)->entry);
	glong curpos = gtk_editable_get_position(GTK_EDITABLE(entry));
	guint32 c;
	char *str = g_strdup(gtk_entry_get_text(entry));
	char *begin = str;
	char *end = NULL;
	char *new = NULL;
	char key[7] = "";
	guint char_len = 0;

	if (gtk_editable_get_selection_bounds(GTK_EDITABLE(entry), NULL, NULL)) {
		/* remove selection */
		gtk_editable_delete_selection(GTK_EDITABLE(entry));
		curpos = gtk_editable_get_position(GTK_EDITABLE(entry));
		/* refresh string */
		g_free(str);
		str = g_strdup(gtk_entry_get_text(entry));
		begin = str;
	}

	if (!(c = gdk_keyval_to_unicode(val))) {
		g_free(str);
		return;
	}
	char_len = g_unichar_to_utf8(c, key);
	if (char_len < 0)
		return;
	key[char_len] = '\0';
	if (curpos < g_utf8_strlen(str, -1)) {
		gchar *stop = g_utf8_offset_to_pointer(begin, curpos);
		end = g_strdup(g_utf8_offset_to_pointer(str, curpos));
		*stop = '\0';
		new = g_strdup_printf("%s%s%s", begin, key, end);
		gtk_entry_set_text(entry, new);
		g_free(end);
	} else {
		new = g_strdup_printf("%s%s", begin, key);
		gtk_entry_set_text(entry, new);
	}
	g_free(str);
	g_free(new);
	gtk_editable_set_position(GTK_EDITABLE(entry), curpos+1);

}

static gboolean quicksearch_match_subfolder(QuickSearch *quicksearch,
				 FolderItem *src)
{
	GSList *msglist = NULL;
	GSList *cur;
	gboolean result = FALSE;
	gint num = 0, total = 0;
	gint interval = quicksearch_is_fast(quicksearch) ? 5000:100;

	statusbar_print_all(_("Searching in %s... \n"),
		src->path ? src->path : "(null)");
		
	msglist = folder_item_get_msg_list(src);
	total = src->total_msgs;
	folder_item_update_freeze();
	for (cur = msglist; cur != NULL; cur = cur->next) {
		MsgInfo *msg = (MsgInfo *)cur->data;
		statusbar_progress_all(num++,total, interval);
		if (quicksearch_match(quicksearch, msg)) {
			result = TRUE;
			break;
		}
		if (num % interval == 0)
			GTK_EVENTS_FLUSH();
		if (!quicksearch_is_active(quicksearch))
			break;
	}
	folder_item_update_thaw();
	statusbar_progress_all(0,0,0);
	statusbar_pop_all();

	procmsg_msg_list_free(msglist);
	return result;
}

void quicksearch_search_subfolders(QuickSearch *quicksearch,
				   FolderView *folderview,
				   FolderItem *folder_item)
{
	FolderItem *cur = NULL;
	GNode *node = folder_item->node->children;

	if (!prefs_common.summary_quicksearch_recurse
	||  quicksearch->in_typing == TRUE)
		return;

	for (; node != NULL; node = node->next) {
		cur = FOLDER_ITEM(node->data);
		if (quicksearch_match_subfolder(quicksearch, cur)) {
			folderview_update_search_icon(cur, TRUE);
		} else {
			folderview_update_search_icon(cur, FALSE);
		}
		if (cur->node->children)
			quicksearch_search_subfolders(quicksearch,
						      folderview,
						      cur);
	}
	quicksearch->root_folder_item = folder_item;
	if (!quicksearch_is_active(quicksearch))
		quicksearch_reset_cur_folder_item(quicksearch);
}

static void quicksearch_reset_folder_items(QuickSearch *quicksearch,
				    FolderItem *folder_item)
{
	FolderItem *cur = NULL;
	GNode *node = (folder_item && folder_item->node) ?
			folder_item->node->children : NULL;

	for (; node != NULL; node = node->next) {
		cur = FOLDER_ITEM(node->data);
		folderview_update_search_icon(cur, FALSE);
		if (cur->node->children)
			quicksearch_reset_folder_items(quicksearch,
						       cur);
	}
}

void quicksearch_reset_cur_folder_item(QuickSearch *quicksearch)
{
	if (quicksearch->root_folder_item)
		quicksearch_reset_folder_items(quicksearch,
					       quicksearch->root_folder_item);

	quicksearch->root_folder_item = NULL;
}

gboolean quicksearch_is_in_typing(QuickSearch *quicksearch)
{
	return quicksearch->in_typing;
}
