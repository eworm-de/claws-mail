/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2025 the Claws Mail team and Hiroyuki Yamamoto
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

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "main.h"
#include "summary_search.h"
#include "summaryview.h"
#include "messageview.h"
#include "mainwindow.h"
#include "menu.h"
#include "utils.h"
#include "gtkutils.h"
#include "combobox.h"
#include "prefs_gtk.h"
#include "manage_window.h"
#include "alertpanel.h"
#include "advsearch.h"
#include "matcher.h"
#include "matcher_parser.h"
#include "prefs_matcher.h"
#include "manual.h"
#include "prefs_common.h"
#include "statusbar.h"

static struct SummarySearchWindow {
	GtkWidget *window;

	GtkWidget *bool_optmenu;

	GtkWidget *from_label;
	GtkWidget *from_entry;
	GtkWidget *to_label;
	GtkWidget *to_entry;
	GtkWidget *subject_label;
	GtkWidget *subject_entry;
	GtkWidget *body_label;
	GtkWidget *body_entry;

	GtkWidget *adv_condition_label;
	GtkWidget *adv_condition_entry;
	GtkWidget *adv_condition_btn;
	GtkWidget *adv_search_checkbtn;

	GtkWidget *case_checkbtn;

	GtkWidget *clear_btn;
	GtkWidget *help_btn;
	GtkWidget *all_btn;
	GtkWidget *prev_btn;
	GtkWidget *next_btn;
	GtkWidget *close_btn;
	GtkWidget *stop_btn;

	SummaryView *summaryview;

	AdvancedSearch	*advsearch;
	gboolean	is_fast;
	gboolean	matcher_is_outdated;
	gboolean	search_in_progress;
	GHashTable	*matched_msgnums;

	gboolean is_searching;
	gboolean from_entry_has_focus;
	gboolean to_entry_has_focus;
	gboolean subject_entry_has_focus;
	gboolean body_entry_has_focus;
	gboolean adv_condition_entry_has_focus;
} search_window;

static gchar* add_history_get(GtkWidget *from, GList **history);

static void summary_search_create	(void);

static gboolean summary_search_verify_match		(MsgInfo *msg);
static gboolean summary_search_prepare_matcher		();
static gboolean summary_search_prereduce_msg_list	();

static void summary_search_execute	(gboolean	 backward,
					 gboolean	 search_all);

static void summary_search_clear	(GtkButton	*button,
					 gpointer	 data);
static void summary_search_prev_clicked	(GtkButton	*button,
					 gpointer	 data);
static void summary_search_next_clicked	(GtkButton	*button,
					 gpointer	 data);
static void summary_search_all_clicked	(GtkButton	*button,
					 gpointer	 data);
static void summary_search_stop_clicked	(GtkButton	*button,
					 gpointer	 data);
static void adv_condition_btn_clicked	(GtkButton	*button,
					 gpointer	 data);

static void optmenu_changed			(GtkComboBox *widget, gpointer user_data);
static void from_changed			(void);
static void to_changed				(void);
static void subject_changed			(void);
static void body_changed			(void);
static void adv_condition_changed	(void);
static void case_changed			(GtkToggleButton *togglebutton, gpointer user_data);

static gboolean from_entry_focus_evt_in(GtkWidget *widget, GdkEventFocus *event,
			      	  gpointer data);
static gboolean from_entry_focus_evt_out(GtkWidget *widget, GdkEventFocus *event,
			      	  gpointer data);
static gboolean to_entry_focus_evt_in(GtkWidget *widget, GdkEventFocus *event,
			      	  gpointer data);
static gboolean to_entry_focus_evt_out(GtkWidget *widget, GdkEventFocus *event,
			      	  gpointer data);
static gboolean subject_entry_focus_evt_in(GtkWidget *widget, GdkEventFocus *event,
			      	  gpointer data);
static gboolean subject_entry_focus_evt_out(GtkWidget *widget, GdkEventFocus *event,
			      	  gpointer data);
static gboolean body_entry_focus_evt_in(GtkWidget *widget, GdkEventFocus *event,
			      	  gpointer data);
static gboolean body_entry_focus_evt_out(GtkWidget *widget, GdkEventFocus *event,
			      	  gpointer data);
static gboolean adv_condition_entry_focus_evt_in(GtkWidget *widget, GdkEventFocus *event,
			      	  gpointer data);
static gboolean adv_condition_entry_focus_evt_out(GtkWidget *widget, GdkEventFocus *event,
			      	  gpointer data);
static gboolean key_pressed		(GtkWidget	*widget,
					 GdkEventKey	*event,
					 gpointer	 data);

#define GTK_BUTTON_SET_SENSITIVE(widget,sensitive) {					\
	gtk_widget_set_sensitive(widget, sensitive);					\
}

static gchar* add_history_get(GtkWidget *from, GList **history)
{
	gchar *result;

	result = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(from));
	if (!result)
		result = gtk_editable_get_chars(GTK_EDITABLE(gtk_bin_get_child(GTK_BIN(from))), 0, -1);

	if (result && result[0] != '\0') {
		/* add to history */
		combobox_unset_popdown_strings(GTK_COMBO_BOX_TEXT(from));
		*history = add_history(*history, result);
		combobox_set_popdown_strings(GTK_COMBO_BOX_TEXT(from), *history);

		return result;
	} else {
		g_free(result);
		return NULL;
	}
}

void summary_search(SummaryView *summaryview)
{
	if (!search_window.window) {
		summary_search_create();
	} else {
		gtk_widget_hide(search_window.window);
	}

	search_window.summaryview = summaryview;

	gtk_widget_grab_focus(search_window.next_btn);
	gtk_widget_grab_focus(search_window.subject_entry);
	gtk_widget_show(search_window.window);
}

