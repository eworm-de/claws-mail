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

/* (alfons) - Just a quick note of how this filtering module works on 
 * new (arriving) messages.
 * 
 * 1) as an initialization step, code in inc.c and mbox.c set up the 
 *    drop folder to the inbox (see inc.c and mbox.c).
 *
 * 2) the message is actually being copied to the drop folder using
 *    folder_item_add_msg(dropfolder, file, TRUE). this function
 *    eventually calls mh->add_msg(). however, the important thing
 *    about this function is, is that the folder is not yet updated
 *    to reflect the copy. i don't know about the validity of this
 *    assumption, however, the filtering code assumes this and
 *    updates the marks itself.
 *
 * 3) technically there's nothing wrong with the matcher (the 
 *    piece of code which matches search strings). there's
 *    one gotcha in procmsg.c:procmsg_get_message_file(): it
 *    only reads a message file based on a MsgInfo. for design
 *    reasons the filtering system should read directly from
 *    a file (based on the file's name).
 *
 * 4) after the matcher sorts out any matches, it looks at the
 *    action. this part again pushes the folder system design
 *    to its limits. based on the assumption in 2), the matcher
 *    knows the message has not been added to the folder system yet.
 *    it can happily update mark files, and in fact it does.
 * 
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
	case MATCHACTION_MOVE:
		dest_folder =
			folder_find_item_from_identifier(action->destination);
		if (!dest_folder)
			return FALSE;
		
		if (folder_item_move_msg(dest_folder, info) == -1) {
			return FALSE;
		}	

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

	case MATCHACTION_COPY:
		dest_folder =
			folder_find_item_from_identifier(action->destination);

		if (!dest_folder)
			return FALSE;

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

	case MATCHACTION_DELETE:
		if (folder_item_remove_msg(info->folder, info->msgnum) == -1)
			return FALSE;
		return TRUE;

	case MATCHACTION_MARK:
		MSG_SET_PERM_FLAGS(info->flags, MSG_MARKED);
		return TRUE;

	case MATCHACTION_UNMARK:
		MSG_UNSET_PERM_FLAGS(info->flags, MSG_MARKED);
		return TRUE;
		
	case MATCHACTION_MARK_AS_READ:
		MSG_UNSET_PERM_FLAGS(info->flags, MSG_UNREAD | MSG_NEW);
		return TRUE;

	case MATCHACTION_MARK_AS_UNREAD:
		MSG_SET_PERM_FLAGS(info->flags, MSG_UNREAD | MSG_NEW);
		return TRUE;
	
	case MATCHACTION_COLOR:
		MSG_SET_COLORLABEL_VALUE(info->flags, action->labelcolor);
		return TRUE;

	case MATCHACTION_FORWARD:
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

	case MATCHACTION_FORWARD_AS_ATTACHMENT:

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

	case MATCHACTION_BOUNCE:
		account = account_find_from_id(action->account_id);
		compose = compose_bounce(account, info);
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
		cmd = matching_build_command(action->destination, info);
		if (cmd == NULL)
			return TRUE;
		else {
			system(cmd);
			g_free(cmd);
		}
		return FALSE;

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
		gboolean result;
		gchar   *action_str;
		gchar    buf[256]; 

		if (FALSE == (result = filteringaction_apply(filtering->action, info,
					       folder_table))) {
			action_str = filteringaction_to_string(buf, sizeof buf, filtering->action);
			g_warning(_("action %s could not be applied"), action_str);
		}

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
		case MATCHACTION_BOUNCE:
			return FALSE; /* MsgInfo still valid for message */
		default:
			return FALSE;
		}
	}
	else
		return FALSE;
}

static void filter_msginfo(GSList * filtering_list, FolderItem *inbox,
			   MsgInfo * info, GHashTable *folder_table)
{
	GSList		*l;
	gboolean	 result;
	
	if (info == NULL) {
		g_warning(_("msginfo is not set"));
		return;
	}
	
	for(l = filtering_list ; l != NULL ; l = g_slist_next(l)) {
		FilteringProp * filtering = (FilteringProp *) l->data;
		if (TRUE == (result = filteringprop_apply(filtering, info, folder_table))) 
			break;
	}

	/* drop in inbox too */
	if (!result) {
		gint val;

		if (folder_item_move_msg(inbox, info) == -1) {
			debug_print(_("*** Could not drop message in inbox; still in .processing\n"));
			return;
		}	

		if (folder_table) {
			val = GPOINTER_TO_INT(g_hash_table_lookup
					      (folder_table, inbox));
			if (val == 0) {
				folder_item_scan(inbox);
				g_hash_table_insert(folder_table, inbox,
						    GINT_TO_POINTER(1));
			}
		}
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
		case MATCHACTION_MOVE:
		case MATCHACTION_DELETE:
			if (filteringprop_apply(filtering, info, folder_table))
				return;
		}
	}
}

void filter_message(GSList *filtering_list, FolderItem *inbox,
		    gint msgnum, GHashTable *folder_table)
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

	msginfo = procheader_parse(filename, msgflags, TRUE);
	
	g_free(filename);

	if (msginfo == NULL) {
		g_warning(_("could not get info for %s"), filename);
		return;
	}

	msginfo->folder = item;
	msginfo->msgnum = msgnum;

	filter_msginfo(filtering_list, inbox, msginfo, folder_table);
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

	case MATCHACTION_BOUNCE:
	case MATCHACTION_FORWARD:
	case MATCHACTION_FORWARD_AS_ATTACHMENT:
		g_snprintf(dest, destlen, "%s %d \"%s\"", command_str, action->account_id, action->destination); 
		return dest; 

	case MATCHACTION_COLOR:
		g_snprintf(dest, destlen, "%s %d", command_str, action->labelcolor);
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
