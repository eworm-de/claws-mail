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

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtkstatusbar.h>
#include <gtk/gtkprogressbar.h>
#include <stdarg.h>

#include "mainwindow.h"
#include "statusbar.h"
#include "gtkutils.h"
#include "utils.h"
#include "log.h"
#include "hooks.h"

#define BUFFSIZE 1024

static GList *statusbar_list = NULL;
gint statusbar_puts_all_hook_id = -1;

GtkWidget *statusbar_create(void)
{
	GtkWidget *statusbar;

	statusbar = gtk_statusbar_new();
	gtk_widget_set_size_request(statusbar, 1, -1);
	statusbar_list = g_list_append(statusbar_list, statusbar);
	gtk_statusbar_set_has_resize_grip(GTK_STATUSBAR(statusbar), 
					  FALSE);
	gtk_container_set_border_width(GTK_CONTAINER(statusbar), 1);

	gtk_widget_ref(GTK_STATUSBAR(statusbar)->label);
	gtk_container_remove(GTK_CONTAINER(GTK_STATUSBAR(statusbar)->frame),
		GTK_STATUSBAR(statusbar)->label);
	gtk_widget_hide(GTK_STATUSBAR(statusbar)->frame);
	gtk_box_pack_start (GTK_BOX(statusbar), GTK_STATUSBAR(statusbar)->label, 
		TRUE, TRUE, 0);
	gtk_widget_unref(GTK_STATUSBAR(statusbar)->label);
	gtk_container_remove(GTK_CONTAINER(statusbar),
		GTK_STATUSBAR(statusbar)->frame);
	GTK_STATUSBAR(statusbar)->frame = gtk_frame_new(NULL);

	return statusbar;
}

void statusbar_puts(GtkStatusbar *statusbar, const gchar *str)
{
	gint cid;
	gchar *buf;
	gchar *tmp;

	tmp = g_strdup(str);
	strretchomp(tmp);
	buf = trim_string(tmp, 76);
	g_free(tmp);

	cid = gtk_statusbar_get_context_id(statusbar, "Standard Output");
	gtk_statusbar_pop(statusbar, cid);
	gtk_statusbar_push(statusbar, cid, buf);
	gtkut_widget_draw_now(GTK_WIDGET(statusbar));

	g_free(buf);
}

void statusbar_puts_all(const gchar *str)
{
	GList *cur;

	for (cur = statusbar_list; cur != NULL; cur = cur->next)
		statusbar_puts(GTK_STATUSBAR(cur->data), str);
}

void statusbar_print(GtkStatusbar *statusbar, const gchar *format, ...)
{
	va_list args;
	gchar buf[BUFFSIZE];

	va_start(args, format);
	g_vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);

	statusbar_puts(statusbar, buf);
}

void statusbar_print_all(const gchar *format, ...)
{
	va_list args;
	gchar buf[BUFFSIZE];
	GList *cur;

	va_start(args, format);
	g_vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);

	for (cur = statusbar_list; cur != NULL; cur = cur->next)
		statusbar_puts(GTK_STATUSBAR(cur->data), buf);
}

void statusbar_pop_all(void)
{
	GList *cur;
	gint cid;

	for (cur = statusbar_list; cur != NULL; cur = cur->next) {
		cid = gtk_statusbar_get_context_id(GTK_STATUSBAR(cur->data),
						   "Standard Output");
		gtk_statusbar_pop(GTK_STATUSBAR(cur->data), cid);
	}
}

gboolean statusbar_puts_all_hook (gpointer source, gpointer data)
{
	LogText *logtext = (LogText *) source;

	g_return_val_if_fail(logtext != NULL, TRUE);
	g_return_val_if_fail(logtext->text != NULL, TRUE);

	statusbar_pop_all();
	if (logtext->type == LOG_NORMAL) {
		statusbar_puts_all(logtext->text + LOG_TIME_LEN);
	} else if (logtext->type == LOG_MSG) {
		statusbar_puts_all(logtext->text);
	}

	return FALSE;
}

void statusbar_verbosity_set(gboolean verbose)
{
	if (verbose && (statusbar_puts_all_hook_id == -1)) {
		statusbar_puts_all_hook_id =
			hooks_register_hook(LOG_APPEND_TEXT_HOOKLIST, statusbar_puts_all_hook, NULL);
	} else if (!verbose && (statusbar_puts_all_hook_id != -1)) {
		hooks_unregister_hook(LOG_APPEND_TEXT_HOOKLIST, statusbar_puts_all_hook_id);
		statusbar_puts_all_hook_id = -1;
		statusbar_pop_all();
	}
}

void statusbar_progress_all (gint done, gint total, gint step) 
{
	gchar buf[32];
	if (total && done % step == 0) {
		g_snprintf(buf, sizeof(buf), "%d / %d", done, total);
		gtk_progress_bar_set_text
			(GTK_PROGRESS_BAR(mainwindow_get_mainwindow()->progressbar), buf);
		gtk_progress_bar_set_fraction
			(GTK_PROGRESS_BAR(mainwindow_get_mainwindow()->progressbar),
			 (total == 0) ? 0 : (gfloat)done / (gfloat)total);
	} else if (total == 0) {
		gtk_progress_bar_set_text
			(GTK_PROGRESS_BAR(mainwindow_get_mainwindow()->progressbar), "");
		gtk_progress_bar_set_fraction
			(GTK_PROGRESS_BAR(mainwindow_get_mainwindow()->progressbar), 0.0);
	}
}