static void summary_show_stop_button(void)
{
	gtk_widget_hide(search_window.close_btn);
	gtk_widget_show(search_window.stop_btn);
	GTK_BUTTON_SET_SENSITIVE(search_window.clear_btn, FALSE)
	GTK_BUTTON_SET_SENSITIVE(search_window.all_btn, FALSE)
	GTK_BUTTON_SET_SENSITIVE(search_window.prev_btn, FALSE)
	GTK_BUTTON_SET_SENSITIVE(search_window.next_btn, FALSE)
	gtk_widget_set_sensitive(search_window.adv_condition_label, FALSE);
	gtk_widget_set_sensitive(search_window.adv_condition_entry, FALSE);
	gtk_widget_set_sensitive(search_window.adv_condition_btn, FALSE);
	gtk_widget_set_sensitive(search_window.to_label, FALSE);
	gtk_widget_set_sensitive(search_window.to_entry, FALSE);
	gtk_widget_set_sensitive(search_window.from_label, FALSE);
	gtk_widget_set_sensitive(search_window.from_entry, FALSE);
	gtk_widget_set_sensitive(search_window.subject_label, FALSE);
	gtk_widget_set_sensitive(search_window.subject_entry, FALSE);
	gtk_widget_set_sensitive(search_window.body_label, FALSE);
	gtk_widget_set_sensitive(search_window.body_entry, FALSE);
	gtk_widget_set_sensitive(search_window.bool_optmenu, FALSE);
	gtk_widget_set_sensitive(search_window.clear_btn, FALSE);
	gtk_widget_set_sensitive(search_window.case_checkbtn, FALSE);
	gtk_widget_set_sensitive(search_window.adv_search_checkbtn, FALSE);
}

static void summary_hide_stop_button(void)
{
	GTK_BUTTON_SET_SENSITIVE(search_window.clear_btn, TRUE)
	gtk_widget_hide(search_window.stop_btn);
	gtk_widget_show(search_window.close_btn);
	if (gtk_toggle_button_get_active
			(GTK_TOGGLE_BUTTON(search_window.adv_search_checkbtn))) {
		gtk_widget_set_sensitive(search_window.adv_condition_label, TRUE);
		gtk_widget_set_sensitive(search_window.adv_condition_entry, TRUE);
		gtk_widget_set_sensitive(search_window.adv_condition_btn, TRUE);
		gtk_widget_set_sensitive(search_window.case_checkbtn, FALSE);
		gtk_widget_set_sensitive(search_window.bool_optmenu, FALSE);
	} else {
		gtk_widget_set_sensitive(search_window.to_label, TRUE);
		gtk_widget_set_sensitive(search_window.to_entry, TRUE);
		gtk_widget_set_sensitive(search_window.from_label, TRUE);
		gtk_widget_set_sensitive(search_window.from_entry, TRUE);
		gtk_widget_set_sensitive(search_window.subject_label, TRUE);
		gtk_widget_set_sensitive(search_window.subject_entry, TRUE);
		gtk_widget_set_sensitive(search_window.body_label, TRUE);
		gtk_widget_set_sensitive(search_window.body_entry, TRUE);
		gtk_widget_set_sensitive(search_window.case_checkbtn, TRUE);
		gtk_widget_set_sensitive(search_window.bool_optmenu, TRUE);
	}
	gtk_widget_set_sensitive(search_window.clear_btn, TRUE);
	gtk_widget_set_sensitive(search_window.all_btn, TRUE);
	gtk_widget_set_sensitive(search_window.prev_btn, TRUE);
	gtk_widget_set_sensitive(search_window.next_btn, TRUE);
	gtk_widget_set_sensitive(search_window.adv_search_checkbtn, TRUE);
}

