#include "libkcc.h"

extern bool nogaiji;

/*---------------------------------------------------------------------
    NAME
	eucgaiji
 ---------------------------------------------------------------------*/
void Kcc_eucgaiji(ddd, c1, c2)
    register int c1, c2;
    char **ddd;
{
    if (nogaiji) {
	**ddd = ZENPAD >> 8 | 0x80; (*ddd)++;
	**ddd = (ZENPAD & 0xff) | 0x80; (*ddd)++;
    } else {
	**ddd = SS3; (*ddd)++;
	**ddd = c1 | 0x80; (*ddd)++;
	**ddd = c2 | 0x80; (*ddd)++;
    }
}

/*---------------------------------------------------------------------
    NAME
	euckana
 ---------------------------------------------------------------------*/
void Kcc_euckana(ddd, c)
    register int c;
    char **ddd;
{
    **ddd = SS2; (*ddd)++;
    **ddd = (!nogaiji || (0x20 < c && c < 0x60) ? c : HANPAD) | 0x80;
    (*ddd)++;
}

/*---------------------------------------------------------------------
    NAME
	euckanji
 ---------------------------------------------------------------------*/
void Kcc_euckanji(ddd, c1, c2)
    register int c1, c2;
    char **ddd;
{
    **ddd = c1 | 0x80; (*ddd)++;
    **ddd = c2 | 0x80; (*ddd)++;
}

