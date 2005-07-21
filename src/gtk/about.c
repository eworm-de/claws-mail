/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2005 Hiroyuki Yamamoto
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
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkhseparator.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtktext.h>
#include <gtk/gtkbutton.h>
#if HAVE_SYS_UTSNAME_H
#  include <sys/utsname.h>
#endif

#include "about.h"
#include "gtkutils.h"
#include "stock_pixmap.h"
#include "prefs_common.h"
#include "utils.h"
#include "version.h"

static GtkWidget *window;

static void about_create(void);
static gboolean key_pressed(GtkWidget *widget, GdkEventKey *event);
static void about_uri_clicked(GtkButton *button, gpointer data);

void about_show(void)
{
	if (!window)
		about_create();
	else
		gtk_window_present(GTK_WINDOW(window));
}

static void about_create(void)
{
	GtkWidget *vbox1;
	GtkWidget *table;
	GtkWidget *image;	
 	GtkWidget *vbox2;
	GtkWidget *label;
	GtkWidget *button;
	GtkWidget *scrolledwin;
	GtkStyle *style;
	GdkColormap *cmap;
	GdkColor uri_color[2] = {{0, 0, 0, 0xffff}, {0, 0xffff, 0, 0}};
	gboolean success[2];
	char *markup;
	GtkWidget *text;
	GtkWidget *confirm_area;
	GtkWidget *close_button;
	GtkTextBuffer *buffer;
	GtkTextIter iter;

#if HAVE_SYS_UTSNAME_H
	struct utsname utsbuf;
#endif
	gchar buf[1024];
	gint i;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), _("About"));
	gtk_container_set_border_width(GTK_CONTAINER(window), 8);
	gtk_widget_set_size_request(window, 518, 358);
	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(gtk_widget_hide_on_delete), NULL);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(key_pressed), NULL);
	gtk_widget_realize(window);

	vbox1 = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(window), vbox1);

	table = gtk_table_new (1, 2, FALSE);
	gtk_box_pack_start(GTK_BOX(vbox1), table, FALSE, FALSE, 0);

	image = stock_pixmap_widget(window, STOCK_PIXMAP_SYLPHEED_LOGO);
	gtk_table_attach(GTK_TABLE(table), image, 0, 1, 0, 1,
			 (GtkAttachOptions) (GTK_SHRINK),
			 (GtkAttachOptions) (GTK_SHRINK), 0, 0);

	vbox2 = gtk_vbox_new (TRUE, 0);
	gtk_table_attach(GTK_TABLE(table), vbox2, 1, 2, 0, 1,
			 (GtkAttachOptions) (GTK_EXPAND),
			 (GtkAttachOptions) (GTK_SHRINK), 0, 0);

	label = gtk_label_new("");
	gtk_label_set_selectable(GTK_LABEL(label), TRUE);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
	gtk_box_pack_start(GTK_BOX(vbox2), label, FALSE, FALSE, 0);
	markup = g_markup_printf_escaped
		("<span weight=\"bold\" size=\"x-large\">Sylpheed-Claws</span>\nversion %s",
		 VERSION);
	gtk_label_set_markup(GTK_LABEL(label), markup);
	g_free(markup);

	button = gtk_button_new_with_label(" "HOMEPAGE_URI" ");
	gtk_box_pack_start(GTK_BOX(vbox2), button, FALSE, FALSE, 0);
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(about_uri_clicked), NULL);
	buf[0] = ' ';
	for (i = 1; i <= strlen(HOMEPAGE_URI); i++) buf[i] = '_';
	strcpy(buf + i, " ");
	gtk_label_set_pattern(GTK_LABEL(GTK_BIN(button)->child), buf);
	cmap = gdk_window_get_colormap(window->window);
	gdk_colormap_alloc_colors(cmap, uri_color, 2, FALSE, TRUE, success);
	if (success[0] == TRUE && success[1] == TRUE) {
		gtk_widget_ensure_style(GTK_BIN(button)->child);
		style = gtk_style_copy
			(gtk_widget_get_style(GTK_BIN(button)->child));
		style->fg[GTK_STATE_NORMAL]   = uri_color[0];
		style->fg[GTK_STATE_ACTIVE]   = uri_color[1];
		style->fg[GTK_STATE_PRELIGHT] = uri_color[0];
		gtk_widget_set_style(GTK_BIN(button)->child, style);
	} else
		g_warning("about_create(): color allocation failed.\n");

