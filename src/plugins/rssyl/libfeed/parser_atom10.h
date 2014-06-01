/*
 * Copyright (C) 2006 Andrej Kacian <andrej@kacian.sk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __PARSER_ATOM03_H
#define __PARSER_ATOM03_H

void feed_parser_atom10_start(void *data, const char *el, const char **attr);
void feed_parser_atom10_end(void *data, const char *el);

enum {
	FEED_LOC_ATOM10_NONE,
	FEED_LOC_ATOM10_ENTRY,
	FEED_LOC_ATOM10_AUTHOR,
	FEED_LOC_ATOM10_SOURCE,
	FEED_LOC_ATOM10_CONTENT
} FeedAtom10Locations;

#endif /* __PARSER_ATOM03_H */
