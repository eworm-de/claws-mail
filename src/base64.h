#ifndef __BASE64_H__
#define __BASE64_H__

void to64frombits(unsigned char *, const unsigned char *, int);
int from64tobits(char *, const char *);

#endif /* __BASE64_H__ */
