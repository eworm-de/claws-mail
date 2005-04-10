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
#include <glib.h>
#include <glib/gi18n.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <gtk/gtk.h>
#include <stdio.h>

#include "utils.h"
#include "procheader.h"
#include "matcher.h"
#include "filtering.h"
#include "prefs_gtk.h"
#include "compose.h"

#define PREFSBUFSIZE		1024

GSList * pre_global_processing = NULL;
GSList * post_global_processing = NULL;
GSList * filtering_rules = NULL;

static gboolean filtering_is_final_action(FilteringAction *filtering_action);

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
				      gint labelcolor, gint score)
{
	FilteringAction * action;

	action = g_new0(FilteringAction, 1);

	action->type = type;
	action->account_id = account_id;
	if (destination) {
		action->destination	  = g_strdup(destination);
	} else {
		action->destination       = NULL;
	}
	action->labelcolor = labelcolor;	
        action->score = score;
	return action;
}

void filteringaction_free(FilteringAction * action)
{
	g_return_if_fail(action);
	if (action->destination)
		g_free(action->destination);
	g_free(action);
}

FilteringProp * filteringprop_new(MatcherList * matchers,
				  GSList * action_list)
{
	FilteringProp * filtering;

	filtering = g_new0(FilteringProp, 1);
	filtering->matchers = matchers;
	filtering->action_list = action_list;

	return filtering;
}

static FilteringAction * filteringaction_copy(FilteringAction * src)
{
        FilteringAction * new;
        
        new = g_new0(FilteringAction, 1);
        
	new->type = src->type;
	new->account_id = src->account_id;
	if (src->destination)
		new->destination = g_strdup(src->destination);
	else 
		new->destination = NULL;
	new->labelcolor = src->labelcolor;
	new->score = src->score;

        return new;
}

FilteringProp * filteringprop_copy(FilteringProp *src)
{
	FilteringProp * new;
	GSList *tmp;
	
	new = g_new0(FilteringProp, 1);
	new->matchers = g_new0(MatcherList, 1);

	for (tmp = src->matchers->matchers; tmp != NULL && tmp->data != NULL;) {
		MatcherProp *matcher = (MatcherProp *)tmp->data;
		
		new->matchers->matchers = g_slist_append(new->matchers->matchers,
						   matcherprop_copy(matcher));
		tmp = tmp->next;
	}

	new->matchers->bool_and = src->matchers->bool_and;

        new->action_list = NULL;

        for (tmp = src->action_list ; tmp != NULL ; tmp = tmp->next) {
                FilteringAction *filtering_action;
                
                filtering_action = tmp->data;
                
                new->action_list = g_slist_append(new->action_list,
                    filteringaction_copy(filtering_action));
        }

	return new;
}

void filteringprop_free(FilteringProp * prop)
{
        GSList * tmp;

	g_return_if_fail(prop);
	matcherlist_free(prop->matchers);
        
        for (tmp = prop->action_list ; tmp != NULL ; tmp = tmp->next) {
                filteringaction_free(tmp->data);
        }
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
		if (!dest_folder) {
			debug_print("*** folder not found '%s'\n",
				action->destination ?action->destination :"");
			return FALSE;
		}
		
		if (folder_item_move_msg(dest_folder, info) == -1) {
			debug_print("*** could not move message\n");
			return FALSE;
		}	

		return TRUE;

	case MATCHACTION_COPY:
		dest_folder =
			folder_find_item_from_identifier(action->destination);

		if (!dest_folder) {
			debug_print("*** folder not found '%s'\n",
				action->destination ?action->destination :"");
			return FALSE;
		}

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

	case MATCHACTION_LOCK:
		procmsg_msginfo_set_flags(info, MSG_LOCKED, 0);
		return TRUE;

	case MATCHACTION_UNLOCK:
		procmsg_msginfo_unset_flags(info, MSG_LOCKED, 0);	
		return TRUE;
		
	case MATCHACTION_MARK_AS_READ:
		procmsg_msginfo_unset_flags(info, MSG_UNREAD | MSG_NEW, 0);
		return TRUE;

	case MATCHACTION_MARK_AS_UNREAD:
		procmsg_msginfo_set_flags(info, MSG_UNREAD | MSG_NEW, 0);
		return TRUE;
	
	case MATCHACTION_COLOR:
		procmsg_msginfo_unset_flags(info, MSG_CLABEL_FLAG_MASK, 0); 
		procmsg_msginfo_set_flags(info, MSG_COLORLABEL_TO_FLAGS(action->labelcolor), 0);
		return TRUE;

	case MATCHACTION_FORWARD:
	case MATCHACTION_FORWARD_AS_ATTACHMENT:
		account = account_find_from_id(action->account_id);
		compose = compose_forward(account, info,
			action->type == MATCHACTION_FORWARD ? FALSE : TRUE,
			NULL, TRUE);
		compose_entry_append(compose, action->destination,
				     compose->account->protocol == A_NNTP
					    ? COMPOSE_NEWSGROUPS
					    : COMPOSE_TO);

		val = compose_send(compose);

		return val == 0 ? TRUE : FALSE;

	case MATCHACTION_REDIRECT:
		account = account_find_from_id(action->account_id);
		compose = compose_redirect(account, info);
		if (compose->account->protocol == A_NNTP)
			break;
		else
			compose_entry_append(compose, action->destination,
					     COMPOSE_TO);

		val = compose_send(compose);
		
		return val == 0 ? TRUE : FALSE;

	case MATCHACTION_EXECUTE:
		cmd = matching_build_command(action->destination, info);
		if (cmd == NULL)
			return FALSE;
		else {
			system(cmd);
			g_free(cmd);
		}
		return TRUE;

	case MATCHACTION_SET_SCORE:
		info->score = action->score;
		return TRUE;

	case MATCHACTION_CHANGE_SCORE:
		info->score += action->score;
		return TRUE;

	case MATCHACTION_STOP:
                return FALSE;

	case MATCHACTION_HIDE:
                info->hidden = TRUE;
                return TRUE;

	default:
		break;
	}
	return FALSE;
}

