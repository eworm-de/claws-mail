/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2003 Hiroyuki Yamamoto
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
#include <gtk/gtkrc.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>

#if USE_GPGME
#  include <gpgme.h>
#  include "passphrase.h"
#endif

#include "sylpheed.h"
#include "intl.h"
#include "main.h"
#include "mainwindow.h"
#include "folderview.h"
#include "summaryview.h"
#include "prefs_common.h"
#include "prefs_account.h"
#include "prefs_actions.h"
#include "prefs_fonts.h"
#include "prefs_spelling.h"
#include "scoring.h"
#include "prefs_display_header.h"
#include "account.h"
#include "procmsg.h"
#include "inc.h"
#include "import.h"
#include "manage_window.h"
#include "alertpanel.h"
#include "statusbar.h"
#include "addressbook.h"
#include "compose.h"
#include "folder.h"
#include "setup.h"
#include "utils.h"
#include "gtkutils.h"
#include "socket.h"
#include "log.h"
#include "prefs_toolbar.h"
#include "plugin.h"

#if USE_GPGME
#  include "rfc2015.h"
#endif
#if USE_OPENSSL
#  include "ssl.h"
#endif

#include "version.h"

#include "crash.h"

gchar *prog_version;
#ifdef CRASH_DIALOG
gchar *argv0;
#endif

static gint lock_socket = -1;
static gint lock_socket_tag = 0;

typedef enum 
{
	ONLINE_MODE_DONT_CHANGE,
 	ONLINE_MODE_ONLINE,
	ONLINE_MODE_OFFLINE
} OnlineMode;

static struct RemoteCmd {
	gboolean receive;
	gboolean receive_all;
	gboolean compose;
	const gchar *compose_mailto;
	GPtrArray *attach_files;
	gboolean status;
	gboolean status_full;
	GPtrArray *status_folders;
	GPtrArray *status_full_folders;
	gboolean send;
	gboolean crash;
	int online_mode;
	gchar   *crash_params;
} cmd;

static void parse_cmd_opt(int argc, char *argv[]);

static gint prohibit_duplicate_launch	(void);
static gchar * get_crashfile_name	(void);
static gint lock_socket_remove		(void);
static void lock_socket_input_cb	(gpointer	   data,
					 gint		   source,
					 GdkInputCondition condition);
#ifndef CLAWS					 
static 
#endif
gchar *get_socket_name		(void);


static void open_compose_new		(const gchar	*address,
					 GPtrArray	*attach_files);

static void send_queue			(void);
static void initial_processing		(FolderItem *item, gpointer data);
static void quit_signal_handler         (int sig);
static void install_basic_sighandlers   (void);

#if 0
/* for gettext */
_("File `%s' already exists.\n"
  "Can't create folder.")
#endif

#define MAKE_DIR_IF_NOT_EXIST(dir) \
{ \
	if (!is_dir_exist(dir)) { \
		if (is_file_exist(dir)) { \
			alertpanel_warning \
				(_("File `%s' already exists.\n" \
				   "Can't create folder."), \
				 dir); \
			return 1; \
		} \
		if (make_dir(dir) < 0) \
			return 1; \
	} \
}

static MainWindow *static_mainwindow;

