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
#include <signal.h>
#include <unistd.h>

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

#define WAIT_LAP 10000
						
typedef enum {
ACTION_NONE 		= 1 << 0,
ACTION_PIPE_IN  	= 1 << 1,
ACTION_PIPE_OUT 	= 1 << 2,
ACTION_SINGLE	 	= 1 << 3,
ACTION_MULTIPLE 	= 1 << 4,
ACTION_ASYNC 		= 1 << 5,
ACTION_OPEN_IN 		= 1 << 6,
ACTION_HIDE_IN 		= 1 << 7,
ACTION_ERROR 		= 1 << 8,
} ActionsTypes;

static struct Actions {
	GtkWidget *window;

	GtkWidget *ok_btn;

	GtkWidget *name_entry;
	GtkWidget *cmd_entry;

	GtkWidget *actions_clist;
} actions;

typedef struct _Children Children;
struct _Children {
	GtkWidget 	*window;
	GtkWidget 	*dialog;
	GtkWidget 	*text;
	GtkWidget 	*input_entry;
	GtkWidget 	*input_hbox;
	GtkWidget 	*abort_btn;
	GtkWidget 	*hide_btn;
	GtkWidget       *scrolledwin;

	gchar 		*action;
	guint		 timer;
	GSList 		*list;
	gint		 nb;
	gint		 open_in;
	gboolean	 output;
};

typedef struct _ChildInfo ChildInfo;
struct _ChildInfo {
	Children	*children;
	gchar 		*cmd;
	guint  		 type;
	pid_t 		 pid;
	gint 		 chld_in;
	gint		 chld_out;
	gint		 chld_err;
	gint		 chld_status;
	gint		 tag_in;
	gint		 tag_out;
	gint		 tag_err;
	gint		 tag_status;
	gint		 new_out;
	GString		*output;
	GtkWidget	*text;
};

/* widget creating functions */
static void prefs_actions_create	(MainWindow *mainwin);
static void prefs_actions_set_dialog	(void);
static gint prefs_actions_clist_set_row	(gint row);

/* callback functions */
static void prefs_actions_help_cb	(GtkWidget *w, gpointer data);
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
static guint get_action_type		(gchar *action);

static gboolean execute_actions		(gchar *action, 
					 GtkWidget *window,
					 GtkCTree *ctree, 
					 GtkWidget *text);

static gchar *parse_action_cmd		(gchar *action,
					 MsgInfo *msginfo,
					 GtkCTree *ctree);
static GString *parse_append_filename	(GString *cmd,
					 MsgInfo *msginfo);

ChildInfo *fork_child			(gchar *cmd,
		      			 gint action_type,
		      			 GtkWidget *text,
					 Children *children);

static gint wait_for_children		(gpointer data);

static void free_children		(Children *children);

static void childinfo_close_pipes	(ChildInfo *child_info);

static void create_io_dialog		(Children *children);

static void update_io_dialog		(Children *children);

static void hide_io_dialog_cb		(GtkWidget *widget,
					 gpointer data);

static void catch_output		(gpointer data,
					 gint source,
					 GdkInputCondition cond);
static void catch_input			(gpointer data, 
					 gint source,
					 GdkInputCondition cond);
