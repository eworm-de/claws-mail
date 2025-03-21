/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2005-2025 the Claws Mail Team and Andrej Kacian <andrej@kacian.sk>
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

#ifndef __RSSYL_FEED_PROPS
#define __RSSYL_FEED_PROPS

#include "rssyl.h"

void rssyl_gtk_prop(RFolderItem *ritem);

struct _RFeedProp {
	GtkWidget *window;
	GtkWidget *url;
	GtkWidget *default_refresh_interval;
	GtkWidget *refresh_interval;
	GtkWidget *keep_old;
	GtkWidget *fetch_comments;
	GtkWidget *fetch_comments_max_age;
	GtkWidget *silent_update;
	GtkWidget *write_heading;
	GtkWidget *ignore_title_rename;
	GtkWidget *ssl_verify_peer;
	GtkWidget *auth_type;
	GtkWidget *auth_username;
	GtkWidget *auth_password;
	GtkWidget *use_default_user_agent;
	GtkWidget *specific_user_agent;
};

typedef struct _RFeedProp RFeedProp;

#endif /* __RSSYL_FEED_PROPS */
