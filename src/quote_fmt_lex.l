%option nounput never-interactive

%{
/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (c) 1999-2007 by Hiroyuki Yamamoto & The Claws Mail Team
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

#include "quote_fmt_lex.h"
#include "quote_fmt_parse.h"

%}

%option prefix="quote_fmt"
%option outfile="lex.yy.c"

%s S_NORMAL S_DATE

%{
/*
 * see notes below.
 */
int quote_fmt_firsttime = 1;
int line = -1;
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
<S_NORMAL>"%A" /* email address */ return SHOW_MAIL_ADDRESS;
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
<S_NORMAL>"%T" /* current dictionary */ return SHOW_DICT;
<S_NORMAL>"%Q" /* quoted message */ return SHOW_QUOTED_MESSAGE;
<S_NORMAL>"%q" /* quoted message with no signature */ return SHOW_QUOTED_MESSAGE_NO_SIGNATURE;
<S_NORMAL>"%af" /* full name in compose account */ return SHOW_ACCOUNT_FULL_NAME;
<S_NORMAL>"%am" /* mail address in compose account */ return SHOW_ACCOUNT_MAIL_ADDRESS;
<S_NORMAL>"%an" /* compose account name itself */ return SHOW_ACCOUNT_NAME;
<S_NORMAL>"%ao" /* organization in compose account */ return SHOW_ACCOUNT_ORGANIZATION;
<S_NORMAL>"%aT" /* main dict (if enabled) in compose account */ return SHOW_ACCOUNT_DICT;
<S_NORMAL>"%ABc" /* completion of 'Cc' from the address book */ return SHOW_ADDRESSBOOK_COMPLETION_FOR_CC;
<S_NORMAL>"%ABf" /* completion of 'From' from the address book */ return SHOW_ADDRESSBOOK_COMPLETION_FOR_FROM;
<S_NORMAL>"%ABt" /* completion of 'To' from the address book */ return SHOW_ADDRESSBOOK_COMPLETION_FOR_TO;
"\\\%" /* % */ return SHOW_PERCENT;
"\\\\" /* \ */ return SHOW_BACKSLASH;
"\\t"|"\t" /* tab */ return SHOW_TAB;
"\\n"|"\n" /* return */ { if (yytext[0] == '\n') line++; return SHOW_EOL; }
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
"?T" /* query current dictionary set and enabled */ return QUERY_DICT;
"?c" /* query cc */ return QUERY_CC;
"?n" /* query newsgroups */ return QUERY_NEWSGROUPS;
"?i" /* query message-id */ return QUERY_MESSAGEID;
"?r" /* query references */ return QUERY_REFERENCES;
"?af" /* query full name in compose account */ return QUERY_ACCOUNT_FULL_NAME;
"?ao" /* query organization in compose account */ return QUERY_ACCOUNT_ORGANIZATION;
"?aT" /* query account main dict enabled */ return QUERY_ACCOUNT_DICT;
"?ABc" /* query completion for 'Cc' in address book */ return QUERY_CC_FOUND_IN_ADDRESSBOOK;
"?ABf" /* query completion for 'From' in address book */ return QUERY_FROM_FOUND_IN_ADDRESSBOOK;
"?ABt" /* query completion for 'To' in address book */ return QUERY_TO_FOUND_IN_ADDRESSBOOK;
"|f" /* insert file */ return INSERT_FILE;
"|p" /* insert program output */ return INSERT_PROGRAMOUTPUT;
"|i" /* insert user input */ return INSERT_USERINPUT;
"!d" /* query date */ return QUERY_NOT_DATE;
"!f" /* query from */ return QUERY_NOT_FROM;
"!N"|"!F"|"!L"|"!I" /* query not(from name) */ return QUERY_NOT_FULLNAME;
"!s" /* query not(subject) */ return QUERY_NOT_SUBJECT;
"!t" /* query not(to) */ return QUERY_NOT_TO;
"!T" /* query not(current dictionary set and enabled) */ return QUERY_NOT_DICT;
"!c" /* query not(cc) */ return QUERY_NOT_CC;
"!n" /* query not(newsgroups) */ return QUERY_NOT_NEWSGROUPS;
"!i" /* query not(message-id) */ return QUERY_NOT_MESSAGEID;
"!r" /* query not(references) */ return QUERY_NOT_REFERENCES;
"!af" /* query not(full name in compose account) */ return QUERY_NOT_ACCOUNT_FULL_NAME;
"!ao" /* query not(organization in compose account) */ return QUERY_NOT_ACCOUNT_ORGANIZATION;
"!aT" /* query not(account main dict enabled and set) */ return QUERY_NOT_ACCOUNT_DICT;
"!ABc" /* query not(completion for 'Cc' in address book) */ return QUERY_NOT_CC_FOUND_IN_ADDRESSBOOK;
"!ABf" /* query not(completion for 'From' in address book) */ return QUERY_NOT_FROM_FOUND_IN_ADDRESSBOOK;
"!ABt" /* query not(completion for 'To' in address book) */ return QUERY_NOT_TO_FOUND_IN_ADDRESSBOOK;
<S_DATE>"{" return OPARENT;
<S_DATE>"}" { BEGIN S_NORMAL; return CPARENT; }
<S_NORMAL>"{" return OPARENT;
<S_NORMAL>"}" return CPARENT;
. { yylval.chr = yytext[0]; return CHARACTER; }

%%
