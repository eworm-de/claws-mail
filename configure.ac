dnl Process this file with autoconf to produce a configure script.
AC_PREREQ(2.50)
AC_INIT(src/main.c)
AC_CONFIG_AUX_DIR(config)

PACKAGE=sylpheed

dnl version number
MAJOR_VERSION=0
MINOR_VERSION=8
MICRO_VERSION=10
INTERFACE_AGE=0
BINARY_AGE=0
EXTRA_VERSION=claws49
VERSION=$MAJOR_VERSION.$MINOR_VERSION.$MICRO_VERSION$EXTRA_VERSION

dnl set $target
AC_CANONICAL_SYSTEM

dnl
AM_INIT_AUTOMAKE($PACKAGE, $VERSION, no-define)
dnl AC_DEFINE_UNQUOTED(PACKAGE, "$PACKAGE")
dnl AC_DEFINE_UNQUOTED(VERSION, "$VERSION")
AC_SUBST(PACKAGE)
AC_SUBST(VERSION)

dnl GNOME installed?
AC_PATH_PROG(GNOME_CONFIG, gnome-config, no)
if test "$GNOME_CONFIG" != no; then
	gnomedir="`gnome-config --prefix`"
	AC_SUBST(gnomedir)
fi
AM_CONDITIONAL(SYLPHEED_GNOME, test -d "$gnomedir")

dnl Claws version
AC_DEFINE(CLAWS, 1, Compiling Claws branch of sylpheed)

dnl libtool versioning
LT_RELEASE=$MAJOR_VERSION.$MINOR_VERSION
LT_CURRENT=`expr $MICRO_VERSION - $INTERFACE_AGE`
LT_REVISION=$INTERFACE_AGE
LT_AGE=`expr $BINARY_AGE - $INTERFACE_AGE`
AC_SUBST(LT_RELEASE)
AC_SUBST(LT_CURRENT)
AC_SUBST(LT_REVISION)
AC_SUBST(LT_AGE)

dnl Specify a header configuration file
AM_CONFIG_HEADER(config.h)

AM_MAINTAINER_MODE

dnl Checks for programs.
dnl AC_ARG_PROGRAM
AC_PROG_CC
AC_ISC_POSIX
AM_PROG_CC_STDC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_CPP
dnl AC_PROG_RANLIB
AM_PROG_LEX
AC_PROG_YACC
AM_PROG_LIBTOOL

SYLPHEED_ACLOCAL_INCLUDE(ac)

case "$target" in
*-darwin*)
	CFLAGS="$CFLAGS -traditional-cpp -fno-common"
	;;
esac

dnl for gettext
ALL_LINGUAS="bg cs de el en_GB es fr hr hu it ja ko nl pl pt_BR ru sr sv"
AM_GNU_GETTEXT
dnl AC_CHECK_FUNC(gettext, AC_CHECK_LIB(intl, gettext))
localedir='${prefix}/${DATADIRNAME}/locale'
AC_SUBST(localedir)

manualdir='${prefix}/${DATADIRNAME}/${PACKAGE}/manual'
AC_SUBST(manualdir)
faqdir='${prefix}/${DATADIRNAME}/${PACKAGE}/faq'
AC_SUBST(faqdir)

dnl Set PACKAGE_DATA_DIR in config.h.
if test "x${datadir}" = 'x${prefix}/share'; then
  if test "x${prefix}" = "xNONE"; then
    AC_DEFINE_UNQUOTED(PACKAGE_DATA_DIR, "${ac_default_prefix}/share/${PACKAGE}", PACKAGE_DATA_DIR)
  else
    AC_DEFINE_UNQUOTED(PACKAGE_DATA_DIR, "${prefix}/share/${PACKAGE}", PACKAGE_DATA_DIR)
  fi
else
  AC_DEFINE_UNQUOTED(PACKAGE_DATA_DIR, "${datadir}/${PACKAGE}", PACKAGE_DATA_DIR)
