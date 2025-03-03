/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2012 Hiroyuki Yamamoto and the Claws Mail team
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#include "claws-features.h"
#endif

#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "xml.h"
#include "utils.h"
#include "codeconv.h"
#include "file-utils.h"

#define SPARSE_MEMORY
/* if this is defined all attr.names and tag.names are stored
 * in a hash table */
#if defined(SPARSE_MEMORY)
#include "stringtable.h" 

static StringTable *xml_string_table;
static XMLTag  *xml_copy_tag		(XMLTag		*tag);
static XMLAttr *xml_copy_attr		(XMLAttr	*attr);
static void xml_free_node		(XMLNode	*node);
static void xml_free_tag		(XMLTag 	*tag);
static void xml_pop_tag		(XMLFile	*file);
static void xml_push_tag		(XMLFile	*file,
				 XMLTag		*tag);
static gint xml_read_line		(XMLFile	*file);
static void xml_truncate_buf		(XMLFile	*file);
static gint xml_unescape_str		(gchar		*str);

static void xml_string_table_create(void)
{
	if (xml_string_table == NULL)
		xml_string_table = string_table_new();
}
#define XML_STRING_ADD(str) \
	string_table_insert_string(xml_string_table, (str))
#define XML_STRING_FREE(str) \
	string_table_free_string(xml_string_table, (str))

#define XML_STRING_TABLE_CREATE() \
	xml_string_table_create()

#else /* !SPARSE_MEMORY */

#define XML_STRING_ADD(str) \
	g_strdup(str)
#define XML_STRING_FREE(str) \
	g_free(str)

#define XML_STRING_TABLE_CREATE()

#endif /* SPARSE_MEMORY */

static gint xml_get_parenthesis	(XMLFile	*file,
				 gchar		*buf,
				 gint		 len);

XMLFile *xml_open_file(const gchar *path)
{
	XMLFile *newfile;

	cm_return_val_if_fail(path != NULL, NULL);

	newfile = g_new(XMLFile, 1);

	newfile->fp = claws_fopen(path, "rb");
	if (!newfile->fp) {
		FILE_OP_ERROR(path, "fopen");
		g_free(newfile);
		return NULL;
	}

	XML_STRING_TABLE_CREATE();

	newfile->buf = g_string_new(NULL);
	newfile->bufp = newfile->buf->str;

	newfile->dtd = NULL;
	newfile->encoding = NULL;
	newfile->tag_stack = NULL;
	newfile->level = 0;
	newfile->is_empty_element = FALSE;

	newfile->path = g_strdup(path);

	return newfile;
}

void xml_close_file(XMLFile *file)
{
	cm_return_if_fail(file != NULL);

	if (file->fp) claws_fclose(file->fp);

	g_string_free(file->buf, TRUE);

	g_free(file->dtd);
	g_free(file->encoding);
	g_free(file->path);

	while (file->tag_stack != NULL)
		xml_pop_tag(file);

	g_free(file);
}

static GNode *xml_build_tree(XMLFile *file, GNode *parent, guint level)
{
	GNode *node = NULL;
	XMLNode *xmlnode;
	XMLTag *tag;

	while (xml_parse_next_tag(file) == 0) {
		if (file->level < level) break;
		if (file->level == level) {
			g_warning("xml_build_tree(): parse error in %s", file->path);
			break;
		}

		tag = xml_get_current_tag(file);
		if (!tag) break;
		xmlnode = xml_node_new(xml_copy_tag(tag), NULL);
		xmlnode->element = xml_get_element(file);
		if (!parent)
			node = g_node_new(xmlnode);
		else
			node = g_node_append_data(parent, xmlnode);

		xml_build_tree(file, node, file->level);
		if (file->level == 0) break;
	}

	return node;
}

GNode *xml_parse_file(const gchar *path)
{
	XMLFile *file;
	GNode *node;

	file = xml_open_file(path);
	if (file == NULL)
		return NULL;

	xml_get_dtd(file);

	node = xml_build_tree(file, NULL, file->level);

	xml_close_file(file);

#if defined(SPARSE_MEMORY)
	if (debug_get_mode())
		string_table_get_stats(xml_string_table);
#endif

	return node;
}

