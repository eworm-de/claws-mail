
/*  A Bison parser, made from matcher_parser_parse.y
    by GNU Bison version 1.28  */

#define YYBISON 1  /* Identify Bison output.  */

#define	MATCHER_ALL	257
#define	MATCHER_UNREAD	258
#define	MATCHER_NOT_UNREAD	259
#define	MATCHER_NEW	260
#define	MATCHER_NOT_NEW	261
#define	MATCHER_MARKED	262
#define	MATCHER_NOT_MARKED	263
#define	MATCHER_DELETED	264
#define	MATCHER_NOT_DELETED	265
#define	MATCHER_REPLIED	266
#define	MATCHER_NOT_REPLIED	267
#define	MATCHER_FORWARDED	268
#define	MATCHER_NOT_FORWARDED	269
#define	MATCHER_SUBJECT	270
#define	MATCHER_NOT_SUBJECT	271
#define	MATCHER_FROM	272
#define	MATCHER_NOT_FROM	273
#define	MATCHER_TO	274
#define	MATCHER_NOT_TO	275
#define	MATCHER_CC	276
#define	MATCHER_NOT_CC	277
#define	MATCHER_TO_OR_CC	278
#define	MATCHER_NOT_TO_AND_NOT_CC	279
#define	MATCHER_AGE_GREATER	280
#define	MATCHER_AGE_LOWER	281
#define	MATCHER_NEWSGROUPS	282
#define	MATCHER_NOT_NEWSGROUPS	283
#define	MATCHER_INREPLYTO	284
#define	MATCHER_NOT_INREPLYTO	285
#define	MATCHER_REFERENCES	286
#define	MATCHER_NOT_REFERENCES	287
#define	MATCHER_SCORE_GREATER	288
#define	MATCHER_SCORE_LOWER	289
#define	MATCHER_HEADER	290
#define	MATCHER_NOT_HEADER	291
#define	MATCHER_HEADERS_PART	292
#define	MATCHER_NOT_HEADERS_PART	293
#define	MATCHER_MESSAGE	294
#define	MATCHER_NOT_MESSAGE	295
#define	MATCHER_BODY_PART	296
#define	MATCHER_NOT_BODY_PART	297
#define	MATCHER_EXECUTE	298
#define	MATCHER_NOT_EXECUTE	299
#define	MATCHER_MATCHCASE	300
#define	MATCHER_MATCH	301
#define	MATCHER_REGEXPCASE	302
#define	MATCHER_REGEXP	303
#define	MATCHER_SCORE	304
#define	MATCHER_MOVE	305
#define	MATCHER_COPY	306
#define	MATCHER_DELETE	307
#define	MATCHER_MARK	308
#define	MATCHER_UNMARK	309
#define	MATCHER_MARK_AS_READ	310
#define	MATCHER_MARK_AS_UNREAD	311
#define	MATCHER_FORWARD	312
#define	MATCHER_FORWARD_AS_ATTACHMENT	313
#define	MATCHER_EOL	314
#define	MATCHER_STRING	315
#define	MATCHER_OR	316
#define	MATCHER_AND	317
#define	MATCHER_COLOR	318
#define	MATCHER_SCORE_EQUAL	319
#define	MATCHER_BOUNCE	320
#define	MATCHER_DELETE_ON_SERVER	321
#define	MATCHER_SECTION	322
#define	MATCHER_INTEGER	323

#line 1 "matcher_parser_parse.y"

#include "filtering.h"
#include "scoring.h"
#include "matcher.h"
#include "matcher_parser.h"
#include "matcher_parser_lex.h"
#include "intl.h"
#include <glib.h>
#include "defs.h"
#include "utils.h"

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

#line 139 "matcher_parser_parse.y"
typedef union {
	char * str;
	int value;
} YYSTYPE;
#include <stdio.h>

#ifndef __cplusplus
#ifndef __STDC__
#define const
#endif
#endif



#define	YYFINAL		159
#define	YYFLAG		-32768
#define	YYNTBASE	70

#define YYTRANSLATE(x) ((unsigned)(x) <= 323 ? yytranslate[x] : 88)

static const char yytranslate[] = {     0,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
     2,     2,     2,     2,     2,     1,     3,     4,     5,     6,
     7,     8,     9,    10,    11,    12,    13,    14,    15,    16,
    17,    18,    19,    20,    21,    22,    23,    24,    25,    26,
    27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
    37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
    47,    48,    49,    50,    51,    52,    53,    54,    55,    56,
    57,    58,    59,    60,    61,    62,    63,    64,    65,    66,
    67,    68,    69
};

#if YYDEBUG != 0
static const short yyprhs[] = {     0,
     0,     1,     4,     7,     9,    11,    13,    16,    19,    22,
    24,    27,    28,    30,    31,    33,    35,    37,    39,    41,
    43,    45,    49,    51,    53,    55,    57,    59,    61,    63,
    65,    67,    69,    71,    73,    75,    77,    79,    81,    85,
    89,    93,    97,   101,   105,   109,   113,   117,   121,   124,
   127,   131,   135,   139,   143,   147,   151,   154,   157,   160,
   161,   167,   168,   174,   178,   182,   186,   190,   194,   198,
   201,   204,   207,   210,   213,   215,   217,   219,   221,   223,
   227,   231,   235,   238,   240
};

static const short yyrhs[] = {    -1,
    71,    72,     0,    73,    72,     0,    73,     0,    74,     0,
    75,     0,     1,    60,     0,    68,    60,     0,    80,    76,
     0,    60,     0,    78,    77,     0,     0,    60,     0,     0,
    86,     0,    87,     0,    46,     0,    47,     0,    48,     0,
    49,     0,    81,     0,    81,    82,    83,     0,    83,     0,
    63,     0,    62,     0,     3,     0,     4,     0,     5,     0,
     6,     0,     7,     0,     8,     0,     9,     0,    10,     0,
    11,     0,    12,     0,    13,     0,    14,     0,    15,     0,
    16,    79,    61,     0,    17,    79,    61,     0,    18,    79,
    61,     0,    19,    79,    61,     0,    20,    79,    61,     0,
    21,    79,    61,     0,    22,    79,    61,     0,    23,    79,
    61,     0,    24,    79,    61,     0,    25,    79,    61,     0,
    26,    69,     0,    27,    69,     0,    28,    79,    61,     0,
    29,    79,    61,     0,    30,    79,    61,     0,    31,    79,
    61,     0,    32,    79,    61,     0,    33,    79,    61,     0,
    34,    69,     0,    35,    69,     0,    65,    69,     0,     0,
    36,    61,    84,    79,    61,     0,     0,    37,    61,    85,
    79,    61,     0,    38,    79,    61,     0,    39,    79,    61,
     0,    40,    79,    61,     0,    41,    79,    61,     0,    42,
    79,    61,     0,    43,    79,    61,     0,    44,    61,     0,
    45,    61,     0,    44,    61,     0,    51,    61,     0,    52,
    61,     0,    53,     0,    54,     0,    55,     0,    56,     0,
    57,     0,    58,    69,    61,     0,    59,    69,    61,     0,
    66,    69,    61,     0,    64,    69,     0,    67,     0,    50,
    69,     0
};

#endif

