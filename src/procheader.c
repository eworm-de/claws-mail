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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#include "claws-features.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/stat.h>

#include "procheader.h"
#include "procmsg.h"
#include "codeconv.h"
#include "prefs_common.h"
#include "hooks.h"
#include "utils.h"
#include "defs.h"
#include "file-utils.h"

#define BUFFSIZE	8192

static gchar monthstr[] = "JanFebMarAprMayJunJulAugSepOctNovDec";

typedef char *(*getlinefunc) (char *, size_t, void *);
typedef int (*peekcharfunc) (void *);
typedef int (*getcharfunc) (void *);
typedef gint (*get_one_field_func) (gchar **, void *, HeaderEntry[]);

static gint string_get_one_field(gchar **buf, char **str,
				 HeaderEntry hentry[]);

static char *string_getline(char *buf, size_t len, char **str);
static int string_peekchar(char **str);
static int file_peekchar(FILE *fp);
static gint generic_get_one_field(gchar **bufptr, void *data,
				  HeaderEntry hentry[],
				  getlinefunc getline, 
				  peekcharfunc peekchar,
				  gboolean unfold);
static MsgInfo *parse_stream(void *data, gboolean isstring, MsgFlags flags,
			     gboolean full, gboolean decrypted);


gint procheader_get_one_field(gchar **buf, FILE *fp,
			      HeaderEntry hentry[])
{
	return generic_get_one_field(buf, fp, hentry,
				     (getlinefunc)fgets_crlf, (peekcharfunc)file_peekchar,
				     TRUE);
}

static gint string_get_one_field(gchar **buf, char **str,
				 HeaderEntry hentry[])
{
	return generic_get_one_field(buf, str, hentry,
				     (getlinefunc)string_getline,
				     (peekcharfunc)string_peekchar,
				     TRUE);
}

gboolean procheader_skip_headers(FILE *fp)
{
	gchar *buf = g_malloc(BUFFSIZE);
	do {
		if (fgets_crlf(buf, BUFFSIZE - 1, fp) == NULL) {
			g_free(buf);
			return FALSE;
		}
		if (buf[0] == '\r' || buf[0] == '\n') {
			break;
		}
	} while (TRUE);
	g_free(buf);

	return TRUE;
}


static char *string_getline(char *buf, size_t len, char **str)
{
	gboolean is_cr = FALSE;
	gboolean last_was_cr = FALSE;

	if (!*str || !**str)
		return NULL;

	for (; **str && len > 1; --len) {
		is_cr = (**str == '\r');
		if ((*buf++ = *(*str)++) == '\n') {
		    break;
		}
		if (last_was_cr) {
			*(--buf) = '\n';
			buf++;
		    break;
		}
		last_was_cr = is_cr;
	}
		
	*buf = '\0';

	return buf;
}

static int string_peekchar(char **str)
{
	return **str;
}

static int file_peekchar(FILE *fp)
{
	return ungetc(getc(fp), fp);
}

static gint generic_get_one_field(gchar **bufptr, void *data,
			  HeaderEntry *hentry,
			  getlinefunc getline, peekcharfunc peekchar,
			  gboolean unfold)
{
	/* returns -1 in case of failure of any kind, whatever it's a parsing error
	   or an allocation error. if returns -1, *bufptr is always NULL, and vice-versa,
	   and if returning 0 (OK), *bufptr is always non-NULL, so callers just have to
	   test the return value
	*/
	gint nexthead;
	gint hnum = 0;
	HeaderEntry *hp = NULL;
	size_t len;
	gchar *buf;

	cm_return_val_if_fail(bufptr != NULL, -1);

	len = BUFFSIZE;
	buf = g_malloc(len);

	if (hentry != NULL) {
		/* skip non-required headers */
		/* and get hentry header line */
		do {
			do {
				if (getline(buf, len, data) == NULL) {
					debug_print("generic_get_one_field: getline\n");
					g_free(buf);
					*bufptr = NULL;
					return -1;
				}
				if (buf[0] == '\r' || buf[0] == '\n') {
					debug_print("generic_get_one_field: empty line\n");
					g_free(buf);
					*bufptr = NULL;
					return -1;
				}
			} while (buf[0] == ' ' || buf[0] == '\t');

			for (hp = hentry, hnum = 0; hp->name != NULL;
			     hp++, hnum++) {
				if (!g_ascii_strncasecmp(hp->name, buf,
						 strlen(hp->name)))
					break;
			}
		} while (hp->name == NULL);
	} else {
		/* read first line */
		if (getline(buf, len, data) == NULL) {
			debug_print("generic_get_one_field: getline\n");
			g_free(buf);
			*bufptr = NULL;
			return -1;
		}
		if (buf[0] == '\r' || buf[0] == '\n') {
			debug_print("generic_get_one_field: empty line\n");
			g_free(buf);
			*bufptr = NULL;
			return -1;
		}
	}
	/* reduce initial buffer to its useful part */
	len = strlen(buf)+1;
	buf = g_realloc(buf, len);
	if (buf == NULL) {
		debug_print("generic_get_one_field: reallocation error\n");
		*bufptr = NULL;
		return -1;
	}

	/* unfold line */
	while (1) {
		nexthead = peekchar(data);
		/* ([*WSP CRLF] 1*WSP) */
		if (nexthead == ' ' || nexthead == '\t') {
			size_t buflen;
			gchar *tmpbuf;
			size_t tmplen;

			gboolean skiptab = (nexthead == '\t');
			/* trim previous trailing \n if requesting one header or
			 * unfolding was requested */
			if ((!hentry && unfold) || (hp && hp->unfold))
				strretchomp(buf);

			buflen = strlen(buf);
			
			/* read next line */
			tmpbuf = g_malloc(BUFFSIZE);

			if (getline(tmpbuf, BUFFSIZE, data) == NULL) {
				g_free(tmpbuf);
				break;
			}
			tmplen = strlen(tmpbuf)+1;

			/* extend initial buffer and concatenate next line */
			len += tmplen;
			buf = g_realloc(buf, len);
			if (buf == NULL) {
				debug_print("generic_get_one_field: reallocation error\n");
				g_free(buf);
				*bufptr = NULL;
				return -1;
			}
			memcpy(buf+buflen, tmpbuf, tmplen);
			g_free(tmpbuf);
			if (skiptab) { /* replace tab with space */
				*(buf + buflen) = ' ';
			}
		} else {
			/* remove trailing new line */
			strretchomp(buf);
			break;
		}
	}

	*bufptr = buf;

	return hnum;
}

