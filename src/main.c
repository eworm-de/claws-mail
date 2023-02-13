/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2023 the Claws Mail team and Hiroyuki Yamamoto
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#include "claws-features.h"
#endif

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

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
#  include <errno.h>
#  include <fcntl.h>
#endif
#ifdef HAVE_LIBSM
#include <X11/SM/SMlib.h>
#endif

#if HAVE_FLOCK
#include <sys/file.h>
#endif

#include "file_checker.h"
#include "wizard.h"
#ifdef HAVE_STARTUP_NOTIFICATION
#ifdef GDK_WINDOWING_X11
# define SN_API_NOT_YET_FROZEN
# include <libsn/sn-launchee.h>
# include <gdk/gdkx.h>
#endif
#endif

#ifdef HAVE_DBUS_GLIB
#include <dbus/dbus-glib.h>
#endif
#ifdef HAVE_NETWORKMANAGER_SUPPORT
#include <NetworkManager.h>
#endif
#ifdef HAVE_VALGRIND
#include <valgrind.h>
#endif
#ifdef HAVE_SVG
#include <librsvg/rsvg.h>
#endif

#include "claws.h"
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
#include "prefs_migration.h"
#include "prefs_receive.h"
#include "prefs_msg_colors.h"
#include "prefs_quote.h"
#include "prefs_spelling.h"
#include "prefs_summaries.h"
#include "prefs_themes.h"
#include "prefs_other.h"
#include "prefs_proxy.h"
#include "prefs_logging.h"
#include "prefs_send.h"
#include "prefs_wrapping.h"
#include "prefs_compose_writing.h"
#include "prefs_display_header.h"
#include "account.h"
#include "procmsg.h"
#include "inc.h"
#include "imap.h"
#include "send_message.h"
#include "md5.h"
#include "import.h"
#include "manage_window.h"
#include "alertpanel.h"
#include "statusbar.h"
#ifndef USE_ALT_ADDRBOOK
	#include "addressbook.h"
#else
	#include "addressbook-dbus.h"
#endif
#include "compose.h"
#include "folder.h"
#include "folder_item_prefs.h"
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
#include "tags.h"
#include "hooks.h"
#include "menu.h"
#include "quicksearch.h"
#include "advsearch.h"
#include "avatars.h"
#include "passwordstore.h"
#include "file-utils.h"

#ifdef HAVE_LIBETPAN
#include "imap-thread.h"
#include "nntp-thread.h"
#endif
#include "stock_pixmap.h"
#ifdef USE_GNUTLS
#  include "ssl.h"
#endif

#include "version.h"

#include "crash.h"

#include "timing.h"

#ifdef HAVE_NETWORKMANAGER_SUPPORT
/* Went offline due to NetworkManager */
static gboolean went_offline_nm;
#endif


#ifdef HAVE_DBUS_GLIB
static DBusGProxy *awn_proxy = NULL;
#endif

gchar *prog_version;
#if (defined HAVE_LIBSM || defined CRASH_DIALOG)
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
	gboolean cancel_receiving;
	gboolean cancel_sending;
	gboolean compose;
	const gchar *compose_mailto;
	GList *attach_files;
	gboolean search;
	const gchar *search_folder;
	const gchar *search_type;
	const gchar *search_request;
	gboolean search_recursive;
	gboolean status;
	gboolean status_full;
	gboolean statistics;
	gboolean reset_statistics;
	GPtrArray *status_folders;
	GPtrArray *status_full_folders;
	gboolean send;
	gboolean crash;
	int online_mode;
	gchar   *crash_params;
	gboolean exit;
	gboolean subscribe;
	const gchar *subscribe_uri;
	const gchar *target;
	gboolean debug;
	const gchar *geometry;
} cmd;

SessionStats session_stats;

static void reset_statistics(void);
		
static void parse_cmd_opt(int argc, char *argv[]);

static gint prohibit_duplicate_launch	(int		*argc,
					 char		***argv);
static gchar * get_crashfile_name	(void);
static gint lock_socket_remove		(void);
static void lock_socket_input_cb	(gpointer	   data,
					 gint		   source,
					 GIOCondition      condition);

static void open_compose_new		(const gchar	*address,
					 GList		*attach_files);

static void send_queue			(void);
static void initial_processing		(FolderItem *item, gpointer data);
#ifndef G_OS_WIN32
static void quit_signal_handler         (int sig);
#endif
static void install_basic_sighandlers   (void);
static void exit_claws			(MainWindow *mainwin);

#ifdef HAVE_NETWORKMANAGER_SUPPORT
static void networkmanager_state_change_cb(DBusGProxy *proxy, gchar *dev,
																					 gpointer data);
#endif

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

#define STRNCMP(S1, S2) (strncmp((S1), (S2), sizeof((S2)) - 1))

#define CM_FD_WRITE(S) fd_write(sock, (S), strlen((S)))
#define CM_FD_WRITE_ALL(S) fd_write_all(sock, (S), strlen((S)))

static MainWindow *static_mainwindow;

static gboolean emergency_exit = FALSE;

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
		gtk_window_move(GTK_WINDOW(hack), 0, 0);
		gtk_widget_set_size_request(hack, 1, 1);
		gtk_widget_show(hack);
	}

	xdisplay = GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
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

static void claws_gtk_idle(void) 
{
	while(gtk_events_pending()) {
		gtk_main_iteration();
	}
	g_usleep(50000);
}

static gboolean sc_starting = FALSE;

static gboolean defer_check_all(void *data)
{
	gboolean autochk = GPOINTER_TO_INT(data);

	if (!sc_starting) {
		inc_all_account_mail(static_mainwindow, autochk, FALSE,
			prefs_common.newmail_notify_manu);

	} else {
		inc_all_account_mail(static_mainwindow, FALSE,
				prefs_common.chk_on_startup,
				prefs_common.newmail_notify_manu);
		sc_starting = FALSE;
		main_window_set_menu_sensitive(static_mainwindow);
		toolbar_main_set_sensitive(static_mainwindow);
	}
	return FALSE;
}

static gboolean defer_check(void *data)
{
	inc_mail(static_mainwindow, prefs_common.newmail_notify_manu);

	if (sc_starting) {
		sc_starting = FALSE;
		main_window_set_menu_sensitive(static_mainwindow);
		toolbar_main_set_sensitive(static_mainwindow);
	}
	return FALSE;
}

static gboolean defer_jump(void *data)
{
	if (cmd.receive_all) {
		defer_check_all(GINT_TO_POINTER(FALSE));
	} else if (prefs_common.chk_on_startup) {
		defer_check_all(GINT_TO_POINTER(TRUE));
	} else if (cmd.receive) {
		defer_check(NULL);
	} 
	mainwindow_jump_to(data, FALSE);
	if (sc_starting) {
		sc_starting = FALSE;
		main_window_set_menu_sensitive(static_mainwindow);
		toolbar_main_set_sensitive(static_mainwindow);
	}
	return FALSE;
}

static void chk_update_val(GtkWidget *widget, gpointer data)
{
        gboolean *val = (gboolean *)data;
	*val = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
}

