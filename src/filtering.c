/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2001 Hiroyuki Yamamoto & The Sylpheed Claws Team
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

/* 
 * initital	Hoa		initial
 * 07/16/01	Alfons		fix bugs
 * 07/18/01	Alfons		rewrite things
 */

/* 07/18/01 (alfons) 
 * 
 * filter_message() is no longer the entry point for filtering. it's
 * replaced with filter_incoming_message() which does not trust
 * on MsgInfo's from a folder. instead it directly works with
 * the temporary file.  
 * updating the mark file is a lot easier now, because we can do that
 * right after a call to folder_item_add_msg(). 
 */ 

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include "intl.h"
#include "utils.h"
#include "defs.h"
#include "procheader.h"
#include "matcher.h"
#include "filtering.h"
#include "prefs.h"
#include "compose.h"

#define PREFSBUFSIZE		1024

GSList * prefs_filtering = NULL;

FilteringAction * filteringaction_new(int type, int account_id,
				      gchar * destination,
				      gint labelcolor)
{
	FilteringAction * action;

	action = g_new0(FilteringAction, 1);

	action->type = type;
	action->account_id = account_id;
	if (destination)
		action->destination = g_strdup(destination);
	action->labelcolor = labelcolor;	
	return action;
}

void filteringaction_free(FilteringAction * action)
{
	if (action->destination)
		g_free(action->destination);
	g_free(action);
}

FilteringAction * filteringaction_parse(gchar ** str)
{
	FilteringAction * action;
	gchar * tmp;
	gchar * destination = NULL;
	gint account_id = 0;
	gint key;
	gint labelcolor = 0;

	tmp = * str;

	key = matcher_parse_keyword(&tmp);

	switch (key) {
	case MATCHING_ACTION_MOVE:
	case MATCHING_ACTION_COPY:
	case MATCHING_EXECUTE:
		destination = matcher_parse_str(&tmp);
		if (tmp == NULL) {
			* str = NULL;
			return NULL;
		}
		break;
	case MATCHING_ACTION_DELETE:
		break;
	case MATCHING_ACTION_MARK:
		break;
	case MATCHING_ACTION_MARK_AS_READ:
		break;
	case MATCHING_ACTION_UNMARK:
		break;
	case MATCHING_ACTION_MARK_AS_UNREAD:
		break;
	case MATCHING_ACTION_FORWARD:
	case MATCHING_ACTION_FORWARD_AS_ATTACHMENT:
		account_id = matcher_parse_number(&tmp);
		if (tmp == NULL) {
			* str = NULL;
			return NULL;
		}

		destination = matcher_parse_str(&tmp);
		if (tmp == NULL) {
			* str = NULL;
			return NULL;
		}

		break;
	case MATCHING_ACTION_COLOR:
		labelcolor = matcher_parse_number(&tmp);
		if (tmp == NULL) {
			*str = NULL;
			return NULL;
		}

		break;
	default:
		* str = NULL;
		return NULL;
	}

	* str = tmp;

	action = filteringaction_new(key, account_id, destination, labelcolor);

	return action;
}

