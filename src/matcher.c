/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2002-2004 by the Sylpheed Claws Team and Hiroyuki Yamamoto
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <glib.h>
#include <glib/gi18n.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "defs.h"
#include "utils.h"
#include "procheader.h"
#include "matcher.h"
#include "matcher_parser.h"
#include "prefs_gtk.h"
#include "addr_compl.h"
#include "codeconv.h"
#include "quoted-printable.h"
#include <ctype.h>

/*!
 *\brief	Keyword lookup element
 */
struct _MatchParser {
	gint id;		/*!< keyword id */ 
	gchar *str;		/*!< keyword */
};
typedef struct _MatchParser MatchParser;

/*!
 *\brief	Table with strings and ids used by the lexer and
 *		the parser. New keywords can be added here.
 */
static const MatchParser matchparser_tab[] = {
	/* msginfo flags */
	{MATCHCRITERIA_ALL, "all"},
	{MATCHCRITERIA_UNREAD, "unread"},
	{MATCHCRITERIA_NOT_UNREAD, "~unread"},
	{MATCHCRITERIA_NEW, "new"},
	{MATCHCRITERIA_NOT_NEW, "~new"},
	{MATCHCRITERIA_MARKED, "marked"},
	{MATCHCRITERIA_NOT_MARKED, "~marked"},
	{MATCHCRITERIA_DELETED, "deleted"},
	{MATCHCRITERIA_NOT_DELETED, "~deleted"},
	{MATCHCRITERIA_REPLIED, "replied"},
	{MATCHCRITERIA_NOT_REPLIED, "~replied"},
	{MATCHCRITERIA_FORWARDED, "forwarded"},
	{MATCHCRITERIA_NOT_FORWARDED, "~forwarded"},
	{MATCHCRITERIA_LOCKED, "locked"},
	{MATCHCRITERIA_NOT_LOCKED, "~locked"},
	{MATCHCRITERIA_COLORLABEL, "colorlabel"},
	{MATCHCRITERIA_NOT_COLORLABEL, "~colorlabel"},
	{MATCHCRITERIA_IGNORE_THREAD, "ignore_thread"},
	{MATCHCRITERIA_NOT_IGNORE_THREAD, "~ignore_thread"},

	/* msginfo headers */
	{MATCHCRITERIA_SUBJECT, "subject"},
	{MATCHCRITERIA_NOT_SUBJECT, "~subject"},
	{MATCHCRITERIA_FROM, "from"},
	{MATCHCRITERIA_NOT_FROM, "~from"},
	{MATCHCRITERIA_TO, "to"},
	{MATCHCRITERIA_NOT_TO, "~to"},
	{MATCHCRITERIA_CC, "cc"},
	{MATCHCRITERIA_NOT_CC, "~cc"},
	{MATCHCRITERIA_TO_OR_CC, "to_or_cc"},
	{MATCHCRITERIA_NOT_TO_AND_NOT_CC, "~to_or_cc"},
	{MATCHCRITERIA_AGE_GREATER, "age_greater"},
	{MATCHCRITERIA_AGE_LOWER, "age_lower"},
	{MATCHCRITERIA_NEWSGROUPS, "newsgroups"},
	{MATCHCRITERIA_NOT_NEWSGROUPS, "~newsgroups"},
	{MATCHCRITERIA_INREPLYTO, "inreplyto"},
	{MATCHCRITERIA_NOT_INREPLYTO, "~inreplyto"},
	{MATCHCRITERIA_REFERENCES, "references"},
	{MATCHCRITERIA_NOT_REFERENCES, "~references"},
	{MATCHCRITERIA_SCORE_GREATER, "score_greater"},
	{MATCHCRITERIA_SCORE_LOWER, "score_lower"},
	{MATCHCRITERIA_SCORE_EQUAL, "score_equal"},
	{MATCHCRITERIA_PARTIAL, "partial"},
	{MATCHCRITERIA_NOT_PARTIAL, "~partial"},

	{MATCHCRITERIA_SIZE_GREATER, "size_greater"},
	{MATCHCRITERIA_SIZE_SMALLER, "size_smaller"},
	{MATCHCRITERIA_SIZE_EQUAL,   "size_equal"},

	/* content have to be read */
	{MATCHCRITERIA_HEADER, "header"},
	{MATCHCRITERIA_NOT_HEADER, "~header"},
	{MATCHCRITERIA_HEADERS_PART, "headers_part"},
	{MATCHCRITERIA_NOT_HEADERS_PART, "~headers_part"},
	{MATCHCRITERIA_MESSAGE, "message"},
	{MATCHCRITERIA_NOT_MESSAGE, "~message"},
	{MATCHCRITERIA_BODY_PART, "body_part"},
	{MATCHCRITERIA_NOT_BODY_PART, "~body_part"},
	{MATCHCRITERIA_TEST, "test"},
	{MATCHCRITERIA_NOT_TEST, "~test"},

	/* match type */
	{MATCHTYPE_MATCHCASE, "matchcase"},
	{MATCHTYPE_MATCH, "match"},
	{MATCHTYPE_REGEXPCASE, "regexpcase"},
	{MATCHTYPE_REGEXP, "regexp"},
	{MATCHTYPE_ANY_IN_ADDRESSBOOK, "any_in_addressbook"},
	{MATCHTYPE_ALL_IN_ADDRESSBOOK, "all_in_addressbook"},

	/* actions */
	{MATCHACTION_SCORE, "score"},    /* for backward compatibility */
	{MATCHACTION_MOVE, "move"},
	{MATCHACTION_COPY, "copy"},
	{MATCHACTION_DELETE, "delete"},
	{MATCHACTION_MARK, "mark"},
	{MATCHACTION_UNMARK, "unmark"},
	{MATCHACTION_LOCK, "lock"},
	{MATCHACTION_UNLOCK, "unlock"},
	{MATCHACTION_MARK_AS_READ, "mark_as_read"},
	{MATCHACTION_MARK_AS_UNREAD, "mark_as_unread"},
	{MATCHACTION_FORWARD, "forward"},
	{MATCHACTION_FORWARD_AS_ATTACHMENT, "forward_as_attachment"},
	{MATCHACTION_EXECUTE, "execute"},
	{MATCHACTION_COLOR, "color"},
	{MATCHACTION_REDIRECT, "redirect"},
	{MATCHACTION_CHANGE_SCORE, "change_score"},
	{MATCHACTION_SET_SCORE, "set_score"},
	{MATCHACTION_STOP, "stop"},
	{MATCHACTION_HIDE, "hide"},
	{MATCHACTION_IGNORE, "ignore"},
};

