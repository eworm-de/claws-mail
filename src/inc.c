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
#include <gtk/gtkmain.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkprogressbar.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

#include "intl.h"
#include "main.h"
#include "inc.h"
#include "mainwindow.h"
#include "folderview.h"
#include "summaryview.h"
#include "prefs_common.h"
#include "prefs_account.h"
#include "account.h"
#include "procmsg.h"
#include "socket.h"
#include "ssl.h"
#include "pop.h"
#include "recv.h"
#include "mbox.h"
#include "utils.h"
#include "gtkutils.h"
#include "statusbar.h"
#include "manage_window.h"
#include "stock_pixmap.h"
#include "progressdialog.h"
#include "inputdialog.h"
#include "alertpanel.h"
#include "filter.h"
#include "automaton.h"
#include "folder.h"
#include "filtering.h"

static guint inc_lock_count = 0;

static GdkPixmap *currentxpm;
static GdkBitmap *currentxpmmask;
static GdkPixmap *errorxpm;
static GdkBitmap *errorxpmmask;
static GdkPixmap *okxpm;
static GdkBitmap *okxpmmask;

#define MSGBUFSIZE	8192

static void inc_finished		(MainWindow		*mainwin,
					 gboolean		 new_messages);
static gint inc_account_mail		(PrefsAccount		*account,
					 MainWindow		*mainwin);

static IncProgressDialog *inc_progress_dialog_create	(void);
static void inc_progress_dialog_destroy	(IncProgressDialog	*inc_dialog);

static IncSession *inc_session_new	(PrefsAccount		*account);
static void inc_session_destroy		(IncSession		*session);
static Pop3State *inc_pop3_state_new	(PrefsAccount		*account);
static void inc_pop3_state_destroy	(Pop3State		*state);
static gint inc_start			(IncProgressDialog	*inc_dialog);
static IncState inc_pop3_session_do	(IncSession		*session);
static gint pop3_automaton_terminate	(SockInfo		*source,
					 Automaton		*atm);

static GHashTable *inc_get_uidl_table	(PrefsAccount		*ac_prefs);
static void inc_write_uidl_list		(Pop3State		*state);

#if USE_THREADS
static gint connection_check_cb		(Automaton	*atm);
#endif

static void inc_pop3_recv_func		(SockInfo	*sock,
					 gint		 count,
					 gint		 read_bytes,
					 gpointer	 data);

static void inc_put_error		(IncState	 istate);

static void inc_cancel			(GtkWidget	*widget,
					 gpointer	 data);

static gint inc_spool			(void);
static gint get_spool			(FolderItem	*dest,
					 const gchar	*mbox);

static void inc_all_spool(void);
static void inc_autocheck_timer_set_interval	(guint		 interval);
static gint inc_autocheck_func			(gpointer	 data);

#define FOLDER_SUMMARY_MISMATCH(f, s) \
	(f) && (s) ? ((s)->newmsgs != (f)->new) || ((f)->unread != (s)->unread) || ((f)->total != (s)->messages) \
	: FALSE
	
/**
 * inc_finished:
 * @mainwin: Main window.
 * @new_messages: TRUE if some messages have been received.
 * 
 * Update the folder view and the summary view after receiving
 * messages.  If @new_messages is FALSE, this function avoids unneeded
 * updating.
 **/
static void inc_finished(MainWindow *mainwin, gboolean new_messages)
{
	FolderItem *item;
	
	if (prefs_common.scan_all_after_inc)
		folderview_update_all_node();

	if (!new_messages && !prefs_common.scan_all_after_inc) return;

	if (prefs_common.open_inbox_on_inc) {
		item = cur_account && cur_account->inbox
			? folder_find_item_from_path(cur_account->inbox)
			: folder_get_default_inbox();
		if (FOLDER_SUMMARY_MISMATCH(item, mainwin->summaryview)) {	
			folderview_unselect(mainwin->folderview);
			folderview_select(mainwin->folderview, item);
		}	
	} else {
		item = mainwin->summaryview->folder_item;
		if (FOLDER_SUMMARY_MISMATCH(item, mainwin->summaryview)) {
			folderview_unselect(mainwin->folderview);
			folderview_select(mainwin->folderview, item);
		}	
	}
}