static void summary_search_create(void)
{
	GtkWidget *window;
	GtkWidget *vbox1;
	GtkWidget *bool_hbox;
	GtkWidget *bool_optmenu;
	GtkListStore *menu;
	GtkTreeIter iter;
	GtkWidget *clear_btn;

	GtkWidget *table1;
	GtkWidget *from_label;
	GtkWidget *from_entry;
	GtkWidget *to_label;
	GtkWidget *to_entry;
	GtkWidget *subject_label;
	GtkWidget *subject_entry;
	GtkWidget *body_label;
	GtkWidget *body_entry;
	GtkWidget *adv_condition_label;
	GtkWidget *adv_condition_entry;
	GtkWidget *adv_condition_btn;

	GtkWidget *checkbtn_hbox;
	GtkWidget *adv_search_checkbtn;
	GtkWidget *case_checkbtn;

	GtkWidget *confirm_area;
	GtkWidget *help_btn;
	GtkWidget *all_btn;
	GtkWidget *prev_btn;
	GtkWidget *next_btn;
	GtkWidget *close_btn;
	GtkWidget *stop_btn;
	gboolean is_searching = FALSE;

	window = gtkut_window_new(GTK_WINDOW_TOPLEVEL, "summary_search");
	gtk_window_set_title(GTK_WINDOW (window), _("Search messages"));
	gtk_window_set_resizable(GTK_WINDOW(window), TRUE);
	gtk_window_set_type_hint(GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_DIALOG);
	gtk_container_set_border_width(GTK_CONTAINER (window), 8);
	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(gtk_widget_hide_on_delete), NULL);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(key_pressed), NULL);
	MANAGE_WINDOW_SIGNALS_CONNECT(window);

	vbox1 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_show (vbox1);
	gtk_container_add (GTK_CONTAINER (window), vbox1);

	bool_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_widget_show(bool_hbox);
	gtk_box_pack_start(GTK_BOX(vbox1), bool_hbox, FALSE, FALSE, 0);

	bool_optmenu = gtkut_sc_combobox_create(NULL, FALSE);
	menu = GTK_LIST_STORE(gtk_combo_box_get_model(GTK_COMBO_BOX(bool_optmenu)));
	gtk_widget_show(bool_optmenu);
	gtk_box_pack_start(GTK_BOX(bool_hbox), bool_optmenu, FALSE, FALSE, 0);

	COMBOBOX_ADD(menu, _("Match any of the following"), 0);
	gtk_combo_box_set_active_iter(GTK_COMBO_BOX(bool_optmenu), &iter);
	COMBOBOX_ADD(menu, _("Match all of the following"), 1);
	g_signal_connect(G_OBJECT(bool_optmenu), "changed",
			 G_CALLBACK(optmenu_changed), NULL);

	clear_btn = gtkut_stock_button("edit-clear", _("C_lear"));
	gtk_widget_show(clear_btn);
	gtk_box_pack_end(GTK_BOX(bool_hbox), clear_btn, FALSE, FALSE, 0);

	table1 = gtk_grid_new();
	gtk_widget_show (table1);
	gtk_box_pack_start (GTK_BOX (vbox1), table1, TRUE, TRUE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (table1), 4);
	gtk_grid_set_row_spacing(GTK_GRID(table1), 8);
	gtk_grid_set_column_spacing(GTK_GRID(table1), 8);

	from_entry = gtk_combo_box_text_new_with_entry ();
	gtk_combo_box_set_active(GTK_COMBO_BOX(from_entry), -1);
	if (prefs_common.summary_search_from_history)
		combobox_set_popdown_strings(GTK_COMBO_BOX_TEXT(from_entry),
				prefs_common.summary_search_from_history);
	gtk_widget_show (from_entry);
	gtk_grid_attach(GTK_GRID(table1), from_entry, 1, 0, 1, 1);
	gtk_widget_set_hexpand(from_entry, TRUE);
	gtk_widget_set_halign(from_entry, GTK_ALIGN_FILL);

	g_signal_connect(G_OBJECT(from_entry), "changed",
			 G_CALLBACK(from_changed), NULL);
	g_signal_connect(G_OBJECT(gtk_bin_get_child(GTK_BIN((from_entry)))),
			 "focus_in_event", G_CALLBACK(from_entry_focus_evt_in), NULL);
	g_signal_connect(G_OBJECT(gtk_bin_get_child(GTK_BIN((from_entry)))),
			 "focus_out_event", G_CALLBACK(from_entry_focus_evt_out), NULL);

	to_entry = gtk_combo_box_text_new_with_entry ();
	gtk_combo_box_set_active(GTK_COMBO_BOX(to_entry), -1);
	if (prefs_common.summary_search_to_history)
		combobox_set_popdown_strings(GTK_COMBO_BOX_TEXT(to_entry),
				prefs_common.summary_search_to_history);
	gtk_widget_show (to_entry);
	gtk_grid_attach(GTK_GRID(table1), to_entry, 1, 1, 1, 1);
	gtk_widget_set_hexpand(to_entry, TRUE);
	gtk_widget_set_halign(to_entry, GTK_ALIGN_FILL);

	g_signal_connect(G_OBJECT(to_entry), "changed",
			 G_CALLBACK(to_changed), NULL);
	g_signal_connect(G_OBJECT(gtk_bin_get_child(GTK_BIN((to_entry)))),
			 "focus_in_event", G_CALLBACK(to_entry_focus_evt_in), NULL);
	g_signal_connect(G_OBJECT(gtk_bin_get_child(GTK_BIN((to_entry)))),
			 "focus_out_event", G_CALLBACK(to_entry_focus_evt_out), NULL);

	subject_entry = gtk_combo_box_text_new_with_entry ();
	gtk_combo_box_set_active(GTK_COMBO_BOX(subject_entry), -1);
	if (prefs_common.summary_search_subject_history)
		combobox_set_popdown_strings(GTK_COMBO_BOX_TEXT(subject_entry),
				prefs_common.summary_search_subject_history);
	gtk_widget_show (subject_entry);
	gtk_grid_attach(GTK_GRID(table1), subject_entry, 1, 2, 1, 1);
	gtk_widget_set_hexpand(subject_entry, TRUE);
	gtk_widget_set_halign(subject_entry, GTK_ALIGN_FILL);

	g_signal_connect(G_OBJECT(subject_entry), "changed",
			 G_CALLBACK(subject_changed), NULL);
	g_signal_connect(G_OBJECT(gtk_bin_get_child(GTK_BIN((subject_entry)))),
			 "focus_in_event", G_CALLBACK(subject_entry_focus_evt_in), NULL);
	g_signal_connect(G_OBJECT(gtk_bin_get_child(GTK_BIN((subject_entry)))),
			 "focus_out_event", G_CALLBACK(subject_entry_focus_evt_out), NULL);

	body_entry = gtk_combo_box_text_new_with_entry ();
	gtk_combo_box_set_active(GTK_COMBO_BOX(body_entry), -1);
	if (prefs_common.summary_search_body_history)
		combobox_set_popdown_strings(GTK_COMBO_BOX_TEXT(body_entry),
				prefs_common.summary_search_body_history);
	gtk_widget_show (body_entry);
	gtk_grid_attach(GTK_GRID(table1), body_entry, 1, 3, 1, 1);
	gtk_widget_set_hexpand(body_entry, TRUE);
	gtk_widget_set_halign(body_entry, GTK_ALIGN_FILL);

	g_signal_connect(G_OBJECT(body_entry), "changed",
			 G_CALLBACK(body_changed), NULL);
	g_signal_connect(G_OBJECT(gtk_bin_get_child(GTK_BIN((body_entry)))),
			 "focus_in_event", G_CALLBACK(body_entry_focus_evt_in), NULL);
	g_signal_connect(G_OBJECT(gtk_bin_get_child(GTK_BIN((body_entry)))),
			 "focus_out_event", G_CALLBACK(body_entry_focus_evt_out), NULL);

	adv_condition_entry = gtk_combo_box_text_new_with_entry ();
	gtk_combo_box_set_active(GTK_COMBO_BOX(adv_condition_entry), -1);
	if (prefs_common.summary_search_adv_condition_history)
		combobox_set_popdown_strings(GTK_COMBO_BOX_TEXT(adv_condition_entry),
				prefs_common.summary_search_adv_condition_history);
	gtk_widget_show (adv_condition_entry);
	gtk_grid_attach(GTK_GRID(table1), adv_condition_entry, 1, 4, 1, 1);
	gtk_widget_set_hexpand(adv_condition_entry, TRUE);
	gtk_widget_set_halign(adv_condition_entry, GTK_ALIGN_FILL);

	g_signal_connect(G_OBJECT(adv_condition_entry), "changed",
			 G_CALLBACK(adv_condition_changed), NULL);
	g_signal_connect(G_OBJECT(gtk_bin_get_child(GTK_BIN((adv_condition_entry)))),
			 "focus_in_event", G_CALLBACK(adv_condition_entry_focus_evt_in), NULL);
	g_signal_connect(G_OBJECT(gtk_bin_get_child(GTK_BIN((adv_condition_entry)))),
			 "focus_out_event", G_CALLBACK(adv_condition_entry_focus_evt_out), NULL);

	adv_condition_btn = gtk_button_new_with_label(" ... ");
	gtk_widget_show (adv_condition_btn);
	gtk_grid_attach(GTK_GRID(table1), adv_condition_btn, 2, 4, 1, 1);

	g_signal_connect(G_OBJECT (adv_condition_btn), "clicked",
			 G_CALLBACK(adv_condition_btn_clicked), search_window.window);

	CLAWS_SET_TIP(adv_condition_btn,
			     _("Edit search criteria"));

	from_label = gtk_label_new (_("From:"));
	gtk_widget_show (from_label);
	gtk_label_set_justify (GTK_LABEL (from_label), GTK_JUSTIFY_RIGHT);
	gtk_label_set_xalign (GTK_LABEL (from_label), 1.0);
	gtk_grid_attach(GTK_GRID(table1), from_label, 0, 0, 1, 1);

	to_label = gtk_label_new (_("To:"));
	gtk_widget_show (to_label);
	gtk_label_set_justify (GTK_LABEL (to_label), GTK_JUSTIFY_RIGHT);
	gtk_label_set_xalign (GTK_LABEL (to_label), 1.0);
	gtk_grid_attach(GTK_GRID(table1), to_label, 0, 1, 1, 1);

	subject_label = gtk_label_new (_("Subject:"));
	gtk_widget_show (subject_label);
	gtk_label_set_justify (GTK_LABEL (subject_label), GTK_JUSTIFY_RIGHT);
	gtk_label_set_xalign (GTK_LABEL (subject_label), 1.0);
	gtk_grid_attach(GTK_GRID(table1), subject_label, 0, 2, 1, 1);

	body_label = gtk_label_new (_("Body:"));
	gtk_widget_show (body_label);
	gtk_label_set_justify (GTK_LABEL (body_label), GTK_JUSTIFY_RIGHT);
	gtk_label_set_xalign (GTK_LABEL (body_label), 1.0);
	gtk_grid_attach(GTK_GRID(table1), body_label, 0, 3, 1, 1);

	adv_condition_label = gtk_label_new (_("Condition:"));
	gtk_widget_show (adv_condition_label);
	gtk_label_set_justify (GTK_LABEL (adv_condition_label), GTK_JUSTIFY_RIGHT);
	gtk_label_set_xalign (GTK_LABEL (adv_condition_label), 1.0);
	gtk_grid_attach(GTK_GRID(table1), adv_condition_label, 0, 4, 1, 1);

	checkbtn_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show (checkbtn_hbox);
	gtk_box_pack_start (GTK_BOX (vbox1), checkbtn_hbox, TRUE, TRUE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (checkbtn_hbox), 8);

	case_checkbtn = gtk_check_button_new_with_label (_("Case sensitive"));
	gtk_widget_show (case_checkbtn);
	gtk_box_pack_start (GTK_BOX (checkbtn_hbox), case_checkbtn,
			    FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(case_checkbtn), "changed",
			 G_CALLBACK(case_changed), NULL);

	adv_search_checkbtn = gtk_check_button_new_with_label (_("Extended Search"));
	gtk_widget_show (adv_search_checkbtn);
	gtk_box_pack_start (GTK_BOX (checkbtn_hbox), adv_search_checkbtn,
			    FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(adv_search_checkbtn), "changed",
			 G_CALLBACK(case_changed), NULL);

	confirm_area = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
	gtk_widget_show (confirm_area);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(confirm_area),
				  GTK_BUTTONBOX_END);
	gtk_box_set_spacing(GTK_BOX(confirm_area), 5);

	gtkut_stock_button_add_help(confirm_area, &help_btn);

	all_btn = gtk_button_new_with_mnemonic(_("Find _all"));
	gtk_widget_set_can_default(all_btn, TRUE);
	gtk_box_pack_start(GTK_BOX(confirm_area), all_btn, TRUE, TRUE, 0);
	gtk_widget_show(all_btn);

	prev_btn = gtkut_stock_button("go-previous", _("_Previous"));
	gtk_widget_set_can_default(prev_btn, TRUE);
	gtk_box_pack_start(GTK_BOX(confirm_area), prev_btn, TRUE, TRUE, 0);
	gtk_widget_show(prev_btn);

	next_btn = gtkut_stock_button("go-next", _("_Next"));
	gtk_widget_set_can_default(next_btn, TRUE);
	gtk_box_pack_start(GTK_BOX(confirm_area), next_btn, TRUE, TRUE, 0);
	gtk_widget_show(next_btn);

	close_btn = gtkut_stock_button("window-close", _("_Close"));
	gtk_widget_set_can_default(close_btn, TRUE);
	gtk_box_pack_start(GTK_BOX(confirm_area), close_btn, TRUE, TRUE, 0);
	gtk_widget_show(close_btn);

	/* stop button hidden */
	stop_btn = gtk_button_new_with_mnemonic(_("_Stop"));
	gtk_widget_set_can_default(stop_btn, TRUE);
	gtk_box_pack_start(GTK_BOX(confirm_area), stop_btn, TRUE, TRUE, 0);

	gtk_box_pack_start (GTK_BOX (vbox1), confirm_area, FALSE, FALSE, 0);
	gtk_widget_grab_default(next_btn);

	SET_TOGGLE_SENSITIVITY_REVERSE(adv_search_checkbtn, bool_optmenu)
	SET_TOGGLE_SENSITIVITY_REVERSE(adv_search_checkbtn, from_label)
	SET_TOGGLE_SENSITIVITY_REVERSE(adv_search_checkbtn, from_entry)
	SET_TOGGLE_SENSITIVITY_REVERSE(adv_search_checkbtn, to_label)
	SET_TOGGLE_SENSITIVITY_REVERSE(adv_search_checkbtn, to_entry)
	SET_TOGGLE_SENSITIVITY_REVERSE(adv_search_checkbtn, subject_label)
	SET_TOGGLE_SENSITIVITY_REVERSE(adv_search_checkbtn, subject_entry)
	SET_TOGGLE_SENSITIVITY_REVERSE(adv_search_checkbtn, body_label)
	SET_TOGGLE_SENSITIVITY_REVERSE(adv_search_checkbtn, body_entry)
	SET_TOGGLE_SENSITIVITY(adv_search_checkbtn, adv_condition_label)
	SET_TOGGLE_SENSITIVITY(adv_search_checkbtn, adv_condition_entry)
	SET_TOGGLE_SENSITIVITY(adv_search_checkbtn, adv_condition_btn)
	SET_TOGGLE_SENSITIVITY_REVERSE(adv_search_checkbtn, case_checkbtn)

	g_signal_connect(G_OBJECT(help_btn), "clicked",
			 G_CALLBACK(manual_open_with_anchor_cb),
			 MANUAL_ANCHOR_SEARCHING);
	g_signal_connect(G_OBJECT(clear_btn), "clicked",
			 G_CALLBACK(summary_search_clear), NULL);
	g_signal_connect(G_OBJECT(all_btn), "clicked",
			 G_CALLBACK(summary_search_all_clicked), NULL);
	g_signal_connect(G_OBJECT(prev_btn), "clicked",
			 G_CALLBACK(summary_search_prev_clicked), NULL);
	g_signal_connect(G_OBJECT(next_btn), "clicked",
			 G_CALLBACK(summary_search_next_clicked), NULL);
	g_signal_connect_closure
		(G_OBJECT(close_btn), "clicked",
		 g_cclosure_new_swap(G_CALLBACK(gtk_widget_hide),
	     window, NULL), FALSE);
	g_signal_connect(G_OBJECT(stop_btn), "clicked",
			 G_CALLBACK(summary_search_stop_clicked), NULL);

	search_window.window = window;
	search_window.bool_optmenu = bool_optmenu;
	search_window.from_label = from_label;
	search_window.from_entry = from_entry;
	search_window.to_label = to_label;
	search_window.to_entry = to_entry;
	search_window.subject_label = subject_label;
	search_window.subject_entry = subject_entry;
	search_window.body_label = body_label;
	search_window.body_entry = body_entry;
	search_window.adv_condition_label = adv_condition_label;
	search_window.adv_condition_entry = adv_condition_entry;
	search_window.adv_condition_btn = adv_condition_btn;
	search_window.case_checkbtn = case_checkbtn;
	search_window.adv_search_checkbtn = adv_search_checkbtn;
	search_window.clear_btn = clear_btn;
	search_window.help_btn = help_btn;
	search_window.all_btn = all_btn;
	search_window.prev_btn = prev_btn;
	search_window.next_btn = next_btn;
	search_window.close_btn = close_btn;
	search_window.stop_btn = stop_btn;
	search_window.advsearch = NULL;
	search_window.matcher_is_outdated = TRUE;
	search_window.search_in_progress = FALSE;
	search_window.matched_msgnums = NULL;
	search_window.is_searching = is_searching;
}

