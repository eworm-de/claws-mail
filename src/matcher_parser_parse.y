%{
#include "defs.h"

#include <glib.h>

#include "intl.h"
#include "utils.h"
#include "filtering.h"
#include "scoring.h"
#include "matcher.h"
#include "matcher_parser.h"
#include "matcher_parser_lex.h"

static gint error = 0;
static gint bool_op = 0;
static gint match_type = 0;
static gchar * header = NULL;

static MatcherProp * prop;

static GSList * matchers_list = NULL;

static MatcherList * cond;
static gint score = 0;
static FilteringAction * action = NULL;

static FilteringProp *  filtering;
static ScoringProp * scoring = NULL;

static GSList ** prefs_scoring = NULL;
static GSList ** prefs_filtering = NULL;

static int matcher_parser_dialog = 0;


/* ******************************************************************** */



void matcher_parser_start_parsing(FILE * f)
{
	matcher_parserrestart(f);
	matcher_parserparse();
}
 
FilteringProp * matcher_parser_get_filtering(gchar * str)
{
	void * bufstate;

	/* bad coding to enable the sub-grammar matching
	   in yacc */
	matcher_parserlineno = 1;
	matcher_parser_dialog = 1;
	bufstate = matcher_parser_scan_string(str);
	if (matcher_parserparse() != 0)
		filtering = NULL;
	matcher_parser_dialog = 0;
	matcher_parser_delete_buffer(bufstate);
	return filtering;
}

ScoringProp * matcher_parser_get_scoring(gchar * str)
{
	void * bufstate;

	/* bad coding to enable the sub-grammar matching
	   in yacc */
	matcher_parserlineno = 1;
	matcher_parser_dialog = 1;
	bufstate = matcher_parser_scan_string(str);
	if (matcher_parserparse() != 0)
		scoring = NULL;
	matcher_parser_dialog = 0;
	matcher_parser_delete_buffer(bufstate);
	return scoring;
}

MatcherList * matcher_parser_get_cond(gchar * str)
{
	void * bufstate;

	/* bad coding to enable the sub-grammar matching
	   in yacc */
	matcher_parserlineno = 1;
	matcher_parser_dialog = 1;
	bufstate = matcher_parser_scan_string(str);
	matcher_parserparse();
	matcher_parser_dialog = 0;
	matcher_parser_delete_buffer(bufstate);
	return cond;
}

MatcherProp * matcher_parser_get_prop(gchar * str)
{
	MatcherList * list;
	MatcherProp * prop;

	matcher_parserlineno = 1;
	list = matcher_parser_get_cond(str);
	if (list == NULL)
		return NULL;

	if (list->matchers == NULL)
		return NULL;

	if (list->matchers->next != NULL)
		return NULL;

	prop = list->matchers->data;

	g_slist_free(list->matchers);
	g_free(list);

	return prop;
}

void matcher_parsererror(char * str)
{
	GSList * l;

	if (matchers_list) {
		for(l = matchers_list ; l != NULL ;
		    l = g_slist_next(l))
			matcherprop_free((MatcherProp *)
					 l->data);
		g_slist_free(matchers_list);
		matchers_list = NULL;
	}

	g_warning(_("scoring / filtering parsing: %i: %s\n"),
		  matcher_parserlineno, str);
	error = 1;
}

int matcher_parserwrap(void)
{
	return 1;
}
%}

%union {
	char * str;
	int value;
}

