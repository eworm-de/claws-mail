/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2003 Hiroyuki Yamamoto & The Sylpheed Claws Team
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
#include "prefs_gtk.h"
#include "inc.h"
#include "utils.h"
#include "gtkutils.h"
#include "manage_window.h"
#include "mainwindow.h"
#include "prefs_common.h"
#include "alertpanel.h"
#include "prefs_actions.h"
#include "action.h"
#include "description_window.h"
#include "gtkutils.h"

static struct Actions
{
	GtkWidget *window;

	GtkWidget *ok_btn;

	GtkWidget *name_entry;
	GtkWidget *cmd_entry;

	GtkWidget *actions_clist;
} actions;

static int modified = FALSE;

/* widget creating functions */
static void prefs_actions_create	(MainWindow *mainwin);
static void prefs_actions_set_dialog	(void);
static gint prefs_actions_clist_set_row	(gint row);

/* callback functions */
static void prefs_actions_help_cb	(GtkWidget	*w,
					 gpointer	 data);
static void prefs_actions_register_cb	(GtkWidget	*w,
					 gpointer	 data);
static void prefs_actions_substitute_cb	(GtkWidget	*w,
					 gpointer	 data);
static void prefs_actions_delete_cb	(GtkWidget	*w,
					 gpointer	 data);
static void prefs_actions_up		(GtkWidget	*w,
					 gpointer	 data);
static void prefs_actions_down		(GtkWidget	*w,
					 gpointer	 data);
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
static void prefs_actions_cancel	(GtkWidget	*w,
					 gpointer	 data);
static void prefs_actions_ok		(GtkWidget	*w,
					 gpointer	 data);


