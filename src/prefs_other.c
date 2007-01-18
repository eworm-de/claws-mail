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
#include <gdk/gdkkeysyms.h>

#include "prefs_common.h"
#include "prefs_gtk.h"

#include "gtk/gtkutils.h"
#include "gtk/prefswindow.h"

#include "manage_window.h"
#ifdef HAVE_LIBETPAN
#include "imap-thread.h"
#endif

typedef struct _OtherPage
{
	PrefsPage page;

	GtkWidget *window;

	GtkWidget *checkbtn_addaddrbyclick;
	GtkWidget *checkbtn_confonexit;
	GtkWidget *checkbtn_cleanonexit;
	GtkWidget *checkbtn_askonclean;
	GtkWidget *checkbtn_warnqueued;
        GtkWidget *checkbtn_cliplog;
	GtkWidget *spinbtn_loglength;
	GtkWidget *spinbtn_iotimeout;
	GtkWidget *chkbtn_never_send_retrcpt;
	GtkWidget *chkbtn_gtk_can_change_accels;
} OtherPage;

void prefs_other_create_widget(PrefsPage *_page, GtkWindow *window, 
			       	  gpointer data)
{
	OtherPage *prefs_other = (OtherPage *) _page;
	
	GtkWidget *vbox1;
	GtkWidget *hbox1;

	GtkWidget *frame_addr;
	GtkWidget *vbox_addr;
	GtkWidget *checkbtn_addaddrbyclick;
	
	GtkWidget *frame_cliplog;
	GtkWidget *vbox_cliplog;
	GtkWidget *hbox_cliplog;
	GtkWidget *checkbtn_cliplog;
	GtkWidget *loglength_label;
	GtkWidget *spinbtn_loglength;
	GtkObject *spinbtn_loglength_adj;
	GtkTooltips *loglength_tooltip;
	GtkWidget *label;

	GtkWidget *frame_exit;
	GtkWidget *vbox_exit;
	GtkWidget *checkbtn_confonexit;
	GtkWidget *checkbtn_cleanonexit;
	GtkWidget *checkbtn_askonclean;
	GtkWidget *checkbtn_warnqueued;

	GtkWidget *label_iotimeout;
	GtkWidget *spinbtn_iotimeout;
	GtkObject *spinbtn_iotimeout_adj;

	GtkWidget *chkbtn_never_send_retrcpt;
	GtkWidget *chkbtn_gtk_can_change_accels;
	GtkTooltips *gtk_can_change_accels_tooltip;

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

	vbox_addr = gtkut_get_options_frame(vbox1, &frame_addr, _("Address book"));

	PACK_CHECK_BUTTON
		(vbox_addr, checkbtn_addaddrbyclick,
		 _("Add address to destination when double-clicked"));

	/* Clip Log */
	vbox_cliplog = gtkut_get_options_frame(vbox1, &frame_cliplog, _("Log Size"));

	PACK_CHECK_BUTTON (vbox_cliplog, checkbtn_cliplog,
			   _("Clip the log size"));
	hbox_cliplog = gtk_hbox_new (FALSE, 8);
	gtk_container_add (GTK_CONTAINER (vbox_cliplog), hbox_cliplog);
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

	/* On Exit */
	vbox_exit = gtkut_get_options_frame(vbox1, &frame_exit, _("On exit"));

	PACK_CHECK_BUTTON (vbox_exit, checkbtn_confonexit,
			   _("Confirm on exit"));

	hbox1 = gtk_hbox_new (FALSE, 32);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox_exit), hbox1, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON (hbox1, checkbtn_cleanonexit,
			   _("Empty trash on exit"));
	PACK_CHECK_BUTTON (hbox1, checkbtn_askonclean,
			   _("Ask before emptying"));
	SET_TOGGLE_SENSITIVITY (checkbtn_cleanonexit, checkbtn_askonclean);

	PACK_CHECK_BUTTON (vbox_exit, checkbtn_warnqueued,
			   _("Warn if there are queued messages"));

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, FALSE, 0);

	label_iotimeout = gtk_label_new (_("Socket I/O timeout"));
	gtk_widget_show (label_iotimeout);
	gtk_box_pack_start (GTK_BOX (hbox1), label_iotimeout, FALSE, FALSE, 0);

	spinbtn_iotimeout_adj = gtk_adjustment_new (60, 0, 1000, 1, 10, 10);
	spinbtn_iotimeout = gtk_spin_button_new
		(GTK_ADJUSTMENT (spinbtn_iotimeout_adj), 1, 0);
	gtk_widget_show (spinbtn_iotimeout);
	gtk_box_pack_start (GTK_BOX (hbox1), spinbtn_iotimeout,
			    FALSE, FALSE, 0);
	gtk_widget_set_size_request (spinbtn_iotimeout, 64, -1);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbtn_iotimeout), TRUE);

	label_iotimeout = gtk_label_new (_("seconds"));
	gtk_widget_show (label_iotimeout);
	gtk_box_pack_start (GTK_BOX (hbox1), label_iotimeout, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON(vbox1, chkbtn_never_send_retrcpt,
			  _("Never send Return Receipts"));

	PACK_CHECK_BUTTON(vbox1, chkbtn_gtk_can_change_accels,
			_("Enable customisable menu shortcuts"));

	gtk_can_change_accels_tooltip = gtk_tooltips_new();
	gtk_tooltips_set_tip(GTK_TOOLTIPS(gtk_can_change_accels_tooltip),
			chkbtn_gtk_can_change_accels,
			_("If checked, you can change the keyboard shortcuts of "
				"most of the menu items by focusing on the menu "
				"item and pressing a key combination.\n"
				"Uncheck this option if you want to lock all "
				"existing menu shortcuts."),
			NULL);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_addaddrbyclick), 
		prefs_common.add_address_by_click);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_confonexit), 
		prefs_common.confirm_on_exit);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_cleanonexit), 
		prefs_common.clean_on_exit);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_askonclean), 
		prefs_common.ask_on_clean);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_warnqueued), 
		prefs_common.warn_queued_on_exit);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_cliplog), 
		prefs_common.cliplog);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chkbtn_never_send_retrcpt),
		prefs_common.never_send_retrcpt);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chkbtn_gtk_can_change_accels),
		prefs_common.gtk_can_change_accels);

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinbtn_loglength),
		prefs_common.loglength);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinbtn_iotimeout),
		prefs_common.io_timeout_secs);

	prefs_other->checkbtn_addaddrbyclick = checkbtn_addaddrbyclick;
	prefs_other->checkbtn_confonexit = checkbtn_confonexit;
	prefs_other->checkbtn_cleanonexit = checkbtn_cleanonexit;
	prefs_other->checkbtn_askonclean = checkbtn_askonclean;
	prefs_other->checkbtn_warnqueued = checkbtn_warnqueued;
	prefs_other->checkbtn_cliplog = checkbtn_cliplog;
	prefs_other->spinbtn_loglength = spinbtn_loglength;
	prefs_other->spinbtn_iotimeout = spinbtn_iotimeout;
	prefs_other->chkbtn_never_send_retrcpt = chkbtn_never_send_retrcpt;
	prefs_other->chkbtn_gtk_can_change_accels = chkbtn_gtk_can_change_accels;

	prefs_other->page.widget = vbox1;
}

