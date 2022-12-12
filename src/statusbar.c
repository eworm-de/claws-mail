/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2022 the Claws Mail team and Hiroyuki Yamamoto
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

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <stdarg.h>

#include "mainwindow.h"
#include "statusbar.h"
#include "gtkutils.h"
#include "utils.h"
#include "log.h"
#include "hooks.h"

#define BUFFSIZE 1024

static GList *statusbar_list = NULL;
static gulong statusbar_puts_all_hook_id = HOOK_NONE;

GtkWidget *statusbar_create(void)
{
	GtkWidget *statusbar;
	GtkWidget *child;
	GtkWidget *parent;
	GtkWidget *hbox;

	statusbar = gtk_statusbar_new();
	
	gtk_widget_set_margin_top(GTK_WIDGET(statusbar), 0);
	gtk_widget_set_margin_bottom(GTK_WIDGET(statusbar), 0);
	statusbar_list = g_list_append(statusbar_list, statusbar);
	gtk_container_set_border_width(GTK_CONTAINER(statusbar), 1);
	child = gtk_statusbar_get_message_area(GTK_STATUSBAR(statusbar));
	gtk_widget_set_margin_top(GTK_WIDGET(child), 0);
	gtk_widget_set_margin_bottom(GTK_WIDGET(child), 0);
	gtk_widget_set_margin_start(GTK_WIDGET(child), 0);
	gtk_widget_set_margin_end(GTK_WIDGET(child), 0);
	parent = gtk_widget_get_parent(child);
	gtk_container_remove(GTK_CONTAINER(parent), g_object_ref(child));
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_container_add(GTK_CONTAINER(parent), hbox);
	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(hbox), child, TRUE, TRUE, 0);
	g_object_unref(child);	

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

	g_free(buf);
}

void statusbar_puts_all(const gchar *str)
{
	GList *cur;

	for (cur = statusbar_list; cur != NULL; cur = cur->next)
		statusbar_puts(GTK_STATUSBAR(cur->data), str);
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

static gboolean statusbar_puts_all_hook (gpointer source, gpointer data)
{
	LogText *logtext = (LogText *) source;

	cm_return_val_if_fail(logtext != NULL, TRUE);
	cm_return_val_if_fail(logtext->text != NULL, TRUE);

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
	if (verbose && (statusbar_puts_all_hook_id == HOOK_NONE)) {
		statusbar_puts_all_hook_id =
			hooks_register_hook(LOG_APPEND_TEXT_HOOKLIST, statusbar_puts_all_hook, NULL);
	} else if (!verbose && (statusbar_puts_all_hook_id != HOOK_NONE)) {
		hooks_unregister_hook(LOG_APPEND_TEXT_HOOKLIST, statusbar_puts_all_hook_id);
		statusbar_puts_all_hook_id = HOOK_NONE;
		statusbar_pop_all();
	}
}

void statusbar_progress_all (gint done, gint total, gint step) 
{
	GtkProgressBar *progressbar = GTK_PROGRESS_BAR(
					mainwindow_get_mainwindow()->progressbar);
	gchar buf[32];
	
	if (total && done % step == 0) {
#ifdef GENERIC_UMPC
		/* use a more compact format */
		const gchar *format = "%d/%d";
#else
		const gchar *format = "%d / %d";
#endif
		g_snprintf(buf, sizeof(buf), format, done, total);
		gtk_progress_bar_set_show_text(progressbar, TRUE);
		gtk_progress_bar_set_text(progressbar, buf);
		gtk_progress_bar_set_fraction(progressbar,
			 (gfloat)done / (gfloat)total);
		if (!gtk_widget_get_visible(GTK_WIDGET(progressbar)))
			gtk_widget_show(GTK_WIDGET(progressbar));
	} else if (total == 0) {
		gtk_progress_bar_set_text(progressbar, "");
		gtk_progress_bar_set_fraction(progressbar, 0.0);
		gtk_widget_hide(GTK_WIDGET(progressbar));
	}
}
