/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2001 Hiroyuki Yamamoto & The Sylpheed Claws Team
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

/* 
 * initial	Hoa		initial
 *
 * 07/18/01	Alfons		when we want a file name from a MsgInfo, get that
 *				from MsgInfo->folder if the message is being filtered
 *				from incorporation. also some more safe string checking.
 */

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "defs.h"
#include "utils.h"
#include "procheader.h"
#include "matcher.h"
#include "intl.h"

struct _MatchParser {
	gint id;
	gchar * str;
};

typedef struct _MatchParser MatchParser;

static MatchParser matchparser_tab[] = {
	/* msginfo flags */
	{MATCHING_ALL, "all"},
	{MATCHING_UNREAD, "unread"},
	{MATCHING_NOT_UNREAD, "~unread"},
	{MATCHING_NEW, "new"},
	{MATCHING_NOT_NEW, "~new"},
	{MATCHING_MARKED, "marked"},
	{MATCHING_NOT_MARKED, "~marked"},
	{MATCHING_DELETED, "deleted"},
	{MATCHING_NOT_DELETED, "~deleted"},
	{MATCHING_REPLIED, "replied"},
	{MATCHING_NOT_REPLIED, "~replied"},
	{MATCHING_FORWARDED, "forwarded"},
	{MATCHING_NOT_FORWARDED, "~forwarded"},

	/* msginfo headers */
	{MATCHING_SUBJECT, "subject"},
	{MATCHING_NOT_SUBJECT, "~subject"},
	{MATCHING_FROM, "from"},
	{MATCHING_NOT_FROM, "~from"},
	{MATCHING_TO, "to"},
	{MATCHING_NOT_TO, "~to"},
	{MATCHING_CC, "cc"},
	{MATCHING_NOT_CC, "~cc"},
	{MATCHING_TO_OR_CC, "to_or_cc"},
	{MATCHING_NOT_TO_AND_NOT_CC, "~to_or_cc"},
	{MATCHING_AGE_GREATER, "age_greater"},
	{MATCHING_AGE_LOWER, "age_lower"},
	{MATCHING_NEWSGROUPS, "newsgroups"},
	{MATCHING_NOT_NEWSGROUPS, "~newsgroups"},
	{MATCHING_INREPLYTO, "inreplyto"},
	{MATCHING_NOT_INREPLYTO, "~inreplyto"},
	{MATCHING_REFERENCES, "references"},
	{MATCHING_NOT_REFERENCES, "~references"},
	{MATCHING_SCORE_GREATER, "score_greater"},
	{MATCHING_SCORE_LOWER, "score_lower"},

	/* content have to be read */
	{MATCHING_HEADER, "header"},
	{MATCHING_NOT_HEADER, "~header"},
	{MATCHING_HEADERS_PART, "headers_part"},
	{MATCHING_NOT_HEADERS_PART, "~headers_part"},
	{MATCHING_MESSAGE, "message"},
	{MATCHING_NOT_MESSAGE, "~message"},
	{MATCHING_BODY_PART, "body_part"},
	{MATCHING_NOT_BODY_PART, "~body_part"},
	{MATCHING_EXECUTE, "execute"},
	{MATCHING_NOT_EXECUTE, "~execute"},

	/* match type */
	{MATCHING_MATCHCASE, "matchcase"},
	{MATCHING_MATCH, "match"},
	{MATCHING_REGEXPCASE, "regexpcase"},
	{MATCHING_REGEXP, "regexp"},

	/* actions */
	{MATCHING_SCORE, "score"},

	/* actions */
	{MATCHING_ACTION_MOVE, "move"},
	{MATCHING_ACTION_COPY, "copy"},
	{MATCHING_ACTION_DELETE, "delete"},
	{MATCHING_ACTION_MARK, "mark"},
	{MATCHING_ACTION_UNMARK, "unmark"},
	{MATCHING_ACTION_MARK_AS_READ, "mark_as_read"},
	{MATCHING_ACTION_MARK_AS_UNREAD, "mark_as_unread"},
	{MATCHING_ACTION_FORWARD, "forward"},
	{MATCHING_ACTION_FORWARD_AS_ATTACHMENT, "forward_as_attachment"},
	/*	{MATCHING_EXECUTE, "execute"}, */
};

gchar * get_matchparser_tab_str(gint id)
{
	gint i;

	for(i = 0 ; i < (int) (sizeof(matchparser_tab) / sizeof(MatchParser)) ;
	    i++) {
		if (matchparser_tab[i].id == id)
			return matchparser_tab[i].str;
	}
	return NULL;
}



/*
  syntax for matcher

  header "x-mailing" match "toto"
  subject match "regexp" & to regexp "regexp"
  subject match "regexp" | to regexpcase "regexp" | age_sup 5
 */

static gboolean matcher_is_blank(gchar ch);

/* ******************* parser *********************** */

static gboolean matcher_is_blank(gchar ch)
{
	return (ch == ' ') || (ch == '\t');
}