static gboolean summary_search_verify_match(MsgInfo *msg)
{
	gpointer msgnum = GUINT_TO_POINTER(msg->msgnum);

	if (g_hash_table_lookup(search_window.matched_msgnums, msgnum) != NULL)
		return TRUE;
	else
		return FALSE;
}

static gboolean summary_search_progress_cb(gpointer data, guint at, guint matched, guint total)
{
	if (!search_window.is_searching) {
		search_window.matcher_is_outdated = TRUE;
		return FALSE;
	}

	return summaryview_search_root_progress(search_window.summaryview, at, matched, total);
}

static gboolean summary_search_prepare_matcher()
{
	gboolean adv_search;
	gboolean bool_and = FALSE;
	gboolean case_sens = FALSE;
	gchar *matcher_str;
	gint match_type;
	gchar *from_str = NULL, *to_str = NULL, *subject_str = NULL;
	gchar *body_str = NULL;
	GSList *matchers = NULL;

	if (!search_window.matcher_is_outdated)
		return TRUE;

	if (search_window.advsearch == NULL) {
		search_window.advsearch = advsearch_new();
		advsearch_set_on_error_cb(search_window.advsearch, NULL, NULL); /* TODO */
		advsearch_set_on_progress_cb(search_window.advsearch, 
			summary_search_progress_cb, NULL);
	}

	adv_search = gtk_toggle_button_get_active
			(GTK_TOGGLE_BUTTON(search_window.adv_search_checkbtn));

	if (adv_search) {
		matcher_str = add_history_get(search_window.adv_condition_entry, &prefs_common.summary_search_adv_condition_history);
	} else {
		MatcherList *matcher_list;
		bool_and = combobox_get_active_data(GTK_COMBO_BOX(search_window.bool_optmenu));
		case_sens = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(search_window.case_checkbtn));

		from_str    = add_history_get(search_window.from_entry, &prefs_common.summary_search_from_history);
		to_str      = add_history_get(search_window.to_entry, &prefs_common.summary_search_to_history);
		subject_str = add_history_get(search_window.subject_entry, &prefs_common.summary_search_subject_history);
		body_str    = add_history_get(search_window.body_entry, &prefs_common.summary_search_body_history);

		if (!from_str && !to_str && !subject_str && !body_str) {
			/* TODO: warn if no search criteria? (or make buttons enabled only when
 			 * at least one search criteria has been set */
			return FALSE;
		}

		match_type = case_sens ? MATCHTYPE_MATCH : MATCHTYPE_MATCHCASE;

		if (from_str) {
			MatcherProp *prop = matcherprop_new(MATCHCRITERIA_FROM, NULL, match_type, from_str, 0);
			matchers = g_slist_append(matchers, prop);
		}
		if (to_str) {
			MatcherProp *prop = matcherprop_new(MATCHCRITERIA_TO, NULL, match_type, to_str, 0);
			matchers = g_slist_append(matchers, prop);
		}
		if (subject_str) {
			MatcherProp *prop = matcherprop_new(MATCHCRITERIA_SUBJECT, NULL, match_type, subject_str, 0);
			matchers = g_slist_append(matchers, prop);
		}
		if (body_str) {
			MatcherProp *prop = matcherprop_new(MATCHCRITERIA_BODY_PART, NULL, match_type, body_str, 0);
			matchers = g_slist_append(matchers, prop);
		}
		g_free(from_str);
		g_free(to_str);
		g_free(subject_str);
		g_free(body_str);

		matcher_list = matcherlist_new(matchers, bool_and);
		if (!matcher_list)
			return FALSE;

		matcher_str = matcherlist_to_string(matcher_list);
		matcherlist_free(matcher_list);
	}
	if (!matcher_str)
		return FALSE;

	advsearch_set(search_window.advsearch, ADVANCED_SEARCH_EXTENDED,
		      matcher_str);

	debug_print("Advsearch set: %s\n", matcher_str);
	g_free(matcher_str);

	if (!advsearch_has_proper_predicate(search_window.advsearch))
		return FALSE;

	search_window.matcher_is_outdated = FALSE;

	return TRUE;
}