void prefs_actions_open(MainWindow *mainwin)
{
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

	GtkWidget *entry_vbox;
	GtkWidget *hbox;
	GtkWidget *name_label;
	GtkWidget *name_entry;
	GtkWidget *cmd_label;
	GtkWidget *cmd_entry;

	GtkWidget *reg_hbox;
	GtkWidget *btn_hbox;
	GtkWidget *arrow;
	GtkWidget *reg_btn;
	GtkWidget *subst_btn;
	GtkWidget *del_btn;

	GtkWidget *cond_hbox;
	GtkWidget *cond_scrolledwin;
	GtkWidget *cond_clist;

	GtkWidget *help_button;

	GtkWidget *btn_vbox;
	GtkWidget *up_btn;
	GtkWidget *down_btn;

	gchar *title[1];

	debug_print("Creating actions configuration window...\n");

	window = gtk_window_new (GTK_WINDOW_DIALOG);

	gtk_container_set_border_width(GTK_CONTAINER (window), 8);
	gtk_window_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
	gtk_window_set_modal(GTK_WINDOW(window), TRUE);
	gtk_window_set_policy(GTK_WINDOW(window), FALSE, TRUE, TRUE);
	gtk_window_set_default_size(GTK_WINDOW(window), 400, -1);

	vbox = gtk_vbox_new(FALSE, 6);
	gtk_widget_show(vbox);
	gtk_container_add(GTK_CONTAINER(window), vbox);

	gtkut_button_set_create(&confirm_area, &ok_btn, _("OK"),
				&cancel_btn, _("Cancel"), NULL, NULL);
	gtk_widget_show(confirm_area);
	gtk_box_pack_end(GTK_BOX(vbox), confirm_area, FALSE, FALSE, 0);
	gtk_widget_grab_default(ok_btn);

	gtk_window_set_title(GTK_WINDOW(window), _("Actions configuration"));
	gtk_signal_connect(GTK_OBJECT(window), "delete_event",
			   GTK_SIGNAL_FUNC(prefs_actions_deleted), NULL);
	gtk_signal_connect(GTK_OBJECT(window), "key_press_event",
			   GTK_SIGNAL_FUNC(prefs_actions_key_pressed), NULL);
	MANAGE_WINDOW_SIGNALS_CONNECT(window);
	gtk_signal_connect(GTK_OBJECT(ok_btn), "clicked",
			   GTK_SIGNAL_FUNC(prefs_actions_ok), mainwin);
	gtk_signal_connect(GTK_OBJECT(cancel_btn), "clicked",
			   GTK_SIGNAL_FUNC(prefs_actions_cancel), NULL);

	vbox1 = gtk_vbox_new(FALSE, 8);
	gtk_widget_show(vbox1);
	gtk_box_pack_start(GTK_BOX(vbox), vbox1, TRUE, TRUE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(vbox1), 2);

	entry_vbox = gtk_vbox_new(FALSE, 4);
	gtk_box_pack_start(GTK_BOX(vbox1), entry_vbox, FALSE, FALSE, 0);

	hbox = gtk_hbox_new(FALSE, 8);
	gtk_box_pack_start(GTK_BOX(entry_vbox), hbox, FALSE, FALSE, 0);

	name_label = gtk_label_new(_("Menu name:"));
	gtk_box_pack_start(GTK_BOX(hbox), name_label, FALSE, FALSE, 0);

	name_entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(hbox), name_entry, TRUE, TRUE, 0);

	hbox = gtk_hbox_new(FALSE, 8);
	gtk_box_pack_start(GTK_BOX(entry_vbox), hbox, TRUE, TRUE, 0);

	cmd_label = gtk_label_new(_("Command line:"));
	gtk_box_pack_start(GTK_BOX(hbox), cmd_label, FALSE, FALSE, 0);

	cmd_entry = gtk_entry_new();
	gtk_box_pack_start(GTK_BOX(hbox), cmd_entry, TRUE, TRUE, 0);

	gtk_widget_show_all(entry_vbox);

	/* register / substitute / delete */

	reg_hbox = gtk_hbox_new(FALSE, 4);
	gtk_widget_show(reg_hbox);
	gtk_box_pack_start(GTK_BOX(vbox1), reg_hbox, FALSE, FALSE, 0);

	arrow = gtk_arrow_new(GTK_ARROW_DOWN, GTK_SHADOW_OUT);
	gtk_widget_show(arrow);
	gtk_box_pack_start(GTK_BOX(reg_hbox), arrow, FALSE, FALSE, 0);
	gtk_widget_set_usize(arrow, -1, 16);

	btn_hbox = gtk_hbox_new(TRUE, 4);
	gtk_widget_show(btn_hbox);
	gtk_box_pack_start(GTK_BOX(reg_hbox), btn_hbox, FALSE, FALSE, 0);

	reg_btn = gtk_button_new_with_label(_("Add"));
	gtk_widget_show(reg_btn);
	gtk_box_pack_start(GTK_BOX(btn_hbox), reg_btn, FALSE, TRUE, 0);
	gtk_signal_connect(GTK_OBJECT(reg_btn), "clicked",
			   GTK_SIGNAL_FUNC(prefs_actions_register_cb), NULL);

	subst_btn = gtk_button_new_with_label(_(" Replace "));
	gtk_widget_show(subst_btn);
	gtk_box_pack_start(GTK_BOX(btn_hbox), subst_btn, FALSE, TRUE, 0);
	gtk_signal_connect(GTK_OBJECT(subst_btn), "clicked",
			   GTK_SIGNAL_FUNC(prefs_actions_substitute_cb),
			   NULL);

	del_btn = gtk_button_new_with_label(_("Delete"));
	gtk_widget_show(del_btn);
	gtk_box_pack_start(GTK_BOX(btn_hbox), del_btn, FALSE, TRUE, 0);
	gtk_signal_connect(GTK_OBJECT(del_btn), "clicked",
			   GTK_SIGNAL_FUNC(prefs_actions_delete_cb), NULL);

	help_button = gtk_button_new_with_label(_(" Syntax help "));
	gtk_widget_show(help_button);
	gtk_box_pack_end(GTK_BOX(reg_hbox), help_button, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(help_button), "clicked",
			   GTK_SIGNAL_FUNC(prefs_actions_help_cb), NULL);

	cond_hbox = gtk_hbox_new(FALSE, 8);
	gtk_widget_show(cond_hbox);
	gtk_box_pack_start(GTK_BOX(vbox1), cond_hbox, TRUE, TRUE, 0);

	cond_scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_show(cond_scrolledwin);
	gtk_widget_set_usize(cond_scrolledwin, -1, 150);
	gtk_box_pack_start(GTK_BOX(cond_hbox), cond_scrolledwin,
			   TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW (cond_scrolledwin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);

	title[0] = _("Current actions");
	cond_clist = gtk_clist_new_with_titles(1, title);
	gtk_widget_show(cond_clist);
	gtk_container_add(GTK_CONTAINER (cond_scrolledwin), cond_clist);
	gtk_clist_set_column_width(GTK_CLIST (cond_clist), 0, 80);
	gtk_clist_set_selection_mode(GTK_CLIST (cond_clist),
				     GTK_SELECTION_BROWSE);
	GTK_WIDGET_UNSET_FLAGS(GTK_CLIST(cond_clist)->column[0].button,
			       GTK_CAN_FOCUS);
	gtk_signal_connect(GTK_OBJECT(cond_clist), "select_row",
			   GTK_SIGNAL_FUNC(prefs_actions_select), NULL);
	gtk_signal_connect_after(GTK_OBJECT(cond_clist), "row_move",
				 GTK_SIGNAL_FUNC(prefs_actions_row_move),
				 NULL);

	btn_vbox = gtk_vbox_new(FALSE, 8);
	gtk_widget_show(btn_vbox);
	gtk_box_pack_start(GTK_BOX(cond_hbox), btn_vbox, FALSE, FALSE, 0);

	up_btn = gtk_button_new_with_label(_("Up"));
	gtk_widget_show(up_btn);
	gtk_box_pack_start(GTK_BOX(btn_vbox), up_btn, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(up_btn), "clicked",
			   GTK_SIGNAL_FUNC(prefs_actions_up), NULL);

	down_btn = gtk_button_new_with_label(_("Down"));
	gtk_widget_show(down_btn);
	gtk_box_pack_start(GTK_BOX(btn_vbox), down_btn, FALSE, FALSE, 0);
	gtk_signal_connect(GTK_OBJECT(down_btn), "clicked",
			   GTK_SIGNAL_FUNC(prefs_actions_down), NULL);

	gtk_widget_show(window);

	actions.window = window;
	actions.ok_btn = ok_btn;

	actions.name_entry = name_entry;
	actions.cmd_entry  = cmd_entry;

	actions.actions_clist = cond_clist;
}