gint xml_get_dtd(XMLFile *file)
{
	gchar buf[XMLBUFSIZE];
	gchar *bufp = buf;

	if (xml_get_parenthesis(file, buf, sizeof(buf)) < 0) return -1;

	if ((*bufp++ == '?') &&
	    (bufp = strcasestr(bufp, "xml")) &&
	    (bufp = strcasestr(bufp + 3, "version")) &&
	    (bufp = strchr(bufp + 7, '?'))) {
		file->dtd = g_strdup(buf);
		if ((bufp = strcasestr(buf, "encoding=\""))) {
			bufp += 9;
			extract_quote(bufp, '"');
			file->encoding = g_strdup(bufp);
			file->need_codeconv =
				g_strcmp0(bufp, CS_INTERNAL);
		} else {
			file->encoding = g_strdup(CS_INTERNAL);
			file->need_codeconv = FALSE;
		}
	} else {
		g_warning("can't get XML DTD in %s", file->path);
		return -1;
	}

	return 0;
}

gint xml_parse_next_tag(XMLFile *file)
{
	gchar buf[XMLBUFSIZE];
	gchar *bufp = buf;
	gchar *tag_str;
	XMLTag *tag;
	gint len;

next:
	if (file->is_empty_element == TRUE) {
		file->is_empty_element = FALSE;
		xml_pop_tag(file);
		return 0;
	}

	if (xml_get_parenthesis(file, buf, sizeof(buf)) < 0) {
		g_warning("xml_parse_next_tag(): can't parse next tag  in %s", file->path);
		return -1;
	}

	len = strlen(buf);

	/* end-tag */
	if (buf[0] == '/') {
		if (strcmp(xml_get_current_tag(file)->tag, buf + 1) != 0) {
			g_warning("xml_parse_next_tag(): tag name mismatch in %s : %s (%s)", file->path, buf, xml_get_current_tag(file)->tag);
			return -1;
		}
		xml_pop_tag(file);
		return 0;
	}

	if (len >= 7 && !strncmp(buf, "!-- ", 4) && !strncmp(buf+len-3, " --", 3)) {
		/* skip comment */
		goto next;
	}

	tag = xml_tag_new(NULL);
	xml_push_tag(file, tag);

	if (len > 0 && buf[len - 1] == '/') {
		file->is_empty_element = TRUE;
		buf[len - 1] = '\0';
		g_strchomp(buf);
	}
	
	if (strlen(buf) == 0) {
		g_warning("xml_parse_next_tag(): tag name is empty in %s", file->path);
		return -1;
	}

	while (*bufp != '\0' && !g_ascii_isspace(*bufp)) bufp++;
	if (*bufp == '\0') {
		if (file->need_codeconv) {
			tag_str = conv_codeset_strdup(buf, file->encoding, CS_INTERNAL);
			if (tag_str) {
				tag->tag = XML_STRING_ADD(tag_str);
				g_free(tag_str);
			} else
				tag->tag = XML_STRING_ADD(buf);
		} else
			tag->tag = XML_STRING_ADD(buf);
		return 0;
	} else {
		*bufp++ = '\0';
		if (file->need_codeconv) {
			tag_str = conv_codeset_strdup(buf, file->encoding, CS_INTERNAL);
			if (tag_str) {
				tag->tag = XML_STRING_ADD(tag_str);
				g_free(tag_str);
			} else
				tag->tag = XML_STRING_ADD(buf);
		} else
			tag->tag = XML_STRING_ADD(buf);
	}

	/* parse attributes ( name=value ) */
	while (*bufp) {
		XMLAttr *attr;
		gchar *attr_name;
		gchar *attr_value;
		gchar *utf8_attr_name;
		gchar *utf8_attr_value;
		gchar *p;
		gchar quote;

		while (g_ascii_isspace(*bufp)) bufp++;
		attr_name = bufp;
		if ((p = strchr(attr_name, '=')) == NULL) {
			g_warning("xml_parse_next_tag(): syntax error in %s, tag (a) %s", file->path, attr_name);
			return -1;
		}
		bufp = p;
		*bufp++ = '\0';
		while (g_ascii_isspace(*bufp)) bufp++;

		if (*bufp != '"' && *bufp != '\'') {
			g_warning("xml_parse_next_tag(): syntax error in %s, tag (b) %s", file->path, bufp);
			return -1;
		}
		quote = *bufp;
		bufp++;
		attr_value = bufp;
		if ((p = strchr(attr_value, quote)) == NULL) {
			g_warning("xml_parse_next_tag(): syntax error in %s, tag (c) %s", file->path, attr_value);
			return -1;
		}
		bufp = p;
		*bufp++ = '\0';

		g_strchomp(attr_name);
		xml_unescape_str(attr_value);
		if (file->need_codeconv) {
			utf8_attr_name = conv_codeset_strdup
				(attr_name, file->encoding, CS_INTERNAL);
			utf8_attr_value = conv_codeset_strdup
				(attr_value, file->encoding, CS_INTERNAL);
			if (!utf8_attr_name)
				utf8_attr_name = g_strdup(attr_name);
			if (!utf8_attr_value)
				utf8_attr_value = g_strdup(attr_value);

			attr = xml_attr_new(utf8_attr_name, utf8_attr_value);
			g_free(utf8_attr_value);
			g_free(utf8_attr_name);
		} else {
			attr = xml_attr_new(attr_name, attr_value);
		}
		xml_tag_add_attr(tag, attr);

	}
	tag->attr = g_list_reverse(tag->attr);

	return 0;
}

