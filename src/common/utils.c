/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2021 The Claws Mail Team and Hiroyuki Yamamoto
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
 * The code of the g_utf8_substring function below is owned by
 * Matthias Clasen <matthiasc@src.gnome.org>/<mclasen@redhat.com>
 * and is got from GLIB 2.30: https://git.gnome.org/browse/glib/commit/
 *  ?h=glib-2-30&id=9eb65dd3ed5e1a9638595cbe10699c7606376511
 *
 * GLib 2.30 is licensed under GPL v2 or later and:
 * Copyright (C) 1999 Tom Tromey
 * Copyright (C) 2000 Red Hat, Inc.
 *
 * https://git.gnome.org/browse/glib/tree/glib/gutf8.c
 *  ?h=glib-2-30&id=9eb65dd3ed5e1a9638595cbe10699c7606376511
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#include "claws-features.h"
#endif

#include "defs.h"

#include <glib.h>
#include <gio/gio.h>

#include <glib/gi18n.h>

#ifdef USE_PTHREAD
#include <pthread.h>
#endif

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <sys/param.h>
#ifdef G_OS_WIN32
#  include <ws2tcpip.h>
#else
#  include <sys/socket.h>
#endif

#if (HAVE_WCTYPE_H && HAVE_WCHAR_H)
#  include <wchar.h>
#  include <wctype.h>
#endif
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>
#if HAVE_SYS_WAIT_H
#  include <sys/wait.h>
#endif
#include <dirent.h>
#include <time.h>
#include <regex.h>

#ifdef G_OS_UNIX
#include <sys/utsname.h>
#endif

#include <fcntl.h>

#ifdef G_OS_WIN32
#  include <direct.h>
#  include <io.h>
#endif

#include "utils.h"
#include "socket.h"
#include "codeconv.h"
#include "tlds.h"
#include "timing.h"
#include "file-utils.h"

#define BUFFSIZE	8192

static gboolean debug_mode = FALSE;

void list_free_strings_full(GList *list)
{
	g_list_free_full(list, (GDestroyNotify)g_free);
}

void slist_free_strings_full(GSList *list)
{
	g_slist_free_full(list, (GDestroyNotify)g_free);
}

static void hash_free_strings_func(gpointer key, gpointer value, gpointer data)
{
	g_free(key);
}

void hash_free_strings(GHashTable *table)
{
	g_hash_table_foreach(table, hash_free_strings_func, NULL);
}

gint str_case_equal(gconstpointer v, gconstpointer v2)
{
	return g_ascii_strcasecmp((const gchar *)v, (const gchar *)v2) == 0;
}

guint str_case_hash(gconstpointer key)
{
	const gchar *p = key;
	guint h = *p;

	if (h) {
		h = g_ascii_tolower(h);
		for (p += 1; *p != '\0'; p++)
			h = (h << 5) - h + g_ascii_tolower(*p);
	}

	return h;
}

gint to_number(const gchar *nstr)
{
	register const gchar *p;

	if (*nstr == '\0') return -1;

	for (p = nstr; *p != '\0'; p++)
		if (!g_ascii_isdigit(*p)) return -1;

	return atoi(nstr);
}

/* convert integer into string,
   nstr must be not lower than 11 characters length */
gchar *itos_buf(gchar *nstr, gint n)
{
	g_snprintf(nstr, 11, "%d", n);
	return nstr;
}

/* convert integer into string */
gchar *itos(gint n)
{
	static gchar nstr[11];

	return itos_buf(nstr, n);
}

#define divide(num,divisor,i,d)		\
{					\
	i = num >> divisor;		\
	d = num & ((1<<divisor)-1);	\
	d = (d*100) >> divisor;		\
}


/*!
 * \brief Convert a given size in bytes in a human-readable string
 *
 * \param size  The size expressed in bytes to convert in string
 * \return      The string that respresents the size in an human-readable way
 */
gchar *to_human_readable(goffset size)
{
	static gchar str[14];
	static gchar *b_format = NULL, *kb_format = NULL,
		     *mb_format = NULL, *gb_format = NULL;
	register int t = 0, r = 0;
	if (b_format == NULL) {
		b_format  = _("%dB");
		kb_format = _("%d.%02dKiB");
		mb_format = _("%d.%02dMiB");
		gb_format = _("%.2fGiB");
	}

	if (size < (goffset)1024) {
		g_snprintf(str, sizeof(str), b_format, (gint)size);
		return str;
	} else if (size >> 10 < (goffset)1024) {
		divide(size, 10, t, r);
		g_snprintf(str, sizeof(str), kb_format, t, r);
		return str;
	} else if (size >> 20 < (goffset)1024) {
		divide(size, 20, t, r);
		g_snprintf(str, sizeof(str), mb_format, t, r);
		return str;
	} else {
		g_snprintf(str, sizeof(str), gb_format, (gfloat)(size >> 30));
		return str;
	}
}

/* compare paths */
gint path_cmp(const gchar *s1, const gchar *s2)
{
	gint len1, len2;
	int rc;
#ifdef G_OS_WIN32
	gchar *s1buf, *s2buf;
#endif

	if (s1 == NULL || s2 == NULL) return -1;
	if (*s1 == '\0' || *s2 == '\0') return -1;

#ifdef G_OS_WIN32
	s1buf = g_strdup (s1);
	s2buf = g_strdup (s2);
	subst_char (s1buf, '/', G_DIR_SEPARATOR);
	subst_char (s2buf, '/', G_DIR_SEPARATOR);
	s1 = s1buf;
	s2 = s2buf;
#endif /* !G_OS_WIN32 */

	len1 = strlen(s1);
	len2 = strlen(s2);

	if (s1[len1 - 1] == G_DIR_SEPARATOR) len1--;
	if (s2[len2 - 1] == G_DIR_SEPARATOR) len2--;

	rc = strncmp(s1, s2, MAX(len1, len2));
#ifdef G_OS_WIN32
	g_free (s1buf);
	g_free (s2buf);
#endif /* !G_OS_WIN32 */
	return rc;
}

/* remove trailing return code */
gchar *strretchomp(gchar *str)
{
	register gchar *s;

	if (!*str) return str;

	for (s = str + strlen(str) - 1;
	     s >= str && (*s == '\n' || *s == '\r');
	     s--)
		*s = '\0';

	return str;
}

/* remove trailing character */
gchar *strtailchomp(gchar *str, gchar tail_char)
{
	register gchar *s;

	if (!*str) return str;
	if (tail_char == '\0') return str;

	for (s = str + strlen(str) - 1; s >= str && *s == tail_char; s--)
		*s = '\0';

	return str;
}

/* remove CR (carriage return) */
gchar *strcrchomp(gchar *str)
{
	register gchar *s;

	if (!*str) return str;

	s = str + strlen(str) - 1;
	if (*s == '\n' && s > str && *(s - 1) == '\r') {
		*(s - 1) = '\n';
		*s = '\0';
	}

	return str;
}

/* truncates string at first CR (carriage return) or LF (line feed) */
gchar *strcrlftrunc(gchar *str)
{
	gchar *p = NULL;

	if ((str == NULL) || (!*str)) return str;

	if ((p = strstr(str, "\r")) != NULL)
		*p = '\0';
	if ((p = strstr(str, "\n")) != NULL)
		*p = '\0';

	return str;
}

#ifndef HAVE_STRCASESTR
/* Similar to `strstr' but this function ignores the case of both strings.  */
gchar *strcasestr(const gchar *haystack, const gchar *needle)
{
	size_t haystack_len = strlen(haystack);

	return strncasestr(haystack, haystack_len, needle);
}
#endif /* HAVE_STRCASESTR */

gchar *strncasestr(const gchar *haystack, gint haystack_len, const gchar *needle)
{
	register size_t needle_len;

	needle_len   = strlen(needle);

	if (haystack_len < needle_len || needle_len == 0)
		return NULL;

	while (haystack_len >= needle_len) {
		if (!g_ascii_strncasecmp(haystack, needle, needle_len))
			return (gchar *)haystack;
		else {
			haystack++;
			haystack_len--;
		}
	}

	return NULL;
}

gpointer my_memmem(gconstpointer haystack, size_t haystacklen,
		   gconstpointer needle, size_t needlelen)
{
	const gchar *haystack_ = (const gchar *)haystack;
	const gchar *needle_ = (const gchar *)needle;
	const gchar *haystack_cur = (const gchar *)haystack;
	size_t haystack_left = haystacklen;

	if (needlelen == 1)
		return memchr(haystack_, *needle_, haystacklen);

	while ((haystack_cur = memchr(haystack_cur, *needle_, haystack_left))
	       != NULL) {
		if (haystacklen - (haystack_cur - haystack_) < needlelen)
			break;
		if (memcmp(haystack_cur + 1, needle_ + 1, needlelen - 1) == 0)
			return (gpointer)haystack_cur;
		else{
			haystack_cur++;
			haystack_left = haystacklen - (haystack_cur - haystack_);
		}
	}

	return NULL;
}

/* Copy no more than N characters of SRC to DEST, with NULL terminating.  */
gchar *strncpy2(gchar *dest, const gchar *src, size_t n)
{
	register const gchar *s = src;
	register gchar *d = dest;

	while (--n && *s)
		*d++ = *s++;
	*d = '\0';

	return dest;
}


/* Examine if next block is non-ASCII string */
gboolean is_next_nonascii(const gchar *s)
{
	const gchar *p;

	/* skip head space */
	for (p = s; *p != '\0' && g_ascii_isspace(*p); p++)
		;
	for (; *p != '\0' && !g_ascii_isspace(*p); p++) {
		if (*(guchar *)p > 127 || *(guchar *)p < 32)
			return TRUE;
	}

	return FALSE;
}

gint get_next_word_len(const gchar *s)
{
	gint len = 0;

	for (; *s != '\0' && !g_ascii_isspace(*s); s++, len++)
		;

	return len;
}

static void trim_subject_for_compare(gchar *str)
{
	gchar *srcp;

	eliminate_parenthesis(str, '[', ']');
	eliminate_parenthesis(str, '(', ')');
	g_strstrip(str);

	srcp = str + subject_get_prefix_length(str);
	if (srcp != str)
		memmove(str, srcp, strlen(srcp) + 1);
}

static void trim_subject_for_sort(gchar *str)
{
	gchar *srcp;

	g_strstrip(str);

	srcp = str + subject_get_prefix_length(str);
	if (srcp != str)
		memmove(str, srcp, strlen(srcp) + 1);
}

/* compare subjects */
gint subject_compare(const gchar *s1, const gchar *s2)
{
	gchar *str1, *str2;

	if (!s1 || !s2) return -1;
	if (!*s1 || !*s2) return -1;

	Xstrdup_a(str1, s1, return -1);
	Xstrdup_a(str2, s2, return -1);

	trim_subject_for_compare(str1);
	trim_subject_for_compare(str2);

	if (!*str1 || !*str2) return -1;

	return strcmp(str1, str2);
}

gint subject_compare_for_sort(const gchar *s1, const gchar *s2)
{
	gchar *str1, *str2;

	if (!s1 || !s2) return -1;

	Xstrdup_a(str1, s1, return -1);
	Xstrdup_a(str2, s2, return -1);

	trim_subject_for_sort(str1);
	trim_subject_for_sort(str2);

	if (!g_utf8_validate(str1, -1, NULL)) {
		g_warning("message subject \"%s\" failed UTF-8 validation", str1);
		return 0;
	} else if (!g_utf8_validate(str2, -1, NULL)) {
		g_warning("message subject \"%s\" failed UTF-8 validation", str2);
		return 0;
	}

	return g_utf8_collate(str1, str2);
}

void trim_subject(gchar *str)
{
	register gchar *srcp;
	gchar op, cl;
	gint in_brace;

	g_strstrip(str);

	srcp = str + subject_get_prefix_length(str);

	if (*srcp == '[') {
		op = '[';
		cl = ']';
	} else if (*srcp == '(') {
		op = '(';
		cl = ')';
	} else
		op = 0;

	if (op) {
		++srcp;
		in_brace = 1;
		while (*srcp) {
			if (*srcp == op)
				in_brace++;
			else if (*srcp == cl)
				in_brace--;
			srcp++;
			if (in_brace == 0)
				break;
		}
	}
	while (g_ascii_isspace(*srcp)) srcp++;
	memmove(str, srcp, strlen(srcp) + 1);
}

void eliminate_parenthesis(gchar *str, gchar op, gchar cl)
{
	register gchar *srcp, *destp;
	gint in_brace;

	destp = str;

	while ((destp = strchr(destp, op))) {
		in_brace = 1;
		srcp = destp + 1;
		while (*srcp) {
			if (*srcp == op)
				in_brace++;
			else if (*srcp == cl)
				in_brace--;
			srcp++;
			if (in_brace == 0)
				break;
		}
		while (g_ascii_isspace(*srcp)) srcp++;
		memmove(destp, srcp, strlen(srcp) + 1);
	}
}

void extract_parenthesis(gchar *str, gchar op, gchar cl)
{
	register gchar *srcp, *destp;
	gint in_brace;

	destp = str;

	while ((srcp = strchr(destp, op))) {
		if (destp > str)
			*destp++ = ' ';
		memmove(destp, srcp + 1, strlen(srcp));
		in_brace = 1;
		while(*destp) {
			if (*destp == op)
				in_brace++;
			else if (*destp == cl)
				in_brace--;

			if (in_brace == 0)
				break;

			destp++;
		}
	}
	*destp = '\0';
}

static void extract_parenthesis_with_skip_quote(gchar *str, gchar quote_chr,
					 gchar op, gchar cl)
{
	register gchar *srcp, *destp;
	gint in_brace;
	gboolean in_quote = FALSE;

	destp = str;

	while ((srcp = strchr_with_skip_quote(destp, quote_chr, op))) {
		if (destp > str)
			*destp++ = ' ';
		memmove(destp, srcp + 1, strlen(srcp));
		in_brace = 1;
		while(*destp) {
			if (*destp == op && !in_quote)
				in_brace++;
			else if (*destp == cl && !in_quote)
				in_brace--;
			else if (*destp == quote_chr)
				in_quote ^= TRUE;

			if (in_brace == 0)
				break;

			destp++;
		}
	}
	*destp = '\0';
}

void extract_quote(gchar *str, gchar quote_chr)
{
	register gchar *p;

	if ((str = strchr(str, quote_chr))) {
		p = str;
		while ((p = strchr(p + 1, quote_chr)) && (p[-1] == '\\')) {
			memmove(p - 1, p, strlen(p) + 1);
			p--;
		}
		if(p) {
			*p = '\0';
			memmove(str, str + 1, p - str);
		}
	}
}

/* Returns a newly allocated string with all quote_chr not at the beginning
   or the end of str escaped with '\' or the given str if not required. */
gchar *escape_internal_quotes(gchar *str, gchar quote_chr)
{
	register gchar *p, *q;
	gchar *qstr;
	int k = 0, l = 0;

	if (str == NULL || *str == '\0')
		return str;

	g_strstrip(str);
	if (*str == '\0')
		return str;
	/* search for unescaped quote_chr */
	p = str;
	if (*p == quote_chr)
		++p, ++l;
	while (*p) {
		if (*p == quote_chr && *(p - 1) != '\\' && *(p + 1) != '\0')
			++k;
		++p, ++l;
	}
	if (!k) /* nothing to escape */
		return str;

	/* unescaped quote_chr found */
	qstr = g_malloc(l + k + 1);
	p = str;
	q = qstr;
	if (*p == quote_chr) {
		*q = quote_chr;
		++p, ++q;
	}
	while (*p) {
		if (*p == quote_chr && *(p - 1) != '\\' && *(p + 1) != '\0')
			*q++ = '\\';
		*q++ = *p++;
	}
	*q = '\0';

	return qstr;
}

void eliminate_address_comment(gchar *str)
{
	register gchar *srcp, *destp;
	gint in_brace;

	destp = str;

	while ((destp = strchr(destp, '"'))) {
		if ((srcp = strchr(destp + 1, '"'))) {
			srcp++;
			if (*srcp == '@') {
				destp = srcp + 1;
			} else {
				while (g_ascii_isspace(*srcp)) srcp++;
				memmove(destp, srcp, strlen(srcp) + 1);
			}
		} else {
			*destp = '\0';
			break;
		}
	}

	destp = str;

	while ((destp = strchr_with_skip_quote(destp, '"', '('))) {
		in_brace = 1;
		srcp = destp + 1;
		while (*srcp) {
			if (*srcp == '(')
				in_brace++;
			else if (*srcp == ')')
				in_brace--;
			srcp++;
			if (in_brace == 0)
				break;
		}
		while (g_ascii_isspace(*srcp)) srcp++;
		memmove(destp, srcp, strlen(srcp) + 1);
	}
}

gchar *strchr_with_skip_quote(const gchar *str, gint quote_chr, gint c)
{
	gboolean in_quote = FALSE;

	while (*str) {
		if (*str == c && !in_quote)
			return (gchar *)str;
		if (*str == quote_chr)
			in_quote ^= TRUE;
		str++;
	}

	return NULL;
}