fi

AC_CHECK_LIB(xpg4, setlocale)

dnl for GThread support (currently disabled)
dnl AC_ARG_ENABLE(threads,
dnl	[  --enable-threads        Enable multithread support [default=no]],
dnl	[use_threads=$enableval], [use_threads=no])

AC_MSG_CHECKING([whether to use threads])
if test x"$use_threads" = xyes ; then
	AC_MSG_RESULT(yes)
	if test ! -z `$GLIB_CONFIG --help 2>&1 |grep 'gthread'` ; then
		CFLAGS="$CFLAGS `$GLIB_CONFIG --cflags gthread`"
		LIBS="$LIBS `$GLIB_CONFIG --libs gthread`"
		AC_DEFINE(USE_THREADS, 1, Whether to use multithread or not)
	else
		AC_MSG_ERROR([Sylpheed requires GThread from GLib to use threading.])
	fi
else
	AC_MSG_RESULT(no)
fi

dnl Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS(fcntl.h sys/file.h unistd.h paths.h \
		 sys/param.h sys/utsname.h sys/select.h \
		 wchar.h wctype.h)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_STRUCT_TM

dnl AC_CHECK_TYPE(wint_t, unsigned int) does not work because wint_t
dnl may be defined only in wchar.h (this happens with gcc-2.96).
dnl So we need to use this extended macro.
SYLPHEED_CHECK_TYPE(wint_t, unsigned int,
[
#if HAVE_WCHAR_H
#include <wchar.h>
#endif
], Define to `unsigned int' if <stddef.h> or <wchar.h> doesn't define.)

GNUPG_CHECK_TYPEDEF(u32, HAVE_U32_TYPEDEF, Used to test for a u32 typedef)
AC_CHECK_SIZEOF(unsigned short, 2)
AC_CHECK_SIZEOF(unsigned int, 4)
AC_CHECK_SIZEOF(unsigned long, 4)

dnl Checks for library functions.
AC_FUNC_ALLOCA
AC_CHECK_FUNCS(gethostname mkdir mktime socket strstr strchr \
	       wcsstr wcswcs iswalnum iswspace towlower \
	       wcslen wcscpy wcsncpy \
	       uname flock lockf inet_aton inet_addr \
	       fchmod mkstemp)

dnl *****************
dnl ** common code **
dnl *****************

dnl check for glib
AM_PATH_GLIB(1.2.6,,
	AC_MSG_ERROR(Test for GLIB failed. See the file 'INSTALL' for help.),
	gthread)

dnl check for IPv6 option
AC_ARG_ENABLE(ipv6,
	[  --enable-ipv6           Enable IPv6 support [default=no]],
	[ac_cv_enable_ipv6=$enableval], [ac_cv_enable_ipv6=no])

dnl automated checks for IPv6 support.
AC_MSG_CHECKING([whether to use IPv6])
if test x"$ac_cv_enable_ipv6" = xyes; then
	AC_MSG_RESULT(yes)
	AC_MSG_CHECKING([for IPv6 support])
	AC_CACHE_VAL(ac_cv_ipv6,[
		AC_TRY_COMPILE([#define INET6
				#include <sys/types.h>
				#include <netinet/in.h>],
			[int x = IPPROTO_IPV6; struct in6_addr a;],
			ac_cv_ipv6=yes, ac_cv_ipv6=no)
	])
	AC_MSG_RESULT($ac_cv_ipv6)
	if test $ac_cv_ipv6 = yes; then
		AC_DEFINE(INET6, 1, Define if you want IPv6 support.)
	else
		AC_MSG_WARN(*** IPv6 will not be supported ***)
		ac_cv_enable_ipv6=no
	fi
else
	AC_MSG_RESULT(no)
fi

dnl Check for OpenSSL
AM_PATH_OPENSSL

dnl ************************
dnl ** GTK user interface **
dnl ************************

dnl Checks for GTK
AM_PATH_GTK(1.2.6,,
	AC_MSG_ERROR(Test for GTK failed. See the file 'INSTALL' for help.))

dnl check if gdk / gtk was compiled with USE_XIM
AC_MSG_CHECKING([whether GTK was compiled with XIM support])
CFLAGS_SAVE="$CFLAGS"
LDFLAGS_SAVE="$LDFLAGS"
CFLAGS="$CFLAGS `$GTK_CONFIG --cflags`"
LDFLAGS="$LDFLAGS `$GTK_CONFIG --libs`"
AC_CACHE_VAL(ac_cv_use_xim, [
	AC_TRY_LINK_FUNC([gdk_ic_attr_new],
			 ac_cv_use_xim=yes, ac_cv_use_xim=no)
])
AC_MSG_RESULT($ac_cv_use_xim)
if test $ac_cv_use_xim = yes; then
	AC_DEFINE(USE_XIM, 1, Whether GTK was compiled with XIM support or not)
fi
CFLAGS="$CFLAGS_SAVE"
LDFLAGS="$LDFLAGS_SAVE"

dnl GNU/Aspell is used for spell checking
AC_ARG_ENABLE(aspell,
	[  --enable-aspell         Enable GNU/aspell support [default=no]],
	[ac_cv_enable_aspell=$enableval], [ac_cv_enable_aspell=no])
AC_MSG_CHECKING([whether to use GNU/aspell])
if test $ac_cv_enable_aspell = yes; then
	AC_MSG_RESULT(yes)
	AM_PATH_ASPELL(0.50, AC_DEFINE(USE_ASPELL, 1, Define if you use ASPELL to support spell checking),
		      [use_aspell=no ac_cv_enable_aspell=no])
else
	AC_MSG_RESULT(no)
fi

dnl want crash dialog
AC_ARG_ENABLE(crash-dialog,
	[  --enable-crash-dialog   Enable crash dialog [default=no]],
	[ac_cv_enable_crash_dialog=$enableval], [ac_cv_enable_crash_dialog=no])
if test $ac_cv_enable_crash_dialog = yes; then
dnl check if GDB is somewhere
	AC_CHECK_PROG(ac_cv_enable_crash_dialog, gdb, yes, no)
	AC_MSG_CHECKING([whether to use crash dialog])
	if test $ac_cv_enable_crash_dialog = yes; then
		AC_DEFINE(CRASH_DIALOG, 1, Pop up crash dialog)
	fi
	AC_MSG_RESULT($ac_cv_enable_crash_dialog)
fi

dnl Check for X-Face support
AC_ARG_ENABLE(compface,
	[  --disable-compface      Do not use compface (X-Face)],
	[ac_cv_enable_compface=$enableval], [ac_cv_enable_compface=yes])
if test "$ac_cv_enable_compface" = yes; then
	AC_CHECK_LIB(compface, uncompface,,[ac_cv_enable_compface=no])
fi

dnl for LDAP support in addressbook
dnl no check for libraries; dynamically loaded
AC_ARG_ENABLE(ldap,
	[  --enable-ldap           Enable LDAP support [default=no]],
	[ac_cv_enable_ldap=$enableval], [ac_cv_enable_ldap=no])
AC_MSG_CHECKING([whether to use LDAP])
if test "$ac_cv_enable_ldap" = yes; then
	AC_MSG_RESULT(yes)

	dnl check for available libraries, and pull them in
	AC_CHECK_LIB(resolv, res_query, LDAP_LIBS="$LDAP_LIBS -lresolv")
	AC_CHECK_LIB(socket, bind, LDAP_LIBS="$LDAP_LIBS -lsocket")
	AC_CHECK_LIB(nsl, gethostbyaddr, LDAP_LIBS="$LDAP_LIBS -lnsl")
	AC_CHECK_LIB(lber, ber_get_tag, LDAP_LIBS="$LDAP_LIBS -llber",,
		     $LDAP_LIBS)

	dnl we need libpthread for sylpheed ldap,  until we find
	dnl a better way to handle ldap requests asynchronously...
	AC_CHECK_LIB(pthread, pthread_create, LDAP_LIBS="$LDAP_LIBS -lpthread")

	AC_CHECK_HEADERS(ldap.h lber.h pthread.h,
			 [ ac_cv_enable_ldap=yes ],
			 [ ac_cv_enable_ldap=no ])

	if test "$ac_cv_enable_ldap" = yes; then
		AC_CHECK_LIB(ldap, ldap_open,
			     [ ac_cv_enable_ldap=yes ],
			     [ ac_cv_enable_ldap=no ],
			     $LDAP_LIBS)
	fi

	AC_MSG_CHECKING([whether ldap library is available])
	AC_MSG_RESULT($ac_cv_enable_ldap)

	if test "$ac_cv_enable_ldap" = yes; then
		CFLAGS="$CFLAGS `$GLIB_CONFIG --cflags gthread`"
		LDAP_LIBS="$LDAP_LIBS -lldap `$GLIB_CONFIG --libs gthread`"
		AC_DEFINE(USE_LDAP, 1, Define if you want LDAP support in addressbook.)
		AC_SUBST(LDAP_LIBS)
	fi
else
	AC_MSG_RESULT(no)
fi

dnl for JPilot support in addressbook
dnl no check for libraries; these are dynamically loaded
AC_ARG_ENABLE(jpilot,
	[  --enable-jpilot         Enable JPilot support [default=no]],
	[ac_cv_enable_jpilot=$enableval], [ac_cv_enable_jpilot=no])
AC_MSG_CHECKING([whether to use JPilot])
if test "$ac_cv_enable_jpilot" = yes; then
	AC_MSG_RESULT(yes)
	AC_CHECK_HEADERS(pi-args.h pi-appinfo.h pi-address.h,
			 [ AC_DEFINE(USE_JPILOT, 1, Define if you want JPilot support in addressbook.) ],
			 [ ac_cv_enable_jpilot=no ])
	if test "$ac_cv_enable_jpilot" = no; then
		AC_CHECK_HEADERS(libpisock/pi-args.h libpisock/pi-appinfo.h libpisock/pi-address.h,
				 [ ac_cv_enable_jpilot=yes
				   AC_DEFINE(USE_JPILOT, 1, Define if you want JPilot support in addressbook.) ])
	fi
	AC_MSG_CHECKING([whether jpilot is available])
	AC_MSG_RESULT($ac_cv_enable_jpilot)

	if test "$ac_cv_enable_jpilot" = yes; then
		LIBS="$LIBS -lpisock"
	fi
else
	AC_MSG_RESULT(no)
fi

dnl Check for built-in image view support
AC_ARG_ENABLE(gdk-pixbuf,
	[  --disable-gdk-pixbuf    Do not use gdk-pixbuf],
	[ac_cv_enable_gdk_pixbuf=$enableval], [ac_cv_enable_gdk_pixbuf=yes])
AC_ARG_ENABLE(imlib,
	[  --disable-imlib         Do not use imlib],
	[ac_cv_enable_imlib=$enableval], [ac_cv_enable_imlib=yes])

if test "$ac_cv_enable_gdk_pixbuf" = yes; then
	AM_PATH_GDK_PIXBUF(0.8.0,
		[AC_DEFINE(HAVE_GDK_PIXBUF, 1, Define if you use gdk-pixbuf to support image view)
		 ac_cv_enable_imlib=no], [ac_cv_enable_gdk_pixbuf=no])
fi
if test "$ac_cv_enable_imlib" = yes; then
	AM_PATH_GDK_IMLIB(1.9,
		AC_DEFINE(HAVE_GDK_IMLIB, 1, Define if you use gdk_imlib to support image view),
		[ac_cv_enable_imlib=no])
fi
if test "$ac_cv_enable_gdk_pixbuf" = no -a "$ac_cv_enable_imlib" = no; then
	AC_MSG_WARN(*** Built-in image view will not be supported ***)
fi

dnl GPGME is used to support OpenPGP 
AC_ARG_ENABLE(gpgme,
	[  --enable-gpgme          Enable GnuPG support using GPGME [default=no]],
	[ac_cv_enable_gpgme=$enableval], [ac_cv_enable_gpgme=no])
AC_MSG_CHECKING([whether to use GPGME])
if test $ac_cv_enable_gpgme = yes; then
	AC_MSG_RESULT(yes)
	AM_PATH_GPGME(0.3.10, AC_DEFINE(USE_GPGME, 1, Define if you use GPGME to support OpenPGP.),
		      [use_gpgme=no
		       ac_cv_enable_gpgme=no])
else
	AC_MSG_RESULT(no)
fi

dnl *************************
dnl ** section for plugins **
dnl *************************

PLUGINS=""

AC_ARG_ENABLE(demo-plugin,
	[  --enable-demo-plugin    Build demo plugin [default=no]],
	[ac_cv_enable_demo_plugin=$enableval], [ac_cv_enable_demo_plugin=no])
AM_CONDITIONAL(BUILD_DEMO_PLUGIN, test x"$ac_cv_enable_demo_plugin" = xyes)
if test x"$ac_cv_enable_demo_plugin" = xyes; then
	PLUGINS="demo $PLUGINS"
fi

AC_ARG_ENABLE(spamassassin-plugin,
	[  --enable-spamassassin-plugin    Build spamassassin plugin [default=no]],
	[ac_cv_enable_spamassassin_plugin=$enableval], [ac_cv_enable_spamassassin_plugin=no])
AM_CONDITIONAL(BUILD_SPAMASSASSIN_PLUGIN, test x"$ac_cv_enable_spamassassin_plugin" = xyes)
if test x"$ac_cv_enable_spamassassin_plugin" = xyes; then
	AC_SPAMASSASSIN
	PLUGINS="spamassassin $PLUGINS"
fi

dnl ****************************
dnl ** Final configure output **
dnl ****************************

AC_OUTPUT([
Makefile
sylpheed.spec
intl/Makefile
ac/Makefile
po/Makefile.in
src/common/version.h
src/Makefile
src/common/Makefile
src/gtk/Makefile
src/plugins/Makefile
src/plugins/demo/Makefile
src/plugins/spamassassin/Makefile
faq/Makefile
faq/de/Makefile
faq/en/Makefile
faq/es/Makefile
faq/fr/Makefile
faq/it/Makefile
man/Makefile
manual/Makefile
manual/de/Makefile
manual/en/Makefile
manual/es/Makefile
manual/fr/Makefile
manual/ja/Makefile
tools/Makefile
config/Makefile
])

dnl Output the configuration summary
echo ""
echo "$PACKAGE $VERSION"
echo ""
if test "$ac_cv_enable_gdk_pixbuf" = yes; then
	echo "image support : yes (gdk-pixbuf)"
elif test "$ac_cv_enable_imlib" = yes; then
	echo "image support : yes (gdk_imlib)"
else
	echo "image support : no"
fi
echo "GnuPG         : $ac_cv_enable_gpgme"
echo "JPilot        : $ac_cv_enable_jpilot"
echo "LDAP          : $ac_cv_enable_ldap"
echo "OpenSSL       : $ac_cv_enable_openssl"
echo "iconv         : $am_cv_func_iconv"
echo "compface      : $ac_cv_enable_compface"
echo "IPv6          : $ac_cv_enable_ipv6"
echo "GNU/aspell    : $ac_cv_enable_aspell"
echo "Crash dialog  : $ac_cv_enable_crash_dialog"
echo "Plugins       : $PLUGINS"
echo ""
echo "The binary will be installed in $prefix/bin"
echo ""
echo "Configure finished, type 'make' to build."
