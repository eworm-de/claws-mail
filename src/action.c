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
#include "utils.h"
#include "gtkutils.h"
#include "manage_window.h"
#include "mainwindow.h"
#include "prefs_common.h"
#include "alertpanel.h"
#include "inputdialog.h"
#include "action.h"
#include "compose.h"
#include "procmsg.h"
#include "gtkstext.h"
#include "textview.h"

typedef struct _Children		Children;
typedef struct _ChildInfo		ChildInfo;
typedef struct _UserStringDialog	UserStringDialog;

struct _Children
{
	GtkWidget	*dialog;
	GtkWidget	*text;
	GtkWidget	*input_entry;
	GtkWidget	*input_hbox;
	GtkWidget	*abort_btn;
	GtkWidget	*close_btn;
	GtkWidget	*scrolledwin;

	gchar		*action;
	GSList		*list;
	gint		 nb;
	gint		 open_in;
	gboolean	 output;

	GtkWidget	*msg_text;
	GdkFont		*msgfont;
};

struct _ChildInfo
{
	Children	*children;
	gchar		*cmd;
	guint		 type;
	pid_t		 pid;
	gint		 chld_in;
	gint		 chld_out;
	gint		 chld_err;
	gint		 chld_status;
	gint		 tag_in;
	gint		 tag_out;
	gint		 tag_err;
	gint		 tag_status;
	gint		 new_out;

	GString		*output;
};

static void action_update_menu		(GtkItemFactory	*ifactory,
					 gchar		*branch_path,
					 gpointer	 callback,
					 gpointer	 data);
static void compose_actions_execute_cb	(Compose	*compose,
					 guint		 action_nb,
					 GtkWidget	*widget);
static void mainwin_actions_execute_cb 	(MainWindow	*mainwin,
					 guint		 action_nb,
					 GtkWidget 	*widget);
static void msgview_actions_execute_cb	(MessageView	*msgview,
					 guint		 action_nb,
					 GtkWidget	*widget);
static void message_actions_execute	(MessageView	*msgview,
					 guint		 action_nb,
					 GSList		*msg_list);

static gboolean execute_actions		(gchar		*action, 
					 GSList		*msg_list, 
					 GtkWidget	*text,
					 GdkFont	*msgfont,
					 gint		 body_pos,
					 MimeInfo	*partinfo);

static gchar *parse_action_cmd		(gchar		*action,
					 MsgInfo	*msginfo,
					 GSList		*msg_list,
					 MimeInfo	*partinfo,
					 const gchar	*user_str,
					 const gchar	*user_hidden_str,
					 const gchar	*sel_str);
static gboolean parse_append_filename	(GString	*cmd,
					 MsgInfo	*msginfo);

static gboolean parse_append_msgpart	(GString	*cmd,
					 MsgInfo	*msginfo,
					 MimeInfo	*partinfo);

static ChildInfo *fork_child		(gchar		*cmd,
					 gint		 action_type,
					 const gchar	*msg_str,
					 Children	*children);

static gint wait_for_children		(Children	*children);

static void free_children		(Children	*children);

static void childinfo_close_pipes	(ChildInfo	*child_info);

static void create_io_dialog		(Children	*children);
static void update_io_dialog		(Children	*children);

static void hide_io_dialog_cb		(GtkWidget	*widget,
					 gpointer	 data);
static gint io_dialog_key_pressed_cb	(GtkWidget	*widget,
					 GdkEventKey	*event,
					 gpointer	 data);

static void catch_output		(gpointer		 data,
					 gint			 source,
					 GdkInputCondition	 cond);
static void catch_input			(gpointer		 data, 
					 gint			 source,
					 GdkInputCondition	 cond);
static void catch_status		(gpointer		 data,
					 gint			 source,
					 GdkInputCondition	 cond);

static gchar *get_user_string		(const gchar	*action,
					 ActionType	 type);