FilteringProp * filteringprop_parse(gchar ** str)
{
	gchar * tmp;
	gint key;
	FilteringProp * filtering;
	MatcherList * matchers;
	FilteringAction * action;
	
	tmp = * str;

	matchers = matcherlist_parse(&tmp);
	if (tmp == NULL) {
		* str = NULL;
		return NULL;
	}

	if (tmp == NULL) {
		matcherlist_free(matchers);
		* str = NULL;
		return NULL;
	}

	action = filteringaction_parse(&tmp);
	if (tmp == NULL) {
		matcherlist_free(matchers);
		* str = NULL;
		return NULL;
	}

	filtering = filteringprop_new(matchers, action);

	* str = tmp;
	return filtering;
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

void filteringprop_free(FilteringProp * prop)
{
	matcherlist_free(prop->matchers);
	filteringaction_free(prop->action);
	g_free(prop);
}

/* filteringaction_update_mark() - updates a mark for a message. note that
 * the message should not have been moved or copied. remember that the
 * procmsg_open_mark_file(PATH, TRUE) actually _appends_ a new record.
 */
static gboolean filteringaction_update_mark(MsgInfo * info)
{
	gchar * dest_path;
	FILE * fp;

	if (info->folder->folder->type == F_MH) {
		dest_path = folder_item_get_path(info->folder);
		if (!is_dir_exist(dest_path))
			make_dir_hier(dest_path);
		
		if (dest_path == NULL) {
			g_warning(_("Can't open mark file.\n"));
			return FALSE;
		}
		
		if ((fp = procmsg_open_mark_file(dest_path, TRUE))
		    == NULL) {
			g_warning(_("Can't open mark file.\n"));
			return FALSE;
		}
		
		procmsg_write_flags(info, fp);
		fclose(fp);
		return TRUE;
	}
	return FALSE;
}

static inline gint strlen_with_check(const gchar *expr, gint fline, const gchar *str)
{
	if (str) 
		return strlen(str);
	else {
		debug_print("%s(%d) - invalid string %s\n", __FILE__, fline, expr);
		return 0;
	}
}

#define STRLEN_WITH_CHECK(expr) \
	strlen_with_check(#expr, __LINE__, expr)
	
static gchar * filteringaction_execute_command(gchar * cmd, MsgInfo * info)
{
	gchar * s = cmd;
	gchar * filename = NULL;
	gchar * processed_cmd;
	gchar * p;
	gint size;

	matcher_unescape_str(cmd);

	size = strlen(cmd) + 1;
	while (*s != '\0') {
		if (*s == '%') {
			s++;
			switch (*s) {
			case '%':
				size -= 1;
				break;
			case 's': /* subject */
				size += STRLEN_WITH_CHECK(info->subject) - 2;
				break;
			case 'f': /* from */
				size += STRLEN_WITH_CHECK(info->from) - 2;
				break;
			case 't': /* to */
				size += STRLEN_WITH_CHECK(info->to) - 2;
				break;
			case 'c': /* cc */
				size += STRLEN_WITH_CHECK(info->cc) - 2;
				break;
			case 'd': /* date */
				size += STRLEN_WITH_CHECK(info->date) - 2;
				break;
			case 'i': /* message-id */
				size += STRLEN_WITH_CHECK(info->msgid) - 2;
				break;
			case 'n': /* newsgroups */
				size += STRLEN_WITH_CHECK(info->newsgroups) - 2;
				break;
			case 'r': /* references */
				size += STRLEN_WITH_CHECK(info->references) - 2;
				break;
			case 'F': /* file */
				if (MSG_IS_FILTERING(info->flags))
					filename = g_strdup(info->folder);
				else
					filename = folder_item_fetch_msg(info->folder, info->msgnum);
				
				if (filename == NULL) {
					g_warning(_("filename is not set"));
					return NULL;
				}
				else
					size += strlen(filename) - 2;
				break;
			}
			s++;
		}
		else s++;
	}


	processed_cmd = g_new0(gchar, size);
	s = cmd;
	p = processed_cmd;

	while (*s != '\0') {
		if (*s == '%') {
			s++;
			switch (*s) {
			case '%':
				*p = '%';
				p++;
				break;
			case 's': /* subject */
				if (info->subject != NULL)
					strcpy(p, info->subject);
				else
					strcpy(p, "(none)");
				p += strlen(p);
				break;
			case 'f': /* from */
				if (info->from != NULL)
					strcpy(p, info->from);
				else
					strcpy(p, "(none)");
				p += strlen(p);
				break;
			case 't': /* to */
				if (info->to != NULL)
					strcpy(p, info->to);
				else
					strcpy(p, "(none)");
				p += strlen(p);
				break;
			case 'c': /* cc */
				if (info->cc != NULL)
					strcpy(p, info->cc);
				else
					strcpy(p, "(none)");
				p += strlen(p);
				break;
			case 'd': /* date */
				if (info->date != NULL)
					strcpy(p, info->date);
				else
					strcpy(p, "(none)");
				p += strlen(p);
				break;
			case 'i': /* message-id */
				if (info->msgid != NULL)
					strcpy(p, info->msgid);
				else
					strcpy(p, "(none)");
				p += strlen(p);
				break;
			case 'n': /* newsgroups */
				if (info->newsgroups != NULL)
					strcpy(p, info->newsgroups);
				else
					strcpy(p, "(none)");
				p += strlen(p);
				break;
			case 'r': /* references */
				if (info->references != NULL)
					strcpy(p, info->references);
				else
					strcpy(p, "(none)");
				p += strlen(p);
				break;
			case 'F': /* file */
				strcpy(p, filename);
				p += strlen(p);
				break;
			default:
				*p = '%';
				p++;
				*p = *s;
				p++;
				break;
			}
			s++;
		}
		else {
			*p = *s;
			p++;
			s++;
		}
	}

	g_free(filename);
	return processed_cmd;
}

/*
  fitleringaction_apply
  runs the action on one MsgInfo
  return value : return TRUE if the action could be applied
*/

#define CHANGE_FLAGS(msginfo) \
{ \
if (msginfo->folder->folder->change_flags != NULL) \
msginfo->folder->folder->change_flags(msginfo->folder->folder, \
				      msginfo->folder, \
				      msginfo); \
}

static gboolean filteringaction_apply(FilteringAction * action, MsgInfo * info,
				      GHashTable *folder_table)
{
	FolderItem * dest_folder;
	gint val;
	Compose * compose;
	PrefsAccount * account;
	gchar * cmd;

	switch(action->type) {
	case MATCHING_ACTION_MOVE:
		dest_folder =
			folder_find_item_from_identifier(action->destination);
		if (!dest_folder)
			return FALSE;

		if (folder_item_move_msg(dest_folder, info) == -1)
			return FALSE;

		/* WRONG: can not update the mark, because the message has 
		 * been moved. info pertains to original location. 
		 * folder_item_move_msg() already updated the mark for the
		 * destination folder.
		info->flags = 0;
		filteringaction_update_mark(info);
		 */
		if (folder_table) {
			val = GPOINTER_TO_INT(g_hash_table_lookup
					      (folder_table, dest_folder));
			if (val == 0) {
				folder_item_scan(dest_folder);
				g_hash_table_insert(folder_table, dest_folder,
						    GINT_TO_POINTER(1));
			}
			val = GPOINTER_TO_INT(g_hash_table_lookup
					      (folder_table, info->folder));
			if (val == 0) {
				folder_item_scan(info->folder);
				g_hash_table_insert(folder_table, info->folder,
						    GINT_TO_POINTER(1));
			}
		}
		return TRUE;

	case MATCHING_ACTION_COPY:
		dest_folder =
			folder_find_item_from_identifier(action->destination);

		if (!dest_folder)
			return FALSE;

		/* NOTE: the following call *will* update the mark file for
		 * the destination folder. but the original message will
		 * still be there in the inbox. */

		if (folder_item_copy_msg(dest_folder, info) == -1)
			return FALSE;

		if (folder_table) {
			val = GPOINTER_TO_INT(g_hash_table_lookup
					      (folder_table, dest_folder));
			if (val == 0) {
				folder_item_scan(dest_folder);
				g_hash_table_insert(folder_table, dest_folder,
						    GINT_TO_POINTER(1));
			}
		}
			
		return TRUE;

	case MATCHING_ACTION_DELETE:
		if (folder_item_remove_msg(info->folder, info->msgnum) == -1)
			return FALSE;

		/* WRONG: can not update the mark. this would actually add
		 * a bogus record to the mark file for the message's original 
		 * folder. 
		info->flags = 0;
		filteringaction_update_mark(info);
		 */

		return TRUE;

	case MATCHING_ACTION_MARK:
		MSG_SET_PERM_FLAGS(info->flags, MSG_MARKED);
		debug_print("*** MARKING message %d, in folder %s, \"%s\"\n",
			    info->msgnum, info->folder->name,
			    info->subject ? info->subject : "<none>");
		filteringaction_update_mark(info);

		CHANGE_FLAGS(info);

		return TRUE;

	case MATCHING_ACTION_UNMARK:
		MSG_UNSET_PERM_FLAGS(info->flags, MSG_MARKED);
		filteringaction_update_mark(info);

		CHANGE_FLAGS(info);

		return TRUE;
		
	case MATCHING_ACTION_MARK_AS_READ:
		MSG_UNSET_PERM_FLAGS(info->flags, MSG_UNREAD | MSG_NEW);
		filteringaction_update_mark(info);

		CHANGE_FLAGS(info);

		return TRUE;

	case MATCHING_ACTION_MARK_AS_UNREAD:
		MSG_SET_PERM_FLAGS(info->flags, MSG_UNREAD | MSG_NEW);
		filteringaction_update_mark(info);

		CHANGE_FLAGS(info);
		
		return TRUE;

	case MATCHING_ACTION_FORWARD:

		account = account_find_from_id(action->account_id);
		compose = compose_forward(account, info, FALSE);
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

	case MATCHING_ACTION_FORWARD_AS_ATTACHMENT:

		account = account_find_from_id(action->account_id);
		compose = compose_forward(account, info, TRUE);
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

	case MATCHING_EXECUTE:

		cmd = matching_build_command(action->destination, info);
		if (cmd == NULL)
			return TRUE;
		else {
			system(cmd);
			g_free(cmd);
		}

		return TRUE;

	default:
		return FALSE;
	}
}

/* filteringprop_apply() - runs the action on one MsgInfo if it matches the 
 * criterium. certain actions can be followed by other actions. in this
 * case the function returns FALSE. if an action can not be followed
 * by others, the function returns TRUE.
 *
 * remember that this is because of the fact that msg flags are always
 * _appended_ to mark files. currently sylpheed does not insert messages 
 * at a certain index. 
 * now, after having performed a certain action, the MsgInfo is still
 * valid for the message. in *this* case the function returns FALSE.
 */
static gboolean filteringprop_apply(FilteringProp * filtering, MsgInfo * info,
				    GHashTable *folder_table)
{
	if (matcherlist_match(filtering->matchers, info)) {
		gint result;
		gchar * action_str;

		result = TRUE;

		result = filteringaction_apply(filtering->action, info,
					       folder_table);
		action_str =
			filteringaction_to_string(filtering->action);
		if (!result) {
			g_warning(_("action %s could not be applied"),
				  action_str);
		}
		else {
			debug_print(_("message %i %s..."),
				      info->msgnum, action_str);
		}

		g_free(action_str);

		switch(filtering->action->type) {
		case MATCHING_ACTION_MOVE:
		case MATCHING_ACTION_DELETE:
			return TRUE; /* MsgInfo invalid for message */
		case MATCHING_EXECUTE:
		case MATCHING_ACTION_COPY:
		case MATCHING_ACTION_MARK:
		case MATCHING_ACTION_MARK_AS_READ:
		case MATCHING_ACTION_UNMARK:
		case MATCHING_ACTION_MARK_AS_UNREAD:
		case MATCHING_ACTION_FORWARD:
		case MATCHING_ACTION_FORWARD_AS_ATTACHMENT:
			return FALSE; /* MsgInfo still valid for message */
		default:
			return FALSE;
		}
	}
	else
		return FALSE;
}

void filter_msginfo(GSList * filtering_list, MsgInfo * info,
		    GHashTable *folder_table)
{
	GSList * l;

	if (info == NULL) {
		g_warning(_("msginfo is not set"));
		return;
	}
	
	for(l = filtering_list ; l != NULL ; l = g_slist_next(l)) {
		FilteringProp * filtering = (FilteringProp *) l->data;
		
		if (filteringprop_apply(filtering, info, folder_table))
			break;
	}
}

void filter_msginfo_move_or_delete(GSList * filtering_list, MsgInfo * info,
				   GHashTable *folder_table)
{
	GSList * l;

	if (info == NULL) {
		g_warning(_("msginfo is not set"));
		return;
	}
	
	for(l = filtering_list ; l != NULL ; l = g_slist_next(l)) {
		FilteringProp * filtering = (FilteringProp *) l->data;

		switch (filtering->action->type) {
		case MATCHING_ACTION_MOVE:
		case MATCHING_ACTION_DELETE:
			if (filteringprop_apply(filtering, info, folder_table))
				return;
		}
	}
}

void filter_message(GSList * filtering_list, FolderItem * item,
		    gint msgnum, GHashTable *folder_table)
{
	MsgInfo * msginfo;
	gchar * filename;
	MsgFlags  msgflags = { 0, 0 };

	if (item == NULL) {
		g_warning(_("folderitem not set"));
		return;
	}

	filename = folder_item_fetch_msg(item, msgnum);

	if (filename == NULL) {
		g_warning(_("filename is not set"));
		return;
	}

	msginfo = procheader_parse(filename, msgflags, TRUE);
	
	g_free(filename);

	if (msginfo == NULL) {
		g_warning(_("could not get info for %s"), filename);
		return;
	}

	msginfo->folder = item;
	msginfo->msgnum = msgnum;

	filter_msginfo(filtering_list, msginfo, folder_table);
}


/******************************************************************************/

/* revised filtering system.  
 * 
 * 07/18/01	alfons		initial revisement
 */

/* (alfons)
 * 
 * the revised filtering system does not rely on a message being "registered"
 * in one of sylpheed's folders. it now uses the file sylpheed created on
 * incorporation of mail. 
 * i do use MsgInfo data; that's needed by the matcher, but for the rest
 * the MsgInfo is not really used. */
 

/* add_mark() - adds a mark for a file */
static void add_mark(FolderItem *folder, gint msgnum, MsgPermFlags flags)
{
	gchar * dest_path;
	FILE * fp;

	if (folder->folder->type == F_MH) {
		dest_path = folder_item_get_path(folder);
		if (!is_dir_exist(dest_path))
			make_dir_hier(dest_path);
		
		if (dest_path == NULL) {
			g_warning(_("Can't open mark file.\n"));
			return;
		}
		
		if ((fp = procmsg_open_mark_file(dest_path, TRUE))
		    == NULL) {
			g_warning(_("Can't open mark file.\n"));
			return;
		}

		/* TODO: straight from procmsg.c:procmsg_write_flags()
		 * should update this when mark file version changes */
#if MARK_VERSION == 2		 
		WRITE_CACHE_DATA_INT(msgnum, fp);
		WRITE_CACHE_DATA_INT(flags, fp);
#else
#error should rewrite the above for new  mark version	
		/* paste the source code of procmsg.c:procmsg_write_flags() */
#endif
		fclose(fp);
		return;
	}
	return;
}

/* prepare_destination() - prepares destination folder by registering it 
 * in the global folder table (if not already there). it returns TRUE if
 * successful. it also returns the FolderItem for the destination */
static gboolean prepare_destination(const gchar *destination, FolderItem **item, 
				    GHashTable *folder_table)
{
	gint val;
	FolderItem *result;

	result = folder_find_item_from_identifier(destination);
	if (!result)
		return FALSE;
	if (folder_table) {
		*item = result;
		val = GPOINTER_TO_INT(g_hash_table_lookup(folder_table, *item));
		if (val == 0) {
			folder_item_scan(*item);
			g_hash_table_insert(folder_table, *item, GINT_TO_POINTER(1));
		}
	}
	return TRUE;
}

/* filter_incoming_perform_actions() - performs actions on incoming
 * message. this function handles updating marks a little bit smarter;
 * remember that marks can only be appended. */
static gboolean filter_incoming_perform_actions(FolderItem *default_folder, 
						MsgInfo    *msginfo,
						GHashTable *folder_table)
{
	/* matching actions for msginfo */
	struct matching_action {
		FilteringAction 	*action;
		struct matching_action	*next;
	};

	struct matching_action  ma_head = { NULL, NULL };
	struct matching_action *ma_tail = &ma_head;

	/* need this for the forwarding stuff */
	PrefsAccount *account;
	Compose      *compose;
	MsgFlags      tmp;
	gchar        *fwd_msg_name;
	gint	      val;
	MsgInfo	     *fwd_msg;

	MsgFlags     markflags = { 0, 0 };

	/* use the global prefs_filtering list */
	GSList *     list = prefs_filtering;

	/* things we get after having added a message to a folder */
	FolderItem  *dest_folder;
	gint         msgnum;
	
	/* after performing certain action, we may have to copy 
	 * the message to inbox too */
	gboolean     copy_to_inbox_too = TRUE;
	
	/* filename is only put in msginfo->folder if MSG_FILTERING is set. */
	const gchar *filename = (gchar *) msginfo->folder;
	
	MsgPermFlags flags;
	gboolean     stop;
	gchar	     *cmd;

	/* build list of matching actions */
	for (; list != NULL; list = g_slist_next(list)) {
		FilteringProp *prop = (FilteringProp *)	list->data;
		
		if (!matcherlist_match(prop->matchers, msginfo))
			continue;

		if (NULL == (ma_tail->next = alloca(sizeof(struct matching_action)))) {
			debug_print(_("action allocation error\n"));
			break;
		}

		debug_print("*** action %d\n", prop->action->type);

		ma_tail->action = prop->action;
		ma_tail->next->action = NULL;
		ma_tail->next->next = NULL;
		ma_tail = ma_tail->next;
	}

	debug_print("*** collecting marks\n");		
	
	/* collect all marks */
	for (ma_tail = &ma_head, stop = FALSE; ma_tail->action && !stop; ma_tail = ma_tail->next) {
		switch (ma_tail->action->type) {
		
		case MATCHING_ACTION_MARK:
			MSG_SET_PERM_FLAGS(markflags, MSG_MARKED);
			break;
			
		case MATCHING_ACTION_UNMARK:
			MSG_UNSET_PERM_FLAGS(markflags, MSG_MARKED);
			break;
		
		case MATCHING_ACTION_MARK_AS_READ:
			MSG_UNSET_PERM_FLAGS(markflags, MSG_UNREAD);
			break;

		case MATCHING_ACTION_MARK_AS_UNREAD:			
			MSG_SET_PERM_FLAGS(markflags, MSG_UNREAD);
			break;

		case MATCHING_ACTION_COLOR:
			MSG_SET_LABEL_VALUE(markflags, ma_tail->action->labelcolor);
			break;

		/* UNCONTINUABLE */
		case MATCHING_ACTION_FORWARD:
		case MATCHING_ACTION_FORWARD_AS_ATTACHMENT:
		case MATCHING_ACTION_MOVE:
		case MATCHING_ACTION_DELETE:
			stop = TRUE;
			break;
		
		default:
			break;
		}
	}


	/* msgnum & dest_folder should have valid values after a succesful
	 * drop; in this case there is a MsgInfo available */
	msgnum      = -1;
	dest_folder = NULL;

	debug_print("*** performing actions\n"); 
	
	for (ma_tail = &ma_head ; ma_tail->action; ma_tail = ma_tail->next) {

		/* there are variables you have to use when defining new actions:
		 *
		 * copy_to_inbox_too - if the original message should be copied to the inbox 
		 *                     (default_folder) too.
		 * 
		 * also note that after dropping it to a folder (folder_item_add_msg()) you have
		 * to mark the message, just to make sure any defined mark actions are applied. */
		 
#define ACTION	(ma_tail->action->type) 

		/* C O N T I N U A B L E */

		if (MATCHING_ACTION_COPY == ACTION) {
			debug_print("*** performing copy\n");
			copy_to_inbox_too = TRUE;
			if (!prepare_destination(ma_tail->action->destination, &dest_folder, folder_table)) {
				debug_print("Rule failed: unknown destination %s\n", ma_tail->action->destination);
				continue; /* try next action */
			}
			if (0 > (msgnum = folder_item_add_msg(dest_folder, filename, FALSE))) {
				debug_print(_("Rule failed: could not copy to folder %s\n"),
					    ma_tail->action->destination);
				continue; /* try next action */	    
			}   
			flags = msginfo->flags.perm_flags | markflags.perm_flags;
			add_mark(dest_folder, msgnum, flags);
		}		
		else if (MATCHING_EXECUTE == ACTION) {
			debug_print("*** performing exec\n");
			copy_to_inbox_too = TRUE;
			
			/* matching_build_command() knows about filtering */
			cmd = matching_build_command(ma_tail->action->destination, msginfo);
			if (cmd) { 
				system(cmd);
				g_free(cmd);
			}
			else
				debug_print(_("Rule failed: no command line\n"));
		} 

		/* U N C O N T I N U A B L E */
		
		else if (MATCHING_ACTION_FORWARD == ACTION
		||       MATCHING_ACTION_FORWARD_AS_ATTACHMENT == ACTION) {
			debug_print("*** performing forward\n");

			/* forwarding messages is complicated because there's currently no 
			 * way to forward a message using "filenames"; you can only forward
			 * a message if you have its MsgInfo. this means we have to drop
			 * the message first */ 
			if (0 > (msgnum = folder_item_add_msg(default_folder, filename, FALSE))) {
				debug_print(_("Rule failed: could not forward\n"));
				copy_to_inbox_too = TRUE;
				continue;
			}
				
			flags = msginfo->flags.perm_flags | markflags.perm_flags;
			flags |= MSG_FORWARDED;
			add_mark(default_folder, msgnum, flags);

			/* grab the dropped message */
			fwd_msg_name = folder_item_fetch_msg(default_folder, msgnum);

			tmp.perm_flags = tmp.tmp_flags = 0;
			fwd_msg = procheader_parse(fwd_msg_name, tmp, TRUE);

			fwd_msg->folder = default_folder;
			fwd_msg->msgnum = msgnum;

			/* do the compose_XXX stuff */
			account = account_find_from_id(ma_tail->action->account_id);
			compose = compose_forward(account, fwd_msg, ACTION == MATCHING_ACTION_FORWARD ? FALSE : TRUE);
			if (compose->account->protocol == A_NNTP)
				compose_entry_append(compose, ma_tail->action->destination,
						     COMPOSE_NEWSGROUPS);
			else
				compose_entry_append(compose, ma_tail->action->destination,
						     COMPOSE_TO);

			compose_send(compose);

			procmsg_msginfo_free(fwd_msg);
			
			gtk_widget_destroy(compose->window);
			break;
		}			
		else if (MATCHING_ACTION_DELETE == ACTION) {
			debug_print("*** performing delete\n");
			copy_to_inbox_too = FALSE;

			/* drop to Trash */
			dest_folder = folder_get_default_trash();
			msgnum = folder_item_add_msg(dest_folder, filename, FALSE);
			if (msgnum < 0) {
				debug_print(_("Rule failed: could not move to trash"));
				copy_to_inbox_too = TRUE;	    
			}
			else {
				flags = msginfo->flags.perm_flags | markflags.perm_flags;
				add_mark(dest_folder, msgnum, flags);
			}
			break;				
		}
		else if (MATCHING_ACTION_MOVE == ACTION) {
			debug_print("*** performing move\n");
			copy_to_inbox_too = FALSE;
			
			if (!prepare_destination(ma_tail->action->destination, &dest_folder, folder_table)) {
				copy_to_inbox_too = TRUE;
				break;
			}
				
			msgnum = folder_item_add_msg(dest_folder, filename, FALSE);
			if (msgnum < 0) {
				debug_print(_("Rule failed: could not move to folder %s\n"),
					    ma_tail->action->destination);
				copy_to_inbox_too = TRUE;					   
				break;
			}   
			
			flags = msginfo->flags.perm_flags | markflags.perm_flags;
			add_mark(dest_folder, msgnum, flags);
			break;
		}
	}

	/* may need to copy it to inbox too */
	if (copy_to_inbox_too) {
		debug_print("*** performing inbox copy\n");
		msgnum = folder_item_add_msg(default_folder, filename, TRUE);
		if (msgnum < 0) {
			debug_print(_("error copying incoming file %s to inbox\n"), 
				    filename);
			return FALSE;				    
		}
		flags = msginfo->flags.perm_flags | markflags.perm_flags;
		add_mark(default_folder, msgnum, flags);
	}
	else {
		debug_print("*** unlinking\n");
		if (unlink(filename) < 0) 
			debug_print(_("error deleting incoming message file\n"));
	}

#undef ACTION

	return TRUE;
}

static void filter_incoming_msginfo(FolderItem *default_folder, MsgInfo *msginfo, 
				    GHashTable *folder_table)
{
	filter_incoming_perform_actions(default_folder, msginfo, folder_table);	
}

/* filter_incoming_message() - tries to apply a filter on one incoming message.
 * it also handles the case when there's no filter match */
void filter_incoming_message(FolderItem *default_folder, const gchar *file_name, 
			     GHashTable *folder_table)
{
	MsgInfo *msginfo;
	MsgFlags msgflags = { 0, 0 };
	
	/* make an "uncomplete" msginfo. it's incomplete because it doesn't
	 * have a message number / folder yet. */
	if (NULL == (msginfo = procheader_parse(file_name, msgflags, TRUE))) {
		g_warning(_("error filtering incoming message %s\n"), 
			  file_name);
		return;		
	}

	/* let matcher know that this is a message that has no
	 * valid body data yet. */
	MSG_SET_TMP_FLAGS(msginfo->flags, MSG_FILTERING);
	msginfo->folder = g_strdup(file_name);
	msginfo->msgnum = 0;

	filter_incoming_msginfo(default_folder, msginfo, folder_table);

	g_free(msginfo->folder);
	procmsg_msginfo_free(msginfo);
}

/******************************************************************************/

void prefs_filtering_read_config(void)
{
	gchar *rcpath;
	FILE *fp;
	gchar buf[PREFSBUFSIZE];

	debug_print(_("Reading filtering configuration...\n"));

	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
			     FILTERING_RC, NULL);
	if ((fp = fopen(rcpath, "r")) == NULL) {
		if (ENOENT != errno) FILE_OP_ERROR(rcpath, "fopen");
		g_free(rcpath);
		prefs_filtering = NULL;
		return;
	}
	g_free(rcpath);

 	/* remove all filtering */
 	while (prefs_filtering != NULL) {
 		FilteringProp * filtering =
			(FilteringProp *) prefs_filtering->data;
 		filteringprop_free(filtering);
 		prefs_filtering = g_slist_remove(prefs_filtering, filtering);
 	}

 	while (fgets(buf, sizeof(buf), fp) != NULL) {
 		FilteringProp * filtering;
		gchar * tmp;

 		g_strchomp(buf);

		if ((*buf != '#') && (*buf != '\0')) {
			tmp = buf;
			filtering = filteringprop_parse(&tmp);
			if (tmp != NULL) {
				prefs_filtering =
					g_slist_append(prefs_filtering,
						       filtering);
			}
			else {
				/* debug */
				g_warning(_("syntax error : %s\n"), buf);
			}
		}
 	}

 	fclose(fp);
}

gchar * filteringaction_to_string(FilteringAction * action)
{
	gchar * command_str;
	gint i;
	gchar * account_id_str;
	gchar * labelcolor_str;

	/* FIXME: use g_sprintf() throughout */

	command_str = NULL;
	command_str = get_matchparser_tab_str(action->type);

	if (command_str == NULL)
		return NULL;

	switch(action->type) {
	case MATCHING_ACTION_MOVE:
	case MATCHING_ACTION_COPY:
	case MATCHING_EXECUTE:
		return g_strconcat(command_str, " \"", action->destination,
				   "\"", NULL);

	case MATCHING_ACTION_DELETE:
	case MATCHING_ACTION_MARK:
	case MATCHING_ACTION_UNMARK:
	case MATCHING_ACTION_MARK_AS_READ:
	case MATCHING_ACTION_MARK_AS_UNREAD:
		return g_strdup(command_str);
		break;

	case MATCHING_ACTION_FORWARD:
	case MATCHING_ACTION_FORWARD_AS_ATTACHMENT:
		account_id_str = itos(action->account_id);
		return g_strconcat(command_str, " ", account_id_str,
				   " \"", action->destination, "\"", NULL);

	case MATCHING_ACTION_COLOR:
		labelcolor_str = itos(action->labelcolor);
		return g_strconcat(command_str, " ", labelcolor_str, NULL);  

	default:
		return NULL;
	}
}

gchar * filteringprop_to_string(FilteringProp * prop)
{
	gchar * list_str;
	gchar * action_str;
	gchar * filtering_str;

	action_str = filteringaction_to_string(prop->action);

	if (action_str == NULL)
		return NULL;

	list_str = matcherlist_to_string(prop->matchers);

	if (list_str == NULL) {
		g_free(action_str);
		return NULL;
	}

	filtering_str = g_strconcat(list_str, " ", action_str, NULL);
	g_free(list_str);
	g_free(action_str);

	return filtering_str;
}

void prefs_filtering_write_config(void)
{
	gchar *rcpath;
	PrefFile *pfile;
	GSList *cur;
	FilteringProp * prop;

	debug_print(_("Writing filtering configuration...\n"));

	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, FILTERING_RC, NULL);

	if ((pfile = prefs_write_open(rcpath)) == NULL) {
		g_warning(_("failed to write configuration to file\n"));
		g_free(rcpath);
		return;
	}

	for (cur = prefs_filtering; cur != NULL; cur = cur->next) {
		gchar *filtering_str;

		prop = (FilteringProp *) cur->data;
		filtering_str = filteringprop_to_string(prop);
		
		if (fputs(filtering_str, pfile->fp) == EOF ||
		    fputc('\n', pfile->fp) == EOF) {
			FILE_OP_ERROR(rcpath, "fputs || fputc");
			prefs_write_close_revert(pfile);
			g_free(rcpath);
			g_free(filtering_str);
			return;
		}
		g_free(filtering_str);
	}

	g_free(rcpath);

	if (prefs_write_close(pfile) < 0) {
		g_warning(_("failed to write configuration to file\n"));
		return;
	}
}
