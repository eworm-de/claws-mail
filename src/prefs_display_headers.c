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
#include "prefs_display_headers.h"
#include "prefs_common.h"
#include "mainwindow.h"
#include "foldersel.h"
#include "manage_window.h"
#include "utils.h"
#include "gtkutils.h"
#include "alertpanel.h"
#include "folder.h"
#include "headers_display.h"

static struct Headers {
	GtkWidget *window;

	GtkWidget *close_btn;

	GtkWidget *hdr_combo;
	GtkWidget *hdr_entry;
	GtkWidget *key_check;
	GtkWidget *headers_clist;

	GtkWidget *other_headers;
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
static void prefs_display_headers_create		(void);

static void prefs_display_headers_set_dialog	(void);
static void prefs_display_headers_set_list	(void);
static gint prefs_display_headers_clist_set_row	(gint	 row);

/* callback functions */
static void prefs_display_headers_select_dest_cb	(void);
static void prefs_display_headers_register_cb	(void);
static void prefs_display_headers_substitute_cb	(void);
static void prefs_display_headers_delete_cb	(void);
static void prefs_display_headers_up		(void);
static void prefs_display_headers_down		(void);
static void prefs_display_headers_select		(GtkCList	*clist,
					 gint		 row,
					 gint		 column,
					 GdkEvent	*event);

static void prefs_display_headers_other_headers_toggled(void);

static void prefs_display_headers_key_pressed	(GtkWidget	*widget,
					 GdkEventKey	*event,
					 gpointer	 data);
static void prefs_display_headers_close		(GtkButton	*button);

PrefsDisplayHeaders prefs_display_headers = { 1, NULL};

static char * defaults[] =
{
	"Sender",
	"From",
	"Reply-To",
	"To",
	"Cc",
	"Subject",
	"Date",
	"-Received",
	"-Message-Id",
	"-In-Reply-To",
	"-References",
	"-Mime-Version",
	"-Content-Type",
	"-Content-Transfer-Encoding",
	"-X-UIDL",
	"-Precedence",
	"-Status",
	"-Priority"
};

static void prefs_display_headers_set_default(void)
{
	int i;

	for(i = 0 ; i < sizeof(defaults) / sizeof(char *) ; i++) {
		HeaderDisplayProp * dp =
			header_display_prop_read_str(defaults[i]);
		prefs_display_headers.headers_list =
			g_slist_append(prefs_display_headers.headers_list, dp);
	}
}

void prefs_display_headers_open(void)
{
	if (!headers.window) {
		prefs_display_headers_create();
	}

	manage_window_set_transient(GTK_WINDOW(headers.window));
	gtk_widget_grab_focus(headers.close_btn);

	prefs_display_headers_set_dialog();

	gtk_widget_show(headers.window);
}

static void prefs_display_headers_create(void)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *close_btn;
	GtkWidget *confirm_area;

	GtkWidget *vbox1;

	GtkWidget *table1;
	GtkWidget *hdr_label;
	GtkWidget *hdr_combo;
	GtkWidget *key_label;
	GtkWidget *key_check;

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

	GtkWidget *checkbtn_other_headers;

	gchar *title[] = {_("Order of headers"), _("Action")};

	debug_print(_("Creating headers setting window...\n"));

