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
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "intl.h"
#include "main.h"
#include "prefs.h"
#include "prefs_filter.h"
#include "prefs_common.h"
#include "mainwindow.h"
#include "foldersel.h"
#include "manage_window.h"
#include "inc.h"
#include "filter.h"
#include "utils.h"
#include "gtkutils.h"
#include "alertpanel.h"
#include "folder.h"

static struct Filter {
	GtkWidget *window;

	GtkWidget *ok_btn;

	GtkWidget *hdr_combo1;
	GtkWidget *hdr_combo2;
	GtkWidget *hdr_entry1;
	GtkWidget *hdr_entry2;
	GtkWidget *key_entry1;
	GtkWidget *key_entry2;
	GtkWidget *pred_combo1;
	GtkWidget *pred_combo2;
	GtkWidget *pred_entry1;
	GtkWidget *pred_entry2;
	GtkWidget *op_combo;
	GtkWidget *op_entry;

	GtkWidget *dest_entry;
	GtkWidget *regex_chkbtn;

	GtkWidget *destsel_btn;
	GtkWidget *dest_radiobtn;
	GtkWidget *notrecv_radiobtn;

	GtkWidget *cond_clist;
} filter;

/*
   parameter name, default value, pointer to the prefs variable, data type,
   pointer to the widget pointer,
   pointer to the function for data setting,
   pointer to the function for widget setting
 */

/* widget creating functions */
static void prefs_filter_create		(void);

static void prefs_filter_set_dialog	(void);
static void prefs_filter_set_list	(void);
static gint prefs_filter_clist_set_row	(gint	 row);

/* callback functions */
static void prefs_filter_select_dest_cb	(void);
static void prefs_filter_register_cb	(void);
static void prefs_filter_substitute_cb	(void);
static void prefs_filter_delete_cb	(void);
static void prefs_filter_up		(void);
static void prefs_filter_down		(void);
static void prefs_filter_select		(GtkCList	*clist,
					 gint		 row,
					 gint		 column,
					 GdkEvent	*event);
static void prefs_filter_row_move	(GtkCList	*clist,
					 gint		 source_row,
					 gint		 dest_row);

static void prefs_filter_dest_radio_button_toggled	(void);
static void prefs_filter_notrecv_radio_button_toggled	(void);
static void prefs_filter_regex_check_button_toggled	(void);

static gint prefs_filter_deleted	(GtkWidget	*widget,
					 GdkEventAny	*event,
					 gpointer	 data);
static void prefs_filter_key_pressed	(GtkWidget	*widget,
					 GdkEventKey	*event,
					 gpointer	 data);
static void prefs_filter_cancel		(void);
static void prefs_filter_ok		(void);

void prefs_filter_open(void)
{
	if (prefs_rc_is_readonly(FILTER_RC))
		return;

	inc_lock();

	if (!filter.window) {
		prefs_filter_create();
	}

	manage_window_set_transient(GTK_WINDOW(filter.window));
	gtk_widget_grab_focus(filter.ok_btn);

	prefs_filter_set_dialog();

	gtk_widget_show(filter.window);
}