void inc_mail(MainWindow *mainwin)
{
	gint new_msgs = 0;

	if (inc_lock_count) return;

	inc_autocheck_timer_remove();
	summary_write_cache(mainwin->summaryview);
	main_window_lock(mainwin);

	if (prefs_common.use_extinc && prefs_common.extinc_cmd) {
		/* external incorporating program */
		if (execute_command_line(prefs_common.extinc_cmd, FALSE) < 0) {
			main_window_unlock(mainwin);
			inc_autocheck_timer_set();
			return;
		}

		if (prefs_common.inc_local)
			new_msgs = inc_spool();
	} else {
		if (prefs_common.inc_local)
			new_msgs = inc_spool();

		new_msgs += inc_account_mail(cur_account, mainwin);
	}

	inc_finished(mainwin, new_msgs > 0);
	main_window_unlock(mainwin);
	inc_autocheck_timer_set();
}

static gint inc_account_mail(PrefsAccount *account, MainWindow *mainwin)
{
	IncProgressDialog *inc_dialog;
	IncSession *session;
	gchar *text[3];

	session = inc_session_new(account);
	if (!session) return 0;

	inc_dialog = inc_progress_dialog_create();
	inc_dialog->queue_list = g_list_append(inc_dialog->queue_list, session);
	inc_dialog->mainwin = mainwin;
	session->data = inc_dialog;

	text[0] = NULL;
	text[1] = account->account_name;
	text[2] = _("Standby");
	gtk_clist_append(GTK_CLIST(inc_dialog->dialog->clist), text);

	return inc_start(inc_dialog);
}

void inc_all_account_mail(MainWindow *mainwin)
{
	GList *list, *queue_list = NULL;
	IncProgressDialog *inc_dialog;
	gint new_msgs = 0;

	if (inc_lock_count) return;

	inc_autocheck_timer_remove();
	summary_write_cache(mainwin->summaryview);
	main_window_lock(mainwin);

	if (prefs_common.inc_local)
		new_msgs = inc_spool();

	list = account_get_list();
	if (!list) {
		inc_finished(mainwin, new_msgs > 0);
		main_window_unlock(mainwin);
		inc_autocheck_timer_set();
		return;
	}

	for (; list != NULL; list = list->next) {
		IncSession *session;
		PrefsAccount *account = list->data;

		if (account->recv_at_getall) {
			session = inc_session_new(account);
			if (session)
				queue_list = g_list_append(queue_list, session);
		}
	}

	if (!queue_list) {
		inc_finished(mainwin, new_msgs > 0);
		main_window_unlock(mainwin);
		inc_autocheck_timer_set();
		return;
	}

	inc_dialog = inc_progress_dialog_create();
	inc_dialog->queue_list = queue_list;
	inc_dialog->mainwin = mainwin;
	for (list = queue_list; list != NULL; list = list->next) {
		IncSession *session = list->data;
		gchar *text[3];

		session->data = inc_dialog;

		text[0] = NULL;
		text[1] = session->pop3_state->ac_prefs->account_name;
		text[2] = _("Standby");
		gtk_clist_append(GTK_CLIST(inc_dialog->dialog->clist), text);
	}

	new_msgs += inc_start(inc_dialog);

	inc_finished(mainwin, new_msgs > 0);
	main_window_unlock(mainwin);
	inc_autocheck_timer_set();
}

static IncProgressDialog *inc_progress_dialog_create(void)
{
	IncProgressDialog *dialog;
	ProgressDialog *progress;

	dialog = g_new0(IncProgressDialog, 1);

	progress = progress_dialog_create();
	gtk_window_set_title(GTK_WINDOW(progress->window),
			     _("Retrieving new messages"));
	gtk_signal_connect(GTK_OBJECT(progress->cancel_btn), "clicked",
			   GTK_SIGNAL_FUNC(inc_cancel), dialog);
	gtk_signal_connect(GTK_OBJECT(progress->window), "delete_event",
			   GTK_SIGNAL_FUNC(gtk_true), NULL);
	/* manage_window_set_transient(GTK_WINDOW(progress->window)); */

	progress_dialog_set_value(progress, 0.0);

	stock_pixmap_gdk(progress->clist, STOCK_PIXMAP_COMPLETE,
			 &okxpm, &okxpmmask);
	stock_pixmap_gdk(progress->clist, STOCK_PIXMAP_CONTINUE,
			 &currentxpm, &currentxpmmask);
	stock_pixmap_gdk(progress->clist, STOCK_PIXMAP_ERROR,
			 &errorxpm, &errorxpmmask);

	if (prefs_common.recv_dialog_mode == RECV_DIALOG_ALWAYS ||
	    (prefs_common.recv_dialog_mode == RECV_DIALOG_ACTIVE &&
	     manage_window_get_focus_window())) {
		dialog->show_dialog = TRUE;
		gtk_widget_show_now(progress->window);
	}

	dialog->dialog = progress;
	dialog->queue_list = NULL;

	return dialog;
}

