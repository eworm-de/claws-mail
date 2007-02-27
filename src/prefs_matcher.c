/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2007 Hiroyuki Yamamoto and the Claws Mail team
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

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "main.h"
#include "prefs_gtk.h"
#include "prefs_matcher.h"
#include "prefs_common.h"
#include "mainwindow.h"
#include "foldersel.h"
#include "manage_window.h"
#include "inc.h"
#include "matcher.h"
#include "utils.h"
#include "gtkutils.h"
#include "alertpanel.h"
#include "folder.h"
#include "description_window.h"

#include "matcher_parser.h"
#include "colorlabel.h"

enum {
	PREFS_MATCHER_COND,
	PREFS_MATCHER_COND_VALID,
	N_PREFS_MATCHER_COLUMNS
};

/*!
 *\brief	UI data for matcher dialog
 */
static struct Matcher {
	GtkWidget *window;

	GtkWidget *ok_btn;

	GtkWidget *predicate_combo;
	GtkWidget *predicate_flag_combo;
	GtkWidget *header_combo;
	GtkWidget *header_addr_combo;

	GtkWidget *criteria_list;

	GtkWidget *predicate_list;
	GtkWidget *predicate_label;
	GtkWidget *predicate_flag_list;

	GtkWidget *bool_op_list;

	GtkWidget *header_entry;
	GtkWidget *header_label;
	GtkWidget *header_addr_entry;
	GtkWidget *header_addr_label;
	GtkWidget *value_entry;
	GtkWidget *value_label;
	GtkWidget *addressbook_folder_label;
	GtkWidget *addressbook_folder_combo;
	GtkWidget *case_checkbtn;
	GtkWidget *regexp_checkbtn;
	GtkWidget *color_optmenu;

	GtkWidget *test_btn;
	GtkWidget *addressbook_select_btn;

	GtkWidget *cond_list_view;

	GtkWidget *criteria_table;

	gint selected_criteria; /*!< selected criteria in combobox */ 
} matcher;

/*!
 *\brief	Conditions with a negate counterpart (like unread and ~unread)
 *		have the same CRITERIA_XXX id). I.e. both unread and ~unread
 *		have criteria id CRITERIA_UNREAD. This id is passed as the
 *		first parameter to #matcherprop_new and #matcherprop_unquote_new.
 *
 *\warning	Currently the enum constants should have the same order as the 
 *		#criteria_text	
 */		
enum {
	CRITERIA_ALL = 0,

	CRITERIA_SUBJECT = 1,
	CRITERIA_FROM = 2,
	CRITERIA_TO = 3,
	CRITERIA_CC = 4,
	CRITERIA_TO_OR_CC = 5,
	CRITERIA_NEWSGROUPS = 6,
	CRITERIA_INREPLYTO = 7,
	CRITERIA_REFERENCES = 8,
	CRITERIA_AGE_GREATER = 9,
	CRITERIA_AGE_LOWER = 10,
	CRITERIA_HEADER = 11,
	CRITERIA_HEADERS_PART = 12,
	CRITERIA_BODY_PART = 13,
	CRITERIA_MESSAGE = 14,

	CRITERIA_UNREAD = 15,
	CRITERIA_NEW = 16,
	CRITERIA_MARKED = 17,
	CRITERIA_DELETED = 18,
	CRITERIA_REPLIED = 19,
	CRITERIA_FORWARDED = 20,
	CRITERIA_LOCKED = 21,
	CRITERIA_SPAM = 22,
	CRITERIA_COLORLABEL = 23,
	CRITERIA_IGNORE_THREAD = 24,

	CRITERIA_SCORE_GREATER = 25,
	CRITERIA_SCORE_LOWER = 26,
	CRITERIA_SCORE_EQUAL = 27,

	CRITERIA_TEST = 28,

	CRITERIA_SIZE_GREATER = 29,
	CRITERIA_SIZE_SMALLER = 30,
	CRITERIA_SIZE_EQUAL   = 31,
	
	CRITERIA_PARTIAL = 32,

	CRITERIA_FOUND_IN_ADDRESSBOOK = 33,
};

/*!
 *\brief	Descriptive text for conditions
 */
static const gchar *criteria_text [] = {
	N_("All messages"), N_("Subject"),
	N_("From"), N_("To"), N_("Cc"), N_("To or Cc"),
	N_("Newsgroups"), N_("In reply to"), N_("References"),
	N_("Age greater than"), N_("Age lower than"),
	N_("Header"), N_("Headers part"),
	N_("Body part"), N_("Whole message"),
	N_("Unread flag"), N_("New flag"),
	N_("Marked flag"), N_("Deleted flag"),
	N_("Replied flag"), N_("Forwarded flag"),
	N_("Locked flag"),
	N_("Spam flag"),
	N_("Color label"),
	N_("Ignored thread"),
	N_("Score greater than"), N_("Score lower than"),
	N_("Score equal to"),
	N_("Test"),
	N_("Size greater than"), 
	N_("Size smaller than"),
	N_("Size exactly"),
	N_("Partially downloaded"),
	N_("Found in addressbook")
};

/*!
 *\brief	Boolean / predicate constants
 *
 *\warning	Same order as #bool_op_text!
 */
enum {
	BOOL_OP_OR = 0,
	BOOL_OP_AND = 1
};

/*!
 *\brief	Descriptive text in UI
 */
static const gchar *bool_op_text [] = {
	N_("or"), N_("and")
};

/*!
 *\brief	Contains predicate	
 *
 *\warning	Same order as in #predicate_text
 */
enum {
	PREDICATE_CONTAINS = 0,
	PREDICATE_DOES_NOT_CONTAIN = 1
};

/*!
 *\brief	Descriptive text in UI for predicate
 */
static const gchar *predicate_text [] = {
	N_("contains"), N_("does not contain")
};

/*!
 *\brief	Preset addressbook book/folder items
 */
static const gchar *addressbook_folder_text [] = {
	N_("Any")
};

/*!
 *\brief	Enabled predicate
 *
 *\warning	Same order as in #predicate_flag_text
 */
enum {
	PREDICATE_FLAG_ENABLED = 0,
	PREDICATE_FLAG_DISABLED = 1
};

/*!
 *\brief	Descriptive text in UI for enabled flag
 */
static const gchar *predicate_flag_text [] = {
	N_("yes"), N_("no")
};

/*!
 *\brief	Hooks
 */
static PrefsMatcherSignal *matchers_callback;

/* widget creating functions */
static void prefs_matcher_create	(void);

static void prefs_matcher_set_dialog	(MatcherList *matchers);
static void prefs_matcher_list_view_set_row	(GtkTreeIter *row, 
						 MatcherProp *prop);

/* callback functions */

static void prefs_matcher_register_cb	(void);
static void prefs_matcher_substitute_cb	(void);
static void prefs_matcher_delete_cb	(void);
static void prefs_matcher_up		(void);
static void prefs_matcher_down		(void);
static gboolean prefs_matcher_key_pressed(GtkWidget	*widget,
					 GdkEventKey	*event,
					 gpointer	 data);
static void prefs_matcher_ok		(void);
static void prefs_matcher_cancel	(void);
static gint prefs_matcher_deleted	(GtkWidget *widget, GdkEventAny *event,
					 gpointer data);
static void prefs_matcher_criteria_select	(GtkList   *list,
						 GtkWidget *widget,
						 gpointer   user_data);
static MatcherList *prefs_matcher_get_list	(void);

static GtkListStore* prefs_matcher_create_data_store	(void);

static void prefs_matcher_list_view_insert_matcher	(GtkWidget *list_view,
							 GtkTreeIter *row_iter,
							 const gchar *matcher,
							 gboolean is_valid);

static GtkWidget *prefs_matcher_list_view_create	(void);

static void prefs_matcher_create_list_view_columns	(GtkWidget *list_view);

static gboolean prefs_matcher_selected			(GtkTreeSelection *selector,
							 GtkTreeModel *model, 
							 GtkTreePath *path,
							 gboolean currently_selected,
							 gpointer data);

/*!
 *\brief	Find index of list selection 
 *
 *\param	list GTK list widget
 *
 *\return	gint Selection index
 */
static gint get_sel_from_list(GtkList *list)
{
	gint row = 0;
	void * sel;
	GList * child;

	sel = list->selection->data;
	for (child = list->children; child != NULL; child = g_list_next(child)) {
		if (child->data == sel)
			return row;
		row ++;
	}
	
	return row;
}

/*!
 *\brief	Opens the matcher dialog with a list of conditions
 *
 *\param	matchers List of conditions
 *\param	cb Callback
 *
 */
void prefs_matcher_open(MatcherList *matchers, PrefsMatcherSignal *cb)
{
	inc_lock();

	if (!matcher.window) {
		prefs_matcher_create();
	} else {
		/* update color label menu */
		gtk_option_menu_set_menu(GTK_OPTION_MENU(matcher.color_optmenu),
				colorlabel_create_color_menu());
	}

	manage_window_set_transient(GTK_WINDOW(matcher.window));
	gtk_widget_grab_focus(matcher.ok_btn);

	matchers_callback = cb;

	prefs_matcher_set_dialog(matchers);

	gtk_widget_show(matcher.window);
}

/*!
 *\brief	Save Gtk object size to prefs dataset
 */
static void prefs_matcher_size_allocate_cb(GtkWidget *widget,
					 GtkAllocation *allocation)
{
	g_return_if_fail(allocation != NULL);

	prefs_common.matcherwin_width = allocation->width;
	prefs_common.matcherwin_height = allocation->height;
}

/*!
 *\brief	Create the matcher dialog
 */
