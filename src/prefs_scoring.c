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
#include "prefs_scoring.h"
#include "prefs_common.h"
#include "mainwindow.h"
#include "foldersel.h"
#include "manage_window.h"
#include "inc.h"
#include "utils.h"
#include "gtkutils.h"
#include "alertpanel.h"
#include "folder.h"
#include "scoring.h"

#include "matcher_parser.h"

static struct Scoring {
	GtkWidget *window;

	GtkWidget *ok_btn;
	GtkWidget *cond_entry;
	GtkWidget *score_entry;
	GtkWidget *kill_score_label;
	GtkWidget *kill_score_entry;
	GtkWidget *important_score_entry;

	GtkWidget *cond_clist;
} scoring;

/*
   parameter name, default value, pointer to the prefs variable, data type,
   pointer to the widget pointer,
   pointer to the function for data setting,
   pointer to the function for widget setting
 */

/* widget creating functions */
static void prefs_scoring_create		(void);

static void prefs_scoring_set_dialog	(ScoringProp * prop);
static void prefs_scoring_set_list	(void);

/* callback functions */
/* static void prefs_scoring_select_dest_cb	(void); */
static void prefs_scoring_register_cb	(void);
static void prefs_scoring_substitute_cb	(void);
static void prefs_scoring_delete_cb	(void);
static void prefs_scoring_up		(void);
static void prefs_scoring_down		(void);
static void prefs_scoring_select		(GtkCList	*clist,
					 gint		 row,
					 gint		 column,
					 GdkEvent	*event);

static gint prefs_scoring_deleted	(GtkWidget	*widget,
					 GdkEventAny	*event,
					 gpointer	 data);
static void prefs_scoring_key_pressed	(GtkWidget	*widget,
					 GdkEventKey	*event,
					 gpointer	 data);
static void prefs_scoring_cancel		(void);
static void prefs_scoring_ok		(void);

static void prefs_scoring_condition_define	(void);
static gint prefs_scoring_clist_set_row(gint row, ScoringProp * prop);
static void prefs_scoring_select_set_dialog(ScoringProp * prop);
static void prefs_scoring_reset_dialog(void);


static FolderItem * cur_item = NULL;
static gint cur_important_score;
static gint cur_kill_score;

void prefs_scoring_open(FolderItem * item)
{
	if (prefs_rc_is_readonly(SCORING_RC))
		return;

	inc_lock();

	if (!scoring.window) {
		prefs_scoring_create();
	}

	manage_window_set_transient(GTK_WINDOW(scoring.window));
	gtk_widget_grab_focus(scoring.ok_btn);

	cur_item = item;

	prefs_scoring_set_dialog(NULL);

	gtk_widget_show(scoring.window);
}

void prefs_scoring_open_with_scoring(ScoringProp * prop)
{
	inc_lock();

	if (!scoring.window) {
		prefs_scoring_create();
	}

	manage_window_set_transient(GTK_WINDOW(scoring.window));
	gtk_widget_grab_focus(scoring.ok_btn);

	prefs_scoring_set_dialog(prop);

	gtk_widget_show(scoring.window);
}

