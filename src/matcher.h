#ifndef MATCHER_H

#define MATCHER_H

#include <sys/types.h>
#include <regex.h>
#include <glib.h>
#include "procmsg.h"

enum {
	SCORING_ALL,
	SCORING_SUBJECT,
	SCORING_NOT_SUBJECT,
	SCORING_FROM,
	SCORING_NOT_FROM,
	SCORING_TO,
	SCORING_NOT_TO,
	SCORING_CC,
	SCORING_NOT_CC,
	SCORING_TO_OR_CC,
	SCORING_NOT_TO_AND_NOT_CC,
	SCORING_AGE_SUP,
	SCORING_AGE_INF,
	SCORING_NEWSGROUPS,
	SCORING_NOT_NEWSGROUPS,
	SCORING_HEADER,
	SCORING_NOT_HEADER,
	SCORING_MESSAGE,
	SCORING_NOT_MESSAGE,
	SCORING_MESSAGEHEADERS,
	SCORING_NOT_MESSAGEHEADERS,
	SCORING_BODY,
	SCORING_NOT_BODY,
	SCORING_SCORE,
	SCORING_MATCH,
	SCORING_REGEXP,
	SCORING_MATCHCASE,
	SCORING_REGEXPCASE
};

struct _MatcherProp {
	int matchtype;
	int criteria;
	gchar * header;
	gchar * expr;
	int age;
	regex_t * preg;
	int error;
};

typedef struct _MatcherProp MatcherProp;

struct _MatcherList {
	GSList * matchers;
	gboolean bool_and;
};

typedef struct _MatcherList MatcherList;

MatcherProp * matcherprop_new(gint criteria, gchar * header,
			      gint matchtype, gchar * expr,
			      int age);
void matcherprop_free(MatcherProp * prop);
MatcherProp * matcherprop_parse(gchar ** str);

gboolean matcherprop_match(MatcherProp * prop, MsgInfo * info);

MatcherList * matcherlist_new(GSList * matchers, gboolean bool_and);
void matcherlist_free(MatcherList * cond);
MatcherList * matcherlist_parse(gchar ** str);

gboolean matcherlist_match(MatcherList * cond, MsgInfo * info);

gint matcher_parse_keyword(gchar ** str);
gint matcher_parse_number(gchar ** str);
gboolean matcher_parse_boolean_op(gchar ** str);
gchar * matcher_parse_regexp(gchar ** str);
gchar * matcher_parse_str(gchar ** str);

#endif
