/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2002 Hiroyuki Yamamoto & The Sylpheed Claws Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
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

#ifndef FILTER_NEW_H

#define FILTER_NEW_H

#include <glib.h>
#include "matcher.h"
#include "procmsg.h"

struct _FilteringAction {
	gint	type;
	gint	account_id;
	gchar  *destination;
	gchar  *unesc_destination;	/* for exec cmd line */
	gint	labelcolor;
};

typedef struct _FilteringAction FilteringAction;

struct _FilteringProp {
	MatcherList * matchers;
	FilteringAction * action;
};

typedef struct _FilteringProp FilteringProp;

/* extern GSList * prefs_filtering; */


FilteringAction * filteringaction_new(int type, int account_id,
				      gchar *destination,
				      gint labelcolor);
void filteringaction_free(FilteringAction *action);
FilteringAction * filteringaction_parse(gchar **str);

FilteringProp * filteringprop_new(MatcherList *matchers,
				  FilteringAction *action);
void filteringprop_free(FilteringProp *prop);

FilteringProp * filteringprop_parse(gchar **str);

void filter_msginfo_move_or_delete(GSList *filtering_list, MsgInfo *info,
				   GHashTable *folder_table);
void filter_message(GSList *filtering_list, FolderItem *inbox,
		    gint msgnum, GHashTable *folder_table);

gchar * filteringaction_to_string(gchar *dest, gint destlen, FilteringAction *action);
void prefs_filtering_write_config(void);
void prefs_filtering_read_config(void);
gchar * filteringprop_to_string(FilteringProp *prop);

void prefs_filtering_clear();
void prefs_filtering_free(GSList *prefs_filtering);

extern GSList * global_processing;

#endif
