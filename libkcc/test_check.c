#include <stdio.h>
main ()
{
  FILE *fp;
  char s[500];
  int i=0, c;

  fp=fopen("test_file","r");
  if (fp == NULL) {printf("not found\n"); exit;}

  while ((c = fgetc(fp)) != EOF && i<490)
  {
    s[i] = c; i++;
  }
  s[i]='\0';

  printf ("%d bytes ; %x\n",i,KCC_check(s,0));
}
