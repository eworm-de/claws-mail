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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "defs.h"

#include <stdio.h>
#include <stdlib.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "intl.h"
#include "utils.h"
#include "prefs_common.h"
#include "prefs_gtk.h"

#include "gtk/gtkutils.h"
#include "gtk/prefswindow.h"

#include "manage_window.h"

typedef struct _FontsPage
{
	PrefsPage page;

	GtkWidget *window;		/* do not modify */

	GtkWidget *entry_folderviewfont;
	GtkWidget *entry_summaryviewfont;
	GtkWidget *entry_messageviewfont;
	GtkWidget *entry_boldfont;
} FontsPage;

static GtkWidget *font_sel_win;
static guint font_sel_conn_id;

static void prefs_font_select	(GtkButton *button, GtkEntry *entry);

static void prefs_font_selection_key_pressed	(GtkWidget	*widget,
						 GdkEventKey	*event,
						 gpointer	 data);
static void prefs_font_selection_ok		(GtkButton	*button, GtkEntry *entry);

static void prefs_font_select(GtkButton *button, GtkEntry *entry)
{
	gchar *font_name;
	
	g_return_if_fail(entry != NULL);
	
	if (!font_sel_win) {
		font_sel_win = gtk_font_selection_dialog_new
			(_("Font selection"));
		gtk_window_set_position(GTK_WINDOW(font_sel_win),
				    GTK_WIN_POS_CENTER);
		g_signal_connect(G_OBJECT(font_sel_win), "delete_event",
				 G_CALLBACK(gtk_widget_hide_on_delete),
				 NULL);
		g_signal_connect(G_OBJECT(font_sel_win), "key_press_event",
			 	 G_CALLBACK(prefs_font_selection_key_pressed),
			 	 NULL);
		g_signal_connect_swapped
			(G_OBJECT(GTK_FONT_SELECTION_DIALOG(font_sel_win)->cancel_button),
			 "clicked",
			 G_CALLBACK(gtk_widget_hide_on_delete),
			 G_OBJECT(font_sel_win));
	}

	if(font_sel_conn_id) {
		g_signal_handler_disconnect
			(G_OBJECT(GTK_FONT_SELECTION_DIALOG(font_sel_win)->ok_button), 
			 font_sel_conn_id);
	}
	font_sel_conn_id = g_signal_connect
		(G_OBJECT(GTK_FONT_SELECTION_DIALOG(font_sel_win)->ok_button),
	         "clicked", G_CALLBACK(prefs_font_selection_ok), entry);

	font_name = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);
	gtk_font_selection_dialog_set_font_name(GTK_FONT_SELECTION_DIALOG(font_sel_win), font_name);
	g_free(font_name);
	manage_window_set_transient(GTK_WINDOW(font_sel_win));
	gtk_window_set_modal(GTK_WINDOW(font_sel_win), TRUE);
	gtk_widget_grab_focus
		(GTK_FONT_SELECTION_DIALOG(font_sel_win)->ok_button);
	gtk_widget_show(font_sel_win);
}

static void prefs_font_selection_key_pressed(GtkWidget *widget,
					     GdkEventKey *event,
					     gpointer data)
{
	if (event && event->keyval == GDK_Escape)
		gtk_widget_hide(font_sel_win);
}

static void prefs_font_selection_ok(GtkButton *button, GtkEntry *entry)
{
	gchar *fontname;

	fontname = gtk_font_selection_dialog_get_font_name
		(GTK_FONT_SELECTION_DIALOG(font_sel_win));

	if (fontname) {
		gtk_entry_set_text(entry, fontname);

		g_free(fontname);
	}

	gtk_widget_hide(font_sel_win);
}

