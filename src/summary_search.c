/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2002 Hiroyuki Yamamoto
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
#include <gtk/gtkwidget.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtktable.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkentry.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkcheckbutton.h>
#include <gtk/gtkhbbox.h>
#include <gtk/gtkbutton.h>
#include <gtk/gtkctree.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "intl.h"
#include "main.h"
#include "summary_search.h"
#include "summaryview.h"
#include "messageview.h"
#include "mainwindow.h"
#include "utils.h"
#include "gtkutils.h"
#include "manage_window.h"
#include "alertpanel.h"

static GtkWidget *window;
static GtkWidget *from_entry;
static GtkWidget *to_entry;
static GtkWidget *subject_entry;
static GtkWidget *body_entry;
static GtkWidget *case_checkbtn;
static GtkWidget *backward_checkbtn;
static GtkWidget *all_checkbtn;
static GtkWidget *search_btn;
static GtkWidget *clear_btn;
static GtkWidget *close_btn;

static void summary_search_create(SummaryView *summaryview);
static void summary_search_execute(GtkButton *button, gpointer data);
static void summary_search_clear(GtkButton *button, gpointer data);
static void from_activated(void);
static void to_activated(void);
static void subject_activated(void);
static void body_activated(void);
static void key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data);

void summary_search(SummaryView *summaryview)
{
	if (!window)
		summary_search_create(summaryview);
	else
		gtk_widget_hide(window);

	gtk_widget_grab_focus(search_btn);
	gtk_widget_grab_focus(subject_entry);
	gtk_widget_show(window);
}