static gboolean migrate_old_config(const gchar *old_cfg_dir, const gchar *new_cfg_dir, const gchar *oldversion)
{
	gchar *message = g_strdup_printf(_("Configuration for %s found.\n"
			 "Do you want to migrate this configuration?"), oldversion);

	if (!strcmp(oldversion, "Sylpheed")) {
		gchar *message2 = g_strdup_printf(_("\n\nYour Sylpheed filtering rules can be converted by a\n"
			     "script available at %s."), TOOLS_URI);
		gchar *tmp = g_strconcat(message, message2, NULL);
		g_free(message2);
		g_free(message);
        message = tmp;
	}

	gint r = 0;
	GtkWidget *window = NULL;
	GtkWidget *keep_backup_chk;
	gboolean backup = TRUE;

	keep_backup_chk = gtk_check_button_new_with_label (_("Keep old configuration"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(keep_backup_chk), TRUE);
	CLAWS_SET_TIP(keep_backup_chk,
			     _("Keeping a backup will allow you to go back to an "
			       "older version, but may take a while if you have "
			       "cached IMAP or News data, and will take some extra "
			       "room on your disk."));

	g_signal_connect(G_OBJECT(keep_backup_chk), "toggled", 
			G_CALLBACK(chk_update_val), &backup);

	if (alertpanel_full(_("Migration of configuration"), message,
		 	NULL, _("_No"), NULL, _("_Yes"), NULL, NULL, ALERTFOCUS_SECOND,
			FALSE, keep_backup_chk, ALERT_QUESTION) != G_ALERTALTERNATE) {
		return FALSE;
	}
	
	/* we can either do a fast migration requiring not any extra disk
	 * space, or a slow one that copies the old configuration and leaves
	 * it in place. */
	if (backup) {
backup_mode:
		window = label_window_create(_("Copying configuration... This may take a while..."));
		GTK_EVENTS_FLUSH();
		
		r = copy_dir(old_cfg_dir, new_cfg_dir);
		label_window_destroy(window);
		
		/* if copy failed, we'll remove the partially copied
		 * new directory */
		if (r != 0) {
			alertpanel_error(_("Migration failed!"));
			remove_dir_recursive(new_cfg_dir);
		} else {
			if (!backup) {
				/* fast mode failed, but we don't want backup */
				remove_dir_recursive(old_cfg_dir);
			}
		}
	} else {
		window = label_window_create(_("Migrating configuration..."));
		GTK_EVENTS_FLUSH();
		
		r = g_rename(old_cfg_dir, new_cfg_dir);
		label_window_destroy(window);
		
		/* if g_rename failed, we'll try to copy */
		if (r != 0) {
			FILE_OP_ERROR(new_cfg_dir, "g_rename");
			debug_print("rename failed, trying copy\n");
			goto backup_mode;
		}
	}
	return (r == 0);
}

static int migrate_common_rc(const gchar *old_rc, const gchar *new_rc)
{
	FILE *oldfp, *newfp;
	gchar *plugin_path, *old_plugin_path, *new_plugin_path;
	gchar buf[BUFFSIZE];
	gboolean err = FALSE;

	oldfp = claws_fopen(old_rc, "r");
	if (!oldfp)
		return -1;
	newfp = claws_fopen(new_rc, "w");
	if (!newfp) {
		claws_fclose(oldfp);
		return -1;
	}
	
	plugin_path = g_strdup(get_plugin_dir());
	new_plugin_path = g_strdup(plugin_path);
	
	if (strstr(plugin_path, "/claws-mail/")) {
		gchar *end = g_strdup(strstr(plugin_path, "/claws-mail/")+strlen("/claws-mail/"));
		*(strstr(plugin_path, "/claws-mail/")) = '\0';
		old_plugin_path = g_strconcat(plugin_path, "/sylpheed-claws/", end, NULL);
		g_free(end);
	} else {
		old_plugin_path = g_strdup(new_plugin_path);
	}
	debug_print("replacing %s with %s\n", old_plugin_path, new_plugin_path);
	while (claws_fgets(buf, sizeof(buf), oldfp)) {
		if (STRNCMP(buf, old_plugin_path)) {
			err |= (claws_fputs(buf, newfp) == EOF);
		} else {
			debug_print("->replacing %s\n", buf);
			debug_print("  with %s%s\n", new_plugin_path, buf+strlen(old_plugin_path));
			err |= (claws_fputs(new_plugin_path, newfp) == EOF);
			err |= (claws_fputs(buf+strlen(old_plugin_path), newfp) == EOF);
		}
	}
	g_free(plugin_path);
	g_free(new_plugin_path);
	g_free(old_plugin_path);
	claws_fclose(oldfp);
	if (claws_safe_fclose(newfp) == EOF)
		err = TRUE;
	
	return (err ? -1:0);
}

#ifdef HAVE_LIBSM
static void
sc_client_set_value (MainWindow *mainwin,
		  gchar       *name,
		  char        *type,
		  int          num_vals,
		  SmPropValue *vals)
{
	SmProp *proplist[1];
	SmProp prop;

	prop.name = name;
	prop.type = type;
	prop.num_vals = num_vals;
	prop.vals = vals;

	proplist[0]= &prop;
	if (mainwin->smc_conn)
		SmcSetProperties ((SmcConn) mainwin->smc_conn, 1, proplist);
}

static void sc_die_callback (SmcConn smc_conn, SmPointer client_data)
{
	clean_quit(NULL);
}

static void sc_save_complete_callback(SmcConn smc_conn, SmPointer client_data)
{
}

static void sc_shutdown_cancelled_callback (SmcConn smc_conn, SmPointer client_data)
{
	MainWindow *mainwin = (MainWindow *)client_data;
	if (mainwin->smc_conn)
		SmcSaveYourselfDone ((SmcConn) mainwin->smc_conn, TRUE);
}

static void sc_save_yourself_callback (SmcConn   smc_conn,
			       SmPointer client_data,
			       int       save_style,
			       gboolean  shutdown,
			       int       interact_style,
			       gboolean  fast) {

	MainWindow *mainwin = (MainWindow *)client_data;
	if (mainwin->smc_conn)
		SmcSaveYourselfDone ((SmcConn) mainwin->smc_conn, TRUE);
}

static IceIOErrorHandler sc_ice_installed_handler;

static void sc_ice_io_error_handler (IceConn connection)
{
	if (sc_ice_installed_handler)
		(*sc_ice_installed_handler) (connection);
}
static gboolean sc_process_ice_messages (GIOChannel   *source,
					 GIOCondition  condition,
					 gpointer      data)
{
	IceConn connection = (IceConn) data;
	IceProcessMessagesStatus status;

	status = IceProcessMessages (connection, NULL, NULL);

	if (status == IceProcessMessagesIOError) {
		IcePointer context = IceGetConnectionContext (connection);

		if (context && G_IS_OBJECT(context)) {
		guint disconnect_id = g_signal_lookup ("disconnect", G_OBJECT_TYPE (context));

		if (disconnect_id > 0)
			g_signal_emit (context, disconnect_id, 0);
		} else {
			IceSetShutdownNegotiation (connection, False);
			IceCloseConnection (connection);
		}
	}

	return TRUE;
}

static void new_ice_connection (IceConn connection, IcePointer client_data, Bool opening,
		    IcePointer *watch_data)
{
	guint input_id;

	if (opening) {
		GIOChannel *channel;
		/* Make sure we don't pass on these file descriptors to any
		exec'ed children */
		fcntl(IceConnectionNumber(connection),F_SETFD,
		fcntl(IceConnectionNumber(connection),F_GETFD,0) | FD_CLOEXEC);

		channel = g_io_channel_unix_new (IceConnectionNumber (connection));
		input_id = g_io_add_watch (channel,
		G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_PRI,
		sc_process_ice_messages,
		connection);
		g_io_channel_unref (channel);

		*watch_data = (IcePointer) GUINT_TO_POINTER (input_id);
	} else {
		input_id = GPOINTER_TO_UINT ((gpointer) *watch_data);
		g_source_remove (input_id);
	}
}

static void sc_session_manager_connect(MainWindow *mainwin)
{
	static gboolean connected = FALSE;
	SmcCallbacks      callbacks;
	gchar            *client_id;
	IceIOErrorHandler default_handler;

	if (connected)
		return;
	connected = TRUE;


	sc_ice_installed_handler = IceSetIOErrorHandler (NULL);
	default_handler = IceSetIOErrorHandler (sc_ice_io_error_handler);

	if (sc_ice_installed_handler == default_handler)
		sc_ice_installed_handler = NULL;

	IceAddConnectionWatch (new_ice_connection, NULL);
      
      
      	callbacks.save_yourself.callback      = sc_save_yourself_callback;
	callbacks.die.callback                = sc_die_callback;
	callbacks.save_complete.callback      = sc_save_complete_callback;
	callbacks.shutdown_cancelled.callback = sc_shutdown_cancelled_callback;

	callbacks.save_yourself.client_data =
		callbacks.die.client_data =
		callbacks.save_complete.client_data =
		callbacks.shutdown_cancelled.client_data = (SmPointer) mainwin;
	if (g_getenv ("SESSION_MANAGER")) {
		gchar error_string_ret[256] = "";

		mainwin->smc_conn = (gpointer)
			SmcOpenConnection (NULL, mainwin,
				SmProtoMajor, SmProtoMinor,
				SmcSaveYourselfProcMask | SmcDieProcMask |
				SmcSaveCompleteProcMask |
				SmcShutdownCancelledProcMask,
				&callbacks,
				NULL, &client_id,
				256, error_string_ret);

		/* From https://www.x.org/releases/X11R7.7/doc/libSM/SMlib.txt:
		 * If SmcOpenConnection succeeds, it returns an opaque connection
		 * pointer of type SmcConn and the client_id_ret argument contains
		 * the client ID to be used for this session. The client_id_ret
		 * should be freed with a call to free when no longer needed. On
		 * failure, SmcOpenConnection returns NULL, and the reason for
		 * failure is returned in error_string_ret. */
		if (mainwin->smc_conn != NULL)
			g_free(client_id);

		if (error_string_ret[0] || mainwin->smc_conn == NULL)
			g_warning("while connecting to session manager: %s",
				error_string_ret);
		else {
			SmPropValue *vals;
			vals = g_new (SmPropValue, 1);
			vals[0].length = strlen(argv0);
			vals[0].value = argv0;
			sc_client_set_value (mainwin, SmCloneCommand, SmLISTofARRAY8, 1, vals);
			sc_client_set_value (mainwin, SmRestartCommand, SmLISTofARRAY8, 1, vals);
			sc_client_set_value (mainwin, SmProgram, SmARRAY8, 1, vals);

			vals[0].length = strlen(g_get_user_name()?g_get_user_name():"");
			vals[0].value = g_strdup(g_get_user_name()?g_get_user_name():"");
			sc_client_set_value (mainwin, SmUserID, SmARRAY8, 1, vals);

			g_free(vals[0].value);
			g_free(vals);
		}
	}
}
#endif

static gboolean sc_exiting = FALSE;
static gboolean show_at_startup = TRUE;
static gboolean claws_crashed_bool = FALSE;

gboolean claws_crashed(void) {
	return claws_crashed_bool;
}

void main_set_show_at_startup(gboolean show)
{
	show_at_startup = show;
}

#ifdef G_OS_WIN32
static HANDLE win32_debug_log = NULL;
static guint win32_log_handler_app_id;
static guint win32_log_handler_glib_id;
static guint win32_log_handler_gtk_id;

static void win32_log_WriteFile(const gchar *string)
{
	BOOL ret;
	DWORD bytes_written;

	ret = WriteFile(win32_debug_log, string, strlen(string), &bytes_written, NULL);
	if (!ret) {
		DWORD err = GetLastError();
		gchar *tmp;

		tmp = g_strdup_printf("Error: WriteFile in failed with error 0x%lx.  Buffer contents:\n%s", err, string);
		OutputDebugString(tmp);
		g_free(tmp);
	}
}

static void win32_print_stdout(const gchar *string)
{
	if (win32_debug_log) {
		win32_log_WriteFile(string);
	}
}

GLogWriterOutput win32_log_writer(GLogLevelFlags log_level, const GLogField *fields, gsize n_fields, gpointer user_data)
{
	gchar *formatted;
	gchar *out;

	g_return_val_if_fail(win32_debug_log != NULL, G_LOG_WRITER_UNHANDLED);
	g_return_val_if_fail(fields != NULL, G_LOG_WRITER_UNHANDLED);
	g_return_val_if_fail(n_fields > 0, G_LOG_WRITER_UNHANDLED);

	formatted = g_log_writer_format_fields(log_level, fields, n_fields, FALSE);
	out = g_strdup_printf("%s\n", formatted);

	win32_log_WriteFile(out);

	g_free(formatted);
	g_free(out);

	return G_LOG_WRITER_HANDLED;
}

static void win32_log(const gchar *log_domain, GLogLevelFlags log_level, const gchar* message, gpointer user_data)
{
	gchar *out;

	if (win32_debug_log) {
		const gchar* type;

		switch(log_level & G_LOG_LEVEL_MASK)
		{
			case G_LOG_LEVEL_ERROR:
				type="error";
				break;
			case G_LOG_LEVEL_CRITICAL:
				type="critical";
				break;
			case G_LOG_LEVEL_WARNING:
				type="warning";
				break;
			case G_LOG_LEVEL_MESSAGE:
				type="message";
				break;
			case G_LOG_LEVEL_INFO:
				type="info";
				break;
			case G_LOG_LEVEL_DEBUG:
				type="debug";
				break;
			default:
				type="N/A";
		}

		if (log_domain)
			out = g_strdup_printf("%s: %s: %s", log_domain, type, message);
		else
			out = g_strdup_printf("%s: %s", type, message);

		win32_log_WriteFile(out);

		g_free(out);
	}
}

static void win32_open_log(void)
{
	gchar *logfile = win32_debug_log_path();
	gchar *oldlogfile = g_strconcat(logfile, ".bak", NULL);

	if (is_file_exist(logfile)) {
		if (rename_force(logfile, oldlogfile) < 0)
			FILE_OP_ERROR(logfile, "rename");
	}

	win32_debug_log = CreateFile(logfile,
		GENERIC_WRITE,
		FILE_SHARE_READ,
		NULL,
		CREATE_NEW,
		FILE_ATTRIBUTE_NORMAL,
		NULL);

	if (win32_debug_log == INVALID_HANDLE_VALUE) {
		win32_debug_log = NULL;
	}

	g_free(logfile);
	g_free(oldlogfile);

	if (win32_debug_log) {
		g_set_print_handler(win32_print_stdout);
		g_set_printerr_handler(win32_print_stdout);

		win32_log_handler_app_id = g_log_set_handler(NULL, G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL
                     | G_LOG_FLAG_RECURSION, win32_log, NULL);
		win32_log_handler_glib_id = g_log_set_handler("GLib", G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL
                     | G_LOG_FLAG_RECURSION, win32_log, NULL);
		win32_log_handler_gtk_id = g_log_set_handler("Gtk", G_LOG_LEVEL_MASK | G_LOG_FLAG_FATAL
                     | G_LOG_FLAG_RECURSION, win32_log, NULL);

		g_log_set_writer_func(&win32_log_writer, NULL, NULL);
	}
}

static void win32_close_log(void)
{
	if (win32_debug_log) {
		g_log_remove_handler("", win32_log_handler_app_id);
		g_log_remove_handler("GLib", win32_log_handler_glib_id);
		g_log_remove_handler("Gtk", win32_log_handler_gtk_id);
		CloseHandle(win32_debug_log);
		win32_debug_log = NULL;
	}
}		
#endif

static void main_dump_features_list(gboolean show_debug_only)
/* display compiled-in features list */
{
	if (show_debug_only && !debug_get_mode())
		return;

	if (show_debug_only)
		debug_print("runtime GTK %d.%d.%d / GLib %d.%d.%d\n",
			   gtk_major_version, gtk_minor_version, gtk_micro_version,
			   glib_major_version, glib_minor_version, glib_micro_version);
	else
		g_print("runtime GTK %d.%d.%d / GLib %d.%d.%d\n",
			   gtk_major_version, gtk_minor_version, gtk_micro_version,
			   glib_major_version, glib_minor_version, glib_micro_version);
	if (show_debug_only)
		debug_print("buildtime GTK %d.%d.%d / GLib %d.%d.%d\n",
			   GTK_MAJOR_VERSION, GTK_MINOR_VERSION, GTK_MICRO_VERSION,
			   GLIB_MAJOR_VERSION, GLIB_MINOR_VERSION, GLIB_MICRO_VERSION);
	else
		g_print("buildtime GTK %d.%d.%d / GLib %d.%d.%d\n",
			   GTK_MAJOR_VERSION, GTK_MINOR_VERSION, GTK_MICRO_VERSION,
			   GLIB_MAJOR_VERSION, GLIB_MINOR_VERSION, GLIB_MICRO_VERSION);
	
	if (show_debug_only)
		debug_print("Compiled-in features:\n");
	else
		g_print("Compiled-in features:\n");
#if HAVE_LIBCOMPFACE
	if (show_debug_only)
		debug_print(" compface\n");
	else
		g_print(" compface\n");
#endif
#if USE_ENCHANT
	if (show_debug_only)
		debug_print(" Enchant\n");
	else
		g_print(" Enchant\n");
#endif
#if USE_GNUTLS
	if (show_debug_only)
		debug_print(" GnuTLS\n");
	else
		g_print(" GnuTLS\n");
#endif
#if INET6
	if (show_debug_only)
		debug_print(" IPv6\n");
	else
		g_print(" IPv6\n");
#endif
#if HAVE_ICONV
	if (show_debug_only)
		debug_print(" iconv\n");
	else
		g_print(" iconv\n");
#endif
#if USE_JPILOT
	if (show_debug_only)
		debug_print(" JPilot\n");
	else
		g_print(" JPilot\n");
#endif
#if USE_LDAP
	if (show_debug_only)
		debug_print(" LDAP\n");
	else
		g_print(" LDAP\n");
#endif
#if HAVE_LIBETPAN
	if (show_debug_only)
		debug_print(" libetpan %d.%d\n", LIBETPAN_VERSION_MAJOR, LIBETPAN_VERSION_MINOR);
	else
		g_print(" libetpan %d.%d\n", LIBETPAN_VERSION_MAJOR, LIBETPAN_VERSION_MINOR);
#endif
#if HAVE_LIBSM
	if (show_debug_only)
		debug_print(" libSM\n");
	else
		g_print(" libSM\n");
#endif
#if HAVE_NETWORKMANAGER_SUPPORT
	if (show_debug_only)
		debug_print(" NetworkManager\n");
	else
		g_print(" NetworkManager\n");
#endif
#if HAVE_SVG
	if (show_debug_only)
		debug_print(" librSVG " LIBRSVG_VERSION "\n");
	else
		g_print(" librSVG " LIBRSVG_VERSION "\n");
#endif
}

#ifdef HAVE_DBUS_GLIB
static gulong dbus_item_hook_id = HOOK_NONE;
static gulong dbus_folder_hook_id = HOOK_NONE;

static void uninstall_dbus_status_handler(void)
{
	if(awn_proxy)
		g_object_unref(awn_proxy);
	awn_proxy = NULL;
	if (dbus_item_hook_id != HOOK_NONE)
		hooks_unregister_hook(FOLDER_ITEM_UPDATE_HOOKLIST, dbus_item_hook_id);
	if (dbus_folder_hook_id != HOOK_NONE)
		hooks_unregister_hook(FOLDER_UPDATE_HOOKLIST, dbus_folder_hook_id);
}

static void dbus_update(FolderItem *removed_item)
{
	guint new, unread, unreadmarked, marked, total;
	guint replied, forwarded, locked, ignored, watched;
	gchar *buf;
	GError *error = NULL;

	folder_count_total_msgs(&new, &unread, &unreadmarked, &marked, &total,
				&replied, &forwarded, &locked, &ignored,
				&watched);
	if (removed_item) {
		total -= removed_item->total_msgs;
		new -= removed_item->new_msgs;
		unread -= removed_item->unread_msgs;
	}

	if (new > 0) {
		buf = g_strdup_printf("%d", new);
		dbus_g_proxy_call(awn_proxy, "SetInfoByName", &error,
			G_TYPE_STRING, "claws-mail",
			G_TYPE_STRING, buf,
			G_TYPE_INVALID, G_TYPE_INVALID);
		g_free(buf);
		
	} else {
		dbus_g_proxy_call(awn_proxy, "UnsetInfoByName", &error, G_TYPE_STRING,
			"claws-mail", G_TYPE_INVALID, G_TYPE_INVALID);
	}
	if (error) {
		debug_print("%s\n", error->message);
		g_error_free(error);
	}
}

static gboolean dbus_status_update_folder_hook(gpointer source, gpointer data)
{
	FolderUpdateData *hookdata;
	hookdata = source;
	if (hookdata->update_flags & FOLDER_REMOVE_FOLDERITEM)
		dbus_update(hookdata->item);
	else
		dbus_update(NULL);

	return FALSE;
}

static gboolean dbus_status_update_item_hook(gpointer source, gpointer data)
{
	dbus_update(NULL);

	return FALSE;
}

static void install_dbus_status_handler(void)
{
	GError *tmp_error = NULL;
	DBusGConnection *connection = dbus_g_bus_get(DBUS_BUS_SESSION, &tmp_error);
	
	if(!connection) {
		/* If calling code doesn't do error checking, at least print some debug */
		debug_print("Failed to open connection to session bus: %s\n",
				 tmp_error->message);
		g_error_free(tmp_error);
		return;
	}
	awn_proxy = dbus_g_proxy_new_for_name(connection,
			"com.google.code.Awn",
			"/com/google/code/Awn",
			"com.google.code.Awn");
	dbus_item_hook_id = hooks_register_hook (FOLDER_ITEM_UPDATE_HOOKLIST, dbus_status_update_item_hook, NULL);
	if (dbus_item_hook_id == HOOK_NONE) {
		g_warning("failed to register folder item update hook");
		uninstall_dbus_status_handler();
		return;
	}

	dbus_folder_hook_id = hooks_register_hook (FOLDER_UPDATE_HOOKLIST, dbus_status_update_folder_hook, NULL);
	if (dbus_folder_hook_id == HOOK_NONE) {
		g_warning("failed to register folder update hook");
		uninstall_dbus_status_handler();
		return;
	}
}
#endif

static void reset_statistics(void)
{
	/* (re-)initialize session statistics */
	session_stats.received = 0;
	session_stats.sent = 0;
	session_stats.replied = 0;
	session_stats.forwarded = 0;
	session_stats.spam = 0;
	session_stats.time_started = time(NULL);
}

int main(int argc, char *argv[])
{
#ifdef HAVE_DBUS_GLIB
	DBusGConnection *connection;
	GError *error;
#endif
#ifdef HAVE_NETWORKMANAGER_SUPPORT
	DBusGProxy *nm_proxy;
#endif
	gchar *userrc;
	MainWindow *mainwin;
	FolderView *folderview;
	GdkPixbuf *icon;
	gboolean crash_file_present = FALSE;
	guint num_folder_class = 0;
	gboolean asked_for_migration = FALSE;
	gboolean start_done = TRUE;
	GSList *plug_list = NULL;
	gboolean never_ran = FALSE;
	gboolean mainwin_shown = FALSE;
	gint ret;

	START_TIMING("startup");

	sc_starting = TRUE;

#ifdef G_OS_WIN32
	win32_open_log();
#endif
	if (!claws_init(&argc, &argv)) {
#ifdef G_OS_WIN32
		win32_close_log();
#endif
		return 0;
	}

	prog_version = PROG_VERSION;
#if (defined HAVE_LIBSM || defined CRASH_DIALOG)
	argv0 = g_strdup(argv[0]);
#endif

	parse_cmd_opt(argc, argv);

	sock_init();

	/* check and create unix domain socket for remote operation */
	lock_socket = prohibit_duplicate_launch(&argc, &argv);
	if (lock_socket < 0) {
#ifdef HAVE_STARTUP_NOTIFICATION
#ifdef GDK_WINDOWING_X11
	if (GDK_IS_X11_DISPLAY(gdk_display_get_default())) {
		if (gtk_init_check(&argc, &argv))
			startup_notification_complete(TRUE);
	}
#endif
#endif
		return 0;
	}

	main_dump_features_list(TRUE);
	prefs_prepare_cache();

#ifdef CRASH_DIALOG
	if (cmd.crash) {
		gtk_init(&argc, &argv);
		crash_main(cmd.crash_params);
#ifdef G_OS_WIN32
		win32_close_log();
#endif
		return 0;
	}
	crash_install_handlers();
#endif
	install_basic_sighandlers();

	if (cmd.status || cmd.status_full || cmd.search ||
		cmd.statistics || cmd.reset_statistics || 
		cmd.cancel_receiving || cmd.cancel_sending ||
		cmd.debug) {
		puts("0 Claws Mail not running.");
		lock_socket_remove();
		return 0;
	}
	
	if (cmd.exit)
		return 0;

	reset_statistics();
	
	gtk_init(&argc, &argv);

#ifdef HAVE_NETWORKMANAGER_SUPPORT
	went_offline_nm = FALSE;
	nm_proxy = NULL;
#endif
#ifdef HAVE_DBUS_GLIB
	error = NULL;
	connection = dbus_g_bus_get(DBUS_BUS_SYSTEM, &error);

	if(!connection) {
		debug_print("Failed to open connection to system bus: %s\n", error->message);
		g_error_free(error);
	}
	else {
#ifdef HAVE_NETWORKMANAGER_SUPPORT
		nm_proxy = dbus_g_proxy_new_for_name(connection,
			"org.freedesktop.NetworkManager",
			"/org/freedesktop/NetworkManager",
			"org.freedesktop.NetworkManager");
		if (nm_proxy) {
			dbus_g_proxy_add_signal(nm_proxy, "StateChanged", G_TYPE_UINT, G_TYPE_INVALID);
			dbus_g_proxy_connect_signal(nm_proxy, "StateChanged",
				G_CALLBACK(networkmanager_state_change_cb),
				NULL,NULL);
		}
#endif
		install_dbus_status_handler();
	}
#endif

	gtkut_create_ui_manager();

	/* Create container for all the menus we will be adding */
	MENUITEM_ADDUI("/", "Menus", NULL, GTK_UI_MANAGER_MENUBAR);

#ifdef G_OS_WIN32
	CHDIR_EXEC_CODE_RETURN_VAL_IF_FAIL(get_home_dir(), 1, win32_close_log(););
#else
	CHDIR_RETURN_VAL_IF_FAIL(get_home_dir(), 1);
#endif
	
	/* no config dir exists. See if we can migrate an old config. */
	if (!is_dir_exist(get_rc_dir())) {
		prefs_destroy_cache();
		gboolean r = FALSE;
		
		/* if one of the old dirs exist, we'll ask if the user 
		 * want to migrates, and r will be TRUE if he said yes
		 * and migration succeeded, and FALSE otherwise.
		 */
		if (is_dir_exist(OLD_GTK2_RC_DIR)) {
			r = migrate_old_config(OLD_GTK2_RC_DIR, get_rc_dir(),
					       g_strconcat("Sylpheed-Claws 2.6.0 ", _("(or older)"), NULL));
			asked_for_migration = TRUE;
		} else if (is_dir_exist(OLDER_GTK2_RC_DIR)) {
			r = migrate_old_config(OLDER_GTK2_RC_DIR, get_rc_dir(),
					       g_strconcat("Sylpheed-Claws 1.9.15 ",_("(or older)"), NULL));
			asked_for_migration = TRUE;
		} else if (is_dir_exist(OLD_GTK1_RC_DIR)) {
			r = migrate_old_config(OLD_GTK1_RC_DIR, get_rc_dir(),
					       g_strconcat("Sylpheed-Claws 1.0.5 ",_("(or older)"), NULL));
			asked_for_migration = TRUE;
		} else if (is_dir_exist(SYLPHEED_RC_DIR)) {
			r = migrate_old_config(SYLPHEED_RC_DIR, get_rc_dir(), "Sylpheed");
			asked_for_migration = TRUE;
		}
		
		/* If migration failed or the user didn't want to do it,
		 * we create a new one (and we'll hit wizard later). 
		 */
		if (r == FALSE && !is_dir_exist(get_rc_dir())) {
#ifdef G_OS_UNIX
			if (copy_dir(SYSCONFDIR "/skel/.claws-mail", get_rc_dir()) < 0) {
#endif
				if (!is_dir_exist(get_rc_dir()) && make_dir(get_rc_dir()) < 0) {
#ifdef G_OS_WIN32
					win32_close_log();
#endif
					exit(1);
				}
#ifdef G_OS_UNIX
			}
#endif
		}
	}
	

	if (!is_file_exist(RC_DIR G_DIR_SEPARATOR_S COMMON_RC) &&
	    is_file_exist(RC_DIR G_DIR_SEPARATOR_S OLD_COMMON_RC)) {
	    	/* post 2.6 name change */
		migrate_common_rc(RC_DIR G_DIR_SEPARATOR_S OLD_COMMON_RC,
			  RC_DIR G_DIR_SEPARATOR_S COMMON_RC);
	}

	if (!cmd.exit)
		plugin_load_all("Common");

	userrc = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, "gtkrc-2.0", NULL);
	gtk_rc_parse(userrc);
	g_free(userrc);

	userrc = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, MENU_RC, NULL);
	gtk_accel_map_load (userrc);
	g_free(userrc);