gint procheader_get_one_field_asis(gchar **buf, FILE *fp)
{
	return generic_get_one_field(buf, fp, NULL,
				     (getlinefunc)fgets_crlf, 
				     (peekcharfunc)file_peekchar,
				     FALSE);
}

GPtrArray *procheader_get_header_array(FILE *fp)
{
	gchar *buf = NULL;
	GPtrArray *headers;
	Header *header;

	cm_return_val_if_fail(fp != NULL, NULL);

	headers = g_ptr_array_new();

	while (procheader_get_one_field(&buf, fp, NULL) != -1) {
		if ((header = procheader_parse_header(buf)) != NULL)
			g_ptr_array_add(headers, header);
		g_free(buf);
		buf = NULL;
	}

	return headers;
}

void procheader_header_array_destroy(GPtrArray *harray)
{
	gint i;
	Header *header;

	cm_return_if_fail(harray != NULL);

	for (i = 0; i < harray->len; i++) {
		header = g_ptr_array_index(harray, i);
		procheader_header_free(header);
	}

	g_ptr_array_free(harray, TRUE);
}

void procheader_header_free(Header *header)
{
	if (!header) return;

	g_free(header->name);
	g_free(header->body);
	g_free(header);
}

/*
  tests whether two headers' names are equal
  remove the trailing ':' or ' ' before comparing
*/

gboolean procheader_headername_equal(char * hdr1, char * hdr2)
{
	int len1;
	int len2;

	len1 = strlen(hdr1);
	len2 = strlen(hdr2);
	if (hdr1[len1 - 1] == ':')
		len1--;
	if (hdr2[len2 - 1] == ':')
		len2--;
	if (len1 != len2)
		return 0;

	return (g_ascii_strncasecmp(hdr1, hdr2, len1) == 0);
}

/*
  parse headers, for example :
  From: dinh@enseirb.fr becomes :
  header->name = "From:"
  header->body = "dinh@enseirb.fr"
 */
static gboolean header_is_addr_field(const gchar *hdr)
{
	static char *addr_headers[] = {
				"To:",
				"Cc:",
				"Bcc:",
				"From:",
				"Reply-To:",
				"Followup-To:",
				"Followup-and-Reply-To:",
				"Disposition-Notification-To:",
				"Return-Receipt-To:",
				NULL};
	int i;

	if (!hdr)
		return FALSE;

	for (i = 0; addr_headers[i] != NULL; i++)
		if (!strcasecmp(hdr, addr_headers[i]))
			return FALSE;

	return FALSE;
}

Header * procheader_parse_header(gchar * buf)
{
	gchar *p;
	Header * header;
	gboolean addr_field = FALSE;

	cm_return_val_if_fail(buf != NULL, NULL);

	if ((*buf == ':') || (*buf == ' '))
		return NULL;

	for (p = buf; *p ; p++) {
		if ((*p == ':') || (*p == ' ')) {
			header = g_new(Header, 1);
			header->name = g_strndup(buf, p - buf + 1);
			addr_field = header_is_addr_field(header->name);
			p++;
			while (*p == ' ' || *p == '\t') p++;
			header->body = conv_unmime_header(p, NULL, addr_field);
			return header;
		}
	}
	return NULL;
}

void procheader_get_header_fields(FILE *fp, HeaderEntry hentry[])
{
	gchar *buf = NULL;
	HeaderEntry *hp;
	gint hnum;
	gchar *p;

	if (hentry == NULL) return;

	while ((hnum = procheader_get_one_field(&buf, fp, hentry)) != -1) {
		hp = hentry + hnum;

		p = buf + strlen(hp->name);
		while (*p == ' ' || *p == '\t') p++;

		if (hp->body == NULL)
			hp->body = g_strdup(p);
		else if (procheader_headername_equal(hp->name, "To") ||
			 procheader_headername_equal(hp->name, "Cc")) {
			gchar *tp = hp->body;
			hp->body = g_strconcat(tp, ", ", p, NULL);
			g_free(tp);
		}
		g_free(buf);
		buf = NULL;
	}
}

