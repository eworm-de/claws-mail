dnl Process this file with autoconf to produce a configure script.
AC_PREREQ(2.50)
AC_INIT(src/main.c)
AC_CONFIG_AUX_DIR(config)

PACKAGE=sylpheed-claws

dnl version number
MAJOR_VERSION=2
MINOR_VERSION=1
MICRO_VERSION=1
INTERFACE_AGE=0
BINARY_AGE=0
EXTRA_VERSION=26
EXTRA_RELEASE=
EXTRA_GTK2_VERSION=

if test \( $EXTRA_VERSION -eq 0 \) -o \( "x$EXTRA_RELEASE" != "x" \); then
    VERSION=${MAJOR_VERSION}.${MINOR_VERSION}.${MICRO_VERSION}${EXTRA_RELEASE}${EXTRA_GTK2_VERSION}
else
    VERSION=${MAJOR_VERSION}.${MINOR_VERSION}.${MICRO_VERSION}cvs${EXTRA_VERSION}${EXTRA_GTK2_VERSION}
fi

dnl set $target
AC_CANONICAL_SYSTEM

dnl
AM_INIT_AUTOMAKE($PACKAGE, $VERSION, no-define)
dnl AC_DEFINE_UNQUOTED(PACKAGE, "$PACKAGE")
dnl AC_DEFINE_UNQUOTED(VERSION, "$VERSION")
AC_SUBST(PACKAGE)
AC_SUBST(VERSION)
AC_SUBST(MAJOR_VERSION)
AC_SUBST(MINOR_VERSION)
AC_SUBST(MICRO_VERSION)
AC_SUBST(EXTRA_VERSION)

dnl GNOME installed?
AC_PATH_PROG(GNOME_CONFIG, gnome-config, no)
if test "$GNOME_CONFIG" != no; then
	gnomedatadir="`gnome-config --datadir`"
	gnomeprefix="`gnome-config --prefix`"
	if test "${prefix}" = "NONE"; then
		gnomedatadir="${ac_default_prefix}/${gnomedatadir#${gnomeprefix}}"
	else
		gnomedatadir="${prefix}/${gnomedatadir#${gnomeprefix}}"
	fi
	AC_SUBST(gnomedatadir)
fi
AM_CONDITIONAL(SYLPHEED_GNOME, test -n "$gnomedatadir")

dnl GNOME 2.x installed?
PKG_CHECK_MODULES(GNOME2, libgnome-2.0 >= 2.0, ac_enable_gnome2=yes, ac_enable_gnome2=no)
AM_CONDITIONAL(SYLPHEED_GNOME2, test x"$ac_enable_gnome2" = x"yes")

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
AC_CONFIG_HEADERS(config.h)

AM_MAINTAINER_MODE

dnl Checks for programs.
dnl AC_ARG_PROGRAM
AC_PROG_CC
AC_ISC_POSIX
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_CPP
dnl AC_PROG_RANLIB
AM_PROG_LEX
AC_PROG_YACC
AC_LIB_PREFIX
AC_LIBTOOL_WIN32_DLL
AC_LIBTOOL_RC
AC_PROG_LIBTOOL

SYLPHEED_ACLOCAL_INCLUDE(m4)
  
dnl ******************************
dnl Checks for host
dnl Not needed anymore because we 
dnl do AC_CANONICAL_SYSTEM above
dnl ******************************
dnl AC_CANONICAL_HOST

dnl Copied from the official gtk+-2 configure.in
AC_MSG_CHECKING([for some Win32 platform])
case "$host" in
  *-*-mingw*|*-*-cygwin*)
    platform_win32=yes
    LDFLAGS="$LDFLAGS -mwindows"
    ;;
  *)
    platform_win32=no
    ;;
esac
AC_MSG_RESULT([$platform_win32])
AM_CONDITIONAL(PLATFORM_WIN32, test x"$platform_win32" = x"yes")

AC_MSG_CHECKING([for native Win32])
case "$host" in
  *-*-mingw*)
    os_win32=yes
    ;;
  *)
    os_win32=no
    ;;
esac
AC_MSG_RESULT([$os_win32])
AM_CONDITIONAL(OS_WIN32, test x"$os_win32" = x"yes")

