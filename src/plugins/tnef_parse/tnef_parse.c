/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2007 Colin Leroy <colin@colino.net>
 * and the Claws Mail Team
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#include "claws-features.h"
#endif

#include <unistd.h>
#include <stdio.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#ifdef YTNEF_H_SUBDIR
#include <libytnef/tnef-types.h>
#include <libytnef/ytnef.h>
#include <libytnef/mapi.h>
#include <libytnef/mapidefs.h>
#else
#include <tnef-types.h>
#include <ytnef.h>
#include <mapi.h>
#include <mapidefs.h>
#endif

#include "common/claws.h"
#include "common/version.h"
#include "main.h"
#include "plugin.h"
#include "procmime.h"
#include "utils.h"
#include "file-utils.h"

#include "tnef_dump.h"

static MimeParser *tnef_parser = NULL;

static MimeInfo *tnef_broken_mimeinfo(const gchar *reason)
{
	MimeInfo *sub_info = NULL;
	gchar *tmpfilename = NULL;
	FILE *fp = get_tmpfile_in_dir(get_mime_tmp_dir(), &tmpfilename);
	GStatBuf statbuf;

	if (!fp) {
		g_free(tmpfilename);
		return NULL;
	}
	sub_info = procmime_mimeinfo_new();
	sub_info->content = MIMECONTENT_FILE;
	sub_info->data.filename = tmpfilename;
	sub_info->type = MIMETYPE_TEXT;
	sub_info->subtype = g_strdup("plain");

	fprintf(fp, _("\n"
			"Claws Mail TNEF parser:\n\n"
			"%s\n"), reason?reason:_("Unknown error"));

	claws_fclose(fp);
	if (g_stat(tmpfilename, &statbuf) < 0) {
		claws_unlink(tmpfilename);
		procmime_mimeinfo_free_all(&sub_info);
		return NULL;

	}

	sub_info->tmp = TRUE;
	sub_info->length = statbuf.st_size;
	sub_info->encoding_type = ENC_BINARY;

	return sub_info;

}

static MimeInfo *tnef_dump_file(const gchar *filename, char *data, size_t size)
{
	MimeInfo *sub_info = NULL;
	gchar *tmpfilename = NULL;
	FILE *fp = get_tmpfile_in_dir(get_mime_tmp_dir(), &tmpfilename);
	GStatBuf statbuf;
	if (!fp) {
		g_free(tmpfilename);
		return NULL;
	}
	sub_info = procmime_mimeinfo_new();
	sub_info->content = MIMECONTENT_FILE;
	sub_info->data.filename = tmpfilename;
	sub_info->type = MIMETYPE_APPLICATION;
	sub_info->subtype = g_strdup("octet-stream");
	
	if (filename) {
		gchar *content_type = NULL;

		g_hash_table_insert(sub_info->typeparameters,
				    g_strdup("filename"),
				    g_strdup(filename));
	
		content_type = procmime_get_mime_type(filename);
		if (content_type && strchr(content_type, '/')) {
			g_free(sub_info->subtype);
			sub_info->subtype = g_strdup(strchr(content_type, '/')+1);
			*(strchr(content_type, '/')) = '\0';
			sub_info->type = procmime_get_media_type(content_type);
		}
		if (content_type)
			g_free(content_type);
	} 

	if (claws_fwrite(data, 1, size, fp) < size) {
		FILE_OP_ERROR(tmpfilename, "claws_fwrite");
		claws_fclose(fp);
                if (claws_unlink(tmpfilename) < 0)
                        FILE_OP_ERROR(tmpfilename, "claws_unlink");
		procmime_mimeinfo_free_all(&sub_info);
		return tnef_broken_mimeinfo(_("Failed to write the part data."));
	}
	claws_fclose(fp);

	if (g_stat(tmpfilename, &statbuf) < 0) {
		if (claws_unlink(tmpfilename) < 0)
                        FILE_OP_ERROR(tmpfilename, "claws_unlink");
		procmime_mimeinfo_free_all(&sub_info);
		return tnef_broken_mimeinfo(_("Failed to write the part data."));
	} else {
		sub_info->tmp = TRUE;
		sub_info->length = statbuf.st_size;
		sub_info->encoding_type = ENC_BINARY;
	}

	return sub_info;
}

MimeInfo *tnef_parse_vcal(TNEFStruct *tnef)
{
	MimeInfo *sub_info = NULL;
	gchar *tmpfilename = NULL;
	FILE *fp = get_tmpfile_in_dir(get_mime_tmp_dir(), &tmpfilename);
	GStatBuf statbuf;
	gboolean result = FALSE;
	if (!fp) {
		g_free(tmpfilename);
		return NULL;
	}
	sub_info = procmime_mimeinfo_new();
	sub_info->content = MIMECONTENT_FILE;
	sub_info->data.filename = tmpfilename;
	sub_info->type = MIMETYPE_TEXT;
	sub_info->subtype = g_strdup("calendar");
	g_hash_table_insert(sub_info->typeparameters,
			    g_strdup("filename"),
			    g_strdup("calendar.ics"));

	result = SaveVCalendar(fp, tnef);

	claws_fclose(fp);

	if (g_stat(tmpfilename, &statbuf) < 0) {
		result = FALSE;
	} else {
		sub_info->tmp = TRUE;
		sub_info->length = statbuf.st_size;
		sub_info->encoding_type = ENC_BINARY;
	}

	if (!result) {
		claws_unlink(tmpfilename);
		procmime_mimeinfo_free_all(&sub_info);
		return tnef_broken_mimeinfo(_("Failed to parse VCalendar data."));
	}
	return sub_info;
}