static void prefs_scoring_create(void)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;
	GtkWidget *confirm_area;

	GtkWidget *vbox1;
	GtkWidget *hbox1;
	GtkWidget *reg_hbox;
	GtkWidget *arrow;
	GtkWidget *btn_hbox;

	GtkWidget *cond_label;
	GtkWidget *cond_entry;
	GtkWidget *cond_btn;
	GtkWidget *score_label;
	GtkWidget *score_entry;

	GtkWidget *reg_btn;
	GtkWidget *subst_btn;
	GtkWidget *del_btn;

	GtkWidget *cond_hbox;
	GtkWidget *cond_scrolledwin;
	GtkWidget *cond_clist;

	GtkWidget *btn_vbox;
	GtkWidget *up_btn;
	GtkWidget *down_btn;

	GtkWidget *important_score_entry;
	GtkWidget *kill_score_entry;
	GtkWidget *kill_score_label;

	gchar *title[1];

	debug_print(_("Creating scoring setting window...\n"));

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
			      _("Scoring setting"));
	gtk_signal_connect (GTK_OBJECT(window), "delete_event",
			    GTK_SIGNAL_FUNC(prefs_scoring_deleted), NULL);
	gtk_signal_connect (GTK_OBJECT(window), "key_press_event",
			    GTK_SIGNAL_FUNC(prefs_scoring_key_pressed), NULL);
	gtk_signal_connect (GTK_OBJECT(window), "focus_in_event",
			    GTK_SIGNAL_FUNC(manage_window_focus_in), NULL);
	gtk_signal_connect (GTK_OBJECT(window), "focus_out_event",
			    GTK_SIGNAL_FUNC(manage_window_focus_out), NULL);
	gtk_signal_connect (GTK_OBJECT(ok_btn), "clicked",
			    GTK_SIGNAL_FUNC(prefs_scoring_ok), NULL);
	gtk_signal_connect (GTK_OBJECT(cancel_btn), "clicked",
			    GTK_SIGNAL_FUNC(prefs_scoring_cancel), NULL);

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_box_pack_start (GTK_BOX (vbox), vbox1, TRUE, TRUE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), 2);

	cond_label = gtk_label_new (_("Condition"));
	gtk_widget_show (cond_label);
	gtk_misc_set_alignment (GTK_MISC (cond_label), 0, 0.5);
	gtk_box_pack_start (GTK_BOX (vbox1), cond_label, FALSE, FALSE, 0);

	hbox1 = gtk_hbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, FALSE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), 2);

	cond_entry = gtk_entry_new ();
	gtk_widget_show (cond_entry);
	gtk_widget_set_usize (cond_entry, 300, -1);
	gtk_box_pack_start (GTK_BOX (hbox1), cond_entry, TRUE, TRUE, 0);

	cond_btn = gtk_button_new_with_label (_("Define ..."));
	gtk_widget_show (cond_btn);
	gtk_box_pack_start (GTK_BOX (hbox1), cond_btn, FALSE, FALSE, 0);
	gtk_signal_connect (GTK_OBJECT (cond_btn), "clicked",
			    GTK_SIGNAL_FUNC (prefs_scoring_condition_define),
			    NULL);

	hbox1 = gtk_hbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, FALSE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), 2);

	score_label = gtk_label_new (_("Score"));
	gtk_widget_show (score_label);
	gtk_misc_set_alignment (GTK_MISC (score_label), 0, 0.5);
	gtk_box_pack_start (GTK_BOX (hbox1), score_label, FALSE, FALSE, 0);

	score_entry = gtk_entry_new ();
	gtk_widget_show (score_entry);
	gtk_widget_set_usize (score_entry, 50, -1);
	gtk_box_pack_start (GTK_BOX (hbox1), score_entry, FALSE, FALSE, 0);

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
			    GTK_SIGNAL_FUNC (prefs_scoring_register_cb), NULL);

	subst_btn = gtk_button_new_with_label (_(" Substitute "));
	gtk_widget_show (subst_btn);
	gtk_box_pack_start (GTK_BOX (btn_hbox), subst_btn, FALSE, TRUE, 0);
	gtk_signal_connect (GTK_OBJECT (subst_btn), "clicked",
			    GTK_SIGNAL_FUNC (prefs_scoring_substitute_cb),
			    NULL);

	del_btn = gtk_button_new_with_label (_("Delete"));
	gtk_widget_show (del_btn);
	gtk_box_pack_start (GTK_BOX (btn_hbox), del_btn, FALSE, TRUE, 0);
	gtk_signal_connect (GTK_OBJECT (del_btn), "clicked",
			    GTK_SIGNAL_FUNC (prefs_scoring_delete_cb), NULL);

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

	title[0] = ("Registered rules");
	cond_clist = gtk_clist_new_with_titles(1, title);
	gtk_widget_show (cond_clist);
	gtk_container_add (GTK_CONTAINER (cond_scrolledwin), cond_clist);
	gtk_clist_set_column_width (GTK_CLIST (cond_clist), 0, 80);
	gtk_clist_set_selection_mode (GTK_CLIST (cond_clist),
				      GTK_SELECTION_BROWSE);
	GTK_WIDGET_UNSET_FLAGS (GTK_CLIST (cond_clist)->column[0].button,
				GTK_CAN_FOCUS);
	gtk_signal_connect (GTK_OBJECT (cond_clist), "select_row",
			    GTK_SIGNAL_FUNC (prefs_scoring_select), NULL);

	btn_vbox = gtk_vbox_new (FALSE, 8);
	gtk_widget_show (btn_vbox);
	gtk_box_pack_start (GTK_BOX (cond_hbox), btn_vbox, FALSE, FALSE, 0);

	up_btn = gtk_button_new_with_label (_("Up"));
	gtk_widget_show (up_btn);
	gtk_box_pack_start (GTK_BOX (btn_vbox), up_btn, FALSE, FALSE, 0);
	gtk_signal_connect (GTK_OBJECT (up_btn), "clicked",
			    GTK_SIGNAL_FUNC (prefs_scoring_up), NULL);

	down_btn = gtk_button_new_with_label (_("Down"));
	gtk_widget_show (down_btn);
	gtk_box_pack_start (GTK_BOX (btn_vbox), down_btn, FALSE, FALSE, 0);
	gtk_signal_connect (GTK_OBJECT (down_btn), "clicked",
			    GTK_SIGNAL_FUNC (prefs_scoring_down), NULL);

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox1, TRUE, TRUE, 0);

	kill_score_label = gtk_label_new (_("Kill score"));
	gtk_widget_show (kill_score_label);
	gtk_misc_set_alignment (GTK_MISC (kill_score_label), 0, 0.5);
	gtk_box_pack_start (GTK_BOX (hbox1), kill_score_label,
			    FALSE, FALSE, 0);

	kill_score_entry = gtk_entry_new ();
	gtk_widget_show (kill_score_entry);
	gtk_widget_set_usize (kill_score_entry, 50, -1);
	gtk_box_pack_start (GTK_BOX (hbox1), kill_score_entry,
			    FALSE, FALSE, 0);

	score_label = gtk_label_new (_("Important score"));
	gtk_widget_show (score_label);
	gtk_misc_set_alignment (GTK_MISC (score_label), 0, 0.5);
	gtk_box_pack_start (GTK_BOX (hbox1), score_label, FALSE, FALSE, 0);

	important_score_entry = gtk_entry_new ();
	gtk_widget_show (important_score_entry);
	gtk_widget_set_usize (important_score_entry, 50, -1);
	gtk_box_pack_start (GTK_BOX (hbox1), important_score_entry,
			    FALSE, FALSE, 0);

	gtk_widget_show_all(window);

	scoring.window    = window;
	scoring.ok_btn = ok_btn;

	scoring.cond_entry = cond_entry;
	scoring.score_entry = score_entry;
	scoring.kill_score_entry = kill_score_entry;
	scoring.kill_score_label = kill_score_label;
	scoring.important_score_entry = important_score_entry;

	scoring.cond_clist   = cond_clist;
}