/* parse for one condition */

MatcherProp * matcherprop_parse(gchar ** str)
{
	MatcherProp * prop;
	gchar * tmp;
	gint key;
	gint value;
	gchar * expr;
	gint match;
	gchar * header = NULL;
	
	tmp = * str;
	key = matcher_parse_keyword(&tmp);
	if (tmp == NULL) {
		* str = NULL;
		return NULL;
	}

	switch (key) {
	case MATCHING_AGE_LOWER:
	case MATCHING_AGE_GREATER:
	case MATCHING_SCORE_LOWER:
	case MATCHING_SCORE_GREATER:
		value = matcher_parse_number(&tmp);
		if (tmp == NULL) {
			* str = NULL;
			return NULL;
		}
		*str = tmp;

		prop = matcherprop_new(key, NULL, 0, NULL, value);

		return prop;

	case MATCHING_ALL:
	case MATCHING_UNREAD:
	case MATCHING_NOT_UNREAD:
	case MATCHING_NEW:
	case MATCHING_NOT_NEW:
	case MATCHING_MARKED:
	case MATCHING_NOT_MARKED:
	case MATCHING_DELETED:
	case MATCHING_NOT_DELETED:
	case MATCHING_REPLIED:
	case MATCHING_NOT_REPLIED:
	case MATCHING_FORWARDED:
	case MATCHING_NOT_FORWARDED:
		prop = matcherprop_new(key, NULL, 0, NULL, 0);
		*str = tmp;

		return prop;

	case MATCHING_SUBJECT:
	case MATCHING_NOT_SUBJECT:
	case MATCHING_FROM:
	case MATCHING_NOT_FROM:
	case MATCHING_TO:
	case MATCHING_NOT_TO:
	case MATCHING_CC:
	case MATCHING_NOT_CC:
	case MATCHING_TO_OR_CC:
	case MATCHING_NOT_TO_AND_NOT_CC:
	case MATCHING_NEWSGROUPS:
	case MATCHING_NOT_NEWSGROUPS:
	case MATCHING_INREPLYTO:
	case MATCHING_NOT_REFERENCES:
	case MATCHING_REFERENCES:
	case MATCHING_NOT_INREPLYTO:
	case MATCHING_MESSAGE:
	case MATCHING_NOT_MESSAGE:
	case MATCHING_EXECUTE:
	case MATCHING_NOT_EXECUTE:
	case MATCHING_HEADERS_PART:
	case MATCHING_NOT_HEADERS_PART:
	case MATCHING_BODY_PART:
	case MATCHING_NOT_BODY_PART:
	case MATCHING_HEADER:
	case MATCHING_NOT_HEADER:
		if ((key == MATCHING_HEADER) || (key == MATCHING_NOT_HEADER)) {
			header = matcher_parse_str(&tmp);
			if (tmp == NULL) {
				* str = NULL;
				return NULL;
			}
		}

		match = matcher_parse_keyword(&tmp);
		if (tmp == NULL) {
			if (header)
				g_free(header);
			* str = NULL;
			return NULL;
		}

		switch(match) {
		case MATCHING_REGEXP:
		case MATCHING_REGEXPCASE:
			expr = matcher_parse_regexp(&tmp);
			if (tmp == NULL) {
				if (header)
					g_free(header);
				* str = NULL;
				return NULL;
			}
 			*str = tmp;
			prop = matcherprop_new(key, header, match, expr, 0);
			g_free(expr);

			return prop;
		case MATCHING_MATCH:
		case MATCHING_MATCHCASE:
			expr = matcher_parse_str(&tmp);
			if (tmp == NULL) {
				if (header)
					g_free(header);
				* str = NULL;
				return NULL;
			}
			*str = tmp;
			prop = matcherprop_new(key, header, match, expr, 0);
			g_free(expr);

			return prop;
		default:
			if (header)
				g_free(header);
			* str = NULL;
			return NULL;
		}
	default:
		* str = NULL;
		return NULL;
	}
}

gint matcher_parse_keyword(gchar ** str)
{
	gchar * p;
	gchar * dup;
	gchar * start;
	gint i;
	gint match;

	dup = alloca(strlen(* str) + 1);
	p = dup;
	strcpy(dup, * str);

	while (matcher_is_blank(*p))
		p++;

	start = p;

	while (!matcher_is_blank(*p) && (*p != '\0'))
		p++;
	
	match = -1;
	for(i = 0 ; i < (int) (sizeof(matchparser_tab) / sizeof(MatchParser)) ;
	    i++) {
		if ((strlen(matchparser_tab[i].str) == p - start) &&
		    (strncasecmp(matchparser_tab[i].str, start,
				 p - start) == 0)) {
				match = i;
				break;
			}
	}

	if (match == -1) {
		* str = NULL;
		return 0;
	}

	*p = '\0';

	*str += p - dup + 1;
	return matchparser_tab[match].id;
}

