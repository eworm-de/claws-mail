/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2005-2007 Colin Leroy <colin@colino.net> & The Claws Mail Team
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

#include "prefs_common.h"
#include "prefs_gtk.h"

#include "gtk/gtkutils.h"
#include "gtk/prefswindow.h"
#include "gtk/menu.h"

#include "manage_window.h"

#include "log.h"

typedef struct _LoggingPage
{
	PrefsPage page;

	GtkWidget *window;

	GtkWidget *checkbtn_cliplog;
	GtkWidget *spinbtn_loglength;
	GtkWidget *checkbtn_debug_cliplog;
	GtkWidget *spinbtn_debug_loglength;
	GtkWidget *checkbtn_filteringdebug;
	GtkWidget *checkbtn_filteringdebug_inc;
	GtkWidget *checkbtn_filteringdebug_manual;
	GtkWidget *checkbtn_filteringdebug_folder_proc;
	GtkWidget *checkbtn_filteringdebug_pre_proc;
	GtkWidget *checkbtn_filteringdebug_post_proc;
	GtkWidget *optmenu_filteringdebug_level;
} LoggingPage;

static void prefs_logging_create_widget(PrefsPage *_page, GtkWindow *window, 
			       	  gpointer data)
{
	LoggingPage *prefs_logging = (LoggingPage *) _page;
	
	GtkWidget *vbox1;

	GtkWidget *frame_logging;
	GtkWidget *vbox_logging;
	GtkWidget *hbox_cliplog;
	GtkWidget *checkbtn_cliplog;
	GtkWidget *loglength_label;
	GtkWidget *spinbtn_loglength;
	GtkObject *spinbtn_loglength_adj;
	GtkTooltips *loglength_tooltip;
	GtkWidget *label;
	GtkWidget *vbox_filteringdebug_log;
	GtkWidget *hbox_debug_cliplog;
	GtkWidget *checkbtn_debug_cliplog;
	GtkWidget *debug_loglength_label;
	GtkWidget *spinbtn_debug_loglength;
	GtkObject *spinbtn_debug_loglength_adj;
	GtkTooltips *debug_loglength_tooltip;
	GtkWidget *hbox_filteringdebug;
	GtkWidget *checkbtn_filteringdebug;
	GtkWidget *frame_filteringdebug;
	GtkWidget *vbox_filteringdebug;
	GtkWidget *hbox_filteringdebug_inc;
	GtkWidget *checkbtn_filteringdebug_inc;
	GtkWidget *hbox_filteringdebug_manual;
	GtkWidget *checkbtn_filteringdebug_manual;
	GtkWidget *hbox_filteringdebug_folder_proc;
	GtkWidget *checkbtn_filteringdebug_folder_proc;
	GtkWidget *hbox_filteringdebug_pre_proc;
	GtkWidget *checkbtn_filteringdebug_pre_proc;
	GtkWidget *hbox_filteringdebug_post_proc;
	GtkWidget *checkbtn_filteringdebug_post_proc;
	GtkTooltips *filteringdebug_tooltip;
	GtkWidget *hbox_filteringdebug_level;
	GtkWidget *label_debug_level;
	GtkWidget *optmenu_filteringdebug_level;
	GtkWidget *menu;
	GtkWidget *menuitem;
	GtkTooltips *filteringdebug_level_tooltip;

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

	/* Protocol log */
	vbox_logging = gtkut_get_options_frame(vbox1, &frame_logging, _("Protocol log"));

	PACK_CHECK_BUTTON (vbox_logging, checkbtn_cliplog,
			   _("Clip the log size"));
	hbox_cliplog = gtk_hbox_new (FALSE, 8);
	gtk_container_add (GTK_CONTAINER (vbox_logging), hbox_cliplog);
	gtk_widget_show (hbox_cliplog);
	
	loglength_label = gtk_label_new (_("Log window length"));
	gtk_box_pack_start (GTK_BOX (hbox_cliplog), loglength_label,
			    FALSE, TRUE, 0);
	gtk_widget_show (GTK_WIDGET (loglength_label));
	
	loglength_tooltip = gtk_tooltips_new();

	spinbtn_loglength_adj = gtk_adjustment_new (500, 0, G_MAXINT, 1, 10, 10);
	spinbtn_loglength = gtk_spin_button_new
		(GTK_ADJUSTMENT (spinbtn_loglength_adj), 1, 0);
	gtk_widget_show (spinbtn_loglength);
	gtk_box_pack_start (GTK_BOX (hbox_cliplog), spinbtn_loglength,
			    FALSE, FALSE, 0);
	gtk_widget_set_size_request (GTK_WIDGET (spinbtn_loglength), 64, -1);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbtn_loglength), TRUE);

	gtk_tooltips_set_tip(GTK_TOOLTIPS(loglength_tooltip), spinbtn_loglength,
			     _("0 to stop logging in the log window"),
			     NULL);

	label = gtk_label_new(_("lines"));
	gtk_widget_show (label);
  	gtk_box_pack_start(GTK_BOX(hbox_cliplog), label, FALSE, FALSE, 0);

	SET_TOGGLE_SENSITIVITY(checkbtn_cliplog, loglength_label);
	SET_TOGGLE_SENSITIVITY(checkbtn_cliplog, spinbtn_loglength);
	SET_TOGGLE_SENSITIVITY(checkbtn_cliplog, label);

	/* Filtering/processing debug log */
	vbox_filteringdebug_log = gtkut_get_options_frame(vbox1,
				&frame_logging, _("Filtering/processing debug log"));

	PACK_CHECK_BUTTON (vbox_filteringdebug_log, checkbtn_filteringdebug,
			   _("Enable debugging of filtering/processing rules"));
	hbox_filteringdebug = gtk_hbox_new (FALSE, 8);
	gtk_container_add (GTK_CONTAINER (vbox_filteringdebug_log), hbox_filteringdebug);
	gtk_widget_show (hbox_filteringdebug);

	filteringdebug_tooltip = gtk_tooltips_new();
	gtk_tooltips_set_tip(GTK_TOOLTIPS(filteringdebug_tooltip), checkbtn_filteringdebug,
			     _("If checked, turns on debugging of filtering and processing rules.\n"
					"Debug log is available from 'Tools/Filtering debug window'.\n"
					"Caution: enabling this option will slow down the filtering/processing, "
					"this might be critical when applying many rules upon thousands of messages."),
			     NULL);

	vbox_filteringdebug = gtkut_get_options_frame(vbox_filteringdebug_log, &frame_filteringdebug,
							_("Debugging of filtering/processing rules when.."));

	PACK_CHECK_BUTTON (vbox_filteringdebug, checkbtn_filteringdebug_inc,
			   _("filtering at incorporation"));
	hbox_filteringdebug_inc = gtk_hbox_new (FALSE, 8);
	gtk_container_add (GTK_CONTAINER (vbox_filteringdebug), hbox_filteringdebug_inc);
	gtk_widget_show (hbox_filteringdebug_inc);

	PACK_CHECK_BUTTON (vbox_filteringdebug, checkbtn_filteringdebug_manual,
			   _("manually filtering"));
	hbox_filteringdebug_manual = gtk_hbox_new (FALSE, 8);
	gtk_container_add (GTK_CONTAINER (vbox_filteringdebug), hbox_filteringdebug_manual);
	gtk_widget_show (hbox_filteringdebug_manual);

	PACK_CHECK_BUTTON (vbox_filteringdebug, checkbtn_filteringdebug_folder_proc,
			   _("processing folders"));
	hbox_filteringdebug_folder_proc = gtk_hbox_new (FALSE, 8);
	gtk_container_add (GTK_CONTAINER (vbox_filteringdebug), hbox_filteringdebug_folder_proc);
	gtk_widget_show (hbox_filteringdebug_folder_proc);

	PACK_CHECK_BUTTON (vbox_filteringdebug, checkbtn_filteringdebug_pre_proc,
			   _("pre-processing folders"));
	hbox_filteringdebug_pre_proc = gtk_hbox_new (FALSE, 8);
	gtk_container_add (GTK_CONTAINER (vbox_filteringdebug), hbox_filteringdebug_pre_proc);
	gtk_widget_show (hbox_filteringdebug_pre_proc);

	PACK_CHECK_BUTTON (vbox_filteringdebug, checkbtn_filteringdebug_post_proc,
			   _("post-processing folders"));
	hbox_filteringdebug_post_proc = gtk_hbox_new (FALSE, 8);
	gtk_container_add (GTK_CONTAINER (vbox_filteringdebug), hbox_filteringdebug_post_proc);
	gtk_widget_show (hbox_filteringdebug_post_proc);

	SET_TOGGLE_SENSITIVITY(checkbtn_filteringdebug, checkbtn_filteringdebug_inc);
	SET_TOGGLE_SENSITIVITY(checkbtn_filteringdebug, checkbtn_filteringdebug_manual);
	SET_TOGGLE_SENSITIVITY(checkbtn_filteringdebug, checkbtn_filteringdebug_folder_proc);
	SET_TOGGLE_SENSITIVITY(checkbtn_filteringdebug, checkbtn_filteringdebug_pre_proc);
	SET_TOGGLE_SENSITIVITY(checkbtn_filteringdebug, checkbtn_filteringdebug_post_proc);

	hbox_filteringdebug_level = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox_filteringdebug_level);
	gtk_box_pack_start(GTK_BOX (vbox_filteringdebug_log), hbox_filteringdebug_level, FALSE, FALSE, 0);

	label_debug_level = gtk_label_new (_("Debug level"));
	gtk_widget_show (label_debug_level);
	gtk_box_pack_start(GTK_BOX(hbox_filteringdebug_level), label_debug_level, FALSE, FALSE, 0);

 	optmenu_filteringdebug_level = gtk_option_menu_new ();
 	gtk_widget_show (optmenu_filteringdebug_level);
 	
	menu = gtk_menu_new ();
	MENUITEM_ADD (menu, menuitem, _("Low"), 0);
	MENUITEM_ADD (menu, menuitem, _("Medium"), 1);
	MENUITEM_ADD (menu, menuitem, _("High"), 2);

	gtk_option_menu_set_menu (GTK_OPTION_MENU (optmenu_filteringdebug_level), menu);
	gtk_box_pack_start(GTK_BOX(hbox_filteringdebug_level), optmenu_filteringdebug_level, FALSE, FALSE, 0);

	filteringdebug_level_tooltip = gtk_tooltips_new();
	gtk_tooltips_set_tip(GTK_TOOLTIPS(filteringdebug_level_tooltip), optmenu_filteringdebug_level,
			     _("Select the level of detail displayed if debugging is enabled.\n"
				"Choose low level to see when rules are applied, what conditions match or not and what actions are performed.\n"
				"Choose medium level to see more detail about the message that is being processed, and why rules are skipped.\n"
				"Choose high level to explicitely show the reason why all rules are skipped or not, and why all conditions are matching or not.\n"
				"Caution: the higher the level is, the more it will impact the performances."),
			     NULL);

	PACK_CHECK_BUTTON (vbox_filteringdebug_log, checkbtn_debug_cliplog,
			   _("Clip the log size"));
	hbox_debug_cliplog = gtk_hbox_new (FALSE, 8);
	gtk_container_add (GTK_CONTAINER (vbox_filteringdebug_log), hbox_debug_cliplog);
	gtk_widget_show (hbox_debug_cliplog);
	
	debug_loglength_label = gtk_label_new (_("Log window length"));
	gtk_box_pack_start (GTK_BOX (hbox_debug_cliplog), debug_loglength_label,
			    FALSE, TRUE, 0);
	gtk_widget_show (GTK_WIDGET (debug_loglength_label));
	
	debug_loglength_tooltip = gtk_tooltips_new();

	spinbtn_debug_loglength_adj = gtk_adjustment_new (500, 0, G_MAXINT, 1, 10, 10);
	spinbtn_debug_loglength = gtk_spin_button_new
		(GTK_ADJUSTMENT (spinbtn_debug_loglength_adj), 1, 0);
	gtk_widget_show (spinbtn_debug_loglength);
	gtk_box_pack_start (GTK_BOX (hbox_debug_cliplog), spinbtn_debug_loglength,
			    FALSE, FALSE, 0);
	gtk_widget_set_size_request (GTK_WIDGET (spinbtn_debug_loglength), 64, -1);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbtn_debug_loglength), TRUE);

	gtk_tooltips_set_tip(GTK_TOOLTIPS(debug_loglength_tooltip), spinbtn_debug_loglength,
			     _("0 to stop logging in the log window"),
			     NULL);

	label = gtk_label_new(_("lines"));
	gtk_widget_show (label);
  	gtk_box_pack_start(GTK_BOX(hbox_debug_cliplog), label, FALSE, FALSE, 0);

	SET_TOGGLE_SENSITIVITY(checkbtn_debug_cliplog, debug_loglength_label);
	SET_TOGGLE_SENSITIVITY(checkbtn_debug_cliplog, spinbtn_debug_loglength);
	SET_TOGGLE_SENSITIVITY(checkbtn_debug_cliplog, label);

	SET_TOGGLE_SENSITIVITY(checkbtn_filteringdebug, optmenu_filteringdebug_level);
	SET_TOGGLE_SENSITIVITY(checkbtn_filteringdebug, checkbtn_debug_cliplog);
	SET_TOGGLE_SENSITIVITY(checkbtn_filteringdebug, label_debug_level);
	SET_TOGGLE_SENSITIVITY(checkbtn_filteringdebug, debug_loglength_label);
	SET_TOGGLE_SENSITIVITY(checkbtn_filteringdebug, spinbtn_debug_loglength);
	SET_TOGGLE_SENSITIVITY(checkbtn_filteringdebug, label);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_cliplog), 
		prefs_common.cliplog);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_debug_cliplog), 
		prefs_common.filtering_debug_cliplog);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_filteringdebug), 
		prefs_common.enable_filtering_debug);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_filteringdebug_inc), 
		prefs_common.enable_filtering_debug_inc);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_filteringdebug_manual), 
		prefs_common.enable_filtering_debug_manual);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_filteringdebug_folder_proc), 
		prefs_common.enable_filtering_debug_folder_proc);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_filteringdebug_pre_proc), 
		prefs_common.enable_filtering_debug_pre_proc);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_filteringdebug_post_proc), 
		prefs_common.enable_filtering_debug_post_proc);

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinbtn_loglength),
		prefs_common.loglength);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinbtn_debug_loglength),
		prefs_common.filtering_debug_loglength);

	gtk_option_menu_set_history(GTK_OPTION_MENU(optmenu_filteringdebug_level),
			prefs_common.filtering_debug_level);

	prefs_logging->checkbtn_cliplog = checkbtn_cliplog;
	prefs_logging->spinbtn_loglength = spinbtn_loglength;
	prefs_logging->checkbtn_debug_cliplog = checkbtn_debug_cliplog;
	prefs_logging->spinbtn_debug_loglength = spinbtn_debug_loglength;
	prefs_logging->checkbtn_filteringdebug = checkbtn_filteringdebug;
	prefs_logging->checkbtn_filteringdebug_inc = checkbtn_filteringdebug_inc;
	prefs_logging->checkbtn_filteringdebug_manual = checkbtn_filteringdebug_manual;
	prefs_logging->checkbtn_filteringdebug_folder_proc = checkbtn_filteringdebug_folder_proc;
	prefs_logging->checkbtn_filteringdebug_pre_proc = checkbtn_filteringdebug_pre_proc;
	prefs_logging->checkbtn_filteringdebug_post_proc = checkbtn_filteringdebug_post_proc;
	prefs_logging->optmenu_filteringdebug_level = optmenu_filteringdebug_level;

	prefs_logging->page.widget = vbox1;
}