static void catch_status		(gpointer data,
					 gint source,
					 GdkInputCondition cond);

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

	GtkWidget *reg_hbox;
	GtkWidget *btn_hbox;
	GtkWidget *arrow;
	GtkWidget *reg_btn;
	GtkWidget *subst_btn;
	GtkWidget *del_btn;

	GtkWidget *cond_hbox;
	GtkWidget *cond_scrolledwin;
	GtkWidget *cond_clist;

	GtkWidget *help_vbox;
	GtkWidget *help_label;
	GtkWidget *help_toggle;

	GtkWidget *btn_vbox;
	GtkWidget *up_btn;
	GtkWidget *down_btn;
	
	gchar *title[1];

	debug_print(_("Creating actions setting window...\n"));

	window = gtk_window_new (GTK_WINDOW_DIALOG);
		
	gtk_container_set_border_width (GTK_CONTAINER (window), 8);
	gtk_window_position (GTK_WINDOW (window), GTK_WIN_POS_CENTER);
	gtk_window_set_modal (GTK_WINDOW (window), TRUE);
	gtk_window_set_policy (GTK_WINDOW (window), FALSE, TRUE, TRUE);

	vbox = gtk_vbox_new (FALSE, 6);
	gtk_widget_show (vbox);
	gtk_container_add (GTK_CONTAINER (window), vbox);

	gtkut_button_set_create(&confirm_area, &ok_btn, _("OK"),
				&cancel_btn, _("Cancel"), NULL, NULL);
	gtk_widget_show (confirm_area);
	gtk_box_pack_end (GTK_BOX(vbox), confirm_area, FALSE, FALSE, 0);
	gtk_widget_grab_default (ok_btn);
					 
	gtk_window_set_title (GTK_WINDOW(window),
			      _("Actions setting"));
	gtk_signal_connect (GTK_OBJECT(window), "delete_event",
			    GTK_SIGNAL_FUNC(prefs_actions_deleted), NULL);
	gtk_signal_connect (GTK_OBJECT(window), "key_press_event",
			    GTK_SIGNAL_FUNC(prefs_actions_key_pressed), NULL);
	MANAGE_WINDOW_SIGNALS_CONNECT (window);
	gtk_signal_connect (GTK_OBJECT(ok_btn), "clicked",
			    GTK_SIGNAL_FUNC(prefs_actions_ok), mainwin);
	gtk_signal_connect (GTK_OBJECT(cancel_btn), "clicked",
			    GTK_SIGNAL_FUNC(prefs_actions_cancel), NULL);

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_box_pack_start (GTK_BOX (vbox), vbox1, TRUE, TRUE, 0);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), 2);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX(vbox1), hbox, TRUE, TRUE, 0);
	
	name_label = gtk_label_new(_("Menu name: "));
	gtk_widget_show(name_label);
	gtk_box_pack_start (GTK_BOX (hbox), name_label, FALSE, FALSE, 0);

	name_entry = gtk_entry_new();
	gtk_widget_show(name_entry);
	gtk_box_pack_start (GTK_BOX (hbox), name_entry, TRUE, TRUE, 0);

	gtk_widget_show_all(hbox);

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start (GTK_BOX(vbox1), hbox, TRUE, TRUE, 0);
		 
	cmd_label = gtk_label_new(_("Command line: "));
	gtk_box_pack_start (GTK_BOX (hbox), cmd_label, FALSE, FALSE, 0);

	cmd_entry = gtk_entry_new();
	gtk_box_pack_start (GTK_BOX (hbox), cmd_entry, TRUE, TRUE, 0);

	gtk_widget_show_all(hbox);

	help_vbox = gtk_vbox_new(FALSE, 8);
	gtk_box_pack_start (GTK_BOX (vbox1), help_vbox, TRUE, TRUE, 0);

	help_label = gtk_label_new (_("Menu name:\n"
				      " Use '/' in menu name to make submenus.\n"
				      "Command line:\n"
				      " Begin with:\n   '|' to send message "
				      "body or selection "
				      "to command\n   '>' to send user provided"
				      " text to command\n   '*' to send user "
				      "provided hidden text to command\n"
				      " End with:\n   '|' to replace "
				      "message body or selection with command "
				      "output\n   '&' to "
				      "run command asynchronously\n Use '%f' "
				      "for message file name\n and '%F' for the"
				      " list of the file names of selected "
				      "messages."));
	gtk_misc_set_alignment(GTK_MISC(help_label), 0, 0.5);
	gtk_label_set_justify (GTK_LABEL(help_label), GTK_JUSTIFY_LEFT);
	gtk_widget_show (help_label);
	gtk_box_pack_start (GTK_BOX (help_vbox), help_label, TRUE, TRUE, 0);
	gtk_widget_hide(help_vbox);

		/* register / substitute / delete */

	reg_hbox = gtk_hbox_new (FALSE, 4);
	gtk_widget_show (reg_hbox);
	gtk_box_pack_start (GTK_BOX (vbox1), reg_hbox, FALSE, FALSE, 0);

	arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_OUT);
	gtk_widget_show (arrow);
	gtk_box_pack_start (GTK_BOX (reg_hbox), arrow, FALSE, FALSE, 0);
	gtk_widget_set_usize (arrow, -1, 16);

	btn_hbox = gtk_hbox_new (FALSE, 4);
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

	help_toggle = gtk_toggle_button_new_with_label (_("Help on syntax"));
	gtk_widget_show (help_toggle);
	gtk_box_pack_end (GTK_BOX (btn_hbox), help_toggle, FALSE, TRUE, 0);
	gtk_signal_connect (GTK_OBJECT (help_toggle), "toggled",
			    GTK_SIGNAL_FUNC (prefs_actions_help_cb), help_vbox);

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

	gtk_widget_show(window);

	actions.window = window;
	actions.ok_btn = ok_btn;

	actions.name_entry = name_entry;
	actions.cmd_entry  = cmd_entry;
	
	actions.actions_clist = cond_clist;
}

static void prefs_actions_help_cb(GtkWidget *w, gpointer data)
{
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w)))
		gtk_widget_show(GTK_WIDGET(data));
	else
		gtk_widget_hide(GTK_WIDGET(data));
}