static void xml_push_tag(XMLFile *file, XMLTag *tag)
{
	cm_return_if_fail(tag != NULL);

	file->tag_stack = g_list_prepend(file->tag_stack, tag);
	file->level++;
}

static void xml_pop_tag(XMLFile *file)
{
	XMLTag *tag;

	if (!file->tag_stack) return;

	tag = (XMLTag *)file->tag_stack->data;

	file->tag_stack = g_list_remove(file->tag_stack, tag);
	xml_free_tag(tag);
	file->level--;
}

XMLTag *xml_get_current_tag(XMLFile *file)
{
	if (file->tag_stack)
		return (XMLTag *)file->tag_stack->data;
	else
		return NULL;
}

GList *xml_get_current_tag_attr(XMLFile *file)
{
	XMLTag *tag;

	tag = xml_get_current_tag(file);
	if (!tag) return NULL;

	return tag->attr;
}

gchar *xml_get_element(XMLFile *file)
{
	gchar *str;
	gchar *new_str;
	gchar *end;

	while ((end = strchr(file->bufp, '<')) == NULL)
		if (xml_read_line(file) < 0) return NULL;

	if (end == file->bufp)
		return NULL;

	str = g_strndup(file->bufp, end - file->bufp);
	/* this is not XML1.0 strict */
	g_strstrip(str);
	xml_unescape_str(str);

	file->bufp = end;
	xml_truncate_buf(file);

	if (str[0] == '\0') {
		g_free(str);
		return NULL;
	}

	if (!file->need_codeconv)
		return str;

	new_str = conv_codeset_strdup(str, file->encoding, CS_INTERNAL);
	if (!new_str)
		new_str = g_strdup(str);
	g_free(str);

	return new_str;
}

static gint xml_read_line(XMLFile *file)
{
	gchar buf[XMLBUFSIZE];
	gint index;

	if (claws_fgets(buf, sizeof(buf), file->fp) == NULL)
		return -1;

	index = file->bufp - file->buf->str;

	g_string_append(file->buf, buf);

	file->bufp = file->buf->str + index;

	return 0;
}

static void xml_truncate_buf(XMLFile *file)
{
	gint len;

	len = file->bufp - file->buf->str;
	if (len > 0) {
		g_string_erase(file->buf, 0, len);
		file->bufp = file->buf->str;
	}
}

gboolean xml_compare_tag(XMLFile *file, const gchar *name)
{
	XMLTag *tag;

	tag = xml_get_current_tag(file);

	if (tag && strcmp(tag->tag, name) == 0)
		return TRUE;
	else
		return FALSE;
}

XMLNode *xml_node_new(XMLTag *tag, const gchar *text)
{
	XMLNode *node;

	node = g_new(XMLNode, 1);
	node->tag = tag;
	node->element = g_strdup(text);

        return node;
}