AC_MSG_CHECKING([for Cygwin])
case "$host" in
  *-*-cygwin*)
    env_cygwin=yes
    ;;
  *)
    env_cygwin=no
    ;;
esac
AC_MSG_RESULT([$env_cygwin])
AM_CONDITIONAL(CYGWIN, test x"$env_cygwin" = x"yes")

CFLAGS="$CFLAGS -Wall"

pthread_name=
case "$target" in
*-darwin*)
	CFLAGS="$CFLAGS -no-cpp-precomp -fno-common"
	;;
*-*-mingw*)
        # Note that we need to link to pthreadGC2 in all cases. This
        # is because some locking is used even when pthread support is
        # disabled.
        pthread_name=pthreadGC2
	CFLAGS="$CFLAGS -mms-bitfields"
	LIBS="$LIBS -l${pthread_name} -lws2_32 -lregex"
	;;
esac
  
dnl Checks for iconv
AM_ICONV

dnl
dnl Check for bind_textdomain_codeset, including -lintl if GLib brings it in.
dnl
syl_save_LIBS=$LIBS
LIBS="$LIBS $GTK_LIBS"
AC_CHECK_FUNCS(bind_textdomain_codeset)
LIBS=$syl_save_LIBS

dnl for gettext
ALL_LINGUAS="bg ca cs de el en_GB es fi fr hr hu it ja ko nb nl pl pt_BR ru sk sr sv zh_CN zh_TW"
GETTEXT_PACKAGE=sylpheed-claws
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [Define text domain.])

AM_GNU_GETTEXT_VERSION([0.14.5])
AM_GNU_GETTEXT([external])

if test "x${DATADIRNAME}" = "x"; then
  DATADIRNAME="share"
  AC_SUBST(DATADIRNAME)
fi

localedir='${prefix}/${DATADIRNAME}/locale'
AC_ARG_WITH(localedir,
	[  --with-localedir=DIR    Locale directory],
	[localedir="$withval"])
AC_SUBST(localedir)

manualdir='${prefix}/${DATADIRNAME}/${PACKAGE}/manual'
AC_ARG_WITH(manualdir,
	[  --with-manualdir=DIR    Manual directory],
	[manualdir="$withval"])
AC_SUBST(manualdir)

AC_ARG_ENABLE(manual,
	[  --disable-manual           Disable manual support],
	[ac_cv_enable_manual=$enableval], [ac_cv_enable_manual=yes])

AM_CONDITIONAL(BUILD_MANUAL, test x"$ac_cv_enable_manual" = xyes)

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