static void summary_search_create(SummaryView *summaryview)
{
	GtkWidget *vbox1;
	GtkWidget *table1;
	GtkWidget *from_label;
	GtkWidget *to_label;
	GtkWidget *subject_label;
	GtkWidget *body_label;
	GtkWidget *checkbtn_hbox;
	GtkWidget *confirm_area;

	window = gtk_window_new (GTK_WINDOW_DIALOG);
	gtk_window_set_title (GTK_WINDOW (window), _("Search messages"));
	gtk_widget_set_usize (window, 450, -1);
	gtk_window_set_policy(GTK_WINDOW(window), FALSE, TRUE, TRUE);
	gtk_container_set_border_width (GTK_CONTAINER (window), 8);
	gtk_signal_connect(GTK_OBJECT(window), "delete_event",
			   GTK_SIGNAL_FUNC(gtk_widget_hide_on_delete), NULL);
	gtk_signal_connect(GTK_OBJECT(window), "key_press_event",
			   GTK_SIGNAL_FUNC(key_pressed), NULL);
	MANAGE_WINDOW_SIGNALS_CONNECT(window);

	vbox1 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox1);
	gtk_container_add (GTK_CONTAINER (window), vbox1);

	table1 = gtk_table_new (4, 3, FALSE);
	gtk_widget_show (table1);
	gtk_box_pack_start (GTK_BOX (vbox1), table1, TRUE, TRUE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (table1), 4);
	gtk_table_set_row_spacings (GTK_TABLE (table1), 8);
	gtk_table_set_col_spacings (GTK_TABLE (table1), 8);

	from_entry = gtk_entry_new ();
	gtk_widget_show (from_entry);
	gtk_table_attach (GTK_TABLE (table1), from_entry, 1, 3, 0, 1,
			  GTK_EXPAND|GTK_FILL, 0, 0, 0);
	gtk_signal_connect(GTK_OBJECT(from_entry), "activate",
			   GTK_SIGNAL_FUNC(from_activated), summaryview);

	to_entry = gtk_entry_new ();
	gtk_widget_show (to_entry);
	gtk_table_attach (GTK_TABLE (table1), to_entry, 1, 3, 1, 2,
			  GTK_EXPAND|GTK_FILL, 0, 0, 0);
	gtk_signal_connect(GTK_OBJECT(to_entry), "activate",
			   GTK_SIGNAL_FUNC(to_activated), summaryview);

	subject_entry = gtk_entry_new ();
	gtk_widget_show (subject_entry);
	gtk_table_attach (GTK_TABLE (table1), subject_entry, 1, 3, 2, 3,
			  GTK_EXPAND|GTK_FILL, 0, 0, 0);
	gtk_signal_connect(GTK_OBJECT(subject_entry), "activate",
			   GTK_SIGNAL_FUNC(subject_activated), summaryview);

	body_entry = gtk_entry_new ();
	gtk_widget_show (body_entry);
	gtk_table_attach (GTK_TABLE (table1), body_entry, 1, 3, 3, 4,
			  GTK_EXPAND|GTK_FILL, 0, 0, 0);
	gtk_signal_connect(GTK_OBJECT(body_entry), "activate",
			   GTK_SIGNAL_FUNC(body_activated), summaryview);

	from_label = gtk_label_new (_("From:"));
	gtk_widget_show (from_label);
	gtk_table_attach (GTK_TABLE (table1), from_label, 0, 1, 0, 1,
			  GTK_FILL, 0, 0, 0);
	gtk_label_set_justify (GTK_LABEL (from_label), GTK_JUSTIFY_RIGHT);
	gtk_misc_set_alignment (GTK_MISC (from_label), 1, 0.5);

	to_label = gtk_label_new (_("To:"));
	gtk_widget_show (to_label);
	gtk_table_attach (GTK_TABLE (table1), to_label, 0, 1, 1, 2,
			  GTK_FILL, 0, 0, 0);
	gtk_label_set_justify (GTK_LABEL (to_label), GTK_JUSTIFY_RIGHT);
	gtk_misc_set_alignment (GTK_MISC (to_label), 1, 0.5);

	subject_label = gtk_label_new (_("Subject:"));
	gtk_widget_show (subject_label);
	gtk_table_attach (GTK_TABLE (table1), subject_label, 0, 1, 2, 3,
			  GTK_FILL, 0, 0, 0);
	gtk_label_set_justify (GTK_LABEL (subject_label), GTK_JUSTIFY_RIGHT);
	gtk_misc_set_alignment (GTK_MISC (subject_label), 1, 0.5);

	body_label = gtk_label_new (_("Body:"));
	gtk_widget_show (body_label);
	gtk_table_attach (GTK_TABLE (table1), body_label, 0, 1, 3, 4,
			  GTK_FILL, 0, 0, 0);
	gtk_label_set_justify (GTK_LABEL (body_label), GTK_JUSTIFY_RIGHT);
	gtk_misc_set_alignment (GTK_MISC (body_label), 1, 0.5);

	checkbtn_hbox = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (checkbtn_hbox);
	gtk_box_pack_start (GTK_BOX (vbox1), checkbtn_hbox, TRUE, TRUE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (checkbtn_hbox), 8);

	case_checkbtn = gtk_check_button_new_with_label (_("Case sensitive"));
	gtk_widget_show (case_checkbtn);
	gtk_box_pack_start (GTK_BOX (checkbtn_hbox), case_checkbtn,
			    FALSE, FALSE, 0);

	backward_checkbtn =
		gtk_check_button_new_with_label (_("Backward search"));
	gtk_widget_show (backward_checkbtn);
	gtk_box_pack_start (GTK_BOX (checkbtn_hbox), backward_checkbtn,
			    FALSE, FALSE, 0);

	all_checkbtn =
		gtk_check_button_new_with_label (_("Select all matched"));
	gtk_widget_show (all_checkbtn);
	gtk_box_pack_start (GTK_BOX (checkbtn_hbox), all_checkbtn,
			    FALSE, FALSE, 0);

	gtkut_button_set_create(&confirm_area,
				&search_btn, _("Search"),
				&clear_btn,  _("Clear"),
				&close_btn,  _("Close"));
	gtk_widget_show (confirm_area);
	gtk_box_pack_start (GTK_BOX (vbox1), confirm_area, FALSE, FALSE, 0);
	gtk_widget_grab_default(search_btn);

	gtk_signal_connect(GTK_OBJECT(search_btn), "clicked",
			   GTK_SIGNAL_FUNC(summary_search_execute),
			   summaryview);
	gtk_signal_connect(GTK_OBJECT(clear_btn), "clicked",
			   GTK_SIGNAL_FUNC(summary_search_clear),
			   summaryview);
	gtk_signal_connect_object(GTK_OBJECT(close_btn), "clicked",
				  GTK_SIGNAL_FUNC(gtk_widget_hide),
				  GTK_OBJECT(window));
}