%token MATCHER_ALL MATCHER_UNREAD  MATCHER_NOT_UNREAD 
%token MATCHER_NEW  MATCHER_NOT_NEW  MATCHER_MARKED
%token MATCHER_NOT_MARKED  MATCHER_DELETED  MATCHER_NOT_DELETED
%token MATCHER_REPLIED  MATCHER_NOT_REPLIED  MATCHER_FORWARDED
%token MATCHER_NOT_FORWARDED  MATCHER_SUBJECT  MATCHER_NOT_SUBJECT
%token MATCHER_FROM  MATCHER_NOT_FROM  MATCHER_TO  MATCHER_NOT_TO
%token MATCHER_CC  MATCHER_NOT_CC  MATCHER_TO_OR_CC  MATCHER_NOT_TO_AND_NOT_CC
%token MATCHER_AGE_GREATER  MATCHER_AGE_LOWER  MATCHER_NEWSGROUPS
%token MATCHER_NOT_NEWSGROUPS  MATCHER_INREPLYTO  MATCHER_NOT_INREPLYTO
%token MATCHER_REFERENCES  MATCHER_NOT_REFERENCES  MATCHER_SCORE_GREATER
%token MATCHER_SCORE_LOWER  MATCHER_HEADER  MATCHER_NOT_HEADER
%token MATCHER_HEADERS_PART  MATCHER_NOT_HEADERS_PART  MATCHER_MESSAGE
%token MATCHER_NOT_MESSAGE  MATCHER_BODY_PART  MATCHER_NOT_BODY_PART
%token MATCHER_EXECUTE  MATCHER_NOT_EXECUTE  MATCHER_MATCHCASE  MATCHER_MATCH
%token MATCHER_REGEXPCASE  MATCHER_REGEXP  MATCHER_SCORE  MATCHER_MOVE
%token MATCHER_COPY  MATCHER_DELETE  MATCHER_MARK  MATCHER_UNMARK
%token MATCHER_MARK_AS_READ  MATCHER_MARK_AS_UNREAD  MATCHER_FORWARD
%token MATCHER_FORWARD_AS_ATTACHMENT  MATCHER_EOL  MATCHER_STRING  
%token MATCHER_OR MATCHER_AND  
%token MATCHER_COLOR MATCHER_SCORE_EQUAL MATCHER_BOUNCE MATCHER_DELETE_ON_SERVER
%token MATCHER_SIZE_GREATER MATCHER_SIZE_SMALLER MATCHER_SIZE_EQUAL

%start file

%token <str> MATCHER_STRING
%token <str> MATCHER_SECTION
%token <value> MATCHER_INTEGER

%%

file:
{
	if (!matcher_parser_dialog) {
		prefs_scoring = &global_scoring;
		prefs_filtering = &global_processing;
	}
}
file_line_list;

file_line_list:
file_line
file_line_list
| file_line
;

file_line:
section_notification
| instruction
| error MATCHER_EOL
{
	yyerrok;
};

section_notification:
MATCHER_SECTION MATCHER_EOL
{
	gchar * folder = $1;
	FolderItem * item = NULL;

	if (!matcher_parser_dialog) {
		item = folder_find_item_from_identifier(folder);
		if (item == NULL) {
			prefs_scoring = &global_scoring;
			prefs_filtering = &global_processing;
		}
		else {
			prefs_scoring = &item->prefs->scoring;
			prefs_filtering = &item->prefs->processing;
		}
	}
}
;

instruction:
condition end_instr_opt
| MATCHER_EOL
;

end_instr_opt:
filtering_or_scoring end_action
|
{
	if (matcher_parser_dialog)
		YYACCEPT;
	else {
		matcher_parsererror("parse error");
		YYERROR;
	}
}
;

end_action:
MATCHER_EOL
|
{
	if (matcher_parser_dialog)
		YYACCEPT;
	else {
		matcher_parsererror("parse error");
		YYERROR;
	}
}
;

filtering_or_scoring:
filtering_action
{
	filtering = filteringprop_new(cond, action);
	cond = NULL;
	action = NULL;
	if (!matcher_parser_dialog) {
		* prefs_filtering = g_slist_append(* prefs_filtering,
						   filtering);
		filtering = NULL;
	}
}
| scoring_rule
{
	scoring = scoringprop_new(cond, score);
	cond = NULL;
	score = 0;
	if (!matcher_parser_dialog) {
		* prefs_scoring = g_slist_append(* prefs_scoring, scoring);
		scoring = NULL;
	}
}
;

