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

#include "defs.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include "intl.h"
#include "utils.h"
#include "procheader.h"
#include "matcher.h"
#include "filtering.h"
#include "prefs.h"
#include "compose.h"

#define PREFSBUFSIZE		1024

GSList * global_processing = NULL;

#define STRLEN_WITH_CHECK(expr) \
        strlen_with_check(#expr, __LINE__, expr)
	        
static inline gint strlen_with_check(const gchar *expr, gint fline, const gchar *str)
{
        if (str) 
		return strlen(str);
	else {
	        debug_print("%s(%d) - invalid string %s\n", __FILE__, fline, expr);
	        return 0;
	}
}

FilteringAction * filteringaction_new(int type, int account_id,
				      gchar * destination,
				      gint labelcolor)
{
	FilteringAction * action;

	action = g_new0(FilteringAction, 1);

	action->type = type;
	action->account_id = account_id;
	if (destination) {
		action->destination	  = g_strdup(destination);
		action->unesc_destination = matcher_unescape_str(g_strdup(destination));
	} else {
		action->destination       = NULL;
		action->unesc_destination = NULL;
	}
	action->labelcolor = labelcolor;	
	return action;
}

void filteringaction_free(FilteringAction * action)
{
	g_return_if_fail(action);
	if (action->destination)
		g_free(action->destination);
	if (action->unesc_destination)
		g_free(action->unesc_destination);
	g_free(action);
}

FilteringProp * filteringprop_new(MatcherList * matchers,
				  FilteringAction * action)
{
	FilteringProp * filtering;

	filtering = g_new0(FilteringProp, 1);
	filtering->matchers = matchers;
	filtering->action = action;

	return filtering;
}

FilteringProp * filteringprop_copy(FilteringProp *src)
{
	FilteringProp * new;
	GSList *tmp;
	
	new = g_new0(FilteringProp, 1);
	new->matchers = g_new0(MatcherList, 1);
	new->action = g_new0(FilteringAction, 1);
	for (tmp = src->matchers->matchers; tmp != NULL && tmp->data != NULL;) {
		MatcherProp *matcher = (MatcherProp *)tmp->data;
		
		new->matchers->matchers = g_slist_append(new->matchers->matchers,
						   matcherprop_copy(matcher));
		tmp = tmp->next;
	}
	new->matchers->bool_and = src->matchers->bool_and;
	new->action->type = src->action->type;
	new->action->account_id = src->action->account_id;
	if (src->action->destination)
		new->action->destination = g_strdup(src->action->destination);
	else 
		new->action->destination = NULL;
	if (src->action->unesc_destination)
		new->action->unesc_destination = g_strdup(src->action->unesc_destination);
	else
		new->action->unesc_destination = NULL;
	new->action->labelcolor = src->action->labelcolor;
	return new;
}

void filteringprop_free(FilteringProp * prop)
{
	matcherlist_free(prop->matchers);
	filteringaction_free(prop->action);
	g_free(prop);
}

/*
  fitleringaction_apply
  runs the action on one MsgInfo
  return value : return TRUE if the action could be applied
*/

static gboolean filteringaction_apply(FilteringAction * action, MsgInfo * info)
{
	FolderItem * dest_folder;
	gint val;
	Compose * compose;
	PrefsAccount * account;
	gchar * cmd;

	switch(action->type) {
	case MATCHACTION_MOVE:
		dest_folder =
			folder_find_item_from_identifier(action->destination);
		if (!dest_folder)
			return FALSE;
		
		if (folder_item_move_msg(dest_folder, info) == -1) {
			debug_print("*** could not move message\n");
			return FALSE;
		}	

		return TRUE;

	case MATCHACTION_COPY:
		dest_folder =
			folder_find_item_from_identifier(action->destination);

		if (!dest_folder)
			return FALSE;

		if (folder_item_copy_msg(dest_folder, info) == -1)
			return FALSE;

		return TRUE;

	case MATCHACTION_DELETE:
		if (folder_item_remove_msg(info->folder, info->msgnum) == -1)
			return FALSE;
		return TRUE;

	case MATCHACTION_MARK:
		procmsg_msginfo_set_flags(info, MSG_MARKED, 0);
		return TRUE;

	case MATCHACTION_UNMARK:
		procmsg_msginfo_unset_flags(info, MSG_MARKED, 0);
		return TRUE;
		
	case MATCHACTION_MARK_AS_READ:
		procmsg_msginfo_unset_flags(info, MSG_UNREAD | MSG_NEW, 0);
		return TRUE;

	case MATCHACTION_MARK_AS_UNREAD:
		debug_print("*** setting unread flags\n");
		procmsg_msginfo_set_flags(info, MSG_UNREAD | MSG_NEW, 0);
		return TRUE;
	
	case MATCHACTION_COLOR:
		procmsg_msginfo_unset_flags(info, MSG_CLABEL_FLAG_MASK, 0); 
		procmsg_msginfo_set_flags(info, MSG_COLORLABEL_TO_FLAGS(action->labelcolor), 0);
		return TRUE;

	case MATCHACTION_FORWARD:
		account = account_find_from_id(action->account_id);
		compose = compose_forward(account, info, FALSE, NULL);
		if (compose->account->protocol == A_NNTP)
			compose_entry_append(compose, action->destination,
					     COMPOSE_NEWSGROUPS);
		else
			compose_entry_append(compose, action->destination,
					     COMPOSE_TO);

		val = compose_send(compose);
		if (val == 0) {
			gtk_widget_destroy(compose->window);
			return TRUE;
		}

		gtk_widget_destroy(compose->window);
		return FALSE;

	case MATCHACTION_FORWARD_AS_ATTACHMENT:

		account = account_find_from_id(action->account_id);
		compose = compose_forward(account, info, TRUE, NULL);
		if (compose->account->protocol == A_NNTP)
			compose_entry_append(compose, action->destination,
					     COMPOSE_NEWSGROUPS);
		else
			compose_entry_append(compose, action->destination,
					     COMPOSE_TO);

		val = compose_send(compose);
		if (val == 0) {
			gtk_widget_destroy(compose->window);
			return TRUE;
		}
		gtk_widget_destroy(compose->window);
		return FALSE;

	case MATCHACTION_REDIRECT:
		account = account_find_from_id(action->account_id);
		compose = compose_redirect(account, info);
		if (compose->account->protocol == A_NNTP)
			break;
		else
			compose_entry_append(compose, action->destination,
					     COMPOSE_TO);

		val = compose_send(compose);
		if (val == 0) {
			gtk_widget_destroy(compose->window);
			return TRUE;
		}

		gtk_widget_destroy(compose->window);
		return FALSE;

	case MATCHACTION_EXECUTE:
		cmd = matching_build_command(action->unesc_destination, info);
		if (cmd == NULL)
			return FALSE;
		else {
			system(cmd);
			g_free(cmd);
		}
		return TRUE;

	default:
		break;
	}
	return FALSE;
}

static gboolean filtering_match_condition(FilteringProp *filtering, MsgInfo *info)
{
	return matcherlist_match(filtering->matchers, info);
}

static gboolean filtering_apply_rule(FilteringProp *filtering, MsgInfo *info)
{
	gboolean result;
	gchar    buf[50];

	if (FALSE == (result = filteringaction_apply(filtering->action, info))) {
		g_warning(_("action %s could not be applied"), 
		filteringaction_to_string(buf, sizeof buf, filtering->action));
	}
	return result;
}

static gboolean filtering_is_final_action(FilteringProp *filtering)
{
	switch(filtering->action->type) {
	case MATCHACTION_MOVE:
	case MATCHACTION_DELETE:
		return TRUE; /* MsgInfo invalid for message */
	case MATCHACTION_EXECUTE:
	case MATCHACTION_COPY:
	case MATCHACTION_MARK:
	case MATCHACTION_MARK_AS_READ:
	case MATCHACTION_UNMARK:
	case MATCHACTION_MARK_AS_UNREAD:
	case MATCHACTION_FORWARD:
	case MATCHACTION_FORWARD_AS_ATTACHMENT:
	case MATCHACTION_REDIRECT:
		return FALSE; /* MsgInfo still valid for message */
	default:
		return FALSE;
	}
}

static void filter_msginfo(GSList * filtering_list, FolderItem *inbox,
			   MsgInfo * info)
{
	GSList	*l;
	gboolean final;
	gboolean applied;
	gint val;
	
	if (info == NULL) {
		g_warning(_("msginfo is not set"));
		return;
	}
	
	for (l = filtering_list, final = FALSE, applied = FALSE; l != NULL; l = g_slist_next(l)) {
		FilteringProp * filtering = (FilteringProp *) l->data;

		if (filtering_match_condition(filtering, info)) {
			applied = filtering_apply_rule(filtering, info);
			if (TRUE == (final = filtering_is_final_action(filtering)))
				break;
		}		
	}

	/* put in inbox if a final rule could not be applied, or
	 * the last rule was not a final one. */
	if ((final && !applied) || !final) {
		if (inbox != info->folder) {
			if (folder_item_move_msg(inbox, info) == -1) {
				debug_print("*** Could not drop message in inbox; check .processing\n");
				return;
			}	
		}	
	}
}

/*!
 *\brief	filters a message based on its message info data
 *
 *\param	flist filter and actions list
 *\param	info message
 */
void filter_message_by_msginfo_with_inbox(GSList *flist, MsgInfo *info, FolderItem *def_inbox)
{
	FolderItem *inbox;

	if ((def_inbox == NULL)) {
		debug_print("using default inbox as final destination!\n");
		inbox = folder_get_default_inbox();
	} else
		inbox = def_inbox;

	/*
	 * message is already in a folder. the filtering code will
	 * handle duplicate moves and copies.
	 */
	filter_msginfo(flist, inbox, info);
}

void filter_message_by_msginfo(GSList *flist, MsgInfo *info)
{
	filter_message_by_msginfo_with_inbox(flist, info, info->folder);
}

/*!
 *\brief	filters a message waiting to be processed in the
 *		.processing folder. 
 *
  *\param	filtering_list list of filters and actions
  *\param	inbox default inbox when no filter could be applied
  *\param	msgnum message number in processing folder
  *		changed after the call to this function
  */
void filter_message(GSList *filtering_list, FolderItem *inbox,
		    gint msgnum)
{
	MsgInfo *msginfo;
	gchar *filename;
	MsgFlags  msgflags = { 0, 0 };
	FolderItem *item = folder_get_default_processing();

	if (item == NULL) {
		g_warning(_("folderitem not set"));
		return;
	}

	filename = folder_item_fetch_msg(item, msgnum);

	if (filename == NULL) {
		g_warning(_("filename is not set"));
		return;
	}

	msginfo = procheader_parse_file(filename, msgflags, TRUE, FALSE);
	
	g_free(filename);

	if (msginfo == NULL) {
		g_warning(_("could not get info for %s"), filename);
		return;
	}

	msginfo->folder = item;
	msginfo->msgnum = msgnum;

	filter_msginfo(filtering_list, inbox, msginfo);

	procmsg_msginfo_free(msginfo);
}

gchar *filteringaction_to_string(gchar *dest, gint destlen, FilteringAction *action)
{
	gchar *command_str;

	command_str = get_matchparser_tab_str(action->type);

	if (command_str == NULL)
		return NULL;

	switch(action->type) {
	case MATCHACTION_MOVE:
	case MATCHACTION_COPY:
	case MATCHACTION_EXECUTE:
		g_snprintf(dest, destlen, "%s \"%s\"", command_str, action->destination);
		return dest;

	case MATCHACTION_DELETE:
	case MATCHACTION_MARK:
	case MATCHACTION_UNMARK:
	case MATCHACTION_MARK_AS_READ:
	case MATCHACTION_MARK_AS_UNREAD:
		g_snprintf(dest, destlen, "%s", command_str);
		return dest;

	case MATCHACTION_REDIRECT:
	case MATCHACTION_FORWARD:
	case MATCHACTION_FORWARD_AS_ATTACHMENT:
		g_snprintf(dest, destlen, "%s %d \"%s\"", command_str, action->account_id, action->destination); 
		return dest; 

	case MATCHACTION_COLOR:
		g_snprintf(dest, destlen, "%s %d", command_str, action->labelcolor);
		return dest;  
	case MATCHACTION_DELETE_ON_SERVER:
		g_snprintf(dest, destlen, "%s", command_str);
		return dest;
	default:
		return NULL;
	}
}

gchar * filteringprop_to_string(FilteringProp * prop)
{
	gchar *list_str;
	gchar *action_str;
	gchar *filtering_str;
	gchar  buf[256];

	action_str = filteringaction_to_string(buf, sizeof buf, prop->action);

	if (action_str == NULL)
		return NULL;

	list_str = matcherlist_to_string(prop->matchers);

	if (list_str == NULL)
		return NULL;

	filtering_str = g_strconcat(list_str, " ", action_str, NULL);
	g_free(list_str);

	return filtering_str;
}

void prefs_filtering_free(GSList * prefs_filtering)
{
 	while (prefs_filtering != NULL) {
 		FilteringProp * filtering = (FilteringProp *)
			prefs_filtering->data;
 		filteringprop_free(filtering);
 		prefs_filtering = g_slist_remove(prefs_filtering, filtering);
 	}
}

static gboolean prefs_filtering_free_func(GNode *node, gpointer data)
{
	FolderItem *item = node->data;

	if(!item->prefs)
		return FALSE;

	prefs_filtering_free(item->prefs->processing);
	item->prefs->processing = NULL;

	return FALSE;
}

void prefs_filtering_clear()
{
	GList * cur;

	for (cur = folder_get_list() ; cur != NULL ; cur = g_list_next(cur)) {
		Folder *folder;

		folder = (Folder *) cur->data;
		g_node_traverse(folder->node, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
				prefs_filtering_free_func, NULL);
	}

	prefs_filtering_free(global_processing);
	global_processing = NULL;
}
