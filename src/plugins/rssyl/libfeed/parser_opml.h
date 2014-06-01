#ifndef __PARSER_OPML
#define __PARSER_OPML

#include <expat.h>

typedef void (*OPMLProcessFunc) (gchar *title, gchar *url, gint depth,
		gpointer data);

struct _OPMLProcessCtx {
	XML_Parser parser;
	guint depth;
	guint prevdepth;
	GString *str;
	OPMLProcessFunc user_function;
	gboolean body_reached;
	gpointer user_data;
};

typedef struct _OPMLProcessCtx OPMLProcessCtx;

void opml_process(gchar *path, OPMLProcessFunc function, gpointer data);

#endif /* __PARSER_OPML */