static gboolean summary_search_prereduce_msg_list()
{
	MsgInfoList *msglist = NULL;
	MsgNumberList *msgnums = NULL;
	MsgNumberList *cur;
	SummaryView *summaryview = search_window.summaryview;
	gboolean result;
	FolderItem *item = summaryview->folder_item;
	static GdkCursor *watch_cursor = NULL;
	if (!watch_cursor)
		watch_cursor = gdk_cursor_new_for_display(
				gtk_widget_get_display(summaryview->scrolledwin), GDK_WATCH);

	if (search_window.matcher_is_outdated && !summary_search_prepare_matcher())
		return FALSE;

	main_window_cursor_wait(mainwindow_get_mainwindow());
	gdk_window_set_cursor(gtk_widget_get_window(search_window.window), watch_cursor);
	statusbar_print_all(_("Searching in %s... \n"),
			item->path ? item->path : "(null)");

	result = advsearch_search_msgs_in_folders(search_window.advsearch,
			&msglist, item, FALSE);
	statusbar_pop_all();
	statusbar_progress_all(0, 0, 0);
	gdk_window_set_cursor(gtk_widget_get_window(search_window.window), NULL);
	main_window_cursor_normal(mainwindow_get_mainwindow());

	if (!result)
		return FALSE;

	msgnums = procmsg_get_number_list_for_msgs(msglist);
	procmsg_msg_list_free(msglist);

	if (search_window.matched_msgnums == NULL)
		search_window.matched_msgnums = g_hash_table_new(g_direct_hash, NULL);

	g_hash_table_remove_all(search_window.matched_msgnums);

	for (cur = msgnums; cur != NULL; cur = cur->next)
		g_hash_table_insert(search_window.matched_msgnums, cur->data, GINT_TO_POINTER(1));

	g_slist_free(msgnums);

	return TRUE;
}

