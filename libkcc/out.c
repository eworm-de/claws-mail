#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "kcc.h"
#include "libkcc.h"

/**********************************************************************
 *                                                                    *
 *  Output Routines                                                   *
 *                                                                    *
 **********************************************************************/
extern void (*outascii)(), (*outkanji)(), (*outgaiji)(), (*outkana)();

/*---------------------------------------------------------------------
    NAME
	outsjis
 ---------------------------------------------------------------------*/
void Kcc_outsjis(ddd, c1, c2)
    register int c1, c2;
    char **ddd;
{
    register int c;

    c = c1 * 2 - (c1 <= 0x9f ? 0x00e1 : (c1 < 0xf0 ? 0x0161 : 0x01bf));
    if (c2 < 0x9f)
	c2 = c2 - (c2 > 0x7f ? 0x20 : 0x1f);
    else {
	c2 = c2 - 0x7e;
	c++;
    }
    (*(c1 <= 0xef ? outkanji : outgaiji))(ddd, c, c2);
}

/*---------------------------------------------------------------------
    NAME
	out
 ---------------------------------------------------------------------*/
unsigned Kcc_out(ddd,   str, len, code,   outcode, inmode, insi, inso, innj, ingj)
    char *str, **ddd;
    int len;
    register unsigned code;
    enum mode *inmode;
    unsigned long *insi, *inso, *innj, *ingj;
    unsigned outcode;
{
    register char *s;
    register int i;

    for (s = str; s < str + len; s += i) {
	i = 1;
	switch (*(u_char *) s) {
	case ESC:
	    if (*inmode == M_SO)
		break;
	    if (Kcc_compare("$B", s + 1) || Kcc_compare("$@", s + 1)) {
		*inmode = M_KANJI;	/* kanji */
		*insi |= bitflag(((u_char *) s)[2]);
		i = 3;
	    } else if (Kcc_compare("&@\033$B", s + 1)) {
		*inmode = M_KANJI;	/* kanji 1990 */
		*innj |= bitflag('B');
		i = 6;
	    } else if (Kcc_compare("(B", s + 1) || Kcc_compare("(J", s + 1) ||
		    Kcc_compare("(H", s + 1)) {
		*inmode = M_ASCII;	/* kanji end */
		*inso |= bitflag(((u_char *) s)[2]);
		i = 3;
	    } else if (Kcc_compare("(I", s + 1)) {
		*inmode = M_ESCI;	/* "ESC(I" */
		*inso |= bitflag('I');
		i = 3;
	    } else if (Kcc_compare("$(D", s + 1)) {
		*inmode = M_GAIJI;	/* gaiji */
		*ingj |= bitflag('D');
		i = 4;
	    } else
		break;
	    code |= JIS;
	    continue;
	case SO:
	    if (*inmode == M_ASCII) {
		code |= JIS;
		*inmode = M_SO;
		continue;
	    }
	    break;
	case SI:
	    if (*inmode == M_SO) {
		*inmode = M_ASCII;
		continue;
	    }
	    break;
	}
	if (*inmode != M_ASCII) {
	    if (0x20 < ((u_char *) s)[0] && ((u_char *) s)[0] < 0x7f)
		switch (*inmode) {
		case M_KANJI:
		    (*outkanji)(ddd, ((u_char *) s)[0], ((u_char *) s)[1] & 0x7f);
		    i = 2;
		    continue;
		case M_GAIJI:
		    (*outgaiji)(ddd, ((u_char *) s)[0], ((u_char *) s)[1] & 0x7f);
		    i = 2;
		    continue;
		case M_SO:
		case M_ESCI:
		    (*outkana)(ddd, ((u_char *) s)[0]);
		    continue;
		default:
		    continue;
		}
	} else if (((u_char *) s)[0] & 0x80) {
	    if (code & (EUC | DEC)) {
		/*
		 * EUC or DEC:
		 */
		if (0xa0 < ((u_char *) s)[0] &&
			((u_char *) s)[0] < 0xff) {
		    if (!(((u_char *) s)[1] & 0x80) && code & DEC) {
			/*
			 * DEC gaiji:
			 */
			code &= ~EUC;	/* definitely DEC  */
			(*outgaiji)(ddd, ((u_char *) s)[0] & 0x7f, ((u_char *) s)[1]);
		    } else
			/*
			 * EUC code set 1 (kanji), DEC kanji:
			 */
			(*outkanji)(ddd, ((u_char *) s)[0] & 0x7f, ((u_char *) s)[1] & 0x7f);
		} else if (((u_char *) s)[0] == SS2 && code & EUC &&
			0xa0 < ((u_char *) s)[1] &&
			((u_char *) s)[1] < 0xff) {
		    /*
		     * EUC code set 2 (hankaku kana):
		     */
		    code &= ~DEC;	/* probably EUC */
		    (*outkana)(ddd, ((u_char *) s)[1] & 0x7f);
		} else if (((u_char *) s)[0] == SS3 && code & EUC &&
			0xa0 < ((u_char *) s)[1] &&
			((u_char *) s)[1] < 0xff &&
			0xa0 < ((u_char *) s)[2] &&
			((u_char *) s)[2] < 0xff) {
		    /*
		     * EUC code set 3 (gaiji):
		     */
		    code &= ~DEC;	/* probably EUC */
		    (*outgaiji)(ddd, ((u_char *) s)[1] & 0x7f, ((u_char *) s)[2] & 0x7f);
		    i = 3;
		    continue;
		} else {
		    /*
		     * Control character (C1):
		     */
		    if (outcode != SJIS && (outcode != EUC || 
			    (((u_char *) s)[0] != SS2 &&
			    ((u_char *) s)[0] != SS3)))
			**ddd = ((u_char *) s)[0]; (*ddd)++;
		    continue;
		}
		i = 2;
		continue;
	    } else if (code & (SJIS | JIS8)) {
		/*
		 * Shift-JIS or JIS8:
		 */
		if (!(code & SJIS) || (0xa0 < ((u_char *) s)[0] &&
			((u_char *) s)[0] < 0xe0))
		    /*
		     * Hankaku kana:
		     */
		    (*outkana)(ddd, ((u_char *) s)[0] & 0x7f);
		else {
		    /*
		     * Shift-JIS kanji:
		     */
		    code &= ~JIS8;	/* definitely shift-JIS */
		    Kcc_outsjis(ddd, ((u_char *) s)[0], ((u_char *) s)[1]);
		    i = 2;
		}
		continue;
	    }
	}
	(*outascii)(ddd, ((u_char *) s)[0]);
    }
    return (code);
}
