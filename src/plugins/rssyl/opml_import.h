#ifndef __OPML_IMPORT
#define __OPML_IMPORT

struct _OPMLImportCtx {
	GSList *current;
	gint depth;
	gint failures;
};

typedef struct _OPMLImportCtx OPMLImportCtx;

gint rssyl_folder_depth(FolderItem *item);
void rssyl_opml_import_func(gchar *title, gchar *url, gint depth, gpointer data);

#endif /* __OPML_IMPORT */