void extract_address(gchar *str)
{
	cm_return_if_fail(str != NULL);
	eliminate_address_comment(str);
	if (strchr_with_skip_quote(str, '"', '<'))
		extract_parenthesis_with_skip_quote(str, '"', '<', '>');
	g_strstrip(str);
}

void extract_list_id_str(gchar *str)
{
	if (strchr_with_skip_quote(str, '"', '<'))
		extract_parenthesis_with_skip_quote(str, '"', '<', '>');
	g_strstrip(str);
}

static GSList *address_list_append_real(GSList *addr_list, const gchar *str, gboolean removecomments)
{
	gchar *work;
	gchar *workp;

	if (!str) return addr_list;

	Xstrdup_a(work, str, return addr_list);

	if (removecomments)
		eliminate_address_comment(work);
	workp = work;

	while (workp && *workp) {
		gchar *p, *next;

		if ((p = strchr_with_skip_quote(workp, '"', ','))) {
			*p = '\0';
			next = p + 1;
		} else
			next = NULL;

		if (removecomments && strchr_with_skip_quote(workp, '"', '<'))
			extract_parenthesis_with_skip_quote
				(workp, '"', '<', '>');

		g_strstrip(workp);
		if (*workp)
			addr_list = g_slist_append(addr_list, g_strdup(workp));

		workp = next;
	}

	return addr_list;
}

GSList *address_list_append(GSList *addr_list, const gchar *str)
{
	return address_list_append_real(addr_list, str, TRUE);
}

GSList *address_list_append_with_comments(GSList *addr_list, const gchar *str)
{
	return address_list_append_real(addr_list, str, FALSE);
}

GSList *references_list_prepend(GSList *msgid_list, const gchar *str)
{
	const gchar *strp;

	if (!str) return msgid_list;
	strp = str;

	while (strp && *strp) {
		const gchar *start, *end;
		gchar *msgid;

		if ((start = strchr(strp, '<')) != NULL) {
			end = strchr(start + 1, '>');
			if (!end) break;
		} else
			break;

		msgid = g_strndup(start + 1, end - start - 1);
		g_strstrip(msgid);
		if (*msgid)
			msgid_list = g_slist_prepend(msgid_list, msgid);
		else
			g_free(msgid);

		strp = end + 1;
	}

	return msgid_list;
}

GSList *references_list_append(GSList *msgid_list, const gchar *str)
{
	GSList *list;

	list = references_list_prepend(NULL, str);
	list = g_slist_reverse(list);
	msgid_list = g_slist_concat(msgid_list, list);

	return msgid_list;
}

GSList *newsgroup_list_append(GSList *group_list, const gchar *str)
{
	gchar *work;
	gchar *workp;

	if (!str) return group_list;

	Xstrdup_a(work, str, return group_list);

	workp = work;

	while (workp && *workp) {
		gchar *p, *next;

		if ((p = strchr_with_skip_quote(workp, '"', ','))) {
			*p = '\0';
			next = p + 1;
		} else
			next = NULL;

		g_strstrip(workp);
		if (*workp)
			group_list = g_slist_append(group_list,
						    g_strdup(workp));

		workp = next;
	}

	return group_list;
}

GList *add_history(GList *list, const gchar *str)
{
	GList *old;
	gchar *oldstr;

	cm_return_val_if_fail(str != NULL, list);

	old = g_list_find_custom(list, (gpointer)str, (GCompareFunc)g_strcmp0);
	if (old) {
		oldstr = old->data;
		list = g_list_remove(list, old->data);
		g_free(oldstr);
	} else if (g_list_length(list) >= MAX_HISTORY_SIZE) {
		GList *last;

		last = g_list_last(list);
		if (last) {
			oldstr = last->data;
			list = g_list_remove(list, last->data);
			g_free(oldstr);
		}
	}

	list = g_list_prepend(list, g_strdup(str));

	return list;
}

void remove_return(gchar *str)
{
	register gchar *p = str;

	while (*p) {
		if (*p == '\n' || *p == '\r')
			memmove(p, p + 1, strlen(p));
		else
			p++;
	}
}

void remove_space(gchar *str)
{
	register gchar *p = str;
	register gint spc;

	while (*p) {
		spc = 0;
		while (g_ascii_isspace(*(p + spc)))
			spc++;
		if (spc)
			memmove(p, p + spc, strlen(p + spc) + 1);
		else
			p++;
	}
}

void unfold_line(gchar *str)
{
	register gchar *ch;
	register gunichar c;
	register gint len;

	ch = str; /* iterator for source string */

	while (*ch != 0) {
		c = g_utf8_get_char_validated(ch, -1);

		if (c == (gunichar)-1 || c == (gunichar)-2) {
			/* non-unicode byte, move past it */
			ch++;
			continue;
		}

		len = g_unichar_to_utf8(c, NULL);

		if ((!g_unichar_isdefined(c) || !g_unichar_isprint(c) ||
				g_unichar_isspace(c)) && c != 173) {
			/* replace anything bad or whitespacey with a single space */
			*ch = ' ';
			ch++;
			if (len > 1) {
				/* move rest of the string forwards, since we just replaced
				 * a multi-byte sequence with one byte */
				memmove(ch, ch + len-1, strlen(ch + len-1) + 1);
			}
		} else {
			/* A valid unicode character, copy it. */
			ch += len;
		}
	}
}

void subst_char(gchar *str, gchar orig, gchar subst)
{
	register gchar *p = str;

	while (*p) {
		if (*p == orig)
			*p = subst;
		p++;
	}
}

void subst_chars(gchar *str, gchar *orig, gchar subst)
{
	register gchar *p = str;

	while (*p) {
		if (strchr(orig, *p) != NULL)
			*p = subst;
		p++;
	}
}

void subst_for_filename(gchar *str)
{
	if (!str)
		return;
#ifdef G_OS_WIN32
	subst_chars(str, "\t\r\n\\/*?:", '_');
#else
	subst_chars(str, "\t\r\n\\/*", '_');
#endif
}

void subst_for_shellsafe_filename(gchar *str)
{
	if (!str)
		return;
	subst_for_filename(str);
	subst_chars(str, " \"'|&;()<>'!{}[]",'_');
}

gboolean is_ascii_str(const gchar *str)
{
	const guchar *p = (const guchar *)str;

	while (*p != '\0') {
		if (*p != '\t' && *p != ' ' &&
		    *p != '\r' && *p != '\n' &&
		    (*p < 32 || *p >= 127))
			return FALSE;
		p++;
	}

	return TRUE;
}

static const gchar * line_has_quote_char_last(const gchar * str, const gchar *quote_chars)
{
	gchar * position = NULL;
	gchar * tmp_pos = NULL;
	int i;

	if (str == NULL || quote_chars == NULL)
		return NULL;

	for (i = 0; i < strlen(quote_chars); i++) {
		tmp_pos = strrchr (str, quote_chars[i]);
		if(position == NULL
				|| (tmp_pos != NULL && position <= tmp_pos) )
			position = tmp_pos;
	}
	return position;
}

gint get_quote_level(const gchar *str, const gchar *quote_chars)
{
	const gchar *first_pos;
	const gchar *last_pos;
	const gchar *p = str;
	gint quote_level = -1;

	/* speed up line processing by only searching to the last '>' */
	if ((first_pos = line_has_quote_char(str, quote_chars)) != NULL) {
		/* skip a line if it contains a '<' before the initial '>' */
		if (memchr(str, '<', first_pos - str) != NULL)
			return -1;
		last_pos = line_has_quote_char_last(first_pos, quote_chars);
	} else
		return -1;

	while (p <= last_pos) {
		while (p < last_pos) {
			if (g_ascii_isspace(*p))
				p++;
			else
				break;
		}

		if (strchr(quote_chars, *p))
			quote_level++;
		else if (*p != '-' && !g_ascii_isspace(*p) && p <= last_pos) {
			/* any characters are allowed except '-','<' and space */
			while (*p != '-' && *p != '<'
			       && !strchr(quote_chars, *p)
			       && !g_ascii_isspace(*p)
			       && p < last_pos)
				p++;
			if (strchr(quote_chars, *p))
				quote_level++;
			else
				break;
		}

		p++;
	}

	return quote_level;
}

gint check_line_length(const gchar *str, gint max_chars, gint *line)
{
	const gchar *p = str, *q;
	gint cur_line = 0, len;

	while ((q = strchr(p, '\n')) != NULL) {
		len = q - p + 1;
		if (len > max_chars) {
			if (line)
				*line = cur_line;
			return -1;
		}
		p = q + 1;
		++cur_line;
	}

	len = strlen(p);
	if (len > max_chars) {
		if (line)
			*line = cur_line;
		return -1;
	}

	return 0;
}

const gchar * line_has_quote_char(const gchar * str, const gchar *quote_chars)
{
	gchar * position = NULL;
	gchar * tmp_pos = NULL;
	int i;

	if (str == NULL || quote_chars == NULL)
		return NULL;

	for (i = 0; i < strlen(quote_chars); i++) {
		tmp_pos = strchr (str, quote_chars[i]);
		if(position == NULL
				|| (tmp_pos != NULL && position >= tmp_pos) )
			position = tmp_pos;
	}
	return position;
}

static gchar *strstr_with_skip_quote(const gchar *haystack, const gchar *needle)
{
	register guint haystack_len, needle_len;
	gboolean in_squote = FALSE, in_dquote = FALSE;

	haystack_len = strlen(haystack);
	needle_len   = strlen(needle);

	if (haystack_len < needle_len || needle_len == 0)
		return NULL;

	while (haystack_len >= needle_len) {
		if (!in_squote && !in_dquote &&
		    !strncmp(haystack, needle, needle_len))
			return (gchar *)haystack;

		/* 'foo"bar"' -> foo"bar"
		   "foo'bar'" -> foo'bar' */
		if (*haystack == '\'') {
			if (in_squote)
				in_squote = FALSE;
			else if (!in_dquote)
				in_squote = TRUE;
		} else if (*haystack == '\"') {
			if (in_dquote)
				in_dquote = FALSE;
			else if (!in_squote)
				in_dquote = TRUE;
		} else if (*haystack == '\\') {
			haystack++;
			haystack_len--;
		}

		haystack++;
		haystack_len--;
	}

	return NULL;
}

gchar **strsplit_with_quote(const gchar *str, const gchar *delim,
			    gint max_tokens)
{
	GSList *string_list = NULL, *slist;
	gchar **str_array, *s, *new_str;
	guint i, n = 1, len;

	cm_return_val_if_fail(str != NULL, NULL);
	cm_return_val_if_fail(delim != NULL, NULL);

	if (max_tokens < 1)
		max_tokens = G_MAXINT;

	s = strstr_with_skip_quote(str, delim);
	if (s) {
		guint delimiter_len = strlen(delim);

		do {
			len = s - str;
			new_str = g_strndup(str, len);

			if (new_str[0] == '\'' || new_str[0] == '\"') {
				if (new_str[len - 1] == new_str[0]) {
					new_str[len - 1] = '\0';
					memmove(new_str, new_str + 1, len - 1);
				}
			}
			string_list = g_slist_prepend(string_list, new_str);
			n++;
			str = s + delimiter_len;
			s = strstr_with_skip_quote(str, delim);
		} while (--max_tokens && s);
	}

	if (*str) {
		new_str = g_strdup(str);
		if (new_str[0] == '\'' || new_str[0] == '\"') {
			len = strlen(str);
			if (new_str[len - 1] == new_str[0]) {
				new_str[len - 1] = '\0';
				memmove(new_str, new_str + 1, len - 1);
			}
		}
		string_list = g_slist_prepend(string_list, new_str);
		n++;
	}

	str_array = g_new(gchar*, n);

	i = n - 1;

	str_array[i--] = NULL;
	for (slist = string_list; slist; slist = slist->next)
		str_array[i--] = slist->data;

	g_slist_free(string_list);

	return str_array;
}

gchar *get_abbrev_newsgroup_name(const gchar *group, gint len)
{
	gchar *abbrev_group;
	gchar *ap;
	const gchar *p = group;
	const gchar *last;

	cm_return_val_if_fail(group != NULL, NULL);

	last = group + strlen(group);
	abbrev_group = ap = g_malloc(strlen(group) + 1);

	while (*p) {
		while (*p == '.')
			*ap++ = *p++;
		if ((ap - abbrev_group) + (last - p) > len && strchr(p, '.')) {
			*ap++ = *p++;
			while (*p != '.') p++;
		} else {
			strcpy(ap, p);
			return abbrev_group;
		}
	}

	*ap = '\0';
	return abbrev_group;
}

gchar *trim_string(const gchar *str, gint len)
{
	const gchar *p = str;
	gint mb_len;
	gchar *new_str;
	gint new_len = 0;

	if (!str) return NULL;
	if (strlen(str) <= len)
		return g_strdup(str);
	if (g_utf8_validate(str, -1, NULL) == FALSE)
		return g_strdup(str);

	while (*p != '\0') {
		mb_len = g_utf8_skip[*(guchar *)p];
		if (mb_len == 0)
			break;
		else if (new_len + mb_len > len)
			break;

		new_len += mb_len;
		p += mb_len;
	}

	Xstrndup_a(new_str, str, new_len, return g_strdup(str));
	return g_strconcat(new_str, "...", NULL);
}

GList *uri_list_extract_filenames(const gchar *uri_list)
{
	GList *result = NULL;
	const gchar *p, *q;
	gchar *escaped_utf8uri;

	p = uri_list;

	while (p) {
		if (*p != '#') {
			while (g_ascii_isspace(*p)) p++;
			if (!strncmp(p, "file:", 5)) {
				q = p;
				q += 5;
				while (*q && *q != '\n' && *q != '\r') q++;

				if (q > p) {
					gchar *file, *locale_file = NULL;
					q--;
					while (q > p && g_ascii_isspace(*q))
						q--;
					Xalloca(escaped_utf8uri, q - p + 2,
						return result);
					Xalloca(file, q - p + 2,
						return result);
					*file = '\0';
					strncpy(escaped_utf8uri, p, q - p + 1);
					escaped_utf8uri[q - p + 1] = '\0';
					decode_uri_with_plus(file, escaped_utf8uri, FALSE);
		    /*
		     * g_filename_from_uri() rejects escaped/locale encoded uri
		     * string which come from Nautilus.
		     */
#ifndef G_OS_WIN32
					if (g_utf8_validate(file, -1, NULL))
						locale_file
							= conv_codeset_strdup(
								file + 5,
								CS_UTF_8,
								conv_get_locale_charset_str());
					if (!locale_file)
						locale_file = g_strdup(file + 5);
#else
					locale_file = g_filename_from_uri(escaped_utf8uri, NULL, NULL);
#endif
					result = g_list_append(result, locale_file);
				}
			}
		}
		p = strchr(p, '\n');
		if (p) p++;
	}

	return result;
}

/* Converts two-digit hexadecimal to decimal.  Used for unescaping escaped
 * characters
 */
static gint axtoi(const gchar *hexstr)
{
	gint hi, lo, result;

	hi = hexstr[0];
	if ('0' <= hi && hi <= '9') {
		hi -= '0';
	} else
		if ('a' <= hi && hi <= 'f') {
			hi -= ('a' - 10);
		} else
			if ('A' <= hi && hi <= 'F') {
				hi -= ('A' - 10);
			}

	lo = hexstr[1];
	if ('0' <= lo && lo <= '9') {
		lo -= '0';
	} else
		if ('a' <= lo && lo <= 'f') {
			lo -= ('a'-10);
		} else
			if ('A' <= lo && lo <= 'F') {
				lo -= ('A' - 10);
			}
	result = lo + (16 * hi);
	return result;
}

gboolean is_uri_string(const gchar *str)
{
	while (str && *str && g_ascii_isspace(*str))
		str++;
	return (g_ascii_strncasecmp(str, "http://", 7) == 0 ||
		g_ascii_strncasecmp(str, "https://", 8) == 0 ||
		g_ascii_strncasecmp(str, "ftp://", 6) == 0 ||
		g_ascii_strncasecmp(str, "ftps://", 7) == 0 ||
		g_ascii_strncasecmp(str, "sftp://", 7) == 0 ||
		g_ascii_strncasecmp(str, "ftp.", 4) == 0 ||
		g_ascii_strncasecmp(str, "webcal://", 9) == 0 ||
		g_ascii_strncasecmp(str, "webcals://", 10) == 0 ||
		g_ascii_strncasecmp(str, "www.", 4) == 0);
}

gchar *get_uri_path(const gchar *uri)
{
	while (uri && *uri && g_ascii_isspace(*uri))
		uri++;
	if (g_ascii_strncasecmp(uri, "http://", 7) == 0)
		return (gchar *)(uri + 7);
	else if (g_ascii_strncasecmp(uri, "https://", 8) == 0)
		return (gchar *)(uri + 8);
	else if (g_ascii_strncasecmp(uri, "ftp://", 6) == 0)
		return (gchar *)(uri + 6);
	else if (g_ascii_strncasecmp(uri, "ftps://", 7) == 0)
		return (gchar *)(uri + 7);
	else if (g_ascii_strncasecmp(uri, "sftp://", 7) == 0)
		return (gchar *)(uri + 7);
	else if (g_ascii_strncasecmp(uri, "webcal://", 9) == 0)
		return (gchar *)(uri + 7);
	else if (g_ascii_strncasecmp(uri, "webcals://", 10) == 0)
		return (gchar *)(uri + 7);
	else
		return (gchar *)uri;
}

