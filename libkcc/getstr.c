#include <stdio.h>

/*---------------------------------------------------------------------
    NAME
	getstr
 ---------------------------------------------------------------------*/
int Kcc_getstr(str, n, sp)
    char *str;
    register int n;
    char **sp;
{
    register int c;
    register char *s;

/*    for (s = str; --n > 0 && (c = **sp) != EOF ; ) {*/
    for (s = str; --n > 0 && (c = **sp) != EOF && c != '\0'; ) {
      (*sp)++;
	if ((*s++ = c) == '\n')
	    break;
    }
    return (s - str);
}