MimeInfo *tnef_parse_vtask(TNEFStruct *tnef)
{
	MimeInfo *sub_info = NULL;
	gchar *tmpfilename = NULL;
	FILE *fp = get_tmpfile_in_dir(get_mime_tmp_dir(), &tmpfilename);
	GStatBuf statbuf;
	gboolean result = FALSE;
	if (!fp) {
		g_free(tmpfilename);
		return NULL;
	}
	sub_info = procmime_mimeinfo_new();
	sub_info->content = MIMECONTENT_FILE;
	sub_info->data.filename = tmpfilename;
	sub_info->type = MIMETYPE_TEXT;
	sub_info->subtype = g_strdup("calendar");
	g_hash_table_insert(sub_info->typeparameters,
			    g_strdup("filename"),
			    g_strdup("task.ics"));

	result = SaveVTask(fp, tnef);

	claws_fclose(fp);

	if (g_stat(tmpfilename, &statbuf) < 0) {
		result = FALSE;
	} else {
		sub_info->tmp = TRUE;
		sub_info->length = statbuf.st_size;
		sub_info->encoding_type = ENC_BINARY;
	}
	if (!result) {
		claws_unlink(tmpfilename);
		procmime_mimeinfo_free_all(&sub_info);
		return tnef_broken_mimeinfo(_("Failed to parse VTask data."));
	}
	return sub_info;
}

MimeInfo *tnef_parse_rtf(TNEFStruct *tnef, variableLength *tmp_var)
{
	variableLength buf;
	MimeInfo *info = NULL;
	buf.data = DecompressRTF(tmp_var, &(buf.size));
	if (buf.data) {
		info = tnef_dump_file("message.rtf", buf.data, buf.size);
		free(buf.data);
		return info;
	} else {
		return NULL;
	}
}

MimeInfo *tnef_parse_vcard(TNEFStruct *tnef)
{
	MimeInfo *sub_info = NULL;
	gchar *tmpfilename = NULL;
	FILE *fp = get_tmpfile_in_dir(get_mime_tmp_dir(), &tmpfilename);
	GStatBuf statbuf;
	gboolean result = FALSE;
	gint ret;
	if (!fp) {
		g_free(tmpfilename);
		return NULL;
	}
	sub_info = procmime_mimeinfo_new();
	sub_info->content = MIMECONTENT_FILE;
	sub_info->data.filename = tmpfilename;
	sub_info->type = MIMETYPE_TEXT;
	sub_info->subtype = g_strdup("x-vcard");
	g_hash_table_insert(sub_info->typeparameters,
			    g_strdup("filename"),
			    g_strdup("contact.vcf"));
	
	result = SaveVCard(fp, tnef);
	
	claws_fclose(fp);

	ret = g_stat(tmpfilename, &statbuf);
	if (ret == -1) {
		debug_print("couldn't stat tmpfilename '%s'\n", tmpfilename);
	}

	if ((ret == -1) || !result) {
		claws_unlink(tmpfilename);
		procmime_mimeinfo_free_all(&sub_info);
		return tnef_broken_mimeinfo(_("Failed to parse VCard data."));
	}

	sub_info->tmp = TRUE;
	sub_info->length = statbuf.st_size;
	sub_info->encoding_type = ENC_BINARY;
	return sub_info;
}

