/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2024 the Claws Mail team and Hiroyuki Yamamoto
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
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#include "claws-features.h"
#endif

#include <stdio.h>

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <string.h>
#if HAVE_LOCALE_H
#  include <locale.h>
#endif
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include "procmime.h"
#include "procheader.h"
#include "quoted-printable.h"
#include "uuencode.h"
#include "unmime.h"
#include "html.h"
#include "enriched.h"
#include "codeconv.h"
#include "utils.h"
#include "prefs_common.h"
#include "prefs_gtk.h"
#include "alertpanel.h"
#include "timing.h"
#include "privacy.h"
#include "account.h"
#include "file-utils.h"

#ifdef G_OS_WIN32
#include "w32_reg.h"
#define REG_MIME_TYPE_VALUE "Content Type"
#endif

#ifndef G_OS_WIN32
static GHashTable *procmime_get_mime_type_table	(void);
#endif
static MimeInfo *procmime_scan_file_short(const gchar *filename);
static MimeInfo *procmime_scan_queue_file_short(const gchar *filename);
static MimeInfo *procmime_scan_queue_file_full(const gchar *filename, gboolean short_scan);

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
			claws_unlink(mimeinfo->data.filename);
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
	g_free(mimeinfo->location);

	g_hash_table_foreach_remove(mimeinfo->typeparameters,
		procmime_mimeinfo_parameters_destroy, NULL);
	g_hash_table_destroy(mimeinfo->typeparameters);
	g_hash_table_foreach_remove(mimeinfo->dispositionparameters,
		procmime_mimeinfo_parameters_destroy, NULL);
	g_hash_table_destroy(mimeinfo->dispositionparameters);

	if (mimeinfo->privacy)
		privacy_free_privacydata(mimeinfo->privacy);

	if (mimeinfo->sig_data)
		privacy_free_signature_data(mimeinfo->sig_data);

	g_free(mimeinfo);

	return FALSE;
}

void procmime_mimeinfo_free_all(MimeInfo **mimeinfo_ptr)
{
	MimeInfo *mimeinfo = *mimeinfo_ptr;
	GNode *node;

	if (!mimeinfo)
		return;

	node = mimeinfo->node;
	g_node_traverse(node, G_IN_ORDER, G_TRAVERSE_ALL, -1, free_func, NULL);

	g_node_destroy(node);

	*mimeinfo_ptr = NULL;
}

MimeInfo *procmime_mimeinfo_parent(MimeInfo *mimeinfo)
{
	cm_return_val_if_fail(mimeinfo != NULL, NULL);
	cm_return_val_if_fail(mimeinfo->node != NULL, NULL);

	if (mimeinfo->node->parent == NULL)
		return NULL;
	return (MimeInfo *) mimeinfo->node->parent->data;
}

MimeInfo *procmime_mimeinfo_next(MimeInfo *mimeinfo)
{
	cm_return_val_if_fail(mimeinfo != NULL, NULL);
	cm_return_val_if_fail(mimeinfo->node != NULL, NULL);

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
		return NULL;
	}

	if (!folder_has_parent_of_type(msginfo->folder, F_QUEUE) &&
	    !folder_has_parent_of_type(msginfo->folder, F_DRAFT))
		mimeinfo = procmime_scan_file(filename);
	else
		mimeinfo = procmime_scan_queue_file(filename);
	g_free(filename);

	return mimeinfo;
}

MimeInfo *procmime_scan_message_short(MsgInfo *msginfo)
{
	gchar *filename;
	MimeInfo *mimeinfo;

	filename = procmsg_get_message_file_path(msginfo);
	if (!filename || !is_file_exist(filename)) {
		g_free(filename);
		return NULL;
	}

	if (!folder_has_parent_of_type(msginfo->folder, F_QUEUE) &&
	    !folder_has_parent_of_type(msginfo->folder, F_DRAFT))
		mimeinfo = procmime_scan_file_short(filename);
	else
		mimeinfo = procmime_scan_queue_file_short(filename);
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

	cm_return_val_if_fail(mimeinfo != NULL, NULL);
	cm_return_val_if_fail(name != NULL, NULL);

	value = g_hash_table_lookup(mimeinfo->dispositionparameters, name);
	if (value == NULL)
		value = g_hash_table_lookup(mimeinfo->typeparameters, name);
	
	return value;
}

#define FLUSH_LASTLINE() {							\
	if (*lastline != '\0') {						\
		gint llen = 0;							\
		strretchomp(lastline);						\
		llen = strlen(lastline);					\
		if (lastline[llen-1] == ' ' && !account_sigsep_matchlist_str_found(lastline, "%s") &&	\
		    !(llen >= 2 && lastline[1] == ' ' && strchr(prefs_common.quote_chars, lastline[0]))) {					\
			/* this is flowed */					\
			if (delsp)						\
				lastline[llen-1] = '\0';			\
			if (claws_fputs(lastline, outfp) == EOF)			\
				err = TRUE;					\
		} else {							\
			if (claws_fputs(lastline, outfp) == EOF)			\
				err = TRUE;					\
			if (claws_fputs("\n", outfp) == EOF)				\
				err = TRUE;					\
		}								\
	} 									\
	strcpy(lastline, buf);							\
}

gboolean procmime_decode_content(MimeInfo *mimeinfo)
{
	gchar buf[BUFFSIZE];
	glong readend;
	gchar *tmpfilename;
	FILE *outfp, *infp;
	GStatBuf statbuf;
	gboolean tmp_file = FALSE;
	gboolean flowed = FALSE;
	gboolean delsp = FALSE; 
	gboolean err = FALSE;
	gint state = 0;
	guint save = 0;

	cm_return_val_if_fail(mimeinfo != NULL, FALSE);

	EncodingType encoding = forced_encoding 
				? forced_encoding
				: mimeinfo->encoding_type;
	gchar lastline[BUFFSIZE];
	memset(lastline, 0, BUFFSIZE);

	if (prefs_common.respect_flowed_format &&
	    mimeinfo->type == MIMETYPE_TEXT && 
	    !strcasecmp(mimeinfo->subtype, "plain")) {
		if (procmime_mimeinfo_get_parameter(mimeinfo, "format") != NULL &&
		    !strcasecmp(procmime_mimeinfo_get_parameter(mimeinfo, "format"),"flowed"))
			flowed = TRUE;
		if (flowed &&
		    procmime_mimeinfo_get_parameter(mimeinfo, "delsp") != NULL &&
		    !strcasecmp(procmime_mimeinfo_get_parameter(mimeinfo, "delsp"),"yes"))
			delsp = TRUE;
	}
	
	if (!flowed && (
	     encoding == ENC_UNKNOWN ||
	     encoding == ENC_BINARY ||
	     encoding == ENC_7BIT ||
	     encoding == ENC_8BIT
	    ))
		return TRUE;

	if (mimeinfo->type == MIMETYPE_MULTIPART || mimeinfo->type == MIMETYPE_MESSAGE)
		return TRUE;

	if (mimeinfo->data.filename == NULL)
		return FALSE;

	infp = claws_fopen(mimeinfo->data.filename, "rb");
	if (!infp) {
		FILE_OP_ERROR(mimeinfo->data.filename, "claws_fopen");
		return FALSE;
	}
	if (fseek(infp, mimeinfo->offset, SEEK_SET) < 0) {
		FILE_OP_ERROR(mimeinfo->data.filename, "fseek");
		claws_fclose(infp);
		return FALSE;
	}

	outfp = get_tmpfile_in_dir(get_mime_tmp_dir(), &tmpfilename);
	if (!outfp) {
		perror("tmpfile");
		claws_fclose(infp);
		g_free(tmpfilename);
		return FALSE;
	}

	tmp_file = TRUE;
	readend = mimeinfo->offset + mimeinfo->length;

	account_sigsep_matchlist_create(); /* FLUSH_LASTLINE will use it */

	*buf = '\0';
	if (encoding == ENC_QUOTED_PRINTABLE) {
		while ((ftell(infp) < readend) && (claws_fgets(buf, sizeof(buf), infp) != NULL)) {
			size_t len;
			len = qp_decode_line(buf);
			buf[len] = '\0';
			if (!flowed) {
				if (claws_fwrite(buf, 1, len, outfp) < len)
					err = TRUE;
			} else {
				FLUSH_LASTLINE();
			}
		}
		if (flowed)
			FLUSH_LASTLINE();
	} else if (encoding == ENC_BASE64) {
		gchar outbuf[BUFFSIZE + 1];
		glong len;
		gsize inlen, inread;
		gboolean got_error = FALSE;
		gboolean uncanonicalize = FALSE;
		FILE *tmpfp = NULL;
		gboolean null_bytes = FALSE;
		gboolean starting = TRUE;

		if (mimeinfo->type == MIMETYPE_TEXT ||
		    mimeinfo->type == MIMETYPE_MESSAGE) {
			uncanonicalize = TRUE;
			tmpfp = my_tmpfile();
			if (!tmpfp) {
				perror("my_tmpfile");
				if (tmp_file) 
					claws_fclose(outfp);
				claws_fclose(infp);
				g_free(tmpfilename);
				return FALSE;
			}
		} else
			tmpfp = outfp;

		while ((inlen = MIN(readend - ftell(infp), sizeof(buf))) > 0 && !err) {
			inread = claws_fread(buf, 1, inlen, infp);
			memset(outbuf, 0, sizeof(buf));
			len = (glong)g_base64_decode_step(buf, inlen, outbuf, &state, &save);
			if (uncanonicalize == TRUE && strlen(outbuf) < (size_t)len && starting) {
				uncanonicalize = FALSE;
				null_bytes = TRUE;
			}
			starting = FALSE;
			if (((inread != inlen) || len < 0) && !got_error) {
				g_warning("bad BASE64 content");
				if (claws_fwrite(_("[Error decoding BASE64]\n"),
					sizeof(gchar),
					strlen(_("[Error decoding BASE64]\n")),
					tmpfp) < strlen(_("[Error decoding BASE64]\n")))
					g_warning("error decoding BASE64");
				got_error = TRUE;
				continue;
			} else if (len >= 0) {
				/* print out the error message only once 
				 * per block */
				if (null_bytes) {
					/* we won't uncanonicalize, output to outfp directly */
					if (claws_fwrite(outbuf, sizeof(gchar), (size_t)len, outfp) < (size_t)len)
						err = TRUE;
				} else {
					if (claws_fwrite(outbuf, sizeof(gchar), (size_t)len, tmpfp) < (size_t)len)
						err = TRUE;
				}
				got_error = FALSE;
			}
		}

		if (uncanonicalize) {
			rewind(tmpfp);
			while (claws_fgets(buf, sizeof(buf), tmpfp) != NULL) {
				strcrchomp(buf);
				if (claws_fputs(buf, outfp) == EOF)
					err = TRUE;
			}
		}
		if (tmpfp != outfp) {
			claws_fclose(tmpfp);
		}
	} else if (encoding == ENC_X_UUENCODE) {
		gchar outbuf[BUFFSIZE];
		glong len;
		gboolean flag = FALSE;

		while ((ftell(infp) < readend) && (claws_fgets(buf, sizeof(buf), infp) != NULL)) {
			if (!flag && strncmp(buf,"begin ", 6)) continue;

			if (flag) {
				len = fromuutobits(outbuf, buf);
				if (len <= 0) {
					if (len < 0) 
						g_warning("bad UUENCODE content (%ld)", len);
					break;
				}
				if (claws_fwrite(outbuf, sizeof(gchar), (size_t)len, outfp) < (size_t)len)
					err = TRUE;
			} else
				flag = TRUE;
		}
	} else {
		while ((ftell(infp) < readend) && (claws_fgets(buf, sizeof(buf), infp) != NULL)) {
			if (!flowed) {
				if (claws_fputs(buf, outfp) == EOF)
					err = TRUE;
			} else {
				FLUSH_LASTLINE();
			}
		}
		if (flowed)
			FLUSH_LASTLINE();
		if (err == TRUE)
			g_warning("write error");
	}

	claws_fclose(outfp);
	claws_fclose(infp);

	account_sigsep_matchlist_delete();

	if (err == TRUE) {
		g_free(tmpfilename);
		return FALSE;
	}

	if (g_stat(tmpfilename, &statbuf) < 0) {
		FILE_OP_ERROR(tmpfilename, "stat");
		g_free(tmpfilename);
		return FALSE;
	}

	if (mimeinfo->tmp)
		claws_unlink(mimeinfo->data.filename);
	g_free(mimeinfo->data.filename);
	mimeinfo->data.filename = tmpfilename;
	mimeinfo->tmp = TRUE;
	mimeinfo->offset = 0;
	mimeinfo->length = statbuf.st_size;
	mimeinfo->encoding_type = ENC_BINARY;

	return TRUE;
}

