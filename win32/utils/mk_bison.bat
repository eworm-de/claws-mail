@echo off
rem Create flex / bison files, remove absolute pathnames
rem Remember to set SRC and SRCESC when running from a different directory

set SRC=..\..\src
set DEST=%SRC%

set BISON_HAIRY=.\apps\bison.hairy
set BISON_SIMPLE=.\apps\bison.simple

echo flex: quote_fmt_lex.l
flex %SRC%\quote_fmt_lex.l
sed -e "/^#/ s,lex\.yy\.c,quote_fmt_lex.c," "lex.yy.c" > "%DEST%\quote_fmt_lex.c"

echo bison: quote_fmt_parse.y
bison -y -d %SRC%\quote_fmt_parse.y
sed -e "/^#/ s,y\.tab\.c,quote_fmt_parse.c," "y.tab.c" > "%DEST%\quote_fmt_parse.c"
sed -e "/^#/ s,y\.tab\.h,quote_fmt_parse.h," "y.tab.h" > "%DEST%\quote_fmt_parse.h"

echo flex: matcher_parser_lex.l
flex %SRC%\matcher_parser_lex.l
rem sed -e "/^#/ s,%SRCESC%,," "lex.yy.c" > "%DEST%\matcher_parser_lex.c"
sed -e "/^#/ s,lex\.yy\.c,matcher_parser_lex.c," "lex.yy.c" > "%DEST%\matcher_parser_lex.c"

echo bison: matcher_parser_parse.y
bison -y -d %SRC%\matcher_parser_parse.y
sed -e "/^#/ s,y\.tab\.c,matcher_parser_parse.c," "y.tab.c" > "%DEST%\matcher_parser_parse.c"
sed -e "/^#/ s,y\.tab\.h,matcher_parser_parse.h," "y.tab.h" > "%DEST%\matcher_parser_parse.h"

del lex.yy.c
del y.tab.c
del y.tab.h

set BISON_HAIRY=
set BISON_SIMPLE=

set SRC=
set DEST=