int main(int argc, char *argv[])
{
	gchar *userrc;
	MainWindow *mainwin;
	FolderView *folderview;

	if (!sylpheed_init(&argc, &argv)) {
		return 0;
	}

	prog_version = PROG_VERSION;
#ifdef CRASH_DIALOG
	argv0 = g_strdup(argv[0]);
#endif

	parse_cmd_opt(argc, argv);

#ifdef CRASH_DIALOG
	if (cmd.crash) {
		gtk_set_locale();
		gtk_init(&argc, &argv);
		crash_main(cmd.crash_params);
		return 0;
	}
	crash_install_handlers();
#endif
	install_basic_sighandlers();

	/* check and create unix domain socket */
	lock_socket = prohibit_duplicate_launch();
	if (lock_socket < 0) return 0;

	if (cmd.status || cmd.status_full) {
		puts("0 Sylpheed not running.");
		lock_socket_remove();
		return 0;
	}

	gtk_set_locale();
	gtk_init(&argc, &argv);

	gdk_rgb_init();
	gtk_widget_set_default_colormap(gdk_rgb_get_cmap());
	gtk_widget_set_default_visual(gdk_rgb_get_visual());

#if USE_THREADS || USE_LDAP
	g_thread_init(NULL);
	if (!g_thread_supported())
		g_error(_("g_thread is not supported by glib.\n"));
#endif

	/* parse gtkrc files */
	userrc = g_strconcat(get_home_dir(), G_DIR_SEPARATOR_S, ".gtkrc",
			     NULL);
	gtk_rc_parse(userrc);
	g_free(userrc);
	userrc = g_strconcat(get_home_dir(), G_DIR_SEPARATOR_S, ".gtk",
			     G_DIR_SEPARATOR_S, "gtkrc", NULL);
	gtk_rc_parse(userrc);
	g_free(userrc);
	userrc = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, "gtkrc", NULL);
	gtk_rc_parse(userrc);
	g_free(userrc);

	gtk_rc_parse("./gtkrc");

	userrc = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, MENU_RC, NULL);
	gtk_item_factory_parse_rc(userrc);
	g_free(userrc);

	CHDIR_RETURN_VAL_IF_FAIL(get_home_dir(), 1);

	MAKE_DIR_IF_NOT_EXIST(RC_DIR);
	MAKE_DIR_IF_NOT_EXIST(get_imap_cache_dir());
	MAKE_DIR_IF_NOT_EXIST(get_news_cache_dir());
	MAKE_DIR_IF_NOT_EXIST(get_mime_tmp_dir());
	MAKE_DIR_IF_NOT_EXIST(get_tmp_dir());
	MAKE_DIR_IF_NOT_EXIST(RC_DIR G_DIR_SEPARATOR_S "uidl");

	set_log_file(RC_DIR G_DIR_SEPARATOR_S "sylpheed.log");

	folder_system_init();
	prefs_common_init();
	prefs_common_read_config();

#if USE_GPGME
	rfc2015_init();
#endif

	prefs_fonts_init();
#ifdef USE_ASPELL
	gtkaspell_checkers_init();
	prefs_spelling_init();