static void inc_progress_dialog_clear(IncProgressDialog *inc_dialog)
{
	progress_dialog_set_value(inc_dialog->dialog, 0.0);
	progress_dialog_set_label(inc_dialog->dialog, "");
	gtk_progress_bar_update
		(GTK_PROGRESS_BAR(inc_dialog->mainwin->progressbar), 0.0);
}

static void inc_progress_dialog_destroy(IncProgressDialog *inc_dialog)
{
	g_return_if_fail(inc_dialog != NULL);

	gtk_progress_bar_update
		(GTK_PROGRESS_BAR(inc_dialog->mainwin->progressbar), 0.0);
	progress_dialog_destroy(inc_dialog->dialog);

	g_free(inc_dialog);
}

static IncSession *inc_session_new(PrefsAccount *account)
{
	IncSession *session;

	g_return_val_if_fail(account != NULL, NULL);

	if (account->protocol != A_POP3 && account->protocol != A_APOP)
		return NULL;
	if (!account->recv_server || !account->userid)
		return NULL;

	session = g_new0(IncSession, 1);
	session->pop3_state = inc_pop3_state_new(account);
	session->pop3_state->session = session;

	return session;
}

static void inc_session_destroy(IncSession *session)
{
	g_return_if_fail(session != NULL);

	inc_pop3_state_destroy(session->pop3_state);
	g_free(session);
}

static Pop3State *inc_pop3_state_new(PrefsAccount *account)
{
	Pop3State *state;

	state = g_new0(Pop3State, 1);

	state->ac_prefs = account;
	state->folder_table = g_hash_table_new(NULL, NULL);
	state->id_table = inc_get_uidl_table(account);
	state->id_list = NULL;
	state->new_id_list = NULL;
	state->inc_state = INC_SUCCESS;

	return state;
}

static void inc_pop3_state_destroy(Pop3State *state)
{
	g_hash_table_destroy(state->folder_table);
	g_free(state->sizes);

	if (state->id_table) {
		hash_free_strings(state->id_table);
		g_hash_table_destroy(state->id_table);
	}
	slist_free_strings(state->id_list);
	slist_free_strings(state->new_id_list);
	g_slist_free(state->id_list);
	g_slist_free(state->new_id_list);

	g_free(state->greeting);
	g_free(state->user);
	g_free(state->pass);
	g_free(state->prev_folder);

	g_free(state);
}