gboolean procmime_encode_content(MimeInfo *mimeinfo, EncodingType encoding)
{
	FILE *infp = NULL, *outfp;
	glong len;
	gchar *tmpfilename;
	GStatBuf statbuf;
	gboolean err = FALSE;

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
		g_free(tmpfilename);
		return FALSE;
	}

	if (mimeinfo->content == MIMECONTENT_FILE && mimeinfo->data.filename) {
		if ((infp = claws_fopen(mimeinfo->data.filename, "rb")) == NULL) {
			g_warning("can't open file %s", mimeinfo->data.filename);
			g_free(tmpfilename);
			claws_fclose(outfp);
			return FALSE;
		}
	} else if (mimeinfo->content == MIMECONTENT_MEM) {
		infp = str_open_as_stream(mimeinfo->data.mem);
		if (infp == NULL) {
			g_free(tmpfilename);
			claws_fclose(outfp);
			return FALSE;
		}
	} else {
		g_free(tmpfilename);
		claws_fclose(outfp);
		g_warning("unknown mimeinfo");
		return FALSE;
	}

	if (encoding == ENC_BASE64) {
		gchar inbuf[B64_LINE_SIZE], *out;
		FILE *tmp_fp = infp;
		gchar *tmp_file = NULL;

		if (mimeinfo->type == MIMETYPE_TEXT ||
		     mimeinfo->type == MIMETYPE_MESSAGE) {
		     	if (mimeinfo->content == MIMECONTENT_FILE) {
				tmp_file = get_tmp_file();
				if (canonicalize_file(mimeinfo->data.filename, tmp_file) < 0) {
					g_free(tmp_file);
					g_free(tmpfilename);
					claws_fclose(infp);
					claws_fclose(outfp);
					return FALSE;
				}
				if ((tmp_fp = claws_fopen(tmp_file, "rb")) == NULL) {
					FILE_OP_ERROR(tmp_file, "claws_fopen");
					claws_unlink(tmp_file);
					g_free(tmp_file);
					g_free(tmpfilename);
					claws_fclose(infp);
					claws_fclose(outfp);
					return FALSE;
				}
			} else {
				gchar *out = canonicalize_str(mimeinfo->data.mem);
				claws_fclose(infp);
				infp = str_open_as_stream(out);
				tmp_fp = infp;
				g_free(out);
				if (infp == NULL) {
					g_free(tmpfilename);
					claws_fclose(outfp);
					return FALSE;
				}
			}
		}

		while ((len = (glong)claws_fread(inbuf, sizeof(gchar),
				    B64_LINE_SIZE, tmp_fp))
		       == B64_LINE_SIZE) {
			out = g_base64_encode(inbuf, B64_LINE_SIZE);
			if (claws_fputs(out, outfp) == EOF)
				err = TRUE;
			g_free(out);
			if (claws_fputc('\n', outfp) == EOF)
				err = TRUE;
		}
		if (len > 0 && claws_feof(tmp_fp)) {
			out = g_base64_encode(inbuf, (gsize)len);
			if (claws_fputs(out, outfp) == EOF)
				err = TRUE;
			g_free(out);
			if (claws_fputc('\n', outfp) == EOF)
				err = TRUE;
		}

		if (tmp_file) {
			claws_fclose(tmp_fp);
			claws_unlink(tmp_file);
			g_free(tmp_file);
		}
	} else if (encoding == ENC_QUOTED_PRINTABLE) {
		gchar inbuf[BUFFSIZE], outbuf[BUFFSIZE * 4];

		while (claws_fgets(inbuf, sizeof(inbuf), infp) != NULL) {
			qp_encode_line(outbuf, inbuf);

			if (!strncmp("From ", outbuf, sizeof("From ")-1)) {
				gchar *tmpbuf = outbuf;
				
				tmpbuf += sizeof("From ")-1;
				
				if (claws_fputs("=46rom ", outfp) == EOF)
					err = TRUE;
				if (claws_fputs(tmpbuf, outfp) == EOF)
					err = TRUE;
			} else {
				if (claws_fputs(outbuf, outfp) == EOF)
					err = TRUE;
			}
		}
	} else {
		gchar buf[BUFFSIZE];

		while (claws_fgets(buf, sizeof(buf), infp) != NULL) {
			strcrchomp(buf);
			if (claws_fputs(buf, outfp) == EOF)
				err = TRUE;
		}
	}

	claws_fclose(outfp);
	claws_fclose(infp);

	if (err == TRUE) {
		g_free(tmpfilename);
		return FALSE;
	}
    
	if (mimeinfo->content == MIMECONTENT_FILE) {
		if (mimeinfo->tmp && (mimeinfo->data.filename != NULL))
			claws_unlink(mimeinfo->data.filename);
		g_free(mimeinfo->data.filename);
	} else if (mimeinfo->content == MIMECONTENT_MEM) {
		if (mimeinfo->tmp && (mimeinfo->data.mem != NULL))
			g_free(mimeinfo->data.mem);
	}

	if (g_stat(tmpfilename, &statbuf) < 0) {
		FILE_OP_ERROR(tmpfilename, "stat");
		g_free(tmpfilename);
		return FALSE;
	}
	mimeinfo->content = MIMECONTENT_FILE;
	mimeinfo->data.filename = tmpfilename;
	mimeinfo->tmp = TRUE;
	mimeinfo->offset = 0;
	mimeinfo->length = statbuf.st_size;
	mimeinfo->encoding_type = encoding;

	return TRUE;
}

