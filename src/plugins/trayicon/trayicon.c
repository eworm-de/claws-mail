/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2003 Hiroyuki Yamamoto and the Sylpheed-Claws Team
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

#include <stdio.h>

#include <glib.h>
#include <gtk/gtk.h>

#include "plugin.h"
#include "hooks.h"
#include "folder.h"
#include "mainwindow.h"
#include "gtkutils.h"

#include "eggtrayicon.h"
#include "newmail.xpm"
#include "unreadmail.xpm"
#include "nomail.xpm"

static gint hook_id;

static GdkPixmap *newmail_pixmap;
static GdkPixmap *newmail_bitmap;
static GdkPixmap *unreadmail_pixmap;
static GdkPixmap *unreadmail_bitmap;
static GdkPixmap *nomail_pixmap;
static GdkPixmap *nomail_bitmap;

static EggTrayIcon *trayicon;
static GtkWidget *eventbox;
static GtkWidget *image;
static GtkTooltips *tooltips;

typedef enum
{
	TRAYICON_NEW,
	TRAYICON_UNREAD,
	TRAYICON_UNREADMARKED,
	TRAYICON_NOTHING,
} TrayIconType;

static gboolean mainwin_hidden = FALSE;

static gboolean click_cb(GtkWidget * widget,
			 GdkEventButton * event, gpointer user_data)
{
/*
	MainWindow *mainwin;

	mainwin = mainwindow_get_mainwindow();
	if (mainwin_hidden) {
		gtk_widget_show(mainwin->window);
		mainwin_hidden = FALSE;
	} else {
		gtk_widget_hide(mainwin->window);
		mainwin_hidden = TRUE;
        }
*/
	return TRUE;
}

static void set_trayicon_pixmap(TrayIconType icontype)
{
	GdkPixmap *pixmap = NULL;
	GdkBitmap *bitmap = NULL;

	switch(icontype) {
	case TRAYICON_NEW:
		pixmap = newmail_pixmap;
		bitmap = newmail_bitmap;
		break;
	case TRAYICON_UNREAD:
	case TRAYICON_UNREADMARKED:
		pixmap = unreadmail_pixmap;
		bitmap = unreadmail_bitmap;
		break;
	default:
		pixmap = nomail_pixmap;
		bitmap = nomail_bitmap;
		break;
	}

	gtk_pixmap_set(GTK_PIXMAP(image), pixmap, bitmap);
	gtk_widget_shape_combine_mask(trayicon, bitmap, 0, 3);
}

static void update()
{
	gint new, unread, unreadmarked, total;
	gchar *buf;

	folder_count_total_msgs(&new, &unread, &unreadmarked, &total);
	buf = g_strdup_printf("New %d, Unread: %d, Total: %d", new, unread, total);

        gtk_tooltips_set_tip(tooltips, eventbox, buf, "");
	g_free(buf);

	set_trayicon_pixmap(new > 0 ? TRAYICON_NEW : (unread > 0 ? TRAYICON_UNREAD : TRAYICON_NOTHING));

	return FALSE;
}

static gboolean folder_item_update_hook(gpointer source, gpointer data)
{
	update();
}

int plugin_init(gchar **error)
{
	hook_id = hooks_register_hook (FOLDER_ITEM_UPDATE_HOOKLIST, folder_item_update_hook, NULL);
	if (hook_id == -1) {
		*error = g_strdup("Failed to register folder item update hook");
		return -1;
	}

        trayicon = egg_tray_icon_new("Sylpheed-Claws");
	gtk_widget_set_usize(GTK_WIDGET(trayicon), 16, 16);
        gtk_container_set_border_width(GTK_CONTAINER(trayicon), 0);

        PIXMAP_CREATE(GTK_WIDGET(trayicon), nomail_pixmap, nomail_bitmap, nomail_xpm);
        PIXMAP_CREATE(GTK_WIDGET(trayicon), unreadmail_pixmap, unreadmail_bitmap, unreadmail_xpm);
        PIXMAP_CREATE(GTK_WIDGET(trayicon), newmail_pixmap, newmail_bitmap, newmail_xpm);

	printf("%p\n", nomail_bitmap->user_data);
    
        eventbox = gtk_event_box_new();
        gtk_container_set_border_width(eventbox, 0);
        gtk_container_add(GTK_CONTAINER(trayicon), GTK_WIDGET(eventbox));

        image = gtk_pixmap_new (nomail_pixmap, nomail_bitmap);
        gtk_container_add(GTK_CONTAINER(eventbox), GTK_WIDGET(image));

        gtk_signal_connect(GTK_OBJECT(eventbox), "button-press-event", GTK_SIGNAL_FUNC(click_cb), NULL);

        tooltips = gtk_tooltips_new();
        gtk_tooltips_set_delay(tooltips, 1000);
        gtk_tooltips_enable(tooltips);

        gtk_widget_show_all(GTK_WIDGET(trayicon));

	update();

        return 0;
}

void plugin_done()
{
	gtk_widget_destroy(GTK_WIDGET(trayicon));
	hooks_unregister_hook(FOLDER_ITEM_UPDATE_HOOKLIST, hook_id);
}

const gchar *plugin_name()
{
	return "Trayicon";
}

const gchar *plugin_desc()
{
	return "";
}

const gchar *plugin_type()
{
	return "GTK";
}
