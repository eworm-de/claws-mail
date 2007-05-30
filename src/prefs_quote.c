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
#include "prefs_template.h"
#include "alertpanel.h"

#include "gtk/gtkutils.h"
#include "gtk/prefswindow.h"

#include "manage_window.h"
#include "quote_fmt.h"

typedef struct _QuotePage
{
	PrefsPage page;

	GtkWidget *window;

	GtkWidget *entry_quotemark;
	GtkWidget *text_quotefmt;
	GtkWidget *entry_fw_quotemark;
	GtkWidget *text_fw_quotefmt;
	GtkWidget *entry_quote_chars;
	GtkWidget *label_quote_chars;
	GtkWidget *btn_quotedesc;
	GtkWidget *checkbtn_reply_with_quote;
} QuotePage;

static void prefs_quote_create_widget(PrefsPage *_page, GtkWindow *window, 
			       	  gpointer data)
{
	QuotePage *prefs_quote = (QuotePage *) _page;
	
	GtkWidget *vbox1;
	GtkWidget *frame_quote;
	GtkWidget *vbox_quote;
	GtkWidget *hbox1;
	GtkWidget *hbox2;

	GtkWidget *entry_quote_chars;
	GtkWidget *label_quote_chars;
	
	GtkWidget *checkbtn_reply_with_quote;

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

	PACK_CHECK_BUTTON (vbox1, checkbtn_reply_with_quote, _("Reply will quote by default"));

	/* reply */

	quotefmt_create_reply_fmt_widgets(
				window,
				vbox1,
				NULL,
				NULL,
				&prefs_quote->entry_quotemark,
				&prefs_quote->text_quotefmt,
				FALSE);

	/* forward */

	quotefmt_create_forward_fmt_widgets(
				window,
				vbox1,
				NULL,
				NULL,
				&prefs_quote->entry_fw_quotemark,
				&prefs_quote->text_fw_quotefmt,
				FALSE);

	/* info button */

	quotefmt_add_info_button(window, vbox1);

	/* quote chars */

	PACK_FRAME (vbox1, frame_quote, _("Quotation characters"));

	vbox_quote = gtk_vbox_new (FALSE, VSPACING_NARROW);
	gtk_widget_show (vbox_quote);
	gtk_container_add (GTK_CONTAINER (frame_quote), vbox_quote);
	gtk_container_set_border_width (GTK_CONTAINER (vbox_quote), 8);

	hbox1 = gtk_hbox_new (FALSE, 32);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox_quote), hbox1, FALSE, FALSE, 0);

	hbox2 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox2);
	gtk_box_pack_start (GTK_BOX (hbox1), hbox2, FALSE, FALSE, 0);

	label_quote_chars = gtk_label_new (_("Treat these characters as quotation marks: "));
	gtk_widget_show (label_quote_chars);
	gtk_box_pack_start (GTK_BOX (hbox2), label_quote_chars, FALSE, FALSE, 0);

	entry_quote_chars = gtk_entry_new ();
	gtk_widget_show (entry_quote_chars);
	gtk_box_pack_start (GTK_BOX (hbox2), entry_quote_chars,
			    FALSE, FALSE, 0);
	gtk_widget_set_size_request (entry_quote_chars, 64, -1);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(prefs_quote->checkbtn_reply_with_quote),
			prefs_common.reply_with_quote);
	gtk_entry_set_text(GTK_ENTRY(prefs_quote->entry_quotemark), 
			prefs_common.quotemark?prefs_common.quotemark:"");
	pref_set_textview_from_pref(GTK_TEXT_VIEW(prefs_quote->text_quotefmt),
			prefs_common.quotefmt);

	pref_set_textview_from_pref(GTK_TEXT_VIEW(prefs_quote->text_fw_quotefmt),
			prefs_common.fw_quotefmt);
	gtk_entry_set_text(GTK_ENTRY(prefs_quote->entry_fw_quotemark), 
			prefs_common.fw_quotemark?prefs_common.fw_quotemark:"");

	gtk_entry_set_text(GTK_ENTRY(entry_quote_chars), 
			prefs_common.quote_chars?prefs_common.quote_chars:"");
	
	prefs_quote->window		= GTK_WIDGET(window);
	prefs_quote->entry_quote_chars	= entry_quote_chars;
	prefs_quote->checkbtn_reply_with_quote
					= checkbtn_reply_with_quote;
	prefs_quote->page.widget = vbox1;
}

static void prefs_quote_save(PrefsPage *_page)
{
	QuotePage *page = (QuotePage *) _page;
	
	g_free(prefs_common.quotefmt); 
	prefs_common.quotefmt = NULL;
	g_free(prefs_common.fw_quotefmt); 
	prefs_common.fw_quotefmt = NULL;
	g_free(prefs_common.quotemark); 
	prefs_common.quotemark = NULL;
	g_free(prefs_common.fw_quotemark); 
	prefs_common.fw_quotemark = NULL;
	g_free(prefs_common.quote_chars); 
	prefs_common.quote_chars = NULL;
	
	prefs_common.quotefmt = pref_get_pref_from_textview(
			GTK_TEXT_VIEW(page->text_quotefmt));
	quotefmt_check_reply_formats(TRUE, prefs_common.quotefmt);

	prefs_common.fw_quotefmt = pref_get_pref_from_textview(
			GTK_TEXT_VIEW(page->text_fw_quotefmt));
	quotefmt_check_forward_formats(TRUE, prefs_common.fw_quotefmt);

	prefs_common.quotemark = gtk_editable_get_chars(
			GTK_EDITABLE(page->entry_quotemark), 0, -1);
	prefs_common.fw_quotemark = gtk_editable_get_chars(
			GTK_EDITABLE(page->entry_fw_quotemark), 0, -1);
	prefs_common.quote_chars = gtk_editable_get_chars(
			GTK_EDITABLE(page->entry_quote_chars), 0, -1);
	prefs_common.reply_with_quote = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(page->checkbtn_reply_with_quote));

	remove_space(prefs_common.quote_chars);
}

static void prefs_quote_destroy_widget(PrefsPage *_page)
{
}

QuotePage *prefs_quote;

void prefs_quote_init(void)
{
	QuotePage *page;
	static gchar *path[3];

	path[0] = _("Compose");
	path[1] = _("Quoting");
	path[2] = NULL;

	page = g_new0(QuotePage, 1);
	page->page.path = path;
	page->page.create_widget = prefs_quote_create_widget;
	page->page.destroy_widget = prefs_quote_destroy_widget;
	page->page.save_page = prefs_quote_save;
	page->page.weight = 185.0;
	prefs_gtk_register_page((PrefsPage *) page);
	prefs_quote = page;
}

void prefs_quote_done(void)
{
	prefs_gtk_unregister_page((PrefsPage *) prefs_quote);
	g_free(prefs_quote);
}
