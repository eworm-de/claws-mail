/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999,2000 Hiroyuki Yamamoto
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

#ifndef __XML_H__
#define __XML_H__

#include <glib.h>
#include <stdio.h>

#define XMLBUFSIZE	8192

typedef struct _XMLAttr		XMLAttr;
typedef struct _XMLTag		XMLTag;
typedef struct _XMLNode		XMLNode;
typedef struct _XMLFile		XMLFile;

struct _XMLAttr
{
	gchar *name;
	gchar *value;
};

struct _XMLTag
{
	gchar *tag;
	GList *attr;
};

struct _XMLNode
{
	XMLTag *tag;
	gchar *element;
};

struct _XMLFile
{
	FILE *fp;

	GString *buf;
	gchar *bufp;

	gchar *dtd;
	GList *tag_stack;
	guint level;

	gboolean is_empty_element;
};

XMLFile *xml_open_file		(const gchar	*path);
void     xml_close_file		(XMLFile	*file);
GNode   *xml_parse_file		(const gchar	*path);

gint xml_get_dtd		(XMLFile	*file);
gint xml_parse_next_tag		(XMLFile	*file);
void xml_push_tag		(XMLFile	*file,
				 XMLTag		*tag);
void xml_pop_tag		(XMLFile	*file);

XMLTag *xml_get_current_tag	(XMLFile	*file);
GList  *xml_get_current_tag_attr(XMLFile	*file);
gchar  *xml_get_element		(XMLFile	*file);

gint xml_read_line		(XMLFile	*file);
void xml_truncate_buf		(XMLFile	*file);
gboolean  xml_compare_tag	(XMLFile	*file,
				 const gchar	*name);

XMLTag  *xml_copy_tag		(XMLTag		*tag);
XMLAttr *xml_copy_attr		(XMLAttr	*attr);

gint xml_unescape_str		(gchar		*str);
gint xml_file_put_escape_str	(FILE		*fp,
				 const gchar	*str);

void xml_free_node		(XMLNode	*node);
void xml_free_tree		(GNode		*node);

#endif /* __XML_H__ */