static void prefs_scoring_set_dialog(ScoringProp * cond)
{
	GtkCList *clist = GTK_CLIST(scoring.cond_clist);
	GSList *cur;
	GSList * prefs_scoring;
	gchar * score_str;

	if (cond == NULL)
		prefs_scoring_reset_dialog();
	else
		prefs_scoring_select_set_dialog(cond);

	gtk_clist_freeze(clist);
	gtk_clist_clear(clist);

	prefs_scoring_clist_set_row(-1, NULL);
	if (cur_item == NULL) {
		prefs_scoring = global_scoring;
		cur_kill_score = prefs_common.kill_score;
		cur_important_score = prefs_common.important_score;
		gtk_widget_show(scoring.kill_score_label);
		gtk_widget_show(scoring.kill_score_entry);
	}
	else {
		prefs_scoring = cur_item->prefs->scoring;
		cur_kill_score = cur_item->prefs->kill_score;
		cur_important_score = cur_item->prefs->important_score;
		if (cur_item->folder->type != F_NEWS) {
			gtk_widget_hide(scoring.kill_score_label);
			gtk_widget_hide(scoring.kill_score_entry);
		}
		else {
			gtk_widget_show(scoring.kill_score_label);
			gtk_widget_show(scoring.kill_score_entry);
		}
	}

	for(cur = prefs_scoring ; cur != NULL ;
	    cur = g_slist_next(cur)) {
		ScoringProp * prop = (ScoringProp *) cur->data;
		
		prefs_scoring_clist_set_row(-1, prop);
	}

	score_str = itos(cur_kill_score);
	gtk_entry_set_text(GTK_ENTRY(scoring.kill_score_entry),
			   score_str);
	score_str = itos(cur_important_score);
	gtk_entry_set_text(GTK_ENTRY(scoring.important_score_entry),
			   score_str);

	gtk_clist_thaw(clist);
}

static void prefs_scoring_reset_dialog(void)
{
	gtk_entry_set_text(GTK_ENTRY(scoring.cond_entry), "");
	gtk_entry_set_text(GTK_ENTRY(scoring.score_entry), "");
}

