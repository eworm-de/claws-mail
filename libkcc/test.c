#include <stdio.h>
#include "kcc.h"

main(argc, argv)
    register int argc;
    register char *argv[];
{
  FILE *fp;
  char s[2048], d[4096];
  int i=0, c;

  fp=fopen("test_file","r");
  if (fp == NULL) {printf("not found\n"); exit;}

  while ((c = fgetc(fp)) != EOF && i<2045)
  {
    s[i] = c; i++;
  }
  s[i]='\0';

  printf ("\n==== Check ===\n");
  printf ("%d bytes ; %x\n",i,KCC_check(s,0));

  printf ("\n==== filter ===\n");


  if (argc>1) {
    if (!strcasecmp(argv[1],"euc")) {
      i = KCC_filter(d, "EUC", s, "AUTO", 0,0,0);
    }
    if (!strcasecmp(argv[1],"jis")) {
      i = KCC_filter(d, "JIS", s, "AUTO", 0,0,0);
    }
    if (!strcasecmp(argv[1],"sjis")) {
      i = KCC_filter(d, "sjis", s, "AUTO", 0,0,0);
    }
  }
  else {
    i = KCC_filter(d, "JIS", s, "AUTO", 0,0,0);
  }

  printf("code = %x\n%s\n",i,d);

}