static void prefs_filter_create(void)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;
	GtkWidget *confirm_area;

	GtkWidget *vbox1;
	GtkWidget *table1;
	GtkWidget *op_label;
	GtkWidget *op_combo;
	GtkWidget *op_entry;
	GtkWidget *hdr_label;
	GtkWidget *hdr_combo1;
	GtkWidget *hdr_combo2;
	GtkWidget *key_label;
	GtkWidget *key_entry1;
	GtkWidget *key_entry2;
	GtkWidget *pred_label;
	GtkWidget *pred_combo1;
	GtkWidget *pred_entry1;
	GtkWidget *pred_combo2;
	GtkWidget *pred_entry2;

	GtkWidget *vbox2;
	GtkWidget *dest_hbox;
	GtkWidget *dest_entry;
	GtkWidget *destsel_btn;
	GtkWidget *dest_radiobtn;
	GtkWidget *notrecv_radiobtn;
	GSList *recv_group = NULL;

	GtkWidget *regex_chkbtn;

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

	gchar *title[] = {_("Registered rules")};

	debug_print(_("Creating filter setting window...\n"));

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

	/*
	gtkut_button_set_create (&confirm_area, &close_btn, _("Close"),
				 NULL, NULL, NULL, NULL);
	gtk_widget_show (confirm_area);
	gtk_box_pack_end (GTK_BOX(vbox), confirm_area, FALSE, FALSE, 0);
	gtk_widget_grab_default (close_btn);
	*/

	gtk_window_set_title (GTK_WINDOW(window),
			      _("Filter setting"));
	gtk_signal_connect (GTK_OBJECT(window), "delete_event",
			    GTK_SIGNAL_FUNC(prefs_filter_deleted), NULL);
	gtk_signal_connect (GTK_OBJECT(window), "key_press_event",
			    GTK_SIGNAL_FUNC(prefs_filter_key_pressed), NULL);
	gtk_signal_connect (GTK_OBJECT(window), "focus_in_event",
			    GTK_SIGNAL_FUNC(manage_window_focus_in), NULL);
	gtk_signal_connect (GTK_OBJECT(window), "focus_out_event",
			    GTK_SIGNAL_FUNC(manage_window_focus_out), NULL);
	gtk_signal_connect (GTK_OBJECT(ok_btn), "clicked",
			    GTK_SIGNAL_FUNC(prefs_filter_ok), NULL);
	gtk_signal_connect (GTK_OBJECT(cancel_btn), "clicked",
			    GTK_SIGNAL_FUNC(prefs_filter_cancel), NULL);

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_box_pack_start (GTK_BOX (vbox), vbox1, TRUE, TRUE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), 2);

	table1 = gtk_table_new (3, 4, FALSE);
	gtk_widget_show (table1);
	gtk_box_pack_start (GTK_BOX (vbox1), table1, FALSE, FALSE, 0);
	gtk_table_set_row_spacings (GTK_TABLE (table1), 8);
	gtk_table_set_col_spacings (GTK_TABLE (table1), 8);

	op_label = gtk_label_new (_("Operator"));
	gtk_widget_show (op_label);
	gtk_table_attach (GTK_TABLE (table1), op_label, 0, 1, 0, 1,
			  GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment (GTK_MISC (op_label), 0, 0.5);

	op_combo = gtk_combo_new ();
	gtk_widget_show (op_combo);
	gtk_table_attach (GTK_TABLE (table1), op_combo, 0, 1, 2, 3,
			  0, 0, 0, 0);
	gtk_widget_set_usize (op_combo, 52, -1);
	gtkut_combo_set_items (GTK_COMBO (op_combo), "and", "or", NULL);

	op_entry = GTK_COMBO (op_combo)->entry;
	gtk_entry_set_editable (GTK_ENTRY (op_entry), FALSE);

	hdr_label = gtk_label_new (_("Header"));
	gtk_widget_show (hdr_label);
	gtk_table_attach (GTK_TABLE (table1), hdr_label, 1, 2, 0, 1,
			  GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment (GTK_MISC (hdr_label), 0, 0.5);

	hdr_combo1 = gtk_combo_new ();
	gtk_widget_show (hdr_combo1);
	gtk_table_attach (GTK_TABLE (table1), hdr_combo1, 1, 2, 1, 2,
			  0, 0, 0, 0);
	gtk_widget_set_usize (hdr_combo1, 96, -1);
	gtkut_combo_set_items (GTK_COMBO (hdr_combo1),
			       "Subject", "From", "To", "Cc", "Reply-To",
			       "Sender", "List-Id",
			       "X-ML-Name", "X-List", "X-Sequence", "X-Mailer",
			       NULL);

	hdr_combo2 = gtk_combo_new ();
	gtk_widget_show (hdr_combo2);
	gtk_table_attach (GTK_TABLE (table1), hdr_combo2, 1, 2, 2, 3,
			  0, 0, 0, 0);
	gtk_widget_set_usize (hdr_combo2, 96, -1);
	gtkut_combo_set_items (GTK_COMBO (hdr_combo2), _("(none)"),
			       "Subject", "From", "To", "Cc", "Reply-To",
			       "Sender", "List-Id",
			       "X-ML-Name", "X-List", "X-Sequence", "X-Mailer",
			       NULL);

	key_label = gtk_label_new (_("Keyword"));
	gtk_widget_show (key_label);
	gtk_table_attach (GTK_TABLE (table1), key_label, 2, 3, 0, 1,
			  GTK_EXPAND | GTK_SHRINK | GTK_FILL,
			  0, 0, 0);
	gtk_misc_set_alignment (GTK_MISC (key_label), 0, 0.5);

	key_entry1 = gtk_entry_new ();
	gtk_widget_show (key_entry1);
	gtk_widget_set_usize (key_entry1, 128, -1);
	gtk_table_attach (GTK_TABLE (table1), key_entry1, 2, 3, 1, 2,
			  GTK_EXPAND | GTK_SHRINK | GTK_FILL,
			  0, 0, 0);

	key_entry2 = gtk_entry_new ();
	gtk_widget_show (key_entry2);
	gtk_widget_set_usize (key_entry2, 128, -1);
	gtk_table_attach (GTK_TABLE (table1), key_entry2, 2, 3, 2, 3,
			  GTK_EXPAND | GTK_SHRINK | GTK_FILL,
			  0, 0, 0);

	pred_label = gtk_label_new (_("Predicate"));
	gtk_widget_show (pred_label);
	gtk_table_attach (GTK_TABLE (table1), pred_label, 3, 4, 0, 1,
			  GTK_FILL, 0, 0, 0);
	gtk_misc_set_alignment (GTK_MISC (pred_label), 0, 0.5);

	pred_combo1 = gtk_combo_new ();
	gtk_widget_show (pred_combo1);
	gtk_table_attach (GTK_TABLE (table1), pred_combo1, 3, 4, 1, 2,
			  0, 0, 0, 0);
	gtk_widget_set_usize (pred_combo1, 92, -1);
	gtkut_combo_set_items (GTK_COMBO (pred_combo1),
			       _("contains"), _("not contain"), NULL);

	pred_entry1 = GTK_COMBO (pred_combo1)->entry;
	gtk_entry_set_editable (GTK_ENTRY (pred_entry1), FALSE);

	pred_combo2 = gtk_combo_new ();
	gtk_widget_show (pred_combo2);
	gtk_table_attach (GTK_TABLE (table1), pred_combo2, 3, 4, 2, 3,
			  0, 0, 0, 0);
	gtk_widget_set_usize (pred_combo2, 92, -1);
	gtkut_combo_set_items (GTK_COMBO (pred_combo2),
			       _("contains"), _("not contain"), NULL);

	pred_entry2 = GTK_COMBO (pred_combo2)->entry;
	gtk_entry_set_editable (GTK_ENTRY (pred_entry2), FALSE);

	/* destination */

	vbox2 = gtk_vbox_new (FALSE, VSPACING_NARROW);
	gtk_widget_show (vbox2);
	gtk_box_pack_start (GTK_BOX (vbox1), vbox2, FALSE, TRUE, 0);

	dest_hbox = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (dest_hbox);
	gtk_box_pack_start (GTK_BOX (vbox2), dest_hbox, FALSE, TRUE, 0);

	dest_radiobtn =
		gtk_radio_button_new_with_label (recv_group, _("Destination"));
	recv_group = gtk_radio_button_group (GTK_RADIO_BUTTON (dest_radiobtn));
	gtk_widget_show (dest_radiobtn);
	gtk_box_pack_start (GTK_BOX (dest_hbox), dest_radiobtn,
			    FALSE, FALSE, 0);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dest_radiobtn), TRUE);
	gtk_signal_connect
		(GTK_OBJECT (dest_radiobtn), "toggled",
		 GTK_SIGNAL_FUNC (prefs_filter_dest_radio_button_toggled),
		 NULL);

	dest_entry = gtk_entry_new ();
	gtk_widget_show (dest_entry);
	gtk_widget_set_usize (dest_entry, DEFAULT_ENTRY_WIDTH, -1);
	gtk_box_pack_start (GTK_BOX (dest_hbox), dest_entry, TRUE, TRUE, 0);
	gtk_entry_set_editable (GTK_ENTRY (dest_entry), FALSE);

	destsel_btn = gtk_button_new_with_label (_(" Select... "));
	gtk_widget_show (destsel_btn);
	gtk_box_pack_start (GTK_BOX (dest_hbox), destsel_btn, FALSE, FALSE, 0);
	gtk_signal_connect (GTK_OBJECT (destsel_btn), "clicked",
			    GTK_SIGNAL_FUNC (prefs_filter_select_dest_cb),
			    NULL);

	PACK_CHECK_BUTTON (dest_hbox, regex_chkbtn, _("Use regex"));
	gtk_signal_connect
		(GTK_OBJECT (regex_chkbtn), "toggled",
		 GTK_SIGNAL_FUNC (prefs_filter_regex_check_button_toggled),
		 NULL);

	notrecv_radiobtn = gtk_radio_button_new_with_label
		(recv_group, _("Don't receive"));
	recv_group = gtk_radio_button_group
		(GTK_RADIO_BUTTON (notrecv_radiobtn));
	gtk_widget_show (notrecv_radiobtn);
	gtk_box_pack_start (GTK_BOX (vbox2), notrecv_radiobtn, FALSE, FALSE, 0);
	gtk_signal_connect
		(GTK_OBJECT (notrecv_radiobtn), "toggled",
		 GTK_SIGNAL_FUNC (prefs_filter_notrecv_radio_button_toggled),
		 NULL);

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
			    GTK_SIGNAL_FUNC (prefs_filter_register_cb), NULL);

	subst_btn = gtk_button_new_with_label (_(" Substitute "));
	gtk_widget_show (subst_btn);
	gtk_box_pack_start (GTK_BOX (btn_hbox), subst_btn, FALSE, TRUE, 0);
	gtk_signal_connect (GTK_OBJECT (subst_btn), "clicked",
			    GTK_SIGNAL_FUNC (prefs_filter_substitute_cb),
			    NULL);

	del_btn = gtk_button_new_with_label (_("Delete"));
	gtk_widget_show (del_btn);
	gtk_box_pack_start (GTK_BOX (btn_hbox), del_btn, FALSE, TRUE, 0);
	gtk_signal_connect (GTK_OBJECT (del_btn), "clicked",
			    GTK_SIGNAL_FUNC (prefs_filter_delete_cb), NULL);

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
			    GTK_SIGNAL_FUNC (prefs_filter_select), NULL);
	gtk_signal_connect (GTK_OBJECT (cond_clist), "row_move",
			    GTK_SIGNAL_FUNC (prefs_filter_row_move), NULL);

	btn_vbox = gtk_vbox_new (FALSE, 8);
	gtk_widget_show (btn_vbox);
	gtk_box_pack_start (GTK_BOX (cond_hbox), btn_vbox, FALSE, FALSE, 0);

	up_btn = gtk_button_new_with_label (_("Up"));
	gtk_widget_show (up_btn);
	gtk_box_pack_start (GTK_BOX (btn_vbox), up_btn, FALSE, FALSE, 0);
	gtk_signal_connect (GTK_OBJECT (up_btn), "clicked",
			    GTK_SIGNAL_FUNC (prefs_filter_up), NULL);

	down_btn = gtk_button_new_with_label (_("Down"));
	gtk_widget_show (down_btn);
	gtk_box_pack_start (GTK_BOX (btn_vbox), down_btn, FALSE, FALSE, 0);
	gtk_signal_connect (GTK_OBJECT (down_btn), "clicked",
			    GTK_SIGNAL_FUNC (prefs_filter_down), NULL);

	gtk_widget_show_all(window);

	filter.window    = window;
	filter.ok_btn = ok_btn;

	filter.hdr_combo1  = hdr_combo1;
	filter.hdr_combo2  = hdr_combo2;
	filter.hdr_entry1  = GTK_COMBO (hdr_combo1)->entry;
	filter.hdr_entry2  = GTK_COMBO (hdr_combo2)->entry;
	filter.key_entry1  = key_entry1;
	filter.key_entry2  = key_entry2;
	filter.pred_combo1 = pred_combo1;
	filter.pred_combo2 = pred_combo2;
	filter.pred_entry1 = pred_entry1;
	filter.pred_entry2 = pred_entry2;
	filter.op_combo    = op_combo;
	filter.op_entry    = op_entry;

	filter.dest_entry	= dest_entry;
	filter.destsel_btn	= destsel_btn;
	filter.dest_radiobtn	= dest_radiobtn;
	filter.notrecv_radiobtn	= notrecv_radiobtn;
	filter.regex_chkbtn     = regex_chkbtn;

	filter.cond_clist   = cond_clist;
}