gint get_uri_len(const gchar *str)
{
	const gchar *p;

	if (is_uri_string(str)) {
		for (p = str; *p != '\0'; p++) {
			if (!g_ascii_isgraph(*p) || strchr("<>\"", *p))
				break;
		}
		return p - str;
	}

	return 0;
}

/* Decodes URL-Encoded strings (i.e. strings in which spaces are replaced by
 * plusses, and escape characters are used)
 */
void decode_uri_with_plus(gchar *decoded_uri, const gchar *encoded_uri, gboolean with_plus)
{
	gchar *dec = decoded_uri;
	const gchar *enc = encoded_uri;

	while (*enc) {
		if (*enc == '%') {
			enc++;
			if (isxdigit((guchar)enc[0]) &&
			    isxdigit((guchar)enc[1])) {
				*dec = axtoi(enc);
				dec++;
				enc += 2;
			}
		} else {
			if (with_plus && *enc == '+')
				*dec = ' ';
			else
				*dec = *enc;
			dec++;
			enc++;
		}
	}

	*dec = '\0';
}

void decode_uri(gchar *decoded_uri, const gchar *encoded_uri)
{
	decode_uri_with_plus(decoded_uri, encoded_uri, TRUE);
}

static gchar *decode_uri_gdup(const gchar *encoded_uri)
{
    gchar *buffer = g_malloc(strlen(encoded_uri)+1);
    decode_uri_with_plus(buffer, encoded_uri, FALSE);
    return buffer;
}

gint scan_mailto_url(const gchar *mailto, gchar **from, gchar **to, gchar **cc, gchar **bcc,
		     gchar **subject, gchar **body, gchar ***attach, gchar **inreplyto)
{
	gchar *tmp_mailto;
	gchar *p;
	const gchar *forbidden_uris[] = { ".gnupg/",
					  "/etc/passwd",
					  "/etc/shadow",
					  ".ssh/",
					  "../",
					  NULL };
	gint num_attach = 0;

	cm_return_val_if_fail(mailto != NULL, -1);

	Xstrdup_a(tmp_mailto, mailto, return -1);

	if (!strncmp(tmp_mailto, "mailto:", 7))
		tmp_mailto += 7;

	p = strchr(tmp_mailto, '?');
	if (p) {
		*p = '\0';
		p++;
	}

	if (to && !*to)
		*to = decode_uri_gdup(tmp_mailto);

	while (p) {
		gchar *field, *value;

		field = p;

		p = strchr(p, '=');
		if (!p) break;
		*p = '\0';
		p++;

		value = p;

		p = strchr(p, '&');
		if (p) {
			*p = '\0';
			p++;
		}

		if (*value == '\0') continue;

		if (from && !g_ascii_strcasecmp(field, "from")) {
			if (!*from) {
				*from = decode_uri_gdup(value);
			} else {
				gchar *tmp = decode_uri_gdup(value);
				gchar *new_from = g_strdup_printf("%s, %s", *from, tmp);
				g_free(tmp);
				g_free(*from);
				*from = new_from;
			}
		} else if (cc && !g_ascii_strcasecmp(field, "cc")) {
			if (!*cc) {
				*cc = decode_uri_gdup(value);
			} else {
				gchar *tmp = decode_uri_gdup(value);
				gchar *new_cc = g_strdup_printf("%s, %s", *cc, tmp);
				g_free(tmp);
				g_free(*cc);
				*cc = new_cc;
			}
		} else if (bcc && !g_ascii_strcasecmp(field, "bcc")) {
			if (!*bcc) {
				*bcc = decode_uri_gdup(value);
			} else {
				gchar *tmp = decode_uri_gdup(value);
				gchar *new_bcc = g_strdup_printf("%s, %s", *bcc, tmp);
				g_free(tmp);
				g_free(*bcc);
				*bcc = new_bcc;
			}
		} else if (subject && !*subject &&
			   !g_ascii_strcasecmp(field, "subject")) {
			*subject = decode_uri_gdup(value);
		} else if (body && !*body && !g_ascii_strcasecmp(field, "body")) {
			*body = decode_uri_gdup(value);
		} else if (body && !*body && !g_ascii_strcasecmp(field, "insert")) {
			int i = 0;
			gchar *tmp = decode_uri_gdup(value);

			for (; forbidden_uris[i]; i++) {
				if (strstr(tmp, forbidden_uris[i])) {
					g_print("Refusing to insert '%s', potential private data leak\n",
							tmp);
					g_free(tmp);
					tmp = NULL;
					break;
				}
			}

			if (tmp) {
				if (!is_file_entry_regular(tmp)) {
					g_warning("refusing to insert '%s', not a regular file", tmp);
				} else if (!g_file_get_contents(tmp, body, NULL, NULL)) {
					g_warning("couldn't set insert file '%s' in body", value);
				}

				g_free(tmp);
			}
		} else if (attach && !g_ascii_strcasecmp(field, "attach")) {
			int i = 0;
			gchar *tmp = decode_uri_gdup(value);
			gchar **my_att = g_malloc(sizeof(char *));

			my_att[0] = NULL;

			for (; forbidden_uris[i]; i++) {
				if (strstr(tmp, forbidden_uris[i])) {
					g_print("Refusing to attach '%s', potential private data leak\n",
							tmp);
					g_free(tmp);
					tmp = NULL;
					break;
				}
			}
			if (tmp) {
				/* attach is correct */
				num_attach++;
				my_att = g_realloc(my_att, (sizeof(char *))*(num_attach+1));
				my_att[num_attach-1] = tmp;
				my_att[num_attach] = NULL;
				*attach = my_att;
			}
            else
				g_free(my_att);
		} else if (inreplyto && !*inreplyto &&
			   !g_ascii_strcasecmp(field, "in-reply-to")) {
			*inreplyto = decode_uri_gdup(value);
		}
	}

	return 0;
}


#ifdef G_OS_WIN32
#include <windows.h>
#ifndef CSIDL_APPDATA
#define CSIDL_APPDATA 0x001a
#endif
#ifndef CSIDL_LOCAL_APPDATA
#define CSIDL_LOCAL_APPDATA 0x001c
#endif
#ifndef CSIDL_FLAG_CREATE
#define CSIDL_FLAG_CREATE 0x8000
#endif
#define DIM(v)		     (sizeof(v)/sizeof((v)[0]))

#define RTLD_LAZY 0
const char *
w32_strerror (int w32_errno)
{
  static char strerr[256];
  int ec = (int)GetLastError ();

  if (w32_errno == 0)
    w32_errno = ec;
  FormatMessage (FORMAT_MESSAGE_FROM_SYSTEM, NULL, w32_errno,
		 MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT),
		 strerr, DIM (strerr)-1, NULL);
  return strerr;
}

static __inline__ void *
dlopen (const char * name, int flag)
{
  void * hd = LoadLibrary (name);
  return hd;
}

static __inline__ void *
dlsym (void * hd, const char * sym)
{
  if (hd && sym)
    {
      void * fnc = GetProcAddress (hd, sym);
      if (!fnc)
	return NULL;
      return fnc;
    }
  return NULL;
}


static __inline__ const char *
dlerror (void)
{
  return w32_strerror (0);
}


static __inline__ int
dlclose (void * hd)
{
  if (hd)
    {
      FreeLibrary (hd);
      return 0;
    }
  return -1;
}

static HRESULT
w32_shgetfolderpath (HWND a, int b, HANDLE c, DWORD d, LPSTR e)
{
  static int initialized;
  static HRESULT (WINAPI * func)(HWND,int,HANDLE,DWORD,LPSTR);

  if (!initialized)
    {
      static char *dllnames[] = { "shell32.dll", "shfolder.dll", NULL };
      void *handle;
      int i;

      initialized = 1;

      for (i=0, handle = NULL; !handle && dllnames[i]; i++)
	{
	  handle = dlopen (dllnames[i], RTLD_LAZY);
	  if (handle)
	    {
	      func = dlsym (handle, "SHGetFolderPathW");
	      if (!func)
		{
		  dlclose (handle);
		  handle = NULL;
		}
	    }
	}
    }

  if (func)
    return func (a,b,c,d,e);
  else
    return -1;
}

/* Returns a static string with the directroy from which the module
   has been loaded.  Returns an empty string on error. */
static char *w32_get_module_dir(void)
{
	static char *moddir;

	if (!moddir) {
		char name[MAX_PATH+10];
		char *p;

		if ( !GetModuleFileNameA (0, name, sizeof (name)-10) )
			*name = 0;
		else {
			p = strrchr (name, '\\');
			if (p)
				*p = 0;
			else
				*name = 0;
		}
		moddir = g_strdup (name);
	}
	return moddir;
}
#endif /* G_OS_WIN32 */

/* Return a static string with the locale dir. */
const gchar *get_locale_dir(void)
{
	static gchar *loc_dir;

#ifdef G_OS_WIN32
	if (!loc_dir)
		loc_dir = g_strconcat(w32_get_module_dir(), G_DIR_SEPARATOR_S,
				      "\\share\\locale", NULL);
#endif
	if (!loc_dir)
		loc_dir = LOCALEDIR;

	return loc_dir;
}


const gchar *get_home_dir(void)
{
#ifdef G_OS_WIN32
	static char home_dir_utf16[MAX_PATH] = "";
	static gchar *home_dir_utf8 = NULL;
	if (home_dir_utf16[0] == '\0') {
		if (w32_shgetfolderpath
			    (NULL, CSIDL_APPDATA|CSIDL_FLAG_CREATE,
			     NULL, 0, home_dir_utf16) < 0)
				strcpy (home_dir_utf16, "C:\\Claws Mail");
		home_dir_utf8 = g_utf16_to_utf8 ((const gunichar2 *)home_dir_utf16, -1, NULL, NULL, NULL);
	}
	return home_dir_utf8;
#else
	static const gchar *homeenv = NULL;

	if (homeenv)
		return homeenv;

	if (!homeenv && g_getenv("HOME") != NULL)
		homeenv = g_strdup(g_getenv("HOME"));
	if (!homeenv)
		homeenv = g_get_home_dir();

	return homeenv;
#endif
}

static gchar *claws_rc_dir = NULL;
static gboolean rc_dir_alt = FALSE;
const gchar *get_rc_dir(void)
{

	if (!claws_rc_dir) {
		claws_rc_dir = g_strconcat(get_home_dir(), G_DIR_SEPARATOR_S,
				     RC_DIR, NULL);
		debug_print("using default rc_dir %s\n", claws_rc_dir);
	}
	return claws_rc_dir;
}

void set_rc_dir(const gchar *dir)
{
	gchar *canonical_dir;
	if (claws_rc_dir != NULL) {
		g_print("Error: rc_dir already set\n");
	} else {
		int err = cm_canonicalize_filename(dir, &canonical_dir);
		int len;

		if (err) {
			g_print("Error looking for %s: %d(%s)\n",
				dir, -err, g_strerror(-err));
			exit(0);
		}
		rc_dir_alt = TRUE;

		claws_rc_dir = canonical_dir;

		len = strlen(claws_rc_dir);
		if (claws_rc_dir[len - 1] == G_DIR_SEPARATOR)
			claws_rc_dir[len - 1] = '\0';

		debug_print("set rc_dir to %s\n", claws_rc_dir);
		if (!is_dir_exist(claws_rc_dir)) {
			if (make_dir_hier(claws_rc_dir) != 0) {
				g_print("Error: can't create %s\n",
				claws_rc_dir);
				exit(0);
			}
		}
	}
}

gboolean rc_dir_is_alt(void) {
	return rc_dir_alt;
}

const gchar *get_mail_base_dir(void)
{
	return get_home_dir();
}

const gchar *get_news_cache_dir(void)
{
	static gchar *news_cache_dir = NULL;
	if (!news_cache_dir)
		news_cache_dir = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
					     NEWS_CACHE_DIR, NULL);

	return news_cache_dir;
}

const gchar *get_imap_cache_dir(void)
{
	static gchar *imap_cache_dir = NULL;

	if (!imap_cache_dir)
		imap_cache_dir = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
					     IMAP_CACHE_DIR, NULL);

	return imap_cache_dir;
}

const gchar *get_mime_tmp_dir(void)
{
	static gchar *mime_tmp_dir = NULL;

	if (!mime_tmp_dir)
		mime_tmp_dir = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
					   MIME_TMP_DIR, NULL);

	return mime_tmp_dir;
}

const gchar *get_template_dir(void)
{
	static gchar *template_dir = NULL;

	if (!template_dir)
		template_dir = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
					   TEMPLATE_DIR, NULL);

	return template_dir;
}

#ifdef G_OS_WIN32
const gchar *w32_get_cert_file(void)
{
	const gchar *cert_file = NULL;
	if (!cert_file)
		cert_file = g_strconcat(w32_get_module_dir(),
				 "\\share\\claws-mail\\",
				"ca-certificates.crt",
				NULL);
	return cert_file;
}
#endif

/* Return the filepath of the claws-mail.desktop file */
const gchar *get_desktop_file(void)
{
#ifdef DESKTOPFILEPATH
  return DESKTOPFILEPATH;
#else
  return NULL;
#endif
}

/* Return the default directory for Plugins. */
const gchar *get_plugin_dir(void)
{
#ifdef G_OS_WIN32
	static gchar *plugin_dir = NULL;

	if (!plugin_dir)
		plugin_dir = g_strconcat(w32_get_module_dir(),
					 "\\lib\\claws-mail\\plugins\\",
					 NULL);
	return plugin_dir;
#else
	if (is_dir_exist(PLUGINDIR))
		return PLUGINDIR;
	else {
		static gchar *plugin_dir = NULL;
		if (!plugin_dir)
			plugin_dir = g_strconcat(get_rc_dir(),
				G_DIR_SEPARATOR_S, "plugins",
				G_DIR_SEPARATOR_S, NULL);
		return plugin_dir;
	}
#endif
}


#ifdef G_OS_WIN32
/* Return the default directory for Themes. */
const gchar *w32_get_themes_dir(void)
{
	static gchar *themes_dir = NULL;

	if (!themes_dir)
		themes_dir = g_strconcat(w32_get_module_dir(),
					 "\\share\\claws-mail\\themes",
					 NULL);
	return themes_dir;
}
#endif

const gchar *get_tmp_dir(void)
{
	static gchar *tmp_dir = NULL;

	if (!tmp_dir)
		tmp_dir = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
				      TMP_DIR, NULL);

	return tmp_dir;
}

gchar *get_tmp_file(void)
{
	gchar *tmp_file;
	static guint32 id = 0;

	tmp_file = g_strdup_printf("%s%ctmpfile.%08x",
				   get_tmp_dir(), G_DIR_SEPARATOR, id++);

	return tmp_file;
}

const gchar *get_domain_name(void)
{
#ifdef G_OS_UNIX
	static gchar *domain_name = NULL;
	struct addrinfo hints, *res;
	char hostname[256];
	int s;

	if (!domain_name) {
		if (gethostname(hostname, sizeof(hostname)) != 0) {
			perror("gethostname");
			domain_name = "localhost";
		} else {
			memset(&hints, 0, sizeof(struct addrinfo));
			hints.ai_family = AF_UNSPEC;
			hints.ai_socktype = 0;
			hints.ai_flags = AI_CANONNAME;
			hints.ai_protocol = 0;

			s = getaddrinfo(hostname, NULL, &hints, &res);
			if (s != 0) {
				fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
				domain_name = g_strdup(hostname);
			} else {
				domain_name = g_strdup(res->ai_canonname);
				freeaddrinfo(res);
			}
		}
		debug_print("domain name = %s\n", domain_name);
	}

	return domain_name;
#else
	return "localhost";
#endif
}

/* Tells whether the given host address string is a valid representation of a
 * numerical IP (v4 or, if supported, v6) address.
 */
gboolean is_numeric_host_address(const gchar *hostaddress)
{
	struct addrinfo hints, *res;
	int err;

	/* See what getaddrinfo makes of the string when told that it is a
	 * numeric IP address representation. */
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = 0;
	hints.ai_flags = AI_NUMERICHOST;
	hints.ai_protocol = 0;

	err = getaddrinfo(hostaddress, NULL, &hints, &res);
	if (err == 0)
		freeaddrinfo(res);

	return (err == 0);
}

