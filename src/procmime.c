/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2003 Hiroyuki Yamamoto
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "defs.h"

#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <locale.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "intl.h"
#include "procmime.h"
#include "procheader.h"
#include "base64.h"
#include "quoted-printable.h"
#include "uuencode.h"
#include "unmime.h"
#include "html.h"
#include "enriched.h"
#include "codeconv.h"
#include "utils.h"
#include "prefs_common.h"

#include "prefs_gtk.h"

static GHashTable *procmime_get_mime_type_table	(void);

MimeInfo *procmime_mimeinfo_new(void)
{
	MimeInfo *mimeinfo;

	mimeinfo = g_new0(MimeInfo, 1);
	mimeinfo->type     	= MIMETYPE_UNKNOWN;
	mimeinfo->encoding_type = ENC_UNKNOWN;

	mimeinfo->parameters = g_hash_table_new(g_str_hash, g_str_equal);

	return mimeinfo;
}

static gboolean procmime_mimeinfo_parameters_destroy(gpointer key, gpointer value, gpointer user_data)
{
	g_free(key);
	g_free(value);
	
	return TRUE;
}

void procmime_mimeinfo_free_all(MimeInfo *mimeinfo)
{
	MimeInfo *next;

	g_return_if_fail(mimeinfo);

	g_free(mimeinfo->encoding);
	g_free(mimeinfo->charset);
	g_free(mimeinfo->name);
	g_free(mimeinfo->content_disposition);
	if(mimeinfo->tmpfile)
		unlink(mimeinfo->filename);
	g_free(mimeinfo->filename);

	if (mimeinfo->children != NULL)
		procmime_mimeinfo_free_all(mimeinfo->children);
	g_free(mimeinfo->subtype);
	g_free(mimeinfo->description);
	g_free(mimeinfo->id);

	g_hash_table_foreach_remove(mimeinfo->parameters, procmime_mimeinfo_parameters_destroy, NULL);
	g_hash_table_destroy(mimeinfo->parameters);

	if (mimeinfo->privacy)
		privacy_free_privacydata(mimeinfo->privacy);

	next = mimeinfo->next;
	g_free(mimeinfo);
	mimeinfo = next;
}

MimeInfo *procmime_mimeinfo_insert(MimeInfo *parent, MimeInfo *mimeinfo)
{
	MimeInfo *child = parent->children;

	if (!child)
		parent->children = mimeinfo;
	else {
		while (child->next != NULL)
			child = child->next;

		child->next = mimeinfo;
	}

	mimeinfo->parent = parent;
	mimeinfo->level = parent->level + 1;

	return mimeinfo;
}

void procmime_mimeinfo_replace(MimeInfo *old, MimeInfo *new)
{
	MimeInfo *parent = old->parent;
	MimeInfo *child;

	g_return_if_fail(parent != NULL);
	g_return_if_fail(new->next == NULL);

	for (child = parent->children; child && child != old;
	     child = child->next)
		;
	if (!child) {
		g_warning("oops: parent can't find it's own child");
		return;
	}
	procmime_mimeinfo_free_all(old);

	if (child == parent->children) {
		new->next = parent->children->next;
		parent->children = new;
	} else {
		new->next = child->next;
		child = new;
	}
}

MimeInfo *procmime_mimeinfo_next(MimeInfo *mimeinfo)
{
	if (!mimeinfo) return NULL;

	if (mimeinfo->children)
		return mimeinfo->children;
	if (mimeinfo->next)
		return mimeinfo->next;

	if (mimeinfo->main) {
		mimeinfo = mimeinfo->main;
		if (mimeinfo->next)
			return mimeinfo->next;
	}

	for (mimeinfo = mimeinfo->parent; mimeinfo != NULL;
	     mimeinfo = mimeinfo->parent) {
		if (mimeinfo->next)
			return mimeinfo->next;
		if (mimeinfo->main) {
			mimeinfo = mimeinfo->main;
			if (mimeinfo->next)
				return mimeinfo->next;
		}
	}

	return NULL;
}

MimeInfo *procmime_scan_message(MsgInfo *msginfo)
{
	gchar *filename;
	MimeInfo *mimeinfo;

	filename = procmsg_get_message_file(msginfo);
	if(!filename)
		return NULL;
	if (msginfo->folder->stype != F_QUEUE)
		mimeinfo = procmime_scan_file(filename);
	else
		mimeinfo = procmime_scan_queue_file(filename);
	g_free(filename);

	return mimeinfo;
}