void prefs_filter_read_config(void)
{
	gchar *rcpath;
	FILE *fp;
	gchar buf[PREFSBUFSIZE];
	Filter *flt;

	debug_print(_("Reading filter configuration...\n"));

	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, FILTER_RC, NULL);
	if ((fp = fopen(rcpath, "r")) == NULL) {
		if (ENOENT != errno) FILE_OP_ERROR(rcpath, "fopen");
		g_free(rcpath);
		return;
	}
	g_free(rcpath);

	/* remove all previous filter list */
	while (prefs_common.fltlist != NULL) {
		flt = (Filter *)prefs_common.fltlist->data;
		filter_free(flt);
		prefs_common.fltlist = g_slist_remove(prefs_common.fltlist,
						      flt);
	}

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		g_strchomp(buf);
		flt = filter_read_str(buf);
		if (flt) {
			prefs_common.fltlist =
				g_slist_append(prefs_common.fltlist, flt);
		}
	}

	fclose(fp);
}

void prefs_filter_write_config(void)
{
	gchar *rcpath;
	PrefFile *pfile;
	GSList *cur;

	debug_print(_("Writing filter configuration...\n"));

	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, FILTER_RC, NULL);
	if ((pfile = prefs_write_open(rcpath)) == NULL) {
		g_warning(_("failed to write configuration to file\n"));
		g_free(rcpath);
		return;
	}

	for (cur = prefs_common.fltlist; cur != NULL; cur = cur->next) {
		Filter *flt = (Filter *)cur->data;
		gchar *fstr;

		fstr = filter_get_str(flt);
		if (fputs(fstr, pfile->fp) == EOF ||
		    fputc('\n', pfile->fp) == EOF) {
			FILE_OP_ERROR(rcpath, "fputs || fputc");
			prefs_write_close_revert(pfile);
			g_free(rcpath);
			g_free(fstr);
			return;
		}
		g_free(fstr);
	}

	g_free(rcpath);

	if (prefs_write_close(pfile) < 0) {
		g_warning(_("failed to write configuration to file\n"));
		return;
	}
}