off_t get_file_size(const gchar *file)
{
#ifdef G_OS_WIN32
	GFile *f;
	GFileInfo *fi;
	GError *error = NULL;
	goffset size;

	f = g_file_new_for_path(file);
	fi = g_file_query_info(f, "standard::size",
			G_FILE_QUERY_INFO_NONE, NULL, &error);
	if (error != NULL) {
		debug_print("get_file_size error: %s\n", error->message);
		g_error_free(error);
		g_object_unref(f);
		return -1;
	}
	size = g_file_info_get_size(fi);
	g_object_unref(fi);
	g_object_unref(f);
	return size;

#else
	GStatBuf s;

	if (g_stat(file, &s) < 0) {
		FILE_OP_ERROR(file, "stat");
		return -1;
	}

	return s.st_size;
#endif
}

time_t get_file_mtime(const gchar *file)
{
	GStatBuf s;

	if (g_stat(file, &s) < 0) {
		FILE_OP_ERROR(file, "stat");
		return -1;
	}

	return s.st_mtime;
}

gboolean file_exist(const gchar *file, gboolean allow_fifo)
{
	GStatBuf s;

	if (file == NULL)
		return FALSE;

	if (g_stat(file, &s) < 0) {
		if (ENOENT != errno) FILE_OP_ERROR(file, "stat");
		return FALSE;
	}

	if (S_ISREG(s.st_mode) || (allow_fifo && S_ISFIFO(s.st_mode)))
		return TRUE;

	return FALSE;
}


/* Test on whether FILE is a relative file name. This is
 * straightforward for Unix but more complex for Windows. */
gboolean is_relative_filename(const gchar *file)
{
	if (!file)
		return TRUE;
#ifdef G_OS_WIN32
	if ( *file == '\\' && file[1] == '\\' && strchr (file+2, '\\') )
		return FALSE; /* Prefixed with a hostname - this can't
			       * be a relative name. */

	if ( ((*file >= 'a' && *file <= 'z')
	      || (*file >= 'A' && *file <= 'Z'))
	     && file[1] == ':')
		file += 2;  /* Skip drive letter. */

	return !(*file == '\\' || *file == '/');
#else
	return !(*file == G_DIR_SEPARATOR);
#endif
}


gboolean is_dir_exist(const gchar *dir)
{
	if (dir == NULL)
		return FALSE;

	return g_file_test(dir, G_FILE_TEST_IS_DIR);
}

gboolean is_file_entry_exist(const gchar *file)
{
	if (file == NULL)
		return FALSE;

	return g_file_test(file, G_FILE_TEST_EXISTS);
}

gboolean is_file_entry_regular(const gchar *file)
{
	if (file == NULL)
		return FALSE;

	return g_file_test(file, G_FILE_TEST_IS_REGULAR);
}

gboolean dirent_is_regular_file(struct dirent *d)
{
#if !defined(G_OS_WIN32) && defined(HAVE_DIRENT_D_TYPE)
	if (d->d_type == DT_REG)
		return TRUE;
	else if (d->d_type != DT_UNKNOWN)
		return FALSE;
#endif

	return g_file_test(d->d_name, G_FILE_TEST_IS_REGULAR);
}

gint change_dir(const gchar *dir)
{
	gchar *prevdir = NULL;

	if (debug_mode)
		prevdir = g_get_current_dir();

	if (g_chdir(dir) < 0) {
		FILE_OP_ERROR(dir, "chdir");
		if (debug_mode) g_free(prevdir);
		return -1;
	} else if (debug_mode) {
		gchar *cwd;

		cwd = g_get_current_dir();
		if (strcmp(prevdir, cwd) != 0)
			g_print("current dir: %s\n", cwd);
		g_free(cwd);
		g_free(prevdir);
	}

	return 0;
}

gint make_dir(const gchar *dir)
{
	if (g_mkdir(dir, S_IRWXU) < 0) {
		FILE_OP_ERROR(dir, "mkdir");
		return -1;
	}
	if (g_chmod(dir, S_IRWXU) < 0)
		FILE_OP_ERROR(dir, "chmod");

	return 0;
}

gint make_dir_hier(const gchar *dir)
{
	gchar *parent_dir;
	const gchar *p;

	for (p = dir; (p = strchr(p, G_DIR_SEPARATOR)) != NULL; p++) {
		parent_dir = g_strndup(dir, p - dir);
		if (*parent_dir != '\0') {
			if (!is_dir_exist(parent_dir)) {
				if (make_dir(parent_dir) < 0) {
					g_free(parent_dir);
					return -1;
				}
			}
		}
		g_free(parent_dir);
	}

	if (!is_dir_exist(dir)) {
		if (make_dir(dir) < 0)
			return -1;
	}

	return 0;
}

gint remove_all_files(const gchar *dir)
{
	GDir *dp;
	const gchar *file_name;
	gchar *tmp;

	if ((dp = g_dir_open(dir, 0, NULL)) == NULL) {
		g_warning("failed to open directory: %s", dir);
		return -1;
	}

	while ((file_name = g_dir_read_name(dp)) != NULL) {
		tmp = g_strconcat(dir, G_DIR_SEPARATOR_S, file_name, NULL);
		if (claws_unlink(tmp) < 0)
			FILE_OP_ERROR(tmp, "unlink");
		g_free(tmp);
	}

	g_dir_close(dp);

	return 0;
}

gint remove_numbered_files(const gchar *dir, guint first, guint last)
{
	GDir *dp;
	const gchar *dir_name;
	gchar *prev_dir;
	gint file_no;

	if (first == last) {
		/* Skip all the dir reading part. */
		gchar *filename = g_strdup_printf("%s%s%u", dir, G_DIR_SEPARATOR_S, first);
		if (is_dir_exist(filename)) {
			/* a numbered directory with this name exists,
			 * remove the dot-file instead */
			g_free(filename);
			filename = g_strdup_printf("%s%s.%u", dir, G_DIR_SEPARATOR_S, first);
		}
		if (claws_unlink(filename) < 0) {
			FILE_OP_ERROR(filename, "unlink");
			g_free(filename);
			return -1;
		}
		g_free(filename);
		return 0;
	}

	prev_dir = g_get_current_dir();

	if (g_chdir(dir) < 0) {
		FILE_OP_ERROR(dir, "chdir");
		g_free(prev_dir);
		return -1;
	}

	if ((dp = g_dir_open(".", 0, NULL)) == NULL) {
		g_warning("failed to open directory: %s", dir);
		g_free(prev_dir);
		return -1;
	}

	while ((dir_name = g_dir_read_name(dp)) != NULL) {
		file_no = to_number(dir_name);
		if (file_no > 0 && first <= file_no && file_no <= last) {
			if (is_dir_exist(dir_name)) {
				gchar *dot_file = g_strdup_printf(".%s", dir_name);
				if (is_file_exist(dot_file) && claws_unlink(dot_file) < 0) {
					FILE_OP_ERROR(dot_file, "unlink");
				}
				g_free(dot_file);
				continue;
			}
			if (claws_unlink(dir_name) < 0)
				FILE_OP_ERROR(dir_name, "unlink");
		}
	}

	g_dir_close(dp);

	if (g_chdir(prev_dir) < 0) {
		FILE_OP_ERROR(prev_dir, "chdir");
		g_free(prev_dir);
		return -1;
	}

	g_free(prev_dir);

	return 0;
}

gint remove_numbered_files_not_in_list(const gchar *dir, GSList *numberlist)
{
	GDir *dp;
	const gchar *dir_name;
	gchar *prev_dir;
	gint file_no;
	GHashTable *wanted_files;
	GSList *cur;
	GError *error = NULL;

	if (numberlist == NULL)
	    return 0;

	prev_dir = g_get_current_dir();

	if (g_chdir(dir) < 0) {
		FILE_OP_ERROR(dir, "chdir");
		g_free(prev_dir);
		return -1;
	}

	if ((dp = g_dir_open(".", 0, &error)) == NULL) {
		g_message("Couldn't open current directory: %s (%d).\n",
				error->message, error->code);
		g_error_free(error);
		g_free(prev_dir);
		return -1;
	}

	wanted_files = g_hash_table_new(g_direct_hash, g_direct_equal);
	for (cur = numberlist; cur != NULL; cur = cur->next) {
		/* numberlist->data is expected to be GINT_TO_POINTER */
		g_hash_table_insert(wanted_files, cur->data, GINT_TO_POINTER(1));
	}

	while ((dir_name = g_dir_read_name(dp)) != NULL) {
		file_no = to_number(dir_name);
		if (is_dir_exist(dir_name))
			continue;
		if (file_no > 0 && g_hash_table_lookup(wanted_files, GINT_TO_POINTER(file_no)) == NULL) {
			debug_print("removing unwanted file %d from %s\n", file_no, dir);
			if (is_dir_exist(dir_name)) {
				gchar *dot_file = g_strdup_printf(".%s", dir_name);
				if (is_file_exist(dot_file) && claws_unlink(dot_file) < 0) {
					FILE_OP_ERROR(dot_file, "unlink");
				}
				g_free(dot_file);
				continue;
			}
			if (claws_unlink(dir_name) < 0)
				FILE_OP_ERROR(dir_name, "unlink");
		}
	}

	g_dir_close(dp);
	g_hash_table_destroy(wanted_files);

	if (g_chdir(prev_dir) < 0) {
		FILE_OP_ERROR(prev_dir, "chdir");
		g_free(prev_dir);
		return -1;
	}

	g_free(prev_dir);

	return 0;
}

gint remove_all_numbered_files(const gchar *dir)
{
	return remove_numbered_files(dir, 0, UINT_MAX);
}

gint remove_dir_recursive(const gchar *dir)
{
	GStatBuf s;
	GDir *dp;
	const gchar *dir_name;
	gchar *prev_dir;

	if (g_stat(dir, &s) < 0) {
		FILE_OP_ERROR(dir, "stat");
		if (ENOENT == errno) return 0;
		return -(errno);
	}

	if (!S_ISDIR(s.st_mode)) {
		if (claws_unlink(dir) < 0) {
			FILE_OP_ERROR(dir, "unlink");
			return -(errno);
		}

		return 0;
	}

	prev_dir = g_get_current_dir();
	/* g_print("prev_dir = %s\n", prev_dir); */

	if (!path_cmp(prev_dir, dir)) {
		g_free(prev_dir);
		if (g_chdir("..") < 0) {
			FILE_OP_ERROR(dir, "chdir");
			return -(errno);
		}
		prev_dir = g_get_current_dir();
	}

	if (g_chdir(dir) < 0) {
		FILE_OP_ERROR(dir, "chdir");
		g_free(prev_dir);
		return -(errno);
	}

	if ((dp = g_dir_open(".", 0, NULL)) == NULL) {
		g_warning("failed to open directory: %s", dir);
		g_chdir(prev_dir);
		g_free(prev_dir);
		return -(errno);
	}

	/* remove all files in the directory */
	while ((dir_name = g_dir_read_name(dp)) != NULL) {
		/* g_print("removing %s\n", dir_name); */

		if (is_dir_exist(dir_name)) {
			gint ret;

			if ((ret = remove_dir_recursive(dir_name)) < 0) {
				g_warning("can't remove directory: %s", dir_name);
				g_dir_close(dp);
				return ret;
			}
		} else {
			if (claws_unlink(dir_name) < 0)
				FILE_OP_ERROR(dir_name, "unlink");
		}
	}

	g_dir_close(dp);

	if (g_chdir(prev_dir) < 0) {
		FILE_OP_ERROR(prev_dir, "chdir");
		g_free(prev_dir);
		return -(errno);
	}

	g_free(prev_dir);

	if (g_rmdir(dir) < 0) {
		FILE_OP_ERROR(dir, "rmdir");
		return -(errno);
	}

	return 0;
}

/* convert line endings into CRLF. If the last line doesn't end with
 * linebreak, add it.
 */
gchar *canonicalize_str(const gchar *str)
{
	const gchar *p;
	guint new_len = 0;
	gchar *out, *outp;

	for (p = str; *p != '\0'; ++p) {
		if (*p != '\r') {
			++new_len;
			if (*p == '\n')
				++new_len;
		}
	}
	if (p == str || *(p - 1) != '\n')
		new_len += 2;

	out = outp = g_malloc(new_len + 1);
	for (p = str; *p != '\0'; ++p) {
		if (*p != '\r') {
			if (*p == '\n')
				*outp++ = '\r';
			*outp++ = *p;
		}
	}
	if (p == str || *(p - 1) != '\n') {
		*outp++ = '\r';
		*outp++ = '\n';
	}
	*outp = '\0';

	return out;
}

gchar *normalize_newlines(const gchar *str)
{
	const gchar *p;
	gchar *out, *outp;

	out = outp = g_malloc(strlen(str) + 1);
	for (p = str; *p != '\0'; ++p) {
		if (*p == '\r') {
			if (*(p + 1) != '\n')
				*outp++ = '\n';
		} else
			*outp++ = *p;
	}

	*outp = '\0';

	return out;
}

gchar *get_outgoing_rfc2822_str(FILE *fp)
{
	gchar buf[BUFFSIZE];
	GString *str;

	str = g_string_new(NULL);

	/* output header part */
	while (claws_fgets(buf, sizeof(buf), fp) != NULL) {
		strretchomp(buf);
		if (!g_ascii_strncasecmp(buf, "Bcc:", 4)) {
			gint next;

			for (;;) {
				next = fgetc(fp);
				if (next == EOF)
					break;
				else if (next != ' ' && next != '\t') {
					ungetc(next, fp);
					break;
				}
				if (claws_fgets(buf, sizeof(buf), fp) == NULL)
					break;
			}
		} else {
			g_string_append(str, buf);
			g_string_append(str, "\r\n");
			if (buf[0] == '\0')
				break;
		}
	}

	/* output body part */
	while (claws_fgets(buf, sizeof(buf), fp) != NULL) {
		strretchomp(buf);
		if (buf[0] == '.')
			g_string_append_c(str, '.');
		g_string_append(str, buf);
		g_string_append(str, "\r\n");
	}

	return g_string_free(str, FALSE);
}

/*
 * Create a new boundary in a way that it is very unlikely that this
 * will occur in the following text.  It would be easy to ensure
 * uniqueness if everything is either quoted-printable or base64
 * encoded (note that conversion is allowed), but because MIME bodies
 * may be nested, it may happen that the same boundary has already
 * been used.
 *
 *   boundary := 0*69<bchars> bcharsnospace
 *   bchars := bcharsnospace / " "
 *   bcharsnospace := DIGIT / ALPHA / "'" / "(" / ")" /
 *		    "+" / "_" / "," / "-" / "." /
 *		    "/" / ":" / "=" / "?"
 *
 * some special characters removed because of buggy MTAs
 */

