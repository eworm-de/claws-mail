/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2002 by the Sylpheed Claws Team and  Hiroyuki Yamamoto
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
#  include <config.h>
#endif

#include <glib.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include <errno.h>
#include <fcntl.h>

#include "intl.h"
#include "crash.h"
#include "utils.h"
#include "prefs.h"

static void crash_handler			(int sig);
static gboolean is_crash_dialog_allowed		(void);
static void crash_debug				(unsigned long crash_pid, GString *string);
static gboolean crash_create_debugger_file	(void);

static const gchar *DEBUG_SCRIPT = "bt full\nq";

/***/

GtkWidget *crash_dialog_new (const gchar *text, const gchar *debug_output)
{
	GtkWidget *window1;
	GtkWidget *vbox1;
	GtkWidget *label1;
	GtkWidget *scrolledwindow1;
	GtkWidget *text1;
	GtkWidget *hbuttonbox1;
	GtkWidget *close;
	GtkWidget *button3;

	window1 = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_object_set_data(GTK_OBJECT(window1), "window1", window1);
	gtk_window_set_title(GTK_WINDOW(window1), _("Sylpheed Claws - It Bites!"));

	vbox1 = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(vbox1);
	gtk_container_add(GTK_CONTAINER(window1), vbox1);
	gtk_container_set_border_width(GTK_CONTAINER (vbox1), 1);

	label1 = gtk_label_new(text);
	gtk_widget_show(label1);
	gtk_box_pack_start(GTK_BOX(vbox1), label1, FALSE, FALSE, 0);

	scrolledwindow1 = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_show(scrolledwindow1);
	gtk_box_pack_start(GTK_BOX(vbox1), scrolledwindow1, TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwindow1), 
				       GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);

	text1 = gtk_text_new(NULL, NULL);
	gtk_text_insert(GTK_TEXT(text1), NULL, NULL, NULL, 
			debug_output, strlen(debug_output));
	gtk_widget_show(text1);
	gtk_container_add(GTK_CONTAINER (scrolledwindow1), text1);
	
	hbuttonbox1 = gtk_hbutton_box_new();
	gtk_widget_show(hbuttonbox1);
	gtk_box_pack_start(GTK_BOX(vbox1), hbuttonbox1, FALSE, TRUE, 0);
	gtk_button_box_set_child_ipadding(GTK_BUTTON_BOX (hbuttonbox1), 5, -1);

	close = gtk_button_new_with_label(_("Close"));
	gtk_widget_show(close);
	gtk_container_add(GTK_CONTAINER(hbuttonbox1), close);
	GTK_WIDGET_SET_FLAGS(close, GTK_CAN_DEFAULT);

	button3 = gtk_button_new_with_label(_("Save..."));
	gtk_widget_show(button3);
	gtk_container_add(GTK_CONTAINER(hbuttonbox1), button3);
	GTK_WIDGET_SET_FLAGS(button3, GTK_CAN_DEFAULT);

	gtk_signal_connect(GTK_OBJECT(window1), "delete_event",
			   GTK_SIGNAL_FUNC(gtk_main_quit), NULL);
	gtk_signal_connect(GTK_OBJECT(close),   "clicked",
			   GTK_SIGNAL_FUNC(gtk_main_quit), NULL);

	gtk_widget_show(window1);
	gtk_main();
	return window1;
}

/***/

void crash_install_handlers(void)
{
#if HAVE_GDB
	sigset_t mask;

	if (!is_crash_dialog_allowed()) return;

	sigemptyset(&mask);

#ifdef SIGSEGV
	signal(SIGSEGV, crash_handler);
	sigaddset(&mask, SIGSEGV);
#endif
	
#ifdef SIGFPE
	signal(SIGFPE, crash_handler);
	sigaddset(&mask, SIGFPE);
#endif

#ifdef SIGILL
	signal(SIGILL, crash_handler);
	sigaddset(&mask, SIGILL);
#endif

#ifdef SIGABRT
	signal(SIGABRT, crash_handler);
	sigaddset(&mask, SIGABRT);
#endif

	sigprocmask(SIG_UNBLOCK, &mask, 0);
#endif /* HAVE_GDB */	
}

/***/

/*
 *\brief	crash dialog entry point 
 */
