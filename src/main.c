/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2006 Hiroyuki Yamamoto and the Sylpheed-Claws team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
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
#ifdef G_OS_UNIX
#  include <signal.h>
#endif
#include "wizard.h"
#ifdef HAVE_STARTUP_NOTIFICATION
# define SN_API_NOT_YET_FROZEN
# include <libsn/sn-launchee.h>
# include <gdk/gdkx.h>
#endif

#include "sylpheed.h"
#include "main.h"
#include "mainwindow.h"
#include "folderview.h"
#include "image_viewer.h"
#include "summaryview.h"
#include "prefs_common.h"
#include "prefs_account.h"
#include "prefs_actions.h"
#include "prefs_ext_prog.h"
#include "prefs_fonts.h"
#include "prefs_image_viewer.h"
#include "prefs_message.h"
#include "prefs_receive.h"
#include "prefs_msg_colors.h"
#include "prefs_quote.h"
#include "prefs_spelling.h"
#include "prefs_summaries.h"
#include "prefs_themes.h"
#include "prefs_other.h"
#include "prefs_send.h"
#include "prefs_wrapping.h"
#include "prefs_compose_writing.h"
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
#include "mh_gtk.h"
#include "imap_gtk.h"
#include "news_gtk.h"
#include "matcher.h"
#ifdef HAVE_LIBETPAN
#include "imap-thread.h"
#endif
#include "stock_pixmap.h"

#if USE_OPENSSL
#  include "ssl.h"
#endif

#include "version.h"

#include "crash.h"

gchar *prog_version;
#ifdef CRASH_DIALOG
gchar *argv0;
#endif

#ifdef HAVE_STARTUP_NOTIFICATION
static SnLauncheeContext *sn_context = NULL;
static SnDisplay *sn_display = NULL;
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
	gboolean exit;
	gboolean subscribe;
	const gchar *subscribe_uri;
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
static void exit_sylpheed		(MainWindow *mainwin);

#define MAKE_DIR_IF_NOT_EXIST(dir) \
{ \
	if (!is_dir_exist(dir)) { \
		if (is_file_exist(dir)) { \
			alertpanel_warning \
				(_("File '%s' already exists.\n" \
				   "Can't create folder."), \
				 dir); \
			return 1; \
		} \
		if (make_dir(dir) < 0) \
			return 1; \
	} \
}

static MainWindow *static_mainwindow;

#ifdef HAVE_STARTUP_NOTIFICATION
static void sn_error_trap_push(SnDisplay *display, Display *xdisplay)
{
	gdk_error_trap_push();
}

static void sn_error_trap_pop(SnDisplay *display, Display *xdisplay)
{
	gdk_error_trap_pop();
}

static void startup_notification_complete(gboolean with_window)
{
	Display *xdisplay;
	GtkWidget *hack = NULL;

	if (with_window) {
		/* this is needed to make the startup notification leave,
		 * if we have been launched from a menu.
		 * We have to display a window, so let it be very little */
		hack = gtk_window_new(GTK_WINDOW_POPUP);
		gtk_widget_set_uposition(hack, 0, 0);
		gtk_widget_set_size_request(hack, 1, 1);
		gtk_widget_show(hack);
	}

	xdisplay = GDK_DISPLAY();
	sn_display = sn_display_new(xdisplay,
				sn_error_trap_push,
				sn_error_trap_pop);
	sn_context = sn_launchee_context_new_from_environment(sn_display,
						 DefaultScreen(xdisplay));

	if (sn_context != NULL)	{
		sn_launchee_context_complete(sn_context);
		sn_launchee_context_unref(sn_context);
		sn_display_unref(sn_display);
	}
	if (with_window) {
		gtk_widget_destroy(hack);
	}
}
#endif /* HAVE_STARTUP_NOTIFICATION */

void sylpheed_gtk_idle(void) 
{
	while(gtk_events_pending()) {
		gtk_main_iteration();
	}
	g_usleep(50000);
}

