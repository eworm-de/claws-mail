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
#include <gtk/gtkmain.h>
#include <gtk/gtkrc.h>

#if HAVE_GDK_IMLIB
#  include <gdk_imlib.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>

#if HAVE_LOCALE_H
#  include <locale.h>
#endif

#if USE_GPGME
#  include <gpgme.h>
#  include "passphrase.h"
#endif

#include "intl.h"
#include "main.h"
#include "mainwindow.h"
#include "folderview.h"
#include "summaryview.h"
#include "prefs_common.h"
#include "prefs_filter.h"
#include "prefs_account.h"
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

#if USE_GPGME
#  include "rfc2015.h"
#endif
#if USE_SSL
#  include "ssl.h"
#endif

#include "version.h"

gchar *prog_version;
gchar *startup_dir;
gboolean debug_mode = FALSE;

static gint lock_socket = -1;
static gint lock_socket_tag = 0;

static struct Cmd {
	gboolean receive;
	gboolean receive_all;
	gboolean compose;
	const gchar *compose_mailto;
	gboolean status;
	gboolean send;
} cmd;

static void parse_cmd_opt(int argc, char *argv[]);

#if USE_GPGME
static void idle_function_for_gpgme(void);
#endif /* USE_GPGME */

static gint prohibit_duplicate_launch	(void);
static void lock_socket_input_cb	(gpointer	   data,
					 gint		   source,
					 GdkInputCondition condition);
static gchar *get_socket_name		(void);

static void open_compose_new_with_recipient	(const gchar	*address);

static void send_queue			(void);

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

int main(int argc, char *argv[])
{
	gchar *userrc;
	MainWindow *mainwin;
	FolderView *folderview;

	setlocale(LC_ALL, "");
	bindtextdomain(PACKAGE, LOCALEDIR);
	textdomain(PACKAGE);

	parse_cmd_opt(argc, argv);

	gtk_set_locale();
	gtk_init(&argc, &argv);

#if USE_THREADS || USE_LDAP
	g_thread_init(NULL);
	if (!g_thread_supported())
		g_error(_("g_thread is not supported by glib.\n"));
#endif

#if HAVE_GDK_IMLIB
	gdk_imlib_init();
	gtk_widget_push_visual(gdk_imlib_get_visual());
	gtk_widget_push_colormap(gdk_imlib_get_colormap());
#endif

#if USE_SSL
	ssl_init();
#endif

	srandom((gint)time(NULL));

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

	prog_version = PROG_VERSION;
	startup_dir = g_get_current_dir();

	CHDIR_RETURN_VAL_IF_FAIL(get_home_dir(), 1);

	/* check and create unix domain socket */
	lock_socket = prohibit_duplicate_launch();
	if (lock_socket < 0) return 0;

	if (cmd.status) {
		puts("0 Sylpheed not running.");
		return 0;
	}

	/* backup if old rc file exists */
	if (is_file_exist(RC_DIR)) {
		if (rename(RC_DIR, RC_DIR ".bak") < 0)
			FILE_OP_ERROR(RC_DIR, "rename");
	}
	MAKE_DIR_IF_NOT_EXIST(RC_DIR);
	MAKE_DIR_IF_NOT_EXIST(get_imap_cache_dir());
	MAKE_DIR_IF_NOT_EXIST(get_news_cache_dir());
	MAKE_DIR_IF_NOT_EXIST(get_mime_tmp_dir());
	MAKE_DIR_IF_NOT_EXIST(RC_DIR G_DIR_SEPARATOR_S "uidl");

	if (is_file_exist(RC_DIR G_DIR_SEPARATOR_S "sylpheed.log")) {
		if (rename(RC_DIR G_DIR_SEPARATOR_S "sylpheed.log",
			   RC_DIR G_DIR_SEPARATOR_S "sylpheed.log.bak") < 0)
			FILE_OP_ERROR("sylpheed.log", "rename");
	}
	set_log_file(RC_DIR G_DIR_SEPARATOR_S "sylpheed.log");

	if (is_file_exist(RC_DIR G_DIR_SEPARATOR_S "assortrc") &&
	    !is_file_exist(RC_DIR G_DIR_SEPARATOR_S "filterrc")) {
		if (rename(RC_DIR G_DIR_SEPARATOR_S "assortrc",
			   RC_DIR G_DIR_SEPARATOR_S "filterrc") < 0)
			FILE_OP_ERROR(RC_DIR G_DIR_SEPARATOR_S "assortrc",
				      "rename");
	}

	prefs_common_init();
	prefs_common_read_config();

#if USE_GPGME
	if (gpgme_check_engine()) {  /* Also does some gpgme init */
		rfc2015_disable_all();
		debug_print("gpgme_engine_version:\n%s\n",
			    gpgme_get_engine_info());

		if (prefs_common.gpg_warning) {
			AlertValue val;

			val = alertpanel_message_with_disable
				(_("Warning"),
				 _("GnuPG is not installed properly.\n"
				   "OpenPGP support disabled."));
			if (val & G_ALERTDISABLE)
				prefs_common.gpg_warning = FALSE;
		}
	}
	gpgme_register_idle(idle_function_for_gpgme);
#endif

#if USE_PSPELL
	gtkpspellcheckers = gtkpspell_checkers_new();
#endif
	

	prefs_common_save_config();
	prefs_filter_read_config();
	prefs_filter_write_config();
	prefs_actions_read_config();
	prefs_actions_write_config();
	prefs_display_header_read_config();
	prefs_display_header_write_config();
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
	account_save_config_all();

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
	processing_apply(mainwin->summaryview);

	addressbook_read_file();

	inc_autocheck_timer_init(mainwin);

	/* ignore SIGPIPE signal for preventing sudden death of program */
	signal(SIGPIPE, SIG_IGN);

	if (cmd.receive_all || prefs_common.chk_on_startup)
		inc_all_account_mail(mainwin, prefs_common.newmail_notify_manu);
	else if (cmd.receive)
		inc_mail(mainwin, prefs_common.newmail_notify_manu);
	else
		gtk_widget_grab_focus(folderview->ctree);

	if (cmd.compose)
		open_compose_new_with_recipient(cmd.compose_mailto);
	if (cmd.send)
		send_queue();

	gtk_main();

	addressbook_destroy();

#if USE_PSPELL       
	gtkpspell_checkers_delete();
#endif

	return 0;
}

