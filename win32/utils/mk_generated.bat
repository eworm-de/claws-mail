@echo off
rem create "generated.diff" in win32/patches subdir
rem for shipping bison/flex generated files, config.h, version.h
rem (you still have to create those files with cygwin or linux)

set GENERATED=generated.diff
set AUTOSRC=auto_src
rem set AUTOSRC=\cygwin\src\sylpheed-claws-w32\sylpheed-claws-head\sylpheed-claws\src
set SRC=..\..\src
set PATCHES=..\patches

deltree /y src
deltree /y empty
mkdir src
mkdir empty

copy %SRC%\config.h			src	> NUL
copy %AUTOSRC%\matcher_parser_lex.c	src	> NUL
copy %AUTOSRC%\matcher_parser_parse.c	src	> NUL
copy %AUTOSRC%\matcher_parser_parse.h	src	> NUL
copy %AUTOSRC%\quote_fmt_lex.c		src	> NUL
copy %AUTOSRC%\quote_fmt_parse.c	src	> NUL
copy %AUTOSRC%\quote_fmt_parse.h	src	> NUL

diff -urN empty src > %PATCHES%\%GENERATED%

deltree /y src
deltree /y empty

set GENERATED=
set AUTOSRC=
set SRC=
set PATCHES=