void prefs_other_save(PrefsPage *_page)
{
	OtherPage *page = (OtherPage *) _page;
	MainWindow *mainwindow;
	gboolean gtk_can_change_accels;

	prefs_common.add_address_by_click = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->checkbtn_addaddrbyclick));
	prefs_common.confirm_on_exit = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->checkbtn_confonexit));
	prefs_common.clean_on_exit = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->checkbtn_cleanonexit)); 
	prefs_common.ask_on_clean = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->checkbtn_askonclean));
	prefs_common.warn_queued_on_exit = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->checkbtn_warnqueued)); 
	prefs_common.cliplog = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->checkbtn_cliplog));
	prefs_common.loglength = gtk_spin_button_get_value_as_int(
		GTK_SPIN_BUTTON(page->spinbtn_loglength));
	prefs_common.io_timeout_secs = gtk_spin_button_get_value_as_int(
		GTK_SPIN_BUTTON(page->spinbtn_iotimeout));
	sock_set_io_timeout(prefs_common.io_timeout_secs);
#ifdef HAVE_LIBETPAN
	imap_main_set_timeout(prefs_common.io_timeout_secs);
#endif
	prefs_common.never_send_retrcpt = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->chkbtn_never_send_retrcpt));

	mainwindow = mainwindow_get_mainwindow();
	log_window_set_clipping(mainwindow->logwin, prefs_common.cliplog,
				prefs_common.loglength);

	gtk_can_change_accels = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->chkbtn_gtk_can_change_accels));

	if (prefs_common.gtk_can_change_accels != gtk_can_change_accels) {

		prefs_common.gtk_can_change_accels = gtk_can_change_accels;

		gtk_settings_set_long_property(gtk_settings_get_default(),
				"gtk-can-change-accels",
				(glong)prefs_common.gtk_can_change_accels,
				"XProperty");

		/* gtk_can_change_accels value changed : we have (only if changed)
		 * to apply the gtk property to all widgets : */
		gtk_rc_reparse_all_for_settings(gtk_settings_get_default(), TRUE);
	}
}

static void prefs_other_destroy_widget(PrefsPage *_page)
{
}

OtherPage *prefs_other;

void prefs_other_init(void)
{
	OtherPage *page;
	static gchar *path[2];

	path[0] = _("Other");
	path[1] = NULL;

	page = g_new0(OtherPage, 1);
	page->page.path = path;
	page->page.create_widget = prefs_other_create_widget;
	page->page.destroy_widget = prefs_other_destroy_widget;
	page->page.save_page = prefs_other_save;
	page->page.weight = 5.0;
	prefs_gtk_register_page((PrefsPage *) page);
	prefs_other = page;

	gtk_settings_set_long_property(gtk_settings_get_default(),
			"gtk-can-change-accels",
			(glong)prefs_common.gtk_can_change_accels,
			"XProperty");
}

void prefs_other_done(void)
{
	prefs_gtk_unregister_page((PrefsPage *) prefs_other);
	g_free(prefs_other);
}