enum
{
	H_CONTENT_TRANSFER_ENCODING = 0,
	H_CONTENT_TYPE		    = 1,
	H_CONTENT_DISPOSITION	    = 2,
	H_CONTENT_DESCRIPTION	    = 3,
	H_SUBJECT              	    = 4
};

gboolean procmime_decode_content(MimeInfo *mimeinfo)
{
	gchar buf[BUFFSIZE];
	gint readend;
	gchar *tmpfilename;
	gchar *mimetmpdir;
	FILE *outfp, *infp;
	struct stat statbuf;

	g_return_val_if_fail(mimeinfo != NULL, FALSE);

	if(mimeinfo->encoding_type == ENC_BINARY)
		return TRUE;

	infp = fopen(mimeinfo->filename, "rb");
	if(!infp) {
		perror("fopen");
		return FALSE;
	}
	fseek(infp, mimeinfo->offset, SEEK_SET);

	mimetmpdir = get_mime_tmp_dir();
	outfp = get_tmpfile_in_dir(mimetmpdir, &tmpfilename);
	if (!outfp) {
		perror("tmpfile");
		return FALSE;
	}

	readend = mimeinfo->offset + mimeinfo->length;

	if (mimeinfo->encoding_type == ENC_QUOTED_PRINTABLE) {
		while ((ftell(infp) < readend) && (fgets(buf, sizeof(buf), infp) != NULL)) {
			gint len;
			len = qp_decode_line(buf);
			fwrite(buf, len, 1, outfp);
		}
	} else if (mimeinfo->encoding_type == ENC_BASE64) {
		gchar outbuf[BUFFSIZE];
		gint len;
		Base64Decoder *decoder;

		decoder = base64_decoder_new();
		while ((ftell(infp) < readend) && (fgets(buf, sizeof(buf), infp) != NULL)) {
			len = base64_decoder_decode(decoder, buf, outbuf);
			if (len < 0) {
				g_warning("Bad BASE64 content\n");
				break;
			}
			fwrite(outbuf, sizeof(gchar), len, outfp);
		}
		base64_decoder_free(decoder);
	} else if (mimeinfo->encoding_type == ENC_X_UUENCODE) {
		gchar outbuf[BUFFSIZE];
		gint len;
		gboolean flag = FALSE;

		while ((ftell(infp) < readend) && (fgets(buf, sizeof(buf), infp) != NULL)) {
			if(!flag && strncmp(buf,"begin ", 6)) continue;

			if (flag) {
				len = fromuutobits(outbuf, buf);
				if (len <= 0) {
					if (len < 0) 
						g_warning("Bad UUENCODE content(%d)\n", len);
					break;
				}
				fwrite(outbuf, sizeof(gchar), len, outfp);
			} else
				flag = TRUE;
		}
	} else {
		while ((ftell(infp) < readend) && (fgets(buf, sizeof(buf), infp) != NULL)) {
			fputs(buf, outfp);
		}
	}

	fclose(outfp);
	fclose(infp);

	stat(tmpfilename, &statbuf);
	if(mimeinfo->tmpfile)
		unlink(mimeinfo->filename);
	g_free(mimeinfo->filename);
	mimeinfo->filename = tmpfilename;
	mimeinfo->tmpfile = TRUE;
	mimeinfo->offset = 0;
	mimeinfo->length = statbuf.st_size;
	mimeinfo->encoding_type = ENC_BINARY;

	return TRUE;
}

gint procmime_get_part(const gchar *outfile, MimeInfo *mimeinfo)
{
	FILE *infp, *outfp;
	gchar buf[BUFFSIZE];
	gint restlength, readlength;

	g_return_val_if_fail(outfile != NULL, -1);
	g_return_val_if_fail(mimeinfo != NULL, -1);

	if(mimeinfo->encoding_type != ENC_BINARY && !procmime_decode_content(mimeinfo))
		return -1;

	if ((infp = fopen(mimeinfo->filename, "rb")) == NULL) {
		FILE_OP_ERROR(mimeinfo->filename, "fopen");
		return -1;
	}
	if (fseek(infp, mimeinfo->offset, SEEK_SET) < 0) {
		FILE_OP_ERROR(mimeinfo->filename, "fseek");
		fclose(infp);
		return -1;
	}
	if ((outfp = fopen(outfile, "wb")) == NULL) {
		FILE_OP_ERROR(outfile, "fopen");
		fclose(infp);
		return -1;
	}

	restlength = mimeinfo->length;

	while ((restlength > 0) && ((readlength = fread(buf, 1, restlength > BUFFSIZE ? BUFFSIZE : restlength, infp)) > 0)) {
		fwrite(buf, 1, readlength, outfp);
		restlength -= readlength;
	}

	fclose(infp);
	if (fclose(outfp) == EOF) {
		FILE_OP_ERROR(outfile, "fclose");
		unlink(outfile);
		return -1;
	}

	return 0;
}