void prefs_actions_read_config(void)
{
	gchar *rcpath;
	FILE *fp;
	gchar buf[PREFSBUFSIZE];
	gchar *act;

	debug_print("Reading actions configurations...\n");

	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, ACTIONS_RC, NULL);
	if ((fp = fopen(rcpath, "rb")) == NULL) {
		if (ENOENT != errno) FILE_OP_ERROR(rcpath, "fopen");
		g_free(rcpath);
		return;
	}
	g_free(rcpath);

	while (prefs_common.actions_list != NULL) {
		act = (gchar *)prefs_common.actions_list->data;
		prefs_common.actions_list =
			g_slist_remove(prefs_common.actions_list, act);
		g_free(act);
	}

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		g_strchomp(buf);
		act = strstr(buf, ": ");
		if (act && act[2] && 
		    action_get_type(&act[2]) != ACTION_ERROR)
			prefs_common.actions_list =
				g_slist_append(prefs_common.actions_list,
					       g_strdup(buf));
	}
	fclose(fp);
}

void prefs_actions_write_config(void)
{
	gchar *rcpath;
	PrefFile *pfile;
	GSList *cur;

	debug_print("Writing actions configuration...\n");

	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, ACTIONS_RC, NULL);
	if ((pfile= prefs_write_open(rcpath)) == NULL) {
		g_warning("failed to write configuration to file\n");
		g_free(rcpath);
		return;
	}

	for (cur = prefs_common.actions_list; cur != NULL; cur = cur->next) {
		gchar *act = (gchar *)cur->data;
		if (fputs(act, pfile->fp) == EOF ||
		    fputc('\n', pfile->fp) == EOF) {
			FILE_OP_ERROR(rcpath, "fputs || fputc");
			prefs_file_close_revert(pfile);
			g_free(rcpath);
			return;
		}
	}
	
	g_free(rcpath);

	if (prefs_file_close(pfile) < 0) {
		g_warning("failed to write configuration to file\n");
		return;
	}
}