MsgInfo *procheader_parse_file(const gchar *file, MsgFlags flags,
			       gboolean full, gboolean decrypted)
{
#ifdef G_OS_WIN32
	GFile *f;
	GFileInfo *fi;
	GTimeVal tv;
	GError *error = NULL;
#else
	GStatBuf s;
#endif
	FILE *fp;
	MsgInfo *msginfo;

#ifdef G_OS_WIN32
	f = g_file_new_for_path(file);
	fi = g_file_query_info(f, "standard::size,standard::type,time::modified",
			G_FILE_QUERY_INFO_NONE, NULL, &error);
	if (error != NULL) {
		g_warning(error->message);
		g_error_free(error);
		g_object_unref(f);
	}
#else
	if (g_stat(file, &s) < 0) {
		FILE_OP_ERROR(file, "stat");
		return NULL;
	}
#endif

#ifdef G_OS_WIN32
	if (g_file_info_get_file_type(fi) != G_FILE_TYPE_REGULAR) {
		g_object_unref(fi);
		g_object_unref(f);
		return NULL;
	}
#else
	if (!S_ISREG(s.st_mode))
		return NULL;
#endif

	if ((fp = claws_fopen(file, "rb")) == NULL) {
		FILE_OP_ERROR(file, "claws_fopen");
		return NULL;
	}

	msginfo = procheader_parse_stream(fp, flags, full, decrypted);
	claws_fclose(fp);

	if (msginfo) {
#ifdef G_OS_WIN32
		msginfo->size = g_file_info_get_size(fi);
		g_file_info_get_modification_time(fi, &tv);
		msginfo->mtime = tv.tv_sec;
#else
		msginfo->size = s.st_size;
		msginfo->mtime = s.st_mtime;
#endif
	}

#ifdef G_OS_WIN32
	g_object_unref(fi);
	g_object_unref(f);
#endif

	return msginfo;
}

MsgInfo *procheader_parse_str(const gchar *str, MsgFlags flags, gboolean full,
			      gboolean decrypted)
{
	return parse_stream(&str, TRUE, flags, full, decrypted);
}

enum
{
	H_DATE = 0,
	H_FROM,
	H_TO,
	H_CC,
	H_NEWSGROUPS,
	H_SUBJECT,
	H_MSG_ID,
	H_REFERENCES,
	H_IN_REPLY_TO,
	H_CONTENT_TYPE,
	H_SEEN,
	H_STATUS,
	H_FROM_SPACE,
	H_SC_PLANNED_DOWNLOAD,
	H_SC_MESSAGE_SIZE,
	H_FACE,
	H_X_FACE,
	H_DISPOSITION_NOTIFICATION_TO,
	H_RETURN_RECEIPT_TO,
	H_SC_PARTIALLY_RETRIEVED,
	H_SC_ACCOUNT_SERVER,
	H_SC_ACCOUNT_LOGIN,
	H_LIST_POST,
	H_LIST_SUBSCRIBE,
	H_LIST_UNSUBSCRIBE,
	H_LIST_HELP,
	H_LIST_ARCHIVE,
	H_LIST_OWNER,
	H_RESENT_FROM,
};

static HeaderEntry hentry_full[] = {
				   {"Date:",		NULL, FALSE},
				   {"From:",		NULL, TRUE},
				   {"To:",		NULL, TRUE},
				   {"Cc:",		NULL, TRUE},
				   {"Newsgroups:",	NULL, TRUE},
				   {"Subject:",		NULL, TRUE},
				   {"Message-ID:",	NULL, FALSE},
				   {"References:",	NULL, FALSE},
				   {"In-Reply-To:",	NULL, FALSE},
				   {"Content-Type:",	NULL, FALSE},
				   {"Seen:",		NULL, FALSE},
				   {"Status:",          NULL, FALSE},
				   {"From ",		NULL, FALSE},
				   {"SC-Marked-For-Download:", NULL, FALSE},
				   {"SC-Message-Size:", NULL, FALSE},
				   {"Face:",		NULL, FALSE},
				   {"X-Face:",		NULL, FALSE},
				   {"Disposition-Notification-To:", NULL, FALSE},
				   {"Return-Receipt-To:", NULL, FALSE},
				   {"SC-Partially-Retrieved:", NULL, FALSE},
				   {"SC-Account-Server:", NULL, FALSE},
				   {"SC-Account-Login:",NULL, FALSE},
				   {"List-Post:",	NULL, TRUE},
				   {"List-Subscribe:",	NULL, TRUE},
				   {"List-Unsubscribe:",NULL, TRUE},
				   {"List-Help:",	NULL, TRUE},
 				   {"List-Archive:",	NULL, TRUE},
 				   {"List-Owner:",	NULL, TRUE},
 				   {"Resent-From:",	NULL, TRUE},
				   {NULL,		NULL, FALSE}};

static HeaderEntry hentry_short[] = {
				    {"Date:",		NULL, FALSE},
				    {"From:",		NULL, TRUE},
				    {"To:",		NULL, TRUE},
				    {"Cc:",		NULL, TRUE},
				    {"Newsgroups:",	NULL, TRUE},
				    {"Subject:",	NULL, TRUE},
				    {"Message-ID:",	NULL, FALSE},
				    {"References:",	NULL, FALSE},
				    {"In-Reply-To:",	NULL, FALSE},
				    {"Content-Type:",	NULL, FALSE},
				    {"Seen:",		NULL, FALSE},
				    {"Status:",		NULL, FALSE},
				    {"From ",		NULL, FALSE},
				    {"SC-Marked-For-Download:", NULL, FALSE},
				    {"SC-Message-Size:",NULL, FALSE},
				    {NULL,		NULL, FALSE}};

static HeaderEntry* procheader_get_headernames(gboolean full)
{
	return full ? hentry_full : hentry_short;
}