#ifdef G_OS_WIN32
	CHDIR_EXEC_CODE_RETURN_VAL_IF_FAIL(get_rc_dir(), 1, win32_close_log(););
#else
	CHDIR_RETURN_VAL_IF_FAIL(get_rc_dir(), 1);
#endif

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

	if (!cmd.crash && crash_file_present)
		claws_crashed_bool = TRUE;

	if (is_file_exist("claws.log")) {
		if (rename_force("claws.log", "claws.log.bak") < 0)
			FILE_OP_ERROR("claws.log", "rename");
	}
	set_log_file(LOG_PROTOCOL, "claws.log");

	if (is_file_exist("filtering.log")) {
		if (rename_force("filtering.log", "filtering.log.bak") < 0)
			FILE_OP_ERROR("filtering.log", "rename");
	}
	set_log_file(LOG_DEBUG_FILTERING, "filtering.log");

#ifdef G_OS_WIN32
	CHDIR_EXEC_CODE_RETURN_VAL_IF_FAIL(get_home_dir(), 1, win32_close_log(););
#else
	CHDIR_RETURN_VAL_IF_FAIL(get_home_dir(), 1);
#endif

	folder_system_init();
	prefs_common_read_config();

	if (prefs_update_config_version_common() < 0) {
		debug_print("Main configuration file version upgrade failed, exiting\n");
#ifdef G_OS_WIN32
		win32_close_log();
#endif
		exit(200);
	}

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
	prefs_proxy_init();
	prefs_logging_init();
	prefs_receive_init();
	prefs_send_init();
	tags_read_tags();
	matcher_init();
