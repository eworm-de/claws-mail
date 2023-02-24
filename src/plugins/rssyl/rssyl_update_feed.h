/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2006-2023 the Claws Mail Team and Andrej Kacian <andrej@kacian.sk>
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

#ifndef __RSSYL_UPDATE_FEED
#define __RSSYL_UPDATE_FEED

#include <glib.h>

#include "rssyl.h"

void rssyl_fetch_feed(RFetchCtx *ctx, RSSylVerboseFlags verbose);

RFetchCtx *rssyl_prep_fetchctx_from_url(gchar *url);
RFetchCtx *rssyl_prep_fetchctx_from_item(RFolderItem *ritem);

gboolean rssyl_update_feed(RFolderItem *ritem, RSSylVerboseFlags verbose);

void rssyl_update_recursively(FolderItem *item);

void rssyl_update_all_feeds(void);

#endif /* __RSSYL_UPDATE_FEED */
