/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999,2000 Hiroyuki Yamamoto
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
#include <gtk/gtkcontainer.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtktext.h>
#include <stdio.h>
#include <string.h>

#include "intl.h"
#include "main.h"
#include "headerwindow.h"
#include "mainwindow.h"
#include "procheader.h"
#include "procmsg.h"
#include "codeconv.h"
#include "utils.h"

static GdkFont *normalfont;
static GdkFont *boldfont;

static void key_pressed(GtkWidget *widget, GdkEventKey *event,
			HeaderWindow *headerwin);

HeaderWindow *header_window_create(void)
{
	HeaderWindow *headerwin;
	GtkWidget *window;
	GtkWidget *scrolledwin;
	GtkWidget *text;

	debug_print(_("Creating header window...\n"));
	headerwin = g_new0(HeaderWindow, 1);

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), _("All header"));
	gtk_window_set_policy(GTK_WINDOW(window), TRUE, TRUE, FALSE);
	gtk_widget_set_usize(window, 600, 500);
	gtk_signal_connect(GTK_OBJECT(window), "delete_event",
			   GTK_SIGNAL_FUNC(gtk_widget_hide_on_delete), NULL);
	gtk_signal_connect(GTK_OBJECT(window), "key_press_event",
			   GTK_SIGNAL_FUNC(key_pressed), headerwin);
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

	headerwin->window = window;
	headerwin->scrolledwin = scrolledwin;
	headerwin->text = text;

	return headerwin;
}

void header_window_init(HeaderWindow *headerwin)
{
	//if (!normalfont)
	//	normalfont = gdk_fontset_load(NORMAL_FONT);
	if (!boldfont)
		boldfont = gdk_fontset_load(BOLD_FONT);
}

void header_window_show(HeaderWindow *headerwin, MsgInfo *msginfo)
{
	gchar *file;
	FILE *fp;
	gchar buf[BUFFSIZE], tmp[BUFFSIZE];
	GtkText *text = GTK_TEXT(headerwin->text);
	gboolean parse_next;

	g_return_if_fail(msginfo != NULL);

	file = procmsg_get_message_file_path(msginfo);
	if ((fp = fopen(file, "r")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		g_free(file);
		return;
	}

	debug_print(_("Displaying the header of %s ...\n"), file);

	g_snprintf(buf, sizeof(buf), _("%s - All header"), file);
	gtk_window_set_title(GTK_WINDOW(headerwin->window), buf);
	g_free(file);

	gtk_text_freeze(text);
	gtk_text_backward_delete(text, gtk_text_get_length(text));
	gtk_text_thaw(text);
	gtk_text_freeze(text);

	parse_next = MSG_IS_QUEUED(msginfo->flags);

	for (;;) {
		gint val;
		gchar *p;

		val = procheader_get_one_field(buf, sizeof(buf), fp, NULL);
		if (val == -1) {
			if (parse_next) {
				parse_next = FALSE;
				gtk_text_insert(text, normalfont, NULL, NULL,
						"\n", 1);
				continue;
			} else
				break;
		}
		conv_unmime_header(tmp, sizeof(tmp), buf, NULL);
		if ((p = strchr(tmp, ':')) != NULL) {
			gtk_text_insert(text, boldfont, NULL, NULL,
					tmp, p - tmp + 1);
			gtk_text_insert(text, normalfont, NULL, NULL,
					p + 1, -1);
		} else
			gtk_text_insert(text, normalfont, NULL, NULL, tmp, -1);
		gtk_text_insert(text, NULL, NULL, NULL, "\n", 1);
	}

	fclose(fp);
	gtk_text_thaw(text);
}

void header_window_show_cb(gpointer data, guint action, GtkWidget *widget)
{
	MainWindow *mainwin = data;
	GtkCTreeNode *node = mainwin->summaryview->displayed;
	MsgInfo *msginfo;

	if (node && !GTK_WIDGET_VISIBLE(mainwin->headerwin->window)) {
		msginfo = gtk_ctree_node_get_row_data
			(GTK_CTREE(mainwin->summaryview->ctree), node);
		header_window_show(mainwin->headerwin, msginfo);
	}

	gtk_widget_hide(mainwin->headerwin->window);
	gtk_widget_show(mainwin->headerwin->window);
}

static void key_pressed(GtkWidget *widget, GdkEventKey *event,
			HeaderWindow *headerwin)
{
	if (event && event->keyval == GDK_Escape)
		gtk_widget_hide(headerwin->window);
}