dnl Check for d_type member in struct dirent
AC_MSG_CHECKING([whether struct dirent has d_type member])
AC_CACHE_VAL(ac_cv_dirent_d_type,[
	AC_TRY_COMPILE([#include <dirent.h>],
		       [struct dirent d; d.d_type = DT_REG;],
		       ac_cv_dirent_d_type=yes, ac_cv_dirent_d_type=no)
])
AC_MSG_RESULT($ac_cv_dirent_d_type)
if test $ac_cv_dirent_d_type = yes; then
	AC_DEFINE(HAVE_DIRENT_D_TYPE, 1,
		  Define if `struct dirent' has `d_type' member.)
fi

# Check whether mkdir does not take the permission argument.
GNUPG_FUNC_MKDIR_TAKES_ONE_ARG

dnl Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS(fcntl.h sys/file.h unistd.h paths.h \
		 sys/param.h sys/utsname.h sys/select.h \
		 wchar.h wctype.h locale.h netdb.h)

dnl alf - Check for apache installation f*ck up. apache may also install an 
dnl fnmatch, which includes their own regex stuff if USE_HSREGEX is defined
AC_TRY_COMPILE([#include <stdlib.h>
		#include <fnmatch.h>],
	[int x = USE_HSREGEX;],
	ac_cv_have_apache_fnmatch=yes, ac_cv_have_apache_fnmatch=no)
if test $ac_cv_have_apache_fnmatch = yes; then
	AC_DEFINE(HAVE_APACHE_FNMATCH, 1, Define if you need to work around apache regex/fnmatch !KLUDGE!)
fi
AC_MSG_CHECKING([whether to use Apache regex header kludge])
AC_MSG_RESULT($ac_cv_have_apache_fnmatch)

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
	       fchmod mkstemp truncate getuid regcomp)

dnl *****************
dnl ** common code **
dnl *****************

dnl check for glib
AM_PATH_GLIB_2_0(2.0.0,,
	AC_MSG_ERROR(Test for GLIB failed. See the file 'INSTALL' for help.),
	gmodule gobject gthread)

dnl Check for bind_textdomain_codeset, including -lintl if GLib brings it in.
syl_save_LIBS=$LIBS
LIBS="$LIBS $GTK_LIBS"
AC_CHECK_FUNCS(bind_textdomain_codeset)
LIBS=$syl_save_LIBS

dnl check for IPv6 option
AC_ARG_ENABLE(ipv6,
	[  --disable-ipv6           Disable IPv6 support],
	[ac_cv_enable_ipv6=$enableval], [ac_cv_enable_ipv6=yes])

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
AC_ARG_ENABLE(openssl,
	[  --disable-openssl    Do not use OpenSSL for SSL support.],
	[ac_cv_enable_openssl=$enableval], [ac_cv_enable_openssl=yes])
AC_MSG_CHECKING([whether to use OpenSSL])
if test x"$ac_cv_enable_openssl" = xyes; then
	AC_MSG_RESULT(yes)
	PKG_CHECK_MODULES(OPENSSL, openssl >= 0.9.7, :, ac_cv_enable_openssl=no)
	if test x$ac_cv_enable_openssl = xyes; then
		AC_DEFINE(USE_OPENSSL, 1, Define if you want OpenSSL support)
	else
		AC_MSG_RESULT(not found)
		AC_MSG_WARN([*** OpenSSL will not be supported ***])
	fi
else
	AC_MSG_RESULT(no)
fi
AC_SUBST(OPENSSL_CFLAGS)
AC_SUBST(OPENSSL_LIBS)

dnl password encryption
OLDLIBS=$LIBS
LIBS=
case $host_os in
	*dragonfly*)
		AC_SEARCH_LIBS(encrypt, cipher, [], AC_MSG_ERROR(['encrypt'-function not found.]))
	;;
	*)
		AC_SEARCH_LIBS(encrypt, crypt, [], AC_MSG_ERROR(['encrypt'-function not found.]))
	;;
esac
CRYPT_LIBS=$LIBS
AC_SUBST(CRYPT_LIBS)
LIBS=$OLDLIBS

AC_ARG_WITH(passcrypt-key, [  --with-passcrypt-key=KEY     Key used to encode passwords (8 byte string)],
	    with_passcrypt_key="$withval", with_passcrypt_key="passkey0")
AC_SUBST(PASSCRYPT_KEY, $with_passcrypt_key)

dnl RC dir (will be default at a certain point in time)
AC_ARG_WITH(config-dir,    [  --with-config-dir=RCDIR      Local configuration dir (default: .sylpheed-claws)],
	      ac_cv_with_config_dir="$withval", ac_cv_with_config_dir=".sylpheed-claws")
if test x"$ac_cv_with_config_dir" = x""; then
	ac_cv_with_config_dir=".sylpheed-claws"
fi 
AC_DEFINE_UNQUOTED(CFG_RC_DIR, "$ac_cv_with_config_dir", Configuration directory)

dnl ************************
dnl ** GTK user interface **
dnl ************************

dnl Checks for GTK
AM_PATH_GTK_2_0(2.4.0,,
	AC_MSG_ERROR(Test for GTK failed. See the file 'INSTALL' for help.))

dnl --disable-deprecated switch for GTK2 purification
AC_ARG_ENABLE(deprecated, [  --disable-deprecated  Disable deprecated GTK functions. ],
			  [GTK_CFLAGS="$GTK_CFLAGS -DG_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED"], [])

AC_SUBST(GTK_CFLAGS)
AC_SUBST(GTK_LIBS)

