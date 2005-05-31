/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2004 Hiroyuki Yamamoto & The Sylpheed-Claws Team
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
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "prefs_common.h"
#include "prefs_gtk.h"
#include "prefs_display_header.h"

#include "gtk/gtkutils.h"
#include "gtk/prefswindow.h"

#include "manage_window.h"

typedef struct _MessagePage
{
	PrefsPage page;

	GtkWidget *window;

	GtkWidget *chkbtn_mbalnum;
	GtkWidget *chkbtn_disphdrpane;
	GtkWidget *chkbtn_disphdr;
	GtkWidget *chkbtn_html;
	GtkWidget *chkbtn_cursor;
	GtkWidget *spinbtn_linespc;

	GtkWidget *chkbtn_smoothscroll;
	GtkWidget *spinbtn_scrollstep;
	GtkWidget *chkbtn_halfpage;

	GtkWidget *chkbtn_attach_desc;
} MessagePage;

void prefs_message_create_widget(PrefsPage *_page, GtkWindow *window, 
			       	  gpointer data)
{
	MessagePage *prefs_message = (MessagePage *) _page;
	
	GtkWidget *vbox1;
	GtkWidget *vbox2;
	GtkWidget *vbox3;
	GtkWidget *hbox1;
	GtkWidget *chkbtn_mbalnum;
	GtkWidget *chkbtn_disphdrpane;
	GtkWidget *chkbtn_disphdr;
	GtkWidget *button_edit_disphdr;
	GtkWidget *chkbtn_html;
	GtkWidget *chkbtn_cursor;
	GtkWidget *hbox_linespc;
	GtkWidget *label_linespc;
	GtkObject *spinbtn_linespc_adj;
	GtkWidget *spinbtn_linespc;

	GtkWidget *frame_scr;
	GtkWidget *vbox_scr;
	GtkWidget *chkbtn_smoothscroll;
	GtkWidget *hbox_scr;
	GtkWidget *label_scr;
	GtkObject *spinbtn_scrollstep_adj;
	GtkWidget *spinbtn_scrollstep;
	GtkWidget *chkbtn_halfpage;

	GtkWidget *chkbtn_attach_desc;

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

	vbox2 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox2);
	gtk_box_pack_start (GTK_BOX (vbox1), vbox2, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON
		(vbox2, chkbtn_mbalnum,
		 _("Display multi-byte alphanumeric as\n"
		   "ASCII character (Japanese only)"));
	gtk_label_set_justify (GTK_LABEL (GTK_BIN(chkbtn_mbalnum)->child),
			       GTK_JUSTIFY_LEFT);

	PACK_CHECK_BUTTON(vbox2, chkbtn_disphdrpane,
			  _("Display header pane above message view"));

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox1, FALSE, TRUE, 0);

	PACK_CHECK_BUTTON(hbox1, chkbtn_disphdr,
			  _("Display short headers on message view"));

	button_edit_disphdr = gtk_button_new_with_label (_(" Edit... "));
	gtk_widget_show (button_edit_disphdr);
	gtk_box_pack_end (GTK_BOX (hbox1), button_edit_disphdr,
			  FALSE, TRUE, 0);
	g_signal_connect (G_OBJECT (button_edit_disphdr), "clicked",
			  G_CALLBACK (prefs_display_header_open),
			  NULL);

	SET_TOGGLE_SENSITIVITY(chkbtn_disphdr, button_edit_disphdr);

	PACK_CHECK_BUTTON(vbox2, chkbtn_html,
			  _("Render HTML messages as text"));

	PACK_CHECK_BUTTON(vbox2, chkbtn_cursor,
			  _("Display cursor in message view"));

	PACK_VSPACER(vbox2, vbox3, VSPACING_NARROW_2);

	hbox1 = gtk_hbox_new (FALSE, 32);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox2), hbox1, FALSE, TRUE, 0);

	hbox_linespc = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (hbox1), hbox_linespc, FALSE, TRUE, 0);

	label_linespc = gtk_label_new (_("Line space"));
	gtk_widget_show (label_linespc);
	gtk_box_pack_start (GTK_BOX (hbox_linespc), label_linespc,
			    FALSE, FALSE, 0);

	spinbtn_linespc_adj = gtk_adjustment_new (2, 0, 16, 1, 1, 16);
	spinbtn_linespc = gtk_spin_button_new
		(GTK_ADJUSTMENT (spinbtn_linespc_adj), 1, 0);
	gtk_widget_show (spinbtn_linespc);
	gtk_box_pack_start (GTK_BOX (hbox_linespc), spinbtn_linespc,
			    FALSE, FALSE, 0);
	gtk_widget_set_size_request (spinbtn_linespc, 64, -1);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbtn_linespc), TRUE);

	label_linespc = gtk_label_new (_("pixel(s)"));
	gtk_widget_show (label_linespc);
	gtk_box_pack_start (GTK_BOX (hbox_linespc), label_linespc,
			    FALSE, FALSE, 0);

	PACK_FRAME(vbox1, frame_scr, _("Scroll"));

	vbox_scr = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox_scr);
	gtk_container_add (GTK_CONTAINER (frame_scr), vbox_scr);
	gtk_container_set_border_width (GTK_CONTAINER (vbox_scr), 8);

	PACK_CHECK_BUTTON(vbox_scr, chkbtn_halfpage, _("Half page"));

	hbox1 = gtk_hbox_new (FALSE, 32);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox_scr), hbox1, FALSE, TRUE, 0);

	PACK_CHECK_BUTTON(hbox1, chkbtn_smoothscroll, _("Smooth scroll"));

	hbox_scr = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox_scr);
	gtk_box_pack_start (GTK_BOX (hbox1), hbox_scr, FALSE, FALSE, 0);

	label_scr = gtk_label_new (_("Step"));
	gtk_widget_show (label_scr);
	gtk_box_pack_start (GTK_BOX (hbox_scr), label_scr, FALSE, FALSE, 0);

	spinbtn_scrollstep_adj = gtk_adjustment_new (1, 1, 100, 1, 10, 10);
	spinbtn_scrollstep = gtk_spin_button_new
		(GTK_ADJUSTMENT (spinbtn_scrollstep_adj), 1, 0);
	gtk_widget_show (spinbtn_scrollstep);
	gtk_box_pack_start (GTK_BOX (hbox_scr), spinbtn_scrollstep,
			    FALSE, FALSE, 0);
	gtk_widget_set_size_request (spinbtn_scrollstep, 64, -1);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spinbtn_scrollstep),
				     TRUE);

	label_scr = gtk_label_new (_("pixel(s)"));
	gtk_widget_show (label_scr);
	gtk_box_pack_start (GTK_BOX (hbox_scr), label_scr, FALSE, FALSE, 0);

	SET_TOGGLE_SENSITIVITY (chkbtn_smoothscroll, hbox_scr)

	vbox3 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox3);
	gtk_box_pack_start (GTK_BOX (vbox1), vbox3, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON(vbox3, chkbtn_attach_desc,
			  _("Show attachment descriptions (rather than names)"));

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chkbtn_mbalnum),
		prefs_common.conv_mb_alnum);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chkbtn_disphdrpane),
		prefs_common.display_header_pane);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chkbtn_disphdr),
		prefs_common.display_header);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chkbtn_html),
		prefs_common.render_html);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chkbtn_cursor),
		prefs_common.textview_cursor_visible);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chkbtn_smoothscroll),
		prefs_common.enable_smooth_scroll);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chkbtn_halfpage),
		prefs_common.scroll_halfpage);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chkbtn_attach_desc),
		prefs_common.attach_desc);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinbtn_linespc),
		prefs_common.line_space);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinbtn_scrollstep),
		prefs_common.scroll_step);
		
	prefs_message->window = GTK_WIDGET(window);
	prefs_message->chkbtn_mbalnum = chkbtn_mbalnum;
	prefs_message->chkbtn_disphdrpane = chkbtn_disphdrpane;
	prefs_message->chkbtn_disphdr = chkbtn_disphdr;
	prefs_message->chkbtn_html = chkbtn_html;
	prefs_message->chkbtn_cursor = chkbtn_cursor;
	prefs_message->spinbtn_linespc = spinbtn_linespc;
	prefs_message->chkbtn_smoothscroll = chkbtn_smoothscroll;
	prefs_message->spinbtn_scrollstep = spinbtn_scrollstep;
	prefs_message->chkbtn_halfpage = chkbtn_halfpage;
	prefs_message->chkbtn_attach_desc = chkbtn_attach_desc;
	
	prefs_message->page.widget = vbox1;
}