#ifdef USE_ENCHANT
	gtkaspell_checkers_init();
	prefs_spelling_init();
#endif

	codeconv_set_allow_jisx0201_kana(prefs_common.allow_jisx0201_kana);
	codeconv_set_broken_are_utf8(prefs_common.broken_are_utf8);

#ifdef G_OS_WIN32
	if(prefs_common.gtk_theme && strcmp(prefs_common.gtk_theme, DEFAULT_W32_GTK_THEME))
		gtk_settings_set_string_property(gtk_settings_get_default(),
			"gtk-theme-name",
			prefs_common.gtk_theme,
			"XProperty");
#endif


	sock_set_io_timeout(prefs_common.io_timeout_secs);
	prefs_actions_read_config();
	prefs_display_header_read_config();
	/* prefs_filtering_read_config(); */
#ifndef USE_ALT_ADDRBOOK
	addressbook_read_file();
#else
	g_clear_error(&error);
	if (! addressbook_start_service(&error)) {
		g_warning("%s", error->message);
		g_clear_error(&error);
	}
	else {
		addressbook_install_hooks(&error);
	}
#endif
	gtkut_widget_init();
	priv_pixbuf_gdk(PRIV_PIXMAP_CLAWS_MAIL_ICON, &icon);
	gtk_window_set_default_icon(icon);

	folderview_initialize();

	mh_gtk_init();
	imap_gtk_init();
	news_gtk_init();

	mainwin = main_window_create();

	if (!check_file_integrity())
		exit(1);

#ifdef HAVE_NETWORKMANAGER_SUPPORT
	networkmanager_state_change_cb(nm_proxy,NULL,mainwin);
#endif

	manage_window_focus_in(mainwin->window, NULL, NULL);
	folderview = mainwin->folderview;

	folderview_freeze(mainwin->folderview);
	folder_item_update_freeze();

	if ((ret = passwd_store_read_config()) < 0) {
		debug_print("Password store configuration file version upgrade failed (%d), exiting\n", ret);
#ifdef G_OS_WIN32
		win32_close_log();
#endif
		exit(202);
	}

	prefs_account_init();
	account_read_config_all();

	if (prefs_update_config_version_accounts() < 0) {
		debug_print("Accounts configuration file version upgrade failed, exiting\n");
#ifdef G_OS_WIN32
		win32_close_log();
#endif
		exit(201);
	}

#ifdef HAVE_LIBETPAN
	imap_main_init(prefs_common.skip_ssl_cert_check);
	imap_main_set_timeout(prefs_common.io_timeout_secs);
	nntp_main_init(prefs_common.skip_ssl_cert_check);
#endif	
	/* If we can't read a folder list or don't have accounts,
	 * it means the configuration's not done. Either this is
	 * a brand new install, a failed/refused migration,
	 * or a failed config_version upgrade.
	 */
	if ((ret = folder_read_list()) < 0) {
		debug_print("Folderlist read failed (%d)\n", ret);
		prefs_destroy_cache();
		
		if (ret == -2) {
			/* config_version update failed in folder_read_list(). We
			 * do not want to run the wizard, just exit. */
			debug_print("Folderlist version upgrade failed, exiting\n");
#ifdef G_OS_WIN32
			win32_close_log();
#endif
			exit(203);
		}

		/* if run_wizard returns FALSE it's because it's
		 * been cancelled. We can't do much but exit.
		 * however, if the user was asked for a migration,
		 * we remove the newly created directory so that
		 * he's asked again for migration on next launch.*/
		if (!run_wizard(mainwin, TRUE)) {
			if (asked_for_migration)
				remove_dir_recursive(RC_DIR);
#ifdef G_OS_WIN32
			win32_close_log();
#endif
			exit(1);
		}
		main_window_reflect_prefs_all_now();
		folder_write_list();
		never_ran = TRUE;
	}

	if (!account_get_list()) {
		prefs_destroy_cache();
		if (!run_wizard(mainwin, FALSE)) {
			if (asked_for_migration)
				remove_dir_recursive(RC_DIR);
#ifdef G_OS_WIN32
			win32_close_log();
#endif
			exit(1);
		}
		if(!account_get_list()) {
			exit_claws(mainwin);
			exit(1);
		}
		never_ran = TRUE;
	}

	
	toolbar_main_set_sensitive(mainwin);
	main_window_set_menu_sensitive(mainwin);

	/* if crashed, show window early so that the user
	 * sees what's happening */
	if (claws_crashed()) {
		main_window_popup(mainwin);
		mainwin_shown = TRUE;
	}

	account_set_missing_folder();
	folder_set_missing_folders();
	folderview_set(folderview);

	prefs_matcher_read_config();
	quicksearch_set_search_strings(mainwin->summaryview->quicksearch);

	/* make one all-folder processing before using claws */
	main_window_cursor_wait(mainwin);
	folder_func_to_all_folders(initial_processing, (gpointer *)mainwin);

	/* if claws crashed, rebuild caches */
	if (claws_crashed()) {
		GTK_EVENTS_FLUSH();
		debug_print("Claws Mail crashed, checking for new messages in local folders\n");
		folder_item_update_thaw();
		folderview_check_new(NULL);
		folder_clean_cache_memory_force();
		folder_item_update_freeze();
	}
	/* make the crash-indicator file */
	if (str_write_to_file("foo", get_crashfile_name(), FALSE) < 0) {
		g_warning("can't create the crash-indicator file");
	}

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

	claws_register_idle_function(claws_gtk_idle);

	avatars_init();
	prefs_toolbar_init();

	num_folder_class = g_list_length(folder_get_list());

	plugin_load_all("GTK3");

	if (g_list_length(folder_get_list()) != num_folder_class) {
		debug_print("new folders loaded, reloading processing rules\n");
		prefs_matcher_read_config();
	}
	
	if ((plug_list = plugin_get_unloaded_list()) != NULL) {
		GSList *cur;
		gchar *list = NULL;
		gint num_plugins = 0;
		for (cur = plug_list; cur; cur = cur->next) {
			Plugin *plugin = (Plugin *)cur->data;
			gchar *tmp = g_strdup_printf("%s\n%s",
				list? list:"",
				plugin_get_name(plugin));
			g_free(list);
			list = tmp;
			num_plugins++;
		}
		main_window_cursor_normal(mainwin);
		main_window_popup(mainwin);
		mainwin_shown = TRUE;
		alertpanel_warning(ngettext(
				     "The following plugin failed to load. "
				     "Check the Plugins configuration "
				     "for more information:\n%s",
				     "The following plugins failed to load. "
				     "Check the Plugins configuration "
				     "for more information:\n%s", 
				     num_plugins), 
				     list);
		main_window_cursor_wait(mainwin);
		g_free(list);
		g_slist_free(plug_list);
	}

	if (never_ran) {
		prefs_common_write_config();
	 	plugin_load_standard_plugins ();
	}

	/* if not crashed, show window now */
	if (!mainwin_shown) {
		/* apart if something told not to show */
		if (show_at_startup)
			main_window_popup(mainwin);
	}

	if (cmd.geometry != NULL) {
		if (!gtk_window_parse_geometry(GTK_WINDOW(mainwin->window), cmd.geometry))
			g_warning("failed to parse geometry '%s'", cmd.geometry);
		else {
			int width, height;

			if (sscanf(cmd.geometry, "%ux%u+", &width, &height) == 2)
				gtk_window_resize(GTK_WINDOW(mainwin->window), width, height);
			else
				g_warning("failed to parse geometry's width/height");
		}
	}

	if (!folder_have_mailbox()) {
		prefs_destroy_cache();
		main_window_cursor_normal(mainwin);
		if (folder_get_list() != NULL) {
			alertpanel_error(_("Claws Mail has detected a configured "
				   "mailbox, but it is incomplete. It is "
				   "possibly due to a failing IMAP account. Use "
				   "\"Rebuild folder tree\" on the mailbox parent "
				   "folder's context menu to try to fix it."));
		} else {
			alertpanel_error(_("Claws Mail has detected a configured "
				   "mailbox, but could not load it. It is "
				   "probably provided by an out-of-date "
				   "external plugin. Please reinstall the "
				   "plugin and try again."));
			exit_claws(mainwin);
			exit(1);
		}
	}
	
	static_mainwindow = mainwin;

#ifdef HAVE_STARTUP_NOTIFICATION
#ifdef GDK_WINDOWING_X11
	if (GDK_IS_X11_DISPLAY(gdk_display_get_default()))
		startup_notification_complete(FALSE);
#endif
#endif
#ifdef HAVE_LIBSM
	sc_session_manager_connect(mainwin);