#endif
	
	sock_set_io_timeout(prefs_common.io_timeout_secs);

	prefs_actions_read_config();
	prefs_display_header_read_config();
	/* prefs_filtering_read_config(); */
	addressbook_read_file();
	renderer_read_config();

	gtkut_widget_init();

	mainwin = main_window_create
		(prefs_common.sep_folder | prefs_common.sep_msg << 1);
	folderview = mainwin->folderview;

	/* register the callback of unix domain socket input */
	lock_socket_tag = gdk_input_add(lock_socket,
					GDK_INPUT_READ | GDK_INPUT_EXCEPTION,
					lock_socket_input_cb,
					mainwin);

	account_read_config_all();

	if (folder_read_list() < 0) {
		setup(mainwin);
		folder_write_list();
	}
	if (!account_get_list()) {
		account_edit_open();
		account_add();
	}

	account_set_missing_folder();
	folder_set_missing_folders();
	folderview_set(folderview);

	/* prefs_scoring_read_config(); */
	prefs_matcher_read_config();

	/* make one all-folder processing before using sylpheed */
	folder_func_to_all_folders(initial_processing, (gpointer *)mainwin);

	/* if Sylpheed crashed, rebuild caches */
	if (!cmd.crash && is_file_exist(get_crashfile_name())) {
		debug_print("Sylpheed crashed, checking for new messages in local folders\n");
		folderview_check_new(NULL);
	}
	/* make the crash-indicator file */
	str_write_to_file("foo", get_crashfile_name());

	addressbook_read_file();

	inc_autocheck_timer_init(mainwin);

	/* ignore SIGPIPE signal for preventing sudden death of program */
	signal(SIGPIPE, SIG_IGN);

	if (cmd.receive_all)
		inc_all_account_mail(mainwin, FALSE, 
				     prefs_common.newmail_notify_manu);
	else if (prefs_common.chk_on_startup)
		inc_all_account_mail(mainwin, TRUE, 
				     prefs_common.newmail_notify_manu);
	else if (cmd.receive)
		inc_mail(mainwin, prefs_common.newmail_notify_manu);
	else
		gtk_widget_grab_focus(folderview->ctree);

	if (cmd.compose)
		open_compose_new(cmd.compose_mailto, cmd.attach_files);
	if (cmd.attach_files) {
		ptr_array_free_strings(cmd.attach_files);
		g_ptr_array_free(cmd.attach_files, TRUE);
		cmd.attach_files = NULL;
	}
	if (cmd.send)
		send_queue();
	if (cmd.status_folders) {
		g_ptr_array_free(cmd.status_folders, TRUE);
		cmd.status_folders = NULL;
	}
	if (cmd.status_full_folders) {
		g_ptr_array_free(cmd.status_full_folders, TRUE);
		cmd.status_full_folders = NULL;
	}

	if (cmd.online_mode == ONLINE_MODE_OFFLINE)
		main_window_toggle_work_offline(mainwin, TRUE);
	if (cmd.online_mode == ONLINE_MODE_ONLINE)
		main_window_toggle_work_offline(mainwin, FALSE);

	prefs_toolbar_init();

	plugin_load_all("GTK");
	
	static_mainwindow = mainwin;
	gtk_main();

	plugin_unload_all("GTK");

	prefs_toolbar_done();

	addressbook_destroy();

#ifdef USE_GPGME
	rfc2015_done();
#endif

	prefs_fonts_done();
#ifdef USE_ASPELL       
	prefs_spelling_done();
	gtkaspell_checkers_quit();
#endif
	sylpheed_done();

	return 0;
}