void prefs_actions_read_config()
{
	gchar *rcpath;
	FILE *fp;
	gchar buf[PREFSBUFSIZE];
	gchar *act;

	debug_print(_("Reading actions configurations...\n"));

	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, ACTIONS_RC, NULL);
	if ((fp = fopen(rcpath, "rb")) == NULL) {
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
		act = strstr(buf, ": ");
		if (act && act[2] && 
		    get_action_type(&act[2]) != ACTION_ERROR)
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

static guint get_action_type(gchar *action)
{
	gchar *p;
	guint action_type = ACTION_NONE;
	
	g_return_val_if_fail(action,  ACTION_ERROR);
	g_return_val_if_fail(*action, ACTION_ERROR);

	p = action;

	if (p[0] == '|') {
		action_type |= ACTION_PIPE_IN;
		p++;
	} else if (p[0] == '>') {
		action_type |= ACTION_OPEN_IN;
		p++;
	} else if (p[0] == '*') {
		action_type |= ACTION_HIDE_IN;
		p++;
	}

	if (p[0] == 0x00)
		return ACTION_ERROR;

	while (*p && action_type != ACTION_ERROR) {
		if (p[0] == '%') {
			switch (p[1]) {
				case 'f': action_type |= ACTION_SINGLE;
					  break;
				case 'F': action_type |= ACTION_MULTIPLE;
					  break;
				default:  action_type  = ACTION_ERROR;
					  break;
			}
		} else if (p[0] == '|') {
			if (p[1] == 0x00)
				action_type |= ACTION_PIPE_OUT;
			else
				action_type  = ACTION_ERROR;
		} else if (p[0] == '&') {
			if (p[1] == 0x00)
				action_type |= ACTION_ASYNC;
			else
				action_type  = ACTION_ERROR;
		}
		p++;
	}

	return action_type;
}

static gchar *parse_action_cmd		(gchar *action,
					 MsgInfo *msginfo,
					 GtkCTree *ctree)
{
	GString *cmd;
	gchar *p;
	GList *cur;
	MsgInfo *msg;
	
	p = action;
	
	if (p[0] == '|' || p[0] == '>' || p[0] == '*')
		p++;

	cmd = g_string_sized_new(strlen(action));

	while (p[0] && p[0] != '|' && p[0] != '&') {
		if (p[0] == '%' && p[1]) {
			switch (p[1]) {
			   case 'f': cmd = parse_append_filename(cmd, msginfo);
				     p++;	
				     break;
			   case 'F':
				     for (cur =GTK_CLIST(ctree)->selection;
				          cur; cur = cur->next) {
					msg = gtk_ctree_node_get_row_data(ctree,
					      GTK_CTREE_NODE(cur->data));
					cmd = parse_append_filename(cmd, msg);
					cmd = g_string_append_c(cmd, ' ');
				     }
				     p++;
				     break;
			   default: cmd = g_string_append_c(cmd, p[0]);
				    cmd = g_string_append_c(cmd, p[1]);
				    p++;
			}
		} else {
			cmd = g_string_append_c(cmd, p[0]);
		}
		p++;
	}
	if (!cmd->len) {
		g_string_free(cmd, TRUE);
		return NULL;
	}

	p = cmd->str;
	g_string_free(cmd, FALSE);
	return p;
}

static GString *parse_append_filename(GString *cmd, MsgInfo *msginfo)
{
	gchar *filename;

	g_return_val_if_fail(msginfo, cmd);

	filename = procmsg_get_message_file(msginfo);

	if (filename) {
		cmd = g_string_append(cmd, filename);
		g_free(filename);
	} 

	return cmd;
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

	if (get_action_type(entry_text) == ACTION_ERROR) {
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
	update_actions_menu(ifactory, "/Tools/Actions", 
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
	gchar *action, *action_p;
	GtkWidget *menu;
	
	GtkItemFactoryEntry ifentry = {
		NULL, NULL, NULL, 0, "<Branch>"};
	ifentry.path = branch_path;
	menuitem = gtk_item_factory_get_item(ifactory, branch_path);	
	g_return_if_fail(menuitem);
	/* FIXME: is there a better way to remove unknown submenu items? */
	/* Deleting and recreating the parent looses the translation */
	menu = GTK_WIDGET(GTK_MENU_ITEM(menuitem)->submenu);
	if (GTK_MENU_SHELL(menu)->children) {
		GList *amenu, *alist;
		for (amenu = (GTK_MENU_SHELL(menu)->children); amenu; ) {
			alist = amenu->next;
			gtk_widget_destroy(GTK_WIDGET(amenu->data));
			amenu = alist;
		}
	}

	ifentry.accelerator     = NULL;
	ifentry.callback_action = 0;
	ifentry.callback        = callback;
	ifentry.item_type       = NULL;

	for (cur = prefs_common.actionslst; cur; cur = cur->next) {
		action   = g_strdup((gchar *) cur->data);
		action_p = strstr(action, ": ");
		if (action_p && action_p[2] &&
		    get_action_type(&action_p[2]) != ACTION_ERROR) {
			action_p[0] = 0x00;
			menu_path = g_strdup_printf("%s/%s", branch_path, 
						    action);
			ifentry.path = menu_path;
			gtk_item_factory_create_item(ifactory, &ifentry, data,
						     1);
			g_free(menu_path);
		}
		g_free(action);
		ifentry.callback_action++;	
	}
}

static void compose_actions_execute_cb	(Compose	*compose,
					 guint		 action_nb,
					 GtkWidget	*widget)
{
	gchar *buf, *action;
	guint action_type;	

	g_return_if_fail(action_nb < g_slist_length(prefs_common.actionslst));
	
	buf = (gchar *) g_slist_nth_data(prefs_common.actionslst, action_nb);
	
	g_return_if_fail(buf);
	g_return_if_fail(action = strstr(buf, ": "));

	/* Point to the beginning of the command-line */
	action++;
	action++;
	
	action_type = get_action_type(action);
	if (action_type & ACTION_MULTIPLE ||
	    action_type & ACTION_SINGLE   ) {		
		alertpanel_warning(_("The selected action is not a pipe "
				     "action.\n You can only use pipe actions "
				     "when composing a message."));
		return;
	}

	execute_actions(action, compose->window, NULL, compose->text);
}

static void mainwin_actions_execute_cb 	(MainWindow	*mainwin,
				 	 guint 	 	 action_nb,
				 	 GtkWidget 	*widget)
{
	MessageView *messageview = mainwin->messageview;
	GtkWidget   *text = NULL;
	gchar 	    *buf,
		    *action;
	
	g_return_if_fail (action_nb < g_slist_length(prefs_common.actionslst));

	buf = (gchar *) g_slist_nth_data(prefs_common.actionslst, action_nb);
	
	g_return_if_fail(buf);
	g_return_if_fail(action = strstr(buf, ": "));

	/* Point to the beginning of the command-line */
	action++;
	action++;

	switch (messageview->type) {
		case MVIEW_TEXT:
			if (messageview->textview && 
			    messageview->textview->text)
				text = messageview->textview->text;
			break;
		case MVIEW_MIME:
			if (messageview->mimeview &&
			    messageview->mimeview->type == MIMEVIEW_TEXT &&
			    messageview->mimeview->textview &&
			    messageview->mimeview->textview->text)
				text = messageview->mimeview->textview->text;
			break;
	}

	execute_actions(action, mainwin->window,
			GTK_CTREE(mainwin->summaryview->ctree), text);
}

static gboolean execute_actions(gchar *action, 
				GtkWidget *window,
				GtkCTree *ctree, 
				GtkWidget *text)
{
	GList *cur, *selection;
	GSList *children_list = NULL;
	gint is_ok  = TRUE;
	gint selection_len;
	Children *children;
	ChildInfo *child_info;
	gint action_type;
	MsgInfo *msginfo;
	gchar *cmd;

	g_return_val_if_fail(action && *action, FALSE);

	action_type = get_action_type(action);

	if (action_type == ACTION_ERROR)
		return FALSE;         /* ERR: syntax error */

	if (action_type & (ACTION_SINGLE | ACTION_MULTIPLE) && 
	    !(ctree && GTK_CLIST(ctree)->selection))
		return FALSE;         /* ERR: file command without selection */

	if (ctree) {
		selection = GTK_CLIST(ctree)->selection;
		selection_len = g_list_length(selection);
	}
	
	if ((action_type & (ACTION_PIPE_OUT | ACTION_PIPE_IN))) {
		if (ctree && selection_len > 1)
			return FALSE; /* ERR: pipe + multiple selection */
		if (!text)
			return FALSE; /* ERR: pipe and no displayed text */
	}

	if (!(action_type & ACTION_ASYNC) && window) {
		gtk_widget_set_sensitive(window, FALSE);
	}

	children = g_new0(Children, 1);

	if (action_type & ACTION_SINGLE) {
		for (cur = selection; cur && is_ok == TRUE; cur = cur->next) {
			msginfo = gtk_ctree_node_get_row_data(ctree,
					GTK_CTREE_NODE(cur->data));
			if (!msginfo) {
				is_ok  = FALSE; /* ERR: msginfo missing */
				break;
			}
			cmd = parse_action_cmd(action, msginfo, ctree);
			if (!cmd) {
				debug_print(_("Action command error\n"));
				is_ok  = FALSE; /* ERR: incorrect command */
				break;
			}
			if ((child_info = fork_child(cmd, action_type, text,
						     children))) {
				children_list = g_slist_append(children_list,
							       child_info);
				children->open_in = (selection_len == 1) ? 
					            (action_type & 
						     (ACTION_OPEN_IN |
						      ACTION_HIDE_IN)) : 0;
			}
			g_free(cmd);
		}
	} else {
		cmd = parse_action_cmd(action, NULL, ctree);
		if (cmd) {
			if ((child_info = fork_child(cmd, action_type, text,
						     children))) {
				children_list = g_slist_append(children_list,
							       child_info);
				children->open_in = action_type &
						    (ACTION_OPEN_IN |
						     ACTION_HIDE_IN);
			}
			g_free(cmd);
		} else
			is_ok  = FALSE;         /* ERR: incorrect command */
	}

	if (!children_list) {
		 /* If not waiting for children, return */
		g_free(children);
		if (!(action_type & ACTION_ASYNC) && window) {
			gtk_widget_set_sensitive(window, TRUE);
		}
	} else {
		GSList *cur;
		
		children->action  = g_strdup(action);
		children->window  = window;
		children->dialog  = NULL;
		children->list    = children_list;
		children->nb	  = g_slist_length(children_list);

		for (cur = children_list; cur; cur = cur->next) {
			child_info = (ChildInfo *) cur->data;
			child_info->tag_status = 
				gdk_input_add(child_info->chld_status,
					      GDK_INPUT_READ,
					      catch_status, child_info);
		}
		children->timer = children->open_in ? 0 :
				  gtk_timeout_add(WAIT_LAP, wait_for_children,
						  children);
	}
	if (children->open_in)
		create_io_dialog(children);

	return is_ok ;
}

ChildInfo *fork_child(gchar *cmd,
		      gint action_type,
		      GtkWidget *text,
		      Children *children)
{
	gint chld_in[2], chld_out[2], chld_err[2], chld_status[2];
	gchar **cmdline;
	gint start, end, is_selection;
	gchar *selection;
	pid_t pid_c, pid_gc;
	ChildInfo *child_info;

	if (action_type & ACTION_ASYNC) {
		execute_command_line(cmd, TRUE);
		return NULL; /* Asynchronous command */
	}

	if (pipe(chld_in) || pipe(chld_out) || pipe(chld_err) ||
	    pipe(chld_status)) {
		alertpanel_error(_("Command could not started. Pipe creation"
				   " failed.\n%s"), g_strerror(errno));
		return NULL; /* Pipe error */
	}
	
	debug_print(_("Forking child and grandchild.\n"));

	pid_c = fork();
	if (pid_c == (pid_t) 0) {/* Child */
		if (setpgid(0, 0))
			perror("setpgid");

		close(ConnectionNumber(gdk_display));

		pid_gc = fork();

		if (pid_gc == 0) {
			if (setpgid(0, getppid()))
				perror("setpgid");
			if (action_type & 
			   (ACTION_PIPE_IN | ACTION_OPEN_IN | ACTION_HIDE_IN)) {
			close(fileno(stdin));
			dup  (chld_in[0]);
			}
			close(chld_in[0]);
			close(chld_in[1]);

			close(fileno(stdout));
			dup  (chld_out[1]);
			close(chld_out[0]);
			close(chld_out[1]);

			close(fileno(stderr));
			dup  (chld_err[1]);
			close(chld_err[0]);
			close(chld_err[1]);

			cmdline = strsplit_with_quote(cmd, " ", 1024);

			execvp(cmdline[0], cmdline);
			perror("execvp");
			g_strfreev(cmdline);

			_exit(1);
		} else if (pid_gc < (pid_t) 0) {/* Fork erro */
			write(chld_status[1], "1\n", 2);
			perror("fork");
			_exit(1);
		} else {/* Child */
			close(chld_in[0]);
			close(chld_in[1]);
			close(chld_out[0]);
			close(chld_out[1]);
			close(chld_err[0]);
			close(chld_err[1]);

			close(chld_status[0]);
			debug_print(_("Child: Waiting for grandchild\n"));
			waitpid(pid_gc, NULL, 0);
			debug_print(_("Child: grandchild ended\n"));
			write(chld_status[1], "0\n", 2);
			close(chld_status[1]);
			_exit(0);
		}
	} else if (pid_c < (pid_t) 0) {/* Fork error */
		alertpanel_error(_("Could not fork to execute the following "
				   "command:\n%s\n%s"), 
				 cmd, g_strerror(errno));
		return NULL; 
	}
	/* Parent */

	close(chld_in[0]);
	if (!(action_type & (ACTION_PIPE_IN | ACTION_OPEN_IN | ACTION_HIDE_IN)))
		close(chld_in[1]);
	close(chld_out[1]);
	close(chld_err[1]);
	close(chld_status[1]);

	child_info = g_new0(ChildInfo, 1);

	child_info->children = children;
	
	child_info->pid      = pid_c;
	child_info->cmd      = g_strdup(cmd);
	child_info->type     = action_type;
	child_info->new_out  = FALSE;
	child_info->output   = g_string_sized_new(0);
	child_info->chld_in  = (action_type & (ACTION_PIPE_IN | ACTION_OPEN_IN
				| ACTION_HIDE_IN))
			       ? chld_in [1] : -1;
	child_info->chld_out = chld_out[0];
	child_info->chld_err = chld_err[0];
	child_info->chld_status = chld_status[0];
	child_info->tag_in   = -1;
	child_info->tag_out  = gdk_input_add(chld_out[0], GDK_INPUT_READ,
					     catch_output, child_info);
	child_info->tag_err  = gdk_input_add(chld_err[0], GDK_INPUT_READ,
					     catch_output, child_info);

	if (!(action_type & (ACTION_PIPE_IN | ACTION_PIPE_OUT)))
		return child_info;

	child_info->text     = text;

	start = 0;
	end   = gtk_stext_get_length(GTK_STEXT(text));

	if (GTK_EDITABLE(text)->has_selection) {
		start = GTK_EDITABLE(text)->selection_start_pos;
		end   = GTK_EDITABLE(text)->selection_end_pos;
		if (start > end) {
			gint tmp;
			tmp = start;
			start = end;
			end = tmp;
		}
		is_selection = TRUE;
		if (start == end) {
			start = 0;
			end   = gtk_stext_get_length(GTK_STEXT(text));
			is_selection = FALSE;
		}
	}

	selection = gtk_editable_get_chars(GTK_EDITABLE(text), start, end);


	if (action_type & ACTION_PIPE_IN) {
		write(chld_in[1], selection, strlen(selection));
		if (!(action_type & (ACTION_OPEN_IN | ACTION_HIDE_IN)))
			close(chld_in[1]);
		child_info->chld_in = -1; /* No more input */
	}
	g_free(selection);

	gtk_stext_freeze(GTK_STEXT(text));
	if (action_type & ACTION_PIPE_OUT) {
		gtk_stext_set_point(GTK_STEXT(text), start);
		gtk_stext_forward_delete(GTK_STEXT(text), end - start);
	}

	gtk_stext_thaw(GTK_STEXT(text));
	
	return child_info;
}
	
static void kill_children_cb(GtkWidget *widget, gpointer data)
{
	GSList *cur;
	Children *children = (Children *) data;
	ChildInfo *child_info;

	for (cur = children->list; cur; cur = cur->next) {
		child_info = (ChildInfo *)(cur->data);
		debug_print(_("Killing child group id %d\n"), child_info->pid);
		if (child_info->pid && (kill(-child_info->pid, SIGTERM) < 0))
				perror("kill");
	}
}

static gint wait_for_children(gpointer data)
{
	gboolean new_output;
	Children *children = (Children *) data;
	ChildInfo *child_info;
	GSList *cur;
	gint nb = children->nb;

	children->nb = 0;
		
	cur = children->list;
	new_output = FALSE;
	while (cur) {
		child_info = (ChildInfo *) cur->data;
		if (child_info->pid)
			children->nb++;
		new_output |= child_info->new_out;
		cur = cur->next;
	}

	if (!children->dialog && 
	    (new_output || children->timer))
		create_io_dialog(children);
	
	if (children->timer) {
		gtk_timeout_remove(children->timer);
		children->timer = 0;
	}
	children->output |= new_output;

	if (new_output || (children->dialog && (nb != children->nb)))
		update_io_dialog(children);

	if (children->nb)
		return FALSE;

	if (!children->dialog) {
		gtk_widget_set_sensitive(children->window, TRUE);
		free_children(children);
	} else if (!children->output) {
		gtk_widget_destroy(children->dialog);
	}
	return FALSE;
}

static void send_input(GtkWidget *w, gpointer data)
{
	Children *children = (Children *) data;
	ChildInfo *child_info = (ChildInfo *) children->list->data;
	
	child_info->tag_in = gdk_input_add(child_info->chld_in,
					   GDK_INPUT_WRITE,
					   catch_input, children);
	gtk_widget_set_sensitive(children->input_hbox, FALSE);
}

static gint delete_io_dialog_cb(GtkWidget *w, GdkEvent *e, gpointer data)
{
	hide_io_dialog_cb(w, data);
	return TRUE;
}

static void hide_io_dialog_cb(GtkWidget *w, gpointer data)
{

	Children *children = (Children *) data;

	if (!children->nb) {
		gtk_widget_set_sensitive(children->window, TRUE);
		gtk_signal_disconnect_by_data(GTK_OBJECT(children->dialog),
					      children);
		gtk_widget_destroy(children->dialog);
		free_children(children);
	}
}

static void childinfo_close_pipes(ChildInfo *child_info)
{
	if (child_info->tag_in > 0)
		gdk_input_remove(child_info->tag_in);
	gdk_input_remove(child_info->tag_out);
	gdk_input_remove(child_info->tag_err);

	if (child_info->chld_in >= 0)
		close(child_info->chld_in);
	close(child_info->chld_out);
	close(child_info->chld_err);
	close(child_info->chld_status);
}

static void free_children(Children *children)
{
	GSList *cur;
	ChildInfo *child_info;

	debug_print(_("Freeing children data %x\n"), (guint) children);
				
	g_free(children->action);
	for (cur = children->list; cur;) {
		child_info = (ChildInfo *) cur->data;
		g_free(child_info->cmd);
		g_string_free(child_info->output, TRUE);
		children->list = g_slist_remove(children->list, child_info);
		g_free(child_info);
		cur = children->list;
	}
	g_free(children);
}	

static void update_io_dialog(Children *children)
{
	GSList *cur;

	debug_print(_("Updating actions input/output dialog.\n"));

	if (!children->nb) {
		gtk_widget_set_sensitive(children->abort_btn, FALSE);
		gtk_widget_set_sensitive(children->hide_btn, TRUE);
		gtk_widget_set_sensitive(children->window, TRUE);
		if (children->input_hbox)
			gtk_widget_set_sensitive(children->input_hbox, FALSE);
	}

	if (children->output) {
		GtkWidget *text = children->text;
		gchar *caption;
		ChildInfo *child_info;

		gtk_widget_show(children->scrolledwin);
		gtk_text_freeze(GTK_TEXT(text));
		gtk_text_set_point(GTK_TEXT(text), 0);
		gtk_text_forward_delete(GTK_TEXT(text), 
					gtk_text_get_length(GTK_TEXT(text)));
		for (cur = children->list; cur; cur = cur->next)
		{
			child_info = (ChildInfo *) cur->data;
			if (child_info->pid)
				caption = g_strdup_printf(_("--- Running: %s\n")
						    , child_info->cmd);
			else
				caption = g_strdup_printf(_("--- Ended: %s\n")
						    , child_info->cmd);
			
			gtk_text_insert(GTK_TEXT(text), NULL, NULL, NULL,
					 caption, -1);
			gtk_text_insert(GTK_TEXT(text), NULL, NULL, NULL,
					 child_info->output->str, -1);
			g_free(caption);
			child_info->new_out = FALSE;
		}
		gtk_text_thaw(GTK_TEXT(text));
	}
}
	
	
static void create_io_dialog(Children *children)
{
	GtkWidget *dialog;
	GtkWidget *entry;
	GtkWidget *input_hbox;
	GtkWidget *send_button;
	GtkWidget *label;
	GtkWidget *text;
	GtkWidget *scrolledwin;
	GtkWidget *hbox;
	GtkWidget *abort_button;
	GtkWidget *hide_button;
	
	debug_print(_("Creating actions dialog\n"));

	dialog = gtk_dialog_new();
	label = gtk_label_new(children->action);
	gtk_misc_set_padding(GTK_MISC(label), 8, 8);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), label, FALSE, 
			   FALSE, 0);

	gtk_window_set_title(GTK_WINDOW(dialog), _("Actions' input/output"));
	gtk_signal_connect(GTK_OBJECT(dialog), "delete_event",
			GTK_SIGNAL_FUNC(delete_io_dialog_cb), children);
	gtk_signal_connect(GTK_OBJECT(dialog), "destroy",
			GTK_SIGNAL_FUNC(hide_io_dialog_cb),
			children);

	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
				       GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), scrolledwin, TRUE,
			   TRUE, 0);
	gtk_widget_set_usize(scrolledwin, 600, 200);
	gtk_widget_hide(scrolledwin);

	text = gtk_text_new(gtk_scrolled_window_get_hadjustment
			    (GTK_SCROLLED_WINDOW(scrolledwin)),
			    gtk_scrolled_window_get_vadjustment
			    (GTK_SCROLLED_WINDOW(scrolledwin)));
	gtk_text_set_editable(GTK_TEXT(text), FALSE);
	gtk_container_add(GTK_CONTAINER(scrolledwin), text);
	gtk_widget_show(text);

	if (children->open_in) {
		input_hbox = gtk_hbox_new(FALSE, 8);
		gtk_widget_show(input_hbox);

		entry = gtk_entry_new();
		gtk_signal_connect(GTK_OBJECT(entry), "activate",
				   GTK_SIGNAL_FUNC(send_input), children);
		gtk_box_pack_start(GTK_BOX(input_hbox), entry, TRUE, TRUE, 8);
		if (children->open_in & ACTION_HIDE_IN)
			gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);
		gtk_widget_show(entry);

		send_button = gtk_button_new_with_label(_("Send"));
		gtk_signal_connect(GTK_OBJECT(send_button), "clicked",
				   GTK_SIGNAL_FUNC(send_input), children);
		gtk_box_pack_start(GTK_BOX(input_hbox), send_button, FALSE, 
				   FALSE, 8);
		gtk_widget_show(send_button);

		gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), 
				input_hbox, FALSE, FALSE, 8);
		gtk_widget_grab_focus(entry);
	}

	hbox = gtk_hbox_new(TRUE, 0);
	gtk_widget_show(hbox);

	abort_button = gtk_button_new_with_label(_("Abort actions"));
	gtk_signal_connect(GTK_OBJECT(abort_button), "clicked",
			GTK_SIGNAL_FUNC(kill_children_cb), children);
	gtk_box_pack_start(GTK_BOX(hbox), abort_button, TRUE, TRUE, 8);
	gtk_widget_show(abort_button);

	hide_button = gtk_button_new_with_label(_("Close window"));
	gtk_signal_connect(GTK_OBJECT(hide_button), "clicked",
			GTK_SIGNAL_FUNC(hide_io_dialog_cb), children);
	gtk_box_pack_start(GTK_BOX(hbox), hide_button, TRUE, TRUE, 8);
	gtk_widget_show(hide_button);

	if (children->nb)
		gtk_widget_set_sensitive(hide_button, FALSE);

	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area), 
			  hbox);

	children->dialog      = dialog;
	children->scrolledwin = scrolledwin;
	children->text        = text;
	children->input_hbox  = children->open_in ? input_hbox : NULL;
	children->input_entry = children->open_in ? entry : NULL;
	children->abort_btn   = abort_button;
	children->hide_btn    = hide_button;

	gtk_widget_show(dialog);
}

