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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "intl.h"
#include "procheader.h"
#include "procmsg.h"
#include "codeconv.h"
#include "prefs_common.h"
#include "utils.h"

#define BUFFSIZE	8192

gint procheader_get_one_field(gchar *buf, gint len, FILE *fp,
			      HeaderEntry hentry[])
{
	gint nexthead;
	gint hnum = 0;
	HeaderEntry *hp = NULL;

	if (hentry != NULL) {
		/* skip non-required headers */
		do {
			do {
				if (fgets(buf, len, fp) == NULL)
					return -1;
				if (buf[0] == '\r' || buf[0] == '\n')
					return -1;
			} while (buf[0] == ' ' || buf[0] == '\t');

			for (hp = hentry, hnum = 0; hp->name != NULL;
			     hp++, hnum++) {
				if (!strncasecmp(hp->name, buf,
						 strlen(hp->name))) {
					break;
				}
			}
		} while (hp->name == NULL);
	} else {
		if (fgets(buf, len, fp) == NULL) return -1;
		if (buf[0] == '\r' || buf[0] == '\n') return -1;
	}

	/* unfold the specified folded line */
	if (hp && hp->unfold) {
		gboolean folded = FALSE;
		gchar *bufp = buf + strlen(buf);

		while (1) {
			nexthead = fgetc(fp);

			/* folded */
			if (nexthead == ' ' || nexthead == '\t')
				folded = TRUE;
			else if (nexthead == EOF)
				break;
			else if (folded == TRUE) {
				/* concatenate next line */
				if ((len - (bufp - buf)) <= 2) break;

				/* replace return code on the tail end
				   with space */
				*(bufp - 1) = ' ';
				*bufp++ = nexthead;
				*bufp = '\0';
				if (nexthead == '\r' || nexthead == '\n') {
					folded = FALSE;
					continue;
				}
				if (fgets(bufp, len - (bufp - buf), fp)
				    == NULL) break;
				bufp += strlen(bufp);

				folded = FALSE;
			} else {
				ungetc(nexthead, fp);
				break;
			}
		}

		/* remove trailing return code */
		strretchomp(buf);

		return hnum;
	}

	while (1) {
		nexthead = fgetc(fp);
		if (nexthead == ' ' || nexthead == '\t') {
			size_t buflen = strlen(buf);

			/* concatenate next line */
			if ((len - buflen) > 2) {
				gchar *p = buf + buflen;

				*p++ = nexthead;
				*p = '\0';
				buflen++;
				if (fgets(p, len - buflen, fp) == NULL)
					break;
			} else
				break;
		} else {
			if (nexthead != EOF)
				ungetc(nexthead, fp);
			break;
		}
	}

	/* remove trailing return code */
	strretchomp(buf);

	return hnum;
}

gchar *procheader_get_unfolded_line(gchar *buf, gint len, FILE *fp)
{
	gboolean folded = FALSE;
	gint nexthead;
	gchar *bufp;

	if (fgets(buf, len, fp) == NULL) return NULL;
	if (buf[0] == '\r' || buf[0] == '\n') return NULL;
	bufp = buf + strlen(buf);

	while (1) {
		nexthead = fgetc(fp);

		/* folded */
		if (nexthead == ' ' || nexthead == '\t')
			folded = TRUE;
		else if (nexthead == EOF)
			break;
		else if (folded == TRUE) {
			/* concatenate next line */
			if ((len - (bufp - buf)) <= 2) break;

			/* replace return code on the tail end
			   with space */
			*(bufp - 1) = ' ';
			*bufp++ = nexthead;
			*bufp = '\0';
			if (nexthead == '\r' || nexthead == '\n') {
				folded = FALSE;
				continue;
			}
			if (fgets(bufp, len - (bufp - buf), fp)
			    == NULL) break;
			bufp += strlen(bufp);

			folded = FALSE;
		} else {
			ungetc(nexthead, fp);
			break;
		}
	}

	/* remove trailing return code */
	strretchomp(buf);

	return buf;
}

