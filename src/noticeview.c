/* 
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2002 Hiroyuki Yamamoto & The Sylpheed Claws Team
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
#include <gtk/gtkwidget.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkpixmap.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#if HAVE_LIBCOMPFACE
#  include <compface.h>
#endif

#include "intl.h"
#include "main.h"
#include "headerview.h"
#include "prefs_common.h"
#include "gtkutils.h"
#include "utils.h"
#include "stock_pixmap.h"

#include "noticeview.h"

static void noticeview_button_pressed	(GtkButton *button, NoticeView *noticeview);

NoticeView *noticeview_create(MainWindow *mainwin)
{
	NoticeView *noticeview;
	GtkWidget  *vbox;
	GtkWidget  *hsep;
	GtkWidget  *hbox;
	GtkWidget  *icon;
	GtkWidget  *text;
	GtkWidget  *widget;

	debug_print("Creating notice view...\n");
	noticeview = g_new0(NoticeView, 1);

	vbox = gtk_vbox_new(FALSE, 4);
	gtk_widget_show(vbox);
	hsep = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), hsep, FALSE, TRUE, 0);
	
	hbox = gtk_hbox_new(FALSE, 4);
	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, TRUE, 0);

	icon = stock_pixmap_widget(mainwin->window, STOCK_PIXMAP_NOTICE_WARN); 
#if 0
	/* also possible... */
	icon = gtk_pixmap_new(NULL, NULL);
#endif
	gtk_widget_show(icon);
	
	gtk_box_pack_start(GTK_BOX(hbox), icon, FALSE, TRUE, 0);
	
	text = gtk_label_new("");
	gtk_widget_show(text);
	gtk_box_pack_start(GTK_BOX(hbox), text, FALSE, FALSE, 0);

	widget = gtk_button_new_with_label("");
	gtk_signal_connect(GTK_OBJECT(widget), "clicked", 
			   GTK_SIGNAL_FUNC(noticeview_button_pressed),
			   (gpointer) noticeview);
	gtk_widget_show(widget);
	gtk_box_pack_start(GTK_BOX(hbox), widget, FALSE, FALSE, 4);
	
	noticeview->vbox   = vbox;
	noticeview->hsep   = hsep;
	noticeview->hbox   = hbox;
	noticeview->icon   = icon;
	noticeview->text   = text;
	noticeview->button = widget;

	noticeview->visible = TRUE;

	return noticeview;
}

void noticeview_destroy(NoticeView *noticeview)
{
	g_free(noticeview);
}

gboolean noticeview_is_visible(NoticeView *noticeview)
{
	return noticeview->visible;
}

void noticeview_show(NoticeView *noticeview)
{
	if (!noticeview->visible) {
		gtk_widget_show_all(GTK_WIDGET_PTR(noticeview));
		noticeview->visible = TRUE;
	}	
}

void noticeview_hide(NoticeView *noticeview)
{
	if (noticeview->visible) {
		gtk_widget_hide(GTK_WIDGET_PTR(noticeview));
		noticeview->visible = FALSE;
	}	
}

void noticeview_set_text(NoticeView *noticeview, const char *text)
{
	g_return_if_fail(noticeview);
	gtk_label_set_text(GTK_LABEL(noticeview->text), text);
}

void noticeview_set_button_text(NoticeView *noticeview, const char *text)
{
	g_return_if_fail(noticeview);
	gtk_label_set_text
		(GTK_LABEL(GTK_BIN(noticeview->button)->child), text);
}

void noticeview_set_button_press_callback(NoticeView	*noticeview,
				          GtkSignalFunc  callback,
					  gpointer	*user_data)
{
	noticeview->press     = (void (*) (NoticeView *, gpointer)) callback;
	noticeview->user_data = user_data;
}

static void noticeview_button_pressed(GtkButton *button, NoticeView *noticeview)
{
	if (noticeview->press) {
		noticeview->press(noticeview, noticeview->user_data);
	}
}