static void prefs_filter_set_dialog(void)
{
	GtkCList *clist = GTK_CLIST(filter.cond_clist);
	GSList *cur;
	gchar *cond_str[1];
	gint row;

	gtk_clist_freeze(clist);
	gtk_clist_clear(clist);

	cond_str[0] = _("(New)");
	row = gtk_clist_append(clist, cond_str);
	gtk_clist_set_row_data(clist, row, NULL);

	for (cur = prefs_common.fltlist; cur != NULL; cur = cur->next) {
		Filter *flt = (Filter *)cur->data;

		cond_str[0] = filter_get_str(flt);
		subst_char(cond_str[0], '\t', ':');
		row = gtk_clist_append(clist, cond_str);
		gtk_clist_set_row_data(clist, row, flt);

		g_free(cond_str[0]);
	}

	gtk_clist_thaw(clist);
}

static void prefs_filter_set_list(void)
{
	gint row = 1;
	Filter *flt;

	g_slist_free(prefs_common.fltlist);
	prefs_common.fltlist = NULL;

	while ((flt = gtk_clist_get_row_data(GTK_CLIST(filter.cond_clist),
		row)) != NULL) {
		prefs_common.fltlist = g_slist_append(prefs_common.fltlist,
						      flt);
		row++;
	}
}