struct ContentRenderer {
	char * content_type;
	char * renderer;
};

static GList * renderer_list = NULL;

static struct ContentRenderer *
content_renderer_new(char * content_type, char * renderer)
{
	struct ContentRenderer * cr;

	cr = g_new(struct ContentRenderer, 1);
	if (cr == NULL)
		return NULL;

	cr->content_type = g_strdup(content_type);
	cr->renderer = g_strdup(renderer);

	return cr;
}

static void content_renderer_free(struct ContentRenderer * cr)
{
	g_free(cr->content_type);
	g_free(cr->renderer);
	g_free(cr);
}

void renderer_read_config(void)
{
	gchar buf[BUFFSIZE];
	FILE * f;
	gchar * rcpath;

	g_list_foreach(renderer_list, (GFunc) content_renderer_free, NULL);
	renderer_list = NULL;

	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, RENDERER_RC, NULL);
	f = fopen(rcpath, "rb");
	g_free(rcpath);
	
	if (f == NULL)
		return;

	while (fgets(buf, BUFFSIZE, f)) {
		char * p;
		struct ContentRenderer * cr;

		strretchomp(buf);
		p = strchr(buf, ' ');
		if (p == NULL)
			continue;
		* p = 0;

		cr = content_renderer_new(buf, p + 1);
		if (cr == NULL)
			continue;

		renderer_list = g_list_append(renderer_list, cr);
	}

	fclose(f);
}

void renderer_write_config(void)
{
	gchar * rcpath;
	PrefFile *pfile;
	GList * cur;

	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, RENDERER_RC, NULL);
	
	if ((pfile = prefs_write_open(rcpath)) == NULL) {
		g_warning("failed to write configuration to file\n");
		g_free(rcpath);
		return;
	}

	g_free(rcpath);

	for(cur = renderer_list ; cur != NULL ; cur = cur->next) {
		struct ContentRenderer * renderer;
		renderer = cur->data;
		fprintf(pfile->fp, "%s %s\n", renderer->content_type,
			renderer->renderer);
	}

	if (prefs_file_close(pfile) < 0) {
		g_warning("failed to write configuration to file\n");
		return;
	}
}

FILE *procmime_get_text_content(MimeInfo *mimeinfo, FILE *infp)
{
	return NULL;

#if 0	/* FIXME */
	FILE *tmpfp, *outfp;
	gchar *src_codeset;
	gboolean conv_fail = FALSE;
	gchar buf[BUFFSIZE];
	gchar *str;
	struct ContentRenderer * renderer;
	GList * cur;

	g_return_val_if_fail(mimeinfo != NULL, NULL);
	g_return_val_if_fail(infp != NULL, NULL);
	g_return_val_if_fail(mimeinfo->mime_type == MIME_TEXT ||
			     mimeinfo->mime_type == MIME_TEXT_HTML ||
			     mimeinfo->mime_type == MIME_TEXT_ENRICHED, NULL);

	if (fseek(infp, mimeinfo->fpos, SEEK_SET) < 0) {
		perror("fseek");
		return NULL;
	}

	while (fgets(buf, sizeof(buf), infp) != NULL)
		if (buf[0] == '\r' || buf[0] == '\n') break;

	tmpfp = procmime_decode_content(NULL, infp, mimeinfo);
	if (!tmpfp)
		return NULL;

	if ((outfp = my_tmpfile()) == NULL) {
		perror("tmpfile");
		fclose(tmpfp);
		return NULL;
	}

	src_codeset = prefs_common.force_charset
		? prefs_common.force_charset : mimeinfo->charset;

	renderer = NULL;

	for(cur = renderer_list ; cur != NULL ; cur = cur->next) {
		struct ContentRenderer * cr;
		cr = cur->data;
		if (g_strcasecmp(cr->content_type,
				 mimeinfo->content_type) == 0) {
			renderer = cr;
			break;
		}
	}

	if (renderer != NULL) {
		FILE * p;
		int oldout;
		
		oldout = dup(1);
		
		dup2(fileno(outfp), 1);
		
		p = popen(renderer->renderer, "w");
		if (p != NULL) {
			size_t count;
			
			while ((count =
				fread(buf, sizeof(char), sizeof(buf),
				      tmpfp)) > 0)
				fwrite(buf, sizeof(char), count, p);
			pclose(p);
		}
		
		dup2(oldout, 1);
	} else if (mimeinfo->mime_type == MIME_TEXT) {
		while (fgets(buf, sizeof(buf), tmpfp) != NULL) {
			str = conv_codeset_strdup(buf, src_codeset, NULL);
			if (str) {
				fputs(str, outfp);
				g_free(str);
			} else {
				conv_fail = TRUE;
				fputs(buf, outfp);
			}
		}
	} else if (mimeinfo->mime_type == MIME_TEXT_HTML) {
		HTMLParser *parser;
		CodeConverter *conv;

		conv = conv_code_converter_new(src_codeset);
		parser = html_parser_new(tmpfp, conv);
		while ((str = html_parse(parser)) != NULL) {
			fputs(str, outfp);
		}
		html_parser_destroy(parser);
		conv_code_converter_destroy(conv);
	} else if (mimeinfo->mime_type == MIME_TEXT_ENRICHED) {
		ERTFParser *parser;
		CodeConverter *conv;

		conv = conv_code_converter_new(src_codeset);
		parser = ertf_parser_new(tmpfp, conv);
		while ((str = ertf_parse(parser)) != NULL) {
			fputs(str, outfp);
		}
		ertf_parser_destroy(parser);
		conv_code_converter_destroy(conv);
	}

	if (conv_fail)
		g_warning("procmime_get_text_content(): Code conversion failed.\n");

	fclose(tmpfp);
	rewind(outfp);

	return outfp;
#endif
}

