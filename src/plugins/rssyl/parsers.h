#ifndef __PARSERS_H
#define __PARSERS_H

#include <glib.h>
#include <libxml/parser.h>

#include "feed.h"

gint rssyl_parse_rss(xmlDocPtr doc, RSSylFolderItem *ritem, gchar *parent);
gint rssyl_parse_rdf(xmlDocPtr doc, RSSylFolderItem *ritem, gchar *parent);
gint rssyl_parse_atom(xmlDocPtr doc, RSSylFolderItem *ritem, gchar *parent);

#endif /* __PARSERS_H */