#endif

	folder_item_update_thaw();
	folderview_thaw(mainwin->folderview);
	main_window_cursor_normal(mainwin);

	if (!cmd.target && prefs_common.goto_folder_on_startup &&
	    folder_find_item_from_identifier(prefs_common.startup_folder) != NULL &&
	    !claws_crashed()) {
		cmd.target = prefs_common.startup_folder;
	} else if (!cmd.target && prefs_common.goto_last_folder_on_startup &&
	    folder_find_item_from_identifier(prefs_common.last_opened_folder) != NULL &&
	    !claws_crashed()) {
		cmd.target = prefs_common.last_opened_folder;
	}

	if (cmd.receive_all && !cmd.target) {
		start_done = FALSE;
		g_timeout_add(1000, defer_check_all, GINT_TO_POINTER(FALSE));
	} else if (prefs_common.chk_on_startup && !cmd.target) {
		start_done = FALSE;
		g_timeout_add(1000, defer_check_all, GINT_TO_POINTER(TRUE));
	} else if (cmd.receive && !cmd.target) {
		start_done = FALSE;
		g_timeout_add(1000, defer_check, NULL);
	}
	folderview_grab_focus(folderview);

	if (cmd.compose) {
		open_compose_new(cmd.compose_mailto, cmd.attach_files);
	}
	if (cmd.attach_files) {
		list_free_strings_full(cmd.attach_files);
		cmd.attach_files = NULL;
	}
	if (cmd.subscribe) {
		folder_subscribe(cmd.subscribe_uri);
	}

	if (cmd.send) {
		send_queue();
	}
	
	if (cmd.target) {
		start_done = FALSE;
		g_timeout_add(500, defer_jump, (gpointer)cmd.target);
	}

	prefs_destroy_cache();
	
	compose_reopen_exit_drafts();

	if (start_done) {
		sc_starting = FALSE;
		main_window_set_menu_sensitive(mainwin);
		toolbar_main_set_sensitive(mainwin);
	}

	/* register the callback of unix domain socket input */
	lock_socket_tag = claws_input_add(lock_socket,
					G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_PRI,
					lock_socket_input_cb,
					mainwin, TRUE);

	END_TIMING();

	gtk_main();

#ifdef HAVE_NETWORKMANAGER_SUPPORT
	if(nm_proxy)
		g_object_unref(nm_proxy);
#endif
#ifdef HAVE_DBUS_GLIB
	uninstall_dbus_status_handler();
	if(connection)
		dbus_g_connection_unref(connection);
#endif
	utils_free_regex();
	exit_claws(mainwin);

	return 0;
}

static void save_all_caches(FolderItem *item, gpointer data)
{
	if (!item->cache) {
		return;
	}

	if (item->opened) {
		folder_item_close(item);
	}

	folder_item_free_cache(item, TRUE);
}

static void exit_claws(MainWindow *mainwin)
{
	gchar *filename;
	gboolean have_connectivity;
	FolderItem *item;

	sc_exiting = TRUE;

	debug_print("shutting down\n");
	inc_autocheck_timer_remove();

#ifdef HAVE_NETWORKMANAGER_SUPPORT
	if (prefs_common.work_offline && went_offline_nm)
		prefs_common.work_offline = FALSE;
#endif

	/* save prefs for opened folder */
	if((item = folderview_get_opened_item(mainwin->folderview)) != NULL) {
		summary_save_prefs_to_folderitem(
			mainwin->summaryview, item);
		if (prefs_common.last_opened_folder != NULL)
			g_free(prefs_common.last_opened_folder);
		prefs_common.last_opened_folder =
			!prefs_common.goto_last_folder_on_startup ? NULL :
			folder_item_get_identifier(item);
	}

	/* save all state before exiting */
	folder_func_to_all_folders(save_all_caches, NULL);
	folder_write_list();

	main_window_get_size(mainwin);
	main_window_get_position(mainwin);

	prefs_common_write_config();
	account_write_config_all();
	passwd_store_write_config();
#ifndef USE_ALT_ADDRBOOK
	addressbook_export_to_file();
#endif
	filename = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, MENU_RC, NULL);
	gtk_accel_map_save(filename);
	g_free(filename);

	/* delete temporary files */
	remove_all_files(get_tmp_dir());
	remove_all_files(get_mime_tmp_dir());

	close_log_file(LOG_PROTOCOL);
	close_log_file(LOG_DEBUG_FILTERING);

#ifdef HAVE_NETWORKMANAGER_SUPPORT
	have_connectivity = networkmanager_is_online(NULL); 
#else
	have_connectivity = TRUE;
#endif
#ifdef HAVE_LIBETPAN
	imap_main_done(have_connectivity);
	nntp_main_done(have_connectivity);
#endif
	/* delete crashfile */
	if (!cmd.crash)
		claws_unlink(get_crashfile_name());

	lock_socket_remove();

#ifdef HAVE_LIBSM
	if (mainwin->smc_conn)
		SmcCloseConnection ((SmcConn)mainwin->smc_conn, 0, NULL);
	mainwin->smc_conn = NULL;
#endif

	main_window_destroy_all();
	
	plugin_unload_all("GTK3");

	matcher_done();
	prefs_toolbar_done();
	avatars_done();

#ifndef USE_ALT_ADDRBOOK
	addressbook_destroy();
#endif
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
	prefs_proxy_done();
	prefs_receive_done();
	prefs_logging_done();
	prefs_send_done();
	tags_write_tags();
#ifdef USE_ENCHANT       
	prefs_spelling_done();
	gtkaspell_checkers_quit();
#endif
	plugin_unload_all("Common");
#ifdef G_OS_WIN32
	win32_close_log();
#endif
	claws_done();
}

#define G_PRINT_EXIT(msg)	\
	{			\
		g_print(msg);	\
		exit(1);	\
	}

static void parse_cmd_compose_from_file(const gchar *fn, GString *body)
{
	GString *headers = g_string_new(NULL);
	gchar *to = NULL;
	gchar *h;
	gchar *v;
	gchar fb[BUFFSIZE];
	FILE *fp;
	gboolean isstdin;

	if (fn == NULL || *fn == '\0')
		G_PRINT_EXIT(_("Missing filename\n"));
	isstdin = (*fn == '-' && *(fn + 1) == '\0');
	if (isstdin)
		fp = stdin;
	else {
		fp = claws_fopen(fn, "r");
		if (!fp)
			G_PRINT_EXIT(_("Cannot open filename for reading\n"));
	}

	while (claws_fgets(fb, sizeof(fb), fp)) {
		gchar *tmp;	
		strretchomp(fb);
		if (*fb == '\0')
			break;
		h = fb;
		while (*h && *h != ':') { ++h; } /* search colon */
        	if (*h == '\0')
			G_PRINT_EXIT(_("Malformed header\n"));
		v = h + 1;
		while (*v && *v == ' ') { ++v; } /* trim value start */
		*h = '\0';
		tmp = g_ascii_strdown(fb, -1); /* get header name */
		if (!strcmp(tmp, "to")) {
			if (to != NULL)
				G_PRINT_EXIT(_("Duplicated 'To:' header\n"));
			to = g_strdup(v);
		} else {
			g_string_append_c(headers, '&');
			g_string_append(headers, tmp);
			g_string_append_c(headers, '=');
			g_string_append_uri_escaped(headers, v, NULL, TRUE);
		}
		g_free(tmp);
	}
	if (to == NULL)
		G_PRINT_EXIT(_("Missing required 'To:' header\n"));
	g_string_append(body, to);
	g_free(to);
	g_string_append(body, "?body=");
	while (claws_fgets(fb, sizeof(fb), fp)) {
		g_string_append_uri_escaped(body, fb, NULL, TRUE);
	}
	if (!isstdin)
		claws_fclose(fp);
	/* append the remaining headers */
	g_string_append(body, headers->str);
	g_string_free(headers, TRUE);
}

#undef G_PRINT_EXIT

static void parse_cmd_opt_error(char *errstr, char* optstr)
{
    gchar tmp[BUFSIZ];

	cm_return_if_fail(errstr != NULL);
	cm_return_if_fail(optstr != NULL);

    g_snprintf(tmp, sizeof(tmp), errstr, optstr);
	g_print(_("%s. Try -h or --help for usage.\n"), tmp);
	exit(1);
}

static GString mailto; /* used to feed cmd.compose_mailto when --compose-from-file is used */