void prefs_message_save(PrefsPage *_page)
{
	MessagePage *page = (MessagePage *) _page;

	prefs_common.conv_mb_alnum = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->chkbtn_mbalnum));
	prefs_common.display_header_pane = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->chkbtn_disphdrpane));
	prefs_common.display_header = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->chkbtn_disphdr));
	prefs_common.render_html = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->chkbtn_html));
	prefs_common.textview_cursor_visible = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->chkbtn_cursor));
	prefs_common.enable_smooth_scroll = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->chkbtn_smoothscroll));
	prefs_common.scroll_halfpage = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->chkbtn_halfpage));
	prefs_common.attach_desc = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->chkbtn_attach_desc));
	prefs_common.line_space = gtk_spin_button_get_value_as_int(
		GTK_SPIN_BUTTON(page->spinbtn_linespc));
	prefs_common.scroll_step = gtk_spin_button_get_value_as_int(
		GTK_SPIN_BUTTON(page->spinbtn_scrollstep));
}

static void prefs_message_destroy_widget(PrefsPage *_page)
{
}

MessagePage *prefs_message;

void prefs_message_init(void)
{
	MessagePage *page;
	static gchar *path[3];

	path[0] = _("Message View");
	path[1] = _("Text options");
	path[2] = NULL;

	page = g_new0(MessagePage, 1);
	page->page.path = path;
	page->page.create_widget = prefs_message_create_widget;
	page->page.destroy_widget = prefs_message_destroy_widget;
	page->page.save_page = prefs_message_save;
	page->page.weight = 170.0;
	prefs_gtk_register_page((PrefsPage *) page);
	prefs_message = page;
}

void prefs_message_done(void)
{
	prefs_gtk_unregister_page((PrefsPage *) prefs_message);
	g_free(prefs_message);
}