#define GET_ENTRY(entry) \
	entry_text = gtk_entry_get_text(GTK_ENTRY(entry))

static gint prefs_filter_clist_set_row(gint row)
{
	GtkCList *clist = GTK_CLIST(filter.cond_clist);
	Filter *flt;
	gchar *entry_text;
	gchar *cond_str[1];

	g_return_val_if_fail(row != 0, -1);

	if (GTK_WIDGET_IS_SENSITIVE(filter.dest_entry))
		GET_ENTRY(filter.dest_entry);
	else
		entry_text = FILTER_NOT_RECEIVE;
	if (entry_text[0] == '\0') {
		alertpanel_error(_("Destination is not set."));
		return -1;
	}
	GET_ENTRY(filter.hdr_entry1);
	if (entry_text[0] == '\0') {
		alertpanel_error(_("Header name is not set."));
		return -1;
	}

	flt = g_new0(Filter, 1);

	flt->name1 = g_strdup(entry_text);

	GET_ENTRY(filter.key_entry1);
	if (entry_text[0] != '\0')
		flt->body1 = g_strdup(entry_text);

	GET_ENTRY(filter.hdr_entry2);
	if (entry_text[0] != '\0' && strcmp(entry_text, _("(none)")) != 0) {
		flt->name2 = g_strdup(entry_text);

		GET_ENTRY(filter.key_entry2);
		if (entry_text[0] != '\0')
			flt->body2 = g_strdup(entry_text);
	}

	if (gtk_toggle_button_get_active
		(GTK_TOGGLE_BUTTON(filter.regex_chkbtn)))
		flt->flag1 = flt->flag2 = FLT_REGEX;

	GET_ENTRY(filter.pred_entry1);
	if (!strcmp(entry_text, _("contains")))
		flt->flag1 |= FLT_CONTAIN;
	GET_ENTRY(filter.pred_entry2);
	if (!strcmp(entry_text, _("contains")))
		flt->flag2 |= FLT_CONTAIN;

	GET_ENTRY(filter.op_entry);
	if (!strcmp(entry_text, "and"))
		flt->cond = FLT_AND;
	else
		flt->cond = FLT_OR;

	if (GTK_WIDGET_IS_SENSITIVE(filter.dest_entry)) {
		entry_text = gtk_entry_get_text(GTK_ENTRY(filter.dest_entry));
		flt->dest = g_strdup(entry_text);
		flt->action = FLT_MOVE;
	} else
		flt->action = FLT_NOTRECV;

	cond_str[0] = filter_get_str(flt);
	subst_char(cond_str[0], '\t', ':');

	if (row < 0)
		row = gtk_clist_append(clist, cond_str);
	else {
		Filter *tmpflt;

		gtk_clist_set_text(clist, row, 0, cond_str[0]);
		tmpflt = gtk_clist_get_row_data(clist, row);
		if (tmpflt)
			filter_free(tmpflt);
	}

	gtk_clist_set_row_data(clist, row, flt);

	g_free(cond_str[0]);

	prefs_filter_set_list();

	return row;
}