gchar *generate_mime_boundary(const gchar *prefix)
{
	static gchar tbl[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
			     "abcdefghijklmnopqrstuvwxyz"
			     "1234567890+_./=";
	gchar buf_uniq[24];
	gint i;

	for (i = 0; i < sizeof(buf_uniq) - 1; i++)
		buf_uniq[i] = tbl[g_random_int_range(0, sizeof(tbl) - 1)];
	buf_uniq[i] = '\0';

	return g_strdup_printf("%s_/%s", prefix ? prefix : "MP",
			       buf_uniq);
}

char *fgets_crlf(char *buf, int size, FILE *stream)
{
	gboolean is_cr = FALSE;
	gboolean last_was_cr = FALSE;
	int c = 0;
	char *cs;

	cs = buf;
	while (--size > 0 && (c = getc(stream)) != EOF)
	{
		*cs++ = c;
		is_cr = (c == '\r');
		if (c == '\n') {
			break;
		}
		if (last_was_cr) {
			*(--cs) = '\n';
			cs++;
			ungetc(c, stream);
			break;
		}
		last_was_cr = is_cr;
	}
	if (c == EOF && cs == buf)
		return NULL;

	*cs = '\0';

	return buf;
}

static gint execute_async(gchar *const argv[], const gchar *working_directory)
{
	cm_return_val_if_fail(argv != NULL && argv[0] != NULL, -1);

	if (g_spawn_async(working_directory, (gchar **)argv, NULL, G_SPAWN_SEARCH_PATH,
			  NULL, NULL, NULL, FALSE) == FALSE) {
		g_warning("couldn't execute command: %s", argv[0]);
		return -1;
	}

	return 0;
}

static gint execute_sync(gchar *const argv[], const gchar *working_directory)
{
	gint status;

	cm_return_val_if_fail(argv != NULL && argv[0] != NULL, -1);

#ifdef G_OS_UNIX
	if (g_spawn_sync(working_directory, (gchar **)argv, NULL, G_SPAWN_SEARCH_PATH,
			 NULL, NULL, NULL, NULL, &status, NULL) == FALSE) {
		g_warning("couldn't execute command: %s", argv[0]);
		return -1;
	}

	if (WIFEXITED(status))
		return WEXITSTATUS(status);
	else
		return -1;
#else
	if (g_spawn_sync(working_directory, (gchar **)argv, NULL,
				G_SPAWN_SEARCH_PATH|
				G_SPAWN_CHILD_INHERITS_STDIN|
				G_SPAWN_LEAVE_DESCRIPTORS_OPEN,
			 NULL, NULL, NULL, NULL, &status, NULL) == FALSE) {
		g_warning("couldn't execute command: %s", argv[0]);
		return -1;
	}

	return status;
#endif
}

gint execute_command_line(const gchar *cmdline, gboolean async,
		const gchar *working_directory)
{
	gchar **argv;
	gint ret;

	cm_return_val_if_fail(cmdline != NULL, -1);

	debug_print("execute_command_line(): executing: %s\n", cmdline);

	argv = strsplit_with_quote(cmdline, " ", 0);

	if (async)
		ret = execute_async(argv, working_directory);
	else
		ret = execute_sync(argv, working_directory);

	g_strfreev(argv);

	return ret;
}

gchar *get_command_output(const gchar *cmdline)
{
	gchar *child_stdout;
	gint status;

	cm_return_val_if_fail(cmdline != NULL, NULL);

	debug_print("get_command_output(): executing: %s\n", cmdline);

	if (g_spawn_command_line_sync(cmdline, &child_stdout, NULL, &status,
				      NULL) == FALSE) {
		g_warning("couldn't execute command: %s", cmdline);
		return NULL;
	}

	return child_stdout;
}

FILE *get_command_output_stream(const char* cmdline)
{
    GPid pid;
	GError *err = NULL;
	gchar **argv = NULL;
    int fd;

	cm_return_val_if_fail(cmdline != NULL, NULL);

	debug_print("get_command_output_stream(): executing: %s\n", cmdline);

	/* turn the command-line string into an array */
	if (!g_shell_parse_argv(cmdline, NULL, &argv, &err)) {
		g_warning("could not parse command line from '%s': %s", cmdline, err->message);
        g_error_free(err);
		return NULL;
	}

    if (!g_spawn_async_with_pipes(NULL, argv, NULL, G_SPAWN_SEARCH_PATH,
                                  NULL, NULL, &pid, NULL, &fd, NULL, &err)
        && err)
    {
        g_warning("could not spawn '%s': %s", cmdline, err->message);
        g_error_free(err);
		g_strfreev(argv);
        return NULL;
    }

	g_strfreev(argv);
	return fdopen(fd, "r");
}

#ifndef G_OS_WIN32
static gint is_unchanged_uri_char(char c)
{
	switch (c) {
		case '(':
		case ')':
			return 0;
		default:
			return 1;
	}
}

static void encode_uri(gchar *encoded_uri, gint bufsize, const gchar *uri)
{
	int i;
	int k;

	k = 0;
	for(i = 0; i < strlen(uri) ; i++) {
		if (is_unchanged_uri_char(uri[i])) {
			if (k + 2 >= bufsize)
				break;
			encoded_uri[k++] = uri[i];
		}
		else {
			char * hexa = "0123456789ABCDEF";

			if (k + 4 >= bufsize)
				break;
			encoded_uri[k++] = '%';
			encoded_uri[k++] = hexa[uri[i] / 16];
			encoded_uri[k++] = hexa[uri[i] % 16];
		}
	}
	encoded_uri[k] = 0;
}
#endif

gint open_uri(const gchar *uri, const gchar *cmdline)
{

#ifndef G_OS_WIN32
	gchar buf[BUFFSIZE];
	gchar *p;
	gchar encoded_uri[BUFFSIZE];
	cm_return_val_if_fail(uri != NULL, -1);

	/* an option to choose whether to use encode_uri or not ? */
	encode_uri(encoded_uri, BUFFSIZE, uri);

	if (cmdline &&
	    (p = strchr(cmdline, '%')) && *(p + 1) == 's' &&
	    !strchr(p + 2, '%'))
		g_snprintf(buf, sizeof(buf), cmdline, encoded_uri);
	else {
		if (cmdline)
			g_warning("Open URI command-line is invalid "
				  "(there must be only one '%%s'): %s",
				  cmdline);
		g_snprintf(buf, sizeof(buf), DEFAULT_BROWSER_CMD, encoded_uri);
	}

	execute_command_line(buf, TRUE, NULL);
#else
	ShellExecute(NULL, "open", uri, NULL, NULL, SW_SHOW);
#endif
	return 0;
}

gint open_txt_editor(const gchar *filepath, const gchar *cmdline)
{
	gchar buf[BUFFSIZE];
	gchar *p;

	cm_return_val_if_fail(filepath != NULL, -1);

	if (cmdline &&
	    (p = strchr(cmdline, '%')) && *(p + 1) == 's' &&
	    !strchr(p + 2, '%'))
		g_snprintf(buf, sizeof(buf), cmdline, filepath);
	else {
		if (cmdline)
			g_warning("Open Text Editor command-line is invalid "
				  "(there must be only one '%%s'): %s",
				  cmdline);
		g_snprintf(buf, sizeof(buf), DEFAULT_EDITOR_CMD, filepath);
	}

	execute_command_line(buf, TRUE, NULL);

	return 0;
}

time_t remote_tzoffset_sec(const gchar *zone)
{
	static gchar ustzstr[] = "PSTPDTMSTMDTCSTCDTESTEDT";
	gchar zone3[4];
	gchar *p;
	gchar c;
	gint iustz;
	gint offset;
	time_t remoteoffset;

	strncpy(zone3, zone, 3);
	zone3[3] = '\0';
	remoteoffset = 0;

	if (sscanf(zone, "%c%d", &c, &offset) == 2 &&
	    (c == '+' || c == '-')) {
		remoteoffset = ((offset / 100) * 60 + (offset % 100)) * 60;
		if (c == '-')
			remoteoffset = -remoteoffset;
	} else if (!strncmp(zone, "UT" , 2) ||
		   !strncmp(zone, "GMT", 3)) {
		remoteoffset = 0;
	} else if (strlen(zone3) == 3) {
		for (p = ustzstr; *p != '\0'; p += 3) {
			if (!g_ascii_strncasecmp(p, zone3, 3)) {
				iustz = ((gint)(p - ustzstr) / 3 + 1) / 2 - 8;
				remoteoffset = iustz * 3600;
				break;
			}
		}
		if (*p == '\0')
			return -1;
	} else if (strlen(zone3) == 1) {
		switch (zone[0]) {
		case 'Z': remoteoffset =   0; break;
		case 'A': remoteoffset =  -1; break;
		case 'B': remoteoffset =  -2; break;
		case 'C': remoteoffset =  -3; break;
		case 'D': remoteoffset =  -4; break;
		case 'E': remoteoffset =  -5; break;
		case 'F': remoteoffset =  -6; break;
		case 'G': remoteoffset =  -7; break;
		case 'H': remoteoffset =  -8; break;
		case 'I': remoteoffset =  -9; break;
		case 'K': remoteoffset = -10; break; /* J is not used */
		case 'L': remoteoffset = -11; break;
		case 'M': remoteoffset = -12; break;
		case 'N': remoteoffset =   1; break;
		case 'O': remoteoffset =   2; break;
		case 'P': remoteoffset =   3; break;
		case 'Q': remoteoffset =   4; break;
		case 'R': remoteoffset =   5; break;
		case 'S': remoteoffset =   6; break;
		case 'T': remoteoffset =   7; break;
		case 'U': remoteoffset =   8; break;
		case 'V': remoteoffset =   9; break;
		case 'W': remoteoffset =  10; break;
		case 'X': remoteoffset =  11; break;
		case 'Y': remoteoffset =  12; break;
		default:  remoteoffset =   0; break;
		}
		remoteoffset = remoteoffset * 3600;
	} else
		return -1;

	return remoteoffset;
}

time_t tzoffset_sec(time_t *now)
{
	struct tm gmt, *lt;
	gint off;
	struct tm buf1, buf2;
#ifdef G_OS_WIN32
	if (now && *now < 0)
		return 0;
#endif
	gmt = *gmtime_r(now, &buf1);
	lt = localtime_r(now, &buf2);

	off = (lt->tm_hour - gmt.tm_hour) * 60 + lt->tm_min - gmt.tm_min;

	if (lt->tm_year < gmt.tm_year)
		off -= 24 * 60;
	else if (lt->tm_year > gmt.tm_year)
		off += 24 * 60;
	else if (lt->tm_yday < gmt.tm_yday)
		off -= 24 * 60;
	else if (lt->tm_yday > gmt.tm_yday)
		off += 24 * 60;

	if (off >= 24 * 60)		/* should be impossible */
		off = 23 * 60 + 59;	/* if not, insert silly value */
	if (off <= -24 * 60)
		off = -(23 * 60 + 59);

	return off * 60;
}

/* calculate timezone offset */
gchar *tzoffset(time_t *now)
{
	static gchar offset_string[6];
	struct tm gmt, *lt;
	gint off;
	gchar sign = '+';
	struct tm buf1, buf2;
#ifdef G_OS_WIN32
	if (now && *now < 0)
		return 0;
#endif
	gmt = *gmtime_r(now, &buf1);
	lt = localtime_r(now, &buf2);

	off = (lt->tm_hour - gmt.tm_hour) * 60 + lt->tm_min - gmt.tm_min;

	if (lt->tm_year < gmt.tm_year)
		off -= 24 * 60;
	else if (lt->tm_year > gmt.tm_year)
		off += 24 * 60;
	else if (lt->tm_yday < gmt.tm_yday)
		off -= 24 * 60;
	else if (lt->tm_yday > gmt.tm_yday)
		off += 24 * 60;

	if (off < 0) {
		sign = '-';
		off = -off;
	}

	if (off >= 24 * 60)		/* should be impossible */
		off = 23 * 60 + 59;	/* if not, insert silly value */

	sprintf(offset_string, "%c%02d%02d", sign, off / 60, off % 60);

	return offset_string;
}

static void _get_rfc822_date(gchar *buf, gint len, gboolean hidetz)
{
	struct tm *lt;
	time_t t;
	gchar day[4], mon[4];
	gint dd, hh, mm, ss, yyyy;
	struct tm buf1;
	gchar buf2[RFC822_DATE_BUFFSIZE];

	t = time(NULL);
	if (hidetz)
		lt = gmtime_r(&t, &buf1);
	else
		lt = localtime_r(&t, &buf1);

	if (sscanf(asctime_r(lt, buf2), "%3s %3s %d %d:%d:%d %d\n",
	       day, mon, &dd, &hh, &mm, &ss, &yyyy) != 7)
		g_warning("failed reading date/time");

	g_snprintf(buf, len, "%s, %d %s %d %02d:%02d:%02d %s",
		   day, dd, mon, yyyy, hh, mm, ss, (hidetz? "-0000": tzoffset(&t)));
}

void get_rfc822_date(gchar *buf, gint len)
{
	_get_rfc822_date(buf, len, FALSE);
}

void get_rfc822_date_hide_tz(gchar *buf, gint len)
{
	_get_rfc822_date(buf, len, TRUE);
}

void debug_set_mode(gboolean mode)
{
	debug_mode = mode;
}

gboolean debug_get_mode(void)
{
	return debug_mode;
}

#ifdef HAVE_VA_OPT
void debug_print_real(const char *file, int line, const gchar *format, ...)
{
	va_list args;
	gchar buf[BUFFSIZE];
	gint prefix_len;

	if (!debug_mode) return;

	prefix_len = g_snprintf(buf, sizeof(buf), "%s:%d:", debug_srcname(file), line);

	va_start(args, format);
	g_vsnprintf(buf + prefix_len, sizeof(buf) - prefix_len, format, args);
	va_end(args);

	g_print("%s", buf);
}
#else
void debug_print_real(const gchar *format, ...)
{
	va_list args;
	gchar buf[BUFFSIZE];

	if (!debug_mode) return;

	va_start(args, format);
	g_vsnprintf(buf, sizeof(buf), format, args);
	va_end(args);

	g_print("%s", buf);
}
#endif


const char * debug_srcname(const char *file)
{
	const char *s = strrchr (file, '/');
	return s? s+1:file;
}


void * subject_table_lookup(GHashTable *subject_table, gchar * subject)
{
	if (subject == NULL)
		subject = "";
	else
		subject += subject_get_prefix_length(subject);

	return g_hash_table_lookup(subject_table, subject);
}

void subject_table_insert(GHashTable *subject_table, gchar * subject,
			  void * data)
{
	if (subject == NULL || *subject == 0)
		return;
	subject += subject_get_prefix_length(subject);
	g_hash_table_insert(subject_table, subject, data);
}

void subject_table_remove(GHashTable *subject_table, gchar * subject)
{
	if (subject == NULL)
		return;

	subject += subject_get_prefix_length(subject);
	g_hash_table_remove(subject_table, subject);
}

static regex_t u_regex;
static gboolean u_init_;

void utils_free_regex(void)
{
	if (u_init_) {
		regfree(&u_regex);
		u_init_ = FALSE;
	}
}

/*!
 *\brief	Check if a string is prefixed with known (combinations)
 *		of prefixes. The function assumes that each prefix
 *		is terminated by zero or exactly _one_ space.
 *
 *\param	str String to check for a prefixes
 *
 *\return	int Number of chars in the prefix that should be skipped
 *		for a "clean" subject line. If no prefix was found, 0
 *		is returned.
 */
int subject_get_prefix_length(const gchar *subject)
{
	/*!< Array with allowable reply prefixes regexps. */
	static const gchar * const prefixes[] = {
		"Re\\:",			/* "Re:" */
		"Re\\[[1-9][0-9]*\\]\\:",	/* "Re[XXX]:" (non-conforming news mail clients) */
		"Antw\\:",			/* "Antw:" (Dutch / German Outlook) */
		"Aw\\:",			/* "Aw:"   (German) */
		"Antwort\\:",			/* "Antwort:" (German Lotus Notes) */
		"Res\\:",			/* "Res:" (Spanish/Brazilian Outlook) */
		"Fw\\:",			/* "Fw:" Forward */
		"Fwd\\:",			/* "Fwd:" Forward */
		"Enc\\:",			/* "Enc:" Forward (Brazilian Outlook) */
		"Odp\\:",			/* "Odp:" Re (Polish Outlook) */
		"Rif\\:",			/* "Rif:" (Italian Outlook) */
		"Sv\\:",			/* "Sv" (Norwegian) */
		"Vs\\:",			/* "Vs" (Norwegian) */
		"Ad\\:",			/* "Ad" (Norwegian) */
		"\347\255\224\345\244\215\\:",	/* "Re" (Chinese, UTF-8) */
		"R\303\251f\\. \\:",		/* "R�f. :" (French Lotus Notes) */
		"Re \\:",			/* "Re :" (French Yahoo Mail) */
		/* add more */
	};
	const int PREFIXES = sizeof prefixes / sizeof prefixes[0];
	int n;
	regmatch_t pos;

	if (!subject) return 0;
	if (!*subject) return 0;

	if (!u_init_) {
		GString *s = g_string_new("");

		for (n = 0; n < PREFIXES; n++)
			/* Terminate each prefix regexpression by a
			 * "\ ?" (zero or ONE space), and OR them */
			g_string_append_printf(s, "(%s\\ ?)%s",
					  prefixes[n],
					  n < PREFIXES - 1 ?
					  "|" : "");

		g_string_prepend(s, "(");
		g_string_append(s, ")+");	/* match at least once */
		g_string_prepend(s, "^\\ *");	/* from beginning of line */


		/* We now have something like "^\ *((PREFIX1\ ?)|(PREFIX2\ ?))+"
		 * TODO: Should this be       "^\ *(((PREFIX1)|(PREFIX2))\ ?)+" ??? */
		if (regcomp(&u_regex, s->str, REG_EXTENDED | REG_ICASE)) {
			debug_print("Error compiling regexp %s\n", s->str);
			g_string_free(s, TRUE);
			return 0;
		} else {
			u_init_ = TRUE;
			g_string_free(s, TRUE);
		}
	}

	if (!regexec(&u_regex, subject, 1, &pos, 0) && pos.rm_so != -1)
		return pos.rm_eo;
	else
		return 0;
}

static guint g_stricase_hash(gconstpointer gptr)
{
	guint hash_result = 0;
	const char *str;

	for (str = gptr; str && *str; str++) {
		hash_result += toupper(*str);
	}

	return hash_result;
}

static gint g_stricase_equal(gconstpointer gptr1, gconstpointer gptr2)
{
	const char *str1 = gptr1;
	const char *str2 = gptr2;

	return !strcasecmp(str1, str2);
}

gint g_int_compare(gconstpointer a, gconstpointer b)
{
	return GPOINTER_TO_INT(a) - GPOINTER_TO_INT(b);
}

/*
   quote_cmd_argument()

   return a quoted string safely usable in argument of a command.

   code is extracted and adapted from etPan! project -- DINH V. Ho�.
*/

gint quote_cmd_argument(gchar * result, guint size,
			const gchar * path)
{
	const gchar * p;
	gchar * result_p;
	guint remaining;

	result_p = result;
	remaining = size;

	for(p = path ; * p != '\0' ; p ++) {

		if (isalnum((guchar)*p) || (* p == '/')) {
			if (remaining > 0) {
				* result_p = * p;
				result_p ++;
				remaining --;
			}
			else {
				result[size - 1] = '\0';
				return -1;
			}
		}
		else {
			if (remaining >= 2) {
				* result_p = '\\';
				result_p ++;
				* result_p = * p;
				result_p ++;
				remaining -= 2;
			}
			else {
				result[size - 1] = '\0';
				return -1;
			}
		}
	}
	if (remaining > 0) {
		* result_p = '\0';
	}
	else {
		result[size - 1] = '\0';
		return -1;
	}

	return 0;
}

typedef struct
{
	GNode 		*parent;
	GNodeMapFunc	 func;
	gpointer	 data;
} GNodeMapData;

static void g_node_map_recursive(GNode *node, gpointer data)
{
	GNodeMapData *mapdata = (GNodeMapData *) data;
	GNode *newnode;
	GNodeMapData newmapdata;
	gpointer newdata;

	newdata = mapdata->func(node->data, mapdata->data);
	if (newdata != NULL) {
		newnode = g_node_new(newdata);
		g_node_append(mapdata->parent, newnode);

		newmapdata.parent = newnode;
		newmapdata.func = mapdata->func;
		newmapdata.data = mapdata->data;

		g_node_children_foreach(node, G_TRAVERSE_ALL, g_node_map_recursive, &newmapdata);
	}
}

GNode *g_node_map(GNode *node, GNodeMapFunc func, gpointer data)
{
	GNode *root;
	GNodeMapData mapdata;

	cm_return_val_if_fail(node != NULL, NULL);
	cm_return_val_if_fail(func != NULL, NULL);

	root = g_node_new(func(node->data, data));

	mapdata.parent = root;
	mapdata.func = func;
	mapdata.data = data;

	g_node_children_foreach(node, G_TRAVERSE_ALL, g_node_map_recursive, &mapdata);

	return root;
}

#define HEX_TO_INT(val, hex)			\
{						\
	gchar c = hex;				\
						\
	if ('0' <= c && c <= '9') {		\
		val = c - '0';			\
	} else if ('a' <= c && c <= 'f') {	\
		val = c - 'a' + 10;		\
	} else if ('A' <= c && c <= 'F') {	\
		val = c - 'A' + 10;		\
	} else {				\
		val = -1;			\
	}					\
}

gboolean get_hex_value(guchar *out, gchar c1, gchar c2)
{
	gint hi, lo;

	HEX_TO_INT(hi, c1);
	HEX_TO_INT(lo, c2);

	if (hi == -1 || lo == -1)
		return FALSE;

	*out = (hi << 4) + lo;
	return TRUE;
}

#define INT_TO_HEX(hex, val)		\
{					\
	if ((val) < 10)			\
		hex = '0' + (val);	\
	else				\
		hex = 'A' + (val) - 10;	\
}

void get_hex_str(gchar *out, guchar ch)
{
	gchar hex;

	INT_TO_HEX(hex, ch >> 4);
	*out++ = hex;
	INT_TO_HEX(hex, ch & 0x0f);
	*out   = hex;
}

#undef REF_DEBUG
#ifndef REF_DEBUG
#define G_PRINT_REF 1 == 1 ? (void) 0 : (void)
#else
#define G_PRINT_REF g_print
#endif

/*!
 *\brief	Register ref counted pointer. It is based on GBoxed, so should
 *		work with anything that uses the GType system. The semantics
 *		are similar to a C++ auto pointer, with the exception that
 *		C doesn't have automatic closure (calling destructors) when
 *		exiting a block scope.
 *		Use the \ref G_TYPE_AUTO_POINTER macro instead of calling this
 *		function directly.
 *
 *\return	GType A GType type.
 */
GType g_auto_pointer_register(void)
{
	static GType auto_pointer_type;
	if (!auto_pointer_type)
		auto_pointer_type =
			g_boxed_type_register_static
				("G_TYPE_AUTO_POINTER",
				 (GBoxedCopyFunc) g_auto_pointer_copy,
				 (GBoxedFreeFunc) g_auto_pointer_free);
	return auto_pointer_type;
}

/*!
 *\brief	Structure with g_new() allocated pointer guarded by the
 *		auto pointer
 */
typedef struct AutoPointerRef {
	void	      (*free) (gpointer);
	gpointer	pointer;
	glong		cnt;
} AutoPointerRef;

/*!
 *\brief	The auto pointer opaque structure that references the
 *		pointer guard block.
 */
typedef struct AutoPointer {
	AutoPointerRef *ref;
	gpointer	ptr; /*!< access to protected pointer */
} AutoPointer;

/*!
 *\brief	Creates an auto pointer for a g_new()ed pointer. Example:
 *
 *\code
 *
 *		... tell gtk_list_store it should use a G_TYPE_AUTO_POINTER
 *		... when assigning, copying and freeing storage elements
 *
 *		gtk_list_store_new(N_S_COLUMNS,
 *				   G_TYPE_AUTO_POINTER,
 *				   -1);
 *
 *
 *		Template *precious_data = g_new0(Template, 1);
 *		g_pointer protect = g_auto_pointer_new(precious_data);
 *
 *		gtk_list_store_set(container, &iter,
 *				   S_DATA, protect,
 *				   -1);
 *
 *		... the gtk_list_store has copied the pointer and
 *		... incremented its reference count, we should free
 *		... the auto pointer (in C++ a destructor would do
 *		... this for us when leaving block scope)
 *
 *		g_auto_pointer_free(protect);
 *
 *		... gtk_list_store_set() now manages the data. When
 *		... *explicitly* requesting a pointer from the list
 *		... store, don't forget you get a copy that should be
 *		... freed with g_auto_pointer_free() eventually.
 *
 *\endcode
 *
 *\param	pointer Pointer to be guarded.
 *
 *\return	GAuto * Pointer that should be used in containers with
 *		GType support.
 */
GAuto *g_auto_pointer_new(gpointer p)
{
	AutoPointerRef *ref;
	AutoPointer    *ptr;

	if (p == NULL)
		return NULL;

	ref = g_new0(AutoPointerRef, 1);
	ptr = g_new0(AutoPointer, 1);

	ref->pointer = p;
	ref->free = g_free;
	ref->cnt = 1;

	ptr->ref = ref;
	ptr->ptr = p;

#ifdef REF_DEBUG
	G_PRINT_REF ("XXXX ALLOC(%lx)\n", p);
#endif
	return ptr;
}

/*!
 *\brief	Allocate an autopointer using the passed \a free function to
 *		free the guarded pointer
 */
GAuto *g_auto_pointer_new_with_free(gpointer p, GFreeFunc free_)
{
	AutoPointer *aptr;

	if (p == NULL)
		return NULL;

	aptr = g_auto_pointer_new(p);
	aptr->ref->free = free_;
	return aptr;
}

gpointer g_auto_pointer_get_ptr(GAuto *auto_ptr)
{
	if (auto_ptr == NULL)
		return NULL;
	return ((AutoPointer *) auto_ptr)->ptr;
}

/*!
 *\brief	Copies an auto pointer by. It's mostly not necessary
 *		to call this function directly, unless you copy/assign
 *		the guarded pointer.
 *
 *\param	auto_ptr Auto pointer returned by previous call to
 *		g_auto_pointer_new_XXX()
 *
 *\return	gpointer An auto pointer
 */
GAuto *g_auto_pointer_copy(GAuto *auto_ptr)
{
	AutoPointer	*ptr;
	AutoPointerRef	*ref;
	AutoPointer	*newp;

	if (auto_ptr == NULL)
		return NULL;

	ptr = auto_ptr;
	ref = ptr->ref;
	newp = g_new0(AutoPointer, 1);

	newp->ref = ref;
	newp->ptr = ref->pointer;
	++(ref->cnt);

#ifdef REF_DEBUG
	G_PRINT_REF ("XXXX COPY(%lx) -- REF (%d)\n", ref->pointer, ref->cnt);
#endif
	return newp;
}

/*!
 *\brief	Free an auto pointer
 */
void g_auto_pointer_free(GAuto *auto_ptr)
{
	AutoPointer	*ptr;
	AutoPointerRef	*ref;

	if (auto_ptr == NULL)
		return;

	ptr = auto_ptr;
	ref = ptr->ref;

	if (--(ref->cnt) == 0) {
#ifdef REF_DEBUG
		G_PRINT_REF ("XXXX FREE(%lx) -- REF (%d)\n", ref->pointer, ref->cnt);
#endif
		ref->free(ref->pointer);
		g_free(ref);
	}
#ifdef REF_DEBUG
	else
		G_PRINT_REF ("XXXX DEREF(%lx) -- REF (%d)\n", ref->pointer, ref->cnt);
#endif
	g_free(ptr);
}

/* get_uri_part() - retrieves a URI starting from scanpos.
		    Returns TRUE if successful */
gboolean get_uri_part(const gchar *start, const gchar *scanpos,
			     const gchar **bp, const gchar **ep, gboolean hdr)
{
	const gchar *ep_;
	gint parenthese_cnt = 0;

	cm_return_val_if_fail(start != NULL, FALSE);
	cm_return_val_if_fail(scanpos != NULL, FALSE);
	cm_return_val_if_fail(bp != NULL, FALSE);
	cm_return_val_if_fail(ep != NULL, FALSE);

	*bp = scanpos;

	/* find end point of URI */
	for (ep_ = scanpos; *ep_ != '\0'; ep_ = g_utf8_next_char(ep_)) {
		gunichar u = g_utf8_get_char_validated(ep_, -1);
		if (!g_unichar_isgraph(u) ||
		    u == (gunichar)-1 ||
		    strchr("[]{}<>\"", *ep_)) {
			break;
		} else if (strchr("(", *ep_)) {
			parenthese_cnt++;
		} else if (strchr(")", *ep_)) {
			if (parenthese_cnt > 0)
				parenthese_cnt--;
			else
				break;
		}
	}

	/* no punctuation at end of string */

	/* FIXME: this stripping of trailing punctuations may bite with other URIs.
	 * should pass some URI type to this function and decide on that whether
	 * to perform punctuation stripping */

#define IS_REAL_PUNCT(ch)	(g_ascii_ispunct(ch) && !strchr("$/?=-_~)", ch))

	for (; ep_ - 1 > scanpos + 1 &&
	       IS_REAL_PUNCT(*(ep_ - 1));
	     ep_--)
		;

#undef IS_REAL_PUNCT

	*ep = ep_;

	return TRUE;
}

gchar *make_uri_string(const gchar *bp, const gchar *ep)
{
	while (bp && *bp && g_ascii_isspace(*bp))
		bp++;
	return g_strndup(bp, ep - bp);
}

/* valid mail address characters */
#define IS_RFC822_CHAR(ch) \
	(IS_ASCII(ch) && \
	 (ch) > 32   && \
	 (ch) != 127 && \
	 !g_ascii_isspace(ch) && \
	 !strchr("(),;<>\"", (ch)))

/* alphabet and number within 7bit ASCII */
#define IS_ASCII_ALNUM(ch)	(IS_ASCII(ch) && g_ascii_isalnum(ch))
#define IS_QUOTE(ch) ((ch) == '\'' || (ch) == '"')

static GHashTable *create_domain_tab(void)
{
	gint n;
	GHashTable *htab = g_hash_table_new(g_stricase_hash, g_stricase_equal);

	cm_return_val_if_fail(htab, NULL);
	for (n = 0; n < sizeof toplvl_domains / sizeof toplvl_domains[0]; n++)
		g_hash_table_insert(htab, (gpointer) toplvl_domains[n], (gpointer) toplvl_domains[n]);
	return htab;
}

static gboolean is_toplvl_domain(GHashTable *tab, const gchar *first, const gchar *last)
{
	gchar buf[BUFFSIZE + 1];
	const gchar *m = buf + BUFFSIZE + 1;
	register gchar *p;

	if (last - first > BUFFSIZE || first > last)
		return FALSE;

	for (p = buf; p < m &&  first < last; *p++ = *first++)
		;
	*p = 0;

	return g_hash_table_lookup(tab, buf) != NULL;
}

/* get_email_part() - retrieves an email address. Returns TRUE if successful */
gboolean get_email_part(const gchar *start, const gchar *scanpos,
			       const gchar **bp, const gchar **ep, gboolean hdr)
{
	/* more complex than the uri part because we need to scan back and forward starting from
	 * the scan position. */
	gboolean result = FALSE;
	const gchar *bp_ = NULL;
	const gchar *ep_ = NULL;
	static GHashTable *dom_tab;
	const gchar *last_dot = NULL;
	const gchar *prelast_dot = NULL;
	const gchar *last_tld_char = NULL;

	/* the informative part of the email address (describing the name
	 * of the email address owner) may contain quoted parts. the
	 * closure stack stores the last encountered quotes. */
	gchar closure_stack[128];
	gchar *ptr = closure_stack;

	cm_return_val_if_fail(start != NULL, FALSE);
	cm_return_val_if_fail(scanpos != NULL, FALSE);
	cm_return_val_if_fail(bp != NULL, FALSE);
	cm_return_val_if_fail(ep != NULL, FALSE);

	if (hdr) {
		const gchar *start_quote = NULL;
		const gchar *end_quote = NULL;
search_again:
		/* go to the real start */
		if (start[0] == ',')
			start++;
		if (start[0] == ';')
			start++;
		while (start[0] == '\n' || start[0] == '\r')
			start++;
		while (start[0] == ' ' || start[0] == '\t')
			start++;

		*bp = start;

		/* check if there are quotes (to skip , in them) */
		if (*start == '"') {
			start_quote = start;
			start++;
			end_quote = strstr(start, "\"");
		} else {
			start_quote = NULL;
			end_quote = NULL;
		}

		/* skip anything between quotes */
		if (start_quote && end_quote) {
			start = end_quote;

		}

		/* find end (either , or ; or end of line) */
		if (strstr(start, ",") && strstr(start, ";"))
			*ep = strstr(start,",") < strstr(start, ";")
				? strstr(start, ",") : strstr(start, ";");
		else if (strstr(start, ","))
			*ep = strstr(start, ",");
		else if (strstr(start, ";"))
			*ep = strstr(start, ";");
		else
			*ep = start+strlen(start);

		/* go back to real start */
		if (start_quote && end_quote) {
			start = start_quote;
		}

		/* check there's still an @ in that, or search
		 * further if possible */
		if (strstr(start, "@") && strstr(start, "@") < *ep)
			return TRUE;
		else if (*ep < start+strlen(start)) {
			start = *ep;
			goto search_again;
		} else if (start_quote && strstr(start, "\"") && strstr(start, "\"") < *ep) {
			*bp = start_quote;
			return TRUE;
		} else
			return FALSE;
	}

	if (!dom_tab)
		dom_tab = create_domain_tab();
	cm_return_val_if_fail(dom_tab, FALSE);

	/* scan start of address */
	for (bp_ = scanpos - 1;
	     bp_ >= start && IS_RFC822_CHAR(*(const guchar *)bp_); bp_--)
		;

	/* TODO: should start with an alnum? */
	bp_++;
	for (; bp_ < scanpos && !IS_ASCII_ALNUM(*(const guchar *)bp_); bp_++)
		;

	if (bp_ != scanpos) {
		/* scan end of address */
		for (ep_ = scanpos + 1;
		     *ep_ && IS_RFC822_CHAR(*(const guchar *)ep_); ep_++)
			if (*ep_ == '.') {
				prelast_dot = last_dot;
				last_dot = ep_;
		 		if (*(last_dot + 1) == '.') {
					if (prelast_dot == NULL)
						return FALSE;
					last_dot = prelast_dot;
					break;
				}
			}

		/* TODO: really should terminate with an alnum? */
		for (; ep_ > scanpos && !IS_ASCII_ALNUM(*(const guchar *)ep_);
		     --ep_)
			;
		ep_++;

		if (last_dot == NULL)
			return FALSE;
		if (last_dot >= ep_)
			last_dot = prelast_dot;
		if (last_dot == NULL || (scanpos + 1 >= last_dot))
			return FALSE;
		last_dot++;

		for (last_tld_char = last_dot; last_tld_char < ep_; last_tld_char++)
			if (*last_tld_char == '?')
				break;

		if (is_toplvl_domain(dom_tab, last_dot, last_tld_char))
			result = TRUE;

		*ep = ep_;
		*bp = bp_;
	}

	if (!result) return FALSE;

	if (*ep_ && bp_ != start && *(bp_ - 1) == '"' && *(ep_) == '"'
	&& *(ep_ + 1) == ' ' && *(ep_ + 2) == '<'
	&& IS_RFC822_CHAR(*(ep_ + 3))) {
		/* this informative part with an @ in it is
		 * followed by the email address */
		ep_ += 3;

		/* go to matching '>' (or next non-rfc822 char, like \n) */
		for (; *ep_ != '>' && *ep_ != '\0' && IS_RFC822_CHAR(*ep_); ep_++)
			;

		/* include the bracket */
		if (*ep_ == '>') ep_++;

		/* include the leading quote */
		bp_--;

		*ep = ep_;
		*bp = bp_;
		return TRUE;
	}

	/* skip if it's between quotes "'alfons@proteus.demon.nl'" <alfons@proteus.demon.nl> */
	if (bp_ - 1 > start && IS_QUOTE(*(bp_ - 1)) && IS_QUOTE(*ep_))
		return FALSE;

	/* see if this is <bracketed>; in this case we also scan for the informative part. */
	if (bp_ - 1 <= start || *(bp_ - 1) != '<' || *ep_ != '>')
		return TRUE;

#define FULL_STACK()	((size_t) (ptr - closure_stack) >= sizeof closure_stack)
#define IN_STACK()	(ptr > closure_stack)
/* has underrun check */
#define POP_STACK()	if(IN_STACK()) --ptr
/* has overrun check */
#define PUSH_STACK(c)	if(!FULL_STACK()) *ptr++ = (c); else return TRUE
/* has underrun check */
#define PEEK_STACK()	(IN_STACK() ? *(ptr - 1) : 0)

	ep_++;

	/* scan for the informative part. */
	for (bp_ -= 2; bp_ >= start; bp_--) {
		/* if closure on the stack keep scanning */
		if (PEEK_STACK() == *bp_) {
			POP_STACK();
			continue;
		}
		if (!IN_STACK() && (*bp_ == '\'' || *bp_ == '"')) {
			PUSH_STACK(*bp_);
			continue;
		}

		/* if nothing in the closure stack, do the special conditions
		 * the following if..else expression simply checks whether
		 * a token is acceptable. if not acceptable, the clause
		 * should terminate the loop with a 'break' */
		if (!PEEK_STACK()) {
			if (*bp_ == '-'
			&& (((bp_ - 1) >= start) && isalnum(*(bp_ - 1)))
			&& (((bp_ + 1) < ep_)    && isalnum(*(bp_ + 1)))) {
				/* hyphens are allowed, but only in
				   between alnums */
			} else if (strchr(" \"'", *bp_)) {
				/* but anything not being a punctiation
				   is ok */
			} else {
				break; /* anything else is rejected */
			}
		}
	}

	bp_++;

	/* scan forward (should start with an alnum) */
	for (; *bp_ != '<' && isspace(*bp_) && *bp_ != '"'; bp_++)
		;
#undef PEEK_STACK
#undef PUSH_STACK
#undef POP_STACK
#undef IN_STACK
#undef FULL_STACK


	*bp = bp_;
	*ep = ep_;

	return result;
}

#undef IS_QUOTE
#undef IS_ASCII_ALNUM
#undef IS_RFC822_CHAR

gchar *make_email_string(const gchar *bp, const gchar *ep)
{
	/* returns a mailto: URI; mailto: is also used to detect the
	 * uri type later on in the button_pressed signal handler */
	gchar *tmp;
	gchar *result;
	gchar *colon, *at;

	tmp = g_strndup(bp, ep - bp);

	/* If there is a colon in the username part of the address,
	 * we're dealing with an URI for some other protocol - do
	 * not prefix with mailto: in such case. */
	colon = strchr(tmp, ':');
	at = strchr(tmp, '@');
	if (colon != NULL && at != NULL && colon < at) {
		result = tmp;
	} else {
		result = g_strconcat("mailto:", tmp, NULL);
		g_free(tmp);
	}

	return result;
}

gchar *make_http_string(const gchar *bp, const gchar *ep)
{
	/* returns an http: URI; */
	gchar *tmp;
	gchar *result;

	while (bp && *bp && g_ascii_isspace(*bp))
		bp++;
	tmp = g_strndup(bp, ep - bp);
	result = g_strconcat("http://", tmp, NULL);
	g_free(tmp);

	return result;
}

static gchar *mailcap_get_command_in_file(const gchar *path, const gchar *type, const gchar *file_to_open)
{
	FILE *fp = claws_fopen(path, "rb");
	gchar buf[BUFFSIZE];
	gchar *result = NULL;
	if (!fp)
		return NULL;
	while (claws_fgets(buf, sizeof (buf), fp) != NULL) {
		gchar **parts = g_strsplit(buf, ";", 3);
		gchar *trimmed = parts[0];
		while (trimmed[0] == ' ' || trimmed[0] == '\t')
			trimmed++;
		while (trimmed[strlen(trimmed)-1] == ' ' || trimmed[strlen(trimmed)-1] == '\t')
			trimmed[strlen(trimmed)-1] = '\0';

		if (!strcmp(trimmed, type)) {
			gboolean needsterminal = FALSE;
			if (parts[2] && strstr(parts[2], "needsterminal")) {
				needsterminal = TRUE;
			}
			if (parts[2] && strstr(parts[2], "test=")) {
				gchar *orig_testcmd = g_strdup(strstr(parts[2], "test=")+5);
				gchar *testcmd = orig_testcmd;
				if (strstr(testcmd,";"))
					*(strstr(testcmd,";")) = '\0';
				while (testcmd[0] == ' ' || testcmd[0] == '\t')
					testcmd++;
				while (testcmd[strlen(testcmd)-1] == '\n')
					testcmd[strlen(testcmd)-1] = '\0';
				while (testcmd[strlen(testcmd)-1] == '\r')
					testcmd[strlen(testcmd)-1] = '\0';
				while (testcmd[strlen(testcmd)-1] == ' ' || testcmd[strlen(testcmd)-1] == '\t')
					testcmd[strlen(testcmd)-1] = '\0';

				if (strstr(testcmd, "%s")) {
					gchar *tmp = g_strdup_printf(testcmd, file_to_open);
					gint res = system(tmp);
					g_free(tmp);
					g_free(orig_testcmd);

					if (res != 0) {
						g_strfreev(parts);
						continue;
					}
				} else {
					gint res = system(testcmd);
					g_free(orig_testcmd);

					if (res != 0) {
						g_strfreev(parts);
						continue;
					}
				}
			}

			trimmed = parts[1];
			while (trimmed[0] == ' ' || trimmed[0] == '\t')
				trimmed++;
			while (trimmed[strlen(trimmed)-1] == '\n')
				trimmed[strlen(trimmed)-1] = '\0';
			while (trimmed[strlen(trimmed)-1] == '\r')
				trimmed[strlen(trimmed)-1] = '\0';
			while (trimmed[strlen(trimmed)-1] == ' ' || trimmed[strlen(trimmed)-1] == '\t')
				trimmed[strlen(trimmed)-1] = '\0';
			result = g_strdup(trimmed);
			g_strfreev(parts);
			claws_fclose(fp);
			if (needsterminal) {
				gchar *tmp = g_strdup_printf("xterm -e %s", result);
				g_free(result);
				result = tmp;
			}
			return result;
		}
		g_strfreev(parts);
	}
	claws_fclose(fp);
	return NULL;
}
gchar *mailcap_get_command_for_type(const gchar *type, const gchar *file_to_open)
{
	gchar *result = NULL;
	gchar *path = NULL;
	if (type == NULL)
		return NULL;
	path = g_strconcat(get_home_dir(), G_DIR_SEPARATOR_S, ".mailcap", NULL);
	result = mailcap_get_command_in_file(path, type, file_to_open);
	g_free(path);
	if (result)
		return result;
	result = mailcap_get_command_in_file("/etc/mailcap", type, file_to_open);
	return result;
}

void mailcap_update_default(const gchar *type, const gchar *command)
{
	gchar *path = NULL, *outpath = NULL;
	path = g_strconcat(get_home_dir(), G_DIR_SEPARATOR_S, ".mailcap", NULL);
	outpath = g_strconcat(get_home_dir(), G_DIR_SEPARATOR_S, ".mailcap.new", NULL);
	FILE *fp = claws_fopen(path, "rb");
	FILE *outfp = NULL;
	gchar buf[BUFFSIZE];
	gboolean err = FALSE;

	if (!fp) {
		fp = claws_fopen(path, "a");
		if (!fp) {
			g_warning("failed to create file %s", path);
			g_free(path);
			g_free(outpath);
			return;
		}
		fp = g_freopen(path, "rb", fp);
		if (!fp) {
			g_warning("failed to reopen file %s", path);
			g_free(path);
			g_free(outpath);
			return;
		}
	}

	outfp = claws_fopen(outpath, "wb");
	if (!outfp) {
		g_warning("failed to create file %s", outpath);
		g_free(path);
		g_free(outpath);
		claws_fclose(fp);
		return;
	}
	while (fp && claws_fgets(buf, sizeof (buf), fp) != NULL) {
		gchar **parts = g_strsplit(buf, ";", 3);
		gchar *trimmed = parts[0];
		while (trimmed[0] == ' ')
			trimmed++;
		while (trimmed[strlen(trimmed)-1] == ' ')
			trimmed[strlen(trimmed)-1] = '\0';

		if (!strcmp(trimmed, type)) {
			g_strfreev(parts);
			continue;
		}
		else {
			if(claws_fputs(buf, outfp) == EOF) {
				err = TRUE;
				g_strfreev(parts);
				break;
			}
		}
		g_strfreev(parts);
	}
	if (fprintf(outfp, "%s; %s\n", type, command) < 0)
		err = TRUE;

	if (fp)
		claws_fclose(fp);

	if (claws_safe_fclose(outfp) == EOF)
		err = TRUE;

	if (!err)
		g_rename(outpath, path);

	g_free(path);
	g_free(outpath);
}

/* crude test to see if a file is an email. */
gboolean file_is_email (const gchar *filename)
{
	FILE *fp = NULL;
	gchar buffer[2048];
	gint score = 0;
	if (filename == NULL)
		return FALSE;
	if ((fp = claws_fopen(filename, "rb")) == NULL)
		return FALSE;
	while (score < 3
	       && claws_fgets(buffer, sizeof (buffer), fp) != NULL) {
		if (!strncmp(buffer, "From:", strlen("From:")))
			score++;
		else if (!strncmp(buffer, "Date:", strlen("Date:")))
			score++;
		else if (!strncmp(buffer, "Message-ID:", strlen("Message-ID:")))
			score++;
		else if (!strncmp(buffer, "Subject:", strlen("Subject:")))
			score++;
		else if (!strcmp(buffer, "\r\n")) {
			debug_print("End of headers\n");
			break;
		}
	}
	claws_fclose(fp);
	return (score >= 3);
}

gboolean sc_g_list_bigger(GList *list, gint max)
{
	GList *cur = list;
	int i = 0;
	while (cur && i <= max+1) {
		i++;
		cur = cur->next;
	}
	return (i > max);
}

gboolean sc_g_slist_bigger(GSList *list, gint max)
{
	GSList *cur = list;
	int i = 0;
	while (cur && i <= max+1) {
		i++;
		cur = cur->next;
	}
	return (i > max);
}

const gchar *daynames[] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL};
const gchar *monthnames[] = {NULL, NULL, NULL, NULL, NULL, NULL,
			     NULL, NULL, NULL, NULL, NULL, NULL};