match_type:
MATCHER_MATCHCASE
{
	match_type = MATCHTYPE_MATCHCASE;
}
| MATCHER_MATCH
{
	match_type = MATCHTYPE_MATCH;
}
| MATCHER_REGEXPCASE
{
	match_type = MATCHTYPE_REGEXPCASE;
}
| MATCHER_REGEXP
{
	match_type = MATCHTYPE_REGEXP;
}
;

condition:
condition_list
{
	cond = matcherlist_new(matchers_list, (bool_op == MATCHERBOOL_AND));
	matchers_list = NULL;
}
;

condition_list:
condition_list bool_op one_condition
{
	matchers_list = g_slist_append(matchers_list, prop);
}
| one_condition
{
	matchers_list = NULL;
	matchers_list = g_slist_append(matchers_list, prop);
}
;

bool_op:
MATCHER_AND
{
	bool_op = MATCHERBOOL_AND;
}
| MATCHER_OR
{
	bool_op = MATCHERBOOL_OR;
}
;

one_condition:
MATCHER_ALL
{
	gint criteria = 0;

	criteria = MATCHCRITERIA_ALL;
	prop = matcherprop_new(criteria, NULL, 0, NULL, 0);
}
| MATCHER_UNREAD
{
	gint criteria = 0;

	criteria = MATCHCRITERIA_UNREAD;
	prop = matcherprop_new(criteria, NULL, 0, NULL, 0);
}
| MATCHER_NOT_UNREAD 
{
	gint criteria = 0;

	criteria = MATCHCRITERIA_NOT_UNREAD;
	prop = matcherprop_new(criteria, NULL, 0, NULL, 0);
}
| MATCHER_NEW
{
	gint criteria = 0;

	criteria = MATCHCRITERIA_NEW;
	prop = matcherprop_new(criteria, NULL, 0, NULL, 0);
}
| MATCHER_NOT_NEW
{
	gint criteria = 0;

	criteria = MATCHCRITERIA_NOT_NEW;
	prop = matcherprop_new(criteria, NULL, 0, NULL, 0);
}
| MATCHER_MARKED
{
	gint criteria = 0;

	criteria = MATCHCRITERIA_MARKED;
	prop = matcherprop_new(criteria, NULL, 0, NULL, 0);
}
| MATCHER_NOT_MARKED
{
	gint criteria = 0;

	criteria = MATCHCRITERIA_NOT_MARKED;
	prop = matcherprop_new(criteria, NULL, 0, NULL, 0);
}
| MATCHER_DELETED
{
	gint criteria = 0;

	criteria = MATCHCRITERIA_DELETED;
	prop = matcherprop_new(criteria, NULL, 0, NULL, 0);
}
| MATCHER_NOT_DELETED
{
	gint criteria = 0;

	criteria = MATCHCRITERIA_NOT_DELETED;
	prop = matcherprop_new(criteria, NULL, 0, NULL, 0);
}
| MATCHER_REPLIED
{
	gint criteria = 0;

	criteria = MATCHCRITERIA_REPLIED;
	prop = matcherprop_new(criteria, NULL, 0, NULL, 0);
}
| MATCHER_NOT_REPLIED
{
	gint criteria = 0;

	criteria = MATCHCRITERIA_NOT_REPLIED;
	prop = matcherprop_new(criteria, NULL, 0, NULL, 0);
}
| MATCHER_FORWARDED
{
	gint criteria = 0;

	criteria = MATCHCRITERIA_FORWARDED;
	prop = matcherprop_new(criteria, NULL, 0, NULL, 0);
}
| MATCHER_NOT_FORWARDED
{
	gint criteria = 0;

	criteria = MATCHCRITERIA_NOT_FORWARDED;
	prop = matcherprop_new(criteria, NULL, 0, NULL, 0);
}
| MATCHER_SUBJECT match_type MATCHER_STRING
{
	gint criteria = 0;
	gchar * expr = NULL;

	criteria = MATCHCRITERIA_SUBJECT;
	expr = $3;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
}
| MATCHER_NOT_SUBJECT match_type MATCHER_STRING
{
	gint criteria = 0;
	gchar * expr = NULL;

	criteria = MATCHCRITERIA_NOT_SUBJECT;
	expr = $3;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
}
| MATCHER_FROM match_type MATCHER_STRING
{
	gint criteria = 0;
	gchar * expr = NULL;

	criteria = MATCHCRITERIA_FROM;
	expr = $3;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
}
| MATCHER_NOT_FROM match_type MATCHER_STRING
{
	gint criteria = 0;
	gchar * expr = NULL;

	criteria = MATCHCRITERIA_NOT_FROM;
	expr = $3;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
}
| MATCHER_TO match_type MATCHER_STRING
{
	gint criteria = 0;
	gchar * expr = NULL;

	criteria = MATCHCRITERIA_TO;
	expr = $3;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
}
| MATCHER_NOT_TO match_type MATCHER_STRING
{
	gint criteria = 0;
	gchar * expr = NULL;

	criteria = MATCHCRITERIA_NOT_TO;
	expr = $3;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
}
| MATCHER_CC match_type MATCHER_STRING
{
	gint criteria = 0;
	gchar * expr = NULL;

	criteria = MATCHCRITERIA_CC;
	expr = $3;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
}
| MATCHER_NOT_CC match_type MATCHER_STRING
{
	gint criteria = 0;
	gchar * expr = NULL;

	criteria = MATCHCRITERIA_NOT_CC;
	expr = $3;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
}
| MATCHER_TO_OR_CC match_type MATCHER_STRING
{
	gint criteria = 0;
	gchar * expr = NULL;

	criteria = MATCHCRITERIA_TO_OR_CC;
	expr = $3;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
}
| MATCHER_NOT_TO_AND_NOT_CC match_type MATCHER_STRING
{
	gint criteria = 0;
	gchar * expr = NULL;

	criteria = MATCHCRITERIA_NOT_TO_AND_NOT_CC;
	expr = $3;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
}
| MATCHER_AGE_GREATER MATCHER_INTEGER
{
	gint criteria = 0;
	gint value = 0;

	criteria = MATCHCRITERIA_AGE_GREATER;
	value = atoi($2);
	prop = matcherprop_new(criteria, NULL, 0, NULL, value);
}
| MATCHER_AGE_LOWER MATCHER_INTEGER
{
	gint criteria = 0;
	gint value = 0;

	criteria = MATCHCRITERIA_AGE_LOWER;
	value = atoi($2);
	prop = matcherprop_new(criteria, NULL, 0, NULL, value);
}
| MATCHER_NEWSGROUPS match_type MATCHER_STRING
{
	gint criteria = 0;
	gchar * expr = NULL;

	criteria = MATCHCRITERIA_NEWSGROUPS;
	expr = $3;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
}
| MATCHER_NOT_NEWSGROUPS match_type MATCHER_STRING
{
	gint criteria = 0;
	gchar * expr = NULL;

	criteria = MATCHCRITERIA_NOT_NEWSGROUPS;
	expr = $3;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
}
| MATCHER_INREPLYTO match_type MATCHER_STRING
{
	gint criteria = 0;
	gchar * expr = NULL;

	criteria = MATCHCRITERIA_INREPLYTO;
	expr = $3;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
}
| MATCHER_NOT_INREPLYTO match_type MATCHER_STRING
{
	gint criteria = 0;
	gchar * expr = NULL;

	criteria = MATCHCRITERIA_NOT_INREPLYTO;
	expr = $3;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
}
| MATCHER_REFERENCES match_type MATCHER_STRING
{
	gint criteria = 0;
	gchar * expr = NULL;

	criteria = MATCHCRITERIA_REFERENCES;
	expr = $3;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
}
| MATCHER_NOT_REFERENCES match_type MATCHER_STRING
{
	gint criteria = 0;
	gchar * expr = NULL;

	criteria = MATCHCRITERIA_NOT_REFERENCES;
	expr = $3;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
}
| MATCHER_SCORE_GREATER MATCHER_INTEGER
{
	gint criteria = 0;
	gint value = 0;

	criteria = MATCHCRITERIA_SCORE_GREATER;
	value = atoi($2);
	prop = matcherprop_new(criteria, NULL, 0, NULL, value);
}
| MATCHER_SCORE_LOWER MATCHER_INTEGER
{
	gint criteria = 0;
	gint value = 0;

	criteria = MATCHCRITERIA_SCORE_LOWER;
	value = atoi($2);
	prop = matcherprop_new(criteria, NULL, 0, NULL, value);
}
| MATCHER_SCORE_EQUAL MATCHER_INTEGER
{
	gint criteria = 0;
	gint value = 0;

	criteria = MATCHCRITERIA_SCORE_EQUAL;
	value = atoi($2);
	prop = matcherprop_new(criteria, NULL, 0, NULL, value);
}
| MATCHER_SIZE_GREATER MATCHER_INTEGER 
{
	gint criteria = 0;
	gint value    = 0;
	criteria = MATCHCRITERIA_SIZE_GREATER;
	value = atoi($2);
	prop = matcherprop_new(criteria, NULL, 0, NULL, value);
}
| MATCHER_SIZE_SMALLER MATCHER_INTEGER
{
	gint criteria = 0;
	gint value    = 0;
	criteria = MATCHCRITERIA_SIZE_SMALLER;
	value = atoi($2);
	prop = matcherprop_new(criteria, NULL, 0, NULL, value);
}
| MATCHER_SIZE_EQUAL MATCHER_INTEGER
{
	gint criteria = 0;
	gint value    = 0;
	criteria = MATCHCRITERIA_SIZE_EQUAL;
	value = atoi($2);
	prop = matcherprop_new(criteria, NULL, 0, NULL, value);
}
| MATCHER_HEADER MATCHER_STRING
{
	header = g_strdup($2);
} match_type MATCHER_STRING
{
	gint criteria = 0;
	gchar * expr = NULL;

	criteria = MATCHCRITERIA_HEADER;
	expr = $2;
	prop = matcherprop_new(criteria, header, match_type, expr, 0);
	g_free(header);
}
| MATCHER_NOT_HEADER MATCHER_STRING
{
	header = g_strdup($2);
} match_type MATCHER_STRING
{
	gint criteria = 0;
	gchar * expr = NULL;

	criteria = MATCHCRITERIA_NOT_HEADER;
	expr = $2;
	prop = matcherprop_new(criteria, header, match_type, expr, 0);
	g_free(header);
}
| MATCHER_HEADERS_PART match_type MATCHER_STRING
{
	gint criteria = 0;
	gchar * expr = NULL;

	criteria = MATCHCRITERIA_HEADERS_PART;
	expr = $3;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
}
| MATCHER_NOT_HEADERS_PART match_type MATCHER_STRING
{
	gint criteria = 0;
	gchar * expr = NULL;

	criteria = MATCHCRITERIA_NOT_HEADERS_PART;
	expr = $3;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
}
| MATCHER_MESSAGE match_type MATCHER_STRING
{
	gint criteria = 0;
	gchar * expr = NULL;

	criteria = MATCHCRITERIA_MESSAGE;
	expr = $3;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
}
| MATCHER_NOT_MESSAGE match_type MATCHER_STRING
{
	gint criteria = 0;
	gchar * expr = NULL;

	criteria = MATCHCRITERIA_NOT_MESSAGE;
	expr = $3;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
}
| MATCHER_BODY_PART match_type MATCHER_STRING
{
	gint criteria = 0;
	gchar * expr = NULL;

	criteria = MATCHCRITERIA_BODY_PART;
	expr = $3;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
}
| MATCHER_NOT_BODY_PART match_type MATCHER_STRING
{
	gint criteria = 0;
	gchar * expr = NULL;

	criteria = MATCHCRITERIA_NOT_BODY_PART;
	expr = $3;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
}
| MATCHER_EXECUTE MATCHER_STRING
{
	gint criteria = 0;
	gchar * expr = NULL;

	criteria = MATCHCRITERIA_EXECUTE;
	expr = $2;
	prop = matcherprop_new(criteria, NULL, 0, expr, 0);
}
| MATCHER_NOT_EXECUTE MATCHER_STRING
{
	gint criteria = 0;
	gchar * expr = NULL;

	criteria = MATCHCRITERIA_NOT_EXECUTE;
	expr = $2;
	prop = matcherprop_new(criteria, NULL, 0, expr, 0);
}
;

