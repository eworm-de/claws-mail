#include "kcc.h"

void (*outascii)(), (*outkanji)(), (*outgaiji)(), (*outkana)();

/**********************************************************************
 *                                                                    *
 *  Conversion Routines                                               *
 *                                                                    *
 **********************************************************************/
void Kcc_outchar();
void Kcc_jisascii(), Kcc_jiskanji(), Kcc_jisgaiji();
void Kcc_jiskana(), Kcc_jiskanak(), Kcc_jiskana8();
void Kcc_euckanji(), Kcc_eucgaiji(), Kcc_euckana();
void Kcc_sjiskanji(), Kcc_sjisgaiji(), Kcc_sjiskana();
void Kcc_decascii(), Kcc_deckanji(), Kcc_decgaiji(), Kcc_deckana();

/*---------------------------------------------------------------------
    NAME
	setfunc
 ---------------------------------------------------------------------*/
void Kcc_setfunc(outcode)
     unsigned outcode;
{
    switch (outcode) {
    case EUC:
	outascii = Kcc_outchar;
	outkanji = Kcc_euckanji;
	outgaiji = Kcc_eucgaiji;
	outkana = Kcc_euckana;
	break;
    case DEC:
	outascii = Kcc_decascii;
	outkanji = Kcc_deckanji;
	outgaiji = Kcc_decgaiji;
	outkana = Kcc_deckana;
	break;
    case SJIS:
	outascii = Kcc_outchar;
	outkanji = Kcc_sjiskanji;
	outgaiji = Kcc_sjisgaiji;
	outkana = Kcc_sjiskana;
	break;
    default:
	outascii = Kcc_jisascii;
	outkanji = Kcc_jiskanji;
	outgaiji = Kcc_jisgaiji;
	switch (outcode) {
	case JIS:		/* mode:  M_ASCII M_KANJI M_GAIJI
				 * M_SO */
	    outkana = Kcc_jiskana;
	    break;
	case JIS | ESCI:	/* mode:  M_ASCII M_KANJI M_GAIJI
				 * M_ESCI */
	    outkana = Kcc_jiskanak;
	    break;
	case JIS | JIS8:	/* mode:  M_ASCII M_KANJI M_GAIJI */
	    outkana = Kcc_jiskana8;
	    break;
	}
	break;
    }
}

