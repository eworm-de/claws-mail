
/*  A Bison parser, made from quote_fmt.y
    by GNU Bison version 1.28  */

#define YYBISON 1  /* Identify Bison output.  */

#define yyparse quote_fmtparse
#define yylex quote_fmtlex
#define yyerror quote_fmterror
#define yylval quote_fmtlval
#define yychar quote_fmtchar
#define yydebug quote_fmtdebug
#define yynerrs quote_fmtnerrs
#define	SHOW_NEWSGROUPS	257
#define	SHOW_DATE	258
#define	SHOW_FROM	259
#define	SHOW_FULLNAME	260
#define	SHOW_FIRST_NAME	261
#define	SHOW_SENDER_INITIAL	262
#define	SHOW_SUBJECT	263
#define	SHOW_TO	264
#define	SHOW_MESSAGEID	265
#define	SHOW_PERCENT	266
#define	SHOW_CC	267
#define	SHOW_REFERENCES	268
#define	SHOW_MESSAGE	269
#define	SHOW_QUOTED_MESSAGE	270
#define	SHOW_BACKSLASH	271
#define	SHOW_TAB	272
#define	SHOW_EOL	273
#define	SHOW_QUESTION_MARK	274
#define	SHOW_OPARENT	275
#define	SHOW_CPARENT	276
#define	QUERY_DATE	277
#define	QUERY_FROM	278
#define	QUERY_FULLNAME	279
#define	QUERY_SUBJECT	280
#define	QUERY_TO	281
#define	QUERY_NEWSGROUPS	282
#define	QUERY_MESSAGEID	283
#define	QUERY_CC	284
#define	QUERY_REFERENCES	285
#define	OPARENT	286
#define	CPARENT	287
#define	CHARACTER	288

#line 1 "quote_fmt.y"

#include <ctype.h>
#include <gtk/gtk.h>
#include <glib.h>

#include "procmsg.h"
#include "procmime.h"
#include "utils.h"
#include "defs.h"
#include "intl.h"

#include "quote_fmt.tab.h"

#include "lex.quote_fmt.c"

/* decl */
/*
flex quote_fmt.l
bison -p quote_fmt quote_fmt.y
*/

static MsgInfo * msginfo = NULL;
static gboolean * visible = NULL;
static gint maxsize = 0;
static gint stacksize = 0;

static gchar * buffer = NULL;
static gint bufmax = 0;
static gint bufsize = 0;
static gchar * quote_str = NULL;
static gint error = 0;

static void add_visibility(gboolean val)
{
	stacksize ++;
	if (maxsize < stacksize) {
		maxsize += 128;
		visible = g_realloc(visible, maxsize * sizeof(gboolean));
		if (visible == NULL)
			maxsize = 0;
	}

	visible[stacksize - 1] = val;
}

static void remove_visibility()
{
	stacksize --;
}


static void add_buffer(gchar * s)
{
	gint len = strlen(s);
	
	if (bufsize + len + 1 > bufmax) {
		if (bufmax == 0)
			bufmax = 128;
		while (bufsize + len + 1 > bufmax)
			bufmax *= 2;
		buffer = g_realloc(buffer, bufmax);
	}
	strcpy(buffer + bufsize, s);
	bufsize += len;
}

static void flush_buffer()
{
	if (buffer != NULL)
		*buffer = 0;
	bufsize = 0;
}

gchar * quote_fmt_get_buffer()
{
	if (error != 0)
		return NULL;
	else
		return buffer;
}

#define INSERT(buf) \
	if (stacksize != 0 && visible[stacksize - 1]) \
		add_buffer(buf)

void quote_fmt_init(MsgInfo * info, gchar * my_quote_str)
{
	quote_str = my_quote_str;
	msginfo = info;
	stacksize = 0;
	add_visibility(TRUE);
	if (buffer != NULL)
		*buffer = 0;
	bufsize = 0;
	error = 0;
}

void quote_fmterror(char * str)
{
	g_warning(_("Error %s\n"), str);
	error = 1;
}

int quote_fmtwrap(void)
{
	return 1;
}

static int isseparator(char ch)
{
	return isspace(ch) || ch == '.' || ch == '-';
}

#line 115 "quote_fmt.y"
typedef union {
	char * str;
} YYSTYPE;
#include <stdio.h>