static void prefs_matcher_create(void)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;
	GtkWidget *confirm_area;

	GtkWidget *vbox1;
	GtkWidget *vbox2;
	GtkWidget *vbox3;
	GtkWidget *criteria_table;

	GtkWidget *hbox1;

	GtkWidget *header_combo;
	GtkWidget *header_entry;
	GtkWidget *header_label;
	GtkWidget *header_addr_combo;
	GtkWidget *header_addr_entry;
	GtkWidget *header_addr_label;
	GtkWidget *criteria_combo;
	GtkWidget *criteria_list;
	GtkWidget *criteria_label;
	GtkWidget *value_label;
	GtkWidget *value_entry;
	GtkWidget *addressbook_folder_label;
	GtkWidget *addressbook_folder_combo;
	GtkWidget *predicate_combo;
	GtkWidget *predicate_list;
	GtkWidget *predicate_flag_combo;
	GtkWidget *predicate_flag_list;
	GtkWidget *predicate_label;
	GtkWidget *bool_op_combo;
	GtkWidget *bool_op_list;
	GtkWidget *bool_op_label;

	GtkWidget *regexp_checkbtn;
	GtkWidget *case_checkbtn;

	GtkWidget *reg_hbox;
	GtkWidget *btn_hbox;
	GtkWidget *arrow;
	GtkWidget *reg_btn;
	GtkWidget *subst_btn;
	GtkWidget *del_btn;

	GtkWidget *cond_hbox;
	GtkWidget *cond_scrolledwin;
	GtkWidget *cond_list_view;

	GtkWidget *btn_vbox;
	GtkWidget *up_btn;
	GtkWidget *down_btn;

	GtkWidget *test_btn;
	GtkWidget *addressbook_select_btn;

	GtkWidget *color_optmenu;

	GList *combo_items;
	gint i;
	static GdkGeometry geometry;

	debug_print("Creating matcher configuration window...\n");

	window = gtkut_window_new(GTK_WINDOW_TOPLEVEL, "prefs_matcher");
	gtk_container_set_border_width(GTK_CONTAINER(window), 8);
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(window), TRUE);
	gtk_window_set_resizable(GTK_WINDOW(window), TRUE);

	vbox = gtk_vbox_new(FALSE, 6);
	gtk_widget_show(vbox);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	gtkut_stock_button_set_create(&confirm_area, &cancel_btn, GTK_STOCK_CANCEL,
				      &ok_btn, GTK_STOCK_OK, NULL, NULL);
	gtk_widget_show(confirm_area);
	gtk_box_pack_end(GTK_BOX(vbox), confirm_area, FALSE, FALSE, 0);
	gtk_widget_grab_default(ok_btn);

	gtk_window_set_title(GTK_WINDOW(window),
			     _("Condition configuration"));
	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(prefs_matcher_deleted), NULL);
	g_signal_connect(G_OBJECT(window), "size_allocate",
			 G_CALLBACK(prefs_matcher_size_allocate_cb), NULL);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(prefs_matcher_key_pressed), NULL);
	MANAGE_WINDOW_SIGNALS_CONNECT(window);
	g_signal_connect(G_OBJECT(ok_btn), "clicked",
			 G_CALLBACK(prefs_matcher_ok), NULL);
	g_signal_connect(G_OBJECT(cancel_btn), "clicked",
			 G_CALLBACK(prefs_matcher_cancel), NULL);

	vbox1 = gtk_vbox_new(FALSE, VSPACING);
	gtk_widget_show(vbox1);
	gtk_box_pack_start(GTK_BOX(vbox), vbox1, TRUE, TRUE, 0);
	gtk_container_set_border_width(GTK_CONTAINER (vbox1), 2);

	criteria_table = gtk_table_new(2, 4, FALSE);
	gtk_widget_show(criteria_table);

	gtk_box_pack_start(GTK_BOX(vbox1), criteria_table, FALSE, FALSE, 0);
	gtk_table_set_row_spacings(GTK_TABLE(criteria_table), 8);
	gtk_table_set_col_spacings(GTK_TABLE(criteria_table), 8);

	/* criteria combo box */

	criteria_label = gtk_label_new(_("Match type"));
	gtk_widget_show(criteria_label);
	gtk_misc_set_alignment(GTK_MISC(criteria_label), 0, 0.5);
	gtk_table_attach(GTK_TABLE(criteria_table), criteria_label, 0, 1, 0, 1,
			 GTK_FILL, 0, 0, 0);

	criteria_combo = gtk_combo_new();
	gtk_widget_show(criteria_combo);

	combo_items = NULL;

	for (i = 0; i < (gint) (sizeof(criteria_text) / sizeof(gchar *)); i++) {
		combo_items = g_list_append(combo_items,
					    (gpointer) _(criteria_text[i]));
	}
	gtk_combo_set_popdown_strings(GTK_COMBO(criteria_combo), combo_items);

	g_list_free(combo_items);

	gtk_widget_set_size_request(criteria_combo, 170, -1);
	gtk_table_attach(GTK_TABLE(criteria_table), criteria_combo, 0, 1, 1, 2,
			  0, 0, 0, 0);
	criteria_list = GTK_COMBO(criteria_combo)->list;
	g_signal_connect(G_OBJECT(criteria_list), "select-child",
			 G_CALLBACK(prefs_matcher_criteria_select),
			 NULL);

	gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(criteria_combo)->entry),
			       FALSE);

	/* header name */

	header_label = gtk_label_new(_("Header name"));
	gtk_widget_show(header_label);
	gtk_misc_set_alignment(GTK_MISC(header_label), 0, 0.5);
	gtk_table_attach(GTK_TABLE(criteria_table), header_label, 1, 2, 0, 1,
			 GTK_FILL, 0, 0, 0);

	header_combo = gtk_combo_new();
	gtk_widget_show(header_combo);
	gtk_widget_set_size_request(header_combo, 120, -1);
	gtkut_combo_set_items(GTK_COMBO (header_combo),
			      "Subject", "From", "To", "Cc", "Reply-To",
			      "Sender", "X-ML-Name", "X-List", "X-Sequence",
			      "X-Mailer","X-BeenThere",
			      NULL);
	gtk_table_attach(GTK_TABLE(criteria_table), header_combo, 1, 2, 1, 2,
			 0, 0, 0, 0);
	header_entry = GTK_COMBO(header_combo)->entry;
	gtk_entry_set_editable(GTK_ENTRY(header_entry), TRUE);

	/* address header name */

	header_addr_label = gtk_label_new(_("Address header"));
	gtk_misc_set_alignment(GTK_MISC(header_addr_label), 0, 0.5);
	gtk_table_attach(GTK_TABLE(criteria_table), header_addr_label, 1, 2, 0, 1,
			 GTK_FILL, 0, 0, 0);

	header_addr_combo = gtk_combo_new();
	gtk_widget_set_size_request(header_addr_combo, 120, -1);
	gtkut_combo_set_items(GTK_COMBO (header_addr_combo),
			      Q_("Filtering Matcher Menu|All"),
			      _("Any"), "From", "To", "Cc", "Reply-To", "Sender",
			      NULL);
	gtk_table_attach(GTK_TABLE(criteria_table), header_addr_combo, 1, 2, 1, 2,
			 0, 0, 0, 0);
	header_addr_entry = GTK_COMBO(header_addr_combo)->entry;
	gtk_entry_set_editable(GTK_ENTRY(header_addr_entry), TRUE);

	/* value */

	value_label = gtk_label_new(_("Value"));
	gtk_widget_show(value_label);
	gtk_misc_set_alignment(GTK_MISC (value_label), 0, 0.5);
	gtk_table_attach(GTK_TABLE(criteria_table), value_label, 2, 3, 0, 1,
			 GTK_FILL | GTK_SHRINK | GTK_EXPAND, 0, 0, 0);

	value_entry = gtk_entry_new();
	gtk_widget_show(value_entry);
	gtk_widget_set_size_request(value_entry, 200, -1);
	gtk_table_attach(GTK_TABLE(criteria_table), value_entry, 2, 3, 1, 2,
			 GTK_FILL | GTK_SHRINK | GTK_EXPAND, 0, 0, 0);

	/* book/folder value */

	addressbook_folder_label = gtk_label_new(_("Book/folder"));
	gtk_misc_set_alignment(GTK_MISC (addressbook_folder_label), 0, 0.5);
	gtk_table_attach(GTK_TABLE(criteria_table), addressbook_folder_label, 2, 3, 0, 1,
			 GTK_FILL | GTK_SHRINK | GTK_EXPAND, 0, 0, 0);

	addressbook_folder_combo = gtk_combo_new();
	gtk_widget_show(addressbook_folder_combo);
	gtk_widget_set_size_request(addressbook_folder_combo, 200, -1);
	gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(addressbook_folder_combo)->entry),
			       TRUE);

	combo_items = NULL;
	for (i = 0; i < (gint) (sizeof(addressbook_folder_text) / sizeof(gchar *)); i++) {
		combo_items = g_list_append(combo_items,
					    (gpointer) _(addressbook_folder_text[i]));
	}
	gtk_combo_set_popdown_strings(GTK_COMBO(addressbook_folder_combo), combo_items);
	g_list_free(combo_items);

	gtk_table_attach(GTK_TABLE(criteria_table), addressbook_folder_combo, 2, 3, 1, 2,
			 GTK_FILL | GTK_SHRINK | GTK_EXPAND, 0, 0, 0);

#if GTK_CHECK_VERSION(2, 8, 0)
	test_btn = gtk_button_new_from_stock(GTK_STOCK_INFO);
#else
	test_btn = gtk_button_new_with_label(_("Info..."));
