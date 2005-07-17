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
#include "gtk/menu.h"

#include "gtk/gtkutils.h"
#include "gtk/prefswindow.h"

#include "manage_window.h"

typedef struct _SendPage
{
	PrefsPage page;

	GtkWidget *window;

	GtkWidget *checkbtn_savemsg;
	GtkWidget *checkbtn_confirm_send_queued_messages;
	GtkWidget *optmenu_senddialog;
	GtkWidget *optmenu_charset;
	GtkWidget *optmenu_encoding_method;
} SendPage;

static gchar * prefs_common_charset_set_data_from_optmenu(GtkWidget *widget)
{
	GtkWidget *menu;
	GtkWidget *menuitem;
	gchar *charset;

	menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(widget));
	menuitem = gtk_menu_get_active(GTK_MENU(menu));
	charset = g_object_get_data(G_OBJECT(menuitem), MENU_VAL_ID);
	return g_strdup(charset);
}

static void prefs_common_charset_set_optmenu(GtkWidget *widget, gchar *data)
{
	GtkOptionMenu *optmenu = GTK_OPTION_MENU(widget);
	gint index;

	g_return_if_fail(optmenu != NULL);
	g_return_if_fail(data != NULL);

	index = menu_find_option_menu_index(optmenu, data,
					    (GCompareFunc)strcmp2);
	if (index >= 0)
		gtk_option_menu_set_history(optmenu, index);
	else
		gtk_option_menu_set_history(optmenu, 0);
}

static TransferEncodingMethod prefs_common_encoding_set_data_from_optmenu(GtkWidget *widget)
{
	GtkWidget *menu;
	GtkWidget *menuitem;

	menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(widget));
	menuitem = gtk_menu_get_active(GTK_MENU(menu));
	return GPOINTER_TO_INT
		(g_object_get_data(G_OBJECT(menuitem), MENU_VAL_ID));
}

static void prefs_common_encoding_set_optmenu(GtkWidget *widget, TransferEncodingMethod method)
{
	GtkOptionMenu *optmenu = GTK_OPTION_MENU(widget);
	gint index;

	g_return_if_fail(optmenu != NULL);

	index = menu_find_option_menu_index(optmenu, GINT_TO_POINTER(method),
					    NULL);
	if (index >= 0)
		gtk_option_menu_set_history(optmenu, index);
	else
		gtk_option_menu_set_history(optmenu, 0);
}


void prefs_send_create_widget(PrefsPage *_page, GtkWindow *window, 
			       	  gpointer data)
{
	SendPage *prefs_send = (SendPage *) _page;
	
	GtkWidget *vbox1;
	GtkWidget *vbox2;
	GtkWidget *hbox1;
	GtkWidget *checkbtn_savemsg;
	GtkWidget *label_outcharset;
	GtkWidget *optmenu_charset;
	GtkWidget *optmenu_menu;
	GtkWidget *menuitem;
	GtkTooltips *charset_tooltip;
	GtkWidget *optmenu_encoding;
	GtkWidget *label_encoding;
	GtkTooltips *encoding_tooltip;
	GtkWidget *label_senddialog;
	GtkWidget *menu;
	GtkWidget *optmenu_senddialog;
	GtkWidget *hbox_senddialog;
	GtkWidget *checkbtn_confirm_send_queued_messages;

	vbox1 = gtk_vbox_new (FALSE, VSPACING);
	gtk_widget_show (vbox1);
	gtk_container_set_border_width (GTK_CONTAINER (vbox1), VBOX_BORDER);

	vbox2 = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (vbox2);
	gtk_box_pack_start (GTK_BOX (vbox1), vbox2, FALSE, FALSE, 0);

	PACK_CHECK_BUTTON (vbox2, checkbtn_savemsg,
			   _("Save sent messages to Sent folder"));

	PACK_CHECK_BUTTON
		(vbox2, checkbtn_confirm_send_queued_messages,
		 _("Confirm before sending queued messages"));

	hbox_senddialog = gtk_hbox_new (FALSE, 8);
	gtk_widget_show(hbox_senddialog);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox_senddialog, FALSE, FALSE, 0);

	label_senddialog = gtk_label_new (_("Show send dialog"));
	gtk_widget_show (label_senddialog);
	gtk_box_pack_start (GTK_BOX (hbox_senddialog), label_senddialog, FALSE, FALSE, 0);

	optmenu_senddialog = gtk_option_menu_new ();
	gtk_widget_show (optmenu_senddialog);
	gtk_box_pack_start (GTK_BOX (hbox_senddialog), optmenu_senddialog, FALSE, FALSE, 0);
	
	menu = gtk_menu_new ();
	MENUITEM_ADD (menu, menuitem, _("Always"), SEND_DIALOG_ALWAYS);
	MENUITEM_ADD (menu, menuitem, _("Never"), SEND_DIALOG_NEVER);

	gtk_option_menu_set_menu (GTK_OPTION_MENU (optmenu_senddialog), menu);

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, FALSE, 0);

	label_outcharset = gtk_label_new (_("Outgoing encoding"));
	gtk_widget_show (label_outcharset);
	gtk_box_pack_start (GTK_BOX (hbox1), label_outcharset, FALSE, FALSE, 0);

	charset_tooltip = gtk_tooltips_new();

	optmenu_charset = gtk_option_menu_new ();
	gtk_widget_show (optmenu_charset);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(charset_tooltip), optmenu_charset,
			     _("If 'Automatic' is selected, the optimal encoding"
		   	       " for the current locale will be used"),
			     NULL);
 	gtk_box_pack_start (GTK_BOX (hbox1), optmenu_charset, FALSE, FALSE, 0);

	optmenu_menu = gtk_menu_new ();

