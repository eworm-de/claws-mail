#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "kcc.h"
#include "libkcc.h"

/**********************************************************************
 *                                                                    *
 *  Hold Buffer Operations                                            *
 *                                                                    *
 **********************************************************************/
char *holdbuf, *bufend;
char *bufp;

/*---------------------------------------------------------------------
    NAME
	buffalloc
 ---------------------------------------------------------------------*/
char *Kcc_buffalloc(len)
    unsigned len;
{
    if ((bufp = holdbuf = (char *) malloc(len)) == NULL)
      return NULL;
    bufend = holdbuf + len;
    return bufend;
}

/*---------------------------------------------------------------------
    NAME
	append
 ---------------------------------------------------------------------*/
bool Kcc_append(s, len)
    register char *s;
    register int len;
{
    if (bufp + len > bufend)
	return (0);
    for (; len; --len)
	*bufp++ = *(u_char *) s++;
    return (1);
}

/*---------------------------------------------------------------------
    NAME
	flush
 ---------------------------------------------------------------------*/
void Kcc_flush(code, ddd, outcode, inmode, insi, inso, innj, ingj)
    unsigned code;
    char **ddd;
    enum mode *inmode;
    unsigned long *insi, *inso, *innj, *ingj;
    unsigned outcode;
{
    unsigned out();

    Kcc_out(ddd, holdbuf, bufp - holdbuf, code, outcode, inmode, insi, inso, innj, ingj);
    bufp = holdbuf;
}

/*---------------------------------------------------------------------
    NAME
	bufffree
 ---------------------------------------------------------------------*/
void Kcc_bufffree(void)
{
    if (holdbuf) {
	free(holdbuf);
	holdbuf = NULL;
    }
}