void crash_main(const char *arg) 
{
#if HAVE_GDB
	gchar *text;
	gchar **tokens;
	unsigned long pid;
	GString *output;

	crash_create_debugger_file();
	tokens = g_strsplit(arg, ",", 0);

	pid = atol(tokens[0]);
	text = g_strdup_printf("Sylpheed process (%lx) received signal %ld",
			       pid, atol(tokens[1]));

	output = g_string_new("DEBUG LOG\n");     
	crash_debug(pid, output);
	crash_dialog_new(text, output->str);
	g_string_free(output, TRUE);
	g_free(text);
	g_strfreev(tokens);
#endif /* HAVE_GDB */	
}

/*
 *\brief	create debugger script file in sylpheed directory.
 *		all the other options (creating temp files) looked too 
 *		convoluted.
 */
static gboolean crash_create_debugger_file(void)
{
	PrefFile *pf;
	gchar *filespec = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, DEBUGGERRC, NULL);

	pf = prefs_write_open(filespec);
	g_free(filespec);
	if (pf) 
		fprintf(pf->fp, DEBUG_SCRIPT);
	prefs_write_close(pf);	
}

/*
 *\brief	launches debugger and attaches it to crashed sylpheed
 */
static void crash_debug(unsigned long crash_pid, GString *string)
{
	int choutput[2];
	pid_t pid;

	pipe(choutput);

	if (0 == (pid = fork())) {
		const char *argp[9];
		const char **argptr = argp;
		const gchar *cmdline;
		gchar *filespec = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, DEBUGGERRC, NULL);

		setgid(getgid());
		setuid(getuid());

		/*
		 * setup debugger to attach to crashed sylpheed
		 */
		*argptr++ = "--nw";
		*argptr++ = "--nx";
		*argptr++ = "--quiet";
		*argptr++ = "--batch";
		*argptr++ = "--command";
		*argptr++ = g_strdup_printf("%s", filespec);
		*argptr++ = "sylpheed"; /* program file name */
		*argptr++ = g_strdup_printf("%d", crash_pid);
		*argptr   = NULL;

		/*
		 * redirect output to write end of pipe
		 */
		close(1);
		dup(choutput[1]);
		close(choutput[0]);
		if (-1 == execvp("gdb", argp)) 
			puts("error execvp\n");
	} else {
		char buf[100];
		int r;
	
		waitpid(pid, NULL, 0);

		/*
		 * make it non blocking
		 */
		if (-1 == fcntl(choutput[0], F_SETFL, O_NONBLOCK))
			puts("set to non blocking failed\n");

		/*
		 * get the output
		 */
		do {
			r = read(choutput[0], buf, sizeof buf - 1);
			if (r > 0) {
				buf[r] = 0;
				g_string_append(string, buf);
			}
		} while (r > 0);
		
		close(choutput[0]);
		close(choutput[1]);
		
		/*
		 * kill the process we attached to
		 */
		kill(crash_pid, SIGCONT); 
	}
}

/*
 *\brief	checks KDE, GNOME and Sylpheed specific variables
 *		to see if the crash dialog is allowed (because some
 *		developers may prefer to run sylpheed under gdb...)
 */
static gboolean is_crash_dialog_allowed(void)
{
	return getenv("KDE_DEBUG") || 
	       !getenv("GNOME_DISABLE_CRASH_DIALOG") ||
	       !getenv("SYLPHEED_NO_CRASH");
}

/*
 *\brief	this handler will probably evolve into 
 *		something better.
 */
static void crash_handler(int sig)
{
	pid_t pid;
	static volatile unsigned long crashed_ = 0;

	/*
	 * besides guarding entrancy it's probably also better 
	 * to mask off signals
	 */
	if (crashed_) 
		return;

	crashed_++;

	/*
	 * gnome ungrabs focus, and flushes gdk. mmmh, good idea.
	 */
	gdk_pointer_ungrab(GDK_CURRENT_TIME);
	gdk_keyboard_ungrab(GDK_CURRENT_TIME);
	gdk_flush();
	 
	if (0 == (pid = fork())) {
		char buf[50];
		const char *args[4];
	
		/*
		 * probably also some other parameters (like GTK+ ones).
		 */
		args[0] = "--debug";
		args[1] = "--crash";
		sprintf(buf, "%ld,%d", getppid(), sig);
		args[2] = buf;
		args[3] = NULL;

		setgid(getgid());
		setuid(getuid());
#if 0		
		execvp("/alfons/Projects/sylpheed-claws/src/sylpheed", args);
#else
		execvp("sylpheed", args);
#endif
	} else {
		waitpid(pid, NULL, 0);
		_exit(253);
	}

	_exit(253);
}