gboolean defer_check_all(void *data)
{
	gboolean autochk = GPOINTER_TO_INT(data);

	inc_all_account_mail(static_mainwindow, autochk, 
			prefs_common.newmail_notify_manu);

	return FALSE;
}

gboolean defer_check(void *data)
{
	inc_mail(static_mainwindow, prefs_common.newmail_notify_manu);

	return FALSE;
}

static gboolean migrate_old_config(const gchar *old_cfg_dir, const gchar *new_cfg_dir)
{
	gchar *message = g_strdup_printf(_("Configuration for Sylpheed-Claws %s found.\n"
			 "Do you want to migrate this configuration?"),
			 !strcmp(old_cfg_dir, OLD_GTK1_RC_DIR)?
			 	_("1.0.5 or previous"):_("1.9.15 or previous"));
	gint r = 0;
	GtkWidget *window = NULL;
	if (alertpanel(_("Migration of configuration"),
		       message,
		       GTK_STOCK_NO, GTK_STOCK_YES, NULL) != G_ALERTALTERNATE) {
		return FALSE;
	}
	
	window = label_window_create(_("Copying configuration..."));
	GTK_EVENTS_FLUSH();
	r = copy_dir(old_cfg_dir, new_cfg_dir);
	gtk_widget_destroy(window);
	if (r != 0) {
		alertpanel_error(_("Migration failed!"));
	}
	return (r == 0);
}

int main(int argc, char *argv[])
{
	gchar *userrc;
	MainWindow *mainwin;
	FolderView *folderview;
	GdkPixbuf *icon;
	gboolean crash_file_present = FALSE;

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
	sock_init();

	/* check and create unix domain socket for remote operation */
#ifdef G_OS_UNIX
	lock_socket = prohibit_duplicate_launch();
	if (lock_socket < 0) {
#ifdef HAVE_STARTUP_NOTIFICATION
		if(gtk_init_check(&argc, &argv))
			startup_notification_complete(TRUE);
#endif
		return 0;
	}

	if (cmd.status || cmd.status_full) {
		puts("0 Sylpheed-Claws not running.");
		lock_socket_remove();
		return 0;
	}
	
	if (cmd.exit)
		return 0;
#endif
	g_thread_init(NULL);
	/* gdk_threads_init(); */

	gtk_set_locale();
	gtk_init(&argc, &argv);

	gdk_rgb_init();
	gtk_widget_set_default_colormap(gdk_rgb_get_colormap());
	gtk_widget_set_default_visual(gdk_rgb_get_visual());

	if (!g_thread_supported()) {
		g_error(_("g_thread is not supported by glib.\n"));
	}

	/* parse gtkrc files */
	userrc = g_strconcat(get_home_dir(), G_DIR_SEPARATOR_S, ".gtkrc-2.0",
			     NULL);
	gtk_rc_parse(userrc);
	g_free(userrc);
	userrc = g_strconcat(get_home_dir(), G_DIR_SEPARATOR_S, ".gtk",
			     G_DIR_SEPARATOR_S, "gtkrc-2.0", NULL);
	gtk_rc_parse(userrc);
	g_free(userrc);

	CHDIR_RETURN_VAL_IF_FAIL(get_home_dir(), 1);
	if (!is_dir_exist(RC_DIR)) {
		gboolean r = FALSE;
		if (is_dir_exist(OLD_GTK2_RC_DIR))
			r = migrate_old_config(OLD_GTK2_RC_DIR, RC_DIR);
		else if (is_dir_exist(OLD_GTK1_RC_DIR))
			r = migrate_old_config(OLD_GTK1_RC_DIR, RC_DIR);
		if (r == FALSE && !is_dir_exist(RC_DIR) && make_dir(RC_DIR) < 0)
			exit(1);
	}

	userrc = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, "gtkrc-2.0", NULL);
	gtk_rc_parse(userrc);
	g_free(userrc);

	userrc = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, MENU_RC, NULL);
	gtk_accel_map_load (userrc);
	g_free(userrc);

	gtk_settings_set_long_property(gtk_settings_get_default(), 
				       "gtk-can-change-accels",
				       (glong)TRUE, "XProperty");

	CHDIR_RETURN_VAL_IF_FAIL(get_rc_dir(), 1);

	MAKE_DIR_IF_NOT_EXIST(get_mail_base_dir());
	MAKE_DIR_IF_NOT_EXIST(get_imap_cache_dir());
	MAKE_DIR_IF_NOT_EXIST(get_news_cache_dir());
	MAKE_DIR_IF_NOT_EXIST(get_mime_tmp_dir());
	MAKE_DIR_IF_NOT_EXIST(get_tmp_dir());
	MAKE_DIR_IF_NOT_EXIST(UIDL_DIR);

	crash_file_present = is_file_exist(get_crashfile_name());
	/* remove temporary files */
	remove_all_files(get_tmp_dir());
	remove_all_files(get_mime_tmp_dir());

	if (is_file_exist("sylpheed.log")) {
		if (rename_force("sylpheed.log", "sylpheed.log.bak") < 0)
			FILE_OP_ERROR("sylpheed.log", "rename");
	}
	set_log_file("sylpheed.log");

	CHDIR_RETURN_VAL_IF_FAIL(get_home_dir(), 1);

	folder_system_init();
	prefs_common_read_config();

	prefs_themes_init();
	prefs_fonts_init();
	prefs_ext_prog_init();
	prefs_wrapping_init();
	prefs_compose_writing_init();
	prefs_msg_colors_init();
	image_viewer_init();
	prefs_image_viewer_init();
	prefs_quote_init();
	prefs_summaries_init();
	prefs_message_init();
	prefs_other_init();
	prefs_receive_init();
	prefs_send_init();