#if YYDEBUG != 0
static const short yyrline[] = { 0,
   173,   180,   182,   185,   188,   190,   191,   196,   216,   218,
   221,   223,   234,   236,   247,   259,   271,   276,   280,   284,
   290,   298,   303,   310,   315,   321,   329,   336,   343,   350,
   357,   364,   371,   378,   385,   392,   399,   406,   413,   422,
   431,   440,   449,   458,   467,   476,   485,   494,   503,   512,
   521,   530,   539,   548,   557,   566,   575,   584,   593,   602,
   605,   615,   618,   628,   637,   646,   655,   664,   673,   682,
   691,   702,   712,   721,   730,   737,   744,   751,   758,   765,
   776,   787,   798,   807,   815
};
#endif


#if YYDEBUG != 0 || defined (YYERROR_VERBOSE)

static const char * const yytname[] = {   "$","error","$undefined.","MATCHER_ALL",
"MATCHER_UNREAD","MATCHER_NOT_UNREAD","MATCHER_NEW","MATCHER_NOT_NEW","MATCHER_MARKED",
"MATCHER_NOT_MARKED","MATCHER_DELETED","MATCHER_NOT_DELETED","MATCHER_REPLIED",
"MATCHER_NOT_REPLIED","MATCHER_FORWARDED","MATCHER_NOT_FORWARDED","MATCHER_SUBJECT",
"MATCHER_NOT_SUBJECT","MATCHER_FROM","MATCHER_NOT_FROM","MATCHER_TO","MATCHER_NOT_TO",
"MATCHER_CC","MATCHER_NOT_CC","MATCHER_TO_OR_CC","MATCHER_NOT_TO_AND_NOT_CC",
"MATCHER_AGE_GREATER","MATCHER_AGE_LOWER","MATCHER_NEWSGROUPS","MATCHER_NOT_NEWSGROUPS",
"MATCHER_INREPLYTO","MATCHER_NOT_INREPLYTO","MATCHER_REFERENCES","MATCHER_NOT_REFERENCES",
"MATCHER_SCORE_GREATER","MATCHER_SCORE_LOWER","MATCHER_HEADER","MATCHER_NOT_HEADER",
"MATCHER_HEADERS_PART","MATCHER_NOT_HEADERS_PART","MATCHER_MESSAGE","MATCHER_NOT_MESSAGE",
"MATCHER_BODY_PART","MATCHER_NOT_BODY_PART","MATCHER_EXECUTE","MATCHER_NOT_EXECUTE",
"MATCHER_MATCHCASE","MATCHER_MATCH","MATCHER_REGEXPCASE","MATCHER_REGEXP","MATCHER_SCORE",
"MATCHER_MOVE","MATCHER_COPY","MATCHER_DELETE","MATCHER_MARK","MATCHER_UNMARK",
"MATCHER_MARK_AS_READ","MATCHER_MARK_AS_UNREAD","MATCHER_FORWARD","MATCHER_FORWARD_AS_ATTACHMENT",
"MATCHER_EOL","MATCHER_STRING","MATCHER_OR","MATCHER_AND","MATCHER_COLOR","MATCHER_SCORE_EQUAL",
"MATCHER_BOUNCE","MATCHER_DELETE_ON_SERVER","MATCHER_SECTION","MATCHER_INTEGER",
"file","@1","file_line_list","file_line","section_notification","instruction",
"end_instr_opt","end_action","filtering_or_scoring","match_type","condition",
"condition_list","bool_op","one_condition","@2","@3","filtering_action","scoring_rule", NULL
};
#endif

static const short yyr1[] = {     0,
    71,    70,    72,    72,    73,    73,    73,    74,    75,    75,
    76,    76,    77,    77,    78,    78,    79,    79,    79,    79,
    80,    81,    81,    82,    82,    83,    83,    83,    83,    83,
    83,    83,    83,    83,    83,    83,    83,    83,    83,    83,
    83,    83,    83,    83,    83,    83,    83,    83,    83,    83,
    83,    83,    83,    83,    83,    83,    83,    83,    83,    84,
    83,    85,    83,    83,    83,    83,    83,    83,    83,    83,
    83,    86,    86,    86,    86,    86,    86,    86,    86,    86,
    86,    86,    86,    86,    87
};

static const short yyr2[] = {     0,
     0,     2,     2,     1,     1,     1,     2,     2,     2,     1,
     2,     0,     1,     0,     1,     1,     1,     1,     1,     1,
     1,     3,     1,     1,     1,     1,     1,     1,     1,     1,
     1,     1,     1,     1,     1,     1,     1,     1,     3,     3,
     3,     3,     3,     3,     3,     3,     3,     3,     2,     2,
     3,     3,     3,     3,     3,     3,     2,     2,     2,     0,
     5,     0,     5,     3,     3,     3,     3,     3,     3,     2,
     2,     2,     2,     2,     1,     1,     1,     1,     1,     3,
     3,     3,     2,     1,     2
};

static const short yydefact[] = {     1,
     0,     0,    26,    27,    28,    29,    30,    31,    32,    33,
    34,    35,    36,    37,    38,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,    10,     0,     0,     2,     0,
     5,     6,    12,    21,    23,     7,    17,    18,    19,    20,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    49,    50,     0,     0,     0,     0,     0,     0,    57,    58,
    60,    62,     0,     0,     0,     0,     0,     0,    70,    71,
    59,     8,     3,     0,     0,     0,     0,    75,    76,    77,
    78,    79,     0,     0,     0,     0,    84,     9,    14,    15,
    16,    25,    24,     0,    39,    40,    41,    42,    43,    44,
    45,    46,    47,    48,    51,    52,    53,    54,    55,    56,
     0,     0,    64,    65,    66,    67,    68,    69,    72,    85,
    73,    74,     0,     0,    83,     0,    13,    11,    22,     0,
     0,    80,    81,    82,    61,    63,     0,     0,     0
};

static const short yydefgoto[] = {   157,
     1,    49,    50,    51,    52,   108,   148,   109,    61,    53,
    54,   114,    55,   131,   132,   110,   111
};

static const short yypact[] = {-32768,
   115,   -31,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,   -29,   -29,   -29,   -29,   -29,
   -29,   -29,   -29,   -29,   -29,     4,     5,   -29,   -29,   -29,
   -29,   -29,   -29,     6,     7,    16,    17,   -29,   -29,   -29,
   -29,   -29,   -29,    18,    19,-32768,    12,    22,-32768,    27,
-32768,-32768,    46,   -53,-32768,-32768,-32768,-32768,-32768,-32768,
    23,    24,    25,    28,    30,    32,    33,    45,    47,    48,
-32768,-32768,    50,    56,   100,   101,   102,   103,-32768,-32768,
-32768,-32768,   104,   105,   106,   107,   108,   109,-32768,-32768,
-32768,-32768,-32768,   110,    14,   111,   112,-32768,-32768,-32768,
-32768,-32768,    38,   113,   158,   159,-32768,-32768,   114,-32768,
-32768,-32768,-32768,   181,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
   -29,   -29,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,   116,   117,-32768,   118,-32768,-32768,-32768,   120,
   168,-32768,-32768,-32768,-32768,-32768,    88,   176,-32768
};

static const short yypgoto[] = {-32768,
-32768,   180,-32768,-32768,-32768,-32768,-32768,-32768,   -17,-32768,
-32768,-32768,   119,-32768,-32768,-32768,-32768
};