/* search the first text part of (multipart) MIME message,
   decode, convert it and output to outfp. */
FILE *procmime_get_first_text_content(MsgInfo *msginfo)
{
	return NULL;

#if 0	/* FIXME */
	FILE *infp, *outfp = NULL;
	MimeInfo *mimeinfo, *partinfo;

	g_return_val_if_fail(msginfo != NULL, NULL);

	mimeinfo = procmime_scan_message(msginfo);
	if (!mimeinfo) return NULL;

	if ((infp = procmsg_open_message(msginfo)) == NULL) {
		procmime_mimeinfo_free_all(mimeinfo);
		return NULL;
	}

	partinfo = mimeinfo;
	while (partinfo && partinfo->mime_type != MIME_TEXT)
		partinfo = procmime_mimeinfo_next(partinfo);
	if (!partinfo) {
		partinfo = mimeinfo;
		while (partinfo && partinfo->mime_type != MIME_TEXT_HTML &&
				partinfo->mime_type != MIME_TEXT_ENRICHED)
			partinfo = procmime_mimeinfo_next(partinfo);
	}
	

	if (partinfo)
		outfp = procmime_get_text_content(partinfo, infp);

	fclose(infp);
	procmime_mimeinfo_free_all(mimeinfo);

	return outfp;
#endif
}

gboolean procmime_find_string_part(MimeInfo *mimeinfo, const gchar *filename,
				   const gchar *str, gboolean case_sens)
{
	return FALSE;

#if 0	/* FIXME */
	FILE *infp, *outfp;
	gchar buf[BUFFSIZE];
	gchar *(* StrFindFunc) (const gchar *haystack, const gchar *needle);

	g_return_val_if_fail(mimeinfo != NULL, FALSE);
	g_return_val_if_fail(mimeinfo->mime_type == MIME_TEXT ||
			     mimeinfo->mime_type == MIME_TEXT_HTML ||
			     mimeinfo->mime_type == MIME_TEXT_ENRICHED, FALSE);
	g_return_val_if_fail(str != NULL, FALSE);

	if ((infp = fopen(filename, "rb")) == NULL) {
		FILE_OP_ERROR(filename, "fopen");
		return FALSE;
	}

	outfp = procmime_get_text_content(mimeinfo, infp);
	fclose(infp);

	if (!outfp)
		return FALSE;

	if (case_sens)
		StrFindFunc = strstr;
	else
		StrFindFunc = strcasestr;

	while (fgets(buf, sizeof(buf), outfp) != NULL) {
		if (StrFindFunc(buf, str) != NULL) {
			fclose(outfp);
			return TRUE;
		}
	}

	fclose(outfp);

	return FALSE;
#endif
}