ActionType action_get_type(const gchar *action_str)
{
	const gchar *p;
	ActionType action_type = ACTION_NONE;

	g_return_val_if_fail(action_str,  ACTION_ERROR);
	g_return_val_if_fail(*action_str, ACTION_ERROR);

	p = action_str;

	if (p[0] == '|') {
		action_type |= ACTION_PIPE_IN;
		p++;
	} else if (p[0] == '>') {
		action_type |= ACTION_USER_IN;
		p++;
	} else if (p[0] == '*') {
		action_type |= ACTION_USER_HIDDEN_IN;
		p++;
	}

	if (p[0] == '\0')
		return ACTION_ERROR;

	while (*p && action_type != ACTION_ERROR) {
		if (p[0] == '%') {
			switch (p[1]) {
			case 'f':
				action_type |= ACTION_SINGLE;
				break;
			case 'F':
				action_type |= ACTION_MULTIPLE;
				break;
			case 'p':
				action_type |= ACTION_SINGLE;
				break;
			case 's':
				action_type |= ACTION_SELECTION_STR;
				break;
			case 'u':
				action_type |= ACTION_USER_STR;
				break;
			case 'h':
				action_type |= ACTION_USER_HIDDEN_STR;
				break;
			default:
				action_type = ACTION_ERROR;
				break;
			}
		} else if (p[0] == '|') {
			if (p[1] == '\0')
				action_type |= ACTION_PIPE_OUT;
		} else if (p[0] == '>') {
			if (p[1] == '\0')
				action_type |= ACTION_INSERT;
		} else if (p[0] == '&') {
			if (p[1] == '\0')
				action_type |= ACTION_ASYNC;
		}
		p++;
	}

	return action_type;
}

static gchar *parse_action_cmd(gchar *action, MsgInfo *msginfo,
			       GSList *msg_list, MimeInfo *partinfo,
			       const gchar *user_str,
			       const gchar *user_hidden_str,
			       const gchar *sel_str)
{
	GString *cmd;
	gchar *p;
	GSList *cur;
	
	p = action;
	
	if (p[0] == '|' || p[0] == '>' || p[0] == '*')
		p++;

	cmd = g_string_sized_new(strlen(action));

	while (p[0] &&
	       !((p[0] == '|' || p[0] == '>' || p[0] == '&') && !p[1])) {
		if (p[0] == '%' && p[1]) {
			switch (p[1]) {
			case 'f':
				if (!parse_append_filename(cmd, msginfo)) {
					g_string_free(cmd, TRUE);
					return NULL;
				}
				p++;
				break;
			case 'F':
				for (cur = msg_list; cur != NULL;
				     cur = cur->next) {
					MsgInfo *msg = (MsgInfo *)cur->data;

					if (!parse_append_filename(cmd, msg)) {
						g_string_free(cmd, TRUE);
						return NULL;
					}
					if (cur->next)
						g_string_append_c(cmd, ' ');
				}
				p++;
				break;
			case 'p':
				if (!parse_append_msgpart(cmd, msginfo,
							  partinfo)) {
					g_string_free(cmd, TRUE);
					return NULL;
				}
				p++;
				break;
			case 's':
				if (sel_str)
					g_string_append(cmd, sel_str);
				p++;
				break;
			case 'u':
				if (user_str)
					g_string_append(cmd, user_str);
				p++;
				break;
			case 'h':
				if (user_hidden_str)
					g_string_append(cmd, user_hidden_str);
				p++;
				break;
			default:
				g_string_append_c(cmd, p[0]);
				g_string_append_c(cmd, p[1]);
				p++;
			}
		} else {
			g_string_append_c(cmd, p[0]);
		}
		p++;
	}
	if (cmd->len == 0) {
		g_string_free(cmd, TRUE);
		return NULL;
	}

	p = cmd->str;
	g_string_free(cmd, FALSE);
	return p;
}

static gboolean parse_append_filename(GString *cmd, MsgInfo *msginfo)
{
	gchar *filename;

	g_return_val_if_fail(msginfo, FALSE);

	filename = procmsg_get_message_file(msginfo);

	if (filename) {
		g_string_append(cmd, filename);
		g_free(filename);
	} else {
		alertpanel_error(_("Could not get message file %d"),
				 msginfo->msgnum);
		return FALSE;
	}

	return TRUE;
}

