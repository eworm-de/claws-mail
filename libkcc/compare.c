#include "libkcc.h"

/*---------------------------------------------------------------------
    NAME
	compare
 ---------------------------------------------------------------------*/
bool Kcc_compare(s, str)
    register char *s, *str;
{
    while (*s)
	if (*s++ != *str++)
	    return (0);
    return (1);
}