gboolean procmime_find_string(MsgInfo *msginfo, const gchar *str,
			      gboolean case_sens)
{
	return FALSE;

#if 0	/* FIXME */
	MimeInfo *mimeinfo;
	MimeInfo *partinfo;
	gchar *filename;
	gboolean found = FALSE;

	g_return_val_if_fail(msginfo != NULL, FALSE);
	g_return_val_if_fail(str != NULL, FALSE);

	filename = procmsg_get_message_file(msginfo);
	if (!filename) return FALSE;
	mimeinfo = procmime_scan_message(msginfo);

	for (partinfo = mimeinfo; partinfo != NULL;
	     partinfo = procmime_mimeinfo_next(partinfo)) {
		if (partinfo->mime_type == MIME_TEXT ||
		    partinfo->mime_type == MIME_TEXT_HTML ||
		    partinfo->mime_type == MIME_TEXT_ENRICHED) {
			if (procmime_find_string_part
				(partinfo, filename, str, case_sens) == TRUE) {
				found = TRUE;
				break;
			}
		}
	}

	procmime_mimeinfo_free_all(mimeinfo);
	g_free(filename);

	return found;
#endif
}

gchar *procmime_get_tmp_file_name(MimeInfo *mimeinfo)
{
	static guint32 id = 0;
	gchar *base;
	gchar *filename;
	gchar f_prefix[10];

	g_return_val_if_fail(mimeinfo != NULL, NULL);

	g_snprintf(f_prefix, sizeof(f_prefix), "%08x.", id++);

	if ((mimeinfo->type == MIMETYPE_TEXT) && !g_strcasecmp(mimeinfo->subtype, "html"))
		base = "mimetmp.html";
	else {
		base = mimeinfo->filename ? mimeinfo->filename
			: mimeinfo->name ? mimeinfo->name : "mimetmp";
		base = g_basename(base);
		if (*base == '\0') base = "mimetmp";
		Xstrdup_a(base, base, return NULL);
		subst_for_filename(base);
	}

	filename = g_strconcat(get_mime_tmp_dir(), G_DIR_SEPARATOR_S,
			       f_prefix, base, NULL);

	return filename;
}

static GList *mime_type_list = NULL;

gchar *procmime_get_mime_type(const gchar *filename)
{
	static GHashTable *mime_type_table = NULL;
	MimeType *mime_type;
	const gchar *p;
	gchar *ext;

	if (!mime_type_table) {
		mime_type_table = procmime_get_mime_type_table();
		if (!mime_type_table) return NULL;
	}

	filename = g_basename(filename);
	p = strrchr(filename, '.');
	if (!p) return NULL;

	Xstrdup_a(ext, p + 1, return NULL);
	g_strdown(ext);
	mime_type = g_hash_table_lookup(mime_type_table, ext);
	if (mime_type) {
		gchar *str;

		str = g_strconcat(mime_type->type, "/", mime_type->sub_type,
				  NULL);
		return str;
	}

	return NULL;
}

static guint procmime_str_hash(gconstpointer gptr)
{
	guint hash_result = 0;
	const char *str;

	for (str = gptr; str && *str; str++) {
		if (isupper(*str)) hash_result += (*str + ' ');
		else hash_result += *str;
	}

	return hash_result;
}

static gint procmime_str_equal(gconstpointer gptr1, gconstpointer gptr2)
{
	const char *str1 = gptr1;
	const char *str2 = gptr2;

	return !strcasecmp(str1, str2);
}

static GHashTable *procmime_get_mime_type_table(void)
{
	GHashTable *table = NULL;
	GList *cur;
	MimeType *mime_type;
	gchar **exts;

	if (!mime_type_list) {
		mime_type_list = procmime_get_mime_type_list();
		if (!mime_type_list) return NULL;
	}

	table = g_hash_table_new(procmime_str_hash, procmime_str_equal);

	for (cur = mime_type_list; cur != NULL; cur = cur->next) {
		gint i;
		gchar *key;

		mime_type = (MimeType *)cur->data;

		if (!mime_type->extension) continue;

		exts = g_strsplit(mime_type->extension, " ", 16);
		for (i = 0; exts[i] != NULL; i++) {
			/* make the key case insensitive */
			g_strdown(exts[i]);
			/* use previously dup'd key on overwriting */
			if (g_hash_table_lookup(table, exts[i]))
				key = exts[i];
			else
				key = g_strdup(exts[i]);
			g_hash_table_insert(table, key, mime_type);
		}
		g_strfreev(exts);
	}

	return table;
}