const gchar *s_daynames[] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL};
const gchar *s_monthnames[] = {NULL, NULL, NULL, NULL, NULL, NULL,
			     NULL, NULL, NULL, NULL, NULL, NULL};

gint daynames_len[] =     {0,0,0,0,0,0,0};
gint monthnames_len[] =   {0,0,0,0,0,0,
				 0,0,0,0,0,0};
gint s_daynames_len[] =   {0,0,0,0,0,0,0};
gint s_monthnames_len[] = {0,0,0,0,0,0,
				 0,0,0,0,0,0};
const gchar *s_am_up = NULL;
const gchar *s_pm_up = NULL;
const gchar *s_am_low = NULL;
const gchar *s_pm_low = NULL;

gint s_am_up_len = 0;
gint s_pm_up_len = 0;
gint s_am_low_len = 0;
gint s_pm_low_len = 0;

static gboolean time_names_init_done = FALSE;

static void init_time_names(void)
{
	int i = 0;

	daynames[0] = C_("Complete day name for use by strftime", "Sunday");
	daynames[1] = C_("Complete day name for use by strftime", "Monday");
	daynames[2] = C_("Complete day name for use by strftime", "Tuesday");
	daynames[3] = C_("Complete day name for use by strftime", "Wednesday");
	daynames[4] = C_("Complete day name for use by strftime", "Thursday");
	daynames[5] = C_("Complete day name for use by strftime", "Friday");
	daynames[6] = C_("Complete day name for use by strftime", "Saturday");

	monthnames[0] = C_("Complete month name for use by strftime", "January");
	monthnames[1] = C_("Complete month name for use by strftime", "February");
	monthnames[2] = C_("Complete month name for use by strftime", "March");
	monthnames[3] = C_("Complete month name for use by strftime", "April");
	monthnames[4] = C_("Complete month name for use by strftime", "May");
	monthnames[5] = C_("Complete month name for use by strftime", "June");
	monthnames[6] = C_("Complete month name for use by strftime", "July");
	monthnames[7] = C_("Complete month name for use by strftime", "August");
	monthnames[8] = C_("Complete month name for use by strftime", "September");
	monthnames[9] = C_("Complete month name for use by strftime", "October");
	monthnames[10] = C_("Complete month name for use by strftime", "November");
	monthnames[11] = C_("Complete month name for use by strftime", "December");

	s_daynames[0] = C_("Abbr. day name for use by strftime", "Sun");
	s_daynames[1] = C_("Abbr. day name for use by strftime", "Mon");
	s_daynames[2] = C_("Abbr. day name for use by strftime", "Tue");
	s_daynames[3] = C_("Abbr. day name for use by strftime", "Wed");
	s_daynames[4] = C_("Abbr. day name for use by strftime", "Thu");
	s_daynames[5] = C_("Abbr. day name for use by strftime", "Fri");
	s_daynames[6] = C_("Abbr. day name for use by strftime", "Sat");

	s_monthnames[0] = C_("Abbr. month name for use by strftime", "Jan");
	s_monthnames[1] = C_("Abbr. month name for use by strftime", "Feb");
	s_monthnames[2] = C_("Abbr. month name for use by strftime", "Mar");
	s_monthnames[3] = C_("Abbr. month name for use by strftime", "Apr");
	s_monthnames[4] = C_("Abbr. month name for use by strftime", "May");
	s_monthnames[5] = C_("Abbr. month name for use by strftime", "Jun");
	s_monthnames[6] = C_("Abbr. month name for use by strftime", "Jul");
	s_monthnames[7] = C_("Abbr. month name for use by strftime", "Aug");
	s_monthnames[8] = C_("Abbr. month name for use by strftime", "Sep");
	s_monthnames[9] = C_("Abbr. month name for use by strftime", "Oct");
	s_monthnames[10] = C_("Abbr. month name for use by strftime", "Nov");
	s_monthnames[11] = C_("Abbr. month name for use by strftime", "Dec");

	for (i = 0; i < 7; i++) {
		daynames_len[i] = strlen(daynames[i]);
		s_daynames_len[i] = strlen(s_daynames[i]);
	}
	for (i = 0; i < 12; i++) {
		monthnames_len[i] = strlen(monthnames[i]);
		s_monthnames_len[i] = strlen(s_monthnames[i]);
	}

	s_am_up = C_("For use by strftime (morning)", "AM");
	s_pm_up = C_("For use by strftime (afternoon)", "PM");
	s_am_low = C_("For use by strftime (morning, lowercase)", "am");
	s_pm_low = C_("For use by strftime (afternoon, lowercase)", "pm");

	s_am_up_len = strlen(s_am_up);
	s_pm_up_len = strlen(s_pm_up);
	s_am_low_len = strlen(s_am_low);
	s_pm_low_len = strlen(s_pm_low);

	time_names_init_done = TRUE;
}

