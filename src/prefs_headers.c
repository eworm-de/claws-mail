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
#include "prefs_headers.h"
#include "prefs_common.h"
#include "prefs_account.h"
#include "mainwindow.h"
#include "foldersel.h"
#include "manage_window.h"
#include "customheader.h"
#include "utils.h"
#include "gtkutils.h"
#include "alertpanel.h"
#include "folder.h"

static struct Headers {
	GtkWidget *window;

	/*
	GtkWidget *close_btn;
	*/
	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;

	GtkWidget *hdr_combo;
	GtkWidget *hdr_entry;
	GtkWidget *key_entry;
	GtkWidget *headers_clist;
} headers;

/*
   parameter name, default value, pointer to the prefs variable, data type,
   pointer to the widget pointer,
   pointer to the function for data setting,
   pointer to the function for widget setting
 */

#define VSPACING		12
#define VSPACING_NARROW		4
#define DEFAULT_ENTRY_WIDTH	80
#define PREFSBUFSIZE		1024

/* widget creating functions */
static void prefs_headers_create		(void);

static void prefs_headers_set_dialog	(PrefsAccount * ac);
static void prefs_headers_set_list	(PrefsAccount * ac);
static gint prefs_headers_clist_set_row	(PrefsAccount * ac,
					 gint	 row);

/* callback functions */
static void prefs_headers_select_dest_cb	(void);
static void prefs_headers_register_cb	(void);
static void prefs_headers_substitute_cb	(void);
static void prefs_headers_delete_cb	(void);
static void prefs_headers_up		(void);
static void prefs_headers_down		(void);
static void prefs_headers_select		(GtkCList	*clist,
					 gint		 row,
					 gint		 column,
					 GdkEvent	*event);

static void prefs_headers_key_pressed	(GtkWidget	*widget,
					 GdkEventKey	*event,
					 gpointer	 data);
/*
static void prefs_headers_close		(GtkButton	*button);
*/
static void prefs_headers_ok		(GtkButton	*button);
static void prefs_headers_cancel	(GtkButton	*button);
static gint prefs_headers_deleted(GtkWidget *widget, GdkEventAny *event,
				  gpointer data);

static PrefsAccount * cur_ac = NULL;

void prefs_headers_open(PrefsAccount * ac)
{
	if (!headers.window) {
		prefs_headers_create();
	}

	manage_window_set_transient(GTK_WINDOW(headers.window));
	/*
	gtk_widget_grab_focus(headers.close_btn);
	*/
	gtk_widget_grab_focus(headers.ok_btn);

	prefs_headers_set_dialog(ac);

	cur_ac = ac;

	gtk_widget_show(headers.window);
}