static void prefs_scoring_set_list(void)
{
	gint row = 1;
	ScoringProp *prop;
	GSList * cur;
	gchar * scoring_str;
	gchar * tmp;
	GSList * prefs_scoring;

	if (cur_item == NULL)
		prefs_scoring = global_scoring;
	else
		prefs_scoring = cur_item->prefs->scoring;

	for(cur = prefs_scoring ; cur != NULL ;
	    cur = g_slist_next(cur))
		scoringprop_free((ScoringProp *) cur->data);
	g_slist_free(prefs_scoring);
	prefs_scoring = NULL;

	while (gtk_clist_get_text(GTK_CLIST(scoring.cond_clist),
				  row, 0, &scoring_str)) {
		if (strcmp(scoring_str, _("(New)")) != 0) {
			prop = matcher_parser_get_scoring(scoring_str);
			if (prop != NULL)
				prefs_scoring = g_slist_append(prefs_scoring,
							       prop);
		}
		row++;
	}

	cur_kill_score = atoi(gtk_entry_get_text(GTK_ENTRY(scoring.kill_score_entry)));
	cur_important_score = atoi(gtk_entry_get_text(GTK_ENTRY(scoring.important_score_entry)));

	if (cur_item == NULL) {
		global_scoring = prefs_scoring;
		prefs_common.kill_score = cur_kill_score;
		prefs_common.important_score = cur_important_score;
	}
	else {
		cur_item->prefs->scoring = prefs_scoring;
		cur_item->prefs->kill_score = cur_kill_score;
		cur_item->prefs->important_score = cur_important_score;
	}
}

static gint prefs_scoring_clist_set_row(gint row, ScoringProp * prop)
{
	GtkCList *clist = GTK_CLIST(scoring.cond_clist);
	gchar * str;
	gchar *cond_str[1];

	if (prop == NULL) {
		cond_str[0] = _("(New)");
		return gtk_clist_append(clist, cond_str);
	}

	str = scoringprop_to_string(prop);
	if (str == NULL) {
		return -1;
	}
	cond_str[0] = str;

	if (row < 0)
		row = gtk_clist_append(clist, cond_str);
	else
		gtk_clist_set_text(clist, row, 0, cond_str[0]);
	g_free(str);

	return row;
}

static void prefs_scoring_condition_define_done(MatcherList * matchers)
{
	gchar * str;

	if (matchers == NULL)
		return;

	str = matcherlist_to_string(matchers);

	if (str != NULL) {
		gtk_entry_set_text(GTK_ENTRY(scoring.cond_entry), str);
		g_free(str);
	}
}

static void prefs_scoring_condition_define(void)
{
	gchar * cond_str;
	MatcherList * matchers = NULL;

	cond_str = gtk_entry_get_text(GTK_ENTRY(scoring.cond_entry));

	if (*cond_str != '\0') {
		gchar * tmp;
		
		matchers = matcher_parser_get_cond(cond_str);
		if (matchers == NULL)
			alertpanel_error(_("Match string is not valid."));
	}

	prefs_matcher_open(matchers, prefs_scoring_condition_define_done);

	if (matchers != NULL)
		matcherlist_free(matchers);
}


/* register / substitute delete buttons */

static void prefs_scoring_register_cb(void)
{
	MatcherList * cond;
	gchar * cond_str;
	gchar * score_str;
	ScoringProp * prop;
	gint score;
	gchar * tmp;

	cond_str = gtk_entry_get_text(GTK_ENTRY(scoring.cond_entry));
	if (*cond_str == '\0') {
		alertpanel_error(_("Score is not set."));
		return;
	}

	score_str = gtk_entry_get_text(GTK_ENTRY(scoring.score_entry));
	if (*score_str == '\0') {
		alertpanel_error(_("Match string is not set."));
		return;
	}

	score = atoi(score_str);
	cond = matcher_parser_get_cond(cond_str);

	if (cond == NULL) {
		alertpanel_error(_("Match string is not valid."));
		return;
	}

	prop = scoringprop_new(cond, score);

	prefs_scoring_clist_set_row(-1, prop);

	scoringprop_free(prop);

	prefs_scoring_reset_dialog();
}