#define CHECK_SIZE() {			\
	total_done += len;		\
	if (total_done >= buflen) {	\
		buf[buflen-1] = '\0';	\
		return 0;		\
	}				\
}

size_t fast_strftime(gchar *buf, gint buflen, const gchar *format, struct tm *lt)
{
	gchar *curpos = buf;
	gint total_done = 0;
	gchar subbuf[64], subfmt[64];
	static time_t last_tzset = (time_t)0;

	if (!time_names_init_done)
		init_time_names();

	if (format == NULL || lt == NULL)
		return 0;

	if (last_tzset != time(NULL)) {
		tzset();
		last_tzset = time(NULL);
	}
	while(*format) {
		if (*format == '%') {
			gint len = 0, tmp = 0;
			format++;
			switch(*format) {
			case '%':
				len = 1; CHECK_SIZE();
				*curpos = '%';
				break;
			case 'a':
				len = s_daynames_len[lt->tm_wday]; CHECK_SIZE();
				strncpy2(curpos, s_daynames[lt->tm_wday], buflen - total_done);
				break;
			case 'A':
				len = daynames_len[lt->tm_wday]; CHECK_SIZE();
				strncpy2(curpos, daynames[lt->tm_wday], buflen - total_done);
				break;
			case 'b':
			case 'h':
				len = s_monthnames_len[lt->tm_mon]; CHECK_SIZE();
				strncpy2(curpos, s_monthnames[lt->tm_mon], buflen - total_done);
				break;
			case 'B':
				len = monthnames_len[lt->tm_mon]; CHECK_SIZE();
				strncpy2(curpos, monthnames[lt->tm_mon], buflen - total_done);
				break;
			case 'c':
				strftime(subbuf, 64, "%c", lt);
				len = strlen(subbuf); CHECK_SIZE();
				strncpy2(curpos, subbuf, buflen - total_done);
				break;
			case 'C':
				total_done += 2; CHECK_SIZE();
				tmp = (lt->tm_year + 1900)/100;
				*curpos++ = '0'+(tmp / 10);
				*curpos++ = '0'+(tmp % 10);
				break;
			case 'd':
				total_done += 2; CHECK_SIZE();
				*curpos++ = '0'+(lt->tm_mday / 10);
				*curpos++ = '0'+(lt->tm_mday % 10);
				break;
			case 'D':
				total_done += 8; CHECK_SIZE();
				*curpos++ = '0'+((lt->tm_mon+1) / 10);
				*curpos++ = '0'+((lt->tm_mon+1) % 10);
				*curpos++ = '/';
				*curpos++ = '0'+(lt->tm_mday / 10);
				*curpos++ = '0'+(lt->tm_mday % 10);
				*curpos++ = '/';
				tmp = lt->tm_year%100;
				*curpos++ = '0'+(tmp / 10);
				*curpos++ = '0'+(tmp % 10);
				break;
			case 'e':
				len = 2; CHECK_SIZE();
				snprintf(curpos, buflen - total_done, "%2d", lt->tm_mday);
				break;
			case 'F':
				len = 10; CHECK_SIZE();
				snprintf(curpos, buflen - total_done, "%4d-%02d-%02d",
					lt->tm_year + 1900, lt->tm_mon +1, lt->tm_mday);
				break;
			case 'H':
				total_done += 2; CHECK_SIZE();
				*curpos++ = '0'+(lt->tm_hour / 10);
				*curpos++ = '0'+(lt->tm_hour % 10);
				break;
			case 'I':
				total_done += 2; CHECK_SIZE();
				tmp = lt->tm_hour;
				if (tmp > 12)
					tmp -= 12;
				else if (tmp == 0)
					tmp = 12;
				*curpos++ = '0'+(tmp / 10);
				*curpos++ = '0'+(tmp % 10);
				break;
			case 'j':
				len = 3; CHECK_SIZE();
				snprintf(curpos, buflen - total_done, "%03d", lt->tm_yday+1);
				break;
			case 'k':
				len = 2; CHECK_SIZE();
				snprintf(curpos, buflen - total_done, "%2d", lt->tm_hour);
				break;
			case 'l':
				len = 2; CHECK_SIZE();
				tmp = lt->tm_hour;
				if (tmp > 12)
					tmp -= 12;
				else if (tmp == 0)
					tmp = 12;
				snprintf(curpos, buflen - total_done, "%2d", tmp);
				break;
			case 'm':
				total_done += 2; CHECK_SIZE();
				tmp = lt->tm_mon + 1;
				*curpos++ = '0'+(tmp / 10);
				*curpos++ = '0'+(tmp % 10);
				break;
			case 'M':
				total_done += 2; CHECK_SIZE();
				*curpos++ = '0'+(lt->tm_min / 10);
				*curpos++ = '0'+(lt->tm_min % 10);
				break;
			case 'n':
				len = 1; CHECK_SIZE();
				*curpos = '\n';
				break;
			case 'p':
				if (lt->tm_hour >= 12) {
					len = s_pm_up_len; CHECK_SIZE();
					snprintf(curpos, buflen-total_done, "%s", s_pm_up);
				} else {
					len = s_am_up_len; CHECK_SIZE();
					snprintf(curpos, buflen-total_done, "%s", s_am_up);
				}
				break;
			case 'P':
				if (lt->tm_hour >= 12) {
					len = s_pm_low_len; CHECK_SIZE();
					snprintf(curpos, buflen-total_done, "%s", s_pm_low);
				} else {
					len = s_am_low_len; CHECK_SIZE();
					snprintf(curpos, buflen-total_done, "%s", s_am_low);
				}
				break;
			case 'r':
#ifdef G_OS_WIN32
				strftime(subbuf, 64, "%I:%M:%S %p", lt);
#else
				strftime(subbuf, 64, "%r", lt);
#endif
				len = strlen(subbuf); CHECK_SIZE();
				strncpy2(curpos, subbuf, buflen - total_done);
				break;
			case 'R':
				total_done += 5; CHECK_SIZE();
				*curpos++ = '0'+(lt->tm_hour / 10);
				*curpos++ = '0'+(lt->tm_hour % 10);
				*curpos++ = ':';
				*curpos++ = '0'+(lt->tm_min / 10);
				*curpos++ = '0'+(lt->tm_min % 10);
				break;
			case 's':
				snprintf(subbuf, 64, "%" CM_TIME_FORMAT, mktime(lt));
				len = strlen(subbuf); CHECK_SIZE();
				strncpy2(curpos, subbuf, buflen - total_done);
				break;
			case 'S':
				total_done += 2; CHECK_SIZE();
				*curpos++ = '0'+(lt->tm_sec / 10);
				*curpos++ = '0'+(lt->tm_sec % 10);
				break;
			case 't':
				len = 1; CHECK_SIZE();
				*curpos = '\t';
				break;
			case 'T':
				total_done += 8; CHECK_SIZE();
				*curpos++ = '0'+(lt->tm_hour / 10);
				*curpos++ = '0'+(lt->tm_hour % 10);
				*curpos++ = ':';
				*curpos++ = '0'+(lt->tm_min / 10);
				*curpos++ = '0'+(lt->tm_min % 10);
				*curpos++ = ':';
				*curpos++ = '0'+(lt->tm_sec / 10);
				*curpos++ = '0'+(lt->tm_sec % 10);
				break;
			case 'u':
				len = 1; CHECK_SIZE();
				snprintf(curpos, buflen - total_done, "%d", lt->tm_wday == 0 ? 7: lt->tm_wday);
				break;
			case 'w':
				len = 1; CHECK_SIZE();
				snprintf(curpos, buflen - total_done, "%d", lt->tm_wday);
				break;
			case 'x':
				strftime(subbuf, 64, "%x", lt);
				len = strlen(subbuf); CHECK_SIZE();
				strncpy2(curpos, subbuf, buflen - total_done);
				break;
			case 'X':
				strftime(subbuf, 64, "%X", lt);
				len = strlen(subbuf); CHECK_SIZE();
				strncpy2(curpos, subbuf, buflen - total_done);
				break;
			case 'y':
				total_done += 2; CHECK_SIZE();
				tmp = lt->tm_year%100;
				*curpos++ = '0'+(tmp / 10);
				*curpos++ = '0'+(tmp % 10);
				break;
			case 'Y':
				len = 4; CHECK_SIZE();
				snprintf(curpos, buflen - total_done, "%4d", lt->tm_year + 1900);
				break;
			case 'G':
			case 'g':
			case 'U':
			case 'V':
			case 'W':
			case 'z':
			case 'Z':
			case '+':
				/* let these complicated ones be done with the libc */
				snprintf(subfmt, 64, "%%%c", *format);
				strftime(subbuf, 64, subfmt, lt);
				len = strlen(subbuf); CHECK_SIZE();
				strncpy2(curpos, subbuf, buflen - total_done);
				break;
			case 'E':
			case 'O':
				/* let these complicated modifiers be done with the libc */
				snprintf(subfmt, 64, "%%%c%c", *format, *(format+1));
				strftime(subbuf, 64, subfmt, lt);
				len = strlen(subbuf); CHECK_SIZE();
				strncpy2(curpos, subbuf, buflen - total_done);
				format++;
				break;
			default:
				g_warning("format error (%c)", *format);
				*curpos = '\0';
				return total_done;
			}
			curpos += len;
			format++;
		} else {
			int len = 1; CHECK_SIZE();
			*curpos++ = *format++;
		}
	}
	*curpos = '\0';
	return total_done;
}

