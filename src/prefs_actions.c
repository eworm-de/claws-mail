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
#include <gdk/gdkx.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "intl.h"
#include "prefs.h"
#include "inc.h"
#include "utils.h"
#include "gtkutils.h"
#include "manage_window.h"
#include "mainwindow.h"
#include "prefs_common.h"
#include "alertpanel.h"
#include "prefs_actions.h"
#include "compose.h"
#include "procmsg.h"
#include "gtkstext.h"

static struct Actions {
	GtkWidget *window;

	GtkWidget *ok_btn;

	GtkWidget *name_entry;
	GtkWidget *cmd_entry;

	GtkWidget *actions_clist;
} actions;

/* widget creating functions */
static void prefs_actions_create	(MainWindow *mainwin);
static void prefs_actions_set_dialog	(void);
static gint prefs_actions_clist_set_row	(gint row);

/* callback functions */
static void prefs_actions_register_cb	(GtkWidget *w, gpointer data);
static void prefs_actions_substitute_cb	(GtkWidget *w, gpointer data);
static void prefs_actions_delete_cb	(GtkWidget *w, gpointer data);
static void prefs_actions_up		(GtkWidget *w, gpointer data);
static void prefs_actions_down		(GtkWidget *w, gpointer data);
static void prefs_actions_select	(GtkCList	*clist,
					 gint		 row,
					 gint		 column,
					 GdkEvent	*event);
static void prefs_actions_row_move	(GtkCList	*clist,
					 gint		 source_row,
					 gint		 dest_row);
static gint prefs_actions_deleted	(GtkWidget	*widget,
					 GdkEventAny	*event,
					 gpointer	*data);
static void prefs_actions_key_pressed	(GtkWidget	*widget,
					 GdkEventKey	*event,
					 gpointer	 data);
static void prefs_actions_cancel	(GtkWidget *w, gpointer data);
static void prefs_actions_ok		(GtkWidget *w, gpointer data);
static void update_actions_menu		(GtkItemFactory *ifactory,
					 gchar *branch_path,
					 gpointer callback,
					 gpointer data);
static void mainwin_actions_execute_cb 	(MainWindow	*mainwin,
				 	 guint 	 	 action_nb,
				 	 GtkWidget 	*widget);
static void compose_actions_execute_cb	(Compose	*compose,
					 guint		 action_nb,
					 GtkWidget	*widget);
static gboolean actions_check		(gchar *action);

static void pipe_command		(gchar *action,
					 GtkEditable *editable);

static void message_command		(gchar *action, 
					 SummaryView *summaryview);
static gint message_command_execute	(gchar *action, 
					 MsgInfo *msginfo);

void prefs_actions_open(MainWindow *mainwin)
{
	if (prefs_rc_is_readonly(ACTIONS_RC))
		return;
	inc_lock();

	if (!actions.window)
		prefs_actions_create(mainwin);
	
	manage_window_set_transient(GTK_WINDOW(actions.window));
	gtk_widget_grab_focus(actions.ok_btn);

	prefs_actions_set_dialog();

	gtk_widget_show(actions.window);
}

