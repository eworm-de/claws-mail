#ifndef PREFS_FOLDER_ITEM_H

#define PREFS_FOLDER_ITEM_H

#include "folder.h"
#include <glib.h>

struct _PrefsFolderItem {
	gchar * directory;

	gboolean sort_by_number;
	gboolean sort_by_size;
	gboolean sort_by_date;
	gboolean sort_by_from;
	gboolean sort_by_subject;
	gboolean sort_by_score;

	gboolean sort_descending;

	GSList * scoring;
};

typedef struct _PrefsFolderItem PrefsFolderItem;

void prefs_folder_item_read_config(FolderItem * item);
void prefs_folder_item_save_config(FolderItem * item);
void prefs_folder_item_set_config(FolderItem * item,
				  int sort_type, gint sort_mode);
PrefsFolderItem * prefs_folder_item_new(void);
void prefs_folder_item_free(PrefsFolderItem * prefs);

#endif
