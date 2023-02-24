/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2005-2023 the Claws Mail Team and Andrej Kacian <andrej@kacian.sk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#  include "claws-features.h"
#endif

/* Global includes */
#include <glib/gi18n.h>
#include <curl/curl.h>

/* Claws Mail includes */
#include <common/claws.h>
#include <common/version.h>
#include <plugin.h>

/* Local includes */
#include "rssyl.h"

gint plugin_init(gchar **error)
{
	if( !check_plugin_version(MAKE_NUMERIC_VERSION(3, 7, 8, 31),
				VERSION_NUMERIC, PLUGIN_NAME, error) )
		return -1;

	curl_global_init(CURL_GLOBAL_DEFAULT);
	rssyl_init();

	return 0;
}

gboolean plugin_done(void)
{
	rssyl_done();
	return TRUE;
}

const gchar *plugin_name(void)
{
	return PLUGIN_NAME;
}

const gchar *plugin_desc(void)
{
	return _("This plugin allows you to create a mailbox tree where you can add "
		"newsfeeds in RSS 1.0, RSS 2.0 or Atom format.\n\n"
		"Each newsfeed will create a folder with appropriate entries, fetched "
		"from the web. You can read them, and delete or keep old entries.");
}

const gchar *plugin_type(void)
{
	return "GTK3";
}

const gchar *plugin_licence(void)
{
	return "GPL2+";
}

const gchar *plugin_version(void)
{
	return VERSION;
}

struct PluginFeature *plugin_provides(void)
{
	static struct PluginFeature features[] = 
		{ {PLUGIN_FOLDERCLASS, N_("RSS feed")},
		  {PLUGIN_NOTHING, NULL}};
	return features;
}
