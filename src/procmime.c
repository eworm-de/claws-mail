/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2004 Hiroyuki Yamamoto & The Sylpheed-Claws Team
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

#include <stdio.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <stdio.h>
#include <string.h>
#include <locale.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

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
	mimeinfo->content	 = MIMECONTENT_EMPTY;
	mimeinfo->data.filename	 = NULL;

	mimeinfo->type     	 = MIMETYPE_UNKNOWN;
	mimeinfo->encoding_type  = ENC_UNKNOWN;
	mimeinfo->typeparameters = g_hash_table_new(g_str_hash, g_str_equal);

	mimeinfo->disposition	 = DISPOSITIONTYPE_UNKNOWN;
	mimeinfo->dispositionparameters 
				 = g_hash_table_new(g_str_hash, g_str_equal);

	mimeinfo->node           = g_node_new(mimeinfo);
	
	return mimeinfo;
}

static gboolean procmime_mimeinfo_parameters_destroy(gpointer key, gpointer value, gpointer user_data)
{
	g_free(key);
	g_free(value);
	
	return TRUE;
}

static gchar *forced_charset = NULL;

void procmime_force_charset(const gchar *str)
{
	g_free(forced_charset);
	forced_charset = NULL;
	if (str)
		forced_charset = g_strdup(str);
}

static EncodingType forced_encoding = 0;

void procmime_force_encoding(EncodingType encoding)
{
	forced_encoding = encoding;
}

static gboolean free_func(GNode *node, gpointer data)
{
	MimeInfo *mimeinfo = (MimeInfo *) node->data;

	switch (mimeinfo->content) {
	case MIMECONTENT_FILE:
		if (mimeinfo->tmp)
			g_unlink(mimeinfo->data.filename);
		g_free(mimeinfo->data.filename);
		break;

	case MIMECONTENT_MEM:
		if (mimeinfo->tmp)
			g_free(mimeinfo->data.mem);
	default:
		break;
	}

	g_free(mimeinfo->subtype);
	g_free(mimeinfo->description);
	g_free(mimeinfo->id);

	g_hash_table_foreach_remove(mimeinfo->typeparameters,
		procmime_mimeinfo_parameters_destroy, NULL);
	g_hash_table_destroy(mimeinfo->typeparameters);
	g_hash_table_foreach_remove(mimeinfo->dispositionparameters,
		procmime_mimeinfo_parameters_destroy, NULL);
	g_hash_table_destroy(mimeinfo->dispositionparameters);

	if (mimeinfo->privacy)
		privacy_free_privacydata(mimeinfo->privacy);

	g_free(mimeinfo);

	return FALSE;
}

void procmime_mimeinfo_free_all(MimeInfo *mimeinfo)
{
	GNode *node;

	if (!mimeinfo)
		return;

	node = mimeinfo->node;
	g_node_traverse(node, G_IN_ORDER, G_TRAVERSE_ALL, -1, free_func, NULL);

	g_node_destroy(node);
}

#if 0 /* UNUSED */
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
#endif

MimeInfo *procmime_mimeinfo_parent(MimeInfo *mimeinfo)
{
	g_return_val_if_fail(mimeinfo != NULL, NULL);
	g_return_val_if_fail(mimeinfo->node != NULL, NULL);

	if (mimeinfo->node->parent == NULL)
		return NULL;
	return (MimeInfo *) mimeinfo->node->parent->data;
}

MimeInfo *procmime_mimeinfo_next(MimeInfo *mimeinfo)
{
	g_return_val_if_fail(mimeinfo != NULL, NULL);
	g_return_val_if_fail(mimeinfo->node != NULL, NULL);

	if (mimeinfo->node->children)
		return (MimeInfo *) mimeinfo->node->children->data;
	if (mimeinfo->node->next)
		return (MimeInfo *) mimeinfo->node->next->data;

	if (mimeinfo->node->parent == NULL)
		return NULL;

	while (mimeinfo->node->parent != NULL) {
		mimeinfo = (MimeInfo *) mimeinfo->node->parent->data;
		if (mimeinfo->node->next)
			return (MimeInfo *) mimeinfo->node->next->data;
	}

	return NULL;
}