dnl GNU/Aspell is used for spell checking
AC_ARG_ENABLE(aspell,
	[  --disable-aspell         Disable GNU/aspell support [default=yes]],
	[ac_cv_enable_aspell=$enableval], [ac_cv_enable_aspell=yes])
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
AC_MSG_CHECKING([whether to use compface])
if test x"$ac_cv_enable_compface" = xyes; then
	AC_MSG_RESULT(yes)
	AC_CHECK_LIB(compface, uncompface, 
		[AC_DEFINE(HAVE_LIBCOMPFACE, 1, Define if you want compface support.)],
		[ac_cv_enable_compface=no])
	if test x"$ac_cv_enable_compface" = xyes; then
		COMPFACE_LIBS="-lcompface"
	else
		COMPFACE_LIBS=""
	fi
	AC_SUBST(COMPFACE_LIBS)
else
	AC_MSG_RESULT(no)
fi

dnl check for pthread support
AC_ARG_ENABLE(pthread,
	[  --disable-pthread           Disable pthread support],
	[ac_cv_enable_pthread=$enableval], [ac_cv_enable_pthread=yes])
AC_MSG_CHECKING([whether to use pthread])
if test x$ac_cv_enable_pthread = xno; then
	AC_MSG_RESULT(no)
else
	AC_MSG_RESULT(yes)

        # For W32 we need to use a special ptrhead lib. In this case we can't
        # use AC_CHECK_LIB because it has no means of checking for a
        # library installed under a different name.  Checking for the
        # header is okay.
        if test -n "${pthread_name}" ; then
           ac_cv_enable_pthread=yes
        else
	AC_CHECK_LIB(pthread, pthread_create, :, ac_cv_enable_pthread=no)
        fi
	AC_CHECK_HEADERS(pthread.h, :, ac_cv_enable_pthread=no)

	if test x$ac_cv_enable_pthread = xyes; then
		AC_DEFINE(USE_PTHREAD, 1, Define if you have pthread)
                if test -z "${pthread_name}" ; then
		PTHREAD_LIBS="-lpthread"
	fi
	fi

fi
AC_SUBST(PTHREAD_LIBS)

