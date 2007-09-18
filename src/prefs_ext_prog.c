/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2004 Hiroyuki Yamamoto & the Claws Mail team
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
 * 
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

#include "utils.h"
#include "prefs_common.h"
#include "prefs_gtk.h"

#include "gtk/gtkutils.h"
#include "gtk/prefswindow.h"

#include "manage_window.h"

typedef struct _ExtProgPage
{
	PrefsPage page;

	GtkWidget *window;		/* do not modify */

	GtkWidget *uri_label;
	GtkWidget *uri_combo;
	GtkWidget *uri_entry;
	
	GtkWidget *printcmd_label;
	GtkWidget *printcmd_entry;
	
	GtkWidget *exteditor_label;
	GtkWidget *exteditor_combo;
	GtkWidget *exteditor_entry;

	GtkWidget *astextviewer_label;
	GtkWidget *astextviewer_entry;
} ExtProgPage;

static void prefs_ext_prog_create_widget(PrefsPage *_page, GtkWindow *window, 
			          gpointer data)
{
	ExtProgPage *prefs_ext_prog = (ExtProgPage *) _page;

	GtkWidget *table;
	GtkWidget *vbox;
	GtkWidget *hint_label;
	GtkWidget *table2;
	GtkWidget *uri_label;
	GtkWidget *uri_combo;
	GtkWidget *uri_entry;
	GtkWidget *exteditor_label;
	GtkWidget *exteditor_combo;
	GtkWidget *exteditor_entry;
	GtkWidget *astextviewer_label;
	GtkWidget *astextviewer_entry;
	GtkWidget *printcmd_label;
	GtkWidget *printcmd_entry;
	GtkTooltips *tooltip;

	tooltip = gtk_tooltips_new();

	table = gtk_table_new(2, 1, FALSE);
	gtk_widget_show(table);
	gtk_container_set_border_width(GTK_CONTAINER(table), 8);
	gtk_table_set_row_spacings(GTK_TABLE(table), 4);
	gtk_table_set_col_spacings(GTK_TABLE(table), 8);

 	vbox = gtk_vbox_new(TRUE, 0);
	gtk_widget_show(vbox);
	
	gtk_table_attach(GTK_TABLE (table), vbox, 0, 1, 0, 1,
                    	 (GtkAttachOptions) (GTK_SHRINK),
                    	 (GtkAttachOptions) (0), 0, 0);

	hint_label = gtk_label_new(_("%s will be replaced with file name / URI"));
	gtk_label_set_justify (GTK_LABEL (hint_label), GTK_JUSTIFY_LEFT);
	gtkut_widget_set_small_font_size (hint_label);
	gtk_widget_show(hint_label);
	gtk_box_pack_start(GTK_BOX (vbox),
			   hint_label, FALSE, FALSE, 4);

	table2 = gtk_table_new(6, 2, FALSE);
	gtk_widget_show(table2);
	gtk_container_set_border_width(GTK_CONTAINER(table2), 8);
	gtk_table_set_row_spacings(GTK_TABLE(table2), 4);
	gtk_table_set_col_spacings(GTK_TABLE(table2), 8);
	
	gtk_table_attach(GTK_TABLE (table), table2, 0, 1, 1, 2,
                    	 (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    	 (GtkAttachOptions) (0), 0, 0);
			 
	uri_label = gtk_label_new (_("Web browser"));
	gtk_widget_show(uri_label);
#ifdef MAEMO
	gtk_widget_set_sensitive(uri_label, FALSE);
#endif
	gtk_table_attach(GTK_TABLE (table2), uri_label, 0, 1, 0, 1,
                    	 (GtkAttachOptions) (GTK_FILL),
                    	 (GtkAttachOptions) (0), 0, 2);
	gtk_label_set_justify(GTK_LABEL (uri_label), GTK_JUSTIFY_RIGHT);
	gtk_misc_set_alignment(GTK_MISC (uri_label), 1, 0.5);

	uri_combo = gtk_combo_new ();
	gtk_widget_show (uri_combo);
#ifdef MAEMO
	gtk_widget_set_sensitive(uri_combo, FALSE);
#endif
	gtk_table_attach (GTK_TABLE (table2), uri_combo, 1, 2, 0, 1,
			  GTK_EXPAND | GTK_FILL, 0, 0, 0);
	gtkut_combo_set_items (GTK_COMBO (uri_combo),
			       DEFAULT_BROWSER_CMD,
			       "galeon --new-tab '%s'",
			       "galeon '%s'",
			       "mozilla -remote 'openurl(%s,new-window)'",
			       "netscape -remote 'openURL(%s, new-window)'",
			       "netscape '%s'",
			       "gnome-moz-remote --newwin '%s'",
			       "kfmclient openURL '%s'",
			       "opera -newwindow '%s'",
			       "rxvt -e w3m '%s'",
			       "rxvt -e lynx '%s'",
			       NULL);
	uri_entry = GTK_COMBO (uri_combo)->entry;
	gtk_entry_set_text(GTK_ENTRY(uri_entry), prefs_common.uri_cmd ? prefs_common.uri_cmd : "");
	
	exteditor_label = gtk_label_new (_("Text editor"));
	gtk_widget_show(exteditor_label);

	gtk_table_attach(GTK_TABLE (table2), exteditor_label, 0, 1, 1, 2,
                    	 (GtkAttachOptions) (GTK_FILL),
                    	 (GtkAttachOptions) (0), 0, 2);
	gtk_label_set_justify(GTK_LABEL (exteditor_label), GTK_JUSTIFY_RIGHT);
	gtk_misc_set_alignment(GTK_MISC (exteditor_label), 1, 0.5);

	exteditor_combo = gtk_combo_new ();
	gtk_widget_show (exteditor_combo);
	gtk_table_attach (GTK_TABLE (table2), exteditor_combo, 1, 2, 1, 2,
			  GTK_EXPAND | GTK_FILL, 0, 0, 0);
	gtkut_combo_set_items (GTK_COMBO (exteditor_combo),
			       "gedit %s",
			       "kedit %s",
			       "mousepad %s",
			       "nedit %s",
			       "mgedit --no-fork %s",
			       "emacs %s",
			       "xemacs %s",
			       "kterm -e jed %s",
			       "kterm -e vi %s",
			       NULL);
	exteditor_entry = GTK_COMBO (exteditor_combo)->entry;
	gtk_entry_set_text(GTK_ENTRY(exteditor_entry), 
			   prefs_common.ext_editor_cmd ? prefs_common.ext_editor_cmd : "");

	astextviewer_label = gtk_label_new(_("Command for 'Display as text'"));
	gtk_widget_show(astextviewer_label);

	gtk_table_attach(GTK_TABLE (table2), astextviewer_label, 0, 1, 2, 3,
                    	 (GtkAttachOptions) (GTK_FILL),
                    	 (GtkAttachOptions) (0), 0, 2);
	gtk_label_set_justify(GTK_LABEL (astextviewer_label), GTK_JUSTIFY_RIGHT);
	gtk_misc_set_alignment(GTK_MISC (astextviewer_label), 1, 0.5);

	astextviewer_entry = gtk_entry_new ();
	gtk_widget_show(astextviewer_entry);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(tooltip), astextviewer_entry,
			     _("This option enables MIME parts to be displayed in the "
 			       "message view via a script when using the 'Display as text' "
			       "contextual menu item"),
			     NULL);
	
	gtk_table_attach(GTK_TABLE (table2), astextviewer_entry, 1, 2, 2, 3,
                    	 (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    	 (GtkAttachOptions) (0), 0, 0);
	gtk_entry_set_text(GTK_ENTRY(astextviewer_entry), 
			   prefs_common.mime_textviewer ? prefs_common.mime_textviewer : "");

	printcmd_label = gtk_label_new (_("Print command"));
#if !defined(USE_GNOMEPRINT) && !GTK_CHECK_VERSION(2,10,0)
	gtk_widget_show(printcmd_label);
#endif
	gtk_table_attach(GTK_TABLE (table2), printcmd_label, 0, 1, 3, 4,
                    	 (GtkAttachOptions) (GTK_FILL),
                    	 (GtkAttachOptions) (0), 0, 2);
	gtk_label_set_justify(GTK_LABEL (printcmd_label), GTK_JUSTIFY_RIGHT);
	gtk_misc_set_alignment(GTK_MISC (printcmd_label), 1, 0.5);

	printcmd_entry = gtk_entry_new ();
#if !defined(USE_GNOMEPRINT) && !GTK_CHECK_VERSION(2,10,0)
	gtk_widget_show(printcmd_entry);
#endif
	gtk_table_attach(GTK_TABLE (table2), printcmd_entry, 1, 2, 3, 4,
                    	 (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
                    	 (GtkAttachOptions) (0), 0, 0);
	gtk_entry_set_text(GTK_ENTRY(printcmd_entry), prefs_common.print_cmd ? prefs_common.print_cmd : "");


	prefs_ext_prog->window			= GTK_WIDGET(window);
	prefs_ext_prog->uri_entry		= uri_entry;
	prefs_ext_prog->exteditor_entry		= exteditor_entry;
	prefs_ext_prog->astextviewer_entry	= astextviewer_entry;
	prefs_ext_prog->printcmd_entry		= printcmd_entry;

	prefs_ext_prog->page.widget = table;
}

static void prefs_ext_prog_save(PrefsPage *_page)
{
	ExtProgPage *ext_prog = (ExtProgPage *) _page;

	prefs_common.uri_cmd = gtk_editable_get_chars
		(GTK_EDITABLE(ext_prog->uri_entry), 0, -1);
	prefs_common.print_cmd = gtk_editable_get_chars
		(GTK_EDITABLE(ext_prog->printcmd_entry), 0, -1);
	prefs_common.ext_editor_cmd = gtk_editable_get_chars
		(GTK_EDITABLE(ext_prog->exteditor_entry), 0, -1);
	prefs_common.mime_textviewer = gtk_editable_get_chars
		(GTK_EDITABLE(ext_prog->astextviewer_entry), 0, -1);
}

static void prefs_ext_prog_destroy_widget(PrefsPage *_page)
{
	/* ExtProgPage *ext_prog = (ExtProgPage *) _page; */

}

ExtProgPage *prefs_ext_prog;

void prefs_ext_prog_init(void)
{
	ExtProgPage *page;
	static gchar *path[3];
	
	path[0] = _("Message View");
	path[1] = _("External Programs");
	path[2] = NULL;

	page = g_new0(ExtProgPage, 1);
	page->page.path = path;
	page->page.create_widget = prefs_ext_prog_create_widget;
	page->page.destroy_widget = prefs_ext_prog_destroy_widget;
	page->page.save_page = prefs_ext_prog_save;
	page->page.weight = 155.0;
	prefs_gtk_register_page((PrefsPage *) page);
	prefs_ext_prog = page;
}

void prefs_ext_prog_done(void)
{
	prefs_gtk_unregister_page((PrefsPage *) prefs_ext_prog);
	g_free(prefs_ext_prog);
}