static void parse_cmd_opt(int argc, char *argv[])
{
	AttachInfo *ainfo;
	gint i;

	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--receive-all")) {
			cmd.receive_all = TRUE;
		} else if (!strcmp(argv[i], "--receive")) {
			cmd.receive = TRUE;
		} else if (!strcmp(argv[i], "--cancel-receiving")) {
			cmd.cancel_receiving = TRUE;
		} else if (!strcmp(argv[i], "--cancel-sending")) {
			cmd.cancel_sending = TRUE;
		} else if (!strcmp(argv[i], "--compose-from-file")) {
		    if (i+1 < argc) {
				const gchar *p = argv[i+1];

				parse_cmd_compose_from_file(p, &mailto);
				cmd.compose = TRUE;
				cmd.compose_mailto = mailto.str;
				i++;
		    } else {
                parse_cmd_opt_error(_("Missing file argument for option %s"), argv[i]);
			}
		} else if (!strcmp(argv[i], "--compose")) {
			const gchar *p = (i+1 < argc)?argv[i+1]:NULL;

			cmd.compose = TRUE;
			cmd.compose_mailto = NULL;
			if (p && *p != '\0' && *p != '-') {
				if (!STRNCMP(p, "mailto:")) {
					cmd.compose_mailto = p + 7;
				} else {
					cmd.compose_mailto = p;
				}
				i++;
			}
		} else if (!strcmp(argv[i], "--subscribe")) {
		    if (i+1 < argc) {
				const gchar *p = argv[i+1];
				if (p && *p != '\0' && *p != '-') {
					cmd.subscribe = TRUE;
					cmd.subscribe_uri = p;
				} else {
                    parse_cmd_opt_error(_("Missing or empty uri argument for option %s"), argv[i]);
				}
		    } else {
                parse_cmd_opt_error(_("Missing uri argument for option %s"), argv[i]);
			}
		} else if (!strcmp(argv[i], "--attach") || 
                !strcmp(argv[i], "--insert")) {
		    if (i+1 < argc) {
				const gchar *p = argv[i+1];
				gint ii = i;
				gchar *file = NULL;
				gboolean insert = !strcmp(argv[i], "--insert");

				while (p && *p != '\0' && *p != '-') {
					if ((file = g_filename_from_uri(p, NULL, NULL)) != NULL) {
						if (!is_file_exist(file)) {
							g_free(file);
							file = NULL;
						}
					}
					if (file == NULL && *p != G_DIR_SEPARATOR) {
						file = g_strconcat(claws_get_startup_dir(),
								   G_DIR_SEPARATOR_S,
								   p, NULL);
					} else if (file == NULL) {
						file = g_strdup(p);
					}

					ainfo = g_new0(AttachInfo, 1);
					ainfo->file = file;
					ainfo->insert = insert;
					cmd.attach_files = g_list_append(cmd.attach_files, ainfo);
					ii++;
					p = (ii+1 < argc)?argv[ii+1]:NULL;
				}
				if (ii==i) {
                    parse_cmd_opt_error(_("Missing at least one non-empty file argument for option %s"), argv[i]);
				} else {
                    i=ii;
                }
		    } else {
                parse_cmd_opt_error(_("Missing file argument for option %s"), argv[i]);
			}
		} else if (!strcmp(argv[i], "--send")) {
			cmd.send = TRUE;
		} else if (!strcmp(argv[i], "--version-full") ||
			   !strcmp(argv[i], "-V")) {
			g_print("Claws Mail version " VERSION_GIT_FULL "\n");
			main_dump_features_list(FALSE);
			exit(0);
		} else if (!strcmp(argv[i], "--version") ||
			   !strcmp(argv[i], "-v")) {
			g_print("Claws Mail version " VERSION "\n");
			exit(0);
 		} else if (!strcmp(argv[i], "--status-full")) {
 			const gchar *p = (i+1 < argc)?argv[i+1]:NULL;
 
 			cmd.status_full = TRUE;
 			while (p && *p != '\0' && *p != '-') {
 				if (!cmd.status_full_folders) {
 					cmd.status_full_folders =
 						g_ptr_array_new();
				}
 				g_ptr_array_add(cmd.status_full_folders,
 						g_strdup(p));
 				i++;
 				p = (i+1 < argc)?argv[i+1]:NULL;
 			}
  		} else if (!strcmp(argv[i], "--status")) {
 			const gchar *p = (i+1 < argc)?argv[i+1]:NULL;
 
  			cmd.status = TRUE;
 			while (p && *p != '\0' && *p != '-') {
 				if (!cmd.status_folders)
 					cmd.status_folders = g_ptr_array_new();
 				g_ptr_array_add(cmd.status_folders,
 						g_strdup(p));
 				i++;
 				p = (i+1 < argc)?argv[i+1]:NULL;
 			}
		} else if (!strcmp(argv[i], "--search")) {
		    if (i+3 < argc) { /* 3 first arguments are mandatory */
		    	const char* p;
		    	/* only set search parameters if they are valid */
		    	p = argv[i+1];
				cmd.search_folder    = (p && *p != '\0' && *p != '-')?p:NULL;
				p = argv[i+2];
				cmd.search_type      = (p && *p != '\0' && *p != '-')?p:NULL;
				p = argv[i+3];
				cmd.search_request   = (p && *p != '\0' && *p != '-')?p:NULL;
				p = (i+4 < argc)?argv[i+4]:NULL;
				const char* rec      = (p && *p != '\0' && *p != '-')?p:NULL;
				cmd.search_recursive = TRUE;
				if (rec) {
                    i++;
                    if (tolower(*rec)=='n' || tolower(*rec)=='f' || *rec=='0')
    					cmd.search_recursive = FALSE;
                }
				if (cmd.search_folder && cmd.search_type && cmd.search_request) {
					cmd.search = TRUE;
                    i+=3;
                }
		    } else {
                switch (argc-i-1) {
                case 0:
                    parse_cmd_opt_error(_("Missing folder, type and request arguments for option %s"), argv[i]);
                    break;
                case 1:
                    parse_cmd_opt_error(_("Missing type and request arguments for option %s"), argv[i]);
                    break;
                case 2:
                    parse_cmd_opt_error(_("Missing request argument for option %s"), argv[i]);
                }
			}
		} else if (!strcmp(argv[i], "--online")) {
			cmd.online_mode = ONLINE_MODE_ONLINE;
		} else if (!strcmp(argv[i], "--offline")) {
			cmd.online_mode = ONLINE_MODE_OFFLINE;
		} else if (!strcmp(argv[i], "--toggle-debug")) {
			cmd.debug = TRUE;
		} else if (!strcmp(argv[i], "--statistics")) {
			cmd.statistics = TRUE;
		} else if (!strcmp(argv[i], "--reset-statistics")) {
			cmd.reset_statistics = TRUE;
		} else if (!strcmp(argv[i], "--help") ||
			   !strcmp(argv[i], "-h")) {
			gchar *base = g_path_get_basename(argv[0]);
			g_print(_("Usage: %s [OPTION]...\n"), base);

			g_print("%s\n", _("  --compose [address]    open composition window"));
			g_print("%s\n", _("  --compose-from-file file\n"
				  	  "                         open composition window with data from given file;\n"
			  	  	  "                         use - as file name for reading from standard input;\n"
			  	  	  "                         content format: headers first (To: required) until an\n"
				  	  "                         empty line, then mail body until end of file."));
			g_print("%s\n", _("  --subscribe uri        subscribe to the given URI if possible"));
			g_print("%s\n", _("  --attach file1 [file2]...\n"
					  "                         open composition window with specified files\n"
					  "                         attached"));
			g_print("%s\n", _("  --insert file1 [file2]...\n"
					  "                         open composition window with specified files\n"
					  "                         inserted"));
			g_print("%s\n", _("  --receive              receive new messages"));
			g_print("%s\n", _("  --receive-all          receive new messages of all accounts"));
			g_print("%s\n", _("  --cancel-receiving     cancel receiving of messages"));
			g_print("%s\n", _("  --cancel-sending       cancel sending of messages"));
			g_print("%s\n", _("  --search folder type request [recursive]\n"
					  "                         searches mail\n"
					  "                         folder ex.: \"#mh/Mailbox/inbox\" or \"Mail\"\n"
					  "                         type: s[ubject],f[rom],t[o],e[xtended],m[ixed] or g: tag\n"
					  "                         request: search string\n"
					  "                         recursive: false if arg. starts with 0, n, N, f or F"));

			g_print("%s\n", _("  --send                 send all queued messages"));
 			g_print("%s\n", _("  --status [folder]...   show the total number of messages"));
 			g_print("%s\n", _("  --status-full [folder]...\n"
 			                  "                         show the status of each folder"));
 			g_print("%s\n", _("  --statistics           show session statistics"));
 			g_print("%s\n", _("  --reset-statistics     reset session statistics"));
			g_print("%s\n", _("  --select folder[/msg]  jump to the specified folder/message\n" 
					  "                         folder is a folder id like 'folder/sub_folder', a file:// uri or an absolute path"));
			g_print("%s\n", _("  --online               switch to online mode"));
			g_print("%s\n", _("  --offline              switch to offline mode"));
			g_print("%s\n", _("  --exit --quit -q       exit Claws Mail"));
			g_print("%s\n", _("  --debug -d             debug mode"));
			g_print("%s\n", _("  --toggle-debug         toggle debug mode"));
			g_print("%s\n", _("  --help -h              display this help"));
			g_print("%s\n", _("  --version -v           output version information"));
			g_print("%s\n", _("  --version-full -V      output version and built-in features information"));
			g_print("%s\n", _("  --config-dir           output configuration directory"));
			g_print("%s\n", _("  --alternate-config-dir directory\n"
			                  "                         use specified configuration directory"));
			g_print("%s\n", _("  --geometry -geometry [WxH][+X+Y]\n"
					  "                         set geometry for main window"));

			g_free(base);
			exit(1);
		} else if (!strcmp(argv[i], "--crash")) {
			cmd.crash = TRUE;
			cmd.crash_params = g_strdup((i+1 < argc)?argv[i+1]:NULL);
			i++;
		} else if (!strcmp(argv[i], "--config-dir")) {
			g_print(RC_DIR "\n");
			exit(0);
		} else if (!strcmp(argv[i], "--alternate-config-dir")) {
		    if (i+1 < argc) {
				set_rc_dir(argv[i+1]);
                i++;
		    } else {
                parse_cmd_opt_error(_("Missing directory argument for option %s"), argv[i]);
			}
		} else if (!strcmp(argv[i], "--geometry") ||
		        !strcmp(argv[i], "-geometry")) {
		    if (i+1 < argc) {
				cmd.geometry = argv[i+1];
                i++;
		    } else {
                parse_cmd_opt_error(_("Missing geometry argument for option %s"), argv[i]);
			}
		} else if (!strcmp(argv[i], "--exit") ||
			   !strcmp(argv[i], "--quit") ||
			   !strcmp(argv[i], "-q")) {
			cmd.exit = TRUE;
		} else if (!strcmp(argv[i], "--select")) {
		    if (i+1 < argc) {
				cmd.target = argv[i+1];
                i++;
		    } else {
                parse_cmd_opt_error(_("Missing folder argument for option %s"), argv[i]);
			}
		} else if (i == 1 && argc == 2) {
			/* only one parameter. Do something intelligent about it */
			if ((strstr(argv[i], "@") || !STRNCMP(argv[i], "mailto:")) && !strstr(argv[i], "://")) {
				const gchar *p = argv[i];

				cmd.compose = TRUE;
				cmd.compose_mailto = NULL;
				if (p && *p != '\0' && *p != '-') {
					if (!STRNCMP(p, "mailto:")) {
						cmd.compose_mailto = p + 7;
					} else {
						cmd.compose_mailto = p;
					}
				}
			} else if (!STRNCMP(argv[i], "file://")) {
				cmd.target = argv[i];
			} else if (!STRNCMP(argv[i], "?attach=file://")) {
                /* Thunar support as per 3.3.0cvs19 */
				cmd.compose = TRUE;
				cmd.compose_mailto = argv[i];
			} else if (strstr(argv[i], "://")) {
				const gchar *p = argv[i];
				if (p && *p != '\0' && *p != '-') {
					cmd.subscribe = TRUE;
					cmd.subscribe_uri = p;
				}
			} else if (!strcmp(argv[i], "--sync")) {
				/* gtk debug */
			} else if (is_dir_exist(argv[i]) || is_file_exist(argv[i])) {
				cmd.target = argv[i];
    		} else {
                parse_cmd_opt_error(_("Unknown option %s"), argv[i]);
            }
		} else {
            parse_cmd_opt_error(_("Unknown option %s"), argv[i]);
        }
	}

	if (cmd.attach_files && cmd.compose == FALSE) {
		cmd.compose = TRUE;
		cmd.compose_mailto = NULL;
	}
}

static void initial_processing(FolderItem *item, gpointer data)
{
	MainWindow *mainwin = (MainWindow *)data;
	gchar *buf;

	cm_return_if_fail(item);
	buf = g_strdup_printf(_("Processing (%s)..."), 
			      item->path 
			      ? item->path 
			      : _("top level folder"));
	g_free(buf);

	if (folder_item_parent(item) != NULL && item->prefs->enable_processing) {
		item->processing_pending = TRUE;
		folder_item_apply_processing(item);
		item->processing_pending = FALSE;
	}

	STATUSBAR_POP(mainwin);
}

static gboolean draft_all_messages(void)
{
	const GList *compose_list = NULL;
	
	compose_clear_exit_drafts();
	compose_list = compose_get_compose_list();
	while (compose_list != NULL) {
		Compose *c = (Compose*)compose_list->data;
		if (!compose_draft(c, COMPOSE_DRAFT_FOR_EXIT))
			return FALSE;
		compose_list = compose_get_compose_list();
	}
	return TRUE;
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
	emergency_exit = TRUE;
	exit_claws(static_mainwindow);
	exit(0);

	return FALSE;
}

void app_will_exit(GtkWidget *widget, gpointer data)
{
	MainWindow *mainwin = data;
	
	if (gtk_main_level() == 0) {
		debug_print("not even started\n");
		return;
	}
	if (sc_exiting == TRUE) {
		debug_print("exit pending\n");
		return;
	}
	sc_exiting = TRUE;
	debug_print("exiting\n");
	if (compose_get_compose_list()) {
		if (!draft_all_messages()) {
			main_window_popup(mainwin);
			sc_exiting = FALSE;
			return;
		}
	}

	if (prefs_common.warn_queued_on_exit && procmsg_have_queued_mails_fast()) {
		if (alertpanel(_("Queued messages"),
			       _("Some unsent messages are queued. Exit now?"),
			       NULL, _("_Cancel"), NULL, _("_OK"), NULL, NULL,
			       ALERTFOCUS_FIRST)
		    != G_ALERTALTERNATE) {
			main_window_popup(mainwin);
		    	sc_exiting = FALSE;
			return;
		}
		manage_window_focus_in(mainwin->window, NULL, NULL);
	}

	sock_cleanup();
#ifdef HAVE_VALGRIND
	if (RUNNING_ON_VALGRIND) {
		summary_clear_list(mainwin->summaryview);
	}
#endif
	if (folderview_get_selected_item(mainwin->folderview))
		folder_item_close(folderview_get_selected_item(mainwin->folderview));
	gtk_main_quit();
}

gboolean claws_is_exiting(void)
{
	return sc_exiting;
}

gboolean claws_is_starting(void)
{
	return sc_starting;
}

#ifdef G_OS_UNIX
/*
 * CLAWS: want this public so crash dialog can delete the
 * lock file too
 */