#ifdef USE_ASPELL
	gtkaspell_checkers_init();
	prefs_spelling_init();
#endif
	
	sock_set_io_timeout(prefs_common.io_timeout_secs);
#ifdef HAVE_LIBETPAN
	imap_main_set_timeout(prefs_common.io_timeout_secs);
#endif
	prefs_actions_read_config();
	prefs_display_header_read_config();
	/* prefs_filtering_read_config(); */
	addressbook_read_file();
	renderer_read_config();

	gtkut_widget_init();
	stock_pixbuf_gdk(NULL, STOCK_PIXMAP_SYLPHEED_ICON, &icon);
	gtk_window_set_default_icon(icon);

	folderview_initialize();

	mh_gtk_init();
	imap_gtk_init();
	news_gtk_init();
		
	mainwin = main_window_create
		(prefs_common.sep_folder | prefs_common.sep_msg << 1);
	folderview = mainwin->folderview;

	gtk_clist_freeze(GTK_CLIST(mainwin->folderview->ctree));
	folder_item_update_freeze();

	/* register the callback of unix domain socket input */
#ifdef G_OS_UNIX
	lock_socket_tag = gdk_input_add(lock_socket,
					GDK_INPUT_READ | GDK_INPUT_EXCEPTION,
					lock_socket_input_cb,
					mainwin);
#endif

	prefs_account_init();
	account_read_config_all();

	if (folder_read_list() < 0) {
		if (!run_wizard(mainwin, TRUE))
			exit(1);
		folder_write_list();
	}

	if (!account_get_list()) {
		if (!run_wizard(mainwin, FALSE))
			exit(1);
		account_read_config_all();
		if(!account_get_list())
			exit_sylpheed(mainwin);
	}

	main_window_popup(mainwin);

#ifdef HAVE_LIBETPAN
	imap_main_init();
#endif	
	account_set_missing_folder();
	folder_set_missing_folders();
	folderview_set(folderview);

	prefs_matcher_read_config();

	/* make one all-folder processing before using sylpheed */
	main_window_cursor_wait(mainwin);
	folder_func_to_all_folders(initial_processing, (gpointer *)mainwin);

	/* if Sylpheed crashed, rebuild caches */
	if (!cmd.crash && crash_file_present) {
		debug_print("Sylpheed-Claws crashed, checking for new messages in local folders\n");
		folder_item_update_thaw();
		folderview_check_new(NULL);
		folder_clean_cache_memory_force();
		folder_item_update_freeze();
	}
	/* make the crash-indicator file */
	str_write_to_file("foo", get_crashfile_name());

	inc_autocheck_timer_init(mainwin);

	/* ignore SIGPIPE signal for preventing sudden death of program */