MsgInfo *procheader_parse_stream(FILE *fp, MsgFlags flags, gboolean full,
				 gboolean decrypted)
{
	return parse_stream(fp, FALSE, flags, full, decrypted);
}

static gboolean avatar_from_some_face(gpointer source, gpointer userdata)
{
	AvatarCaptureData *acd = (AvatarCaptureData *)source;
	
	if (*(acd->content) == '\0') /* won't be null, but may be empty */
		return FALSE;

	if (!strcmp(acd->header, hentry_full[H_FACE].name)) {
		debug_print("avatar_from_some_face: found 'Face' header\n");
		procmsg_msginfo_add_avatar(acd->msginfo, AVATAR_FACE, acd->content);
	}
#if HAVE_LIBCOMPFACE
	else if (!strcmp(acd->header, hentry_full[H_X_FACE].name)) {
		debug_print("avatar_from_some_face: found 'X-Face' header\n");
		procmsg_msginfo_add_avatar(acd->msginfo, AVATAR_XFACE, acd->content);
	}
#endif
	return FALSE;
}

static gulong avatar_hook_id = HOOK_NONE;

static MsgInfo *parse_stream(void *data, gboolean isstring, MsgFlags flags,
			     gboolean full, gboolean decrypted)
{
	MsgInfo *msginfo;
	gchar *buf = NULL;
	gchar *p, *tmp;
	gchar *hp;
	HeaderEntry *hentry;
	gint hnum;
	void *orig_data = data;

	get_one_field_func get_one_field =
		isstring ? (get_one_field_func)string_get_one_field
			 : (get_one_field_func)procheader_get_one_field;

	hentry = procheader_get_headernames(full);

	if (MSG_IS_QUEUED(flags) || MSG_IS_DRAFT(flags)) {
		while (get_one_field(&buf, data, NULL) != -1) {
			if ((!strncmp(buf, "X-Claws-End-Special-Headers: 1",
				strlen("X-Claws-End-Special-Headers:"))) ||
			    (!strncmp(buf, "X-Sylpheed-End-Special-Headers: 1",
				strlen("X-Sylpheed-End-Special-Headers:")))) {
				g_free(buf);
				buf = NULL;
				break;
			}
			/* from other mailers */
			if (!strncmp(buf, "Date: ", 6)
			||  !strncmp(buf, "To: ", 4)
			||  !strncmp(buf, "From: ", 6)
			||  !strncmp(buf, "Subject: ", 9)) {
				if (isstring)
					data = orig_data;
				else 
					rewind((FILE *)data);
				g_free(buf);
				buf = NULL;
				break;
			}
			g_free(buf);
			buf = NULL;
		}
	}

	msginfo = procmsg_msginfo_new();
	
	if (flags.tmp_flags || flags.perm_flags) 
		msginfo->flags = flags;
	else 
		MSG_SET_PERM_FLAGS(msginfo->flags, MSG_NEW | MSG_UNREAD);
	
	msginfo->inreplyto = NULL;

	if (avatar_hook_id == HOOK_NONE &&
	    (prefs_common.enable_avatars && (AVATARS_ENABLE_CAPTURE || AVATARS_ENABLE_RENDER))) {
		avatar_hook_id = hooks_register_hook(AVATAR_HEADER_UPDATE_HOOKLIST,
						     avatar_from_some_face, NULL);
	} else if (avatar_hook_id != HOOK_NONE &&
		   !(prefs_common.enable_avatars && AVATARS_ENABLE_CAPTURE)) {
		hooks_unregister_hook(AVATAR_HEADER_UPDATE_HOOKLIST, avatar_hook_id);
		avatar_hook_id = HOOK_NONE;
	}

	while ((hnum = get_one_field(&buf, data, hentry)) != -1) {
		hp = buf + strlen(hentry[hnum].name);
		while (*hp == ' ' || *hp == '\t') hp++;

		switch (hnum) {
		case H_DATE:
			if (msginfo->date) break;
			msginfo->date_t =
				procheader_date_parse(NULL, hp, 0);
			if (g_utf8_validate(hp, -1, NULL)) {
				msginfo->date = g_strdup(hp);
			} else {
				gchar *utf = conv_codeset_strdup(
					hp, 
					conv_get_locale_charset_str_no_utf8(),
					CS_INTERNAL);
				if (utf == NULL || 
				    !g_utf8_validate(utf, -1, NULL)) {
					g_free(utf);
					utf = g_malloc(strlen(buf)*2+1);
					conv_localetodisp(utf, 
						strlen(hp)*2+1, hp);
				}
				msginfo->date = utf;
			}
			break;
		case H_FROM:
			if (msginfo->from) break;
			msginfo->from = conv_unmime_header(hp, NULL, TRUE);
			msginfo->fromname = procheader_get_fromname(msginfo->from);
			remove_return(msginfo->from);
			remove_return(msginfo->fromname);
			break;
		case H_TO:
			tmp = conv_unmime_header(hp, NULL, TRUE);
			remove_return(tmp);
			if (msginfo->to) {
				p = msginfo->to;
				msginfo->to =
					g_strconcat(p, ", ", tmp, NULL);
				g_free(p);
			} else
				msginfo->to = g_strdup(tmp);
                        g_free(tmp);                                
			break;
		case H_CC:
			tmp = conv_unmime_header(hp, NULL, TRUE);
			remove_return(tmp);
			if (msginfo->cc) {
				p = msginfo->cc;
				msginfo->cc =
					g_strconcat(p, ", ", tmp, NULL);
				g_free(p);
			} else
				msginfo->cc = g_strdup(tmp);
                        g_free(tmp);                                
			break;
		case H_NEWSGROUPS:
			if (msginfo->newsgroups) {
				p = msginfo->newsgroups;
				msginfo->newsgroups =
					g_strconcat(p, ",", hp, NULL);
				g_free(p);
			} else
				msginfo->newsgroups = g_strdup(hp);
			break;
		case H_SUBJECT:
			if (msginfo->subject) break;
			msginfo->subject = conv_unmime_header(hp, NULL, FALSE);
			unfold_line(msginfo->subject);
                       break;
		case H_MSG_ID:
			if (msginfo->msgid) break;

			extract_parenthesis(hp, '<', '>');
			remove_space(hp);
			msginfo->msgid = g_strdup(hp);
			break;
		case H_REFERENCES:
			msginfo->references =
				references_list_prepend(msginfo->references,
							hp);
			break;
		case H_IN_REPLY_TO:
			if (msginfo->inreplyto) break;

			eliminate_parenthesis(hp, '(', ')');
			if ((p = strrchr(hp, '<')) != NULL &&
			    strchr(p + 1, '>') != NULL) {
				extract_parenthesis(p, '<', '>');
				remove_space(p);
				if (*p != '\0')
					msginfo->inreplyto = g_strdup(p);
			}
			break;
		case H_CONTENT_TYPE:
			if (!g_ascii_strncasecmp(hp, "multipart/", 10))
				MSG_SET_TMP_FLAGS(msginfo->flags, MSG_MULTIPART);
			break;
		case H_DISPOSITION_NOTIFICATION_TO:
			if (!msginfo->extradata)
				msginfo->extradata = g_new0(MsgInfoExtraData, 1);
			if (msginfo->extradata->dispositionnotificationto) break;
			msginfo->extradata->dispositionnotificationto = g_strdup(hp);
			break;
		case H_RETURN_RECEIPT_TO:
			if (!msginfo->extradata)
				msginfo->extradata = g_new0(MsgInfoExtraData, 1);
			if (msginfo->extradata->returnreceiptto) break;
			msginfo->extradata->returnreceiptto = g_strdup(hp);
			break;
/* partial download infos */			
		case H_SC_PARTIALLY_RETRIEVED:
			if (!msginfo->extradata)
				msginfo->extradata = g_new0(MsgInfoExtraData, 1);
			if (msginfo->extradata->partial_recv) break;
			msginfo->extradata->partial_recv = g_strdup(hp);
			break;
		case H_SC_ACCOUNT_SERVER:
			if (!msginfo->extradata)
				msginfo->extradata = g_new0(MsgInfoExtraData, 1);
			if (msginfo->extradata->account_server) break;
			msginfo->extradata->account_server = g_strdup(hp);
			break;
		case H_SC_ACCOUNT_LOGIN:
			if (!msginfo->extradata)
				msginfo->extradata = g_new0(MsgInfoExtraData, 1);
			if (msginfo->extradata->account_login) break;
			msginfo->extradata->account_login = g_strdup(hp);
			break;
		case H_SC_MESSAGE_SIZE:
			if (msginfo->total_size) break;
			msginfo->total_size = atoi(hp);
			break;
		case H_SC_PLANNED_DOWNLOAD:
			msginfo->planned_download = atoi(hp);
			break;
/* end partial download infos */
		case H_FROM_SPACE:
			if (msginfo->fromspace) break;
			msginfo->fromspace = g_strdup(hp);
			remove_return(msginfo->fromspace);
			break;
/* list infos */
 		case H_LIST_POST:
			if (!msginfo->extradata)
				msginfo->extradata = g_new0(MsgInfoExtraData, 1);
			if (msginfo->extradata->list_post) break;
			msginfo->extradata->list_post = g_strdup(hp);
			break;
		case H_LIST_SUBSCRIBE:
			if (!msginfo->extradata)
				msginfo->extradata = g_new0(MsgInfoExtraData, 1);
			if (msginfo->extradata->list_subscribe) break;
			msginfo->extradata->list_subscribe = g_strdup(hp);
			break;
		case H_LIST_UNSUBSCRIBE:
			if (!msginfo->extradata)
				msginfo->extradata = g_new0(MsgInfoExtraData, 1);
			if (msginfo->extradata->list_unsubscribe) break;
			msginfo->extradata->list_unsubscribe = g_strdup(hp);
			break;
		case H_LIST_HELP:
			if (!msginfo->extradata)
				msginfo->extradata = g_new0(MsgInfoExtraData, 1);
			if (msginfo->extradata->list_help) break;
			msginfo->extradata->list_help = g_strdup(hp);
			break;
		case H_LIST_ARCHIVE:
			if (!msginfo->extradata)
				msginfo->extradata = g_new0(MsgInfoExtraData, 1);
			if (msginfo->extradata->list_archive) break;
			msginfo->extradata->list_archive = g_strdup(hp);
			break;
		case H_LIST_OWNER:
			if (!msginfo->extradata)
				msginfo->extradata = g_new0(MsgInfoExtraData, 1);
			if (msginfo->extradata->list_owner) break;
			msginfo->extradata->list_owner = g_strdup(hp);
			break;
		case H_RESENT_FROM:
			if (!msginfo->extradata)
				msginfo->extradata = g_new0(MsgInfoExtraData, 1);
			if (msginfo->extradata->resent_from) break;
			msginfo->extradata->resent_from = g_strdup(hp);
			break;
/* end list infos */
		default:
			break;
		}
		/* to avoid performance penalty hooklist is invoked only for
		   headers known to be able to generate avatars */
		if (hnum == H_FROM || hnum == H_X_FACE || hnum == H_FACE) {
			AvatarCaptureData *acd = g_new0(AvatarCaptureData, 1);
			/* no extra memory is wasted, hooks are expected to
			   take care of copying members when needed */
			acd->msginfo = msginfo;
			acd->header  = hentry_full[hnum].name;
			acd->content = hp;
			hooks_invoke(AVATAR_HEADER_UPDATE_HOOKLIST, (gpointer)acd);
			g_free(acd);
		}
		g_free(buf);
		buf = NULL;
	}

	if (!msginfo->inreplyto && msginfo->references)
		msginfo->inreplyto =
			g_strdup((gchar *)msginfo->references->data);

	return msginfo;
}