static gboolean parse_append_msgpart(GString *cmd, MsgInfo *msginfo,
				     MimeInfo *partinfo)
{
	gboolean single_part = FALSE;
	gchar *filename;
	gchar *part_filename;
	gint ret;

	if (!partinfo) {
		FILE *fp;
#if USE_GPGME
		if ((fp = procmsg_open_message_decrypted(msginfo, &partinfo))
		    == NULL) {
			alertpanel_error(_("Could not get message file."));
			return FALSE;
		}
#else
		if ((fp = procmsg_open_message(msginfo)) == NULL) {
			alertpanel_error(_("Could not get message file."));
			return FALSE;
		}
		partinfo = procmime_scan_mime_header(fp);
#endif
		fclose(fp);
		if (!partinfo) {
			procmime_mimeinfo_free_all(partinfo);
			alertpanel_error(_("Could not get message part."));
			return FALSE;
		}

		single_part = TRUE;
	}

	filename = procmsg_get_message_file_path(msginfo);
	part_filename = procmime_get_tmp_file_name(partinfo);

	ret = procmime_get_part(part_filename, filename, partinfo); 

	if (single_part)
		procmime_mimeinfo_free_all(partinfo);
	g_free(filename);

	if (ret < 0) {
		alertpanel_error(_("Can't get part of multipart message"));
		g_free(part_filename);
		return FALSE;
	}

	g_string_append(cmd, part_filename);

	g_free(part_filename);

	return TRUE;
}

void actions_execute(gpointer data, 
		     guint action_nb,
		     GtkWidget *widget,
		     gint source)
{
	if (source == TOOLBAR_MAIN) 
		mainwin_actions_execute_cb((MainWindow*)data, action_nb, widget);
	else if (source == TOOLBAR_COMPOSE)
		compose_actions_execute_cb((Compose*)data, action_nb, widget);
	else if (source == TOOLBAR_MSGVIEW)
		msgview_actions_execute_cb((MessageView*)data, action_nb, widget);	
}

void action_update_mainwin_menu(GtkItemFactory *ifactory, MainWindow *mainwin)
{
	action_update_menu(ifactory, "/Tools/Actions",
			   mainwin_actions_execute_cb, mainwin);
}

void action_update_msgview_menu(GtkItemFactory *ifactory, MessageView *msgview)
{
	action_update_menu(ifactory, "/Tools/Actions",
			   msgview_actions_execute_cb, msgview);
}

void action_update_compose_menu(GtkItemFactory *ifactory, Compose *compose)
{
	action_update_menu(ifactory, "/Tools/Actions",
			   compose_actions_execute_cb, compose);
}

