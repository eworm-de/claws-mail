/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999,2000 Hiroyuki Yamamoto
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

#include <glib.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>

#include "intl.h"
#include "customheader.h"
#include "utils.h"


gchar * custom_header_get_str(CustomHeader *ch)
{
 	return g_strdup_printf
 		("%i:%s: %s", ch->account_id, ch->name, ch->value);
}
 
CustomHeader * custom_header_read_str(gchar * buf)
{
 	CustomHeader * ch;
 	gchar * account_id_str;
 	gchar * name;
 	gchar * value;
 	gchar * tmp;
 
 	Xalloca(tmp, strlen(buf) + 1, return NULL);
 	strcpy(tmp, buf);

	account_id_str = tmp;

 	name = strchr(account_id_str, ':');
 	if (!name)
 		return NULL;
 	else
 		*name++ = '\0';

 	while (*name == ' ')
 		name ++;
 	
 	ch = g_new0(CustomHeader, 1);

 	ch->account_id = atoi(account_id_str);
	if (ch->account_id == 0) {
		g_free(ch);
		return NULL;
	}

 	value = strchr(name, ':');
 	if (!value)
		{
			g_free(ch);
			return NULL;
		}
 	else
 		*value++ = '\0';
 
 	ch->name = *name ? g_strdup(name) : NULL;
 
 	while (*value == ' ')
 		value ++;

 	ch->value = *value ? g_strdup(value) : NULL;

 	return ch;
}
 
void custom_header_free(CustomHeader *ch)
{
 	if (!ch) return;
 
	if (ch->name)
		g_free(ch->name);
	if (ch->value)
		g_free(ch->value);
 
 	g_free(ch);
}