static gint inc_start(IncProgressDialog *inc_dialog)
{
	IncSession *session;
	GtkCList *clist = GTK_CLIST(inc_dialog->dialog->clist);
	Pop3State *pop3_state;
	IncState inc_state;
	gint num = 0;
	gint error_num = 0;
	gint new_msgs = 0;

	while (inc_dialog->queue_list != NULL) {
		session = inc_dialog->queue_list->data;
		pop3_state = session->pop3_state;

		inc_progress_dialog_clear(inc_dialog);

		gtk_clist_moveto(clist, num, -1, 1.0, 0.0);

		pop3_state->user = g_strdup(pop3_state->ac_prefs->userid);
		if (pop3_state->ac_prefs->passwd)
			pop3_state->pass =
				g_strdup(pop3_state->ac_prefs->passwd);
		else if (pop3_state->ac_prefs->tmp_pass)
			pop3_state->pass =
				g_strdup(pop3_state->ac_prefs->tmp_pass);
		else {
			gchar *pass;

			pass = input_dialog_query_password
				(pop3_state->ac_prefs->recv_server,
				 pop3_state->user);

			if (inc_dialog->show_dialog)
				manage_window_focus_in
					(inc_dialog->mainwin->window,
					 NULL, NULL);
			if (pass) {
				pop3_state->ac_prefs->tmp_pass = g_strdup(pass);
				pop3_state->pass = pass;
			} else {
				inc_session_destroy(session);
				inc_dialog->queue_list = g_list_remove
					(inc_dialog->queue_list, session);
				continue;
			}
		}

		gtk_clist_set_pixmap(clist, num, 0, currentxpm, currentxpmmask);
		gtk_clist_set_text(clist, num, 2, _("Retrieving"));

		/* begin POP3 session */
		inc_state = inc_pop3_session_do(session);

		if (inc_state == INC_SUCCESS) {
			gtk_clist_set_pixmap(clist, num, 0, okxpm, okxpmmask);
			gtk_clist_set_text(clist, num, 2, _("Done"));
		} else if (inc_state == INC_CANCEL) {
			gtk_clist_set_pixmap(clist, num, 0, okxpm, okxpmmask);
			gtk_clist_set_text(clist, num, 2, _("Cancelled"));
		} else {
			gtk_clist_set_pixmap(clist, num, 0, errorxpm, errorxpmmask);
			if (inc_state == INC_CONNECT_ERROR)
				gtk_clist_set_text(clist, num, 2,
						   _("Connection failed"));
			else if (inc_state == INC_AUTH_FAILED)
				gtk_clist_set_text(clist, num, 2,
						   _("Auth failed"));
			else
				gtk_clist_set_text(clist, num, 2, _("Error"));
		}

		if (pop3_state->error_val == PS_AUTHFAIL) {
			if(!prefs_common.noerrorpanel) {
				if((prefs_common.recv_dialog_mode == RECV_DIALOG_ALWAYS) ||
				    ((prefs_common.recv_dialog_mode == RECV_DIALOG_ACTIVE) && focus_window)) {
					manage_window_focus_in(inc_dialog->dialog->window, NULL, NULL);
				}
				alertpanel_error
					(_("Authorization for %s on %s failed"),
					 pop3_state->user,
					 pop3_state->ac_prefs->recv_server);
			}
		}

		statusbar_pop_all();

		new_msgs += pop3_state->cur_total_num;

		if (!prefs_common.scan_all_after_inc) {
			folder_item_scan_foreach(pop3_state->folder_table);
			folderview_update_item_foreach
				(pop3_state->folder_table);
		}

		if (pop3_state->error_val == PS_AUTHFAIL &&
		    pop3_state->ac_prefs->tmp_pass) {
			g_free(pop3_state->ac_prefs->tmp_pass);
			pop3_state->ac_prefs->tmp_pass = NULL;
		}

		inc_write_uidl_list(pop3_state);

		if (inc_state != INC_SUCCESS && inc_state != INC_CANCEL) {
			error_num++;
			if (inc_state == INC_NOSPACE) {
				inc_put_error(inc_state);
				break;
			}
		}

		inc_session_destroy(session);
		inc_dialog->queue_list =
			g_list_remove(inc_dialog->queue_list, session);

		num++;
	}

	if (error_num && !prefs_common.noerrorpanel) {
		if (inc_dialog->show_dialog)
			manage_window_focus_in(inc_dialog->dialog->window,
					       NULL, NULL);
		alertpanel_error(_("Some errors occured while getting mail."));
		if (inc_dialog->show_dialog)
			manage_window_focus_out(inc_dialog->dialog->window,
						NULL, NULL);
	}

	while (inc_dialog->queue_list != NULL) {
		session = inc_dialog->queue_list->data;
		inc_session_destroy(session);
		inc_dialog->queue_list =
			g_list_remove(inc_dialog->queue_list, session);
	}

	inc_progress_dialog_destroy(inc_dialog);

	return new_msgs;
}

