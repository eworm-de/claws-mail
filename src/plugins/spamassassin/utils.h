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

ssize_t timeout_read(ssize_t (*reader)(int d, void *buf, size_t nbytes), 
                     int, void *, size_t );  

int full_read(int fd, unsigned char *buf, int min, int len);
int full_write(int fd, const unsigned char *buf, int len);

#endif
