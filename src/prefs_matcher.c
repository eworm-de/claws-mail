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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "defs.h"

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "intl.h"
#include "main.h"
#include "prefs.h"
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

static struct Matcher {
	GtkWidget *window;

	GtkWidget *ok_btn;

	GtkWidget *predicate_combo;
	GtkWidget *predicate_flag_combo;
	GtkWidget *header_combo;

	GtkWidget *criteria_list;

	GtkWidget *predicate_list;
	GtkWidget *predicate_label;
	GtkWidget *predicate_flag_list;

	GtkWidget *bool_op_list;

	GtkWidget *header_entry;
	GtkWidget *header_label;
	GtkWidget *value_entry;
	GtkWidget *value_label;
	GtkWidget *case_chkbtn;
	GtkWidget *regexp_chkbtn;

	GtkWidget *exec_btn;

	GtkWidget *cond_clist;
} matcher;

/* choice in the list */

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

	CRITERIA_SCORE_GREATER = 21,
	CRITERIA_SCORE_LOWER = 22,

	CRITERIA_EXECUTE = 23
};

enum {
	BOOL_OP_OR = 0,
	BOOL_OP_AND = 1
};

gchar * bool_op_text [] = {
	"or", "and"
};

enum {
	PREDICATE_CONTAINS = 0,
	PREDICATE_DOES_NOT_CONTAIN = 1
};

gchar * predicate_text [] = {
	"contains", "does not contain"
};

enum {
	PREDICATE_FLAG_ENABLED = 0,
	PREDICATE_FLAG_DISABLED = 1
};

gchar * predicate_flag_text [] = {
	"yes", "no"
};

gchar * criteria_text [] = {
	"All messages", "Subject",
	"From", "To", "Cc", "To or Cc",
	"Newsgroups", "In reply to", "References",
	"Age greater than", "Age lower than",
	"Header", "Headers part",
	"Body part", "Whole message",
	"Unread flag", "New flag",
	"Marked flag", "Deleted flag",
	"Replied flag", "Forwarded flag",
	"Score greater than", "Score lower than",
	"Execute"
};

static gint get_sel_from_list(GtkList * list)
{
	gint row = 0;
	void * sel;
	GList * child;

	sel = list->selection->data;
	for(child = list->children ; child != NULL ;
	    child = g_list_next(child)) {
		if (child->data == sel)
			return row;
		row ++;
	}
	
	return row;
}

static PrefsMatcherSignal * matchers_callback;

#define VSPACING		12
#define VSPACING_NARROW		4
#define DEFAULT_ENTRY_WIDTH	80
#define PREFSBUFSIZE		1024

/* widget creating functions */
static void prefs_matcher_create	(void);

static void prefs_matcher_set_dialog	(MatcherList * matchers);

/*
static void prefs_matcher_set_list	(void);
static gint prefs_matcher_clist_set_row	(gint	 row);
*/

/* callback functions */

/*
static void prefs_matcher_select_dest_cb	(void);
*/
static void prefs_matcher_register_cb	(void);
static void prefs_matcher_substitute_cb	(void);
static void prefs_matcher_delete_cb	(void);
static void prefs_matcher_up		(void);
static void prefs_matcher_down		(void);
static void prefs_matcher_select	(GtkCList	*clist,
					 gint		 row,
					 gint		 column,
					 GdkEvent	*event);

static void prefs_matcher_key_pressed	(GtkWidget	*widget,
					 GdkEventKey	*event,
					 gpointer	 data);
static void prefs_matcher_ok		(void);
static void prefs_matcher_cancel		(void);
static gint prefs_matcher_deleted(GtkWidget *widget, GdkEventAny *event,
				  gpointer data);
static void prefs_matcher_criteria_select(GtkList *list,
					  GtkWidget *widget,
					  gpointer user_data);
static MatcherList * prefs_matcher_get_list(void);
static void prefs_matcher_exec_info_create(void);