gboolean filteringaction_apply_action_list(GSList *action_list, MsgInfo *info)
{
	GSList *p;
	g_return_val_if_fail(action_list, FALSE);
	g_return_val_if_fail(info, FALSE);
	for (p = action_list; p && p->data; p = g_slist_next(p)) {
		FilteringAction *a = (FilteringAction *) p->data;
		if (filteringaction_apply(a, info)) {
			if (filtering_is_final_action(a))
				break;
		} else
			return FALSE;
		
	}
	return TRUE;
}

static gboolean filtering_match_condition(FilteringProp *filtering, MsgInfo *info)
{
	return matcherlist_match(filtering->matchers, info);
}

/*!
 *\brief	Apply a rule on message.
 *
 *\param	filtering List of filtering rules.
 *\param	info Message to apply rules on.
 *\param	final Variable returning TRUE or FALSE if one of the
 *		encountered actions was final. 
 *		See also \ref filtering_is_final_action.
 *
 *\return	gboolean TRUE to continue applying rules.
 */
static gboolean filtering_apply_rule(FilteringProp *filtering, MsgInfo *info,
    gboolean * final)
{
	gboolean result = TRUE;
	gchar    buf[50];
        GSList * tmp;
        
        * final = FALSE;
        for (tmp = filtering->action_list ; tmp != NULL ; tmp = tmp->next) {
                FilteringAction * action;
                
                action = tmp->data;
                
                if (FALSE == (result = filteringaction_apply(action, info))) {
                        g_warning("No further processing after rule %s\n",
                            filteringaction_to_string(buf, sizeof buf, action));
                }
                
                if (filtering_is_final_action(action)) {
                        * final = TRUE;
                        break;
                }
        }
	return result;
}

/*!
 *\brief	Check if an action is "final", i.e. should break further
 *		processing.
 *
 *\param	filtering_action Action to check.
 *
 *\return	gboolean TRUE if \a filtering_action is final.	
 */
static gboolean filtering_is_final_action(FilteringAction *filtering_action)
{
	switch(filtering_action->type) {
	case MATCHACTION_MOVE:
	case MATCHACTION_DELETE:
	case MATCHACTION_STOP:
		return TRUE; /* MsgInfo invalid for message */
	default:
		return FALSE;
	}
}

static gboolean filter_msginfo(GSList * filtering_list, MsgInfo * info)
{
	GSList	*l;
	gboolean final;
	gboolean apply_next;
	
	g_return_val_if_fail(info != NULL, TRUE);
	
	for (l = filtering_list, final = FALSE, apply_next = FALSE; l != NULL; l = g_slist_next(l)) {
		FilteringProp * filtering = (FilteringProp *) l->data;

		if (filtering_match_condition(filtering, info)) {
			apply_next = filtering_apply_rule(filtering, info, &final);
                        if (final)
                                break;
		}		
	}

	/* put in inbox if a final rule could not be applied, or
	 * the last rule was not a final one. */
	if ((final && !apply_next) || !final) {
		return FALSE;
	}

	return TRUE;
}

