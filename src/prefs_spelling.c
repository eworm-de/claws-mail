/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2002-2006 Hiroyuki Yamamoto & the Sylpheed-Claws team
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

	GtkWidget *automatic_frame;
	GtkWidget *dictionary_frame;
	GtkWidget *path_frame;
	
	GtkWidget *enable_aspell_checkbtn;
	GtkWidget *recheck_when_changing_dict_checkbtn;
	GtkWidget *check_while_typing_checkbtn;
	GtkWidget *use_alternate_checkbtn;

	GtkWidget *aspell_path_entry;
	GtkWidget *aspell_path_select;

	GtkWidget *default_dict_label;
	GtkWidget *default_dict_optmenu;
	GtkWidget *default_dict_optmenu_menu;

	GtkWidget *sugmode_label;
	GtkWidget *sugmode_optmenu;
	GtkWidget *sugmode_optmenu_menu;

	GtkWidget *misspelled_label;
	GtkWidget *misspelled_colorbtn;
	GtkWidget *misspelled_useblack_label;

	gint	   misspell_col;
} SpellingPage;

static void prefs_spelling_enable(SpellingPage *spelling, gboolean enable)
{
	gtk_widget_set_sensitive(spelling->automatic_frame, enable);
	gtk_widget_set_sensitive(spelling->dictionary_frame, enable);
	gtk_widget_set_sensitive(spelling->path_frame, enable);
	gtk_widget_set_sensitive(spelling->misspelled_label, enable);
	gtk_widget_set_sensitive(spelling->misspelled_colorbtn, enable);
	gtk_widget_set_sensitive(spelling->use_alternate_checkbtn, enable);
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
		gtk_option_menu_set_menu(GTK_OPTION_MENU(spelling->default_dict_optmenu),
					 new_menu);

		gtk_entry_set_text(GTK_ENTRY(spelling->aspell_path_entry), tmp);
		/* select first one */
		gtk_option_menu_set_history(GTK_OPTION_MENU(
					spelling->default_dict_optmenu), 0);
	
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
	gtkut_set_widget_bgcolor_rgb(spelling->misspelled_colorbtn, rgbcolor);
	spelling->misspell_col = rgbcolor;
}

#define SAFE_STRING(str) \
	(str) ? (str) : ""