gchar *procheader_get_fromname(const gchar *str)
{
	gchar *tmp, *name;

	Xstrdup_a(tmp, str, return NULL);

	if (*tmp == '\"') {
		extract_quote(tmp, '\"');
		g_strstrip(tmp);
	} else if (strchr(tmp, '<')) {
		eliminate_parenthesis(tmp, '<', '>');
		g_strstrip(tmp);
		if (*tmp == '\0') {
			strcpy(tmp, str);
			extract_parenthesis(tmp, '<', '>');
			g_strstrip(tmp);
		}
	} else if (strchr(tmp, '(')) {
		extract_parenthesis(tmp, '(', ')');
		g_strstrip(tmp);
	}

	if (*tmp == '\0')
		name = g_strdup(str);
	else
		name = g_strdup(tmp);

	return name;
}

static gint procheader_remove_comment_in_date_string(gchar *o_str)
{
	gchar str[strlen(o_str)+1];
	int i, j = 0;
	int in_comment_nest_level = 0;
	gboolean flag_escape_backslash = FALSE;

	for (i=0; i < strlen(o_str); i++) {
		switch (o_str[i]) {
		case '(':
			in_comment_nest_level++;
			if (in_comment_nest_level > 16) {
				str[j] = '\0';
				return TRUE;
			}
			continue;
		case '\\':
			if (in_comment_nest_level > 0) {
				flag_escape_backslash = TRUE;
				continue;
			}
			break;
		case ')':
			if (flag_escape_backslash == TRUE) {
				flag_escape_backslash = FALSE;
				continue;
			}
			in_comment_nest_level--;
			if (in_comment_nest_level < 0) {
				str[j] = '\0';
				return TRUE;
			}
			continue;
		default:
			if (in_comment_nest_level > 0) {
				if (flag_escape_backslash == TRUE)
					flag_escape_backslash = FALSE;
				continue;
			}
			break;
		}
		str[j++] = o_str[i];
	}
	str[j] = '\0';
	strcpy(o_str, str);
	return TRUE;
}


