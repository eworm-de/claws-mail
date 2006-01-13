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

#include <stddef.h>

#include <glib.h>
#include <glib/gi18n.h>

#include "sylpheed.h"
#include "version.h"
#include "pgpinline.h"

gint plugin_init(gchar **error)
{
	if ((sylpheed_get_version() > VERSION_NUMERIC)) {
		*error = g_strdup("Your version of Sylpheed-Claws is newer than the version the PGP/inline plugin was built with");
		return -1;
	}

	if ((sylpheed_get_version() < MAKE_NUMERIC_VERSION(1, 9, 12, 49))) {
		*error = g_strdup("Your version of Sylpheed-Claws is too old for the PGP/inline plugin");
		return -1;
	}

	pgpinline_init();

	return 0;	
}

void plugin_done(void)
{
	pgpinline_done();
}

const gchar *plugin_name(void)
{
	return _("PGP/inline");
}

const gchar *plugin_desc(void)
{
	return _("This plugin enables signature verification of "
		 "digitally signed messages, and decryption of "
		 "encrypted messages. \n"
		 "\n"
		 "It also lets you send signed and encrypted "
		 "messages.");
}

const gchar *plugin_type(void)
{
	return "GTK2";
}

const gchar *plugin_licence(void)
{
	return "GPL";
}

const gchar *plugin_version(void)
{
	return VERSION;
}