#endif
	gtk_widget_show(test_btn);
	gtk_table_attach(GTK_TABLE (criteria_table), test_btn, 3, 4, 1, 2,
			 0, 0, 0, 0);
	g_signal_connect(G_OBJECT (test_btn), "clicked",
			 G_CALLBACK(prefs_matcher_test_info),
			 NULL);

	addressbook_select_btn = gtk_button_new_with_label(_("Select ..."));
	gtk_table_attach(GTK_TABLE (criteria_table), addressbook_select_btn, 3, 4, 1, 2,
			 0, 0, 0, 0);
	g_signal_connect(G_OBJECT (addressbook_select_btn), "clicked",
			 G_CALLBACK(prefs_matcher_addressbook_select),
			 NULL);

	color_optmenu = gtk_option_menu_new();
	gtk_option_menu_set_menu(GTK_OPTION_MENU(color_optmenu),
				 colorlabel_create_color_menu());

	/* predicate */

	vbox2 = gtk_vbox_new(FALSE, VSPACING);
	gtk_widget_show(vbox2);
	gtk_box_pack_start(GTK_BOX(vbox1), vbox2, FALSE, FALSE, 0);

	hbox1 = gtk_hbox_new(FALSE, 8);
	gtk_widget_show(hbox1);
	gtk_box_pack_start(GTK_BOX(vbox2), hbox1, FALSE, FALSE, 0);

	predicate_label = gtk_label_new(_("Predicate"));
	gtk_widget_show(predicate_label);
	gtk_box_pack_start(GTK_BOX(hbox1), predicate_label,
			   FALSE, FALSE, 0);

	predicate_combo = gtk_combo_new();
	gtk_widget_show(predicate_combo);
	gtk_widget_set_size_request(predicate_combo, 120, -1);
	predicate_list = GTK_COMBO(predicate_combo)->list;
	gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(predicate_combo)->entry),
			       FALSE);

	combo_items = NULL;

	for (i = 0; i < (gint) (sizeof(predicate_text) / sizeof(gchar *)); i++) {
		combo_items = g_list_append(combo_items,
					    (gpointer) _(predicate_text[i]));
	}
	gtk_combo_set_popdown_strings(GTK_COMBO(predicate_combo), combo_items);

	g_list_free(combo_items);

	gtk_box_pack_start(GTK_BOX(hbox1), predicate_combo,
			   FALSE, FALSE, 0);

	/* predicate flag */

	predicate_flag_combo = gtk_combo_new();
	gtk_widget_hide(predicate_flag_combo);
	gtk_widget_set_size_request(predicate_flag_combo, 120, -1);
	predicate_flag_list = GTK_COMBO(predicate_flag_combo)->list;
	gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(predicate_flag_combo)->entry), FALSE);

	combo_items = NULL;

	for (i = 0; i < (gint) (sizeof(predicate_text) / sizeof(gchar *)); i++) {
		combo_items = g_list_append(combo_items, (gpointer) _(predicate_flag_text[i]));
	}
	gtk_combo_set_popdown_strings(GTK_COMBO(predicate_flag_combo),
				      combo_items);

	g_list_free(combo_items);

	gtk_box_pack_start(GTK_BOX(hbox1), predicate_flag_combo,
			   FALSE, FALSE, 0);

	vbox3 = gtk_vbox_new(FALSE, 0);
	gtk_widget_show (vbox3);
	gtk_box_pack_start(GTK_BOX(hbox1), vbox3, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON(vbox3, case_checkbtn, _("Case sensitive"));
	PACK_CHECK_BUTTON(vbox3, regexp_checkbtn, _("Use regexp"));

	/* register / substitute / delete */

	reg_hbox = gtk_hbox_new(FALSE, 4);
	gtk_widget_show(reg_hbox);
	gtk_box_pack_start(GTK_BOX(vbox1), reg_hbox, FALSE, FALSE, 0);

	arrow = gtk_arrow_new(GTK_ARROW_DOWN, GTK_SHADOW_OUT);
	gtk_widget_show(arrow);
	gtk_box_pack_start(GTK_BOX(reg_hbox), arrow, FALSE, FALSE, 0);
	gtk_widget_set_size_request(arrow, -1, 16);

	btn_hbox = gtk_hbox_new(FALSE, 4);
	gtk_widget_show(btn_hbox);
	gtk_box_pack_start(GTK_BOX(reg_hbox), btn_hbox, FALSE, FALSE, 0);

	reg_btn = gtk_button_new_from_stock(GTK_STOCK_ADD);
	gtk_widget_show(reg_btn);
	gtk_box_pack_start(GTK_BOX(btn_hbox), reg_btn, FALSE, TRUE, 0);
	g_signal_connect(G_OBJECT(reg_btn), "clicked",
			 G_CALLBACK(prefs_matcher_register_cb), NULL);

	subst_btn = gtkut_get_replace_btn(_("Replace"));
	gtk_widget_show(subst_btn);
	gtk_box_pack_start(GTK_BOX(btn_hbox), subst_btn, FALSE, TRUE, 0);
	g_signal_connect(G_OBJECT(subst_btn), "clicked",
			 G_CALLBACK(prefs_matcher_substitute_cb),
			 NULL);

	del_btn = gtk_button_new_from_stock(GTK_STOCK_DELETE);
	gtk_widget_show(del_btn);
	gtk_box_pack_start(GTK_BOX(btn_hbox), del_btn, FALSE, TRUE, 0);
	g_signal_connect(G_OBJECT(del_btn), "clicked",
			 G_CALLBACK(prefs_matcher_delete_cb), NULL);

	/* boolean operation */

	bool_op_label = gtk_label_new(_("Boolean Op"));
	gtk_misc_set_alignment(GTK_MISC(value_label), 0, 0.5);
	gtk_widget_show(bool_op_label);
	gtk_box_pack_start(GTK_BOX(btn_hbox), bool_op_label,
			   FALSE, FALSE, 0);

	bool_op_combo = gtk_combo_new();
	gtk_widget_show(bool_op_combo);
	gtk_widget_set_size_request(bool_op_combo, 60, -1);
	bool_op_list = GTK_COMBO(bool_op_combo)->list;
	gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(bool_op_combo)->entry),
			       FALSE);

	combo_items = NULL;

	for (i = 0; i < (gint) (sizeof(bool_op_text) / sizeof(gchar *)); i++) {
		combo_items = g_list_append(combo_items,
					    (gpointer) _(bool_op_text[i]));
	}
	gtk_combo_set_popdown_strings(GTK_COMBO(bool_op_combo), combo_items);

	g_list_free(combo_items);

	gtk_box_pack_start(GTK_BOX(btn_hbox), bool_op_combo,
			   FALSE, FALSE, 0);

	cond_hbox = gtk_hbox_new(FALSE, 8);
	gtk_widget_show(cond_hbox);
	gtk_box_pack_start(GTK_BOX(vbox1), cond_hbox, TRUE, TRUE, 0);

	cond_scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_show(cond_scrolledwin);
	gtk_widget_set_size_request(cond_scrolledwin, -1, 150);
	gtk_box_pack_start(GTK_BOX(cond_hbox), cond_scrolledwin,
			   TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(cond_scrolledwin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);

	cond_list_view = prefs_matcher_list_view_create(); 				       
	gtk_widget_show(cond_list_view);
	gtk_container_add(GTK_CONTAINER(cond_scrolledwin), cond_list_view);

	btn_vbox = gtk_vbox_new(FALSE, 8);
	gtk_widget_show(btn_vbox);
	gtk_box_pack_start(GTK_BOX(cond_hbox), btn_vbox, FALSE, FALSE, 0);

	up_btn = gtk_button_new_from_stock(GTK_STOCK_GO_UP);
	gtk_widget_show(up_btn);
	gtk_box_pack_start(GTK_BOX(btn_vbox), up_btn, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(up_btn), "clicked",
			 G_CALLBACK(prefs_matcher_up), NULL);

	down_btn = gtk_button_new_from_stock(GTK_STOCK_GO_DOWN);
	gtk_widget_show(down_btn);
	gtk_box_pack_start(GTK_BOX(btn_vbox), down_btn, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(down_btn), "clicked",
			 G_CALLBACK(prefs_matcher_down), NULL);

	if (!geometry.min_height) {
		geometry.min_width = 520;
		geometry.min_height = 368;
	}

	gtk_window_set_geometry_hints(GTK_WINDOW(window), NULL, &geometry,
				      GDK_HINT_MIN_SIZE);
	gtk_widget_set_size_request(window, prefs_common.matcherwin_width,
				    prefs_common.matcherwin_height);

	gtk_widget_show_all(window);
	gtk_widget_hide(header_addr_label);
	gtk_widget_hide(header_addr_combo);
	gtk_widget_hide(addressbook_select_btn);
	gtk_widget_hide(addressbook_folder_label);

	matcher.window    = window;

	matcher.ok_btn = ok_btn;

	matcher.criteria_list = criteria_list;
	matcher.header_combo = header_combo;
	matcher.header_entry = header_entry;
	matcher.header_label = header_label;
	matcher.header_addr_combo = header_addr_combo;
	matcher.header_addr_entry = header_addr_entry;
	matcher.header_addr_label = header_addr_label;
	matcher.value_entry = value_entry;
	matcher.value_label = value_label;
	matcher.addressbook_folder_label = addressbook_folder_label;
	matcher.addressbook_folder_combo = addressbook_folder_combo;
	matcher.predicate_label = predicate_label;
	matcher.predicate_list = predicate_list;
	matcher.predicate_combo = predicate_combo;
	matcher.predicate_flag_list = predicate_flag_list;
	matcher.predicate_flag_combo = predicate_flag_combo;
	matcher.case_checkbtn = case_checkbtn;
	matcher.regexp_checkbtn = regexp_checkbtn;
	matcher.bool_op_list = bool_op_list;
	matcher.test_btn = test_btn;
	matcher.addressbook_select_btn = addressbook_select_btn;
	matcher.color_optmenu = color_optmenu;
	matcher.criteria_table = criteria_table;

	matcher.cond_list_view = cond_list_view;

	matcher.selected_criteria = -1;
	prefs_matcher_criteria_select(GTK_LIST(criteria_list), NULL, NULL);
}

/*!
 *\brief	Set the contents of a row
 *
 *\param	row Index of row to set
 *\param	prop Condition to set
 *
 *\return	gint Row index \a prop has been added
 */
static void prefs_matcher_list_view_set_row(GtkTreeIter *row, MatcherProp *prop)
{
	gchar *matcher_str;

	if (prop == NULL) {
		prefs_matcher_list_view_insert_matcher(matcher.cond_list_view,
						       NULL, _("(New)"), FALSE);
		return;						       
	}

	matcher_str = matcherprop_to_string(prop);
	if (!row)
		prefs_matcher_list_view_insert_matcher(matcher.cond_list_view,
						       NULL, matcher_str,
						       TRUE);
	else
		prefs_matcher_list_view_insert_matcher(matcher.cond_list_view,
						       row, matcher_str, 
						       TRUE);
	g_free(matcher_str);
}

/*!
 *\brief	Clears a condition in the list widget
 */
static void prefs_matcher_reset_condition(void)
{
	gtk_list_select_item(GTK_LIST(matcher.criteria_list), 0);
	gtk_list_select_item(GTK_LIST(matcher.predicate_list), 0);
	gtk_entry_set_text(GTK_ENTRY(matcher.header_entry), "");
	gtk_entry_set_text(GTK_ENTRY(matcher.header_addr_entry), "");
	gtk_entry_set_text(GTK_ENTRY(matcher.value_entry), "");
	gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(matcher.addressbook_folder_combo)->entry), "");
}