#ifndef __cplusplus
#ifndef __STDC__
#define const
#endif
#endif



#define	YYFINAL		75
#define	YYFLAG		-32768
#define	YYNTBASE	35

#define YYTRANSLATE(x) ((unsigned)(x) <= 288 ? yytranslate[x] : 50)

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
    27,    28,    29,    30,    31,    32,    33,    34
};

#if YYDEBUG != 0
static const short yyprhs[] = {     0,
     0,     2,     5,     7,     9,    11,    13,    15,    17,    19,
    21,    23,    25,    27,    29,    31,    33,    35,    37,    39,
    41,    43,    45,    47,    49,    51,    53,    55,    56,    62,
    63,    69,    70,    76,    77,    83,    84,    90,    91,    97,
    98,   104,   105,   111,   112
};

static const short yyrhs[] = {    36,
     0,    37,    36,     0,    37,     0,    39,     0,    38,     0,
    40,     0,    34,     0,     3,     0,     4,     0,     5,     0,
     6,     0,     7,     0,     8,     0,     9,     0,    10,     0,
    11,     0,    12,     0,    13,     0,    14,     0,    15,     0,
    16,     0,    17,     0,    18,     0,    19,     0,    20,     0,
    21,     0,    22,     0,     0,    23,    41,    32,    35,    33,
     0,     0,    24,    42,    32,    35,    33,     0,     0,    25,
    43,    32,    35,    33,     0,     0,    26,    44,    32,    35,
    33,     0,     0,    27,    45,    32,    35,    33,     0,     0,
    28,    46,    32,    35,    33,     0,     0,    29,    47,    32,
    35,    33,     0,     0,    30,    48,    32,    35,    33,     0,
     0,    31,    49,    32,    35,    33,     0
};

#endif

#if YYDEBUG != 0
static const short yyrline[] = { 0,
   137,   140,   142,   144,   146,   150,   153,   159,   165,   170,
   175,   180,   196,   224,   229,   234,   239,   243,   248,   253,
   265,   279,   283,   287,   291,   295,   299,   304,   309,   313,
   317,   321,   325,   329,   333,   337,   341,   345,   349,   353,
   357,   361,   365,   369,   373
};
#endif


#if YYDEBUG != 0 || defined (YYERROR_VERBOSE)

static const char * const yytname[] = {   "$","error","$undefined.","SHOW_NEWSGROUPS",
"SHOW_DATE","SHOW_FROM","SHOW_FULLNAME","SHOW_FIRST_NAME","SHOW_SENDER_INITIAL",
"SHOW_SUBJECT","SHOW_TO","SHOW_MESSAGEID","SHOW_PERCENT","SHOW_CC","SHOW_REFERENCES",
"SHOW_MESSAGE","SHOW_QUOTED_MESSAGE","SHOW_BACKSLASH","SHOW_TAB","SHOW_EOL",
"SHOW_QUESTION_MARK","SHOW_OPARENT","SHOW_CPARENT","QUERY_DATE","QUERY_FROM",
"QUERY_FULLNAME","QUERY_SUBJECT","QUERY_TO","QUERY_NEWSGROUPS","QUERY_MESSAGEID",
"QUERY_CC","QUERY_REFERENCES","OPARENT","CPARENT","CHARACTER","quote_fmt","character_or_special_or_query_list",
"character_or_special_or_query","character","special","query","@1","@2","@3",
"@4","@5","@6","@7","@8","@9", NULL
};
#endif

static const short yyr1[] = {     0,
    35,    36,    36,    37,    37,    37,    38,    39,    39,    39,
    39,    39,    39,    39,    39,    39,    39,    39,    39,    39,
    39,    39,    39,    39,    39,    39,    39,    41,    40,    42,
    40,    43,    40,    44,    40,    45,    40,    46,    40,    47,
    40,    48,    40,    49,    40
};

static const short yyr2[] = {     0,
     1,     2,     1,     1,     1,     1,     1,     1,     1,     1,
     1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
     1,     1,     1,     1,     1,     1,     1,     0,     5,     0,
     5,     0,     5,     0,     5,     0,     5,     0,     5,     0,
     5,     0,     5,     0,     5
};

