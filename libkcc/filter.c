#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "kcc.h"
#include "libkcc.h"

unsigned incode, outcode;
char shiftin[7] = "\033$B";
char shiftout[4] = "\033(J";

/**********************************************************************
 *                                                                    *
 *  Filter                                                            *
 *                                                                    *
 **********************************************************************/
enum mode gsmode;		/* guess:  M_ASCII M_KANJI M_SO */
enum mode inmode;		/* input:  M_ASCII M_KANJI M_GAIJI
				 * M_SO M_ESCI */
enum mode outmode;		/* output: M_ASCII M_KANJI M_GAIJI
				 * M_SO M_ESCI */

unsigned long insi;		/* JIS shift-in sequence flag */
unsigned long inso;		/* JIS shift-out sequence flag
				 * including "ESC(I" */
unsigned long innj;		/* JIS 1990 sequence flag */
unsigned long ingj;		/* JIS 1990 aux flag */

bool nogaiji = 0;
/*---------------------------------------------------------------------
    NAME
	filter - filtering routine
 ---------------------------------------------------------------------*/
int KCC_filter(ddd, outcode_name, sss, incode_name, extend, zenkaku, gaiji)
    char *sss, *ddd;
    int extend, zenkaku;
    char *incode_name, *outcode_name;
    int gaiji;
{
    register bool hold;
    register unsigned code, c = ASCII;
    register int len;
    char str[LENLINE];
    char *dummy, *dst;
    unsigned incode, outcode;
    unsigned size = HOLDBUFSIZ;
    char s[3];
    s[0]='\0'; s[1]='\0'; s[2]='\0';

    nogaiji = gaiji;
    if (extend<0) {extend=0;} ;     if (extend>1) {extend=1;}
    if (zenkaku<0) {zenkaku=0;};    if (zenkaku>1) {zenkaku=1;}
    if (nogaiji<0) {nogaiji=0;};    if (nogaiji>1) {nogaiji=1;}

    /* allocate hold buf */
    if (Kcc_buffalloc(size) == NULL)  return (-1);

    incode =0; outcode = EUC;
    if (!strcasecmp(incode_name,"AUTO")) { incode=0; }
    if (!strcasecmp(incode_name,"SJIS")) { incode=SJIS; }
    if (!strcasecmp(incode_name,"DEC"))  { incode=DEC; }
    if (!strcasecmp(incode_name,"JIS"))  { incode=JIS; }
    if (!strcasecmp(incode_name,"JIS8")) { incode=JIS8; }
    if (!strcasecmp(incode_name,"JISI")) { incode=ESCI; }

    if (!strcasecmp(outcode_name,"EUC"))  { outcode=EUC; }
    if (!strcasecmp(outcode_name,"SJIS")) { outcode=SJIS; }
    if (!strcasecmp(outcode_name,"DEC"))  { outcode=DEC; }
    if (!strncasecmp(outcode_name,"JIS", 3))
      { outcode=JIS;
        if (outcode_name[3]!='\0' && outcode_name[3]!='8' && outcode_name[3]!='I' )
	  { s[0]=outcode_name[3] ; s[1]=outcode_name[4]; }
      }
    if (!strncasecmp(outcode_name,"JIS8",4))
      { outcode=JIS8;
        if (outcode_name[4]!='\0')
	  { s[0]=outcode_name[4] ; s[1]=outcode_name[5]; }
      }
    if (!strncasecmp(outcode_name,"JISI",4))
      { outcode=ESCI;
        if (outcode_name[4]!='\0')
	  { s[0]=outcode_name[4] ; s[1]=outcode_name[5]; }
      }
    if ((s[0] == 'B' || s[0] == '@' || s[0] == '+') &&
	(s[1] == 'B' || s[1] == 'J' || s[1] == 'H'))
      {
	if (s[0] == '+')
	  sprintf(shiftin, "\033&@\033$B");
	else
	  sprintf(shiftin, "\033$%c", s[0]);
	sprintf(shiftout, "\033(%c", s[1]);
      }

    Kcc_setfunc(outcode);

    dummy = sss;
    dst = ddd;

    code = incode ? incode : extend ? BIT8 : BIT8 & ~DEC;
    gsmode = inmode = outmode = M_ASCII;
    insi = inso = innj = ingj = 0;
    hold = 0;
    while ((len = Kcc_getstr(str, sizeof str, &dummy)) != 0) {
	if ((!(code & NONASCII) && code & BIT8) ||
		(code & (EUC | DEC) && code & SJIS && !(code & ASSUME))) {
	    /*
	     * So far, no kanji has been seen, or ambiguous.
	     */
	    c = Kcc_guess(str, len, extend, zenkaku, &gsmode, &insi, &inso, &innj, &ingj);
	    code |= c & (JIS | NONASCII), code &= c | ~BIT8;
	    if (code & NONASCII && code & (EUC | DEC) && code & SJIS) {
		/*
		 * If ambiguous, store the line in hold buffer.
		 */
		if (Kcc_append(str, len)) {
		    hold = 1;
		    continue;
		}
		/*
		 * When buffer is full, assume EUC/DEC.
		 */
		code |= ASSUME;
	    }
	}
	if (hold) {
	    /*
	     * Flush hold buffer.
	     */
	    Kcc_flush(code,  &dst, outcode, &inmode, &insi, &inso, &innj, &ingj);
	    hold = 0;
	}
	c = Kcc_out(&dst,   str, len, code,   outcode, &inmode, &insi, &inso, &innj, &ingj);
	code |= c & JIS, code &= c | ~BIT8;
    }
    if (hold)
	/*
	 * Assume EUC.
	 */
	Kcc_flush(code |= ASSUME,   &dst, outcode, &inmode, &insi, &inso, &innj, &ingj);

    *dst = '\0';
    Kcc_bufffree();
    return (Kcc_showcode(c));
}