MimeInfo *procmime_scan_message(MsgInfo *msginfo)
{
	gchar *filename;
	MimeInfo *mimeinfo;

	filename = procmsg_get_message_file_path(msginfo);
	if (!filename || !is_file_exist(filename)) {
		g_free(filename);
		filename = procmsg_get_message_file(msginfo);
	}
	if (!filename || !is_file_exist(filename)) 
		return NULL;

	if (!folder_has_parent_of_type(msginfo->folder, F_QUEUE) &&
	    !folder_has_parent_of_type(msginfo->folder, F_DRAFT))
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

const gchar *procmime_mimeinfo_get_parameter(MimeInfo *mimeinfo, const gchar *name)
{
	const gchar *value;

	g_return_val_if_fail(mimeinfo != NULL, NULL);
	g_return_val_if_fail(name != NULL, NULL);

	value = g_hash_table_lookup(mimeinfo->dispositionparameters, name);
	if (value == NULL)
		value = g_hash_table_lookup(mimeinfo->typeparameters, name);
	
	return value;
}

gboolean procmime_decode_content(MimeInfo *mimeinfo)
{
	gchar buf[BUFFSIZE];
	gint readend;
	gchar *tmpfilename;
	FILE *outfp, *infp;
	struct stat statbuf;
	gboolean tmp_file = FALSE;

	EncodingType encoding = forced_encoding 
				? forced_encoding
				: mimeinfo->encoding_type;
		   
	g_return_val_if_fail(mimeinfo != NULL, FALSE);

	if (encoding == ENC_UNKNOWN ||
	    encoding == ENC_BINARY)
		return TRUE;

	infp = g_fopen(mimeinfo->data.filename, "rb");
	if (!infp) {
		perror("fopen");
		return FALSE;
	}
	fseek(infp, mimeinfo->offset, SEEK_SET);

	outfp = get_tmpfile_in_dir(get_mime_tmp_dir(), &tmpfilename);
	if (!outfp) {
		perror("tmpfile");
		return FALSE;
	}
	tmp_file = TRUE;
	readend = mimeinfo->offset + mimeinfo->length;

	if (encoding == ENC_QUOTED_PRINTABLE) {
		while ((ftell(infp) < readend) && (fgets(buf, sizeof(buf), infp) != NULL)) {
			gint len;
			len = qp_decode_line(buf);
			fwrite(buf, 1, len, outfp);
		}
	} else if (encoding == ENC_BASE64) {
		gchar outbuf[BUFFSIZE];
		gint len;
		Base64Decoder *decoder;
		gboolean got_error = FALSE;
		gboolean uncanonicalize = FALSE;
		FILE *tmpfp = outfp;

		if (mimeinfo->type == MIMETYPE_TEXT ||
		    mimeinfo->type == MIMETYPE_MESSAGE) {
			uncanonicalize = TRUE;
			tmpfp = my_tmpfile();
			if (!tmpfp) {
				perror("tmpfile");
				if (tmp_file) fclose(outfp);
				return FALSE;
			}
		}

		decoder = base64_decoder_new();
		while ((ftell(infp) < readend) && (fgets(buf, sizeof(buf), infp) != NULL)) {
			len = base64_decoder_decode(decoder, buf, outbuf);
			if (len < 0 && !got_error) {
				g_warning("Bad BASE64 content.\n");
				fwrite(_("[Error decoding BASE64]\n"),
					sizeof(gchar),
					strlen(_("[Error decoding BASE64]\n")),
					tmpfp);
				got_error = TRUE;
				continue;
			} else if (len >= 0) {
				/* print out the error message only once 
				 * per block */
				fwrite(outbuf, sizeof(gchar), len, tmpfp);
				got_error = FALSE;
			}
		}
		base64_decoder_free(decoder);

		if (uncanonicalize) {
			rewind(tmpfp);
			while (fgets(buf, sizeof(buf), tmpfp) != NULL) {
				strcrchomp(buf);
				fputs(buf, outfp);
			}
			fclose(tmpfp);
		}
	} else if (encoding == ENC_X_UUENCODE) {
		gchar outbuf[BUFFSIZE];
		gint len;
		gboolean flag = FALSE;

		while ((ftell(infp) < readend) && (fgets(buf, sizeof(buf), infp) != NULL)) {
			if (!flag && strncmp(buf,"begin ", 6)) continue;

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
	if (mimeinfo->tmp && (mimeinfo->data.filename != NULL))
		g_unlink(mimeinfo->data.filename);
	if (mimeinfo->data.filename != NULL)
		g_free(mimeinfo->data.filename);
	mimeinfo->data.filename = tmpfilename;
	mimeinfo->tmp = TRUE;
	mimeinfo->offset = 0;
	mimeinfo->length = statbuf.st_size;
	mimeinfo->encoding_type = ENC_BINARY;

	return TRUE;
}

#define B64_LINE_SIZE		57
#define B64_BUFFSIZE		77

gboolean procmime_encode_content(MimeInfo *mimeinfo, EncodingType encoding)
{
	FILE *infp = NULL, *outfp;
	gint len;
	gchar *tmpfilename;
	struct stat statbuf;

	if (mimeinfo->content == MIMECONTENT_EMPTY)
		return TRUE;

	if (mimeinfo->encoding_type != ENC_UNKNOWN &&
	    mimeinfo->encoding_type != ENC_BINARY &&
	    mimeinfo->encoding_type != ENC_7BIT &&
	    mimeinfo->encoding_type != ENC_8BIT)
		if(!procmime_decode_content(mimeinfo))
			return FALSE;

	outfp = get_tmpfile_in_dir(get_mime_tmp_dir(), &tmpfilename);
	if (!outfp) {
		perror("tmpfile");
		return FALSE;
	}

	if (mimeinfo->content == MIMECONTENT_FILE) {
		if ((infp = g_fopen(mimeinfo->data.filename, "rb")) == NULL) {
			g_warning("Can't open file %s\n", mimeinfo->data.filename);
			return FALSE;
		}
	} else if (mimeinfo->content == MIMECONTENT_MEM) {
		infp = str_open_as_stream(mimeinfo->data.mem);
		if (infp == NULL)
			return FALSE;
	}

	if (encoding == ENC_BASE64) {
		gchar inbuf[B64_LINE_SIZE], outbuf[B64_BUFFSIZE];
		FILE *tmp_fp = infp;
		gchar *tmp_file = NULL;

		if (mimeinfo->type == MIMETYPE_TEXT ||
		     mimeinfo->type == MIMETYPE_MESSAGE) {
		     	if (mimeinfo->content == MIMECONTENT_FILE) {
				tmp_file = get_tmp_file();
				if (canonicalize_file(mimeinfo->data.filename, tmp_file) < 0) {
					g_free(tmp_file);
					fclose(infp);
					return FALSE;
				}
				if ((tmp_fp = g_fopen(tmp_file, "rb")) == NULL) {
					FILE_OP_ERROR(tmp_file, "fopen");
					g_unlink(tmp_file);
					g_free(tmp_file);
					fclose(infp);
					return FALSE;
				}
			} else {
				gchar *out = canonicalize_str(mimeinfo->data.mem);
				fclose(infp);
				infp = str_open_as_stream(out);
				tmp_fp = infp;
				g_free(out);
				if (infp == NULL)
					return FALSE;
			}
		}

		while ((len = fread(inbuf, sizeof(gchar),
				    B64_LINE_SIZE, tmp_fp))
		       == B64_LINE_SIZE) {
			base64_encode(outbuf, inbuf, B64_LINE_SIZE);
			fputs(outbuf, outfp);
			fputc('\n', outfp);
		}
		if (len > 0 && feof(tmp_fp)) {
			base64_encode(outbuf, inbuf, len);
			fputs(outbuf, outfp);
			fputc('\n', outfp);
		}

		if (tmp_file) {
			fclose(tmp_fp);
			g_unlink(tmp_file);
			g_free(tmp_file);
		}
	} else if (encoding == ENC_QUOTED_PRINTABLE) {
		gchar inbuf[BUFFSIZE], outbuf[BUFFSIZE * 4];

		while (fgets(inbuf, sizeof(inbuf), infp) != NULL) {
			qp_encode_line(outbuf, inbuf);

			if (!strncmp("From ", outbuf, sizeof("From ")-1)) {
				gchar *tmpbuf = outbuf;
				
				tmpbuf += sizeof("From ")-1;
				
				fputs("=46rom ", outfp);
				fputs(tmpbuf, outfp);
			} else 
				fputs(outbuf, outfp);
		}
	} else {
		gchar buf[BUFFSIZE];

		while (fgets(buf, sizeof(buf), infp) != NULL) {
			strcrchomp(buf);
			fputs(buf, outfp);
		}
	}

	fclose(outfp);
	fclose(infp);

	if (mimeinfo->content == MIMECONTENT_FILE) {
		if (mimeinfo->tmp && (mimeinfo->data.filename != NULL))
			g_unlink(mimeinfo->data.filename);
		g_free(mimeinfo->data.filename);
	} else if (mimeinfo->content == MIMECONTENT_MEM) {
		if (mimeinfo->tmp && (mimeinfo->data.mem != NULL))
			g_free(mimeinfo->data.mem);
	}

	stat(tmpfilename, &statbuf);
	mimeinfo->content = MIMECONTENT_FILE;
	mimeinfo->data.filename = tmpfilename;
	mimeinfo->tmp = TRUE;
	mimeinfo->offset = 0;
	mimeinfo->length = statbuf.st_size;
	mimeinfo->encoding_type = encoding;

	return TRUE;
}

gint procmime_get_part(const gchar *outfile, MimeInfo *mimeinfo)
{
	FILE *infp, *outfp;
	gchar buf[BUFFSIZE];
	gint restlength, readlength;

	g_return_val_if_fail(outfile != NULL, -1);
	g_return_val_if_fail(mimeinfo != NULL, -1);

	if (mimeinfo->encoding_type != ENC_BINARY && !procmime_decode_content(mimeinfo))
		return -1;

	if ((infp = g_fopen(mimeinfo->data.filename, "rb")) == NULL) {
		FILE_OP_ERROR(mimeinfo->data.filename, "fopen");
		return -1;
	}
	if (fseek(infp, mimeinfo->offset, SEEK_SET) < 0) {
		FILE_OP_ERROR(mimeinfo->data.filename, "fseek");
		fclose(infp);
		return -1;
	}
	if ((outfp = g_fopen(outfile, "wb")) == NULL) {
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
		g_unlink(outfile);
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
	f = g_fopen(rcpath, "rb");
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

	for (cur = renderer_list ; cur != NULL ; cur = cur->next) {
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

FILE *procmime_get_text_content(MimeInfo *mimeinfo)
{
	FILE *tmpfp, *outfp;
	const gchar *src_codeset;
	gboolean conv_fail = FALSE;
	gchar buf[BUFFSIZE];
	gchar *str;
	struct ContentRenderer * renderer;
	GList * cur;
	gchar *tmpfile, *content_type;
    
	g_return_val_if_fail(mimeinfo != NULL, NULL);

	if (!procmime_decode_content(mimeinfo))
		return NULL;

	tmpfile = procmime_get_tmp_file_name(mimeinfo);
	if (tmpfile == NULL)
		return NULL;

	if (procmime_get_part(tmpfile, mimeinfo) < 0) {
		g_free(tmpfile);
		return NULL;
	}

	tmpfp = g_fopen(tmpfile, "rb");
	if (tmpfp == NULL) {
		g_free(tmpfile);
		return NULL;
	}

	if ((outfp = my_tmpfile()) == NULL) {
		perror("tmpfile");
		fclose(tmpfp);
		g_free(tmpfile);
		return NULL;
	}

	src_codeset = forced_charset
		      ? forced_charset : 
		      procmime_mimeinfo_get_parameter(mimeinfo, "charset");

	renderer = NULL;

	content_type = procmime_get_content_type_str(mimeinfo->type,
						     mimeinfo->subtype);
	for (cur = renderer_list ; cur != NULL ; cur = cur->next) {
		struct ContentRenderer * cr;

		cr = cur->data;
		if (g_ascii_strcasecmp(cr->content_type, content_type) == 0) {
			renderer = cr;
			break;
		}
	}
	g_free(content_type);

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
/* CodeConverter seems to have no effect here */
	} else if (mimeinfo->type == MIMETYPE_TEXT && !g_ascii_strcasecmp(mimeinfo->subtype, "html")) {
		HTMLParser *parser;
		CodeConverter *conv;

		conv = conv_code_converter_new(src_codeset);
		parser = html_parser_new(tmpfp, conv);
		while ((str = html_parse(parser)) != NULL) {
			fputs(str, outfp);
		}
		html_parser_destroy(parser);
		conv_code_converter_destroy(conv);
	} else if (mimeinfo->type == MIMETYPE_TEXT && !g_ascii_strcasecmp(mimeinfo->subtype, "enriched")) {
		ERTFParser *parser;
		CodeConverter *conv;

		conv = conv_code_converter_new(src_codeset);
		parser = ertf_parser_new(tmpfp, conv);
		while ((str = ertf_parse(parser)) != NULL) {
			fputs(str, outfp);
		}
		ertf_parser_destroy(parser);
		conv_code_converter_destroy(conv);
	} else if (mimeinfo->type == MIMETYPE_TEXT) {
		while (fgets(buf, sizeof(buf), tmpfp) != NULL) {
			str = conv_codeset_strdup(buf, src_codeset, CS_UTF_8);
			if (str) {
				fputs(str, outfp);
				g_free(str);
			} else {
				conv_fail = TRUE;
				fputs(buf, outfp);
			}
		}
	}

	if (conv_fail)
		g_warning("procmime_get_text_content(): Code conversion failed.\n");

	fclose(tmpfp);
	rewind(outfp);
	g_unlink(tmpfile);
	g_free(tmpfile);

	return outfp;
}

/* search the first text part of (multipart) MIME message,
   decode, convert it and output to outfp. */
FILE *procmime_get_first_text_content(MsgInfo *msginfo)
{
	FILE *outfp = NULL;
	MimeInfo *mimeinfo, *partinfo;

	g_return_val_if_fail(msginfo != NULL, NULL);

	mimeinfo = procmime_scan_message(msginfo);
	if (!mimeinfo) return NULL;

	partinfo = mimeinfo;
	while (partinfo && partinfo->type != MIMETYPE_TEXT) {
		partinfo = procmime_mimeinfo_next(partinfo);
	}
	if (partinfo)
		outfp = procmime_get_text_content(partinfo);

	procmime_mimeinfo_free_all(mimeinfo);

	return outfp;
}


static gboolean find_encrypted_func(GNode *node, gpointer data)
{
	MimeInfo *mimeinfo = (MimeInfo *) node->data;
	MimeInfo **encinfo = (MimeInfo **) data;
	
	if (privacy_mimeinfo_is_encrypted(mimeinfo)) {
		*encinfo = mimeinfo;
		return TRUE;
	}
	
	return FALSE;
}

static MimeInfo *find_encrypted_part(MimeInfo *rootinfo)
{
	MimeInfo *encinfo = NULL;

	g_node_traverse(rootinfo->node, G_IN_ORDER, G_TRAVERSE_ALL, -1,
		find_encrypted_func, &encinfo);
	
	return encinfo;
}

/* search the first encrypted text part of (multipart) MIME message,
   decode, convert it and output to outfp. */
FILE *procmime_get_first_encrypted_text_content(MsgInfo *msginfo)
{
	FILE *outfp = NULL;
	MimeInfo *mimeinfo, *partinfo, *encinfo;

	g_return_val_if_fail(msginfo != NULL, NULL);

	mimeinfo = procmime_scan_message(msginfo);
	if (!mimeinfo) {
		return NULL;
	}

	partinfo = mimeinfo;
	if ((encinfo = find_encrypted_part(partinfo)) != NULL) {
		debug_print("decrypting message part\n");
		if (privacy_mimeinfo_decrypt(encinfo) < 0)
			return NULL;
	}
	partinfo = mimeinfo;
	while (partinfo && partinfo->type != MIMETYPE_TEXT) {
		partinfo = procmime_mimeinfo_next(partinfo);
	}

	if (partinfo)
		outfp = procmime_get_text_content(partinfo);

	procmime_mimeinfo_free_all(mimeinfo);

	return outfp;
}

gboolean procmime_msginfo_is_encrypted(MsgInfo *msginfo)
{
	MimeInfo *mimeinfo, *partinfo;
	gboolean result = FALSE;

	g_return_val_if_fail(msginfo != NULL, FALSE);

	mimeinfo = procmime_scan_message(msginfo);
	if (!mimeinfo) {
		return FALSE;
	}

	partinfo = mimeinfo;
	result = (find_encrypted_part(partinfo) != NULL);
	procmime_mimeinfo_free_all(mimeinfo);

	return result;
}

gboolean procmime_find_string_part(MimeInfo *mimeinfo, const gchar *filename,
				   const gchar *str, StrFindFunc find_func)
{
	FILE *outfp;
	gchar buf[BUFFSIZE];

	g_return_val_if_fail(mimeinfo != NULL, FALSE);
	g_return_val_if_fail(mimeinfo->type == MIMETYPE_TEXT, FALSE);
	g_return_val_if_fail(str != NULL, FALSE);
	g_return_val_if_fail(find_func != NULL, FALSE);

	outfp = procmime_get_text_content(mimeinfo);

	if (!outfp)
		return FALSE;

	while (fgets(buf, sizeof(buf), outfp) != NULL) {
		strretchomp(buf);
		if (find_func(buf, str)) {
			fclose(outfp);
			return TRUE;
		}
	}

	fclose(outfp);

	return FALSE;
}

gboolean procmime_find_string(MsgInfo *msginfo, const gchar *str,
			      StrFindFunc find_func)
{
	MimeInfo *mimeinfo;
	MimeInfo *partinfo;
	gchar *filename;
	gboolean found = FALSE;

	g_return_val_if_fail(msginfo != NULL, FALSE);
	g_return_val_if_fail(str != NULL, FALSE);
	g_return_val_if_fail(find_func != NULL, FALSE);

	filename = procmsg_get_message_file(msginfo);
	if (!filename) return FALSE;
	mimeinfo = procmime_scan_message(msginfo);

	for (partinfo = mimeinfo; partinfo != NULL;
	     partinfo = procmime_mimeinfo_next(partinfo)) {
		if (partinfo->type == MIMETYPE_TEXT) {
			if (procmime_find_string_part
				(partinfo, filename, str, find_func) == TRUE) {
				found = TRUE;
				break;
			}
		}
	}

	procmime_mimeinfo_free_all(mimeinfo);
	g_free(filename);

	return found;
}

gchar *procmime_get_tmp_file_name(MimeInfo *mimeinfo)
{
	static guint32 id = 0;
	gchar *base;
	gchar *filename;
	gchar f_prefix[10];

	g_return_val_if_fail(mimeinfo != NULL, NULL);

	g_snprintf(f_prefix, sizeof(f_prefix), "%08x.", id++);

	if ((mimeinfo->type == MIMETYPE_TEXT) && !g_ascii_strcasecmp(mimeinfo->subtype, "html"))
		base = g_strdup("mimetmp.html");
	else {
		const gchar *basetmp;

		basetmp = procmime_mimeinfo_get_parameter(mimeinfo, "filename");
		if (basetmp == NULL)
			basetmp = procmime_mimeinfo_get_parameter(mimeinfo, "name");
		if (basetmp == NULL)
			basetmp = "mimetmp";
		basetmp = g_path_get_basename(basetmp);
		if (*basetmp == '\0') basetmp = g_strdup("mimetmp");
		base = conv_filename_from_utf8(basetmp);
		subst_for_shellsafe_filename(base);
	}

	filename = g_strconcat(get_mime_tmp_dir(), G_DIR_SEPARATOR_S,
			       f_prefix, base, NULL);

	g_free(base);
	
	return filename;
}

static GList *mime_type_list = NULL;

gchar *procmime_get_mime_type(const gchar *filename)
{
	static GHashTable *mime_type_table = NULL;
	MimeType *mime_type;
	const gchar *p;
	gchar *ext = NULL;
	gchar *base;

	if (!mime_type_table) {
		mime_type_table = procmime_get_mime_type_table();
		if (!mime_type_table) return NULL;
	}

	base = g_path_get_basename(filename);
	if ((p = strrchr(base, '.')) != NULL)
		Xstrdup_a(ext, p + 1, p = NULL );
	g_free(base);
	if (!p) return NULL;

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

	return !g_utf8_collate(str1, str2);
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
	gchar *p;
	gchar *delim;
	MimeType *mime_type;
	gboolean fp_is_glob_file = TRUE;

	if (mime_type_list) 
		return mime_type_list;
	
	if ((fp = g_fopen("/usr/share/mime/globs", "rb")) == NULL) {
		fp_is_glob_file = FALSE;
		if ((fp = g_fopen("/etc/mime.types", "rb")) == NULL) {
			if ((fp = g_fopen(SYSCONFDIR "/mime.types", "rb")) 
				== NULL) {
				FILE_OP_ERROR(SYSCONFDIR "/mime.types", 
					"fopen");
				return NULL;
			}
		}
	}

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		p = strchr(buf, '#');
		if (p) *p = '\0';
		g_strstrip(buf);

		p = buf;
		
		if (fp_is_glob_file) {
			while (*p && !g_ascii_isspace(*p) && (*p!=':')) p++;
		} else {
			while (*p && !g_ascii_isspace(*p)) p++;
		}

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

		if (fp_is_glob_file) {
			while (*p && (g_ascii_isspace(*p)||(*p=='*')||(*p=='.'))) p++;
		} else {
			while (*p && g_ascii_isspace(*p)) p++;
		}

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
	else if (!g_ascii_strncasecmp(charset, "ISO-2022-", 9) ||
		 !g_ascii_strcasecmp(charset, "US-ASCII"))
		return ENC_7BIT;
	else if (!g_ascii_strcasecmp(charset, "ISO-8859-5") ||
		 !g_ascii_strncasecmp(charset, "KOI8-", 5) ||
		 !g_ascii_strcasecmp(charset, "Windows-1251"))
		return ENC_8BIT;
	else if (!g_ascii_strncasecmp(charset, "ISO-8859-", 9))
		return ENC_QUOTED_PRINTABLE;
	else
		return ENC_8BIT;
}

EncodingType procmime_get_encoding_for_text_file(const gchar *file)
{
	FILE *fp;
	guchar buf[BUFFSIZE];
	size_t len;
	size_t octet_chars = 0;
	size_t total_len = 0;
	gfloat octet_percentage;

	if ((fp = g_fopen(file, "rb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		return ENC_UNKNOWN;
	}

	while ((len = fread(buf, sizeof(guchar), sizeof(buf), fp)) > 0) {
		guchar *p;
		gint i;

		for (p = buf, i = 0; i < len; ++p, ++i) {
			if (*p & 0x80)
				++octet_chars;
		}
		total_len += len;
	}

	fclose(fp);
	
	if (total_len > 0)
		octet_percentage = (gfloat)octet_chars / (gfloat)total_len;
	else
		octet_percentage = 0.0;

	debug_print("procmime_get_encoding_for_text_file(): "
		    "8bit chars: %d / %d (%f%%)\n", octet_chars, total_len,
		    100.0 * octet_percentage);

	if (octet_percentage > 0.20) {
		debug_print("using BASE64\n");
		return ENC_BASE64;
	} else if (octet_chars > 0) {
		debug_print("using quoted-printable\n");
		return ENC_QUOTED_PRINTABLE;
	} else {
		debug_print("using 7bit\n");
		return ENC_7BIT;
	}
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

const gchar *procmime_get_media_type_str(MimeMediaType type)
{
	struct TypeTable *type_table;
	
	for (type_table = mime_type_table; type_table->str != NULL; type_table++) {
		if (type_table->type == type)
			return type_table->str;
	}
	return NULL;
}

MimeMediaType procmime_get_media_type(const gchar *str)
{
	struct TypeTable *typetablearray;

	for (typetablearray = mime_type_table; typetablearray->str != NULL; typetablearray++)
		if (g_ascii_strncasecmp(str, typetablearray->str, strlen(typetablearray->str)) == 0)
			return typetablearray->type;

	return MIMETYPE_UNKNOWN;
}

/*!
 *\brief	Safe wrapper for content type string.
 *
 *\return	const gchar * Pointer to content type string. 
 */
gchar *procmime_get_content_type_str(MimeMediaType type,
					   const char *subtype)
{
	const gchar *type_str = NULL;

	if (subtype == NULL || !(type_str = procmime_get_media_type_str(type)))
		return g_strdup("unknown");
	return g_strdup_printf("%s/%s", type_str, subtype);
}

int procmime_parse_mimepart(MimeInfo *parent,
			     gchar *content_type,
			     gchar *content_encoding,
			     gchar *content_description,
			     gchar *content_id,
			     gchar *content_disposition,
			     const gchar *filename,
			     guint offset,
			     guint length);

void procmime_parse_message_rfc822(MimeInfo *mimeinfo)
{
	HeaderEntry hentry[] = {{"Content-Type:",  NULL, TRUE},
			        {"Content-Transfer-Encoding:",
			  			   NULL, FALSE},
				{"Content-Description:",
						   NULL, TRUE},
			        {"Content-ID:",
						   NULL, TRUE},
				{"Content-Disposition:",
				                   NULL, TRUE},
				{"MIME-Version:",
						   NULL, TRUE},
				{NULL,		   NULL, FALSE}};
	guint content_start, i;
	FILE *fp;
        gchar *tmp;

	procmime_decode_content(mimeinfo);

	fp = g_fopen(mimeinfo->data.filename, "rb");
	if (fp == NULL) {
		FILE_OP_ERROR(mimeinfo->data.filename, "fopen");
		return;
	}
	fseek(fp, mimeinfo->offset, SEEK_SET);
	procheader_get_header_fields(fp, hentry);
	if (hentry[0].body != NULL) {
                tmp = conv_unmime_header(hentry[0].body, NULL);
                g_free(hentry[0].body);
                hentry[0].body = tmp;
        }                
	if (hentry[2].body != NULL) {
                tmp = conv_unmime_header(hentry[2].body, NULL);
                g_free(hentry[2].body);
                hentry[2].body = tmp;
        }                
	if (hentry[4].body != NULL) {
                tmp = conv_unmime_header(hentry[4].body, NULL);
                g_free(hentry[4].body);
                hentry[4].body = tmp;
        }                
	content_start = ftell(fp);
	fclose(fp);
	
	procmime_parse_mimepart(mimeinfo,
				hentry[0].body, hentry[1].body,
				hentry[2].body, hentry[3].body,
				hentry[4].body,
				mimeinfo->data.filename, content_start,
				mimeinfo->length - (content_start - mimeinfo->offset));
	
	for (i = 0; i < (sizeof hentry / sizeof hentry[0]); i++) {
		g_free(hentry[i].body);
		hentry[i].body = NULL;
	}
}

void procmime_parse_multipart(MimeInfo *mimeinfo)
{
	HeaderEntry hentry[] = {{"Content-Type:",  NULL, TRUE},
			        {"Content-Transfer-Encoding:",
			  			   NULL, FALSE},
				{"Content-Description:",
						   NULL, TRUE},
			        {"Content-ID:",
						   NULL, TRUE},
				{"Content-Disposition:",
				                   NULL, TRUE},
				{NULL,		   NULL, FALSE}};
	gchar *p, *tmp;
	gchar *boundary;
	gint boundary_len = 0, lastoffset = -1, i;
	gchar buf[BUFFSIZE];
	FILE *fp;
	int result = 0;

	boundary = g_hash_table_lookup(mimeinfo->typeparameters, "boundary");
	if (!boundary)
		return;
	boundary_len = strlen(boundary);

	procmime_decode_content(mimeinfo);

	fp = g_fopen(mimeinfo->data.filename, "rb");
	if (fp == NULL) {
		FILE_OP_ERROR(mimeinfo->data.filename, "fopen");
		return;
	}
	fseek(fp, mimeinfo->offset, SEEK_SET);
	while ((p = fgets(buf, sizeof(buf), fp)) != NULL && result == 0) {
		if (ftell(fp) > (mimeinfo->offset + mimeinfo->length))
			break;

		if (IS_BOUNDARY(buf, boundary, boundary_len)) {
			if (lastoffset != -1) {
				result = procmime_parse_mimepart(mimeinfo,
				                        hentry[0].body, hentry[1].body,
							hentry[2].body, hentry[3].body, 
							hentry[4].body, 
							mimeinfo->data.filename, lastoffset,
							(ftell(fp) - strlen(buf)) - lastoffset - 1);
			}
			
			if (buf[2 + boundary_len]     == '-' &&
			    buf[2 + boundary_len + 1] == '-')
				break;

			for (i = 0; i < (sizeof hentry / sizeof hentry[0]) ; i++) {
				g_free(hentry[i].body);
				hentry[i].body = NULL;
			}
			procheader_get_header_fields(fp, hentry);
                        if (hentry[0].body != NULL) {
                                tmp = conv_unmime_header(hentry[0].body, NULL);
                                g_free(hentry[0].body);
                                hentry[0].body = tmp;
                        }                
                        if (hentry[2].body != NULL) {
                                tmp = conv_unmime_header(hentry[2].body, NULL);
                                g_free(hentry[2].body);
                                hentry[2].body = tmp;
                        }                
                        if (hentry[4].body != NULL) {
                                tmp = conv_unmime_header(hentry[4].body, NULL);
                                g_free(hentry[4].body);
                                hentry[4].body = tmp;
                        }                
			lastoffset = ftell(fp);
		}
	}
	for (i = 0; i < (sizeof hentry / sizeof hentry[0]); i++) {
		g_free(hentry[i].body);
		hentry[i].body = NULL;
	}
	fclose(fp);
}

static void parse_parameters(const gchar *parameters, GHashTable *table)
{
	gchar *params, *param, *next;
	GSList *convlist = NULL, *concatlist = NULL, *cur;

	params = g_strdup(parameters);
	param = params;
	next = params;
	for (; next != NULL; param = next) {
		gchar *attribute, *value, *tmp;
		gint len;
		gboolean convert = FALSE;

		next = strchr_with_skip_quote(param, '"', ';');
		if (next != NULL) {
			next[0] = '\0';
			next++;
		}

		g_strstrip(param);

		attribute = param;
		value = strchr(attribute, '=');
		if (value == NULL)
			continue;

		value[0] = '\0';
		value++;

		g_strdown(attribute);

		len = strlen(attribute);
		if (attribute[len - 1] == '*') {
			gchar *srcpos, *dstpos, *endpos;

			convert = TRUE;
			attribute[len - 1] = '\0';

			srcpos = value;
			dstpos = value;
			endpos = value + strlen(value);
			while (srcpos < endpos) {
				if (*srcpos != '%')
					*dstpos = *srcpos;
				else {
					guchar dstvalue;

					if (!get_hex_value(&dstvalue, srcpos[1], srcpos[2]))
						*dstpos = '?';
					else
						*dstpos = dstvalue;
					srcpos += 2;
				}
				srcpos++;
				dstpos++;
			}
			*dstpos = '\0';
		} else {
			if (value[0] == '"')
				extract_quote(value, '"');
			else if ((tmp = strchr(value, ' ')) != NULL)
				*tmp = '\0';
		}

		if (strrchr(attribute, '*') != NULL) {
			gchar *tmpattr;

			tmpattr = g_strdup(attribute);
			tmp = strrchr(tmpattr, '*');
			tmp[0] = '\0';

			if ((tmp[1] == '0') && (tmp[2] == '\0') && 
			    (g_slist_find_custom(concatlist, attribute, g_str_equal) == NULL))
				concatlist = g_slist_prepend(concatlist, g_strdup(tmpattr));

			if (convert && (g_slist_find_custom(convlist, attribute, g_str_equal) == NULL))
				convlist = g_slist_prepend(convlist, g_strdup(tmpattr));

			g_free(tmpattr);
		} else if (convert) {
			if (g_slist_find_custom(convlist, attribute, g_str_equal) == NULL)
				convlist = g_slist_prepend(convlist, g_strdup(attribute));
		}

		if (g_hash_table_lookup(table, attribute) == NULL)
			g_hash_table_insert(table, g_strdup(attribute), g_strdup(value));
	}

	for (cur = concatlist; cur != NULL; cur = g_slist_next(cur)) {
		gchar *attribute, *attrwnum, *partvalue;
		gint n = 0;
		GString *value;

		attribute = (gchar *) cur->data;
		value = g_string_sized_new(64);

		attrwnum = g_strdup_printf("%s*%d", attribute, n);
		while ((partvalue = g_hash_table_lookup(table, attrwnum)) != NULL) {
			g_string_append(value, partvalue);

			g_free(attrwnum);
			n++;
			attrwnum = g_strdup_printf("%s*%d", attribute, n);
		}
		g_free(attrwnum);

		g_hash_table_insert(table, g_strdup(attribute), g_strdup(value->str));
		g_string_free(value, TRUE);
	}
	slist_free_strings(concatlist);
	g_slist_free(concatlist);

	for (cur = convlist; cur != NULL; cur = g_slist_next(cur)) {
		gchar *attribute, *key, *value;
		gchar *charset, *lang, *oldvalue, *newvalue;

		attribute = (gchar *) cur->data;
		if (!g_hash_table_lookup_extended(table, attribute, (gpointer *) &key, (gpointer *) &value))
			continue;

		charset = value;
		lang = strchr(charset, '\'');
		if (lang == NULL)
			continue;
		lang[0] = '\0';
		lang++;
		oldvalue = strchr(lang, '\'');
		if (oldvalue == NULL)
			continue;
		oldvalue[0] = '\0';
		oldvalue++;

		newvalue = conv_codeset_strdup(oldvalue, charset, CS_UTF_8);

		g_hash_table_remove(table, attribute);
		g_free(key);
		g_free(value);

		g_hash_table_insert(table, g_strdup(attribute), newvalue);
	}
	slist_free_strings(convlist);
	g_slist_free(convlist);

	g_free(params);
}	

static void procmime_parse_content_type(const gchar *content_type, MimeInfo *mimeinfo)
{
	g_return_if_fail(content_type != NULL);
	g_return_if_fail(mimeinfo != NULL);

	/* RFC 2045, page 13 says that the mime subtype is MANDATORY;
	 * if it's not available we use the default Content-Type */
	if ((content_type[0] == '\0') || (strchr(content_type, '/') == NULL)) {
		mimeinfo->type = MIMETYPE_TEXT;
		mimeinfo->subtype = g_strdup("plain");
		if (g_hash_table_lookup(mimeinfo->typeparameters,
				       "charset") == NULL)
			g_hash_table_insert(mimeinfo->typeparameters,
					    g_strdup("charset"),
					    g_strdup("us-ascii"));
	} else {
		gchar *type, *subtype, *params;

		type = g_strdup(content_type);
		subtype = strchr(type, '/') + 1;
		*(subtype - 1) = '\0';
		if ((params = strchr(subtype, ';')) != NULL) {
			params[0] = '\0';
			params++;
		}

		mimeinfo->type = procmime_get_media_type(type);
		mimeinfo->subtype = g_strdup(subtype);

		/* Get mimeinfo->typeparameters */
		if (params != NULL)
			parse_parameters(params, mimeinfo->typeparameters);

		g_free(type);
	}
}

static void procmime_parse_content_disposition(const gchar *content_disposition, MimeInfo *mimeinfo)
{
	gchar *tmp, *params;

	g_return_if_fail(content_disposition != NULL);
	g_return_if_fail(mimeinfo != NULL);

	tmp = g_strdup(content_disposition);
	if ((params = strchr(tmp, ';')) != NULL) {
		params[0] = '\0';
		params++;
	}	
	g_strstrip(tmp);

	if (!g_ascii_strcasecmp(tmp, "inline")) 
		mimeinfo->disposition = DISPOSITIONTYPE_INLINE;
	else if (!g_ascii_strcasecmp(tmp, "attachment"))
		mimeinfo->disposition = DISPOSITIONTYPE_ATTACHMENT;
	else
		mimeinfo->disposition = DISPOSITIONTYPE_ATTACHMENT;
	
	if (params != NULL)
		parse_parameters(params, mimeinfo->dispositionparameters);

	g_free(tmp);
}


static void procmime_parse_content_encoding(const gchar *content_encoding, MimeInfo *mimeinfo)
{
	struct EncodingTable *enc_table;
	
	for (enc_table = encoding_table; enc_table->str != NULL; enc_table++) {
		if (g_ascii_strcasecmp(enc_table->str, content_encoding) == 0) {
			mimeinfo->encoding_type = enc_table->enc_type;
			return;
		}
	}
	mimeinfo->encoding_type = ENC_UNKNOWN;
	return;
}

int procmime_parse_mimepart(MimeInfo *parent,
			     gchar *content_type,
			     gchar *content_encoding,
			     gchar *content_description,
			     gchar *content_id,
			     gchar *content_disposition,
			     const gchar *filename,
			     guint offset,
			     guint length)
{
	MimeInfo *mimeinfo;

	/* Create MimeInfo */
	mimeinfo = procmime_mimeinfo_new();
	mimeinfo->content = MIMECONTENT_FILE;
	if (parent != NULL) {
		if (g_node_depth(parent->node) > 32) {
			/* 32 is an arbitrary value
			 * this avoids DOSsing ourselves 
			 * with enormous messages
			 */
			procmime_mimeinfo_free_all(mimeinfo);
			return -1;			
		}
		g_node_append(parent->node, mimeinfo->node);
	}
	mimeinfo->data.filename = g_strdup(filename);
	mimeinfo->offset = offset;
	mimeinfo->length = length;

	if (content_type != NULL) {
		procmime_parse_content_type(content_type, mimeinfo);
	} else {
		mimeinfo->type = MIMETYPE_TEXT;
		mimeinfo->subtype = g_strdup("plain");
		if (g_hash_table_lookup(mimeinfo->typeparameters,
				       "charset") == NULL)
			g_hash_table_insert(mimeinfo->typeparameters, g_strdup("charset"), g_strdup("us-ascii"));
	}

	if (content_encoding != NULL) {
		procmime_parse_content_encoding(content_encoding, mimeinfo);
	} else {
		mimeinfo->encoding_type = ENC_UNKNOWN;
	}

	if (content_description != NULL)
		mimeinfo->description = g_strdup(content_description);
	else
		mimeinfo->description = NULL;

	if (content_id != NULL)
		mimeinfo->id = g_strdup(content_id);
	else
		mimeinfo->id = NULL;

	if (content_disposition != NULL) 
		procmime_parse_content_disposition(content_disposition, mimeinfo);
	else
		mimeinfo->disposition = DISPOSITIONTYPE_UNKNOWN;

	/* Call parser for mime type */
	switch (mimeinfo->type) {
		case MIMETYPE_MESSAGE:
			if (g_ascii_strcasecmp(mimeinfo->subtype, "rfc822") == 0) {
				procmime_parse_message_rfc822(mimeinfo);
			}
			break;
			
		case MIMETYPE_MULTIPART:
			procmime_parse_multipart(mimeinfo);
			break;
			
		default:
			break;
	}

	return 0;
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

static gboolean output_func(GNode *node, gpointer data)
{
	guint i, depth;
	MimeInfo *mimeinfo = (MimeInfo *) node->data;

	depth = g_node_depth(node);
	for (i = 0; i < depth; i++)
		printf("    ");
	printf("%s/%s (offset:%d length:%d encoding: %d)\n", typenames[mimeinfo->type], mimeinfo->subtype, mimeinfo->offset, mimeinfo->length, mimeinfo->encoding_type);

	return FALSE;
}

static void output_mime_structure(MimeInfo *mimeinfo, int indent)
{
	g_node_traverse(mimeinfo->node, G_PRE_ORDER, G_TRAVERSE_ALL, -1, output_func, NULL);
}

MimeInfo *procmime_scan_file_with_offset(const gchar *filename, int offset)
{
	MimeInfo *mimeinfo;
	struct stat buf;

	stat(filename, &buf);

	mimeinfo = procmime_mimeinfo_new();
	mimeinfo->content = MIMECONTENT_FILE;
	mimeinfo->encoding_type = ENC_UNKNOWN;
	mimeinfo->type = MIMETYPE_MESSAGE;
	mimeinfo->subtype = g_strdup("rfc822");
	mimeinfo->data.filename = g_strdup(filename);
	mimeinfo->offset = offset;
	mimeinfo->length = buf.st_size - offset;

	procmime_parse_message_rfc822(mimeinfo);
	if (debug_get_mode())
		output_mime_structure(mimeinfo, 0);

	return mimeinfo;
}

MimeInfo *procmime_scan_file(const gchar *filename)
{
	MimeInfo *mimeinfo;

	g_return_val_if_fail(filename != NULL, NULL);

	mimeinfo = procmime_scan_file_with_offset(filename, 0);

	return mimeinfo;
}

MimeInfo *procmime_scan_queue_file(const gchar *filename)
{
	FILE *fp;
	MimeInfo *mimeinfo;
	gchar buf[BUFFSIZE];
	gint offset = 0;

	g_return_val_if_fail(filename != NULL, NULL);

	/* Open file */
	if ((fp = g_fopen(filename, "rb")) == NULL)
		return NULL;
	/* Skip queue header */
	while (fgets(buf, sizeof(buf), fp) != NULL)
		if (buf[0] == '\r' || buf[0] == '\n') break;
	offset = ftell(fp);
	fclose(fp);

	mimeinfo = procmime_scan_file_with_offset(filename, offset);

	return mimeinfo;
}

typedef enum {
    ENC_AS_TOKEN,
    ENC_AS_QUOTED_STRING,
    ENC_AS_EXTENDED,
    ENC_TO_ASCII,
} EncodeAs;

typedef struct _ParametersData {
	FILE *fp;
	guint len;
	guint ascii_only;
} ParametersData;

static void write_parameters(gpointer key, gpointer value, gpointer user_data)
{
	gchar *param = key;
	gchar *val = value, *valpos, *tmp;
	ParametersData *pdata = (ParametersData *)user_data;
	GString *buf = g_string_new("");

	EncodeAs encas = ENC_AS_TOKEN;

	for (valpos = val; *valpos != 0; valpos++) {
		if (!IS_ASCII(*valpos) || *valpos == '"') {
			encas = ENC_AS_EXTENDED;
			break;
		}
	    
		/* CTLs */
		if (((*valpos >= 0) && (*valpos < 037)) || (*valpos == 0177)) {
			encas = ENC_AS_QUOTED_STRING;
			continue;
		}

		/* tspecials + SPACE */
		switch (*valpos) {
		case ' ':
		case '(': 
		case ')':
		case '<':
		case '>':
		case '@':
        	case ',':
		case ';':
		case ':':
		case '\\':
		case '\'':
        	case '/':
		case '[':
		case ']':
		case '?':
		case '=':
			encas = ENC_AS_QUOTED_STRING;
			continue;
		}
	}
	
	if (encas == ENC_AS_EXTENDED && pdata->ascii_only == TRUE) 
		encas = ENC_TO_ASCII;

	switch (encas) {
	case ENC_AS_TOKEN:
		g_string_append_printf(buf, "%s=%s", param, val);
		break;

	case ENC_TO_ASCII:
		tmp = g_strdup(val);
		g_strcanon(tmp, 
			" ()<>@,';:\\/[]?=.0123456789"
			"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			"abcdefghijklmnopqrstuvwxyz",
			'_');
		g_string_append_printf(buf, "%s=\"%s\"", param, tmp);
		g_free(tmp);
		break;

	case ENC_AS_QUOTED_STRING:
		g_string_append_printf(buf, "%s=\"%s\"", param, val);
		break;

	case ENC_AS_EXTENDED:
		if (!g_utf8_validate(val, -1, NULL))
			g_string_append_printf(buf, "%s*=%s''", param,
				conv_get_locale_charset_str());
		else
			g_string_append_printf(buf, "%s*=%s''", param,
				CS_INTERNAL);
		for (valpos = val; *valpos != '\0'; valpos++) {
			if (IS_ASCII(*valpos) && isalnum(*valpos)) {
				g_string_append_printf(buf, "%c", *valpos);
			} else {
				gchar hexstr[3] = "XX";
				get_hex_str(hexstr, *valpos);
				g_string_append_printf(buf, "%%%s", hexstr);
			}
		}
		break;		
	}
	
	if (buf->str && strlen(buf->str)) {
		if (pdata->len + strlen(buf->str) + 2 > 76) {
			fprintf(pdata->fp, ";\n %s", buf->str);
			pdata->len = strlen(buf->str) + 1;
		} else {
			fprintf(pdata->fp, "; %s", buf->str);
			pdata->len += strlen(buf->str) + 2;
		}
	}
	g_string_free(buf, TRUE);
}

void procmime_write_mime_header(MimeInfo *mimeinfo, FILE *fp)
{
	struct TypeTable *type_table;
	ParametersData *pdata = g_new0(ParametersData, 1);
	debug_print("procmime_write_mime_header\n");
	
	pdata->fp = fp;
	pdata->ascii_only = FALSE;

	for (type_table = mime_type_table; type_table->str != NULL; type_table++)
		if (mimeinfo->type == type_table->type) {
			gchar *buf = g_strdup_printf(
				"Content-Type: %s/%s", type_table->str, mimeinfo->subtype);
			fprintf(fp, "%s", buf);
			pdata->len = strlen(buf);
			pdata->ascii_only = TRUE;
			g_free(buf);
			break;
		}
	g_hash_table_foreach(mimeinfo->typeparameters, write_parameters, pdata);
	g_free(pdata);

	fprintf(fp, "\n");

	if (mimeinfo->encoding_type != ENC_UNKNOWN)
		fprintf(fp, "Content-Transfer-Encoding: %s\n", procmime_get_encoding_str(mimeinfo->encoding_type));

	if (mimeinfo->description != NULL)
		fprintf(fp, "Content-Description: %s\n", mimeinfo->description);

	if (mimeinfo->id != NULL)
		fprintf(fp, "Content-ID: %s\n", mimeinfo->id);

	if (mimeinfo->disposition != DISPOSITIONTYPE_UNKNOWN) {
		ParametersData *pdata = g_new0(ParametersData, 1);
		gchar *buf = NULL;
		if (mimeinfo->disposition == DISPOSITIONTYPE_INLINE)
			buf = g_strdup("Content-Disposition: inline");
		else if (mimeinfo->disposition == DISPOSITIONTYPE_ATTACHMENT)
			buf = g_strdup("Content-Disposition: attachment");
		else
			buf = g_strdup("Content-Disposition: unknown");

		fprintf(fp, "%s", buf);
		pdata->len = strlen(buf);
		g_free(buf);

		pdata->fp = fp;
		pdata->ascii_only = FALSE;

		g_hash_table_foreach(mimeinfo->dispositionparameters, write_parameters, pdata);
		g_free(pdata);
		fprintf(fp, "\n");
	}

	fprintf(fp, "\n");
}

gint procmime_write_message_rfc822(MimeInfo *mimeinfo, FILE *fp)
{
	FILE *infp;
	GNode *childnode;
	MimeInfo *child;
	gchar buf[BUFFSIZE];
	gboolean skip = FALSE;;

	debug_print("procmime_write_message_rfc822\n");

	/* write header */
	switch (mimeinfo->content) {
	case MIMECONTENT_FILE:
		if ((infp = g_fopen(mimeinfo->data.filename, "rb")) == NULL) {
			FILE_OP_ERROR(mimeinfo->data.filename, "fopen");
			return -1;
		}
		fseek(infp, mimeinfo->offset, SEEK_SET);
		while (fgets(buf, sizeof(buf), infp) == buf) {
			if (buf[0] == '\n' && buf[1] == '\0')
				break;
			if (skip && (buf[0] == ' ' || buf[0] == '\t'))
				continue;
			if (g_ascii_strncasecmp(buf, "Mime-Version:", 13) == 0 ||
			    g_ascii_strncasecmp(buf, "Content-Type:", 13) == 0 ||
			    g_ascii_strncasecmp(buf, "Content-Transfer-Encoding:", 26) == 0 ||
			    g_ascii_strncasecmp(buf, "Content-Description:", 20) == 0 ||
			    g_ascii_strncasecmp(buf, "Content-ID:", 11) == 0 ||
			    g_ascii_strncasecmp(buf, "Content-Disposition:", 20) == 0) {
				skip = TRUE;
				continue;
			}
			fwrite(buf, sizeof(gchar), strlen(buf), fp);
			skip = FALSE;
		}
		fclose(infp);
		break;

	case MIMECONTENT_MEM:
		fwrite(mimeinfo->data.mem, 
				sizeof(gchar), 
				strlen(mimeinfo->data.mem), 
				fp);
		break;

	default:
		break;
	}

	childnode = mimeinfo->node->children;
	if (childnode == NULL)
		return -1;

	child = (MimeInfo *) childnode->data;
	fprintf(fp, "Mime-Version: 1.0\n");
	procmime_write_mime_header(child, fp);
	return procmime_write_mimeinfo(child, fp);
}

gint procmime_write_multipart(MimeInfo *mimeinfo, FILE *fp)
{
	FILE *infp;
	GNode *childnode;
	gchar *boundary, *str, *str2;
	gchar buf[BUFFSIZE];
	gboolean firstboundary;

	debug_print("procmime_write_multipart\n");

	boundary = g_hash_table_lookup(mimeinfo->typeparameters, "boundary");

	switch (mimeinfo->content) {
	case MIMECONTENT_FILE:
		if ((infp = g_fopen(mimeinfo->data.filename, "rb")) == NULL) {
			FILE_OP_ERROR(mimeinfo->data.filename, "fopen");
			return -1;
		}
		fseek(infp, mimeinfo->offset, SEEK_SET);
		while (fgets(buf, sizeof(buf), infp) == buf) {
			if (IS_BOUNDARY(buf, boundary, strlen(boundary)))
				break;
			fwrite(buf, sizeof(gchar), strlen(buf), fp);
		}
		fclose(infp);
		break;

	case MIMECONTENT_MEM:
		str = g_strdup(mimeinfo->data.mem);
		if (((str2 = strstr(str, boundary)) != NULL) && ((str2 - str) >= 2) &&
		    (*(str2 - 1) == '-') && (*(str2 - 2) == '-'))
			*(str2 - 2) = '\0';
		fwrite(str, sizeof(gchar), strlen(str), fp);
		g_free(str);
		break;

	default:
		break;
	}

	childnode = mimeinfo->node->children;
	firstboundary = TRUE;
	while (childnode != NULL) {
		MimeInfo *child = childnode->data;

		if (firstboundary)
			firstboundary = FALSE;
		else
			fprintf(fp, "\n");
		fprintf(fp, "--%s\n", boundary);

		procmime_write_mime_header(child, fp);
		if (procmime_write_mimeinfo(child, fp) < 0)
			return -1;

		childnode = g_node_next_sibling(childnode);
	}	
	fprintf(fp, "\n--%s--\n", boundary);

	return 0;
}

gint procmime_write_mimeinfo(MimeInfo *mimeinfo, FILE *fp)
{
	FILE *infp;

	debug_print("procmime_write_mimeinfo\n");

	if (G_NODE_IS_LEAF(mimeinfo->node)) {
		switch (mimeinfo->content) {
		case MIMECONTENT_FILE:
			if ((infp = g_fopen(mimeinfo->data.filename, "rb")) == NULL) {
				FILE_OP_ERROR(mimeinfo->data.filename, "fopen");
				return -1;
			}
			copy_file_part_to_fp(infp, mimeinfo->offset, mimeinfo->length, fp);
			fclose(infp);
			return 0;

		case MIMECONTENT_MEM:
			fwrite(mimeinfo->data.mem, 
					sizeof(gchar), 
					strlen(mimeinfo->data.mem), 
					fp);
			return 0;

		default:
			return 0;
		}
	} else {
		/* Call writer for mime type */
		switch (mimeinfo->type) {
		case MIMETYPE_MESSAGE:
			if (g_ascii_strcasecmp(mimeinfo->subtype, "rfc822") == 0) {
				return procmime_write_message_rfc822(mimeinfo, fp);
			}
			break;
			
		case MIMETYPE_MULTIPART:
			return procmime_write_multipart(mimeinfo, fp);
			
		default:
			break;
		}

		return -1;
	}

	return 0;
}

gchar *procmime_get_part_file_name(MimeInfo *mimeinfo)
{
	gchar *base;

	if ((mimeinfo->type == MIMETYPE_TEXT) && !g_ascii_strcasecmp(mimeinfo->subtype, "html"))
		base = g_strdup("mimetmp.html");
	else {
		const gchar *basetmp;
		gchar *basename;

		basetmp = procmime_mimeinfo_get_parameter(mimeinfo, "filename");
		if (basetmp == NULL)
			basetmp = procmime_mimeinfo_get_parameter(mimeinfo, "name");
		if (basetmp == NULL)
			basetmp = "mimetmp";
		basename = g_path_get_basename(basetmp);
		if (*basename == '\0') {
			g_free(basename);
			basename = g_strdup("mimetmp");
		}
		base = conv_filename_from_utf8(basename);
		g_free(basename);
		subst_for_shellsafe_filename(base);
	}
	
	return base;
}