/*!
 *\brief	Initializes dialog with a set of conditions
 *
 *\param	matchers List of conditions
 */
static void prefs_matcher_set_dialog(MatcherList *matchers)
{
	GSList *cur;
	gboolean bool_op = 1;
	GtkListStore *store = GTK_LIST_STORE(gtk_tree_view_get_model
				(GTK_TREE_VIEW(matcher.cond_list_view)));

	gtk_list_store_clear(store);				

	prefs_matcher_list_view_set_row(NULL, NULL);
	if (matchers != NULL) {
		for (cur = matchers->matchers; cur != NULL;
		     cur = g_slist_next(cur)) {
			MatcherProp *prop;
			prop = (MatcherProp *) cur->data;
			prefs_matcher_list_view_set_row(NULL, prop);
		}

		bool_op = matchers->bool_and;
	}
	
	gtk_list_select_item(GTK_LIST(matcher.bool_op_list), bool_op);

	prefs_matcher_reset_condition();
}

/*!
 *\brief	Converts current conditions in list box in
 *		a matcher list used by the matcher.
 *
 *\return	MatcherList * List of conditions.
 */
static MatcherList *prefs_matcher_get_list(void)
{
	gchar *matcher_str;
	MatcherProp *prop;
	gboolean bool_and;
	GSList *matcher_list;
	MatcherList *matchers;
	GtkTreeModel *model;
	GtkTreeIter iter;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(matcher.cond_list_view));
	if (!gtk_tree_model_get_iter_first(model, &iter))
		return NULL;

	matcher_list = NULL;

	do {
		gboolean is_valid;
	
		gtk_tree_model_get(model, &iter,
				   PREFS_MATCHER_COND, &matcher_str,
				   PREFS_MATCHER_COND_VALID, &is_valid,
				   -1);
		
		if (is_valid) {
			/* tmp = matcher_str; */
			prop = matcher_parser_get_prop(matcher_str);
			g_free(matcher_str);
			if (prop == NULL)
				break;
			
			matcher_list = g_slist_append(matcher_list, prop);
		}
	} while (gtk_tree_model_iter_next(model, &iter));

	bool_and = get_sel_from_list(GTK_LIST(matcher.bool_op_list));

	matchers = matcherlist_new(matcher_list, bool_and);

	return matchers;
}

/*!
 *\brief	Maps a keyword id (see #get_matchparser_tab_id) to a 
 *		criteria type (see first parameter of #matcherprop_new
 *		or #matcherprop_unquote_new)
 *
 *\param	matching_id Id returned by the matcher parser.
 *
 *\return	gint One of the CRITERIA_xxx constants.
 */
static gint prefs_matcher_get_criteria_from_matching(gint matching_id)
{
	switch(matching_id) {
	case MATCHCRITERIA_ALL:
		return CRITERIA_ALL;
	case MATCHCRITERIA_NOT_UNREAD:
	case MATCHCRITERIA_UNREAD:
		return CRITERIA_UNREAD;
	case MATCHCRITERIA_NOT_NEW:
	case MATCHCRITERIA_NEW:
		return CRITERIA_NEW;
	case MATCHCRITERIA_NOT_MARKED:
	case MATCHCRITERIA_MARKED:
		return CRITERIA_MARKED;
	case MATCHCRITERIA_NOT_DELETED:
	case MATCHCRITERIA_DELETED:
		return CRITERIA_DELETED;
	case MATCHCRITERIA_NOT_REPLIED:
	case MATCHCRITERIA_REPLIED:
		return CRITERIA_REPLIED;
	case MATCHCRITERIA_NOT_FORWARDED:
	case MATCHCRITERIA_FORWARDED:
		return CRITERIA_FORWARDED;
	case MATCHCRITERIA_LOCKED:
	case MATCHCRITERIA_NOT_LOCKED:
		return CRITERIA_LOCKED;
	case MATCHCRITERIA_NOT_SPAM:
	case MATCHCRITERIA_SPAM:
		return CRITERIA_SPAM;
	case MATCHCRITERIA_PARTIAL:
	case MATCHCRITERIA_NOT_PARTIAL:
		return CRITERIA_PARTIAL;
	case MATCHCRITERIA_COLORLABEL:
	case MATCHCRITERIA_NOT_COLORLABEL:
		return CRITERIA_COLORLABEL;
	case MATCHCRITERIA_IGNORE_THREAD:
	case MATCHCRITERIA_NOT_IGNORE_THREAD:
		return CRITERIA_IGNORE_THREAD;
	case MATCHCRITERIA_NOT_SUBJECT:
	case MATCHCRITERIA_SUBJECT:
		return CRITERIA_SUBJECT;
	case MATCHCRITERIA_NOT_FROM:
	case MATCHCRITERIA_FROM:
		return CRITERIA_FROM;
	case MATCHCRITERIA_NOT_TO:
	case MATCHCRITERIA_TO:
		return CRITERIA_TO;
	case MATCHCRITERIA_NOT_CC:
	case MATCHCRITERIA_CC:
		return CRITERIA_CC;
	case MATCHCRITERIA_NOT_NEWSGROUPS:
	case MATCHCRITERIA_NEWSGROUPS:
		return CRITERIA_NEWSGROUPS;
	case MATCHCRITERIA_NOT_INREPLYTO:
	case MATCHCRITERIA_INREPLYTO:
		return CRITERIA_INREPLYTO;
	case MATCHCRITERIA_NOT_REFERENCES:
	case MATCHCRITERIA_REFERENCES:
		return CRITERIA_REFERENCES;
	case MATCHCRITERIA_NOT_TO_AND_NOT_CC:
	case MATCHCRITERIA_TO_OR_CC:
		return CRITERIA_TO_OR_CC;
	case MATCHCRITERIA_NOT_BODY_PART:
	case MATCHCRITERIA_BODY_PART:
		return CRITERIA_BODY_PART;
	case MATCHCRITERIA_NOT_MESSAGE:
	case MATCHCRITERIA_MESSAGE:
		return CRITERIA_MESSAGE;
	case MATCHCRITERIA_NOT_HEADERS_PART:
	case MATCHCRITERIA_HEADERS_PART:
		return CRITERIA_HEADERS_PART;
	case MATCHCRITERIA_NOT_HEADER:
	case MATCHCRITERIA_HEADER:
		return CRITERIA_HEADER;
	case MATCHCRITERIA_AGE_GREATER:
		return CRITERIA_AGE_GREATER;
	case MATCHCRITERIA_AGE_LOWER:
		return CRITERIA_AGE_LOWER;
	case MATCHCRITERIA_SCORE_GREATER:
		return CRITERIA_SCORE_GREATER;
	case MATCHCRITERIA_SCORE_LOWER:
		return CRITERIA_SCORE_LOWER;
	case MATCHCRITERIA_SCORE_EQUAL:
		return CRITERIA_SCORE_EQUAL;
	case MATCHCRITERIA_NOT_TEST:
	case MATCHCRITERIA_TEST:
		return CRITERIA_TEST;
	case MATCHCRITERIA_SIZE_GREATER:
		return CRITERIA_SIZE_GREATER;
	case MATCHCRITERIA_SIZE_SMALLER:
		return CRITERIA_SIZE_SMALLER;
	case MATCHCRITERIA_SIZE_EQUAL:
		return CRITERIA_SIZE_EQUAL;
	case MATCHCRITERIA_FOUND_IN_ADDRESSBOOK:
	case MATCHCRITERIA_NOT_FOUND_IN_ADDRESSBOOK:
		return CRITERIA_FOUND_IN_ADDRESSBOOK;
	default:
		return -1;
	}
}

/*!
 *\brief	Returns the matcher keyword id from a criteria id
 *
 *\param	criteria_id Criteria id (should not be the negate
 *		one)
 *
 *\return	gint A matcher keyword id. See #get_matchparser_tab_id.
 */
static gint prefs_matcher_get_matching_from_criteria(gint criteria_id)
{
	switch (criteria_id) {
	case CRITERIA_ALL:
		return MATCHCRITERIA_ALL;
	case CRITERIA_UNREAD:
		return MATCHCRITERIA_UNREAD;
	case CRITERIA_NEW:
		return MATCHCRITERIA_NEW;
	case CRITERIA_MARKED:
		return MATCHCRITERIA_MARKED;
	case CRITERIA_DELETED:
		return MATCHCRITERIA_DELETED;
	case CRITERIA_REPLIED:
		return MATCHCRITERIA_REPLIED;
	case CRITERIA_FORWARDED:
		return MATCHCRITERIA_FORWARDED;
	case CRITERIA_LOCKED:
		return MATCHCRITERIA_LOCKED;
	case CRITERIA_SPAM:
		return MATCHCRITERIA_SPAM;
	case CRITERIA_PARTIAL:
		return MATCHCRITERIA_PARTIAL;
	case CRITERIA_COLORLABEL:
		return MATCHCRITERIA_COLORLABEL;
	case CRITERIA_IGNORE_THREAD:
		return MATCHCRITERIA_IGNORE_THREAD;
	case CRITERIA_SUBJECT:
		return MATCHCRITERIA_SUBJECT;
	case CRITERIA_FROM:
		return MATCHCRITERIA_FROM;
	case CRITERIA_TO:
		return MATCHCRITERIA_TO;
	case CRITERIA_CC:
		return MATCHCRITERIA_CC;
	case CRITERIA_TO_OR_CC:
		return MATCHCRITERIA_TO_OR_CC;
	case CRITERIA_NEWSGROUPS:
		return MATCHCRITERIA_NEWSGROUPS;
	case CRITERIA_INREPLYTO:
		return MATCHCRITERIA_INREPLYTO;
	case CRITERIA_REFERENCES:
		return MATCHCRITERIA_REFERENCES;
	case CRITERIA_AGE_GREATER:
		return MATCHCRITERIA_AGE_GREATER;
	case CRITERIA_AGE_LOWER:
		return MATCHCRITERIA_AGE_LOWER;
	case CRITERIA_SCORE_GREATER:
		return MATCHCRITERIA_SCORE_GREATER;
	case CRITERIA_SCORE_LOWER:
		return MATCHCRITERIA_SCORE_LOWER;
	case CRITERIA_SCORE_EQUAL:
		return MATCHCRITERIA_SCORE_EQUAL;
	case CRITERIA_HEADER:
		return MATCHCRITERIA_HEADER;
	case CRITERIA_HEADERS_PART:
		return MATCHCRITERIA_HEADERS_PART;
	case CRITERIA_BODY_PART:
		return MATCHCRITERIA_BODY_PART;
	case CRITERIA_MESSAGE:
		return MATCHCRITERIA_MESSAGE;
	case CRITERIA_TEST:
		return MATCHCRITERIA_TEST;
	case CRITERIA_SIZE_GREATER:
		return MATCHCRITERIA_SIZE_GREATER;
	case CRITERIA_SIZE_SMALLER:
		return MATCHCRITERIA_SIZE_SMALLER;
	case CRITERIA_SIZE_EQUAL:
		return MATCHCRITERIA_SIZE_EQUAL;
	case CRITERIA_FOUND_IN_ADDRESSBOOK:
		return MATCHCRITERIA_FOUND_IN_ADDRESSBOOK;
	default:
		return -1;
	}
}