void prefs_matcher_open(MatcherList * matchers, PrefsMatcherSignal * cb)
{
	inc_autocheck_timer_remove();

	if (!matcher.window) {
		prefs_matcher_create();
	}

	manage_window_set_transient(GTK_WINDOW(matcher.window));
	gtk_widget_grab_focus(matcher.ok_btn);

	matchers_callback = cb;

	prefs_matcher_set_dialog(matchers);

	gtk_widget_show(matcher.window);
}

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
	GtkWidget *table1;

	GtkWidget *hbox1;

	GtkWidget *header_combo;
	GtkWidget *header_entry;
	GtkWidget *header_label;
	GtkWidget *criteria_combo;
	GtkWidget *criteria_list;
	GtkWidget *criteria_label;
	GtkWidget *value_label;
	GtkWidget *value_entry;
	GtkWidget *predicate_combo;
	GtkWidget *predicate_list;
	GtkWidget *predicate_flag_combo;
	GtkWidget *predicate_flag_list;
	GtkWidget *predicate_label;
	GtkWidget *bool_op_combo;
	GtkWidget *bool_op_list;
	GtkWidget *bool_op_label;

	GtkWidget *regexp_chkbtn;
	GtkWidget *case_chkbtn;

	GtkWidget *reg_hbox;
	GtkWidget *btn_hbox;
	GtkWidget *arrow;
	GtkWidget *reg_btn;
	GtkWidget *subst_btn;
	GtkWidget *del_btn;

	GtkWidget *cond_hbox;
	GtkWidget *cond_scrolledwin;
	GtkWidget *cond_clist;

	GtkWidget *btn_vbox;
	GtkWidget *up_btn;
	GtkWidget *down_btn;

	GtkWidget *exec_btn;

	GList *combo_items;
	gint i;

	gchar *title[] = {_("Registered rules")};

	debug_print(_("Creating matcher setting window...\n"));

	window = gtk_window_new (GTK_WINDOW_DIALOG);
	gtk_container_set_border_width (GTK_CONTAINER (window), 8);
	gtk_window_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);
	gtk_window_set_modal (GTK_WINDOW (window), TRUE);
	gtk_window_set_policy (GTK_WINDOW (window), FALSE, TRUE, FALSE);

	vbox = gtk_vbox_new (FALSE, 6);
	gtk_widget_show (vbox);
	gtk_container_add (GTK_CONTAINER (window), vbox);

	gtkut_button_set_create(&confirm_area, &ok_btn, _("OK"),
				&cancel_btn, _("Cancel"), NULL, NULL);
	gtk_widget_show (confirm_area);
	gtk_box_pack_end (GTK_BOX(vbox), confirm_area, FALSE, FALSE, 0);
	gtk_widget_grab_default (ok_btn);

	gtk_window_set_title (GTK_WINDOW(window),
			      _("Condition setting"));
	gtk_signal_connect (GTK_OBJECT(window), "delete_event",
			    GTK_SIGNAL_FUNC(prefs_matcher_deleted), NULL);
	gtk_signal_connect (GTK_OBJECT(window), "key_press_event",
			    GTK_SIGNAL_FUNC(prefs_matcher_key_pressed), NULL);
	gtk_signal_connect (GTK_OBJECT(window), "focus_in_event",
			    GTK_SIGNAL_FUNC(manage_window_focus_in), NULL);
	gtk_signal_connect (GTK_OBJECT(window), "focus_out_event",
			    GTK_SIGNAL_FUNC(manage_window_focus_out), NULL);
	gtk_signal_connect (GTK_OBJECT(ok_btn), "clicked",
			    GTK_SIGNAL_FUNC(prefs_matcher_ok), NULL);
	gtk_signal_connect (GTK_OBJECT(cancel_btn), "clicked",
			    GTK_SIGNAL_FUNC(prefs_matcher_cancel), NULL);

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_box_pack_start (GTK_BOX (vbox), vbox1, TRUE, TRUE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), 2);

	table1 = gtk_table_new (2, 4, FALSE);
	gtk_widget_show (table1);

	gtk_box_pack_start (GTK_BOX (vbox1), table1, FALSE, FALSE, 0);
	gtk_table_set_row_spacings (GTK_TABLE (table1), 8);
	gtk_table_set_col_spacings (GTK_TABLE (table1), 8);

	/* criteria combo box */

	criteria_label = gtk_label_new (_("Match type"));
	gtk_widget_show (criteria_label);
	gtk_misc_set_alignment (GTK_MISC (criteria_label), 0, 0.5);
	gtk_table_attach (GTK_TABLE (table1), criteria_label, 0, 1, 0, 1,
			  GTK_FILL, 0, 0, 0);

	criteria_combo = gtk_combo_new ();
	gtk_widget_show (criteria_combo);

	combo_items = NULL;

	for(i = 0 ; i < (gint) (sizeof(criteria_text) / sizeof(gchar *)) ;
	    i++) {
		combo_items = g_list_append(combo_items,
					    (gpointer) _(criteria_text[i]));
	}
	gtk_combo_set_popdown_strings(GTK_COMBO(criteria_combo), combo_items);

	g_list_free(combo_items);

	gtk_widget_set_usize (criteria_combo, 120, -1);
	gtk_table_attach (GTK_TABLE (table1), criteria_combo, 0, 1, 1, 2,
			  0, 0, 0, 0);
	criteria_list = GTK_COMBO(criteria_combo)->list;
	gtk_signal_connect (GTK_OBJECT (criteria_list), "select-child",
			    GTK_SIGNAL_FUNC (prefs_matcher_criteria_select),
			    NULL);

	gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(criteria_combo)->entry),
			       FALSE);

	/* header name */

	header_label = gtk_label_new (_("Header name"));
	gtk_widget_show (header_label);
	gtk_misc_set_alignment (GTK_MISC (header_label), 0, 0.5);
	gtk_table_attach (GTK_TABLE (table1), header_label, 1, 2, 0, 1,
			  GTK_FILL, 0, 0, 0);

	header_combo = gtk_combo_new ();
	gtk_widget_show (header_combo);
	gtk_widget_set_usize (header_combo, 96, -1);
	gtkut_combo_set_items (GTK_COMBO (header_combo),
			       "Subject", "From", "To", "Cc", "Reply-To",
			       "Sender", "X-ML-Name", "X-List", "X-Sequence",
			       "X-Mailer",
			       NULL);
	gtk_table_attach (GTK_TABLE (table1), header_combo, 1, 2, 1, 2,
			  0, 0, 0, 0);
	header_entry = GTK_COMBO (header_combo)->entry;
	gtk_entry_set_editable (GTK_ENTRY (header_entry), TRUE);

	/* value */

	value_label = gtk_label_new (_("Value"));
	gtk_widget_show (value_label);
	gtk_misc_set_alignment (GTK_MISC (value_label), 0, 0.5);
	gtk_table_attach (GTK_TABLE (table1), value_label, 2, 3, 0, 1,
			  GTK_FILL | GTK_SHRINK | GTK_EXPAND, 0, 0, 0);

	value_entry = gtk_entry_new ();
	gtk_widget_show (value_entry);
	gtk_widget_set_usize (value_entry, 200, -1);
	gtk_table_attach (GTK_TABLE (table1), value_entry, 2, 3, 1, 2,
			  GTK_FILL | GTK_SHRINK | GTK_EXPAND, 0, 0, 0);

	exec_btn = gtk_button_new_with_label (_("Info ..."));
	gtk_widget_show (exec_btn);
	gtk_table_attach (GTK_TABLE (table1), exec_btn, 3, 4, 1, 2,
			  GTK_FILL | GTK_SHRINK | GTK_EXPAND, 0, 0, 0);
	gtk_signal_connect (GTK_OBJECT (exec_btn), "clicked",
			    GTK_SIGNAL_FUNC (prefs_matcher_exec_info),
			    NULL);

	/* predicate */

	vbox2 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox2);
	gtk_box_pack_start (GTK_BOX (vbox1), vbox2, FALSE, FALSE, 0);

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox1, FALSE, FALSE, 0);

	predicate_label = gtk_label_new (_("Predicate"));
	gtk_widget_show (predicate_label);
	gtk_box_pack_start (GTK_BOX (hbox1), predicate_label,
			    FALSE, FALSE, 0);

	predicate_combo = gtk_combo_new ();
	gtk_widget_show (predicate_combo);
	gtk_widget_set_usize (predicate_combo, 120, -1);
	predicate_list = GTK_COMBO(predicate_combo)->list;
	gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(predicate_combo)->entry),
			       FALSE);

	combo_items = NULL;

	for(i = 0 ; i < (gint) (sizeof(predicate_text) / sizeof(gchar *)) ;
	    i++) {
		combo_items = g_list_append(combo_items,
					    (gpointer) _(predicate_text[i]));
	}
	gtk_combo_set_popdown_strings(GTK_COMBO(predicate_combo), combo_items);

	g_list_free(combo_items);

	gtk_box_pack_start (GTK_BOX (hbox1), predicate_combo,
			    FALSE, FALSE, 0);

	/* predicate flag */

	predicate_flag_combo = gtk_combo_new ();
	gtk_widget_hide (predicate_flag_combo);
	gtk_widget_set_usize (predicate_flag_combo, 120, -1);
	predicate_flag_list = GTK_COMBO(predicate_flag_combo)->list;
	gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(predicate_flag_combo)->entry), FALSE);

	combo_items = NULL;

	for(i = 0 ; i < (gint) (sizeof(predicate_text) / sizeof(gchar *)) ;
	    i++) {
		combo_items = g_list_append(combo_items, (gpointer) _(predicate_flag_text[i]));
	}
	gtk_combo_set_popdown_strings(GTK_COMBO(predicate_flag_combo),
				      combo_items);

	g_list_free(combo_items);

	gtk_box_pack_start (GTK_BOX (hbox1), predicate_flag_combo,
			    FALSE, FALSE, 0);

	vbox3 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox3);
	gtk_box_pack_start (GTK_BOX (hbox1), vbox3, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON (vbox3, case_chkbtn, _("Case sensitive"));
	PACK_CHECK_BUTTON (vbox3, regexp_chkbtn, _("Use regexp"));

	/* register / substitute / delete */

	reg_hbox = gtk_hbox_new (FALSE, 4);
	gtk_widget_show (reg_hbox);
	gtk_box_pack_start (GTK_BOX (vbox1), reg_hbox, FALSE, FALSE, 0);

	arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_OUT);
	gtk_widget_show (arrow);
	gtk_box_pack_start (GTK_BOX (reg_hbox), arrow, FALSE, FALSE, 0);
	gtk_widget_set_usize (arrow, -1, 16);

	btn_hbox = gtk_hbox_new (TRUE, 4);
	gtk_widget_show (btn_hbox);
	gtk_box_pack_start (GTK_BOX (reg_hbox), btn_hbox, FALSE, FALSE, 0);

	reg_btn = gtk_button_new_with_label (_("Register"));
	gtk_widget_show (reg_btn);
	gtk_box_pack_start (GTK_BOX (btn_hbox), reg_btn, FALSE, TRUE, 0);
	gtk_signal_connect (GTK_OBJECT (reg_btn), "clicked",
			    GTK_SIGNAL_FUNC (prefs_matcher_register_cb), NULL);

	subst_btn = gtk_button_new_with_label (_(" Substitute "));
	gtk_widget_show (subst_btn);
	gtk_box_pack_start (GTK_BOX (btn_hbox), subst_btn, FALSE, TRUE, 0);
	gtk_signal_connect (GTK_OBJECT (subst_btn), "clicked",
			    GTK_SIGNAL_FUNC (prefs_matcher_substitute_cb),
			    NULL);

	del_btn = gtk_button_new_with_label (_("Delete"));
	gtk_widget_show (del_btn);
	gtk_box_pack_start (GTK_BOX (btn_hbox), del_btn, FALSE, TRUE, 0);
	gtk_signal_connect (GTK_OBJECT (del_btn), "clicked",
			    GTK_SIGNAL_FUNC (prefs_matcher_delete_cb), NULL);

	/* boolean operation */

	bool_op_label = gtk_label_new (_("Boolean Op"));
	gtk_misc_set_alignment (GTK_MISC (value_label), 0, 0.5);
	gtk_widget_show (bool_op_label);
	gtk_box_pack_start (GTK_BOX (btn_hbox), bool_op_label,
			    FALSE, FALSE, 0);

	bool_op_combo = gtk_combo_new ();
	gtk_widget_show (bool_op_combo);
	gtk_widget_set_usize (bool_op_combo, 50, -1);
	bool_op_list = GTK_COMBO(bool_op_combo)->list;
	gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(bool_op_combo)->entry),
			       FALSE);

	combo_items = NULL;

	for(i = 0 ; i < (gint) (sizeof(bool_op_text) / sizeof(gchar *)) ;
	    i++) {
		combo_items = g_list_append(combo_items,
					    (gpointer) _(bool_op_text[i]));
	}
	gtk_combo_set_popdown_strings(GTK_COMBO(bool_op_combo), combo_items);

	g_list_free(combo_items);

	gtk_box_pack_start (GTK_BOX (btn_hbox), bool_op_combo,
			    FALSE, FALSE, 0);

	cond_hbox = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (cond_hbox);
	gtk_box_pack_start (GTK_BOX (vbox1), cond_hbox, TRUE, TRUE, 0);

	cond_scrolledwin = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (cond_scrolledwin);
	gtk_widget_set_usize (cond_scrolledwin, -1, 150);
	gtk_box_pack_start (GTK_BOX (cond_hbox), cond_scrolledwin,
			    TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (cond_scrolledwin),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);

	cond_clist = gtk_clist_new_with_titles(1, title);
	gtk_widget_show (cond_clist);
	gtk_container_add (GTK_CONTAINER (cond_scrolledwin), cond_clist);
	gtk_clist_set_column_width (GTK_CLIST (cond_clist), 0, 80);
	gtk_clist_set_selection_mode (GTK_CLIST (cond_clist),
				      GTK_SELECTION_BROWSE);
	GTK_WIDGET_UNSET_FLAGS (GTK_CLIST (cond_clist)->column[0].button,
				GTK_CAN_FOCUS);
	gtk_signal_connect (GTK_OBJECT (cond_clist), "select_row",
			    GTK_SIGNAL_FUNC (prefs_matcher_select), NULL);

	btn_vbox = gtk_vbox_new (FALSE, 8);
	gtk_widget_show (btn_vbox);
	gtk_box_pack_start (GTK_BOX (cond_hbox), btn_vbox, FALSE, FALSE, 0);

	up_btn = gtk_button_new_with_label (_("Up"));
	gtk_widget_show (up_btn);
	gtk_box_pack_start (GTK_BOX (btn_vbox), up_btn, FALSE, FALSE, 0);
	gtk_signal_connect (GTK_OBJECT (up_btn), "clicked",
			    GTK_SIGNAL_FUNC (prefs_matcher_up), NULL);

	down_btn = gtk_button_new_with_label (_("Down"));
	gtk_widget_show (down_btn);
	gtk_box_pack_start (GTK_BOX (btn_vbox), down_btn, FALSE, FALSE, 0);
	gtk_signal_connect (GTK_OBJECT (down_btn), "clicked",
			    GTK_SIGNAL_FUNC (prefs_matcher_down), NULL);

	gtk_widget_show_all(window);

	matcher.window    = window;

	matcher.ok_btn = ok_btn;

	matcher.criteria_list = criteria_list;
	matcher.header_combo = header_combo;
	matcher.header_entry = header_entry;
	matcher.header_label = header_label;
	matcher.value_entry = value_entry;
	matcher.value_label = value_label;
	matcher.predicate_label = predicate_label;
	matcher.predicate_list = predicate_list;
	matcher.predicate_combo = predicate_combo;
	matcher.predicate_flag_list = predicate_flag_list;
	matcher.predicate_flag_combo = predicate_flag_combo;
	matcher.case_chkbtn = case_chkbtn;
	matcher.regexp_chkbtn = regexp_chkbtn;
	matcher.bool_op_list = bool_op_list;
	matcher.exec_btn = exec_btn;

	matcher.cond_clist   = cond_clist;
}