gchar *claws_get_socket_name(void)
{
	static gchar *filename = NULL;
	gchar *socket_dir = NULL;
	gchar md5sum[33];

	if (filename == NULL) {
		GStatBuf st;
		gint stat_ok;

		socket_dir = g_strdup_printf("%s%cclaws-mail",
					   g_get_user_runtime_dir(), G_DIR_SEPARATOR);
		stat_ok = g_stat(socket_dir, &st);
		if (stat_ok < 0 && errno != ENOENT) {
			g_print("Error stat'ing socket_dir %s: %s\n",
				socket_dir, g_strerror(errno));
		} else if (stat_ok == 0 && S_ISSOCK(st.st_mode)) {
			/* old versions used a sock in $TMPDIR/claws-mail-$UID */
			debug_print("Using legacy socket %s\n", socket_dir);
			filename = g_strdup(socket_dir);
			g_free(socket_dir);
			return filename;
		}

		if (!is_dir_exist(socket_dir) && make_dir(socket_dir) < 0) {
			g_print("Error creating socket_dir %s: %s\n",
				socket_dir, g_strerror(errno));
		}

		md5_hex_digest(md5sum, get_rc_dir());

		filename = g_strdup_printf("%s%c%s", socket_dir, G_DIR_SEPARATOR,
					   md5sum);
		g_free(socket_dir);
		debug_print("Using control socket %s\n", filename);
	}

	return filename;
}
#endif

static gchar *get_crashfile_name(void)
{
	static gchar *filename = NULL;

	if (filename == NULL) {
		filename = g_strdup_printf("%s%cclaws-crashed",
					   get_tmp_dir(), G_DIR_SEPARATOR);
	}

	return filename;
}

static gint prohibit_duplicate_launch(int *argc, char ***argv)
{
	gint sock;
	GList *curr;
#ifdef G_OS_UNIX
	gchar *path;

	path = claws_get_socket_name();
	/* Try to connect to the control socket */
	sock = fd_connect_unix(path);

	if (sock < 0) {
		gint ret;
#if HAVE_FLOCK
		gchar *socket_lock;
		gint lock_fd;
		/* If connect failed, no other process is running.
		 * Unlink the potentially existing socket, then
		 * open it. This has to be done locking a temporary
		 * file to avoid the race condition where another
		 * process could have created the socket just in
		 * between.
		 */
		socket_lock = g_strconcat(path, ".lock",
					  NULL);
		lock_fd = g_open(socket_lock, O_RDWR|O_CREAT, 0);
		if (lock_fd < 0) {
			debug_print("Couldn't open %s: %s (%d)\n", socket_lock,
				g_strerror(errno), errno);
			g_free(socket_lock);
			return -1;
		}
		if (flock(lock_fd, LOCK_EX) < 0) {
			debug_print("Couldn't lock %s: %s (%d)\n", socket_lock,
				g_strerror(errno), errno);
			close(lock_fd);
			g_free(socket_lock);
			return -1;
		}
#endif

		claws_unlink(path);
		debug_print("Opening socket %s\n", path);
		ret = fd_open_unix(path);
#if HAVE_FLOCK
		flock(lock_fd, LOCK_UN);
		close(lock_fd);
		claws_unlink(socket_lock);
		g_free(socket_lock);
#endif
		return ret;
	}
#else
        HANDLE hmutex;

        hmutex = CreateMutexA(NULL, FALSE, "ClawsMail");
        if (!hmutex) {
                debug_print("cannot create Mutex\n");
                return -1;
        }
        if (GetLastError() != ERROR_ALREADY_EXISTS) {
                sock = fd_open_inet(50216);
                if (sock < 0)
                        return 0;
                return sock;
        }

        sock = fd_connect_inet(50216);
        if (sock < 0)
                return -1;
#endif
	/* remote command mode */

	debug_print("another Claws Mail instance is already running.\n");

	if (cmd.receive_all) {
		CM_FD_WRITE_ALL("receive_all\n");
	} else if (cmd.receive) {
		CM_FD_WRITE_ALL("receive\n");
	} else if (cmd.cancel_receiving) {
		CM_FD_WRITE_ALL("cancel_receiving\n");
	} else if (cmd.cancel_sending) {
		CM_FD_WRITE_ALL("cancel_sending\n");
	} else if (cmd.compose && cmd.attach_files) {
		gchar *str, *compose_str;

		if (cmd.compose_mailto) {
			compose_str = g_strdup_printf("compose_attach %s\n",
						      cmd.compose_mailto);
		} else {
			compose_str = g_strdup("compose_attach\n");
		}

		CM_FD_WRITE_ALL(compose_str);
		g_free(compose_str);

		for (curr = cmd.attach_files; curr != NULL ; curr = curr->next) {
			str = (gchar *) ((AttachInfo *)curr->data)->file;
			if (((AttachInfo *)curr->data)->insert)
				CM_FD_WRITE_ALL("insert ");
			else
				CM_FD_WRITE_ALL("attach ");
			CM_FD_WRITE_ALL(str);
			CM_FD_WRITE_ALL("\n");
		}

		CM_FD_WRITE_ALL(".\n");
	} else if (cmd.compose) {
		gchar *compose_str;

		if (cmd.compose_mailto) {
			compose_str = g_strdup_printf
				("compose %s\n", cmd.compose_mailto);
		} else {
			compose_str = g_strdup("compose\n");
		}

		CM_FD_WRITE_ALL(compose_str);
		g_free(compose_str);
	} else if (cmd.subscribe) {
		gchar *str = g_strdup_printf("subscribe %s\n", cmd.subscribe_uri);
		CM_FD_WRITE_ALL(str);
		g_free(str);
	} else if (cmd.send) {
		CM_FD_WRITE_ALL("send\n");
	} else if (cmd.online_mode == ONLINE_MODE_ONLINE) {
		CM_FD_WRITE("online\n");
	} else if (cmd.online_mode == ONLINE_MODE_OFFLINE) {
		CM_FD_WRITE("offline\n");
	} else if (cmd.debug) {
		CM_FD_WRITE("debug\n");
 	} else if (cmd.status || cmd.status_full) {
  		gchar buf[BUFFSIZE];
 		gint i;
 		const gchar *command;
 		GPtrArray *folders;
 		gchar *folder;
 
 		command = cmd.status_full ? "status-full\n" : "status\n";
 		folders = cmd.status_full ? cmd.status_full_folders :
 			cmd.status_folders;
 
 		CM_FD_WRITE_ALL(command);
 		for (i = 0; folders && i < folders->len; ++i) {
 			folder = g_ptr_array_index(folders, i);
 			CM_FD_WRITE_ALL(folder);
 			CM_FD_WRITE_ALL("\n");
 		}
 		CM_FD_WRITE_ALL(".\n");
 		for (;;) {
 			fd_gets(sock, buf, sizeof(buf) - 1);
			buf[sizeof(buf) - 1] = '\0';
 			if (!STRNCMP(buf, ".\n")) break;
			if (claws_fputs(buf, stdout) == EOF) {
				g_warning("writing to stdout failed");
				break;
			}
 		}
	} else if (cmd.exit) {
		CM_FD_WRITE_ALL("exit\n");
	} else if (cmd.statistics) {
		gchar buf[BUFSIZ];
		CM_FD_WRITE("statistics\n");
 		for (;;) {
 			fd_gets(sock, buf, sizeof(buf) - 1);
			buf[sizeof(buf) - 1] = '\0';
 			if (!STRNCMP(buf, ".\n")) break;
			if (claws_fputs(buf, stdout) == EOF) {
				g_warning("writing to stdout failed");
				break;
			}
 		}
	} else if (cmd.reset_statistics) {
		CM_FD_WRITE("reset_statistics\n");
	} else if (cmd.target) {
		gchar *str = g_strdup_printf("select %s\n", cmd.target);
		CM_FD_WRITE_ALL(str);
		g_free(str);
	} else if (cmd.search) {
		gchar buf[BUFFSIZE];
		gchar *str =
			g_strdup_printf("search %s\n%s\n%s\n%c\n",
							cmd.search_folder, cmd.search_type, cmd.search_request,
							(cmd.search_recursive==TRUE)?'1':'0');
		CM_FD_WRITE_ALL(str);
		g_free(str);
		for (;;) {
			fd_gets(sock, buf, sizeof(buf) - 1);
			buf[sizeof(buf) - 1] = '\0';
			if (!STRNCMP(buf, ".\n")) break;
			if (claws_fputs(buf, stdout) == EOF) {
				g_warning("writing to stdout failed");
				break;
			}
		}
	} else {
#ifdef G_OS_UNIX
		gchar buf[BUFSIZ];
		CM_FD_WRITE_ALL("get_display\n");
		memset(buf, 0, sizeof(buf));
		fd_gets(sock, buf, sizeof(buf) - 1);
		buf[sizeof(buf) - 1] = '\0';

		/* Try to connect to a display; if it is the same one as
		 * the other Claws instance, then ask it to pop up. */
		int diff_display = 1;
		if (gtk_init_check(argc, argv)) {
			GdkDisplay *display = gdk_display_get_default();
			diff_display = g_strcmp0(buf, gdk_display_get_name(display));
		}
		if (diff_display) {
			g_print("Claws Mail is already running on display %s.\n",
				buf);
		} else {
			g_print("Claws Mail is already running on this display (%s).\n",
				buf);
			fd_close(sock);
			sock = fd_connect_unix(path);
			CM_FD_WRITE_ALL("popup\n");
		}
#else
		CM_FD_WRITE_ALL("popup\n");
#endif
	}

	fd_close(sock);
	return -1;
}

static gint lock_socket_remove(void)
{
#ifdef G_OS_UNIX
	gchar *filename, *dirname;
#endif
	if (lock_socket < 0) {
		return -1;
	}

	if (lock_socket_tag > 0) {
		g_source_remove(lock_socket_tag);
	}
	fd_close(lock_socket);

#ifdef G_OS_UNIX
	filename = claws_get_socket_name();
	dirname = g_path_get_dirname(filename);
	if (claws_unlink(filename) < 0)
                FILE_OP_ERROR(filename, "claws_unlink");
	g_rmdir(dirname);
	g_free(dirname);
#endif

	return 0;
}

static GPtrArray *get_folder_item_list(gint sock)
{
	gchar buf[BUFFSIZE];
	FolderItem *item;
	GPtrArray *folders = NULL;

	for (;;) {
		fd_gets(sock, buf, sizeof(buf) - 1);
		buf[sizeof(buf) - 1] = '\0';
		if (!STRNCMP(buf, ".\n")) {
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
			g_warning("no such folder: %s", buf);
		}
	}

	return folders;
}