void prefs_spelling_create_widget(PrefsPage *_page, GtkWindow *window, gpointer data)
{
	SpellingPage *prefs_spelling = (SpellingPage *) _page;

	GtkWidget *vbox1, *vbox2, *hbox;

	GtkWidget *enable_aspell_checkbtn;
	GtkWidget *check_while_typing_checkbtn;
	GtkWidget *recheck_when_changing_dict_checkbtn;
	GtkWidget *use_alternate_checkbtn;

	GtkWidget *label;

	GtkWidget *automatic_frame;
	GtkWidget *dictionary_frame;
	GtkWidget *path_frame;

	GtkWidget *aspell_path_hbox;
	GtkWidget *aspell_path_entry;
	GtkWidget *aspell_path_select;

	GtkWidget *default_dict_label;
	GtkWidget *default_dict_optmenu;
	GtkWidget *default_dict_optmenu_menu;

	GtkWidget *sugmode_label;
	GtkWidget *sugmode_optmenu;
	GtkWidget *sugmode_optmenu_menu;

	GtkWidget *misspelled_label;
	GtkWidget *misspelled_hbox;
	GtkWidget *misspelled_colorbtn;
	GtkTooltips *tooltips;

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

	vbox2 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox2);
	gtk_box_pack_start (GTK_BOX (vbox1), vbox2, FALSE, FALSE, 0);

	enable_aspell_checkbtn = gtk_check_button_new_with_label(
			_("Enable spell checker"));
	gtk_widget_show(enable_aspell_checkbtn);
	gtk_box_pack_start(GTK_BOX(vbox2), enable_aspell_checkbtn, TRUE, TRUE, 0);

	use_alternate_checkbtn = gtk_check_button_new_with_label(
			_("Enable alternate dictionary"));
	gtk_widget_show(use_alternate_checkbtn);
	gtk_box_pack_start(GTK_BOX(vbox2), use_alternate_checkbtn, TRUE, TRUE, 0);

	tooltips = gtk_tooltips_new();
	gtk_tooltips_set_tip(tooltips, use_alternate_checkbtn, 
			_("Faster switching with last used dictionary"), NULL);

	PACK_FRAME(vbox1, path_frame, _("Dictionary path"));
	aspell_path_hbox = gtk_hbox_new(FALSE, 8);
	gtk_widget_show(aspell_path_hbox);
	gtk_container_add(GTK_CONTAINER(path_frame), aspell_path_hbox);
	gtk_container_set_border_width(GTK_CONTAINER(aspell_path_hbox), 8);	

	aspell_path_entry = gtk_entry_new();
	gtk_widget_show(aspell_path_entry);
	gtk_box_pack_start(GTK_BOX(aspell_path_hbox), aspell_path_entry, TRUE, TRUE, 0);
	gtk_widget_set_size_request(aspell_path_entry, 30, 20);

	aspell_path_select = gtkut_get_browse_directory_btn(_("_Browse"));
	gtk_widget_show(aspell_path_select);
	gtk_box_pack_start(GTK_BOX(aspell_path_hbox), aspell_path_select, FALSE, FALSE, 0);

	PACK_FRAME(vbox1, automatic_frame, _("Automatic spelling"));
	vbox2 = gtk_vbox_new(FALSE, VSPACING_NARROW);
	gtk_widget_show(vbox2);
	gtk_container_add(GTK_CONTAINER(automatic_frame), vbox2);
	gtk_container_set_border_width(GTK_CONTAINER(vbox2), 8);
	
	check_while_typing_checkbtn = gtk_check_button_new_with_label(
			_("Check while typing"));
	gtk_widget_show(check_while_typing_checkbtn);
	gtk_box_pack_start(GTK_BOX(vbox2), check_while_typing_checkbtn, TRUE, TRUE, 0);

	recheck_when_changing_dict_checkbtn = gtk_check_button_new_with_label(
			_("Re-check message when changing dictionary"));
	gtk_widget_show(recheck_when_changing_dict_checkbtn);
	gtk_box_pack_start(GTK_BOX(vbox2), recheck_when_changing_dict_checkbtn, TRUE, TRUE, 0);
	
	PACK_FRAME(vbox1, dictionary_frame, _("Dictionary"));
	vbox2 = gtk_vbox_new(FALSE, VSPACING_NARROW);
	gtk_widget_show(vbox2);
	gtk_container_add(GTK_CONTAINER(dictionary_frame), vbox2);
	gtk_container_set_border_width(GTK_CONTAINER(vbox2), 8);
	
	hbox = gtk_hbox_new(FALSE, 10);
	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox2), hbox, TRUE, TRUE, 0);
	
	default_dict_label = gtk_label_new(_("Default dictionary"));
	gtk_widget_show(default_dict_label);
	gtk_label_set_justify(GTK_LABEL(default_dict_label), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment(GTK_MISC(default_dict_label), 1, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), default_dict_label, FALSE, FALSE, 0);
	
	label = gtk_label_new("");
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);
	
	default_dict_optmenu = gtk_option_menu_new();
	gtk_widget_show(default_dict_optmenu);
	gtk_widget_set_size_request(default_dict_optmenu, 180, -1);

	default_dict_optmenu_menu = gtk_menu_new();
	gtk_option_menu_set_menu(GTK_OPTION_MENU(default_dict_optmenu),
			default_dict_optmenu_menu);
	gtk_box_pack_start(GTK_BOX(hbox), default_dict_optmenu, FALSE, FALSE, 0);

	hbox = gtk_hbox_new(FALSE, 10);
	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox2), hbox, TRUE, TRUE, 0);
	
	sugmode_label = gtk_label_new(_("Default suggestion mode"));
	gtk_widget_show(sugmode_label);
	gtk_label_set_justify(GTK_LABEL(sugmode_label), GTK_JUSTIFY_LEFT);
	gtk_misc_set_alignment(GTK_MISC(sugmode_label), 1, 0.5);
	gtk_box_pack_start(GTK_BOX(hbox), sugmode_label, FALSE, FALSE, 0);

	label = gtk_label_new("");
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);

	sugmode_optmenu = gtk_option_menu_new();
	gtk_widget_show(sugmode_optmenu);
	gtk_widget_set_size_request(sugmode_optmenu, 180, -1); 
	
	sugmode_optmenu_menu = gtk_menu_new();
	gtk_option_menu_set_menu(GTK_OPTION_MENU(sugmode_optmenu),
			sugmode_optmenu_menu);
	gtk_box_pack_start(GTK_BOX(hbox), sugmode_optmenu, FALSE, FALSE, 0);
	
	misspelled_hbox = gtk_hbox_new(FALSE, 10);
	gtk_widget_show(misspelled_hbox);
	gtk_box_pack_start(GTK_BOX(vbox1), misspelled_hbox, FALSE, FALSE, 0);
		
	misspelled_label = gtk_label_new(_("Misspelled word color:"));
	gtk_widget_show(misspelled_label);
	gtk_box_pack_start(GTK_BOX(misspelled_hbox), misspelled_label,
		FALSE, FALSE, 0);
	gtk_label_set_justify(GTK_LABEL(misspelled_label), GTK_JUSTIFY_RIGHT);
	gtk_misc_set_alignment(GTK_MISC(misspelled_label), 1, 0.5);

	misspelled_colorbtn = gtk_button_new_with_label("");
	gtk_widget_show(misspelled_colorbtn);
	gtk_box_pack_start(GTK_BOX(misspelled_hbox), misspelled_colorbtn,
		FALSE, FALSE, 0);
	gtk_widget_set_size_request(misspelled_colorbtn, 30, 20);
	tooltips = gtk_tooltips_new();
	gtk_tooltips_set_tip(tooltips, misspelled_colorbtn,
			     _("Pick color for misspelled word. "
			       "Use black to underline"), NULL);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enable_aspell_checkbtn),
			prefs_common.enable_aspell);
	g_signal_connect(G_OBJECT(enable_aspell_checkbtn), "toggled",
			 G_CALLBACK(prefs_spelling_checkbtn_enable_aspell_toggle_cb),
			 prefs_spelling);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_while_typing_checkbtn),
			prefs_common.check_while_typing);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(recheck_when_changing_dict_checkbtn),
			prefs_common.recheck_when_changing_dict);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(use_alternate_checkbtn),
			prefs_common.use_alternate);
	gtk_entry_set_text(GTK_ENTRY(aspell_path_entry), 
			SAFE_STRING(prefs_common.aspell_path));
	g_signal_connect(G_OBJECT(aspell_path_select), "clicked", 
			 G_CALLBACK(prefs_spelling_btn_aspell_path_clicked_cb),
			 prefs_spelling);
	gtk_option_menu_set_menu(GTK_OPTION_MENU(default_dict_optmenu),
				 gtkaspell_dictionary_option_menu_new(prefs_common.aspell_path));
	gtkaspell_set_dictionary_menu_active_item(default_dict_optmenu, prefs_common.dictionary);
	gtk_option_menu_set_menu(GTK_OPTION_MENU(sugmode_optmenu),
				 gtkaspell_sugmode_option_menu_new(prefs_common.aspell_sugmode));
	gtkaspell_sugmode_option_menu_set(GTK_OPTION_MENU(sugmode_optmenu),
					  prefs_common.aspell_sugmode);
	g_signal_connect(G_OBJECT(misspelled_colorbtn), "clicked",
			 G_CALLBACK(prefs_spelling_colorsel), prefs_spelling);

	prefs_spelling->misspell_col = prefs_common.misspelled_col;
	gtkut_set_widget_bgcolor_rgb(misspelled_colorbtn, prefs_spelling->misspell_col);

	prefs_spelling->window			= GTK_WIDGET(window);
	prefs_spelling->automatic_frame =	automatic_frame;
	prefs_spelling->dictionary_frame =	dictionary_frame;
	prefs_spelling->path_frame =	path_frame;
	prefs_spelling->enable_aspell_checkbtn	= enable_aspell_checkbtn;
	prefs_spelling->check_while_typing_checkbtn
		= check_while_typing_checkbtn;
	prefs_spelling->recheck_when_changing_dict_checkbtn
		= recheck_when_changing_dict_checkbtn;
	prefs_spelling->use_alternate_checkbtn	= use_alternate_checkbtn;
	prefs_spelling->aspell_path_entry	= aspell_path_entry;
	prefs_spelling->aspell_path_select	= aspell_path_select;
	prefs_spelling->default_dict_label	= default_dict_label;
	prefs_spelling->default_dict_optmenu	= default_dict_optmenu;
	prefs_spelling->default_dict_optmenu_menu
		= default_dict_optmenu_menu;
	prefs_spelling->sugmode_label		= sugmode_label;
	prefs_spelling->sugmode_optmenu		= sugmode_optmenu;
	prefs_spelling->sugmode_optmenu_menu	= sugmode_optmenu_menu;
	prefs_spelling->misspelled_label	= misspelled_label;
	prefs_spelling->misspelled_colorbtn	= misspelled_colorbtn;

	prefs_spelling->page.widget = vbox1;

	prefs_spelling_enable(prefs_spelling, prefs_common.enable_aspell);
}

void prefs_spelling_save(PrefsPage *_page)
{
	SpellingPage *spelling = (SpellingPage *) _page;

	prefs_common.enable_aspell =
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(spelling->enable_aspell_checkbtn));
	prefs_common.check_while_typing =
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(spelling->check_while_typing_checkbtn));
	prefs_common.recheck_when_changing_dict =
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(spelling->recheck_when_changing_dict_checkbtn));
	prefs_common.use_alternate =
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(spelling->use_alternate_checkbtn));

	g_free(prefs_common.aspell_path);
	prefs_common.aspell_path =
		gtk_editable_get_chars(GTK_EDITABLE(spelling->aspell_path_entry), 0, -1);

	g_free(prefs_common.dictionary);
	prefs_common.dictionary = 
		gtkaspell_get_dictionary_menu_active_item(
			gtk_option_menu_get_menu(
				GTK_OPTION_MENU(
					spelling->default_dict_optmenu)));

	prefs_common.aspell_sugmode =
		gtkaspell_get_sugmode_from_option_menu(
			GTK_OPTION_MENU(spelling->sugmode_optmenu));

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