GSList *procheader_get_header_list_from_file(const gchar *file)
{
	FILE *fp;
	GSList *hlist;

	if ((fp = fopen(file, "rb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		return NULL;
	}

	hlist = procheader_get_header_list(fp);

	fclose(fp);
	return hlist;
}

GSList *procheader_get_header_list(FILE *fp)
{
	gchar buf[BUFFSIZE], tmp[BUFFSIZE];
	gchar *p;
	GSList *hlist = NULL;
	Header *header;

	g_return_val_if_fail(fp != NULL, NULL);

	while (procheader_get_unfolded_line(buf, sizeof(buf), fp) != NULL) {
		if (header = procheader_parse_header(buf))
			hlist = g_slist_append(hlist, header);
		/*
		if (*buf == ':') continue;
		for (p = buf; *p && *p != ' '; p++) {
			if (*p == ':') {
				header = g_new(Header, 1);
				header->name = g_strndup(buf, p - buf);
				p++;
				while (*p == ' ' || *p == '\t') p++;
				conv_unmime_header(tmp, sizeof(tmp), p, NULL);
				header->body = g_strdup(tmp);

				hlist = g_slist_append(hlist, header);
				break;
			}
		}
		*/
	}

	return hlist;
}

GPtrArray *procheader_get_header_array(FILE *fp)
{
	gchar buf[BUFFSIZE], tmp[BUFFSIZE];
	gchar *p;
	GPtrArray *headers;
	Header *header;

	g_return_val_if_fail(fp != NULL, NULL);

	headers = g_ptr_array_new();

	while (procheader_get_unfolded_line(buf, sizeof(buf), fp) != NULL) {
		if (header = procheader_parse_header(buf))
			g_ptr_array_add(headers, header);
		/*
		if (*buf == ':') continue;
		for (p = buf; *p && *p != ' '; p++) {
			if (*p == ':') {
				header = g_new(Header, 1);
				header->name = g_strndup(buf, p - buf);
				p++;
				while (*p == ' ' || *p == '\t') p++;
				conv_unmime_header(tmp, sizeof(tmp), p, NULL);
				header->body = g_strdup(tmp);

				g_ptr_array_add(headers, header);
				break;
			}
		}
		*/
	}

	return headers;
}

GPtrArray *procheader_get_header_array_asis(FILE *fp)
{
	gchar buf[BUFFSIZE], tmp[BUFFSIZE];
	gchar *p;
	GPtrArray *headers;
	Header *header;

	g_return_val_if_fail(fp != NULL, NULL);

	headers = g_ptr_array_new();

	while (procheader_get_one_field(buf, sizeof(buf), fp, NULL) != -1) {
		if (header = procheader_parse_header(buf))
			g_ptr_array_add(headers, header);
			/*
		if (*buf == ':') continue;
		for (p = buf; *p && *p != ' '; p++) {
			if (*p == ':') {
				header = g_new(Header, 1);
				header->name = g_strndup(buf, p - buf);
				p++;
				conv_unmime_header(tmp, sizeof(tmp), p, NULL);
				header->body = g_strdup(tmp);

				g_ptr_array_add(headers, header);
				break;
			}
		}
			*/
	}

	return headers;
}

void procheader_header_list_destroy(GSList *hlist)
{
	Header *header;

	while (hlist != NULL) {
		header = hlist->data;
		procheader_header_free(header);
		hlist = g_slist_remove(hlist, header);
	}
}

void procheader_header_array_destroy(GPtrArray *harray)
{
	gint i;
	Header *header;

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

	return (g_strncasecmp(hdr1, hdr2, len1) == 0);
}

/*
  parse headers, for example :
  From: dinh@enseirb.fr becomes :
  header->name = "From:"
  header->body = "dinh@enseirb.fr"
 */

Header * procheader_parse_header(gchar * buf)
{
	gchar tmp[BUFFSIZE];
	gchar *p = buf;
	Header * header;

	if ((*buf == ':') || (*buf == ' '))
		return NULL;

	for (p = buf; *p ; p++) {
		if ((*p == ':') || (*p == ' ')) {
			header = g_new(Header, 1);
			header->name = g_strndup(buf, p - buf + 1);
			p++;
			while (*p == ' ' || *p == '\t') p++;
			conv_unmime_header(tmp, sizeof(tmp), p, NULL);
			header->body = g_strdup(tmp);
			return header;
		}
	}
	return NULL;
}

void procheader_get_header_fields(FILE *fp, HeaderEntry hentry[])
{
	gchar buf[BUFFSIZE];
	HeaderEntry *hp;
	gint hnum;
	gchar *p;

	if (hentry == NULL) return;

	while ((hnum = procheader_get_one_field(buf, sizeof(buf), fp, hentry))
	       != -1) {
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
	}
}

MsgInfo *procheader_parse_file(const gchar *file, MsgFlags flags,
			       gboolean full, gboolean decrypted)
{
	FILE *fp;
	MsgInfo *msginfo;

	if ((fp = fopen(file, "rb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		return NULL;
	}

	msginfo = procheader_parse_stream(fp, flags, full, decrypted);
	fclose(fp);
	return msginfo;
}

MsgInfo *procheader_parse_str(const gchar *str, MsgFlags flags, gboolean full,
			      gboolean decrypted)
{
	FILE *fp;
	MsgInfo *msginfo;

	if ((fp = str_open_as_stream(str)) == NULL)
		return NULL;

	msginfo = procheader_parse_stream(fp, flags, full, decrypted);
	fclose(fp);
	return msginfo;
}

enum
{
	H_DATE		= 0,
	H_FROM		= 1,
	H_TO		= 2,
	H_CC		= 3,
	H_NEWSGROUPS	= 4,
	H_SUBJECT	= 5,
	H_MSG_ID	= 6,
	H_REFERENCES	= 7,
	H_IN_REPLY_TO	= 8,
	H_CONTENT_TYPE	= 9,
	H_SEEN		= 10,
	H_STATUS        = 11,
	H_X_STATUS      = 12,
	H_FROM_SPACE	= 13,
	H_X_FACE	= 14,
	H_DISPOSITION_NOTIFICATION_TO = 15,
	H_RETURN_RECEIPT_TO = 16
};

MsgInfo *procheader_parse_stream(FILE *fp, MsgFlags flags, gboolean full, 
				 gboolean decrypted)
{
	static HeaderEntry hentry_full[] = {{"Date:",		NULL, FALSE},
					   {"From:",		NULL, TRUE},
					   {"To:",		NULL, TRUE},
					   {"Cc:",		NULL, TRUE},
					   {"Newsgroups:",	NULL, TRUE},
					   {"Subject:",		NULL, TRUE},
					   {"Message-Id:",	NULL, FALSE},
					   {"References:",	NULL, FALSE},
					   {"In-Reply-To:",	NULL, FALSE},
					   {"Content-Type:",	NULL, FALSE},
					   {"Seen:",		NULL, FALSE},
					   {"Status:",          NULL, FALSE},
					   {"X-Status:",        NULL, FALSE},
					   {"From ",		NULL, FALSE},
					   {"X-Face:",		NULL, FALSE},
					   {"Disposition-Notification-To:", NULL, FALSE},
					   {"Return-Receipt-To:", NULL, FALSE},
					   {NULL,		NULL, FALSE}};

	static HeaderEntry hentry_short[] = {{"Date:",		NULL, FALSE},
					    {"From:",		NULL, TRUE},
					    {"To:",		NULL, TRUE},
					    {"Cc:",		NULL, TRUE},
					    {"Newsgroups:",	NULL, TRUE},
					    {"Subject:",	NULL, TRUE},
					    {"Message-Id:",	NULL, FALSE},
					    {"References:",	NULL, FALSE},
					    {"In-Reply-To:",	NULL, FALSE},
					    {"Content-Type:",	NULL, FALSE},
					    {"Seen:",		NULL, FALSE},
					    {"Status:",		NULL, FALSE},
					    {"X-Status:",	NULL, FALSE},
					    {"From ",		NULL, FALSE},
					    {NULL,		NULL, FALSE}};

	MsgInfo *msginfo;
	gchar buf[BUFFSIZE], tmp[BUFFSIZE];
	gchar *reference = NULL;
	gchar *p;
	gchar *hp;
	HeaderEntry *hentry;
	gint hnum;

	hentry = full ? hentry_full : hentry_short;

	if (MSG_IS_QUEUED(flags)) {
		while (fgets(buf, sizeof(buf), fp) != NULL)
			if (buf[0] == '\r' || buf[0] == '\n') break;
	}

	msginfo = procmsg_msginfo_new();
	
	if (flags.tmp_flags || flags.perm_flags) 
		msginfo->flags = flags;
	else 
		MSG_SET_PERM_FLAGS(msginfo->flags, MSG_NEW | MSG_UNREAD);
	
	if (decrypted)
		MSG_UNSET_TMP_FLAGS(msginfo->flags, MSG_MIME);

	msginfo->inreplyto = NULL;

	while ((hnum = procheader_get_one_field(buf, sizeof(buf), fp, hentry))
	       != -1) {
		hp = buf + strlen(hentry[hnum].name);
		while (*hp == ' ' || *hp == '\t') hp++;

		switch (hnum) {
		case H_DATE:
			if (msginfo->date) break;
			msginfo->date_t =
				procheader_date_parse(NULL, hp, 0);
			msginfo->date = g_strdup(hp);
			break;
		case H_FROM:
			if (msginfo->from) break;
			conv_unmime_header(tmp, sizeof(tmp), hp, NULL);
			msginfo->from = g_strdup(tmp);
			msginfo->fromname = procheader_get_fromname(tmp);
			break;
		case H_TO:
			conv_unmime_header(tmp, sizeof(tmp), hp, NULL);
			if (msginfo->to) {
				p = msginfo->to;
				msginfo->to =
					g_strconcat(p, ", ", tmp, NULL);
				g_free(p);
			} else
				msginfo->to = g_strdup(tmp);
			break;
		case H_CC:
			conv_unmime_header(tmp, sizeof(tmp), hp, NULL);
			if (msginfo->cc) {
				p = msginfo->cc;
				msginfo->cc =
					g_strconcat(p, ", ", tmp, NULL);
				g_free(p);
			} else
				msginfo->cc = g_strdup(tmp);
			break;
		case H_NEWSGROUPS:
			if (msginfo->newsgroups) {
				p = msginfo->newsgroups;
				msginfo->newsgroups =
					g_strconcat(p, ",", hp, NULL);
				g_free(p);
			} else
				msginfo->newsgroups = g_strdup(buf + 12);
			break;
		case H_SUBJECT:
			if (msginfo->subject) break;
			conv_unmime_header(tmp, sizeof(tmp), hp, NULL);
			msginfo->subject = g_strdup(tmp);
			break;
		case H_MSG_ID:
			if (msginfo->msgid) break;

			extract_parenthesis(hp, '<', '>');
			remove_space(hp);
			msginfo->msgid = g_strdup(hp);
			break;
		case H_REFERENCES:
		case H_IN_REPLY_TO:
			if (!reference) {
				msginfo->references = g_strdup(hp);
				eliminate_parenthesis(hp, '(', ')');
				if ((p = strrchr(hp, '<')) != NULL &&
				    strchr(p + 1, '>') != NULL) {
					extract_parenthesis(p, '<', '>');
					remove_space(p);
					if (*p != '\0')
						reference = g_strdup(p);
				}
			}
			break;
		case H_CONTENT_TYPE:
			if (decrypted) {
				if (!strncasecmp(hp, "multipart", 9) &&
				    strncasecmp(hp, "multipart/signed", 16)) {
					MSG_SET_TMP_FLAGS(msginfo->flags,
							  MSG_MIME);
				}
			}
			else if (!strncasecmp(hp, "multipart/encrypted", 19)) {
				MSG_SET_TMP_FLAGS(msginfo->flags,
						  MSG_ENCRYPTED);
			}
			else if (!strncasecmp(hp, "multipart", 9))
				MSG_SET_TMP_FLAGS(msginfo->flags, MSG_MIME);
			break;
#ifdef ALLOW_HEADER_HINT			
		case H_SEEN:
			/* mnews Seen header */
			MSG_UNSET_PERM_FLAGS(msginfo->flags, MSG_NEW|MSG_UNREAD);
			break;
#endif			
		case H_X_FACE:
			if (msginfo->xface) break;
			msginfo->xface = g_strdup(hp);
			break;
		case H_DISPOSITION_NOTIFICATION_TO:
			if (msginfo->dispositionnotificationto) break;
			msginfo->dispositionnotificationto = g_strdup(hp);
			MSG_SET_PERM_FLAGS(msginfo->flags, MSG_RETRCPT_PENDING);	
			break;
		case H_RETURN_RECEIPT_TO:
			if (msginfo->returnreceiptto) break;
			msginfo->returnreceiptto = g_strdup(hp);
			MSG_SET_PERM_FLAGS(msginfo->flags, MSG_RETRCPT_PENDING);	
			break;
#ifdef ALLOW_HEADER_HINT			
		case H_STATUS:
			if (strchr(hp, 'R') != NULL)
				MSG_UNSET_PERM_FLAGS(msginfo->flags, MSG_UNREAD);
			if (strchr(hp, 'O') != NULL)
				MSG_UNSET_PERM_FLAGS(msginfo->flags, MSG_NEW);
			if (strchr(hp, 'U') != NULL)
				MSG_SET_PERM_FLAGS(msginfo->flags, MSG_UNREAD);
			break;
		case H_X_STATUS:
			if (strchr(hp, 'D') != NULL)
				MSG_SET_PERM_FLAGS(msginfo->flags,
					      MSG_REALLY_DELETED);
			if (strchr(hp, 'F') != NULL)
				MSG_SET_PERM_FLAGS(msginfo->flags, MSG_MARKED);
			if (strchr(hp, 'd') != NULL)
				MSG_SET_PERM_FLAGS(msginfo->flags, MSG_DELETED);
			if (strchr(hp, 'r') != NULL)
				MSG_SET_PERM_FLAGS(msginfo->flags, MSG_REPLIED);
			if (strchr(hp, 'f') != NULL)
				MSG_SET_PERM_FLAGS(msginfo->flags, MSG_FORWARDED);
			break;
#endif			
		case H_FROM_SPACE:
			if (msginfo->fromspace) break;
			msginfo->fromspace = g_strdup(hp);
			break;
		default:
			break;
		}
	}
	msginfo->inreplyto = reference;

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

static gint procheader_scan_date_string(const gchar *str,
					gchar *weekday, gint *day,
					gchar *month, gint *year,
					gint *hh, gint *mm, gint *ss,
					gchar *zone)
{
	gint result;

	result = sscanf(str, "%10s %d %9s %d %2d:%2d:%2d %5s",
			weekday, day, month, year, hh, mm, ss, zone);
	if (result == 8) return 0;

	result = sscanf(str, "%3s,%d %9s %d %2d:%2d:%2d %5s",
			weekday, day, month, year, hh, mm, ss, zone);
	if (result == 8) return 0;

	result = sscanf(str, "%d %9s %d %2d:%2d:%2d %5s",
			day, month, year, hh, mm, ss, zone);
	if (result == 7) return 0;

	*ss = 0;
	result = sscanf(str, "%10s %d %9s %d %2d:%2d %5s",
			weekday, day, month, year, hh, mm, zone);
	if (result == 7) return 0;

	result = sscanf(str, "%d %9s %d %2d:%2d %5s",
			day, month, year, hh, mm, zone);
	if (result == 6) return 0;

	return -1;
}

/*
 * Hiro, most UNIXen support this function:
 * http://www.mcsr.olemiss.edu/cgi-bin/man-cgi?getdate
 */
gboolean procheader_date_parse_to_tm(const gchar *src, struct tm *t, char *zone)
{
	static gchar monthstr[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
	gchar weekday[11];
	gint day;
	gchar month[10];
	gint year;
	gint hh, mm, ss;
	GDateMonth dmonth;
	gchar *p;
	time_t timer;

	if (!t)
		return FALSE;
	
	memset(t, 0, sizeof *t);	

	if (procheader_scan_date_string(src, weekday, &day, month, &year,
					&hh, &mm, &ss, zone) < 0) {
		g_warning("Invalid date: %s\n", src);
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
		g_warning("Invalid month: %s\n", month);
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
	static gchar monthstr[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
	gchar weekday[11];
	gint day;
	gchar month[10];
	gint year;
	gint hh, mm, ss;
	gchar zone[6];
	GDateMonth dmonth;
	struct tm t;
	gchar *p;
	time_t timer;

	if (procheader_scan_date_string(src, weekday, &day, month, &year,
					&hh, &mm, &ss, zone) < 0) {
		g_warning("Invalid date: %s\n", src);
		if (dest && len > 0)
			strncpy2(dest, src, len);
		return 0;
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
		g_warning("Invalid month: %s\n", month);
		dmonth = G_DATE_BAD_MONTH;
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
	timer += tzoffset_sec(&timer) - remote_tzoffset_sec(zone);

	if (dest)
		procheader_date_get_localtime(dest, len, timer);

	return timer;
}

void procheader_date_get_localtime(gchar *dest, gint len, const time_t timer)
{
	struct tm *lt;
	gchar *default_format = "%y/%m/%d(%a) %H:%M";

	lt = localtime(&timer);

	if (prefs_common.date_format)
		strftime(dest, len, prefs_common.date_format, lt);
	else
		strftime(dest, len, default_format, lt);
}

gint get_header_from_msginfo(MsgInfo *msginfo, gchar *buf, gint len,gchar *header)
{
       gchar *file;
       FILE *fp;
       HeaderEntry hentry[]={{header,NULL,TRUE},
                                    {NULL,NULL,FALSE}};
       gint val;
       g_return_if_fail(msginfo != NULL);
       file = procmsg_get_message_file_path(msginfo);
       if ((fp = fopen(file, "rb")) == NULL) {
               FILE_OP_ERROR(file, "fopen");
               g_free(file);
               return;
        }
       val=procheader_get_one_field(buf,len, fp, hentry);
     	if (fclose(fp) == EOF) {
		FILE_OP_ERROR(file, "fclose");
		unlink(file);
		return -1;
	}
       if (val == -1)
          return -1;
       return 0;
}
