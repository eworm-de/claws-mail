/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2007 Hiroyuki Yamamoto and the Claws Mail team
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
#ifdef HAVE_LIBSM
#include <X11/SM/SMlib.h>
#include <fcntl.h>
#endif

#include "wizard.h"
#ifdef HAVE_STARTUP_NOTIFICATION
# define SN_API_NOT_YET_FROZEN
# include <libsn/sn-launchee.h>
# include <gdk/gdkx.h>
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
#ifdef HAVE_VALGRIND
#include "valgrind.h"
#endif
#if USE_OPENSSL
#  include "ssl.h"
#endif

#include "version.h"

#include "crash.h"

#include "timing.h"

gchar *prog_version;
gchar *argv0;

#ifdef HAVE_STARTUP_NOTIFICATION
static SnLauncheeContext *sn_context = NULL;
static SnDisplay *sn_display = NULL;
#endif

static gint lock_socket = -1;
static gint lock_socket_tag = 0;
static gchar *x_display = NULL;
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
	const gchar *target;
} cmd;

static void parse_cmd_opt(int argc, char *argv[]);

static gint prohibit_duplicate_launch	(void);
static gchar * get_crashfile_name	(void);
static gint lock_socket_remove		(void);
static void lock_socket_input_cb	(gpointer	   data,
					 gint		   source,
					 GdkInputCondition condition);

static void open_compose_new		(const gchar	*address,
					 GPtrArray	*attach_files);

static void send_queue			(void);
static void initial_processing		(FolderItem *item, gpointer data);
static void quit_signal_handler         (int sig);
static void install_basic_sighandlers   (void);
static void exit_claws			(MainWindow *mainwin);

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

static void claws_gtk_idle(void) 
{
	while(gtk_events_pending()) {
		gtk_main_iteration();
	}
	g_usleep(50000);
}

static gboolean defer_check_all(void *data)
{
	gboolean autochk = GPOINTER_TO_INT(data);

	inc_all_account_mail(static_mainwindow, autochk, 
			prefs_common.newmail_notify_manu);

	return FALSE;
}

static gboolean defer_check(void *data)
{
	inc_mail(static_mainwindow, prefs_common.newmail_notify_manu);

	return FALSE;
}

static gboolean defer_jump(void *data)
{
	mainwindow_jump_to(data);
	return FALSE;
}

static void chk_update_val(GtkWidget *widget, gpointer data)
{
        gboolean *val = (gboolean *)data;
	*val = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
}

static gboolean migrate_old_config(const gchar *old_cfg_dir, const gchar *new_cfg_dir, const gchar *oldversion)
{
	gchar *message = g_strdup_printf(_("Configuration for %s (or previous) found.\n"
			 "Do you want to migrate this configuration?"), oldversion);
	gint r = 0;
	GtkWidget *window = NULL;
	GtkWidget *keep_backup_chk;
	GtkTooltips *tips = gtk_tooltips_new();
	gboolean backup = TRUE;

	keep_backup_chk = gtk_check_button_new_with_label (_("Keep old configuration"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(keep_backup_chk), TRUE);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(tips), keep_backup_chk,
			     _("Keeping a backup will allow you to go back to an "
			       "older version, but may take a while if you have "
			       "cached IMAP or News data, and will take some extra "
			       "room on your disk."),
			     NULL);

	g_signal_connect(G_OBJECT(keep_backup_chk), "toggled", 
			G_CALLBACK(chk_update_val), &backup);

	if (alertpanel_full(_("Migration of configuration"), message,
		 	GTK_STOCK_NO, "+" GTK_STOCK_YES, NULL, FALSE,
			keep_backup_chk, ALERT_QUESTION, G_ALERTDEFAULT) != G_ALERTALTERNATE) {
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
		gtk_widget_destroy(window);
		
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
		gtk_widget_destroy(window);
		
		/* if g_rename failed, we'll try to copy */
		if (r != 0) {
			FILE_OP_ERROR(new_cfg_dir, "g_rename failed, trying copy\n");
			goto backup_mode;
		}
	}
	return (r == 0);
}

