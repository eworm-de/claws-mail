#ifndef __BASE64_H__
#define __BASE64_H__

typedef struct _Base64Decoder Base64Decoder;

Base64Decoder * base64_decoder_new	(void);
void		base64_decoder_free	(Base64Decoder *decoder);
int		base64_decoder_decode	(Base64Decoder *decoder,
					 const char    *in, 
					 char          *out);

void to64frombits(unsigned char *, const unsigned char *, int);
int from64tobits(char *, const char *);

#endif /* __BASE64_H__ */
