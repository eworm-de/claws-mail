/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2005 Colin Leroy <colin@colino.net> & The Sylpheed-Claws Team
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

typedef struct _WritingPage
{
	PrefsPage page;

	GtkWidget *window;

	GtkWidget *checkbtn_autoextedit;
	GtkWidget *checkbtn_reply_account_autosel;
	GtkWidget *checkbtn_forward_account_autosel;
	GtkWidget *checkbtn_reedit_account_autosel;
	GtkWidget *spinbtn_undolevel;
	GtkWidget *checkbtn_default_reply_list;
	GtkWidget *checkbtn_forward_as_attachment;
	GtkWidget *checkbtn_redirect_keep_from;
	GtkWidget *hbox_autosave;
	GtkWidget *checkbtn_autosave;
	GtkWidget *spinbtn_autosave_length;
} WritingPage;

void prefs_compose_writing_create_widget(PrefsPage *_page, GtkWindow *window, 
			       	  gpointer data)
{
	WritingPage *prefs_writing = (WritingPage *) _page;
	
	GtkWidget *vbox1;
	GtkWidget *vbox2;

	GtkWidget *checkbtn_autoextedit;

	GtkWidget *frame_autosel;
	GtkWidget *hbox_autosel;
	GtkWidget *checkbtn_reply_account_autosel;
	GtkWidget *checkbtn_forward_account_autosel;
	GtkWidget *checkbtn_reedit_account_autosel;

	GtkWidget *hbox_undolevel;
	GtkWidget *label_undolevel;
	GtkObject *spinbtn_undolevel_adj;
	GtkWidget *spinbtn_undolevel;

	GtkWidget *checkbtn_default_reply_list;

	GtkWidget *checkbtn_forward_as_attachment;
	GtkWidget *checkbtn_redirect_keep_from;

	GtkWidget *hbox_autosave;
	GtkWidget *checkbtn_autosave;
	GtkObject *spinbtn_autosave_adj;
	GtkWidget *spinbtn_autosave_length;
	GtkWidget *label_autosave_length;
	
	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

        /* Account autoselection */
	PACK_FRAME(vbox1, frame_autosel, _("Automatic account selection"));

	hbox_autosel = gtk_hbox_new (FALSE, VSPACING_NARROW);
	gtk_widget_show (hbox_autosel);
	gtk_container_add (GTK_CONTAINER (frame_autosel), hbox_autosel);
	gtk_container_set_border_width (GTK_CONTAINER (hbox_autosel), 8);

        PACK_CHECK_BUTTON (hbox_autosel, checkbtn_reply_account_autosel,
			   _("when replying"));
	PACK_CHECK_BUTTON (hbox_autosel, checkbtn_forward_account_autosel,
			   _("when forwarding"));
	PACK_CHECK_BUTTON (hbox_autosel, checkbtn_reedit_account_autosel,
			   _("when re-editing"));

	vbox2 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox2);
	gtk_box_pack_start (GTK_BOX (vbox1), vbox2, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON (vbox2, checkbtn_default_reply_list,
			   _("Reply button invokes mailing list reply"));

	PACK_CHECK_BUTTON (vbox2, checkbtn_autoextedit,
			   _("Automatically launch the external editor"));

	PACK_CHECK_BUTTON (vbox2, checkbtn_forward_as_attachment,
			   _("Forward as attachment"));

	PACK_CHECK_BUTTON (vbox2, checkbtn_redirect_keep_from,
			   _("Keep the original 'From' header when redirecting"));

	hbox_autosave = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox_autosave);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox_autosave, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON (hbox_autosave, checkbtn_autosave,
			   _("Autosave to Drafts folder every"));

	spinbtn_autosave_adj = gtk_adjustment_new (50, 0, 1000, 1, 10, 10);
	spinbtn_autosave_length = gtk_spin_button_new
		(GTK_ADJUSTMENT (spinbtn_autosave_adj), 1, 0);
	gtk_widget_set_size_request (spinbtn_autosave_length, 64, -1);	
	gtk_widget_show (spinbtn_autosave_length);
	gtk_box_pack_start (GTK_BOX (hbox_autosave), spinbtn_autosave_length, FALSE, FALSE, 0);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbtn_autosave_length), TRUE);
	
	label_autosave_length = gtk_label_new(_("characters"));
	gtk_widget_show (label_autosave_length);
	gtk_box_pack_start (GTK_BOX (hbox_autosave), label_autosave_length, FALSE, FALSE, 0);
	
	hbox_undolevel = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox_undolevel);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox_undolevel, FALSE, FALSE, 0);

	label_undolevel = gtk_label_new (_("Undo level"));
	gtk_widget_show (label_undolevel);
	gtk_box_pack_start (GTK_BOX (hbox_undolevel), label_undolevel, FALSE, FALSE, 0);

	spinbtn_undolevel_adj = gtk_adjustment_new (50, 0, 100, 1, 10, 10);
	spinbtn_undolevel = gtk_spin_button_new
		(GTK_ADJUSTMENT (spinbtn_undolevel_adj), 1, 0);
	gtk_widget_show (spinbtn_undolevel);
	gtk_box_pack_start (GTK_BOX (hbox_undolevel), spinbtn_undolevel, FALSE, FALSE, 0);
	gtk_widget_set_size_request (spinbtn_undolevel, 64, -1);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbtn_undolevel), TRUE);

	SET_TOGGLE_SENSITIVITY (checkbtn_autosave, spinbtn_autosave_length);
	SET_TOGGLE_SENSITIVITY (checkbtn_autosave, label_autosave_length);

	prefs_writing->checkbtn_autoextedit = checkbtn_autoextedit;

        prefs_writing->checkbtn_reply_account_autosel   = checkbtn_reply_account_autosel;
	prefs_writing->checkbtn_forward_account_autosel = checkbtn_forward_account_autosel;
	prefs_writing->checkbtn_reedit_account_autosel  = checkbtn_reedit_account_autosel;

	prefs_writing->spinbtn_undolevel     = spinbtn_undolevel;

	prefs_writing->checkbtn_autosave     = checkbtn_autosave;
	prefs_writing->spinbtn_autosave_length = spinbtn_autosave_length;
	
	prefs_writing->checkbtn_forward_as_attachment =
		checkbtn_forward_as_attachment;
	prefs_writing->checkbtn_redirect_keep_from =
		checkbtn_redirect_keep_from;
	prefs_writing->checkbtn_default_reply_list = checkbtn_default_reply_list;

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prefs_writing->checkbtn_autoextedit),
		prefs_common.auto_exteditor);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prefs_writing->checkbtn_forward_as_attachment),
		prefs_common.forward_as_attachment);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prefs_writing->checkbtn_redirect_keep_from),
		prefs_common.redirect_keep_from);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prefs_writing->checkbtn_autosave),
		prefs_common.autosave);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(prefs_writing->spinbtn_autosave_length),
		prefs_common.autosave_length);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(prefs_writing->spinbtn_undolevel),
		prefs_common.undolevels);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prefs_writing->checkbtn_reply_account_autosel),
		prefs_common.reply_account_autosel);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prefs_writing->checkbtn_forward_account_autosel),
		prefs_common.forward_account_autosel);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prefs_writing->checkbtn_reedit_account_autosel),
		prefs_common.reedit_account_autosel);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prefs_writing->checkbtn_default_reply_list),
		prefs_common.default_reply_list);

	prefs_writing->page.widget = vbox1;
}

