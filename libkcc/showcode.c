#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "kcc.h"

/*---------------------------------------------------------------------
    NAME
	showcode
 ---------------------------------------------------------------------*/
int Kcc_showcode(code)
    register unsigned code;
{
    /* char *s; */
    /*    void showjis();*/
    int k, m;
    /* int k, m,n; */

    if (!(code & NONASCII)) {
	/*
	 * 7-bit JIS / ASCII.
	 */
	if (code & JIS) {
            return JIS;
	} else { return ASCII; }
    } else if (code & (EUC | DEC)) {
      k = code & EUC ? code & DEC ? EUC : EUC : DEC;
	if (code & SJIS) {
	    /*
	     * Ambiguous.
	     */
	    if (code & JIS8) {
		m = code & JIS ? JIS : SJIS;
		if (code & ASSUME) {
		  return code & JIS ? m : k;
		}
		return k;
	    }
	    if (code & ASSUME) { return k; }
	    return SJIS;
	} else {
	    /*
	     * EUC/DEC.
	     */
	  return k;
        }
    } else if (code & JIS8) {
	/*
	 * 8-bit JIS / shift-JIS or 8-bit JIS.
	 */
	if (!(code & JIS))
	    return SJIS;
	return JIS8;
    } else if (code & SJIS)
	/*
	 * Shift-JIS.
	 */
      return SJIS;
    else {
	/*
	 * Non-ASCII deteced but neither EUC/DEC nor SJIS.
	 */
	return BINARY;
    }
    if (code & JIS) {
      return JIS;
    }

    return BINARY;
}