static void catch_status(gpointer data, gint source, GdkInputCondition cond)
{
	ChildInfo *child_info = (ChildInfo *) data;
	gchar buf;
	gint c;

	gdk_input_remove(child_info->tag_status);

	c = read(source, &buf, 1);
	debug_print(_("Child returned %c\n"), buf);

	waitpid(-child_info->pid, NULL, 0);
	childinfo_close_pipes(child_info);
	child_info->pid = 0;
	
	wait_for_children(child_info->children);
}
	
static void catch_input(gpointer data, gint source, GdkInputCondition cond)
{
	Children *children = (Children *) data;
	ChildInfo *child_info = (ChildInfo *) children->list->data;
	gchar *input;
	gint c;

	debug_print(_("Sending input to grand child.\n"));
	if (!(cond && GDK_INPUT_WRITE))
		return;
	
	gdk_input_remove(child_info->tag_in);
	child_info->tag_in = -1;

	input = gtk_editable_get_chars(GTK_EDITABLE(children->input_entry), 0,
				       -1);
	c = write(child_info->chld_in, input, strlen(input));

	g_free(input);

	write(child_info->chld_in, "\n", 2);

	gtk_entry_set_text(GTK_ENTRY(children->input_entry), "");
	gtk_widget_set_sensitive(children->input_hbox, TRUE);
	debug_print(_("Input to grand child sent.\n"));
}