static void prefs_actions_set_dialog(void)
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

	for (cur = prefs_common.actions_list; cur != NULL; cur = cur->next) {
		gchar *action[1];

		action[0] = (gchar *)cur->data;
		row = gtk_clist_append(clist, action);
		gtk_clist_set_row_data(clist, row, action[0]);
	}

	gtk_clist_thaw(clist);
}

static void prefs_actions_set_list(void)
{
	gint row = 1;
	gchar *action;

	g_slist_free(prefs_common.actions_list);
	prefs_common.actions_list = NULL;

	while ((action = (gchar *)gtk_clist_get_row_data
		(GTK_CLIST(actions.actions_clist), row)) != NULL) {
		prefs_common.actions_list =
			g_slist_append(prefs_common.actions_list, action);
		row++;
	}
}

#define GET_ENTRY(entry) \
	entry_text = gtk_entry_get_text(GTK_ENTRY(entry))

static gint prefs_actions_clist_set_row(gint row)
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

	if (action_get_type(entry_text) == ACTION_ERROR) {
		alertpanel_error(_("The command\n%s\nhas a syntax error."), 
				 entry_text);
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

static void prefs_actions_register_cb(GtkWidget *w, gpointer data)
{
	prefs_actions_clist_set_row(-1);
	modified = FALSE;
}

static void prefs_actions_substitute_cb(GtkWidget *w, gpointer data)
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
	modified = FALSE;
}

static void prefs_actions_delete_cb(GtkWidget *w, gpointer data)
{
	GtkCList *clist = GTK_CLIST(actions.actions_clist);
	gchar *action;
	gint row;

	if (!clist->selection) return;
	row = GPOINTER_TO_INT(clist->selection->data);
	if (row == 0) return;

	if (alertpanel(_("Delete action"),
		       _("Do you really want to delete this action?"),
		       _("Yes"), _("No"), NULL) != G_ALERTDEFAULT)
		return;

	action = gtk_clist_get_row_data(clist, row);
	g_free(action);
	gtk_clist_remove(clist, row);
	prefs_common.actions_list = g_slist_remove(prefs_common.actions_list,
						   action);
}

static void prefs_actions_up(GtkWidget *w, gpointer data)
{
	GtkCList *clist = GTK_CLIST(actions.actions_clist);
	gint row;

	if (!clist->selection) return;

	row = GPOINTER_TO_INT(clist->selection->data);
	if (row > 1)
		gtk_clist_row_move(clist, row, row - 1);
}

static void prefs_actions_down(GtkWidget *w, gpointer data)
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

static void prefs_actions_select(GtkCList *clist, gint row, gint column,
				 GdkEvent *event)
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
	cmd = strstr(buf, ": ");

	if (cmd && cmd[2])
		ENTRY_SET_TEXT(actions.cmd_entry, &cmd[2]);
	else
		return;

	*cmd = 0x00;
	ENTRY_SET_TEXT(actions.name_entry, buf);
}

static void prefs_actions_row_move(GtkCList *clist,
				   gint source_row, gint dest_row)
{
	prefs_actions_set_list();
	if (gtk_clist_row_is_visible(clist, dest_row) != GTK_VISIBILITY_FULL) {
		gtk_clist_moveto(clist, dest_row, -1,
				 source_row < dest_row ? 1.0 : 0.0, 0.0);
	}
}

