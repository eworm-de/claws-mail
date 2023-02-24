/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2006-2023 the Claws Mail Team and Andrej Kacian <andrej@kacian.sk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __PARSER_H

#include "feed.h"
#include "parser_rss20.h"
#include "parser_rdf.h"
#include "parser_atom10.h"

void libfeed_expat_chparse(void *data, const gchar *s, gint len);
void feed_parser_set_expat_handlers(FeedParserCtx *ctx);
size_t feed_writefunc(void *ptr, size_t size, size_t nmemb, void *stream);
gchar *feed_parser_get_attribute_value(const gchar **attr, const gchar *name);

int feed_parser_unknown_encoding_handler(void *encdata, const XML_Char *name,
		XML_Encoding *info);

#endif /* __PARSER_H */
