/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2005-2022 the Claws Mail team
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
#include <gdk/gdkkeysyms.h>

#include "gtkutils.h"
#include "stock_pixmap.h"
#include "prefs_gtk.h"

#define ICONS 23
#define ROWS ((ICONS % 2 == 0)? ICONS / 2: (ICONS + 1) / 2)

StockPixmap legend_icons[ICONS] = {
	STOCK_PIXMAP_NEW,
	STOCK_PIXMAP_UNREAD,
	STOCK_PIXMAP_REPLIED, 
	STOCK_PIXMAP_FORWARDED, 
	STOCK_PIXMAP_REPLIED_AND_FORWARDED, 
	STOCK_PIXMAP_IGNORETHREAD,
	STOCK_PIXMAP_WATCHTHREAD,
	STOCK_PIXMAP_SPAM,
	STOCK_PIXMAP_CLIP,
	STOCK_PIXMAP_GPG_SIGNED,
	STOCK_PIXMAP_KEY,
	STOCK_PIXMAP_CLIP_GPG_SIGNED,
	STOCK_PIXMAP_CLIP_KEY,
	STOCK_PIXMAP_MARK,
	STOCK_PIXMAP_DELETED,
	STOCK_PIXMAP_MOVED,
	STOCK_PIXMAP_COPIED,
	STOCK_PIXMAP_LOCKED,
	STOCK_PIXMAP_DIR_OPEN, 
	STOCK_PIXMAP_DIR_OPEN_HRM,
	STOCK_PIXMAP_DIR_OPEN_MARK,
	STOCK_PIXMAP_DIR_NOSELECT_OPEN,
	STOCK_PIXMAP_DIR_SUBS_OPEN,
};

static gchar *legend_icon_desc[] = {
/* status column */
	N_("New message"),
	N_("Unread message"),
	N_("Message has been replied to"),
	N_("Message has been forwarded"),
	N_("Message has been forwarded and replied to"),
	N_("Message is in an ignored thread"),
	N_("Message is in a watched thread"),
	N_("Message is spam"),
/* attachment column */
	N_("Message has attachment(s)"),
	N_("Digitally signed message"),
	N_("Encrypted message"),
	N_("Message is signed and has attachment(s)"),
	N_("Message is encrypted and has attachment(s)"),
/* mark column */
	N_("Marked message"),
	N_("Message is marked for deletion"),
	N_("Message is marked for moving"),
	N_("Message is marked for copying"),
/* locked column */
	N_("Locked message"),
/* others */
	N_("Folder (normal, opened)"),
	N_("Folder with read messages hidden"),
	N_("Folder contains marked messages"),
	N_("IMAP folder which contains subfolders only"),
	N_("IMAP mailbox showing only subscribed folders"),
};

static struct LegendDialog {
	GtkWidget *window;
} legend;

static void legend_create(void);
static gboolean key_pressed(GtkWidget *widget, GdkEventKey *event);
static void legend_close(void);

void legend_show(void)
{
	if (!legend.window)
		legend_create();
	else
		gtk_window_present(GTK_WINDOW(legend.window));
}

static void legend_create(void)
{
	GtkWidget *window;
	GtkWidget *scrolledwindow;
	GtkWidget *vbox;
	GtkWidget *confirm_area;
	GtkWidget *close_button;
	GtkWidget *hbox;
	GtkWidget *label;
	GtkWidget *icon_label;
	GtkWidget *desc_label;
	GtkWidget *table;
	gint i, j, k;

	window = gtkut_window_new(GTK_WINDOW_TOPLEVEL, "icon_legend");
	gtk_window_set_title(GTK_WINDOW(window), _("Icon Legend"));
	gtk_container_set_border_width(GTK_CONTAINER(window), 8);
	gtk_window_set_resizable(GTK_WINDOW(window), TRUE);
	gtk_window_set_type_hint(GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_DIALOG);
	gtk_window_set_default_size(GTK_WINDOW(window), 666, 340);
	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(legend_close), NULL);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(key_pressed), NULL);
	gtk_widget_realize(window);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
	gtk_container_add(GTK_CONTAINER(window), vbox);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 2);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	label = gtk_label_new(g_strconcat("<span weight=\"bold\">",_("The following icons "
				"are used to show the status of messages and "
				"folders:"), "</span>", NULL));
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);

	scrolledwindow = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy
		(GTK_SCROLLED_WINDOW(scrolledwindow),
		 GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_propagate_natural_height(GTK_SCROLLED_WINDOW(scrolledwindow), TRUE);
	gtk_widget_show(scrolledwindow);
	gtk_box_pack_start(GTK_BOX(vbox), scrolledwindow, TRUE, TRUE, 0);

	table = gtk_grid_new();
	gtk_container_set_border_width(GTK_CONTAINER(table), 8);
	gtk_grid_set_row_spacing(GTK_GRID(table), 4);
	gtk_grid_set_column_spacing(GTK_GRID(table), 8);

	for (i = 0, j = 0, k = 0; i < ICONS; ++i, ++k) {
		icon_label = stock_pixmap_widget(legend_icons[i]);
		gtk_widget_set_halign(icon_label, GTK_ALIGN_CENTER);
		gtk_widget_set_valign(icon_label, GTK_ALIGN_CENTER);
		gtk_grid_attach(GTK_GRID(table), icon_label, j, k, 1, 1);

		desc_label = gtk_label_new(gettext(legend_icon_desc[i]));
		gtk_label_set_xalign(GTK_LABEL(desc_label), 0.0);
		gtk_label_set_line_wrap(GTK_LABEL(desc_label), TRUE);
		gtk_grid_attach(GTK_GRID(table), desc_label, j+1, k, 1, 1);

		if (i == ICONS / 2) {
			j = 2;
			k = -1;
		}
	}

	gtk_container_add(GTK_CONTAINER(scrolledwindow), GTK_WIDGET(table));

	gtkut_stock_button_set_create(&confirm_area, &close_button, "window-close", _("_Close"),
				      NULL, NULL, NULL, NULL, NULL, NULL);
	gtk_box_pack_end(GTK_BOX(vbox), confirm_area, FALSE, FALSE, 4);
	gtk_widget_grab_default(close_button);
	g_signal_connect_closure
		(G_OBJECT(close_button), "clicked",
		 g_cclosure_new_swap(G_CALLBACK(legend_close),
				     window, NULL), FALSE);

	gtk_widget_show_all(window);

	legend.window = window;
}

static gboolean key_pressed(GtkWidget *widget, GdkEventKey *event)
{
	if (event && event->keyval == GDK_KEY_Escape) {
		legend_close();
	}
	return FALSE;
}

static void legend_close(void)
{
	gtk_widget_destroy(legend.window);
	legend.window = NULL;
}