static void parse_cmd_opt(int argc, char *argv[])
{
	gint i;

	for (i = 1; i < argc; i++) {
		if (!strncmp(argv[i], "--receive-all", 13))
			cmd.receive_all = TRUE;
		else if (!strncmp(argv[i], "--receive", 9))
			cmd.receive = TRUE;
		else if (!strncmp(argv[i], "--compose", 9)) {
			const gchar *p = argv[i + 1];

			cmd.compose = TRUE;
			cmd.compose_mailto = NULL;
			if (p && *p != '\0' && *p != '-') {
				if (!strncmp(p, "mailto:", 7))
					cmd.compose_mailto = p + 7;
				else
					cmd.compose_mailto = p;
				i++;
			}
		} else if (!strncmp(argv[i], "--attach", 8)) {
			const gchar *p = argv[i + 1];
			gchar *file;

			while (p && *p != '\0' && *p != '-') {
				if (!cmd.attach_files)
					cmd.attach_files = g_ptr_array_new();
				if (*p != G_DIR_SEPARATOR)
					file = g_strconcat(sylpheed_get_startup_dir(),
							   G_DIR_SEPARATOR_S,
							   p, NULL);
				else
					file = g_strdup(p);
				g_ptr_array_add(cmd.attach_files, file);
				i++;
				p = argv[i + 1];
			}
		} else if (!strncmp(argv[i], "--send", 6)) {
			cmd.send = TRUE;
		} else if (!strncmp(argv[i], "--version", 9)) {
			puts("Sylpheed version " VERSION);
			exit(0);
 		} else if (!strncmp(argv[i], "--status-full", 13)) {
 			const gchar *p = argv[i + 1];
 
 			cmd.status_full = TRUE;
 			while (p && *p != '\0' && *p != '-') {
 				if (!cmd.status_full_folders)
 					cmd.status_full_folders =
 						g_ptr_array_new();
 				g_ptr_array_add(cmd.status_full_folders,
 						g_strdup(p));
 				i++;
 				p = argv[i + 1];
 			}
  		} else if (!strncmp(argv[i], "--status", 8)) {
 			const gchar *p = argv[i + 1];
 
  			cmd.status = TRUE;
 			while (p && *p != '\0' && *p != '-') {
 				if (!cmd.status_folders)
 					cmd.status_folders = g_ptr_array_new();
 				g_ptr_array_add(cmd.status_folders,
 						g_strdup(p));
 				i++;
 				p = argv[i + 1];
 			}
		} else if (!strncmp(argv[i], "--online", 8)) {
			cmd.online_mode = ONLINE_MODE_ONLINE;
		} else if (!strncmp(argv[i], "--offline", 9)) {
			cmd.online_mode = ONLINE_MODE_OFFLINE;
		} else if (!strncmp(argv[i], "--help", 6)) {
			g_print(_("Usage: %s [OPTION]...\n"),
				g_basename(argv[0]));

			puts(_("  --compose [address]    open composition window"));
			puts(_("  --attach file1 [file2]...\n"
			       "                         open composition window with specified files\n"
			       "                         attached"));
			puts(_("  --receive              receive new messages"));
			puts(_("  --receive-all          receive new messages of all accounts"));
			puts(_("  --send                 send all queued messages"));
 			puts(_("  --status [folder]...   show the total number of messages"));
 			puts(_("  --status-full [folder]...\n"
 			       "                         show the status of each folder"));
			puts(_("  --online               switch to online mode"));
			puts(_("  --offline              switch to offline mode"));
			puts(_("  --debug                debug mode"));
			puts(_("  --help                 display this help and exit"));
			puts(_("  --version              output version information and exit"));
			puts(_("  --config-dir           output configuration directory"));

			exit(1);
		} else if (!strncmp(argv[i], "--crash", 7)) {
			cmd.crash = TRUE;
			cmd.crash_params = g_strdup(argv[i + 1]);
			i++;
		} else if (!strncmp(argv[i], "--config-dir", sizeof "--config-dir" - 1)) {
			puts(RC_DIR);
			exit(0);
		}
		
	}

	if (cmd.attach_files && cmd.compose == FALSE) {
		cmd.compose = TRUE;
		cmd.compose_mailto = NULL;
	}
}

static gint get_queued_message_num(void)
{
	FolderItem *queue;

	queue = folder_get_default_queue();
	if (!queue) return -1;

	folder_item_scan(queue);
	return queue->total_msgs;
}

static void save_all_caches(FolderItem *item, gpointer data)
{
	if (!item->cache)
		return;
	folder_item_write_cache(item);
}

static void initial_processing(FolderItem *item, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	gchar *buf;

	g_return_if_fail(item);
	buf = g_strdup_printf(_("Processing (%s)..."), 
			      item->path 
			      ? item->path 
			      : _("top level folder"));
	debug_print("%s\n", buf);
	g_free(buf);

	main_window_cursor_wait(mainwin);
	
	folder_item_apply_processing(item);

	debug_print("done.\n");
	STATUSBAR_POP(mainwin);
	main_window_cursor_normal(mainwin);
}

static void draft_all_messages(void)
{
	GList *compose_list = compose_get_compose_list();
	GList *elem = NULL;
	
	if (compose_list) {
		for (elem = compose_list; elem != NULL && elem->data != NULL; 
		     elem = elem->next) {
			Compose *c = (Compose*)elem->data;
			compose_draft(c);
		}
	}	
}