filtering_action:
MATCHER_EXECUTE MATCHER_STRING
{
	gchar * cmd = NULL;
	gint action_type = 0;

	action_type = MATCHACTION_EXECUTE;
	cmd = $2;
	action = filteringaction_new(action_type, 0, cmd, 0);
}
| MATCHER_MOVE MATCHER_STRING
{
	gchar * destination = NULL;
	gint action_type = 0;

	action_type = MATCHACTION_MOVE;
	destination = $2;
	action = filteringaction_new(action_type, 0, destination, 0);
}
| MATCHER_COPY MATCHER_STRING
{
	gchar * destination = NULL;
	gint action_type = 0;

	action_type = MATCHACTION_COPY;
	destination = $2;
	action = filteringaction_new(action_type, 0, destination, 0);
}
| MATCHER_DELETE
{
	gint action_type = 0;

	action_type = MATCHACTION_DELETE;
	action = filteringaction_new(action_type, 0, NULL, 0);
}
| MATCHER_MARK
{
	gint action_type = 0;

	action_type = MATCHACTION_MARK;
	action = filteringaction_new(action_type, 0, NULL, 0);
}
| MATCHER_UNMARK
{
	gint action_type = 0;

	action_type = MATCHACTION_UNMARK;
	action = filteringaction_new(action_type, 0, NULL, 0);
}
| MATCHER_MARK_AS_READ
{
	gint action_type = 0;

	action_type = MATCHACTION_MARK_AS_READ;
	action = filteringaction_new(action_type, 0, NULL, 0);
}
| MATCHER_MARK_AS_UNREAD
{
	gint action_type = 0;

	action_type = MATCHACTION_MARK_AS_UNREAD;
	action = filteringaction_new(action_type, 0, NULL, 0);
}
| MATCHER_FORWARD MATCHER_INTEGER MATCHER_STRING
{
	gchar * destination = NULL;
	gint action_type = 0;
	gint account_id = 0;

	action_type = MATCHACTION_FORWARD;
	account_id = atoi($2);
	destination = $3;
	action = filteringaction_new(action_type, account_id, destination, 0);
}
| MATCHER_FORWARD_AS_ATTACHMENT MATCHER_INTEGER MATCHER_STRING
{
	gchar * destination = NULL;
	gint action_type = 0;
	gint account_id = 0;

	action_type = MATCHACTION_FORWARD_AS_ATTACHMENT;
	account_id = atoi($2);
	destination = $3;
	action = filteringaction_new(action_type, account_id, destination, 0);
}
| MATCHER_BOUNCE MATCHER_INTEGER MATCHER_STRING
{
	gchar * destination = NULL;
	gint action_type = 0;
	gint account_id = 0;

	action_type = MATCHACTION_BOUNCE;
	account_id = atoi($2);
	destination = $3;
	action = filteringaction_new(action_type, account_id, destination, 0);
}
| MATCHER_COLOR MATCHER_INTEGER
{
	gint action_type = 0;
	gint color = 0;

	action_type = MATCHACTION_COLOR;
	color = atoi($2);
	action = filteringaction_new(action_type, 0, NULL, color);
}
| MATCHER_DELETE_ON_SERVER
{
	gint action_type = 0;
	action_type = MATCHACTION_DELETE_ON_SERVER;
	action = filteringaction_new(action_type, 0, NULL, 0);
}
;

scoring_rule:
MATCHER_SCORE MATCHER_INTEGER
{
	score = atoi($2);
}
;
