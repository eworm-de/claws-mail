/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2023 the Claws Mail team and Hiroyuki Yamamoto
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 * 
 */

#ifndef __PROCHEADER_H__
#define __PROCHEADER_H__

#include <glib.h>
#include <stdio.h>
#include <time.h>

#include "proctypes.h"

#define AVATAR_HEADER_UPDATE_HOOKLIST "avatar_header_update"

struct _HeaderEntry
{
	gchar	 *name;
	gchar	 *body;
	gboolean  unfold;
};

struct _Header
{
	gchar *name;
	gchar *body;
};

gint procheader_get_one_field		(gchar		**buf,
					 FILE		*fp,
					 HeaderEntry	 hentry[]);
gint procheader_get_one_field_asis	(gchar		**buf,
					 FILE		*fp);

GPtrArray *procheader_get_header_array		(FILE		*fp);
void procheader_header_array_destroy		(GPtrArray	*harray);
void procheader_header_free			(Header		*header);

gboolean procheader_skip_headers(FILE *fp);

void procheader_get_header_fields	(FILE		*fp,
					 HeaderEntry	 hentry[]);
MsgInfo *procheader_parse_file		(const gchar	*file,
					 MsgFlags	 flags,
					 gboolean	 full,
					 gboolean	 decrypted);
MsgInfo *procheader_parse_str		(const gchar	*str,
					 MsgFlags	 flags,
					 gboolean	 full,
					 gboolean	 decrypted);
MsgInfo *procheader_parse_stream	(FILE		*fp,
					 MsgFlags	 flags,
					 gboolean	 full,
					 gboolean	 decrypted);

gchar *procheader_get_fromname		(const gchar	*str);

gboolean procheader_date_parse_to_tm	(const gchar	*str,
					 struct tm	*t,
					 char		*zone);

time_t procheader_date_parse		(gchar		*dest,
					 const gchar	*src,
					 gint		 len);
void procheader_date_get_localtime	(gchar		*dest,
					 gint		 len,
					 const time_t	 timer);
Header * procheader_parse_header        (gchar * buf);

gboolean procheader_headername_equal    (char * hdr1, char * hdr2);
void procheader_header_free             (Header * header);

gint procheader_get_header_from_msginfo	(MsgInfo	*msginfo,
					 gchar		**buf,
					 gchar 		*header);

HeaderEntry *procheader_entries_from_str(const gchar	*str);
void procheader_entries_free		(HeaderEntry	*entries);
gboolean procheader_header_is_internal	(const gchar	*hdr_name);
#endif /* __PROCHEADER_H__ */