static gint procmime_get_part_to_stream(FILE *outfp, MimeInfo *mimeinfo)
{
	FILE *infp;
	gchar buf[BUFFSIZE];
	glong restlength, readlength;
	gint saved_errno = 0;

	cm_return_val_if_fail(outfp != NULL, -1);
	cm_return_val_if_fail(mimeinfo != NULL, -1);

	if (mimeinfo->encoding_type != ENC_BINARY && !procmime_decode_content(mimeinfo))
		return -EINVAL;

	if ((infp = claws_fopen(mimeinfo->data.filename, "rb")) == NULL) {
		saved_errno = errno;
		FILE_OP_ERROR(mimeinfo->data.filename, "claws_fopen");
		return -(saved_errno);
	}
	if (fseek(infp, mimeinfo->offset, SEEK_SET) < 0) {
		saved_errno = errno;
		FILE_OP_ERROR(mimeinfo->data.filename, "fseek");
		claws_fclose(infp);
		return -(saved_errno);
	}

	restlength = mimeinfo->length;

	while ((restlength > 0) && ((readlength = claws_fread(buf, 1, restlength > BUFFSIZE ? BUFFSIZE : restlength, infp)) > 0)) {
		if (claws_fwrite(buf, 1, (size_t)readlength, outfp) != (size_t)readlength) {
			saved_errno = errno;
			claws_fclose(infp);
			return -(saved_errno);
		}
		restlength -= readlength;
	}

	claws_fclose(infp);
	rewind(outfp);

	return 0;
}

gint procmime_get_part(const gchar *outfile, MimeInfo *mimeinfo)
{
	FILE *outfp;
	gint result;
	gint saved_errno = 0;

	cm_return_val_if_fail(outfile != NULL, -1);

	if ((outfp = claws_fopen(outfile, "wb")) == NULL) {
		saved_errno = errno;
		FILE_OP_ERROR(outfile, "claws_fopen");
		return -(saved_errno);
	}

	if (change_file_mode_rw(outfp, outfile) < 0) {
		FILE_OP_ERROR(outfile, "chmod");
		g_warning("can't change file mode: %s", outfile);
	}

	result = procmime_get_part_to_stream(outfp, mimeinfo);

	if (claws_fclose(outfp) == EOF) {
		saved_errno = errno;
		FILE_OP_ERROR(outfile, "claws_fclose");
		if (claws_unlink(outfile) < 0)
                        FILE_OP_ERROR(outfile, "claws_unlink");
		return -(saved_errno);
	}

	return result;
}

gboolean procmime_scan_text_content(MimeInfo *mimeinfo,
		gboolean (*scan_callback)(const gchar *str, gpointer cb_data),
		gpointer cb_data) 
{
	FILE *tmpfp;
	const gchar *src_codeset;
	gboolean conv_fail = FALSE;
	gchar buf[BUFFSIZE];
	gchar *str;
	gboolean scan_ret = FALSE;
	int r;

	cm_return_val_if_fail(mimeinfo != NULL, TRUE);
	cm_return_val_if_fail(scan_callback != NULL, TRUE);

	if (!procmime_decode_content(mimeinfo))
		return TRUE;

	tmpfp = my_tmpfile();

	if (tmpfp == NULL) {
		FILE_OP_ERROR("tmpfile", "open");
		return TRUE;
	}

	if ((r = procmime_get_part_to_stream(tmpfp, mimeinfo)) < 0) {
		g_warning("procmime_get_part_to_stream error %d", r);
		return TRUE;
	}

	src_codeset = forced_charset
		      ? forced_charset : 
		      procmime_mimeinfo_get_parameter(mimeinfo, "charset");

	/* use supersets transparently when possible */
	if (!forced_charset && src_codeset && !strcasecmp(src_codeset, CS_ISO_8859_1))
		src_codeset = CS_WINDOWS_1252;
	else if (!forced_charset && src_codeset && !strcasecmp(src_codeset, CS_X_GBK))
		src_codeset = CS_GB18030;
	else if (!forced_charset && src_codeset && !strcasecmp(src_codeset, CS_GBK))
		src_codeset = CS_GB18030;
	else if (!forced_charset && src_codeset && !strcasecmp(src_codeset, CS_GB2312))
		src_codeset = CS_GB18030;
	else if (!forced_charset && src_codeset && !strcasecmp(src_codeset, CS_X_VIET_VPS))
		src_codeset = CS_WINDOWS_874;

	if (mimeinfo->type == MIMETYPE_TEXT && !g_ascii_strcasecmp(mimeinfo->subtype, "html")) {
		SC_HTMLParser *parser;
		CodeConverter *conv;

		conv = conv_code_converter_new(src_codeset);
		parser = sc_html_parser_new(tmpfp, conv);
		while ((str = sc_html_parse(parser)) != NULL) {
			if ((scan_ret = scan_callback(str, cb_data)) == TRUE)
				break;
		}
		sc_html_parser_destroy(parser);
		conv_code_converter_destroy(conv);
	} else if (mimeinfo->type == MIMETYPE_TEXT && !g_ascii_strcasecmp(mimeinfo->subtype, "enriched")) {
		ERTFParser *parser;
		CodeConverter *conv;

		conv = conv_code_converter_new(src_codeset);
		parser = ertf_parser_new(tmpfp, conv);
		while ((str = ertf_parse(parser)) != NULL) {
			if ((scan_ret = scan_callback(str, cb_data)) == TRUE)
				break;
		}
		ertf_parser_destroy(parser);
		conv_code_converter_destroy(conv);
	} else if (mimeinfo->type == MIMETYPE_TEXT && mimeinfo->disposition != DISPOSITIONTYPE_ATTACHMENT) {
		while (claws_fgets(buf, sizeof(buf), tmpfp) != NULL) {
			str = conv_codeset_strdup(buf, src_codeset, CS_UTF_8);
			if (str) {
				if ((scan_ret = scan_callback(str, cb_data)) == TRUE) {
					g_free(str);
					break;
				}
				g_free(str);
			} else {
				conv_fail = TRUE;
				if ((scan_ret = scan_callback(buf, cb_data)) == TRUE)
					break;
			}
		}
	}

	if (conv_fail)
		g_warning("procmime_get_text_content(): code conversion failed");

	claws_fclose(tmpfp);

	return scan_ret;
}

static gboolean scan_fputs_cb(const gchar *str, gpointer fp)
{
	if (claws_fputs(str, (FILE *)fp) == EOF)
		return TRUE;
	
	return FALSE;
}

FILE *procmime_get_text_content(MimeInfo *mimeinfo)
{
	FILE *outfp;
	gboolean err;

	if ((outfp = my_tmpfile()) == NULL) {
		perror("my_tmpfile");
		return NULL;
	}

	err = procmime_scan_text_content(mimeinfo, scan_fputs_cb, outfp);

	rewind(outfp);
	if (err == TRUE) {
		claws_fclose(outfp);
		return NULL;
	}
	return outfp;

}

FILE *procmime_get_binary_content(MimeInfo *mimeinfo)
{
	FILE *outfp;

	cm_return_val_if_fail(mimeinfo != NULL, NULL);

	if (!procmime_decode_content(mimeinfo))
		return NULL;

	outfp = my_tmpfile();

	if (procmime_get_part_to_stream(outfp, mimeinfo) < 0) {
		return NULL;
	}

	return outfp;
}

/* search the first text part of (multipart) MIME message,
   decode, convert it and output to outfp. */
FILE *procmime_get_first_text_content(MsgInfo *msginfo)
{
	FILE *outfp = NULL;
	MimeInfo *mimeinfo, *partinfo;
	gboolean empty_ok = FALSE, short_scan = TRUE;

    	cm_return_val_if_fail(msginfo != NULL, NULL);

	/* first we try to short-scan (for speed), refusing empty parts */
scan_again:
	if (short_scan)
		mimeinfo = procmime_scan_message_short(msginfo);
	else
		mimeinfo = procmime_scan_message(msginfo);
	if (!mimeinfo) return NULL;

	partinfo = mimeinfo;
	while (partinfo && (partinfo->type != MIMETYPE_TEXT ||
	       (partinfo->length == 0 && !empty_ok))) {
		partinfo = procmime_mimeinfo_next(partinfo);
	}
	if (partinfo)
		outfp = procmime_get_text_content(partinfo);
	else if (!empty_ok && short_scan) {
		/* if short scan didn't find a non-empty part, rescan
		 * fully for non-empty parts
		 */
		short_scan = FALSE;
		procmime_mimeinfo_free_all(&mimeinfo);
		goto scan_again;
	} else if (!empty_ok && !short_scan) {
		/* if full scan didn't find a non-empty part, rescan
		 * accepting empty parts 
		 */
		empty_ok = TRUE;
		procmime_mimeinfo_free_all(&mimeinfo);
		goto scan_again;
	}
	procmime_mimeinfo_free_all(&mimeinfo);

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

	cm_return_val_if_fail(msginfo != NULL, NULL);

	mimeinfo = procmime_scan_message(msginfo);
	if (!mimeinfo) {
		return NULL;
	}

	partinfo = mimeinfo;
	if ((encinfo = find_encrypted_part(partinfo)) != NULL) {
		debug_print("decrypting message part\n");
		if (privacy_mimeinfo_decrypt(encinfo) < 0) {
			alertpanel_error(_("Couldn't decrypt: %s"),
				privacy_get_error());
			return NULL;
		}
	}
	partinfo = mimeinfo;
	while (partinfo && partinfo->type != MIMETYPE_TEXT) {
		partinfo = procmime_mimeinfo_next(partinfo);
		if (privacy_mimeinfo_is_signed(partinfo))
			procmsg_msginfo_set_flags(msginfo, 0, MSG_SIGNED);
	}

	if (partinfo)
		outfp = procmime_get_text_content(partinfo);

	procmime_mimeinfo_free_all(&mimeinfo);

	return outfp;
}