static void migrate_common_rc(const gchar *old_rc, const gchar *new_rc)
{
	FILE *oldfp, *newfp;
	gchar *plugin_path, *old_plugin_path, *new_plugin_path;
	gchar buf[BUFFSIZE];
	oldfp = g_fopen(old_rc, "r");
	if (!oldfp)
		return;
	newfp = g_fopen(new_rc, "w");
	if (!newfp) {
		fclose(oldfp);
		return;
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
	while (fgets(buf, sizeof(buf), oldfp)) {
		if (strncmp(buf, old_plugin_path, strlen(old_plugin_path))) {
			fputs(buf, newfp);
		} else {
			debug_print("->replacing %s", buf);
			debug_print("  with %s%s", new_plugin_path, buf+strlen(old_plugin_path));
			fputs(new_plugin_path, newfp);
			fputs(buf+strlen(old_plugin_path), newfp);
		}
	}
	g_free(plugin_path);
	g_free(new_plugin_path);
	g_free(old_plugin_path);
	fclose(oldfp);
	fclose(newfp);
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

		if (context && GTK_IS_OBJECT (context)) {
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

		if (error_string_ret[0] || mainwin->smc_conn == NULL)
			g_warning ("While connecting to session manager:\n%s.",
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
		}
	}
}
#endif

static gboolean sc_exiting = FALSE;
static gboolean sc_starting = FALSE;
static gboolean show_at_startup = TRUE;

void main_set_show_at_startup(gboolean show)
{
	show_at_startup = show;
}

int main(int argc, char *argv[])
{
	gchar *userrc;
	MainWindow *mainwin;
	FolderView *folderview;
	GdkPixbuf *icon;
	gboolean crash_file_present = FALSE;
	gint num_folder_class = 0;
	gboolean asked_for_migration = FALSE;

	START_TIMING("startup");
	
	sc_starting = TRUE;

	if (!claws_init(&argc, &argv)) {
		return 0;
	}

	prefs_prepare_cache();
	prog_version = PROG_VERSION;
	argv0 = g_strdup(argv[0]);

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
		puts("0 Claws Mail not running.");
		lock_socket_remove();
		return 0;
	}
	
	if (cmd.exit)
		return 0;
#endif
	if (!g_thread_supported())
		g_thread_init(NULL);

	gtk_set_locale();
	gtk_init(&argc, &argv);

	gdk_rgb_init();
	gtk_widget_set_default_colormap(gdk_rgb_get_colormap());
	gtk_widget_set_default_visual(gdk_rgb_get_visual());

	if (!g_thread_supported()) {
		g_error(_("g_thread is not supported by glib.\n"));
	}

	/* check that we're not on a too recent/old gtk+ */
#if GTK_CHECK_VERSION(2, 9, 0)
	if (gtk_check_version(2, 9, 0) != NULL) {
		alertpanel_error(_("Claws Mail has been compiled with "
				   "a more recent GTK+ library than is "
				   "currently available. This will cause "
				   "crashes. You need to upgrade GTK+ or "
				   "recompile Claws Mail."));
		exit(1);
	}
#else
	if (gtk_check_version(2, 9, 0) == NULL) {
		alertpanel_error(_("Claws Mail has been compiled with "
				   "an older GTK+ library than is "
				   "currently available. This will cause "
				   "crashes. You need to recompile "
				   "Claws Mail."));
		exit(1);
	}
#endif	
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
	
	/* no config dir exists. See if we can migrate an old config. */
	if (!is_dir_exist(RC_DIR)) {
		prefs_destroy_cache();
		gboolean r = FALSE;
		
		/* if one of the old dirs exist, we'll ask if the user 
		 * want to migrates, and r will be TRUE if he said yes
		 * and migration succeeded, and FALSE otherwise.
		 */
		if (is_dir_exist(OLD_GTK2_RC_DIR)) {
			r = migrate_old_config(OLD_GTK2_RC_DIR, RC_DIR, "Sylpheed-Claws 2.6.0");
			asked_for_migration = TRUE;
		} else if (is_dir_exist(OLDER_GTK2_RC_DIR)) {
			r = migrate_old_config(OLDER_GTK2_RC_DIR, RC_DIR, "Sylpheed-Claws 1.9.15");
			asked_for_migration = TRUE;
		} else if (is_dir_exist(OLD_GTK1_RC_DIR)) {
			r = migrate_old_config(OLD_GTK1_RC_DIR, RC_DIR, "Sylpheed-Claws 1.0.5");
			asked_for_migration = TRUE;
		}
		
		/* If migration failed or the user didn't want to do it,
		 * we create a new one (and we'll hit wizard later). 
		 */
		if (r == FALSE && !is_dir_exist(RC_DIR) && make_dir(RC_DIR) < 0)
			exit(1);
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

	if (is_file_exist("claws.log")) {
		if (rename_force("claws.log", "claws.log.bak") < 0)
			FILE_OP_ERROR("claws.log", "rename");
	}
	set_log_file("claws.log");

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
	stock_pixbuf_gdk(NULL, STOCK_PIXMAP_CLAWS_MAIL_ICON, &icon);
	gtk_window_set_default_icon(icon);

	folderview_initialize();

	mh_gtk_init();
	imap_gtk_init();
	news_gtk_init();

	mainwin = main_window_create();

	manage_window_focus_in(mainwin->window, NULL, NULL);
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

	/* If we can't read a folder list or don't have accounts,
	 * it means the configuration's not done. Either this is
	 * a brand new install, either a failed/refused migration.
	 * So we'll start the wizard.
	 */
	if (folder_read_list() < 0) {
		prefs_destroy_cache();
		
		/* if run_wizard returns FALSE it's because it's
		 * been cancelled. We can't do much but exit.
		 * however, if the user was asked for a migration,
		 * we remove the newly created directory so that
		 * he's asked again for migration on next launch.*/
		if (!run_wizard(mainwin, TRUE)) {
			if (asked_for_migration)
				remove_dir_recursive(RC_DIR);
			exit(1);
		}
		main_window_reflect_prefs_all_now();
		folder_write_list();
	}

	if (!account_get_list()) {
		prefs_destroy_cache();
		if (!run_wizard(mainwin, FALSE)) {
			if (asked_for_migration)
				remove_dir_recursive(RC_DIR);
			exit(1);
		}
		account_read_config_all();
		if(!account_get_list()) {
			exit_claws(mainwin);
			exit(1);
		}
	}

	
	toolbar_main_set_sensitive(mainwin);
	main_window_set_menu_sensitive(mainwin);

	/* if crashed, show window early so that the user
	 * sees what's happening */
	if (!cmd.crash && crash_file_present)
		main_window_popup(mainwin);

#ifdef HAVE_LIBETPAN
	imap_main_init(prefs_common.skip_ssl_cert_check);
#endif	
	account_set_missing_folder();
	folder_set_missing_folders();
	folderview_set(folderview);

	prefs_matcher_read_config();

	/* make one all-folder processing before using claws */
	main_window_cursor_wait(mainwin);
	folder_func_to_all_folders(initial_processing, (gpointer *)mainwin);

	/* if claws crashed, rebuild caches */
	if (!cmd.crash && crash_file_present) {
		GTK_EVENTS_FLUSH();
		debug_print("Claws Mail crashed, checking for new messages in local folders\n");
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

	claws_register_idle_function(claws_gtk_idle);

	prefs_toolbar_init();

	num_folder_class = g_list_length(folder_get_list());

	plugin_load_all("GTK2");

	if (g_list_length(folder_get_list()) != num_folder_class) {
		debug_print("new folders loaded, reloading processing rules\n");
		prefs_matcher_read_config();
	}
	
	if (plugin_get_unloaded_list() != NULL) {
		main_window_cursor_normal(mainwin);
		alertpanel_warning(_("Some plugin(s) failed to load. "
				     "Check the Plugins configuration "
				     "for more information."));
		main_window_cursor_wait(mainwin);
	}

 	plugin_load_standard_plugins ();
       
	/* if not crashed, show window now */
	if (!(!cmd.crash && crash_file_present)) {
		/* apart if something told not to show */
		if (show_at_startup)
			main_window_popup(mainwin);
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
	startup_notification_complete(FALSE);
#endif
#ifdef HAVE_LIBSM
	sc_session_manager_connect(mainwin);
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
	
	if (cmd.target) {
		g_timeout_add(500, defer_jump, (gpointer)cmd.target);
	}

	prefs_destroy_cache();
	
	compose_reopen_exit_drafts();

	sc_starting = FALSE;
	END_TIMING();

	gtk_main();

	exit_claws(mainwin);

	return 0;
}

static void save_all_caches(FolderItem *item, gpointer data)
{
	if (!item->cache) {
		return;
	}

	if (item->opened)
		folder_item_close(item);
	
	folder_item_free_cache(item, TRUE);
}

static void exit_claws(MainWindow *mainwin)
{
	gchar *filename;

	sc_exiting = TRUE;

	debug_print("shutting down\n");
	inc_autocheck_timer_remove();

	if (prefs_common.clean_on_exit && !emergency_exit) {
		main_window_empty_trash(mainwin, prefs_common.ask_on_clean);
	}

	/* save prefs for opened folder */
	if(mainwin->folderview->opened) {
		FolderItem *item;

		item = gtk_ctree_node_get_row_data(GTK_CTREE(mainwin->folderview->ctree), mainwin->folderview->opened);
		summary_save_prefs_to_folderitem(mainwin->folderview->summaryview, item);
	}

	/* save all state before exiting */
	folder_func_to_all_folders(save_all_caches, NULL);
	folder_write_list();

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

#ifdef HAVE_LIBSM
	if (mainwin->smc_conn)
		SmcCloseConnection ((SmcConn)mainwin->smc_conn, 0, NULL);
	mainwin->smc_conn = NULL;
#endif

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
	plugin_unload_all("Common");
	claws_done();
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
			gchar *file = NULL;

			while (p && *p != '\0' && *p != '-') {
				if (!cmd.attach_files) {
					cmd.attach_files = g_ptr_array_new();
				}
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
				g_ptr_array_add(cmd.attach_files, file);
				i++;
				p = argv[i + 1];
			}
		} else if (!strncmp(argv[i], "--send", 6)) {
			cmd.send = TRUE;
		} else if (!strncmp(argv[i], "--version", 9) ||
			   !strncmp(argv[i], "-v", 2)) {
			puts("Claws Mail version " VERSION);
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
		} else if (!strncmp(argv[i], "--help", 6) ||
			   !strncmp(argv[i], "-h", 2)) {
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
			g_print("%s\n", _("  --select folder[/msg]  jumps to the specified folder/message\n" 
			                  "                         folder is a folder id like 'folder/sub_folder'"));
			g_print("%s\n", _("  --online               switch to online mode"));
			g_print("%s\n", _("  --offline              switch to offline mode"));
			g_print("%s\n", _("  --exit --quit -q       exit Claws Mail"));
			g_print("%s\n", _("  --debug                debug mode"));
			g_print("%s\n", _("  --help -h              display this help and exit"));
			g_print("%s\n", _("  --version -v           output version information and exit"));
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
		} else if (!strncmp(argv[i], "--exit", 6) ||
			   !strncmp(argv[i], "--quit", 6) ||
			   !strncmp(argv[i], "-q", 2)) {
			cmd.exit = TRUE;
		} else if (!strncmp(argv[i], "--select", 8) && i+1 < argc) {
			cmd.target = argv[i+1];
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
			} else if (!strcmp(argv[i], "--sync")) {
				/* gtk debug */
			} else {
				g_print(_("Unknown option\n"));
				exit(1);
			}
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

	g_return_if_fail(item);
	buf = g_strdup_printf(_("Processing (%s)..."), 
			      item->path 
			      ? item->path 
			      : _("top level folder"));
	g_free(buf);

	
	if (item->prefs->enable_processing) {
		folder_item_apply_processing(item);
	}

	STATUSBAR_POP(mainwin);
}

static void draft_all_messages(void)
{
	GList *compose_list = NULL;
	
	compose_clear_exit_drafts();
	while ((compose_list = compose_get_compose_list()) != NULL) {
		Compose *c = (Compose*)compose_list->data;
		compose_draft(c, COMPOSE_DRAFT_FOR_EXIT);
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
	emergency_exit = TRUE;
	exit_claws(static_mainwindow);
	exit(0);

	return FALSE;
}

void app_will_exit(GtkWidget *widget, gpointer data)
{
	MainWindow *mainwin = data;
	
	if (sc_exiting == TRUE) {
		debug_print("exit pending\n");
		return;
	}
	sc_exiting = TRUE;
	debug_print("exiting\n");
	if (compose_get_compose_list()) {
		draft_all_messages();
	}

	if (prefs_common.warn_queued_on_exit && procmsg_have_queued_mails_fast()) {
		if (alertpanel(_("Queued messages"),
			       _("Some unsent messages are queued. Exit now?"),
			       GTK_STOCK_CANCEL, GTK_STOCK_OK, NULL)
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

/*
 * CLAWS: want this public so crash dialog can delete the
 * lock file too
 */
gchar *claws_get_socket_name(void)
{
	static gchar *filename = NULL;

	if (filename == NULL) {
		filename = g_strdup_printf("%s%cclaws-mail-%d",
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
		filename = g_strdup_printf("%s%cclaws-crashed",
					   get_tmp_dir(), G_DIR_SEPARATOR);
	}

	return filename;
}

static gint prohibit_duplicate_launch(void)
{
	gint uxsock;
	gchar *path;

	path = claws_get_socket_name();
	uxsock = fd_connect_unix(path);
	
	if (x_display == NULL)
		x_display = g_strdup(g_getenv("DISPLAY"));

	if (uxsock < 0) {
		g_unlink(path);
		return fd_open_unix(path);
	}

	/* remote command mode */

	debug_print("another Claws Mail instance is already running.\n");

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
	} else if (cmd.target) {
		gchar *str = g_strdup_printf("select %s\n", cmd.target);
		fd_write_all(uxsock, str, strlen(str));
		g_free(str);
	} else {
		gchar buf[BUFSIZ];
		fd_write_all(uxsock, "get_display\n", 12);
		fd_gets(uxsock, buf, sizeof(buf));
		if (strcmp2(buf, x_display)) {
			printf("Claws Mail is already running on display %s.\n",
				buf);
		} else {
			fd_close(uxsock);
			uxsock = fd_connect_unix(path);
			fd_write_all(uxsock, "popup\n", 6);
		}
	}

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
	filename = claws_get_socket_name();
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
	} else if (!strncmp(buf, "get_display", 11)) {
		fd_write_all(sock, x_display, strlen(x_display));
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
	} else if (!strncmp(buf, "select ", 7)) {
		const gchar *target = buf+7;
		mainwindow_jump_to(target);
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
	gchar *errstr = NULL;
	for (list = folder_get_list(); list != NULL; list = list->next) {
		Folder *folder = list->data;

		if (folder->queue) {
			gint res = procmsg_send_queue
				(folder->queue, prefs_common.savemsg,
				&errstr);

			if (res) {
				folder_item_scan(folder->queue);
			}
		}
	}
	if (errstr) {
		gchar *tmp = g_strdup_printf(_("Some errors occurred "
				"while sending queued messages:\n%s"), errstr);
		g_free(errstr);
		alertpanel_error_log(tmp);
		g_free(tmp);
	} else {
		alertpanel_error_log("Some errors occurred "
				"while sending queued messages.");
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