static void prefs_headers_create(void)
{
	GtkWidget *window;
	GtkWidget *vbox;

	/*	GtkWidget *close_btn; */
	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;

	GtkWidget *confirm_area;

	GtkWidget *vbox1;

	GtkWidget *table1;
	GtkWidget *hdr_label;
	GtkWidget *hdr_combo;
	GtkWidget *key_label;
	GtkWidget *key_entry;

	GtkWidget *reg_hbox;
	GtkWidget *btn_hbox;
	GtkWidget *arrow;
	GtkWidget *reg_btn;
	GtkWidget *subst_btn;
	GtkWidget *del_btn;

	GtkWidget *ch_hbox;
	GtkWidget *ch_scrolledwin;
	GtkWidget *headers_clist;

	GtkWidget *btn_vbox;
	GtkWidget *up_btn;
	GtkWidget *down_btn;

	gchar *title[] = {_("Custom headers")};

	debug_print(_("Creating headers setting window...\n"));

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
	/*
	gtkut_button_set_create (&confirm_area, &close_btn, _("Close"),
				 NULL, NULL, NULL, NULL);
	*/
	gtk_widget_show (confirm_area);
	gtk_box_pack_end (GTK_BOX(vbox), confirm_area, FALSE, FALSE, 0);
	/*
	gtk_widget_grab_default (close_btn);
	*/
	gtk_widget_grab_default (ok_btn);

	gtk_window_set_title (GTK_WINDOW(window),
			      _("Headers setting"));
	gtk_signal_connect (GTK_OBJECT(window), "delete_event",
			    GTK_SIGNAL_FUNC(prefs_headers_deleted), NULL);
	gtk_signal_connect (GTK_OBJECT(window), "key_press_event",
			    GTK_SIGNAL_FUNC(prefs_headers_key_pressed), NULL);
	gtk_signal_connect (GTK_OBJECT(window), "focus_in_event",
			    GTK_SIGNAL_FUNC(manage_window_focus_in), NULL);
	gtk_signal_connect (GTK_OBJECT(window), "focus_out_event",
			    GTK_SIGNAL_FUNC(manage_window_focus_out), NULL);
	gtk_signal_connect (GTK_OBJECT(ok_btn), "clicked",
			    GTK_SIGNAL_FUNC(prefs_headers_ok), NULL);
	gtk_signal_connect (GTK_OBJECT(cancel_btn), "clicked",
			    GTK_SIGNAL_FUNC(prefs_headers_cancel), NULL);

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_box_pack_start (GTK_BOX (vbox), vbox1, TRUE, TRUE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), 2);

	table1 = gtk_table_new (2, 2, FALSE);
	gtk_widget_show (table1);
	gtk_box_pack_start (GTK_BOX (vbox1), table1,
			    FALSE, FALSE, 0);
	gtk_table_set_row_spacings (GTK_TABLE (table1), 8);
	gtk_table_set_col_spacings (GTK_TABLE (table1), 8);

	hdr_label = gtk_label_new (_("Header"));
	gtk_widget_show (hdr_label);
	gtk_table_attach (GTK_TABLE (table1), hdr_label, 0, 1, 0, 1,
			  GTK_EXPAND | GTK_SHRINK | GTK_FILL,
			  0, 0, 0);
	gtk_misc_set_alignment (GTK_MISC (hdr_label), 0, 0.5);
	
	hdr_combo = gtk_combo_new ();
	gtk_widget_show (hdr_combo);
	gtk_table_attach (GTK_TABLE (table1), hdr_combo, 0, 1, 1, 2,
			  GTK_EXPAND | GTK_SHRINK | GTK_FILL,
			  0, 0, 0);
	gtk_widget_set_usize (hdr_combo, 150 /* 96 */, -1);
	gtkut_combo_set_items (GTK_COMBO (hdr_combo),
			       "User-Agent", "X-Operating-System", NULL);

	key_label = gtk_label_new (_("Value"));
	gtk_widget_show (key_label);
	gtk_table_attach (GTK_TABLE (table1), key_label, 1, 2, 0, 1,
			  GTK_EXPAND | GTK_SHRINK | GTK_FILL,
			  0, 0, 0);
	gtk_misc_set_alignment (GTK_MISC (key_label), 0, 0.5);
	
	key_entry = gtk_entry_new ();
	gtk_widget_show (key_entry);
	gtk_table_attach (GTK_TABLE (table1), key_entry, 1, 2, 1, 2,
			  GTK_EXPAND | GTK_SHRINK | GTK_FILL,
			  0, 0, 0);

	/* register / substitute / delete */

	reg_hbox = gtk_hbox_new (FALSE, 4);
	gtk_widget_show (reg_hbox);
	gtk_box_pack_start (GTK_BOX (vbox1), reg_hbox,
			    FALSE, FALSE, 0);

	arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_OUT);
	gtk_widget_show (arrow);
	gtk_box_pack_start (GTK_BOX (reg_hbox), arrow, FALSE, FALSE, 0);
	gtk_widget_set_usize (arrow, -1, 16);

	btn_hbox = gtk_hbox_new (TRUE, 4);
	gtk_widget_show (btn_hbox);
	gtk_box_pack_start (GTK_BOX (reg_hbox), btn_hbox, FALSE, FALSE, 0);

	reg_btn = gtk_button_new_with_label (_("Add"));
	gtk_widget_show (reg_btn);
	gtk_box_pack_start (GTK_BOX (btn_hbox), reg_btn, FALSE, TRUE, 0);
	gtk_signal_connect (GTK_OBJECT (reg_btn), "clicked",
			    GTK_SIGNAL_FUNC (prefs_headers_register_cb), NULL);

	subst_btn = gtk_button_new_with_label (_(" Substitute "));
	gtk_widget_show (subst_btn);
	gtk_box_pack_start (GTK_BOX (btn_hbox), subst_btn, FALSE, TRUE, 0);
	gtk_signal_connect (GTK_OBJECT (subst_btn), "clicked",
			    GTK_SIGNAL_FUNC (prefs_headers_substitute_cb),
			    NULL);

	del_btn = gtk_button_new_with_label (_("Delete"));
	gtk_widget_show (del_btn);
	gtk_box_pack_start (GTK_BOX (btn_hbox), del_btn, FALSE, TRUE, 0);
	gtk_signal_connect (GTK_OBJECT (del_btn), "clicked",
			    GTK_SIGNAL_FUNC (prefs_headers_delete_cb), NULL);


	ch_hbox = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (ch_hbox);
	gtk_box_pack_start (GTK_BOX (vbox1), ch_hbox,
			    TRUE, TRUE, 0);

	ch_scrolledwin = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_set_usize (ch_scrolledwin, -1, 100);
	gtk_widget_show (ch_scrolledwin);
	gtk_box_pack_start (GTK_BOX (ch_hbox), ch_scrolledwin,
			    TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (ch_scrolledwin),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);

	headers_clist = gtk_clist_new_with_titles(1, title);
	gtk_widget_show (headers_clist);
	gtk_container_add (GTK_CONTAINER (ch_scrolledwin), headers_clist);
	gtk_clist_set_column_width (GTK_CLIST (headers_clist), 0, 80);
	gtk_clist_set_selection_mode (GTK_CLIST (headers_clist),
				      GTK_SELECTION_BROWSE);
	GTK_WIDGET_UNSET_FLAGS (GTK_CLIST (headers_clist)->column[0].button,
				GTK_CAN_FOCUS);
	gtk_signal_connect (GTK_OBJECT (headers_clist), "select_row",
			    GTK_SIGNAL_FUNC (prefs_headers_select),
			    NULL);


	btn_vbox = gtk_vbox_new (FALSE, 8);
	gtk_widget_show (btn_vbox);
	gtk_box_pack_start (GTK_BOX (ch_hbox), btn_vbox, FALSE, FALSE, 0);

	up_btn = gtk_button_new_with_label (_("Up"));
	gtk_widget_show (up_btn);
	gtk_box_pack_start (GTK_BOX (btn_vbox), up_btn, FALSE, FALSE, 0);
	gtk_signal_connect (GTK_OBJECT (up_btn), "clicked",
			    GTK_SIGNAL_FUNC (prefs_headers_up), NULL);

	down_btn = gtk_button_new_with_label (_("Down"));
	gtk_widget_show (down_btn);
	gtk_box_pack_start (GTK_BOX (btn_vbox), down_btn, FALSE, FALSE, 0);
	gtk_signal_connect (GTK_OBJECT (down_btn), "clicked",
			    GTK_SIGNAL_FUNC (prefs_headers_down), NULL);


	gtk_widget_show_all(window);

	headers.window    = window;
	/*	headers.close_btn = close_btn; */
	headers.ok_btn = ok_btn;
	headers.cancel_btn = cancel_btn;

	headers.hdr_combo  = hdr_combo;
	headers.hdr_entry  = GTK_COMBO (hdr_combo)->entry;
	headers.key_entry  = key_entry;
	headers.headers_clist   = headers_clist;
}

