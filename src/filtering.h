#ifndef FILTER_NEW_H

#define FILTER_NEW_H

#include <glib.h>
#include "matcher.h"
#include "procmsg.h"

struct _FilteringAction {
	gint type;
	gint account_id;
	gchar * destination;
};

typedef struct _FilteringAction FilteringAction;

struct _FilteringProp {
	MatcherList * matchers;
	FilteringAction * action;
};

typedef struct _FilteringProp FilteringProp;

extern GSList * prefs_filtering;


FilteringAction * filteringaction_new(int type, int account_id,
				      gchar * destination);
void filteringaction_free(FilteringAction * action);
FilteringAction * filteringaction_parse(gchar ** str);

FilteringProp * filteringprop_new(MatcherList * matchers,
				  FilteringAction * action);
void filteringprop_free(FilteringProp * prop);

FilteringProp * filteringprop_parse(gchar ** str);


void filter_msginfo(GSList * filtering_list, MsgInfo * info,
		    GHashTable *folder_table);
void filter_msginfo_move_or_delete(GSList * filtering_list, MsgInfo * info,
				   GHashTable *folder_table);
void filter_message(GSList * filtering_list, FolderItem * item,
		    gint msgnum, GHashTable *folder_table);

gchar * filteringaction_to_string(FilteringAction * action);
void prefs_filtering_write_config(void);
void prefs_filtering_read_config(void);
gchar * filteringprop_to_string(FilteringProp * prop);

#endif