static void parse_cmd_opt(int argc, char *argv[])
{
	gint i;

	for (i = 1; i < argc; i++) {
		if (!strncmp(argv[i], "--debug", 7))
			debug_mode = TRUE;
		else if (!strncmp(argv[i], "--receive-all", 13))
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
		} else if (!strncmp(argv[i], "--send", 6)) {
			cmd.send = TRUE;
		} else if (!strncmp(argv[i], "--version", 9)) {
			puts("Sylpheed version " VERSION);
			exit(0);
		} else if (!strncmp(argv[i], "--status", 8)) {
			cmd.status = TRUE;
		} else if (!strncmp(argv[i], "--help", 6)) {
			g_print(_("Usage: %s [OPTION]...\n"),
				g_basename(argv[0]));

			puts(_("  --compose [address]    open composition window"));
			puts(_("  --receive              receive new messages"));
			puts(_("  --receive-all          receive new messages of all accounts"));
			puts(_("  --send                 send all queued messages"));
			puts(_("  --status               show the total number of messages"));
			puts(_("  --debug                debug mode"));
			puts(_("  --help                 display this help and exit"));
			puts(_("  --version              output version information and exit"));

			exit(1);
		}
	}
}

static gint get_queued_message_num(void)
{
	FolderItem *queue;

	queue = folder_get_default_queue();
	if (!queue) return -1;

	folder_item_scan(queue);
	return queue->total;
}

static void save_all_caches(FolderItem *item, gpointer data)
{
	if(!item->cache)
		return;
		
	folder_item_write_cache(item);
}