static void summary_search_execute(GtkButton *button, gpointer data)
{
	SummaryView *summaryview = data;
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GtkCTreeNode *node;
	MsgInfo *msginfo;
	gboolean case_sens;
	gboolean backward;
	gboolean search_all;
	gboolean all_searched = FALSE;
	gboolean from_matched;
	gboolean   to_matched;
	gboolean subj_matched;
	gboolean body_matched;
	gchar *body_str;
	wchar_t *wcs_hs, *fromwcs, *towcs, *subjwcs;
	wchar_t *(* WCSFindFunc) (const wchar_t *haystack,
				  const wchar_t *needle);

	if (summary_is_locked(summaryview)) return;
	summary_lock(summaryview);

	case_sens = gtk_toggle_button_get_active
		(GTK_TOGGLE_BUTTON(case_checkbtn));
	backward = gtk_toggle_button_get_active
		(GTK_TOGGLE_BUTTON(backward_checkbtn));
	search_all = gtk_toggle_button_get_active
		(GTK_TOGGLE_BUTTON(all_checkbtn));

	if (case_sens)
#if HAVE_WCSSTR
		WCSFindFunc = wcsstr;
#else
#if HAVE_WCSWCS
		WCSFindFunc = wcswcs;
#else
		WCSFindFunc = wcscasestr;
#endif
#endif /* HAVE_WCSSTR */
	else
		WCSFindFunc = wcscasestr;

	fromwcs = (wchar_t *)GTK_ENTRY(from_entry)->text;
	towcs   = (wchar_t *)GTK_ENTRY(to_entry)->text;
	subjwcs = (wchar_t *)GTK_ENTRY(subject_entry)->text;
	body_str = gtk_entry_get_text(GTK_ENTRY(body_entry));

	if (search_all) {
		gtk_clist_freeze(GTK_CLIST(ctree));
		gtk_clist_unselect_all(GTK_CLIST(ctree));
		node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);
	} else if (!summaryview->selected) {
		if (backward)
			node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list_end);
		else
			node = GTK_CTREE_NODE(GTK_CLIST(ctree)->row_list);

		if (!node) {
			summary_unlock(summaryview);
			return;
		}
	} else {
		if (backward)
			node = GTK_CTREE_NODE_PREV(summaryview->selected);
		else
			node = GTK_CTREE_NODE_NEXT(summaryview->selected);
	}

	if (*body_str)
		main_window_cursor_wait(summaryview->mainwin);

	for (;;) {
		if (!node) {
			gchar *str;
			AlertValue val;

			if (search_all) {
				gtk_clist_thaw(GTK_CLIST(ctree));
				break;
			}

			if (all_searched) {
				alertpanel_message
					(_("Search failed"),
					 _("Search string not found."));
				break;
			}

			if (backward)
				str = _("Beginning of list reached; continue from end?");
			else
				str = _("End of list reached; continue from beginning?");

			val = alertpanel(_("Search finished"), str,
					 _("Yes"), _("No"), NULL);
			if (G_ALERTDEFAULT == val) {
				if (backward)
					node = GTK_CTREE_NODE
						(GTK_CLIST(ctree)->row_list_end);
				else
					node = GTK_CTREE_NODE
						(GTK_CLIST(ctree)->row_list);

				all_searched = TRUE;

				manage_window_focus_in(window, NULL, NULL);
			} else
				break;
		}

		from_matched = to_matched = subj_matched = body_matched = FALSE;

		msginfo = gtk_ctree_node_get_row_data(ctree, node);

		if (*fromwcs && msginfo->from) {
			wcs_hs = strdup_mbstowcs(msginfo->from);
			if (wcs_hs && WCSFindFunc(wcs_hs, fromwcs) != NULL)
				from_matched = TRUE;
			g_free(wcs_hs);
		}
		if (*towcs && msginfo->to) {
			wcs_hs = strdup_mbstowcs(msginfo->to);
			if (wcs_hs && WCSFindFunc(wcs_hs, towcs) != NULL)
				to_matched = TRUE;
			g_free(wcs_hs);
		}
		if (*subjwcs && msginfo->subject) {
			wcs_hs = strdup_mbstowcs(msginfo->subject);
			if (wcs_hs && WCSFindFunc(wcs_hs, subjwcs) != NULL)
				subj_matched = TRUE;
			g_free(wcs_hs);
		}
		if (*body_str) {
			if (procmime_find_string(msginfo, body_str, case_sens))
				body_matched = TRUE;
		}

		if (from_matched || to_matched || subj_matched || body_matched) {
			if (search_all)
				gtk_ctree_select(ctree, node);
			else {
				if (messageview_is_visible
					(summaryview->messageview)) {
					summary_unlock(summaryview);
					summary_select_node
						(summaryview, node, TRUE, TRUE);
					summary_lock(summaryview);
					if (body_matched) {
						messageview_search_string
							(summaryview->messageview,
							 body_str, case_sens);
					}
				} else {
					summary_select_node
						(summaryview, node, FALSE, TRUE);
				}
				break;
			}
		}

		node = backward ? GTK_CTREE_NODE_PREV(node)
				: GTK_CTREE_NODE_NEXT(node);
	}

	if (*body_str)
		main_window_cursor_normal(summaryview->mainwin);

	summary_unlock(summaryview);
}

static void summary_search_clear(GtkButton *button, gpointer data)
{
	gtk_editable_delete_text(GTK_EDITABLE(from_entry),    0, -1);
	gtk_editable_delete_text(GTK_EDITABLE(to_entry),      0, -1);
	gtk_editable_delete_text(GTK_EDITABLE(subject_entry), 0, -1);
	gtk_editable_delete_text(GTK_EDITABLE(body_entry),    0, -1);
}

static void from_activated(void)
{
	gtk_widget_grab_focus(to_entry);
}

static void to_activated(void)
{
	gtk_widget_grab_focus(subject_entry);
}

static void subject_activated(void)
{
	gtk_button_clicked(GTK_BUTTON(search_btn));
}

static void body_activated(void)
{
	gtk_button_clicked(GTK_BUTTON(search_btn));
}

static void key_pressed(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	if (event && event->keyval == GDK_Escape)
		gtk_widget_hide(window);
}