void prefs_headers_read_config(PrefsAccount * ac)
{
	gchar *rcpath;
	FILE *fp;
	gchar buf[PREFSBUFSIZE];
	CustomHeader *ch;

	debug_print(_("Reading headers configuration...\n"));

	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, HEADERS_RC, NULL);
	if ((fp = fopen(rcpath, "r")) == NULL) {
		if (ENOENT != errno) FILE_OP_ERROR(rcpath, "fopen");
		g_free(rcpath);
		ac->customhdr_list = NULL;
		return;
	}
	g_free(rcpath);

 	/* remove all previous headers list */
 	while (ac->customhdr_list != NULL) {
 		ch = (CustomHeader *)ac->customhdr_list->data;
 		custom_header_free(ch);
 		ac->customhdr_list = g_slist_remove(ac->customhdr_list, ch);
 	}
 
 	while (fgets(buf, sizeof(buf), fp) != NULL) {
 		g_strchomp(buf);
 		ch = custom_header_read_str(buf);
 		if (ch) {
			if (ch->account_id == ac->account_id)
				ac->customhdr_list =
					g_slist_append(ac->customhdr_list, ch);
			else
				custom_header_free(ch);
 		}
 	}
 
 	fclose(fp);
}

void prefs_headers_write_config(PrefsAccount * ac)
{
	gchar *rcpath;
	PrefFile *pfile;
	GSList *cur;
	gchar buf[PREFSBUFSIZE];
	FILE * fp;
	CustomHeader *ch;

	GSList *all_hdrs = NULL;

	debug_print(_("Writing headers configuration...\n"));

	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, HEADERS_RC, NULL);

	if ((fp = fopen(rcpath, "r")) == NULL) {
		if (ENOENT != errno) FILE_OP_ERROR(rcpath, "fopen");
	}
	else {
		all_hdrs = NULL;

		while (fgets(buf, sizeof(buf), fp) != NULL) {
			g_strchomp(buf);
			ch = custom_header_read_str(buf);
			if (ch) {
				if (ch->account_id != ac->account_id)
					all_hdrs =
						g_slist_append(all_hdrs, ch);
				else
					custom_header_free(ch);
			}
		}

		fclose(fp);
	}

	if ((pfile = prefs_write_open(rcpath)) == NULL) {
		g_warning(_("failed to write configuration to file\n"));
		g_free(rcpath);
		return;
	}

	for (cur = all_hdrs; cur != NULL; cur = cur->next) {
 		CustomHeader *hdr = (CustomHeader *)cur->data;
		gchar *chstr;

		chstr = custom_header_get_str(hdr);
		if (fputs(chstr, pfile->fp) == EOF ||
		    fputc('\n', pfile->fp) == EOF) {
			FILE_OP_ERROR(rcpath, "fputs || fputc");
			prefs_write_close_revert(pfile);
			g_free(rcpath);
			g_free(chstr);
			return;
		}
		g_free(chstr);
	}

	for (cur = ac->customhdr_list; cur != NULL; cur = cur->next) {
 		CustomHeader *hdr = (CustomHeader *)cur->data;
		gchar *chstr;

		chstr = custom_header_get_str(hdr);
		if (fputs(chstr, pfile->fp) == EOF ||
		    fputc('\n', pfile->fp) == EOF) {
			FILE_OP_ERROR(rcpath, "fputs || fputc");
			prefs_write_close_revert(pfile);
			g_free(rcpath);
			g_free(chstr);
			return;
		}
		g_free(chstr);
	}

	g_free(rcpath);

 	while (all_hdrs != NULL) {
 		ch = (CustomHeader *)all_hdrs->data;
 		custom_header_free(ch);
 		all_hdrs = g_slist_remove(all_hdrs, ch);
 	}

	if (prefs_write_close(pfile) < 0) {
		g_warning(_("failed to write configuration to file\n"));
		return;
	}
}