static IncState inc_pop3_session_do(IncSession *session)
{
	Pop3State *pop3_state = session->pop3_state;
	IncProgressDialog *inc_dialog = (IncProgressDialog *)session->data;
	Automaton *atm;
	SockInfo *sockinfo;
	gint i;
	gchar *server;
	gushort port;
	gchar *buf;
	static AtmHandler handlers[] = {
		pop3_greeting_recv      ,
		pop3_getauth_user_send  , pop3_getauth_user_recv,
		pop3_getauth_pass_send  , pop3_getauth_pass_recv,
		pop3_getauth_apop_send  , pop3_getauth_apop_recv,
		pop3_getrange_stat_send , pop3_getrange_stat_recv,
		pop3_getrange_last_send , pop3_getrange_last_recv,
		pop3_getrange_uidl_send , pop3_getrange_uidl_recv,
		pop3_getsize_list_send  , pop3_getsize_list_recv,
		pop3_retr_send          , pop3_retr_recv,
		pop3_delete_send        , pop3_delete_recv,
		pop3_logout_send        , pop3_logout_recv
	};

	debug_print(_("getting new messages of account %s...\n"),
		    pop3_state->ac_prefs->account_name);

	atm = automaton_create(N_POP3_PHASE);

	session->atm = atm;
	atm->data = pop3_state;

	buf = g_strdup_printf(_("%s: Retrieving new messages"),
			      pop3_state->ac_prefs->recv_server);
	gtk_window_set_title(GTK_WINDOW(inc_dialog->dialog->window), buf);
	g_free(buf);

	for (i = POP3_GREETING_RECV; i < N_POP3_PHASE; i++)
		atm->state[i].handler = handlers[i];
	atm->state[POP3_GREETING_RECV].condition = GDK_INPUT_READ;
	for (i = POP3_GETAUTH_USER_SEND; i < N_POP3_PHASE; ) {
		atm->state[i++].condition = GDK_INPUT_WRITE;
		atm->state[i++].condition = GDK_INPUT_READ;
	}

	atm->terminate = (AtmHandler)pop3_automaton_terminate;

	atm->num = POP3_GREETING_RECV;

	server = pop3_state->ac_prefs->recv_server;
#if USE_SSL
	port = pop3_state->ac_prefs->set_popport ?
		pop3_state->ac_prefs->popport :
		pop3_state->ac_prefs->ssl_pop ? 995 : 110;
#else
	port = pop3_state->ac_prefs->set_popport ?
		pop3_state->ac_prefs->popport : 110;
#endif

	buf = g_strdup_printf(_("Connecting to POP3 server: %s ..."), server);
	log_message("%s\n", buf);
	progress_dialog_set_label(inc_dialog->dialog, buf);
	g_free(buf);
	GTK_EVENTS_FLUSH();
	statusbar_pop_all();

#if USE_THREADS
	if ((sockinfo = sock_connect_with_thread(server, port)) == NULL) {
#else
	if ((sockinfo = sock_connect(server, port)) == NULL) {
#endif
		log_warning(_("Can't connect to POP3 server: %s:%d\n"),
			    server, port);
		if(!prefs_common.noerrorpanel) {
			if((prefs_common.recv_dialog_mode == RECV_DIALOG_ALWAYS) ||
			    ((prefs_common.recv_dialog_mode == RECV_DIALOG_ACTIVE) && focus_window)) {
				manage_window_focus_in(inc_dialog->dialog->window, NULL, NULL);
			}
			alertpanel_error(_("Can't connect to POP3 server: %s:%d"),
					 server, port);
			manage_window_focus_out(inc_dialog->dialog->window, NULL, NULL);
		}
		pop3_automaton_terminate(NULL, atm);
		automaton_destroy(atm);
		return INC_CONNECT_ERROR;
	}

#if USE_SSL
	if (pop3_state->ac_prefs->ssl_pop && !ssl_init_socket(sockinfo)) {
		pop3_automaton_terminate(NULL, atm);
		automaton_destroy(atm);
		return INC_CONNECT_ERROR;
	}
#endif

	/* :WK: Hmmm, with the later sock_gdk_input, we have 2 references
	 * to the sock structure - implement a reference counter?? */
	pop3_state->sockinfo = sockinfo;
	atm->help_sock = sockinfo;

	log_verbosity_set(TRUE);
	recv_set_ui_func(inc_pop3_recv_func, session);

#if USE_THREADS
	atm->timeout_tag = gtk_timeout_add
		(TIMEOUT_ITV, (GtkFunction)connection_check_cb, atm);
#else
	atm->tag = sock_gdk_input_add(sockinfo,
				      atm->state[atm->num].condition,
				      automaton_input_cb, atm);
#endif

	while (!atm->terminated)
		gtk_main_iteration();

	log_verbosity_set(FALSE);
	recv_set_ui_func(NULL, NULL);

#if USE_THREADS
	/* pthread_join(sockinfo->connect_thr, NULL); */
#endif
	automaton_destroy(atm);

	return pop3_state->inc_state;
}

static gint pop3_automaton_terminate(SockInfo *source, Automaton *atm)
{
	if (atm->terminated) return 0;

	if (atm->tag > 0) {
		gdk_input_remove(atm->tag);
		atm->tag = 0;
	}
	if (atm->timeout_tag > 0) {
		gtk_timeout_remove(atm->timeout_tag);
		atm->timeout_tag = 0;
	}
	if (source)
		sock_close(source);

	atm->terminated = TRUE;

	return 0;
}

static GHashTable *inc_get_uidl_table(PrefsAccount *ac_prefs)
{
	GHashTable *table;
	gchar *path;
	FILE *fp;
	gchar buf[IDLEN + 3];

	path = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
			   "uidl-", ac_prefs->recv_server,
			   "-", ac_prefs->userid, NULL);
	if ((fp = fopen(path, "r")) == NULL) {
		if (ENOENT != errno) FILE_OP_ERROR(path, "fopen");
		g_free(path);
		return NULL;
	}
	g_free(path);

	table = g_hash_table_new(g_str_hash, g_str_equal);

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		strretchomp(buf);
		g_hash_table_insert(table, g_strdup(buf), GINT_TO_POINTER(1));
	}

	fclose(fp);

	return table;
}