static void summary_search_execute(gboolean backward, gboolean search_all)
{
	SummaryView *summaryview = search_window.summaryview;
	GtkCMCTree *ctree = GTK_CMCTREE(summaryview->ctree);
	GtkCMCTreeNode *node;
	MsgInfo *msginfo;
	gboolean all_searched = FALSE;
	gboolean matched = FALSE;
	gint i = 0;

	if (summary_is_locked(summaryview))
		return;

	summary_lock(summaryview);

	search_window.is_searching = TRUE;
	main_window_cursor_wait(summaryview->mainwin);
	summary_show_stop_button();

	if (search_window.matcher_is_outdated && !summary_search_prereduce_msg_list())
		goto exit;

	if (search_all) {
		summary_freeze(summaryview);
		summary_unselect_all(summaryview);
		node = GTK_CMCTREE_NODE(GTK_CMCLIST(ctree)->row_list);
		backward = FALSE;
	} else if (!summaryview->selected) {
		/* no selection, search from from list start */
		if (backward)
			node = GTK_CMCTREE_NODE(GTK_CMCLIST(ctree)->row_list_end);
		else
			node = GTK_CMCTREE_NODE(GTK_CMCLIST(ctree)->row_list);

		if (!node) {
			search_window.is_searching = FALSE;
			summary_hide_stop_button();
			main_window_cursor_normal(summaryview->mainwin);
			summary_unlock(summaryview);
			return;
		}
	} else {
		/* search from current selection */
		if (backward)
			node = gtkut_ctree_node_prev(ctree, summaryview->selected);
		else
			node = gtkut_ctree_node_next(ctree, summaryview->selected);
	}

	for (; search_window.is_searching; i++) {
		if (!node) {
			gchar *str;
			AlertValue val;

			if (search_all)
				break;

			if (all_searched) {
				alertpanel_full(_("Search failed"),
						_("Search string not found."),
				 		"window-close-symbolic", _("_Close"), NULL, NULL,
						NULL, NULL, ALERTFOCUS_FIRST,
						FALSE, NULL, ALERT_WARNING);
				break;
			}

			if (backward)
				str = _("Beginning of list reached; continue from end?");
			else
				str = _("End of list reached; continue from beginning?");

			val = alertpanel(_("Search finished"), str,
					 NULL, _("_No"), NULL, _("_Yes"), NULL, NULL,
					 ALERTFOCUS_SECOND);
			if (G_ALERTALTERNATE == val) {
				if (backward) {
					node = GTK_CMCTREE_NODE(GTK_CMCLIST(ctree)->row_list_end);
				} else {
					node = GTK_CMCTREE_NODE(GTK_CMCLIST(ctree)->row_list);
				}

				all_searched = TRUE;

				manage_window_focus_in(search_window.window, NULL, NULL);
			} else
				break;
		}

		msginfo = gtk_cmctree_node_get_row_data(ctree, node);

		if (msginfo)
			matched = summary_search_verify_match(msginfo);
		else
			matched = FALSE;

		if (matched) {
			if (search_all) {
				gtk_cmctree_select(ctree, node);
			} else {
				summary_unlock(summaryview);
				summary_select_node(summaryview, node,
						OPEN_SELECTED_ON_SEARCH_RESULTS);
				summary_lock(summaryview);
				break;
			}
		}

		if (i % (search_window.is_fast ? 1000 : 100) == 0)
			GTK_EVENTS_FLUSH();

		node = backward ? gtkut_ctree_node_prev(ctree, node)
				: gtkut_ctree_node_next(ctree, node);
	}

exit:
	search_window.is_searching = FALSE;
	summary_hide_stop_button();
	main_window_cursor_normal(summaryview->mainwin);
	if (search_all)
		summary_thaw_with_status(summaryview);
	summary_unlock(summaryview);
}