static void prefs_headers_set_dialog(PrefsAccount * ac)
{
	GtkCList *clist = GTK_CLIST(headers.headers_clist);
	GSList *cur;
	gchar *ch_str[1];
	gint row;

	gtk_clist_freeze(clist);
	gtk_clist_clear(clist);

	ch_str[0] = _("(New)");
	row = gtk_clist_append(clist, ch_str);
	gtk_clist_set_row_data(clist, row, NULL);

	for (cur = ac->customhdr_list; cur != NULL; cur = cur->next) {
 		CustomHeader *ch = (CustomHeader *)cur->data;

		ch_str[0] = g_strdup_printf("%s: %s", ch->name, ch->value);
		row = gtk_clist_append(clist, ch_str);
		gtk_clist_set_row_data(clist, row, ch);

		g_free(ch_str[0]);
	}

	gtk_clist_thaw(clist);
}

static void prefs_headers_set_list(PrefsAccount * ac)
{
	gint row = 1;
	CustomHeader *ch;

	g_slist_free(ac->customhdr_list);
	ac->customhdr_list = NULL;

	while ((ch = gtk_clist_get_row_data(GTK_CLIST(headers.headers_clist),
		row)) != NULL) {
		ac->customhdr_list = g_slist_append(ac->customhdr_list,
						      ch);
		row++;
	}
}

#define GET_ENTRY(entry) \
	entry_text = gtk_entry_get_text(GTK_ENTRY(entry))