gint matcher_parse_number(gchar ** str)
{
	gchar * p;
	gchar * dup;
	gchar * start;

	dup = alloca(strlen(* str) + 1);
	p = dup;
	strcpy(dup, * str);

	while (matcher_is_blank(*p))
		p++;

	start = p;

	if (!isdigit(*p) && *p != '-' && *p != '+') {
		*str = NULL;
		return 0;
	}
	if (*p == '-' || *p == '+')
		p++;
	while (isdigit(*p))
		p++;

	*p = '\0';

	*str += p - dup + 1;
	return atoi(start);
}

gboolean matcher_parse_boolean_op(gchar ** str)
{
	gchar * p;

	p = * str;

	while (matcher_is_blank(*p))
		p++;

	if (*p == '|') {
		*str += p - * str + 1;
		return FALSE;
	}
	else if (*p == '&') {
		*str += p - * str + 1;
		return TRUE;
	}
	else {
		*str = NULL;
		return FALSE;
	}
}

gchar * matcher_parse_regexp(gchar ** str)
{
	gchar * p;
	gchar * dup;
	gchar * start;

	dup = alloca(strlen(* str) + 1);
	p = dup;
	strcpy(dup, * str);

	while (matcher_is_blank(*p))
		p++;

	if (*p != '/') {
		* str = NULL;
		return NULL;
	}

	p ++;
	start = p;
	while (*p != '/') {
		if (*p == '\\')
			p++;
		p++;
	}
	*p = '\0';

	*str += p - dup + 2;
	return g_strdup(start);
}

gchar * matcher_parse_str(gchar ** str)
{
	gchar * p;
	gchar * dup;
	gchar * start;
	gchar * dest;

	dup = alloca(strlen(* str) + 1);
	p = dup;
	strcpy(dup, * str);

	while (matcher_is_blank(*p))
		p++;

	if (*p != '"') {
		* str = NULL;
		return NULL;
	}
	
	p ++;
	start = p;
	dest = p;
	while (*p != '"') {
		if (*p == '\\') {
			p++;
			*dest = *p;
		}
		else
			*dest = *p;
		dest++;
		p++;
	}
	*dest = '\0';

	*str += dest - dup + 2;
	return g_strdup(start);
}

/* **************** data structure allocation **************** */


MatcherProp * matcherprop_new(gint criteria, gchar * header,
			      gint matchtype, gchar * expr,
			      int value)
{
	MatcherProp * prop;

 	prop = g_new0(MatcherProp, 1);
	prop->criteria = criteria;
	if (header != NULL)
		prop->header = g_strdup(header);
	else
		prop->header = NULL;
	if (expr != NULL)
		prop->expr = g_strdup(expr);
	else
		prop->expr = NULL;
	prop->matchtype = matchtype;
	prop->preg = NULL;
	prop->value = value;
	prop->error = 0;

	return prop;
}

void matcherprop_free(MatcherProp * prop)
{
	g_free(prop->expr);
	if (prop->preg != NULL) {
		regfree(prop->preg);
		g_free(prop->preg);
	}
	g_free(prop);
}


/* ************** match ******************************/


/* match the given string */

static gboolean matcherprop_string_match(MatcherProp * prop, gchar * str)
{
	gchar * str1;
	gchar * str2;

	if (str == NULL)
		return FALSE;

	switch(prop->matchtype) {
	case MATCHING_REGEXPCASE:
	case MATCHING_REGEXP:
		if (!prop->preg && (prop->error == 0)) {
			prop->preg = g_new0(regex_t, 1);
			if (regcomp(prop->preg, prop->expr,
				    REG_NOSUB | REG_EXTENDED
				    | ((prop->matchtype == MATCHING_REGEXPCASE)
				    ? REG_ICASE : 0)) != 0) {
				prop->error = 1;
				g_free(prop->preg);
			}
		}
		if (prop->preg == NULL)
			return FALSE;
		
		if (regexec(prop->preg, str, 0, NULL, 0) == 0)
			return TRUE;
		else
			return FALSE;

	case MATCHING_MATCH:
		return (strstr(str, prop->expr) != NULL);

	case MATCHING_MATCHCASE:
		str2 = alloca(strlen(prop->expr) + 1);
		strcpy(str2, prop->expr);
		g_strup(str2);
		str1 = alloca(strlen(str) + 1);
		strcpy(str1, str);
		g_strup(str1);
		return (strstr(str1, str2) != NULL);
		
	default:
		return FALSE;
	}
}

gboolean matcherprop_match_execute(MatcherProp * prop, MsgInfo * info)
{
	gchar * cmd;

	cmd = matching_build_command(prop->expr, info);
	if (cmd == NULL)
		return FALSE;

	return (system(cmd) == 0);
}

/* match a message and his headers, hlist can be NULL if you don't
   want to use headers */