gboolean procmime_msginfo_is_encrypted(MsgInfo *msginfo)
{
	MimeInfo *mimeinfo, *partinfo;
	gboolean result = FALSE;

	cm_return_val_if_fail(msginfo != NULL, FALSE);

	mimeinfo = procmime_scan_message(msginfo);
	if (!mimeinfo) {
		return FALSE;
	}

	partinfo = mimeinfo;
	result = (find_encrypted_part(partinfo) != NULL);
	procmime_mimeinfo_free_all(&mimeinfo);

	return result;
}

gchar *procmime_get_tmp_file_name(MimeInfo *mimeinfo)
{
	static guint32 id = 0;
	gchar *base;
	gchar *filename;
	gchar f_prefix[10];

	cm_return_val_if_fail(mimeinfo != NULL, NULL);

	g_snprintf(f_prefix, sizeof(f_prefix), "%08x.", id++);

	if ((mimeinfo->type == MIMETYPE_TEXT) && !g_ascii_strcasecmp(mimeinfo->subtype, "html"))
		base = g_strdup("mimetmp.html");
	else {
		const gchar *basetmp1;
		gchar *basetmp2;

		basetmp1 = procmime_mimeinfo_get_parameter(mimeinfo, "filename");
		if (basetmp1 == NULL)
			basetmp1 = procmime_mimeinfo_get_parameter(mimeinfo, "name");
		if (basetmp1 == NULL)
			basetmp1 = "mimetmp";
		basetmp2 = g_path_get_basename(basetmp1);
		if (*basetmp2 == '\0') {
			g_free(basetmp2);
			basetmp2 = g_strdup("mimetmp");
		}
		base = conv_filename_from_utf8(basetmp2);
		g_free(basetmp2);
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
	const gchar *p;
	gchar *ext = NULL;
	gchar *base;
	gchar *str;
#ifndef G_OS_WIN32
	static GHashTable *mime_type_table = NULL;
	MimeType *mime_type;

	if (!mime_type_table) {
		mime_type_table = procmime_get_mime_type_table();
		if (!mime_type_table) return NULL;
	}
#endif

	if (filename == NULL)
		return NULL;

	base = g_path_get_basename(filename);
	if ((p = strrchr(base, '.')) != NULL)
#ifndef G_OS_WIN32
		ext = g_utf8_strdown(p + 1, -1);
#else
		ext = g_utf8_strdown(p, -1);
#endif
	else
		ext = g_utf8_strdown(base, -1);
	g_free(base);

#ifndef G_OS_WIN32
	mime_type = g_hash_table_lookup(mime_type_table, ext);
	
	if (mime_type) {
		str = g_strconcat(mime_type->type, "/", mime_type->sub_type,
				  NULL);
		debug_print("got type %s for %s\n", str, ext);
		g_free(ext);
		return str;
	}
	g_free(ext);
	return NULL;
#else
	str = read_w32_registry_string(HKEY_CLASSES_ROOT, ext, REG_MIME_TYPE_VALUE);
	debug_print("got type %s for %s\n", str, ext);
	g_free(ext);
	return str;
#endif
}

#ifndef G_OS_WIN32
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
			/* Don't overwrite previously inserted extension */
			if (!g_hash_table_lookup(table, exts[i])) {
				key = g_strdup(exts[i]);
				g_hash_table_insert(table, key, mime_type);
			}
		}
		g_strfreev(exts);
	}

	return table;
}
#endif

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
	
#if defined(__NetBSD__) || defined(__OpenBSD__) || defined(__FreeBSD__)
	if ((fp = claws_fopen(DATAROOTDIR "/mime/globs", "rb")) == NULL) 
#else
	if ((fp = claws_fopen("/usr/share/mime/globs", "rb")) == NULL) 
#endif
	{
		fp_is_glob_file = FALSE;
		if ((fp = claws_fopen("/etc/mime.types", "rb")) == NULL) {
			if ((fp = claws_fopen(SYSCONFDIR "/mime.types", "rb")) 
				== NULL) {
				FILE_OP_ERROR(SYSCONFDIR "/mime.types", 
					"claws_fopen");
				return NULL;
			}
		}
	}

	while (claws_fgets(buf, sizeof(buf), fp) != NULL) {
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
			mime_type->extension = g_utf8_strdown(p, -1);
		else
			mime_type->extension = NULL;

		list = g_list_append(list, mime_type);
	}

	claws_fclose(fp);

	if (!list)
		g_warning("can't read mime.types");

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
		 !g_ascii_strcasecmp(charset, "X-MAC-CYRILLIC") ||
		 !g_ascii_strcasecmp(charset, "MAC-CYRILLIC") ||
		 !g_ascii_strcasecmp(charset, "Windows-1251"))
		return ENC_8BIT;
	else if (!g_ascii_strncasecmp(charset, "ISO-8859-", 9))
		return ENC_QUOTED_PRINTABLE;
	else if (!g_ascii_strncasecmp(charset, "UTF-8", 5))
		return ENC_QUOTED_PRINTABLE;
	else 
		return ENC_8BIT;
}

EncodingType procmime_get_encoding_for_text_file(const gchar *file, gboolean *has_binary)
{
	FILE *fp;
	guchar buf[BUFFSIZE];
	size_t len;
	size_t octet_chars = 0;
	size_t total_len = 0;
	gfloat octet_percentage;
	gboolean force_b64 = FALSE;

	if ((fp = claws_fopen(file, "rb")) == NULL) {
		FILE_OP_ERROR(file, "claws_fopen");
		return ENC_UNKNOWN;
	}

	while ((len = claws_fread(buf, sizeof(guchar), sizeof(buf), fp)) > 0) {
		guchar *p;
		gulong i;

		for (p = buf, i = 0; i < len; ++p, ++i) {
			if (*p & 0x80)
				++octet_chars;
			if (*p == '\0') {
				force_b64 = TRUE;
				*has_binary = TRUE;
			}
		}
		total_len += len;
	}

	claws_fclose(fp);
	
	if (total_len > 0)
		octet_percentage = (gfloat)octet_chars / (gfloat)total_len;
	else
		octet_percentage = 0.0;

	debug_print("procmime_get_encoding_for_text_file(): "
		    "8bit chars: %"G_GSIZE_FORMAT" / %"G_GSIZE_FORMAT" (%f%%)\n", octet_chars, total_len,
		    100.0 * octet_percentage);

	if (octet_percentage > 0.20 || force_b64) {
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
	{"font",  MIMETYPE_FONT},
	{"model", MIMETYPE_MODEL},
	{"chemical", MIMETYPE_CHEMICAL},
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

static int procmime_parse_mimepart(MimeInfo *parent,
			     gchar *content_type,
			     gchar *content_encoding,
			     gchar *content_description,
			     gchar *content_id,
			     gchar *content_disposition,
			     gchar *content_location,
			     const gchar *original_msgid,
			     const gchar *disposition_notification_hdr,
			     const gchar *filename,
			     guint offset,
			     guint length,
			     gboolean short_scan);

static void procmime_parse_message_rfc822(MimeInfo *mimeinfo, gboolean short_scan)
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
				{"Content-Location:",
						   NULL, TRUE},
				{"MIME-Version:",
						   NULL, TRUE},
				{"Original-Message-ID:",
						   NULL, TRUE},
				{"Disposition:",
						   NULL, TRUE},
				{NULL,		   NULL, FALSE}};
	glong content_start;
	guint i;
	FILE *fp;
        gchar *tmp;
	glong len = 0;

	procmime_decode_content(mimeinfo);

	fp = claws_fopen(mimeinfo->data.filename, "rb");
	if (fp == NULL) {
		FILE_OP_ERROR(mimeinfo->data.filename, "claws_fopen");
		return;
	}
	if (fseek(fp, mimeinfo->offset, SEEK_SET) < 0) {
		FILE_OP_ERROR(mimeinfo->data.filename, "fseek");
		claws_fclose(fp);
		return;
	}
	procheader_get_header_fields(fp, hentry);
	if (hentry[0].body != NULL) {
		tmp = conv_unmime_header(hentry[0].body, NULL, FALSE);
                g_free(hentry[0].body);
                hentry[0].body = tmp;
        }                
	if (hentry[2].body != NULL) {
		tmp = conv_unmime_header(hentry[2].body, NULL, FALSE);
                g_free(hentry[2].body);
                hentry[2].body = tmp;
        }                
	if (hentry[4].body != NULL) {
		tmp = conv_unmime_header(hentry[4].body, NULL, FALSE);
                g_free(hentry[4].body);
                hentry[4].body = tmp;
        }                
	if (hentry[5].body != NULL) {
		tmp = conv_unmime_header(hentry[5].body, NULL, FALSE);
                g_free(hentry[5].body);
                hentry[5].body = tmp;
        }                
	if (hentry[7].body != NULL) {
		tmp = conv_unmime_header(hentry[7].body, NULL, FALSE);
                g_free(hentry[7].body);
                hentry[7].body = tmp;
        }
	if (hentry[8].body != NULL) {
		tmp = conv_unmime_header(hentry[8].body, NULL, FALSE);
                g_free(hentry[8].body);
                hentry[8].body = tmp;
        }
  
	content_start = ftell(fp);
	claws_fclose(fp);
	
	len = mimeinfo->length - (content_start - mimeinfo->offset);
	if (len < 0)
		len = 0;
	procmime_parse_mimepart(mimeinfo,
				hentry[0].body, hentry[1].body,
				hentry[2].body, hentry[3].body,
				hentry[4].body, hentry[5].body,
				hentry[7].body, hentry[8].body, 
				mimeinfo->data.filename, content_start,
				len, short_scan);
	
	for (i = 0; i < (sizeof hentry / sizeof hentry[0]); i++) {
		g_free(hentry[i].body);
		hentry[i].body = NULL;
	}
}