void prefs_compose_writing_save(PrefsPage *_page)
{
	WritingPage *page = (WritingPage *) _page;
	prefs_common.auto_exteditor = 
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->checkbtn_autoextedit));
	prefs_common.forward_as_attachment =
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->checkbtn_forward_as_attachment));
	prefs_common.redirect_keep_from =
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->checkbtn_redirect_keep_from));
	prefs_common.autosave = 
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->checkbtn_autosave));
	prefs_common.autosave_length =
		gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(page->spinbtn_autosave_length));
	prefs_common.undolevels = 
		gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(page->spinbtn_undolevel));
		
	prefs_common.reply_account_autosel =
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->checkbtn_reply_account_autosel));
	prefs_common.forward_account_autosel =
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->checkbtn_forward_account_autosel));
	prefs_common.reedit_account_autosel =
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->checkbtn_reedit_account_autosel));
	prefs_common.default_reply_list =
		gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->checkbtn_default_reply_list));
	
}

static void prefs_compose_writing_destroy_widget(PrefsPage *_page)
{
}

WritingPage *prefs_writing;

void prefs_compose_writing_init(void)
{
	WritingPage *page;
	static gchar *path[3];

	path[0] = _("Compose");
	path[1] = _("Writing");
	path[2] = NULL;

	page = g_new0(WritingPage, 1);
	page->page.path = path;
	page->page.create_widget = prefs_compose_writing_create_widget;
	page->page.destroy_widget = prefs_compose_writing_destroy_widget;
	page->page.save_page = prefs_compose_writing_save;
	page->page.weight = 190.0;
	prefs_gtk_register_page((PrefsPage *) page);
	prefs_writing = page;
}

void prefs_compose_writing_done(void)
{
	prefs_gtk_unregister_page((PrefsPage *) prefs_writing);
	g_free(prefs_writing);
}
