#if 0
#if !defined lint
static char sccsid[] = "@(#)kcc.c 2.3 (Y.Tonooka) 7/1/94";
static char copyright[] = "@(#)Copyright (c) 1992 Yasuhiro Tonooka";
#endif
#endif

#define BINARY          0x100
#define ASCII           0x00

#define NONASCII        0x01    /* non-ASCII character */
#define JIS             0x02    /* JIS */
#define ESCI            0x04    /* "ESC(I" */
#define ASSUME          0x08    /* assumed EUC (or DEC) */
#define EUC             0x10
#define DEC             0x20
#define SJIS            0x40
#define JIS8            0x80    /* 8-bit JIS */
#define BIT8            (EUC | DEC | SJIS | JIS8)

extern int KCC_filter(char *ddd, char *outcode_name, char *sss, char *incode_name, int extend, int zenkaku, int gaiji);
extern unsigned KCC_check(char *s, int extend);