void clean_quit(void)	
{
	/*!< Good idea to have the main window stored in a 
	 *   static variable so we can check that variable
	 *   to see if we're really allowed to do things
	 *   that actually the spawner is supposed to 
	 *   do (like: sending mail, composing messages).
	 *   Because, really, if we're the spawnee, and
	 *   we touch GTK stuff, we're hosed. See the 
	 *   next fixme. */

	/* FIXME: Use something else to signal that we're
	 * in the original spawner, and not in a spawned
	 * child. */
	if (!static_mainwindow) 
		return;
		
	draft_all_messages();

	if (prefs_common.warn_queued_on_exit) {	
		/* disable the popup */ 
		prefs_common.warn_queued_on_exit = FALSE;	
		app_will_exit(NULL, static_mainwindow);
		prefs_common.warn_queued_on_exit = TRUE;
		prefs_common_save_config();
	} else {
		app_will_exit(NULL, static_mainwindow);
	}
	exit(0);
}

void app_will_exit(GtkWidget *widget, gpointer data)
{
	MainWindow *mainwin = data;
	gchar *filename;
	GList *list;
	
	if (compose_get_compose_list()) {
		gint val = alertpanel(_("Notice"),
			       _("Composing message exists."),
			       _("Draft them"), _("Discard them"), _("Don't quit"));
		switch (val) {
			case G_ALERTOTHER:
				return;
			case G_ALERTALTERNATE:
				break;
			default:
				draft_all_messages();
		}
		
		manage_window_focus_in(mainwin->window, NULL, NULL);
	}

	if (prefs_common.warn_queued_on_exit && get_queued_message_num() > 0) {
		if (alertpanel(_("Queued messages"),
			       _("Some unsent messages are queued. Exit now?"),
			       _("OK"), _("Cancel"), NULL) != G_ALERTDEFAULT)
			return;
		manage_window_focus_in(mainwin->window, NULL, NULL);
	}

	inc_autocheck_timer_remove();

	if (prefs_common.clean_on_exit)
		main_window_empty_trash(mainwin, prefs_common.ask_on_clean);

	/* save prefs for opened folder */
	if(mainwin->folderview->opened)
	{
		FolderItem *item;

		item = gtk_ctree_node_get_row_data(GTK_CTREE(mainwin->folderview->ctree), mainwin->folderview->opened);
		summary_save_prefs_to_folderitem(mainwin->folderview->summaryview, item);
	}

	/* save all state before exiting */
	folder_write_list();
	folder_func_to_all_folders(save_all_caches, NULL);
	for (list = folder_get_list(); list != NULL; list = g_list_next(list)) {
		Folder *folder = FOLDER(list->data);

		folder_tree_destroy(folder);
	}

	main_window_get_size(mainwin);
	main_window_get_position(mainwin);
	prefs_common_save_config();
	account_save_config_all();
	addressbook_export_to_file();

	filename = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, MENU_RC, NULL);
	gtk_item_factory_dump_rc(filename, NULL, TRUE);
	g_free(filename);

	/* delete temporary files */
	remove_all_files(get_mime_tmp_dir());

	close_log_file();

	/* delete crashfile */
	if (!cmd.crash)
		unlink(get_crashfile_name());

	lock_socket_remove();

	gtk_main_quit();
}

/*
 * CLAWS: want this public so crash dialog can delete the
 * lock file too
 */
#ifndef CLAWS
static
#endif
gchar *get_socket_name(void)
{
	static gchar *filename = NULL;

	if (filename == NULL) {
		filename = g_strdup_printf("%s%csylpheed-%d",
					   g_get_tmp_dir(), G_DIR_SEPARATOR,
					   getuid());
	}

	return filename;
}

static gchar *get_crashfile_name(void)
{
	static gchar *filename = NULL;

	if (filename == NULL) {
		filename = g_strdup_printf("%s%csylpheed-crashed",
					   get_tmp_dir(), G_DIR_SEPARATOR);
	}

	return filename;
}