#ifdef G_OS_WIN32
#define WEXITSTATUS(x) (x)
#endif

static gchar *canonical_list_to_file(GSList *list)
{
	GString *result = g_string_new(NULL);
	GSList *pathlist = g_slist_reverse(g_slist_copy(list));
	GSList *cur;

#ifndef G_OS_WIN32
	result = g_string_append(result, G_DIR_SEPARATOR_S);
#else
	if (pathlist->data) {
		const gchar *root = (gchar *)pathlist->data;
		if (root[0] != '\0' && g_ascii_isalpha(root[0]) &&
		    root[1] == ':') {
			/* drive - don't prepend dir separator */
		} else {
			result = g_string_append(result, G_DIR_SEPARATOR_S);
		}
	}
#endif

	for (cur = pathlist; cur; cur = cur->next) {
		result = g_string_append(result, (gchar *)cur->data);
		if (cur->next)
			result = g_string_append(result, G_DIR_SEPARATOR_S);
	}
	g_slist_free(pathlist);

	return g_string_free(result, FALSE);
}

static GSList *cm_split_path(const gchar *filename, int depth)
{
	gchar **path_parts;
	GSList *canonical_parts = NULL;
	GStatBuf st;
	int i;
#ifndef G_OS_WIN32
	gboolean follow_symlinks = TRUE;
#endif

	if (depth > 32) {
#ifndef G_OS_WIN32
		errno = ELOOP;
#else
		errno = EINVAL; /* can't happen, no symlink handling */
#endif
		return NULL;
	}

	if (!g_path_is_absolute(filename)) {
		errno =EINVAL;
		return NULL;
	}

	path_parts = g_strsplit(filename, G_DIR_SEPARATOR_S, -1);

	for (i = 0; path_parts[i] != NULL; i++) {
		if (!strcmp(path_parts[i], ""))
			continue;
		if (!strcmp(path_parts[i], "."))
			continue;
		else if (!strcmp(path_parts[i], "..")) {
			if (i == 0) {
				errno =ENOTDIR;
				g_strfreev(path_parts);
				return NULL;
			}
			else /* Remove the last inserted element */
				canonical_parts =
					g_slist_delete_link(canonical_parts,
							    canonical_parts);
		} else {
			gchar *tmp_path;

			canonical_parts = g_slist_prepend(canonical_parts,
						g_strdup(path_parts[i]));

			tmp_path = canonical_list_to_file(canonical_parts);

			if(g_stat(tmp_path, &st) < 0) {
				if (errno == ENOENT) {
					errno = 0;
#ifndef G_OS_WIN32
					follow_symlinks = FALSE;
#endif
				}
				if (errno != 0) {
					g_free(tmp_path);
					slist_free_strings_full(canonical_parts);
					g_strfreev(path_parts);

					return NULL;
				}
			}
#ifndef G_OS_WIN32
			if (follow_symlinks && g_file_test(tmp_path, G_FILE_TEST_IS_SYMLINK)) {
				GError *error = NULL;
				gchar *target = g_file_read_link(tmp_path, &error);

				if (!g_path_is_absolute(target)) {
					/* remove the last inserted element */
					canonical_parts =
						g_slist_delete_link(canonical_parts,
							    canonical_parts);
					/* add the target */
					canonical_parts = g_slist_prepend(canonical_parts,
						g_strdup(target));
					g_free(target);

					/* and get the new target */
					target = canonical_list_to_file(canonical_parts);
				}

				/* restart from absolute target */
				slist_free_strings_full(canonical_parts);
				canonical_parts = NULL;
				if (!error)
					canonical_parts = cm_split_path(target, depth + 1);
				else
					g_error_free(error);
				if (canonical_parts == NULL) {
					g_free(tmp_path);
					g_strfreev(path_parts);
					return NULL;
				}
				g_free(target);
			}
#endif
			g_free(tmp_path);
		}
	}
	g_strfreev(path_parts);
	return canonical_parts;
}

/*
 * Canonicalize a filename, resolving symlinks along the way.
 * Returns a negative errno in case of error.
 */
int cm_canonicalize_filename(const gchar *filename, gchar **canonical_name) {
	GSList *canonical_parts;
	gboolean is_absolute;

	if (filename == NULL)
		return -EINVAL;
	if (canonical_name == NULL)
		return -EINVAL;
	*canonical_name = NULL;

	is_absolute = g_path_is_absolute(filename);
	if (!is_absolute) {
		/* Always work on absolute filenames. */
		gchar *cur = g_get_current_dir();
		gchar *absolute_filename = g_strconcat(cur, G_DIR_SEPARATOR_S,
						       filename, NULL);

		canonical_parts = cm_split_path(absolute_filename, 0);
		g_free(absolute_filename);
		g_free(cur);
	} else
		canonical_parts = cm_split_path(filename, 0);

	if (canonical_parts == NULL)
		return -errno;

	*canonical_name = canonical_list_to_file(canonical_parts);
	slist_free_strings_full(canonical_parts);
	return 0;
}

/* Returns a decoded base64 string, guaranteed to be null-terminated. */
guchar *g_base64_decode_zero(const gchar *text, gsize *out_len)
{
	gchar *tmp = g_base64_decode(text, out_len);
	gchar *out = g_strndup(tmp, *out_len);

	g_free(tmp);

	if (strlen(out) != *out_len) {
		g_warning("strlen(out) %"G_GSIZE_FORMAT" != *out_len %"G_GSIZE_FORMAT, strlen(out), *out_len);
	}

	return out;
}

/* Attempts to read count bytes from a PRNG into memory area starting at buf.
 * It is up to the caller to make sure there is at least count bytes
 * available at buf. */
gboolean
get_random_bytes(void *buf, size_t count)
{
	/* Open our prng source. */
#if defined G_OS_WIN32
	HCRYPTPROV rnd;

	if (!CryptAcquireContext(&rnd, NULL, NULL, PROV_RSA_FULL, 0) &&
			!CryptAcquireContext(&rnd, NULL, NULL, PROV_RSA_FULL, CRYPT_NEWKEYSET)) {
		debug_print("Could not acquire a CSP handle.\n");
		return FALSE;
	}
#else
	int rnd;
	ssize_t ret;

	rnd = open("/dev/urandom", O_RDONLY);
	if (rnd == -1) {
		FILE_OP_ERROR("/dev/urandom", "open");
		debug_print("Could not open /dev/urandom.\n");
		return FALSE;
	}
#endif

	/* Read data from the source into buf. */
#if defined G_OS_WIN32
	if (!CryptGenRandom(rnd, count, buf)) {
		debug_print("Could not read %"G_GSIZE_FORMAT" random bytes.\n", count);
		CryptReleaseContext(rnd, 0);
		return FALSE;
	}
#else
	ret = read(rnd, buf, count);
	if (ret != count) {
		FILE_OP_ERROR("/dev/urandom", "read");
		debug_print("Could not read enough data from /dev/urandom, read only %ld of %lu bytes.\n", ret, count);
		close(rnd);
		return FALSE;
	}
#endif

	/* Close the prng source. */
#if defined G_OS_WIN32
	CryptReleaseContext(rnd, 0);
#else
	close(rnd);
#endif

	return TRUE;
}

/* returns FALSE if parsing failed, otherwise returns TRUE and sets *server, *port
   and eventually *fp from filename (if not NULL, they must be free'd by caller after
   user.
   filenames we expect: 'host.name.port.cert' or 'host.name.port.f:i:n:g:e:r:p:r:i:n:t.cert' */
gboolean get_serverportfp_from_filename(const gchar *str, gchar **server, gchar **port, gchar **fp)
{
	const gchar *pos, *dotport_pos = NULL, *dotcert_pos = NULL, *dotfp_pos = NULL;

	g_return_val_if_fail(str != NULL, FALSE);

	pos = str + strlen(str) - 1;
	while ((pos > str) && !dotport_pos) {
		if (*pos == '.') {
			if (!dotcert_pos) {
				/* match the .cert suffix */
				if (strcmp(pos, ".cert") == 0) {
					dotcert_pos = pos;
				}
			} else {
				if (!dotfp_pos) {
					/* match an eventual fingerprint */
					/* or the port number */
					if (strncmp(pos + 3, ":", 1) == 0) {
						dotfp_pos = pos;
					} else {
						dotport_pos = pos;
					}
				} else {
					/* match the port number */
					dotport_pos = pos;
				}
			}
		}
		pos--;
	}
	if (!dotport_pos || !dotcert_pos) {
		g_warning("could not parse filename %s", str);
		return FALSE;
	}

	if (server != NULL)
		*server = g_strndup(str, dotport_pos - str);
	if (dotfp_pos) {
		if (port != NULL)
			*port = g_strndup(dotport_pos + 1, dotfp_pos - dotport_pos - 1);
		if (fp != NULL)
			*fp = g_strndup(dotfp_pos + 1, dotcert_pos - dotfp_pos - 1);
	} else {
		if (port != NULL)
			*port = g_strndup(dotport_pos + 1, dotcert_pos - dotport_pos - 1);
		if (fp != NULL)
			*fp = NULL;
	}

	debug_print("filename='%s' => server='%s' port='%s' fp='%s'\n",
			str,
			(server ? *server : "(n/a)"),
			(port ? *port : "(n/a)"),
			(fp ? *fp : "(n/a)"));

	if (!(server && *server) || !(port && *port))
		return FALSE;
	else
		return TRUE;
}

#ifdef G_OS_WIN32
gchar *win32_debug_log_path(void)
{
	return g_strconcat(g_get_tmp_dir(), G_DIR_SEPARATOR_S,
			"claws-win32.log", NULL);
}
#endif