/*!
 *\brief	Filter a message against a list of rules.
 *
 *\param	flist List of filter rules.
 *\param	info Message.
 *
 *\return	gboolean TRUE if filter rules handled the message.
 *
 *\note		Returning FALSE means the message was not handled,
 *		and that the calling code should do the default
 *		processing. E.g. \ref inc.c::inc_start moves the 
 *		message to the inbox. 	
 */
gboolean filter_message_by_msginfo(GSList *flist, MsgInfo *info)
{
	return filter_msginfo(flist, info);
}

gchar *filteringaction_to_string(gchar *dest, gint destlen, FilteringAction *action)
{
	const gchar *command_str;
	gchar * quoted_dest;
	
	command_str = get_matchparser_tab_str(action->type);

	if (command_str == NULL)
		return NULL;

	switch(action->type) {
	case MATCHACTION_MOVE:
	case MATCHACTION_COPY:
	case MATCHACTION_EXECUTE:
		quoted_dest = matcher_quote_str(action->destination);
		g_snprintf(dest, destlen, "%s \"%s\"", command_str, quoted_dest);
		g_free(quoted_dest);
		return dest;

	case MATCHACTION_DELETE:
	case MATCHACTION_MARK:
	case MATCHACTION_UNMARK:
	case MATCHACTION_LOCK:
	case MATCHACTION_UNLOCK:
	case MATCHACTION_MARK_AS_READ:
	case MATCHACTION_MARK_AS_UNREAD:
	case MATCHACTION_STOP:
	case MATCHACTION_HIDE:
		g_snprintf(dest, destlen, "%s", command_str);
		return dest;

	case MATCHACTION_REDIRECT:
	case MATCHACTION_FORWARD:
	case MATCHACTION_FORWARD_AS_ATTACHMENT:
		quoted_dest = matcher_quote_str(action->destination);
		g_snprintf(dest, destlen, "%s %d \"%s\"", command_str, action->account_id, quoted_dest);
		g_free(quoted_dest);
		return dest; 

	case MATCHACTION_COLOR:
		g_snprintf(dest, destlen, "%s %d", command_str, action->labelcolor);
		return dest;  

	case MATCHACTION_CHANGE_SCORE:
	case MATCHACTION_SET_SCORE:
		g_snprintf(dest, destlen, "%s %d", command_str, action->score);
		return dest;  

	default:
		return NULL;
	}
}

gchar * filteringaction_list_to_string(GSList * action_list)
{
	gchar *action_list_str;
	gchar  buf[256];
        GSList * tmp;
	gchar *list_str;

        action_list_str = NULL;
        for (tmp = action_list ; tmp != NULL ; tmp = tmp->next) {
                gchar *action_str;
                FilteringAction * action;
                
                action = tmp->data;
                
                action_str = filteringaction_to_string(buf,
                    sizeof buf, action);
                
                if (action_list_str != NULL) {
                        list_str = g_strconcat(action_list_str, " ", action_str, NULL);
                        g_free(action_list_str);
                }
                else {
                        list_str = g_strdup(action_str);
                }
                action_list_str = list_str;
        }

        return action_list_str;
}

gchar * filteringprop_to_string(FilteringProp * prop)
{
	gchar *list_str;
	gchar *action_list_str;
	gchar *filtering_str;

        action_list_str = filteringaction_list_to_string(prop->action_list);

	if (action_list_str == NULL)
		return NULL;

	list_str = matcherlist_to_string(prop->matchers);

	if (list_str == NULL) {
                g_free(action_list_str);
		return NULL;
        }

	filtering_str = g_strconcat(list_str, " ", action_list_str, NULL);
	g_free(action_list_str);
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

	g_return_val_if_fail(item, FALSE);
	g_return_val_if_fail(item->prefs, FALSE);

	prefs_filtering_free(item->prefs->processing);
	item->prefs->processing = NULL;

	return FALSE;
}

void prefs_filtering_clear(void)
{
	GList * cur;

	for (cur = folder_get_list() ; cur != NULL ; cur = g_list_next(cur)) {
		Folder *folder;

		folder = (Folder *) cur->data;
		g_node_traverse(folder->node, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
				prefs_filtering_free_func, NULL);
	}

	prefs_filtering_free(filtering_rules);
	filtering_rules = NULL;
	prefs_filtering_free(pre_global_processing);
	pre_global_processing = NULL;
	prefs_filtering_free(post_global_processing);
	post_global_processing = NULL;
}

void prefs_filtering_clear_folder(Folder *folder)
{
	g_return_if_fail(folder);
	g_return_if_fail(folder->node);

	g_node_traverse(folder->node, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
			prefs_filtering_free_func, NULL);
	/* FIXME: Note folder settings were changed, where the updates? */
}