gboolean matcherprop_match(MatcherProp * prop, MsgInfo * info)
{
	time_t t;

	switch(prop->criteria) {
	case MATCHING_ALL:
		return 1;
	case MATCHING_UNREAD:
		return MSG_IS_UNREAD(info->flags);
	case MATCHING_NOT_UNREAD:
		return !MSG_IS_UNREAD(info->flags);
	case MATCHING_NEW:
		return MSG_IS_NEW(info->flags);
	case MATCHING_NOT_NEW:
		return !MSG_IS_NEW(info->flags);
	case MATCHING_MARKED:
		return MSG_IS_MARKED(info->flags);
	case MATCHING_NOT_MARKED:
		return !MSG_IS_MARKED(info->flags);
	case MATCHING_DELETED:
		return MSG_IS_DELETED(info->flags);
	case MATCHING_NOT_DELETED:
		return !MSG_IS_DELETED(info->flags);
	case MATCHING_REPLIED:
		return MSG_IS_REPLIED(info->flags);
	case MATCHING_NOT_REPLIED:
		return !MSG_IS_REPLIED(info->flags);
	case MATCHING_FORWARDED:
		return MSG_IS_FORWARDED(info->flags);
	case MATCHING_NOT_FORWARDED:
		return !MSG_IS_FORWARDED(info->flags);
	case MATCHING_SUBJECT:
		return matcherprop_string_match(prop, info->subject);
	case MATCHING_NOT_SUBJECT:
		return !matcherprop_string_match(prop, info->subject);
	case MATCHING_FROM:
		return matcherprop_string_match(prop, info->from);
	case MATCHING_NOT_FROM:
		return !matcherprop_string_match(prop, info->from);
	case MATCHING_TO:
		return matcherprop_string_match(prop, info->to);
	case MATCHING_NOT_TO:
		return !matcherprop_string_match(prop, info->to);
	case MATCHING_CC:
		return matcherprop_string_match(prop, info->cc);
	case MATCHING_NOT_CC:
		return !matcherprop_string_match(prop, info->cc);
	case MATCHING_TO_OR_CC:
		return matcherprop_string_match(prop, info->to)
			|| matcherprop_string_match(prop, info->cc);
	case MATCHING_NOT_TO_AND_NOT_CC:
		return !(matcherprop_string_match(prop, info->to)
		|| matcherprop_string_match(prop, info->cc));
	case MATCHING_AGE_GREATER:
		t = time(NULL);
		return ((t - info->date_t) / (60 * 60 * 24)) >= prop->value;
	case MATCHING_AGE_LOWER:
		t = time(NULL);
		return ((t - info->date_t) / (60 * 60 * 24)) <= prop->value;
	case MATCHING_SCORE_GREATER:
		return info->score >= prop->value;
	case MATCHING_SCORE_LOWER:
		return info->score <= prop->value;
	case MATCHING_NEWSGROUPS:
		return matcherprop_string_match(prop, info->newsgroups);
	case MATCHING_NOT_NEWSGROUPS:
		return !matcherprop_string_match(prop, info->newsgroups);
	case MATCHING_INREPLYTO:
		return matcherprop_string_match(prop, info->inreplyto);
	case MATCHING_NOT_INREPLYTO:
		return !matcherprop_string_match(prop, info->inreplyto);
	case MATCHING_REFERENCES:
		return matcherprop_string_match(prop, info->references);
	case MATCHING_NOT_REFERENCES:
		return !matcherprop_string_match(prop, info->references);
	case MATCHING_EXECUTE:
		return matcherprop_match_execute(prop, info);
	case MATCHING_NOT_EXECUTE:
		return !matcherprop_match_execute(prop, info);
	default:
		return 0;
	}
}

/* ********************* MatcherList *************************** */


/* parse for a list of conditions */

MatcherList * matcherlist_parse(gchar ** str)
{
	gchar * tmp;
	MatcherProp * matcher;
	GSList * matchers_list = NULL;
	gboolean bool_and = TRUE;
	gchar * save;
	MatcherList * cond;
	gboolean main_bool_and = TRUE;
	GSList * l;

	tmp = * str;

	matcher = matcherprop_parse(&tmp);

	if (tmp == NULL) {
		* str = NULL;
		return NULL;
	}
	matchers_list = g_slist_append(matchers_list, matcher);
	while (matcher) {
		save = tmp;
		bool_and = matcher_parse_boolean_op(&tmp);
		if (tmp == NULL) {
			tmp = save;
			matcher = NULL;
		}
		else {
			main_bool_and = bool_and;
			matcher = matcherprop_parse(&tmp);
			if (tmp != NULL) {
				matchers_list =
					g_slist_append(matchers_list, matcher);
			}
			else {
				for(l = matchers_list ; l != NULL ;
				    l = g_slist_next(l))
					matcherprop_free((MatcherProp *)
							 l->data);
				g_slist_free(matchers_list);
				* str = NULL;
				return NULL;
			}
		}
	}

	cond = matcherlist_new(matchers_list, main_bool_and);

	* str = tmp;

	return cond;
}

