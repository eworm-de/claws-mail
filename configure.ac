
AC_PREREQ(2.59)
AC_INIT(src/main.c)
AC_CONFIG_AUX_DIR(config)
AC_CONFIG_MACRO_DIR([m4])

PACKAGE=claws-mail

dnl version number
INTERFACE_AGE=0
BINARY_AGE=0
EXTRA_RELEASE=
EXTRA_GTK2_VERSION=

if test \( -d .git \); then
    AC_CHECK_PROG([GIT], [git], [yes], [no], [$PATH])
    if test \( "$GIT" = "no" \); then
	AC_MSG_ERROR([*** git not found. See http://git-scm.com/])
    else
	GIT_VERSION=`git describe --abbrev=6 --dirty --always`
	echo "echo ${GIT_VERSION}" > ./version
    fi
else
    GIT_VERSION=`sh -c ". $srcdir/version"`
fi

if test \( -z "$GIT_VERSION" \); then
	AC_MSG_ERROR([*** could not determine program version])
fi

MAJOR_VERSION=${GIT_VERSION%%.*}
MINOR_VERSION=${GIT_VERSION#*.}
MINOR_VERSION=${MINOR_VERSION%%.*}
MICRO_VERSION=${GIT_VERSION##*.}
MICRO_VERSION=${MICRO_VERSION%%-*}
EXTRA_VERSION=${GIT_VERSION#*-}
EXTRA_VERSION=${EXTRA_VERSION%%-*}

if test \( "x$EXTRA_VERSION" != "x" -a `echo -n $EXTRA_VERSION | wc -c` -lt 5 \); then
    VERSION=${MAJOR_VERSION}.${MINOR_VERSION}.${MICRO_VERSION}git${EXTRA_VERSION}
else
    VERSION=${MAJOR_VERSION}.${MINOR_VERSION}.${MICRO_VERSION}
    EXTRA_VERSION=0
fi

if test \( "x$EXTRA_RELEASE" != "x" \); then
    VERSION=${VERSION}${EXTRA_RELEASE}${EXTRA_GTK2_VERSION}
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
AC_SUBST(GIT_VERSION)

AC_CHECK_PROG(HAVE_GTK_ICON_CACHE, gtk-update-icon-cache, yes, no)
AM_CONDITIONAL(UPDATE_GTK_ICON_CACHE, test x"$HAVE_GTK_ICON_CACHE" = xyes)

dnl Require pkg-config
m4_ifndef([PKG_PROG_PKG_CONFIG],
    [m4_fatal([Could not locate the pkg-config autoconf macros. These
are usually located in /usr/share/aclocal/pkg.m4. If your macros
are in a different location, try setting the environment variable
ACLOCAL_FLAGS before running ./autogen.sh or autoreconf again. E.g.:
export ACLOCAL_FLAGS="-I/other/macro/dir"])
])
PKG_PROG_PKG_CONFIG

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
AC_CONFIG_HEADERS(claws-features.h)

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
LT_INIT
LT_AC_PROG_RC
AC_LIBTOOL_RC
AC_PROG_LIBTOOL
AC_PROG_AWK

dnl AC_PROG_CXX will set CXX=g++ even if it finds no useable C++
dnl compiler, so we have to check whether the program named by
dnl CXX exists.
AC_PROG_CXX
AC_PATH_PROG(REAL_CXX, $CXX)
HAVE_CXX=no
if test -n "$REAL_CXX"; then
	HAVE_CXX=yes
fi

AC_SYS_LARGEFILE

dnl ******************************
dnl Checks for host
dnl Not needed anymore because we
dnl do AC_CANONICAL_SYSTEM above
dnl ******************************
dnl AC_CANONICAL_HOST

dnl Copied from the official gtk+-2 configure.in
AC_MSG_CHECKING([for host platform])
case "$host" in
  *-*-mingw*|*-*-cygwin*)
    platform_win32=yes
    LDFLAGS="$LDFLAGS -mwindows -Wl,--export-all-symbols"
    ;;
	*-apple-*)
		platform_osx=yes
		LDFLAGS="$LDFLAGS -Wl,-export_dynamic"
		;;
  *)
    platform_win32=no
		platform_osx=no
		LDFLAGS="$LDFLAGS -Wl,--export-dynamic"
    ;;
esac
AM_CONDITIONAL(PLATFORM_WIN32, test x"$platform_win32" = x"yes")
AM_CONDITIONAL(PLATFORM_OSX, test x"$platform_osx" = x"yes")
AC_MSG_RESULT([$host])

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

if test "$GCC" = "yes"
then
	CFLAGS="$CFLAGS -Wno-unused-function"
	#CFLAGS="-g -Wall -Wno-unused-function"
fi

AC_MSG_CHECKING([if gcc supports -Wno-pointer-sign])
_gcc_cflags_save=$CFLAGS
CFLAGS="-Wno-pointer-sign"
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([])],_gcc_psign=yes,_gcc_psign=no)
AC_MSG_RESULT($_gcc_psign)
CFLAGS=$_gcc_cflags_save;
if test x"$_gcc_psign" = xyes ; then
	CFLAGS="$CFLAGS -Wno-pointer-sign"
fi

CFLAGS="$CFLAGS -Wall -D_GNU_SOURCE"

if test $USE_MAINTAINER_MODE = yes; then
	CFLAGS="$CFLAGS -g -Wno-pointer-sign -DUSE_MAINTAINER_MODE"
fi

pthread_name=
case "$target" in
*-darwin*)
	CFLAGS="$CFLAGS -no-cpp-precomp -fno-common"
	;;
*-*-mingw*)
        # Note that we need to link to pthread in all cases. This
        # is because some locking is used even when pthread support is
        # disabled.
        pthread_name=pthread
	CFLAGS="$CFLAGS -mms-bitfields"
	LIBS="$LIBS -l${pthread_name} -lws2_32 -lregex"
	;;
*-*-solaris*)
	CPPFLAGS="$CPPFLAGS -D_POSIX_PTHREAD_SEMANTICS"
	CFLAGS="$CFLAGS -std=gnu99 -DSOLARIS"
	;;
esac

dnl Checks for iconv
AM_ICONV

dnl floor and ceil are  in -lm
LIBS="$LIBS -lm"

dnl
dnl Check for bind_textdomain_codeset, including -lintl if GLib brings it in.
dnl
syl_save_LIBS=$LIBS
LIBS="$LIBS $GTK_LIBS"
AC_CHECK_FUNCS(bind_textdomain_codeset)
LIBS=$syl_save_LIBS

dnl for gettext
ALL_LINGUAS="ca cs da de el_GR en_GB es fi fr hu id_ID it ja nb nl pl pt_BR pt_PT ro ru sk sv tr zh_TW"
GETTEXT_PACKAGE=claws-mail
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [Define text domain.])

AM_GNU_GETTEXT_VERSION([0.18])
AM_GNU_GETTEXT([external])

AC_ARG_ENABLE(manual,
		[  --disable-manual                Do not build user manual],
		[enable_manual=$enableval], [enable_manual=yes])

AC_ARG_ENABLE(libsm,
		[  --disable-libsm                 Do not build libSM support for session management],
		[enable_libsm=$enableval], [enable_libsm=yes])

AC_ARG_ENABLE(ipv6,
		[  --disable-ipv6                  Do not build IPv6 support],
		[enable_ipv6=$enableval], [enable_ipv6=yes])

AC_ARG_ENABLE(gnutls,
		[  --disable-gnutls                Do not build GnuTLS support for SSL/TLS],
		    [enable_gnutls=$enableval], [enable_gnutls=yes])

AC_ARG_ENABLE(enchant,
		[  --disable-enchant               Do not build Enchant support for spell-checking],
		[enable_enchant=$enableval], [enable_enchant=yes])

AC_ARG_ENABLE(crash-dialog,
		[  --enable-crash-dialog           Build crash dialog],
		[enable_crash_dialog=$enableval], [enable_crash_dialog=no])

AC_ARG_ENABLE(generic-umpc,
		[  --enable-generic-umpc           Build generic UMPC code],
		[enable_generic_umpc=$enableval], [enable_generic_umpc=no])

AC_ARG_ENABLE(compface,
		[  --disable-compface              Do not build compface support for X-Face],
		[enable_compface=$enableval], [enable_compface=yes])

AC_ARG_ENABLE(pthread,
		[  --disable-pthread               Do not build pthread support],
		[enable_pthread=$enableval], [enable_pthread=yes])

AC_ARG_ENABLE(startup-notification,
		[  --disable-startup-notification  Do not startup notification support],
		[enable_startup_notification=$enableval], [enable_startup_notification=yes])

AC_ARG_ENABLE(dbus,
		[  --disable-dbus                  Do not build DBUS support],
		[enable_dbus=$enableval], [enable_dbus=yes])

AC_ARG_ENABLE(ldap,
		[  --disable-ldap                  Do not build LDAP support],
		[enable_ldap=$enableval], [enable_ldap=yes])

AC_ARG_ENABLE(jpilot,
		[  --disable-jpilot                Do not build JPilot support],
		[enable_jpilot=$enableval], [enable_jpilot=yes])

AC_ARG_ENABLE(networkmanager,
		[  --disable-networkmanager        Do not build NetworkManager support],
		[enable_networkmanager=$enableval], [enable_networkmanager=yes])

AC_ARG_ENABLE(libetpan,
		[  --disable-libetpan              Do not build libetpan support for IMAP4/NNTP],
		[enable_libetpan=$enableval], [enable_libetpan=yes])

AC_ARG_ENABLE(valgrind,
		[  --disable-valgrind              Do not build valgrind support for debugging],
		[enable_valgrind=$enableval], [enable_valgrind=yes])

AC_ARG_ENABLE(alternate-addressbook,
		[  --enable-alternate-addressbook  Build alternate external address book support],
		[enable_alternate_addressbook=$enableval], [enable_alternate_addressbook=no])

AC_ARG_ENABLE(svg,
        [  --disable-svg                   Do not build SVG support],
        [enable_svg=$enableval], [enable_svg=yes])

AC_ARG_ENABLE(tests,
				[  --enable-tests                   Build unit tests],
				[enable_tests=$enableval], [enable_tests=no])

manualdir='${docdir}/manual'
AC_ARG_WITH(manualdir,
	[  --with-manualdir=DIR    Manual directory],
	[manualdir="$withval"])
AC_SUBST(manualdir)

dnl ******************************
dnl ** Check for required tools **
dnl ** to build manuals         **
dnl ******************************

AC_PATH_PROG(DOCBOOK2HTML, docbook2html)
AC_PATH_PROG(DOCBOOK2TXT, docbook2txt)
AC_PATH_PROG(DOCBOOK2PS, docbook2ps)
AC_PATH_PROG(DOCBOOK2PDF, docbook2pdf)

AM_CONDITIONAL(MANUAL_HTML, test -n "$DOCBOOK2HTML")
AM_CONDITIONAL(MANUAL_TXT, test -n "$DOCBOOK2TXT")
AM_CONDITIONAL(MANUAL_PDF, test -n "$DOCBOOK2PDF")
AM_CONDITIONAL(MANUAL_PS, test -n "$DOCBOOK2PS")

if test x"$enable_manual" = x"yes"; then
    if test -n "$DOCBOOK2TXT" -o -n "$DOCBOOK2HTML" \
	-o -n "$DOCBOOK2PS" -o -n "$DOCBOOK2PDF"; then
	    enable_manual=yes
	else
	    enable_manual=no
    fi
fi

AM_CONDITIONAL(BUILD_MANUAL, test x"$enable_manual" = xyes)

dnl Set PACKAGE_DATA_DIR in config.h.
if test "x${datarootdir}" = 'x${prefix}/share'; then
	if test "x${prefix}" = "xNONE"; then
    		AC_DEFINE_UNQUOTED(PACKAGE_DATA_DIR, "${ac_default_prefix}/share/${PACKAGE}",[PACKAGE_DATA_DIR])
  	else
    		AC_DEFINE_UNQUOTED(PACKAGE_DATA_DIR, "${prefix}/share/${PACKAGE}",[PACKAGE_DATA_DIR])
  	fi