static void prefs_actions_create(MainWindow *mainwin)
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *ok_btn;
	GtkWidget *cancel_btn;
	GtkWidget *confirm_area;

	GtkWidget *vbox1;
	GtkWidget *hbox;
	GtkWidget *name_label;
	GtkWidget *name_entry;
	GtkWidget *cmd_label;
	GtkWidget *cmd_entry;
	GtkWidget *help_label;

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
	
	gchar *title[1];

	debug_print(_("Creating actions setting window...\n"));

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
					 
	gtk_signal_connect (GTK_OBJECT(window), "delete_event",
			    GTK_SIGNAL_FUNC(prefs_actions_deleted), NULL);
	gtk_signal_connect (GTK_OBJECT(window), "key_press_event",
			    GTK_SIGNAL_FUNC(prefs_actions_key_pressed), NULL);
	gtk_signal_connect (GTK_OBJECT(window), "focus_in_event",
			    GTK_SIGNAL_FUNC(manage_window_focus_in), NULL);
	gtk_signal_connect (GTK_OBJECT(window), "focus_out_event",
			    GTK_SIGNAL_FUNC(manage_window_focus_out), NULL);
	gtk_signal_connect (GTK_OBJECT(ok_btn), "clicked",
			    GTK_SIGNAL_FUNC(prefs_actions_ok), mainwin);
	gtk_signal_connect (GTK_OBJECT(cancel_btn), "clicked",
			    GTK_SIGNAL_FUNC(prefs_actions_cancel), NULL);

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_box_pack_start (GTK_BOX (vbox), vbox1, TRUE, TRUE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), 2);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX(vbox1), hbox, TRUE, TRUE, 0);
	
	name_label = gtk_label_new(_("Menu name: "));
	gtk_widget_show(name_label);
	gtk_box_pack_start (GTK_BOX (hbox), name_label, FALSE, FALSE, 0);

	name_entry = gtk_entry_new();
	gtk_widget_show(name_entry);
	gtk_box_pack_start (GTK_BOX (hbox), name_entry, TRUE, TRUE, 0);

	help_label = gtk_label_new (_("Use '/' in menu name to make submenus."));
	gtk_misc_set_alignment(GTK_MISC(help_label), 0, 0.5);
	gtk_widget_show (help_label);
	gtk_box_pack_start (GTK_BOX (vbox1), help_label, TRUE, TRUE, 0);
	
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX(vbox1), hbox, TRUE, TRUE, 0);
		 
	cmd_label = gtk_label_new(_("Command line: "));
	gtk_widget_show(cmd_label);
	gtk_box_pack_start (GTK_BOX (hbox), cmd_label, FALSE, FALSE, 0);

	cmd_entry = gtk_entry_new();
	gtk_widget_show(cmd_entry);
	gtk_box_pack_start (GTK_BOX (hbox), cmd_entry, TRUE, TRUE, 0);

	help_label = gtk_label_new (_("Prepend command with '|' to pipe selection through it.\nUse '%f' to apply the command to the message file."));
	gtk_misc_set_alignment(GTK_MISC(help_label), 0, 0.5);
	gtk_label_set_justify (GTK_LABEL(help_label), GTK_JUSTIFY_LEFT);
	gtk_widget_show (help_label);
	gtk_box_pack_start (GTK_BOX (vbox1), help_label, TRUE, TRUE, 0);

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
			    GTK_SIGNAL_FUNC (prefs_actions_register_cb), NULL);

	subst_btn = gtk_button_new_with_label (_(" Substitute "));
	gtk_widget_show (subst_btn);
	gtk_box_pack_start (GTK_BOX (btn_hbox), subst_btn, FALSE, TRUE, 0);
	gtk_signal_connect (GTK_OBJECT (subst_btn), "clicked",
			    GTK_SIGNAL_FUNC (prefs_actions_substitute_cb),
			    NULL);

	del_btn = gtk_button_new_with_label (_("Delete"));
	gtk_widget_show (del_btn);
	gtk_box_pack_start (GTK_BOX (btn_hbox), del_btn, FALSE, TRUE, 0);
	gtk_signal_connect (GTK_OBJECT (del_btn), "clicked",
			    GTK_SIGNAL_FUNC (prefs_actions_delete_cb), NULL);

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

	title[0] = _("Registered actions");
	cond_clist = gtk_clist_new_with_titles(1, title);
	gtk_widget_show (cond_clist);
	gtk_container_add (GTK_CONTAINER (cond_scrolledwin), cond_clist);
	gtk_clist_set_column_width (GTK_CLIST (cond_clist), 0, 80);
	gtk_clist_set_selection_mode (GTK_CLIST (cond_clist),
				      GTK_SELECTION_BROWSE);
	GTK_WIDGET_UNSET_FLAGS (GTK_CLIST (cond_clist)->column[0].button,
				GTK_CAN_FOCUS);
	gtk_signal_connect (GTK_OBJECT (cond_clist), "select_row",
			    GTK_SIGNAL_FUNC (prefs_actions_select), NULL);
	gtk_signal_connect_after (GTK_OBJECT (cond_clist), "row_move",
				  GTK_SIGNAL_FUNC (prefs_actions_row_move),
				  NULL);

	btn_vbox = gtk_vbox_new (FALSE, 8);
	gtk_widget_show (btn_vbox);
	gtk_box_pack_start (GTK_BOX (cond_hbox), btn_vbox, FALSE, FALSE, 0);

	up_btn = gtk_button_new_with_label (_("Up"));
	gtk_widget_show (up_btn);
	gtk_box_pack_start (GTK_BOX (btn_vbox), up_btn, FALSE, FALSE, 0);
	gtk_signal_connect (GTK_OBJECT (up_btn), "clicked",
			    GTK_SIGNAL_FUNC (prefs_actions_up), NULL);

	down_btn = gtk_button_new_with_label (_("Down"));
	gtk_widget_show (down_btn);
	gtk_box_pack_start (GTK_BOX (btn_vbox), down_btn, FALSE, FALSE, 0);
	gtk_signal_connect (GTK_OBJECT (down_btn), "clicked",
			    GTK_SIGNAL_FUNC (prefs_actions_down), NULL);

	gtk_widget_show_all(window);

	actions.window = window;
	actions.ok_btn = ok_btn;

	actions.name_entry = name_entry;
	actions.cmd_entry  = cmd_entry;
	
	actions.actions_clist = cond_clist;
}

