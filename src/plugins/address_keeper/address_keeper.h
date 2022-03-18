/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2009-2018 Ricardo Mones and the Claws Mail Team
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

#ifndef __ADDRESS_KEEPER_H
#define __ADDRESS_KEEPER_H

#include <glib.h>

#include "version.h"
#include "claws.h"
#include "plugin.h"
#include "utils.h"
#include "hooks.h"
#include "compose.h"
#include "addressbook.h"

#define PLUGIN_NAME	(_("Address Keeper"))

gint 		plugin_init	  	  (gchar **error);
gboolean	plugin_done		  (void);
const gchar *	plugin_name		  (void);
const gchar *	plugin_desc		  (void);
const gchar *	plugin_type		  (void);
const gchar *	plugin_licence		  (void);
const gchar *	plugin_version		  (void);
struct PluginFeature *plugin_provides	  (void);

#endif