static void lock_socket_input_cb(gpointer data,
				 gint source,
				 GIOCondition condition)
{
	MainWindow *mainwin = (MainWindow *)data;
	gint sock;
	gchar buf[BUFFSIZE];

	sock = fd_accept(source);
	if (sock < 0)
		return;

	fd_gets(sock, buf, sizeof(buf) - 1);
	buf[sizeof(buf) - 1] = '\0';

	if (!STRNCMP(buf, "popup")) {
		main_window_popup(mainwin);
#ifdef G_OS_UNIX
	} else if (!STRNCMP(buf, "get_display")) {
		GdkDisplay* display = gtk_widget_get_display(mainwin->window);
		const gchar *display_name = gdk_display_get_name(display);
		CM_FD_WRITE_ALL(display_name);
#endif
	} else if (!STRNCMP(buf, "receive_all")) {
		inc_all_account_mail(mainwin, FALSE, FALSE,
				     prefs_common.newmail_notify_manu);
	} else if (!STRNCMP(buf, "receive")) {
		inc_mail(mainwin, prefs_common.newmail_notify_manu);
	} else if (!STRNCMP(buf, "cancel_receiving")) {
		inc_cancel_all();
		imap_cancel_all();
	} else if (!STRNCMP(buf, "cancel_sending")) {
		send_cancel();
	} else if (!STRNCMP(buf, "compose_attach")) {
		GList *files = NULL, *curr;
		AttachInfo *ainfo;
		gchar *mailto;

		mailto = g_strdup(buf + strlen("compose_attach") + 1);
		while (fd_gets(sock, buf, sizeof(buf) - 1) > 0) {
			buf[sizeof(buf) - 1] = '\0';
			strretchomp(buf);
			if (!g_strcmp0(buf, "."))
				break;

			ainfo = g_new0(AttachInfo, 1);
			ainfo->file = g_strdup(strstr(buf, " ") + 1);
			ainfo->insert = !STRNCMP(buf, "insert ");
			files = g_list_append(files, ainfo);
		}
		open_compose_new(mailto, files);
		
		curr = g_list_first(files);
		while (curr != NULL) {
			ainfo = (AttachInfo *)curr->data;
			g_free(ainfo->file);
			g_free(ainfo);
			curr = curr->next;
		}
		g_list_free(files);
		g_free(mailto);
	} else if (!STRNCMP(buf, "compose")) {
		open_compose_new(buf + strlen("compose") + 1, NULL);
	} else if (!STRNCMP(buf, "subscribe")) {
		main_window_popup(mainwin);
		folder_subscribe(buf + strlen("subscribe") + 1);
	} else if (!STRNCMP(buf, "send")) {
		send_queue();
	} else if (!STRNCMP(buf, "online")) {
		main_window_toggle_work_offline(mainwin, FALSE, FALSE);
	} else if (!STRNCMP(buf, "offline")) {
		main_window_toggle_work_offline(mainwin, TRUE, FALSE);
	} else if (!STRNCMP(buf, "debug")) {
		debug_set_mode(debug_get_mode() ? FALSE : TRUE);
 	} else if (!STRNCMP(buf, "status-full") ||
 		   !STRNCMP(buf, "status")) {
 		gchar *status;
 		GPtrArray *folders;
 
 		folders = get_folder_item_list(sock);
 		status = folder_get_status
 			(folders, !STRNCMP(buf, "status-full"));
 		CM_FD_WRITE_ALL(status);
 		CM_FD_WRITE_ALL(".\n");
 		g_free(status);
 		if (folders) g_ptr_array_free(folders, TRUE);
	} else if (!STRNCMP(buf, "statistics")) {
		gchar tmp[BUFSIZ];

		g_snprintf(tmp, sizeof(tmp), _("Session statistics\n"));
 		CM_FD_WRITE_ALL(tmp);

		if (prefs_common.date_format) {
			struct tm *lt;
			gint len = 100;
			gchar date[len];

			lt = localtime(&session_stats.time_started);
			fast_strftime(date, len, prefs_common.date_format, lt);
			g_snprintf(tmp, sizeof(tmp), _("Started: %s\n"),
					lt ? date : ctime(&session_stats.time_started));
		} else
			g_snprintf(tmp, sizeof(tmp), _("Started: %s\n"),
					ctime(&session_stats.time_started));
 		CM_FD_WRITE_ALL(tmp);

 		CM_FD_WRITE_ALL("\n");

		g_snprintf(tmp, sizeof(tmp), _("Incoming traffic\n"));
 		CM_FD_WRITE_ALL(tmp);

		g_snprintf(tmp, sizeof(tmp), _("Received messages: %d\n"),
				session_stats.received);
 		CM_FD_WRITE_ALL(tmp);

		if (session_stats.spam > 0) {
			g_snprintf(tmp, sizeof(tmp), _("Spam messages: %d\n"),
					session_stats.spam);
			CM_FD_WRITE_ALL(tmp);
		}

 		CM_FD_WRITE_ALL("\n");

		g_snprintf(tmp, sizeof(tmp), _("Outgoing traffic\n"));
 		CM_FD_WRITE_ALL(tmp);

		g_snprintf(tmp, sizeof(tmp), _("New/redirected messages: %d\n"),
				session_stats.sent);
 		CM_FD_WRITE_ALL(tmp);

		g_snprintf(tmp, sizeof(tmp), _("Replied messages: %d\n"),
				session_stats.replied);
 		CM_FD_WRITE_ALL(tmp);

		g_snprintf(tmp, sizeof(tmp), _("Forwarded messages: %d\n"),
				session_stats.forwarded);
 		CM_FD_WRITE_ALL(tmp);

		g_snprintf(tmp, sizeof(tmp), _("Total outgoing messages: %d\n"),
				(session_stats.sent + session_stats.replied +
				 session_stats.forwarded));
 		CM_FD_WRITE_ALL(tmp);

 		CM_FD_WRITE_ALL(".\n");
	} else if (!STRNCMP(buf, "reset_statistics")) {
		reset_statistics();
	} else if (!STRNCMP(buf, "select ")) {
		const gchar *target = buf+7;
		mainwindow_jump_to(target, TRUE);
	} else if (!STRNCMP(buf, "search ")) {
		FolderItem* folderItem = NULL;
		GSList *messages = NULL;
		gchar *folder_name = NULL;
		gchar *request = NULL;
		AdvancedSearch *search;
		gboolean recursive;
		AdvancedSearchType searchType = ADVANCED_SEARCH_EXTENDED;
		
		search = advsearch_new();

		folder_name = g_strdup(buf+7);
		strretchomp(folder_name);

		if (fd_gets(sock, buf, sizeof(buf) - 1) <= 0) 
			goto search_exit;
		buf[sizeof(buf) - 1] = '\0';

		switch (toupper(buf[0])) {
		case 'S': searchType = ADVANCED_SEARCH_SUBJECT; break;
		case 'F': searchType = ADVANCED_SEARCH_FROM; break;
		case 'T': searchType = ADVANCED_SEARCH_TO; break;
		case 'M': searchType = ADVANCED_SEARCH_MIXED; break;
		case 'G': searchType = ADVANCED_SEARCH_TAG; break;
		case 'E': searchType = ADVANCED_SEARCH_EXTENDED; break;
		}

		if (fd_gets(sock, buf, sizeof(buf) - 1) <= 0) 
			goto search_exit;

		buf[sizeof(buf) - 1] = '\0';
		request = g_strdup(buf);
		strretchomp(request);

		recursive = TRUE;
		if (fd_gets(sock, buf, sizeof(buf) - 1) > 0)
			recursive = buf[0] != '0';

		buf[sizeof(buf) - 1] = '\0';

		debug_print("search: %s %i %s %i\n", folder_name, searchType, request, recursive);

		folderItem = folder_find_item_from_identifier(folder_name);

		if (folderItem == NULL) {
			debug_print("Unknown folder item : '%s', searching folder\n",folder_name);
			Folder* folder = folder_find_from_path(folder_name);
			if (folder != NULL)
				folderItem = FOLDER_ITEM(folder->node->data);
			else
				debug_print("Unknown folder: '%s'\n",folder_name);
		} else {
			debug_print("%s %s\n",folderItem->name, folderItem->path);
		}

		if (folderItem != NULL) {
			advsearch_set(search, searchType, request);
			advsearch_search_msgs_in_folders(search, &messages, folderItem, recursive);
		} else {
			g_print("Folder '%s' not found.\n'", folder_name);
		}

		GSList *cur;
		for (cur = messages; cur != NULL; cur = cur->next) {
			MsgInfo* msg = (MsgInfo *)cur->data;
			gchar *file = procmsg_get_message_file_path(msg);
			CM_FD_WRITE_ALL(file);
			CM_FD_WRITE_ALL("\n");
			g_free(file);
		}
		CM_FD_WRITE_ALL(".\n");

search_exit:
		g_free(folder_name);
		g_free(request);
		advsearch_free(search);
		if (messages != NULL)
			procmsg_msg_list_free(messages);
	} else if (!STRNCMP(buf, "exit")) {
		if (prefs_common.clean_on_exit && !prefs_common.ask_on_clean) {
			procmsg_empty_all_trash();
                }
		app_will_exit(NULL, mainwin);
	}
	fd_close(sock);

}

static void open_compose_new(const gchar *address, GList *attach_files)
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
	gchar *errstr = NULL;
	gboolean error = FALSE;
	for (list = folder_get_list(); list != NULL; list = list->next) {
		Folder *folder = list->data;

		if (folder->queue) {
			gint res = procmsg_send_queue
				(folder->queue, prefs_common.savemsg,
				&errstr);

			if (res) {
				folder_item_scan(folder->queue);
			}
			
			if (res < 0)
				error = TRUE;
		}
	}
	if (errstr) {
		alertpanel_error_log(_("Some errors occurred "
				"while sending queued messages:\n%s"), errstr);
		g_free(errstr);
	} else if (error) {
		alertpanel_error_log("Some errors occurred "
				"while sending queued messages.");
	}
}

#ifndef G_OS_WIN32
static void quit_signal_handler(int sig)
{
	debug_print("Quitting on signal %d\n", sig);

	g_timeout_add(0, clean_quit, NULL);
}
#endif

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

#ifdef HAVE_NETWORKMANAGER_SUPPORT
static void networkmanager_state_change_cb(DBusGProxy *proxy, gchar *dev,
					 gpointer data)
{
	MainWindow *mainWin;

	mainWin = NULL;
	if (static_mainwindow)
		mainWin = static_mainwindow;
	else if (data)
		mainWin = (MainWindow*)data;
	
	if (!prefs_common.use_networkmanager)
		return;

	if (mainWin) {
		GError *error = NULL;
		gboolean online;

		online = networkmanager_is_online(&error);
		if(!error) {
			if(online && went_offline_nm) {
				went_offline_nm = FALSE;
				main_window_toggle_work_offline(mainWin, FALSE, FALSE);
				debug_print("NetworkManager: Went online\n");
				log_message(LOG_PROTOCOL, _("NetworkManager: network is online.\n"));
			}
			else if(!online) {
				went_offline_nm = TRUE;
				main_window_toggle_work_offline(mainWin, TRUE, FALSE);
				debug_print("NetworkManager: Went offline\n");
				log_message(LOG_PROTOCOL, _("NetworkManager: network is offline.\n"));
			}
		}
		else {
			debug_print("Failed to get online information from NetworkManager: %s\n",
							 error->message);
			g_error_free(error);
		}
	}
	else
		debug_print("NetworkManager: Cannot change connection state because "
						 "main window does not exist\n");
}

/* Returns true (and sets error appropriately, if given) in case of error */
gboolean networkmanager_is_online(GError **error)
{
	DBusGConnection *connection;
	DBusGProxy *proxy;
	GError *tmp_error = NULL;
	gboolean retVal;
	guint32 state;

	if (!prefs_common.use_networkmanager)
		return TRUE;

	tmp_error = NULL;
	proxy = NULL;
	connection = dbus_g_bus_get(DBUS_BUS_SYSTEM, &tmp_error);

	if(!connection) {
		/* If calling code doesn't do error checking, at least print some debug */
		if((error == NULL) || (*error == NULL))
			debug_print("Failed to open connection to system bus: %s\n",
							 tmp_error->message);
		g_propagate_error(error, tmp_error);
		return TRUE;
	}

	proxy = dbus_g_proxy_new_for_name(connection,
			"org.freedesktop.NetworkManager",
			"/org/freedesktop/NetworkManager",
			"org.freedesktop.NetworkManager");

	retVal = dbus_g_proxy_call(proxy,"state",&tmp_error, G_TYPE_INVALID,
			G_TYPE_UINT, &state, G_TYPE_INVALID);

	if(proxy)
		g_object_unref(proxy);
	if(connection)
		dbus_g_connection_unref(connection);

	if(!retVal) {
		/* If calling code doesn't do error checking, at least print some debug */
		if((error == NULL) || (*error == NULL))
			debug_print("Failed to get state info from NetworkManager: %s\n",
							 tmp_error->message);
		g_propagate_error(error, tmp_error);
		return TRUE;
	}
    	return (state == NM_STATE_CONNECTED_LOCAL ||
		state == NM_STATE_CONNECTED_SITE ||
		state == NM_STATE_CONNECTED_GLOBAL ||
		state == NM_STATE_UNKNOWN);
}
#endif