static gint prefs_matcher_clist_set_row(gint row, MatcherProp * prop)
{
	GtkCList *clist = GTK_CLIST(matcher.cond_clist);
	gchar * cond_str[1];
	gchar * matcher_str;

	if (prop == NULL) {
		cond_str[0] = _("(New)");
		return gtk_clist_append(clist, cond_str);
	}

	matcher_str = matcherprop_to_string(prop);
	cond_str[0] = matcher_str;
	if (row < 0)
		row = gtk_clist_append(clist, cond_str);
	else
		gtk_clist_set_text(clist, row, 0, cond_str[0]);
	g_free(matcher_str);

	return row;
}

static void prefs_matcher_reset_condition(void)
{
	gtk_list_select_item(GTK_LIST(matcher.criteria_list), 0);
	gtk_list_select_item(GTK_LIST(matcher.predicate_list), 0);
	gtk_entry_set_text(GTK_ENTRY(matcher.header_entry), "");
	gtk_entry_set_text(GTK_ENTRY(matcher.value_entry), "");
}

static void prefs_matcher_update_hscrollbar(void)
{
	gint optwidth = gtk_clist_optimal_column_width(GTK_CLIST(matcher.cond_clist), 0);
	gtk_clist_set_column_width(GTK_CLIST(matcher.cond_clist), 0, optwidth);
}