static gint procheader_scan_date_string(const gchar *o_str,
					gchar *weekday, gint *day,
					gchar *month, gint *year,
					gint *hh, gint *mm, gint *ss,
					gchar *zone)
{
	gint result;
	gint month_n;
	gint secfract;
	gint zone1 = 0, zone2 = 0;
	gchar offset_sign, zonestr[7];
	gchar sep1;

	if (o_str == NULL)
		return -1;

	gchar str[strlen(o_str)+1];

	strcpy(str, o_str);
	if (strchr(str, '(') != NULL)
		procheader_remove_comment_in_date_string(str);

	result = sscanf(str, "%10s %d %9s %d %2d:%2d:%2d %6s",
			weekday, day, month, year, hh, mm, ss, zone);
	if (result == 8) return 0;

	/* RFC2822 */
	result = sscanf(str, "%3s,%d %9s %d %2d:%2d:%2d %6s",
			weekday, day, month, year, hh, mm, ss, zone);
	if (result == 8) return 0;

	result = sscanf(str, "%3s %3s %d %2d:%2d:%2d %d %6s",
			weekday, month, day, hh, mm, ss, year, zone);
	if (result == 8) return 0;

	result = sscanf(str, "%d %9s %d %2d:%2d:%2d %6s",
			day, month, year, hh, mm, ss, zone);
	if (result == 7) return 0;

	*zone = '\0';
	result = sscanf(str, "%10s %d %9s %d %2d:%2d:%2d",
			weekday, day, month, year, hh, mm, ss);
	if (result == 7) return 0;

	result = sscanf(str, "%3s %3s %d %2d:%2d:%2d %d",
			weekday, month, day, hh, mm, ss, year);
	if (result == 7) return 0;

	result = sscanf(str, "%d %9s %d %2d:%2d:%2d",
			day, month, year, hh, mm, ss);
	if (result == 6) return 0;

	*ss = 0;
	result = sscanf(str, "%10s %d %9s %d %2d:%2d %6s",
			weekday, day, month, year, hh, mm, zone);
	if (result == 7) return 0;

	result = sscanf(str, "%d %9s %d %2d:%2d %5s",
			day, month, year, hh, mm, zone);
	if (result == 6) return 0;

	*zone = '\0';
	result = sscanf(str, "%10s %d %9s %d %2d:%2d",
			weekday, day, month, year, hh, mm);
	if (result == 6) return 0;

	result = sscanf(str, "%d %9s %d %2d:%2d",
			day, month, year, hh, mm);
	if (result == 5) return 0;

	*weekday = '\0';

	/* RFC3339 subset, with fraction of second */
	result = sscanf(str, "%4d-%2d-%2d%c%2d:%2d:%2d.%d%6s",
			year, &month_n, day, &sep1, hh, mm, ss, &secfract, zonestr);
	if (result == 9
			&& (sep1 == 'T' || sep1 == 't' || sep1 == ' ')) {
		if (month_n >= 1 && month_n <= 12) {
			strncpy2(month, monthstr+((month_n-1)*3), 4);
			if (zonestr[0] == 'z' || zonestr[0] == 'Z') {
				strcat(zone, "+00:00");
			} else if (sscanf(zonestr, "%c%2d:%2d",
						&offset_sign, &zone1, &zone2) == 3) {
				strcat(zone, zonestr);
			}
			return 0;
		}
	}

	/* RFC3339 subset, no fraction of second */
	result = sscanf(str, "%4d-%2d-%2d%c%2d:%2d:%2d%6s",
			year, &month_n, day, &sep1, hh, mm, ss, zonestr);
	if (result == 8
			&& (sep1 == 'T' || sep1 == 't' || sep1 == ' ')) {
		if (month_n >= 1 && month_n <= 12) {
			strncpy2(month, monthstr+((month_n-1)*3), 4);
			if (zonestr[0] == 'z' || zonestr[0] == 'Z') {
				strcat(zone, "+00:00");
			} else if (sscanf(zonestr, "%c%2d:%2d",
						&offset_sign, &zone1, &zone2) == 3) {
				strcat(zone, zonestr);
			}
			return 0;
		}
	}

	*zone = '\0';

	/* RFC3339 subset, no fraction of second, and no timezone offset */
	/* This particular "subset" is invalid, RFC requires the offset */
	result = sscanf(str, "%4d-%2d-%2d %2d:%2d:%2d",
			year, &month_n, day, hh, mm, ss);
	if (result == 6) {
		if (1 <= month_n && month_n <= 12) {
			strncpy2(month, monthstr+((month_n-1)*3), 4);
			return 0;
		}
	}

	/* ISO8601 format with just date (YYYY-MM-DD) */
	result = sscanf(str, "%4d-%2d-%2d",
			year, &month_n, day);
	if (result == 3) {
		*hh = *mm = *ss = 0;
		if (1 <= month_n && month_n <= 12) {
			strncpy2(month, monthstr+((month_n-1)*3), 4);
			return 0;
		}
	}

	return -1;
}

