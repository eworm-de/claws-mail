@echo off
rem create "generated.diff" in win32/patches subdir
rem for shipping bison/flex generated files, config.h, version.h
rem (you still have to create those files with cygwin or linux)

set GENERATED=generated.diff
set AUTOSRC=auto_src
rem set AUTOSRC=..\..\src

deltree /y src
deltree /y empty
mkdir src
mkdir empty

copy %AUTOSRC%\matcher_parser_lex.c	src	> NUL
copy %AUTOSRC%\matcher_parser_parse.c	src	> NUL
copy %AUTOSRC%\matcher_parser_parse.h	src	> NUL
copy %AUTOSRC%\quote_fmt_lex.c		src	> NUL
copy %AUTOSRC%\quote_fmt_parse.c	src	> NUL
copy %AUTOSRC%\quote_fmt_parse.h	src	> NUL
copy %AUTOSRC%\config.h			src	> NUL
copy %AUTOSRC%\version.h		src	> NUL

diff -urN empty src > %GENERATED%
copy %GENERATED% ..\patches

deltree /y src
deltree /y empty
