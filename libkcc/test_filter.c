#include <stdio.h>
#include "kcc.h"

main ()
{
  FILE *fp;
  char s[500], d[2000];
  int i=0, c;

  fp=fopen("test_file","r");
  if (fp == NULL) {printf("not found\n"); exit;}

  while ((c = fgetc(fp)) != EOF && i<490)
  {
    s[i] = c; i++;
  }
  s[i]='\0';

  i = KCC_filter(d, "EUC", s, "AUTO", 0,0,0);
  printf("code = %x\n%s\n",i,d);
  i = KCC_filter(d, "SJIS", s, "AUTO", 0,0,0);
  printf("code = %x\n%s\n",i,d);
  i = KCC_filter(d, "JIS", s, "AUTO", 0,0,0);
  printf("code = %x\n%s\n",i,d);
}
