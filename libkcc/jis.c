#include "libkcc.h"

#include <string.h>

extern enum mode outmode;
extern char shiftout[], shiftin[];
extern bool nogaiji;
/*---------------------------------------------------------------------
    NAME
	jisascii
 ---------------------------------------------------------------------*/
void Kcc_jisascii(ddd, c)
    register int c;
    char **ddd;
{
  int i;

    switch (outmode) {
    case M_ASCII:
	break;
    case M_SO:
	**ddd = SI; (*ddd)++;
	outmode = M_ASCII;
	break;
    default:
        for (i=0; i< strlen(shiftout); i++) { **ddd = shiftout[i]; (*ddd)++; }
	outmode = M_ASCII;
	break;
    }
    **ddd = c; (*ddd)++;
}

/*---------------------------------------------------------------------
    NAME
	jisgaiji
 ---------------------------------------------------------------------*/
void Kcc_jisgaiji(ddd, c1, c2)
    register int c1, c2;
    char **ddd;
{

    if (nogaiji)
	Kcc_jiskanji(ddd, ZENPAD >> 8, ZENPAD & 0xff);
    else {
	if (outmode != M_GAIJI) {
	  if (outmode == M_SO)  { **ddd = SI; (*ddd)++; }
	  **ddd = '\033'; (*ddd)++;
	  **ddd = '$'; (*ddd)++;
	  **ddd = '('; (*ddd)++;
	  **ddd = 'D'; (*ddd)++;
	  outmode = M_GAIJI;
	}
	**ddd = c1;
	**ddd = c2;
    }
}

/*---------------------------------------------------------------------
    NAME
	jiskana
 ---------------------------------------------------------------------*/
void Kcc_jiskana(ddd, c)
    register int c;
    char **ddd;
{
  int i;

    if (outmode != M_SO) {
      if (outmode != M_ASCII) {
	for (i=0; i< strlen(shiftout); i++) {**ddd = shiftout[i]; (*ddd)++; }
      }
      **ddd = SO; (*ddd)++;
      outmode = M_SO;
    }
    **ddd = !nogaiji || (0x20 < c && c < 0x60) ? c : HANPAD; (*ddd)++;
}

/*---------------------------------------------------------------------
    NAME
	jiskana8
 ---------------------------------------------------------------------*/
void Kcc_jiskana8(ddd, c)
    register int c;
    char **ddd;
{
  int i;

    if (outmode != M_ASCII) {
	for (i=0; i< strlen(shiftout); i++) {**ddd = shiftout[i]; (*ddd)++; }
	outmode = M_ASCII;
    }
    **ddd = (!nogaiji || (0x20 < c && c < 0x60) ? c : HANPAD) | 0x80; (*ddd)++;
}

/*---------------------------------------------------------------------
    NAME
	jiskanak
 ---------------------------------------------------------------------*/
void Kcc_jiskanak(ddd, c)
    register int c;
    char **ddd;
{

    if (outmode != M_ESCI) {
	**ddd = '\033'; (*ddd)++;
	**ddd = '('; (*ddd)++;
	**ddd = 'I'; (*ddd)++;
	outmode = M_ESCI;
    }
    **ddd = !nogaiji || (0x20 < c && c < 0x60) ? c : HANPAD; (*ddd)++;
}

/*---------------------------------------------------------------------
    NAME
	jiskanji
 ---------------------------------------------------------------------*/
void Kcc_jiskanji(ddd, c1, c2)
    register int c1, c2;
    char **ddd;
{
  int i;

    if (outmode != M_KANJI) {
        if (outmode == M_SO)
	  { **ddd = SI; (*ddd)++; }
	for (i=0; i< strlen(shiftin); i++) {**ddd = shiftin[i]; (*ddd)++; }
	outmode = M_KANJI;
    }
    **ddd = c1; (*ddd)++;
    **ddd = c2; (*ddd)++;
}