static const short yydefact[] = {     0,
     8,     9,    10,    11,    12,    13,    14,    15,    16,    17,
    18,    19,    20,    21,    22,    23,    24,    25,    26,    27,
    28,    30,    32,    34,    36,    38,    40,    42,    44,     7,
     1,     3,     5,     4,     6,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     2,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,    29,    31,    33,    35,    37,    39,    41,
    43,    45,     0,     0,     0
};

static const short yydefgoto[] = {    55,
    31,    32,    33,    34,    35,    36,    37,    38,    39,    40,
    41,    42,    43,    44
};

static const short yypact[] = {    -2,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,    -2,-32768,-32768,-32768,    -1,     1,     2,     3,     4,
     5,     6,     7,     8,-32768,    -2,    -2,    -2,    -2,    -2,
    -2,    -2,    -2,    -2,    -3,     9,    10,    11,    12,    13,
    22,    23,    24,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,    41,    58,-32768
};

static const short yypgoto[] = {     0,
    27,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,-32768,
-32768,-32768,-32768,-32768
};


#define	YYLAST		59


static const short yytable[] = {    73,
     1,     2,     3,     4,     5,     6,     7,     8,     9,    10,
    11,    12,    13,    14,    15,    16,    17,    18,    19,    20,
    21,    22,    23,    24,    25,    26,    27,    28,    29,    64,
    46,    30,    47,    48,    49,    50,    51,    52,    53,    54,
    74,    65,    66,    67,    68,    69,    56,    57,    58,    59,
    60,    61,    62,    63,    70,    71,    72,    75,    45
};

static const short yycheck[] = {     0,
     3,     4,     5,     6,     7,     8,     9,    10,    11,    12,
    13,    14,    15,    16,    17,    18,    19,    20,    21,    22,
    23,    24,    25,    26,    27,    28,    29,    30,    31,    33,
    32,    34,    32,    32,    32,    32,    32,    32,    32,    32,
     0,    33,    33,    33,    33,    33,    47,    48,    49,    50,
    51,    52,    53,    54,    33,    33,    33,     0,    32
};
/* -*-C-*-  Note some compilers choke on comments on `#line' lines.  */
#line 3 "/usr/share/bison/bison.simple"
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

#line 217 "/usr/share/bison/bison.simple"

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

case 5:
#line 147 "quote_fmt.y"
{
		INSERT(yyvsp[0].str);		
	;
    break;}
case 7:
#line 155 "quote_fmt.y"
{
		yyval.str = yytext;
	;
    break;}
case 8:
#line 161 "quote_fmt.y"
{
		if (msginfo->newsgroups)
			INSERT(msginfo->newsgroups);
	;
    break;}
case 9:
#line 166 "quote_fmt.y"
{
		if (msginfo->date)
			INSERT(msginfo->date);
	;
    break;}
case 10:
#line 171 "quote_fmt.y"
{
		if (msginfo->from)
			INSERT(msginfo->from);
	;
    break;}
case 11:
#line 176 "quote_fmt.y"
{
		if (msginfo->fromname)
			INSERT(msginfo->fromname);
	;
    break;}
case 12:
#line 181 "quote_fmt.y"
{
		if (msginfo->fromname) {
			gchar * p;
			gchar * str;

			str = alloca(strlen(msginfo->fromname) + 1);
			if (str != NULL) {
				strcpy(str, msginfo->fromname);
				p = str;
				while (*p && !isspace(*p)) p++;
				*p = '\0';
				INSERT(str);
			}
		}
	;
    break;}
case 13:
#line 197 "quote_fmt.y"
{
#define MAX_SENDER_INITIAL 20
		if (msginfo->fromname) {
			gchar tmp[MAX_SENDER_INITIAL];
			gchar * p;	
			gchar * cur;
			gint len = 0;

			p = msginfo->fromname;
			cur = tmp;
			while (*p) {
				if (*p && isalnum(*p)) {
					*cur = toupper(*p);
						cur ++;
					len ++;
					if (len >= MAX_SENDER_INITIAL - 1)
						break;
				}
				else
					break;
				while (*p && !isseparator(*p)) p++;
				while (*p && isseparator(*p)) p++;
			}
			*cur = '\0';
			INSERT(tmp);
		}
	;
    break;}
case 14:
#line 225 "quote_fmt.y"
{
		if (msginfo->subject)
			INSERT(msginfo->subject);
	;
    break;}
