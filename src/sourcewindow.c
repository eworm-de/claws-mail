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

#include "defs.h"

#include <glib.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtktext.h>
#include <gtk/gtkstyle.h>
#include <stdio.h>
#include <stdlib.h>

#include "intl.h"
#include "sourcewindow.h"
#include "procmsg.h"
#include "utils.h"
#include "gtkutils.h"
#include "prefs_common.h"

static void source_window_destroy_cb	(GtkWidget	*widget,
					 SourceWindow	*sourcewin);
static void key_pressed			(GtkWidget	*widget,
					 GdkEventKey	*event,
					 SourceWindow	*sourcewin);

static GdkFont *msgfont = NULL;

#define FONT_LOAD(font, s) \
{ \
	gchar *fontstr, *p; \
 \
	Xstrdup_a(fontstr, s, ); \
	if ((p = strchr(fontstr, ',')) != NULL) *p = '\0'; \
	font = gdk_font_load(fontstr); \
}

static void source_window_init()
{
	if (msgfont != NULL || prefs_common.textfont == NULL)
		return;

	if (MB_CUR_MAX == 1) {
		FONT_LOAD(msgfont, prefs_common.textfont);
	} else {
		msgfont = gdk_fontset_load(prefs_common.textfont);
	}
}

SourceWindow *source_window_create(void)
{
	SourceWindow *sourcewin;
	GtkWidget *window;
	GtkWidget *scrolledwin;
	GtkWidget *text;

	debug_print(_("Creating source window...\n"));
	sourcewin = g_new0(SourceWindow, 1);

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), _("Source of the message"));
	gtk_window_set_wmclass(GTK_WINDOW(window), "source_window", "Sylpheed");
	gtk_window_set_policy(GTK_WINDOW(window), TRUE, TRUE, FALSE);
	gtk_widget_set_usize(window, 600, 500);
	gtk_signal_connect(GTK_OBJECT(window), "destroy",
			   GTK_SIGNAL_FUNC(source_window_destroy_cb),
			   sourcewin);
	gtk_signal_connect(GTK_OBJECT(window), "key_press_event",
			   GTK_SIGNAL_FUNC(key_pressed), sourcewin);
	gtk_widget_realize(window);

	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
				       GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
	gtk_container_add(GTK_CONTAINER(window), scrolledwin);
	gtk_widget_show(scrolledwin);

	text = gtk_text_new(gtk_scrolled_window_get_hadjustment
			    (GTK_SCROLLED_WINDOW(scrolledwin)),
			    gtk_scrolled_window_get_vadjustment
			    (GTK_SCROLLED_WINDOW(scrolledwin)));
	gtk_container_add(GTK_CONTAINER(scrolledwin), text);
	gtk_widget_show(text);

	sourcewin->window = window;
	sourcewin->scrolledwin = scrolledwin;
	sourcewin->text = text;

	source_window_init();

	return sourcewin;
}

void source_window_show(SourceWindow *sourcewin)
{
	gtk_widget_show_all(sourcewin->window);
}

void source_window_destroy(SourceWindow *sourcewin)
{
	g_free(sourcewin);
}

void source_window_show_msg(SourceWindow *sourcewin, MsgInfo *msginfo)
{
	gchar *file;
	gchar *title;
	FILE *fp;
	gchar buf[BUFFSIZE];

	g_return_if_fail(msginfo != NULL);

	file = procmsg_get_message_file(msginfo);
	g_return_if_fail(file != NULL);

	if ((fp = fopen(file, "r")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		g_free(file);
		return;
	}

	debug_print(_("Displaying the source of %s ...\n"), file);

	title = g_strdup_printf(_("%s - Source"), file);
	gtk_window_set_title(GTK_WINDOW(sourcewin->window), title);
	g_free(title);
	g_free(file);

	gtk_text_freeze(GTK_TEXT(sourcewin->text));

	while (fgets(buf, sizeof(buf), fp) != NULL)
		source_window_append(sourcewin, buf);

	gtk_text_thaw(GTK_TEXT(sourcewin->text));

	fclose(fp);
}

void source_window_append(SourceWindow *sourcewin, const gchar *str)
{
	gtk_text_insert(GTK_TEXT(sourcewin->text), msgfont, NULL, NULL,
			str, -1);
}

static void source_window_destroy_cb(GtkWidget *widget,
				     SourceWindow *sourcewin)
{
	source_window_destroy(sourcewin);
}

static void key_pressed(GtkWidget *widget, GdkEventKey *event,
			SourceWindow *sourcewin)
{
	if (event && event->keyval == GDK_Escape)
		gtk_widget_destroy(sourcewin->window);
}