GList *procmime_get_mime_type_list(void)
{
	GList *list = NULL;
	FILE *fp;
	gchar buf[BUFFSIZE];
	gchar *p, *delim;
	MimeType *mime_type;

	if (mime_type_list) 
		return mime_type_list;

	if ((fp = fopen("/etc/mime.types", "rb")) == NULL) {
		if ((fp = fopen(SYSCONFDIR "/mime.types", "rb")) == NULL) {
			FILE_OP_ERROR(SYSCONFDIR "/mime.types", "fopen");
			return NULL;
		}
	}

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		p = strchr(buf, '#');
		if (p) *p = '\0';
		g_strstrip(buf);

		p = buf;
		while (*p && !isspace(*p)) p++;
		if (*p) {
			*p = '\0';
			p++;
		}
		delim = strchr(buf, '/');
		if (delim == NULL) continue;
		*delim = '\0';

		mime_type = g_new(MimeType, 1);
		mime_type->type = g_strdup(buf);
		mime_type->sub_type = g_strdup(delim + 1);

		while (*p && isspace(*p)) p++;
		if (*p)
			mime_type->extension = g_strdup(p);
		else
			mime_type->extension = NULL;

		list = g_list_append(list, mime_type);
	}

	fclose(fp);

	if (!list)
		g_warning("Can't read mime.types\n");

	return list;
}

EncodingType procmime_get_encoding_for_charset(const gchar *charset)
{
	if (!charset)
		return ENC_8BIT;
	else if (!strncasecmp(charset, "ISO-2022-", 9) ||
		 !strcasecmp(charset, "US-ASCII"))
		return ENC_7BIT;
	else if (!strcasecmp(charset, "ISO-8859-5") ||
		 !strncasecmp(charset, "KOI8-", 5) ||
		 !strcasecmp(charset, "Windows-1251"))
		return ENC_8BIT;
	else if (!strncasecmp(charset, "ISO-8859-", 9))
		return ENC_QUOTED_PRINTABLE;
	else
		return ENC_8BIT;
}