static void prefs_logging_save(PrefsPage *_page)
{
	LoggingPage *page = (LoggingPage *) _page;
	MainWindow *mainwindow;
	gboolean filtering_debug_enabled;
	GtkWidget *menu;
	GtkWidget *menuitem;

	menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(page->optmenu_filteringdebug_level));
	menuitem = gtk_menu_get_active(GTK_MENU(menu));
	prefs_common.filtering_debug_level = GPOINTER_TO_INT
			(g_object_get_data(G_OBJECT(menuitem), MENU_VAL_ID));

	prefs_common.cliplog = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->checkbtn_cliplog));
	prefs_common.loglength = gtk_spin_button_get_value_as_int(
		GTK_SPIN_BUTTON(page->spinbtn_loglength));
	prefs_common.filtering_debug_cliplog = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->checkbtn_debug_cliplog));
	prefs_common.filtering_debug_loglength = gtk_spin_button_get_value_as_int(
		GTK_SPIN_BUTTON(page->spinbtn_debug_loglength));
	filtering_debug_enabled = prefs_common.enable_filtering_debug;
	prefs_common.enable_filtering_debug = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->checkbtn_filteringdebug));
	if (filtering_debug_enabled != prefs_common.enable_filtering_debug) {
		if (prefs_common.enable_filtering_debug)
			log_message(LOG_DEBUG_FILTERING, _("filtering debug enabled\n"));
		else
			log_message(LOG_DEBUG_FILTERING, _("filtering debug disabled\n"));
	}
	prefs_common.enable_filtering_debug_inc = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->checkbtn_filteringdebug_inc));
	prefs_common.enable_filtering_debug_manual = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->checkbtn_filteringdebug_manual));
	prefs_common.enable_filtering_debug_folder_proc = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->checkbtn_filteringdebug_folder_proc));
	prefs_common.enable_filtering_debug_pre_proc = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->checkbtn_filteringdebug_pre_proc));
	prefs_common.enable_filtering_debug_post_proc = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->checkbtn_filteringdebug_post_proc));

	mainwindow = mainwindow_get_mainwindow();
	log_window_set_clipping(mainwindow->logwin, prefs_common.cliplog,
				prefs_common.loglength);
	log_window_set_clipping(mainwindow->filtering_debugwin, prefs_common.filtering_debug_cliplog,
				prefs_common.filtering_debug_loglength);
}

static void prefs_logging_destroy_widget(PrefsPage *_page)
{
}

LoggingPage *prefs_logging;

void prefs_logging_init(void)
{
	LoggingPage *page;
	static gchar *path[3];

	path[0] = _("Other");
	path[1] = _("Logging");
	path[2] = NULL;

	page = g_new0(LoggingPage, 1);
	page->page.path = path;
	page->page.create_widget = prefs_logging_create_widget;
	page->page.destroy_widget = prefs_logging_destroy_widget;
	page->page.save_page = prefs_logging_save;
	page->page.weight = 5.0;
	prefs_gtk_register_page((PrefsPage *) page);
	prefs_logging = page;
}

void prefs_logging_done(void)
{
	prefs_gtk_unregister_page((PrefsPage *) prefs_logging);
	g_free(prefs_logging);
}