#define	YYLAST		246


static const short yytable[] = {    62,
    63,    64,    65,    66,    67,    68,    69,    70,   112,   113,
    73,    74,    75,    76,    77,    78,    57,    58,    59,    60,
    83,    84,    85,    86,    87,    88,    -4,     2,    56,     3,
     4,     5,     6,     7,     8,     9,    10,    11,    12,    13,
    14,    15,    16,    17,    18,    19,    20,    21,    22,    23,
    24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
    34,    35,    36,    37,    38,    39,    40,    41,    42,    43,
    44,    45,    71,    72,    79,    80,    81,    82,    89,    90,
    91,    92,   140,   115,   116,   117,    46,   158,   118,    94,
   119,    47,   120,   121,    48,    95,    96,    97,    98,    99,
   100,   101,   102,   103,   104,   122,   143,   123,   124,   105,
   125,   106,   107,   150,   151,     2,   126,     3,     4,     5,
     6,     7,     8,     9,    10,    11,    12,    13,    14,    15,
    16,    17,    18,    19,    20,    21,    22,    23,    24,    25,
    26,    27,    28,    29,    30,    31,    32,    33,    34,    35,
    36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
   127,   128,   129,   130,   133,   134,   135,   136,   137,   138,
   139,   141,   142,   147,    46,   159,   152,   153,   154,    47,
   155,   144,    48,     3,     4,     5,     6,     7,     8,     9,
    10,    11,    12,    13,    14,    15,    16,    17,    18,    19,
    20,    21,    22,    23,    24,    25,    26,    27,    28,    29,
    30,    31,    32,    33,    34,    35,    36,    37,    38,    39,
    40,    41,    42,    43,    44,    45,   145,   146,   156,    93,
     0,     0,   149,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,    47
};

static const short yycheck[] = {    17,
    18,    19,    20,    21,    22,    23,    24,    25,    62,    63,
    28,    29,    30,    31,    32,    33,    46,    47,    48,    49,
    38,    39,    40,    41,    42,    43,     0,     1,    60,     3,
     4,     5,     6,     7,     8,     9,    10,    11,    12,    13,
    14,    15,    16,    17,    18,    19,    20,    21,    22,    23,
    24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
    34,    35,    36,    37,    38,    39,    40,    41,    42,    43,
    44,    45,    69,    69,    69,    69,    61,    61,    61,    61,
    69,    60,    69,    61,    61,    61,    60,     0,    61,    44,
    61,    65,    61,    61,    68,    50,    51,    52,    53,    54,
    55,    56,    57,    58,    59,    61,    69,    61,    61,    64,
    61,    66,    67,   131,   132,     1,    61,     3,     4,     5,
     6,     7,     8,     9,    10,    11,    12,    13,    14,    15,
    16,    17,    18,    19,    20,    21,    22,    23,    24,    25,
    26,    27,    28,    29,    30,    31,    32,    33,    34,    35,
    36,    37,    38,    39,    40,    41,    42,    43,    44,    45,
    61,    61,    61,    61,    61,    61,    61,    61,    61,    61,
    61,    61,    61,    60,    60,     0,    61,    61,    61,    65,
    61,    69,    68,     3,     4,     5,     6,     7,     8,     9,
    10,    11,    12,    13,    14,    15,    16,    17,    18,    19,
    20,    21,    22,    23,    24,    25,    26,    27,    28,    29,
    30,    31,    32,    33,    34,    35,    36,    37,    38,    39,
    40,    41,    42,    43,    44,    45,    69,    69,    61,    50,
    -1,    -1,   114,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
    -1,    -1,    -1,    -1,    -1,    65
};
/* -*-C-*-  Note some compilers choke on comments on `#line' lines.  */
#line 3 "/usr/lib/bison.simple"
/* This file comes from bison-1.28.  */

/* Skeleton output parser for bison,
   Copyright (C) 1984, 1989, 1990 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

/* As a special exception, when this file is copied by Bison into a
   Bison output file, you may use that output file without restriction.
   This special exception was added by the Free Software Foundation
   in version 1.24 of Bison.  */

/* This is the parser code that is written into each bison parser
  when the %semantic_parser declaration is not specified in the grammar.
  It was written by Richard Stallman by simplifying the hairy parser
  used when %semantic_parser is specified.  */

#ifndef YYSTACK_USE_ALLOCA
#ifdef alloca
#define YYSTACK_USE_ALLOCA
#else /* alloca not defined */
#ifdef __GNUC__
#define YYSTACK_USE_ALLOCA
#define alloca __builtin_alloca
#else /* not GNU C.  */
#if (!defined (__STDC__) && defined (sparc)) || defined (__sparc__) || defined (__sparc) || defined (__sgi) || (defined (__sun) && defined (__i386))
#define YYSTACK_USE_ALLOCA
#include <alloca.h>
#else /* not sparc */
/* We think this test detects Watcom and Microsoft C.  */
/* This used to test MSDOS, but that is a bad idea
   since that symbol is in the user namespace.  */
#if (defined (_MSDOS) || defined (_MSDOS_)) && !defined (__TURBOC__)
#if 0 /* No need for malloc.h, which pollutes the namespace;
	 instead, just don't use alloca.  */
#include <malloc.h>
#endif
#else /* not MSDOS, or __TURBOC__ */
#if defined(_AIX)
/* I don't know what this was needed for, but it pollutes the namespace.
   So I turned it off.   rms, 2 May 1997.  */
/* #include <malloc.h>  */
 #pragma alloca
#define YYSTACK_USE_ALLOCA
#else /* not MSDOS, or __TURBOC__, or _AIX */
#if 0
#ifdef __hpux /* haible@ilog.fr says this works for HPUX 9.05 and up,
		 and on HPUX 10.  Eventually we can turn this on.  */
#define YYSTACK_USE_ALLOCA
#define alloca __builtin_alloca
#endif /* __hpux */
#endif
#endif /* not _AIX */
#endif /* not MSDOS, or __TURBOC__ */
#endif /* not sparc */
#endif /* not GNU C */
#endif /* alloca not defined */
#endif /* YYSTACK_USE_ALLOCA not defined */

#ifdef YYSTACK_USE_ALLOCA
#define YYSTACK_ALLOC alloca
#else
#define YYSTACK_ALLOC malloc
#endif

/* Note: there must be only one dollar sign in this file.
   It is replaced by the list of actions, each action
   as one case of the switch.  */

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		-2
#define YYEOF		0
#define YYACCEPT	goto yyacceptlab
#define YYABORT 	goto yyabortlab
#define YYERROR		goto yyerrlab1
/* Like YYERROR except do call yyerror.
   This remains here temporarily to ease the
   transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */
#define YYFAIL		goto yyerrlab
#define YYRECOVERING()  (!!yyerrstatus)
#define YYBACKUP(token, value) \
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    { yychar = (token), yylval = (value);			\
      yychar1 = YYTRANSLATE (yychar);				\
      YYPOPSTACK;						\
      goto yybackup;						\
    }								\
  else								\
    { yyerror ("syntax error: cannot back up"); YYERROR; }	\
while (0)

#define YYTERROR	1
#define YYERRCODE	256

#ifndef YYPURE
#define YYLEX		yylex()
#endif