static void catch_output(gpointer data, gint source, GdkInputCondition cond)
{
	ChildInfo *child_info = (ChildInfo *) data;
	gint c, i;
	gchar buf[PREFSBUFSIZE];

	debug_print(_("Catching grand child's output.\n"));
	if (child_info->type & ACTION_PIPE_OUT
	    && source == child_info->chld_out) {
		gboolean is_selection = FALSE;
		GtkWidget *text = child_info->text;
		if (GTK_EDITABLE(text)->has_selection)
			is_selection = TRUE;
		gtk_stext_freeze(GTK_STEXT(text));
		while (TRUE) {
			c =read(source, buf, PREFSBUFSIZE - 1);
			if (c == 0)
				break;
			gtk_stext_insert(GTK_STEXT(text), NULL, NULL, NULL,
					 buf, c);
		}
		if (is_selection) {
			/* Using the select_region draws things. Should not.
			 * so we just change selection position and 
			 * defere drawing when thawing. Hack?
			 */
		  	GTK_EDITABLE(text)->selection_end_pos = 
					gtk_stext_get_point(GTK_STEXT(text));
		}
		gtk_stext_thaw(GTK_STEXT(child_info->text));
	} else {
		while (TRUE) {
			c = read(source, buf, PREFSBUFSIZE - 1);
			if (c == 0) 
				break;
			for (i = 0; i < c; i++)
				child_info->output = g_string_append_c(
						child_info->output, buf[i]);
			child_info->new_out = TRUE;
		}
	}
}