static void inc_write_uidl_list(Pop3State *state)
{
	gchar *path;
	FILE *fp;
	GSList *cur;

	if (!state->id_list) return;

	path = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
			   "uidl-", state->ac_prefs->recv_server,
			   "-", state->user, NULL);
	if ((fp = fopen(path, "w")) == NULL) {
		FILE_OP_ERROR(path, "fopen");
		g_free(path);
		return;
	}

	for (cur = state->id_list; cur != NULL; cur = cur->next) {
		if (fputs((gchar *)cur->data, fp) == EOF) {
			FILE_OP_ERROR(path, "fputs");
			break;
		}
		if (fputc('\n', fp) == EOF) {
			FILE_OP_ERROR(path, "fputc");
			break;
		}
	}

	if (fclose(fp) == EOF) FILE_OP_ERROR(path, "fclose");
	g_free(path);
}

#if USE_THREADS
static gint connection_check_cb(Automaton *atm)
{
	Pop3State *state = atm->data;
	IncProgressDialog *inc_dialog = state->session->data;
	SockInfo *sockinfo = state->sockinfo;

	/* g_print("connection check\n"); */

	if (sockinfo->state == CONN_LOOKUPFAILED ||
	    sockinfo->state == CONN_FAILED) {
		atm->timeout_tag = 0;
		log_warning(_("Can't connect to POP3 server: %s:%d\n"),
			    sockinfo->hostname, sockinfo->port);
		if(!prefs_common.noerrorpanel) {
			if((prefs_common.recv_dialog_mode == RECV_DIALOG_ALWAYS) ||
			    ((prefs_common.recv_dialog_mode == RECV_DIALOG_ACTIVE) && focus_window)) {
				manage_window_focus_in(inc_dialog->dialog->window, NULL, NULL);
			}
			alertpanel_error(_("Can't connect to POP3 server: %s:%d"),
					 sockinfo->hostname, sockinfo->port);
			manage_window_focus_out(inc_dialog->dialog->window, NULL, NULL);
		}
		pop3_automaton_terminate(sockinfo, atm);
		state->sockinfo = NULL;
		return FALSE;
	} else if (sockinfo->state == CONN_ESTABLISHED) {
		atm->timeout_tag = 0;
		atm->tag = sock_gdk_input_add(sockinfo,
					      atm->state[atm->num].condition,
					      automaton_input_cb, atm);
		return FALSE;
	} else {
		return TRUE;
	}
}
#endif

static void inc_pop3_recv_func(SockInfo *sock, gint count, gint read_bytes,
			       gpointer data)
{
	gchar buf[MSGBUFSIZE];
	IncSession *session = (IncSession *)data;
	Pop3State *state = session->pop3_state;
	IncProgressDialog *inc_dialog = session->data;
	ProgressDialog *dialog = inc_dialog->dialog;
	gint cur_total;
	gchar *total_size;

	cur_total = state->cur_total_bytes + read_bytes;
	if (cur_total > state->total_bytes)
		cur_total = state->total_bytes;

	Xstrdup_a(total_size, to_human_readable(state->total_bytes), return);
	g_snprintf(buf, sizeof(buf),
		   _("Retrieving message (%d / %d) (%s / %s)"),
		   state->cur_msg, state->count,
		   to_human_readable(cur_total), total_size);
	progress_dialog_set_label(dialog, buf);

	progress_dialog_set_percentage
		(dialog, (gfloat)cur_total / (gfloat)state->total_bytes);
	gtk_progress_bar_update
		(GTK_PROGRESS_BAR(inc_dialog->mainwin->progressbar),
		 (gfloat)cur_total / (gfloat)state->total_bytes);
	GTK_EVENTS_FLUSH();
}