#ifdef G_OS_UNIX
	signal(SIGPIPE, SIG_IGN);
#endif
	if (cmd.online_mode == ONLINE_MODE_OFFLINE) {
		main_window_toggle_work_offline(mainwin, TRUE, FALSE);
	}
	if (cmd.online_mode == ONLINE_MODE_ONLINE) {
		main_window_toggle_work_offline(mainwin, FALSE, FALSE);
	}

	if (cmd.status_folders) {
		g_ptr_array_free(cmd.status_folders, TRUE);
		cmd.status_folders = NULL;
	}
	if (cmd.status_full_folders) {
		g_ptr_array_free(cmd.status_full_folders, TRUE);
		cmd.status_full_folders = NULL;
	}

	sylpheed_register_idle_function(sylpheed_gtk_idle);

	prefs_toolbar_init();

	plugin_load_all("GTK2");
	
	if (plugin_get_unloaded_list() != NULL) {
		alertpanel_warning(_("Some plugin(s) failed to load. "
				     "Check the Plugins configuration "
				     "for more information."));
	}
	
	if (!folder_have_mailbox()) {
		alertpanel_error(_("Sylpheed-Claws has detected a configured "
				   "mailbox, but could not load it. It is "
				   "probably provided by an out-of-date "
				   "external plugin. Please reinstall the "
				   "plugin and try again."));
		exit(1);
	}
	
	static_mainwindow = mainwin;

#ifdef HAVE_STARTUP_NOTIFICATION
	startup_notification_complete(FALSE);
#endif
	folder_item_update_thaw();
	gtk_clist_thaw(GTK_CLIST(mainwin->folderview->ctree));
	main_window_cursor_normal(mainwin);

	if (cmd.receive_all) {
		g_timeout_add(1000, defer_check_all, GINT_TO_POINTER(FALSE));
	} else if (prefs_common.chk_on_startup) {
		g_timeout_add(1000, defer_check_all, GINT_TO_POINTER(TRUE));
	} else if (cmd.receive) {
		g_timeout_add(1000, defer_check, NULL);
	} else {
		gtk_widget_grab_focus(folderview->ctree);
	}

	if (cmd.compose) {
		open_compose_new(cmd.compose_mailto, cmd.attach_files);
	}
	if (cmd.attach_files) {
		ptr_array_free_strings(cmd.attach_files);
		g_ptr_array_free(cmd.attach_files, TRUE);
		cmd.attach_files = NULL;
	}
	if (cmd.subscribe) {
		folder_subscribe(cmd.subscribe_uri);
	}

	if (cmd.send) {
		send_queue();
	}
	
	gtk_main();

	exit_sylpheed(mainwin);

	return 0;
}

static void save_all_caches(FolderItem *item, gpointer data)
{
	if (!item->cache) {
		return;
	}
	folder_item_write_cache(item);
}

static void exit_sylpheed(MainWindow *mainwin)
{
	gchar *filename;

	debug_print("shutting down\n");
	inc_autocheck_timer_remove();

	if (prefs_common.clean_on_exit) {
		main_window_empty_trash(mainwin, prefs_common.ask_on_clean);
	}

	/* save prefs for opened folder */
	if(mainwin->folderview->opened) {
		FolderItem *item;

		item = gtk_ctree_node_get_row_data(GTK_CTREE(mainwin->folderview->ctree), mainwin->folderview->opened);
		summary_save_prefs_to_folderitem(mainwin->folderview->summaryview, item);
	}

	/* save all state before exiting */
	folder_write_list();
	folder_func_to_all_folders(save_all_caches, NULL);

	main_window_get_size(mainwin);
	main_window_get_position(mainwin);
	prefs_common_write_config();
	account_write_config_all();
	addressbook_export_to_file();

	filename = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, MENU_RC, NULL);
	gtk_accel_map_save(filename);
	g_free(filename);

	/* delete temporary files */
	remove_all_files(get_tmp_dir());
	remove_all_files(get_mime_tmp_dir());

	close_log_file();