static void action_update_menu(GtkItemFactory *ifactory, gchar *branch_path,
			       gpointer callback, gpointer data)
{
	GtkWidget *menuitem;
	gchar *menu_path;
	GSList *cur;
	gchar *action, *action_p;
	GList *amenu;
	GtkItemFactoryEntry ifentry = {NULL, NULL, NULL, 0, "<Branch>"};

	ifentry.path = branch_path;
	menuitem = gtk_item_factory_get_widget(ifactory, branch_path);
	g_return_if_fail(menuitem != NULL);

	amenu = GTK_MENU_SHELL(menuitem)->children;
	while (amenu != NULL) {
		GList *alist = amenu->next;
		gtk_widget_destroy(GTK_WIDGET(amenu->data));
		amenu = alist;
	}

	ifentry.accelerator     = NULL;
	ifentry.callback_action = 0;
	ifentry.callback        = callback;
	ifentry.item_type       = NULL;

	for (cur = prefs_common.actions_list; cur; cur = cur->next) {
		action   = g_strdup((gchar *)cur->data);
		action_p = strstr(action, ": ");
		if (action_p && action_p[2] &&
		    action_get_type(&action_p[2]) != ACTION_ERROR) {
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

static void compose_actions_execute_cb(Compose *compose, guint action_nb,
				       GtkWidget *widget)
{
	gchar *buf, *action;
	guint action_type;

	g_return_if_fail(action_nb < g_slist_length(prefs_common.actions_list));

	buf = (gchar *)g_slist_nth_data(prefs_common.actions_list, action_nb);
	g_return_if_fail(buf != NULL);
	action = strstr(buf, ": ");
	g_return_if_fail(action != NULL);

	/* Point to the beginning of the command-line */
	action += 2;

	action_type = action_get_type(action);
	if (action_type & (ACTION_SINGLE | ACTION_MULTIPLE)) {
		alertpanel_warning
			(_("The selected action cannot be used in the compose window\n"
			   "because it contains %%f, %%F or %%p."));
		return;
	}

	execute_actions(action, NULL, compose->text, NULL, 0, NULL);
}

static void mainwin_actions_execute_cb(MainWindow *mainwin, guint action_nb,
				       GtkWidget *widget)
{
	GSList *msg_list;

	msg_list = summary_get_selected_msg_list(mainwin->summaryview);
	message_actions_execute(mainwin->messageview, action_nb, msg_list);
	g_slist_free(msg_list);
}

static void msgview_actions_execute_cb(MessageView *msgview, guint action_nb,
				       GtkWidget *widget)
{
	GSList *msg_list = NULL;

	if (msgview->msginfo)
		msg_list = g_slist_append(msg_list, msgview->msginfo);
	message_actions_execute(msgview, action_nb, msg_list);
	g_slist_free(msg_list);
}

static void message_actions_execute(MessageView *msgview, guint action_nb,
				    GSList *msg_list)
{
	TextView *textview;
	MimeInfo *partinfo;
	gchar *buf;
	gchar *action;
	GtkWidget *text = NULL;
	GdkFont *msgfont = NULL;
	guint body_pos = 0;

	g_return_if_fail(action_nb < g_slist_length(prefs_common.actions_list));

	buf = (gchar *)g_slist_nth_data(prefs_common.actions_list, action_nb);

	g_return_if_fail(buf);
	g_return_if_fail(action = strstr(buf, ": "));

	/* Point to the beginning of the command-line */
	action += 2;

	textview = messageview_get_current_textview(msgview);
	if (textview) {
		text     = textview->text;
		msgfont  = textview->msgfont;
		body_pos = textview->body_pos;
	}
	partinfo = messageview_get_selected_mime_part(msgview);

	execute_actions(action, msg_list, text, msgfont, body_pos, partinfo);
}

static gboolean execute_actions(gchar *action, GSList *msg_list,
				GtkWidget *text, GdkFont *msgfont,
				gint body_pos, MimeInfo *partinfo)
{
	GSList *children_list = NULL;
	gint is_ok  = TRUE;
	gint msg_list_len;
	Children *children;
	ChildInfo *child_info;
	gint action_type;
	MsgInfo *msginfo;
	gchar *cmd;
	guint start = 0, end = 0;
	gchar *sel_str = NULL;
	gchar *msg_str = NULL;
	gchar *user_str = NULL;
	gchar *user_hidden_str = NULL;

	g_return_val_if_fail(action && *action, FALSE);

	action_type = action_get_type(action);

	if (action_type == ACTION_ERROR)
		return FALSE;         /* ERR: syntax error */

	if (action_type & (ACTION_SINGLE | ACTION_MULTIPLE) && !msg_list)
		return FALSE;         /* ERR: file command without selection */

	msg_list_len = g_slist_length(msg_list);

	if (action_type & (ACTION_PIPE_OUT | ACTION_PIPE_IN | ACTION_INSERT)) {
		if (msg_list_len > 1)
			return FALSE; /* ERR: pipe + multiple selection */
		if (!text)
			return FALSE; /* ERR: pipe and no displayed text */
	}

	if (action_type & ACTION_SELECTION_STR) {
		if (!text)
			return FALSE; /* ERR: selection string but no text */
	}

	if (text) {
		if (GTK_EDITABLE(text)->has_selection) {
			start = GTK_EDITABLE(text)->selection_start_pos;
			end   = GTK_EDITABLE(text)->selection_end_pos;
			if (start > end) {
				guint tmp;
				tmp = start;
				start = end;
				end = tmp;
			}

			if (start == end) {
				start = body_pos;
				end = gtk_stext_get_length(GTK_STEXT(text));
				msg_str = gtk_editable_get_chars
					(GTK_EDITABLE(text), start, end);
			} else {
				sel_str = gtk_editable_get_chars
					(GTK_EDITABLE(text), start, end);
				msg_str = g_strdup(sel_str);
			}
		} else {
			start = body_pos;
			end = gtk_stext_get_length(GTK_STEXT(text));
			msg_str = gtk_editable_get_chars(GTK_EDITABLE(text),
							 start, end);
		}
	}

	if (action_type & ACTION_USER_STR) {
		if (!(user_str = get_user_string(action, ACTION_USER_STR))) {
			g_free(msg_str);
			g_free(sel_str);
			return FALSE;
		}
	}

	if (action_type & ACTION_USER_HIDDEN_STR) {
		if (!(user_hidden_str =
			get_user_string(action, ACTION_USER_HIDDEN_STR))) {
			g_free(msg_str);
			g_free(sel_str);
			g_free(user_str);
			return FALSE;
		}
	}

	if (action_type & ACTION_PIPE_OUT) {
		gtk_stext_freeze(GTK_STEXT(text));
		gtk_stext_set_point(GTK_STEXT(text), start);
		gtk_stext_forward_delete(GTK_STEXT(text), end - start);
		gtk_stext_thaw(GTK_STEXT(text));
	}

	children = g_new0(Children, 1);

	children->msg_text = text;
	children->msgfont = msgfont;

	if ((action_type & (ACTION_USER_IN | ACTION_USER_HIDDEN_IN)) &&
	    ((action_type & ACTION_SINGLE) == 0 || msg_list_len == 1))
		children->open_in = 1;

	if (action_type & ACTION_SINGLE) {
		GSList *cur;

		for (cur = msg_list; cur && is_ok == TRUE; cur = cur->next) {
			msginfo = (MsgInfo *)cur->data;
			if (!msginfo) {
				is_ok  = FALSE; /* ERR: msginfo missing */
				break;
			}
			cmd = parse_action_cmd(action, msginfo, msg_list,
					       partinfo, user_str,
					       user_hidden_str, sel_str);
			if (!cmd) {
				debug_print("Action command error\n");
				is_ok  = FALSE; /* ERR: incorrect command */
				break;
			}
			if ((child_info = fork_child(cmd, action_type, msg_str,
						     children))) {
				children_list = g_slist_append(children_list,
							       child_info);
			}
			g_free(cmd);
		}
	} else {
		cmd = parse_action_cmd(action, NULL, msg_list, partinfo,
				       user_str, user_hidden_str, sel_str);
		if (cmd) {
			if ((child_info = fork_child(cmd, action_type, msg_str,
						     children))) {
				children_list = g_slist_append(children_list,
							       child_info);
			}
			g_free(cmd);
		} else
			is_ok  = FALSE;         /* ERR: incorrect command */
	}

	g_free(msg_str);
	g_free(sel_str);
	g_free(user_str);
	g_free(user_hidden_str);

	if (!children_list) {
		 /* If not waiting for children, return */
		g_free(children);
	} else {
		GSList *cur;

		children->action  = g_strdup(action);
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

		create_io_dialog(children);
	}

	return is_ok;
}

static ChildInfo *fork_child(gchar *cmd, gint action_type,
			     const gchar *msg_str, Children *children)
{
	gint chld_in[2], chld_out[2], chld_err[2], chld_status[2];
	gchar *cmdline[4];
	pid_t pid, gch_pid;
	ChildInfo *child_info;
	gint sync;

	sync = !(action_type & ACTION_ASYNC);

	chld_in[0] = chld_in[1] = chld_out[0] = chld_out[1] = chld_err[0]
		= chld_err[1] = chld_status[0] = chld_status[1] = -1;

	if (sync) {
		if (pipe(chld_status) || pipe(chld_in) || pipe(chld_out) ||
		    pipe(chld_err)) {
			alertpanel_error(_("Command could not be started. "
					   "Pipe creation failed.\n%s"),
					g_strerror(errno));
			/* Closing fd = -1 fails silently */
			close(chld_in[0]);
			close(chld_in[1]);
			close(chld_out[0]);
			close(chld_out[1]);
			close(chld_err[0]);
			close(chld_err[1]);
			close(chld_status[0]);
			close(chld_status[1]);
			return NULL; /* Pipe error */
		}
	}

	debug_print("Forking child and grandchild.\n");

	pid = fork();
	if (pid == 0) { /* Child */
		if (setpgid(0, 0))
			perror("setpgid");

		close(ConnectionNumber(gdk_display));

		gch_pid = fork();

		if (gch_pid == 0) {
			if (setpgid(0, getppid()))
				perror("setpgid");

			if (sync) {
				if (action_type &
				    (ACTION_PIPE_IN |
				     ACTION_USER_IN |
				     ACTION_USER_HIDDEN_IN)) {
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
			}

			cmdline[0] = "sh";
			cmdline[1] = "-c";
			cmdline[2] = cmd;
			cmdline[3] = 0;
			execvp("/bin/sh", cmdline);

			perror("execvp");
			_exit(1);
		} else if (gch_pid < (pid_t) 0) { /* Fork error */
			if (sync)
				write(chld_status[1], "1\n", 2);
			perror("fork");
			_exit(1);
		} else { /* Child */
			if (sync) {
				close(chld_in[0]);
				close(chld_in[1]);
				close(chld_out[0]);
				close(chld_out[1]);
				close(chld_err[0]);
				close(chld_err[1]);
				close(chld_status[0]);

				debug_print("Child: Waiting for grandchild\n");
				waitpid(gch_pid, NULL, 0);
				debug_print("Child: grandchild ended\n");
				write(chld_status[1], "0\n", 2);
				close(chld_status[1]);
			}
			_exit(0);
		}
	} else if (pid < 0) { /* Fork error */
		alertpanel_error(_("Could not fork to execute the following "
				   "command:\n%s\n%s"),
				 cmd, g_strerror(errno));
		return NULL; 
	}

	/* Parent */

	if (!sync) {
		waitpid(pid, NULL, 0);
		return NULL;
	}

	close(chld_in[0]);
	if (!(action_type &
	      (ACTION_PIPE_IN | ACTION_USER_IN | ACTION_USER_HIDDEN_IN)))
		close(chld_in[1]);
	close(chld_out[1]);
	close(chld_err[1]);
	close(chld_status[1]);

	child_info = g_new0(ChildInfo, 1);

	child_info->children    = children;

	child_info->pid         = pid;
	child_info->cmd         = g_strdup(cmd);
	child_info->type        = action_type;
	child_info->new_out     = FALSE;
	child_info->output      = g_string_new(NULL);
	child_info->chld_in     =
		(action_type &
		 (ACTION_PIPE_IN | ACTION_USER_IN | ACTION_USER_HIDDEN_IN))
			? chld_in [1] : -1;
	child_info->chld_out    = chld_out[0];
	child_info->chld_err    = chld_err[0];
	child_info->chld_status = chld_status[0];
	child_info->tag_in      = -1;
	child_info->tag_out     = gdk_input_add(chld_out[0], GDK_INPUT_READ,
						catch_output, child_info);
	child_info->tag_err     = gdk_input_add(chld_err[0], GDK_INPUT_READ,
						catch_output, child_info);

	if (!(action_type & (ACTION_PIPE_IN | ACTION_PIPE_OUT | ACTION_INSERT)))
		return child_info;

	if ((action_type & ACTION_PIPE_IN) && msg_str) {
		write(chld_in[1], msg_str, strlen(msg_str));
		if (!(action_type & (ACTION_USER_IN | ACTION_USER_HIDDEN_IN)))
			close(chld_in[1]);
		child_info->chld_in = -1; /* No more input */
	}

	return child_info;
}

static void kill_children_cb(GtkWidget *widget, gpointer data)
{
	GSList *cur;
	Children *children = (Children *) data;
	ChildInfo *child_info;

	for (cur = children->list; cur; cur = cur->next) {
		child_info = (ChildInfo *)(cur->data);
		debug_print("Killing child group id %d\n", child_info->pid);
		if (child_info->pid && kill(-child_info->pid, SIGTERM) < 0)
			perror("kill");
	}
}

static gint wait_for_children(Children *children)
{
	gboolean new_output;
	ChildInfo *child_info;
	GSList *cur;
	gint nb = children->nb;

	children->nb = 0;

	cur = children->list;
	new_output = FALSE;
	while (cur) {
		child_info = (ChildInfo *)cur->data;
		if (child_info->pid)
			children->nb++;
		new_output |= child_info->new_out;
		cur = cur->next;
	}

	children->output |= new_output;

	if (new_output || (children->dialog && (nb != children->nb)))
		update_io_dialog(children);

	if (children->nb)
		return FALSE;

	if (!children->dialog) {
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

	Children *children = (Children *)data;

	if (!children->nb) {
		gtk_signal_disconnect_by_data(GTK_OBJECT(children->dialog),
					      children);
		gtk_widget_destroy(children->dialog);
		free_children(children);
	}
}

static gint io_dialog_key_pressed_cb(GtkWidget *widget, GdkEventKey *event,
				     gpointer data)
{
	if (event && event->keyval == GDK_Escape)
		hide_io_dialog_cb(widget, data);
	return TRUE;
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

	debug_print("Freeing children data %p\n", children);

	g_free(children->action);
	for (cur = children->list; cur;) {
		child_info = (ChildInfo *)cur->data;
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

	debug_print("Updating actions input/output dialog.\n");

	if (!children->nb) {
		gtk_widget_set_sensitive(children->abort_btn, FALSE);
		gtk_widget_set_sensitive(children->close_btn, TRUE);
		if (children->input_hbox)
			gtk_widget_set_sensitive(children->input_hbox, FALSE);
		gtk_widget_grab_focus(children->close_btn);
		gtk_signal_connect(GTK_OBJECT(children->dialog),
				   "key_press_event",
				   GTK_SIGNAL_FUNC(io_dialog_key_pressed_cb),
				   children);
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
		for (cur = children->list; cur; cur = cur->next) {
			child_info = (ChildInfo *)cur->data;
			if (child_info->pid)
				caption = g_strdup_printf
					(_("--- Running: %s\n"),
					 child_info->cmd);
			else
				caption = g_strdup_printf
					(_("--- Ended: %s\n"),
					 child_info->cmd);

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
	GtkWidget *vbox;
	GtkWidget *entry = NULL;
	GtkWidget *input_hbox = NULL;
	GtkWidget *send_button;
	GtkWidget *label;
	GtkWidget *text;
	GtkWidget *scrolledwin;
	GtkWidget *hbox;
	GtkWidget *abort_button;
	GtkWidget *close_button;

	debug_print("Creating action IO dialog\n");

	dialog = gtk_dialog_new();
	gtk_container_set_border_width
		(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area), 5);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
	gtk_window_set_title(GTK_WINDOW(dialog), _("Action's input/output"));
	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
	manage_window_set_transient(GTK_WINDOW(dialog));
	gtk_signal_connect(GTK_OBJECT(dialog), "delete_event",
			GTK_SIGNAL_FUNC(delete_io_dialog_cb), children);
	gtk_signal_connect(GTK_OBJECT(dialog), "destroy",
			GTK_SIGNAL_FUNC(hide_io_dialog_cb),
			children);

	vbox = gtk_vbox_new(FALSE, 8);
	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->vbox), vbox);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 8);
	gtk_widget_show(vbox);

	label = gtk_label_new(children->action);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
				       GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	gtk_box_pack_start(GTK_BOX(vbox), scrolledwin, TRUE, TRUE, 0);
	gtk_widget_set_usize(scrolledwin, 480, 200);
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
		gtk_widget_set_usize(entry, 320, -1);
		gtk_signal_connect(GTK_OBJECT(entry), "activate",
				   GTK_SIGNAL_FUNC(send_input), children);
		gtk_box_pack_start(GTK_BOX(input_hbox), entry, TRUE, TRUE, 0);
		if (children->open_in & ACTION_USER_HIDDEN_IN)
			gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);
		gtk_widget_show(entry);

		send_button = gtk_button_new_with_label(_(" Send "));
		gtk_signal_connect(GTK_OBJECT(send_button), "clicked",
				   GTK_SIGNAL_FUNC(send_input), children);
		gtk_box_pack_start(GTK_BOX(input_hbox), send_button, FALSE,
				   FALSE, 0);
		gtk_widget_show(send_button);

		gtk_box_pack_start(GTK_BOX(vbox), input_hbox, FALSE, FALSE, 0);
		gtk_widget_grab_focus(entry);
	}

	gtkut_button_set_create(&hbox, &abort_button, _("Abort"),
				&close_button, _("Close"), NULL, NULL);
	gtk_signal_connect(GTK_OBJECT(abort_button), "clicked",
			GTK_SIGNAL_FUNC(kill_children_cb), children);
	gtk_signal_connect(GTK_OBJECT(close_button), "clicked",
			GTK_SIGNAL_FUNC(hide_io_dialog_cb), children);
	gtk_widget_show(hbox);

	if (children->nb)
		gtk_widget_set_sensitive(close_button, FALSE);

	gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area), hbox);

	children->dialog      = dialog;
	children->scrolledwin = scrolledwin;
	children->text        = text;
	children->input_hbox  = children->open_in ? input_hbox : NULL;
	children->input_entry = children->open_in ? entry : NULL;
	children->abort_btn   = abort_button;
	children->close_btn   = close_button;

	gtk_widget_show(dialog);
}

