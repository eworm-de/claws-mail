#define LENLINE		(BUFSIZ * 4)
#define HOLDBUFSIZ	8192	/* default size of hold buffer */

#define ESC		0x1b
#define SO		0x0e
#define SI		0x0f
#define SS2		0x8e	/* EUC single shift 2 */
#define SS3		0x8f	/* EUC single shift 3 */

#define ZENPAD		0x2222	/* padding char for zenkaku */
#define HANPAD		0x25	/* padding char for hankaku */

typedef int bool;

#define bitflag(c)	(1L << ((c) - '@'))

enum mode {
    M_ASCII,
    M_KANJI,
    M_GAIJI,
    M_SO,			/* hankaku kana with SO */
    M_ESCI,			/* hankaku kana with "ESC(I" */
};

/* buffer.c */
char *Kcc_buffalloc(unsigned len);
bool Kcc_append(register char *s, register int len);
void Kcc_flush(unsigned code, char **ddd, unsigned outcode,
	       enum mode *inmode, unsigned long *insi,
	       unsigned long *inso, unsigned long *innj,
	       unsigned long *ingj);
void Kcc_bufffree(void);

/* check.c */
unsigned KCC_check(char *s, int extend);

/* compare.c */
bool Kcc_compare(register char *s, register char *str);

/* dec.c */
void Kcc_decascii(char **ddd, register int c);
void Kcc_decgaiji(char **ddd, register int c1, register int c2);
void Kcc_deckana(char **ddd, register int c);
void Kcc_deckanji(char **ddd, register int c1, register int c2);

/* euc.c */
void Kcc_eucgaiji(char **ddd, register int c1, register int c2);
void Kcc_euckana(char **ddd, register int c);
void Kcc_euckanji(char **ddd, register int c1, register int c2);

/* filter.c */
int KCC_filter(char *ddd, char *outcode_name, char *sss, char *incode_name,
	       int extend, int zenkaku, int gaiji);

/* getstr.c */
int Kcc_getstr(char *str, register int n, char **sp);

/* guess.c */
unsigned Kcc_guess(char *str, int len, int extend, bool zenkaku,
		   enum mode *gsmode, unsigned long *insi,
		   unsigned long *inso, unsigned long *innj,
		   unsigned long *ingj);

/* jis.c */
void Kcc_jisascii(char **ddd, register int c);
void Kcc_jisgaiji(char **ddd, register int c1, register int c2);
void Kcc_jiskana(char **ddd, register int c);
void Kcc_jiskana8(char **ddd, register int c);
void Kcc_jiskanak(char **ddd, register int c);
void Kcc_jiskanji(char **ddd, register int c1, register int c2);

/* out.c */
void Kcc_outsjis(char **ddd, register int c1, register int c2);
unsigned Kcc_out(char **ddd, char *str, int len, register unsigned code,
		 unsigned outcode, enum mode *inmode,
		 unsigned long *insi, unsigned long *inso,
		 unsigned long *innj, unsigned long *ingj);

/* outchar.c */
void Kcc_outchar(char **ddd, register int c);

/* outsjis.c */
void outsjis(register int c1, register int c2);

/* setfunc.c */
void Kcc_setfunc(unsigned outcode);

/* showcode.c */
int Kcc_showcode(register unsigned code);

/* sjis.c */
void Kcc_sjisgaiji(char **ddd, register int c1, register int c2);
void Kcc_sjiskana(char **ddd, register int c);
void Kcc_sjiskanji(char **ddd, register int c1, register int c2);