dnl
dnl Check whether we need to pass -lresolv
dnl We know that we don't need it for W32.
dnl
if test x$os_win32 = xno; then
  t_oldLibs="$LIBS"
  LIBS="$LIBS"
  ac_cv_var__res_options=no
  AC_TRY_LINK([#include <sys/types.h>
 	     #include <sys/socket.h>
	     #include <netinet/in.h>
 	     #include <arpa/nameser.h>
	     #include <resolv.h>],
 	  	[_res.options = RES_INIT;],
 	 	ac_cv_var__res_options=yes);
  if test "$ac_cv_var__res_options" != "yes"; then
	LIBRESOLV="-lresolv"
  fi
  LIBS="$t_oldLibs"

  if test "x$LIBRESOLV" = "x"; then
	AC_CHECK_LIB(resolv, res_query, [LIBRESOLV=-lresolv])
	LIBS="$t_oldLibs"
  fi
fi
AC_SUBST(LIBRESOLV)

LIBS="$LIBS $LIBRESOLV"
 	  	 
dnl for LDAP support in addressbook
dnl no check for libraries; dynamically loaded
AC_ARG_ENABLE(ldap,
	[  --disable-ldap         Do not build LDAP support [default=yes]],
	[ac_cv_enable_ldap=$enableval], [ac_cv_enable_ldap=yes])
AC_MSG_CHECKING([whether to use LDAP])
if test x"$ac_cv_enable_ldap" = xno; then
	AC_MSG_RESULT(no)
elif test x"$ac_cv_enable_ldap" = xyes -a x"$ac_cv_enable_pthread" = xno; then
	AC_MSG_RESULT(no - LDAP support needs pthread support)

	ac_cv_enable_ldap=no
else
	AC_MSG_RESULT(yes)

	dnl check for available libraries, and pull them in
	AC_CHECK_LIB(resolv, res_query, LDAP_LIBS="$LDAP_LIBS -lresolv")
	AC_CHECK_LIB(socket, bind, LDAP_LIBS="$LDAP_LIBS -lsocket")
	AC_CHECK_LIB(nsl, gethostbyaddr, LDAP_LIBS="$LDAP_LIBS -lnsl")
	AC_CHECK_LIB(lber, ber_get_tag, LDAP_LIBS="$LDAP_LIBS -llber",,
		     $LDAP_LIBS)

	AC_CHECK_HEADERS(ldap.h lber.h,
			 [ ac_cv_enable_ldap=yes ],
			 [ ac_cv_enable_ldap=no ])

	if test "$ac_cv_enable_ldap" = yes; then
		AC_CHECK_LIB(ldap, ldap_open,
			     [ ac_cv_enable_ldap=yes ],
			     [ ac_cv_enable_ldap=no ],
			     $LDAP_LIBS)

		AC_CHECK_LIB(ldap, ldap_start_tls_s,
	     		     [ ac_cv_have_tls=yes ],
	     		     [ ac_cv_have_tls=no ])

	fi

	AC_MSG_CHECKING([whether ldap library is available])
	AC_MSG_RESULT($ac_cv_enable_ldap)

	AC_MSG_CHECKING([whether TLS library is available])
	AC_MSG_RESULT($ac_cv_have_tls)

	if test "$ac_cv_enable_ldap" = yes; then
		AC_DEFINE(USE_LDAP, 1, Define if you want LDAP support in addressbook.)
		LDAP_LIBS="$LDAP_LIBS -lldap"
		AC_SUBST(LDAP_LIBS)
		if test "$ac_cv_have_tls" = yes; then
			AC_DEFINE(USE_LDAP_TLS, 1, Define if you want LDAP TLS support in addressbook.)
		fi
	fi
fi

dnl for JPilot support in addressbook
dnl no check for libraries; these are dynamically loaded
AC_ARG_ENABLE(jpilot,
	[  --disable-jpilot         Enable JPilot support [default=yes]],
	[ac_cv_enable_jpilot=$enableval], [ac_cv_enable_jpilot=yes])
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

	AC_CHECK_LIB(pisock, unpack_Address, [JPILOT_LIBS="-lpisock"], [JPILOT_LIBS="" ac_cv_enable_jpilot="no"])
	if test x"$ac_cv_enable_jpilot" = xyes; then
		AC_DEFINE(USE_JPILOT, 1, Define if you want JPilot support in addressbook.)
	else
		AC_MSG_NOTICE([JPilot support not available])
	fi
	AC_SUBST(JPILOT_LIBS)
else
	AC_MSG_RESULT(no)
fi

dnl #######################################################################
dnl # Check for startup notification
dnl #######################################################################
AC_ARG_ENABLE(startup-notification, [  --disable-startup-notification    compile without startup notification support],,enable_startup_notification=yes)

if test "x$enable_startup_notification" = "xyes"; then
	PKG_CHECK_MODULES(STARTUP_NOTIFICATION, libstartup-notification-1.0 >= 0.5,
	[
		AC_DEFINE(HAVE_STARTUP_NOTIFICATION, 1, [Define if we're using libstartup-notification.])
		echo "Building with libstartup-notification"
		enable_startup_notification=yes
	],
	[
		echo "Building without libstartup-notification"
		enable_startup_notification=no
	])

	AC_SUBST(STARTUP_NOTIFICATION_CFLAGS)
	AC_SUBST(STARTUP_NOTIFICATION_LIBS)
fi

dnl *************************
dnl ** section for plugins **
dnl *************************

PLUGINS=""

dnl --- Trayicon ---
AC_MSG_CHECKING([whether to build Trayicon plugin])
AC_ARG_ENABLE(trayicon-plugin,
	[  --disable-trayicon-plugin         Do not build System Tray Icon plugin],
	[ac_cv_enable_trayicon_plugin=$enableval], [ac_cv_enable_trayicon_plugin=yes])
if test x"$ac_cv_enable_trayicon_plugin" = xyes; then
	AC_MSG_RESULT(yes)
	PLUGINS="trayicon $PLUGINS"
else
	AC_MSG_RESULT(no)
fi
AM_CONDITIONAL(BUILD_TRAYICON_PLUGIN, test x"$ac_cv_enable_trayicon_plugin" = xyes)

dnl --- SpamAssassin ---
AC_MSG_CHECKING([whether to build SpamAssassin plugin])
AC_ARG_ENABLE(spamassassin-plugin,
	[  --disable-spamassassin-plugin      Build SpamAssassin plugin [default=yes]],
	[ac_cv_enable_spamassassin_plugin=$enableval], [ac_cv_enable_spamassassin_plugin=yes])
if test x"$ac_cv_enable_spamassassin_plugin" = xyes; then
	AC_MSG_RESULT(yes)
	AC_SPAMASSASSIN
	PLUGINS="spamassassin $PLUGINS"
	AC_DEFINE(USE_SPAMASSASSIN_PLUGIN, 1, Define if spamassassin plugin is being build.)
else
	AC_MSG_RESULT(no)
fi
AM_CONDITIONAL(BUILD_SPAMASSASSIN_PLUGIN, test x"$ac_cv_enable_spamassassin_plugin" = xyes)

dnl --- PGP/CORE ---
AC_MSG_CHECKING([whether to build PGP/CORE plugin])
AC_ARG_ENABLE(pgpcore-plugin,
	[  --disable-pgpcore-plugin           Do not build PGP/Core plugin],
	[ac_cv_enable_pgpcore_plugin=$enableval], [ac_cv_enable_pgpcore_plugin=yes])
if test x"$ac_cv_enable_pgpcore_plugin" = xyes; then
	AC_MSG_RESULT(yes)
	AM_PATH_GPGME(1.0.0, AC_DEFINE(USE_GPGME, 1, Define if you use GPGME to support OpenPGP.),
		      [ac_cv_enable_pgpcore_plugin=no])
	if test x"$ac_cv_enable_pgpcore_plugin" = xyes; then
		PLUGINS="pgpcore $PLUGINS"
                AM_PATH_GPGME(1.1.1,
                              AC_DEFINE(HAVE_GPGME_PKA_TRUST, 1,
                                        [Define if GPGME supports PKA.]))
		#needed to get GPGME_LIBS and al correctly
		AM_PATH_GPGME(1.0.0, AC_DEFINE(USE_GPGME, 1, Define if you use GPGME to support OpenPGP.),
		    	  [ac_cv_enable_pgpcore_plugin=no])
	else
		AC_MSG_WARN([*** PGP/CORE plugin will not be built ***])
	fi
else
	AC_MSG_RESULT(no)
fi
AM_CONDITIONAL(BUILD_PGPCORE_PLUGIN, test x"$ac_cv_enable_pgpcore_plugin" = xyes)

AC_MSG_CHECKING([whether to build PGP/MIME plugin])
AC_ARG_ENABLE(pgpmime-plugin,
	[  --disable-pgpmime-plugin           Do not build PGP/MIME plugin],
	[ac_cv_enable_pgpmime_plugin=$enableval], [ac_cv_enable_pgpmime_plugin=yes])
if test x"$ac_cv_enable_pgpmime_plugin" = xyes; then
	AC_MSG_RESULT(yes)
	if test x"$ac_cv_enable_pgpcore_plugin" = xyes; then
		PLUGINS="pgpmime $PLUGINS"
	else
		AC_MSG_WARN([*** PGP/MIME plugin cannot be built ***])
		AC_MSG_WARN([*** without the PGP/CORE plugin     ***])
	fi
else
	AC_MSG_RESULT(no)
fi
AM_CONDITIONAL(BUILD_PGPMIME_PLUGIN, test x"$ac_cv_enable_pgpmime_plugin" = xyes)

AC_MSG_CHECKING([whether to build PGP/Inline plugin])
AC_ARG_ENABLE(pgpinline-plugin,
	[  --disable-pgpinline-plugin           Do not build PGP/Inline plugin],
	[ac_cv_enable_pgpinline_plugin=$enableval], [ac_cv_enable_pgpinline_plugin=yes])
if test x"$ac_cv_enable_pgpinline_plugin" = xyes; then
	AC_MSG_RESULT(yes)
	if test x"$ac_cv_enable_pgpcore_plugin" = xyes; then
		PLUGINS="pgpinline $PLUGINS"
	else
		AC_MSG_WARN([*** PGP/Inline plugin cannot be built ***])
		AC_MSG_WARN([*** without the PGP/CORE plugin     ***])
	fi
else
	AC_MSG_RESULT(no)
fi
AM_CONDITIONAL(BUILD_PGPINLINE_PLUGIN, test x"$ac_cv_enable_pgpinline_plugin" = xyes)

dnl --- Dillo Viewer ---
AC_MSG_CHECKING([whether to build Dillo plugin])
AC_ARG_ENABLE(dillo-viewer-plugin,
	[  --disable-dillo-viewer-plugin     Do not build Dillo plugin for html mail rendering],
	[ac_cv_enable_dillo_viewer_plugin=$enableval], [ac_cv_enable_dillo_viewer_plugin=yes])
if test x"$ac_cv_enable_dillo_viewer_plugin" = xyes; then
	AC_MSG_RESULT(yes)
	PLUGINS="dillo-viewer $PLUGINS"
else
	AC_MSG_RESULT(no)
fi
AM_CONDITIONAL(BUILD_DILLO_VIEWER_PLUGIN, test x"$ac_cv_enable_dillo_viewer_plugin" = xyes)

dnl --- Demo ---
AC_ARG_ENABLE(demo-plugin,
	[  --enable-demo-plugin              Build demo plugin [default=no]],
	[ac_cv_enable_demo_plugin=$enableval], [ac_cv_enable_demo_plugin=no])
if test x"$ac_cv_enable_demo_plugin" = xyes; then
	PLUGINS="demo $PLUGINS"
fi
AM_CONDITIONAL(BUILD_DEMO_PLUGIN, test x"$ac_cv_enable_demo_plugin" = xyes)

dnl --- ClamAV ---
AC_MSG_CHECKING([whether to build ClamAV plugin])
AC_ARG_ENABLE(clamav-plugin,
	[  --disable-clamav-plugin           Do not build Clam AntiVirus plugin],
	[ac_cv_enable_clamav_plugin=$enableval], [ac_cv_enable_clamav_plugin=yes])
if test x"$ac_cv_enable_clamav_plugin" = xyes; then
	AC_MSG_RESULT(yes)
	PKG_CHECK_MODULES(CLAMAV, libclamav, ac_cv_enable_clamav_plugin=yes,
			  ac_cv_enable_clamav_plugin=no)
	if test x"$ac_cv_enable_clamav_plugin" = xyes; then
		PLUGINS="clamav $PLUGINS"
	else
		AC_MSG_RESULT(not found)
		AC_MSG_WARN([*** ClamAV plugin will not be built ***])
	fi
else
	AC_MSG_RESULT(no)
fi
AC_SUBST(CLAMAV_LIBS)
AM_CONDITIONAL(BUILD_CLAMAV_PLUGIN, test x"$ac_cv_enable_clamav_plugin" = xyes)

dnl Libetpan
AC_MSG_CHECKING([whether to use libetpan])
AC_ARG_ENABLE(libetpan,
	[  --disable-libetpan		Do not compile IMAP4 support with libetpan],
	[ac_cv_enable_libetpan=$enableval], [ac_cv_enable_libetpan=yes])
if test x"$ac_cv_enable_libetpan" = xyes; then
	AC_MSG_RESULT(yes)
	libetpan_result=no
	AC_PATH_PROG(libetpanconfig, [libetpan-config])
	if test "x$libetpanconfig" != "x"; then
	  CPPFLAGS="$CPPFLAGS `$libetpanconfig --cflags 2>/dev/null`"
	  AC_CHECK_HEADER(libetpan/libetpan.h, [libetpan_result=yes])
	  if test "x$libetpan_result" = "xyes"; then
	    AC_MSG_CHECKING([whether libetpan-config hints compiles and links fine])
	    LIBS="$LIBS `$libetpanconfig --libs 2>/dev/null`"
	    AC_TRY_LINK([#include <libetpan/dbstorage.h>], [db_mailstorage_init(NULL, NULL);], [libetpan_result=yes], [libetpan_result=no])
	    AC_MSG_RESULT([$libetpan_result])
	  fi
	fi
	if test "x$libetpan_result" = "xyes"; then
	   LIBETPAN_CPPFLAGS="`$libetpanconfig --cflags`"
	   LIBETPAN_LIBS="`$libetpanconfig --libs`"
	   LIBETPAN_VERSION=`$libetpanconfig --version | sed "s/\.//g" | sed "s/-.*$//"`
	   if test "$LIBETPAN_VERSION" -lt "045"; then
		AC_MSG_RESULT([*** Sylpheed requires libetpan 0.45 or newer. See http://www.etpan.org/])
		AC_MSG_RESULT([*** You can use --disable-libetpan if you don't need IMAP4 support.])
                AC_MSG_ERROR([libetpan 0.45 not found])
	   fi
	   AC_SUBST(LIBETPAN_FLAGS)
	   AC_SUBST(LIBETPAN_LIBS)
	   AC_DEFINE(HAVE_LIBETPAN, 1, Define if you want IMAP support.)
	else
	   AC_MSG_RESULT([*** Sylpheed requires libetpan 0.45 or newer. See http://www.etpan.org/ ])
	   AC_MSG_RESULT([*** You can use --disable-libetpan if you don't need IMAP4 support.])
           AC_MSG_ERROR([libetpan 0.45 not found])
	fi
else
	AC_MSG_RESULT(no)
fi
AM_CONDITIONAL(SYLPHEED_LIBETPAN, test "x$libetpan_result" = "xyes")


dnl Libgnomeprint
AC_MSG_CHECKING([whether to use libgnomeprint])
AC_ARG_ENABLE(gnomeprint,
	[  --disable-gnomeprint		Do not use libgnomeprint for printing],
	[ac_cv_enable_gnomeprint=$enableval], [ac_cv_enable_gnomeprint=yes])
if test x$ac_cv_enable_gnomeprint = xyes; then
	AC_MSG_RESULT(yes)
	PKG_CHECK_MODULES(GNOMEPRINT, libgnomeprint-2.2 libgnomeprintui-2.2,
			  ac_cv_enable_gnomeprint=yes, ac_cv_enable_gnomeprint=no)
	if test x"$ac_cv_enable_gnomeprint" = xyes; then
		AC_DEFINE(USE_GNOMEPRINT, 1, Define if you want libgnomeprint support)
	else
		AC_MSG_RESULT(not found)
     		AC_MSG_WARN([*** libgnomeprintui wasn't found    ***])
		AC_MSG_WARN([*** using built-in printing support ***])
	fi
else
	AC_MSG_RESULT(no)
fi
AM_CONDITIONAL(SYLPHEED_GNOMEPRINT, test x"$ac_cv_enable_gnomeprint" = x"yes")

dnl ****************************
dnl ** Final configure output **
dnl ****************************

AC_OUTPUT([
Makefile
m4/Makefile
po/Makefile.in
src/common/version.h
src/Makefile
src/common/Makefile
src/common/passcrypt.h
src/gtk/Makefile
src/etpan/Makefile
src/plugins/Makefile
src/plugins/demo/Makefile
src/plugins/spamassassin/Makefile
src/plugins/dillo_viewer/Makefile
src/plugins/trayicon/Makefile
src/plugins/trayicon/libeggtrayicon/Makefile
src/plugins/clamav/Makefile
src/plugins/pgpcore/Makefile
src/plugins/pgpmime/Makefile
src/plugins/pgpinline/Makefile
doc/Makefile
doc/man/Makefile
tools/Makefile
config/Makefile
manual/Makefile
manual/dtd/Makefile
manual/dist/Makefile
manual/dist/pdf/Makefile
manual/dist/ps/Makefile
manual/dist/html/Makefile
manual/dist/txt/Makefile
manual/fr/Makefile
manual/fr/dist/Makefile
manual/fr/dist/pdf/Makefile
manual/fr/dist/ps/Makefile
manual/fr/dist/html/Makefile
manual/fr/dist/txt/Makefile
sylpheed-claws.pc
])

dnl Output the configuration summary
echo ""
echo "$PACKAGE $VERSION"
echo ""
echo "JPilot        : $ac_cv_enable_jpilot"
echo "LDAP          : $ac_cv_enable_ldap"
echo "OpenSSL       : $ac_cv_enable_openssl"
echo "iconv         : $am_cv_func_iconv"
echo "compface      : $ac_cv_enable_compface"
echo "IPv6          : $ac_cv_enable_ipv6"
echo "GNU/aspell    : $ac_cv_enable_aspell"
echo "IMAP4         : $ac_cv_enable_libetpan"
echo "Crash dialog  : $ac_cv_enable_crash_dialog"
echo "Libgnomeprint : $ac_cv_enable_gnomeprint"
echo "Manual        : $ac_cv_enable_manual"
echo "Plugins       : $PLUGINS"
echo "Config dir    : $ac_cv_with_config_dir"
echo ""
echo "The binary will be installed in $prefix/bin"
echo ""
echo "Configure finished, type 'make' to build."