#ifdef YYPURE
#ifdef YYLSP_NEEDED
#ifdef YYLEX_PARAM
#define YYLEX		yylex(&yylval, &yylloc, YYLEX_PARAM)
#else
#define YYLEX		yylex(&yylval, &yylloc)
#endif
#else /* not YYLSP_NEEDED */
#ifdef YYLEX_PARAM
#define YYLEX		yylex(&yylval, YYLEX_PARAM)
#else
#define YYLEX		yylex(&yylval)
#endif
#endif /* not YYLSP_NEEDED */
#endif

/* If nonreentrant, generate the variables here */

#ifndef YYPURE

int	yychar;			/*  the lookahead symbol		*/
YYSTYPE	yylval;			/*  the semantic value of the		*/
				/*  lookahead symbol			*/

#ifdef YYLSP_NEEDED
YYLTYPE yylloc;			/*  location data for the lookahead	*/
				/*  symbol				*/
#endif

int yynerrs;			/*  number of parse errors so far       */
#endif  /* not YYPURE */

#if YYDEBUG != 0
int yydebug;			/*  nonzero means print parse trace	*/
/* Since this is uninitialized, it does not stop multiple parsers
   from coexisting.  */
#endif

/*  YYINITDEPTH indicates the initial size of the parser's stacks	*/

#ifndef	YYINITDEPTH
#define YYINITDEPTH 200
#endif

/*  YYMAXDEPTH is the maximum size the stacks can grow to
    (effective only if the built-in stack extension method is used).  */

#if YYMAXDEPTH == 0
#undef YYMAXDEPTH
#endif

#ifndef YYMAXDEPTH
#define YYMAXDEPTH 10000
#endif

/* Define __yy_memcpy.  Note that the size argument
   should be passed with type unsigned int, because that is what the non-GCC
   definitions require.  With GCC, __builtin_memcpy takes an arg
   of type size_t, but it can handle unsigned int.  */

#if __GNUC__ > 1		/* GNU C and GNU C++ define this.  */
#define __yy_memcpy(TO,FROM,COUNT)	__builtin_memcpy(TO,FROM,COUNT)
#else				/* not GNU C or C++ */
#ifndef __cplusplus

/* This is the most reliable way to avoid incompatibilities
   in available built-in functions on various systems.  */
static void
__yy_memcpy (to, from, count)
     char *to;
     char *from;
     unsigned int count;
{
  register char *f = from;
  register char *t = to;
  register int i = count;

  while (i-- > 0)
    *t++ = *f++;
}

#else /* __cplusplus */

/* This is the most reliable way to avoid incompatibilities
   in available built-in functions on various systems.  */
static void
__yy_memcpy (char *to, char *from, unsigned int count)
{
  register char *t = to;
  register char *f = from;
  register int i = count;

  while (i-- > 0)
    *t++ = *f++;
}

#endif
#endif

#line 217 "/usr/lib/bison.simple"

/* The user can define YYPARSE_PARAM as the name of an argument to be passed
   into yyparse.  The argument should have type void *.
   It should actually point to an object.
   Grammar actions can access the variable by casting it
   to the proper pointer type.  */

#ifdef YYPARSE_PARAM
#ifdef __cplusplus
#define YYPARSE_PARAM_ARG void *YYPARSE_PARAM
#define YYPARSE_PARAM_DECL
#else /* not __cplusplus */
#define YYPARSE_PARAM_ARG YYPARSE_PARAM
#define YYPARSE_PARAM_DECL void *YYPARSE_PARAM;
#endif /* not __cplusplus */
#else /* not YYPARSE_PARAM */
#define YYPARSE_PARAM_ARG
#define YYPARSE_PARAM_DECL
#endif /* not YYPARSE_PARAM */

/* Prevent warning if -Wstrict-prototypes.  */
#ifdef __GNUC__
#ifdef YYPARSE_PARAM
int yyparse (void *);
#else
int yyparse (void);
#endif
#endif

