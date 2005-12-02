/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2003-2004 Hiroyuki Yamamoto & The Sylpheed-Claws Team
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

#include "defs.h"

#include <stdio.h>
#include <stdlib.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "prefs_common.h"
#include "prefs_gtk.h"

#include "gtk/prefswindow.h"

typedef struct _FontsPage
{
	PrefsPage page;

	GtkWidget *window;		/* do not modify */

	GtkWidget *entry_folderviewfont;
	GtkWidget *entry_messageviewfont;
} FontsPage;

void prefs_fonts_create_widget(PrefsPage *_page, GtkWindow *window, 
			       gpointer data)
{
	FontsPage *prefs_fonts = (FontsPage *) _page;

	GtkWidget *table;
	GtkWidget *entry_folderviewfont;
	GtkWidget *entry_messageviewfont;
	GtkWidget *tmplabel;
	GtkWidget *vbox;

	table = gtk_table_new(7, 2, FALSE);
	gtk_widget_show(table);
	gtk_container_set_border_width(GTK_CONTAINER(table), VBOX_BORDER);
	gtk_table_set_row_spacings(GTK_TABLE(table), 4);
	gtk_table_set_col_spacings(GTK_TABLE(table), 8);

	tmplabel = gtk_label_new (_("Folder and Message Lists"));
	gtk_widget_show (tmplabel);
	gtk_table_attach (GTK_TABLE (table), tmplabel, 0, 1, 0, 1,
			 (GtkAttachOptions) GTK_FILL,
			 (GtkAttachOptions) (0), 0, 0);
	gtk_label_set_justify(GTK_LABEL(tmplabel), GTK_JUSTIFY_RIGHT);
	gtk_misc_set_alignment(GTK_MISC(tmplabel), 1, 0.5);

	entry_folderviewfont = gtk_font_button_new_with_font (prefs_common.normalfont);
	g_object_set(G_OBJECT(entry_folderviewfont), 
			      "use-font", TRUE, 
			      NULL);
	gtk_widget_show (entry_folderviewfont);
	gtk_table_attach (GTK_TABLE (table), entry_folderviewfont, 1, 2, 0, 1,
			 (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);

	tmplabel = gtk_label_new (_("Message"));
	gtk_widget_show (tmplabel);
	gtk_table_attach (GTK_TABLE (table), tmplabel, 0, 1, 2, 3,
			 (GtkAttachOptions) GTK_FILL,
			 (GtkAttachOptions) (0), 0, 0);
	gtk_label_set_justify(GTK_LABEL(tmplabel), GTK_JUSTIFY_RIGHT);
	gtk_misc_set_alignment(GTK_MISC(tmplabel), 1, 0.5);

	entry_messageviewfont = gtk_font_button_new_with_font (prefs_common.textfont);
	g_object_set(G_OBJECT(entry_messageviewfont), 
			      "use-font", TRUE, 
			      NULL);
	gtk_widget_show (entry_messageviewfont);
	gtk_table_attach (GTK_TABLE (table), entry_messageviewfont, 1, 2, 2, 3,
			 (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);

	vbox = gtk_vbox_new(FALSE, VSPACING_NARROW);
	gtk_widget_show(vbox);
	gtk_table_attach (GTK_TABLE (table), vbox, 0, 4, 4, 5,
			 (GtkAttachOptions) GTK_FILL,
			 (GtkAttachOptions) (0), 0, 0);
	
	prefs_fonts->window		   = GTK_WIDGET(window);
	prefs_fonts->entry_folderviewfont  = entry_folderviewfont;
	prefs_fonts->entry_messageviewfont = entry_messageviewfont;

	prefs_fonts->page.widget = table;
}

void prefs_fonts_save(PrefsPage *_page)
{
	FontsPage *fonts = (FontsPage *) _page;

	g_free(prefs_common.normalfont);
	prefs_common.normalfont = g_strdup(gtk_font_button_get_font_name
		(GTK_FONT_BUTTON(fonts->entry_folderviewfont)));
		
	g_free(prefs_common.smallfont);		
	prefs_common.smallfont  = g_strdup(gtk_font_button_get_font_name
		(GTK_FONT_BUTTON(fonts->entry_folderviewfont)));

	g_free(prefs_common.textfont);		
	prefs_common.textfont   = g_strdup(gtk_font_button_get_font_name
		(GTK_FONT_BUTTON(fonts->entry_messageviewfont)));

	main_window_reflect_prefs_all();
}

static void prefs_fonts_destroy_widget(PrefsPage *_page)
{
	/* FontsPage *fonts = (FontsPage *) _page; */

}

FontsPage *prefs_fonts;

void prefs_fonts_init(void)
{
	FontsPage *page;
	static gchar *path[3];

	path[0] = _("Display");
	path[1] = _("Fonts");
	path[2] = NULL;

	page = g_new0(FontsPage, 1);
	page->page.path = path;
	page->page.create_widget = prefs_fonts_create_widget;
	page->page.destroy_widget = prefs_fonts_destroy_widget;
	page->page.save_page = prefs_fonts_save;
	page->page.weight = 135.0;
	prefs_gtk_register_page((PrefsPage *) page);
	prefs_fonts = page;
}

void prefs_fonts_done(void)
{
	prefs_gtk_unregister_page((PrefsPage *) prefs_fonts);
	g_free(prefs_fonts);
}