static void procmime_parse_disposition_notification(MimeInfo *mimeinfo, 
		const gchar *original_msgid, const gchar *disposition_notification_hdr,
		gboolean short_scan)
{
	HeaderEntry hentry[] = {{"Original-Message-ID:",  NULL, TRUE},
			        {"Disposition:",	  NULL, TRUE},
				{NULL,			  NULL, FALSE}};
	guint i;
	FILE *fp;
	gchar *orig_msg_id = NULL;
	gchar *disp = NULL;

	procmime_decode_content(mimeinfo);

	debug_print("parse disposition notification\n");
	fp = claws_fopen(mimeinfo->data.filename, "rb");
	if (fp == NULL) {
		FILE_OP_ERROR(mimeinfo->data.filename, "claws_fopen");
		return;
	}
	if (fseek(fp, mimeinfo->offset, SEEK_SET) < 0) {
		FILE_OP_ERROR(mimeinfo->data.filename, "fseek");
		claws_fclose(fp);
		return;
	}

	if (original_msgid && disposition_notification_hdr) {
		hentry[0].body = g_strdup(original_msgid);
		hentry[1].body = g_strdup(disposition_notification_hdr);
	} else {
		procheader_get_header_fields(fp, hentry);
	}
    
        claws_fclose(fp);

    	if (!hentry[0].body || !hentry[1].body) {
		debug_print("MsgId %s, Disp %s\n",
			hentry[0].body ? hentry[0].body:"(nil)",
			hentry[1].body ? hentry[1].body:"(nil)");
		goto bail;
	}

	orig_msg_id = g_strdup(hentry[0].body);
	disp = g_strdup(hentry[1].body);

	extract_parenthesis(orig_msg_id, '<', '>');
	remove_space(orig_msg_id);
	
	if (strstr(disp, "displayed")) {
		/* find sent message, if possible */
		MsgInfo *info = NULL;
		GList *flist;
		debug_print("%s has been displayed.\n", orig_msg_id);
		for (flist = folder_get_list(); flist != NULL; flist = g_list_next(flist)) {
			FolderItem *outbox = ((Folder *)(flist->data))->outbox;
			if (!outbox) {
				debug_print("skipping folder with no outbox...\n");
				continue;
			}
			info = folder_item_get_msginfo_by_msgid(outbox, orig_msg_id);
			debug_print("%s %s in %s\n", info?"found":"didn't find", orig_msg_id, outbox->path);
			if (info) {
				procmsg_msginfo_set_flags(info, MSG_RETRCPT_GOT, 0);
				procmsg_msginfo_free(&info);
			}
		}
	}
	g_free(orig_msg_id);
	g_free(disp);
bail:
	for (i = 0; i < (sizeof hentry / sizeof hentry[0]); i++) {
		g_free(hentry[i].body);
		hentry[i].body = NULL;
	}
}

#define GET_HEADERS() {						\
	procheader_get_header_fields(fp, hentry);		\
        if (hentry[0].body != NULL) {				\
		tmp = conv_unmime_header(hentry[0].body, NULL, FALSE);	\
                g_free(hentry[0].body);				\
                hentry[0].body = tmp;				\
        }                					\
        if (hentry[2].body != NULL) {				\
		tmp = conv_unmime_header(hentry[2].body, NULL, FALSE);	\
                g_free(hentry[2].body);				\
                hentry[2].body = tmp;				\
        }                					\
        if (hentry[4].body != NULL) {				\
		tmp = conv_unmime_header(hentry[4].body, NULL, FALSE);	\
                g_free(hentry[4].body);				\
                hentry[4].body = tmp;				\
        }                					\
        if (hentry[5].body != NULL) {				\
		tmp = conv_unmime_header(hentry[5].body, NULL, FALSE);	\
                g_free(hentry[5].body);				\
                hentry[5].body = tmp;				\
        }                					\
	if (hentry[6].body != NULL) {				\
		tmp = conv_unmime_header(hentry[6].body, NULL, FALSE);	\
                g_free(hentry[6].body);				\
                hentry[6].body = tmp;				\
        }                					\
	if (hentry[7].body != NULL) {				\
		tmp = conv_unmime_header(hentry[7].body, NULL, FALSE);	\
                g_free(hentry[7].body);				\
                hentry[7].body = tmp;				\
        }							\
}

static void procmime_parse_multipart(MimeInfo *mimeinfo, gboolean short_scan)
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
				{"Content-Location:",
						   NULL, TRUE},
				{"Original-Message-ID:",
						   NULL, TRUE},
				{"Disposition:",
						   NULL, TRUE},
				{NULL,		   NULL, FALSE}};
	gchar *tmp;
	gchar *boundary;
	gsize boundary_len = 0;
	glong lastoffset = -1;
	gulong i;
	gchar buf[BUFFSIZE];
	FILE *fp;
	int result = 0;
	gboolean start_found = FALSE;
	gboolean end_found = FALSE;

	boundary = g_hash_table_lookup(mimeinfo->typeparameters, "boundary");
	if (!boundary)
		return;
	boundary_len = strlen(boundary);

	procmime_decode_content(mimeinfo);

	fp = claws_fopen(mimeinfo->data.filename, "rb");
	if (fp == NULL) {
		FILE_OP_ERROR(mimeinfo->data.filename, "claws_fopen");
		return;
	}

	if (fseek(fp, mimeinfo->offset, SEEK_SET) < 0) {
		FILE_OP_ERROR(mimeinfo->data.filename, "fseek");
		claws_fclose(fp);
		return;
	}

	while (claws_fgets(buf, sizeof(buf), fp) != NULL && result == 0) {
		if (ftell(fp) - 1 > (mimeinfo->offset + mimeinfo->length))
			break;

		if (IS_BOUNDARY(buf, boundary, boundary_len)) {
			start_found = TRUE;

			if (lastoffset != -1) {
				glong len = (ftell(fp) - strlen(buf)) - lastoffset - 1;
				if (len < 0)
					len = 0;
				result = procmime_parse_mimepart(mimeinfo,
				                        hentry[0].body, hentry[1].body,
							hentry[2].body, hentry[3].body, 
							hentry[4].body, hentry[5].body,
							hentry[6].body, hentry[7].body,
							mimeinfo->data.filename, lastoffset,
							len, short_scan);
				if (result == 1 && short_scan)
					break;
				
			} 
			
			if (buf[2 + boundary_len]     == '-' &&
			    buf[2 + boundary_len + 1] == '-') {
			    	end_found = TRUE;
				break;
			}
			for (i = 0; i < (sizeof hentry / sizeof hentry[0]) ; i++) {
				g_free(hentry[i].body);
				hentry[i].body = NULL;
			}
			GET_HEADERS();
			lastoffset = ftell(fp);
		}
	}
	
	if (start_found && !end_found && lastoffset != -1) {
		glong len = (ftell(fp) - strlen(buf)) - lastoffset - 1;

		if (len >= 0) {
			result = procmime_parse_mimepart(mimeinfo,
				        hentry[0].body, hentry[1].body,
					hentry[2].body, hentry[3].body, 
					hentry[4].body, hentry[5].body,
					hentry[6].body, hentry[7].body,
					mimeinfo->data.filename, lastoffset,
					len, short_scan);
		}
		mimeinfo->broken = TRUE;
	}
	
	for (i = 0; i < (sizeof hentry / sizeof hentry[0]); i++) {
		g_free(hentry[i].body);
		hentry[i].body = NULL;
	}
	claws_fclose(fp);
}