/*!
 *\brief	Returns the negate matcher keyword id from a matcher keyword
 *		id.
 *
 *\param	matcher_criteria Matcher keyword id. 
 *
 *\return	gint A matcher keyword id. See #get_matchparser_tab_id.
 */
static gint prefs_matcher_not_criteria(gint matcher_criteria)
{
	switch(matcher_criteria) {
	case MATCHCRITERIA_UNREAD:
		return MATCHCRITERIA_NOT_UNREAD;
	case MATCHCRITERIA_NEW:
		return MATCHCRITERIA_NOT_NEW;
	case MATCHCRITERIA_MARKED:
		return MATCHCRITERIA_NOT_MARKED;
	case MATCHCRITERIA_DELETED:
		return MATCHCRITERIA_NOT_DELETED;
	case MATCHCRITERIA_REPLIED:
		return MATCHCRITERIA_NOT_REPLIED;
	case MATCHCRITERIA_FORWARDED:
		return MATCHCRITERIA_NOT_FORWARDED;
	case MATCHCRITERIA_LOCKED:
		return MATCHCRITERIA_NOT_LOCKED;
	case MATCHCRITERIA_SPAM:
		return MATCHCRITERIA_NOT_SPAM;
	case MATCHCRITERIA_PARTIAL:
		return MATCHCRITERIA_NOT_PARTIAL;
	case MATCHCRITERIA_COLORLABEL:
		return MATCHCRITERIA_NOT_COLORLABEL;
	case MATCHCRITERIA_IGNORE_THREAD:
		return MATCHCRITERIA_NOT_IGNORE_THREAD;
	case MATCHCRITERIA_SUBJECT:
		return MATCHCRITERIA_NOT_SUBJECT;
	case MATCHCRITERIA_FROM:
		return MATCHCRITERIA_NOT_FROM;
	case MATCHCRITERIA_TO:
		return MATCHCRITERIA_NOT_TO;
	case MATCHCRITERIA_CC:
		return MATCHCRITERIA_NOT_CC;
	case MATCHCRITERIA_TO_OR_CC:
		return MATCHCRITERIA_NOT_TO_AND_NOT_CC;
	case MATCHCRITERIA_NEWSGROUPS:
		return MATCHCRITERIA_NOT_NEWSGROUPS;
	case MATCHCRITERIA_INREPLYTO:
		return MATCHCRITERIA_NOT_INREPLYTO;
	case MATCHCRITERIA_REFERENCES:
		return MATCHCRITERIA_NOT_REFERENCES;
	case MATCHCRITERIA_HEADER:
		return MATCHCRITERIA_NOT_HEADER;
	case MATCHCRITERIA_HEADERS_PART:
		return MATCHCRITERIA_NOT_HEADERS_PART;
	case MATCHCRITERIA_MESSAGE:
		return MATCHCRITERIA_NOT_MESSAGE;
	case MATCHCRITERIA_TEST:
		return MATCHCRITERIA_NOT_TEST;
	case MATCHCRITERIA_BODY_PART:
		return MATCHCRITERIA_NOT_BODY_PART;
	case MATCHCRITERIA_FOUND_IN_ADDRESSBOOK:
		return MATCHCRITERIA_NOT_FOUND_IN_ADDRESSBOOK;
	default:
		return matcher_criteria;
	}
}

/*!
 *\brief	Converts the text in the selected row to a 
 *		matcher structure
 *
 *\return	MatcherProp * Newly allocated matcher structure.
 */
static MatcherProp *prefs_matcher_dialog_to_matcher(void)
{
	MatcherProp *matcherprop;
	gint criteria;
	gint matchtype;
	gint value_pred;
	gint value_pred_flag;
	gint value_criteria;
	gboolean use_regexp;
	gboolean case_sensitive;
	const gchar *header;
	const gchar *expr;
	gint value;
	const gchar *value_str;

	value_criteria = get_sel_from_list(GTK_LIST(matcher.criteria_list));

	criteria = prefs_matcher_get_matching_from_criteria(value_criteria);

	value_pred = get_sel_from_list(GTK_LIST(matcher.predicate_list));
	value_pred_flag = get_sel_from_list(GTK_LIST(matcher.predicate_flag_list));

	use_regexp = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(matcher.regexp_checkbtn));
	case_sensitive = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(matcher.case_checkbtn));

	switch (value_criteria) {
	case CRITERIA_UNREAD:
	case CRITERIA_NEW:
	case CRITERIA_MARKED:
	case CRITERIA_DELETED:
	case CRITERIA_REPLIED:
	case CRITERIA_FORWARDED:
	case CRITERIA_LOCKED:
	case CRITERIA_SPAM:
	case CRITERIA_PARTIAL:
	case CRITERIA_TEST:
	case CRITERIA_COLORLABEL:
	case CRITERIA_IGNORE_THREAD:
	case CRITERIA_FOUND_IN_ADDRESSBOOK:
		if (value_pred_flag == PREDICATE_FLAG_DISABLED)
			criteria = prefs_matcher_not_criteria(criteria);
		break;
	case CRITERIA_SUBJECT:
	case CRITERIA_FROM:
	case CRITERIA_TO:
	case CRITERIA_CC:
	case CRITERIA_TO_OR_CC:
	case CRITERIA_NEWSGROUPS:
	case CRITERIA_INREPLYTO:
	case CRITERIA_REFERENCES:
	case CRITERIA_HEADERS_PART:
	case CRITERIA_BODY_PART:
	case CRITERIA_MESSAGE:
	case CRITERIA_AGE_GREATER:
	case CRITERIA_AGE_LOWER:
	case CRITERIA_HEADER:
		if (value_pred == PREDICATE_DOES_NOT_CONTAIN)
			criteria = prefs_matcher_not_criteria(criteria);
		break;
	}

	if (use_regexp) {
		if (case_sensitive)
			matchtype = MATCHTYPE_REGEXP;
		else
			matchtype = MATCHTYPE_REGEXPCASE;
	}
	else {
		if (case_sensitive)
			matchtype = MATCHTYPE_MATCH;
		else
			matchtype = MATCHTYPE_MATCHCASE;
	}

	header = NULL;
	expr = NULL;
	value = 0;

	switch (value_criteria) {
	case CRITERIA_ALL:
	case CRITERIA_UNREAD:
	case CRITERIA_NEW:
	case CRITERIA_MARKED:
	case CRITERIA_DELETED:
	case CRITERIA_REPLIED:
	case CRITERIA_FORWARDED:
	case CRITERIA_LOCKED:
	case CRITERIA_SPAM:
	case CRITERIA_PARTIAL:
	case CRITERIA_IGNORE_THREAD:
		break;

	case CRITERIA_SUBJECT:
	case CRITERIA_FROM:
	case CRITERIA_TO:
	case CRITERIA_CC:
	case CRITERIA_TO_OR_CC:
	case CRITERIA_NEWSGROUPS:
	case CRITERIA_INREPLYTO:
	case CRITERIA_REFERENCES:
	case CRITERIA_HEADERS_PART:
	case CRITERIA_BODY_PART:
	case CRITERIA_MESSAGE:
	case CRITERIA_TEST:
		expr = gtk_entry_get_text(GTK_ENTRY(matcher.value_entry));
		break;

	case CRITERIA_AGE_GREATER:
	case CRITERIA_AGE_LOWER:
	case CRITERIA_SCORE_GREATER:
	case CRITERIA_SCORE_LOWER:
	case CRITERIA_SCORE_EQUAL:
	case CRITERIA_SIZE_GREATER:
	case CRITERIA_SIZE_SMALLER:
	case CRITERIA_SIZE_EQUAL:
		value_str = gtk_entry_get_text(GTK_ENTRY(matcher.value_entry));

		if (*value_str == '\0') {
		    alertpanel_error(_("Value is not set."));
		    return NULL;
		}

		value = atoi(value_str);
		break;
		
	case CRITERIA_COLORLABEL:
		value = colorlabel_get_color_menu_active_item
			(gtk_option_menu_get_menu(GTK_OPTION_MENU
				(matcher.color_optmenu))); 
		break;

	case CRITERIA_HEADER:
		header = gtk_entry_get_text(GTK_ENTRY(matcher.header_entry));
		expr = gtk_entry_get_text(GTK_ENTRY(matcher.value_entry));

		if (*header == '\0') {
		    alertpanel_error(_("Header name is not set."));
		    return NULL;
		}
		break;

	case CRITERIA_FOUND_IN_ADDRESSBOOK:
		header = gtk_entry_get_text(GTK_ENTRY(matcher.header_addr_entry));
		expr = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(matcher.addressbook_folder_combo)->entry));

		if (*header == '\0') {
		    alertpanel_error(_("Header name is not set."));
		    return NULL;
		}
		if (*expr == '\0') {
			gchar *msg;
			gchar *tmp;

			if (strcasecmp(header, Q_("Filtering Matcher Menu|All")) == 0)
				tmp = g_strdup(_("all addresses in all headers"));
			else
			if (strcasecmp(header, _("Any")) == 0)
				tmp = g_strdup(_("any address in any header"));
			else
				tmp = g_strdup_printf(_("the address(es) in header '%s'"), header);
			msg = g_strdup_printf(_("Book/folder path is not set.\n\n"
						"If you want to match %s against the whole address book, "
						"you have to select 'Any' from the book/folder drop-down list."),
						tmp);
		    alertpanel_error(msg);
			g_free(msg);
			g_free(tmp);
		    return NULL;
		}
		/* don't store translated "Any"/"All" in matcher expressions */
		if (strcasecmp(header, Q_("Filtering Matcher Menu|All")) == 0)
			header = "All";
		else
			if (strcasecmp(header, _("Any")) == 0)
				header = "Any";
		if (strcasecmp(expr, _("Any")) == 0)
			expr = "Any";
		break;
	}

	matcherprop = matcherprop_new(criteria, header, matchtype,
				      expr, value);

	return matcherprop;
}