	window = gtk_window_new (GTK_WINDOW_DIALOG);
	gtk_container_set_border_width (GTK_CONTAINER (window), 8);
	gtk_window_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);
	gtk_window_set_modal (GTK_WINDOW (window), TRUE);
	gtk_window_set_policy (GTK_WINDOW (window), FALSE, TRUE, FALSE);

	vbox = gtk_vbox_new (FALSE, 6);
	gtk_widget_show (vbox);
	gtk_container_add (GTK_CONTAINER (window), vbox);

	gtkut_button_set_create (&confirm_area, &close_btn, _("Close"),
				 NULL, NULL, NULL, NULL);
	gtk_widget_show (confirm_area);
	gtk_box_pack_end (GTK_BOX(vbox), confirm_area, FALSE, FALSE, 0);
	gtk_widget_grab_default (close_btn);

	gtk_window_set_title (GTK_WINDOW(window),
			      _("Headers setting"));
	gtk_signal_connect (GTK_OBJECT(window), "delete_event",
			    GTK_SIGNAL_FUNC(gtk_widget_hide_on_delete), NULL);
	gtk_signal_connect (GTK_OBJECT(window), "key_press_event",
			    GTK_SIGNAL_FUNC(prefs_display_headers_key_pressed),
			    NULL);
	gtk_signal_connect (GTK_OBJECT(window), "focus_in_event",
			    GTK_SIGNAL_FUNC(manage_window_focus_in), NULL);
	gtk_signal_connect (GTK_OBJECT(window), "focus_out_event",
			    GTK_SIGNAL_FUNC(manage_window_focus_out), NULL);
	gtk_signal_connect (GTK_OBJECT(close_btn), "clicked",
			    GTK_SIGNAL_FUNC(prefs_display_headers_close),
			    NULL);

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
			       "From", "To", "Subject", "Date", NULL);

	key_label = gtk_label_new (_("Order (or Hide)"));
	gtk_widget_show (key_label);
	gtk_table_attach (GTK_TABLE (table1), key_label, 1, 2, 0, 1,
			  GTK_EXPAND | GTK_SHRINK | GTK_FILL,
			  0, 0, 0);
	gtk_misc_set_alignment (GTK_MISC (key_label), 0, 0.5);
	
	//	key_entry = gtk_entry_new ();
	key_check = gtk_check_button_new();
	gtk_widget_show (key_check);
	gtk_table_attach (GTK_TABLE (table1), key_check, 1, 2, 1, 2,
			  GTK_EXPAND | GTK_SHRINK | GTK_FILL,
			  0, 0, 0);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(key_check), 1);

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
			    GTK_SIGNAL_FUNC
			    (prefs_display_headers_register_cb), NULL);

	subst_btn = gtk_button_new_with_label (_(" Substitute "));
	gtk_widget_show (subst_btn);
	gtk_box_pack_start (GTK_BOX (btn_hbox), subst_btn, FALSE, TRUE, 0);
	gtk_signal_connect (GTK_OBJECT (subst_btn), "clicked",
			    GTK_SIGNAL_FUNC
			    (prefs_display_headers_substitute_cb),
			    NULL);

	del_btn = gtk_button_new_with_label (_("Delete"));
	gtk_widget_show (del_btn);
	gtk_box_pack_start (GTK_BOX (btn_hbox), del_btn, FALSE, TRUE, 0);
	gtk_signal_connect (GTK_OBJECT (del_btn), "clicked",
			    GTK_SIGNAL_FUNC (prefs_display_headers_delete_cb),
			    NULL);


	ch_hbox = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (ch_hbox);
	gtk_box_pack_start (GTK_BOX (vbox1), ch_hbox,
			    TRUE, TRUE, 0);

	ch_scrolledwin = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_set_usize (ch_scrolledwin, 230, 200);
	gtk_widget_show (ch_scrolledwin);
	gtk_box_pack_start (GTK_BOX (ch_hbox), ch_scrolledwin,
			    TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (ch_scrolledwin),
					GTK_POLICY_AUTOMATIC,
					GTK_POLICY_AUTOMATIC);

	headers_clist = gtk_clist_new_with_titles(2, title);
	gtk_widget_show (headers_clist);
	gtk_container_add (GTK_CONTAINER (ch_scrolledwin), headers_clist);
	gtk_clist_set_column_width (GTK_CLIST (headers_clist), 0, 150);
	gtk_clist_set_column_width (GTK_CLIST (headers_clist), 1, 50);
	gtk_clist_set_selection_mode (GTK_CLIST (headers_clist),
				      GTK_SELECTION_BROWSE);
	GTK_WIDGET_UNSET_FLAGS (GTK_CLIST (headers_clist)->column[0].button,
				GTK_CAN_FOCUS);
	gtk_signal_connect (GTK_OBJECT (headers_clist), "select_row",
			    GTK_SIGNAL_FUNC (prefs_display_headers_select),
			    NULL);


	btn_vbox = gtk_vbox_new (FALSE, 8);
	gtk_widget_show (btn_vbox);
	gtk_box_pack_start (GTK_BOX (ch_hbox), btn_vbox, FALSE, FALSE, 0);

	up_btn = gtk_button_new_with_label (_("Up"));
	gtk_widget_show (up_btn);
	gtk_box_pack_start (GTK_BOX (btn_vbox), up_btn, FALSE, FALSE, 0);
	gtk_signal_connect (GTK_OBJECT (up_btn), "clicked",
			    GTK_SIGNAL_FUNC (prefs_display_headers_up), NULL);

	down_btn = gtk_button_new_with_label (_("Down"));
	gtk_widget_show (down_btn);
	gtk_box_pack_start (GTK_BOX (btn_vbox), down_btn, FALSE, FALSE, 0);
	gtk_signal_connect (GTK_OBJECT (down_btn), "clicked",
			    GTK_SIGNAL_FUNC (prefs_display_headers_down), NULL);

	PACK_CHECK_BUTTON (vbox1, checkbtn_other_headers,
			   _("Show other headers"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_other_headers),
				     1);

	gtk_signal_connect
		(GTK_OBJECT (checkbtn_other_headers), "toggled",
		 GTK_SIGNAL_FUNC (prefs_display_headers_other_headers_toggled),
		 NULL);

	gtk_widget_show_all(window);

	headers.window    = window;
	headers.close_btn = close_btn;

	headers.hdr_combo  = hdr_combo;
	headers.hdr_entry  = GTK_COMBO (hdr_combo)->entry;
	headers.key_check  = key_check;
	headers.headers_clist   = headers_clist;

	headers.other_headers = checkbtn_other_headers;
}