static void parse_parameters(const gchar *parameters, GHashTable *table)
{
	gchar *params, *param, *next;
	GSList *convlist = NULL, *concatlist = NULL, *cur;

	params = g_strdup(parameters);
	param = params;
	next = params;
	for (; next != NULL; param = next) {
		gchar *attribute, *value, *tmp, *down_attr, *orig_down_attr;
		glong len;
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
		while (value[0] != '\0' && value[0] == ' ')
			value++;

		down_attr = g_utf8_strdown(attribute, -1);
		orig_down_attr = down_attr;
	
		len = down_attr ? strlen(down_attr):0;
		if (len > 0 && down_attr[len - 1] == '*') {
			gchar *srcpos, *dstpos, *endpos;

			convert = TRUE;
			down_attr[len - 1] = '\0';

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
			if (value[0] == '"')
				extract_quote(value, '"');
		} else {
			if (value[0] == '"')
				extract_quote(value, '"');
			else if ((tmp = strchr(value, ' ')) != NULL)
				*tmp = '\0';
		}

		if (down_attr) {
			while (down_attr[0] == ' ')
				down_attr++;
			while (down_attr[strlen(down_attr)-1] == ' ') 
				down_attr[strlen(down_attr)-1] = '\0';
		} 

		while (value[0] != '\0' && value[0] == ' ')
			value++;
		while (value[strlen(value)-1] == ' ') 
			value[strlen(value)-1] = '\0';

		if (down_attr && strrchr(down_attr, '*') != NULL) {
			gchar *tmpattr;

			tmpattr = g_strdup(down_attr);
			tmp = strrchr(tmpattr, '*');
			tmp[0] = '\0';

			if ((tmp[1] == '0') && (tmp[2] == '\0') && 
			    (g_slist_find_custom(concatlist, down_attr, (GCompareFunc)g_strcmp0) == NULL))
				concatlist = g_slist_prepend(concatlist, g_strdup(tmpattr));

			if (convert && (g_slist_find_custom(convlist, tmpattr, (GCompareFunc)g_strcmp0) == NULL))
				convlist = g_slist_prepend(convlist, g_strdup(tmpattr));

			g_free(tmpattr);
		} else if (convert) {
			if (g_slist_find_custom(convlist, down_attr, (GCompareFunc)g_strcmp0) == NULL)
				convlist = g_slist_prepend(convlist, g_strdup(down_attr));
		}

		if (g_hash_table_lookup(table, down_attr) == NULL)
			g_hash_table_insert(table, g_strdup(down_attr), g_strdup(value));
		g_free(orig_down_attr);
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

			g_hash_table_remove(table, attrwnum);
			g_free(attrwnum);
			n++;
			attrwnum = g_strdup_printf("%s*%d", attribute, n);
		}
		g_free(attrwnum);

		g_hash_table_insert(table, g_strdup(attribute), g_strdup(value->str));
		g_string_free(value, TRUE);
	}
	slist_free_strings_full(concatlist);

	for (cur = convlist; cur != NULL; cur = g_slist_next(cur)) {
		gchar *attribute, *key, *value;
		gchar *charset, *lang, *oldvalue, *newvalue;

		attribute = (gchar *) cur->data;
		if (!g_hash_table_lookup_extended(
			table, attribute, (gpointer *)(gchar *) &key, (gpointer *)(gchar *) &value))
			continue;

		charset = value;
		if (charset == NULL)
			continue;
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
	slist_free_strings_full(convlist);

	g_free(params);
}	

static void procmime_parse_content_type(const gchar *content_type, MimeInfo *mimeinfo)
{
	cm_return_if_fail(content_type != NULL);
	cm_return_if_fail(mimeinfo != NULL);

	/* RFC 2045, page 13 says that the mime subtype is MANDATORY;
	 * if it's not available we use the default Content-Type */
	if ((content_type[0] == '\0') || (strchr(content_type, '/') == NULL)) {
		mimeinfo->type = MIMETYPE_TEXT;
		mimeinfo->subtype = g_strdup("plain");
		if (g_hash_table_lookup(mimeinfo->typeparameters,
				       "charset") == NULL) {
			g_hash_table_insert(mimeinfo->typeparameters,
				    g_strdup("charset"),
				    g_strdup(
					conv_get_locale_charset_str_no_utf8()));
		}
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
		mimeinfo->subtype = g_strstrip(g_strdup(subtype));

		/* Get mimeinfo->typeparameters */
		if (params != NULL)
			parse_parameters(params, mimeinfo->typeparameters);

		g_free(type);
	}
}