void prefs_actions_read_config()
{
	gchar *rcpath;
	FILE *fp;
	gchar buf[PREFSBUFSIZE];
	gchar *act;

	debug_print(_("Reading actions configurations...\n"));

	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, ACTIONS_RC, NULL);
	if ((fp = fopen(rcpath, "r")) == NULL) {
		if (ENOENT != errno) FILE_OP_ERROR(rcpath, "fopen");
		g_free(rcpath);
		return;
	}
	g_free(rcpath);

	while (prefs_common.actionslst != NULL) {
		act = (gchar *) prefs_common.actionslst->data;
		prefs_common.actionslst = g_slist_remove(
						prefs_common.actionslst,
						act);
		g_free(act);
	}
	
	while (fgets(buf, sizeof(buf), fp) != NULL) {
		g_strchomp(buf);
		if (actions_check(buf))
			prefs_common.actionslst = g_slist_append(
							prefs_common.actionslst,
							g_strdup(buf));
	}
	fclose(fp);
}

void prefs_actions_write_config()
{
	gchar *rcpath;
	PrefFile *pfile;
	GSList *cur;

	debug_print(_("Writing actions configuration...\n"));

	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, ACTIONS_RC, NULL);
	if ((pfile= prefs_write_open(rcpath)) == NULL) {
		g_warning(_("failed to write configuration to file\n"));
		g_free(rcpath);
		return;
	}

	for (cur = prefs_common.actionslst; cur != NULL; cur = cur->next) {
		gchar *act = (gchar *) cur->data;
		if (fputs(act, pfile->fp) == EOF ||
		    fputc('\n', pfile->fp) == EOF) {
			FILE_OP_ERROR(rcpath, "fputs || fputc");
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

static gboolean actions_check(gchar *action)
{
	/* FIXME: make a real syntax checking function */
	return TRUE;
}

static void prefs_actions_set_dialog	(void)
{
	GtkCList *clist = GTK_CLIST(actions.actions_clist);
	GSList *cur;
	gchar *action_str[1];
	gint row;

	gtk_clist_freeze(clist);
	gtk_clist_clear(clist);

	action_str[0] = _("(New)");
	row = gtk_clist_append(clist, action_str);
	gtk_clist_set_row_data(clist, row, NULL);

	for (cur = prefs_common.actionslst; cur != NULL; cur = cur->next) {
		gchar *action[1];
		action[0] = (gchar *) cur->data;

		row = gtk_clist_append(clist, action);
		gtk_clist_set_row_data(clist, row, action[0]);
	}
	
	gtk_clist_thaw(clist);
}
static void prefs_actions_set_list(void)
{
	gint row = 1;
	gchar *action;
	
	g_slist_free(prefs_common.actionslst);
	prefs_common.actionslst = NULL;

	while ( (action = (gchar *) gtk_clist_get_row_data(GTK_CLIST(
						 actions.actions_clist), row))
		!= NULL) {
		prefs_common.actionslst = g_slist_append(prefs_common.actionslst,
							 action);
		row++;
	}
}
		

#define GET_ENTRY(entry) \
	entry_text = gtk_entry_get_text(GTK_ENTRY(entry))
static gint prefs_actions_clist_set_row	(gint row)
{
	GtkCList *clist = GTK_CLIST(actions.actions_clist);
	gchar *entry_text;
	gint len;
	gchar action[PREFSBUFSIZE];
	gchar *buf[1];
	
	
	g_return_val_if_fail(row != 0, -1);

	

	GET_ENTRY(actions.name_entry);
	if (entry_text[0] == '\0') {
		alertpanel_error(_("Menu name is not set."));
		return -1;
	}

	if (strchr(entry_text, ':')) {
		alertpanel_error(_("Colon ':' is not allowed in the menu name."));
		return -1;
	}
	
	strncpy(action, entry_text, PREFSBUFSIZE - 1);
	g_strstrip(action);

	/* Keep space for the ': ' delimiter */
	len = strlen(action) + 2;
	if (len >= PREFSBUFSIZE - 1) {
		alertpanel_error(_("Menu name is too long."));
		return -1;
	}

	strcat(action, ": ");

	GET_ENTRY(actions.cmd_entry);

	if (entry_text[0] == '\0') {
		alertpanel_error(_("Command line not set."));
		return -1;
	}

	if (len + strlen(entry_text) >= PREFSBUFSIZE - 1) {
		alertpanel_error(_("Menu name and command are too long."));
		return -1;
	}

	if (!actions_check(entry_text)) {
		alertpanel_error(_("Command syntax error."));
		return -1;
	}
	
	strcat(action, entry_text);

	buf[0] = action;
	if (row < 0)
		row = gtk_clist_append(clist, buf);
	else {
		gchar *old_action;
		gtk_clist_set_text(clist, row, 0, action);
		old_action = (gchar *) gtk_clist_get_row_data(clist, row);
		if (old_action)
			g_free(old_action);
	}

	buf[0] = g_strdup(action);
	
	gtk_clist_set_row_data(clist, row, buf[0]);

	prefs_actions_set_list();

	return 0;
}
	
/* callback functions */
static void prefs_actions_register_cb	(GtkWidget *w, gpointer data)
{
	prefs_actions_clist_set_row(-1);
}

static void prefs_actions_substitute_cb	(GtkWidget *w, gpointer data)
{
	GtkCList *clist = GTK_CLIST(actions.actions_clist);
	gchar *action;
	gint row;

	if (!clist->selection) return;

	row = GPOINTER_TO_INT(clist->selection->data);
	if (row == 0) return;

	action = gtk_clist_get_row_data(clist, row);
	if (!action) return;
	
	prefs_actions_clist_set_row(row);
}

static void prefs_actions_delete_cb	(GtkWidget *w, gpointer data)
{
	GtkCList *clist = GTK_CLIST(actions.actions_clist);
	gchar *action;
	gint row;

	if (!clist->selection) return;
	row = GPOINTER_TO_INT(clist->selection->data);
	if (row == 0) return;

	if (alertpanel(_("Delete action"),
		       _("Do you really want to delete this action?"),
		       _("Yes"), _("No"), NULL) == G_ALERTALTERNATE)
		return;

	action = gtk_clist_get_row_data(clist, row);
	g_free(action);
	gtk_clist_remove(clist, row);
	prefs_common.actionslst = g_slist_remove(prefs_common.actionslst, action);
}

static void prefs_actions_up		(GtkWidget *w, gpointer data)
{
	GtkCList *clist = GTK_CLIST(actions.actions_clist);
	gint row;

	if (!clist->selection) return;

	row = GPOINTER_TO_INT(clist->selection->data);
	if (row > 1)
		gtk_clist_row_move(clist, row, row - 1);
}

static void prefs_actions_down		(GtkWidget *w, gpointer data)
{
	GtkCList *clist = GTK_CLIST(actions.actions_clist);
	gint row;

	if (!clist->selection) return;

	row = GPOINTER_TO_INT(clist->selection->data);
	if (row > 0 && row < clist->rows - 1)
		gtk_clist_row_move(clist, row, row + 1);
}

#define ENTRY_SET_TEXT(entry, str) \
	gtk_entry_set_text(GTK_ENTRY(entry), str ? str : "")
static void prefs_actions_select	(GtkCList	*clist,
					 gint		 row,
					 gint		 column,
					 GdkEvent	*event)
{
	gchar *action;
	gchar *cmd;
	gchar buf[PREFSBUFSIZE];
	action = gtk_clist_get_row_data(clist, row);

	if (!action) {
		ENTRY_SET_TEXT(actions.name_entry, "");
		ENTRY_SET_TEXT(actions.cmd_entry, "");
		return;
	}
	
	strncpy(buf, action, PREFSBUFSIZE - 1);
	buf[PREFSBUFSIZE - 1] = 0x00;
	cmd = strstr2(buf, ": ");

	if (cmd && cmd[2])
		ENTRY_SET_TEXT(actions.cmd_entry, &cmd[2]);
	else
		return;

	*cmd = 0x00;
	ENTRY_SET_TEXT(actions.name_entry, buf);
}
	
static void prefs_actions_row_move	(GtkCList	*clist,
					 gint		 source_row,
					 gint		 dest_row)
{
	prefs_actions_set_list();
	if (gtk_clist_row_is_visible(clist, dest_row) != GTK_VISIBILITY_FULL) {
		gtk_clist_moveto(clist, dest_row, -1,
				 source_row < dest_row ? 1.0 : 0.0, 0.0);
	}
}

static gint prefs_actions_deleted	(GtkWidget	*widget,
					 GdkEventAny	*event,
					 gpointer	*data)
{
	prefs_actions_cancel(widget, data);
	return TRUE;
}

static void prefs_actions_key_pressed	(GtkWidget	*widget,
					 GdkEventKey	*event,
					 gpointer	 data)
{
	if (event && event->keyval == GDK_Escape)
		prefs_actions_cancel(widget, data);
}

static void prefs_actions_cancel	(GtkWidget *w, gpointer data)
{
	prefs_actions_read_config();
	gtk_widget_hide(actions.window);
	inc_unlock();
}

static void prefs_actions_ok		(GtkWidget *widget, gpointer data)
{
	GtkItemFactory *ifactory;
	MainWindow *mainwin = (MainWindow *) data;

	prefs_actions_write_config();
	ifactory = gtk_item_factory_from_widget(mainwin->menubar);
	update_mainwin_actions_menu(ifactory, mainwin);
	gtk_widget_hide(actions.window);
	inc_unlock();
}

void update_mainwin_actions_menu(GtkItemFactory *ifactory, 
				 MainWindow *mainwin)
{
	update_actions_menu(ifactory, "/Edit/Actions", 
			    mainwin_actions_execute_cb, 
			    mainwin);
}

void update_compose_actions_menu(GtkItemFactory *ifactory, 
				 gchar *branch_path,
				 Compose *compose)
{
	update_actions_menu(ifactory, branch_path, 
			    compose_actions_execute_cb, 
			    compose);
}
				 
static void update_actions_menu(GtkItemFactory *ifactory,
				gchar *branch_path,
				gpointer callback,
				gpointer data)
{
	GtkWidget *menuitem;
	gchar *menu_path;
	GSList *cur;
	gchar *buf;
	gint sensitive;
	GtkItemFactoryEntry ifentry = {
		branch_path, NULL, NULL, 0, "<Branch>"};
	sensitive = GTK_WIDGET_IS_SENSITIVE(gtk_item_factory_get_item(ifactory, branch_path));
	gtk_item_factory_delete_item(ifactory, branch_path);
	gtk_item_factory_create_item(ifactory, &ifentry, NULL, 1);
	menuitem = gtk_item_factory_get_item(ifactory, branch_path);
	gtk_widget_set_sensitive(menuitem, sensitive);

	ifentry.accelerator     = NULL;
	ifentry.callback_action = 0;
	ifentry.callback        = callback;
	ifentry.item_type       = NULL;

	for (cur = prefs_common.actionslst; cur; cur = cur->next) {
		menu_path = (gchar *) cur->data;
		if (actions_check(menu_path)) {
			buf = g_strdup_printf("%s/%s", branch_path, menu_path);
			menu_path = strstr(buf, ": ");
			if (menu_path) {
				*menu_path = 0x00;
				menu_path = strchr(buf, '/');
				ifentry.path = menu_path;
				gtk_item_factory_create_item(ifactory, &ifentry,
						data, 1);
				}
			g_free(menu_path);
		}
		ifentry.callback_action++;	
	}
}

static void compose_actions_execute_cb	(Compose	*compose,
					 guint		 action_nb,
					 GtkWidget	*widget)
{
	gchar *buf, *action;
	
	g_return_if_fail(action_nb < g_slist_length(prefs_common.actionslst));
	
	buf = (gchar *) g_slist_nth_data(prefs_common.actionslst, action_nb);
	
	g_return_if_fail(buf);

	g_return_if_fail(action = strstr(buf, ": "));

	/* Point to the beginning of the command-line */
	action++;
	action++;
	
	if (action[0] == '|')
		pipe_command(&action[1], GTK_EDITABLE(compose->text));
	else {
		alertpanel_warning(_("The selected action is not a pipe action.\n You can only use pipe actions when composing a message."));
		return;
	}
}

static void mainwin_actions_execute_cb 	(MainWindow	*mainwin,
				 	 guint 	 	 action_nb,
				 	 GtkWidget 	*widget)
{
	MessageView *messageview = mainwin->messageview;
	GtkEditable *editable = NULL;
	gchar *buf, *action;
	
	g_return_if_fail (action_nb < g_slist_length(prefs_common.actionslst));

	buf = (gchar *) g_slist_nth_data(prefs_common.actionslst, action_nb);
	
	g_return_if_fail(buf);

	g_return_if_fail(action = strstr(buf, ": "));

	switch (messageview->type) {
		case MVIEW_TEXT:
			if (messageview->textview && messageview->textview->text)
				editable = GTK_EDITABLE(messageview->textview->text);
			break;
		case MVIEW_MIME:
			if (messageview->mimeview && messageview->mimeview->type == MIMEVIEW_TEXT &&
			    messageview->mimeview->textview && messageview->mimeview->textview->text)
				editable = GTK_EDITABLE(messageview->mimeview->textview->text);
			break;
		default:
			return;
	}

	if (!editable)
		return;

	/* Point to the beginning of the command-line */
	action++;
	action++;
	

	if (action[0] == '|')
		pipe_command(&action[1], editable);
	else 
		message_command(action, mainwin->summaryview);
}

static void pipe_command(gchar *action, GtkEditable *editable)
{
	gchar *selection;
	gint start =  0;
	gint end   = gtk_stext_get_length(GTK_STEXT(editable));
	gint orig_pos;
	gint fd1[2], fd2[2];
	pid_t pid;
	gchar **cmdline;

	if (editable->has_selection) {
		start = editable->selection_start_pos;
		end   = editable->selection_end_pos;
		if (start > end) {
			gint tmp;
			tmp = start;
			start = end;
			end = tmp;
		}
		if (start == end) {
			start =  0;
			end   = gtk_stext_get_length(GTK_STEXT(editable));
		}
	}


	if (pipe(fd1)) {
		alertpanel_error(_("Command could not started. Pipe creation failed.\n%s"), g_strerror(errno));
		return;
	}
	if (pipe(fd2)) {
		alertpanel_error(_("Command could not started. Pipe creation failed.\n%s"), g_strerror(errno));
		return;
	}

	pid = fork();
	if (pid == (pid_t) 0) {
		close(0);
		dup(fd1[0]);
		close(fd1[0]);
		close(fd1[1]);
	
		close(1);
		dup(fd2[1]);
		close(fd2[0]);
		close(fd2[1]);
		close(ConnectionNumber(gdk_display));

		cmdline = strsplit_with_quote(action, " ", 1024);
		execvp(cmdline[0], cmdline);
		
		perror("execvp");
		g_strfreev(cmdline);

		_exit(1);
	} else if (pid < (pid_t) 0) {
		alertpanel_error(_("Could not fork to execute command.\n%s"), g_strerror(errno));
		return;
	} else {
		gchar buf[PREFSBUFSIZE];
		gint c;
		
		buf[PREFSBUFSIZE - 1] = 0x00;

		gtk_stext_freeze(GTK_STEXT(editable));
		
		selection = gtk_editable_get_chars(editable, start, end);

		close(fd1[0]);
		close(fd2[1]);

		write(fd1[1], selection, strlen(selection));

		close(fd1[1]);
		close(fd2[1]);
		
		orig_pos = gtk_editable_get_position(editable);	

		gtk_stext_set_point(GTK_STEXT(editable), start);
		gtk_stext_forward_delete(GTK_STEXT(editable), end - start);

		while (TRUE) {
			c = read(fd2[0], buf, PREFSBUFSIZE - 1);
			gtk_stext_insert(GTK_STEXT(editable), NULL, NULL, NULL, buf, c);
			if (c == 0) break;
		}
		close(fd2[0]);
		/* FIXME: should we wait kindly as in message_command_execute 
		   and blocking interactions? */
		waitpid(0, NULL, 0); 

		end = gtk_stext_get_length(GTK_STEXT(editable));
		if (end < orig_pos)
			orig_pos = end;

		gtk_stext_thaw(GTK_STEXT(editable));
		gtk_stext_set_point(GTK_STEXT(editable), orig_pos);
		gtk_editable_set_position(editable, orig_pos);
	}
}
	
static void message_command(gchar *action, SummaryView *summaryview)
{
	GtkCTree *ctree = GTK_CTREE(summaryview->ctree);
	GList *cur;
	MsgInfo *msginfo;

	main_window_cursor_wait(summaryview->mainwin);
	/* Do no allow interaction, as these commmands can mess with the message files */
	gtk_widget_set_sensitive(summaryview->mainwin->window, FALSE);
	for (cur = GTK_CLIST(ctree)->selection; cur != NULL; cur = cur->next) {
		msginfo = gtk_ctree_node_get_row_data(ctree, 
						GTK_CTREE_NODE(cur->data));
		if(message_command_execute(action, msginfo))
			break;
	}
	main_window_cursor_normal(summaryview->mainwin);
	gtk_widget_set_sensitive(summaryview->mainwin->window, TRUE);
}

static gint message_command_execute(gchar *action, MsgInfo *msginfo)
{
	gchar *filename, *p;	
	gchar buf[PREFSBUFSIZE];
	pid_t pid;
	gchar **cmdline;

	filename = procmsg_get_message_file(msginfo);

	if ((p = strchr(action, '%')) && p[1] == 'f' && !strchr(p + 2, '%')) {
		p[1] = 's';
		g_snprintf(buf, sizeof(buf) - 1, action, filename);
	} else {
		alertpanel_error(_("Syntax error in command line. Only one '%%' is alloawed, and it must be followed by 'f'."));
		return -1;
	}
	
	g_free(filename);
	
	pid = fork();
	if (pid == (pid_t) 0) {
		close(ConnectionNumber(gdk_display));

		cmdline = strsplit_with_quote(buf, " ", 1024);
		execvp(cmdline[0], cmdline);

		perror("execvp");
		g_strfreev(cmdline);
	
		_exit(1);
	} else if (pid < (pid_t) 0) {
		alertpanel_error(_("Could not fork to execute command.\n%s"), g_strerror(errno));
		return -1;
	} else {
		while (!waitpid(0, NULL, WNOHANG))
			while (gtk_events_pending())
				gtk_main_iteration();
	}
	return 0;
}