static void prefs_scoring_substitute_cb(void)
{
	GtkCList *clist = GTK_CLIST(scoring.cond_clist);
	gint row;
	MatcherList * cond;
	gchar * cond_str;
	gchar * score_str;
	ScoringProp * prop;
	gint score;
	gchar * tmp;

	if (!clist->selection) return;

	row = GPOINTER_TO_INT(clist->selection->data);
	if (row == 0) return;

	cond_str = gtk_entry_get_text(GTK_ENTRY(scoring.cond_entry));
	if (*cond_str == '\0') {
		alertpanel_error(_("Score is not set."));
		return;
	}

	score_str = gtk_entry_get_text(GTK_ENTRY(scoring.score_entry));
	if (*score_str == '\0') {
		alertpanel_error(_("Match string is not set."));
		return;
	}

	score = atoi(score_str);
	cond = matcher_parser_get_cond(cond_str);

	if (cond == NULL) {
		alertpanel_error(_("Match string is not valid."));
		return;
	}

	prop = scoringprop_new(cond, score);

	prefs_scoring_clist_set_row(row, prop);

	scoringprop_free(prop);

	prefs_scoring_reset_dialog();
}

static void prefs_scoring_delete_cb(void)
{
	GtkCList *clist = GTK_CLIST(scoring.cond_clist);
	gint row;

	if (!clist->selection) return;
	row = GPOINTER_TO_INT(clist->selection->data);
	if (row == 0) return;

	if (alertpanel(_("Delete rule"),
		       _("Do you really want to delete this rule?"),
		       _("Yes"), _("No"), NULL) == G_ALERTALTERNATE)
		return;

	gtk_clist_remove(clist, row);
}

static void prefs_scoring_up(void)
{
	GtkCList *clist = GTK_CLIST(scoring.cond_clist);
	gint row;

	if (!clist->selection) return;

	row = GPOINTER_TO_INT(clist->selection->data);
	if (row > 1) {
		gtk_clist_row_move(clist, row, row - 1);
		if(gtk_clist_row_is_visible(clist, row - 1) != GTK_VISIBILITY_FULL) {
			gtk_clist_moveto(clist, row - 1, 0, 0, 0);
		} 
	}
}

static void prefs_scoring_down(void)
{
	GtkCList *clist = GTK_CLIST(scoring.cond_clist);
	gint row;

	if (!clist->selection) return;

	row = GPOINTER_TO_INT(clist->selection->data);
	if (row > 0 && row < clist->rows - 1) {
		gtk_clist_row_move(clist, row, row + 1);
		if(gtk_clist_row_is_visible(clist, row + 1) != GTK_VISIBILITY_FULL) {
			gtk_clist_moveto(clist, row + 1, 0, 1, 0);
		} 
	}
}

static void prefs_scoring_select_set_dialog(ScoringProp * prop)
{
	gchar * matcher_str;
	gchar * score_str;

	if (prop == NULL)
		return;

	matcher_str = matcherlist_to_string(prop->matchers);
	if (matcher_str == NULL) {
		scoringprop_free(prop);
		return;
	}

	score_str = itos(prop->score);

	gtk_entry_set_text(GTK_ENTRY(scoring.cond_entry), matcher_str);
	gtk_entry_set_text(GTK_ENTRY(scoring.score_entry), score_str);

	g_free(matcher_str);
}

static void prefs_scoring_select(GtkCList *clist, gint row, gint column,
				GdkEvent *event)
{
	ScoringProp * prop;
	gchar * tmp;

	gchar * scoring_str;

	if (row == 0) {
		prefs_scoring_reset_dialog();
		return;
	}

        if (!gtk_clist_get_text(GTK_CLIST(scoring.cond_clist),
				row, 0, &scoring_str))
		return;

	prop = matcher_parser_get_scoring(scoring_str);
	if (prop == NULL)
		return;

	prefs_scoring_select_set_dialog(prop);

	scoringprop_free(prop);
}

static gint prefs_scoring_deleted(GtkWidget *widget, GdkEventAny *event,
				 gpointer data)
{
	prefs_scoring_cancel();
	return TRUE;
}

static void prefs_scoring_key_pressed(GtkWidget *widget, GdkEventKey *event,
				     gpointer data)
{
	if (event && event->keyval == GDK_Escape)
		prefs_scoring_cancel();
}

static void prefs_scoring_ok(void)
{
	prefs_common_save_config();
	prefs_scoring_set_list();
	prefs_matcher_write_config();
	if (cur_item != NULL)
		prefs_folder_item_save_config(cur_item);
	gtk_widget_hide(scoring.window);
	inc_unlock();
}

static void prefs_scoring_cancel(void)
{
	prefs_matcher_read_config();
	gtk_widget_hide(scoring.window);
	inc_unlock();
}