void prefs_display_headers_read_config(void)
{
	gchar *rcpath;
	FILE *fp;
	gchar buf[PREFSBUFSIZE];
	HeaderDisplayProp *dp;

	debug_print(_("Reading configuration for displaying of headers...\n"));

	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
			     HEADERS_DISPLAY_RC, NULL);
	if ((fp = fopen(rcpath, "r")) == NULL) {
		if (ENOENT != errno) FILE_OP_ERROR(rcpath, "fopen");
		g_free(rcpath);
		prefs_display_headers.headers_list = NULL;
		prefs_display_headers_set_default();
		return;
	}
	g_free(rcpath);

 	/* remove all previous headers list */
 	while (prefs_display_headers.headers_list != NULL) {
 		dp = (HeaderDisplayProp *)
			prefs_display_headers.headers_list->data;
 		header_display_prop_free(dp);
 		prefs_display_headers.headers_list =
			g_slist_remove(prefs_display_headers.headers_list, dp);
 	}

	prefs_display_headers.show_other_headers = 1;
 
 	while (fgets(buf, sizeof(buf), fp) != NULL) {
 		g_strchomp(buf);
		if (strcmp(buf, "-") == 0)
			prefs_display_headers.show_other_headers = 0;
		else {
			dp = header_display_prop_read_str(buf);
			if (dp)
				prefs_display_headers.headers_list = g_slist_append(prefs_display_headers.headers_list, dp);
		}
 	}
 
 	fclose(fp);
}