static void procmime_parse_content_disposition(const gchar *content_disposition, MimeInfo *mimeinfo)
{
	gchar *tmp, *params;

	cm_return_if_fail(content_disposition != NULL);
	cm_return_if_fail(mimeinfo != NULL);

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

static GSList *registered_parsers = NULL;

static MimeParser *procmime_get_mimeparser_for_type(MimeMediaType type, const gchar *sub_type)
{
	GSList *cur;
	for (cur = registered_parsers; cur; cur = cur->next) {
		MimeParser *parser = (MimeParser *)cur->data;
		if (parser->type == type && !g_strcmp0(parser->sub_type, sub_type))
			return parser;
	}
	return NULL;
}

void procmime_mimeparser_register(MimeParser *parser)
{
	if (!procmime_get_mimeparser_for_type(parser->type, parser->sub_type))
		registered_parsers = g_slist_append(registered_parsers, parser);
}


void procmime_mimeparser_unregister(MimeParser *parser) 
{
	registered_parsers = g_slist_remove(registered_parsers, parser);
}

static gboolean procmime_mimeparser_parse(MimeParser *parser, MimeInfo *mimeinfo)
{
	cm_return_val_if_fail(parser->parse != NULL, FALSE);
	return parser->parse(parser, mimeinfo);	
}

static int procmime_parse_mimepart(MimeInfo *parent,
			     gchar *content_type,
			     gchar *content_encoding,
			     gchar *content_description,
			     gchar *content_id,
			     gchar *content_disposition,
			     gchar *content_location,
			     const gchar *original_msgid,
			     const gchar *disposition_notification_hdr,
			     const gchar *filename,
			     guint offset,
			     guint length,
			     gboolean short_scan)
{
	MimeInfo *mimeinfo;
	MimeParser *parser = NULL;
	gboolean parsed = FALSE;
	int result = 0;

	/* Create MimeInfo */
	mimeinfo = procmime_mimeinfo_new();
	mimeinfo->content = MIMECONTENT_FILE;

	if (parent != NULL) {
		if (g_node_depth(parent->node) > 32) {
			/* 32 is an arbitrary value
			 * this avoids DOSsing ourselves 
			 * with enormous messages
			 */
			procmime_mimeinfo_free_all(&mimeinfo);
			return -1;			
		}
		g_node_append(parent->node, mimeinfo->node);
	}
	mimeinfo->data.filename = g_strdup(filename);
	mimeinfo->offset = offset;
	mimeinfo->length = length;

	if (content_type != NULL) {
		g_strchomp(content_type);
		procmime_parse_content_type(content_type, mimeinfo);
	} else {
		mimeinfo->type = MIMETYPE_TEXT;
		mimeinfo->subtype = g_strdup("plain");
		if (g_hash_table_lookup(mimeinfo->typeparameters,
				       "charset") == NULL) {
			g_hash_table_insert(mimeinfo->typeparameters,
				    g_strdup("charset"),
				    g_strdup(
					conv_get_locale_charset_str_no_utf8()));
		}
	}

	if (content_encoding != NULL) {
 		g_strchomp(content_encoding);
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

	if (content_location != NULL)
		mimeinfo->location = g_strdup(content_location);
	else
		mimeinfo->location = NULL;

	if (content_disposition != NULL) {
 		g_strchomp(content_disposition);
		procmime_parse_content_disposition(content_disposition, mimeinfo);
	} else
		mimeinfo->disposition = DISPOSITIONTYPE_UNKNOWN;

	/* Call parser for mime type */
	if ((parser = procmime_get_mimeparser_for_type(mimeinfo->type, mimeinfo->subtype)) != NULL) {
		parsed = procmime_mimeparser_parse(parser, mimeinfo);
	} 
	if (!parsed) {
		switch (mimeinfo->type) {
		case MIMETYPE_TEXT:
			if (g_ascii_strcasecmp(mimeinfo->subtype, "plain") == 0 && short_scan) {
				return 1;
			}
			break;

		case MIMETYPE_MESSAGE:
			if (g_ascii_strcasecmp(mimeinfo->subtype, "rfc822") == 0) {
				procmime_parse_message_rfc822(mimeinfo, short_scan);
			}
			if (g_ascii_strcasecmp(mimeinfo->subtype, "disposition-notification") == 0) {
				procmime_parse_disposition_notification(mimeinfo, 
					original_msgid, disposition_notification_hdr, short_scan);
			}
			break;
			
		case MIMETYPE_MULTIPART:
			procmime_parse_multipart(mimeinfo, short_scan);
			break;
		
		case MIMETYPE_APPLICATION:
			if (g_ascii_strcasecmp(mimeinfo->subtype, "octet-stream") == 0
			&& original_msgid && *original_msgid 
			&& disposition_notification_hdr && *disposition_notification_hdr) {
				procmime_parse_disposition_notification(mimeinfo, 
					original_msgid, disposition_notification_hdr, short_scan);
			}
			break;
		default:
			break;
		}
	}

	return result;
}

static gchar *typenames[] = {
    "text",
    "image",
    "audio",
    "video",
    "application",
    "message",
    "multipart",
    "font",
    "model",
    "chemical",
    "unknown",
};

static gboolean output_func(GNode *node, gpointer data)
{
	guint i, depth;
	MimeInfo *mimeinfo = (MimeInfo *) node->data;

	depth = g_node_depth(node);
	for (i = 0; i < depth; i++)
		g_print("    ");
	g_print("%s/%s (offset:%ld length:%ld encoding: %d)\n", 
		(mimeinfo->type <= MIMETYPE_UNKNOWN)? typenames[mimeinfo->type] : "unknown", 
		mimeinfo->subtype, mimeinfo->offset, mimeinfo->length, mimeinfo->encoding_type);

	return FALSE;
}

static void output_mime_structure(MimeInfo *mimeinfo, int indent)
{
	g_node_traverse(mimeinfo->node, G_PRE_ORDER, G_TRAVERSE_ALL, -1, output_func, NULL);
}

static MimeInfo *procmime_scan_file_with_offset(const gchar *filename, glong offset, gboolean short_scan)
{
	MimeInfo *mimeinfo;
	GStatBuf buf;

	if (g_stat(filename, &buf) < 0) {
		FILE_OP_ERROR(filename, "stat");
		return NULL;
	}

	mimeinfo = procmime_mimeinfo_new();
	mimeinfo->content = MIMECONTENT_FILE;
	mimeinfo->encoding_type = ENC_UNKNOWN;
	mimeinfo->type = MIMETYPE_MESSAGE;
	mimeinfo->subtype = g_strdup("rfc822");
	mimeinfo->data.filename = g_strdup(filename);
	mimeinfo->offset = offset;
	mimeinfo->length = buf.st_size - offset;

	procmime_parse_message_rfc822(mimeinfo, short_scan);
	if (debug_get_mode())
		output_mime_structure(mimeinfo, 0);

	return mimeinfo;
}

static MimeInfo *procmime_scan_file_full(const gchar *filename, gboolean short_scan)
{
	MimeInfo *mimeinfo;

	cm_return_val_if_fail(filename != NULL, NULL);

	mimeinfo = procmime_scan_file_with_offset(filename, 0, short_scan);

	return mimeinfo;
}

MimeInfo *procmime_scan_file(const gchar *filename)
{
	return procmime_scan_file_full(filename, FALSE);
}

static MimeInfo *procmime_scan_file_short(const gchar *filename)
{
	return procmime_scan_file_full(filename, TRUE);
}

static MimeInfo *procmime_scan_queue_file_full(const gchar *filename, gboolean short_scan)
{
	FILE *fp;
	MimeInfo *mimeinfo;
	gchar buf[BUFFSIZE];
	glong offset = 0;

	cm_return_val_if_fail(filename != NULL, NULL);

	/* Open file */
	if ((fp = claws_fopen(filename, "rb")) == NULL)
		return NULL;
	/* Skip queue header */
	while (claws_fgets(buf, sizeof(buf), fp) != NULL) {
		/* new way */
		if ((!strncmp(buf, "X-Claws-End-Special-Headers: 1",
			strlen("X-Claws-End-Special-Headers:"))) ||
		   (!strncmp(buf, "X-Sylpheed-End-Special-Headers: 1",
			strlen("X-Sylpheed-End-Special-Headers:"))))
			break;
		/* old way */
		if (buf[0] == '\r' || buf[0] == '\n') break;
		/* from other mailers */
		if (!strncmp(buf, "Date: ", 6)
		||  !strncmp(buf, "To: ", 4)
		||  !strncmp(buf, "From: ", 6)
		||  !strncmp(buf, "Subject: ", 9)) {
			rewind(fp);
			break;
		}
	}
	offset = ftell(fp);
	claws_fclose(fp);

	mimeinfo = procmime_scan_file_with_offset(filename, offset, short_scan);

	return mimeinfo;
}

MimeInfo *procmime_scan_queue_file(const gchar *filename)
{
	return procmime_scan_queue_file_full(filename, FALSE);
}

static MimeInfo *procmime_scan_queue_file_short(const gchar *filename)
{
	return procmime_scan_queue_file_full(filename, TRUE);
}

typedef enum {
    ENC_AS_TOKEN,
    ENC_AS_QUOTED_STRING,
    ENC_AS_EXTENDED,
    ENC_AS_ENCWORD
} EncodeAs;

typedef struct _ParametersData {
	FILE *fp;
	guint len;
	gint error;
} ParametersData;

static void write_parameters(gpointer key, gpointer value, gpointer user_data)
{
	gchar *param = key;
	gchar *val = value, *valpos, *tmp;
	ParametersData *pdata = (ParametersData *)user_data;
	GString *buf = g_string_new("");
	glong len;

	EncodeAs encas = ENC_AS_TOKEN;

	for (valpos = val; *valpos != 0; valpos++) {
		if (!IS_ASCII(*valpos)) {
			encas = ENC_AS_ENCWORD;
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
		case '"':
        	case '/':
		case '[':
		case ']':
		case '?':
		case '=':
			encas = ENC_AS_QUOTED_STRING;
			continue;
		}
	}
	
	switch (encas) {
	case ENC_AS_TOKEN:
		g_string_append_printf(buf, "%s=%s", param, val);
		break;

	case ENC_AS_QUOTED_STRING:
		g_string_append_printf(buf, "%s=\"%s\"", param, val);
		break;

#if 0 /* we don't use that for now */
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
#else
	case ENC_AS_EXTENDED:
		debug_print("Unhandled ENC_AS_EXTENDED.\n");
		break;
#endif
	case ENC_AS_ENCWORD:
		len = MAX(strlen(val)*6, 512);
		tmp = g_malloc(len+1);
		codeconv_set_strict(TRUE);
		conv_encode_header_full(tmp, len, val, pdata->len + strlen(param) + 4 , FALSE,
			prefs_common.outgoing_charset);
		codeconv_set_strict(FALSE);
		if (!tmp || !*tmp) {
			codeconv_set_strict(TRUE);
			conv_encode_header_full(tmp, len, val, pdata->len + strlen(param) + 4 , FALSE,
				conv_get_outgoing_charset_str());
			codeconv_set_strict(FALSE);
		}
		if (!tmp || !*tmp) {
			codeconv_set_strict(TRUE);
			conv_encode_header_full(tmp, len, val, pdata->len + strlen(param) + 4 , FALSE,
				CS_UTF_8);
			codeconv_set_strict(FALSE);
		}
		if (!tmp || !*tmp) {
			conv_encode_header_full(tmp, len, val, pdata->len + strlen(param) + 4 , FALSE,
				CS_UTF_8);
		}
		g_string_append_printf(buf, "%s=\"%s\"", param, tmp);
		g_free(tmp);
		break;

	}
	
	if (buf->str && strlen(buf->str)) {
		tmp = strstr(buf->str, "\n");
		if (tmp)
			len = (tmp - buf->str);
		else
			len = strlen(buf->str);
		if (pdata->len + len > 76) {
			if (fprintf(pdata->fp, ";\n %s", buf->str) < 0)
				pdata->error = TRUE;
			pdata->len = strlen(buf->str) + 1;
		} else {
			if (fprintf(pdata->fp, "; %s", buf->str) < 0)
				pdata->error = TRUE;
			pdata->len += strlen(buf->str) + 2;
		}
	}
	g_string_free(buf, TRUE);
}

#define TRY(func) { \
	if (!(func)) { \
		return -1; \
	} \
}

int procmime_write_mime_header(MimeInfo *mimeinfo, FILE *fp)
{
	struct TypeTable *type_table;
	ParametersData *pdata = g_new0(ParametersData, 1);
	debug_print("procmime_write_mime_header\n");
	
	pdata->fp = fp;
	pdata->error = FALSE;
	for (type_table = mime_type_table; type_table->str != NULL; type_table++)
		if (mimeinfo->type == type_table->type) {
			gchar *buf = g_strdup_printf(
				"Content-Type: %s/%s", type_table->str, mimeinfo->subtype);
			if (fprintf(fp, "%s", buf) < 0) {
				g_free(buf);
				g_free(pdata);
				return -1;
			}
			pdata->len = strlen(buf);
			g_free(buf);
			break;
		}
	g_hash_table_foreach(mimeinfo->typeparameters, write_parameters, pdata);
	if (pdata->error == TRUE) {
		g_free(pdata);
		return -1;
	}
	g_free(pdata);

	TRY(fprintf(fp, "\n") >= 0);

	if (mimeinfo->encoding_type != ENC_UNKNOWN)
		TRY(fprintf(fp, "Content-Transfer-Encoding: %s\n", procmime_get_encoding_str(mimeinfo->encoding_type)) >= 0);

	if (mimeinfo->description != NULL)
		TRY(fprintf(fp, "Content-Description: %s\n", mimeinfo->description) >= 0);

	if (mimeinfo->id != NULL)
		TRY(fprintf(fp, "Content-ID: %s\n", mimeinfo->id) >= 0);

	if (mimeinfo->location != NULL)
		TRY(fprintf(fp, "Content-Location: %s\n", mimeinfo->location) >= 0);

	if (mimeinfo->disposition != DISPOSITIONTYPE_UNKNOWN) {
		ParametersData *pdata = g_new0(ParametersData, 1);
		gchar *buf = NULL;
		if (mimeinfo->disposition == DISPOSITIONTYPE_INLINE)
			buf = g_strdup("Content-Disposition: inline");
		else if (mimeinfo->disposition == DISPOSITIONTYPE_ATTACHMENT)
			buf = g_strdup("Content-Disposition: attachment");
		else
			buf = g_strdup("Content-Disposition: unknown");

		if (fprintf(fp, "%s", buf) < 0) {
			g_free(buf);
			g_free(pdata);
			return -1;
		}
		pdata->len = strlen(buf);
		g_free(buf);

		pdata->fp = fp;
		pdata->error = FALSE;
		g_hash_table_foreach(mimeinfo->dispositionparameters, write_parameters, pdata);
		if (pdata->error == TRUE) {
			g_free(pdata);
			return -1;
		}
		g_free(pdata);
		TRY(fprintf(fp, "\n") >= 0);
	}

	TRY(fprintf(fp, "\n") >= 0);
	
	return 0;
}

static gint procmime_write_message_rfc822(MimeInfo *mimeinfo, FILE *fp)
{
	FILE *infp;
	GNode *childnode;
	MimeInfo *child;
	gchar buf[BUFFSIZE];
	gboolean skip = FALSE;
	size_t len;

	debug_print("procmime_write_message_rfc822\n");

	/* write header */
	switch (mimeinfo->content) {
	case MIMECONTENT_FILE:
		if ((infp = claws_fopen(mimeinfo->data.filename, "rb")) == NULL) {
			FILE_OP_ERROR(mimeinfo->data.filename, "claws_fopen");
			return -1;
		}
		if (fseek(infp, mimeinfo->offset, SEEK_SET) < 0) {
			FILE_OP_ERROR(mimeinfo->data.filename, "fseek");
			claws_fclose(infp);
			return -1;
		}
		while (claws_fgets(buf, sizeof(buf), infp) == buf) {
			strcrchomp(buf);
			if (buf[0] == '\n' && buf[1] == '\0')
				break;
			if (skip && (buf[0] == ' ' || buf[0] == '\t'))
				continue;
			if (g_ascii_strncasecmp(buf, "MIME-Version:", 13) == 0 ||
			    g_ascii_strncasecmp(buf, "Content-Type:", 13) == 0 ||
			    g_ascii_strncasecmp(buf, "Content-Transfer-Encoding:", 26) == 0 ||
			    g_ascii_strncasecmp(buf, "Content-Description:", 20) == 0 ||
			    g_ascii_strncasecmp(buf, "Content-ID:", 11) == 0 ||
			    g_ascii_strncasecmp(buf, "Content-Location:", 17) == 0 ||
			    g_ascii_strncasecmp(buf, "Content-Disposition:", 20) == 0) {
				skip = TRUE;
				continue;
			}
			len = strlen(buf);
			if (claws_fwrite(buf, sizeof(gchar), len, fp) < len) {
				g_warning("failed to dump %"G_GSIZE_FORMAT" bytes from file", len);
				claws_fclose(infp);
				return -1;
			}
			skip = FALSE;
		}
		claws_fclose(infp);
		break;

	case MIMECONTENT_MEM:
		len = strlen(mimeinfo->data.mem);
		if (claws_fwrite(mimeinfo->data.mem, sizeof(gchar), len, fp) < len) {
			g_warning("failed to dump %"G_GSIZE_FORMAT" bytes from mem", len);
			return -1;
		}
		break;

	default:
		break;
	}

	childnode = mimeinfo->node->children;
	if (childnode == NULL)
		return -1;

	child = (MimeInfo *) childnode->data;
	if (fprintf(fp, "MIME-Version: 1.0\n") < 0) {
		g_warning("failed to write mime version");
		return -1;
	}
	if (procmime_write_mime_header(child, fp) < 0)
		return -1;
	return procmime_write_mimeinfo(child, fp);
}

static gint procmime_write_multipart(MimeInfo *mimeinfo, FILE *fp)
{
	FILE *infp;
	GNode *childnode;
	gchar *boundary, *str, *str2;
	gchar buf[BUFFSIZE];
	gboolean firstboundary;
	size_t len;

	debug_print("procmime_write_multipart\n");

	boundary = g_hash_table_lookup(mimeinfo->typeparameters, "boundary");

	switch (mimeinfo->content) {
	case MIMECONTENT_FILE:
		if ((infp = claws_fopen(mimeinfo->data.filename, "rb")) == NULL) {
			FILE_OP_ERROR(mimeinfo->data.filename, "claws_fopen");
			return -1;
		}
		if (fseek(infp, mimeinfo->offset, SEEK_SET) < 0) {
			FILE_OP_ERROR(mimeinfo->data.filename, "fseek");
			claws_fclose(infp);
			return -1;
		}
		while (claws_fgets(buf, sizeof(buf), infp) == buf) {
			if (IS_BOUNDARY(buf, boundary, strlen(boundary)))
				break;
			len = strlen(buf);
			if (claws_fwrite(buf, sizeof(gchar), len, fp) < len) {
				g_warning("failed to write %"G_GSIZE_FORMAT, len);
				claws_fclose(infp);
				return -1;
			}
		}
		claws_fclose(infp);
		break;

	case MIMECONTENT_MEM:
		str = g_strdup(mimeinfo->data.mem);
		if (((str2 = strstr(str, boundary)) != NULL) && ((str2 - str) >= 2) &&
		    (*(str2 - 1) == '-') && (*(str2 - 2) == '-'))
			*(str2 - 2) = '\0';
		len = strlen(str);
		if (claws_fwrite(str, sizeof(gchar), len, fp) < len) {
			g_warning("failed to write %"G_GSIZE_FORMAT" from mem", len);
			g_free(str);
			return -1;
		}
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
			TRY(fprintf(fp, "\n") >= 0);
			
		TRY(fprintf(fp, "--%s\n", boundary) >= 0);

		if (procmime_write_mime_header(child, fp) < 0)
			return -1;
		if (procmime_write_mimeinfo(child, fp) < 0)
			return -1;

		childnode = g_node_next_sibling(childnode);
	}	
	TRY(fprintf(fp, "\n--%s--\n", boundary) >= 0);

	return 0;
}

gint procmime_write_mimeinfo(MimeInfo *mimeinfo, FILE *fp)
{
	FILE *infp;
	size_t len;
	debug_print("procmime_write_mimeinfo\n");

	if (G_NODE_IS_LEAF(mimeinfo->node)) {
		switch (mimeinfo->content) {
		case MIMECONTENT_FILE:
			if ((infp = claws_fopen(mimeinfo->data.filename, "rb")) == NULL) {
				FILE_OP_ERROR(mimeinfo->data.filename, "claws_fopen");
				return -1;
			}
			copy_file_part_to_fp(infp, mimeinfo->offset, mimeinfo->length, fp);
			claws_fclose(infp);
			return 0;

		case MIMECONTENT_MEM:
			len = strlen(mimeinfo->data.mem);
			if (claws_fwrite(mimeinfo->data.mem, sizeof(gchar), len, fp) < len)
				return -1;
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

void *procmime_get_part_as_string(MimeInfo *mimeinfo,
		gboolean null_terminate)
{
	FILE *infp;
	gchar *data;
	glong length, readlength;

	cm_return_val_if_fail(mimeinfo != NULL, NULL);

	if (mimeinfo->encoding_type != ENC_BINARY &&
			!procmime_decode_content(mimeinfo))
		return NULL;

	if (mimeinfo->content == MIMECONTENT_MEM)
		return g_strdup(mimeinfo->data.mem);

	if ((infp = claws_fopen(mimeinfo->data.filename, "rb")) == NULL) {
		FILE_OP_ERROR(mimeinfo->data.filename, "claws_fopen");
		return NULL;
	}

	if (fseek(infp, mimeinfo->offset, SEEK_SET) < 0) {
		FILE_OP_ERROR(mimeinfo->data.filename, "fseek");
		claws_fclose(infp);
		return NULL;
	}

	length = mimeinfo->length;

	data = g_malloc(null_terminate ? length + 1 : length);
	if (data == NULL) {
		g_warning("could not allocate %ld bytes for procmime_get_part_as_string",
				(null_terminate ? length + 1 : length));
		claws_fclose(infp);
		return NULL;
	}

	readlength = claws_fread(data, length, 1, infp);
	if (readlength <= 0) {
		FILE_OP_ERROR(mimeinfo->data.filename, "fread");
		g_free(data);
		claws_fclose(infp);
		return NULL;
	}

	claws_fclose(infp);

	if (null_terminate)
		data[length] = '\0';

	return data;
}

/* Returns an open GInputStream. The caller should just
 * read mimeinfo->length bytes from it and then release it. */
GInputStream *procmime_get_part_as_inputstream(MimeInfo *mimeinfo)
{
	cm_return_val_if_fail(mimeinfo != NULL, NULL);

	if (mimeinfo->encoding_type != ENC_BINARY &&
			!procmime_decode_content(mimeinfo)) {
		g_warning("could not decode part");
		return NULL;
	}
	if (mimeinfo->content == MIMECONTENT_MEM) {
		/* NULL for destroy func, since we're not copying
		 * the data for the stream. */
		return g_memory_input_stream_new_from_data(
				mimeinfo->data.mem,
				(gssize)mimeinfo->length, NULL);
	} else {
		return g_memory_input_stream_new_from_data(
				procmime_get_part_as_string(mimeinfo, FALSE),
				mimeinfo->length, g_free);
	}
}

GdkPixbuf *procmime_get_part_as_pixbuf(MimeInfo *mimeinfo, GError **error)
{
	GdkPixbuf *pixbuf;
	GInputStream *stream;

	if (error)
		*error = NULL;

	stream = procmime_get_part_as_inputstream(mimeinfo);
	if (stream == NULL) {
		if (error)
			*error = g_error_new_literal(G_FILE_ERROR, -1, _("Could not decode part"));
		return NULL;
	}

	pixbuf = gdk_pixbuf_new_from_stream(stream, NULL, error);
	g_object_unref(stream);

	if (error && *error != NULL)
		return NULL;

	return pixbuf;
}

