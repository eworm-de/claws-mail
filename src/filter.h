/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2002 Hiroyuki Yamamoto
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef __FILTER_H__
#define __FILTER_H__

#include <glib.h>

#include "folder.h"

typedef struct _Filter	Filter;

typedef enum
{
	FLT_CONTAIN	= 1 << 0,
	FLT_CASE_SENS	= 1 << 1,
	FLT_REGEX	= 1 << 2
} FilterFlag;

typedef enum
{
	FLT_AND,
	FLT_OR,
	FLT_NOT
} FilterCond;

typedef enum
{
	FLT_MOVE,
	FLT_NOTRECV,
	FLT_DELETE
} FilterAction;

#define FLT_IS_CONTAIN(flag)	((flag & FLT_CONTAIN) != 0)
#define FLT_IS_CASE_SENS(flag)	((flag & FLT_CASE_SENS) != 0)
#define FLT_IS_REGEX(flag)	((flag & FLT_REGEX) != 0)

struct _Filter
{
	gchar *name1;
	gchar *body1;

	gchar *name2;
	gchar *body2;

	FilterFlag flag1;
	FilterFlag flag2;

	FilterCond cond;

	/* destination folder */
	gchar *dest;

	FilterAction action;
};

#define FILTER_NOT_RECEIVE	"//NOT_RECEIVE//"

FolderItem *filter_get_dest_folder	(GSList		*fltlist,
					 const gchar	*file);
gboolean filter_match_condition		(Filter		*filter,
					 GSList		*hlist);
gchar *filter_get_str			(Filter		*filter);
Filter *filter_read_str			(const gchar	*str);
void filter_free			(Filter		*filter);

#endif /* __FILTER_H__ */