#define SET_MENUITEM(str, data) \
{ \
	MENUITEM_ADD(optmenu_menu, menuitem, str, data); \
}

	SET_MENUITEM(_("Automatic (Recommended)"),	 CS_AUTO);
	SET_MENUITEM(NULL, NULL);
	SET_MENUITEM(_("7bit ascii (US-ASCII)"),	 CS_US_ASCII);
	SET_MENUITEM(NULL, NULL);
	SET_MENUITEM(_("Unicode (UTF-8)"),		 CS_UTF_8);
	SET_MENUITEM(NULL, NULL);
	SET_MENUITEM(_("Western European (ISO-8859-1)"),  CS_ISO_8859_1);
	SET_MENUITEM(_("Western European (ISO-8859-15)"), CS_ISO_8859_15);
	SET_MENUITEM(NULL, NULL);
	SET_MENUITEM(_("Central European (ISO-8859-2)"), CS_ISO_8859_2);
	SET_MENUITEM(NULL, NULL);
	SET_MENUITEM(_("Baltic (ISO-8859-13)"),		 CS_ISO_8859_13);
	SET_MENUITEM(_("Baltic (ISO-8859-4)"),		 CS_ISO_8859_4);
	SET_MENUITEM(NULL, NULL);
	SET_MENUITEM(_("Greek (ISO-8859-7)"),		 CS_ISO_8859_7);
	SET_MENUITEM(NULL, NULL);
	SET_MENUITEM(_("Hebrew (ISO-8859-8)"),		 CS_ISO_8859_8);
	SET_MENUITEM(_("Hebrew (Windows-1255)"),	 CS_WINDOWS_1255);
	SET_MENUITEM(NULL, NULL);
	SET_MENUITEM(_("Turkish (ISO-8859-9)"),		 CS_ISO_8859_9);
	SET_MENUITEM(NULL, NULL);
	SET_MENUITEM(_("Cyrillic (ISO-8859-5)"),	 CS_ISO_8859_5);
	SET_MENUITEM(_("Cyrillic (KOI8-R)"),		 CS_KOI8_R);
	SET_MENUITEM(_("Cyrillic (KOI8-U)"),		 CS_KOI8_U);
	SET_MENUITEM(_("Cyrillic (Windows-1251)"),	 CS_WINDOWS_1251);
	SET_MENUITEM(NULL, NULL);
	SET_MENUITEM(_("Japanese (ISO-2022-JP)"),	 CS_ISO_2022_JP);
#if 0
	SET_MENUITEM(_("Japanese (EUC-JP)"),		 CS_EUC_JP);
	SET_MENUITEM(_("Japanese (Shift_JIS)"),		 CS_SHIFT_JIS);
#endif /* 0 */
	SET_MENUITEM(NULL, NULL);
	SET_MENUITEM(_("Simplified Chinese (GB2312)"),	 CS_GB2312);
	SET_MENUITEM(_("Simplified Chinese (GBK)"),	 CS_GBK);
	SET_MENUITEM(_("Traditional Chinese (Big5)"),	 CS_BIG5);
#if 0
	SET_MENUITEM(_("Traditional Chinese (EUC-TW)"),  CS_EUC_TW);
	SET_MENUITEM(_("Chinese (ISO-2022-CN)"),	 CS_ISO_2022_CN);
