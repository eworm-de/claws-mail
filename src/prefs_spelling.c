/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2002 Hiroyuki Yamamoto & the Sylpheed-Claws team
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

/*
 * General functions for accessing address book files.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#if USE_ASPELL

#include "defs.h"

#include <stdio.h>
#include <stdlib.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "utils.h"
#include "prefs_common.h"
#include "prefs_gtk.h"

#include "gtk/gtkutils.h"
#include "gtk/prefswindow.h"
#include "gtk/filesel.h"
#include "gtk/colorsel.h"

typedef struct _SpellingPage
{
	PrefsPage page;

	GtkWidget *window;		/* do not modify */

	GtkWidget *checkbtn_enable_aspell;
	GtkWidget *entry_aspell_path;
	GtkWidget *btn_aspell_path;
	GtkWidget *optmenu_dictionary;
	GtkWidget *optmenu_sugmode;
	GtkWidget *misspelled_btn;
	GtkWidget *checkbtn_use_alternate;
	GtkWidget *checkbtn_check_while_typing;

	gint	   misspell_col;
} SpellingPage;

static void prefs_spelling_enable(SpellingPage *spelling, gboolean enable)
{
	gtk_widget_set_sensitive(spelling->entry_aspell_path,   	enable);
	gtk_widget_set_sensitive(spelling->optmenu_dictionary,  	enable);
	gtk_widget_set_sensitive(spelling->optmenu_sugmode,     	enable);
	gtk_widget_set_sensitive(spelling->btn_aspell_path,     	enable);
	gtk_widget_set_sensitive(spelling->misspelled_btn,      	enable);
	gtk_widget_set_sensitive(spelling->checkbtn_use_alternate,      enable);
	gtk_widget_set_sensitive(spelling->checkbtn_check_while_typing, enable);
}

static void prefs_spelling_checkbtn_enable_aspell_toggle_cb
	(GtkWidget *widget,
	 gpointer data)
{
	SpellingPage *spelling = (SpellingPage *) data;
	gboolean toggled;

	toggled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
	prefs_spelling_enable(spelling, toggled);
}

static void prefs_spelling_btn_aspell_path_clicked_cb(GtkWidget *widget,
						     gpointer data)
{
	SpellingPage *spelling = (SpellingPage *) data;
	gchar *file_path;
	GtkWidget *new_menu;

	file_path = filesel_select_file_open(_("Select dictionaries location"),
					prefs_common.aspell_path);
	if (file_path != NULL) {
		gchar *tmp_path, *tmp;

		tmp_path = g_path_get_dirname(file_path);
		tmp = g_strdup_printf("%s%s", tmp_path, G_DIR_SEPARATOR_S);
		g_free(tmp_path);

		new_menu = gtkaspell_dictionary_option_menu_new(tmp);
		gtk_option_menu_set_menu(GTK_OPTION_MENU(spelling->optmenu_dictionary),
					 new_menu);

		gtk_entry_set_text(GTK_ENTRY(spelling->entry_aspell_path), tmp);
		/* select first one */
		gtk_option_menu_set_history(GTK_OPTION_MENU(
					spelling->optmenu_dictionary), 0);
	
		g_free(tmp);

	}
}

static void prefs_spelling_colorsel(GtkWidget *widget,
				    gpointer data)
{
	SpellingPage *spelling = (SpellingPage *) data;
	gint rgbcolor;

	rgbcolor = colorsel_select_color_rgb(_("Pick color for misspelled word"), 
					     spelling->misspell_col);
	gtkut_set_widget_bgcolor_rgb(spelling->misspelled_btn, rgbcolor);
	spelling->misspell_col = rgbcolor;
}

#define SAFE_STRING(str) \
	(str) ? (str) : ""

