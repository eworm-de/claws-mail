/*
 * This code is copyright 2001 by Craig Hughes
 * Portions copyright 2002 by Brad Jorsch
 * It is licensed under the same license as Perl itself.  The text of this
 * license is included in the SpamAssassin distribution in the file named
 * "License".
 */

#ifdef _MSC_VER
# include <w32lib.h>
#else
#include <unistd.h>
#endif
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#ifndef WIN32
#include <sys/uio.h>
#include <unistd.h>
#endif
#include <stdio.h>
#ifdef __MINGW32__
#include "libspamc_utils.h"
#else
#include "utils.h"
#endif

/* Dec 13 2001 jm: added safe full-read and full-write functions.  These
 * can cope with networks etc., where a write or read may not read all
 * the data that's there, in one call.
 */
/* Aug 14, 2002 bj: EINTR and EAGAIN aren't fatal, are they? */
/* Aug 14, 2002 bj: moved these to utils.c */
/* Jan 13, 2003 ym: added timeout functionality */

/* -------------------------------------------------------------------------- */

typedef void    sigfunc(int);   /* for signal handlers */

#ifdef WIN32
ssize_t socketread(int fd, void *buf, ssize_t len) {
  return recv(fd, buf, len, 0);
}
#endif

#ifndef WIN32
sigfunc* sig_catch(int sig, void (*f)(int))
{
  struct sigaction act, oact;
  act.sa_handler = f;
  act.sa_flags = 0;
  sigemptyset(&act.sa_mask);
  sigaction(sig, &act, &oact);
  return oact.sa_handler;
}
#endif

static void catch_alrm(int x) {
  /* dummy */
}

ssize_t
fd_timeout_read (int fd, void *buf, size_t nbytes)
{
  ssize_t nred;
  sigfunc* sig;

#ifndef WIN32
  sig = sig_catch(SIGALRM, catch_alrm);
  if (libspamc_timeout > 0) {
    alarm(libspamc_timeout);
  }
#endif
  do {
#ifdef WIN32
    nred = recv(fd, buf, nbytes, 0);
#else
    nred = read (fd, buf, nbytes);
#endif
  } while(nred < 0 && errno == EAGAIN);

  if(nred < 0 && errno == EINTR)
    errno = ETIMEDOUT;

#ifndef WIN32
  if (libspamc_timeout > 0) {
    alarm(0);
  }

  /* restore old signal handler */
  sig_catch(SIGALRM, sig);
#endif

  return nred;
}

int
ssl_timeout_read (SSL *ssl, void *buf, int nbytes)
{
  int nred;
  sigfunc* sig;

#ifndef WIN32
  sig = sig_catch(SIGALRM, catch_alrm);
  if (libspamc_timeout > 0) {
    alarm(libspamc_timeout);
  }
#endif

  do {
#ifdef SPAMC_SSL
    nred = SSL_read (ssl, buf, nbytes);
#else
    nred = 0;			/* never used */
#endif
  } while(nred < 0 && errno == EAGAIN);

  if(nred < 0 && errno == EINTR)
    errno = ETIMEDOUT;

#ifndef WIN32
  if (libspamc_timeout > 0) {
    alarm(0);
  }
#endif

  /* restore old signal handler */
#ifndef WIN32
  sig_catch(SIGALRM, sig);
#endif

  return nred;
}

/* -------------------------------------------------------------------------- */

int
full_read (int fd, unsigned char *buf, int min, int len)
{
  int total;
  int thistime;

  for (total = 0; total < min; ) {
#ifdef WIN32
    thistime = read (fd, buf+total, len-total);
#else
    thistime = fd_timeout_read (fd, buf+total, len-total);
#endif

    if (thistime < 0) {
      return -1;
    } else if (thistime == 0) {
      /* EOF, but we didn't read the minimum.  return what we've read
       * so far and next read (if there is one) will return 0. */
      return total;
    }

    total += thistime;
  }
  return total;
}

int
full_write (int fd, const unsigned char *buf, int len)
{
  int total;
  int thistime;

  for (total = 0; total < len; ) {
#ifdef WIN32
    thistime = send (fd, buf+total, len-total, 0);
#else
    thistime = write (fd, buf+total, len-total);
#endif

    if (thistime < 0) {
      if(EINTR == errno || EAGAIN == errno) continue;
      return thistime;        /* always an error for writes */
    }
    total += thistime;
  }
  return total;
}
