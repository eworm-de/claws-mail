
%{

/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (c) 2001-2002 by Hiroyuki Yamamoto & The Sylpheed Claws Team.
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

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>

#include "utils.h"
#include "filtering.h"
#include "matcher.h"
#include "matcher_parser.h"
#include "matcher_parser_lex.h"
#include "procmsg.h"

#define MAX_COLORLABELS (MSG_CLABEL_7 - MSG_CLABEL_NONE)

static gint error = 0;
static gint bool_op = 0;
static gint match_type = 0;
static gchar *header = NULL;

static MatcherProp *prop;

static GSList *matchers_list = NULL;

static gchar *name = NULL;
static MatcherList *cond;
static GSList *action_list = NULL;
static FilteringAction *action = NULL;

static FilteringProp *filtering;

static GSList **prefs_filtering = NULL;
static int enable_compatibility = 0;

enum {
        MATCHER_PARSE_FILE,
        MATCHER_PARSE_NO_EOL,
	MATCHER_PARSE_NAME,
        MATCHER_PARSE_CONDITION,
        MATCHER_PARSE_FILTERING_ACTION,
};

static int matcher_parse_op = MATCHER_PARSE_FILE;


/* ******************************************************************** */
/* redeclarations to avoid warnings */
void matcher_parserrestart(FILE *input_file);
void matcher_parser_init(void);
void matcher_parser_switch_to_buffer(void * new_buffer);
void matcher_parser_delete_buffer(void * b);
int matcher_parserlex(void);

void matcher_parser_start_parsing(FILE *f)
{
	matcher_parserrestart(f);
	matcher_parserparse();
}

 
void * matcher_parser_scan_string(const char * str);
 
FilteringProp *matcher_parser_get_filtering(gchar *str)
{
	void *bufstate;
	void *tmp_str = NULL;
	
	/* little hack to allow passing rules with no names */
	if (!strncmp(str, "rulename ", 9))
		tmp_str = g_strdup(str);
	else 
		tmp_str = g_strconcat("rulename \"\" ", str, NULL);

	/* bad coding to enable the sub-grammar matching
	   in yacc */
	matcher_parserlineno = 1;
	matcher_parse_op = MATCHER_PARSE_NO_EOL;
	matcher_parserrestart(NULL);
        matcher_parser_init();
	bufstate = matcher_parser_scan_string((const char *) tmp_str);
        matcher_parser_switch_to_buffer(bufstate);
	if (matcher_parserparse() != 0)
		filtering = NULL;
	matcher_parse_op = MATCHER_PARSE_FILE;
	matcher_parser_delete_buffer(bufstate);
	g_free(tmp_str);
	return filtering;
}

static gboolean check_quote_symetry(gchar *str)
{
	const gchar *walk;
	int ret = 0;
	
	if (str == NULL)
		return TRUE; /* heh, that's symetric */
	if (*str == '\0')
		return TRUE;
	for (walk = str; *walk; walk++) {
		if (*walk == '\"') {
			if (walk == str 	/* first char */
			|| *(walk - 1) != '\\') /* not escaped */
				ret ++;
		}
	}
	return !(ret % 2);
}

MatcherList *matcher_parser_get_name(gchar *str)
{
	void *bufstate;

	if (!check_quote_symetry(str)) {
		cond = NULL;
		return cond;
	}
	
	/* bad coding to enable the sub-grammar matching
	   in yacc */
	matcher_parserlineno = 1;
	matcher_parse_op = MATCHER_PARSE_NAME;
	matcher_parserrestart(NULL);
        matcher_parser_init();
	bufstate = matcher_parser_scan_string(str);
	matcher_parserparse();
	matcher_parse_op = MATCHER_PARSE_FILE;
	matcher_parser_delete_buffer(bufstate);
	return cond;
}

MatcherList *matcher_parser_get_cond(gchar *str)
{
	void *bufstate;

	if (!check_quote_symetry(str)) {
		cond = NULL;
		return cond;
	}
	
	/* bad coding to enable the sub-grammar matching
	   in yacc */
	matcher_parserlineno = 1;
	matcher_parse_op = MATCHER_PARSE_CONDITION;
	matcher_parserrestart(NULL);
        matcher_parser_init();
	bufstate = matcher_parser_scan_string(str);
	matcher_parserparse();
	matcher_parse_op = MATCHER_PARSE_FILE;
	matcher_parser_delete_buffer(bufstate);
	return cond;
}

GSList *matcher_parser_get_action_list(gchar *str)
{
	void *bufstate;

	if (!check_quote_symetry(str)) {
		action_list = NULL;
		return action_list;
	}
	
	/* bad coding to enable the sub-grammar matching
	   in yacc */
	matcher_parserlineno = 1;
	matcher_parse_op = MATCHER_PARSE_FILTERING_ACTION;
	matcher_parserrestart(NULL);
        matcher_parser_init();
	bufstate = matcher_parser_scan_string(str);
	matcher_parserparse();
	matcher_parse_op = MATCHER_PARSE_FILE;
	matcher_parser_delete_buffer(bufstate);
	return action_list;
}

MatcherProp *matcher_parser_get_prop(gchar *str)
{
	MatcherList *list;
	MatcherProp *prop;

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

void matcher_parsererror(char *str)
{
	GSList *l;

	if (matchers_list) {
		for (l = matchers_list; l != NULL; l = g_slist_next(l)) {
			matcherprop_free((MatcherProp *)
					 l->data);
			l->data = NULL;
		}
		g_slist_free(matchers_list);
		matchers_list = NULL;
	}
	cond = NULL;
	g_warning("filtering parsing: %i: %s\n",
		  matcher_parserlineno, str);
	error = 1;
}

int matcher_parserwrap(void)
{
	return 1;
}
%}

%union {
	char *str;
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
%token MATCHER_TEST  MATCHER_NOT_TEST  MATCHER_MATCHCASE  MATCHER_MATCH
%token MATCHER_REGEXPCASE  MATCHER_REGEXP  MATCHER_SCORE  MATCHER_MOVE
%token MATCHER_ANY_IN_ADDRESSBOOK MATCHER_ALL_IN_ADDRESSBOOK
%token MATCHER_COPY  MATCHER_DELETE  MATCHER_MARK  MATCHER_UNMARK
%token MATCHER_LOCK MATCHER_UNLOCK
%token MATCHER_EXECUTE
%token MATCHER_MARK_AS_READ  MATCHER_MARK_AS_UNREAD  MATCHER_FORWARD
%token MATCHER_FORWARD_AS_ATTACHMENT  MATCHER_EOL  MATCHER_STRING  
%token MATCHER_OR MATCHER_AND  
%token MATCHER_COLOR MATCHER_SCORE_EQUAL MATCHER_REDIRECT 
%token MATCHER_SIZE_GREATER MATCHER_SIZE_SMALLER MATCHER_SIZE_EQUAL
%token MATCHER_LOCKED MATCHER_NOT_LOCKED
%token MATCHER_PARTIAL MATCHER_NOT_PARTIAL
%token MATCHER_COLORLABEL MATCHER_NOT_COLORLABEL
%token MATCHER_IGNORE_THREAD MATCHER_NOT_IGNORE_THREAD
%token MATCHER_CHANGE_SCORE MATCHER_SET_SCORE
%token MATCHER_STOP MATCHER_HIDE

%start file

%token MATCHER_RULENAME
%token <str> MATCHER_STRING
%token <str> MATCHER_SECTION
%token <str> MATCHER_INTEGER

%%

file:
{
	if (matcher_parse_op == MATCHER_PARSE_FILE) {
		prefs_filtering = &pre_global_processing;
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
| 
{ action_list = NULL; }
instruction
| error MATCHER_EOL
{
	yyerrok;
};

section_notification:
MATCHER_SECTION MATCHER_EOL
{
	gchar *folder = $1;
	FolderItem *item = NULL;

	if (matcher_parse_op == MATCHER_PARSE_FILE) {
                enable_compatibility = 0;
		if (!strcmp(folder, "global")) {
                        /* backward compatibility */
                        enable_compatibility = 1;
                }
		else if (!strcmp(folder, "preglobal")) {
			prefs_filtering = &pre_global_processing;
                }
		else if (!strcmp(folder, "postglobal")) {
			prefs_filtering = &post_global_processing;
                }
		else if (!strcmp(folder, "filtering")) {
                        prefs_filtering = &filtering_rules;
		}
                else {
			item = folder_find_item_from_identifier(folder);
			if (item != NULL) {
				prefs_filtering = &item->prefs->processing;
			} else {
				prefs_filtering = NULL;
			}
		}
	}
}
;

instruction:
name condition filtering MATCHER_EOL
| name condition filtering
{
	if (matcher_parse_op == MATCHER_PARSE_NO_EOL)
		YYACCEPT;
	else {
		matcher_parsererror("parse error a");
		YYERROR;
	}
}
| name
{
	if (matcher_parse_op == MATCHER_PARSE_NAME)
		YYACCEPT;
	else {
		matcher_parsererror("parse error b");
		YYERROR;
	}
}
| condition
{
	if (matcher_parse_op == MATCHER_PARSE_CONDITION)
		YYACCEPT;
	else {
		matcher_parsererror("parse error c");
		YYERROR;
	}
}
| filtering_action_list
{
	if (matcher_parse_op == MATCHER_PARSE_FILTERING_ACTION)
		YYACCEPT;
	else {
		matcher_parsererror("parse error d");
		YYERROR;
	}
}
| MATCHER_EOL
;

name:
MATCHER_RULENAME MATCHER_STRING
{
	name = g_strdup($2);
}
;

filtering:
filtering_action_list
{
	filtering = filteringprop_new(name, cond, action_list);
        g_free(name);
	name = NULL;
        if (enable_compatibility) {
                prefs_filtering = &filtering_rules;
                if (action_list != NULL) {
                        FilteringAction * first_action;
                        
                        first_action = action_list->data;
                        
                        if (first_action->type == MATCHACTION_CHANGE_SCORE)
                                prefs_filtering = &pre_global_processing;
                }
        }
        
	cond = NULL;
	action_list = NULL;
        
	if ((matcher_parse_op == MATCHER_PARSE_FILE) &&
            (prefs_filtering != NULL)) {
		*prefs_filtering = g_slist_append(*prefs_filtering,
						  filtering);
		filtering = NULL;
	}
}
;

filtering_action_list:
filtering_action_b filtering_action_list
| filtering_action_b
;

filtering_action_b:
filtering_action
{
        action_list = g_slist_append(action_list, action);
        action = NULL;
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
| MATCHER_ANY_IN_ADDRESSBOOK
{
	match_type = MATCHTYPE_ANY_IN_ADDRESSBOOK;
}
| MATCHER_ALL_IN_ADDRESSBOOK
{
	match_type = MATCHTYPE_ALL_IN_ADDRESSBOOK;
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
| MATCHER_LOCKED
{
	gint criteria = 0;

	criteria = MATCHCRITERIA_LOCKED;
	prop = matcherprop_new(criteria, NULL, 0, NULL, 0);
}
| MATCHER_NOT_LOCKED
{
	gint criteria = 0;

	criteria = MATCHCRITERIA_NOT_LOCKED;
	prop = matcherprop_new(criteria, NULL, 0, NULL, 0);
}
| MATCHER_PARTIAL
{
	gint criteria = 0;

	criteria = MATCHCRITERIA_PARTIAL;
	prop = matcherprop_new(criteria, NULL, 0, NULL, 0);
}
| MATCHER_NOT_PARTIAL
{
	gint criteria = 0;

	criteria = MATCHCRITERIA_NOT_PARTIAL;
	prop = matcherprop_new(criteria, NULL, 0, NULL, 0);
}
| MATCHER_COLORLABEL MATCHER_INTEGER
{
	gint criteria = 0;
	gint value = 0;

	criteria = MATCHCRITERIA_COLORLABEL;
	value = strtol($2, NULL, 10);
	if (value < 0) value = 0;
	else if (value > MAX_COLORLABELS) value = MAX_COLORLABELS;
	prop = matcherprop_new(criteria, NULL, 0, NULL, value);
}
| MATCHER_NOT_COLORLABEL MATCHER_INTEGER
{
	gint criteria = 0;
	gint value = 0;

	criteria = MATCHCRITERIA_NOT_COLORLABEL;
	value = strtol($2, NULL, 0);
	if (value < 0) value = 0;
	else if (value > MAX_COLORLABELS) value = MAX_COLORLABELS;
	prop = matcherprop_new(criteria, NULL, 0, NULL, value);
}
| MATCHER_IGNORE_THREAD
{
	gint criteria = 0;

	criteria = MATCHCRITERIA_IGNORE_THREAD;
	prop = matcherprop_new(criteria, NULL, 0, NULL, 0);
}
| MATCHER_NOT_IGNORE_THREAD
{
	gint criteria = 0;

	criteria = MATCHCRITERIA_NOT_IGNORE_THREAD;
	prop = matcherprop_new(criteria, NULL, 0, NULL, 0);
}
| MATCHER_SUBJECT match_type MATCHER_STRING
{
	gint criteria = 0;
	gchar *expr = NULL;

	criteria = MATCHCRITERIA_SUBJECT;
	expr = $3;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
}
| MATCHER_NOT_SUBJECT match_type MATCHER_STRING
{
	gint criteria = 0;
	gchar *expr = NULL;

	criteria = MATCHCRITERIA_NOT_SUBJECT;
	expr = $3;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
}
| MATCHER_FROM match_type MATCHER_STRING
{
	gint criteria = 0;
	gchar *expr = NULL;

	criteria = MATCHCRITERIA_FROM;
	expr = $3;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
}
| MATCHER_NOT_FROM match_type MATCHER_STRING
{
	gint criteria = 0;
	gchar *expr = NULL;

	criteria = MATCHCRITERIA_NOT_FROM;
	expr = $3;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
}
| MATCHER_TO match_type MATCHER_STRING
{
	gint criteria = 0;
	gchar *expr = NULL;

	criteria = MATCHCRITERIA_TO;
	expr = $3;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
}
| MATCHER_NOT_TO match_type MATCHER_STRING
{
	gint criteria = 0;
	gchar *expr = NULL;

	criteria = MATCHCRITERIA_NOT_TO;
	expr = $3;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
}
| MATCHER_CC match_type MATCHER_STRING
{
	gint criteria = 0;
	gchar *expr = NULL;

	criteria = MATCHCRITERIA_CC;
	expr = $3;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
}
| MATCHER_NOT_CC match_type MATCHER_STRING
{
	gint criteria = 0;
	gchar *expr = NULL;

	criteria = MATCHCRITERIA_NOT_CC;
	expr = $3;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
}
| MATCHER_TO_OR_CC match_type MATCHER_STRING
{
	gint criteria = 0;
	gchar *expr = NULL;

	criteria = MATCHCRITERIA_TO_OR_CC;
	expr = $3;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
}
| MATCHER_NOT_TO_AND_NOT_CC match_type MATCHER_STRING
{
	gint criteria = 0;
	gchar *expr = NULL;

	criteria = MATCHCRITERIA_NOT_TO_AND_NOT_CC;
	expr = $3;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
}
| MATCHER_AGE_GREATER MATCHER_INTEGER
{
	gint criteria = 0;
	gint value = 0;

	criteria = MATCHCRITERIA_AGE_GREATER;
	value = strtol($2, NULL, 0);
	prop = matcherprop_new(criteria, NULL, 0, NULL, value);
}
| MATCHER_AGE_LOWER MATCHER_INTEGER
{
	gint criteria = 0;
	gint value = 0;

	criteria = MATCHCRITERIA_AGE_LOWER;
	value = strtol($2, NULL, 0);
	prop = matcherprop_new(criteria, NULL, 0, NULL, value);
}
| MATCHER_NEWSGROUPS match_type MATCHER_STRING
{
	gint criteria = 0;
	gchar *expr = NULL;

	criteria = MATCHCRITERIA_NEWSGROUPS;
	expr = $3;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
}
| MATCHER_NOT_NEWSGROUPS match_type MATCHER_STRING
{
	gint criteria = 0;
	gchar *expr = NULL;

	criteria = MATCHCRITERIA_NOT_NEWSGROUPS;
	expr = $3;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
}
| MATCHER_INREPLYTO match_type MATCHER_STRING
{
	gint criteria = 0;
	gchar *expr = NULL;

	criteria = MATCHCRITERIA_INREPLYTO;
	expr = $3;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
}
| MATCHER_NOT_INREPLYTO match_type MATCHER_STRING
{
	gint criteria = 0;
	gchar *expr = NULL;

	criteria = MATCHCRITERIA_NOT_INREPLYTO;
	expr = $3;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
}
| MATCHER_REFERENCES match_type MATCHER_STRING
{
	gint criteria = 0;
	gchar *expr = NULL;

	criteria = MATCHCRITERIA_REFERENCES;
	expr = $3;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
}
| MATCHER_NOT_REFERENCES match_type MATCHER_STRING
{
	gint criteria = 0;
	gchar *expr = NULL;

	criteria = MATCHCRITERIA_NOT_REFERENCES;
	expr = $3;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
}
| MATCHER_SCORE_GREATER MATCHER_INTEGER
{
	gint criteria = 0;
	gint value = 0;

	criteria = MATCHCRITERIA_SCORE_GREATER;
	value = strtol($2, NULL, 0);
	prop = matcherprop_new(criteria, NULL, 0, NULL, value);
}
| MATCHER_SCORE_LOWER MATCHER_INTEGER
{
	gint criteria = 0;
	gint value = 0;

	criteria = MATCHCRITERIA_SCORE_LOWER;
	value = strtol($2, NULL, 0);
	prop = matcherprop_new(criteria, NULL, 0, NULL, value);
}
| MATCHER_SCORE_EQUAL MATCHER_INTEGER
{
	gint criteria = 0;
	gint value = 0;

	criteria = MATCHCRITERIA_SCORE_EQUAL;
	value = strtol($2, NULL, 0);
	prop = matcherprop_new(criteria, NULL, 0, NULL, value);
}
| MATCHER_SIZE_GREATER MATCHER_INTEGER 
{
	gint criteria = 0;
	gint value    = 0;
	criteria = MATCHCRITERIA_SIZE_GREATER;
	value = strtol($2, NULL, 0);
	prop = matcherprop_new(criteria, NULL, 0, NULL, value);
}
| MATCHER_SIZE_SMALLER MATCHER_INTEGER
{
	gint criteria = 0;
	gint value    = 0;
	criteria = MATCHCRITERIA_SIZE_SMALLER;
	value = strtol($2, NULL, 0);
	prop = matcherprop_new(criteria, NULL, 0, NULL, value);
}
| MATCHER_SIZE_EQUAL MATCHER_INTEGER
{
	gint criteria = 0;
	gint value    = 0;
	criteria = MATCHCRITERIA_SIZE_EQUAL;
	value = strtol($2, NULL, 0);
	prop = matcherprop_new(criteria, NULL, 0, NULL, value);
}
| MATCHER_HEADER MATCHER_STRING
{
	header = g_strdup($2);
} match_type MATCHER_STRING
{
	gint criteria = 0;
	gchar *expr = NULL;

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
	gchar *expr = NULL;

	criteria = MATCHCRITERIA_NOT_HEADER;
	expr = $2;
	prop = matcherprop_new(criteria, header, match_type, expr, 0);
	g_free(header);
}
| MATCHER_HEADERS_PART match_type MATCHER_STRING
{
	gint criteria = 0;
	gchar *expr = NULL;

	criteria = MATCHCRITERIA_HEADERS_PART;
	expr = $3;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
}
| MATCHER_NOT_HEADERS_PART match_type MATCHER_STRING
{
	gint criteria = 0;
	gchar *expr = NULL;

	criteria = MATCHCRITERIA_NOT_HEADERS_PART;
	expr = $3;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
}
| MATCHER_MESSAGE match_type MATCHER_STRING
{
	gint criteria = 0;
	gchar *expr = NULL;

	criteria = MATCHCRITERIA_MESSAGE;
	expr = $3;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
}
| MATCHER_NOT_MESSAGE match_type MATCHER_STRING
{
	gint criteria = 0;
	gchar *expr = NULL;

	criteria = MATCHCRITERIA_NOT_MESSAGE;
	expr = $3;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
}
| MATCHER_BODY_PART match_type MATCHER_STRING
{
	gint criteria = 0;
	gchar *expr = NULL;

	criteria = MATCHCRITERIA_BODY_PART;
	expr = $3;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
}
| MATCHER_NOT_BODY_PART match_type MATCHER_STRING
{
	gint criteria = 0;
	gchar *expr = NULL;

	criteria = MATCHCRITERIA_NOT_BODY_PART;
	expr = $3;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
}
| MATCHER_TEST MATCHER_STRING
{
	gint criteria = 0;
	gchar *expr = NULL;

	criteria = MATCHCRITERIA_TEST;
	expr = $2;
	prop = matcherprop_new(criteria, NULL, 0, expr, 0);
}
| MATCHER_NOT_TEST MATCHER_STRING
{
	gint criteria = 0;
	gchar *expr = NULL;

	criteria = MATCHCRITERIA_NOT_TEST;
	expr = $2;
	prop = matcherprop_new(criteria, NULL, 0, expr, 0);
}
;

filtering_action:
MATCHER_EXECUTE MATCHER_STRING
{
	gchar *cmd = NULL;
	gint action_type = 0;

	action_type = MATCHACTION_EXECUTE;
	cmd = $2;
	action = filteringaction_new(action_type, 0, cmd, 0, 0);
}
| MATCHER_MOVE MATCHER_STRING
{
	gchar *destination = NULL;
	gint action_type = 0;

	action_type = MATCHACTION_MOVE;
	destination = $2;
	action = filteringaction_new(action_type, 0, destination, 0, 0);
}
| MATCHER_COPY MATCHER_STRING
{
	gchar *destination = NULL;
	gint action_type = 0;

	action_type = MATCHACTION_COPY;
	destination = $2;
	action = filteringaction_new(action_type, 0, destination, 0, 0);
}
| MATCHER_DELETE
{
	gint action_type = 0;

	action_type = MATCHACTION_DELETE;
	action = filteringaction_new(action_type, 0, NULL, 0, 0);
}
| MATCHER_MARK
{
	gint action_type = 0;

	action_type = MATCHACTION_MARK;
	action = filteringaction_new(action_type, 0, NULL, 0, 0);
}
| MATCHER_UNMARK
{
	gint action_type = 0;

	action_type = MATCHACTION_UNMARK;
	action = filteringaction_new(action_type, 0, NULL, 0, 0);
}
| MATCHER_LOCK
{
	gint action_type = 0;

	action_type = MATCHACTION_LOCK;
	action = filteringaction_new(action_type, 0, NULL, 0, 0);
}
| MATCHER_UNLOCK
{
	gint action_type = 0;

	action_type = MATCHACTION_UNLOCK;
	action = filteringaction_new(action_type, 0, NULL, 0, 0);
}
| MATCHER_MARK_AS_READ
{
	gint action_type = 0;

	action_type = MATCHACTION_MARK_AS_READ;
	action = filteringaction_new(action_type, 0, NULL, 0, 0);
}
| MATCHER_MARK_AS_UNREAD
{
	gint action_type = 0;

	action_type = MATCHACTION_MARK_AS_UNREAD;
	action = filteringaction_new(action_type, 0, NULL, 0, 0);
}
| MATCHER_FORWARD MATCHER_INTEGER MATCHER_STRING
{
	gchar *destination = NULL;
	gint action_type = 0;
	gint account_id = 0;

	action_type = MATCHACTION_FORWARD;
	account_id = strtol($2, NULL, 10);
	destination = $3;
	action = filteringaction_new(action_type,
            account_id, destination, 0, 0);
}
| MATCHER_FORWARD_AS_ATTACHMENT MATCHER_INTEGER MATCHER_STRING
{
	gchar *destination = NULL;
	gint action_type = 0;
	gint account_id = 0;

	action_type = MATCHACTION_FORWARD_AS_ATTACHMENT;
	account_id = strtol($2, NULL, 10);
	destination = $3;
	action = filteringaction_new(action_type,
            account_id, destination, 0, 0);
}
| MATCHER_REDIRECT MATCHER_INTEGER MATCHER_STRING
{
	gchar *destination = NULL;
	gint action_type = 0;
	gint account_id = 0;

	action_type = MATCHACTION_REDIRECT;
	account_id = strtol($2, NULL, 10);
	destination = $3;
	action = filteringaction_new(action_type,
            account_id, destination, 0, 0);
}
| MATCHER_COLOR MATCHER_INTEGER
{
	gint action_type = 0;
	gint color = 0;

	action_type = MATCHACTION_COLOR;
	color = strtol($2, NULL, 10);
	action = filteringaction_new(action_type, 0, NULL, color, 0);
}
| MATCHER_CHANGE_SCORE MATCHER_INTEGER
{
        gint score = 0;
        
        score = strtol($2, NULL, 10);
	action = filteringaction_new(MATCHACTION_CHANGE_SCORE, 0,
				     NULL, 0, score);
}
/* backward compatibility */
| MATCHER_SCORE MATCHER_INTEGER
{
        gint score = 0;
        
        score = strtol($2, NULL, 10);
	action = filteringaction_new(MATCHACTION_CHANGE_SCORE, 0,
				     NULL, 0, score);
}
| MATCHER_SET_SCORE MATCHER_INTEGER
{
        gint score = 0;
        
        score = strtol($2, NULL, 10);
	action = filteringaction_new(MATCHACTION_SET_SCORE, 0,
				     NULL, 0, score);
}
| MATCHER_HIDE
{
	action = filteringaction_new(MATCHACTION_HIDE, 0, NULL, 0, 0);
}
| MATCHER_STOP
{
	action = filteringaction_new(MATCHACTION_STOP, 0, NULL, 0, 0);
}
;
