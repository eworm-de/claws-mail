%{
#include <string.h>
#include <glib.h>

#include "matcher_parser_lex.h"
#include "matcher_parser_parse.h"

#define MAX_STR_CONST 512
#define YY_NO_UNPUT 1

char string_buf[MAX_STR_CONST];
char * string_buf_ptr;

static void add_char(char ch)
{
	if (string_buf_ptr - string_buf < sizeof(string_buf))
		*string_buf_ptr++ = ch;
}


/* this function will reinitializes the parser */

void matcher_parser_init(void)
{
        BEGIN(0);
}
%}

%option prefix="matcher_parser"
%option outfile="lex.yy.c"
%option yylineno

%x string
%x section

%%
			
			/*
			 * a keyword consists of alpha and underscore 
			 * characters, possibly preceded by a tilde (~)
			 */

(~|[a-z])[a-z_]*	{
				gint id;

				if (-1 == (id = get_matchparser_tab_id(yytext))) { 
					REJECT;
				} else					
					return id;
			}
[ \t]+
"\n"		return MATCHER_EOL;
"&"		return MATCHER_AND;
"|"		return MATCHER_OR;
\"		{
		BEGIN(string);
		string_buf_ptr = string_buf;
		}
		/* alfons - OK, the new attempt is to just swallow 
		 * *EVERYTHING* and make sure everything is escaped
		 * when actually performing things. */
<string>\\\"	{
		/* take care of escaped \" because this means the
		 * quote char should be skipped */
		add_char('\\');
		add_char('\"');
		}
<string>\"	{
		/* get out of the state: string ends. */
		BEGIN(0);
		*string_buf_ptr = '\0';
		yylval.str = string_buf;
		return MATCHER_STRING;
		}
		/* put everything else in the output. */
<string>.	{
		add_char(yytext[0]);
		}
\[[^\[\]]*\]	{
		BEGIN(0);
		yylval.str = yytext + 1;
		yytext[strlen(yytext) - 1] = '\0';
		return MATCHER_SECTION;
		}
[-+]?[0-9]+	{
		yylval.str = yytext;
		return MATCHER_INTEGER;
		}

%%