static void prefs_matcher_set_dialog(MatcherList * matchers)
{
	GtkCList *clist = GTK_CLIST(matcher.cond_clist);
	GSList * cur;
	gboolean bool_op = 1;

	gtk_clist_freeze(clist);
	gtk_clist_clear(clist);

	prefs_matcher_clist_set_row(-1, NULL);
	if (matchers != NULL) {
		for (cur = matchers->matchers ; cur != NULL ;
		     cur = g_slist_next(cur)) {
			MatcherProp * prop;
			prop = (MatcherProp *) cur->data;
			prefs_matcher_clist_set_row(-1, prop);
		}

		bool_op = matchers->bool_and;
	}
	
	prefs_matcher_update_hscrollbar();

	gtk_clist_thaw(clist);

	gtk_list_select_item(GTK_LIST(matcher.bool_op_list), bool_op);

	prefs_matcher_reset_condition();
}

static MatcherList * prefs_matcher_get_list(void)
{
	gchar * matcher_str;
	MatcherProp * prop;
	gint row = 1;
	gchar * tmp;
	gboolean bool_and;
	GSList * matcher_list;
	MatcherList * matchers;

	matcher_list = NULL;

	while (gtk_clist_get_text(GTK_CLIST(matcher.cond_clist),
				  row, 0, &matcher_str)) {

		if (strcmp(matcher_str, _("(New)")) != 0) {
			tmp = matcher_str;
			prop = matcherprop_parse(&tmp);
			
			if (tmp == NULL)
				break;
			
			matcher_list = g_slist_append(matcher_list, prop);
		}
		row ++;
	}

	bool_and = get_sel_from_list(GTK_LIST(matcher.bool_op_list));

	matchers = matcherlist_new(matcher_list, bool_and);

	return matchers;
}