static gint prefs_actions_deleted(GtkWidget *widget, GdkEventAny *event,
				  gpointer *data)
{
	prefs_actions_cancel(widget, data);
	return TRUE;
}

static void prefs_actions_key_pressed(GtkWidget *widget, GdkEventKey *event,
				      gpointer data)
{
	if (event && event->keyval == GDK_Escape)
		prefs_actions_cancel(widget, data);
	else {
		GtkWidget *focused = gtkut_get_focused_child(
					GTK_CONTAINER(widget));
		if (focused && GTK_IS_EDITABLE(focused)) {
			modified = TRUE;
		}
	}
	return;
}

static void prefs_actions_cancel(GtkWidget *w, gpointer data)
{
	if (modified && alertpanel(_("Entry not saved"),
				 _("The entry was not saved. Close anyway?"),
				 _("Yes"), _("No"), NULL) != G_ALERTDEFAULT) {
		return;
	}
	modified = FALSE;
	prefs_actions_read_config();
	gtk_widget_hide(actions.window);
	inc_unlock();
}

static void prefs_actions_ok(GtkWidget *widget, gpointer data)
{
	MainWindow *mainwin = (MainWindow *) data;
	GList *list;
	GList *iter;
	MessageView *msgview;
	Compose *compose;

	if (modified && alertpanel(_("Entry not saved"),
				 _("The entry was not saved. Close anyway?"),
				 _("Yes"), _("No"), NULL) != G_ALERTDEFAULT) {
		return;
	}
	modified = FALSE;
	prefs_actions_write_config();

	/* Update mainwindow actions menu */
	main_window_update_actions_menu(mainwin);

	/* Update separated message view actions menu */
	list = messageview_get_msgview_list();
	for (iter = list; iter; iter = iter->next) {
		msgview = (MessageView *) iter->data;
		messageview_update_actions_menu(msgview);
	}

	/* Update compose windows actions menu */
	list = compose_get_compose_list();
	for (iter = list; iter; iter = iter->next) {
		compose = (Compose *) iter->data;
		compose_update_actions_menu(compose);
	}

	gtk_widget_hide(actions.window);
	inc_unlock();
}

/*
 * Strings describing action format strings
 * 
 * When adding new lines, remember to put one string for each line
 */
static gchar *actions_desc_strings[] = {
	N_("MENU NAME:"), NULL,
	"      ",   N_("Use / in menu name to make submenus."),
	"", NULL,
	N_("COMMAND LINE:"), NULL,
	N_("Begin with:"), NULL,
	"     |",   N_("to send message body or selection to command's standard input"),
	"     >",   N_("to send user provided text to command's standard input"),
	"     *",   N_("to send user provided hidden text to command's standard input"),
	N_("End with:"), NULL, 
	"     |",   N_("to replace message body or selection with command's standard output"),
	"     >",   N_("to insert command's standard output without replacing old text"),
	"     &",   N_("to run command asynchronously"),
	N_("Use:"), NULL, 
	"     %f",  N_("for the file of the selected message in RFC822/2822 format "),
	"     %F",  N_("for the list of the files of the selected messages in RFC822/2822 format"),
	"     %p",  N_("for the file of the selected decoded message MIME part"),
	"     %u",  N_("for a user provided argument"),
	"     %h",  N_("for a user provided hidden argument (e.g. password)"),
	"     %s",  N_("for the text selection"),
	"  %as{}",  N_("apply filtering actions between {} to selected messages"),
	NULL
};


static DescriptionWindow actions_desc_win = { 
        NULL, 
        2,
        N_("Description of symbols"),
        actions_desc_strings
};


static void prefs_actions_help_cb(GtkWidget *w, gpointer data)
{
	description_window_create(&actions_desc_win);
}