static void summary_search_clear(GtkButton *button, gpointer data)
{
	if (gtk_toggle_button_get_active
		(GTK_TOGGLE_BUTTON(search_window.adv_search_checkbtn))) {
		gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN((search_window.adv_condition_entry)))), "");
	} else {
		gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN((search_window.from_entry)))), "");
		gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN((search_window.to_entry)))), "");
		gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN((search_window.subject_entry)))), "");
		gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN((search_window.body_entry)))), "");
	}
	/* stop searching */
	if (search_window.is_searching) {
		search_window.is_searching = FALSE;
	}
	search_window.matcher_is_outdated = TRUE;
}

static void summary_search_prev_clicked(GtkButton *button, gpointer data)
{
	summary_search_execute(TRUE, FALSE);
}

static void summary_search_next_clicked(GtkButton *button, gpointer data)
{
	summary_search_execute(FALSE, FALSE);
}

static void summary_search_all_clicked(GtkButton *button, gpointer data)
{
	summary_search_execute(FALSE, TRUE);
}

static void adv_condition_btn_done(MatcherList * matchers)
{
	gchar *str;

	cm_return_if_fail(
			mainwindow_get_mainwindow()->summaryview->quicksearch != NULL);

	if (matchers == NULL) {
		return;
	}

	str = matcherlist_to_string(matchers);
	search_window.matcher_is_outdated = TRUE;

	if (str != NULL) {
		gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN((search_window.adv_condition_entry)))), str);
		g_free(str);
	}
}

static void summary_search_stop_clicked(GtkButton *button, gpointer data)
{
	search_window.is_searching = FALSE;
}