MatcherList * matcherlist_new(GSList * matchers, gboolean bool_and)
{
	MatcherList * cond;

	cond = g_new0(MatcherList, 1);

	cond->matchers = matchers;
	cond->bool_and = bool_and;

	return cond;
}

void matcherlist_free(MatcherList * cond)
{
	GSList * l;

	for(l = cond->matchers ; l != NULL ; l = g_slist_next(l)) {
		matcherprop_free((MatcherProp *) l->data);
	}
	g_free(cond);
}

/*
  skip the headers
 */

static void matcherlist_skip_headers(FILE *fp)
{
	gchar buf[BUFFSIZE];

	while (procheader_get_one_field(buf, sizeof(buf), fp, NULL) != -1) {
	}
}

/*
  matcherprop_match_one_header
  returns TRUE if buf matchs the MatchersProp criteria
 */

static gboolean matcherprop_match_one_header(MatcherProp * matcher,
					     gchar * buf)
{
	gboolean result;
	Header *header;

	switch(matcher->criteria) {
	case MATCHING_HEADER:
	case MATCHING_NOT_HEADER:
		header = procheader_parse_header(buf);
		if (!header)
			return FALSE;
		if (procheader_headername_equal(header->name,
						matcher->header)) {
			if (matcher->criteria == MATCHING_HEADER)
				result = matcherprop_string_match(matcher, header->body);
			else
				result = !matcherprop_string_match(matcher, header->body);
			procheader_header_free(header);
			return result;
		}
		else {
			procheader_header_free(header);
		}
		break;
	case MATCHING_HEADERS_PART:
	case MATCHING_MESSAGE:
		return matcherprop_string_match(matcher, buf);
	case MATCHING_NOT_MESSAGE:
	case MATCHING_NOT_HEADERS_PART:
		return !matcherprop_string_match(matcher, buf);
	}
	return FALSE;
}

/*
  matcherprop_criteria_header
  returns TRUE if the headers must be matched
 */

static gboolean matcherprop_criteria_headers(MatcherProp * matcher)
{
	switch(matcher->criteria) {
	case MATCHING_HEADER:
	case MATCHING_NOT_HEADER:
	case MATCHING_HEADERS_PART:
	case MATCHING_NOT_HEADERS_PART:
		return TRUE;
	default:
		return FALSE;
	}
}

static gboolean matcherprop_criteria_message(MatcherProp * matcher)
{
	switch(matcher->criteria) {
	case MATCHING_MESSAGE:
	case MATCHING_NOT_MESSAGE:
		return TRUE;
	default:
		return FALSE;
	}
}

/*
  matcherlist_match_one_header
  returns TRUE if match should stop
 */

static gboolean matcherlist_match_one_header(MatcherList * matchers,
					 gchar * buf)
{
	GSList * l;

	for(l = matchers->matchers ; l != NULL ; l = g_slist_next(l)) {
		MatcherProp * matcher = (MatcherProp *) l->data;

		if (matcherprop_criteria_headers(matcher) ||
		    matcherprop_criteria_message(matcher)) {
			if (matcherprop_match_one_header(matcher, buf)) {
				matcher->result = TRUE;
			}
		}

		if (matcherprop_criteria_headers(matcher)) {
			if (matcher->result) {
				if (!matchers->bool_and)
					return TRUE;
			}
		}
	}

	return FALSE;
}

/*
  matcherlist_match_headers
  returns TRUE if one of the headers matchs the MatcherList criteria
 */

static gboolean matcherlist_match_headers(MatcherList * matchers, FILE * fp)
{
	gchar buf[BUFFSIZE];

	while (procheader_get_one_field(buf, sizeof(buf), fp, NULL) != -1)
		if (matcherlist_match_one_header(matchers, buf))
			return TRUE;

	return FALSE;
}

/*
  matcherprop_criteria_body
  returns TRUE if the body must be matched
 */

static gboolean matcherprop_criteria_body(MatcherProp * matcher)
{
	switch(matcher->criteria) {
	case MATCHING_BODY_PART:
	case MATCHING_NOT_BODY_PART:
		return TRUE;
	default:
		return FALSE;
	}
}

/*
  matcherprop_match_line
  returns TRUE if the string matchs the MatcherProp criteria
 */

static gboolean matcherprop_match_line(MatcherProp * matcher, gchar * line)
{
	switch(matcher->criteria) {
	case MATCHING_BODY_PART:
	case MATCHING_MESSAGE:
		return matcherprop_string_match(matcher, line);
	case MATCHING_NOT_BODY_PART:
	case MATCHING_NOT_MESSAGE:
		return !matcherprop_string_match(matcher, line);
	}
	return FALSE;
}

/*
  matcherlist_match_line
  returns TRUE if the string matchs the MatcherList criteria
 */

