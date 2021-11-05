/* Notification plugin for Claws Mail
 * Copyright (C) 2005-2009 Holger Berndt and the Claws Mail Team.
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef NOTIFICATION_HOTKEYS_H
#define NOTIFICATION_HOTKEYS_H NOTIFICATION_HOTKEYS_H

#ifdef HAVE_CONFIG_H
#  include "claws-features.h"
#endif

#ifdef NOTIFICATION_HOTKEYS

#include <glib.h>

void notification_hotkeys_update_bindings();
void notification_hotkeys_unbind_all();

#endif /* NOTIFICATION_HOTKEYS */
#endif /* NOTIFICATION_HOTKEYS_H */
