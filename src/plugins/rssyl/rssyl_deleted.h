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

#ifndef __RSSYL_DELETED_H
#define __RSSYL_DELETED_H

#include <glib.h>

#include "rssyl.h"

void rssyl_deleted_update(RFolderItem *ritem);
void rssyl_deleted_free(RFolderItem *ritem);
void rssyl_deleted_add(RFolderItem *ritem, gchar *path);
void rssyl_deleted_store(RFolderItem *ritem);
gboolean rssyl_deleted_check(RFolderItem *ritem, FeedItem *fitem);
void rssyl_deleted_expire(RFolderItem *ritem, Feed *feed);

#endif /* __RSSYL_DELETED_H */