/*!
 *\brief	Signal handler for register button
 */
static void prefs_matcher_register_cb(void)
{
	MatcherProp *matcherprop;
	
	matcherprop = prefs_matcher_dialog_to_matcher();
	if (matcherprop == NULL)
		return;

	prefs_matcher_list_view_set_row(NULL, matcherprop);

	matcherprop_free(matcherprop);

	prefs_matcher_reset_condition();
}

/*!
 *\brief	Signal handler for substitute button
 */
static void prefs_matcher_substitute_cb(void)
{
	MatcherProp *matcherprop;
	GtkTreeIter row;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	gboolean is_valid;

	selection = gtk_tree_view_get_selection
			(GTK_TREE_VIEW(matcher.cond_list_view));
	
	if (!gtk_tree_selection_get_selected(selection, &model, &row))
		return;
	
	gtk_tree_model_get(model, &row, 
			   PREFS_MATCHER_COND_VALID, &is_valid,
			   -1);
	if (!is_valid)
		return;

	matcherprop = prefs_matcher_dialog_to_matcher();
	if (matcherprop == NULL)
		return;

	prefs_matcher_list_view_set_row(&row, matcherprop);

	matcherprop_free(matcherprop);

	prefs_matcher_reset_condition();
}

/*!
 *\brief	Signal handler for delete button
 */
static void prefs_matcher_delete_cb(void)
{
	GtkTreeIter row;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	gboolean is_valid;

	selection = gtk_tree_view_get_selection
			(GTK_TREE_VIEW(matcher.cond_list_view));
	
	if (!gtk_tree_selection_get_selected(selection, &model, &row))
		return;
		
	gtk_tree_model_get(model, &row, 
			   PREFS_MATCHER_COND_VALID, &is_valid,
			   -1);

	if (!is_valid)
		return;

	gtk_list_store_remove(GTK_LIST_STORE(model), &row);		

	prefs_matcher_reset_condition();
}

/*!
 *\brief	Signal handler for 'move up' button
 */
static void prefs_matcher_up(void)
{
	GtkTreePath *prev, *sel, *try;
	GtkTreeIter isel;
	GtkListStore *store = NULL;
	GtkTreeModel *model = NULL;
	GtkTreeIter iprev;
	
	if (!gtk_tree_selection_get_selected
		(gtk_tree_view_get_selection
			(GTK_TREE_VIEW(matcher.cond_list_view)),
		 &model,	
		 &isel))
		return;
	store = (GtkListStore *)model;
	sel = gtk_tree_model_get_path(GTK_TREE_MODEL(store), &isel);
	if (!sel)
		return;
	
	/* no move if we're at row 0 or 1, looks phony, but other
	 * solutions are more convoluted... */
	try = gtk_tree_path_copy(sel);
	if (!gtk_tree_path_prev(try) || !gtk_tree_path_prev(try)) {
		gtk_tree_path_free(try);
		gtk_tree_path_free(sel);
		return;
	}
	gtk_tree_path_free(try);

	prev = gtk_tree_path_copy(sel);		
	if (gtk_tree_path_prev(prev)) {
		gtk_tree_model_get_iter(GTK_TREE_MODEL(store),
					&iprev, prev);
		gtk_list_store_swap(store, &iprev, &isel);
		/* XXX: GTK2 select row?? */
	}

	gtk_tree_path_free(sel);
	gtk_tree_path_free(prev);
}

/*!
 *\brief	Signal handler for 'move down' button
 */
static void prefs_matcher_down(void)
{
	GtkListStore *store = NULL;
	GtkTreeModel *model = NULL;
	GtkTreeIter next, sel;
	GtkTreePath *try;
	
	if (!gtk_tree_selection_get_selected
		(gtk_tree_view_get_selection
			(GTK_TREE_VIEW(matcher.cond_list_view)),
		 &model,
		 &sel))
		return;
	store = (GtkListStore *)model;
	try = gtk_tree_model_get_path(GTK_TREE_MODEL(store), &sel);
	if (!try) 
		return;
	
	/* move when not at row 0 ... */
	if (gtk_tree_path_prev(try)) {
		next = sel;
		if (gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &next))
			gtk_list_store_swap(store, &next, &sel);
	}
		
	gtk_tree_path_free(try);
}

/*!
 *\brief	Helper function that allows us to replace the 'Value' entry box
 *		by another widget.
 *
 *\param	old_widget Widget that needs to be removed.
 *\param	new_widget Replacement widget
 */
static void prefs_matcher_set_value_widget(GtkWidget *old_widget, 
					   GtkWidget *new_widget)
{
	/* TODO: find out why the following spews harmless "parent errors" */

	/* NOTE: we first need to bump up the refcount of the old_widget,
	 * because the gtkut_container_remove() will otherwise destroy it */
	gtk_widget_ref(old_widget);
	gtkut_container_remove(GTK_CONTAINER(matcher.criteria_table), old_widget);
	gtk_widget_show(new_widget);
	gtk_widget_set_size_request(new_widget, 200, -1);
	gtk_table_attach(GTK_TABLE(matcher.criteria_table), new_widget, 
			 2, 3, 1, 2, 
			 GTK_FILL | GTK_SHRINK | GTK_EXPAND, 
			 0, 0, 0);
}

static void prefs_matcher_disable_widget(GtkWidget* widget)
{
	g_return_if_fail( widget != NULL);

	gtk_widget_set_sensitive(widget, FALSE);
	gtk_widget_hide(widget);
}

static void prefs_matcher_enable_widget(GtkWidget* widget)
{
	g_return_if_fail( widget != NULL);

	gtk_widget_set_sensitive(widget, TRUE);
	gtk_widget_show(widget);
}

/*!
 *\brief	Change widgets depending on the selected condition
 *
 *\param	list List widget
 *\param	widget Not used
 *\param	user_data Not used	
 */