#ifdef HAVE_LIBETPAN
	imap_main_done();
#endif
	/* delete crashfile */
	if (!cmd.crash)
		g_unlink(get_crashfile_name());

	lock_socket_remove();

	main_window_destroy_all();
	
	plugin_unload_all("GTK2");

	prefs_toolbar_done();

	addressbook_destroy();

	prefs_themes_done();
	prefs_fonts_done();
	prefs_ext_prog_done();
	prefs_wrapping_done();
	prefs_compose_writing_done();
	prefs_msg_colors_done();
	prefs_image_viewer_done();
	image_viewer_done();
	prefs_quote_done();
	prefs_summaries_done();
	prefs_message_done();
	prefs_other_done();
	prefs_receive_done();
	prefs_send_done();
#ifdef USE_ASPELL       
	prefs_spelling_done();
	gtkaspell_checkers_quit();
#endif
	sylpheed_done();

}

static void parse_cmd_opt(int argc, char *argv[])
{
	gint i;

	for (i = 1; i < argc; i++) {
		if (!strncmp(argv[i], "--receive-all", 13)) {
			cmd.receive_all = TRUE;
		} else if (!strncmp(argv[i], "--receive", 9)) {
			cmd.receive = TRUE;
		} else if (!strncmp(argv[i], "--compose", 9)) {
			const gchar *p = argv[i + 1];

			cmd.compose = TRUE;
			cmd.compose_mailto = NULL;
			if (p && *p != '\0' && *p != '-') {
				if (!strncmp(p, "mailto:", 7)) {
					cmd.compose_mailto = p + 7;
				} else {
					cmd.compose_mailto = p;
				}
				i++;
			}
		} else if (!strncmp(argv[i], "--subscribe", 11)) {
			const gchar *p = argv[i + 1];
			if (p && *p != '\0' && *p != '-') {
				cmd.subscribe = TRUE;
				cmd.subscribe_uri = p;
			}
		} else if (!strncmp(argv[i], "--attach", 8)) {
			const gchar *p = argv[i + 1];
			gchar *file;

			while (p && *p != '\0' && *p != '-') {
				if (!cmd.attach_files) {
					cmd.attach_files = g_ptr_array_new();
				}
				if (*p != G_DIR_SEPARATOR) {
					file = g_strconcat(sylpheed_get_startup_dir(),
							   G_DIR_SEPARATOR_S,
							   p, NULL);
				} else {
					file = g_strdup(p);
				}
				g_ptr_array_add(cmd.attach_files, file);
				i++;
				p = argv[i + 1];
			}
		} else if (!strncmp(argv[i], "--send", 6)) {
			cmd.send = TRUE;
		} else if (!strncmp(argv[i], "--version", 9)) {
			puts("Sylpheed-Claws version " VERSION);
			exit(0);
 		} else if (!strncmp(argv[i], "--status-full", 13)) {
 			const gchar *p = argv[i + 1];
 
 			cmd.status_full = TRUE;
 			while (p && *p != '\0' && *p != '-') {
 				if (!cmd.status_full_folders) {
 					cmd.status_full_folders =
 						g_ptr_array_new();
				}
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
			gchar *base = g_path_get_basename(argv[0]);
			g_print(_("Usage: %s [OPTION]...\n"), base);

			g_print("%s\n", _("  --compose [address]    open composition window"));
			g_print("%s\n", _("  --subscribe [uri]      subscribe to the given URI if possible"));
			g_print("%s\n", _("  --attach file1 [file2]...\n"
			          "                         open composition window with specified files\n"
			          "                         attached"));
			g_print("%s\n", _("  --receive              receive new messages"));
			g_print("%s\n", _("  --receive-all          receive new messages of all accounts"));
			g_print("%s\n", _("  --send                 send all queued messages"));
 			g_print("%s\n", _("  --status [folder]...   show the total number of messages"));
 			g_print("%s\n", _("  --status-full [folder]...\n"
 			       "                         show the status of each folder"));
			g_print("%s\n", _("  --online               switch to online mode"));
			g_print("%s\n", _("  --offline              switch to offline mode"));
			g_print("%s\n", _("  --exit                 exit Sylpheed-Claws"));
			g_print("%s\n", _("  --debug                debug mode"));
			g_print("%s\n", _("  --help                 display this help and exit"));
			g_print("%s\n", _("  --version              output version information and exit"));
			g_print("%s\n", _("  --config-dir           output configuration directory"));

			g_free(base);
			exit(1);
		} else if (!strncmp(argv[i], "--crash", 7)) {
			cmd.crash = TRUE;
			cmd.crash_params = g_strdup(argv[i + 1]);
			i++;
		} else if (!strncmp(argv[i], "--config-dir", sizeof "--config-dir" - 1)) {
			puts(RC_DIR);
			exit(0);
		} else if (!strncmp(argv[i], "--exit", 6)) {
			cmd.exit = TRUE;
		} else if (i == 1 && argc == 2) {
			/* only one parameter. Do something intelligent about it */
			if (strstr(argv[i], "@") && !strstr(argv[i], "://")) {
				const gchar *p = argv[i];

				cmd.compose = TRUE;
				cmd.compose_mailto = NULL;
				if (p && *p != '\0' && *p != '-') {
					if (!strncmp(p, "mailto:", 7)) {
						cmd.compose_mailto = p + 7;
					} else {
						cmd.compose_mailto = p;
					}
				}
			} else if (strstr(argv[i], "://")) {
				const gchar *p = argv[i];
				if (p && *p != '\0' && *p != '-') {
					cmd.subscribe = TRUE;
					cmd.subscribe_uri = p;
				}
			}
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
	if (!queue) {
		return -1;
	}

	folder_item_scan(queue);
	return queue->total_msgs;
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

	
	if (item->prefs->enable_processing) {
		folder_item_apply_processing(item);
	}

	debug_print("done.\n");
	STATUSBAR_POP(mainwin);
}

static void draft_all_messages(void)
{
	GList *compose_list = NULL;
	
	while ((compose_list = compose_get_compose_list()) != NULL) {
		Compose *c = (Compose*)compose_list->data;
		compose_draft(c);
	}	
}

gboolean clean_quit(gpointer data)
{
	static gboolean firstrun = TRUE;

	if (!firstrun) {
		return FALSE;
	}
	firstrun = FALSE;

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
	if (!static_mainwindow) {
		return FALSE;
	}
		
	draft_all_messages();

	exit_sylpheed(static_mainwindow);
	exit(0);

	return FALSE;
}

void app_will_exit(GtkWidget *widget, gpointer data)
{
	MainWindow *mainwin = data;

	if (compose_get_compose_list()) {
		gint val = alertpanel(_("Really quit?"),
			       _("Composing message exists."),
			       _("_Save to Draft"), _("_Discard them"), _("Do_n't quit"));
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
			       GTK_STOCK_CANCEL, GTK_STOCK_OK, NULL)
		    != G_ALERTALTERNATE) {
			return;
		}
		manage_window_focus_in(mainwin->window, NULL, NULL);
	}

	sock_cleanup();

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
		filename = g_strdup_printf("%s%csylpheed-claws-%d",
					   g_get_tmp_dir(), G_DIR_SEPARATOR,
#if HAVE_GETUID
					   getuid());
#else
					   0);						
#endif
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
		g_unlink(path);
		return fd_open_unix(path);
	}

	/* remote command mode */

	debug_print("another Sylpheed-Claws instance is already running.\n");

	if (cmd.receive_all) {
		fd_write_all(uxsock, "receive_all\n", 12);
	} else if (cmd.receive) {
		fd_write_all(uxsock, "receive\n", 8);
	} else if (cmd.compose && cmd.attach_files) {
		gchar *str, *compose_str;
		gint i;

		if (cmd.compose_mailto) {
			compose_str = g_strdup_printf("compose_attach %s\n",
						      cmd.compose_mailto);
		} else {
			compose_str = g_strdup("compose_attach\n");
		}

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

		if (cmd.compose_mailto) {
			compose_str = g_strdup_printf
				("compose %s\n", cmd.compose_mailto);
		} else {
			compose_str = g_strdup("compose\n");
		}

		fd_write_all(uxsock, compose_str, strlen(compose_str));
		g_free(compose_str);
	} else if (cmd.subscribe) {
		gchar *str = g_strdup_printf("subscribe %s\n", cmd.subscribe_uri);
		fd_write_all(uxsock, str, strlen(str));
		g_free(str);
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
	} else if (cmd.exit) {
		fd_write_all(uxsock, "exit\n", 5);
	} else
		fd_write_all(uxsock, "popup\n", 6);

	fd_close(uxsock);
	return -1;
}

