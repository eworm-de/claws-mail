/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2003 Hiroyuki Yamamoto
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

#include "version.h"
#include "common/sylpheed.h"
#include "pgpmime.h"

gint plugin_init(gchar **error)
{
	if ((sylpheed_get_version() > VERSION_NUMERIC)) {
		*error = g_strdup("Your sylpheed version is newer than the version the plugin was built with");
		return -1;
	}

	if ((sylpheed_get_version() < MAKE_NUMERIC_VERSION(1, 9, 12, 40))) {
		*error = g_strdup("Your sylpheed version is too old");
		return -1;
	}

	pgpmime_init();

	return 0;	
}

void plugin_done(void)
{
	pgpmime_done();
}

const gchar *plugin_name(void)
{
	return _("PGP/MIME");
}

const gchar *plugin_desc(void)
{
	return _("This plugin handles PGP/MIME signed and/or encrypted "
		 "mails. You can decrypt mails, verify signatures or "
                 "sign and encrypt your own mails.\n"
		 "\n"
		 "The plugin uses the GPGME library as a wrapper for GnuPG.\n"
		 "\n"
		 "GPGME is copyright 2001 by Werner Koch <dd9jn@gnu.org>\n");
}

const gchar *plugin_type(void)
{
	return "GTK2";
}