static void prefs_matcher_criteria_select(GtkList *list,
					  GtkWidget *widget,
					  gpointer user_data)
{
	gint value, old_value;

	old_value = matcher.selected_criteria;
	matcher.selected_criteria = value = get_sel_from_list
		(GTK_LIST(matcher.criteria_list));

	if (old_value == matcher.selected_criteria)
		return;

	/* CLAWS: the value widget is currently either the color label combo box,
	 * or a GtkEntry, so kiss for now */
	if (matcher.selected_criteria == CRITERIA_COLORLABEL) { 
		prefs_matcher_set_value_widget(matcher.value_entry, 
					       matcher.color_optmenu);
	} else if (old_value == CRITERIA_COLORLABEL) {
		prefs_matcher_set_value_widget(matcher.color_optmenu,
					       matcher.value_entry);
	}					       

	switch (value) {
	case CRITERIA_ALL:
		prefs_matcher_disable_widget(matcher.header_combo);
		prefs_matcher_disable_widget(matcher.header_label);
		prefs_matcher_disable_widget(matcher.header_addr_combo);
		prefs_matcher_disable_widget(matcher.header_addr_label);
		prefs_matcher_disable_widget(matcher.value_label);
		prefs_matcher_disable_widget(matcher.value_entry);
		prefs_matcher_disable_widget(matcher.addressbook_folder_label);
		prefs_matcher_disable_widget(matcher.addressbook_folder_combo);
		prefs_matcher_disable_widget(matcher.predicate_label);
		prefs_matcher_disable_widget(matcher.predicate_combo);
		prefs_matcher_disable_widget(matcher.predicate_flag_combo);
		prefs_matcher_disable_widget(matcher.case_checkbtn);
		prefs_matcher_disable_widget(matcher.regexp_checkbtn);
		prefs_matcher_disable_widget(matcher.test_btn);
		prefs_matcher_disable_widget(matcher.addressbook_select_btn);
		break;

	case CRITERIA_UNREAD:
	case CRITERIA_NEW:
	case CRITERIA_MARKED:
	case CRITERIA_DELETED:
	case CRITERIA_REPLIED:
	case CRITERIA_FORWARDED:
	case CRITERIA_LOCKED:
	case CRITERIA_SPAM:
	case CRITERIA_PARTIAL:
	case CRITERIA_IGNORE_THREAD:
		prefs_matcher_disable_widget(matcher.header_combo);
		prefs_matcher_disable_widget(matcher.header_label);
		prefs_matcher_disable_widget(matcher.header_addr_combo);
		prefs_matcher_disable_widget(matcher.header_addr_label);
		prefs_matcher_disable_widget(matcher.value_label);
		prefs_matcher_disable_widget(matcher.value_entry);
		prefs_matcher_disable_widget(matcher.addressbook_folder_label);
		prefs_matcher_disable_widget(matcher.addressbook_folder_combo);
		prefs_matcher_enable_widget(matcher.predicate_label);
		prefs_matcher_disable_widget(matcher.predicate_combo);
		prefs_matcher_enable_widget(matcher.predicate_flag_combo);
		prefs_matcher_disable_widget(matcher.case_checkbtn);
		prefs_matcher_disable_widget(matcher.regexp_checkbtn);
		prefs_matcher_disable_widget(matcher.test_btn);
		prefs_matcher_disable_widget(matcher.addressbook_select_btn);
		break;
		
	case CRITERIA_COLORLABEL:
		prefs_matcher_disable_widget(matcher.header_combo);
		prefs_matcher_disable_widget(matcher.header_label);
		prefs_matcher_disable_widget(matcher.header_addr_combo);
		prefs_matcher_disable_widget(matcher.header_addr_label);
		prefs_matcher_enable_widget(matcher.value_label);
		prefs_matcher_disable_widget(matcher.value_entry);
		prefs_matcher_disable_widget(matcher.addressbook_folder_label);
		prefs_matcher_disable_widget(matcher.addressbook_folder_combo);
		prefs_matcher_enable_widget(matcher.predicate_label);
		prefs_matcher_disable_widget(matcher.predicate_combo);
		prefs_matcher_enable_widget(matcher.predicate_flag_combo);
		prefs_matcher_disable_widget(matcher.case_checkbtn);
		prefs_matcher_disable_widget(matcher.regexp_checkbtn);
		prefs_matcher_disable_widget(matcher.test_btn);
		prefs_matcher_disable_widget(matcher.addressbook_select_btn);
		break;

	case CRITERIA_SUBJECT:
	case CRITERIA_FROM:
	case CRITERIA_TO:
	case CRITERIA_CC:
	case CRITERIA_TO_OR_CC:
	case CRITERIA_NEWSGROUPS:
	case CRITERIA_INREPLYTO:
	case CRITERIA_REFERENCES:
	case CRITERIA_HEADERS_PART:
	case CRITERIA_BODY_PART:
	case CRITERIA_MESSAGE:
		prefs_matcher_disable_widget(matcher.header_combo);
		prefs_matcher_disable_widget(matcher.header_label);
		prefs_matcher_disable_widget(matcher.header_addr_combo);
		prefs_matcher_disable_widget(matcher.header_addr_label);
		prefs_matcher_enable_widget(matcher.value_label);
		prefs_matcher_enable_widget(matcher.value_entry);
		prefs_matcher_disable_widget(matcher.addressbook_folder_label);
		prefs_matcher_disable_widget(matcher.addressbook_folder_combo);
		prefs_matcher_enable_widget(matcher.predicate_label);
		prefs_matcher_enable_widget(matcher.predicate_combo);
		prefs_matcher_disable_widget(matcher.predicate_flag_combo);
		prefs_matcher_enable_widget(matcher.case_checkbtn);
		prefs_matcher_enable_widget(matcher.regexp_checkbtn);
		prefs_matcher_disable_widget(matcher.test_btn);
		prefs_matcher_disable_widget(matcher.addressbook_select_btn);
		break;

	case CRITERIA_TEST:
		prefs_matcher_disable_widget(matcher.header_combo);
		prefs_matcher_disable_widget(matcher.header_label);
		prefs_matcher_disable_widget(matcher.header_addr_combo);
		prefs_matcher_disable_widget(matcher.header_addr_label);
		prefs_matcher_enable_widget(matcher.value_label);
		prefs_matcher_enable_widget(matcher.value_entry);
		prefs_matcher_disable_widget(matcher.addressbook_folder_label);
		prefs_matcher_disable_widget(matcher.addressbook_folder_combo);
		prefs_matcher_enable_widget(matcher.predicate_label);
		prefs_matcher_disable_widget(matcher.predicate_combo);
		prefs_matcher_enable_widget(matcher.predicate_flag_combo);
		prefs_matcher_disable_widget(matcher.case_checkbtn);
		prefs_matcher_disable_widget(matcher.regexp_checkbtn);
		prefs_matcher_enable_widget(matcher.test_btn);
		prefs_matcher_disable_widget(matcher.addressbook_select_btn);
		break;

	case CRITERIA_AGE_GREATER:
	case CRITERIA_AGE_LOWER:
	case CRITERIA_SCORE_GREATER:
	case CRITERIA_SCORE_LOWER:
	case CRITERIA_SCORE_EQUAL:
	case CRITERIA_SIZE_GREATER:
	case CRITERIA_SIZE_SMALLER:
	case CRITERIA_SIZE_EQUAL:
		prefs_matcher_disable_widget(matcher.header_combo);
		prefs_matcher_disable_widget(matcher.header_label);
		prefs_matcher_disable_widget(matcher.header_addr_combo);
		prefs_matcher_disable_widget(matcher.header_addr_label);
		prefs_matcher_enable_widget(matcher.value_label);
		prefs_matcher_enable_widget(matcher.value_entry);
		prefs_matcher_disable_widget(matcher.addressbook_folder_label);
		prefs_matcher_disable_widget(matcher.addressbook_folder_combo);
		prefs_matcher_disable_widget(matcher.predicate_label);
		prefs_matcher_disable_widget(matcher.predicate_combo);
		prefs_matcher_disable_widget(matcher.predicate_flag_combo);
		prefs_matcher_disable_widget(matcher.case_checkbtn);
		prefs_matcher_disable_widget(matcher.regexp_checkbtn);
		prefs_matcher_disable_widget(matcher.test_btn);
		prefs_matcher_disable_widget(matcher.addressbook_select_btn);
		break;

	case CRITERIA_HEADER:
		prefs_matcher_enable_widget(matcher.header_combo);
		prefs_matcher_enable_widget(matcher.header_label);
		prefs_matcher_disable_widget(matcher.header_addr_combo);
		prefs_matcher_disable_widget(matcher.header_addr_label);
		prefs_matcher_enable_widget(matcher.value_label);
		prefs_matcher_enable_widget(matcher.value_entry);
		prefs_matcher_disable_widget(matcher.addressbook_folder_label);
		prefs_matcher_disable_widget(matcher.addressbook_folder_combo);
		prefs_matcher_enable_widget(matcher.predicate_label);
		prefs_matcher_enable_widget(matcher.predicate_combo);
		prefs_matcher_disable_widget(matcher.predicate_flag_combo);
		prefs_matcher_enable_widget(matcher.case_checkbtn);
		prefs_matcher_enable_widget(matcher.regexp_checkbtn);
		prefs_matcher_disable_widget(matcher.test_btn);
		prefs_matcher_disable_widget(matcher.addressbook_select_btn);
		break;

	case CRITERIA_FOUND_IN_ADDRESSBOOK:
		prefs_matcher_disable_widget(matcher.header_combo);
		prefs_matcher_disable_widget(matcher.header_label);
		prefs_matcher_enable_widget(matcher.header_addr_combo);
		prefs_matcher_enable_widget(matcher.header_addr_label);
		prefs_matcher_disable_widget(matcher.value_label);
		prefs_matcher_disable_widget(matcher.value_entry);
		prefs_matcher_enable_widget(matcher.addressbook_folder_label);
		prefs_matcher_enable_widget(matcher.addressbook_folder_combo);
		prefs_matcher_enable_widget(matcher.predicate_label);
		prefs_matcher_disable_widget(matcher.predicate_combo);
		prefs_matcher_enable_widget(matcher.predicate_flag_combo);
		prefs_matcher_disable_widget(matcher.case_checkbtn);
		prefs_matcher_disable_widget(matcher.regexp_checkbtn);
		prefs_matcher_disable_widget(matcher.test_btn);
		prefs_matcher_enable_widget(matcher.addressbook_select_btn);
		break;
	}
}

/*!
 *\brief	Handle key press
 *
 *\param	widget Widget receiving key press
 *\param	event Key event
 *\param	data User data
 */
static gboolean prefs_matcher_key_pressed(GtkWidget *widget, GdkEventKey *event,
				     gpointer data)
{
	if (event && event->keyval == GDK_Escape) {
		prefs_matcher_cancel();
		return TRUE;		
	}
	return FALSE;
}

/*!
 *\brief	Cancel matcher dialog
 */
static void prefs_matcher_cancel(void)
{
	gtk_widget_hide(matcher.window);
	inc_unlock();
}

/*!
 *\brief	Accept current matchers
 */
static void prefs_matcher_ok(void)
{
	MatcherList *matchers;
	MatcherProp *matcherprop;
	AlertValue val;
	gchar *matcher_str = NULL;
	gchar *str = NULL;
	gint row = 1;
	GtkTreeModel *model;
	GtkTreeIter iter;

	matchers = prefs_matcher_get_list();

	if (matchers != NULL) {
		matcherprop = prefs_matcher_dialog_to_matcher();
		if (matcherprop != NULL) {
			str = matcherprop_to_string(matcherprop);
			matcherprop_free(matcherprop);
			if (strcmp(str, "all") != 0) {
				model = gtk_tree_view_get_model(GTK_TREE_VIEW
						(matcher.cond_list_view));

				while (gtk_tree_model_iter_nth_child(model, &iter, NULL, row)) {
					gtk_tree_model_get(model, &iter,
							   PREFS_MATCHER_COND, &matcher_str,
							   -1);
					if (matcher_str && strcmp(matcher_str, str) == 0) 
						break;
					row++;
					g_free(matcher_str);
					matcher_str = NULL;
				}

				if (!matcher_str || strcmp(matcher_str, str) != 0) {
	                        	val = alertpanel(_("Entry not saved"),
       		                        	 _("The entry was not saved.\nClose anyway?"),
               		                	 GTK_STOCK_CLOSE, _("+_Continue editing"), NULL);
					if (G_ALERTDEFAULT != val) {
						g_free(matcher_str);						 
	        	                        g_free(str);
						return;
					}
				}
				g_free(matcher_str);
			}
		}
                g_free(str);
		gtk_widget_hide(matcher.window);
		inc_unlock();
		if (matchers_callback != NULL)
			matchers_callback(matchers);
		matcherlist_free(matchers);
	}
}

/*!
 *\brief	Called when closing dialog box
 *
 *\param	widget Dialog widget
 *\param	event Event info
 *\param	data User data
 *
 *\return	gint TRUE
 */
static gint prefs_matcher_deleted(GtkWidget *widget, GdkEventAny *event,
				  gpointer data)
{
	prefs_matcher_cancel();
	return TRUE;
}

/*
 * Strings describing test format strings
 * 
 * When adding new lines, remember to put 2 strings for each line
 */
static gchar *test_desc_strings[] = {
	"%%",	N_("literal %"),
	"%s",	N_("Subject"),
	"%f",	N_("From"),
	"%t",	N_("To"),
	"%c",	N_("Cc"),
	"%d",	N_("Date"),
	"%i",	N_("Message-ID"),
	"%n",	N_("Newsgroups"),
	"%r",	N_("References"),
	"%F",	N_("filename (should not be modified)"),
	"\\n",	N_("new line"),
	"\\",	N_("escape character for quotes"),
	"\\\"", N_("quote character"),
	NULL,   NULL
};

static DescriptionWindow test_desc_win = { 
	NULL,
        NULL, 
        2,
        N_("Match Type: 'Test'"),
	N_("'Test' allows you to test a message or message element "
	   "using an external program or script. The program will "
	   "return either 0 or 1.\n\n"
	   "The following symbols can be used:"),
        test_desc_strings
};