static gint prefs_matcher_get_criteria_from_matching(gint matching_id)
{
	switch(matching_id) {
	case MATCHING_ALL:
		return CRITERIA_ALL;
	case MATCHING_NOT_UNREAD:
	case MATCHING_UNREAD:
		return CRITERIA_UNREAD;
	case MATCHING_NOT_NEW:
	case MATCHING_NEW:
		return CRITERIA_NEW;
	case MATCHING_NOT_MARKED:
	case MATCHING_MARKED:
		return CRITERIA_MARKED;
	case MATCHING_NOT_DELETED:
	case MATCHING_DELETED:
		return CRITERIA_DELETED;
		break;
	case MATCHING_NOT_REPLIED:
	case MATCHING_REPLIED:
		return CRITERIA_REPLIED;
	case MATCHING_NOT_FORWARDED:
	case MATCHING_FORWARDED:
		return CRITERIA_FORWARDED;
	case MATCHING_NOT_SUBJECT:
	case MATCHING_SUBJECT:
		return CRITERIA_SUBJECT;
	case MATCHING_NOT_FROM:
	case MATCHING_FROM:
		return CRITERIA_FROM;
	case MATCHING_NOT_TO:
	case MATCHING_TO:
		return CRITERIA_TO;
	case MATCHING_NOT_CC:
	case MATCHING_CC:
		return CRITERIA_CC;
	case MATCHING_NOT_NEWSGROUPS:
	case MATCHING_NEWSGROUPS:
		return CRITERIA_NEWSGROUPS;
	case MATCHING_NOT_INREPLYTO:
	case MATCHING_INREPLYTO:
		return CRITERIA_INREPLYTO;
	case MATCHING_NOT_REFERENCES:
	case MATCHING_REFERENCES:
		return CRITERIA_REFERENCES;
	case MATCHING_NOT_TO_AND_NOT_CC:
	case MATCHING_TO_OR_CC:
		return CRITERIA_TO_OR_CC;
	case MATCHING_NOT_BODY_PART:
	case MATCHING_BODY_PART:
		return CRITERIA_BODY_PART;
	case MATCHING_NOT_MESSAGE:
	case MATCHING_MESSAGE:
		return CRITERIA_MESSAGE;
		break;
	case MATCHING_NOT_HEADERS_PART:
	case MATCHING_HEADERS_PART:
		return CRITERIA_HEADERS_PART;
	case MATCHING_NOT_HEADER:
	case MATCHING_HEADER:
		return CRITERIA_HEADER;
	case MATCHING_AGE_GREATER:
		return CRITERIA_AGE_GREATER;
	case MATCHING_AGE_LOWER:
		return CRITERIA_AGE_LOWER;
	case MATCHING_SCORE_GREATER:
		return CRITERIA_SCORE_GREATER;
	case MATCHING_SCORE_LOWER:
		return CRITERIA_SCORE_LOWER;
	case MATCHING_NOT_EXECUTE:
	case MATCHING_EXECUTE:
		return CRITERIA_EXECUTE;
		break;
	default:
		return -1;
	}
}

static gint prefs_matcher_get_matching_from_criteria(gint criteria_id)
{
	switch (criteria_id) {
	case CRITERIA_ALL:
		return MATCHING_ALL;
	case CRITERIA_UNREAD:
		return MATCHING_UNREAD;
	case CRITERIA_NEW:
		return MATCHING_NEW;
	case CRITERIA_MARKED:
		return MATCHING_MARKED;
	case CRITERIA_DELETED:
		return MATCHING_DELETED;
	case CRITERIA_REPLIED:
		return MATCHING_REPLIED;
	case CRITERIA_FORWARDED:
		return MATCHING_FORWARDED;
	case CRITERIA_SUBJECT:
		return MATCHING_SUBJECT;
	case CRITERIA_FROM:
		return MATCHING_FROM;
	case CRITERIA_TO:
		return MATCHING_TO;
	case CRITERIA_CC:
		return MATCHING_CC;
	case CRITERIA_TO_OR_CC:
		return MATCHING_TO_OR_CC;
	case CRITERIA_NEWSGROUPS:
		return MATCHING_NEWSGROUPS;
	case CRITERIA_INREPLYTO:
		return MATCHING_INREPLYTO;
	case CRITERIA_REFERENCES:
		return MATCHING_REFERENCES;
	case CRITERIA_AGE_GREATER:
		return MATCHING_AGE_GREATER;
	case CRITERIA_AGE_LOWER:
		return MATCHING_AGE_LOWER;
	case CRITERIA_SCORE_GREATER:
		return MATCHING_SCORE_GREATER;
	case CRITERIA_SCORE_LOWER:
		return MATCHING_SCORE_LOWER;
	case CRITERIA_HEADER:
		return MATCHING_HEADER;
	case CRITERIA_HEADERS_PART:
		return MATCHING_HEADERS_PART;
	case CRITERIA_BODY_PART:
		return MATCHING_BODY_PART;
	case CRITERIA_MESSAGE:
		return MATCHING_MESSAGE;
	case CRITERIA_EXECUTE:
		return MATCHING_EXECUTE;
	default:
		return -1;
	}
}

static gint prefs_matcher_not_criteria(gint matcher_criteria)
{
	switch(matcher_criteria) {
	case MATCHING_UNREAD:
		return MATCHING_NOT_UNREAD;
	case MATCHING_NEW:
		return MATCHING_NOT_NEW;
	case MATCHING_MARKED:
		return MATCHING_NOT_MARKED;
	case MATCHING_DELETED:
		return MATCHING_NOT_DELETED;
	case MATCHING_REPLIED:
		return MATCHING_NOT_REPLIED;
	case MATCHING_FORWARDED:
		return MATCHING_NOT_FORWARDED;
	case MATCHING_SUBJECT:
		return MATCHING_NOT_SUBJECT;
	case MATCHING_FROM:
		return MATCHING_NOT_FROM;
	case MATCHING_TO:
		return MATCHING_NOT_TO;
	case MATCHING_CC:
		return MATCHING_NOT_CC;
	case MATCHING_TO_OR_CC:
		return MATCHING_NOT_TO_AND_NOT_CC;
	case MATCHING_NEWSGROUPS:
		return MATCHING_NOT_NEWSGROUPS;
	case MATCHING_INREPLYTO:
		return MATCHING_NOT_INREPLYTO;
	case MATCHING_REFERENCES:
		return MATCHING_NOT_REFERENCES;
	case MATCHING_HEADER:
		return MATCHING_NOT_HEADER;
	case MATCHING_HEADERS_PART:
		return MATCHING_NOT_HEADERS_PART;
	case MATCHING_MESSAGE:
		return MATCHING_NOT_MESSAGE;
	case MATCHING_EXECUTE:
		return MATCHING_NOT_EXECUTE;
	case MATCHING_BODY_PART:
		return MATCHING_NOT_BODY_PART;
	default:
		return matcher_criteria;
	}
}