void inc_progress_update(Pop3State *state, Pop3Phase phase)
{
	gchar buf[MSGBUFSIZE];
	IncProgressDialog *inc_dialog = state->session->data;
	ProgressDialog *dialog = inc_dialog->dialog;
	gchar *total_size;

	switch (phase) {
	case POP3_GREETING_RECV:
		break;
	case POP3_GETAUTH_USER_SEND:
	case POP3_GETAUTH_USER_RECV:
	case POP3_GETAUTH_PASS_SEND:
	case POP3_GETAUTH_PASS_RECV:
	case POP3_GETAUTH_APOP_SEND:
	case POP3_GETAUTH_APOP_RECV:
		progress_dialog_set_label(dialog, _("Authenticating..."));
		break;
	case POP3_GETRANGE_STAT_SEND:
	case POP3_GETRANGE_STAT_RECV:
		progress_dialog_set_label
			(dialog, _("Getting the number of new messages (STAT)..."));
		break;
	case POP3_GETRANGE_LAST_SEND:
	case POP3_GETRANGE_LAST_RECV:
		progress_dialog_set_label
			(dialog, _("Getting the number of new messages (LAST)..."));
		break;
	case POP3_GETRANGE_UIDL_SEND:
	case POP3_GETRANGE_UIDL_RECV:
		progress_dialog_set_label
			(dialog, _("Getting the number of new messages (UIDL)..."));
		break;
	case POP3_GETSIZE_LIST_SEND:
	case POP3_GETSIZE_LIST_RECV:
		progress_dialog_set_label
			(dialog, _("Getting the size of messages (LIST)..."));
		break;
	case POP3_RETR_SEND:
	case POP3_RETR_RECV:
		Xstrdup_a(total_size, to_human_readable(state->total_bytes), return);
		g_snprintf(buf, sizeof(buf),
			   _("Retrieving message (%d / %d) (%s / %s)"),
			   state->cur_msg, state->count,
			   to_human_readable(state->cur_total_bytes),
			   total_size);
		progress_dialog_set_label(dialog, buf);
		progress_dialog_set_percentage
			(dialog,
			 (gfloat)(state->cur_total_bytes) /
			 (gfloat)(state->total_bytes));
		gtk_progress_bar_update
			(GTK_PROGRESS_BAR(inc_dialog->mainwin->progressbar),
			 (gfloat)(state->cur_total_bytes) /
			 (gfloat)(state->total_bytes));
		break;
	case POP3_DELETE_SEND:
	case POP3_DELETE_RECV:
		progress_dialog_set_label(dialog, _("Deleting message"));
		break;
	case POP3_LOGOUT_SEND:
	case POP3_LOGOUT_RECV:
		progress_dialog_set_label(dialog, _("Quitting"));
		break;
	default:
		break;
	}
}

gint inc_drop_message(const gchar *file, Pop3State *state)
{
	FolderItem *inbox;
	FolderItem *dropfolder;
	gint val;
	gint msgnum;

	/* CLAWS: get default inbox (perhaps per account) */
	if (state->ac_prefs->inbox) {
		/* CLAWS: get destination folder / mailbox */
		inbox = folder_find_item_from_identifier(state->ac_prefs->inbox);
		if (!inbox)
			inbox = folder_get_default_inbox();
	} else
		inbox = folder_get_default_inbox();
	if (!inbox) {
		unlink(file);
		return -1;
	}

	/* CLAWS: claws uses a global .processing folder for the filtering. */
	if (global_processing == NULL) {
		if (state->ac_prefs->filter_on_recv) {
			dropfolder =
				filter_get_dest_folder(prefs_common.fltlist, file);
			if (!dropfolder) dropfolder = inbox;
			else if (!strcmp(dropfolder->path, FILTER_NOT_RECEIVE)) {
				g_warning(_("a message won't be received\n"));
				return 1;
			}
		} else
			dropfolder = inbox;
	} else {
		dropfolder = folder_get_default_processing();
	}

	val = GPOINTER_TO_INT(g_hash_table_lookup
			      (state->folder_table, dropfolder));
	if (val == 0) {
		folder_item_scan(dropfolder);
		g_hash_table_insert(state->folder_table, dropfolder,
				    GINT_TO_POINTER(1));
	}
	
	/* add msg file to drop folder */
	if ((msgnum = folder_item_add_msg(dropfolder, file, TRUE)) < 0) {
		unlink(file);
		return -1;
	}

	/* CLAWS: perform filtering actions on dropped message */
	if (global_processing != NULL) { 
		if (state->ac_prefs->filter_on_recv)
			filter_message(global_processing, inbox, msgnum,
				       state->folder_table);
	}

	return 0;
}

static void inc_put_error(IncState istate)
{
	switch (istate) {
	case INC_ERROR:
		if(!prefs_common.noerrorpanel) {
			alertpanel_error(_("Error occurred while processing mail."));
		}
		break;
	case INC_NOSPACE:
		alertpanel_error(_("No disk space left."));
		break;
	default:
		break;
	}
}

static void inc_cancel(GtkWidget *widget, gpointer data)
{
	IncProgressDialog *dialog = data;
	IncSession *session = dialog->queue_list->data;
	SockInfo *sockinfo = session->pop3_state->sockinfo;

	if (!sockinfo || session->atm->terminated == TRUE) return;

#if USE_THREADS
	if (sockinfo->state == CONN_READY ||
	    sockinfo->state == CONN_LOOKUPSUCCESS) {
		pthread_cancel(sockinfo->connect_thr);
		/* pthread_kill(sockinfo->connect_thr, SIGINT); */
		g_print("connection was cancelled.\n");
	}
#endif

	session->pop3_state->inc_state = INC_CANCEL;
	pop3_automaton_terminate(sockinfo, session->atm);
	session->pop3_state->sockinfo = NULL;
}