static gint prohibit_duplicate_launch(void)
{
	gint uxsock;
	gchar *path;

	path = get_socket_name();
	uxsock = fd_connect_unix(path);
	if (uxsock < 0) {
		unlink(path);
		return fd_open_unix(path);
	}

	/* remote command mode */

	debug_print("another Sylpheed is already running.\n");

	if (cmd.receive_all)
		fd_write_all(uxsock, "receive_all\n", 12);
	else if (cmd.receive)
		fd_write_all(uxsock, "receive\n", 8);
	else if (cmd.compose && cmd.attach_files) {
		gchar *str, *compose_str;
		gint i;

		if (cmd.compose_mailto)
			compose_str = g_strdup_printf("compose_attach %s\n",
						      cmd.compose_mailto);
		else
			compose_str = g_strdup("compose_attach\n");

		fd_write_all(uxsock, compose_str, strlen(compose_str));
		g_free(compose_str);

		for (i = 0; i < cmd.attach_files->len; i++) {
			str = g_ptr_array_index(cmd.attach_files, i);
			fd_write_all(uxsock, str, strlen(str));
			fd_write_all(uxsock, "\n", 1);
		}

		fd_write_all(uxsock, ".\n", 2);
	} else if (cmd.compose) {
		gchar *compose_str;

		if (cmd.compose_mailto)
			compose_str = g_strdup_printf
				("compose %s\n", cmd.compose_mailto);
		else
			compose_str = g_strdup("compose\n");

		fd_write_all(uxsock, compose_str, strlen(compose_str));
		g_free(compose_str);
	} else if (cmd.send) {
		fd_write_all(uxsock, "send\n", 5);
	} else if (cmd.online_mode == ONLINE_MODE_ONLINE) {
		fd_write(uxsock, "online\n", 6);
	} else if (cmd.online_mode == ONLINE_MODE_OFFLINE) {
		fd_write(uxsock, "offline\n", 7);
 	} else if (cmd.status || cmd.status_full) {
  		gchar buf[BUFFSIZE];
 		gint i;
 		const gchar *command;
 		GPtrArray *folders;
 		gchar *folder;
 
 		command = cmd.status_full ? "status-full\n" : "status\n";
 		folders = cmd.status_full ? cmd.status_full_folders :
 			cmd.status_folders;
 
 		fd_write_all(uxsock, command, strlen(command));
 		for (i = 0; folders && i < folders->len; ++i) {
 			folder = g_ptr_array_index(folders, i);
 			fd_write_all(uxsock, folder, strlen(folder));
 			fd_write_all(uxsock, "\n", 1);
 		}
 		fd_write_all(uxsock, ".\n", 2);
 		for (;;) {
 			fd_gets(uxsock, buf, sizeof(buf));
 			if (!strncmp(buf, ".\n", 2)) break;
 			fputs(buf, stdout);
 		}
	} else
		fd_write_all(uxsock, "popup\n", 6);

	fd_close(uxsock);
	return -1;
}

static gint lock_socket_remove(void)
{
	gchar *filename;

	if (lock_socket < 0) return -1;

	if (lock_socket_tag > 0)
		gdk_input_remove(lock_socket_tag);
	fd_close(lock_socket);
	filename = get_socket_name();
	unlink(filename);

	return 0;
}

static GPtrArray *get_folder_item_list(gint sock)
{
	gchar buf[BUFFSIZE];
	FolderItem *item;
	GPtrArray *folders = NULL;

	for (;;) {
		fd_gets(sock, buf, sizeof(buf));
		if (!strncmp(buf, ".\n", 2)) break;
		strretchomp(buf);
		if (!folders) folders = g_ptr_array_new();
		item = folder_find_item_from_identifier(buf);
		if (item)
			g_ptr_array_add(folders, item);
		else
			g_warning("no such folder: %s\n", buf);
	}

	return folders;
}

