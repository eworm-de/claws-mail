#ifndef UTILS_H
#define UTILS_H

#ifdef WIN32
#ifndef ssize_t
# define ssize_t size_t
#endif

#define EX_OK		 0
#define EX_DATAERR	 1
#define EX_IOERR	 2
#define EX_NOHOST	 3
#define EX_NOPERM	 4
#define EX_OSERR	 5
#define EX_PROTOCOL	 6
#define EX_SOFTWARE	 7
#define EX_TEMPFAIL	 8
#define EX_UNAVAILABLE	 9
#define EX_USAGE	10

#define EADDRINUSE	101
#define EAFNOSUPPORT	102
#define EALREADY	103
#define ECONNREFUSED	104
#define EINPROGRESS	105
#define EISCONN		106
#define ENETUNREACH	107
#define ENOBUFS		108
#define ENOTSOCK	109
#define EPROTONOSUPPORT	110
#define ETIMEDOUT	111

#define snprintf _snprintf
#define syslog fprintf
#define LOG_ERR		stderr
#define LOG_DEBUG	stdout

ssize_t socketread(int fd, void *buf, ssize_t len);
#endif

extern int libspamc_timeout;  /* default timeout in seconds */

#ifdef WIN32 /* somewhere ssl.h gets included -> conflicting typedef's */
# define SPAMC_SSL 1
#endif
#ifdef SPAMC_SSL
#include <openssl/crypto.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#else
typedef int	SSL;	/* fake type to avoid conditional compilation */
typedef int	SSL_CTX;
typedef int	SSL_METHOD;
#endif
#ifdef WIN32 /* somewhere ssl.h gets included -> conflicting typedef's */
# undef SPAMC_SSL
#endif

ssize_t fd_timeout_read (int fd, void *, size_t );  
int ssl_timeout_read (SSL *ssl, void *, int );  

/* these are fd-only, no SSL support */
int full_read(int fd, unsigned char *buf, int min, int len);
int full_write(int fd, const unsigned char *buf, int len);

#endif