void app_will_exit(GtkWidget *widget, gpointer data)
{
	MainWindow *mainwin = data;
	gchar *filename;

	if (compose_get_compose_list()) {
		if (alertpanel(_("Notice"),
			       _("Composing message exists. Really quit?"),
			       _("OK"), _("Cancel"), NULL) != G_ALERTDEFAULT)
			return;
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

#if USE_GPGME
        gpgmegtk_free_passphrase();
#endif

	if (prefs_common.clean_on_exit)
		main_window_empty_trash(mainwin, prefs_common.ask_on_clean);

	/* save all state before exiting */
	folder_write_list();
	summary_write_cache(mainwin->summaryview);
	folder_func_to_all_folders(save_all_caches, NULL);

	main_window_get_size(mainwin);
	main_window_get_position(mainwin);
	prefs_common_save_config();
	prefs_filter_write_config();
	account_save_config_all();
	addressbook_export_to_file();

	filename = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, MENU_RC, NULL);
	gtk_item_factory_dump_rc(filename, NULL, TRUE);
	g_free(filename);

	/* delete temporary files */
	remove_all_files(get_mime_tmp_dir());

	close_log_file();

	/* delete unix domain socket */
	gdk_input_remove(lock_socket_tag);
	fd_close(lock_socket);
	filename = get_socket_name();
	unlink(filename);

#if USE_SSL
	ssl_done();
#endif

	gtk_main_quit();
}

#if USE_GPGME
static void idle_function_for_gpgme(void)
{
	while (gtk_events_pending())
		gtk_main_iteration();
}
#endif /* USE_GPGME */

static gchar *get_socket_name(void)
{
	static gchar *filename = NULL;

	if (filename == NULL) {
		filename = g_strdup_printf("%s%csylpheed-%d",
					   g_get_tmp_dir(), G_DIR_SEPARATOR,
					   getuid());
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

	debug_print(_("another Sylpheed is already running.\n"));

	if (cmd.receive_all)
		fd_write(uxsock, "receive_all\n", 12);
	else if (cmd.receive)
		fd_write(uxsock, "receive\n", 8);
	else if (cmd.compose) {
		gchar *compose_str;

		if (cmd.compose_mailto)
			compose_str = g_strdup_printf("compose %s\n", cmd.compose_mailto);
		else
			compose_str = g_strdup("compose\n");

		fd_write(uxsock, compose_str, strlen(compose_str));
		g_free(compose_str);
	} else if (cmd.send) {
		fd_write(uxsock, "send\n", 5);
	} else if (cmd.status) {
		gchar buf[BUFFSIZE];

		fd_write(uxsock, "status\n", 7);
		fd_gets(uxsock, buf, sizeof(buf));
		fputs(buf, stdout);
	} else
		fd_write(uxsock, "popup\n", 6);

	fd_close(uxsock);
	return -1;
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

	if (!strncmp(buf, "popup", 5)){
		main_window_popup(mainwin);
	} else if (!strncmp(buf, "receive_all", 11)){
		main_window_popup(mainwin);
		inc_all_account_mail(mainwin, prefs_common.newmail_notify_manu);
	} else if (!strncmp(buf, "receive", 7)){
		main_window_popup(mainwin);
		inc_mail(mainwin, prefs_common.newmail_notify_manu);
	} else if (!strncmp(buf, "compose", 7)) {
		open_compose_new_with_recipient(buf + strlen("compose") + 1);
	} else if (!strncmp(buf, "send", 4)) {
		send_queue();
	} else if (!strncmp(buf, "status", 6)) {
		guint new, unread, total;

		folder_count_total_msgs(&new, &unread, &total);
		g_snprintf(buf, sizeof(buf), "%d %d %d\n", new, unread, total);
		fd_write(sock, buf, strlen(buf));
	}

	fd_close(sock);
}

static void open_compose_new_with_recipient(const gchar *address)
{
	gchar *addr = NULL;

	if (address) {
		Xstrdup_a(addr, address, return);
		g_strstrip(addr);
	}

	if (addr && *addr != '\0')
		compose_new_with_recipient(NULL, addr);
	else
		compose_new(NULL);
}

static void send_queue(void)
{
	GList *list;
	FolderItem *def_outbox;

	def_outbox = folder_get_default_outbox();

	for (list = folder_get_list(); list != NULL; list = list->next) {
		Folder *folder = list->data;

		if (folder->queue) {
			if (procmsg_send_queue
				(folder->queue, prefs_common.savemsg) < 0)
				alertpanel_error(_("Some errors occurred while sending queued messages."));
			statusbar_pop_all();
			folder_item_scan(folder->queue);
			folderview_update_item(folder->queue, TRUE);
			if (prefs_common.savemsg && folder->outbox) {
				folderview_update_item(folder->outbox, TRUE);
				if (folder->outbox == def_outbox)
					def_outbox = NULL;
			}
		}
	}

	if (prefs_common.savemsg && def_outbox)
		folderview_update_item(def_outbox, TRUE);
}