static void adv_condition_btn_clicked(GtkButton *button, gpointer data)
{
	const gchar * cond_str;
	MatcherList * matchers = NULL;

	cm_return_if_fail( search_window.window != NULL );

	/* re-use the current search value if it's a condition expression,
	   otherwise ignore it silently */
	cond_str = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(search_window.adv_condition_entry));
	if (cond_str && *cond_str != '\0') {
		matchers = matcher_parser_get_cond((gchar*)cond_str, NULL);
	}

	prefs_matcher_open(matchers, adv_condition_btn_done);

	if (matchers != NULL) {
		matcherlist_free(matchers);
	}
};

static void optmenu_changed(GtkComboBox *widget, gpointer user_data)
{
	search_window.matcher_is_outdated = TRUE;
}

static void from_changed(void)
{
	if (!search_window.from_entry_has_focus)
		gtk_widget_grab_focus(search_window.from_entry);
	search_window.matcher_is_outdated = TRUE;
}

static void to_changed(void)
{
	if (!search_window.to_entry_has_focus)
		gtk_widget_grab_focus(search_window.to_entry);
	search_window.matcher_is_outdated = TRUE;
}

static void subject_changed(void)
{
	if (!search_window.subject_entry_has_focus)
		gtk_widget_grab_focus(search_window.subject_entry);
	search_window.matcher_is_outdated = TRUE;
}

static void body_changed(void)
{
	if (!search_window.body_entry_has_focus)
		gtk_widget_grab_focus(search_window.body_entry);
	search_window.matcher_is_outdated = TRUE;
}

static void adv_condition_changed(void)
{
	if (!search_window.adv_condition_entry_has_focus)
		gtk_widget_grab_focus(search_window.adv_condition_entry);
	search_window.matcher_is_outdated = TRUE;
}

static void case_changed(GtkToggleButton *togglebutton, gpointer user_data)
{
	search_window.matcher_is_outdated = TRUE;
}

static gboolean from_entry_focus_evt_in(GtkWidget *widget, GdkEventFocus *event,
			      	  gpointer data)
{
	search_window.from_entry_has_focus = TRUE;
	return FALSE;
}

static gboolean from_entry_focus_evt_out(GtkWidget *widget, GdkEventFocus *event,
			      	  gpointer data)
{
	search_window.from_entry_has_focus = FALSE;
	return FALSE;
}

static gboolean to_entry_focus_evt_in(GtkWidget *widget, GdkEventFocus *event,
			      	  gpointer data)
{
	search_window.to_entry_has_focus = TRUE;
	return FALSE;
}

static gboolean to_entry_focus_evt_out(GtkWidget *widget, GdkEventFocus *event,
			      	  gpointer data)
{
	search_window.to_entry_has_focus = FALSE;
	return FALSE;
}

static gboolean subject_entry_focus_evt_in(GtkWidget *widget, GdkEventFocus *event,
			      	  gpointer data)
{
	search_window.subject_entry_has_focus = TRUE;
	return FALSE;
}

static gboolean subject_entry_focus_evt_out(GtkWidget *widget, GdkEventFocus *event,
			      	  gpointer data)
{
	search_window.subject_entry_has_focus = FALSE;
	return FALSE;
}

static gboolean body_entry_focus_evt_in(GtkWidget *widget, GdkEventFocus *event,
			      	  gpointer data)
{
	search_window.body_entry_has_focus = TRUE;
	return FALSE;
}

static gboolean body_entry_focus_evt_out(GtkWidget *widget, GdkEventFocus *event,
			      	  gpointer data)
{
	search_window.body_entry_has_focus = FALSE;
	return FALSE;
}

static gboolean adv_condition_entry_focus_evt_in(GtkWidget *widget, GdkEventFocus *event,
			      	  gpointer data)
{
	search_window.adv_condition_entry_has_focus = TRUE;
	return FALSE;
}

static gboolean adv_condition_entry_focus_evt_out(GtkWidget *widget, GdkEventFocus *event,
			      	  gpointer data)
{
	search_window.adv_condition_entry_has_focus = FALSE;
	return FALSE;
}

static gboolean key_pressed(GtkWidget *widget, GdkEventKey *event,
			    gpointer data)
{
	if (event && (event->keyval == GDK_KEY_Escape)) {
		/* ESC key will:
			- stop a running search
			- close the search window if no search is running
		*/
		if (!search_window.is_searching) {
			gtk_widget_hide(search_window.window);
		} else {
			search_window.is_searching = FALSE;
		}
	}

	if (event && (event->keyval == GDK_KEY_Return || event->keyval == GDK_KEY_KP_Enter)) {
		if (!search_window.is_searching) {
			summary_search_execute(FALSE, FALSE);
		}
	}

	if (event && (event->keyval == GDK_KEY_Down || event->keyval == GDK_KEY_Up)) {
		if (search_window.from_entry_has_focus) {
			combobox_set_value_from_arrow_key(
					GTK_COMBO_BOX(search_window.from_entry),
					event->keyval);
			return TRUE;
		}
		if (search_window.to_entry_has_focus) {
			combobox_set_value_from_arrow_key(
					GTK_COMBO_BOX(search_window.to_entry),
					event->keyval);
			return TRUE;
		}
		if (search_window.subject_entry_has_focus) {
			combobox_set_value_from_arrow_key(
					GTK_COMBO_BOX(search_window.subject_entry),
					event->keyval);
			return TRUE;
		}
		if (search_window.body_entry_has_focus) {
			combobox_set_value_from_arrow_key(
					GTK_COMBO_BOX(search_window.body_entry),
					event->keyval);
			return TRUE;
		}
		if (search_window.adv_condition_entry_has_focus) {
			combobox_set_value_from_arrow_key(
					GTK_COMBO_BOX(search_window.adv_condition_entry),
					event->keyval);
			return TRUE;
		}
	}

	return FALSE;
}