int
yyparse(YYPARSE_PARAM_ARG)
     YYPARSE_PARAM_DECL
{
  register int yystate;
  register int yyn;
  register short *yyssp;
  register YYSTYPE *yyvsp;
  int yyerrstatus;	/*  number of tokens to shift before error messages enabled */
  int yychar1 = 0;		/*  lookahead token as an internal (translated) token number */

  short	yyssa[YYINITDEPTH];	/*  the state stack			*/
  YYSTYPE yyvsa[YYINITDEPTH];	/*  the semantic value stack		*/

  short *yyss = yyssa;		/*  refer to the stacks thru separate pointers */
  YYSTYPE *yyvs = yyvsa;	/*  to allow yyoverflow to reallocate them elsewhere */

#ifdef YYLSP_NEEDED
  YYLTYPE yylsa[YYINITDEPTH];	/*  the location stack			*/
  YYLTYPE *yyls = yylsa;
  YYLTYPE *yylsp;

#define YYPOPSTACK   (yyvsp--, yyssp--, yylsp--)
#else
#define YYPOPSTACK   (yyvsp--, yyssp--)
#endif

  int yystacksize = YYINITDEPTH;
  int yyfree_stacks = 0;

#ifdef YYPURE
  int yychar;
  YYSTYPE yylval;
  int yynerrs;
#ifdef YYLSP_NEEDED
  YYLTYPE yylloc;
#endif
#endif

  YYSTYPE yyval;		/*  the variable used to return		*/
				/*  semantic values from the action	*/
				/*  routines				*/

  int yylen;

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Starting parse\n");
#endif

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY;		/* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */

  yyssp = yyss - 1;
  yyvsp = yyvs;
#ifdef YYLSP_NEEDED
  yylsp = yyls;
#endif

/* Push a new state, which is found in  yystate  .  */
/* In all cases, when you get here, the value and location stacks
   have just been pushed. so pushing a state here evens the stacks.  */
yynewstate:

  *++yyssp = yystate;

  if (yyssp >= yyss + yystacksize - 1)
    {
      /* Give user a chance to reallocate the stack */
      /* Use copies of these so that the &'s don't force the real ones into memory. */
      YYSTYPE *yyvs1 = yyvs;
      short *yyss1 = yyss;
#ifdef YYLSP_NEEDED
      YYLTYPE *yyls1 = yyls;
#endif

      /* Get the current used size of the three stacks, in elements.  */
      int size = yyssp - yyss + 1;

#ifdef yyoverflow
      /* Each stack pointer address is followed by the size of
	 the data in use in that stack, in bytes.  */
#ifdef YYLSP_NEEDED
      /* This used to be a conditional around just the two extra args,
	 but that might be undefined if yyoverflow is a macro.  */
      yyoverflow("parser stack overflow",
		 &yyss1, size * sizeof (*yyssp),
		 &yyvs1, size * sizeof (*yyvsp),
		 &yyls1, size * sizeof (*yylsp),
		 &yystacksize);
#else
      yyoverflow("parser stack overflow",
		 &yyss1, size * sizeof (*yyssp),
		 &yyvs1, size * sizeof (*yyvsp),
		 &yystacksize);
#endif

      yyss = yyss1; yyvs = yyvs1;
#ifdef YYLSP_NEEDED
      yyls = yyls1;
#endif
#else /* no yyoverflow */
      /* Extend the stack our own way.  */
      if (yystacksize >= YYMAXDEPTH)
	{
	  yyerror("parser stack overflow");
	  if (yyfree_stacks)
	    {
	      free (yyss);
	      free (yyvs);
#ifdef YYLSP_NEEDED
	      free (yyls);
#endif
	    }
	  return 2;
	}
      yystacksize *= 2;
      if (yystacksize > YYMAXDEPTH)
	yystacksize = YYMAXDEPTH;
#ifndef YYSTACK_USE_ALLOCA
      yyfree_stacks = 1;
#endif
      yyss = (short *) YYSTACK_ALLOC (yystacksize * sizeof (*yyssp));
      __yy_memcpy ((char *)yyss, (char *)yyss1,
		   size * (unsigned int) sizeof (*yyssp));
      yyvs = (YYSTYPE *) YYSTACK_ALLOC (yystacksize * sizeof (*yyvsp));
      __yy_memcpy ((char *)yyvs, (char *)yyvs1,
		   size * (unsigned int) sizeof (*yyvsp));
#ifdef YYLSP_NEEDED
      yyls = (YYLTYPE *) YYSTACK_ALLOC (yystacksize * sizeof (*yylsp));
      __yy_memcpy ((char *)yyls, (char *)yyls1,
		   size * (unsigned int) sizeof (*yylsp));
#endif
#endif /* no yyoverflow */

      yyssp = yyss + size - 1;
      yyvsp = yyvs + size - 1;
#ifdef YYLSP_NEEDED
      yylsp = yyls + size - 1;
#endif

#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Stack size increased to %d\n", yystacksize);
#endif

      if (yyssp >= yyss + yystacksize - 1)
	YYABORT;
    }

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Entering state %d\n", yystate);
#endif

  goto yybackup;
 yybackup:

/* Do appropriate processing given the current state.  */
/* Read a lookahead token if we need one and don't already have one.  */
/* yyresume: */

  /* First try to decide what to do without reference to lookahead token.  */

  yyn = yypact[yystate];
  if (yyn == YYFLAG)
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* yychar is either YYEMPTY or YYEOF
     or a valid token in external form.  */

  if (yychar == YYEMPTY)
    {
#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Reading a token: ");
#endif
      yychar = YYLEX;
    }

  /* Convert token to internal form (in yychar1) for indexing tables with */

  if (yychar <= 0)		/* This means end of input. */
    {
      yychar1 = 0;
      yychar = YYEOF;		/* Don't call YYLEX any more */

#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Now at end of input.\n");
#endif
    }
  else
    {
      yychar1 = YYTRANSLATE(yychar);

#if YYDEBUG != 0
      if (yydebug)
	{
	  fprintf (stderr, "Next token is %d (%s", yychar, yytname[yychar1]);
	  /* Give the individual parser a way to print the precise meaning
	     of a token, for further debugging info.  */
#ifdef YYPRINT
	  YYPRINT (stderr, yychar, yylval);
#endif
	  fprintf (stderr, ")\n");
	}
#endif
    }

  yyn += yychar1;
  if (yyn < 0 || yyn > YYLAST || yycheck[yyn] != yychar1)
    goto yydefault;

  yyn = yytable[yyn];

  /* yyn is what to do for this token type in this state.
     Negative => reduce, -yyn is rule number.
     Positive => shift, yyn is new state.
       New state is final state => don't bother to shift,
       just return success.
     0, or most negative number => error.  */

  if (yyn < 0)
    {
      if (yyn == YYFLAG)
	goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }
  else if (yyn == 0)
    goto yyerrlab;

  if (yyn == YYFINAL)
    YYACCEPT;

  /* Shift the lookahead token.  */

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Shifting token %d (%s), ", yychar, yytname[yychar1]);
#endif

  /* Discard the token being shifted unless it is eof.  */
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  *++yyvsp = yylval;
#ifdef YYLSP_NEEDED
  *++yylsp = yylloc;
#endif

  /* count tokens shifted since error; after three, turn off error status.  */
  if (yyerrstatus) yyerrstatus--;

  yystate = yyn;
  goto yynewstate;

/* Do the default action for the current state.  */
yydefault:

  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;

/* Do a reduction.  yyn is the number of a rule to reduce with.  */
yyreduce:
  yylen = yyr2[yyn];
  if (yylen > 0)
    yyval = yyvsp[1-yylen]; /* implement default value of the action */

#if YYDEBUG != 0
  if (yydebug)
    {
      int i;

      fprintf (stderr, "Reducing via rule %d (line %d), ",
	       yyn, yyrline[yyn]);

      /* Print the symbols being reduced, and their result.  */
      for (i = yyprhs[yyn]; yyrhs[i] > 0; i++)
	fprintf (stderr, "%s ", yytname[yyrhs[i]]);
      fprintf (stderr, " -> %s\n", yytname[yyr1[yyn]]);
    }
#endif


  switch (yyn) {

case 1:
#line 174 "matcher_parser_parse.y"
{
	if (!matcher_parser_dialog) {
		prefs_scoring = &global_scoring;
		prefs_filtering = &global_processing;
	}
;
    break;}
case 7:
#line 192 "matcher_parser_parse.y"
{
	yyerrok;
;
    break;}
case 8:
#line 198 "matcher_parser_parse.y"
{
	gchar * folder = yyvsp[-1].str;
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
;
    break;}
case 12:
#line 224 "matcher_parser_parse.y"
{
	if (matcher_parser_dialog)
		YYACCEPT;
	else {
		matcher_parsererror("parse error");
		YYERROR;
	}
;
    break;}
case 14:
#line 237 "matcher_parser_parse.y"
{
	if (matcher_parser_dialog)
		YYACCEPT;
	else {
		matcher_parsererror("parse error");
		YYERROR;
	}
;
    break;}
case 15:
#line 249 "matcher_parser_parse.y"
{
	filtering = filteringprop_new(cond, action);
	cond = NULL;
	action = NULL;
	if (!matcher_parser_dialog) {
		* prefs_filtering = g_slist_append(* prefs_filtering,
						   filtering);
		filtering = NULL;
	}
;
    break;}
case 16:
#line 260 "matcher_parser_parse.y"
{
	scoring = scoringprop_new(cond, score);
	cond = NULL;
	score = 0;
	if (!matcher_parser_dialog) {
		* prefs_scoring = g_slist_append(* prefs_scoring, scoring);
		scoring = NULL;
	}
;
    break;}
case 17:
#line 273 "matcher_parser_parse.y"
{
	match_type = MATCHTYPE_MATCHCASE;
;
    break;}
case 18:
#line 277 "matcher_parser_parse.y"
{
	match_type = MATCHTYPE_MATCH;
;
    break;}
case 19:
#line 281 "matcher_parser_parse.y"
{
	match_type = MATCHTYPE_REGEXPCASE;
;
    break;}
case 20:
#line 285 "matcher_parser_parse.y"
{
	match_type = MATCHTYPE_REGEXP;
;
    break;}
case 21:
#line 292 "matcher_parser_parse.y"
{
	cond = matcherlist_new(matchers_list, (bool_op == MATCHERBOOL_AND));
	matchers_list = NULL;
;
    break;}
case 22:
#line 300 "matcher_parser_parse.y"
{
	matchers_list = g_slist_append(matchers_list, prop);
;
    break;}
case 23:
#line 304 "matcher_parser_parse.y"
{
	matchers_list = NULL;
	matchers_list = g_slist_append(matchers_list, prop);
;
    break;}
case 24:
#line 312 "matcher_parser_parse.y"
{
	bool_op = MATCHERBOOL_AND;
;
    break;}
case 25:
#line 316 "matcher_parser_parse.y"
{
	bool_op = MATCHERBOOL_OR;
;
    break;}
case 26:
#line 323 "matcher_parser_parse.y"
{
	gint criteria = 0;

	criteria = MATCHCRITERIA_ALL;
	prop = matcherprop_new(criteria, NULL, 0, NULL, 0);
;
    break;}
case 27:
#line 330 "matcher_parser_parse.y"
{
	gint criteria = 0;

	criteria = MATCHCRITERIA_UNREAD;
	prop = matcherprop_new(criteria, NULL, 0, NULL, 0);
;
    break;}
case 28:
#line 337 "matcher_parser_parse.y"
{
	gint criteria = 0;

	criteria = MATCHCRITERIA_NOT_UNREAD;
	prop = matcherprop_new(criteria, NULL, 0, NULL, 0);
;
    break;}
case 29:
#line 344 "matcher_parser_parse.y"
{
	gint criteria = 0;

	criteria = MATCHCRITERIA_NEW;
	prop = matcherprop_new(criteria, NULL, 0, NULL, 0);
;
    break;}
case 30:
#line 351 "matcher_parser_parse.y"
{
	gint criteria = 0;

	criteria = MATCHCRITERIA_NOT_NEW;
	prop = matcherprop_new(criteria, NULL, 0, NULL, 0);
;
    break;}
case 31:
#line 358 "matcher_parser_parse.y"
{
	gint criteria = 0;

	criteria = MATCHCRITERIA_MARKED;
	prop = matcherprop_new(criteria, NULL, 0, NULL, 0);
;
    break;}
case 32:
#line 365 "matcher_parser_parse.y"
{
	gint criteria = 0;

	criteria = MATCHCRITERIA_NOT_MARKED;
	prop = matcherprop_new(criteria, NULL, 0, NULL, 0);
;
    break;}
case 33:
#line 372 "matcher_parser_parse.y"
{
	gint criteria = 0;

	criteria = MATCHCRITERIA_DELETED;
	prop = matcherprop_new(criteria, NULL, 0, NULL, 0);
;
    break;}
case 34:
#line 379 "matcher_parser_parse.y"
{
	gint criteria = 0;

	criteria = MATCHCRITERIA_NOT_DELETED;
	prop = matcherprop_new(criteria, NULL, 0, NULL, 0);
;
    break;}
case 35:
#line 386 "matcher_parser_parse.y"
{
	gint criteria = 0;

	criteria = MATCHCRITERIA_REPLIED;
	prop = matcherprop_new(criteria, NULL, 0, NULL, 0);
;
    break;}
case 36:
#line 393 "matcher_parser_parse.y"
{
	gint criteria = 0;

	criteria = MATCHCRITERIA_NOT_REPLIED;
	prop = matcherprop_new(criteria, NULL, 0, NULL, 0);
;
    break;}
case 37:
#line 400 "matcher_parser_parse.y"
{
	gint criteria = 0;

	criteria = MATCHCRITERIA_FORWARDED;
	prop = matcherprop_new(criteria, NULL, 0, NULL, 0);
;
    break;}
case 38:
#line 407 "matcher_parser_parse.y"
{
	gint criteria = 0;

	criteria = MATCHCRITERIA_NOT_FORWARDED;
	prop = matcherprop_new(criteria, NULL, 0, NULL, 0);
;
    break;}
case 39:
#line 414 "matcher_parser_parse.y"
{
	gint criteria = 0;
	gchar * expr = NULL;

	criteria = MATCHCRITERIA_SUBJECT;
	expr = yyvsp[0].str;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
;
    break;}
case 40:
#line 423 "matcher_parser_parse.y"
{
	gint criteria = 0;
	gchar * expr = NULL;

	criteria = MATCHCRITERIA_NOT_SUBJECT;
	expr = yyvsp[0].str;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
;
    break;}
case 41:
#line 432 "matcher_parser_parse.y"
{
	gint criteria = 0;
	gchar * expr = NULL;

	criteria = MATCHCRITERIA_FROM;
	expr = yyvsp[0].str;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
;
    break;}
case 42:
#line 441 "matcher_parser_parse.y"
{
	gint criteria = 0;
	gchar * expr = NULL;

	criteria = MATCHCRITERIA_NOT_FROM;
	expr = yyvsp[0].str;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
;
    break;}
case 43:
#line 450 "matcher_parser_parse.y"
{
	gint criteria = 0;
	gchar * expr = NULL;

	criteria = MATCHCRITERIA_TO;
	expr = yyvsp[0].str;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
;
    break;}
case 44:
#line 459 "matcher_parser_parse.y"
{
	gint criteria = 0;
	gchar * expr = NULL;

	criteria = MATCHCRITERIA_NOT_TO;
	expr = yyvsp[0].str;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
;
    break;}
case 45:
#line 468 "matcher_parser_parse.y"
{
	gint criteria = 0;
	gchar * expr = NULL;

	criteria = MATCHCRITERIA_CC;
	expr = yyvsp[0].str;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
;
    break;}
case 46:
#line 477 "matcher_parser_parse.y"
{
	gint criteria = 0;
	gchar * expr = NULL;

	criteria = MATCHCRITERIA_NOT_CC;
	expr = yyvsp[0].str;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
;
    break;}
case 47:
#line 486 "matcher_parser_parse.y"
{
	gint criteria = 0;
	gchar * expr = NULL;

	criteria = MATCHCRITERIA_TO_OR_CC;
	expr = yyvsp[0].str;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
;
    break;}
case 48:
#line 495 "matcher_parser_parse.y"
{
	gint criteria = 0;
	gchar * expr = NULL;

	criteria = MATCHCRITERIA_NOT_TO_AND_NOT_CC;
	expr = yyvsp[0].str;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
;
    break;}
case 49:
#line 504 "matcher_parser_parse.y"
{
	gint criteria = 0;
	gint value = 0;

	criteria = MATCHCRITERIA_AGE_GREATER;
	value = atoi(yyvsp[0].value);
	prop = matcherprop_new(criteria, NULL, 0, NULL, value);
;
    break;}
case 50:
#line 513 "matcher_parser_parse.y"
{
	gint criteria = 0;
	gint value = 0;

	criteria = MATCHCRITERIA_AGE_LOWER;
	value = atoi(yyvsp[0].value);
	prop = matcherprop_new(criteria, NULL, 0, NULL, value);
;
    break;}
case 51:
#line 522 "matcher_parser_parse.y"
{
	gint criteria = 0;
	gchar * expr = NULL;

	criteria = MATCHCRITERIA_NEWSGROUPS;
	expr = yyvsp[0].str;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
;
    break;}
case 52:
#line 531 "matcher_parser_parse.y"
{
	gint criteria = 0;
	gchar * expr = NULL;

	criteria = MATCHCRITERIA_NOT_NEWSGROUPS;
	expr = yyvsp[0].str;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
;
    break;}
case 53:
#line 540 "matcher_parser_parse.y"
{
	gint criteria = 0;
	gchar * expr = NULL;

	criteria = MATCHCRITERIA_INREPLYTO;
	expr = yyvsp[0].str;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
;
    break;}
case 54:
#line 549 "matcher_parser_parse.y"
{
	gint criteria = 0;
	gchar * expr = NULL;

	criteria = MATCHCRITERIA_NOT_INREPLYTO;
	expr = yyvsp[0].str;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
;
    break;}
case 55:
#line 558 "matcher_parser_parse.y"
{
	gint criteria = 0;
	gchar * expr = NULL;

	criteria = MATCHCRITERIA_REFERENCES;
	expr = yyvsp[0].str;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
;
    break;}
case 56:
#line 567 "matcher_parser_parse.y"
{
	gint criteria = 0;
	gchar * expr = NULL;

	criteria = MATCHCRITERIA_NOT_REFERENCES;
	expr = yyvsp[0].str;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
;
    break;}
case 57:
#line 576 "matcher_parser_parse.y"
{
	gint criteria = 0;
	gint value = 0;

	criteria = MATCHCRITERIA_SCORE_GREATER;
	value = atoi(yyvsp[0].value);
	prop = matcherprop_new(criteria, NULL, 0, NULL, value);
;
    break;}
case 58:
#line 585 "matcher_parser_parse.y"
{
	gint criteria = 0;
	gint value = 0;

	criteria = MATCHCRITERIA_SCORE_LOWER;
	value = atoi(yyvsp[0].value);
	prop = matcherprop_new(criteria, NULL, 0, NULL, value);
;
    break;}
case 59:
#line 594 "matcher_parser_parse.y"
{
	gint criteria = 0;
	gint value = 0;

	criteria = MATCHCRITERIA_SCORE_EQUAL;
	value = atoi(yyvsp[0].value);
	prop = matcherprop_new(criteria, NULL, 0, NULL, value);
;
    break;}
case 60:
#line 603 "matcher_parser_parse.y"
{
	header = g_strdup(yyvsp[0].str);
;
    break;}
case 61:
#line 606 "matcher_parser_parse.y"
{
	gint criteria = 0;
	gchar * expr = NULL;

	criteria = MATCHCRITERIA_HEADER;
	expr = yyvsp[-3].str;
	prop = matcherprop_new(criteria, header, match_type, expr, 0);
	g_free(header);
;
    break;}
case 62:
#line 616 "matcher_parser_parse.y"
{
	header = g_strdup(yyvsp[0].str);
;
    break;}
case 63:
#line 619 "matcher_parser_parse.y"
{
	gint criteria = 0;
	gchar * expr = NULL;

	criteria = MATCHCRITERIA_NOT_HEADER;
	expr = yyvsp[-3].str;
	prop = matcherprop_new(criteria, header, match_type, expr, 0);
	g_free(header);
;
    break;}
case 64:
#line 629 "matcher_parser_parse.y"
{
	gint criteria = 0;
	gchar * expr = NULL;

	criteria = MATCHCRITERIA_HEADERS_PART;
	expr = yyvsp[0].str;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
;
    break;}
case 65:
#line 638 "matcher_parser_parse.y"
{
	gint criteria = 0;
	gchar * expr = NULL;

	criteria = MATCHCRITERIA_NOT_HEADERS_PART;
	expr = yyvsp[0].str;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
;
    break;}
case 66:
#line 647 "matcher_parser_parse.y"
{
	gint criteria = 0;
	gchar * expr = NULL;

	criteria = MATCHCRITERIA_MESSAGE;
	expr = yyvsp[0].str;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
;
    break;}
case 67:
#line 656 "matcher_parser_parse.y"
{
	gint criteria = 0;
	gchar * expr = NULL;

	criteria = MATCHCRITERIA_NOT_MESSAGE;
	expr = yyvsp[0].str;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
;
    break;}
case 68:
#line 665 "matcher_parser_parse.y"
{
	gint criteria = 0;
	gchar * expr = NULL;

	criteria = MATCHCRITERIA_BODY_PART;
	expr = yyvsp[0].str;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
;
    break;}
case 69:
#line 674 "matcher_parser_parse.y"
{
	gint criteria = 0;
	gchar * expr = NULL;

	criteria = MATCHCRITERIA_NOT_BODY_PART;
	expr = yyvsp[0].str;
	prop = matcherprop_new(criteria, NULL, match_type, expr, 0);
;
    break;}
case 70:
#line 683 "matcher_parser_parse.y"
{
	gint criteria = 0;
	gchar * expr = NULL;

	criteria = MATCHCRITERIA_EXECUTE;
	expr = yyvsp[0].str;
	prop = matcherprop_new(criteria, NULL, 0, expr, 0);
;
    break;}
case 71:
#line 692 "matcher_parser_parse.y"
{
	gint criteria = 0;
	gchar * expr = NULL;

	criteria = MATCHCRITERIA_NOT_EXECUTE;
	expr = yyvsp[0].str;
	prop = matcherprop_new(criteria, NULL, 0, expr, 0);
;
    break;}
case 72:
#line 704 "matcher_parser_parse.y"
{
	gchar * cmd = NULL;
	gint action_type = 0;

	action_type = MATCHACTION_EXECUTE;
	cmd = yyvsp[0].str;
	action = filteringaction_new(action_type, 0, cmd, 0);
;
    break;}
case 73:
#line 713 "matcher_parser_parse.y"
{
	gchar * destination = NULL;
	gint action_type = 0;

	action_type = MATCHACTION_MOVE;
	destination = yyvsp[0].str;
	action = filteringaction_new(action_type, 0, destination, 0);
;
    break;}
case 74:
#line 722 "matcher_parser_parse.y"
{
	gchar * destination = NULL;
	gint action_type = 0;

	action_type = MATCHACTION_COPY;
	destination = yyvsp[0].str;
	action = filteringaction_new(action_type, 0, destination, 0);
;
    break;}
case 75:
#line 731 "matcher_parser_parse.y"
{
	gint action_type = 0;

	action_type = MATCHACTION_DELETE;
	action = filteringaction_new(action_type, 0, NULL, 0);
;
    break;}
case 76:
#line 738 "matcher_parser_parse.y"
{
	gint action_type = 0;

	action_type = MATCHACTION_MARK;
	action = filteringaction_new(action_type, 0, NULL, 0);
;
    break;}
case 77:
#line 745 "matcher_parser_parse.y"
{
	gint action_type = 0;

	action_type = MATCHACTION_UNMARK;
	action = filteringaction_new(action_type, 0, NULL, 0);
;
    break;}
case 78:
#line 752 "matcher_parser_parse.y"
{
	gint action_type = 0;

	action_type = MATCHACTION_MARK_AS_READ;
	action = filteringaction_new(action_type, 0, NULL, 0);
;
    break;}
case 79:
#line 759 "matcher_parser_parse.y"
{
	gint action_type = 0;

	action_type = MATCHACTION_MARK_AS_UNREAD;
	action = filteringaction_new(action_type, 0, NULL, 0);
;
    break;}
case 80:
#line 766 "matcher_parser_parse.y"
{
	gchar * destination = NULL;
	gint action_type = 0;
	gint account_id = 0;

	action_type = MATCHACTION_FORWARD;
	account_id = atoi(yyvsp[-1].value);
	destination = yyvsp[0].str;
	action = filteringaction_new(action_type, account_id, destination, 0);
;
    break;}
case 81:
#line 777 "matcher_parser_parse.y"
{
	gchar * destination = NULL;
	gint action_type = 0;
	gint account_id = 0;

	action_type = MATCHACTION_FORWARD_AS_ATTACHMENT;
	account_id = atoi(yyvsp[-1].value);
	destination = yyvsp[0].str;
	action = filteringaction_new(action_type, account_id, destination, 0);
;
    break;}
case 82:
#line 788 "matcher_parser_parse.y"
{
	gchar * destination = NULL;
	gint action_type = 0;
	gint account_id = 0;

	action_type = MATCHACTION_BOUNCE;
	account_id = atoi(yyvsp[-1].value);
	destination = yyvsp[0].str;
	action = filteringaction_new(action_type, account_id, destination, 0);
;
    break;}
case 83:
#line 799 "matcher_parser_parse.y"
{
	gint action_type = 0;
	gint color = 0;

	action_type = MATCHACTION_COLOR;
	color = atoi(yyvsp[0].value);
	action = filteringaction_new(action_type, 0, NULL, color);
;
    break;}
case 84:
#line 808 "matcher_parser_parse.y"
{
	gint action_type = 0;
	action_type = MATCHACTION_DELETE_ON_SERVER;
	action = filteringaction_new(action_type, 0, NULL, 0);
;
    break;}
case 85:
#line 817 "matcher_parser_parse.y"
{
	score = atoi(yyvsp[0].value);
;
    break;}
}
   /* the action file gets copied in in place of this dollarsign */
#line 543 "/usr/lib/bison.simple"

  yyvsp -= yylen;
  yyssp -= yylen;
#ifdef YYLSP_NEEDED
  yylsp -= yylen;
#endif

#if YYDEBUG != 0
  if (yydebug)
    {
      short *ssp1 = yyss - 1;
      fprintf (stderr, "state stack now");
      while (ssp1 != yyssp)
	fprintf (stderr, " %d", *++ssp1);
      fprintf (stderr, "\n");
    }
#endif

  *++yyvsp = yyval;

#ifdef YYLSP_NEEDED
  yylsp++;
  if (yylen == 0)
    {
      yylsp->first_line = yylloc.first_line;
      yylsp->first_column = yylloc.first_column;
      yylsp->last_line = (yylsp-1)->last_line;
      yylsp->last_column = (yylsp-1)->last_column;
      yylsp->text = 0;
    }
  else
    {
      yylsp->last_line = (yylsp+yylen-1)->last_line;
      yylsp->last_column = (yylsp+yylen-1)->last_column;
    }
#endif

  /* Now "shift" the result of the reduction.
     Determine what state that goes to,
     based on the state we popped back to
     and the rule number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTBASE] + *yyssp;
  if (yystate >= 0 && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTBASE];

  goto yynewstate;

yyerrlab:   /* here on detecting error */

  if (! yyerrstatus)
    /* If not already recovering from an error, report this error.  */
    {
      ++yynerrs;

#ifdef YYERROR_VERBOSE
      yyn = yypact[yystate];

      if (yyn > YYFLAG && yyn < YYLAST)
	{
	  int size = 0;
	  char *msg;
	  int x, count;

	  count = 0;
	  /* Start X at -yyn if nec to avoid negative indexes in yycheck.  */
	  for (x = (yyn < 0 ? -yyn : 0);
	       x < (sizeof(yytname) / sizeof(char *)); x++)
	    if (yycheck[x + yyn] == x)
	      size += strlen(yytname[x]) + 15, count++;
	  msg = (char *) malloc(size + 15);
	  if (msg != 0)
	    {
	      strcpy(msg, "parse error");

	      if (count < 5)
		{
		  count = 0;
		  for (x = (yyn < 0 ? -yyn : 0);
		       x < (sizeof(yytname) / sizeof(char *)); x++)
		    if (yycheck[x + yyn] == x)
		      {
			strcat(msg, count == 0 ? ", expecting `" : " or `");
			strcat(msg, yytname[x]);
			strcat(msg, "'");
			count++;
		      }
		}
	      yyerror(msg);
	      free(msg);
	    }
	  else
	    yyerror ("parse error; also virtual memory exceeded");
	}
      else
#endif /* YYERROR_VERBOSE */
	yyerror("parse error");
    }

  goto yyerrlab1;
yyerrlab1:   /* here on error raised explicitly by an action */

  if (yyerrstatus == 3)
    {
      /* if just tried and failed to reuse lookahead token after an error, discard it.  */

      /* return failure if at end of input */
      if (yychar == YYEOF)
	YYABORT;

#if YYDEBUG != 0
      if (yydebug)
	fprintf(stderr, "Discarding token %d (%s).\n", yychar, yytname[yychar1]);
#endif

      yychar = YYEMPTY;
    }

  /* Else will try to reuse lookahead token
     after shifting the error token.  */

  yyerrstatus = 3;		/* Each real token shifted decrements this */

  goto yyerrhandle;

yyerrdefault:  /* current state does not do anything special for the error token. */

#if 0
  /* This is wrong; only states that explicitly want error tokens
     should shift them.  */
  yyn = yydefact[yystate];  /* If its default is to accept any token, ok.  Otherwise pop it.*/
  if (yyn) goto yydefault;
#endif

yyerrpop:   /* pop the current state because it cannot handle the error token */

  if (yyssp == yyss) YYABORT;
  yyvsp--;
  yystate = *--yyssp;
#ifdef YYLSP_NEEDED
  yylsp--;
#endif

#if YYDEBUG != 0
  if (yydebug)
    {
      short *ssp1 = yyss - 1;
      fprintf (stderr, "Error: state stack now");
      while (ssp1 != yyssp)
	fprintf (stderr, " %d", *++ssp1);
      fprintf (stderr, "\n");
    }
#endif

yyerrhandle:

  yyn = yypact[yystate];
  if (yyn == YYFLAG)
    goto yyerrdefault;

  yyn += YYTERROR;
  if (yyn < 0 || yyn > YYLAST || yycheck[yyn] != YYTERROR)
    goto yyerrdefault;

  yyn = yytable[yyn];
  if (yyn < 0)
    {
      if (yyn == YYFLAG)
	goto yyerrpop;
      yyn = -yyn;
      goto yyreduce;
    }
  else if (yyn == 0)
    goto yyerrpop;

  if (yyn == YYFINAL)
    YYACCEPT;

#if YYDEBUG != 0
  if (yydebug)
    fprintf(stderr, "Shifting error token, ");
#endif

  *++yyvsp = yylval;
#ifdef YYLSP_NEEDED
  *++yylsp = yylloc;
#endif

  yystate = yyn;
  goto yynewstate;

 yyacceptlab:
  /* YYACCEPT comes here.  */
  if (yyfree_stacks)
    {
      free (yyss);
      free (yyvs);
#ifdef YYLSP_NEEDED
      free (yyls);
#endif
    }
  return 0;

 yyabortlab:
  /* YYABORT comes here.  */
  if (yyfree_stacks)
    {
      free (yyss);
      free (yyvs);
#ifdef YYLSP_NEEDED
      free (yyls);
#endif
    }
  return 1;
}
#line 821 "matcher_parser_parse.y"