/*!
 *\brief	Look up table with keywords defined in \sa matchparser_tab
 */
static GHashTable *matchparser_hashtab;

/*!
 *\brief	Translate keyword id to keyword string
 *
 *\param	id Id of keyword
 *
 *\return	const gchar * Keyword
 */
const gchar *get_matchparser_tab_str(gint id)
{
	gint i;

	for (i = 0; i < sizeof matchparser_tab / sizeof matchparser_tab[0]; i++) {
		if (matchparser_tab[i].id == id)
			return matchparser_tab[i].str;
	}
	return NULL;
}

/*!
 *\brief	Create keyword lookup table
 */
static void create_matchparser_hashtab(void)
{
	int i;
	
	if (matchparser_hashtab) return;
	matchparser_hashtab = g_hash_table_new(g_str_hash, g_str_equal);
	for (i = 0; i < sizeof matchparser_tab / sizeof matchparser_tab[0]; i++)
		g_hash_table_insert(matchparser_hashtab,
				    matchparser_tab[i].str,
				    (gpointer) &matchparser_tab[i]);
}

/*!
 *\brief	Return a keyword id from a keyword string
 *
 *\param	str Keyword string
 *
 *\return	gint Keyword id
 */
gint get_matchparser_tab_id(const gchar *str)
{
	MatchParser *res;

	if (NULL != (res = g_hash_table_lookup(matchparser_hashtab, str))) {
		return res->id;
	} else
		return -1;
}

/* **************** data structure allocation **************** */

/*!
 *\brief	Allocate a structure for a filtering / scoring
 *		"condition" (a matcher structure)
 *
 *\param	criteria Criteria ID (MATCHCRITERIA_XXXX)
 *\param	header Header string (if criteria is MATCHCRITERIA_HEADER)
 *\param	matchtype Type of action (MATCHTYPE_XXX)
 *\param	expr String value or expression to check
 *\param	value Integer value to check
 *
 *\return	MatcherProp * Pointer to newly allocated structure
 */
MatcherProp *matcherprop_new(gint criteria, const gchar *header,
			      gint matchtype, const gchar *expr,
			      int value)
{
	MatcherProp *prop;

 	prop = g_new0(MatcherProp, 1);
	prop->criteria = criteria;
	prop->header = header != NULL ? g_strdup(header) : NULL;
	prop->expr = expr != NULL ? g_strdup(expr) : NULL;
	prop->matchtype = matchtype;
	prop->preg = NULL;
	prop->value = value;
	prop->error = 0;

	return prop;
}

/*!
 *\brief	Free a matcher structure
 *
 *\param	prop Pointer to matcher structure allocated with
 *		#matcherprop_new
 */
void matcherprop_free(MatcherProp *prop)
{
	g_free(prop->expr);
	g_free(prop->header);
	if (prop->preg != NULL) {
		regfree(prop->preg);
		g_free(prop->preg);
	}
	g_free(prop);
}

/*!
 *\brief	Copy a matcher structure
 *
 *\param	src Matcher structure to copy
 *
 *\return	MatcherProp * Pointer to newly allocated matcher structure
 */
MatcherProp *matcherprop_copy(const MatcherProp *src)
{
	MatcherProp *prop = g_new0(MatcherProp, 1);
	
	prop->criteria = src->criteria;
	prop->header = src->header ? g_strdup(src->header) : NULL;
	prop->expr = src->expr ? g_strdup(src->expr) : NULL;
	prop->matchtype = src->matchtype;
	
	prop->preg = NULL; /* will be re-evaluated */
	prop->value = src->value;
	prop->error = src->error;	
	return prop;		
}

/* ************** match ******************************/

static gboolean match_with_addresses_in_addressbook
	(MatcherProp *prop, const gchar *str, gint type)
{
	GSList *address_list = NULL;
	GSList *walk;
	gboolean res = FALSE;

	if (str == NULL || *str == 0) 
		return FALSE;
	
	/* XXX: perhaps complete with comments too */
	address_list = address_list_append(address_list, str);
	if (!address_list) 
		return FALSE;

	start_address_completion();		
	res = FALSE;
	for (walk = address_list; walk != NULL; walk = walk->next) {
		gboolean found = complete_address(walk->data) ? TRUE : FALSE;
		
		g_free(walk->data);
		if (!found && type == MATCHTYPE_ALL_IN_ADDRESSBOOK) {
			res = FALSE;
			break;
		} else if (found) 
			res = TRUE;
	}

	g_slist_free(address_list);

	end_address_completion();
	
	return res;
}

/*!
 *\brief	Find out if a string matches a condition
 *
 *\param	prop Matcher structure
 *\param	str String to check 
 *
 *\return	gboolean TRUE if str matches the condition in the 
 *		matcher structure
 */