void prefs_display_headers_write_config(void)
{
	gchar *rcpath;
	PrefFile *pfile;
	GSList *cur;
	gchar buf[PREFSBUFSIZE];
	FILE * fp;
	HeaderDisplayProp *dp;

	debug_print(_("Writing configuration for displaying of headers...\n"));

	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
			     HEADERS_DISPLAY_RC, NULL);

	if ((pfile = prefs_write_open(rcpath)) == NULL) {
		g_warning(_("failed to write configuration to file\n"));
		g_free(rcpath);
		return;
	}

	for (cur = prefs_display_headers.headers_list; cur != NULL;
	     cur = cur->next) {
 		HeaderDisplayProp *hdr = (HeaderDisplayProp *)cur->data;
		gchar *dpstr;

		dpstr = header_display_prop_get_str(hdr);
		if (fputs(dpstr, pfile->fp) == EOF ||
		    fputc('\n', pfile->fp) == EOF) {
			FILE_OP_ERROR(rcpath, "fputs || fputc");
			prefs_write_close_revert(pfile);
			g_free(rcpath);
			g_free(dpstr);
			return;
		}
		g_free(dpstr);
	}

	if (!prefs_display_headers.show_other_headers) {
		if (fputs("-\n", pfile->fp) == EOF) {
			FILE_OP_ERROR(rcpath, "fputs");
			prefs_write_close_revert(pfile);
			g_free(rcpath);
			return;
		}
	}

	g_free(rcpath);

	if (prefs_write_close(pfile) < 0) {
		g_warning(_("failed to write configuration to file\n"));
		return;
	}
}

static void prefs_display_headers_set_dialog()
{
	GtkCList *clist = GTK_CLIST(headers.headers_clist);
	GSList *cur;
	gchar *dp_str[2];
	gint row;

	gtk_clist_freeze(clist);
	gtk_clist_clear(clist);

	dp_str[0] = _("(New)");
	dp_str[1] = "";
	row = gtk_clist_append(clist, dp_str);
	gtk_clist_set_row_data(clist, row, NULL);

	for (cur = prefs_display_headers.headers_list; cur != NULL;
	     cur = cur->next) {
 		HeaderDisplayProp *dp = (HeaderDisplayProp *)cur->data;

		/*
		if (dp->hidden)
			dp_str[0] = g_strdup_printf("(%s)", dp->name);
		else
			dp_str[0] = g_strdup_printf("%s", dp->name);
		*/
		dp_str[0] = dp->name;
		dp_str[1] = dp->hidden ? _("Hide") : _("Order");

		row = gtk_clist_append(clist, dp_str);
		gtk_clist_set_row_data(clist, row, dp);

		//		g_free(dp_str[0]);
	}

	gtk_clist_thaw(clist);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(headers.other_headers), prefs_display_headers.show_other_headers);
}

static void prefs_display_headers_set_list()
{
	gint row = 1;
	HeaderDisplayProp *dp;

	g_slist_free(prefs_display_headers.headers_list);
	prefs_display_headers.headers_list = NULL;

	while ((dp = gtk_clist_get_row_data(GTK_CLIST(headers.headers_clist),
		row)) != NULL) {
		prefs_display_headers.headers_list =
			g_slist_append(prefs_display_headers.headers_list, dp);
		row++;
	}
}

#define GET_ENTRY(entry) \
	entry_text = gtk_entry_get_text(GTK_ENTRY(entry))

static gint prefs_display_headers_clist_set_row(gint row)
{
	GtkCList *clist = GTK_CLIST(headers.headers_clist);
	HeaderDisplayProp *dp;
	gchar *entry_text;
	gchar *dp_str[2];

	g_return_val_if_fail(row != 0, -1);

	GET_ENTRY(headers.hdr_entry);
	if (entry_text[0] == '\0') {
		alertpanel_error(_("Header name is not set."));
		return -1;
	}

	dp = g_new0(HeaderDisplayProp, 1);

	dp->name = g_strdup(entry_text);

	dp->hidden = !gtk_toggle_button_get_active
		(GTK_TOGGLE_BUTTON(headers.key_check));

	dp_str[0] = dp->name;
	dp_str[1] = dp->hidden ? _("Hide") : _("Order");
	/*
	if (dp->hidden)
		dp_str[0] = g_strdup_printf("(%s)", dp->name);
	else
		dp_str[0] = g_strdup_printf("%s", dp->name);
	*/

	if (row < 0)
		row = gtk_clist_append(clist, dp_str);
	else {
		HeaderDisplayProp *tmpdp;

		gtk_clist_set_text(clist, row, 0, dp_str[0]);
		gtk_clist_set_text(clist, row, 1, dp_str[1]);
		tmpdp = gtk_clist_get_row_data(clist, row);
		if (tmpdp)
			header_display_prop_free(tmpdp);
	}

	gtk_clist_set_row_data(clist, row, dp);

	//	g_free(dp_str[0]);

	prefs_display_headers_set_list();

	return row;
}