static gboolean tnef_parse (MimeParser *parser, MimeInfo *mimeinfo)
{
	TNEFStruct *tnef;
	MimeInfo *sub_info = NULL;
	variableLength *tmp_var;
	Attachment *att;
	int parse_result = 0;
	gboolean cal_done = FALSE;

	if (!procmime_decode_content(mimeinfo)) {
		debug_print("error decoding\n");
		return FALSE;
	}
	debug_print("Tnef parser parsing part (%d).\n", mimeinfo->length);
	if (mimeinfo->content == MIMECONTENT_FILE)
		debug_print("content: %s\n", mimeinfo->data.filename);
	else 
		debug_print("contents in memory (len %"G_GSIZE_FORMAT")\n", 
			strlen(mimeinfo->data.mem));
	
	tnef = g_new0(TNEFStruct, 1);
	TNEFInitialize(tnef);

	tnef->Debug = debug_get_mode();

	if (mimeinfo->content == MIMECONTENT_MEM)
		parse_result = TNEFParseMemory(mimeinfo->data.mem, mimeinfo->length, tnef);
	else
		parse_result = TNEFParseFile(mimeinfo->data.filename, tnef);
	
	mimeinfo->type = MIMETYPE_MULTIPART;
	mimeinfo->subtype = g_strdup("mixed");
	g_hash_table_insert(mimeinfo->typeparameters,
			    g_strdup("description"),
			    g_strdup("Parsed from MS-TNEF"));

	if (parse_result != 0) {
		g_warning("failed to parse TNEF data");
		TNEFFree(tnef);
		return FALSE;
	}
	
	sub_info = NULL;
	if (tnef->messageClass[0] != '\0') {
		if (strcmp(tnef->messageClass, "IPM.Contact") == 0)
			sub_info = tnef_parse_vcard(tnef);
		else if (strcmp(tnef->messageClass, "IPM.Task") == 0)
			sub_info = tnef_parse_vtask(tnef);
		else if (strcmp(tnef->messageClass, "IPM.Appointment") == 0) {
			sub_info = tnef_parse_vcal(tnef);
			cal_done = TRUE;
		}
	}

	if (sub_info)
		g_node_append(mimeinfo->node, sub_info->node);
	sub_info = NULL;

	if (tnef->MapiProperties.count > 0) {
		tmp_var = MAPIFindProperty (&(tnef->MapiProperties), PROP_TAG(PT_BINARY,PR_RTF_COMPRESSED));
		if (tmp_var != MAPI_UNDEFINED) {
			sub_info = tnef_parse_rtf(tnef, tmp_var);
		}
	}

	if (sub_info)
		g_node_append(mimeinfo->node, sub_info->node);
	sub_info = NULL;

	tmp_var = MAPIFindUserProp(&(tnef->MapiProperties), PROP_TAG(PT_STRING8,0x24));
	if (tmp_var != MAPI_UNDEFINED) {
		if (!cal_done && strcmp(tmp_var->data, "IPM.Appointment") == 0) {
			sub_info = tnef_parse_vcal(tnef);
		}
	}
	
	if (sub_info)
		g_node_append(mimeinfo->node, sub_info->node);
	sub_info = NULL;

	att = tnef->starting_attach.next;
	while (att) {
		gchar *filename = NULL;
		gboolean is_object = TRUE;
		DWORD signature;

		tmp_var = MAPIFindProperty(&(att->MAPI), PROP_TAG(30,0x3707));
		if (tmp_var == MAPI_UNDEFINED)
			tmp_var = MAPIFindProperty(&(att->MAPI), PROP_TAG(30,0x3001));
		if (tmp_var == MAPI_UNDEFINED)
			tmp_var = &(att->Title);

		if (tmp_var->data)
			filename = g_strdup(tmp_var->data);
		
		tmp_var = MAPIFindProperty(&(att->MAPI), PROP_TAG(PT_OBJECT, PR_ATTACH_DATA_OBJ));
		if (tmp_var == MAPI_UNDEFINED)
			tmp_var = MAPIFindProperty(&(att->MAPI), PROP_TAG(PT_BINARY, PR_ATTACH_DATA_OBJ));
		if (tmp_var == MAPI_UNDEFINED) {
			tmp_var = &(att->FileData);
			is_object = FALSE;
		}
		
		sub_info = tnef_dump_file(filename, 
			tmp_var->data + (is_object ? 16:0), 
			tmp_var->size - (is_object ? 16:0));
		
		if (sub_info)
			g_node_append(mimeinfo->node, sub_info->node);
	
		memcpy(&signature, tmp_var->data+(is_object ? 16:0), sizeof(DWORD));

		if (TNEFCheckForSignature(signature) == 0) {
			debug_print("that's TNEF stuff, process it\n");
			tnef_parse(parser, sub_info);
		}

		sub_info = NULL;
		
		att = att->next;

		g_free(filename);
	}
	
	TNEFFree(tnef);
	return TRUE;
}

gint plugin_init(gchar **error)
{
	if (!check_plugin_version(MAKE_NUMERIC_VERSION(2,9,2,72),
				VERSION_NUMERIC, _("TNEF Parser"), error))
		return -1;

	tnef_parser = g_new0(MimeParser, 1);
	tnef_parser->type = MIMETYPE_APPLICATION;
	tnef_parser->sub_type = "ms-tnef";
	tnef_parser->parse = tnef_parse;
	
	procmime_mimeparser_register(tnef_parser);

	return 0;
}

gboolean plugin_done(void)
{
	procmime_mimeparser_unregister(tnef_parser);
	g_free(tnef_parser);
	tnef_parser = NULL;

	return TRUE;
}

const gchar *plugin_name(void)
{
	return _("TNEF Parser");
}

const gchar *plugin_desc(void)
{
	return _("This Claws Mail plugin allows you to read application/ms-tnef attachments.\n\n"
		 "The plugin uses the Ytnef library, which is copyright 2002-2007 by "
		 "Randall Hand <yerase@yerot.com>");
}

const gchar *plugin_type(void)
{
	return "GTK3";
}

const gchar *plugin_licence(void)
{
	return "GPL3+";
}

const gchar *plugin_version(void)
{
	return VERSION;
}

struct PluginFeature *plugin_provides(void)
{
	static struct PluginFeature features[] = 
		{ {PLUGIN_MIMEPARSER, "application/ms-tnef"},
		  {PLUGIN_NOTHING, NULL}};
	return features;
}