static gboolean matcherprop_string_match(MatcherProp *prop, const gchar *str)
{
	gchar *str1;
	gchar *str2;

	if (str == NULL)
		return FALSE;

	switch (prop->matchtype) {
	case MATCHTYPE_REGEXPCASE:
	case MATCHTYPE_REGEXP:
		if (!prop->preg && (prop->error == 0)) {
			prop->preg = g_new0(regex_t, 1);
			/* if regexp then don't use the escaped string */
			if (regcomp(prop->preg, prop->expr,
				    REG_NOSUB | REG_EXTENDED
				    | ((prop->matchtype == MATCHTYPE_REGEXPCASE)
				    ? REG_ICASE : 0)) != 0) {
				prop->error = 1;
				g_free(prop->preg);
				prop->preg = NULL;
			}
		}
		if (prop->preg == NULL)
			return FALSE;
		
		if (regexec(prop->preg, str, 0, NULL, 0) == 0)
			return TRUE;
		else
			return FALSE;
			
	case MATCHTYPE_ALL_IN_ADDRESSBOOK:	
	case MATCHTYPE_ANY_IN_ADDRESSBOOK:
		return match_with_addresses_in_addressbook
			(prop, str, prop->matchtype);

	case MATCHTYPE_MATCH:
		return (strstr(str, prop->expr) != NULL);

	/* FIXME: put upper in unesc_str */
	case MATCHTYPE_MATCHCASE:
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

/* FIXME body search is a hack. */
static gboolean matcherprop_string_decode_match(MatcherProp *prop, const gchar *str)
{
	gchar *utf = NULL;
	gchar tmp[BUFFSIZE];
	gboolean res = FALSE;

	if (str == NULL)
		return FALSE;

	/* we try to decode QP first, because it's faster than base64 */
	qp_decode_const(tmp, BUFFSIZE-1, str);
	if (!g_utf8_validate(tmp, -1, NULL)) {
		utf = conv_codeset_strdup
			(tmp, conv_get_locale_charset_str_no_utf8(),
			 CS_INTERNAL);
		res = matcherprop_string_match(prop, utf);
		g_free(utf);
	} else {
		res = matcherprop_string_match(prop, tmp);
	}
	
	if (res == FALSE && strchr(prop->expr, '=') || strchr(prop->expr, '_') ) {
		/* if searching for something with an equal char, maybe 
		 * we should try to match the non-decoded string. 
		 * In case it was not qp-encoded. */
		if (!g_utf8_validate(str, -1, NULL)) {
			utf = conv_codeset_strdup
				(str, conv_get_locale_charset_str_no_utf8(),
				 CS_INTERNAL);
			res = matcherprop_string_match(prop, utf);
		} else {
			res = matcherprop_string_match(prop, str);
		}
	}

	/* FIXME base64 decoding is too slow, especially since text can 
	 * easily be handled as base64. Don't even try now. */

	return res;
}

/*!
 *\brief	Execute a command defined in the matcher structure
 *
 *\param	prop Pointer to matcher structure
 *\param	info Pointer to message info structure
 *
 *\return	gboolean TRUE if command was executed succesfully
 */
static gboolean matcherprop_match_test(const MatcherProp *prop, 
					  MsgInfo *info)
{
	gchar *file;
	gchar *cmd;
	gint retval;

	file = procmsg_get_message_file(info);
	if (file == NULL)
		return FALSE;
	g_free(file);		

	cmd = matching_build_command(prop->expr, info);
	if (cmd == NULL)
		return FALSE;

	retval = system(cmd);
	debug_print("Command exit code: %d\n", retval);

	g_free(cmd);
	return (retval == 0);
}

/*!
 *\brief	Check if a message matches the condition in a matcher
 *		structure.
 *
 *\param	prop Pointer to matcher structure
 *\param	info Pointer to message info
 *
 *\return	gboolean TRUE if a match
 */
gboolean matcherprop_match(MatcherProp *prop, 
			   MsgInfo *info)
{
	time_t t;

	switch(prop->criteria) {
	case MATCHCRITERIA_ALL:
		return 1;
	case MATCHCRITERIA_UNREAD:
		return MSG_IS_UNREAD(info->flags);
	case MATCHCRITERIA_NOT_UNREAD:
		return !MSG_IS_UNREAD(info->flags);
	case MATCHCRITERIA_NEW:
		return MSG_IS_NEW(info->flags);
	case MATCHCRITERIA_NOT_NEW:
		return !MSG_IS_NEW(info->flags);
	case MATCHCRITERIA_MARKED:
		return MSG_IS_MARKED(info->flags);
	case MATCHCRITERIA_NOT_MARKED:
		return !MSG_IS_MARKED(info->flags);
	case MATCHCRITERIA_DELETED:
		return MSG_IS_DELETED(info->flags);
	case MATCHCRITERIA_NOT_DELETED:
		return !MSG_IS_DELETED(info->flags);
	case MATCHCRITERIA_REPLIED:
		return MSG_IS_REPLIED(info->flags);
	case MATCHCRITERIA_NOT_REPLIED:
		return !MSG_IS_REPLIED(info->flags);
	case MATCHCRITERIA_FORWARDED:
		return MSG_IS_FORWARDED(info->flags);
	case MATCHCRITERIA_NOT_FORWARDED:
		return !MSG_IS_FORWARDED(info->flags);
	case MATCHCRITERIA_LOCKED:
		return MSG_IS_LOCKED(info->flags);
	case MATCHCRITERIA_NOT_LOCKED:
		return !MSG_IS_LOCKED(info->flags);
	case MATCHCRITERIA_COLORLABEL:
		return MSG_GET_COLORLABEL_VALUE(info->flags) == prop->value; 
	case MATCHCRITERIA_NOT_COLORLABEL:
		return MSG_GET_COLORLABEL_VALUE(info->flags) != prop->value;
	case MATCHCRITERIA_IGNORE_THREAD:
		return MSG_IS_IGNORE_THREAD(info->flags);
	case MATCHCRITERIA_NOT_IGNORE_THREAD:
		return !MSG_IS_IGNORE_THREAD(info->flags);
	case MATCHCRITERIA_SUBJECT:
		return matcherprop_string_match(prop, info->subject);
	case MATCHCRITERIA_NOT_SUBJECT:
		return !matcherprop_string_match(prop, info->subject);
	case MATCHCRITERIA_FROM:
		return matcherprop_string_match(prop, info->from);
	case MATCHCRITERIA_NOT_FROM:
		return !matcherprop_string_match(prop, info->from);
	case MATCHCRITERIA_TO:
		return matcherprop_string_match(prop, info->to);
	case MATCHCRITERIA_NOT_TO:
		return !matcherprop_string_match(prop, info->to);
	case MATCHCRITERIA_CC:
		return matcherprop_string_match(prop, info->cc);
	case MATCHCRITERIA_NOT_CC:
		return !matcherprop_string_match(prop, info->cc);
	case MATCHCRITERIA_TO_OR_CC:
		return matcherprop_string_match(prop, info->to)
			|| matcherprop_string_match(prop, info->cc);
	case MATCHCRITERIA_NOT_TO_AND_NOT_CC:
		return !(matcherprop_string_match(prop, info->to)
		|| matcherprop_string_match(prop, info->cc));
	case MATCHCRITERIA_AGE_GREATER:
		t = time(NULL);
		return ((t - info->date_t) / (60 * 60 * 24)) >= prop->value;
	case MATCHCRITERIA_AGE_LOWER:
		t = time(NULL);
		return ((t - info->date_t) / (60 * 60 * 24)) <= prop->value;
	case MATCHCRITERIA_SCORE_GREATER:
		return info->score >= prop->value;
	case MATCHCRITERIA_SCORE_LOWER:
		return info->score <= prop->value;
	case MATCHCRITERIA_SCORE_EQUAL:
		return info->score == prop->value;
	case MATCHCRITERIA_SIZE_GREATER:
		/* FIXME: info->size is an off_t */
		return info->size > (off_t) prop->value;
	case MATCHCRITERIA_SIZE_EQUAL:
		/* FIXME: info->size is an off_t */
		return info->size == (off_t) prop->value;
	case MATCHCRITERIA_SIZE_SMALLER:
		/* FIXME: info->size is an off_t */
		return info->size <  (off_t) prop->value;
	case MATCHCRITERIA_PARTIAL:
		/* FIXME: info->size is an off_t */
		return (info->total_size != 0 && info->size != (off_t)info->total_size);
	case MATCHCRITERIA_NOT_PARTIAL:
		/* FIXME: info->size is an off_t */
		return (info->total_size == 0 || info->size == (off_t)info->total_size);
	case MATCHCRITERIA_NEWSGROUPS:
		return matcherprop_string_match(prop, info->newsgroups);
	case MATCHCRITERIA_NOT_NEWSGROUPS:
		return !matcherprop_string_match(prop, info->newsgroups);
	case MATCHCRITERIA_INREPLYTO:
		return matcherprop_string_match(prop, info->inreplyto);
	case MATCHCRITERIA_NOT_INREPLYTO:
		return !matcherprop_string_match(prop, info->inreplyto);
	/* FIXME: Using inreplyto, but matching the (newly implemented)
         * list of references is better */
        case MATCHCRITERIA_REFERENCES:
		return matcherprop_string_match(prop, info->inreplyto);
	case MATCHCRITERIA_NOT_REFERENCES:
		return !matcherprop_string_match(prop, info->inreplyto);
	case MATCHCRITERIA_TEST:
		return matcherprop_match_test(prop, info);
	case MATCHCRITERIA_NOT_TEST:
		return !matcherprop_match_test(prop, info);
	default:
		return 0;
	}
}

/* ********************* MatcherList *************************** */

/*!
 *\brief	Create a new list of matchers 
 *
 *\param	matchers List of matcher structures
 *\param	bool_and Operator
 *
 *\return	MatcherList * New list
 */
MatcherList *matcherlist_new(GSList *matchers, gboolean bool_and)
{
	MatcherList *cond;

	cond = g_new0(MatcherList, 1);

	cond->matchers = matchers;
	cond->bool_and = bool_and;

	return cond;
}

/*!
 *\brief	Frees a list of matchers
 *
 *\param	cond List of matchers
 */
void matcherlist_free(MatcherList *cond)
{
	GSList *l;

	g_return_if_fail(cond);
	for (l = cond->matchers ; l != NULL ; l = g_slist_next(l)) {
		matcherprop_free((MatcherProp *) l->data);
	}
	g_free(cond);
}

/*!
 *\brief	Skip all headers in a message file
 *
 *\param	fp Message file
 */
static void matcherlist_skip_headers(FILE *fp)
{
	gchar buf[BUFFSIZE];

	while (procheader_get_one_field(buf, sizeof(buf), fp, NULL) != -1)
		;
}

/*!
 *\brief	Check if a header matches a matcher condition
 *
 *\param	matcher Matcher structure to check header for
 *\param	buf Header name
 *
 *\return	boolean TRUE if matching header
 */
static gboolean matcherprop_match_one_header(MatcherProp *matcher,
					     gchar *buf)
{
	gboolean result;
	Header *header;

	switch (matcher->criteria) {
	case MATCHCRITERIA_HEADER:
	case MATCHCRITERIA_NOT_HEADER:
		header = procheader_parse_header(buf);
		if (!header)
			return FALSE;
		if (procheader_headername_equal(header->name,
						matcher->header)) {
			if (matcher->criteria == MATCHCRITERIA_HEADER)
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
	case MATCHCRITERIA_HEADERS_PART:
		return matcherprop_string_match(matcher, buf);
	case MATCHCRITERIA_MESSAGE:
		return matcherprop_string_decode_match(matcher, buf);
	case MATCHCRITERIA_NOT_MESSAGE:
		return !matcherprop_string_decode_match(matcher, buf);
	case MATCHCRITERIA_NOT_HEADERS_PART:
		return !matcherprop_string_match(matcher, buf);
	}
	return FALSE;
}

/*!
 *\brief	Check if the matcher structure wants headers to
 *		be matched
 *
 *\param	matcher Matcher structure
 *
 *\return	gboolean TRUE if the matcher structure describes
 *		a header match condition
 */
static gboolean matcherprop_criteria_headers(const MatcherProp *matcher)
{
	switch (matcher->criteria) {
	case MATCHCRITERIA_HEADER:
	case MATCHCRITERIA_NOT_HEADER:
	case MATCHCRITERIA_HEADERS_PART:
	case MATCHCRITERIA_NOT_HEADERS_PART:
		return TRUE;
	default:
		return FALSE;
	}
}

/*!
 *\brief	Check if the matcher structure wants the message
 *		to be matched (just perform an action on any
 *		message)
 *
 *\param	matcher Matcher structure
 *
 *\return	gboolean TRUE if matcher condition should match
 *		a message
 */
static gboolean matcherprop_criteria_message(MatcherProp *matcher)
{
	switch (matcher->criteria) {
	case MATCHCRITERIA_MESSAGE:
	case MATCHCRITERIA_NOT_MESSAGE:
		return TRUE;
	default:
		return FALSE;
	}
}

/*!
 *\brief	Check if a list of conditions matches one header in
 *		a message file.
 *
 *\param	matchers List of conditions
 *\param	fp Message file
 *
 *\return	gboolean TRUE if one of the headers is matched by
 *		the list of conditions.	
 */
static gboolean matcherlist_match_headers(MatcherList *matchers, FILE *fp)
{
	GSList *l;
	gchar buf[BUFFSIZE];

	while (procheader_get_one_field(buf, sizeof(buf), fp, NULL) != -1) {
		for (l = matchers->matchers ; l != NULL ; l = g_slist_next(l)) {
			MatcherProp *matcher = (MatcherProp *) l->data;

			if (matcher->done)
				continue;

			/* if the criteria is ~headers_part or ~message, ZERO lines
			 * must NOT match for the rule to match. */
			if (matcher->criteria == MATCHCRITERIA_NOT_HEADERS_PART ||
			    matcher->criteria == MATCHCRITERIA_NOT_MESSAGE) {
				if (matcherprop_match_one_header(matcher, buf)) {
					matcher->result = TRUE;
				} else {
					matcher->result = FALSE;
					matcher->done = TRUE;
				}
			/* else, just one line matching is enough for the rule to match
			 */
			} else if (matcherprop_criteria_headers(matcher) ||
			           matcherprop_criteria_message(matcher)){
				if (matcherprop_match_one_header(matcher, buf)) {
					matcher->result = TRUE;
					matcher->done = TRUE;
				}
			}
			
			/* if the rule matched and the matchers are OR, no need to
			 * check the others */
			if (matcher->result && matcher->done) {
				if (!matchers->bool_and)
					return TRUE;
			}
		}
	}
	return FALSE;
}

/*!
 *\brief	Check if a matcher wants to check the message body
 *
 *\param	matcher Matcher structure
 *
 *\return	gboolean TRUE if body must be matched.
 */
static gboolean matcherprop_criteria_body(const MatcherProp *matcher)
{
	switch (matcher->criteria) {
	case MATCHCRITERIA_BODY_PART:
	case MATCHCRITERIA_NOT_BODY_PART:
		return TRUE;
	default:
		return FALSE;
	}
}

/*!
 *\brief	Check if a (line) string matches the criteria
 *		described by a matcher structure
 *
 *\param	matcher Matcher structure
 *\param	line String
 *
 *\return	gboolean TRUE if string matches criteria
 */
static gboolean matcherprop_match_line(MatcherProp *matcher, const gchar *line)
{
	switch (matcher->criteria) {
	case MATCHCRITERIA_BODY_PART:
	case MATCHCRITERIA_MESSAGE:
		return matcherprop_string_decode_match(matcher, line);
	case MATCHCRITERIA_NOT_BODY_PART:
	case MATCHCRITERIA_NOT_MESSAGE:
		return !matcherprop_string_decode_match(matcher, line);
	}
	return FALSE;
}

/*!
 *\brief	Check if a line in a message file's body matches
 *		the criteria
 *
 *\param	matchers List of conditions
 *\param	fp Message file
 *
 *\return	gboolean TRUE if succesful match
 */
static gboolean matcherlist_match_body(MatcherList *matchers, FILE *fp)
{
	GSList *l;
	gchar buf[BUFFSIZE];
	
	while (fgets(buf, sizeof(buf), fp) != NULL) {
		for (l = matchers->matchers ; l != NULL ; l = g_slist_next(l)) {
			MatcherProp *matcher = (MatcherProp *) l->data;
			
			if (matcher->done) 
				continue;

			/* if the criteria is ~body_part or ~message, ZERO lines
			 * must NOT match for the rule to match. */
			if (matcher->criteria == MATCHCRITERIA_NOT_BODY_PART ||
			    matcher->criteria == MATCHCRITERIA_NOT_MESSAGE) {
				if (matcherprop_match_line(matcher, buf)) {
					matcher->result = TRUE;
				} else {
					matcher->result = FALSE;
					matcher->done = TRUE;
				}
			/* else, just one line has to match */
			} else if (matcherprop_criteria_body(matcher) ||
			           matcherprop_criteria_message(matcher)) {
				if (matcherprop_match_line(matcher, buf)) {
					matcher->result = TRUE;
					matcher->done = TRUE;
				}
			}

			/* if the matchers are OR'ed and the rule matched,
			 * no need to check the others. */
			if (matcher->result && matcher->done) {
				if (!matchers->bool_and)
					return TRUE;
			}
		}
	}
	return FALSE;
}

/*!
 *\brief	Check if a message file matches criteria
 *
 *\param	matchers Criteria
 *\param	info Message info
 *\param	result Default result
 *
 *\return	gboolean TRUE if matched
 */
gboolean matcherlist_match_file(MatcherList *matchers, MsgInfo *info,
				gboolean result)
{
	gboolean read_headers;
	gboolean read_body;
	GSList *l;
	FILE *fp;
	gchar *file;

	/* file need to be read ? */

	read_headers = FALSE;
	read_body = FALSE;
	for (l = matchers->matchers ; l != NULL ; l = g_slist_next(l)) {
		MatcherProp *matcher = (MatcherProp *) l->data;

		if (matcherprop_criteria_headers(matcher))
			read_headers = TRUE;
		if (matcherprop_criteria_body(matcher))
			read_body = TRUE;
		if (matcherprop_criteria_message(matcher)) {
			read_headers = TRUE;
			read_body = TRUE;
		}
		matcher->result = FALSE;
		matcher->done = FALSE;
	}

	if (!read_headers && !read_body)
		return result;

	file = procmsg_get_message_file_full(info, read_headers, read_body);
	if (file == NULL)
		return FALSE;

	if ((fp = g_fopen(file, "rb")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		g_free(file);
		return result;
	}

	/* read the headers */

	if (read_headers) {
		if (matcherlist_match_headers(matchers, fp))
			read_body = FALSE;
	} else {
		matcherlist_skip_headers(fp);
	}

	/* read the body */
	if (read_body) {
		matcherlist_match_body(matchers, fp);
	}
	
	for (l = matchers->matchers; l != NULL; l = g_slist_next(l)) {
		MatcherProp *matcher = (MatcherProp *) l->data;

		if (matcherprop_criteria_headers(matcher) ||
		    matcherprop_criteria_body(matcher)	  ||
		    matcherprop_criteria_message(matcher)) {
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
	}

	g_free(file);

	fclose(fp);
	
	return result;
}

/*!
 *\brief	Test list of conditions on a message.
 *
 *\param	matchers List of conditions
 *\param	info Message info
 *
 *\return	gboolean TRUE if matched
 */
gboolean matcherlist_match(MatcherList *matchers, MsgInfo *info)
{
	GSList *l;
	gboolean result;

	if (!matchers)
		return FALSE;

	if (matchers->bool_and)
		result = TRUE;
	else
		result = FALSE;

	/* test the cached elements */

	for (l = matchers->matchers; l != NULL ;l = g_slist_next(l)) {
		MatcherProp *matcher = (MatcherProp *) l->data;

		switch(matcher->criteria) {
		case MATCHCRITERIA_ALL:
		case MATCHCRITERIA_UNREAD:
		case MATCHCRITERIA_NOT_UNREAD:
		case MATCHCRITERIA_NEW:
		case MATCHCRITERIA_NOT_NEW:
		case MATCHCRITERIA_MARKED:
		case MATCHCRITERIA_NOT_MARKED:
		case MATCHCRITERIA_DELETED:
		case MATCHCRITERIA_NOT_DELETED:
		case MATCHCRITERIA_REPLIED:
		case MATCHCRITERIA_NOT_REPLIED:
		case MATCHCRITERIA_FORWARDED:
		case MATCHCRITERIA_NOT_FORWARDED:
		case MATCHCRITERIA_LOCKED:
		case MATCHCRITERIA_NOT_LOCKED:
		case MATCHCRITERIA_COLORLABEL:
		case MATCHCRITERIA_NOT_COLORLABEL:
		case MATCHCRITERIA_IGNORE_THREAD:
		case MATCHCRITERIA_NOT_IGNORE_THREAD:
		case MATCHCRITERIA_SUBJECT:
		case MATCHCRITERIA_NOT_SUBJECT:
		case MATCHCRITERIA_FROM:
		case MATCHCRITERIA_NOT_FROM:
		case MATCHCRITERIA_TO:
		case MATCHCRITERIA_NOT_TO:
		case MATCHCRITERIA_CC:
		case MATCHCRITERIA_NOT_CC:
		case MATCHCRITERIA_TO_OR_CC:
		case MATCHCRITERIA_NOT_TO_AND_NOT_CC:
		case MATCHCRITERIA_AGE_GREATER:
		case MATCHCRITERIA_AGE_LOWER:
		case MATCHCRITERIA_NEWSGROUPS:
		case MATCHCRITERIA_NOT_NEWSGROUPS:
		case MATCHCRITERIA_INREPLYTO:
		case MATCHCRITERIA_NOT_INREPLYTO:
		case MATCHCRITERIA_REFERENCES:
		case MATCHCRITERIA_NOT_REFERENCES:
		case MATCHCRITERIA_SCORE_GREATER:
		case MATCHCRITERIA_SCORE_LOWER:
		case MATCHCRITERIA_SCORE_EQUAL:
		case MATCHCRITERIA_SIZE_GREATER:
		case MATCHCRITERIA_SIZE_SMALLER:
		case MATCHCRITERIA_SIZE_EQUAL:
		case MATCHCRITERIA_TEST:
		case MATCHCRITERIA_NOT_TEST:
		case MATCHCRITERIA_PARTIAL:
		case MATCHCRITERIA_NOT_PARTIAL:
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


static gint quote_filter_str(gchar * result, guint size,
			     const gchar * path)
{
	const gchar * p;
	gchar * result_p;
	guint remaining;

	result_p = result;
	remaining = size;

	for(p = path ; * p != '\0' ; p ++) {

		if ((* p != '\"') && (* p != '\\')) {
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


gchar * matcher_quote_str(const gchar * src)
{
	gchar * res;
	gint len;
	
	len = strlen(src) * 2 + 1;
	res = g_malloc(len);
	quote_filter_str(res, len, src);
	
	return res;
}

/*!
 *\brief	Convert a matcher structure to a string
 *
 *\param	matcher Matcher structure
 *
 *\return	gchar * Newly allocated string
 */
gchar *matcherprop_to_string(MatcherProp *matcher)
{
	gchar *matcher_str = NULL;
	const gchar *criteria_str;
	const gchar *matchtype_str;
	int i;
	gchar * quoted_expr;
	
	criteria_str = NULL;
	for (i = 0; i < (int) (sizeof(matchparser_tab) / sizeof(MatchParser)); i++) {
		if (matchparser_tab[i].id == matcher->criteria)
			criteria_str = matchparser_tab[i].str;
	}
	if (criteria_str == NULL)
		return NULL;

	switch (matcher->criteria) {
	case MATCHCRITERIA_AGE_GREATER:
	case MATCHCRITERIA_AGE_LOWER:
	case MATCHCRITERIA_SCORE_GREATER:
	case MATCHCRITERIA_SCORE_LOWER:
	case MATCHCRITERIA_SCORE_EQUAL:
	case MATCHCRITERIA_SIZE_GREATER:
	case MATCHCRITERIA_SIZE_SMALLER:
	case MATCHCRITERIA_SIZE_EQUAL:
	case MATCHCRITERIA_COLORLABEL:
	case MATCHCRITERIA_NOT_COLORLABEL:
		return g_strdup_printf("%s %i", criteria_str, matcher->value);
	case MATCHCRITERIA_ALL:
	case MATCHCRITERIA_UNREAD:
	case MATCHCRITERIA_NOT_UNREAD:
	case MATCHCRITERIA_NEW:
	case MATCHCRITERIA_NOT_NEW:
	case MATCHCRITERIA_MARKED:
	case MATCHCRITERIA_NOT_MARKED:
	case MATCHCRITERIA_DELETED:
	case MATCHCRITERIA_NOT_DELETED:
	case MATCHCRITERIA_REPLIED:
	case MATCHCRITERIA_NOT_REPLIED:
	case MATCHCRITERIA_FORWARDED:
	case MATCHCRITERIA_NOT_FORWARDED:
	case MATCHCRITERIA_LOCKED:
	case MATCHCRITERIA_NOT_LOCKED:
	case MATCHCRITERIA_PARTIAL:
	case MATCHCRITERIA_NOT_PARTIAL:
	case MATCHCRITERIA_IGNORE_THREAD:
	case MATCHCRITERIA_NOT_IGNORE_THREAD:
		return g_strdup(criteria_str);
	case MATCHCRITERIA_TEST:
	case MATCHCRITERIA_NOT_TEST:
		quoted_expr = matcher_quote_str(matcher->expr);
		matcher_str = g_strdup_printf("%s \"%s\"",
					      criteria_str, quoted_expr);
		g_free(quoted_expr);
                return matcher_str;
	}

	matchtype_str = NULL;
	for (i = 0; i < sizeof matchparser_tab / sizeof matchparser_tab[0]; i++) {
		if (matchparser_tab[i].id == matcher->matchtype)
			matchtype_str = matchparser_tab[i].str;
	}

	if (matchtype_str == NULL)
		return NULL;

	switch (matcher->matchtype) {
	case MATCHTYPE_MATCH:
	case MATCHTYPE_MATCHCASE:
	case MATCHTYPE_REGEXP:
	case MATCHTYPE_REGEXPCASE:
	case MATCHTYPE_ALL_IN_ADDRESSBOOK:
	case MATCHTYPE_ANY_IN_ADDRESSBOOK:
		quoted_expr = matcher_quote_str(matcher->expr);
		if (matcher->header) {
			gchar * quoted_header;
			
			quoted_header = matcher_quote_str(matcher->header);
			matcher_str = g_strdup_printf
					("%s \"%s\" %s \"%s\"",
					 criteria_str, quoted_header,
					 matchtype_str, quoted_expr);
			g_free(quoted_header);
		}
		else
			matcher_str = g_strdup_printf
					("%s %s \"%s\"", criteria_str,
					 matchtype_str, quoted_expr);
                g_free(quoted_expr);
		break;
	}

	return matcher_str;
}

/*!
 *\brief	Convert a list of conditions to a string
 *
 *\param	matchers List of conditions
 *
 *\return	gchar * Newly allocated string
 */
gchar *matcherlist_to_string(const MatcherList *matchers)
{
	gint count;
	gchar **vstr;
	GSList *l;
	gchar **cur_str;
	gchar *result = NULL;

	count = g_slist_length(matchers->matchers);
	vstr = g_new(gchar *, count + 1);

	for (l = matchers->matchers, cur_str = vstr; l != NULL;
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

	for (cur_str = vstr ; *cur_str != NULL ; cur_str ++)
		g_free(*cur_str);
	g_free(vstr);

	return result;
}


#define STRLEN_ZERO(s) ((s) ? strlen(s) : 0)
#define STRLEN_DEFAULT(s,d) ((s) ? strlen(s) : STRLEN_ZERO(d))

static void add_str_default(gchar ** dest,
			    const gchar * s, const gchar * d)
{
	gchar quoted_str[4096];
	const gchar * str;
	
        if (s != NULL)
		str = s;
	else
		str = d;
	
	quote_cmd_argument(quoted_str, sizeof(quoted_str), str);
	strcpy(* dest, quoted_str);
	
	(* dest) += strlen(* dest);
}

/* matching_build_command() - preferably cmd should be unescaped */
/*!
 *\brief	Build the command line to execute
 *
 *\param	cmd String with command line specifiers
 *\param	info Message info to use for command
 *
 *\return	gchar * Newly allocated string
 */
gchar *matching_build_command(const gchar *cmd, MsgInfo *info)
{
	const gchar *s = cmd;
	gchar *filename = NULL;
	gchar *processed_cmd;
	gchar *p;
	gint size;

	const gchar *const no_subject    = _("(none)") ;
	const gchar *const no_from       = _("(none)") ;
	const gchar *const no_to         = _("(none)") ;
	const gchar *const no_cc         = _("(none)") ;
	const gchar *const no_date       = _("(none)") ;
	const gchar *const no_msgid      = _("(none)") ;
	const gchar *const no_newsgroups = _("(none)") ;
	const gchar *const no_references = _("(none)") ;

	size = STRLEN_ZERO(cmd) + 1;
	while (*s != '\0') {
		if (*s == '%') {
			s++;
			switch (*s) {
			case '%':
				size -= 1;
				break;
			case 's': /* subject */
				size += STRLEN_DEFAULT(info->subject, no_subject) - 2;
				break;
			case 'f': /* from */
				size += STRLEN_DEFAULT(info->from, no_from) - 2;
				break;
			case 't': /* to */
				size += STRLEN_DEFAULT(info->to, no_to) - 2;
				break;
			case 'c': /* cc */
				size += STRLEN_DEFAULT(info->cc, no_cc) - 2;
				break;
			case 'd': /* date */
				size += STRLEN_DEFAULT(info->date, no_date) - 2;
				break;
			case 'i': /* message-id */
				size += STRLEN_DEFAULT(info->msgid, no_msgid) - 2;
				break;
			case 'n': /* newsgroups */
				size += STRLEN_DEFAULT(info->newsgroups, no_newsgroups) - 2;
				break;
			case 'r': /* references */
                                /* FIXME: using the inreplyto header for reference */
				size += STRLEN_DEFAULT(info->inreplyto, no_references) - 2;
				break;
			case 'F': /* file */
				if (filename == NULL)
					filename = folder_item_fetch_msg(info->folder, info->msgnum);
				
				if (filename == NULL) {
					g_warning("filename is not set");
					return NULL;
				}
				else {
					size += strlen(filename) - 2;
				}
				break;
			}
			s++;
		}
		else s++;
	}
	
	/* as the string can be quoted, we double the result */
	size *= 2;

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
				add_str_default(&p, info->subject,
						no_subject);
				break;
			case 'f': /* from */
				add_str_default(&p, info->from,
						no_from);
				break;
			case 't': /* to */
				add_str_default(&p, info->to,
						no_to);
				break;
			case 'c': /* cc */
				add_str_default(&p, info->cc,
						no_cc);
				break;
			case 'd': /* date */
				add_str_default(&p, info->date,
						no_date);
				break;
			case 'i': /* message-id */
				add_str_default(&p, info->msgid,
						no_msgid);
				break;
			case 'n': /* newsgroups */
				add_str_default(&p, info->newsgroups,
						no_newsgroups);
				break;
			case 'r': /* references */
                                /* FIXME: using the inreplyto header for references */
				add_str_default(&p, info->inreplyto, no_references);
				break;
			case 'F': /* file */
				if (filename != NULL)
					add_str_default(&p, filename, NULL);
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
#undef STRLEN_DEFAULT
#undef STRLEN_ZERO

/* ************************************************************ */


/*!
 *\brief	Write filtering list to file
 *
 *\param	fp File
 *\param	prefs_filtering List of filtering conditions
 */
static void prefs_filtering_write(FILE *fp, GSList *prefs_filtering)
{
	GSList *cur;

	for (cur = prefs_filtering; cur != NULL; cur = cur->next) {
		gchar *filtering_str;
		gchar *tmp_name = NULL;
		FilteringProp *prop;

		if (NULL == (prop = (FilteringProp *) cur->data))
			continue;
		
		if (NULL == (filtering_str = filteringprop_to_string(prop)))
			continue;
				
		if (fputs("rulename \"", fp) == EOF) {
			FILE_OP_ERROR("filtering config", "fputs || fputc");
			g_free(filtering_str);
			return;
		}
		tmp_name = prop->name;
		while (tmp_name && *tmp_name != '\0') {
			if (*tmp_name != '"') {
				if (fputc(*tmp_name, fp) == EOF) {
					FILE_OP_ERROR("filtering config", "fputc");
					g_free(filtering_str);
					return;
				}
			} else if (*tmp_name == '"') {
				if (fputc('\\', fp) == EOF ||
				    fputc('"', fp) == EOF) {
					FILE_OP_ERROR("filtering config", "fputc");
					g_free(filtering_str);
					return;
				}
			}
			tmp_name ++;
		}
		if(fputs("\" ", fp) == EOF ||
		    fputs(filtering_str, fp) == EOF ||
		    fputc('\n', fp) == EOF) {
			FILE_OP_ERROR("filtering config", "fputs || fputc");
			g_free(filtering_str);
			return;
		}
		g_free(filtering_str);
	}
}

/*!
 *\brief	Write matchers from a folder item
 *
 *\param	node Node with folder info
 *\param	data File pointer
 *
 *\return	gboolean FALSE
 */
static gboolean prefs_matcher_write_func(GNode *node, gpointer data)
{
	FolderItem *item;
	FILE *fp = data;
	gchar *id;
	GSList *prefs_filtering;

        item = node->data;
        /* prevent warning */
        if (item->path == NULL)
                return FALSE;
        id = folder_item_get_identifier(item);
        if (id == NULL)
                return FALSE;
        prefs_filtering = item->prefs->processing;

	if (prefs_filtering != NULL) {
		fprintf(fp, "[%s]\n", id);
		prefs_filtering_write(fp, prefs_filtering);
		fputc('\n', fp);
	}

	g_free(id);

	return FALSE;
}

/*!
 *\brief	Save matchers from folder items
 *
 *\param	fp File
 */
static void prefs_matcher_save(FILE *fp)
{
	GList *cur;

	for (cur = folder_get_list() ; cur != NULL ; cur = g_list_next(cur)) {
		Folder *folder;

		folder = (Folder *) cur->data;
		g_node_traverse(folder->node, G_PRE_ORDER, G_TRAVERSE_ALL, -1,
				prefs_matcher_write_func, fp);
	}
        
        /* pre global rules */
        fprintf(fp, "[preglobal]\n");
        prefs_filtering_write(fp, pre_global_processing);
        fputc('\n', fp);

        /* post global rules */
        fprintf(fp, "[postglobal]\n");
        prefs_filtering_write(fp, post_global_processing);
        fputc('\n', fp);
        
        /* filtering rules */
        fprintf(fp, "[filtering]\n");
        prefs_filtering_write(fp, filtering_rules);
        fputc('\n', fp);
}

/*!
 *\brief	Write filtering / matcher configuration file
 */
void prefs_matcher_write_config(void)
{
	gchar *rcpath;
	PrefFile *pfile;

	debug_print("Writing matcher configuration...\n");

	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
			     MATCHER_RC, NULL);

	if ((pfile = prefs_write_open(rcpath)) == NULL) {
		g_warning("failed to write configuration to file\n");
		g_free(rcpath);
		return;
	}


	prefs_matcher_save(pfile->fp);

	g_free(rcpath);

	if (prefs_file_close(pfile) < 0) {
		g_warning("failed to write configuration to file\n");
		return;
	}
}

/* ******************************************************************* */

void matcher_add_rulenames(const gchar *rcpath)
{
	gchar *newpath = g_strconcat(rcpath, ".new", NULL);
	FILE *src = g_fopen(rcpath, "rb");
	FILE *dst = g_fopen(newpath, "wb");
	gchar buf[BUFFSIZE];

	if (dst == NULL) {
		perror("fopen");
		g_free(newpath);
		return;
	}

	while (fgets (buf, sizeof(buf), src) != NULL) {
		if (strlen(buf) > 2 && buf[0] != '['
		&& strncmp(buf, "rulename \"", 10)) {
			fwrite("rulename \"\" ",
				strlen("rulename \"\" "), 1, dst);
		}
		fwrite(buf, strlen(buf), 1, dst);
	}
	fclose(dst);
	fclose(src);
	move_file(newpath, rcpath, TRUE);
	g_free(newpath);
}

/*!
 *\brief	Read matcher configuration
 */
void prefs_matcher_read_config(void)
{
	gchar *rcpath;
	gchar *rc_old_format;
	FILE *f;

	create_matchparser_hashtab();
	prefs_filtering_clear();

	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, MATCHER_RC, NULL);
	rc_old_format = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, MATCHER_RC, 
				".pre_names", NULL);
	
	if (!is_file_exist(rc_old_format) && is_file_exist(rcpath)) {
		/* backup file with no rules names, in case 
		 * anything goes wrong */
		copy_file(rcpath, rc_old_format, FALSE);
		/* now hack the file in order to have it to the new format */
		matcher_add_rulenames(rcpath);
	}
	
	g_free(rc_old_format);

	f = g_fopen(rcpath, "rb");
	g_free(rcpath);

	if (f != NULL) {
		matcher_parser_start_parsing(f);
		fclose(matcher_parserin);
	}
	else {
		/* previous version compatibily */

		/* printf("reading filtering\n"); */
		rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
				     FILTERING_RC, NULL);
		f = g_fopen(rcpath, "rb");
		g_free(rcpath);
		
		if (f != NULL) {
			matcher_parser_start_parsing(f);
			fclose(matcher_parserin);
		}
		
		/* printf("reading scoring\n"); */
		rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
				     SCORING_RC, NULL);
		f = g_fopen(rcpath, "rb");
		g_free(rcpath);
		
		if (f != NULL) {
			matcher_parser_start_parsing(f);
			fclose(matcher_parserin);
		}
	}
}