static void prefs_display_headers_register_cb(void)
{
	prefs_display_headers_clist_set_row(-1);
}

static void prefs_display_headers_substitute_cb(void)
{
	GtkCList *clist = GTK_CLIST(headers.headers_clist);
	HeaderDisplayProp *dp;
	gint row;

	if (!clist->selection) return;

	row = GPOINTER_TO_INT(clist->selection->data);
	if (row == 0) return;

	dp = gtk_clist_get_row_data(clist, row);
	if (!dp) return;

	prefs_display_headers_clist_set_row(row);
}

static void prefs_display_headers_delete_cb(void)
{
	GtkCList *clist = GTK_CLIST(headers.headers_clist);
	HeaderDisplayProp *dp;
	gint row;

	if (!clist->selection) return;
	row = GPOINTER_TO_INT(clist->selection->data);
	if (row == 0) return;

	if (alertpanel(_("Delete header"),
		       _("Do you really want to delete this header?"),
		       _("Yes"), _("No"), NULL) == G_ALERTALTERNATE)
		return;

	dp = gtk_clist_get_row_data(clist, row);
	header_display_prop_free(dp);
	gtk_clist_remove(clist, row);
	prefs_display_headers.headers_list =
		g_slist_remove(prefs_display_headers.headers_list, dp);
}

static void prefs_display_headers_up(void)
{
	GtkCList *clist = GTK_CLIST(headers.headers_clist);
	gint row;

	if (!clist->selection) return;

	row = GPOINTER_TO_INT(clist->selection->data);
	if (row > 1) {
		gtk_clist_row_move(clist, row, row - 1);
		prefs_display_headers_set_list();
	}
}

static void prefs_display_headers_down(void)
{
	GtkCList *clist = GTK_CLIST(headers.headers_clist);
	gint row;

	if (!clist->selection) return;

	row = GPOINTER_TO_INT(clist->selection->data);
	if (row > 0 && row < clist->rows - 1) {
		gtk_clist_row_move(clist, row, row + 1);
		prefs_display_headers_set_list();
	}
}

#define ENTRY_SET_TEXT(entry, str) \
	gtk_entry_set_text(GTK_ENTRY(entry), str ? str : "")

static void prefs_display_headers_select(GtkCList *clist, gint row,
					 gint column, GdkEvent *event)
{
 	HeaderDisplayProp *dp;
 	HeaderDisplayProp default_dp = { "", 0 };
 
 	dp = gtk_clist_get_row_data(clist, row);
 	if (!dp)
 		dp = &default_dp;
 
 	ENTRY_SET_TEXT(headers.hdr_entry, dp->name);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(headers.key_check),
				     !dp->hidden);

	if ((row != 0) && event && (event->type == GDK_2BUTTON_PRESS)) {
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(headers.key_check), dp->hidden);
		prefs_display_headers_clist_set_row(row);
	}
}

static void prefs_display_headers_key_pressed(GtkWidget *widget,
					      GdkEventKey *event,
					      gpointer data)
{
	if (event && event->keyval == GDK_Escape)
		gtk_widget_hide(headers.window);
}

static void prefs_display_headers_close(GtkButton *button)
{
	prefs_display_headers_write_config();
	gtk_widget_hide(headers.window);
}

static void prefs_display_headers_other_headers_toggled(void)
{
	prefs_display_headers.show_other_headers = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(headers.other_headers));
}