static MatcherProp * prefs_matcher_dialog_to_matcher()
{
	MatcherProp * matcherprop;
	gint criteria;
	gint matchtype;
	gint value_pred;
	gint value_pred_flag;
	gint value_criteria;
	gboolean use_regexp;
	gboolean case_sensitive;
	gchar * header;
	gchar * expr;
	gint value;
	gchar * value_str;

	value_criteria = get_sel_from_list(GTK_LIST(matcher.criteria_list));

	criteria = prefs_matcher_get_matching_from_criteria(value_criteria);

	value_pred = get_sel_from_list(GTK_LIST(matcher.predicate_list));
	value_pred_flag = get_sel_from_list(GTK_LIST(matcher.predicate_flag_list));

	use_regexp = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(matcher.regexp_chkbtn));
	case_sensitive = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(matcher.case_chkbtn));

	switch (value_criteria) {
	case CRITERIA_UNREAD:
	case CRITERIA_NEW:
	case CRITERIA_MARKED:
	case CRITERIA_DELETED:
	case CRITERIA_REPLIED:
	case CRITERIA_FORWARDED:
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
	case CRITERIA_EXECUTE:
	case CRITERIA_AGE_GREATER:
	case CRITERIA_AGE_LOWER:
	case CRITERIA_HEADER:
		if (value_pred == PREDICATE_DOES_NOT_CONTAIN)
			criteria = prefs_matcher_not_criteria(criteria);
		break;
	}

	if (use_regexp) {
		if (case_sensitive)
			matchtype = MATCHING_REGEXP;
		else
			matchtype = MATCHING_REGEXPCASE;
	}
	else {
		if (case_sensitive)
			matchtype = MATCHING_MATCH;
		else
			matchtype = MATCHING_MATCHCASE;
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
	case CRITERIA_EXECUTE:
		expr = gtk_entry_get_text(GTK_ENTRY(matcher.value_entry));

		/*
		if (*expr == '\0') {
		    alertpanel_error(_("Match string is not set."));
		    return NULL;
		}
		*/
		break;

	case CRITERIA_AGE_GREATER:
	case CRITERIA_AGE_LOWER:
	case CRITERIA_SCORE_GREATER:
	case CRITERIA_SCORE_LOWER:
		value_str = gtk_entry_get_text(GTK_ENTRY(matcher.value_entry));

		if (*value_str == '\0') {
		    alertpanel_error(_("Value is not set."));
		    return NULL;
		}

		value = atoi(value_str);

		break;

	case CRITERIA_HEADER:

		header = gtk_entry_get_text(GTK_ENTRY(matcher.header_entry));
		expr = gtk_entry_get_text(GTK_ENTRY(matcher.value_entry));

		if (*header == '\0') {
		    alertpanel_error(_("Header name is not set."));
		    return NULL;
		}
		/*
		if (*expr == '\0') {
		    alertpanel_error(_("Match string is not set."));
		    return NULL;
		}
		*/
		break;
	}

	matcherprop =  matcherprop_new(criteria, header, matchtype,
				       expr, value);

	return matcherprop;
}

static void prefs_matcher_register_cb(void)
{
	MatcherProp * matcherprop;
	
	matcherprop = prefs_matcher_dialog_to_matcher();
	if (matcherprop == NULL)
		return;

	prefs_matcher_clist_set_row(-1, matcherprop);

	matcherprop_free(matcherprop);

	prefs_matcher_reset_condition();
	prefs_matcher_update_hscrollbar();
}

static void prefs_matcher_substitute_cb(void)
{
	GtkCList *clist = GTK_CLIST(matcher.cond_clist);
	gint row;
	MatcherProp * matcherprop;

	row = GPOINTER_TO_INT(clist->selection->data);
	if (row == 0)
		return;
	
	matcherprop = prefs_matcher_dialog_to_matcher();
	if (matcherprop == NULL)
		return;

	prefs_matcher_clist_set_row(row, matcherprop);

	matcherprop_free(matcherprop);

	prefs_matcher_reset_condition();
	
	prefs_matcher_update_hscrollbar();
}

static void prefs_matcher_delete_cb(void)
{
	GtkCList *clist = GTK_CLIST(matcher.cond_clist);
	gint row;

	if (!clist->selection) return;
	row = GPOINTER_TO_INT(clist->selection->data);
	if (row == 0)
		return;

	gtk_clist_remove(clist, row);
	
	prefs_matcher_update_hscrollbar();
}

static void prefs_matcher_up(void)
{
	GtkCList *clist = GTK_CLIST(matcher.cond_clist);
	gint row;

	if (!clist->selection) return;

	row = GPOINTER_TO_INT(clist->selection->data);
	if (row > 1)
		gtk_clist_row_move(clist, row, row - 1);
}

static void prefs_matcher_down(void)
{
	GtkCList *clist = GTK_CLIST(matcher.cond_clist);
	gint row;

	if (!clist->selection) return;

	row = GPOINTER_TO_INT(clist->selection->data);
	if (row >= 1 && row < clist->rows - 1)
		gtk_clist_row_move(clist, row, row + 1);
}