static gboolean matcherlist_match_line(MatcherList * matchers, gchar * line)
{
	GSList * l;

	for(l = matchers->matchers ; l != NULL ; l = g_slist_next(l)) {
		MatcherProp * matcher = (MatcherProp *) l->data;

		if (matcherprop_criteria_body(matcher) ||
		    matcherprop_criteria_message(matcher)) {
			if (matcherprop_match_line(matcher, line)) {
				matcher->result = TRUE;
			}
		}
			
		if (matcher->result) {
			if (!matchers->bool_and)
				return TRUE;
		}
	}
	return FALSE;
}

/*
  matcherlist_match_body
  returns TRUE if one line of the body matchs the MatcherList criteria
 */

static gboolean matcherlist_match_body(MatcherList * matchers, FILE * fp)
{
	gchar buf[BUFFSIZE];

	while (fgets(buf, sizeof(buf), fp) != NULL)
		if (matcherlist_match_line(matchers, buf))
			return TRUE;

	return FALSE;
}

gboolean matcherlist_match_file(MatcherList * matchers, MsgInfo * info,
				gboolean result)
{
	gboolean read_headers;
	gboolean read_body;
	GSList * l;
	FILE * fp;
	gchar * file;
	gboolean is_incorporating = MSG_IS_FILTERING(info->flags);
	
	/* check which things we need to do */
	read_headers = FALSE;
	read_body    = FALSE;
	
	for(l = matchers->matchers ; l != NULL ; l = g_slist_next(l)) {
		MatcherProp * matcher = (MatcherProp *) l->data;

		if (matcherprop_criteria_headers(matcher))
			read_headers = TRUE;
		if (matcherprop_criteria_body(matcher))
			read_body = TRUE;
		if (matcherprop_criteria_message(matcher)) {
			read_headers = TRUE;
			read_body = TRUE;
		}
		matcher->result = FALSE;
	}

	if (!read_headers && !read_body)
		return result;

	file = is_incorporating ? g_strdup(info->folder) 
		: procmsg_get_message_file(info);
		
	if (file == NULL)
		return FALSE;

	if ((fp = fopen(file, "r")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		g_free(file);
		return result;
	}

	/* read the headers */

	if (read_headers) {
		if (matcherlist_match_headers(matchers, fp))
			read_body = FALSE;
	}
	else {
		matcherlist_skip_headers(fp);
	}

	/* read the body */
	if (read_body) {
		matcherlist_match_body(matchers, fp);
	}
	
	for(l = matchers->matchers ; l != NULL ; l = g_slist_next(l)) {
		MatcherProp * matcher = (MatcherProp *) l->data;

		if (matcherprop_criteria_headers(matcher) ||
		    matcherprop_criteria_body(matcher) ||
		    matcherprop_criteria_message(matcher))
			if (matcher->result) {
				if (!matchers->bool_and) {
					result = TRUE;
					break;
				}
			}
			else {
				if (matchers->bool_and) {
					result = FALSE;
					break;
				}
		}
	}

	g_free(file);

	fclose(fp);
	
	return result;
}

/* test a list of condition */

gboolean matcherlist_match(MatcherList * matchers, MsgInfo * info)
{
	GSList * l;
	gboolean result;

	if (matchers->bool_and)
		result = TRUE;
	else
		result = FALSE;

	/* test the cached elements */

	for(l = matchers->matchers ; l != NULL ; l = g_slist_next(l)) {
		MatcherProp * matcher = (MatcherProp *) l->data;

		switch(matcher->criteria) {
		case MATCHING_ALL:
		case MATCHING_UNREAD:
		case MATCHING_NOT_UNREAD:
		case MATCHING_NEW:
		case MATCHING_NOT_NEW:
		case MATCHING_MARKED:
		case MATCHING_NOT_MARKED:
		case MATCHING_DELETED:
		case MATCHING_NOT_DELETED:
		case MATCHING_REPLIED:
		case MATCHING_NOT_REPLIED:
		case MATCHING_FORWARDED:
		case MATCHING_NOT_FORWARDED:
		case MATCHING_SUBJECT:
		case MATCHING_NOT_SUBJECT:
		case MATCHING_FROM:
		case MATCHING_NOT_FROM:
		case MATCHING_TO:
		case MATCHING_NOT_TO:
		case MATCHING_CC:
		case MATCHING_NOT_CC:
		case MATCHING_TO_OR_CC:
		case MATCHING_NOT_TO_AND_NOT_CC:
		case MATCHING_AGE_GREATER:
		case MATCHING_AGE_LOWER:
		case MATCHING_NEWSGROUPS:
		case MATCHING_NOT_NEWSGROUPS:
		case MATCHING_INREPLYTO:
		case MATCHING_NOT_INREPLYTO:
		case MATCHING_REFERENCES:
		case MATCHING_NOT_REFERENCES:
		case MATCHING_SCORE_GREATER:
		case MATCHING_SCORE_LOWER:
		case MATCHING_EXECUTE:
		case MATCHING_NOT_EXECUTE:
			if (matcherprop_match(matcher, info)) {
				if (!matchers->bool_and) {
					return TRUE;
				}
			}
			else {
				if (matchers->bool_and) {
					return FALSE;
				}
			}
		}
	}

	/* test the condition on the file */

	if (matcherlist_match_file(matchers, info, result)) {
		if (!matchers->bool_and)
			return TRUE;
	}
	else {
		if (matchers->bool_and)
			return FALSE;
	}

	return result;
}

#if 0
static void matcherprop_print(MatcherProp * matcher)
{
  int i;

	if (matcher == NULL) {
		printf("no matcher\n");
		return;
	}

	switch (matcher->matchtype) {
	case MATCHING_MATCH:
		printf("match\n");
		break;
	case MATCHING_REGEXP:
		printf("regexp\n");
		break;
	case MATCHING_MATCHCASE:
		printf("matchcase\n");
		break;
	case MATCHING_REGEXPCASE:
		printf("regexpcase\n");
		break;
	}

	for(i = 0 ; i < (int) (sizeof(matchparser_tab) / sizeof(MatchParser)) ;
	    i++) {
		if (matchparser_tab[i].id == matcher->criteria)
			printf("%s\n", matchparser_tab[i].str);
	}

	if (matcher->expr)
		printf("expr : %s\n", matcher->expr);

	printf("age: %i\n", matcher->value;

	printf("compiled : %s\n", matcher->preg != NULL ? "yes" : "no");
	printf("error: %i\n",  matcher->error);
}
#endif

gchar * matcherprop_to_string(MatcherProp * matcher)
{
	gchar * matcher_str = NULL;
	gchar * criteria_str;
	gchar * matchtype_str;
	int i;
	gchar * p;
	gint count;
	gchar * expr_str;
	gchar * out;

	criteria_str = NULL;
	for(i = 0 ; i < (int) (sizeof(matchparser_tab) / sizeof(MatchParser)) ;
	    i++) {
		if (matchparser_tab[i].id == matcher->criteria)
			criteria_str = matchparser_tab[i].str;
	}
	if (criteria_str == NULL)
		return NULL;

	switch(matcher->criteria) {
	case MATCHING_AGE_GREATER:
	case MATCHING_AGE_LOWER:
	case MATCHING_SCORE_GREATER:
	case MATCHING_SCORE_LOWER:
		return g_strdup_printf("%s %i", criteria_str, matcher->value);
		break;
	case MATCHING_ALL:
	case MATCHING_UNREAD:
	case MATCHING_NOT_UNREAD:
	case MATCHING_NEW:
	case MATCHING_NOT_NEW:
	case MATCHING_MARKED:
	case MATCHING_NOT_MARKED:
	case MATCHING_DELETED:
	case MATCHING_NOT_DELETED:
	case MATCHING_REPLIED:
	case MATCHING_NOT_REPLIED:
	case MATCHING_FORWARDED:
	case MATCHING_NOT_FORWARDED:
		return g_strdup(criteria_str);
	}

	matchtype_str = NULL;
	for(i = 0 ; i < (int) (sizeof(matchparser_tab) / sizeof(MatchParser)) ;
	    i++) {
		if (matchparser_tab[i].id == matcher->matchtype)
			matchtype_str = matchparser_tab[i].str;
	}

	if (matchtype_str == NULL)
		return NULL;

	switch (matcher->matchtype) {
	case MATCHING_MATCH:
	case MATCHING_MATCHCASE:
		count = 0;
		for(p = matcher->expr; *p != 0 ; p++)
			if (*p == '\"') count ++;
		
		expr_str = g_new(char, strlen(matcher->expr) + count + 1);

		for(p = matcher->expr, out = expr_str ; *p != 0 ; p++, out++) {
			if (*p == '\"') {
				*out = '\\'; out++;
				*out = '\"';
			}
			else
				*out = *p;
		}
		* out = '\0';

		if (matcher->header)
			matcher_str =
				g_strdup_printf("%s \"%s\" %s \"%s\"",
					   criteria_str, matcher->header,
					   matchtype_str, expr_str);
		else
			matcher_str =
				g_strdup_printf("%s %s \"%s\"", criteria_str,
						matchtype_str, expr_str);
		
		g_free(expr_str);
		
		break;

	case MATCHING_REGEXP:
	case MATCHING_REGEXPCASE:

		if (matcher->header)
			matcher_str =
				g_strdup_printf("%s \"%s\" %s /%s/",
						criteria_str, matcher->header,
						matchtype_str, matcher->expr);
		else
			matcher_str =
				g_strdup_printf("%s %s /%s/", criteria_str,
						matchtype_str, matcher->expr);

		break;
	}

	return matcher_str;
}

gchar * matcherlist_to_string(MatcherList * matchers)
{
	gint count;
	gchar ** vstr;
	GSList * l;
	gchar ** cur_str;
	gchar * result;

	count = g_slist_length(matchers->matchers);
	vstr = g_new(gchar *, count + 1);

	for (l = matchers->matchers, cur_str = vstr ; l != NULL ;
	     l = g_slist_next(l), cur_str ++) {
		*cur_str = matcherprop_to_string((MatcherProp *) l->data);
		if (*cur_str == NULL)
			break;
	}
	*cur_str = NULL;
	
	if (matchers->bool_and)
		result = g_strjoinv(" & ", vstr);
	else
		result = g_strjoinv(" | ", vstr);

	for(cur_str = vstr ; *cur_str != NULL ; cur_str ++)
		g_free(*cur_str);
	g_free(vstr);

	return result;
}

static inline gint strlen_with_check(const gchar *expr, gint fline, const gchar *str)
{
	if (str) 
		return strlen(str);
	else {
		debug_print("%s(%d) - invalid string %s\n", __FILE__, fline, expr);
		return 0;
	}
}

#define STRLEN_WITH_CHECK(expr) \
	strlen_with_check(#expr, __LINE__, expr)

gchar * matching_build_command(gchar * cmd, MsgInfo * info)
{
	gchar * s = cmd;
	gchar * filename = NULL;
	gchar * processed_cmd;
	gchar * p;
	gint size;

	size = strlen(cmd) + 1;
	while (*s != '\0') {
		if (*s == '%') {
			s++;
			switch (*s) {
			case '%':
				size -= 1;
				break;
			case 's': /* subject */
				size += STRLEN_WITH_CHECK(info->subject) - 2;
				break;
			case 'f': /* from */
				size += STRLEN_WITH_CHECK(info->from) - 2;
				break;
			case 't': /* to */
				size += STRLEN_WITH_CHECK(info->to) - 2;
				break;
			case 'c': /* cc */
				size += STRLEN_WITH_CHECK(info->cc) - 2;
				break;
			case 'd': /* date */
				size += STRLEN_WITH_CHECK(info->date) - 2;
				break;
			case 'i': /* message-id */
				size += STRLEN_WITH_CHECK(info->msgid) - 2;
				break;
			case 'n': /* newsgroups */
				size += STRLEN_WITH_CHECK(info->newsgroups) - 2;
				break;
			case 'r': /* references */
				size += STRLEN_WITH_CHECK(info->references) - 2;
				break;
			case 'F': /* file */
				if (MSG_IS_FILTERING(info->flags))
					filename = g_strdup(info->folder);
				else						
					filename = folder_item_fetch_msg(info->folder, info->msgnum);
				
				if (filename == NULL) {
					debug_print(_("%s(%d) - filename is not set"), __FILE__, __LINE__);
					return NULL;
				}
				else
					size += strlen(filename) - 2;
				break;
			}
			s++;
		}
		else s++;
	}

	processed_cmd = g_new0(gchar, size);
	s = cmd;
	p = processed_cmd;

	while (*s != '\0') {
		if (*s == '%') {
			s++;
			switch (*s) {
			case '%':
				*p = '%';
				p++;
				break;
			case 's': /* subject */
				if (info->subject != NULL)
					strcpy(p, info->subject);
				else
					strcpy(p, "(none)");
				p += strlen(p);
				break;
			case 'f': /* from */
				if (info->from != NULL)
					strcpy(p, info->from);
				else
					strcpy(p, "(none)");
				p += strlen(p);
				break;
			case 't': /* to */
				if (info->to != NULL)
					strcpy(p, info->to);
				else
					strcpy(p, "(none)");
				p += strlen(p);
				break;
			case 'c': /* cc */
				if (info->cc != NULL)
					strcpy(p, info->cc);
				else
					strcpy(p, "(none)");
				p += strlen(p);
				break;
			case 'd': /* date */
				if (info->date != NULL)
					strcpy(p, info->date);
				else
					strcpy(p, "(none)");
				p += strlen(p);
				break;
			case 'i': /* message-id */
				if (info->msgid != NULL)
					strcpy(p, info->msgid);
				else
					strcpy(p, "(none)");
				p += strlen(p);
				break;
			case 'n': /* newsgroups */
				if (info->newsgroups != NULL)
					strcpy(p, info->newsgroups);
				else
					strcpy(p, "(none)");
				p += strlen(p);
				break;
			case 'r': /* references */
				if (info->references != NULL)
					strcpy(p, info->references);
				else
					strcpy(p, "(none)");
				p += strlen(p);
				break;
			case 'F': /* file */
				strcpy(p, filename);
				p += strlen(p);
				break;
			default:
				*p = '%';
				p++;
				*p = *s;
				p++;
				break;
			}
			s++;
		}
		else {
			*p = *s;
			p++;
			s++;
		}
	}

	g_free(filename);
	return processed_cmd;
}


/* ************************************************************ */

/*
static void matcher_parse		(gchar * str)
{
	matcher_parser_scan_string(str);
	matcher_parserparse();
}
*/