XMLTag *xml_tag_new(const gchar *tag)
{
	XMLTag *new_tag;
 
	new_tag = g_new(XMLTag, 1);
	if (tag)
		new_tag->tag = XML_STRING_ADD(tag);
	else
		new_tag->tag = NULL;
	new_tag->attr = NULL;
 
	return new_tag;
}

XMLAttr *xml_attr_new(const gchar *name, const gchar *value)
{
	XMLAttr *new_attr;
 
	new_attr = g_new(XMLAttr, 1);
	new_attr->name = XML_STRING_ADD(name);
	new_attr->value = g_strdup(value);
 
	return new_attr;
}

XMLAttr *xml_attr_new_int(const gchar *name, const gint value)
{
	XMLAttr *new_attr;
	gchar *valuestr;

	valuestr = g_strdup_printf("%d", value);

	new_attr = g_new(XMLAttr, 1);
	new_attr->name = XML_STRING_ADD(name);
	new_attr->value = valuestr;
 
	return new_attr;
}

XMLAttr *xml_attr_new_time_t(const gchar *name, const time_t value)
{
	XMLAttr *new_attr;
	gchar *valuestr;

	valuestr = g_strdup_printf("%ld", value);

	new_attr = g_new(XMLAttr, 1);
	new_attr->name = XML_STRING_ADD(name);
	new_attr->value = valuestr;

	return new_attr;
}

void xml_tag_add_attr(XMLTag *tag, XMLAttr *attr)
{
	tag->attr = g_list_prepend(tag->attr, attr);
}

static XMLTag *xml_copy_tag(XMLTag *tag)
{
	XMLTag *new_tag;
	XMLAttr *attr;
	GList *list;

	new_tag = xml_tag_new(tag->tag);
	for (list = tag->attr; list != NULL; list = list->next) {
		attr = xml_copy_attr((XMLAttr *)list->data);
		xml_tag_add_attr(new_tag, attr);
	}
	tag->attr = g_list_reverse(tag->attr);

	return new_tag;
}

static XMLAttr *xml_copy_attr(XMLAttr *attr)
{
	return xml_attr_new(attr->name, attr->value);
}

static gint xml_unescape_str(gchar *str)
{
	gchar *start;
	gchar *end;
	gchar *p = str;
	gchar *esc_str;
	gchar ch;
	gint len;

	while ((start = strchr(p, '&')) != NULL) {
		if ((end = strchr(start + 1, ';')) == NULL) {
			g_warning("unescaped '&' appeared");
			p = start + 1;
			continue;
		}
		len = end - start + 1;
		if (len < 3) {
			p = end + 1;
			continue;
		}

		Xstrndup_a(esc_str, start, len, return -1);
		if (!strcmp(esc_str, "&lt;"))
			ch = '<';
		else if (!strcmp(esc_str, "&gt;"))
			ch = '>';
		else if (!strcmp(esc_str, "&amp;"))
			ch = '&';
		else if (!strcmp(esc_str, "&apos;"))
			ch = '\'';
		else if (!strcmp(esc_str, "&quot;"))
			ch = '\"';
		else {
			p = end + 1;
			continue;
		}

		*start = ch;
		memmove(start + 1, end + 1, strlen(end + 1) + 1);
		p = start + 1;
	}

	return 0;
}

gint xml_file_put_escape_str(FILE *fp, const gchar *str)
{
	const gchar *p;
	int result = 0;
	cm_return_val_if_fail(fp != NULL, -1);

	if (!str) return 0;

	for (p = str; *p != '\0'; p++) {
		switch (*p) {
		case '<':
			result = claws_fputs("&lt;", fp);
			break;
		case '>':
			result = claws_fputs("&gt;", fp);
			break;
		case '&':
			result = claws_fputs("&amp;", fp);
			break;
		case '\'':
			result = claws_fputs("&apos;", fp);
			break;
		case '\"':
			result = claws_fputs("&quot;", fp);
			break;
		default:
			result = claws_fputc(*p, fp);
		}
	}

	return (result == EOF ? -1 : 0);
}

