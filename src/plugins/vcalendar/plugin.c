/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2007 Colin Leroy <colin@colino.net> and 
 * the Claws Mail team
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
#include "claws-features.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>

#include <time.h>
#include <stddef.h>
#include <curl/curl.h>

#include "version.h"
#include "vcalendar.h"
#include "claws.h"
#include "plugin.h"

#include "vcal_dbus.h"
#include "vcal_prefs.h"

gint plugin_init(gchar **error)
{
	if (!check_plugin_version(MAKE_NUMERIC_VERSION(3,13,2,39),
				VERSION_NUMERIC, PLUGIN_NAME, error))
		return -1;

	tzset();

	curl_global_init(CURL_GLOBAL_DEFAULT);
	vcalendar_init();
	if (vcalprefs.calendar_server)
		connect_dbus();

	return 0;
}

gboolean plugin_done(void)
{
	if (vcalprefs.calendar_server)
		disconnect_dbus();
	vcalendar_done();

	return TRUE;
}

const gchar *plugin_name(void)
{
	return _(PLUGIN_NAME);
}

const gchar *plugin_desc(void)
{
	return _("This plugin enables vCalendar message handling like that produced "
		 "by Evolution or Outlook.\n\n"
		 "When loaded, it will create a vCalendar mailbox in the Folder "
		 "List, which will be populated by meetings that you have accepted "
		 "or created.\n"
		 "Meeting requests that you receive will be presented in an "
		 "appropriate form and you will be able to accept or decline them.\n"
		 "To create a meeting right-click on the vCalendar or "
		 "Meetings folder and choose \"New meeting...\".\n\n"
		 "You will also be able to subscribe to remote Webcal feeds, "
		 "export your meetings and calendars, publish your free/busy "
		 "information and retrieve that information from others.");
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
		{ {PLUGIN_MIMEVIEWER, "text/calendar"},
		  {PLUGIN_FOLDERCLASS, N_("Calendar")},
		  {PLUGIN_NOTHING, NULL}};
	return features;
}
