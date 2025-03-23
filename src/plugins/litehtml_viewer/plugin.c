/*
 * Claws Mail -- A GTK based, lightweight, and fast e-mail client
 * Copyright(C) 2019-2024 the Claws Mail Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write tothe Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#include "claws-features.h"
#endif

#include <glib/gi18n.h>

#include "common/version.h"
#include <mimeview.h>
#include <plugin.h>

#include "lh_prefs.h"

extern MimeViewerFactory lh_viewer_factory;

gint plugin_init(gchar **error)
{
	if (!check_plugin_version(MAKE_NUMERIC_VERSION(4,3,0,1),
				  VERSION_NUMERIC, _("LiteHTML viewer"), error))
		return -1;

	debug_print("LH: plugin_init\n");
	lh_prefs_init();
	mimeview_register_viewer_factory(&lh_viewer_factory);
	return 0;
}

gboolean plugin_done(void)
{
	debug_print("LH: plugin_done\n");
	mimeview_unregister_viewer_factory(&lh_viewer_factory);
	lh_prefs_done();
	return TRUE;
}

const gchar *plugin_name(void)
{
	return _("LiteHTML viewer");
}

const gchar *plugin_desc(void)
{
	return _("This plugin renders HTML mail using the litehtml library "
		 "(http://www.litehtml.com/).\n\n"
		 "By default all remote content is blocked.\n\n"
		 "Options can be found in '/Configuration/Preferences/Plugins/LiteHTML'.");
}

const gchar *plugin_type(void)
{
	return "GTK3";
}

const gchar *plugin_licence(void)
{
	return "GPL3";
}

const gchar *plugin_version(void)
{
	return VERSION;
}

struct PluginFeature *plugin_provides(void)
{
	static struct PluginFeature features[] = {
		{ PLUGIN_MIMEVIEWER, "text/html" },
		{ PLUGIN_NOTHING, NULL }
	};

	return features;
}
