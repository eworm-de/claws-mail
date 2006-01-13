/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2006 Hiroyuki Yamamoto & The Sylpheed Claws Team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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
	gint	labelcolor;
	gint	score;
};

typedef struct _FilteringAction FilteringAction;

struct _FilteringProp {
	gchar *name;
	MatcherList * matchers;
	GSList * action_list;
};

typedef struct _FilteringProp FilteringProp;

/* extern GSList * prefs_filtering; */


FilteringAction * filteringaction_new(int type, int account_id,
				      gchar * destination,
                                      gint labelcolor, gint score);
void filteringaction_free(FilteringAction *action);
FilteringAction * filteringaction_parse(gchar **str);
gboolean filteringaction_apply_action_list (GSList *action_list, MsgInfo *info);

FilteringProp * filteringprop_new(const gchar *name,
				  MatcherList *matchers,
				  GSList *action_list);
void filteringprop_free(FilteringProp *prop);

FilteringProp * filteringprop_parse(gchar **str);

void filter_msginfo_move_or_delete(GSList *filtering_list, MsgInfo *info);
gboolean filter_message_by_msginfo(GSList *flist, MsgInfo *info);

gchar * filteringaction_to_string(gchar *dest, gint destlen, FilteringAction *action);
void prefs_filtering_write_config(void);
void prefs_filtering_read_config(void);
gchar * filteringaction_list_to_string(GSList * action_list);
gchar * filteringprop_to_string(FilteringProp *prop);

void prefs_filtering_clear(void);
void prefs_filtering_clear_folder(Folder *folder);
void prefs_filtering_free(GSList *prefs_filtering);

FilteringProp * filteringprop_copy(FilteringProp *src);

extern GSList * filtering_rules;
extern GSList * pre_global_processing;
extern GSList * post_global_processing;

#endif