void prefs_spelling_create_widget(PrefsPage *_page, GtkWindow *window, gpointer data)
{
	SpellingPage *prefs_spelling = (SpellingPage *) _page;

	/* START GLADE CODE */
	GtkWidget *table;
	GtkWidget *checkbtn_enable_aspell;
	GtkWidget *checkbtn_check_while_typing;
	GtkWidget *checkbtn_use_alternate;
	GtkWidget *label2;
	GtkWidget *entry_aspell_path;
	GtkWidget *label3;
	GtkWidget *optmenu_dictionary;
	GtkWidget *optmenu_dictionary_menu;
	GtkWidget *label4;
	GtkWidget *optmenu_sugmode;
	GtkWidget *optmenu_sugmode_menu;
	GtkWidget *label5;
	GtkWidget *btn_aspell_path;
	GtkWidget *hbox1;
	GtkWidget *misspelled_btn;
	GtkTooltips *tooltips;
	PangoFontDescription *font_desc;
	gint size;

	tooltips = gtk_tooltips_new ();

	table = gtk_table_new(8, 3, FALSE);
	gtk_widget_show(table);
	gtk_container_set_border_width(GTK_CONTAINER(table), 8);
	gtk_table_set_row_spacings(GTK_TABLE(table), 4);
	gtk_table_set_col_spacings(GTK_TABLE(table), 8);

	checkbtn_enable_aspell =
	    gtk_check_button_new_with_label(_("Enable spell checker"));
	gtk_widget_show(checkbtn_enable_aspell);
	gtk_table_attach(GTK_TABLE(table), checkbtn_enable_aspell, 0, 3, 0,
			 1, (GtkAttachOptions) (GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);

	checkbtn_check_while_typing =
	    gtk_check_button_new_with_label(_("Check while typing"));
	gtk_widget_show(checkbtn_check_while_typing);
	gtk_table_attach(GTK_TABLE(table), checkbtn_check_while_typing, 0,
			 3, 1, 2, (GtkAttachOptions) (GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);

	checkbtn_use_alternate =
	    gtk_check_button_new_with_label(_
					    ("Enable alternate dictionary"));
	gtk_widget_show(checkbtn_use_alternate);
	gtk_table_attach(GTK_TABLE(table), checkbtn_use_alternate, 0, 3, 2,
			 3, (GtkAttachOptions) (GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);
	gtk_tooltips_set_tip (tooltips, checkbtn_use_alternate, 
			_("Faster switching with last used dictionary"), NULL);

	label2 = gtk_label_new(_("Dictionaries path:"));
	gtk_widget_show(label2);
	gtk_table_attach(GTK_TABLE(table), label2, 0, 1, 4, 5,
			 (GtkAttachOptions) (GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);
	gtk_label_set_justify(GTK_LABEL(label2), GTK_JUSTIFY_RIGHT);
	gtk_misc_set_alignment(GTK_MISC(label2), 1, 0.5);

	entry_aspell_path = gtk_entry_new();
	gtk_widget_show(entry_aspell_path);
	gtk_table_attach(GTK_TABLE(table), entry_aspell_path, 1, 2, 4, 5,
			 (GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);

	label3 = gtk_label_new(_("Default dictionary:"));
	gtk_widget_show(label3);
	gtk_table_attach(GTK_TABLE(table), label3, 0, 1, 5, 6,
			 (GtkAttachOptions) (GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);
	gtk_label_set_justify(GTK_LABEL(label3), GTK_JUSTIFY_RIGHT);
	gtk_misc_set_alignment(GTK_MISC(label3), 1, 0.5);

	optmenu_dictionary = gtk_option_menu_new();
	gtk_widget_show(optmenu_dictionary);
	gtk_table_attach(GTK_TABLE(table), optmenu_dictionary, 1, 3, 5, 6,
			 (GtkAttachOptions) (GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);
	optmenu_dictionary_menu = gtk_menu_new();
	gtk_option_menu_set_menu(GTK_OPTION_MENU(optmenu_dictionary),
				 optmenu_dictionary_menu);

	label4 = gtk_label_new(_("Default suggestion mode:"));
	gtk_widget_show(label4);
	gtk_table_attach(GTK_TABLE(table), label4, 0, 1, 6, 7,
			 (GtkAttachOptions) (GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);
	gtk_label_set_justify(GTK_LABEL(label4), GTK_JUSTIFY_RIGHT);
	gtk_misc_set_alignment(GTK_MISC(label4), 1, 0.5);

	optmenu_sugmode = gtk_option_menu_new();
	gtk_widget_show(optmenu_sugmode);
	gtk_table_attach(GTK_TABLE(table), optmenu_sugmode, 1, 3, 6, 7,
			 (GtkAttachOptions) (GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);
	optmenu_sugmode_menu = gtk_menu_new();
	gtk_option_menu_set_menu(GTK_OPTION_MENU(optmenu_sugmode),
				 optmenu_sugmode_menu);

	label5 = gtk_label_new(_("Misspelled word color:"));
	gtk_widget_show(label5);
	gtk_table_attach(GTK_TABLE(table), label5, 0, 1, 7, 8,
			 (GtkAttachOptions) (GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);
	gtk_label_set_justify(GTK_LABEL(label5), GTK_JUSTIFY_RIGHT);
	gtk_misc_set_alignment(GTK_MISC(label5), 1, 0.5);

	btn_aspell_path = gtk_button_new_with_label(_(" ... "));
	gtk_widget_show(btn_aspell_path);
	gtk_table_attach(GTK_TABLE(table), btn_aspell_path, 2, 3, 4, 5,
			 (GtkAttachOptions) (GTK_FILL),
			 (GtkAttachOptions) (0), 0, 0);

	hbox1 = gtk_hbox_new(FALSE, 0);
	gtk_widget_show(hbox1);
	gtk_table_attach(GTK_TABLE(table), hbox1, 1, 2, 7, 8,
			 (GtkAttachOptions) (GTK_FILL),
			 (GtkAttachOptions) (GTK_FILL), 0, 0);

	misspelled_btn = gtk_button_new_with_label("");
	gtk_widget_show(misspelled_btn);
	gtk_box_pack_start(GTK_BOX(hbox1), misspelled_btn, FALSE, FALSE,
			   0);
	gtk_widget_set_size_request(misspelled_btn, 30, 20);
	label5 = gtk_label_new(_("(Black to use underline)"));
	gtk_misc_set_alignment(GTK_MISC(label5), 0, 0.5);
	gtk_label_set_justify(GTK_LABEL(label4), GTK_JUSTIFY_LEFT);
	gtk_widget_show(label5);
	font_desc = pango_font_description_new();
	size = pango_font_description_get_size
		(label5->style->font_desc);
	pango_font_description_set_size(font_desc, size * PANGO_SCALE_SMALL);
	gtk_widget_modify_font(label5, font_desc);
	pango_font_description_free(font_desc);
	gtk_box_pack_start(GTK_BOX(hbox1), label5, FALSE, FALSE,
			   4);
	/* END GLADE CODE */

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_enable_aspell),
				     prefs_common.enable_aspell);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_check_while_typing),
				     prefs_common.check_while_typing);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_use_alternate),
				     prefs_common.use_alternate);
	gtk_entry_set_text(GTK_ENTRY(entry_aspell_path), 
			   SAFE_STRING(prefs_common.aspell_path));

	g_signal_connect(G_OBJECT(checkbtn_enable_aspell), "toggled",
			 G_CALLBACK(prefs_spelling_checkbtn_enable_aspell_toggle_cb),
			 prefs_spelling);
	g_signal_connect(G_OBJECT(btn_aspell_path), "clicked", 
			 G_CALLBACK(prefs_spelling_btn_aspell_path_clicked_cb),
			 prefs_spelling);
	g_signal_connect(G_OBJECT(misspelled_btn), "clicked",
			 G_CALLBACK(prefs_spelling_colorsel), prefs_spelling);

	gtk_option_menu_set_menu(GTK_OPTION_MENU(optmenu_dictionary),
				 gtkaspell_dictionary_option_menu_new(prefs_common.aspell_path));
	gtkaspell_set_dictionary_menu_active_item(optmenu_dictionary, prefs_common.dictionary);

	gtk_option_menu_set_menu(GTK_OPTION_MENU(optmenu_sugmode),
				 gtkaspell_sugmode_option_menu_new(prefs_common.aspell_sugmode));
	gtkaspell_sugmode_option_menu_set(GTK_OPTION_MENU(optmenu_sugmode),
					  prefs_common.aspell_sugmode);

	prefs_spelling->misspell_col = prefs_common.misspelled_col;
	gtkut_set_widget_bgcolor_rgb(misspelled_btn, prefs_spelling->misspell_col);

	prefs_spelling->window
		= GTK_WIDGET(window);
	prefs_spelling->checkbtn_enable_aspell 
		= checkbtn_enable_aspell;
	prefs_spelling->entry_aspell_path
		= entry_aspell_path;
	prefs_spelling->btn_aspell_path
		= btn_aspell_path;
	prefs_spelling->optmenu_dictionary
		= optmenu_dictionary;
	prefs_spelling->optmenu_sugmode
		= optmenu_sugmode;
	prefs_spelling->checkbtn_use_alternate
		= checkbtn_use_alternate;
	prefs_spelling->checkbtn_check_while_typing
		= checkbtn_check_while_typing;
	prefs_spelling->misspelled_btn
		= misspelled_btn;

	prefs_spelling->page.widget = table;

	prefs_spelling_enable(prefs_spelling, prefs_common.enable_aspell);
}

void prefs_spelling_save(PrefsPage *_page)
{
	SpellingPage *spelling = (SpellingPage *) _page;

	prefs_common.enable_aspell =
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(spelling->checkbtn_enable_aspell));
	prefs_common.check_while_typing =
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(spelling->checkbtn_check_while_typing));
	prefs_common.use_alternate =
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(spelling->checkbtn_use_alternate));

	if (prefs_common.aspell_path)
		g_free(prefs_common.aspell_path);
	prefs_common.aspell_path =
		gtk_editable_get_chars(GTK_EDITABLE(spelling->entry_aspell_path), 0, -1);

	if (prefs_common.dictionary != NULL)
		g_free(prefs_common.dictionary);
	prefs_common.dictionary = 
		gtkaspell_get_dictionary_menu_active_item(
			gtk_option_menu_get_menu(
				GTK_OPTION_MENU(
					spelling->optmenu_dictionary)));

	prefs_common.aspell_sugmode =
		gtkaspell_get_sugmode_from_option_menu(
			GTK_OPTION_MENU(spelling->optmenu_sugmode));

	prefs_common.misspelled_col = spelling->misspell_col;
}

static void prefs_spelling_destroy_widget(PrefsPage *_page)
{
	/* SpellingPage *spelling = (SpellingPage *) _page; */

}

SpellingPage *prefs_spelling;

void prefs_spelling_init(void)
{
	SpellingPage *page;
	static gchar *path[3];
	const gchar* language = NULL;
	
	path[0] = _("Compose");
	path[1] = _("Spell Checking");
	path[2] = NULL;

	page = g_new0(SpellingPage, 1);
	page->page.path = path;
	page->page.create_widget = prefs_spelling_create_widget;
	page->page.destroy_widget = prefs_spelling_destroy_widget;
	page->page.save_page = prefs_spelling_save;
	page->page.weight = 180.0;

	prefs_gtk_register_page((PrefsPage *) page);
	prefs_spelling = page;
	
	language = g_getenv("LANG");
	if (language == NULL)
		language = "en";
	else if (!strcmp(language, "POSIX") || !strcmp(language, "C"))
		language = "en";
	
	if (!prefs_common.dictionary)
		prefs_common.dictionary = g_strdup_printf("%s%s",
						prefs_common.aspell_path,
						language);
	if (!strlen(prefs_common.dictionary)
	||  !strcmp(prefs_common.dictionary,"(None"))
		prefs_common.dictionary = g_strdup_printf("%s%s",
						prefs_common.aspell_path,
						language);
	if (strcasestr(prefs_common.dictionary,".utf"))
		*(strcasestr(prefs_common.dictionary,".utf")) = '\0';
	if (strstr(prefs_common.dictionary,"@"))
		*(strstr(prefs_common.dictionary,"@")) = '\0';
}

void prefs_spelling_done(void)
{
	prefs_gtk_unregister_page((PrefsPage *) prefs_spelling);
	g_free(prefs_spelling);
}

#endif /* USE_ASPELL */