static void catch_status(gpointer data, gint source, GdkInputCondition cond)
{
	ChildInfo *child_info = (ChildInfo *)data;
	gchar buf;
	gint c;

	gdk_input_remove(child_info->tag_status);

	c = read(source, &buf, 1);
	debug_print("Child returned %c\n", buf);

	waitpid(-child_info->pid, NULL, 0);
	childinfo_close_pipes(child_info);
	child_info->pid = 0;

	wait_for_children(child_info->children);
}
	
static void catch_input(gpointer data, gint source, GdkInputCondition cond)
{
	Children *children = (Children *)data;
	ChildInfo *child_info = (ChildInfo *)children->list->data;
	gchar *input;
	gint c, count, len;

	debug_print("Sending input to grand child.\n");
	if (!(cond && GDK_INPUT_WRITE))
		return;

	gdk_input_remove(child_info->tag_in);
	child_info->tag_in = -1;

	input = gtk_editable_get_chars(GTK_EDITABLE(children->input_entry),
				       0, -1);
	len = strlen(input);
	count = 0;

	do {
		c = write(child_info->chld_in, input + count, len - count);
		if (c >= 0)
			count += c;
	} while (c >= 0 && count < len);

	if (c >= 0)
		write(child_info->chld_in, "\n", 2);

	g_free(input);

	gtk_entry_set_text(GTK_ENTRY(children->input_entry), "");
	gtk_widget_set_sensitive(children->input_hbox, TRUE);
	close(child_info->chld_in);
	child_info->chld_in = -1;
	debug_print("Input to grand child sent.\n");
}