static void prefs_filter_select_dest_cb(void)
{
	FolderItem *dest;

	dest = foldersel_folder_sel(NULL, NULL);
	if (!dest) return;

	gtk_entry_set_text(GTK_ENTRY(filter.dest_entry), dest->path);
}

static void prefs_filter_register_cb(void)
{
	prefs_filter_clist_set_row(-1);
}

static void prefs_filter_substitute_cb(void)
{
	GtkCList *clist = GTK_CLIST(filter.cond_clist);
	Filter *flt;
	gint row;

	if (!clist->selection) return;

	row = GPOINTER_TO_INT(clist->selection->data);
	if (row == 0) return;

	flt = gtk_clist_get_row_data(clist, row);
	if (!flt) return;

	prefs_filter_clist_set_row(row);
}

static void prefs_filter_delete_cb(void)
{
	GtkCList *clist = GTK_CLIST(filter.cond_clist);
	Filter *flt;
	gint row;

	if (!clist->selection) return;
	row = GPOINTER_TO_INT(clist->selection->data);
	if (row == 0) return;

	if (alertpanel(_("Delete rule"),
		       _("Do you really want to delete this rule?"),
		       _("Yes"), _("No"), NULL) == G_ALERTALTERNATE)
		return;

	flt = gtk_clist_get_row_data(clist, row);
	filter_free(flt);
	gtk_clist_remove(clist, row);
	prefs_common.fltlist = g_slist_remove(prefs_common.fltlist, flt);
}

static void prefs_filter_up(void)
{
	GtkCList *clist = GTK_CLIST(filter.cond_clist);
	gint row;

	if (!clist->selection) return;

	row = GPOINTER_TO_INT(clist->selection->data);
	if (row > 1)
		gtk_clist_row_move(clist, row, row - 1);
}

static void prefs_filter_down(void)
{
	GtkCList *clist = GTK_CLIST(filter.cond_clist);
	gint row;

	if (!clist->selection) return;

	row = GPOINTER_TO_INT(clist->selection->data);
	if (row > 0 && row < clist->rows - 1)
		gtk_clist_row_move(clist, row, row + 1);
}

