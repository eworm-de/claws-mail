/* A Bison parser, made by GNU Bison 3.0.2.  */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2013 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

#ifndef YY_ICAL_YY_ICALYACC_H_INCLUDED
# define YY_ICAL_YY_ICALYACC_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 1
#endif
#if YYDEBUG
extern int ical_yydebug;
#endif

/* Token type.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    DIGITS = 258,
    INTNUMBER = 259,
    FLOATNUMBER = 260,
    STRING = 261,
    EOL = 262,
    EQUALS = 263,
    CHARACTER = 264,
    COLON = 265,
    COMMA = 266,
    SEMICOLON = 267,
    MINUS = 268,
    TIMESEPERATOR = 269,
    TRUE = 270,
    FALSE = 271,
    FREQ = 272,
    BYDAY = 273,
    BYHOUR = 274,
    BYMINUTE = 275,
    BYMONTH = 276,
    BYMONTHDAY = 277,
    BYSECOND = 278,
    BYSETPOS = 279,
    BYWEEKNO = 280,
    BYYEARDAY = 281,
    DAILY = 282,
    MINUTELY = 283,
    MONTHLY = 284,
    SECONDLY = 285,
    WEEKLY = 286,
    HOURLY = 287,
    YEARLY = 288,
    INTERVAL = 289,
    COUNT = 290,
    UNTIL = 291,
    WKST = 292,
    MO = 293,
    SA = 294,
    SU = 295,
    TU = 296,
    WE = 297,
    TH = 298,
    FR = 299,
    BIT8 = 300,
    ACCEPTED = 301,
    ADD = 302,
    AUDIO = 303,
    BASE64 = 304,
    BINARY = 305,
    BOOLEAN = 306,
    BUSY = 307,
    BUSYTENTATIVE = 308,
    BUSYUNAVAILABLE = 309,
    CALADDRESS = 310,
    CANCEL = 311,
    CANCELLED = 312,
    CHAIR = 313,
    CHILD = 314,
    COMPLETED = 315,
    CONFIDENTIAL = 316,
    CONFIRMED = 317,
    COUNTER = 318,
    DATE = 319,
    DATETIME = 320,
    DECLINECOUNTER = 321,
    DECLINED = 322,
    DELEGATED = 323,
    DISPLAY = 324,
    DRAFT = 325,
    DURATION = 326,
    EMAIL = 327,
    END = 328,
    FINAL = 329,
    FLOAT = 330,
    FREE = 331,
    GREGORIAN = 332,
    GROUP = 333,
    INDIVIDUAL = 334,
    INPROCESS = 335,
    INTEGER = 336,
    NEEDSACTION = 337,
    NONPARTICIPANT = 338,
    OPAQUE = 339,
    OPTPARTICIPANT = 340,
    PARENT = 341,
    PERIOD = 342,
    PRIVATE = 343,
    PROCEDURE = 344,
    PUBLIC = 345,
    PUBLISH = 346,
    RECUR = 347,
    REFRESH = 348,
    REPLY = 349,
    REQPARTICIPANT = 350,
    REQUEST = 351,
    RESOURCE = 352,
    ROOM = 353,
    SIBLING = 354,
    START = 355,
    TENTATIVE = 356,
    TEXT = 357,
    THISANDFUTURE = 358,
    THISANDPRIOR = 359,
    TIME = 360,
    TRANSPAENT = 361,
    UNKNOWN = 362,
    UTCOFFSET = 363,
    XNAME = 364,
    ALTREP = 365,
    CN = 366,
    CUTYPE = 367,
    DAYLIGHT = 368,
    DIR = 369,
    ENCODING = 370,
    EVENT = 371,
    FBTYPE = 372,
    FMTTYPE = 373,
    LANGUAGE = 374,
    MEMBER = 375,
    PARTSTAT = 376,
    RANGE = 377,
    RELATED = 378,
    RELTYPE = 379,
    ROLE = 380,
    RSVP = 381,
    SENTBY = 382,
    STANDARD = 383,
    URI = 384,
    CHARSET = 385,
    TIME_CHAR = 386,
    UTC_CHAR = 387
  };
#endif
/* Tokens.  */
#define DIGITS 258
#define INTNUMBER 259
#define FLOATNUMBER 260
#define STRING 261
#define EOL 262
#define EQUALS 263
#define CHARACTER 264
#define COLON 265
#define COMMA 266
#define SEMICOLON 267
#define MINUS 268
#define TIMESEPERATOR 269
#define TRUE 270
#define FALSE 271
#define FREQ 272
#define BYDAY 273
#define BYHOUR 274
#define BYMINUTE 275
#define BYMONTH 276
#define BYMONTHDAY 277
#define BYSECOND 278
#define BYSETPOS 279
#define BYWEEKNO 280
#define BYYEARDAY 281
#define DAILY 282
#define MINUTELY 283
#define MONTHLY 284
#define SECONDLY 285
#define WEEKLY 286
#define HOURLY 287
#define YEARLY 288
#define INTERVAL 289
#define COUNT 290
#define UNTIL 291
#define WKST 292
#define MO 293
#define SA 294
#define SU 295
#define TU 296
#define WE 297
#define TH 298
#define FR 299
#define BIT8 300
#define ACCEPTED 301
#define ADD 302
#define AUDIO 303
#define BASE64 304
#define BINARY 305
#define BOOLEAN 306
#define BUSY 307
#define BUSYTENTATIVE 308
#define BUSYUNAVAILABLE 309
#define CALADDRESS 310
#define CANCEL 311
#define CANCELLED 312
#define CHAIR 313
#define CHILD 314
#define COMPLETED 315
#define CONFIDENTIAL 316
#define CONFIRMED 317
#define COUNTER 318
#define DATE 319
#define DATETIME 320
#define DECLINECOUNTER 321
#define DECLINED 322
#define DELEGATED 323
#define DISPLAY 324
#define DRAFT 325
#define DURATION 326
#define EMAIL 327
#define END 328
#define FINAL 329
#define FLOAT 330
#define FREE 331
#define GREGORIAN 332
#define GROUP 333
#define INDIVIDUAL 334
#define INPROCESS 335
#define INTEGER 336
#define NEEDSACTION 337
#define NONPARTICIPANT 338
#define OPAQUE 339
#define OPTPARTICIPANT 340
#define PARENT 341
#define PERIOD 342
#define PRIVATE 343
#define PROCEDURE 344
#define PUBLIC 345
#define PUBLISH 346
#define RECUR 347
#define REFRESH 348
#define REPLY 349
#define REQPARTICIPANT 350
#define REQUEST 351
#define RESOURCE 352
#define ROOM 353
#define SIBLING 354
#define START 355
#define TENTATIVE 356
#define TEXT 357
#define THISANDFUTURE 358
#define THISANDPRIOR 359
#define TIME 360
#define TRANSPAENT 361
#define UNKNOWN 362
#define UTCOFFSET 363
#define XNAME 364
#define ALTREP 365
#define CN 366
#define CUTYPE 367
#define DAYLIGHT 368
#define DIR 369
#define ENCODING 370
#define EVENT 371
#define FBTYPE 372
#define FMTTYPE 373
#define LANGUAGE 374
#define MEMBER 375
#define PARTSTAT 376
#define RANGE 377
#define RELATED 378
#define RELTYPE 379
#define ROLE 380
#define RSVP 381
#define SENTBY 382
#define STANDARD 383
#define URI 384
#define CHARSET 385
#define TIME_CHAR 386
#define UTC_CHAR 387

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE YYSTYPE;
union YYSTYPE
{
#line 71 "icalyacc.y" /* yacc.c:1909  */

	float v_float;
	int   v_int;
	char* v_string;

#line 324 "icalyacc.h" /* yacc.c:1909  */
};
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE ical_yylval;

int ical_yyparse (void);

#endif /* !YY_ICAL_YY_ICALYACC_H_INCLUDED  */