static void lock_socket_input_cb(gpointer data,
				 gint source,
				 GdkInputCondition condition)
{
	MainWindow *mainwin = (MainWindow *)data;
	gint sock;
	gchar buf[BUFFSIZE];

	sock = fd_accept(source);
	fd_gets(sock, buf, sizeof(buf));

	if (!strncmp(buf, "popup", 5)) {
		main_window_popup(mainwin);
	} else if (!strncmp(buf, "receive_all", 11)) {
		inc_all_account_mail(mainwin, FALSE,
				     prefs_common.newmail_notify_manu);
	} else if (!strncmp(buf, "receive", 7)) {
		inc_mail(mainwin, prefs_common.newmail_notify_manu);
	} else if (!strncmp(buf, "compose_attach", 14)) {
		GPtrArray *files;
		gchar *mailto;

		mailto = g_strdup(buf + strlen("compose_attach") + 1);
		files = g_ptr_array_new();
		while (fd_gets(sock, buf, sizeof(buf)) > 0) {
			if (buf[0] == '.' && buf[1] == '\n') break;
			strretchomp(buf);
			g_ptr_array_add(files, g_strdup(buf));
		}
		open_compose_new(mailto, files);
		ptr_array_free_strings(files);
		g_ptr_array_free(files, TRUE);
		g_free(mailto);
	} else if (!strncmp(buf, "compose", 7)) {
		open_compose_new(buf + strlen("compose") + 1, NULL);
	} else if (!strncmp(buf, "send", 4)) {
		send_queue();
	} else if (!strncmp(buf, "online", 6)) {
		main_window_toggle_work_offline(mainwin, FALSE);
	} else if (!strncmp(buf, "offline", 7)) {
		main_window_toggle_work_offline(mainwin, TRUE);
 	} else if (!strncmp(buf, "status-full", 11) ||
 		   !strncmp(buf, "status", 6)) {
 		gchar *status;
 		GPtrArray *folders;
 
 		folders = get_folder_item_list(sock);
 		status = folder_get_status
 			(folders, !strncmp(buf, "status-full", 11));
 		fd_write_all(sock, status, strlen(status));
 		fd_write_all(sock, ".\n", 2);
 		g_free(status);
 		if (folders) g_ptr_array_free(folders, TRUE);
	}

	fd_close(sock);
}

static void open_compose_new(const gchar *address, GPtrArray *attach_files)
{
	gchar *addr = NULL;

	if (address) {
		Xstrdup_a(addr, address, return);
		g_strstrip(addr);
	}

	compose_new(NULL, addr, attach_files);
}

static void send_queue(void)
{
	GList *list;
	FolderItem *def_outbox;

	def_outbox = folder_get_default_outbox();

	for (list = folder_get_list(); list != NULL; list = list->next) {
		Folder *folder = list->data;

		if (folder->queue) {
			gint res = procmsg_send_queue
				(folder->queue, prefs_common.savemsg);

			if (res < 0)	
				alertpanel_error(_("Some errors occurred while sending queued messages."));
			if (res) 	
				folder_item_scan(folder->queue);
			if (prefs_common.savemsg && folder->outbox) {
				if (folder->outbox == def_outbox)
					def_outbox = NULL;
			}
		}
	}
}

static void quit_signal_handler(int sig)
{
	debug_print("Quitting on signal %d\n", sig);
	clean_quit();
}

static void install_basic_sighandlers()
{
	sigset_t    mask;
	struct sigaction act;

	sigemptyset(&mask);

#ifdef SIGTERM
	sigaddset(&mask, SIGTERM);
#endif
#ifdef SIGINT
	sigaddset(&mask, SIGINT);
#endif

	act.sa_handler = quit_signal_handler;
	act.sa_mask    = mask;
	act.sa_flags   = 0;

#ifdef SIGTERM
	sigaction(SIGTERM, &act, 0);
#endif
#ifdef SIGINT
	sigaction(SIGINT, &act, 0);
#endif	

	sigprocmask(SIG_UNBLOCK, &mask, 0);
}
