/* unmime.c */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

typedef char    flag;
/* Bit-mask returned by MimeBodyType */
#define MSG_IS_7BIT       0x01
#define MSG_IS_8BIT       0x02
#define MSG_NEEDS_DECODE  0x80

#if !HAVE_LIBJCONV
extern void UnMimeHeader(unsigned char *buf);
#else
extern void UnMimeHeaderConv(unsigned char *buf, unsigned char *conv_r,
			     int conv_len);
#endif
extern int  MimeBodyType(unsigned char *hdrs, int WantDecode);
extern int  DoOneQPLine(unsigned char **bufp, flag delimited, flag issoftline);
#if 0
extern int  UnMimeBodyline(unsigned char **buf, flag delimited, flag issoftline);
#endif