/*
 * Hiro, most UNIXen support this function:
 * http://www.mcsr.olemiss.edu/cgi-bin/man-cgi?getdate
 */
gboolean procheader_date_parse_to_tm(const gchar *src, struct tm *t, char *zone)
{
	gchar weekday[11];
	gint day;
	gchar month[10];
	gint year;
	gint hh, mm, ss;
	GDateMonth dmonth;
	gchar *p;

	if (!t)
		return FALSE;
	
	memset(t, 0, sizeof *t);	

	if (procheader_scan_date_string(src, weekday, &day, month, &year,
					&hh, &mm, &ss, zone) < 0) {
		g_warning("invalid date: %s", src);
		return FALSE;
	}

	/* Y2K compliant :) */
	if (year < 100) {
		if (year < 70)
			year += 2000;
		else
			year += 1900;
	}

	month[3] = '\0';
	if ((p = strstr(monthstr, month)) != NULL)
		dmonth = (gint)(p - monthstr) / 3 + 1;
	else {
		g_warning("invalid month: %s", month);
		dmonth = G_DATE_BAD_MONTH;
	}

	t->tm_sec = ss;
	t->tm_min = mm;
	t->tm_hour = hh;
	t->tm_mday = day;
	t->tm_mon = dmonth - 1;
	t->tm_year = year - 1900;
	t->tm_wday = 0;
	t->tm_yday = 0;
	t->tm_isdst = -1;

	mktime(t);

	return TRUE;
}

time_t procheader_date_parse(gchar *dest, const gchar *src, gint len)
{
	gchar weekday[11];
	gint day;
	gchar month[10];
	gint year;
	gint hh, mm, ss;
	gchar zone[7];
	GDateMonth dmonth = G_DATE_BAD_MONTH;
	gchar *p;
	time_t timer;

	if (procheader_scan_date_string(src, weekday, &day, month, &year,
					&hh, &mm, &ss, zone) < 0) {
		if (dest && len > 0)
			strncpy2(dest, src, len);
		return 0;
	}

	month[3] = '\0';
	for (p = monthstr; *p != '\0'; p += 3) {
		if (!g_ascii_strncasecmp(p, month, 3)) {
			dmonth = (gint)(p - monthstr) / 3 + 1;
			break;
		}
	}

#ifdef G_OS_WIN32
	GTimeZone *tz;
	GDateTime *dt, *dt2;

#if GLIB_CHECK_VERSION(2,68,0)
	tz = g_time_zone_new_identifier(zone);
	if (tz == NULL)
		tz = g_time_zone_new_utc();
#else
	tz = g_time_zone_new(zone); // can't return NULL no need to check for it
#endif
	dt = g_date_time_new(tz, 1, 1, 1, 0, 0, 0);
	g_time_zone_unref(tz);
	dt2 = g_date_time_add_full(dt, year-1, dmonth-1, day-1, hh, mm, ss);
	g_date_time_unref(dt);

	timer = g_date_time_to_unix(dt2);
	g_date_time_unref(dt2);

#else
	struct tm t;
	time_t tz_offset;

	/* Y2K compliant :) */
	if (year < 1000) {
		if (year < 50)
			year += 2000;
		else
			year += 1900;
	}

	t.tm_sec = ss;
	t.tm_min = mm;
	t.tm_hour = hh;
	t.tm_mday = day;
	t.tm_mon = dmonth - 1;
	t.tm_year = year - 1900;
	t.tm_wday = 0;
	t.tm_yday = 0;
	t.tm_isdst = -1;

	timer = mktime(&t);
	tz_offset = remote_tzoffset_sec(zone);
	if (tz_offset != -1)
		timer += tzoffset_sec(&timer) - tz_offset;

#endif
	if (dest)
		procheader_date_get_localtime(dest, len, timer);

	return timer;
}

