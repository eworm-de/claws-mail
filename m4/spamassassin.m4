dnl check for libspamc required includes

AC_DEFUN([AC_SPAMASSASSIN],
[dnl

AC_CHECK_HEADERS(sys/time.h syslog.h unistd.h errno.h sys/errno.h)
AC_CHECK_HEADERS(time.h sysexits.h sys/socket.h netdb.h netinet/in.h)

AC_CACHE_CHECK([for SHUT_RD],
       shutrd, [
                AC_TRY_COMPILE([#include <sys/types.h>
#include <sys/socket.h>],
                        [printf ("%d", SHUT_RD); return 0;],
                                        [shutrd=yes],
                                        [shutrd=no]),
       ])
if test $shutrd = yes ; then
  AC_DEFINE(HAVE_SHUT_RD, 1, HAVE_SHUT_RD)
fi

dnl ----------------------------------------------------------------------

AC_CHECK_FUNCS(socket strdup strtod strtol snprintf shutdown)

dnl ----------------------------------------------------------------------

AC_CACHE_CHECK([for h_errno],
        herrno, [
                AC_TRY_COMPILE([#include <netdb.h>],
                        [printf ("%d", h_errno); return 0;],
                                        [herrno=yes],
                                        [herrno=no]),
        ])
if test $herrno = yes ; then
  AC_DEFINE(HAVE_H_ERRNO, 1, HAVE_H_ERRNO)
fi

dnl ----------------------------------------------------------------------

dnl ----------------------------------------------------------------------

AC_CACHE_CHECK([for in_addr_t],
        inaddrt, [
                AC_TRY_COMPILE([#include <sys/types.h>
#include <netinet/in.h>],
                        [in_addr_t foo; return 0;],
                                        [inaddrt=yes],
                                        [inaddrt=no]),
        ])
if test $inaddrt = no ; then
  AC_CHECK_TYPE(in_addr_t, unsigned long)
fi

dnl ----------------------------------------------------------------------

AC_CACHE_CHECK([for INADDR_NONE],
        haveinaddrnone, [
                AC_TRY_COMPILE([#include <sys/types.h>
#include <netinet/in.h>],
                        [in_addr_t foo = INADDR_NONE; return 0;],
                                        [haveinaddrnone=yes],
                                        [haveinaddrnone=no]),
        ])
if test $haveinaddrnone = yes ; then
  AC_DEFINE(HAVE_INADDR_NONE, 1, HAVE_INADDR_NONE)
fi

dnl ----------------------------------------------------------------------

AC_CACHE_CHECK([for EX__MAX],
        haveexmax, [
                AC_TRY_COMPILE([#ifdef HAVE_SYSEXITS_H
#include <sysexits.h>
#endif
#include <errno.h>],
                        [int foo = EX__MAX; return 0;],
                                        [haveexmax=yes],
                                        [haveexmax=no]),
        ])
if test $haveexmax = yes ; then
  AC_DEFINE(HAVE_EX__MAX, 1, HAVE_EX__MAX)
fi

])
