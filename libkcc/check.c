#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "kcc.h"
#include "libkcc.h"

unsigned KCC_check(s, extend)
    char *s;
    int extend;
{
    register unsigned code, c;
    register int len;
    char str[LENLINE], *dummy;
    unsigned KCC_guess();
enum mode gsmode;               /* guess:  M_ASCII M_KANJI M_SO */
unsigned long insi;             /* JIS shift-in sequence flag */
unsigned long inso;             /* JIS shift-out sequence flag
                                 * including "ESC(I" */
unsigned long innj;             /* JIS 1990 sequence flag */
unsigned long ingj;             /* JIS 1990 aux flag */

    dummy = s;
    code = extend ? BIT8 : BIT8 & ~DEC;
    gsmode = M_ASCII;
    insi = inso = innj = ingj = 0;
    while ((len = Kcc_getstr(str, sizeof str, &dummy)) != 0) {
	c = Kcc_guess(str, len, extend, 0, &gsmode, &insi, &inso, &innj, &ingj);
	code |= c & (JIS | NONASCII), code &= c | ~BIT8;
	if (code & NONASCII && !(code & BIT8))
	    break;
    }

    return Kcc_showcode(code);
}