#if HAVE_SYS_UTSNAME_H
	uname(&utsbuf);
	g_snprintf(buf, sizeof(buf),
		   _("GTK+ version %d.%d.%d\n"
		   "Operating System: %s %s (%s)"),
		   gtk_major_version, gtk_minor_version, gtk_micro_version,
		   utsbuf.sysname, utsbuf.release, utsbuf.machine);
#else
	g_snprintf(buf, sizeof(buf),
		   "GTK+ version %d.%d.%d\n"
		   "Operating System: Windoze",
		   gtk_major_version, gtk_minor_version, gtk_micro_version);
#endif

	label = gtk_label_new(buf);
	gtk_label_set_selectable(GTK_LABEL(label), TRUE);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_box_pack_start(GTK_BOX(vbox2), label, FALSE, FALSE, 0);

	g_snprintf(buf, sizeof(buf),
		   _("Compiled-in features:%s"),
#if USE_THREADS
		   " gthread"
#endif
#if INET6
		   " IPv6"
#endif
#if HAVE_ICONV
		   " iconv"
#endif
#if HAVE_LIBCOMPFACE
		   " compface"
#endif
#if USE_OPENSSL
		   " OpenSSL"
#endif
#if USE_LDAP
		   " LDAP"
#endif
#if USE_JPILOT
		   " JPilot"
#endif
#if USE_ASPELL
		   " GNU/aspell"
#endif
#if HAVE_LIBETPAN
		   " libetpan"
#endif
#if USE_GNOMEPRINT
		   " libgnomeprint"
#endif
	"");

	label = gtk_label_new(buf);
	gtk_label_set_selectable(GTK_LABEL(label), TRUE);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_box_pack_start(GTK_BOX(vbox2), label, FALSE, FALSE, 0);

	label = gtk_label_new
		("Copyright (C) 1999-2005 Hiroyuki Yamamoto <hiro-y@kcn.ne.jp>\n"
		 "and the Sylpheed-Claws team");
	gtk_label_set_selectable(GTK_LABEL(label), TRUE);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
	gtk_box_pack_start(GTK_BOX(vbox1), label, TRUE, TRUE, 0);

	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
				       GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledwin),
					    GTK_SHADOW_IN);
	gtk_box_pack_start(GTK_BOX(vbox1), scrolledwin, TRUE, TRUE, 0);

	text = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(text), FALSE);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text), GTK_WRAP_WORD);
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(text), 6);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(text), 6);
	gtk_container_add(GTK_CONTAINER(scrolledwin), text);

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text));
	gtk_text_buffer_get_iter_at_offset(buffer, &iter, 0);

	gtk_text_buffer_insert(buffer, &iter,
		_("This program is free software; you can redistribute it and/or modify "
		  "it under the terms of the GNU General Public License as published by "
		  "the Free Software Foundation; either version 2, or (at your option) "
		  "any later version.\n\n"), -1);

	gtk_text_buffer_insert(buffer, &iter,
		_("This program is distributed in the hope that it will be useful, "
		  "but WITHOUT ANY WARRANTY; without even the implied warranty of "
		  "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. "
		  "See the GNU General Public License for more details.\n\n"), -1);

	gtk_text_buffer_insert(buffer, &iter,
		_("You should have received a copy of the GNU General Public License "
		  "along with this program; if not, write to the Free Software "
		  "Foundation, Inc., 59 Temple Place - Suite 330, Boston, "
		  "MA 02111-1307, USA."), -1);
#ifdef USE_OPENSSL
	gtk_text_buffer_insert(buffer, &iter,
		_("\n\nThis product includes software developed by the OpenSSL Project "
		  "for use in the OpenSSL Toolkit (http://www.openssl.org/)"), -1);
#endif
	gtkut_stock_button_set_create(&confirm_area, &close_button, GTK_STOCK_CLOSE,
				      NULL, NULL, NULL, NULL);
	gtk_box_pack_end(GTK_BOX(vbox1), confirm_area, FALSE, FALSE, 4);
	gtk_widget_grab_default(close_button);
	g_signal_connect_closure
		(G_OBJECT(close_button), "clicked",
		 g_cclosure_new_swap(G_CALLBACK(gtk_widget_hide_on_delete),
				     window, NULL), FALSE);

	gtk_widget_show_all(window);
}

static gboolean key_pressed(GtkWidget *widget, GdkEventKey *event)
{
	if (event && event->keyval == GDK_Escape)
		gtk_widget_hide(window);
	return FALSE;
}

static void about_uri_clicked(GtkButton *button, gpointer data)
{
	open_uri(HOMEPAGE_URI, prefs_common.uri_cmd);
}