static gint inc_spool(void)
{
	gchar *mbox, *logname;
	gint msgs;

	logname = g_get_user_name();
	mbox = g_strconcat(prefs_common.spool_path
			   ? prefs_common.spool_path : DEFAULT_SPOOL_PATH,
			   G_DIR_SEPARATOR_S, logname, NULL);
	msgs = get_spool(folder_get_default_inbox(), mbox);
	g_free(mbox);

	return msgs;
}

static void inc_spool_account(PrefsAccount *account)
{
	FolderItem *inbox;
	FolderItem *dropfolder;
	gint val;

	if (account->inbox) {
		inbox = folder_find_item_from_path(account->inbox);
		if (!inbox)
			inbox = folder_get_default_inbox();
	} else
		inbox = folder_get_default_inbox();

	get_spool(inbox, account->local_mbox);
}

static void inc_all_spool(void)
{
	GList *list = NULL;

	list = account_get_list();
	if (!list) return;

	for (; list != NULL; list = list->next) {
		IncSession *session;
		PrefsAccount *account = list->data;

		if (account->protocol == A_LOCAL)
			inc_spool_account(account);
	}
}

static gint get_spool(FolderItem *dest, const gchar *mbox)
{
	gint msgs, size;
	gint lockfd;
	gchar tmp_mbox[MAXPATHLEN + 1];
	GHashTable *folder_table = NULL;

	g_return_val_if_fail(dest != NULL, -1);
	g_return_val_if_fail(mbox != NULL, -1);

	if (!is_file_exist(mbox) || (size = get_file_size(mbox)) == 0) {
		debug_print(_("no messages in local mailbox.\n"));
		return 0;
	} else if (size < 0)
		return -1;

	if ((lockfd = lock_mbox(mbox, LOCK_FLOCK)) < 0)
		return -1;

	g_snprintf(tmp_mbox, sizeof(tmp_mbox), "%s%ctmpmbox%d",
		   get_rc_dir(), G_DIR_SEPARATOR, (gint)mbox);

	if (copy_mbox(mbox, tmp_mbox) < 0)
		return -1;

	debug_print(_("Getting new messages from %s into %s...\n"),
		    mbox, dest->path);

	if (prefs_common.filter_on_inc)
		folder_table = g_hash_table_new(NULL, NULL);
	msgs = proc_mbox(dest, tmp_mbox, folder_table);

	unlink(tmp_mbox);
	if (msgs >= 0) empty_mbox(mbox);
	unlock_mbox(mbox, lockfd, LOCK_FLOCK);

	if (folder_table) {
		if (!prefs_common.scan_all_after_inc) {
		g_hash_table_insert(folder_table, dest,
				    GINT_TO_POINTER(1));
			folder_item_scan_foreach(folder_table);
			folderview_update_item_foreach(folder_table);
		}
		g_hash_table_destroy(folder_table);
	} else if (!prefs_common.scan_all_after_inc) {
		folder_item_scan(dest);
		folderview_update_item(dest, FALSE);
	}

	return msgs;
}

void inc_lock(void)
{
	inc_lock_count++;
}

void inc_unlock(void)
{
	if (inc_lock_count > 0)
		inc_lock_count--;
}

static guint autocheck_timer = 0;
static gpointer autocheck_data = NULL;

void inc_autocheck_timer_init(MainWindow *mainwin)
{
	autocheck_data = mainwin;
	inc_autocheck_timer_set();
}

static void inc_autocheck_timer_set_interval(guint interval)
{
	inc_autocheck_timer_remove();

	if (prefs_common.autochk_newmail && autocheck_data) {
		autocheck_timer = gtk_timeout_add
			(interval, inc_autocheck_func, autocheck_data);
		debug_print("added timer = %d\n", autocheck_timer);
	}
}

void inc_autocheck_timer_set(void)
{
	inc_autocheck_timer_set_interval(prefs_common.autochk_itv * 60000);
}

void inc_autocheck_timer_remove(void)
{
	if (autocheck_timer) {
		debug_print("removed timer = %d\n", autocheck_timer);
		gtk_timeout_remove(autocheck_timer);
		autocheck_timer = 0;
	}
}

static gint inc_autocheck_func(gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;

	if (inc_lock_count) {
		debug_print("autocheck is locked.\n");
		inc_autocheck_timer_set_interval(1000);
		return FALSE;
	}

	inc_all_account_mail(mainwin);

	return FALSE;
}