#define ENTRY_SET_TEXT(entry, str) \
	gtk_entry_set_text(GTK_ENTRY(entry), str ? str : "")

static void prefs_filter_select(GtkCList *clist, gint row, gint column,
				GdkEvent *event)
{
	Filter *flt;
	Filter default_flt = {"Subject", NULL, _("(none)"), NULL,
			      FLT_CONTAIN, FLT_CONTAIN, FLT_AND,
			      NULL, FLT_MOVE};
	gboolean is_regex;

	flt = gtk_clist_get_row_data(clist, row);
	if (!flt)
		flt = &default_flt;

	ENTRY_SET_TEXT(filter.dest_entry, flt->dest);
	ENTRY_SET_TEXT(filter.hdr_entry1, flt->name1);
	ENTRY_SET_TEXT(filter.key_entry1, flt->body1);
	ENTRY_SET_TEXT(filter.hdr_entry2,
		       flt->name2 ? flt->name2 : _("(none)"));
	ENTRY_SET_TEXT(filter.key_entry2, flt->body2);

	ENTRY_SET_TEXT(filter.pred_entry1,
		       FLT_IS_CONTAIN(flt->flag1)
		       ? _("contains") : _("not contain"));
	ENTRY_SET_TEXT(filter.pred_entry2,
		       FLT_IS_CONTAIN(flt->flag2)
		       ? _("contains") : _("not contain"));

	is_regex = FLT_IS_REGEX(flt->flag1);
	gtk_widget_set_sensitive(filter.pred_combo1, !is_regex);
	gtk_widget_set_sensitive(filter.pred_combo2, !is_regex);
	gtk_toggle_button_set_active
		(GTK_TOGGLE_BUTTON(filter.regex_chkbtn), is_regex);

	gtk_entry_set_text(GTK_ENTRY(filter.op_entry),
			   flt->cond == FLT_OR ? "or" : "and");
	if (flt->action == FLT_NOTRECV)
		gtk_toggle_button_set_active
			(GTK_TOGGLE_BUTTON(filter.notrecv_radiobtn), TRUE);
	else
		gtk_toggle_button_set_active
			(GTK_TOGGLE_BUTTON(filter.dest_radiobtn), TRUE);
}

static void prefs_filter_row_move(GtkCList *clist, gint source_row,
				  gint dest_row)
{
	prefs_filter_set_list();
	if (gtk_clist_row_is_visible(clist, dest_row) != GTK_VISIBILITY_FULL) {
		gtk_clist_moveto(clist, dest_row, -1,
				 source_row < dest_row ? 1.0 : 0.0, 0.0);
	}
}

static void prefs_filter_dest_radio_button_toggled(void)
{
	gtk_widget_set_sensitive(filter.dest_entry, TRUE);
	gtk_widget_set_sensitive(filter.destsel_btn, TRUE);
}

static void prefs_filter_notrecv_radio_button_toggled(void)
{
	gtk_widget_set_sensitive(filter.dest_entry, FALSE);
	gtk_widget_set_sensitive(filter.destsel_btn, FALSE);
}

static void prefs_filter_regex_check_button_toggled(void)
{
	gboolean is_regex;

	is_regex = gtk_toggle_button_get_active
		(GTK_TOGGLE_BUTTON(filter.regex_chkbtn));
	gtk_widget_set_sensitive(filter.pred_combo1, !is_regex);
	gtk_widget_set_sensitive(filter.pred_combo2, !is_regex);
}

static gint prefs_filter_deleted(GtkWidget *widget, GdkEventAny *event,
				 gpointer data)
{
	prefs_filter_cancel();
	return TRUE;
}

static void prefs_filter_key_pressed(GtkWidget *widget, GdkEventKey *event,
				     gpointer data)
{
	if (event && event->keyval == GDK_Escape)
		prefs_filter_cancel();
}

static void prefs_filter_ok(void)
{
	prefs_filter_write_config();
	gtk_widget_hide(filter.window);
	inc_unlock();
}

static void prefs_filter_cancel(void)
{
	prefs_filter_read_config();
	gtk_widget_hide(filter.window);
	inc_unlock();
}
