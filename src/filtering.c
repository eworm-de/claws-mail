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
				      gchar * destination)
{
	FilteringAction * action;

	action = g_new0(FilteringAction, 1);

	action->type = type;
	action->account_id = account_id;
	if (destination)
		action->destination = g_strdup(destination);

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

	tmp = * str;

	key = matcher_parse_keyword(&tmp);

	switch (key) {
	case MATCHING_ACTION_MOVE:
		destination = matcher_parse_str(&tmp);
		if (tmp == NULL) {
			* str = NULL;
			return NULL;
		}
		break;
	case MATCHING_ACTION_COPY:
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
	default:
		* str = NULL;
		return NULL;
	}

	* str = tmp;
	action = filteringaction_new(key, account_id, destination);

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

static gboolean filteringaction_update_mark(MsgInfo * info)
{
	gchar * dest_path;
	FILE * fp;

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

/*
  fitleringaction_apply
  runs the action on one MsgInfo
  return value : return TRUE if the action could be applied
*/

static gboolean filteringaction_apply(FilteringAction * action, MsgInfo * info,
				      GHashTable *folder_table)
{
	FolderItem * dest_folder;
	gint val;
	Compose * compose;
	PrefsAccount * account;

	switch(action->type) {
	case MATCHING_ACTION_MOVE:
		dest_folder = folder_find_item_from_path(action->destination);
		if (!dest_folder)
			return FALSE;

		if (folder_item_move_msg(dest_folder, info) == -1)
			return FALSE;

		info->flags = 0;
		filteringaction_update_mark(info);
		
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

		return TRUE;

	case MATCHING_ACTION_COPY:
		dest_folder = folder_find_item_from_path(action->destination);
		if (!dest_folder)
			return FALSE;

		if (folder_item_copy_msg(dest_folder, info) == -1)
			return FALSE;

		val = GPOINTER_TO_INT(g_hash_table_lookup
				      (folder_table, dest_folder));
		if (val == 0) {
			folder_item_scan(dest_folder);
			g_hash_table_insert(folder_table, dest_folder,
					    GINT_TO_POINTER(1));
		}

		return TRUE;

	case MATCHING_ACTION_DELETE:
		if (folder_item_remove_msg(info->folder, info->msgnum) == -1)
			return FALSE;

		info->flags = 0;
		filteringaction_update_mark(info);

		return TRUE;

	case MATCHING_ACTION_MARK:
		MSG_SET_FLAGS(info->flags, MSG_MARKED);
		filteringaction_update_mark(info);

		return TRUE;

	case MATCHING_ACTION_UNMARK:
		MSG_UNSET_FLAGS(info->flags, MSG_MARKED);
		filteringaction_update_mark(info);

		return TRUE;
		
	case MATCHING_ACTION_MARK_AS_READ:
		MSG_UNSET_FLAGS(info->flags, MSG_UNREAD | MSG_NEW);
		filteringaction_update_mark(info);

		return TRUE;

	case MATCHING_ACTION_MARK_AS_UNREAD:
		MSG_SET_FLAGS(info->flags, MSG_UNREAD | MSG_NEW);
		filteringaction_update_mark(info);

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

	default:
		return FALSE;
	}
}

/*
  filteringprop_apply
  runs the action on one MsgInfo if it matches the criterium
  return value : return TRUE if the action doesn't allow more actions
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
			return TRUE;
		case MATCHING_ACTION_COPY:
		case MATCHING_ACTION_MARK:
		case MATCHING_ACTION_MARK_AS_READ:
		case MATCHING_ACTION_UNMARK:
		case MATCHING_ACTION_MARK_AS_UNREAD:
		case MATCHING_ACTION_FORWARD:
		case MATCHING_ACTION_FORWARD_AS_ATTACHMENT:
			return FALSE;
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

void filter_message(GSList * filtering_list, FolderItem * item,
		    gint msgnum, GHashTable *folder_table)
{
	MsgInfo * msginfo;
	gchar * filename;

	if (item == NULL) {
		g_warning(_("folderitem not set"));
		return;
	}

	filename = folder_item_fetch_msg(item, msgnum);

	if (filename == NULL) {
		g_warning(_("filename is not set"));
		return;
	}

	msginfo = procheader_parse(filename, 0, TRUE);

	g_free(filename);

	if (msginfo == NULL) {
		g_warning(_("could not get info for %s"), filename);
		return;
	}

	msginfo->folder = item;
	msginfo->msgnum = msgnum;

	filter_msginfo(filtering_list, msginfo, folder_table);
}

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

	command_str = NULL;
	command_str = get_matchparser_tab_str(action->type);

	if (command_str == NULL)
		return NULL;

	switch(action->type) {
	case MATCHING_ACTION_MOVE:
	case MATCHING_ACTION_COPY:
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
