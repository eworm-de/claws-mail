#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "kcc.h"
#include "libkcc.h"

/**********************************************************************
 *                                                                    *
 *  Guessing                                                          *
 *                                                                    *
 **********************************************************************/
/*---------------------------------------------------------------------
    NAME
	guess - distinguish code system
 ---------------------------------------------------------------------*/
unsigned Kcc_guess(str, len, extend, zenkaku, gsmode, insi, inso, innj, ingj)
    char *str;
    int len, extend;
    enum mode *gsmode;
   unsigned long *insi, *inso, *innj, *ingj;
   bool zenkaku;
{
    register char *s;
    register int euc, sjis, dec;
    bool jis8;
    register unsigned code;
    register int i;
    enum mode old;

    euc = sjis = 1;
    dec = extend ? 1 : 0;
    jis8 = 1;
    code = 0;
    for (s = str; s < str + len; s += i) {
	i = 1;
	switch (*(u_char *) s) {
	case ESC:
	    if (*gsmode == M_SO)
		continue;
	    old = *gsmode;
	    if (Kcc_compare("$B", s + 1) || Kcc_compare("$@", s + 1)) {
		*gsmode = M_KANJI;	/* kanji */
		*insi |= bitflag(((u_char *) s)[2]);
		i = 3;
	    } else if (Kcc_compare("&@\033$B", s + 1)) {
		*gsmode = M_KANJI;	/* kanji 1990 */
		*innj |= bitflag('B');
		i = 6;
	    } else if (Kcc_compare("(B", s + 1) ||
		    Kcc_compare("(J", s + 1) || Kcc_compare("(H", s + 1)) {
		*gsmode = M_ASCII;	/* kanji end */
		*inso |= bitflag(((u_char *) s)[2]);
		i = 3;
	    } else if (Kcc_compare("(I", s + 1)) {
		*gsmode = M_KANJI;	/* "ESC(I" */
		*inso |= bitflag('I');
		i = 3;
	    } else if (Kcc_compare("$(D", s + 1)) {
		*gsmode = M_KANJI;	/* gaiji */
		*ingj |= bitflag('D');
		i = 4;
	    } else
		break;
	    code |= JIS;
	    if (old != M_ASCII)
		continue;
	    break;
	case SO:
	    if (*gsmode == M_ASCII) {
		code |= JIS;
		*gsmode = M_SO;
		break;
	    }
	    continue;
	case SI:
	    if (*gsmode == M_SO) {
		*gsmode = M_ASCII;
		continue;
	    }
	    /* fall thru */
	default:
	    if (*gsmode != M_ASCII)
		continue;
	    break;
	}
	if (*(u_char *) s & 0x80)
	    code |= NONASCII;
	switch (euc) {
	case 1:
	    /*
	     * EUC first byte.
	     */
	    if (*(u_char *) s & 0x80) {
		if ((0xa0 < *(u_char *) s && *(u_char *) s < 0xff) ||
			(!zenkaku && *(u_char *) s == SS2)) {
		    euc = 2;
		    break;
		}
		if (extend) {
		    if (*(u_char *) s == SS3) {
			euc = 2;
			break;
		    } else if (*(u_char *) s < 0xa0)
			break;
		}
		euc = 0;	/* not EUC */
	    }
	    break;
	case 2:
	    /*
	     * EUC second byte or third byte of CS3.
	     */
	    if (((u_char *) s)[-1] == SS2) {
		if (0xa0 < *(u_char *) s &&
			*(u_char *) s < (extend ? 0xff : 0xe0)) {
		    euc = 1;	/* hankaku kana */
		    break;
		}
	    } else
		if (0xa0 < *(u_char *) s && *(u_char *) s < 0xff) {
		    if (((u_char *) s)[-1] != SS3)
			euc = 1;/* zenkaku */
		    break;
		}
	    euc = 0;		/* not EUC */
	    break;
	}
	if (extend)
	    switch (dec) {
	    case 1:
		/*
		 * DEC first byte.
		 */
		if (*(u_char *) s & 0x80) {
		    if (0xa0 < *(u_char *) s && *(u_char *) s < 0xff) {
			dec = 2;
			break;
		    } else if (*(u_char *) s < 0xa0)
			break;
		    dec = 0;	/* not DEC */
		}
		break;
	    case 2:
		/*
		 * DEC second byte.
		 */
		if (0x20 < (*(u_char *) s & 0x7f) &&
			(*(u_char *) s & 0x7f) < 0x7f) {
		    dec = 1;
		} else
		    dec = 0;	/* not DEC */
		break;
	    }
	switch (sjis) {
	case 1:
	    /*
	     * shift-JIS first byte.
	     */
	    if (*(u_char *) s & 0x80) {
		if (0xa0 < *(u_char *) s && *(u_char *) s < 0xe0) {
		    if (!zenkaku)
			break;	/* hankaku */
		} else if (*(u_char *) s != 0x80 &&
			*(u_char *) s != 0xa0 &&
			*(u_char *) s <= (extend ? 0xfc : 0xef)) {
		    sjis = 2;	/* zenkaku */
		    jis8 = 0;
		    break;
		}
		sjis = 0;	/* not SJIS */
	    }
	    break;
	case 2:
	    /*
	     * shift-JIS second byte.
	     */
	    if (0x40 <= *(u_char *) s && *(u_char *) s != 0x7f &&
		    *(u_char *) s <= 0xfc)
		sjis = 1;
	    else
		sjis = 0;	/* not SJIS */
	    break;
	}
    }
    if (euc == 1)
	code |= EUC;
    if (dec == 1)
	code |= DEC;
    if (sjis == 1)
	code |= zenkaku || !jis8 ? SJIS : SJIS | JIS8;
    return (code);
}
