#include "libkcc.h"

/*---------------------------------------------------------------------
    TYPE
	table
    NAME
	katakana, hiragana, dakuon - JIS X0201 kana to JIS kanji in DEC
 ---------------------------------------------------------------------*/
unsigned short katakana[] = {
    0,      0xa1a3, 0xa1d6, 0xa1d7, 0xa1a2, 0xa1a6, 0xa5f2, 0xa5a1,
    0xa5a3, 0xa5a5, 0xa5a7, 0xa5a9, 0xa5e3, 0xa5e5, 0xa5e7, 0xa5c3,
    0xa1bc, 0xa5a2, 0xa5a4, 0xa5a6, 0xa5a8, 0xa5aa, 0xa5ab, 0xa5ad,
    0xa5af, 0xa5b1, 0xa5b3, 0xa5b5, 0xa5b7, 0xa5b9, 0xa5bb, 0xa5bd,
    0xa5bf, 0xa5c1, 0xa5c4, 0xa5c6, 0xa5c8, 0xa5ca, 0xa5cb, 0xa5cc,
    0xa5cd, 0xa5ce, 0xa5cf, 0xa5d2, 0xa5d5, 0xa5d8, 0xa5db, 0xa5de,
    0xa5df, 0xa5e0, 0xa5e1, 0xa5e2, 0xa5e4, 0xa5e6, 0xa5e8, 0xa5e9,
    0xa5ea, 0xa5eb, 0xa5ec, 0xa5ed, 0xa5ef, 0xa5f3, 0xa1ab, 0xa1ac,
};

unsigned short hiragana[] = {
    0,      0xa1a3, 0xa1d6, 0xa1d7, 0xa1a2, 0xa1a6, 0xa4f2, 0xa4a1,
    0xa4a3, 0xa4a5, 0xa4a7, 0xa4a9, 0xa4e3, 0xa4e5, 0xa4e7, 0xa4c3,
    0xa1bc, 0xa4a2, 0xa4a4, 0xa4a6, 0xa4a8, 0xa4aa, 0xa4ab, 0xa4ad,
    0xa4af, 0xa4b1, 0xa4b3, 0xa4b5, 0xa4b7, 0xa4b9, 0xa4bb, 0xa4bd,
    0xa4bf, 0xa4c1, 0xa4c4, 0xa4c6, 0xa4c8, 0xa4ca, 0xa4cb, 0xa4cc,
    0xa4cd, 0xa4ce, 0xa4cf, 0xa4d2, 0xa4d5, 0xa4d8, 0xa4db, 0xa4de,
    0xa4df, 0xa4e0, 0xa4e1, 0xa4e2, 0xa4e4, 0xa4e6, 0xa4e8, 0xa4e9,
    0xa4ea, 0xa4eb, 0xa4ec, 0xa4ed, 0xa4ef, 0xa4f3, 0xa1ab, 0xa1ac,
};

unsigned char dakuon[] = {
    0,      0,      0,      0,      0,      0,      0,      0,
    0,      0,      0,      0,      0,      0,      0,      0,
    0,      0,      0,      0,      0,      0,      1,      1,
    1,      1,      1,      1,      1,      1,      1,      1,
    1,      1,      1,      1,      1,      0,      0,      0,
    0,      0,      3,      3,      3,      3,      3,      0,
    0,      0,      0,      0,      0,      0,      0,      0,
    0,      0,      0,      0,      0,      0,      0,      0,
};

/*********************************************************/
unsigned short *kanatbl = katakana;
int lastkana=0;
extern bool nogaiji;

/*---------------------------------------------------------------------
    NAME
	decascii
 ---------------------------------------------------------------------*/
void Kcc_decascii(ddd, c)
    register int c;
    char **ddd;
{
    if (lastkana) {
	**ddd = kanatbl[lastkana] >> 8; (*ddd)++;
	**ddd = kanatbl[lastkana] & 0xff; (*ddd)++;
	lastkana = 0;
    }
    **ddd = c; (*ddd)++;
}

/*---------------------------------------------------------------------
    NAME
	decgaiji
 ---------------------------------------------------------------------*/
void Kcc_decgaiji(ddd, c1, c2)
    register int c1, c2;
    char **ddd;
{
    if (lastkana) {
        **ddd = kanatbl[lastkana] >> 8; (*ddd)++;
        **ddd = kanatbl[lastkana] & 0xff; (*ddd)++;
	lastkana = 0;
    }
    if (nogaiji) {
	**ddd = ZENPAD >> 8 | 0x80; (*ddd)++;
	**ddd = (ZENPAD & 0xff) | 0x80; (*ddd)++;
    } else {
	**ddd = c1 | 0x80; (*ddd)++;
	**ddd = c2; (*ddd)++;
    }
}

/*---------------------------------------------------------------------
    NAME
	deckana
 ---------------------------------------------------------------------*/
void Kcc_deckana(ddd, c)
    register int c;
    char **ddd;
{
    register int cc;
    int i;
    extern unsigned char dakuon[];

    if (lastkana) {
	cc = kanatbl[lastkana];
	if ((c == 0x5e || c == 0x5f) &&
		(i = dakuon[lastkana] & (c == 0x5e ? 1 : 2))) {
	    cc += i;
	    c = -1;
	}
	**ddd = cc >> 8; (*ddd)++;
	**ddd = cc & 0xff; (*ddd)++;
    }
    if (c < 0x21 || 0x5f < c) {
	if (c > 0) {
	    **ddd = ZENPAD >> 8; (*ddd)++;
	    **ddd = ZENPAD & 0xff; (*ddd)++;
	}
	lastkana = 0;
    } else
	lastkana = c - 0x20;
}
/*---------------------------------------------------------------------
    NAME
	deckanji
 ---------------------------------------------------------------------*/
void Kcc_deckanji(ddd, c1, c2)
    register int c1, c2;
    char **ddd;
{
    if (lastkana) {
	**ddd = kanatbl[lastkana] >> 8; (*ddd)++;
	**ddd = kanatbl[lastkana] & 0xff; (*ddd)++;
	lastkana = 0;
    }
    **ddd = c1 | 0x80; (*ddd)++;
    **ddd = c2 | 0x80; (*ddd)++;
}