/*!
 *\brief	Show Test action's info
 */
void prefs_matcher_test_info(void)
{
	description_window_create(&test_desc_win);
}

void prefs_matcher_addressbook_select(void)
{
	gchar *folderpath = NULL;
	gboolean ret = FALSE;

	folderpath = (gchar *) gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(matcher.addressbook_folder_combo)->entry));
	ret = addressbook_folder_selection(&folderpath);
	if ( ret != FALSE && folderpath != NULL)
		gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(matcher.addressbook_folder_combo)->entry), folderpath);
}


/*
 * list view
 */

static GtkListStore* prefs_matcher_create_data_store(void)
{
	return gtk_list_store_new(N_PREFS_MATCHER_COLUMNS,
				  G_TYPE_STRING,	
				  G_TYPE_BOOLEAN,
				  -1);
}

static void prefs_matcher_list_view_insert_matcher(GtkWidget *list_view,
						   GtkTreeIter *row_iter,
						   const gchar *matcher,
						   gboolean is_valid) 
{
	GtkTreeIter iter;
	GtkListStore *list_store = GTK_LIST_STORE(gtk_tree_view_get_model
					(GTK_TREE_VIEW(list_view)));

	if (row_iter == NULL) {
		/* append new */
		gtk_list_store_append(list_store, &iter);
	} else {
		/* change existing */
		iter = *row_iter;
	}

	gtk_list_store_set(list_store, &iter,
			   PREFS_MATCHER_COND, matcher,
			   PREFS_MATCHER_COND_VALID, is_valid,
			   -1);
}

static GtkWidget *prefs_matcher_list_view_create(void)
{
	GtkTreeView *list_view;
	GtkTreeSelection *selector;
	GtkTreeModel *model;

	model = GTK_TREE_MODEL(prefs_matcher_create_data_store());
	list_view = GTK_TREE_VIEW(gtk_tree_view_new_with_model(model));
	g_object_unref(model);	
	
	gtk_tree_view_set_rules_hint(list_view, prefs_common.use_stripes_everywhere);
	
	selector = gtk_tree_view_get_selection(list_view);
	gtk_tree_selection_set_mode(selector, GTK_SELECTION_BROWSE);
	gtk_tree_selection_set_select_function(selector, prefs_matcher_selected,
					       NULL, NULL);

	/* create the columns */
	prefs_matcher_create_list_view_columns(GTK_WIDGET(list_view));

	return GTK_WIDGET(list_view);
}

static void prefs_matcher_create_list_view_columns(GtkWidget *list_view)
{
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;

	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes
		(_("Current condition rules"),
		 renderer,
		 "text", PREFS_MATCHER_COND,
		 NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(list_view), column);		
}

static gboolean prefs_matcher_selected(GtkTreeSelection *selector,
				       GtkTreeModel *model, 
				       GtkTreePath *path,
				       gboolean currently_selected,
				       gpointer data)
{
	gchar *matcher_str;
	MatcherProp *prop;
	gboolean negative_cond;
	gint criteria;
	GtkTreeIter iter;
	gboolean is_valid;

	if (currently_selected)
		return TRUE;

	if (!gtk_tree_model_get_iter(model, &iter, path))
		return TRUE;

	gtk_tree_model_get(model, &iter, 
			   PREFS_MATCHER_COND_VALID,  &is_valid,
			   PREFS_MATCHER_COND, &matcher_str,
			   -1);
	
	if (!is_valid) {
		g_free(matcher_str);
		prefs_matcher_reset_condition();
		return TRUE;
	}

	negative_cond = FALSE;

	prop = matcher_parser_get_prop(matcher_str);
	if (prop == NULL) {
		g_free(matcher_str);
		return TRUE;
	}		

	criteria = prefs_matcher_get_criteria_from_matching(prop->criteria);
	if (criteria != -1)
		gtk_list_select_item(GTK_LIST(matcher.criteria_list),
				     criteria);

	switch(prop->criteria) {
	case MATCHCRITERIA_NOT_UNREAD:
	case MATCHCRITERIA_NOT_NEW:
	case MATCHCRITERIA_NOT_MARKED:
	case MATCHCRITERIA_NOT_DELETED:
	case MATCHCRITERIA_NOT_REPLIED:
	case MATCHCRITERIA_NOT_FORWARDED:
	case MATCHCRITERIA_NOT_LOCKED:
	case MATCHCRITERIA_NOT_SPAM:
	case MATCHCRITERIA_NOT_PARTIAL:
	case MATCHCRITERIA_NOT_COLORLABEL:
	case MATCHCRITERIA_NOT_IGNORE_THREAD:
	case MATCHCRITERIA_NOT_SUBJECT:
	case MATCHCRITERIA_NOT_FROM:
	case MATCHCRITERIA_NOT_TO:
	case MATCHCRITERIA_NOT_CC:
	case MATCHCRITERIA_NOT_TO_AND_NOT_CC:
	case MATCHCRITERIA_NOT_NEWSGROUPS:
	case MATCHCRITERIA_NOT_INREPLYTO:
	case MATCHCRITERIA_NOT_REFERENCES:
	case MATCHCRITERIA_NOT_HEADER:
	case MATCHCRITERIA_NOT_HEADERS_PART:
	case MATCHCRITERIA_NOT_MESSAGE:
	case MATCHCRITERIA_NOT_BODY_PART:
	case MATCHCRITERIA_NOT_TEST:
	case MATCHCRITERIA_NOT_FOUND_IN_ADDRESSBOOK:
		negative_cond = TRUE;
		break;
	}
	
	switch(prop->criteria) {
	case MATCHCRITERIA_ALL:
		break;

	case MATCHCRITERIA_NOT_SUBJECT:
	case MATCHCRITERIA_NOT_FROM:
	case MATCHCRITERIA_NOT_TO:
	case MATCHCRITERIA_NOT_CC:
	case MATCHCRITERIA_NOT_TO_AND_NOT_CC:
	case MATCHCRITERIA_NOT_NEWSGROUPS:
	case MATCHCRITERIA_NOT_INREPLYTO:
	case MATCHCRITERIA_NOT_REFERENCES:
	case MATCHCRITERIA_NOT_HEADERS_PART:
	case MATCHCRITERIA_NOT_BODY_PART:
	case MATCHCRITERIA_NOT_MESSAGE:
	case MATCHCRITERIA_NOT_TEST:
	case MATCHCRITERIA_SUBJECT:
	case MATCHCRITERIA_FROM:
	case MATCHCRITERIA_TO:
	case MATCHCRITERIA_CC:
	case MATCHCRITERIA_TO_OR_CC:
	case MATCHCRITERIA_NEWSGROUPS:
	case MATCHCRITERIA_INREPLYTO:
	case MATCHCRITERIA_REFERENCES:
	case MATCHCRITERIA_HEADERS_PART:
	case MATCHCRITERIA_BODY_PART:
	case MATCHCRITERIA_MESSAGE:
	case MATCHCRITERIA_TEST:
		gtk_entry_set_text(GTK_ENTRY(matcher.value_entry), prop->expr);
		break;

	case MATCHCRITERIA_FOUND_IN_ADDRESSBOOK:
	case MATCHCRITERIA_NOT_FOUND_IN_ADDRESSBOOK:
	{
		gchar *header;
		gchar *expr;

		/* matcher expressions contain UNtranslated "Any"/"All",
		  select the relevant translated combo item */
		if (strcasecmp(prop->header, "All") == 0)
			header = (gchar*)Q_("Filtering Matcher Menu|All");
		else
			if (strcasecmp(prop->header, "Any") == 0)
				header = _("Any");
			else
				header = prop->header;
		if (strcasecmp(prop->expr, "Any") == 0)
			expr = _("Any");
		else
			expr = prop->expr;

		gtk_entry_set_text(GTK_ENTRY(matcher.header_addr_entry), header);
		gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(matcher.addressbook_folder_combo)->entry), expr);
		break;
	}

	case MATCHCRITERIA_AGE_GREATER:
	case MATCHCRITERIA_AGE_LOWER:
	case MATCHCRITERIA_SCORE_GREATER:
	case MATCHCRITERIA_SCORE_LOWER:
	case MATCHCRITERIA_SCORE_EQUAL:
	case MATCHCRITERIA_SIZE_GREATER:
	case MATCHCRITERIA_SIZE_SMALLER:
	case MATCHCRITERIA_SIZE_EQUAL:
		gtk_entry_set_text(GTK_ENTRY(matcher.value_entry), itos(prop->value));
		break;

	case MATCHCRITERIA_NOT_COLORLABEL:
	case MATCHCRITERIA_COLORLABEL:
		gtk_option_menu_set_history(GTK_OPTION_MENU(matcher.color_optmenu),
					    prop->value);
		break;

	case MATCHCRITERIA_NOT_HEADER:
	case MATCHCRITERIA_HEADER:
		gtk_entry_set_text(GTK_ENTRY(matcher.header_entry), prop->header);
		gtk_entry_set_text(GTK_ENTRY(matcher.value_entry), prop->expr);
		break;
	}

	if (negative_cond) {
		gtk_list_select_item(GTK_LIST(matcher.predicate_list), 1);
		gtk_list_select_item(GTK_LIST(matcher.predicate_flag_list), 1);
	} else {
		gtk_list_select_item(GTK_LIST(matcher.predicate_list), 0);
		gtk_list_select_item(GTK_LIST(matcher.predicate_flag_list), 0);
	}
	
	switch(prop->matchtype) {
	case MATCHTYPE_MATCH:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(matcher.regexp_checkbtn), FALSE);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(matcher.case_checkbtn), TRUE);
		break;

	case MATCHTYPE_MATCHCASE:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(matcher.regexp_checkbtn), FALSE);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(matcher.case_checkbtn), FALSE);
		break;

	case MATCHTYPE_REGEXP:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(matcher.regexp_checkbtn), TRUE);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(matcher.case_checkbtn), TRUE);
		break;

	case MATCHTYPE_REGEXPCASE:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(matcher.regexp_checkbtn), TRUE);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(matcher.case_checkbtn), FALSE);
		break;
	}

	g_free(matcher_str);
	return TRUE;
}

