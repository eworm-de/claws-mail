%{
#include "matcher_parser_lex.h"
#include "matcher_parser_parse.h"
#include <glib.h>

#define MAX_STR_CONST 512

char string_buf[MAX_STR_CONST];
char * string_buf_ptr;

static void add_char(char ch)
{
	if (string_buf_ptr - string_buf < sizeof(string_buf))
		*string_buf_ptr++ = ch;
}
%}

%option prefix="matcher_parser"
%option outfile="lex.yy.c"
%option yylineno

%x string
%x section

%%

"all"		return MATCHER_ALL;
"unread"	return MATCHER_UNREAD;
"~unread"	return MATCHER_NOT_UNREAD;
"new"		return MATCHER_NEW;
"~new"		return MATCHER_NOT_NEW;
"marked"	return MATCHER_MARKED;
"~marked"	return MATCHER_NOT_MARKED;
"deleted"	return MATCHER_DELETED;
"~deleted"	return MATCHER_NOT_DELETED;
"replied"	return MATCHER_REPLIED;
"~replied"	return MATCHER_NOT_REPLIED;
"forwarded"	return MATCHER_FORWARDED;
"~forwarded"	return MATCHER_NOT_FORWARDED;
"subject"	return MATCHER_SUBJECT;
"~subject"	return MATCHER_NOT_SUBJECT;
"from"		return MATCHER_FROM;
"~from"		return MATCHER_NOT_FROM;
"to"		return MATCHER_TO;
"~to"		return MATCHER_NOT_TO;
"cc"		return MATCHER_CC;
"~cc"		return MATCHER_NOT_CC;
"to_or_cc"	return MATCHER_TO_OR_CC;
"~to_or_cc"	return MATCHER_NOT_TO_AND_NOT_CC;
"age_greater"	return MATCHER_AGE_GREATER;
"age_lower"	return MATCHER_AGE_LOWER;
"newsgroups"	return MATCHER_NEWSGROUPS;
"~newsgroups"	return MATCHER_NOT_NEWSGROUPS;
"inreplyto"	return MATCHER_INREPLYTO;
"~inreplyto"	return MATCHER_NOT_INREPLYTO;
"references"	return MATCHER_REFERENCES;
"~references"	return MATCHER_NOT_REFERENCES;
"score_greater"	return MATCHER_SCORE_GREATER;
"score_lower"	return MATCHER_SCORE_LOWER;
"score_equal"	return MATCHER_SCORE_EQUAL;
"size_greater"  return MATCHER_SIZE_GREATER;
"size_smaller"  return MATCHER_SIZE_SMALLER;
"size_equal"    return MATCHER_SIZE_EQUAL;
"header"	return MATCHER_HEADER;
"~header"	return MATCHER_NOT_HEADER;
"headers_part"	return MATCHER_HEADERS_PART;
"~headers_part"	return MATCHER_NOT_HEADERS_PART;
"message"	return MATCHER_MESSAGE;
"~message"	return MATCHER_NOT_MESSAGE;
"body_part"	return MATCHER_BODY_PART;
"~body_part"	return MATCHER_NOT_BODY_PART;
"execute"	return MATCHER_EXECUTE;
"~execute"	return MATCHER_NOT_EXECUTE;
"matchcase"	return MATCHER_MATCHCASE;
"match"		return MATCHER_MATCH;
"regexpcase"	return MATCHER_REGEXPCASE;
"regexp"	return MATCHER_REGEXP;
"score"		return MATCHER_SCORE;
"move"		return MATCHER_MOVE;
"copy"		return MATCHER_COPY;
"delete"	return MATCHER_DELETE;
"mark"		return MATCHER_MARK;
"unmark"	return MATCHER_UNMARK;
"mark_as_read"	return MATCHER_MARK_AS_READ;
"mark_as_unread"       return MATCHER_MARK_AS_UNREAD;
"forward"	       return MATCHER_FORWARD;
"forward_as_attachment"	      return MATCHER_FORWARD_AS_ATTACHMENT;
"color"         return MATCHER_COLOR;
"bounce"	return MATCHER_BOUNCE;
"delete_on_server"	return MATCHER_DELETE_ON_SERVER;
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