static void catch_output(gpointer data, gint source, GdkInputCondition cond)
{
	ChildInfo *child_info = (ChildInfo *)data;
	gint c, i;
	gchar buf[BUFFSIZE];

	debug_print("Catching grand child's output.\n");
	if (child_info->type & (ACTION_PIPE_OUT | ACTION_INSERT)
	    && source == child_info->chld_out) {
		gboolean is_selection = FALSE;
		GtkWidget *text = child_info->children->msg_text;

		if (GTK_EDITABLE(text)->has_selection)
			is_selection = TRUE;
		gtk_stext_freeze(GTK_STEXT(text));
		while (TRUE) {
			c = read(source, buf, sizeof(buf) - 1);
			if (c == 0)
				break;
			gtk_stext_insert(GTK_STEXT(text),
					 child_info->children->msgfont,
					 NULL, NULL, buf, c);
		}
		if (is_selection) {
			/* Using the select_region draws things. Should not.
			 * so we just change selection position and 
			 * defere drawing when thawing. Hack?
			 */
			GTK_EDITABLE(text)->selection_end_pos =
					gtk_stext_get_point(GTK_STEXT(text));
		}
		gtk_stext_thaw(GTK_STEXT(text));
	} else {
		c = read(source, buf, sizeof(buf) - 1);
		for (i = 0; i < c; i++)
			g_string_append_c(child_info->output, buf[i]);
		if (c > 0)
			child_info->new_out = TRUE;
	}
	wait_for_children(child_info->children);
}

static gchar *get_user_string(const gchar *action, ActionType type)
{
	gchar *message;
	gchar *user_str = NULL;

	switch (type) {
	case ACTION_USER_HIDDEN_STR:
		message = g_strdup_printf
			(_("Enter the argument for the following action:\n"
			   "(`%%h' will be replaced with the argument)\n"
			   "  %s"),
			 action);
		user_str = input_dialog_with_invisible
			(_("Action's hidden user argument"), message, NULL);
		break;
	case ACTION_USER_STR:
		message = g_strdup_printf
			(_("Enter the argument for the following action:\n"
			   "(`%%u' will be replaced with the argument)\n"
			   "  %s"),
			 action);
		user_str = input_dialog
			(_("Action's user argument"), message, NULL);
		break;
	default:
		g_warning("Unsupported action type %d", type);
	}

	return user_str;
}