void procheader_date_get_localtime(gchar *dest, gint len, const time_t timer)
{
	struct tm *lt;
	gchar *default_format = "%y/%m/%d(%a) %H:%M";
	gchar *str;
	const gchar *src_codeset, *dest_codeset;
	struct tm buf;

	if (timer > 0)
		lt = localtime_r(&timer, &buf);
	else {
		time_t dummy = 1;
		lt = localtime_r(&dummy, &buf);
	}

	if (prefs_common.date_format)
		fast_strftime(dest, len, prefs_common.date_format, lt);
	else
		fast_strftime(dest, len, default_format, lt);

	if (!g_utf8_validate(dest, -1, NULL)) {
		src_codeset = conv_get_locale_charset_str_no_utf8();
		dest_codeset = CS_UTF_8;
		str = conv_codeset_strdup(dest, src_codeset, dest_codeset);
		if (str) {
			strncpy2(dest, str, len);
			g_free(str);
		}
	}
}

/* Added by Mel Hadasht on 27 Aug 2001 */
/* Get a header from msginfo */
gint procheader_get_header_from_msginfo(MsgInfo *msginfo, gchar **buf, gchar *header)
{
	gchar *file;
	FILE *fp;
	HeaderEntry hentry[]={ { NULL, NULL, TRUE  },
						   { NULL, NULL, FALSE } };
	gint val;

	cm_return_val_if_fail(msginfo != NULL, -1);
	cm_return_val_if_fail(buf != NULL, -1);
	cm_return_val_if_fail(header != NULL, -1);

	hentry[0].name = header;

	file = procmsg_get_message_file_path(msginfo);
	if ((fp = claws_fopen(file, "rb")) == NULL) {
		FILE_OP_ERROR(file, "claws_fopen");
		g_free(file);
		g_free(*buf);
		*buf = NULL;
		return -1;
	}
	val = procheader_get_one_field(buf, fp, hentry);

	if (claws_fclose(fp) == EOF) {
		FILE_OP_ERROR(file, "claws_fclose");
		claws_unlink(file);
		g_free(file);
		g_free(*buf);
		*buf = NULL;
		return -1;
	}

	g_free(file);
	if (val == -1) {
		/* *buf is already NULL in that case, see procheader_get_one_field() */
		return -1;
	}

	return 0;
}

HeaderEntry *procheader_entries_from_str(const gchar *str)
{
	HeaderEntry *entries = NULL, *he;
	int numh = 0, i = 0;
	gchar **names = NULL;
	const gchar *s = str;

	if (s == NULL) {
		return NULL;
	}
	while (*s != '\0') {
		if (*s == ' ') ++numh;
		++s;
	}
	if (numh == 0) {
		return NULL;
	}
	entries = g_new0(HeaderEntry, numh + 1); /* room for last NULL */
	s = str;
	++s; /* skip first space */
	names = g_strsplit(s, " ", numh);
	he = entries;
	while (names[i]) {
		he->name = g_strdup_printf("%s:", names[i]);
		he->body = NULL;
		he->unfold = FALSE;
		++i, ++he;
	}
	he->name = NULL;
	g_strfreev(names);
	return entries;
}

void procheader_entries_free (HeaderEntry *entries)
{
	if (entries != NULL) {
		HeaderEntry *he = entries;
		while (he->name != NULL) {
			g_free(he->name);
			if (he->body != NULL)
				g_free(he->body);
			++he;			
		}
		g_free(entries);
	}
}

gboolean procheader_header_is_internal(const gchar *hdr_name)
{
	const gchar *internal_hdrs[] = {
		"AF:", "NF:", "PS:", "SRH:", "SFN:", "DSR:", "MID:", "CFG:",
		"PT:", "S:", "RQ:", "SSV:", "NSV:", "SSH:", "R:", "MAID:",
		"SCF:", "RMID:", "FMID:", "NAID:",
		"X-Claws-Account-Id:",
		"X-Claws-Sign:",
		"X-Claws-Encrypt:",
		"X-Claws-Privacy-System:",
		"X-Claws-Auto-Wrapping:",
		"X-Claws-Auto-Indent:",
		"X-Claws-End-Special-Headers:",
		"X-Sylpheed-Account-Id:",
		"X-Sylpheed-Sign:",
		"X-Sylpheed-Encrypt:",
		"X-Sylpheed-Privacy-System:",
		"X-Sylpheed-End-Special-Headers:",
	         NULL
	};
	int i;

	for (i = 0; internal_hdrs[i]; i++) {
	        if (!strcmp(hdr_name, internal_hdrs[i]))
	                return TRUE;
	}
	return FALSE;
}