void prefs_fonts_create_widget(PrefsPage *_page, GtkWindow *window, 
			       gpointer data)
{
	FontsPage *prefs_fonts = (FontsPage *) _page;

	GtkWidget *table;
	GtkWidget *entry_folderviewfont;
	GtkWidget *entry_summaryviewfont;
	GtkWidget *entry_messageviewfont;
	GtkWidget *entry_boldfont;
	GtkWidget *tmplabel, *tmpbutton;
	GtkWidget *vbox;
	GtkWidget *hint_label;

	table = gtk_table_new(8, 3, FALSE);
	gtk_widget_show(table);
	gtk_container_set_border_width(GTK_CONTAINER(table), 8);
	gtk_table_set_row_spacings(GTK_TABLE(table), 4);
	gtk_table_set_col_spacings(GTK_TABLE(table), 8);

	tmplabel = gtk_label_new (_("Folder List"));
	gtk_widget_show (tmplabel);
	gtk_table_attach (GTK_TABLE (table), tmplabel, 0, 1, 0, 1,
			 (GtkAttachOptions) GTK_FILL,
			 (GtkAttachOptions) (0), 0, 0);
	gtk_label_set_justify(GTK_LABEL(tmplabel), GTK_JUSTIFY_RIGHT);
	gtk_misc_set_alignment(GTK_MISC(tmplabel), 1, 0.5);

	entry_folderviewfont = gtk_entry_new ();
	gtk_widget_show (entry_folderviewfont);
	gtk_table_attach (GTK_TABLE (table), entry_folderviewfont, 1, 2, 0, 1,
			 (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);
	gtk_entry_set_text(GTK_ENTRY(entry_folderviewfont), prefs_common.normalfont);

	tmpbutton = gtk_button_new_with_label (" ... ");
	gtk_widget_show (tmpbutton);
	gtk_table_attach (GTK_TABLE (table), tmpbutton, 2, 3, 0, 1,
			  0, 0, 0, 0);
	g_signal_connect(G_OBJECT(tmpbutton), "clicked",
			 G_CALLBACK(prefs_font_select), entry_folderviewfont);

	tmplabel = gtk_label_new (_("Message List"));
	gtk_widget_show (tmplabel);
	gtk_table_attach (GTK_TABLE (table), tmplabel, 0, 1, 1, 2,
			 (GtkAttachOptions) GTK_FILL,
			 (GtkAttachOptions) (0), 0, 0);
	gtk_label_set_justify(GTK_LABEL(tmplabel), GTK_JUSTIFY_RIGHT);
	gtk_misc_set_alignment(GTK_MISC(tmplabel), 1, 0.5);

	entry_summaryviewfont = gtk_entry_new ();
	gtk_widget_show (entry_summaryviewfont);
	gtk_table_attach (GTK_TABLE (table), entry_summaryviewfont, 1, 2, 1, 2,
			 (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);
	gtk_entry_set_text(GTK_ENTRY(entry_summaryviewfont), prefs_common.smallfont);

	tmpbutton = gtk_button_new_with_label (" ... ");
	gtk_widget_show (tmpbutton);
	gtk_table_attach (GTK_TABLE (table), tmpbutton, 2, 3, 1, 2,
			  0, 0, 0, 0);
	g_signal_connect(G_OBJECT(tmpbutton), "clicked",
			 G_CALLBACK(prefs_font_select), entry_summaryviewfont);

	tmplabel = gtk_label_new (_("Message"));
	gtk_widget_show (tmplabel);
	gtk_table_attach (GTK_TABLE (table), tmplabel, 0, 1, 2, 3,
			 (GtkAttachOptions) GTK_FILL,
			 (GtkAttachOptions) (0), 0, 0);
	gtk_label_set_justify(GTK_LABEL(tmplabel), GTK_JUSTIFY_RIGHT);
	gtk_misc_set_alignment(GTK_MISC(tmplabel), 1, 0.5);

	entry_messageviewfont = gtk_entry_new ();
	gtk_widget_show (entry_messageviewfont);
	gtk_table_attach (GTK_TABLE (table), entry_messageviewfont, 1, 2, 2, 3,
			 (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);
	gtk_entry_set_text(GTK_ENTRY(entry_messageviewfont), prefs_common.textfont);

	tmpbutton = gtk_button_new_with_label (" ... ");
	gtk_widget_show (tmpbutton);
	gtk_table_attach (GTK_TABLE (table), tmpbutton, 2, 3, 2, 3,
			  0, 0, 0, 0);
	g_signal_connect(G_OBJECT(tmpbutton), "clicked",
			 G_CALLBACK(prefs_font_select), entry_messageviewfont);

	tmplabel = gtk_label_new (_("Bold"));
	gtk_widget_show (tmplabel);
	gtk_table_attach (GTK_TABLE (table), tmplabel, 0, 1, 3, 4,
			 (GtkAttachOptions) GTK_FILL,
			 (GtkAttachOptions) (0), 0, 0);
	gtk_label_set_justify(GTK_LABEL(tmplabel), GTK_JUSTIFY_RIGHT);
	gtk_misc_set_alignment(GTK_MISC(tmplabel), 1, 0.5);

	entry_boldfont = gtk_entry_new ();
	gtk_widget_show (entry_boldfont);
	gtk_table_attach (GTK_TABLE (table), entry_boldfont, 1, 2, 3, 4,
			 (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);
	gtk_entry_set_text(GTK_ENTRY(entry_boldfont), prefs_common.boldfont);

	tmpbutton = gtk_button_new_with_label (" ... ");
	gtk_widget_show (tmpbutton);
	gtk_table_attach (GTK_TABLE (table), tmpbutton, 2, 3, 3, 4,
			  0, 0, 0, 0);
	g_signal_connect(G_OBJECT(tmpbutton), "clicked",
			 G_CALLBACK(prefs_font_select), entry_boldfont);

	vbox = gtk_vbox_new(FALSE, VSPACING_NARROW);
	gtk_widget_show(vbox);
	gtk_table_attach (GTK_TABLE (table), vbox, 0, 4, 4, 5,
			 (GtkAttachOptions) GTK_FILL,
			 (GtkAttachOptions) (0), 0, 0);
	
	hint_label = gtk_label_new (_("You will need to restart for the "
				      "changes to take effect"));
	gtk_widget_show (hint_label);
	gtk_box_pack_start (GTK_BOX (vbox), 
			    hint_label, FALSE, FALSE, 0);
	gtk_misc_set_alignment(GTK_MISC(hint_label), 0.5, 0.5);

	prefs_fonts->window		   = GTK_WIDGET(window);
	prefs_fonts->entry_folderviewfont  = entry_folderviewfont;
	prefs_fonts->entry_summaryviewfont = entry_summaryviewfont;
	prefs_fonts->entry_messageviewfont = entry_messageviewfont;
	prefs_fonts->entry_boldfont	   = entry_boldfont;

	prefs_fonts->page.widget = table;
}

void prefs_fonts_save(PrefsPage *_page)
{
	FontsPage *fonts = (FontsPage *) _page;

	prefs_common.normalfont = gtk_editable_get_chars
		(GTK_EDITABLE(fonts->entry_folderviewfont), 0, -1);
	prefs_common.smallfont  = gtk_editable_get_chars
		(GTK_EDITABLE(fonts->entry_summaryviewfont), 0, -1);
	prefs_common.textfont   = gtk_editable_get_chars
		(GTK_EDITABLE(fonts->entry_messageviewfont), 0, -1);
	prefs_common.boldfont   = gtk_editable_get_chars
		(GTK_EDITABLE(fonts->entry_boldfont), 0, -1);
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
	page->page.weight = 60.0;
	prefs_gtk_register_page((PrefsPage *) page);
	prefs_fonts = page;
}

void prefs_fonts_done(void)
{
	prefs_gtk_unregister_page((PrefsPage *) prefs_fonts);
	g_free(prefs_fonts);
}
