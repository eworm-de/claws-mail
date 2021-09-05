/*
 * mailmbox Plugin -- mbox support for Sylpheed
 * Copyright (C) 2003 Christoph Hohmann
 * Copyright (C) 2003-2005 Hoa v. Dinh, Alfons Hoogervorst
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


#ifdef HAVE_CONFIG_H
#  include "config.h"
#  include "claws-features.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include "plugin.h"
#include "folder.h"
#include "mailmbox_folder.h"
#include "common/version.h"
#include "plugin_gtk.h"
#include "main.h"

gint plugin_init(gchar **error)
{
	if (!check_plugin_version(MAKE_NUMERIC_VERSION(3,8,1,46),
				VERSION_NUMERIC, "Mailmbox", error))
		return -1;

	folder_register_class(claws_mailmbox_get_class());
	plugin_gtk_init(error);
	return 0;
}

gboolean plugin_done(void)
{
	plugin_gtk_done();
	if (!claws_is_exiting())
		folder_unregister_class(claws_mailmbox_get_class());
	return TRUE;
}

const gchar *plugin_name(void)
{
	return _("mailmbox folder");
}

const gchar *plugin_desc(void)
{
	return _("This is a plugin to handle mailboxes in mbox format.");
}

const gchar *plugin_type(void)
{
	return "GTK3";
}

const gchar *plugin_licence(void)
{
		return "GPL3+";
}

const gchar *plugin_version(void)
{
	return VERSION;
}

struct PluginFeature *plugin_provides(void)
{
	static struct PluginFeature features[] = 
		{ {PLUGIN_FOLDERCLASS, N_("MBOX")},
		  {PLUGIN_NOTHING, NULL}};
	return features;
}
