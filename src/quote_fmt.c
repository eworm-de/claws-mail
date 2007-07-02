/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2007 Hiroyuki Yamamoto and the Claws Mail team
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

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "manage_window.h"
#include "description_window.h"
#include "gtkutils.h"

#include "prefs_gtk.h"
#include "prefs_common.h"
#include "quote_fmt.h"
#include "alertpanel.h"
#include "prefs_template.h"


/*
 * Strings describing quote format strings
 * 
 * When adding new lines, remember to put 2 strings for each line
 */
static gchar *quote_desc_strings[] = {
	N_("<span weight=\"bold\">symbols:</span>"),				NULL,
	"%date_fmt{<span style=\"oblique\">fmt</span>} (%D{<span style=\"oblique\">fmt</span>})",	N_("customized date format (see 'man strftime')"), /* date expression */
	"%date (%d)",				N_("Date"), /* date */
	"%from (%f)",				N_("From"), /* from */
	"%email (%A)",				N_("email address of sender"), /* email address */
	"%fullname (%N)",			N_("full name of sender"), /* full name */
	"%firstname (%F)",			N_("first name of sender"), /* first name */
	"%lastname (%L)",			N_("last name of sender"), /* last name */
	"%initials (%I)",			N_("initials of sender"), /* initial of sender */
	"%subject (%s)",			N_("Subject"), /* subject */ 
	"%to (%t)",					N_("To"), /* to */ 
	"%cc (%c)",					N_("Cc"), /* cc */ 
	"%newsgroups (%n)",			N_("Newsgroups"), /* newsgroups */ 
	"%references (%r)",			N_("References"), /* references */ 
	"%messageid (%i)",			N_("Message-ID"), /* message-id */ 
	"%msg (%M)",				N_("message body"), /* message */
	"%quoted_msg (%Q)",			N_("quoted message body"), /* quoted message */
	"%msg_no_sig (%m)",			N_("message body without signature"), /* message with no signature */
	"%quoted_msg_no_sig (%q)",	N_("quoted message body without signature"), /* quoted message with no signature */
	"%tags",				N_("message tags"), /* message tags */
	"%dict (%T)",				N_("current dictionary"), /* current dictionary */
	"%cursor (%X)",				N_("cursor position"), /* X marks the cursor spot */
	"%account_fullname (%af)",	N_("account property: your name"), /* full name in compose account */
	"%account_email (%ae)",		N_("account property: your email address"), /* mail address in compose account */
	"%account_name (%an)",		N_("account property: account name"), /* compose account name itself */
	"%account_org (%ao)",		N_("account property: organization"), /* organization in compose account */
	"%account_dict (%aT)",		N_("account property: default dictionary"), /* main dict (if enabled) in account */
	"%addrbook_cc (%ABc)",		N_("address book <span style=\"oblique\">completion</span>: Cc"), /* completion of 'Cc' from address book */
	"%addrbook_from (%ABf)",	N_("address book <span style=\"oblique\">completion</span>: From"), /* completion of 'From' from address book */
	"%addrbook_to (%ABt)",		N_("address book <span style=\"oblique\">completion</span>: To"), /* completion of 'To' from address book */
	"\\%", 				N_("literal %"),
	"\\\\",				N_("literal backslash"),
	"\\?",				N_("literal question mark"),
	"\\!",				N_("literal exclamation mark"),
	"\\|",				N_("literal pipe"),
	"\\{",				N_("literal opening curly brace"),
	"\\}",				N_("literal closing curly brace"),
	"\\t", 				N_("tab"),
	"\\n", 				N_("new line"),
	"",					NULL,
	N_("<span weight=\"bold\">commands:</span>"),		NULL,
	"?x{<span style=\"oblique\">expr</span>}\n\n",		N_("insert <span style=\"oblique\">expr</span> if x is set, where x is one of\nthe [dfNFLIstcnriT, ad, af, ao, aT, ABc, ABf, ABt]\nsymbols (or their long equivalent)"),
	"!x{<span style=\"oblique\">expr</span>}\n\n",		N_("insert <span style=\"oblique\">expr</span> if x is not set, where x is one of\nthe [dfNFLIstcnriT, ad, af, ao, aT, ABc, ABf, ABt]\nsymbols (or their long equivalent)"),
	"|file{<span style=\"oblique\">sub_expr</span>}\n(|f{<span style=\"oblique\">sub_expr</span>})",		N_("insert file:\n<span style=\"oblique\">sub_expr</span> is evaluated as the path of the file to insert"), /* insert file */
	"|program{<span style=\"oblique\">sub_expr</span>}\n(|p{<span style=\"oblique\">sub_expr</span>})\n",	N_("insert program output:\n<span style=\"oblique\">sub_expr</span> is evaluated as a command-line to get\nthe output from"), /* insert program output */
	"|input{<span style=\"oblique\">sub_expr</span>}\n(|i{<span style=\"oblique\">sub_expr</span>})\n",		N_("insert user input:\n<span style=\"oblique\">sub_expr</span> is a variable to be replaced by\nuser-entered text"), /* insert user input */
	"",					NULL,
	N_("<span weight=\"bold\">definition of terms:</span>"),	NULL,
	"<span style=\"oblique\">expr</span>\n",			N_("text that can contain any of the symbols or\ncommands above"),
	"<span style=\"oblique\">sub_expr</span>\n",		N_("text that can contain any of the symbols (no\ncommands) above"),
	"<span style=\"oblique\">completion</span>\n\n\n",	N_("completion from address book only works with the first\naddress of the header, it outputs the full name\nof the contact if that address matches exactly\none contact in the address book"),
	NULL,NULL
};

