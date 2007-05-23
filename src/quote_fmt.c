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