else
	AC_DEFINE_UNQUOTED(PACKAGE_DATA_DIR, "${datarootdir}/${PACKAGE}",[PACKAGE_DATA_DIR])
fi

AC_CHECK_LIB(xpg4, setlocale)

SM_LIBS=""
dnl Check for LibSM
AC_MSG_CHECKING([whether to use LibSM])
if test x"$enable_libsm" = xyes; then
	AC_MSG_RESULT(yes)
	AC_CHECK_LIB(SM, SmcSaveYourselfDone,
		[SM_LIBS="$X_LIBS -lSM -lICE"],enable_libsm=no,
		$X_LIBS -lICE)
	AC_CHECK_HEADERS(X11/SM/SMlib.h,,enable_libsm=no)
	if test x"$enable_libsm" = xyes; then
		AC_DEFINE(HAVE_LIBSM, 1, [Define to 1 if you have libSM installed])
	else
		AC_MSG_RESULT(not found)
		AC_MSG_WARN([*** LibSM will not be supported ***])
	fi
else
	AC_MSG_RESULT(no)
fi
AC_SUBST(SM_LIBS)

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
AC_CHECK_HEADER([execinfo.h], [AC_DEFINE(HAVE_BACKTRACE,1,[Has backtrace*() needed for retrieving stack traces])])
AC_SEARCH_LIBS(backtrace_symbols, [execinfo])

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_STRUCT_TM

dnl AC_CHECK_TYPE(wint_t, unsigned int) does not work because wint_t
dnl may be defined only in wchar.h (this happens with gcc-2.96).
dnl So we need to use this extended macro.
CLAWS_CHECK_TYPE(wint_t, unsigned int,
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
AC_CHECK_FUNCS(gethostname mkdir mktime socket strstr strchr strcasestr \
	       uname flock lockf inet_aton inet_addr \
	       fchmod mkstemp truncate getuid regcomp)

AC_CHECK_FUNCS(fgets_unlocked fgetc_unlocked fputs_unlocked fputc_unlocked fread_unlocked fwrite_unlocked feof_unlocked ferror_unlocked fmemopen)

dnl *****************
dnl ** common code **
dnl *****************

dnl check for glib
PKG_CHECK_MODULES(GLIB, [glib-2.0 >= 2.28 gmodule-2.0 >= 2.28 gobject-2.0 >= 2.28 gthread-2.0 >= 2.28])

GLIB_GENMARSHAL=`$PKG_CONFIG --variable=glib_genmarshal glib-2.0`
AC_SUBST(GLIB_GENMARSHAL)

AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

PKG_CHECK_MODULES(GDK_PIXBUF, [gdk-pixbuf-2.0 >= 2.26])

dnl Check for bind_textdomain_codeset, including -lintl if GLib brings it in.
syl_save_LIBS=$LIBS
LIBS="$LIBS $GTK_LIBS"
AC_CHECK_FUNCS(bind_textdomain_codeset)
LIBS=$syl_save_LIBS

dnl check for IPv6 option
dnl automated checks for IPv6 support.
AC_MSG_CHECKING([whether to use IPv6])
if test x"$enable_ipv6" = xyes; then
	AC_MSG_RESULT(yes)
	AC_MSG_CHECKING([for IPv6 support])
	if test x"$platform_win32" = xyes; then
		AC_CACHE_VAL(ac_cv_ipv6,[
			AC_TRY_COMPILE([
					#include <ws2tcpip.h>
				], [struct in6_addr a;],
				ac_cv_ipv6=yes, ac_cv_ipv6=no)
		])
	else
		AC_CACHE_VAL(ac_cv_ipv6,[
			AC_TRY_COMPILE([
					#define INET6
					#include <sys/types.h>
					#include <netinet/in.h>
				], [int x = IPPROTO_IPV6; struct in6_addr a;],
				ac_cv_ipv6=yes, ac_cv_ipv6=no)
		])
	fi
	AC_MSG_RESULT($ac_cv_ipv6)
	if test $ac_cv_ipv6 = yes; then
		AC_DEFINE(INET6, 1, Define if you want IPv6 support.)
	else
		AC_MSG_WARN(*** IPv6 will not be supported ***)
		enable_ipv6=no
	fi
else
	AC_MSG_RESULT(no)
fi

dnl GNUTLS
AC_MSG_CHECKING([whether to use GnuTLS])
AC_MSG_RESULT($enable_gnutls)
if test "x$enable_gnutls" != "xno"; then
        PKG_CHECK_MODULES(GNUTLS, gnutls >= 2.2,
        [
                AC_DEFINE(USE_GNUTLS, 1, gnutls)
                echo "Building with GnuTLS"
		PKG_CHECK_MODULES(GNUTLS, gnutls >= 2.11,
		[
			dnl No linking against libgcrypt needed
		],
		[
			dnl linking against libgcrypt *is* needed
			GNUTLS_LIBS="$GNUTLS_LIBS -lgcrypt"
		])
        ],
        [
                echo "Building without GnuTLS"
		AC_MSG_RESULT([*** GnuTLS support is recommended ])
		AC_MSG_RESULT([*** You can use --disable-gnutls if you don't need it.])
		AC_MSG_ERROR([GnuTLS not found])
        ])
        AC_SUBST(GNUTLS_LIBS)
        AC_SUBST(GNUTLS_CFLAGS)
fi

PKG_CHECK_MODULES(NETTLE, nettle)
AC_SUBST(NETTLE_LIBS)

AC_ARG_WITH(passcrypt-key, [  --with-passcrypt-key=KEY     Key used to encode passwords (8 byte string)],
	    with_passcrypt_key="$withval", with_passcrypt_key="passkey0")
AC_SUBST(PASSCRYPT_KEY, $with_passcrypt_key)

dnl RC dir (will be default at a certain point in time)
AC_ARG_WITH(config-dir,    [  --with-config-dir=RCDIR      Local configuration dir (default: .claws-mail)],
	      ac_cv_with_config_dir="$withval", ac_cv_with_config_dir="")

dnl Set correct default value based on platform
if test x"$ac_cv_with_config_dir" = x""; then
	if test x"$platform_win32" = xyes; then
		ac_cv_with_config_dir="Claws-mail"
	else
		ac_cv_with_config_dir=".claws-mail"
	fi
fi
AC_DEFINE_UNQUOTED(CFG_RC_DIR, "$ac_cv_with_config_dir", Configuration directory)

AC_ARG_WITH(password-encryption, [  --with-password-encryption=PROVIDER    Which cryptographic library to use for encrypting stored passwords (gnutls, old, default)],
						pwd_crypto="$withval", pwd_crypto="default")

if test x"$pwd_crypto" = xdefault; then
	if test x"$enable_gnutls" = xyes; then
		if `$PKG_CONFIG --atleast-version=3.0 gnutls`; then
			pwd_crypto="gnutls"
		fi
	fi
fi
if test x"$pwd_crypto" = xdefault; then
	pwd_crypto="old"
fi

case $pwd_crypto in
	gnutls)
		if test x"$enable_gnutls" = xno; then
			AC_MSG_ERROR([GnuTLS password encryption requested but GnuTLS is not available.])
		fi
		if ! `$PKG_CONFIG --atleast-version=3.0 gnutls`; then
			AC_MSG_ERROR([GnuTLS version at least 3.0 is required for password encryption.])
		fi
		AC_DEFINE(PASSWORD_CRYPTO_GNUTLS, 1, Use GnuTLS for stored password encryption)
		;;
	old)
		AC_DEFINE(PASSWORD_CRYPTO_OLD, 1, Use old insecure method for stored password encryption)
		;;
	*)
		AC_MSG_ERROR([Unknown password encryption provider requested.])
		;;
esac


dnl ************************
dnl ** GTK user interface **
dnl ************************

dnl Checks for GTK
PKG_CHECK_MODULES(GTK, gtk+-2.0 >= 2.24)
AC_ARG_ENABLE(deprecated,
		[  --disable-deprecated            Disable deprecated GTK functions],
		[GTK_CFLAGS="$GTK_CFLAGS -DG_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED"], [])

dnl Make sure the code does not regress to using deprecated GTK stuff...
GTK_CFLAGS="$GTK_CFLAGS -DGTK_DISABLE_SINGLE_INCLUDES -DGSEAL_ENABLE"

AC_SUBST(GTK_CFLAGS)
AC_SUBST(GTK_LIBS)

dnl enchant is used for spell checking
AC_MSG_CHECKING([whether to use enchant])
AC_MSG_RESULT($enable_enchant)
if test $enable_enchant = yes; then
	PKG_CHECK_MODULES(ENCHANT, enchant >= 1.4.0,
	[
		AC_DEFINE(USE_ENCHANT, 1, enchant)
		echo "Building with enchant"
		enable_enchant=yes
	],
	[
		PKG_CHECK_MODULES(ENCHANT, enchant-2 >= 2.0.0,
		[
			AC_DEFINE(USE_ENCHANT, 1, enchant-2)
			echo "Building with enchant-2"
			enable_enchant=yes
		],
		[
			echo "Building without enchant-notification"
			enable_enchant=no
		])
	])
	AC_SUBST(ENCHANT_CFLAGS)
	AC_SUBST(ENCHANT_LIBS)
fi

dnl want crash dialog
if test $enable_crash_dialog = yes; then
dnl check if GDB is somewhere
	AC_CHECK_PROG(enable_crash_dialog, gdb, yes, no)
	AC_MSG_CHECKING([whether to use crash dialog])
	if test $enable_crash_dialog = yes; then
		AC_DEFINE(CRASH_DIALOG, 1, Pop up crash dialog)
	fi
	AC_MSG_RESULT($enable_crash_dialog)
fi

dnl generic umpc
if test $enable_generic_umpc = yes; then
	AC_DEFINE(GENERIC_UMPC, 1, Generic UMPC code)
	AC_MSG_RESULT($enable_generic_umpc)
fi

dnl Check for X-Face support
AC_MSG_CHECKING([whether to use compface])
if test x"$enable_compface" = xyes; then
	AC_MSG_RESULT(yes)
	AC_CHECK_LIB(compface, uncompface,
		[AC_DEFINE(HAVE_LIBCOMPFACE, 1, Define if you want compface support.)],
		[enable_compface=no])
	if test x"$enable_compface" = xyes; then
		COMPFACE_LIBS="-lcompface"
	else
		COMPFACE_LIBS=""
	fi
	AC_SUBST(COMPFACE_LIBS)
else
	AC_MSG_RESULT(no)
fi

dnl check for pthread support
AC_MSG_CHECKING([whether to use pthread])
if test x$enable_pthread = xno; then
	AC_MSG_RESULT(no)
else
	AC_MSG_RESULT(yes)

        # For W32 we need to use a special ptrhead lib. In this case we can't
        # use AC_CHECK_LIB because it has no means of checking for a
        # library installed under a different name.  Checking for the
        # header is okay.
        if test -n "${pthread_name}" ; then
           enable_pthread=yes
        else
	AC_CHECK_LIB(pthread, pthread_create, :, enable_pthread=no)
        fi
	AC_CHECK_HEADERS(pthread.h, :, enable_pthread=no)

	if test x$enable_pthread = xyes; then
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

dnl #######################################################################
dnl # Check for startup notification
dnl #######################################################################
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

dnl #######################################################################
dnl # Check for D-Bus support
dnl #######################################################################
if test "x$enable_dbus" = "xyes"; then
	PKG_CHECK_MODULES(DBUS, [dbus-1 >= 0.60, dbus-glib-1 >= 0.60],
	[
		AC_DEFINE(HAVE_DBUS_GLIB, 1, [Define if glib bindings of D-Bus are available])
		enable_dbus=yes
	],
	[
		echo "D-Bus requirements not met. D-Bus support not activated."
		enable_dbus=no
	])
	AC_SUBST(DBUS_CFLAGS)
	AC_SUBST(DBUS_LIBS)