EncodingType procmime_get_encoding_for_file(const gchar *file)
{
	FILE *fp;
	guchar buf[BUFSIZ];
	size_t len;

	if ((fp = fopen(file, "rb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		return ENC_UNKNOWN;
	}

	while ((len = fread(buf, sizeof(gchar), sizeof(buf), fp)) > 0) {
		guchar *p;
		gint i;

		for (p = buf, i = 0; i < len; p++, i++) {
			if (*p & 0x80) {
				fclose(fp);
				return ENC_BASE64;
			}
		}
	}

	fclose(fp);
	return ENC_7BIT;
}

struct EncodingTable 
{
	gchar *str;
	EncodingType enc_type;
};

struct EncodingTable encoding_table[] = {
	{"7bit", ENC_7BIT},
	{"8bit", ENC_8BIT},
	{"binary", ENC_BINARY},
	{"quoted-printable", ENC_QUOTED_PRINTABLE},
	{"base64", ENC_BASE64},
	{"x-uuencode", ENC_UNKNOWN},
	{NULL, ENC_UNKNOWN},
};

const gchar *procmime_get_encoding_str(EncodingType encoding)
{
	struct EncodingTable *enc_table;
	
	for (enc_table = encoding_table; enc_table->str != NULL; enc_table++) {
		if (enc_table->enc_type == encoding)
			return enc_table->str;
	}
	return NULL;
}

/* --- NEW MIME STUFF --- */
struct TypeTable
{
	gchar *str;
	MimeMediaType type;
};

static struct TypeTable mime_type_table[] = {
	{"text", MIMETYPE_TEXT},
	{"image", MIMETYPE_IMAGE},
	{"audio", MIMETYPE_AUDIO},
	{"video", MIMETYPE_VIDEO},
	{"application", MIMETYPE_APPLICATION},
	{"message", MIMETYPE_MESSAGE},
	{"multipart", MIMETYPE_MULTIPART},
	{NULL, 0},
};

const gchar *procmime_get_type_str(MimeMediaType type)
{
	struct TypeTable *type_table;
	
	for (type_table = mime_type_table; type_table->str != NULL; type_table++) {
		if (type_table->type == type)
			return type_table->str;
	}
	return NULL;
}

MimeInfo *procmime_parse_mimepart(MimeInfo *parent,
				  gchar *content_type,
				  gchar *content_encoding,
				  gchar *content_description,
				  gchar *content_id,
				  FILE *fp,
				  const gchar *filename,
				  guint offset,
				  guint length);

void procmime_parse_message_rfc822(MimeInfo *mimeinfo, FILE *fp)
{
	HeaderEntry hentry[] = {{"Content-Type:",  NULL, TRUE},
			        {"Content-Transfer-Encoding:",
			  			   NULL, FALSE},
				{"Content-Description:",
						   NULL, TRUE},
			        {"Content-ID:",
						   NULL, TRUE},
				{NULL,		   NULL, FALSE}};
	guint content_start, i;

	fseek(fp, mimeinfo->offset, SEEK_SET);
	procheader_get_header_fields(fp, hentry);
	content_start = ftell(fp);

	mimeinfo->children = procmime_parse_mimepart(mimeinfo,
						     hentry[0].body, hentry[1].body,
						     hentry[2].body, hentry[3].body, 
						     fp, mimeinfo->filename, content_start,
						     mimeinfo->length - (content_start - mimeinfo->offset));
	for (i = 0; i < 4; i++) {
		g_free(hentry[i].body);
		hentry[i].body = NULL;
	}
}

void procmime_parse_multipart(MimeInfo *mimeinfo, FILE *fp)
{
	HeaderEntry hentry[] = {{"Content-Type:",  NULL, TRUE},
			        {"Content-Transfer-Encoding:",
			  			   NULL, FALSE},
				{"Content-Description:",
						   NULL, TRUE},
			        {"Content-ID:",
						   NULL, TRUE},
				{NULL,		   NULL, FALSE}};
	gchar *p;
	gchar *boundary;
	gint boundary_len = 0, lastoffset = -1, i;
	gchar buf[BUFFSIZE];
	MimeInfo *lastmimeinfo = NULL;

	boundary = g_hash_table_lookup(mimeinfo->parameters, "boundary");
	if(!boundary)
		return;
	boundary_len = strlen(boundary);

	fseek(fp, mimeinfo->offset, SEEK_SET);
	while ((p = fgets(buf, sizeof(buf), fp)) != NULL) {
		if (ftell(fp) > (mimeinfo->offset + mimeinfo->length))
			break;

		if (IS_BOUNDARY(buf, boundary, boundary_len)) {
			if(lastoffset != -1) {
				MimeInfo *newmimeinfo = NULL;

				newmimeinfo = procmime_parse_mimepart(mimeinfo,
				                                      hentry[0].body, hentry[1].body,
								      hentry[2].body, hentry[3].body, 
								      fp, mimeinfo->filename, lastoffset,
								      (ftell(fp) - strlen(buf)) - lastoffset);
								      
				if (lastmimeinfo == NULL)
					mimeinfo->children = newmimeinfo;
				else
					lastmimeinfo->next = newmimeinfo;
				lastmimeinfo = newmimeinfo;
			}
			
			if (buf[2 + boundary_len]     == '-' &&
			    buf[2 + boundary_len + 1] == '-')
				break;

			for (i = 0; i < 4; i++) {
				g_free(hentry[i].body);
				hentry[i].body = NULL;
			}
			procheader_get_header_fields(fp, hentry);
			lastoffset = ftell(fp);
		}
	}
}

static void procmime_parse_content_type(const gchar *content_type, MimeInfo *mimeinfo)
{
	gchar **content_type_parts;
	gchar **strarray;
	gchar *str;
	struct TypeTable *typetablearray;
	
	/* Split content type into parts and remove trailing
	   and leading whitespaces from all strings */
	content_type_parts = g_strsplit(content_type, ";", 0);
	for (strarray = content_type_parts; *strarray != NULL; strarray++) {
		g_strstrip(*strarray);
	}

	/* Get mimeinfo->type and mimeinfo->subtype */
	mimeinfo->type = MIMETYPE_UNKNOWN;
	str = content_type_parts[0];
	for (typetablearray = mime_type_table; typetablearray->str != NULL; typetablearray++) {
		if (g_strncasecmp(str, typetablearray->str, strlen(typetablearray->str)) == 0 &&
		    str[strlen(typetablearray->str)] == '/') {
			mimeinfo->type = typetablearray->type;
			mimeinfo->subtype = g_strdup(str + strlen(typetablearray->str) + 1);
			break;
		}
	}

	/* Get mimeinfo->parmeters */
	for (strarray = &content_type_parts[1]; *strarray != NULL; strarray++) {
		gchar **parameters_parts;

		parameters_parts = g_strsplit(*strarray, "=", 2);
		g_strdown(parameters_parts[0]);
		if(parameters_parts[1][0] == '"')
			extract_quote(parameters_parts[1], '"');

		g_hash_table_insert(mimeinfo->parameters,
				    g_strdup(parameters_parts[0]),
				    g_strdup(parameters_parts[1]));

		g_strfreev(parameters_parts);
	}

	g_strfreev(content_type_parts);
}

static void procmime_parse_content_encoding(const gchar *content_encoding, MimeInfo *mimeinfo)
{
	struct EncodingTable *enc_table;
	
	for (enc_table = encoding_table; enc_table->str != NULL; enc_table++) {
		if (g_strcasecmp(enc_table->str, content_encoding) == 0) {
			mimeinfo->encoding_type = enc_table->enc_type;
			return;
		}
	}
	mimeinfo->encoding_type = ENC_UNKNOWN;
	return;
}

MimeInfo *procmime_parse_mimepart(MimeInfo *parent,
				  gchar *content_type,
				  gchar *content_encoding,
				  gchar *content_description,
				  gchar *content_id,
				  FILE *fp,
				  const gchar *filename,
				  guint offset,
				  guint length)
{
	MimeInfo *mimeinfo;
	guint oldpos;

	g_return_val_if_fail(fp != NULL, NULL);

	/* Create MimeInfo */
	mimeinfo = procmime_mimeinfo_new();
	mimeinfo->parent = parent;
	mimeinfo->filename = g_strdup(filename);
	mimeinfo->offset = offset;
	mimeinfo->length = length;

	if (content_type != NULL) {
		procmime_parse_content_type(content_type, mimeinfo);
	} else {
		mimeinfo->type = MIMETYPE_TEXT;
		mimeinfo->subtype = g_strdup("plain");
		g_hash_table_insert(mimeinfo->parameters, g_strdup("charset"), g_strdup("us-ascii"));
	}

	if (content_encoding != NULL) {
		procmime_parse_content_encoding(content_encoding, mimeinfo);
	} else {
		mimeinfo->encoding_type = ENC_7BIT;
	}

	if (content_description != NULL)
		mimeinfo->description = g_strdup(content_description);
	else
		mimeinfo->description = NULL;

	if (content_id != NULL)
		mimeinfo->id = g_strdup(content_id);
	else
		mimeinfo->id = NULL;

	oldpos = ftell(fp);
	/* Call parser for mime type */
	switch (mimeinfo->type) {
		case MIMETYPE_MESSAGE:
			if (g_strcasecmp(mimeinfo->subtype, "rfc822") == 0) {
				procmime_parse_message_rfc822(mimeinfo, fp);
			}
			break;
			
		case MIMETYPE_MULTIPART:
			procmime_parse_multipart(mimeinfo, fp);
			break;
			
		default:
			break;
	}
	fseek(fp, oldpos, SEEK_SET);

	return mimeinfo;
}

static gchar *typenames[] = {
    "text",
    "image",
    "audio",
    "video",
    "application",
    "message",
    "multipart",
    "unknown",
};

static void output_mime_structure(MimeInfo *mimeinfo, int indent)
{
	int i;
	
	for(i = 0; i < indent; i++)
		printf(" ");
	printf("%s/%s (offset:%d length:%d encoding: %d)\n", typenames[mimeinfo->type], mimeinfo->subtype, mimeinfo->offset, mimeinfo->length, mimeinfo->encoding_type);

	if(mimeinfo->children)
		output_mime_structure(mimeinfo->children, indent + 4);
	if(mimeinfo->next)
		output_mime_structure(mimeinfo->next, indent);
}

MimeInfo *procmime_scan_fp(FILE *fp, const gchar *filename)
{
	MimeInfo *mimeinfo;
	gint offset;
	struct stat stat;

	fstat(fileno(fp), &stat);
	offset = ftell(fp);

	mimeinfo = procmime_parse_mimepart(NULL, "message/rfc822", "binary", NULL, NULL, fp, filename, offset, stat.st_size - offset);
	output_mime_structure(mimeinfo, 0);

	return mimeinfo;
}

MimeInfo *procmime_scan_file(gchar *filename)
{
	FILE *fp;
	MimeInfo *mimeinfo;

	g_return_val_if_fail(filename != NULL, NULL);

	/* Open file */
	if((fp = fopen(filename, "rb")) == NULL)
		return NULL;

	mimeinfo = procmime_scan_fp(fp, filename);

	fclose(fp);

	return mimeinfo;
}

MimeInfo *procmime_scan_queue_file(gchar *filename)
{
	FILE *fp;
	MimeInfo *mimeinfo;
	gchar buf[BUFFSIZE];

	g_return_val_if_fail(filename != NULL, NULL);

	/* Open file */
	if((fp = fopen(filename, "rb")) == NULL)
		return NULL;

	/* Skip queue header */
	while (fgets(buf, sizeof(buf), fp) != NULL)
		if (buf[0] == '\r' || buf[0] == '\n') break;

	mimeinfo = procmime_scan_fp(fp, filename);

	fclose(fp);

	return mimeinfo;
}