static void prefs_matcher_select(GtkCList *clist, gint row, gint column,
				 GdkEvent *event)
{
	gchar * matcher_str;
	gchar * tmp;
	MatcherProp * prop;
	gboolean negative_cond;
	gint criteria;

	if (!gtk_clist_get_text(GTK_CLIST(matcher.cond_clist),
				row, 0, &matcher_str))
		return;

	negative_cond = FALSE;

	if (row == 0) {
		prefs_matcher_reset_condition();
		return;
	}

	tmp = matcher_str;
	prop = matcherprop_parse(&tmp);
	if (tmp == NULL)
		return;

	criteria = prefs_matcher_get_criteria_from_matching(prop->criteria);
	if (criteria != -1)
		gtk_list_select_item(GTK_LIST(matcher.criteria_list),
				     criteria);

	switch(prop->criteria) {
	case MATCHING_NOT_UNREAD:
	case MATCHING_NOT_NEW:
	case MATCHING_NOT_MARKED:
	case MATCHING_NOT_DELETED:
	case MATCHING_NOT_REPLIED:
	case MATCHING_NOT_FORWARDED:
	case MATCHING_NOT_SUBJECT:
	case MATCHING_NOT_FROM:
	case MATCHING_NOT_TO:
	case MATCHING_NOT_CC:
	case MATCHING_NOT_NEWSGROUPS:
	case MATCHING_NOT_INREPLYTO:
	case MATCHING_NOT_REFERENCES:
	case MATCHING_NOT_TO_AND_NOT_CC:
	case MATCHING_NOT_BODY_PART:
	case MATCHING_NOT_MESSAGE:
	case MATCHING_NOT_HEADERS_PART:
	case MATCHING_NOT_HEADER:
		negative_cond = TRUE;
		break;
	}
	
	switch(prop->criteria) {
	case MATCHING_ALL:
		break;

	case MATCHING_NOT_SUBJECT:
	case MATCHING_NOT_FROM:
	case MATCHING_NOT_TO:
	case MATCHING_NOT_CC:
	case MATCHING_NOT_TO_AND_NOT_CC:
	case MATCHING_NOT_NEWSGROUPS:
	case MATCHING_NOT_INREPLYTO:
	case MATCHING_NOT_REFERENCES:
	case MATCHING_NOT_HEADERS_PART:
	case MATCHING_NOT_BODY_PART:
	case MATCHING_NOT_MESSAGE:
	case MATCHING_SUBJECT:
	case MATCHING_FROM:
	case MATCHING_TO:
	case MATCHING_CC:
	case MATCHING_TO_OR_CC:
	case MATCHING_NEWSGROUPS:
	case MATCHING_INREPLYTO:
	case MATCHING_REFERENCES:
	case MATCHING_HEADERS_PART:
	case MATCHING_BODY_PART:
	case MATCHING_MESSAGE:
		gtk_entry_set_text(GTK_ENTRY(matcher.value_entry), prop->expr);
		break;

	case MATCHING_AGE_GREATER:
	case MATCHING_AGE_LOWER:
	case MATCHING_SCORE_GREATER:
	case MATCHING_SCORE_LOWER:
		gtk_entry_set_text(GTK_ENTRY(matcher.value_entry), itos(prop->value));
		break;

	case MATCHING_NOT_HEADER:
	case MATCHING_HEADER:
		gtk_entry_set_text(GTK_ENTRY(matcher.header_entry), prop->header);
		gtk_entry_set_text(GTK_ENTRY(matcher.value_entry), prop->expr);
		break;
	}

	if (negative_cond)
		gtk_list_select_item(GTK_LIST(matcher.predicate_list), 1);
	else
		gtk_list_select_item(GTK_LIST(matcher.predicate_list), 0);

	switch(prop->matchtype) {
	case MATCHING_MATCH:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(matcher.regexp_chkbtn), FALSE);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(matcher.case_chkbtn), TRUE);
		break;

	case MATCHING_MATCHCASE:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(matcher.regexp_chkbtn), FALSE);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(matcher.case_chkbtn), FALSE);
		break;

	case MATCHING_REGEXP:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(matcher.regexp_chkbtn), TRUE);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(matcher.case_chkbtn), TRUE);
		break;

	case MATCHING_REGEXPCASE:
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(matcher.regexp_chkbtn), TRUE);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(matcher.case_chkbtn), FALSE);
		break;
	}
}

static void prefs_matcher_criteria_select(GtkList *list,
					  GtkWidget *widget,
					  gpointer user_data)
{
	gint value;

	value = get_sel_from_list(GTK_LIST(matcher.criteria_list));

	switch (value) {
	case CRITERIA_ALL:
		gtk_widget_set_sensitive(matcher.header_combo, FALSE);
		gtk_widget_set_sensitive(matcher.header_label, FALSE);
		gtk_widget_set_sensitive(matcher.value_label, FALSE);
		gtk_widget_set_sensitive(matcher.value_entry, FALSE);
		gtk_widget_set_sensitive(matcher.predicate_label, FALSE);
		gtk_widget_set_sensitive(matcher.predicate_combo, FALSE);
		gtk_widget_set_sensitive(matcher.predicate_flag_combo, FALSE);
		gtk_widget_hide(matcher.predicate_combo);
		gtk_widget_show(matcher.predicate_flag_combo);
		gtk_widget_set_sensitive(matcher.case_chkbtn, FALSE);
		gtk_widget_set_sensitive(matcher.regexp_chkbtn, FALSE);
		gtk_widget_set_sensitive(matcher.exec_btn, FALSE);
		break;

	case CRITERIA_UNREAD:
	case CRITERIA_NEW:
	case CRITERIA_MARKED:
	case CRITERIA_DELETED:
	case CRITERIA_REPLIED:
	case CRITERIA_FORWARDED:
		gtk_widget_set_sensitive(matcher.header_combo, FALSE);
		gtk_widget_set_sensitive(matcher.header_label, FALSE);
		gtk_widget_set_sensitive(matcher.value_label, FALSE);
		gtk_widget_set_sensitive(matcher.value_entry, FALSE);
		gtk_widget_set_sensitive(matcher.predicate_label, TRUE);
		gtk_widget_set_sensitive(matcher.predicate_combo, FALSE);
		gtk_widget_set_sensitive(matcher.predicate_flag_combo, TRUE);
		gtk_widget_hide(matcher.predicate_combo);
		gtk_widget_show(matcher.predicate_flag_combo);
		gtk_widget_set_sensitive(matcher.case_chkbtn, FALSE);
		gtk_widget_set_sensitive(matcher.regexp_chkbtn, FALSE);
		gtk_widget_set_sensitive(matcher.exec_btn, FALSE);
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
		gtk_widget_set_sensitive(matcher.header_combo, FALSE);
		gtk_widget_set_sensitive(matcher.header_label, FALSE);
		gtk_widget_set_sensitive(matcher.value_label, TRUE);
		gtk_widget_set_sensitive(matcher.value_entry, TRUE);
		gtk_widget_set_sensitive(matcher.predicate_label, TRUE);
		gtk_widget_set_sensitive(matcher.predicate_combo, TRUE);
		gtk_widget_set_sensitive(matcher.predicate_flag_combo, FALSE);
		gtk_widget_show(matcher.predicate_combo);
		gtk_widget_hide(matcher.predicate_flag_combo);
		gtk_widget_set_sensitive(matcher.case_chkbtn, TRUE);
		gtk_widget_set_sensitive(matcher.regexp_chkbtn, TRUE);
		gtk_widget_set_sensitive(matcher.exec_btn, FALSE);
		break;

	case CRITERIA_EXECUTE:
		gtk_widget_set_sensitive(matcher.header_combo, FALSE);
		gtk_widget_set_sensitive(matcher.header_label, FALSE);
		gtk_widget_set_sensitive(matcher.value_label, TRUE);
		gtk_widget_set_sensitive(matcher.value_entry, TRUE);
		gtk_widget_set_sensitive(matcher.predicate_label, TRUE);
		gtk_widget_set_sensitive(matcher.predicate_combo, FALSE);
		gtk_widget_set_sensitive(matcher.predicate_flag_combo, TRUE);
		gtk_widget_hide(matcher.predicate_combo);
		gtk_widget_show(matcher.predicate_flag_combo);
		gtk_widget_set_sensitive(matcher.case_chkbtn, FALSE);
		gtk_widget_set_sensitive(matcher.regexp_chkbtn, FALSE);
		gtk_widget_set_sensitive(matcher.exec_btn, TRUE);
		break;

	case CRITERIA_AGE_GREATER:
	case CRITERIA_AGE_LOWER:
	case CRITERIA_SCORE_GREATER:
	case CRITERIA_SCORE_LOWER:
		gtk_widget_set_sensitive(matcher.header_combo, FALSE);
		gtk_widget_set_sensitive(matcher.header_label, FALSE);
		gtk_widget_set_sensitive(matcher.value_label, TRUE);
		gtk_widget_set_sensitive(matcher.value_entry, TRUE);
		gtk_widget_set_sensitive(matcher.predicate_label, FALSE);
		gtk_widget_set_sensitive(matcher.predicate_combo, FALSE);
		gtk_widget_set_sensitive(matcher.predicate_flag_combo, FALSE);
		gtk_widget_show(matcher.predicate_combo);
		gtk_widget_hide(matcher.predicate_flag_combo);
		gtk_widget_set_sensitive(matcher.case_chkbtn, FALSE);
		gtk_widget_set_sensitive(matcher.regexp_chkbtn, FALSE);
		gtk_widget_set_sensitive(matcher.exec_btn, FALSE);
		break;

	case CRITERIA_HEADER:
		gtk_widget_set_sensitive(matcher.header_combo, TRUE);
		gtk_widget_set_sensitive(matcher.header_label, TRUE);
		gtk_widget_set_sensitive(matcher.value_label, TRUE);
		gtk_widget_set_sensitive(matcher.value_entry, TRUE);
		gtk_widget_set_sensitive(matcher.predicate_label, TRUE);
		gtk_widget_set_sensitive(matcher.predicate_combo, TRUE);
		gtk_widget_set_sensitive(matcher.predicate_flag_combo, FALSE);
		gtk_widget_show(matcher.predicate_combo);
		gtk_widget_hide(matcher.predicate_flag_combo);
		gtk_widget_set_sensitive(matcher.case_chkbtn, TRUE);
		gtk_widget_set_sensitive(matcher.regexp_chkbtn, TRUE);
		gtk_widget_set_sensitive(matcher.exec_btn, FALSE);
		break;
	}
}