static gint prefs_headers_clist_set_row(PrefsAccount * ac, gint row)
{
	GtkCList *clist = GTK_CLIST(headers.headers_clist);
	CustomHeader *ch;
	gchar *entry_text;
	gchar *ch_str[1];

	g_return_val_if_fail(row != 0, -1);

	GET_ENTRY(headers.hdr_entry);
	if (entry_text[0] == '\0') {
		alertpanel_error(_("Header name is not set."));
		return -1;
	}

	ch = g_new0(CustomHeader, 1);

	ch->account_id = ac->account_id;

	ch->name = g_strdup(entry_text);

	GET_ENTRY(headers.key_entry);
	if (entry_text[0] != '\0')
		ch->value = g_strdup(entry_text);

	ch_str[0] = g_strdup_printf("%s: %s", ch->name, ch->value);

	if (row < 0)
		row = gtk_clist_append(clist, ch_str);
	else {
		CustomHeader *tmpch;

		gtk_clist_set_text(clist, row, 0, ch_str[0]);
		tmpch = gtk_clist_get_row_data(clist, row);
		if (tmpch)
			custom_header_free(tmpch);
	}

	gtk_clist_set_row_data(clist, row, ch);

	g_free(ch_str[0]);

	prefs_headers_set_list(cur_ac);

	return row;
}

static void prefs_headers_register_cb(void)
{
	prefs_headers_clist_set_row(cur_ac, -1);
}

static void prefs_headers_substitute_cb(void)
{
	GtkCList *clist = GTK_CLIST(headers.headers_clist);
	CustomHeader *ch;
	gint row;

	if (!clist->selection) return;

	row = GPOINTER_TO_INT(clist->selection->data);
	if (row == 0) return;

	ch = gtk_clist_get_row_data(clist, row);
	if (!ch) return;

	prefs_headers_clist_set_row(cur_ac, row);
}

static void prefs_headers_delete_cb(void)
{
	GtkCList *clist = GTK_CLIST(headers.headers_clist);
	CustomHeader *ch;
	gint row;

	if (!clist->selection) return;
	row = GPOINTER_TO_INT(clist->selection->data);
	if (row == 0) return;

	if (alertpanel(_("Delete header"),
		       _("Do you really want to delete this header?"),
		       _("Yes"), _("No"), NULL) == G_ALERTALTERNATE)
		return;

	ch = gtk_clist_get_row_data(clist, row);
	custom_header_free(ch);
	gtk_clist_remove(clist, row);
	cur_ac->customhdr_list = g_slist_remove(cur_ac->customhdr_list, ch);
}

static void prefs_headers_up(void)
{
	GtkCList *clist = GTK_CLIST(headers.headers_clist);
	gint row;

	if (!clist->selection) return;

	row = GPOINTER_TO_INT(clist->selection->data);
	if (row > 1) {
		gtk_clist_row_move(clist, row, row - 1);
		prefs_headers_set_list(cur_ac);
	}
}

static void prefs_headers_down(void)
{
	GtkCList *clist = GTK_CLIST(headers.headers_clist);
	gint row;

	if (!clist->selection) return;

	row = GPOINTER_TO_INT(clist->selection->data);
	if (row > 0 && row < clist->rows - 1) {
		gtk_clist_row_move(clist, row, row + 1);
		prefs_headers_set_list(cur_ac);
	}
}

#define ENTRY_SET_TEXT(entry, str) \
	gtk_entry_set_text(GTK_ENTRY(entry), str ? str : "")

static void prefs_headers_select(GtkCList *clist, gint row, gint column,
				GdkEvent *event)
{
 	CustomHeader *ch;
 	CustomHeader default_ch = { 0, "", NULL };
 
 	ch = gtk_clist_get_row_data(clist, row);
 	if (!ch)
 		ch = &default_ch;
 
 	ENTRY_SET_TEXT(headers.hdr_entry, ch->name);
 	ENTRY_SET_TEXT(headers.key_entry, ch->value);
}

static void prefs_headers_key_pressed(GtkWidget *widget, GdkEventKey *event,
				     gpointer data)
{
	if (event && event->keyval == GDK_Escape)
		gtk_widget_hide(headers.window);
}

static void prefs_headers_ok(GtkButton *button)
{
	prefs_headers_write_config(cur_ac);
	gtk_widget_hide(headers.window);
}

static void prefs_headers_cancel(GtkButton *button)
{
	/*
	prefs_headers_write_config(cur_ac); 
	*/
	prefs_headers_read_config(cur_ac); 
	gtk_widget_hide(headers.window);
}

static gint prefs_headers_deleted(GtkWidget *widget, GdkEventAny *event,
				  gpointer data)
{
	prefs_headers_cancel(GTK_BUTTON(widget));
	return TRUE;
}