case 15:
#line 230 "quote_fmt.y"
{
		if (msginfo->to)
			INSERT(msginfo->to);
	;
    break;}
case 16:
#line 235 "quote_fmt.y"
{
		if (msginfo->msgid)
			INSERT(msginfo->msgid);
	;
    break;}
case 17:
#line 240 "quote_fmt.y"
{
		INSERT("%");
	;
    break;}
case 18:
#line 244 "quote_fmt.y"
{
		if (msginfo->cc)
			INSERT(msginfo->cc);
	;
    break;}
case 19:
#line 249 "quote_fmt.y"
{
		if (msginfo->references)
			INSERT(msginfo->references);
	;
    break;}
case 20:
#line 254 "quote_fmt.y"
{
		gchar buf[BUFFSIZE];
		FILE * fp;

		if ((fp = procmime_get_text_part(msginfo)) == NULL)
			g_warning(_("Can't get text part\n"));
		while (fgets(buf, sizeof(buf), fp) != NULL) {
			INSERT(buf);
		}
		fclose(fp);
	;
    break;}
case 21:
#line 266 "quote_fmt.y"
{
		gchar buf[BUFFSIZE];
		FILE * fp;

		if ((fp = procmime_get_text_part(msginfo)) == NULL)
			g_warning(_("Can't get text part\n"));
		while (fgets(buf, sizeof(buf), fp) != NULL) {
			if (quote_str)
				INSERT(quote_str);
			INSERT(buf);
		}
		fclose(fp);
	;
    break;}
case 22:
#line 280 "quote_fmt.y"
{
		INSERT("\\");
	;
    break;}
case 23:
#line 284 "quote_fmt.y"
{
		INSERT("\t");
	;
    break;}
case 24:
#line 288 "quote_fmt.y"
{
		INSERT("\n");
	;
    break;}
case 25:
#line 292 "quote_fmt.y"
{
		INSERT("?");
	;
    break;}
case 26:
#line 296 "quote_fmt.y"
{
		INSERT("(");
	;
    break;}
case 27:
#line 300 "quote_fmt.y"
{
		INSERT(")");
	;
    break;}
case 28:
#line 306 "quote_fmt.y"
{
		add_visibility(msginfo->date != NULL);
	;
    break;}
case 29:
#line 310 "quote_fmt.y"
{
		remove_visibility();
	;
    break;}
case 30:
#line 314 "quote_fmt.y"
{
		add_visibility(msginfo->from != NULL);
	;
    break;}
case 31:
#line 318 "quote_fmt.y"
{
		remove_visibility();
	;
    break;}
case 32:
#line 322 "quote_fmt.y"
{
		add_visibility(msginfo->fromname != NULL);
	;
    break;}
case 33:
#line 326 "quote_fmt.y"
{
		remove_visibility();
	;
    break;}
case 34:
#line 330 "quote_fmt.y"
{
		add_visibility(msginfo->subject != NULL);
	;
    break;}
case 35:
#line 334 "quote_fmt.y"
{
		remove_visibility();
	;
    break;}
case 36:
#line 338 "quote_fmt.y"
{
		add_visibility(msginfo->to != NULL);
	;
    break;}
case 37:
#line 342 "quote_fmt.y"
{
		remove_visibility();
	;
    break;}
case 38:
#line 346 "quote_fmt.y"
{
		add_visibility(msginfo->newsgroups != NULL);
	;
    break;}
case 39:
#line 350 "quote_fmt.y"
{
		remove_visibility();
	;
    break;}
case 40:
#line 354 "quote_fmt.y"
{
		add_visibility(msginfo->msgid != NULL);
	;
    break;}
case 41:
#line 358 "quote_fmt.y"
{
		remove_visibility();
	;
    break;}
case 42:
#line 362 "quote_fmt.y"
{
		add_visibility(msginfo->cc != NULL);
	;
    break;}
case 43:
#line 366 "quote_fmt.y"
{
		remove_visibility();
	;
    break;}
case 44:
#line 370 "quote_fmt.y"
{
		add_visibility(msginfo->references != NULL);
	;
    break;}
case 45:
#line 374 "quote_fmt.y"
{
		remove_visibility();
	;
    break;}
}
   /* the action file gets copied in in place of this dollarsign */
#line 543 "/usr/share/bison/bison.simple"

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
#line 377 "quote_fmt.y"
