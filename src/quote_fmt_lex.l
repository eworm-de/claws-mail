%{
#include "quote_fmt_lex.h"
#include "quote_fmt_parse.h"

#define YY_NO_UNPUT 1
%}

%option prefix="quote_fmt"
%option outfile="lex.yy.c"

%s S_NORMAL S_DATE

%{
/*
 * see notes below.
 */
int quote_fmt_firsttime = 1;
%}

%%

%{
/*
 * NOTES:
 * this lex script used to use characters also in use
 * by strftime() (which we want to use for custom
 * time formats in replies and templates). to circumvent
 * this we have to play a little bit with states.
 *
 * these are the characters we also want to use in the
 * %D time customizer:
 *
 * %a %A %b %B %c %C %d %H %I %j %m %M %p %S %w %x %y %Y %Z
 *
 * you can use these characters too, but don't forget to
 * prepend them with the <S_NORMAL> state.
 *
 * also there is also work around for resetting the state
 * (firsttime variable). this assumes that yylex() will
 * always return to S_NORMAL after quote fmt parsing is
 * done.
 */
%} 

%{
	if (quote_fmt_firsttime) {
		BEGIN S_NORMAL;
		quote_fmt_firsttime = 0;
	}	
%}

<S_NORMAL>"%X" /* cursor pos */ return SET_CURSOR_POS;
<S_NORMAL>"%c" /* cc */ return SHOW_CC;
<S_NORMAL>"%d" /* date */ return SHOW_DATE;
<S_NORMAL>"%D" /* date */ { BEGIN S_DATE; return SHOW_DATE_EXPR; }
<S_NORMAL>"%f" /* from */ return SHOW_FROM;
<S_NORMAL>"%F" /* first name */ return SHOW_FIRST_NAME;
<S_NORMAL>"%i" /* message-id */ return SHOW_MESSAGEID;
<S_NORMAL>"%I" /* initial of sender */ return SHOW_SENDER_INITIAL;
<S_NORMAL>"%m" /* message with no signature */ return SHOW_MESSAGE_NO_SIGNATURE;
<S_NORMAL>"%M" /* message */ return SHOW_MESSAGE;
<S_NORMAL>"%n" /* newsgroups */ return SHOW_NEWSGROUPS;
<S_NORMAL>"%N" /* full name */ return SHOW_FULLNAME;
<S_NORMAL>"%L" /* last name */ return SHOW_LAST_NAME;
<S_NORMAL>"%r" /* references */ return SHOW_REFERENCES;
<S_NORMAL>"%s" /* subject */ return SHOW_SUBJECT;
<S_NORMAL>"%t" /* to */ return SHOW_TO;
<S_NORMAL>"%Q" /* quoted message */ return SHOW_QUOTED_MESSAGE;
<S_NORMAL>"%q" /* quoted message with no signature */ return SHOW_QUOTED_MESSAGE_NO_SIGNATURE;
"\\\%" /* % */ return SHOW_PERCENT;
"\\\\" /* \ */ return SHOW_BACKSLASH;
"\\t"|"\t" /* tab */ return SHOW_TAB;
"\\n"|"\n" /* return */ return SHOW_EOL;
"\\?" /* ? */ return SHOW_QUESTION_MARK;
"\\!" return SHOW_EXCLAMATION_MARK;
"\\|" return SHOW_PIPE;
"\\{" return SHOW_OPARENT;
"\\}" return SHOW_CPARENT;
"?d" /* query date */ return QUERY_DATE;
"?f" /* query from */ return QUERY_FROM;
"?N"|"?F"|"?L"|"?I" /* query from name */ return QUERY_FULLNAME;
"?s" /* query subject */ return QUERY_SUBJECT;
"?t" /* query to */ return QUERY_TO;
"?c" /* query cc */ return QUERY_CC;
"?n" /* query newsgroups */ return QUERY_NEWSGROUPS;
"?i" /* query message-id */ return QUERY_MESSAGEID;
"?r" /* query references */ return QUERY_REFERENCES;
"|f" /* insert file */ return INSERT_FILE;
"|p" /* insert program output */ return INSERT_PROGRAMOUTPUT;
"!d" /* query date */ return QUERY_NOT_DATE;
"!f" /* query from */ return QUERY_NOT_FROM;
"!N"|"!F"|"!L"|"!I" /* query not(from name) */ return QUERY_NOT_FULLNAME;
"!s" /* query not(subject) */ return QUERY_NOT_SUBJECT;
"!t" /* query not(to) */ return QUERY_NOT_TO;
"!c" /* query not(cc) */ return QUERY_NOT_CC;
"!n" /* query not(newsgroups) */ return QUERY_NOT_NEWSGROUPS;
"!i" /* query not(message-id) */ return QUERY_NOT_MESSAGEID;
"!r" /* query not(references) */ return QUERY_NOT_REFERENCES;
<S_DATE>"{" return OPARENT;
<S_DATE>"}" { BEGIN S_NORMAL; return CPARENT; }
<S_NORMAL>"{" return OPARENT;
<S_NORMAL>"}" return CPARENT;
. { yylval.chr = yytext[0]; return CHARACTER; }

%%