static void prefs_matcher_key_pressed(GtkWidget *widget, GdkEventKey *event,
				     gpointer data)
{
	if (event && event->keyval == GDK_Escape)
		prefs_matcher_cancel();
}

static void prefs_matcher_cancel(void)
{
	gtk_widget_hide(matcher.window);
}

static void prefs_matcher_ok(void)
{
	MatcherList * matchers;

	matchers = prefs_matcher_get_list();
	if (matchers != NULL) {
		gtk_widget_hide(matcher.window);
		if (matchers_callback != NULL)
			matchers_callback(matchers);
		matcherlist_free(matchers);
	}
	else {
		gtk_widget_hide(matcher.window);
	}
}

static gint prefs_matcher_deleted(GtkWidget *widget, GdkEventAny *event,
				  gpointer data)
{
	prefs_matcher_cancel();
	return TRUE;
}

static GtkWidget * exec_info_win;

void prefs_matcher_exec_info(void)
{
	if (!exec_info_win)
		prefs_matcher_exec_info_create();

	gtk_widget_show(exec_info_win);
	gtk_main();
	gtk_widget_hide(exec_info_win);
}

static void prefs_matcher_exec_info_create(void)
{
	GtkWidget *vbox;
	GtkWidget *hbox;
	GtkWidget *hbbox;
	GtkWidget *label;
	GtkWidget *ok_btn;

	exec_info_win = gtk_window_new(GTK_WINDOW_DIALOG);
	gtk_window_set_title(GTK_WINDOW(exec_info_win),
			     _("Description of symbols"));
	gtk_container_set_border_width(GTK_CONTAINER(exec_info_win), 8);
	gtk_window_set_position(GTK_WINDOW(exec_info_win), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(exec_info_win), TRUE);
	gtk_window_set_policy(GTK_WINDOW(exec_info_win), FALSE, TRUE, FALSE);

	vbox = gtk_vbox_new(FALSE, 8);
	gtk_container_add(GTK_CONTAINER(exec_info_win), vbox);

	hbox = gtk_hbox_new(FALSE, 4);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

	label = gtk_label_new
		("%%:\n"
		 "%s:\n"
		 "%f:\n"
		 "%t:\n"
		 "%c:\n"
		 "%d:\n"
		 "%i:\n"
		 "%n:\n"
		 "%r:\n"
		 "%F:\n"
		 "%%:");

	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);

	label = gtk_label_new
		(_("%\n"
		   "Subject\n"
		   "From\n"
		   "To\n"
		   "Cc\n"
		   "Date\n"
		   "Message-ID\n"
		   "Newsgroups\n"
		   "References\n"
		   "Filename - should not be modified\n"
		   "%"));

	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);

	gtkut_button_set_create(&hbbox, &ok_btn, _("OK"),
				NULL, NULL, NULL, NULL);
	gtk_box_pack_end(GTK_BOX(vbox), hbbox, FALSE, FALSE, 0);

	gtk_widget_grab_default(ok_btn);
	gtk_signal_connect(GTK_OBJECT(ok_btn), "clicked",
				  GTK_SIGNAL_FUNC(gtk_main_quit), NULL);

	gtk_signal_connect(GTK_OBJECT(exec_info_win), "delete_event",
					  GTK_SIGNAL_FUNC(gtk_main_quit), NULL);

	gtk_widget_show_all(vbox);
}
