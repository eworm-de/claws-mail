dnl Autoconf macros for libaspell
dnl $Id$

# Configure paths for ASPELL
# Shamelessly stolen from the one of GPGME by Werner Koch 
# Melvin Hadasht  2001-09-17, 2002

dnl AM_PATH_ASPELL([MINIMUM-VERSION,
dnl               [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND ]]])
dnl Test for aspell, and define ASPELL_CFLAGS and ASPELL_LIBS
dnl
AC_DEFUN(AM_PATH_ASPELL,
[dnl
dnl Get the cflags and libraries from the aspell-config script
dnl
  AC_ARG_WITH(aspell-prefix,
   [  --with-aspell-prefix=PFX           Prefix where aspell is installed (optional)],
          aspell_prefix="$withval", aspell_prefix="")
  AC_ARG_ENABLE(aspell-test,
   [  --disable-aspell-test   Do not try to compile and run a test GNU/aspell program],
          , enable_aspelltest=yes)
  AC_ARG_WITH(aspell-libs,
   [  --with-aspell-libs=LIBS            Where GNU/aspell library reside (/usr/local/lib)],
          aspell_libs="$withval", aspell_libs="")
  AC_ARG_WITH(aspell-includes,
   [  --with-aspell-includes=INCLUDES    Where GNU/aspell headers reside (/usr/local/include)],
          aspell_includes="$withval", aspell_includes="")

  if test x$aspell_prefix != x ; then
     if test x${ASPELL+set} != xset ; then
        ASPELL=$aspell_prefix/bin/aspell
     fi
     if test x$aspell_includes == x ; then
        aspell_includes=$aspell_prefix/include
     fi
     if test x$aspell_libs == x ; then
        aspell_libs=$aspell_prefix/lib
     fi
  fi
  if test x$aspell_includes == x ; then
     aspell_includes=/usr/local/include
  fi
  if test x$aspell_libs == x ; then
     aspell_libs=/usr/local/lib
  fi
  AC_PATH_PROG(ASPELL, aspell, no)
  min_aspell_version=ifelse([$1], ,.50,$1)
  AC_MSG_CHECKING(for GNU/aspell - version >= $min_aspell_version)
  no_aspell=""
  if test "$ASPELL" = "no" ; then
    no_aspell=yes
  else
    ASPELL_CFLAGS="-I$aspell_includes"
    ASPELL_LIBS="-L$aspell_libs -laspell"
    aspell_version=`$ASPELL version|sed -e "s/\(@(#) International Ispell Version 3.1.20 (but really Aspell \)\(.*\))/\2/"`
    if test "x$enable_aspelltest" = "xyes" ; then
      ac_save_CFLAGS="$CFLAGS"
      ac_save_LIBS="$LIBS"
      CFLAGS="$CFLAGS $ASPELL_CFLAGS"
      LIBS="$LIBS $ASPELL_LIBS"
dnl
dnl 
dnl
      rm -f conf.aspelltest
      AC_TRY_RUN([
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <aspell.h>

int
main ()
{
 system ("touch conf.aspelltest");
 if(strcmp("$aspell_version","$min_aspell_version")<0){
   printf("no\n");
   return 1;
   }
 return 0;
}
],, no_aspell=yes,[echo $ac_n "cross compiling; assumed OK... $ac_c"])
       CFLAGS="$ac_save_CFLAGS"
       LIBS="$ac_save_LIBS"
     fi
  fi
  if test "x$no_aspell" = x ; then
     AC_MSG_RESULT(yes)
     ifelse([$2], , :, [$2])
  else
     if test -f conf.aspelltest ; then
        :
     else
        AC_MSG_RESULT(no)
     fi
     if test "$ASPELL" = "no" ; then
       echo "*** The aspell executable could not be found"
       echo "*** If aspell was installed in PREFIX, make sure PREFIX/bin is in"
       echo "*** your path, or set the ASPELL environment variable to the"
       echo "*** full path to aspell."
     else
       if test -f conf.aspelltest ; then
        :
       else
          echo "*** Could not run aspell test program, checking why..."
          CFLAGS="$CFLAGS $ASPELL_CFLAGS"
          LIBS="$LIBS $ASPELL_LIBS"
          AC_TRY_LINK([
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <aspell.h>
],      [ AspellConfig * aspellconfig= new_aspell_config(); return 0 ],
        [ 
echo "*** The test program compiled, but did not run. This usually means"
echo "*** that the run-time linker is not finding GNU/aspell or finding the wrong"
echo "*** version of GNU/aspell. If it is not finding GNU/aspell, you'll need to set"
echo "*** your LD_LIBRARY_PATH environment variable, or edit /etc/ld.so.conf to point"
echo "*** to the installed location  Also, make sure you have run ldconfig if"
echo "*** that is required on your system"
echo "***"
echo "*** If you have an old version installed, it is best to remove it,"
echo "*** although you may also be able to get things to work by"
echo "*** modifying LD_LIBRARY_PATH"
echo "***"
        ],
        [
echo "*** The test program failed to compile or link. See the file config.log"
echo "*** for the exact error that occured. This usually means GNU/aspell was"
echo "*** incorrectly installed or that you have moved GNU/aspell since it was"
echo "*** installed. "
        ])
          CFLAGS="$ac_save_CFLAGS"
          LIBS="$ac_save_LIBS"
       fi
     fi
     ASPELL_CFLAGS=""
     ASPELL_LIBS=""
     ifelse([$3], , :, [$3])
  fi
  AC_SUBST(ASPELL_CFLAGS)
  AC_SUBST(ASPELL_LIBS)
  rm -f conf.aspelltest
])

