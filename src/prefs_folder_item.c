#include "defs.h"
#include "folder.h"
#include "prefs_folder_item.h"
#include "summaryview.h"
#include "prefs.h"

PrefsFolderItem tmp_prefs;

static PrefParam param[] = {
	{"sort_by_number", "FALSE", &tmp_prefs.sort_by_number, P_BOOL,
	 NULL, NULL, NULL},
	{"sort_by_size", "FALSE", &tmp_prefs.sort_by_size, P_BOOL,
	 NULL, NULL, NULL},
	{"sort_by_date", "FALSE", &tmp_prefs.sort_by_date, P_BOOL,
	 NULL, NULL, NULL},
	{"sort_by_from", "FALSE", &tmp_prefs.sort_by_from, P_BOOL,
	 NULL, NULL, NULL},
	{"sort_by_subject", "FALSE", &tmp_prefs.sort_by_subject, P_BOOL,
	 NULL, NULL, NULL},
	{"sort_by_score", "FALSE", &tmp_prefs.sort_by_score, P_BOOL,
	 NULL, NULL, NULL},
	{"sort_descending", "FALSE", &tmp_prefs.sort_descending, P_BOOL,
	 NULL, NULL, NULL},
	{"enable_thread", "TRUE", &tmp_prefs.enable_thread, P_BOOL,
	 NULL, NULL, NULL},
	{NULL, NULL, NULL, P_OTHER, NULL, NULL, NULL}
};

void prefs_folder_item_read_config(FolderItem * item)
{
	gchar * path;

	path = folder_item_get_path(item);
	/*
	if (!is_dir_exist(path))
		make_dir_hier(path);
	*/
	prefs_read_config(param, path, FOLDERITEM_RC);
	g_free(path);

	* item->prefs = tmp_prefs;
}

void prefs_folder_item_save_config(FolderItem * item)
{	
	gchar * path;

	tmp_prefs = * item->prefs;

	path = folder_item_get_path(item);
	/*
	if (!is_dir_exist(path))
		make_dir_hier(path);
	*/

	prefs_save_config(param, path, FOLDERITEM_RC);
	g_free(path);
}

void prefs_folder_item_set_config(FolderItem * item,
				  int sort_type, gint sort_mode)
{
	tmp_prefs = * item->prefs;

	tmp_prefs.sort_by_number = FALSE;
	tmp_prefs.sort_by_size = FALSE;
	tmp_prefs.sort_by_date = FALSE;
	tmp_prefs.sort_by_from = FALSE;
	tmp_prefs.sort_by_subject = FALSE;
	tmp_prefs.sort_by_score = FALSE;

	switch (sort_mode) {
	case SORT_BY_NUMBER:
		tmp_prefs.sort_by_number = TRUE;
		break;
	case SORT_BY_SIZE:
		tmp_prefs.sort_by_size = TRUE;
		break;
	case SORT_BY_DATE:
		tmp_prefs.sort_by_date = TRUE;
		break;
	case SORT_BY_FROM:
		tmp_prefs.sort_by_from = TRUE;
		break;
	case SORT_BY_SUBJECT:
		tmp_prefs.sort_by_subject = TRUE;
		break;
	case SORT_BY_SCORE:
		tmp_prefs.sort_by_score = TRUE;
		break;
	}
	tmp_prefs.sort_descending = (sort_type == GTK_SORT_DESCENDING);

	* item->prefs = tmp_prefs;
}

PrefsFolderItem * prefs_folder_item_new(void)
{
	PrefsFolderItem * prefs;

	prefs = g_new0(PrefsFolderItem, 1);

	tmp_prefs.sort_by_number = FALSE;
	tmp_prefs.sort_by_size = FALSE;
	tmp_prefs.sort_by_date = FALSE;
	tmp_prefs.sort_by_from = FALSE;
	tmp_prefs.sort_by_subject = FALSE;
	tmp_prefs.sort_by_score = FALSE;
	tmp_prefs.sort_descending = FALSE;

	* prefs = tmp_prefs;
	
	return prefs;
}

void prefs_folder_item_free(PrefsFolderItem * prefs)
{
	g_free(prefs);
}

gint prefs_folder_item_get_sort_mode(FolderItem * item)
{
	tmp_prefs = * item->prefs;

	if (tmp_prefs.sort_by_number)
		return SORT_BY_NUMBER;
	if (tmp_prefs.sort_by_size)
		return SORT_BY_SIZE;
	if (tmp_prefs.sort_by_date)
		return SORT_BY_DATE;
	if (tmp_prefs.sort_by_from)
		return SORT_BY_FROM;
	if (tmp_prefs.sort_by_subject)
		return SORT_BY_SUBJECT;
	if (tmp_prefs.sort_by_score)
		return SORT_BY_SCORE;
	return SORT_BY_NONE;
}

gint prefs_folder_item_get_sort_type(FolderItem * item)
{
	tmp_prefs = * item->prefs;

	if (tmp_prefs.sort_descending)
		return GTK_SORT_DESCENDING;
	else
		return GTK_SORT_ASCENDING;
}