static DescriptionWindow quote_desc_win = { 
        NULL,
	NULL,
        2,
        N_("Description of symbols"),
	N_("The following symbols and commands can be used:"),
        quote_desc_strings
};


void quote_fmt_quote_description(GtkWidget *widget, GtkWidget *pref_window)
{
	quote_desc_win.parent = pref_window;
	description_window_create(&quote_desc_win);
}

void quotefmt_create_new_msg_fmt_widgets(GtkWindow *parent_window,
						GtkWidget *parent_box,
						GtkWidget **checkbtn_compose_with_format,
						gchar *checkbtn_compose_text,
						GtkWidget **edit_subject_format,
						GtkWidget **edit_body_format,
						gboolean add_info_button)
{
	GtkWidget *checkbtn_use_format = NULL;
	GtkWidget *frame_format;
	GtkWidget *vbox_format;
	GtkWidget *hbox_format;
	GtkWidget *label_subject;
	GtkWidget *entry_subject;
	GtkWidget *scrolledwin_format;
	GtkWidget *text_format;

	if (add_info_button)
		g_return_if_fail(parent_window != NULL);
	g_return_if_fail(parent_box != NULL);
	if (checkbtn_compose_with_format) {
		g_return_if_fail(checkbtn_compose_with_format != NULL);
		g_return_if_fail(checkbtn_compose_text != NULL);
	}
	g_return_if_fail(edit_subject_format != NULL);
	g_return_if_fail(edit_body_format != NULL);

	if (checkbtn_compose_with_format)
		PACK_CHECK_BUTTON (parent_box, checkbtn_use_format, 
				   _("Use format when composing new messages"));

	vbox_format = gtkut_get_options_frame(parent_box, &frame_format, checkbtn_compose_text);

	if (checkbtn_compose_with_format)
		SET_TOGGLE_SENSITIVITY(checkbtn_use_format, frame_format);

	hbox_format = gtk_hbox_new (FALSE, 8);
	gtk_widget_show (hbox_format);
	gtk_box_pack_start (GTK_BOX (vbox_format), hbox_format, FALSE, FALSE, 0);

	label_subject = gtk_label_new (_("Subject"));
	gtk_widget_show (label_subject);
	gtk_box_pack_start (GTK_BOX (hbox_format), label_subject, FALSE, FALSE, 0);

	entry_subject = gtk_entry_new ();
	gtk_widget_show (entry_subject);
	gtk_box_pack_start (GTK_BOX (hbox_format), entry_subject, TRUE, TRUE, 0);
	gtk_widget_set_size_request (entry_subject, 100, -1);

	scrolledwin_format = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (scrolledwin_format);
	gtk_box_pack_start (GTK_BOX (vbox_format), scrolledwin_format,
			    TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy
		(GTK_SCROLLED_WINDOW (scrolledwin_format),
		 GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type
		(GTK_SCROLLED_WINDOW (scrolledwin_format), GTK_SHADOW_IN);

	text_format = gtk_text_view_new ();
	if (prefs_common.textfont) {
		PangoFontDescription *font_desc;

		font_desc = pango_font_description_from_string
						(prefs_common.textfont);
		if (font_desc) {
			gtk_widget_modify_font(text_format, font_desc);
			pango_font_description_free(font_desc);
		}
	}
	gtk_widget_show(text_format);
	gtk_container_add(GTK_CONTAINER(scrolledwin_format), text_format);
	gtk_text_view_set_editable (GTK_TEXT_VIEW (text_format), TRUE);
	gtk_widget_set_size_request(text_format, -1, 100);

	if (add_info_button)
		quotefmt_add_info_button(parent_window, vbox_format);

	if (checkbtn_compose_with_format)
		*checkbtn_compose_with_format = checkbtn_use_format;
	*edit_subject_format = entry_subject;
	*edit_body_format = text_format;
}

void quotefmt_create_reply_fmt_widgets(GtkWindow *parent_window,
						GtkWidget *parent_box,
						GtkWidget **checkbtn_reply_with_format,
						gchar *checkbtn_reply_text,
						GtkWidget **edit_reply_quotemark,
						GtkWidget **edit_reply_format,
						gboolean add_info_button)
{
	GtkWidget *checkbtn_use_format = NULL;
	GtkWidget *frame_quote;
	GtkWidget *vbox_quote;
	GtkWidget *hbox1;
	GtkWidget *hbox2;
	GtkWidget *label_quotemark;
	GtkWidget *entry_quotemark;
	GtkWidget *scrolledwin_quotefmt;
	GtkWidget *text_quotefmt;

	if (add_info_button)
		g_return_if_fail(parent_window != NULL);
	g_return_if_fail(parent_box != NULL);
	if (checkbtn_reply_with_format) {
		g_return_if_fail(checkbtn_reply_with_format != NULL);
		g_return_if_fail(checkbtn_reply_text != NULL);
	}
	g_return_if_fail(edit_reply_quotemark != NULL);
	g_return_if_fail(edit_reply_format != NULL);

	if (checkbtn_reply_with_format)
		PACK_CHECK_BUTTON (parent_box, checkbtn_use_format, checkbtn_reply_text);

	PACK_FRAME (parent_box, frame_quote, _("Reply format"));

	if (checkbtn_reply_with_format)
		SET_TOGGLE_SENSITIVITY(checkbtn_use_format, frame_quote);

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

	label_quotemark = gtk_label_new (_("Quotation mark"));
	gtk_widget_show (label_quotemark);
	gtk_box_pack_start (GTK_BOX (hbox2), label_quotemark, FALSE, FALSE, 0);

	entry_quotemark = gtk_entry_new ();
	gtk_widget_show (entry_quotemark);
	gtk_box_pack_start (GTK_BOX (hbox2), entry_quotemark, FALSE, FALSE, 0);
	gtk_widget_set_size_request (entry_quotemark, 64, -1);

	scrolledwin_quotefmt = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (scrolledwin_quotefmt);
	gtk_box_pack_start (GTK_BOX (vbox_quote), scrolledwin_quotefmt,
			    TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy
		(GTK_SCROLLED_WINDOW (scrolledwin_quotefmt),
		 GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type
		(GTK_SCROLLED_WINDOW (scrolledwin_quotefmt), GTK_SHADOW_IN);

	text_quotefmt = gtk_text_view_new ();
	if (prefs_common.textfont) {
		PangoFontDescription *font_desc;

		font_desc = pango_font_description_from_string
						(prefs_common.textfont);
		if (font_desc) {
			gtk_widget_modify_font(text_quotefmt, font_desc);
			pango_font_description_free(font_desc);
		}
	}
	gtk_widget_show(text_quotefmt);
	gtk_container_add(GTK_CONTAINER(scrolledwin_quotefmt), text_quotefmt);
	gtk_text_view_set_editable (GTK_TEXT_VIEW (text_quotefmt), TRUE);
	gtk_widget_set_size_request(text_quotefmt, -1, 100);

	if (add_info_button)
		quotefmt_add_info_button(parent_window, vbox_quote);

	if (checkbtn_reply_with_format)
		*checkbtn_reply_with_format = checkbtn_use_format;
	*edit_reply_quotemark = entry_quotemark;
	*edit_reply_format = text_quotefmt;
}

void quotefmt_create_forward_fmt_widgets(GtkWindow *parent_window,
						GtkWidget *parent_box,
						GtkWidget **checkbtn_forward_with_format,
						gchar *checkbtn_forward_text,
						GtkWidget **edit_fw_quotemark,
						GtkWidget **edit_fw_format,
						gboolean add_info_button)
{
	GtkWidget *checkbtn_use_format = NULL;
	GtkWidget *frame_quote;
	GtkWidget *vbox_quote;
	GtkWidget *hbox1;
	GtkWidget *hbox2;
	GtkWidget *label_quotemark;
	GtkWidget *entry_fw_quotemark;
	GtkWidget *scrolledwin_quotefmt;
	GtkWidget *text_fw_quotefmt;

	if (add_info_button)
		g_return_if_fail(parent_window != NULL);
	g_return_if_fail(parent_box != NULL);
	if (checkbtn_forward_with_format) {
		g_return_if_fail(checkbtn_forward_with_format != NULL);
		g_return_if_fail(checkbtn_forward_text != NULL);
	}
	g_return_if_fail(edit_fw_quotemark != NULL);
	g_return_if_fail(edit_fw_format != NULL);

	if (checkbtn_forward_with_format)
		PACK_CHECK_BUTTON (parent_box, checkbtn_use_format, checkbtn_forward_text);

	PACK_FRAME (parent_box, frame_quote, _("Forward format"));

	if (checkbtn_forward_with_format)
		SET_TOGGLE_SENSITIVITY(checkbtn_use_format, frame_quote);

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

	label_quotemark = gtk_label_new (_("Quotation mark"));
	gtk_widget_show (label_quotemark);
	gtk_box_pack_start (GTK_BOX (hbox2), label_quotemark, FALSE, FALSE, 0);

	entry_fw_quotemark = gtk_entry_new ();
	gtk_widget_show (entry_fw_quotemark);
	gtk_box_pack_start (GTK_BOX (hbox2), entry_fw_quotemark,
			    FALSE, FALSE, 0);
	gtk_widget_set_size_request (entry_fw_quotemark, 64, -1);

	scrolledwin_quotefmt = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (scrolledwin_quotefmt);
	gtk_box_pack_start (GTK_BOX (vbox_quote), scrolledwin_quotefmt,
			    TRUE, TRUE, 0);
	gtk_scrolled_window_set_policy
		(GTK_SCROLLED_WINDOW (scrolledwin_quotefmt),
		 GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type
		(GTK_SCROLLED_WINDOW (scrolledwin_quotefmt), GTK_SHADOW_IN);

	text_fw_quotefmt = gtk_text_view_new ();
	if (prefs_common.textfont) {
		PangoFontDescription *font_desc;

		font_desc = pango_font_description_from_string
						(prefs_common.textfont);
		if (font_desc) {
			gtk_widget_modify_font(text_fw_quotefmt, font_desc);
			pango_font_description_free(font_desc);
		}
	}
	gtk_widget_show(text_fw_quotefmt);
	gtk_container_add(GTK_CONTAINER(scrolledwin_quotefmt),
			  text_fw_quotefmt);
	gtk_text_view_set_editable (GTK_TEXT_VIEW (text_fw_quotefmt), TRUE);
	gtk_widget_set_size_request (text_fw_quotefmt, -1, 100);

	if (add_info_button)
		quotefmt_add_info_button(parent_window, vbox_quote);

	if (checkbtn_forward_with_format)
		*checkbtn_forward_with_format = checkbtn_use_format;
	*edit_fw_quotemark = entry_fw_quotemark;
	*edit_fw_format = text_fw_quotefmt;
}

void quotefmt_add_info_button(GtkWindow *parent_window, GtkWidget *parent_box)
{
	GtkWidget *hbox_formatdesc;
	GtkWidget *btn_formatdesc;

	hbox_formatdesc = gtk_hbox_new (FALSE, 32);
	gtk_widget_show (hbox_formatdesc);
	gtk_box_pack_start (GTK_BOX (parent_box), hbox_formatdesc, FALSE, FALSE, 0);

#if GTK_CHECK_VERSION(2, 8, 0)
	btn_formatdesc = gtk_button_new_from_stock(GTK_STOCK_INFO);
#else
	btn_formatdesc =
		gtk_button_new_with_label (_("Description of symbols..."));
#endif
	gtk_widget_show (btn_formatdesc);
	gtk_box_pack_start (GTK_BOX (hbox_formatdesc), btn_formatdesc, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(btn_formatdesc), "clicked",
			 G_CALLBACK(quote_fmt_quote_description), GTK_WIDGET(parent_window));
}


void quotefmt_check_new_msg_formats(gboolean use_format,
									gchar *subject_fmt,
									gchar *body_fmt)
{
	if (use_format) {
		gint line;

		if (!prefs_template_string_is_valid(subject_fmt, NULL))
			alertpanel_error(_("New message subject format error."));

		if (!prefs_template_string_is_valid(body_fmt, &line)) {
			gchar *msg = g_strdup_printf(_("New message body format error at line %d."), line);
			alertpanel_error(msg);
			g_free(msg);
		}
	}
}

void quotefmt_check_reply_formats(gboolean use_format,
									gchar *quotation_mark,
									gchar *body_fmt)
{
	if (use_format) {
		gint line;

		if (!prefs_template_string_is_valid(quotation_mark, NULL))
			alertpanel_error(_("Message reply quotation mark format error."));

		if (!prefs_template_string_is_valid(body_fmt, &line)) {
			gchar *msg = g_strdup_printf(_("Message reply format error at line %d."), line);
			alertpanel_error(msg);
			g_free(msg);
		}
	}
}

void quotefmt_check_forward_formats(gboolean use_format,
									gchar *quotation_mark,
									gchar *body_fmt)
{
	if (use_format) {
		gint line;

		if (!prefs_template_string_is_valid(quotation_mark, NULL))
			alertpanel_error(_("Message forward quotation mark format error."));

		if (!prefs_template_string_is_valid(body_fmt, &line)) {
			gchar *msg = g_strdup_printf(_("Message forward format error at line %d."), line);
			alertpanel_error(msg);
			g_free(msg);
		}
	}
}
