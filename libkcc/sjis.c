#include "libkcc.h"

void Kcc_sjiskanji(char **ddd, register int c1, register int c2);

extern bool nogaiji;

/*---------------------------------------------------------------------
    NAME
	sjisgaiji
    DESCRIPTION
	Characters are mapped as follows:
	    0x2121 to 0x3a7e --> 0xf040 to 0xfcfc
	    0x3b21 to 0x7e7e --> 0xfcfc
 ---------------------------------------------------------------------*/
void Kcc_sjisgaiji(ddd, c1, c2)
    register int c1, c2;
    char **ddd;
{
    if (nogaiji)
	Kcc_sjiskanji(ddd, ZENPAD >> 8, ZENPAD & 0xff);
    else {
      **ddd = c1 < 0x3b ? ((c1 - 1) >> 1) + 0xe0 : 0xfc; (*ddd)++;
      **ddd = c1 < 0x3b ? c2 +
		(c1 & 1 ? (c2 < 0x60 ? 0x1f : 0x20) : 0x7e) : 0xfc; (*ddd)++;
    }
}

/*---------------------------------------------------------------------
    NAME
	sjiskana
 ---------------------------------------------------------------------*/
void Kcc_sjiskana(ddd, c)
    register int c;
    char **ddd;
{
    **ddd = 0x20 < c && c < 0x60 ? c | 0x80 : HANPAD | 0x80; (*ddd)++;
}

/*---------------------------------------------------------------------
    NAME
	sjiskanji
 ---------------------------------------------------------------------*/
void Kcc_sjiskanji(ddd, c1, c2)
    register int c1, c2;
    char **ddd;
{
    **ddd = ((c1 - 1) >> 1) + (c1 <= 0x5e ? 0x71 : 0xb1); (*ddd)++;
    **ddd = c2 + (c1 & 1 ? (c2 < 0x60 ? 0x1f : 0x20) : 0x7e); (*ddd)++;
}