fi

dnl #######################################################################
dnl # Configure address book support
dnl #######################################################################

dnl #######################################################################
dnl # Check for alternate address book support
dnl #######################################################################
AC_MSG_CHECKING([whether DBUS support for alternate address book is present])
if test x"$enable_dbus" = xyes; then
	AC_MSG_RESULT([yes])
	AC_MSG_CHECKING([whether to enable alternate address book])
	if test x"$enable_alternate_addressbook" = xyes; then
		AC_MSG_RESULT([yes])
		PKG_CHECK_MODULES(CONTACTS, [claws-contacts],
		[
			AC_DEFINE(USE_ALT_ADDRBOOK, 1, [Define if alternate address book is to be activated.])
			enable_alternate_addressbook=yes
			AC_SUBST(CONTACTS_CFLAGS)
			AC_SUBST(CONTACTS_LIBS)
		],
		[
			enable_alternate_addressbook=no
		])
	else
		AC_MSG_RESULT([no])
		enable_alternate_addressbook=no
	fi
else
	AC_MSG_RESULT([no])
	enable_alternate_addressbook=no
fi

dnl #######################################################################
dnl # Check for old address book support
dnl #######################################################################
if test x"$enable_alternate_addressbook" = xno; then
	dnl for LDAP support in addressbook
	dnl no check for libraries; dynamically loaded
	AC_MSG_CHECKING([whether to use LDAP])
	if test x"$enable_ldap" = xno; then
		AC_MSG_RESULT(no)
	elif test x"$enable_ldap" = xyes -a x"$enable_pthread" = xno; then
		AC_MSG_RESULT(no - LDAP support needs pthread support)

		enable_ldap=no
	elif test x"$platform_win32" = xyes; then
		AC_MSG_RESULT(yes)
		AC_DEFINE(USE_LDAP, 1, Define if you want LDAP support in addressbook.)
		LDAP_LIBS="-lwldap32"
		AC_SUBST(LDAP_LIBS)
	else
		AC_MSG_RESULT(yes)

		dnl check for available libraries, and pull them in
		AC_CHECK_LIB(resolv, res_query, LDAP_LIBS="$LDAP_LIBS -lresolv")
		AC_CHECK_LIB(socket, bind, LDAP_LIBS="$LDAP_LIBS -lsocket")
		AC_CHECK_LIB(nsl, gethostbyaddr, LDAP_LIBS="$LDAP_LIBS -lnsl")
		AC_CHECK_LIB(lber, ber_get_tag, LDAP_LIBS="$LDAP_LIBS -llber",,
				 $LDAP_LIBS)

		AC_CHECK_HEADERS(ldap.h lber.h,
				 [ enable_ldap=yes ],
				 [ enable_ldap=no ])

		if test "$enable_ldap" = yes; then
			AC_CHECK_LIB(ldap, ldap_open,
					 [ enable_ldap=yes ],
					 [ enable_ldap=no ],
					 $LDAP_LIBS)

			AC_CHECK_LIB(ldap, ldap_start_tls_s,
			 		     [ ac_cv_have_tls=yes ],
			 		     [ ac_cv_have_tls=no ])

		fi

		AC_MSG_CHECKING([whether ldap library is available])
		AC_MSG_RESULT($enable_ldap)

		AC_MSG_CHECKING([whether TLS library is available])
		AC_MSG_RESULT($ac_cv_have_tls)

		if test "$enable_ldap" = yes; then
			AC_DEFINE(USE_LDAP, 1, Define if you want LDAP support in addressbook.)
			LDAP_LIBS="$LDAP_LIBS -lldap"
			AC_SUBST(LDAP_LIBS)
			if test "$ac_cv_have_tls" = yes; then
				AC_DEFINE(USE_LDAP_TLS, 1, Define if you want LDAP TLS support in addressbook.)
			fi
			dnl As of OpenLDAP API version 3000 a number of functions has
			dnl been deprecated. As Claws-mail compiles and runs on many
			dnl platforms and many versions of OpenLDAP we need to be able
			dnl to switch between the old and new API because new API has
			dnl added new functions replacing old ones and at the same time
			dnl old functions has been changed.
			dnl If cross-compiling defaults to enable deprecated features
			dnl for maximum portability
			AC_MSG_CHECKING([The API version of OpenLDAP])
			AC_RUN_IFELSE(
				[AC_LANG_PROGRAM(
				 [#include <ldap.h>],
				 [if (LDAP_API_VERSION >= 3000)
						return 1
				])],
				[AC_MSG_RESULT([version < 3000])
				 AC_DEFINE(OPEN_LDAP_API_AT_LEAST_3000, 0, Define if OpenLDAP API is at least version 3000.)],
				[AC_MSG_RESULT([version >= 3000])
				 AC_DEFINE(OPEN_LDAP_API_AT_LEAST_3000, 1, Define if OpenLDAP API is at least version 3000.)],
				[AC_MSG_RESULT([Enabling deprecated features in OpenLDAP])
				 AC_DEFINE(OPEN_LDAP_API_AT_LEAST_3000, 0, Define if OpenLDAP API is at least version 3000.)
				 AC_DEFINE(LDAP_DEPRECATED, 1, Define to activate deprecated features in OpenLDAP)]
			)
		fi
	fi

	dnl for JPilot support in addressbook
	dnl no check for libraries; these are dynamically loaded
	AC_MSG_CHECKING([whether to use JPilot])
	if test "$enable_jpilot" = yes; then
		AC_MSG_RESULT(yes)
		AC_CHECK_HEADERS(pi-args.h pi-appinfo.h pi-address.h pi-version.h,
				 [ AC_DEFINE(USE_JPILOT, 1, Define if you want JPilot support in addressbook.) ],
				 [ enable_jpilot=no ])
		if test "$enable_jpilot" = no; then
			AC_CHECK_HEADERS(libpisock/pi-args.h libpisock/pi-appinfo.h libpisock/pi-address.h libpisock/pi-version.h,
					 [ enable_jpilot=yes
					   AC_DEFINE(USE_JPILOT, 1, Define if you want JPilot support in addressbook.) ])
		fi

		AC_CHECK_LIB(pisock, unpack_Address, [JPILOT_LIBS="-lpisock"], [JPILOT_LIBS="" enable_jpilot="no"])
		if test x"$enable_jpilot" = xyes; then
			AC_DEFINE(USE_JPILOT, 1, Define if you want JPilot support in addressbook.)
		else
			AC_MSG_NOTICE([JPilot support not available])
		fi
		AC_SUBST(JPILOT_LIBS)
	else
		AC_MSG_RESULT(no)
	fi
fi

AM_CONDITIONAL(BUILD_ALTADDRBOOK, test x"$enable_alternate_addressbook" = x"yes")

dnl #######################################################################
dnl # Check for NetworkManager support
dnl #######################################################################
if test x"$enable_dbus" = xyes; then
	if test x"$enable_networkmanager" = xyes; then
		PKG_CHECK_MODULES(NETWORKMANAGER_SUPPORT, libnm,
		[
			AC_DEFINE(HAVE_NETWORKMANAGER_SUPPORT, 1, [Define if NetworkManager support is to be included.])
			echo "Building with NetworkManager support"
			enable_networkmanager=yes
		],
		[
			echo "NetworkManager not found."
			enable_networkmanager=no
		])
		AC_SUBST(NETWORKMANAGER_SUPPORT_CFLAGS)
	fi
else
	echo "NetworkManager support deactivated as D-Bus requirements were not met."
	enable_networkmanager=no
fi

dnl Libetpan
AC_MSG_CHECKING([whether to use libetpan])
if test x"$enable_libetpan" = xyes; then
    AC_MSG_RESULT(yes)

    libetpan_config=no
    libetpan_result=no
    libetpan_versiontype=0

    # since 1.9.4, libetpan uses pkg-config
    PKG_CHECK_MODULES([LIBETPAN], [libetpan >= 1.9.4],
    [
        LIBETPAN_VERSION=`pkg-config --modversion libetpan | $AWK -F. '{printf "%d", ($1 * 10000) + ($2 * 100) + $3}'`
        libetpan_config=yes
    ],
    [
        # before 1.9.4, libetpan uses its own libetpan-config script
        AC_PATH_PROG(libetpanconfig, [libetpan-config])
        if test "x$libetpanconfig" != "x"; then
            LIBETPAN_CPPFLAGS="`$libetpanconfig --cflags`"
            LIBETPAN_LIBS="`$libetpanconfig --libs`"
            # support libetpan version like x.x and x.x.x
            libetpan_versiontype=`$libetpanconfig --version | tr -dc . | wc -c`
            if test $libetpan_versiontype -eq 1; then
                LIBETPAN_VERSION=`$libetpanconfig --version | $AWK -F. '{printf "%d", ($1 * 100) + $2}'`
            else
                LIBETPAN_VERSION=`$libetpanconfig --version | $AWK -F. '{printf "%d", ($1 * 10000) + ($2 * 100) + $3}'`
            fi
            libetpan_config=yes
        fi
    ])
    if test "x$libetpan_config" = "xyes"; then
        CPPFLAGS="$CPPFLAGS $LIBETPAN_FLAGS"
        AC_CHECK_HEADER(libetpan/libetpan.h, [libetpan_result=yes])
        if test "x$libetpan_result" = "xyes"; then
            AC_MSG_CHECKING([whether libetpan-config hints compiles and links fine])
            LIBS="$LIBS $LIBETPAN_LIBS"
            AC_TRY_LINK([#include <libetpan/dbstorage.h>], [db_mailstorage_init(NULL, NULL);], [libetpan_result=yes], [libetpan_result=no])
            AC_MSG_RESULT([$libetpan_result])
        fi
    fi
    if test "x$libetpan_result" = "xyes"; then
        if test $libetpan_versiontype -eq 1; then
            if test "$LIBETPAN_VERSION" -lt "57"; then
                AC_MSG_RESULT([*** Claws Mail requires libetpan 0.57 or newer. See http://www.etpan.org/])
                AC_MSG_RESULT([*** You can use --disable-libetpan if you don't need IMAP4 and/or NNTP support.])
                AC_MSG_ERROR([libetpan 0.57 not found])
            fi
        fi
        AC_SUBST(LIBETPAN_FLAGS)
        AC_SUBST(LIBETPAN_LIBS)
        AC_DEFINE(HAVE_LIBETPAN, 1, Define if you want IMAP and/or NNTP support.)
    else
        AC_MSG_RESULT([*** Claws Mail requires libetpan 0.57 or newer. See http://www.etpan.org/ ])
        AC_MSG_RESULT([*** You can use --disable-libetpan if you don't need IMAP4 and/or NNTP support.])
        AC_MSG_ERROR([libetpan 0.57 not found])
    fi
else
    AC_MSG_RESULT(no)
fi
AM_CONDITIONAL(CLAWS_LIBETPAN, test "x$libetpan_result" = "xyes")

dnl librsvg
AC_MSG_CHECKING([whether to use librsvg])
if test x"$enable_svg" = xyes; then
	AC_MSG_RESULT(yes)
    PKG_CHECK_MODULES([SVG], [librsvg-2.0 >= 2.39.0 cairo >= 1.0.0],
    [
        AC_SUBST(SVG_CFLAGS)
        AC_SUBST(SVG_LIBS)
		AC_DEFINE(HAVE_SVG, 1, [Define if librsvg2 is available for SVG support])
		enable_svg=yes
	],
	[
		AC_MSG_NOTICE([SVG support deactivated as librsvg2 >= 2.39.0 was not found])
		enable_svg=no
	])
else
	AC_MSG_RESULT(no)
fi

AC_MSG_CHECKING([whether to use valgrind])
if test x$enable_valgrind = xyes; then
	AC_MSG_RESULT(yes)
	PKG_CHECK_MODULES(VALGRIND, valgrind >= 2.4.0,
			  enable_valgrind=yes, enable_valgrind=no)
	if test x"$enable_valgrind" = xyes; then
		AC_DEFINE(HAVE_VALGRIND, 1, Define if you want valgrind support)
	else
		AC_MSG_RESULT(not found)
	fi
else
	AC_MSG_RESULT(no)
fi
AM_CONDITIONAL(CLAWS_VALGRIND, test x"$enable_valgrind" = x"yes")

AC_MSG_CHECKING([whether to build unit tests])
if test x$enable_tests = xyes; then
	AC_MSG_RESULT(yes)
else
	AC_MSG_RESULT(no)
fi
AM_CONDITIONAL(BUILD_TESTS, test "x$enable_tests" = "xyes")

dnl *************************
dnl ** section for plugins **
dnl *************************

PLUGINS=""
DISABLED_PLUGINS=""
MISSING_DEPS_PLUGINS=""

dnl First we set the enabled status - either enabled (yes), auto-enabled (auto)
dnl or (auto-)disabled (no for both)
dnl
dnl All plugins are auto-enabled except for Demo which is just there to help
dnl potential plugins writers.

AC_ARG_ENABLE(acpi_notifier-plugin,
		[  --disable-acpi_notifier-plugin  Do not build acpi_notifier plugin],
		[enable_acpi_notifier_plugin=$enableval], [enable_acpi_notifier_plugin=auto])

AC_ARG_ENABLE(address_keeper-plugin,
		[  --disable-address_keeper-plugin Do not build address_keeper plugin],
		[enable_address_keeper_plugin=$enableval], [enable_address_keeper_plugin=auto])

AC_ARG_ENABLE(archive-plugin,
		[  --disable-archive-plugin        Do not build archive plugin],
		[enable_archive_plugin=$enableval], [enable_archive_plugin=auto])

AC_ARG_ENABLE(att_remover-plugin,
		[  --disable-att_remover-plugin    Do not build att_remover plugin],
		[enable_att_remover_plugin=$enableval], [enable_att_remover_plugin=auto])

AC_ARG_ENABLE(attachwarner-plugin,
		[  --disable-attachwarner-plugin   Do not build attachwarner plugin],
		[enable_attachwarner_plugin=$enableval], [enable_attachwarner_plugin=auto])

AC_ARG_ENABLE(bogofilter-plugin,
		[  --disable-bogofilter-plugin     Do not build bogofilter plugin],
		[enable_bogofilter_plugin=$enableval], [enable_bogofilter_plugin=auto])

AC_ARG_ENABLE(bsfilter-plugin,
		[  --disable-bsfilter-plugin       Do not build bsfilter plugin],
		[enable_bsfilter_plugin=$enableval], [enable_bsfilter_plugin=auto])

AC_ARG_ENABLE(clamd-plugin,
		[  --disable-clamd-plugin          Do not build clamd plugin],
		[enable_clamd_plugin=$enableval], [enable_clamd_plugin=auto])

AC_ARG_ENABLE(dillo-plugin,
		[  --disable-dillo-plugin          Do not build dillo plugin],
		[enable_dillo_plugin=$enableval], [enable_dillo_plugin=auto])

AC_ARG_ENABLE(fancy-plugin,
		[  --disable-fancy-plugin          Do not build fancy plugin],
		[enable_fancy_plugin=$enableval], [enable_fancy_plugin=auto])

AC_ARG_ENABLE(fetchinfo-plugin,
		[  --disable-fetchinfo-plugin      Do not build fetchinfo plugin],
		[enable_fetchinfo_plugin=$enableval], [enable_fetchinfo_plugin=auto])

AC_ARG_ENABLE(gdata-plugin,
		[  --disable-gdata-plugin          Do not build gdata plugin],
		[enable_gdata_plugin=$enableval], [enable_gdata_plugin=auto])

AC_ARG_ENABLE(libravatar-plugin,
		[  --disable-libravatar-plugin     Do not build libravatar  plugin],
		[enable_libravatar_plugin=$enableval], [enable_libravatar_plugin=auto])

AC_ARG_ENABLE(litehtml_viewer-plugin,
		[  --disable-litehtml_viewer-plugin       Do not build litehtml_viewer plugin],
		[enable_litehtml_viewer_plugin=$enableval], [enable_litehtml_viewer_plugin=auto])

AC_ARG_ENABLE(mailmbox-plugin,
		[  --disable-mailmbox-plugin       Do not build mailmbox plugin],
		[enable_mailmbox_plugin=$enableval], [enable_mailmbox_plugin=auto])

AC_ARG_ENABLE(managesieve-plugin,
		[  --disable-managesieve-plugin    Do not build managesieve plugin],
		[enable_managesieve_plugin=$enableval], [enable_managesieve_plugin=auto])

AC_ARG_ENABLE(newmail-plugin,
		[  --disable-newmail-plugin        Do not build newmail plugin],
		[enable_newmail_plugin=$enableval], [enable_newmail_plugin=auto])

AC_ARG_ENABLE(notification-plugin,
		[  --disable-notification-plugin   Do not build notification plugin],
		[enable_notification_plugin=$enableval], [enable_notification_plugin=auto])

AC_ARG_ENABLE(pdf_viewer-plugin,
		[  --disable-pdf_viewer-plugin     Do not build pdf_viewer plugin],
		[enable_pdf_viewer_plugin=$enableval], [enable_pdf_viewer_plugin=auto])

AC_ARG_ENABLE(perl-plugin,
		[  --disable-perl-plugin           Do not build perl plugin],
		[enable_perl_plugin=$enableval], [enable_perl_plugin=auto])

AC_ARG_ENABLE(python-plugin,
		[  --disable-python-plugin         Do not build python plugin],
		[enable_python_plugin=$enableval], [enable_python_plugin=auto])

AC_ARG_ENABLE(pgpcore-plugin,
		[  --disable-pgpcore-plugin        Do not build pgpcore plugin],
		[enable_pgpcore_plugin=$enableval], [enable_pgpcore_plugin=auto])

AC_ARG_ENABLE(pgpmime-plugin,
		[  --disable-pgpmime-plugin        Do not build pgpmime plugin],
		[enable_pgpmime_plugin=$enableval], [enable_pgpmime_plugin=auto])

AC_ARG_ENABLE(pgpinline-plugin,
		[  --disable-pgpinline-plugin      Do not build pgpinline plugin],
		[enable_pgpinline_plugin=$enableval], [enable_pgpinline_plugin=auto])

AC_ARG_ENABLE(rssyl-plugin,
		[  --disable-rssyl-plugin          Do not build rssyl plugin],
		[enable_rssyl_plugin=$enableval], [enable_rssyl_plugin=auto])

AC_ARG_ENABLE(smime-plugin,
		[  --disable-smime-plugin          Do not build smime plugin],
		[enable_smime_plugin=$enableval], [enable_smime_plugin=auto])

AC_ARG_ENABLE(spamassassin-plugin,
		[  --disable-spamassassin-plugin   Do not build spamassassin plugin],
		[enable_spamassassin_plugin=$enableval], [enable_spamassassin_plugin=auto])

AC_ARG_ENABLE(spam_report-plugin,
		[  --disable-spam_report-plugin    Do not build spam_report plugin],
		[enable_spam_report_plugin=$enableval], [enable_spam_report_plugin=auto])

AC_ARG_ENABLE(tnef_parse-plugin,
		[  --disable-tnef_parse-plugin     Do not build tnef_parse plugin],
		[enable_tnef_parse_plugin=$enableval], [enable_tnef_parse_plugin=auto])

AC_ARG_ENABLE(vcalendar-plugin,
		[  --disable-vcalendar-plugin      Do not build vcalendar plugin],
		[enable_vcalendar_plugin=$enableval], [enable_vcalendar_plugin=auto])

dnl disabled by default
AC_ARG_ENABLE(demo-plugin,
		[  --enable-demo-plugin            Build demo plugin],
		[enable_demo_plugin=$enableval], [enable_demo_plugin=no])


dnl Then we check (unconditionnaly) for plugins dependencies
dnl Some dependencies are optional, some mandatory. This is taken care of
dnl later.
dnl
dnl During this dependancy check we do the checks themselves, define HAVE_X to
dnl either yes or no, and do the AC_SUBST calls.

dnl Archive:		libarchive
dnl Fancy:		Webkit, curl, optionally libsoup-gnome
dnl Gdata:		libgdata
dnl Litehtml		a C++ compiler, >=glib-2.36, cairo, fontconfig, gumbo, curl
dnl Libravatar:		libcurl
dnl Notification:	optionally libnotify  unity/messaging-menu
dnl 				   libcanberra_gtk hotkey
dnl Pdf-Viewer:		libpoppler
dnl Perl:		sed perl
dnl PGP/Core:		libgpgme
dnl PGP/Mime:		pgpcore libgpgme
dnl PGP/Inline:		pgpcore libgpgme
dnl S/Mime:		pgpcore libgpgme
dnl Python:		Python
dnl RSSyl:		expat libcurl
dnl SpamReport:		libcurl
dnl vCalendar:		libcurl, libical
dnl tnef_parse: 	libytnef

dnl libcurl ********************************************************************
PKG_CHECK_MODULES(CURL, libcurl, HAVE_CURL=yes, HAVE_CURL=no)
AC_SUBST(CURL_LIBS)
AC_SUBST(CURL_CFLAGS)

dnl expat **********************************************************************
PKG_CHECK_MODULES(EXPAT, expat, HAVE_EXPAT=yes, HAVE_EXPAT=no)

if test x"$HAVE_EXPAT" = xno; then
	AC_CHECK_HEADER(expat.h, [expat_header=yes], [expat_header=no])
	AC_CHECK_LIB(expat, XML_ParserCreate, [expat_lib=yes], [expat_lib=no])
	if test x"$expat_header" = xyes -a x"$expat_lib"=xyes; then
		HAVE_EXPAT=yes
		EXPAT_CFLAGS=""
		EXPAT_LIBS="-lexpat"
	fi
fi

AC_SUBST(EXPAT_CFLAGS)
AC_SUBST(EXPAT_LIBS)

dnl webkit *********************************************************************
PKG_CHECK_MODULES(WEBKIT, webkit-1.0 >= 1.10.0, HAVE_WEBKIT=yes, HAVE_WEBKIT=no)
AC_SUBST(WEBKIT_LIBS)
AC_SUBST(WEBKIT_CFLAGS)

dnl libsoup ********************************************************************
PKG_CHECK_MODULES(LIBSOUP, libsoup-2.4, HAVE_LIBSOUP=yes, HAVE_LIBSOUP=no)
if test x"$HAVE_LIBSOUP" = xyes; then
	AC_DEFINE(HAVE_LIBSOUP, 1, [Define if libsoup is available])
fi
AC_SUBST(LIBSOUP_CFLAGS)
AC_SUBST(LIBSOUP_LIBS)

dnl libsoup-gnome **************************************************************
PKG_CHECK_MODULES(LIBSOUP_GNOME, libsoup-gnome-2.4 >= 2.26, HAVE_LIBSOUP_GNOME=yes, HAVE_LIBSOUP_GNOME=no)
if test x"$HAVE_LIBSOUP_GNOME" = xyes; then
	AC_DEFINE(HAVE_LIBSOUP_GNOME, 1, [Define if libsoup_gnome is available])
fi
AC_SUBST(LIBSOUP_GNOME_CFLAGS)
AC_SUBST(LIBSOUP_GNOME_LIBS)

dnl libarchive *****************************************************************
PKG_CHECK_MODULES(LIBARCHIVE, libarchive, HAVE_ARCHIVE=yes, HAVE_ARCHIVE=no)
AC_SUBST(ARCHIVE_LIBS)
AC_SUBST(ARCHIVE_CFLAGS)
AC_CHECK_LIB([archive], [archive_read_new],
		       ARCHIVE_LIBS=-larchive
		       HAVE_ARCHIVE=yes
		       AC_SUBST(ARCHIVE_LIBS,$ARCHIVE_CFLAGS),
		       HAVE_ARCHIVE=no
		       )

dnl libgdata *******************************************************************
PKG_CHECK_MODULES(GDATA, libgdata >= 0.17.2, HAVE_GDATA=yes, HAVE_GDATA=no)
AC_SUBST(GDATA_CFLAGS)
AC_SUBST(GDATA_LIBS)

dnl cairo **********************************************************************
PKG_CHECK_MODULES(CAIRO, cairo, HAVE_CAIRO=yes, HAVE_CAIRO=no)
AC_SUBST(CAIRO_CFLAGS)
AC_SUBST(CAIRO_LIBS)

dnl fontconfig *****************************************************************
PKG_CHECK_MODULES(FONTCONFIG, fontconfig, HAVE_FONTCONFIG=yes, HAVE_FONTCONFIG=no)
AC_SUBST(FONTCONFIG_CFLAGS)
AC_SUBST(FONTCONFIG_LIBS)

dnl gumbo **********************************************************************
PKG_CHECK_MODULES(LIBGUMBO, gumbo >= 0.10, HAVE_LIBGUMBO=yes, HAVE_LIBGUMBO=no)
AC_SUBST(LIBGUMBO_CFLAGS)
AC_SUBST(LIBGUMBO_LIBS)

dnl libical ********************************************************************
PKG_CHECK_MODULES(LIBICAL, libical >= 2.0, HAVE_LIBICAL=yes, HAVE_LIBICAL=no)
AC_SUBST(LIBICAL_CFLAGS)
AC_SUBST(LIBICAL_LIBS)

dnl Poppler ********************************************************************
PKG_CHECK_MODULES(POPPLER, poppler-glib >= 0.12.0, HAVE_POPPLER=yes, HAVE_POPPLER=no)
AC_SUBST(POPPLER_LIBS)
AC_SUBST(POPPLER_CFLAGS)

dnl check for Poppler extra features that we conditionally support
if test x"$HAVE_POPPLER" = xyes; then
	OLD_CFLAGS=$CFLAGS
	CFLAGS="$POPPLER_CFLAGS $GTK_CFLAGS $GLIB_CFLAGS"
	AC_CHECK_DECL(POPPLER_DEST_NAMED,
		[AC_DEFINE([HAVE_POPPLER_DEST_NAMED], [], [Description])],
		,[#include <poppler-action.h>])
	AC_CHECK_DECL(POPPLER_DEST_XYZ,
		[AC_DEFINE([HAVE_POPPLER_DEST_XYZ], [], [Description])],
		,[#include <poppler-action.h>])
	CFLAGS=$OLD_CFLAGS
fi

dnl perl ***********************************************************************
AC_CHECK_PROG(HAVE_PERL, perl, yes, no)
if test x"$HAVE_PERL" = xyes; then
	AC_MSG_CHECKING(for perl >= 5.8.0)
	PERL_VER=`perl -e 'print $] > 5.0079999?"yes":"no"'`
	if test "$PERL_VER" = "yes"; then
		AC_MSG_RESULT(yes)
	else
		AC_MSG_RESULT(no)
		HAVE_PERL=no
	fi
fi
if test x"$HAVE_PERL" = xyes; then
	AC_MSG_CHECKING(for Perl compile flags)
	PERL_CFLAGS=`perl -MExtUtils::Embed -e ccopts`
	PERL_CFLAGS=`echo $PERL_CFLAGS | sed 's/-D_FILE_OFFSET_BITS=[[0-9]]*//'`
	PERL_LDFLAGS=`perl -MExtUtils::Embed -e ldopts |sed 's/-lgdbm\>//'`
	PERL_LDFLAGS=`echo $PERL_LDFLAGS |sed 's/-ldb\>//'`
	PERL_LDFLAGS=`echo $PERL_LDFLAGS |sed 's/-lndbm\>//'`
	PERL_LDFLAGS=`echo $PERL_LDFLAGS |sed 's/-lc\>//'`
	AC_MSG_RESULT(ok)
	AC_MSG_NOTICE([Adding perl LIBS ${PERL_CFLAGS}])
	
	if test x"$HAVE_PERL" = xyes; then
		AC_CHECK_LIB(perl,perl_alloc,[AC_DEFINE(HAVE_LIBPERL, 1, Check for libperl.)], 
			 		     [ HAVE_LIBPERL=no ])
	fi
	if test x"$HAVE_LIBPERL" = xno; then
		LIBPERL_PREFIX=`perl -MExtUtils::Embed -e perl_inc`
		LIBPERL_PREFIX=`echo $LIBPERL_PREFIX |sed 's/-I//'`
		AC_MSG_CHECKING([for libperl.so])
		if test -f "$LIBPERL_PREFIX/libperl.so"; then
			AC_MSG_RESULT(yes)
			HAVE_LIBPERL=yes
		else
			AC_MSG_RESULT(no)
		fi	
	fi
	PERL="perl"
	AC_SUBST(PERL)
	AC_SUBST(PERL_CFLAGS)
	AC_SUBST(PERL_LDFLAGS)
fi

dnl Gpgme **********************************************************************
AM_PATH_GPGME(1.0.0, HAVE_GPGME=yes, HAVE_GPGME=no)
if test x"$HAVE_GPGME" = xyes; then
	AC_DEFINE(USE_GPGME, 1, Define if you use GPGME to support OpenPGP.)
	AM_PATH_GPGME(1.1.1, AC_DEFINE(HAVE_GPGME_PKA_TRUST, 1, [Define if GPGME supports PKA.]))
fi

dnl Python *********************************************************************
missing_python=""
AM_PATH_PYTHON([2.5], [
	AC_PATH_PROG(PYTHON_CONFIG, python$PYTHON_VERSION-config)
	if test x"$PYTHON_CONFIG" = x"" ; then
		AC_PATH_PROG(PYTHON_CONFIG, python-config)
	fi
	if test x"$PYTHON_CONFIG" != x""; then
		PYTHON_CFLAGS=`$PYTHON_CONFIG --includes`
		PYTHON_LIBS=`$PYTHON_CONFIG --libs`
		PYTHON_PREFIX=`$PYTHON_CONFIG --prefix`
		HAVE_PYTHON=yes
	else
		AC_MSG_WARN(python-config not found. Maybe you need to install development packages for Python.)
		HAVE_PYTHON=no
		missing_python="python-config"
	fi

	if test x"$HAVE_PYTHON" = xyes; then
		_save_libs="$LIBS"
		if test x"$platform_win32" = xno; then
			# libpython.so
			PYTHON_SHARED_LIB="libpython${PYTHON_VERSION}.so"
			AC_CHECK_LIB(dl, dlopen, [LIBS="-ldl"])
			AC_MSG_CHECKING([whether to dlopen $PYTHON_SHARED_LIB works])
			AC_RUN_IFELSE(
				[AC_LANG_PROGRAM(
					[#include <dlfcn.h>
					 #define PYTHON_SO_FILE "${PYTHON_SHARED_LIB}"
					],
					[if (!dlopen(PYTHON_SO_FILE, RTLD_NOW | RTLD_GLOBAL)) return 1; return 0;])
				],
				[found_libpython_so="yes"],
				[found_libpython_so="no"],
				[AC_MSG_FAILURE([cross-compiling not supported])])
		fi
		if test x"$found_libpython_so" != x"yes"; then
			AC_MSG_RESULT(no)
			AC_MSG_WARN(Could not find Python shared libary: ${PYTHON_SHARED_LIB}. Maybe you need to install development packages for Python.)
			HAVE_PYTHON=no
			missing_python="libpython"
		else
			AC_MSG_RESULT(yes)
		fi
		LIBS="$_save_libs";
	fi
	if test x"$HAVE_PYTHON" = xyes; then
		PKG_CHECK_MODULES(PYGTK, pygtk-2.0 >= 2.10.3, [AC_DEFINE(ENABLE_PYTHON, [1], [Enable Python support])], HAVE_PYTHON=no)
		if test x"$HAVE_PYTHON" = xno; then
			missing_python="pygtk-2.0 >= 2.10.3"
		fi
	fi
], [
		HAVE_PYTHON=no
		missing_python="python interpreter"
])
AC_SUBST(PYTHON_SHARED_LIB)
AC_SUBST(PYTHON_CFLAGS)
AC_SUBST(PYTHON_LIBS)
AC_SUBST(PYGTK_CFLAGS)
AC_SUBST(PYGTK_LIBS)

dnl libnotify ******************************************************************
PKG_CHECK_MODULES(libnotify, libnotify >= 0.4.3, HAVE_LIBNOTIFY=yes, HAVE_LIBNOTIFY=no)
if test x"$HAVE_LIBNOTIFY" = xyes; then
	AC_DEFINE(HAVE_LIBNOTIFY, 1, [Define if libnotify support is enabled])
fi
AC_SUBST(libnotify_CFLAGS)
AC_SUBST(libnotify_LIBS)

dnl libcanberra-gtk ************************************************************
PKG_CHECK_MODULES(libcanberra_gtk, libcanberra-gtk >= 0.6, HAVE_LIBCANBERRA_GTK=yes, HAVE_LIBCANBERRA_GTK=no)
if test x"$HAVE_LIBCANBERRA_GTK" = xyes; then
	AC_DEFINE(HAVE_LIBCANBERRA_GTK, 1, [Define if libcanberra-gtk support is enabled])
fi
AC_SUBST(libcanberra_gtk_CFLAGS)
AC_SUBST(libcanberra_gtk_LIBS)

dnl unity/messaging-menu *******************************************************
PKG_CHECK_MODULES(unity, unity messaging-menu, HAVE_UNITY=yes, HAVE_UNITY=no)
if test x"$HAVE_UNITY" = xyes; then
	AC_DEFINE(NOTIFICATION_INDICATOR, 1, [Activate support for unity and messaging-menu])
fi
AC_SUBST(unity_CFLAGS)
AC_SUBST(unity_LIBS)

dnl hotkeys ********************************************************************
PKG_CHECK_MODULES(CM_NP_HOTKEY, [gio-2.0 >= 2.15.6 gio-unix-2.0 >= 2.15.6], HAVE_HOTKEYS=yes, HAVE_HOTKEYS=no)
if test x"$HAVE_HOTKEYS" = xyes; then
	AC_DEFINE(NOTIFICATION_HOTKEYS, 1, Activate support for global hotkeys)
fi
AC_SUBST(CM_NP_HOTKEY_CFLAGS)
AC_SUBST(CM_NP_HOTKEY_LIBS)

dnl libytnef *******************************************************************
YTNEF_CFLAGS=""
YTNEF_LIBS=""
have_ytnef=0
# Check both ytnef.h and libytnef/ytnef.h, and adjust YTNEF_CFLAGS
# accordingly
AC_CHECK_HEADER(ytnef.h, [have_ytnef=1], [have_ytnef=0])
if test $have_ytnef -eq 0; then
	AC_CHECK_HEADER(libytnef/ytnef.h,
									[have_ytnef=1;
									 YTNEF_CFLAGS="${YTNEF_CFLAGS} -DYTNEF_H_SUBDIR"],
									[have_ytnef=0])
fi
if test $have_ytnef -eq 1; then
	AC_MSG_CHECKING([how libytnef's SwapDDWord() should be called])
	# Now we have to figure out which libytnef version we're using,
	# based on whether SwapDDWord takes one argument or two.
	if test "x${YTNEF_CFLAGS}" = "x"; then
		ytnef_include="#include <ytnef.h>"
	else
		ytnef_include="#include <libytnef/ytnef.h>"
	fi
	AC_TRY_COMPILE([#include <stdio.h>
									${ytnef_include}],
									[SwapDDWord(0, 0);],
									[have_ytnef=1],
									[have_ytnef=0])
	if test $have_ytnef -eq 0; then
		AC_TRY_COMPILE([#include <stdio.h>
										${ytnef_include}],
										[SwapDDWord(0);],
										[have_ytnef=1;
										 YTNEF_CFLAGS="${YTNEF_CFLAGS} -DYTNEF_OLD_SWAPDDWORD"],
										[have_ytnef=0])
	fi
	if test $have_ytnef -eq 1; then
		YTNEF_LIBS="-lytnef"
		AC_MSG_RESULT(ok)
	else
		AC_MSG_RESULT(no idea, unsupported libytnef version?)
	fi
fi
AC_SUBST(YTNEF_CFLAGS)
AC_SUBST(YTNEF_LIBS)

dnl Third, we now cross the requested plugins and the available dependencies
dnl If some dependencies are missing and the plugin was explicitely enabled,
dnl we error out, else we only inform.

AC_MSG_CHECKING([whether to build acpi_notifier plugin])
if test x"$enable_acpi_notifier_plugin" != xno; then
	PLUGINS="$PLUGINS acpi_notifier"
	AC_MSG_RESULT(yes)
else
	DISABLED_PLUGINS="$DISABLED_PLUGINS acpi_notifier"
	AC_MSG_RESULT(no)
fi

AC_MSG_CHECKING([whether to build address_keeper plugin])
if test x"$enable_address_keeper_plugin" != xno; then
	PLUGINS="$PLUGINS address_keeper"
	AC_MSG_RESULT(yes)
else
	DISABLED_PLUGINS="$DISABLED_PLUGINS address_keeper"
	AC_MSG_RESULT(no)
fi

AC_MSG_CHECKING([whether to build archive plugin])
if test x"$enable_archive_plugin" != xno; then
	dependencies_missing=""

	if test x"$HAVE_ARCHIVE" = xno; then
		dependencies_missing="libarchive $dependencies_missing"
	fi

	if test x"$dependencies_missing" = x; then
		PLUGINS="$PLUGINS archive"
		AC_MSG_RESULT(yes)
	elif test x"$enable_archive_plugin" = xauto; then
		AC_MSG_RESULT(no)
		AC_MSG_WARN("Plugin archive will not be built; missing $dependencies_missing")
		enable_archive_plugin=no
		MISSING_DEPS_PLUGINS="$MISSING_DEPS_PLUGINS archive"
	else
		AC_MSG_RESULT(no)
		AC_MSG_ERROR("Plugin archive cannot be built; missing $dependencies_missing")
	fi
else
	DISABLED_PLUGINS="$DISABLED_PLUGINS archive"
	AC_MSG_RESULT(no)
fi

AC_MSG_CHECKING([whether to build att_remover plugin])
if test x"$enable_att_remover_plugin" != xno; then
	PLUGINS="$PLUGINS att_remover"
	AC_MSG_RESULT(yes)
else
	DISABLED_PLUGINS="$DISABLED_PLUGINS att_remover"
	AC_MSG_RESULT(no)
fi

AC_MSG_CHECKING([whether to build attachwarner plugin])
if test x"$enable_attachwarner_plugin" != xno; then
	PLUGINS="$PLUGINS attachwarner"
	AC_MSG_RESULT(yes)
else
	DISABLED_PLUGINS="$DISABLED_PLUGINS attachwarner"
	AC_MSG_RESULT(no)
fi

AC_MSG_CHECKING([whether to build bogofilter plugin])
if test x"$enable_bogofilter_plugin" != xno; then
	PLUGINS="$PLUGINS bogofilter"
	AC_MSG_RESULT(yes)
else
	DISABLED_PLUGINS="$DISABLED_PLUGINS bogofilter"
	AC_MSG_RESULT(no)
fi

AC_MSG_CHECKING([whether to build bsfilter plugin])
if test x"$enable_bsfilter_plugin" != xno; then
	PLUGINS="$PLUGINS bsfilter"
	AC_MSG_RESULT(yes)
else
	DISABLED_PLUGINS="$DISABLED_PLUGINS bsfilter"
	AC_MSG_RESULT(no)
fi

AC_MSG_CHECKING([whether to build clamd plugin])
if test x"$enable_clamd_plugin" != xno; then
	PLUGINS="$PLUGINS clamd"
	AC_MSG_RESULT(yes)
else
	DISABLED_PLUGINS="$DISABLED_PLUGINS clamd"
	AC_MSG_RESULT(no)
fi

AC_MSG_CHECKING([whether to build demo plugin])
if test x"$enable_demo_plugin" != xno; then
	PLUGINS="$PLUGINS demo"
	AC_MSG_RESULT(yes)
else
	DISABLED_PLUGINS="$DISABLED_PLUGINS demo"
	AC_MSG_RESULT(no)
fi

AC_MSG_CHECKING([whether to build Dillo plugin])
if test x"$enable_dillo_plugin" != xno; then
	PLUGINS="$PLUGINS dillo"
	AC_MSG_RESULT(yes)
else
	DISABLED_PLUGINS="$DISABLED_PLUGINS dillo"
	AC_MSG_RESULT(no)
fi

AC_MSG_CHECKING([whether to build fancy plugin])
if test x"$enable_fancy_plugin" != xno; then
	dependencies_missing=""

	if test x"$HAVE_WEBKIT" = xno; then
		dependencies_missing="libwebkit-1.0 $dependencies_missing"
	fi
	if test x"$HAVE_CURL" = xno; then
		dependencies_missing="libcurl $dependencies_missing"
	fi

	if test x"$dependencies_missing" = x; then
		PLUGINS="$PLUGINS fancy"
		AC_MSG_RESULT(yes)
	elif test x"$enable_fancy_plugin" = xauto; then
		AC_MSG_RESULT(no)
		AC_MSG_WARN("Plugin fancy will not be built; missing $dependencies_missing")
		enable_fancy_plugin=no
		MISSING_DEPS_PLUGINS="$MISSING_DEPS_PLUGINS fancy"
	else
		AC_MSG_RESULT(no)
		AC_MSG_ERROR("Plugin fancy cannot be built; missing $dependencies_missing")
	fi
else
	DISABLED_PLUGINS="$DISABLED_PLUGINS fancy"
	AC_MSG_RESULT(no)
fi

AC_MSG_CHECKING([whether to build fetchinfo plugin])
if test x"$enable_fetchinfo_plugin" != xno; then
	PLUGINS="$PLUGINS fetchinfo"
	AC_MSG_RESULT(yes)
else
	DISABLED_PLUGINS="$DISABLED_PLUGINS fetchinfo"
	AC_MSG_RESULT(no)
fi

AC_MSG_CHECKING([whether to build gdata plugin])
if test x"$enable_gdata_plugin" != xno; then
	dependencies_missing=""

	if test x"$HAVE_GDATA" = xno; then
		dependencies_missing="libgdata $dependencies_missing"
	fi

	if test x"$dependencies_missing" = x; then
		PLUGINS="$PLUGINS gdata"
		AC_MSG_RESULT(yes)
	elif test x"$enable_gdata_plugin" = xauto; then
		AC_MSG_RESULT(no)
		AC_MSG_WARN("Plugin gdata will not be built; missing $dependencies_missing")
		enable_gdata_plugin=no
		MISSING_DEPS_PLUGINS="$MISSING_DEPS_PLUGINS gdata"
	else
		AC_MSG_RESULT(no)
		AC_MSG_ERROR("Plugin gdata cannot be built; missing $dependencies_missing")
	fi
else
	DISABLED_PLUGINS="$DISABLED_PLUGINS gdata"
	AC_MSG_RESULT(no)
fi

AC_MSG_CHECKING([whether to build libravatar plugin])
if test x"$enable_libravatar_plugin" != xno; then
	dependencies_missing=""

	if test x"$HAVE_CURL" = xno; then
		dependencies_missing="libcurl $dependencies_missing"
	fi

	if test x"$dependencies_missing" = x; then
		PLUGINS="$PLUGINS libravatar"
		AC_MSG_RESULT(yes)
	elif test x"$enable_libravatar_plugin" = xauto; then
		AC_MSG_RESULT(no)
		AC_MSG_WARN("Plugin libravatar will not be built; missing $dependencies_missing")
		enable_libravatar_plugin=no
		MISSING_DEPS_PLUGINS="$MISSING_DEPS_PLUGINS libravatar"
	else
		AC_MSG_RESULT(no)
		AC_MSG_ERROR("Plugin libravatar cannot be built; missing $dependencies_missing")
	fi
else
	DISABLED_PLUGINS="$DISABLED_PLUGINS libravatar"
	AC_MSG_RESULT(no)
fi

AC_MSG_CHECKING([whether to build litehtml_viewer plugin])
if test x"$enable_litehtml_viewer_plugin" != xno; then
        dependencies_missing=""

        if test x"$HAVE_CXX" = xno; then
                dependencies_missing="C++ compiler $dependencies_missing"
        fi
        PKG_CHECK_EXISTS([glib-2.0 >= 2.36], [],
                [dependencies_missing="glib-2.0 >= 2.36 $dependencies_missing"])
        if test x"$HAVE_CAIRO" = xno; then
                dependencies_missing="cairo $dependencies_missing"
        fi
        if test x"$HAVE_FONTCONFIG" = xno; then
                dependencies_missing="fontconfig $dependencies_missing"
        fi
	if test x"$HAVE_LIBGUMBO" = xno; then
		dependencies_missing="libgumbo $dependencies_missing"
	fi
        if test x"$HAVE_CURL" = xno; then
                dependencies_missing="libcurl $dependencies_missing"
        fi


        if test x"$dependencies_missing" = x; then
                PLUGINS="$PLUGINS litehtml_viewer"
                AC_MSG_RESULT(yes)
        elif test x"$enable_litehtml_viewer_plugin" = xauto; then
                AC_MSG_RESULT(no)
                AC_MSG_WARN("Plugin litehtml_viewer will not be built; missing $dependencies_missing")
                enable_litehtml_viewer_plugin=no
                MISSING_DEPS_PLUGINS="$MISSING_DEPS_PLUGINS litehtml_viewer"
        else
                AC_MSG_RESULT(no)
                AC_MSG_ERROR("Plugin litehtml_viewer cannot be built; missing $dependencies_missing")
        fi
else
        DISABLED_PLUGINS="$DISABLED_PLUGINS litehtml_viewer"
        AC_MSG_RESULT(no)
fi

AC_MSG_CHECKING([whether to build mailmbox plugin])
if test x"$enable_mailmbox_plugin" != xno; then
	PLUGINS="$PLUGINS mailmbox"
	AC_MSG_RESULT(yes)
else
	DISABLED_PLUGINS="$DISABLED_PLUGINS mailmbox"
	AC_MSG_RESULT(no)
fi

AC_MSG_CHECKING([whether to build managesieve plugin])
if test x"$enable_managesieve_plugin" != xno; then
	PLUGINS="$PLUGINS managesieve"
	AC_MSG_RESULT(yes)
else
	DISABLED_PLUGINS="$DISABLED_PLUGINS managesieve"
	AC_MSG_RESULT(no)
fi

AC_MSG_CHECKING([whether to build newmail plugin])
if test x"$enable_newmail_plugin" != xno; then
	PLUGINS="$PLUGINS newmail"
	AC_MSG_RESULT(yes)
else
	DISABLED_PLUGINS="$DISABLED_PLUGINS newmail"
	AC_MSG_RESULT(no)
fi

AC_MSG_CHECKING([whether to build notification plugin])
if test x"$enable_notification_plugin" != xno; then
	PLUGINS="$PLUGINS notification"
	AC_MSG_RESULT(yes)

	AC_DEFINE(NOTIFICATION_POPUP, 1, Activate notification popup)
	AC_DEFINE(NOTIFICATION_BANNER, 1, Activate notification banner)
	AC_DEFINE(NOTIFICATION_COMMAND, 1, Activate notification shell command)
	AC_DEFINE(NOTIFICATION_TRAYICON, 1, Activate notification trayicon)
	if test x"$platform_win32" = xno; then
		AC_DEFINE(NOTIFICATION_LCDPROC, 1, Activate lcdproc support)
	fi

	notification_features="banner command"
	notification_missing_dependencies=""
	if test x"$HAVE_HOTKEYS" = xyes; then
		notification_features="$notification_features hotkeys"
	else
		notification_missing_dependencies="$notification_missing_dependencies hotkeys"
	fi
	notification_features="$notification_features lcdproc"
	if test x"$HAVE_UNITY" = xyes; then
		notification_features="$notification_features unity/messaging-menu"
	else
		notification_missing_dependencies="$notification_missing_dependencies unity/messaging-menu"
	fi
	if test x"$HAVE_LIBNOTIFY" = xyes; then
		notification_features="$notification_features libnotify"
	else
		notification_missing_dependencies="$notification_missing_dependencies libnotify"
	fi
	if test x"$HAVE_LIBCANBERRA_GTK" = xyes; then
		notification_features="$notification_features libcanberra-gtk"
	else
		notification_missing_dependencies="$notification_missing_dependencies libcanberra-gtk"
	fi
	notification_features="$notification_features popup trayicon"
else
	DISABLED_PLUGINS="$DISABLED_PLUGINS notification"
	AC_MSG_RESULT(no)
fi

AC_MSG_CHECKING([whether to build pdf_viewer plugin])
if test x"$enable_pdf_viewer_plugin" != xno; then
	dependencies_missing=""

	if test x"$HAVE_POPPLER" = xno; then
		dependencies_missing="libpoppler-glib $dependencies_missing"
	fi

	if test x"$dependencies_missing" = x; then
		PLUGINS="$PLUGINS pdf_viewer"
		AC_MSG_RESULT(yes)
	elif test x"$enable_pdf_viewer_plugin" = xauto; then
		AC_MSG_RESULT(no)
		AC_MSG_WARN("Plugin pdf_viewer will not be built; missing $dependencies_missing")
		enable_pdf_viewer_plugin=no
		MISSING_DEPS_PLUGINS="$MISSING_DEPS_PLUGINS pdf_viewer"
	else
		AC_MSG_RESULT(no)
		AC_MSG_ERROR("Plugin pdf_viewer cannot be built; missing $dependencies_missing")
	fi
else
	DISABLED_PLUGINS="$DISABLED_PLUGINS pdf_viewer"
	AC_MSG_RESULT(no)
fi

AC_MSG_CHECKING([whether to build perl plugin])
if test x"$enable_perl_plugin" != xno; then
	dependencies_missing=""

	if test x"$HAVE_LIBPERL" = xno; then
		dependencies_missing="libperl $dependencies_missing"
	fi

	if test x"$dependencies_missing" = x; then
		PLUGINS="$PLUGINS perl"
		AC_MSG_RESULT(yes)
	elif test x"$enable_perl_plugin" = xauto; then
		AC_MSG_RESULT(no)
		AC_MSG_WARN("Plugin perl will not be built; missing $dependencies_missing")
		enable_perl_plugin=no
		MISSING_DEPS_PLUGINS="$MISSING_DEPS_PLUGINS perl"
	else
		AC_MSG_RESULT(no)
		AC_MSG_ERROR("Plugin perl cannot be built; missing $dependencies_missing")
	fi
else
	DISABLED_PLUGINS="$DISABLED_PLUGINS perl"
	AC_MSG_RESULT(no)
fi

AC_MSG_CHECKING([whether to build python plugin])
if test x"$enable_python_plugin" != xno; then
	dependencies_missing=""

	if test x"$HAVE_PYTHON" = xno; then
		dependencies_missing="$missing_python $dependencies_missing"
	fi

	if test x"$dependencies_missing" = x; then
		PLUGINS="$PLUGINS python"
		AC_MSG_RESULT(yes)
	elif test x"$enable_python_plugin" = xauto; then
		AC_MSG_RESULT(no)
		AC_MSG_WARN("Plugin python will not be built; missing $dependencies_missing")
		enable_python_plugin=no
		MISSING_DEPS_PLUGINS="$MISSING_DEPS_PLUGINS python"
	else
		AC_MSG_RESULT(no)
		AC_MSG_ERROR("Plugin python cannot be built; missing $dependencies_missing")
	fi
else
	DISABLED_PLUGINS="$DISABLED_PLUGINS python"
	AC_MSG_RESULT(no)
fi

AC_MSG_CHECKING([whether to build pgpcore plugin])
if test x"$enable_pgpcore_plugin" != xno; then
	dependencies_missing=""

	if test x"$HAVE_GPGME" = xno; then
		dependencies_missing="libgpgme $dependencies_missing"
	fi

	if test x"$dependencies_missing" = x; then
		PLUGINS="$PLUGINS pgpcore"
		AC_MSG_RESULT(yes)
	elif test x"$enable_pgpcore_plugin" = xauto; then
		AC_MSG_RESULT(no)
		AC_MSG_WARN("Plugin pgpcore will not be built; missing $dependencies_missing")
		enable_pgpcore_plugin=no
		MISSING_DEPS_PLUGINS="$MISSING_DEPS_PLUGINS pgpcore"
	else
		AC_MSG_RESULT(no)
		AC_MSG_ERROR("Plugin pgpcore cannot be built; missing $dependencies_missing")
	fi
else
	DISABLED_PLUGINS="$DISABLED_PLUGINS pgpcore"
	AC_MSG_RESULT(no)
fi

AC_MSG_CHECKING([whether to build pgpmime plugin])
if test x"$enable_pgpmime_plugin" != xno; then
	dependencies_missing=""

	if test x"$HAVE_GPGME" = xno; then
		dependencies_missing="libgpgme $dependencies_missing"
	fi
	if test x"$enable_pgpcore_plugin" = xno; then
		dependencies_missing="pgpcore plugin $dependencies_missing"
	fi

	if test x"$dependencies_missing" = x; then
		PLUGINS="$PLUGINS pgpmime"
		AC_MSG_RESULT(yes)
	elif test x"$enable_pgpmime_plugin" = xauto; then
		AC_MSG_RESULT(no)
		AC_MSG_WARN("Plugin pgpmime will not be built; missing $dependencies_missing")
		enable_pgpmime_plugin=no
		MISSING_DEPS_PLUGINS="$MISSING_DEPS_PLUGINS pgpmime"
	else
		AC_MSG_RESULT(no)
		AC_MSG_ERROR("Plugin pgpmime cannot be built; missing $dependencies_missing")
	fi
else
	DISABLED_PLUGINS="$DISABLED_PLUGINS pgpmime"
	AC_MSG_RESULT(no)
fi

AC_MSG_CHECKING([whether to build pgpinline plugin])
if test x"$enable_pgpinline_plugin" != xno; then
	dependencies_missing=""

	if test x"$HAVE_GPGME" = xno; then
		dependencies_missing="libgpgme $dependencies_missing"
	fi
	if test x"$enable_pgpcore_plugin" = xno; then
		dependencies_missing="pgpcore plugin $dependencies_missing"
	fi

	if test x"$dependencies_missing" = x; then
		PLUGINS="$PLUGINS pgpinline"
		AC_MSG_RESULT(yes)
	elif test x"$enable_pgpinline_plugin" = xauto; then
		AC_MSG_RESULT(no)
		AC_MSG_WARN("Plugin pgpinline will not be built; missing $dependencies_missing")
		enable_pgpinline_plugin=no
		MISSING_DEPS_PLUGINS="$MISSING_DEPS_PLUGINS pgpinline"
	else
		AC_MSG_RESULT(no)
		AC_MSG_ERROR("Plugin pgpinline cannot be built; missing $dependencies_missing")
	fi
else
	DISABLED_PLUGINS="$DISABLED_PLUGINS pgpinline"
	AC_MSG_RESULT(no)
fi

AC_MSG_CHECKING([whether to build rssyl plugin])
if test x"$enable_rssyl_plugin" != xno; then
	dependencies_missing=""

	if test x"$HAVE_EXPAT" = xno; then
		dependencies_missing="expat $dependencies_missing"
	fi
	if test x"$HAVE_CURL" = xno; then
		dependencies_missing="libcurl $dependencies_missing"
	fi

	if test x"$dependencies_missing" = x; then
		PLUGINS="$PLUGINS rssyl"
		AC_MSG_RESULT(yes)
	elif test x"$enable_rssyl_plugin" = xauto; then
		AC_MSG_RESULT(no)
		AC_MSG_WARN("Plugin rssyl will not be built; missing $dependencies_missing")
		enable_rssyl_plugin=no
		MISSING_DEPS_PLUGINS="$MISSING_DEPS_PLUGINS rssyl"
	else
		AC_MSG_RESULT(no)
		AC_MSG_ERROR("Plugin rssyl cannot be built; missing $dependencies_missing")
	fi
else
	DISABLED_PLUGINS="$DISABLED_PLUGINS rssyl"
	AC_MSG_RESULT(no)
fi

AC_MSG_CHECKING([whether to build spamassassin plugin])
if test x"$enable_spamassassin_plugin" != xno; then
	PLUGINS="$PLUGINS spamassassin"
	AC_MSG_RESULT(yes)
	AC_SPAMASSASSIN

	dnl check for zlib (optional)
	spamassassin_zlib=0
	SPAMASSASSIN_CFLAGS=""
	SPAMASSASSIN_LIBS=""
	AC_CHECK_HEADER([zlib.h],
			[AC_DEFINE(HAVE_ZLIB_H,1,[optional zlib support for spamassassin plugin])]
			[spamassassin_zlib=1],
			[spamassassin_zlib=0])
	if test $spamassassin_zlib -eq 1; then
		AC_CHECK_LIB(z, deflate, [spamassassin_zlib=1], [spamassassin_zlib=0])
		AC_MSG_CHECKING([whether to build spamassassin plugin with zlib support])
		if test $spamassassin_zlib -eq 1; then
			AC_MSG_RESULT(yes)
			SPAMASSASSIN_CFLAGS="-DHAVE_LIBZ"
			SPAMASSASSIN_LIBS="-lz"
		else
			AC_MSG_RESULT(no)
		fi
	fi
	AC_SUBST(SPAMASSASSIN_CFLAGS)
	AC_SUBST(SPAMASSASSIN_LIBS)
else
	DISABLED_PLUGINS="$DISABLED_PLUGINS spamassassin"
	AC_MSG_RESULT(no)
fi

AC_MSG_CHECKING([whether to build smime plugin])
if test x"$enable_smime_plugin" != xno; then
	dependencies_missing=""

	if test x"$HAVE_GPGME" = xno; then
		dependencies_missing="libgpgme $dependencies_missing"
	fi
	if test x"$enable_pgpcore_plugin" = xno; then
		dependencies_missing="pgpcore plugin $dependencies_missing"
	fi

	if test x"$dependencies_missing" = x; then
		PLUGINS="$PLUGINS smime"
		AC_MSG_RESULT(yes)
	elif test x"$enable_smime_plugin" = xauto; then
		AC_MSG_RESULT(no)
		AC_MSG_WARN("Plugin smime will not be built; missing $dependencies_missing")
		enable_smime_plugin=no
		MISSING_DEPS_PLUGINS="$MISSING_DEPS_PLUGINS smime"
	else
		AC_MSG_RESULT(no)
		AC_MSG_ERROR("Plugin smime cannot be built; missing $dependencies_missing")
	fi
else
	DISABLED_PLUGINS="$DISABLED_PLUGINS smime"
	AC_MSG_RESULT(no)
fi

AC_MSG_CHECKING([whether to build spam_report plugin])
if test x"$enable_spam_report_plugin" != xno; then
	dependencies_missing=""

	if test x"$HAVE_CURL" = xno; then
		dependencies_missing="libcurl $dependencies_missing"
	fi

	if test x"$dependencies_missing" = x; then
		PLUGINS="$PLUGINS spam_report"
		AC_MSG_RESULT(yes)
	elif test x"$enable_spam_report_plugin" = xauto; then
		AC_MSG_RESULT(no)
		AC_MSG_WARN("Plugin spam_report will not be built; missing $dependencies_missing")
		enable_spam_report_plugin=no
		MISSING_DEPS_PLUGINS="$MISSING_DEPS_PLUGINS spam_report"
	else
		AC_MSG_RESULT(no)
		AC_MSG_ERROR("Plugin spam_report cannot be built; missing $dependencies_missing")
	fi
else
	DISABLED_PLUGINS="$DISABLED_PLUGINS spam_report"
	AC_MSG_RESULT(no)
fi

AC_MSG_CHECKING([whether to build tnef_parse plugin])
if test x"$enable_tnef_parse_plugin" != xno; then
	dependencies_missing=""

	if test $have_ytnef -eq 0; then
		dependencies_missing="libytnef"
	fi

	if test x"$dependencies_missing" = x; then
		PLUGINS="$PLUGINS tnef_parse"
		AC_MSG_RESULT(yes)
	elif test x"$enable_tnef_parse_plugin" = xauto; then
		AC_MSG_RESULT(no)
		AC_MSG_WARN(Plugin tnef_parse will not be built; missing $dependencies_missing")
		enable_tnef_parse_plugin=no
		MISSING_DEPS_PLUGINS="$MISSING_DEPS_PLUGINS tnef_parse"
	else
		AC_MSG_RESULT(no)
		AC_MSG_ERROR(Plugin tnef_parse will not be built; missing $dependencies_missing")
	fi
else
	DISABLED_PLUGINS="$DISABLED_PLUGINS tnef_parse"
	AC_MSG_RESULT(no)
fi


AC_MSG_CHECKING([whether to build vcalendar plugin])
if test x"$enable_vcalendar_plugin" != xno; then
	dependencies_missing=""

	if test x"$HAVE_CURL" = xno; then
		dependencies_missing="libcurl $dependencies_missing"
	fi

  if test x"$HAVE_LIBICAL" = xno; then
    dependencies_missing="libical $dependencies_missing"
  fi

	if test x"$HAVE_PERL" = xno; then
		dependencies_missing="perl $dependencies_missing"
	fi

	if test x"$dependencies_missing" = x; then
		PLUGINS="$PLUGINS vcalendar"
		AC_MSG_RESULT(yes)
	elif test x"$enable_vcalendar_plugin" = xauto; then
		AC_MSG_RESULT(no)
		AC_MSG_WARN("Plugin vcalendar will not be built; missing $dependencies_missing")
		enable_vcalendar_plugin=no
		MISSING_DEPS_PLUGINS="$MISSING_DEPS_PLUGINS vcalendar"
	else
		AC_MSG_RESULT(no)
		AC_MSG_ERROR("Plugin vcalendar cannot be built; missing $dependencies_missing")
	fi
else
	DISABLED_PLUGINS="$DISABLED_PLUGINS vcalendar"
	AC_MSG_RESULT(no)
fi

dnl And finally the automake conditionals.

AM_CONDITIONAL(BUILD_ACPI_NOTIFIER_PLUGIN,	test x"$enable_acpi_notifier_plugin" != xno)
AM_CONDITIONAL(BUILD_ADDRESS_KEEPER_PLUGIN,	test x"$enable_address_keeper_plugin" != xno)
AM_CONDITIONAL(BUILD_ARCHIVE_PLUGIN,		test x"$enable_archive_plugin" != xno)
AM_CONDITIONAL(BUILD_ATT_REMOVER_PLUGIN,	test x"$enable_att_remover_plugin" != xno)
AM_CONDITIONAL(BUILD_ATTACHWARNER_PLUGIN,	test x"$enable_attachwarner_plugin" != xno)
AM_CONDITIONAL(BUILD_BOGOFILTER_PLUGIN,		test x"$enable_bogofilter_plugin" != xno)
AM_CONDITIONAL(BUILD_BSFILTER_PLUGIN,		test x"$enable_bsfilter_plugin" != xno)
AM_CONDITIONAL(BUILD_CLAMD_PLUGIN,		test x"$enable_clamd_plugin" != xno)
AM_CONDITIONAL(BUILD_DEMO_PLUGIN,		test x"$enable_demo_plugin" != xno)
AM_CONDITIONAL(BUILD_DILLO_PLUGIN,		test x"$enable_dillo_plugin" != xno)
AM_CONDITIONAL(BUILD_FANCY_PLUGIN,		test x"$enable_fancy_plugin" != xno)
AM_CONDITIONAL(BUILD_FETCHINFO_PLUGIN,		test x"$enable_fetchinfo_plugin" != xno)
AM_CONDITIONAL(BUILD_GDATA_PLUGIN,		test x"$enable_gdata_plugin" != xno)
AM_CONDITIONAL(BUILD_LIBRAVATAR_PLUGIN,		test x"$enable_libravatar_plugin" != xno)
AM_CONDITIONAL(BUILD_LITEHTML_VIEWER_PLUGIN,	test x"$enable_litehtml_viewer_plugin" != xno)
AM_CONDITIONAL(BUILD_MAILMBOX_PLUGIN,		test x"$enable_mailmbox_plugin" != xno)
AM_CONDITIONAL(BUILD_MANAGESIEVE_PLUGIN,	test x"$enable_managesieve_plugin" != xno)
AM_CONDITIONAL(BUILD_NEWMAIL_PLUGIN,		test x"$enable_newmail_plugin" != xno)
AM_CONDITIONAL(BUILD_NOTIFICATION_PLUGIN,	test x"$enable_notification_plugin" != xno)
AM_CONDITIONAL(BUILD_HOTKEYS,			test x"$enable_notification_plugin" != xno -a x"$HAVE_HOTKEYS" = xyes)
AM_CONDITIONAL(BUILD_PDF_VIEWER_PLUGIN,		test x"$enable_pdf_viewer_plugin" != xno)
AM_CONDITIONAL(BUILD_PERL_PLUGIN,		test x"$enable_perl_plugin" != xno)
AM_CONDITIONAL(BUILD_PYTHON_PLUGIN,		test x"$enable_python_plugin" != xno)
AM_CONDITIONAL(BUILD_PGPCORE_PLUGIN,		test x"$enable_pgpcore_plugin" != xno)
AM_CONDITIONAL(BUILD_PGPMIME_PLUGIN,		test x"$enable_pgpmime_plugin" != xno)
AM_CONDITIONAL(BUILD_PGPINLINE_PLUGIN,		test x"$enable_pgpinline_plugin" != xno)
AM_CONDITIONAL(BUILD_RSSYL_PLUGIN,		test x"$enable_rssyl_plugin" != xno)
AM_CONDITIONAL(BUILD_SMIME_PLUGIN,		test x"$enable_smime_plugin" != xno)
AM_CONDITIONAL(BUILD_SPAMASSASSIN_PLUGIN,	test x"$enable_spamassassin_plugin" != xno)
AM_CONDITIONAL(BUILD_SPAM_REPORT_PLUGIN,	test x"$enable_spam_report_plugin" != xno)
AM_CONDITIONAL(BUILD_TNEF_PARSE_PLUGIN,		test x"$enable_tnef_parse_plugin" != xno)
AM_CONDITIONAL(BUILD_VCALENDAR_PLUGIN,		test x"$enable_vcalendar_plugin" != xno)


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
src/common/tests/Makefile
src/gtk/Makefile
src/etpan/Makefile
src/plugins/Makefile
src/plugins/acpi_notifier/Makefile
src/plugins/address_keeper/Makefile
src/plugins/archive/Makefile
src/plugins/att_remover/Makefile
src/plugins/attachwarner/Makefile
src/plugins/bogofilter/Makefile
src/plugins/bsfilter/Makefile
src/plugins/clamd/Makefile
src/plugins/clamd/libclamd/Makefile
src/plugins/demo/Makefile
src/plugins/dillo/Makefile
src/plugins/fancy/Makefile
src/plugins/fetchinfo/Makefile
src/plugins/gdata/Makefile
src/plugins/litehtml_viewer/Makefile
src/plugins/litehtml_viewer/litehtml/Makefile
src/plugins/libravatar/Makefile
src/plugins/mailmbox/Makefile
src/plugins/managesieve/Makefile
src/plugins/newmail/Makefile
src/plugins/notification/Makefile
src/plugins/notification/gtkhotkey/Makefile
src/plugins/pdf_viewer/Makefile
src/plugins/perl/Makefile
src/plugins/perl/tools/Makefile
src/plugins/python/Makefile
src/plugins/python/examples/Makefile
src/plugins/pgpcore/Makefile
src/plugins/pgpcore/tests/Makefile
src/plugins/pgpmime/Makefile
src/plugins/pgpinline/Makefile
src/plugins/rssyl/Makefile
src/plugins/rssyl/tests/Makefile
src/plugins/rssyl/libfeed/Makefile
src/plugins/rssyl/libfeed/tests/Makefile
src/plugins/smime/Makefile
src/plugins/spamassassin/Makefile
src/plugins/spam_report/Makefile
src/plugins/tnef_parse/Makefile
src/plugins/vcalendar/Makefile
src/tests/Makefile
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
manual/es/Makefile
manual/es/dist/Makefile
manual/es/dist/pdf/Makefile
manual/es/dist/ps/Makefile
manual/es/dist/html/Makefile
manual/es/dist/txt/Makefile
claws-mail.pc
])

dnl Output the configuration summary
echo ""
echo "$PACKAGE $VERSION"
echo ""
if test x"$enable_alternate_addressbook" = xyes; then
	echo "Using Address Book : Alternate experimental interface"
else
	echo "Using Address Book : Original stable interface"
	echo "JPilot             : $enable_jpilot"
	echo "LDAP               : $enable_ldap"
fi
echo "gnuTLS             : $enable_gnutls"
echo "iconv              : $am_cv_func_iconv"
echo "compface           : $enable_compface"
echo "IPv6               : $enable_ipv6"
echo "enchant            : $enable_enchant"
echo "IMAP4              : $enable_libetpan"
echo "NNTP               : $enable_libetpan"
echo "Crash dialog       : $enable_crash_dialog"
echo "LibSM              : $enable_libsm"
echo "DBUS               : $enable_dbus"
echo "NetworkManager     : $enable_networkmanager"
echo "Manual             : $enable_manual"
echo "Generic UMPC code  : $enable_generic_umpc"
echo "SVG support        : $enable_svg"
echo "Config dir         : $ac_cv_with_config_dir"
echo "Password crypto    : $pwd_crypto"
echo "Unit tests         : $enable_tests"

echo "Plugins"
echo "   Built:"
for plugin in $PLUGINS; do
echo "            - $plugin"
if test x"$plugin" = xnotification; then
	echo "                Features:"
	for notif_feature in $notification_features; do
		echo "                    $notif_feature"
	done;
	if test "x$notification_missing_dependencies" != x; then
		echo "                Disabled due to missing dependencies:"
		for notif_miss_dep in $notification_missing_dependencies; do
			echo "                    $notif_miss_dep"
		done;
	fi
fi
done;
if test "x$DISABLED_PLUGINS" != x; then
echo "   Disabled:"
for plugin in $DISABLED_PLUGINS; do
echo "            - $plugin"
done;
fi

if test "x$MISSING_DEPS_PLUGINS" != x; then
echo "   Disabled due to missing dependencies:"
for plugin in $MISSING_DEPS_PLUGINS; do
echo "            - $plugin"
done;
fi
echo ""
echo "The binary will be installed in $prefix/bin"
echo ""
echo "Configure finished, type 'make' to build."