static gint lock_socket_remove(void)
{
#ifdef G_OS_UNIX
	gchar *filename;

	if (lock_socket < 0) {
		return -1;
	}

	if (lock_socket_tag > 0) {
		gdk_input_remove(lock_socket_tag);
	}
	fd_close(lock_socket);
	filename = get_socket_name();
	g_unlink(filename);
#endif

	return 0;
}

static GPtrArray *get_folder_item_list(gint sock)
{
	gchar buf[BUFFSIZE];
	FolderItem *item;
	GPtrArray *folders = NULL;

	for (;;) {
		fd_gets(sock, buf, sizeof(buf));
		if (!strncmp(buf, ".\n", 2)) {
			break;
		}
		strretchomp(buf);
		if (!folders) {
			folders = g_ptr_array_new();
		}
		item = folder_find_item_from_identifier(buf);
		if (item) {
			g_ptr_array_add(folders, item);
		} else {
			g_warning("no such folder: %s\n", buf);
		}
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
			if (buf[0] == '.' && buf[1] == '\n') {
				break;
			}
			strretchomp(buf);
			g_ptr_array_add(files, g_strdup(buf));
		}
		open_compose_new(mailto, files);
		ptr_array_free_strings(files);
		g_ptr_array_free(files, TRUE);
		g_free(mailto);
	} else if (!strncmp(buf, "compose", 7)) {
		open_compose_new(buf + strlen("compose") + 1, NULL);
	} else if (!strncmp(buf, "subscribe", 9)) {
		main_window_popup(mainwin);
		folder_subscribe(buf + strlen("subscribe") + 1);
	} else if (!strncmp(buf, "send", 4)) {
		send_queue();
	} else if (!strncmp(buf, "online", 6)) {
		main_window_toggle_work_offline(mainwin, FALSE, FALSE);
	} else if (!strncmp(buf, "offline", 7)) {
		main_window_toggle_work_offline(mainwin, TRUE, FALSE);
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
	} else if (!strncmp(buf, "exit", 4)) {
		app_will_exit(NULL, mainwin);
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

	for (list = folder_get_list(); list != NULL; list = list->next) {
		Folder *folder = list->data;

		if (folder->queue) {
			gint res = procmsg_send_queue
				(folder->queue, prefs_common.savemsg);

			if (res < 0) {
				alertpanel_error(_("Some errors occurred while sending queued messages."));
			}
			if (res) {
				folder_item_scan(folder->queue);
			}
		}
	}
}

static void quit_signal_handler(int sig)
{
	debug_print("Quitting on signal %d\n", sig);

	g_timeout_add(0, clean_quit, NULL);
}

static void install_basic_sighandlers()
{
#ifndef G_OS_WIN32
	sigset_t    mask;
	struct sigaction act;

	sigemptyset(&mask);

#ifdef SIGTERM
	sigaddset(&mask, SIGTERM);
#endif
#ifdef SIGINT
	sigaddset(&mask, SIGINT);
#endif
#ifdef SIGHUP
	sigaddset(&mask, SIGHUP);
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
#ifdef SIGHUP
	sigaction(SIGHUP, &act, 0);
#endif	

	sigprocmask(SIG_UNBLOCK, &mask, 0);
#endif /* !G_OS_WIN32 */
}
