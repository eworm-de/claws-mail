dnl Autoconf macros for libpspell
dnl $Id$

# Configure paths for PSPELL
# Shamelessly stolen from the one of GPGME by Werner Koch 
# Melvin Hadasht  2001-09-17

dnl AM_PATH_PSPELL([MINIMUM-VERSION,
dnl               [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND ]]])
dnl Test for pspell, and define PSPELL_CFLAGS and PSPELL_LIBS
dnl
AC_DEFUN(AM_PATH_PSPELL,
[dnl
dnl Get the cflags and libraries from the pspell-config script
dnl
  AC_ARG_WITH(pspell-prefix,
   [  --with-pspell-prefix=PFX           Prefix where pspell-config is installed (optional)],
          pspell_config_prefix="$withval", pspell_config_prefix="")
  AC_ARG_ENABLE(pspelltest,
   [  --disable-pspelltest               Do not try to compile and run a test pspell program],
          , enable_pspelltest=yes)
  AC_ARG_WITH(pspelllibs,
   [  --with-pspell-libs=LIBS            Where pspell library reside (/usr/local/lib)],
          pspell_libs=$pspelllibs, pspell_libs='/usr/local/lib')
  AC_ARG_WITH(pspellincludes,
   [  --with-pspell-includes=INCLUDES    Where pspell headers reside (/usr/local/include)],
          pspell_includes=$pspelllibs, pspell_includes='/usr/local/include')


  if test x$pspell_config_prefix != x ; then
     if test x${PSPELL_CONFIG+set} != xset ; then
        PSPELL_CONFIG=$pspell_config_prefix/bin/pspell-config
     fi
  fi

  AC_PATH_PROG(PSPELL_CONFIG, pspell-config, no)
  min_pspell_version=ifelse([$1], ,.12.2,$1)
  AC_MSG_CHECKING(for pspell - version >= $min_pspell_version)
  no_pspell=""
  if test "$PSPELL_CONFIG" = "no" ; then
    no_pspell=yes
  else
    PSPELL_CFLAGS="-I$pspell_includes"
    PSPELL_LIBS="-L$pspell_libs -lpspell"
    pspell_config_version=`$PSPELL_CONFIG version`
    if test "x$enable_pspelltest" = "xyes" ; then
      ac_save_CFLAGS="$CFLAGS"
      ac_save_LIBS="$LIBS"
      CFLAGS="$CFLAGS $PSPELL_CFLAGS"
      LIBS="$LIBS $PSPELL_LIBS"
dnl
dnl 
dnl
      rm -f conf.pspelltest
      AC_TRY_RUN([
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pspell/pspell.h>

int
main ()
{
 system ("touch conf.pspelltest");
 if(strcmp("$pspell_config_version","$min_pspell_version")<0){
   printf("no\n");
   return 1;
   }
 return 0;
}
],, no_pspell=yes,[echo $ac_n "cross compiling; assumed OK... $ac_c"])
       CFLAGS="$ac_save_CFLAGS"
       LIBS="$ac_save_LIBS"
     fi
  fi
  if test "x$no_pspell" = x ; then
     AC_MSG_RESULT(yes)
     ifelse([$2], , :, [$2])
  else
     if test -f conf.pspelltest ; then
        :
     else
        AC_MSG_RESULT(no)
     fi
     if test "$PSPELL_CONFIG" = "no" ; then
       echo "*** The pspell-config script installed by pspell could not be found"
       echo "*** If pspell was installed in PREFIX, make sure PREFIX/bin is in"
       echo "*** your path, or set the PSPELL_CONFIG environment variable to the"
       echo "*** full path to pspell-config."
     else
       if test -f conf.pspelltest ; then
        :
       else
          echo "*** Could not run pspell test program, checking why..."
          CFLAGS="$CFLAGS $PSPELL_CFLAGS"
          LIBS="$LIBS $PSPELL_LIBS"
          AC_TRY_LINK([
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pspell/pspell.h>
],      [ PspellConfig * pspellconfig= new_pspell_config(); return 0 ],
        [ 
echo "*** The test program compiled, but did not run. This usually means"
echo "*** that the run-time linker is not finding pspell or finding the wrong"
echo "*** version of pspell. If it is not finding pspell, you'll need to set your"
echo "*** LD_LIBRARY_PATH environment variable, or edit /etc/ld.so.conf to point"
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
echo "*** for the exact error that occured. This usually means pspell was"
echo "*** incorrectly installed or that you have moved pspell since it was"
echo "*** installed. In the latter case, you may want to edit the"
echo "*** pspell-config script: $PSPELL_CONFIG" 
        ])
          CFLAGS="$ac_save_CFLAGS"
          LIBS="$ac_save_LIBS"
       fi
     fi
     PSPELL_CFLAGS=""
     PSPELL_LIBS=""
     ifelse([$3], , :, [$3])
  fi
  AC_SUBST(PSPELL_CFLAGS)
  AC_SUBST(PSPELL_LIBS)
  rm -f conf.pspelltest
])