#endif /* 0 */
	SET_MENUITEM(NULL, NULL);
	SET_MENUITEM(_("Korean (EUC-KR)"),		 CS_EUC_KR);
	SET_MENUITEM(NULL, NULL);
	SET_MENUITEM(_("Thai (TIS-620)"),		 CS_TIS_620);
	SET_MENUITEM(_("Thai (Windows-874)"),		 CS_WINDOWS_874);

	gtk_option_menu_set_menu (GTK_OPTION_MENU (optmenu_charset),
				  optmenu_menu);

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, FALSE, 0);

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, FALSE, 0);

	label_encoding = gtk_label_new (_("Transfer encoding"));
	gtk_widget_show (label_encoding);
	gtk_box_pack_start (GTK_BOX (hbox1), label_encoding, FALSE, FALSE, 0);

	encoding_tooltip = gtk_tooltips_new();

	optmenu_encoding = gtk_option_menu_new ();
	gtk_widget_show (optmenu_encoding);
	gtk_tooltips_set_tip(GTK_TOOLTIPS(encoding_tooltip), optmenu_encoding,
			     _("Specify Content-Transfer-Encoding used when"
		   	       " message body contains non-ASCII characters"),
			     NULL);
 	gtk_box_pack_start (GTK_BOX (hbox1), optmenu_encoding, FALSE, FALSE, 0);

	optmenu_menu = gtk_menu_new ();

	SET_MENUITEM(_("Automatic"),	 CTE_AUTO);
	SET_MENUITEM("base64",		 CTE_BASE64);
	SET_MENUITEM("quoted-printable", CTE_QUOTED_PRINTABLE);
	SET_MENUITEM("8bit",		 CTE_8BIT);

	gtk_option_menu_set_menu (GTK_OPTION_MENU (optmenu_encoding),
				  optmenu_menu);

	hbox1 = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox1);
	gtk_box_pack_start (GTK_BOX (vbox1), hbox1, FALSE, FALSE, 0);

	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_savemsg),
		prefs_common.savemsg);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbtn_confirm_send_queued_messages),
		prefs_common.confirm_send_queued_messages);
	gtk_option_menu_set_history(GTK_OPTION_MENU(optmenu_senddialog),
		prefs_common.send_dialog_mode);
	prefs_common_charset_set_optmenu(optmenu_charset, 
		prefs_common.outgoing_charset);
	prefs_common_encoding_set_optmenu(optmenu_encoding,
		prefs_common.encoding_method);
	
	prefs_send->window			= GTK_WIDGET(window);
	
	prefs_send->checkbtn_savemsg = checkbtn_savemsg;
	prefs_send->checkbtn_confirm_send_queued_messages = checkbtn_confirm_send_queued_messages;
	prefs_send->optmenu_senddialog = optmenu_senddialog;
	prefs_send->optmenu_charset = optmenu_charset;
	prefs_send->optmenu_encoding_method = optmenu_encoding;

	prefs_send->page.widget = vbox1;
}

void prefs_send_save(PrefsPage *_page)
{
	SendPage *page = (SendPage *) _page;
	GtkWidget *menu;
	GtkWidget *menuitem;

	prefs_common.savemsg = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->checkbtn_savemsg));
	prefs_common.confirm_send_queued_messages = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->checkbtn_confirm_send_queued_messages));

	menu = gtk_option_menu_get_menu(GTK_OPTION_MENU(page->optmenu_senddialog));
	menuitem = gtk_menu_get_active(GTK_MENU(menu));
	prefs_common.send_dialog_mode = GPOINTER_TO_INT
		(g_object_get_data(G_OBJECT(menuitem), MENU_VAL_ID));
	
	g_free(prefs_common.outgoing_charset);
	prefs_common.outgoing_charset = prefs_common_charset_set_data_from_optmenu(
		page->optmenu_charset);
	prefs_common.encoding_method = prefs_common_encoding_set_data_from_optmenu(
		page->optmenu_encoding_method);
}

static void prefs_send_destroy_widget(PrefsPage *_page)
{
}

SendPage *prefs_send;

void prefs_send_init(void)
{
	SendPage *page;
	static gchar *path[3];

	path[0] = _("Mail Handling");
	path[1] = _("Send");
	path[2] = NULL;

	page = g_new0(SendPage, 1);
	page->page.path = path;
	page->page.create_widget = prefs_send_create_widget;
	page->page.destroy_widget = prefs_send_destroy_widget;
	page->page.save_page = prefs_send_save;
	page->page.weight = 195.0;
	prefs_gtk_register_page((PrefsPage *) page);
	prefs_send = page;
}

void prefs_send_done(void)
{
	prefs_gtk_unregister_page((PrefsPage *) prefs_send);
	g_free(prefs_send);
}
