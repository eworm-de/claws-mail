#ifndef __STRUTILS_H
#define __STRUTILS_H

#include <glib.h>

gchar *rssyl_strreplace(gchar *source, gchar *pattern,
		gchar *replacement);

gchar *rssyl_replace_html_stuff(gchar *text,
		gboolean symbols, gboolean tags);

gchar *rssyl_format_string(gchar *str, gboolean replace_html,
		gboolean strip_nl);

gchar **strsplit_no_copy(gchar *str, char delimiter);

void strip_html(gchar *str);

gchar *my_normalize_url(const gchar *url);

#endif /* __STRUTILS_H */