gint xml_file_put_xml_decl(FILE *fp)
{
	cm_return_val_if_fail(fp != NULL, -1);
	XML_STRING_TABLE_CREATE();

	return fprintf(fp, "<?xml version=\"1.0\" encoding=\"%s\"?>\n", CS_INTERNAL);
}

static void xml_free_node(XMLNode *node)
{
	if (!node) return;

	xml_free_tag(node->tag);
	g_free(node->element);
	g_free(node);
}

static gboolean xml_free_func(GNode *node, gpointer data)
{
	XMLNode *xmlnode = node->data;

	xml_free_node(xmlnode);
	return FALSE;
}

void xml_free_tree(GNode *node)
{
	cm_return_if_fail(node != NULL);

	g_node_traverse(node, G_PRE_ORDER, G_TRAVERSE_ALL, -1, xml_free_func,
			NULL);

	g_node_destroy(node);
}

static void xml_free_tag(XMLTag *tag)
{
	if (!tag) return;

	XML_STRING_FREE(tag->tag);
	while (tag->attr != NULL) {
		XMLAttr *attr = (XMLAttr *)tag->attr->data;
		tag->attr = g_list_remove(tag->attr, tag->attr->data);
		XML_STRING_FREE(attr->name);
		g_free(attr->value); /* __not__ XML_STRING_FREE */
		g_free(attr);
	}
	g_free(tag);
}

static gint xml_get_parenthesis(XMLFile *file, gchar *buf, gint len)
{
	gchar *start;
	gchar *end;

	buf[0] = '\0';

	while ((start = strchr(file->bufp, '<')) == NULL)
		if (xml_read_line(file) < 0) return -1;

	start++;
	file->bufp = start;

	while ((end = strchr(file->bufp, '>')) == NULL)
		if (xml_read_line(file) < 0) return -1;

	strncpy2(buf, file->bufp, MIN(end - file->bufp + 1, len));
	g_strstrip(buf);
	file->bufp = end + 1;
	xml_truncate_buf(file);

	return 0;
}

#define TRY(func) \
if (!(func)) \
{ \
	g_warning("failed to write part of XML tree"); \
	return -1; \
} \

static int xml_write_tree_recursive(GNode *node, FILE *fp)
{
	gint i, depth;
	XMLTag *tag;
	GList *cur;

	cm_return_val_if_fail(node != NULL, -1);
	cm_return_val_if_fail(fp != NULL, -1);

	depth = g_node_depth(node) - 1;
	for (i = 0; i < depth; i++)
		TRY(claws_fputs("    ", fp) != EOF);

	tag = ((XMLNode *) node->data)->tag;

	TRY(fprintf(fp, "<%s", tag->tag) > 0);

	for (cur = tag->attr; cur != NULL; cur = g_list_next(cur)) {
		XMLAttr *attr = (XMLAttr *) cur->data;

		TRY(fprintf(fp, " %s=\"", attr->name) > 0);
		TRY(xml_file_put_escape_str(fp, attr->value) == 0);
		TRY(claws_fputs("\"", fp) != EOF);
		
	}

	if (node->children) {
		GNode *child;
		TRY(claws_fputs(">\n", fp) != EOF);

		child = node->children;
		while (child) {
			GNode *cur;

			cur = child;
			child = cur->next;
			TRY(xml_write_tree_recursive(cur, fp) == 0);
		}

		for (i = 0; i < depth; i++)
			TRY(claws_fputs("    ", fp) != EOF);
		TRY(fprintf(fp, "</%s>\n", tag->tag) > 0);
	} else
		TRY(claws_fputs(" />\n", fp) != EOF);
	
	return 0;
}

#undef TRY

int xml_write_tree(GNode *node, FILE *fp)
{
	return xml_write_tree_recursive(node, fp);
}

static gpointer copy_node_func(gpointer nodedata, gpointer data)
{
	XMLNode *xmlnode = (XMLNode *) nodedata;
	XMLNode *newxmlnode;
	
	newxmlnode = g_new0(XMLNode, 1);
	newxmlnode->tag = xml_copy_tag(xmlnode->tag);
	newxmlnode->element = g_strdup(xmlnode->element);

	return newxmlnode;
}

GNode *xml_copy_tree(GNode *node)
{
	return g_node_map(node, copy_node_func, NULL);
}
