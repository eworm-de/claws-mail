#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "utils.h"
#include "defs.h"
#include "procheader.h"
#include "matcher.h"

struct _MatchParser {
	gint id;
	gchar * str;
};

typedef struct _MatchParser MatchParser;

MatchParser matchparser_tab[] = {
	/* msginfo */
	{SCORING_ALL, "all"},
	{SCORING_SUBJECT, "subject"},
	{SCORING_NOT_SUBJECT, "~subject"},
	{SCORING_FROM, "from"},
	{SCORING_NOT_FROM, "~from"},
	{SCORING_TO, "to"},
	{SCORING_NOT_TO, "~to"},
	{SCORING_CC, "cc"},
	{SCORING_NOT_CC, "~cc"},
	{SCORING_TO_OR_CC, "to_or_cc"},
	{SCORING_NOT_TO_AND_NOT_CC, "~to_or_cc"},
	{SCORING_AGE_SUP, "age_sup"},
	{SCORING_AGE_INF, "age_inf"},
	{SCORING_NEWSGROUPS, "newsgroups"},
	{SCORING_NOT_NEWSGROUPS, "~newsgroups"},

	/* content have to be read */
	{SCORING_HEADER, "header"},
	{SCORING_NOT_HEADER, "~header"},
	{SCORING_MESSAGEHEADERS, "messageheaders"},
	{SCORING_NOT_MESSAGEHEADERS, "~messageheaders"},
	{SCORING_MESSAGE, "message"},
	{SCORING_NOT_MESSAGE, "~message"},
	{SCORING_BODY, "body"},
	{SCORING_NOT_BODY, "~body"},

	/* match type */
	{SCORING_MATCHCASE, "matchcase"},
	{SCORING_MATCH, "match"},
	{SCORING_REGEXPCASE, "regexpcase"},
	{SCORING_REGEXP, "regexp"},

	/* actions */
	{SCORING_SCORE, "score"},
};

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
	gint age;
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
	case SCORING_AGE_INF:
	case SCORING_AGE_SUP:
		age = matcher_parse_number(&tmp);
		if (tmp == NULL) {
			* str = NULL;
			return NULL;
		}
		*str = tmp;

		prop = matcherprop_new(key, NULL, 0, NULL, age);

		return prop;

	case SCORING_ALL:
		prop = matcherprop_new(key, NULL, 0, NULL, 0);
		*str = tmp;

		return prop;

	case SCORING_SUBJECT:
	case SCORING_NOT_SUBJECT:
	case SCORING_FROM:
	case SCORING_NOT_FROM:
	case SCORING_TO:
	case SCORING_NOT_TO:
	case SCORING_CC:
	case SCORING_NOT_CC:
	case SCORING_TO_OR_CC:
	case SCORING_NOT_TO_AND_NOT_CC:
	case SCORING_NEWSGROUPS:
	case SCORING_NOT_NEWSGROUPS:
	case SCORING_MESSAGE:
	case SCORING_NOT_MESSAGE:
	case SCORING_MESSAGEHEADERS:
	case SCORING_NOT_MESSAGEHEADERS:
	case SCORING_BODY:
	case SCORING_NOT_BODY:
	case SCORING_HEADER:
	case SCORING_NOT_HEADER:
		if ((key == SCORING_HEADER) || (key == SCORING_NOT_HEADER)) {
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
		case SCORING_REGEXP:
		case SCORING_REGEXPCASE:
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
		case SCORING_MATCH:
		case SCORING_MATCHCASE:
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
	
	match = -1;
	for(i = 0 ; i < (int) (sizeof(matchparser_tab) / sizeof(MatchParser)) ;
	    i++) {
		if (strncasecmp(matchparser_tab[i].str, p,
				strlen(matchparser_tab[i].str)) == 0) {
			p += strlen(matchparser_tab[i].str);
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
			      int age)
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
	prop->age = age;
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

	if (str == NULL)
		str = "";

	switch(prop->matchtype) {
	case SCORING_REGEXP:
	case SCORING_REGEXPCASE:
		if (!prop->preg && (prop->error == 0)) {
			prop->preg = g_new0(regex_t, 1);
			if (regcomp(prop->preg, prop->expr,
				    REG_NOSUB | REG_EXTENDED
				    | ((prop->matchtype == SCORING_REGEXPCASE)
				    ? REG_ICASE : 0)) != 0) {
				prop->error = 1;
				g_free(prop->preg);
			}
		}
		if (prop->preg == NULL)
			return 0;
		
		if (regexec(prop->preg, str, 0, NULL, 0) == 0)
			return 1;
		else
			return 0;

	case SCORING_MATCH:
		return (strstr(str, prop->expr) != NULL);

	case SCORING_MATCHCASE:
		g_strup(prop->expr);
		str1 = alloca(strlen(str) + 1);
		strcpy(str1, str);
		g_strup(str1);
		return (strstr(str1, prop->expr) != NULL);
		
	default:
		return 0;
	}
}

/* match a message and his headers, hlist can be NULL if you don't
   want to use headers */

gboolean matcherprop_match(MatcherProp * prop, MsgInfo * info)
{
	time_t t;

	switch(prop->criteria) {
	case SCORING_ALL:
		return 1;
	case SCORING_SUBJECT:
		return matcherprop_string_match(prop, info->subject);
	case SCORING_NOT_SUBJECT:
		return !matcherprop_string_match(prop, info->subject);
	case SCORING_FROM:
		return matcherprop_string_match(prop, info->from);
	case SCORING_NOT_FROM:
		return !matcherprop_string_match(prop, info->from);
	case SCORING_TO:
		return matcherprop_string_match(prop, info->to);
	case SCORING_NOT_TO:
		return !matcherprop_string_match(prop, info->to);
	case SCORING_CC:
		return matcherprop_string_match(prop, info->cc);
	case SCORING_NOT_CC:
		return !matcherprop_string_match(prop, info->cc);
	case SCORING_TO_OR_CC:
		return matcherprop_string_match(prop, info->to)
			|| matcherprop_string_match(prop, info->cc);
	case SCORING_NOT_TO_AND_NOT_CC:
		return !(matcherprop_string_match(prop, info->to)
		|| matcherprop_string_match(prop, info->cc));
	case SCORING_AGE_SUP:
		t = time(NULL);
		return (t - info->date_t) > prop->age;
	case SCORING_AGE_INF:
		t = time(NULL);
		return (t - info->date_t) < prop->age;
	case SCORING_NEWSGROUPS:
		return matcherprop_string_match(prop, info->newsgroups);
	case SCORING_NOT_NEWSGROUPS:
		return !matcherprop_string_match(prop, info->newsgroups);
	case SCORING_HEADER:
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
	gboolean main_bool_and;

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

static void matcherprop_skip_headers(FILE *fp)
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
	case SCORING_HEADER:
	case SCORING_NOT_HEADER:
		header = procheader_parse_header(buf);
		if (procheader_headername_equal(header->name,
						matcher->header)) {
			if (matcher->criteria == SCORING_HEADER)
				result = matcherprop_string_match(matcher, header->body);
			else
				result = !matcherprop_string_match(matcher, header->body);
			procheader_header_free(header);
			return result;
		}
		break;
	case SCORING_MESSAGEHEADERS:
	case SCORING_MESSAGE:
		return matcherprop_string_match(matcher, buf);
	case SCORING_NOT_MESSAGE:
	case SCORING_NOT_MESSAGEHEADERS:
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
	case SCORING_HEADER:
	case SCORING_NOT_HEADER:
	case SCORING_MESSAGEHEADERS:
	case SCORING_NOT_MESSAGEHEADERS:
	case SCORING_MESSAGE:
	case SCORING_NOT_MESSAGE:
		return TRUE;
	default:
		return FALSE;
	}
}

/*
  matcherlist_match_one_header
  returns TRUE if buf matchs the MatchersList criteria
 */

static gboolean matcherlist_match_one_header(MatcherList * matchers,
					     gchar * buf, gboolean result)
{
	GSList * l;
	
	for(l = matchers->matchers ; l != NULL ; l = g_slist_next(l)) {
		MatcherProp * matcher = (MatcherProp *) l->data;
		gboolean matched = FALSE;
		gboolean partial_result;

		if (matcherprop_criteria_headers(matcher)) {
			if (matcherprop_match_one_header(matcher, buf)) {
				if (!matchers->bool_and)
					return TRUE;
			}
			else {
				if (matchers->bool_and)
					return FALSE;
			}
		}
	}

	return result;
}

/*
  matcherlist_match_headers
  returns TRUE if one of the headers matchs the MatcherList criteria
 */

static gboolean matcherlist_match_headers(MatcherList * matchers, FILE * fp,
					  gboolean result)
{
	gchar buf[BUFFSIZE];

	while (procheader_get_one_field(buf, sizeof(buf), fp, NULL) != -1) {
		if (matcherlist_match_one_header(matchers, buf, result)) {
			if (!matchers->bool_and)
				return TRUE;
		}
		else {
			if (matchers->bool_and)
				return FALSE;
		}
	}
	return result;
}

/*
  matcherprop_criteria_body
  returns TRUE if the body must be matched
 */

static gboolean matcherprop_criteria_body(MatcherProp * matcher)
{
	switch(matcher->criteria) {
	case SCORING_BODY:
	case SCORING_NOT_BODY:
	case SCORING_MESSAGE:
	case SCORING_NOT_MESSAGE:
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
	case SCORING_BODY:
	case SCORING_MESSAGE:
		return matcherprop_string_match(matcher, line);
	case SCORING_NOT_BODY:
	case SCORING_NOT_MESSAGE:
		return !matcherprop_string_match(matcher, line);
	}
}

/*
  matcherlist_match_line
  returns TRUE if the string matchs the MatcherList criteria
 */

static gboolean matcherlist_match_line(MatcherList * matchers, gchar * line,
				       gboolean result)
{
	GSList * l;

	for(l = matchers->matchers ; l != NULL ; l = g_slist_next(l)) {
		MatcherProp * matcher = (MatcherProp *) l->data;

		if (matcherprop_criteria_body(matcher)) {
			if (matcherprop_match_line(matcher, line)) {
				if (!matchers->bool_and)
					return TRUE;
			}
			else {
				if (matchers->bool_and)
					return FALSE;
			}
		}
	}
	return result;
}

/*
  matcherlist_match_body
  returns TRUE if one line of the body matchs the MatcherList criteria
 */

static gboolean matcherlist_match_body(MatcherList * matchers, FILE * fp,
				       gboolean result)
{
	gchar buf[BUFFSIZE];

	while (fgets(buf, sizeof(buf), fp) != NULL) {
		if (matcherlist_match_line(matchers, buf, result)) {
			if (!matchers->bool_and)
				return TRUE;
		}
		else {
			if (matchers->bool_and)
				return FALSE;
		}
	}
	return result;
}

gboolean matcherlist_match_file(MatcherList * matchers, MsgInfo * info,
				gboolean result)
{
	gboolean read_headers;
	gboolean read_body;
	GSList * l;
	FILE * fp;
	gchar * file;

	/* file need to be read ? */

	read_headers = FALSE;
	read_body = FALSE;
	for(l = matchers->matchers ; l != NULL ; l = g_slist_next(l)) {
		MatcherProp * matcher = (MatcherProp *) l->data;

		if (matcherprop_criteria_headers(matcher))
			read_headers = TRUE;
		if (matcherprop_criteria_body(matcher))
			read_body = TRUE;
	}

	if (!read_headers && !read_body)
		return result;

	file = procmsg_get_message_file_path(info);
	g_return_if_fail(file != NULL);

	if ((fp = fopen(file, "r")) == NULL) {
		FILE_OP_ERROR(file, "fopen");
		g_free(file);
		return result;
	}
	
	/* read the headers */

	if (read_headers) {
		if (matcherlist_match_headers(matchers, fp, result)) {
			if (!matchers->bool_and)
				result = TRUE;
		}
		else {
			if (matchers->bool_and)
				result = FALSE;
		}
	}
	
	/* read the body */
	if (read_body) {
		if (matcherlist_match_body(matchers, fp, result)) {
			if (!matchers->bool_and)
				result = TRUE;
		}
		else {
			if (matchers->bool_and)
				result = FALSE;
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

	/* test the condition on the file */

	if (matcherlist_match_file(matchers, info, result)) {
		if (!matchers->bool_and)
			return TRUE;
	}
	else {
		if (matchers->bool_and)
			return FALSE;
	}

	/* test the cached elements */

	for(l = matchers->matchers ; l != NULL ; l = g_slist_next(l)) {
		MatcherProp * matcher = (MatcherProp *) l->data;

		if (matcherprop_match(matcher, info)) {
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

	return result;
}

#ifdef 0
static void matcherprop_print(MatcherProp * matcher)
{
  int i;

	if (matcher == NULL) {
		printf("no matcher\n");
		return;
	}

	switch (matcher->matchtype) {
	case SCORING_MATCH:
		printf("match\n");
		break;
	case SCORING_REGEXP:
		printf("regexp\n");
		break;
	case SCORING_MATCHCASE:
		printf("matchcase\n");
		break;
	case SCORING_REGEXPCASE:
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

	printf("age: %i\n", matcher->age);

	printf("compiled : %s\n", matcher->preg != NULL ? "yes" : "no");
	printf("error: %i\n",  matcher->error);
}
#endif
