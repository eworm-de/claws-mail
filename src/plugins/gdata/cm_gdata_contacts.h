/* GData plugin for Claws-Mail
 * Copyright (C) 2011 Holger Berndt
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

#ifndef CM_GDATA_CONTACTS_H
#define CM_GDATA_CONTACTS_H

#include <glib.h>

void cm_gdata_add_contacts(GList **address_list);
void cm_gdata_contacts_done(void);
gboolean cm_gdata_update_contacts_cache(void);
void cm_gdata_load_contacts_cache_from_file(void);

#endif /* CM_GDATA_CONTACTS_H */
