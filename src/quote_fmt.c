/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2006 Hiroyuki Yamamoto and the Sylpheed-Claws team
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
	"%D{fmt}",	N_("Customize date format (see man strftime)"), /* date expression */
	"%d",		N_("Date"), /* date */
	"%f",		N_("From"), /* from */
	"%N",		N_("Full Name of Sender"), /* full name */
	"%F",		N_("First Name of Sender"), /* first name */
	"%L",		N_("Last Name of Sender"), /* last name */
	"%I",		N_("Initials of Sender"), /* initial of sender */
	"%s",		N_("Subject"), /* subject */ 
	"%t",		N_("To"), /* to */ 
	"%c",		N_("Cc"), /* cc */ 
	"%n",		N_("Newsgroups"), /* newsgroups */ 
	"%r",		N_("References"), /* references */ 
	"%i",		N_("Message-ID"), /* message-id */ 
	"%M",		N_("Message body"), /* message */ 
	"%Q",		N_("Quoted message body"), /* quoted message */ 
	"%m",		N_("Message body without signature"), /* message with no signature */ 
	"%q",		N_("Quoted message body without signature"), /* quoted message with no signature */ 
	"%X",		N_("Cursor position"), /* X marks the cursor spot */
	"",		NULL,
	"?x{expr}",	N_("Insert expr if x is set\nx is one of the characters above after %"),
	"",		NULL,
	"\\%", 		N_("Literal %"),
	"\\\\",		N_("Literal backslash"),
	"\\?",		N_("Literal question mark"),
	"\\|",		N_("Literal pipe"),
	"\\{",		N_("Literal opening curly brace"),
	"\\}",		N_("Literal closing curly brace"),
	"",		NULL,
	"|f{file}",	N_("Insert File"),
	"|p{command}",  N_("Insert program output"), /* insert program output */ 
	NULL,NULL
};

static DescriptionWindow quote_desc_win = { 
        NULL,
	NULL,
        2,
        N_("Description of symbols"),
        quote_desc_strings
};


void quote_fmt_quote_description(GtkWidget *widget, GtkWidget *pref_window)
{
	quote_desc_win.parent = pref_window;
	description_window_create(&quote_desc_win);
}

