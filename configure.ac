dnl Process this file with autoconf to produce a configure script.
AC_PREREQ(2.50)
AC_INIT(src/main.c)
AC_CONFIG_AUX_DIR(config)

PACKAGE=sylpheed

dnl version number
MAJOR_VERSION=0
MINOR_VERSION=9
MICRO_VERSION=9
INTERFACE_AGE=0
BINARY_AGE=0
EXTRA_VERSION=0
if test $EXTRA_VERSION -eq 0; then
    VERSION=${MAJOR_VERSION}.${MINOR_VERSION}.${MICRO_VERSION}claws
else
    VERSION=${MAJOR_VERSION}.${MINOR_VERSION}.${MICRO_VERSION}claws${EXTRA_VERSION}
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
AC_PROG_LIBTOOL

SYLPHEED_ACLOCAL_INCLUDE(ac)
  
dnl ******************************
dnl Checks for host
dnl ******************************
AC_CANONICAL_HOST

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

if test x"$platform_win32" = x"yes"; then
    WINDRES=windres
    AC_SUBST(WINDRES)
fi

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

case "$target" in
*-darwin*)
	CFLAGS="$CFLAGS -traditional-cpp -fno-common"
	;;
esac
  
dnl Checks for iconv
AM_ICONV

dnl for gettext
ALL_LINGUAS="bg cs de el en_GB es fr hr hu it ja ko nl pl pt_BR ru sk sr sv zh_CN zh_TW.Big5"
GETTEXT_PACKAGE=sylpheed
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE,"$GETTEXT_PACKAGE", [Define text domain.])
AM_GLIB_GNU_GETTEXT
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

dnl Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS(fcntl.h sys/file.h unistd.h paths.h \
		 sys/param.h sys/utsname.h sys/select.h \
		 wchar.h wctype.h locale.h)

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
	       fchmod mkstemp)

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
AM_PATH_OPENSSL

dnl Key for password encryption
AC_ARG_WITH(passcrypt-key, [  --with-passcrypt-key=KEY     Key used to encode passwords (8 byte string)],
	    with_passcrypt_key="$withval", with_passcrypt_key="passkey0")
AC_SUBST(PASSCRYPT_KEY, $with_passcrypt_key)

dnl RC dir (will be default at a certain point in time)
AC_ARG_WITH(config-dir,    [  --with-config-dir=RCDIR      Local configuration dir (default: .sylpheed)],
	      ac_cv_with_config_dir="$withval", ac_cv_with_config_dir=".sylpheed")
if test x"$ac_cv_with_config_dir" = x""; then
	ac_cv_with_config_dir=".sylpheed"
fi 
AC_DEFINE_UNQUOTED(CFG_RC_DIR, "$ac_cv_with_config_dir", Configuration directory)

dnl ************************
dnl ** GTK user interface **
dnl ************************

dnl Checks for GTK
AM_PATH_GTK_2_0(2.0.0,,
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

dnl --- Trayicon ---
AC_ARG_ENABLE(trayicon-plugin,
	[  --disable-trayicon-plugin         Do not build System Tray Icon plugin],
	[ac_cv_enable_trayicon_plugin=$enableval], [ac_cv_enable_trayicon_plugin=yes])
if test x"$ac_cv_enable_trayicon_plugin" = xyes; then
	PLUGINS="trayicon $PLUGINS"
fi
AM_CONDITIONAL(BUILD_TRAYICON_PLUGIN, test x"$ac_cv_enable_trayicon_plugin" = xyes)

dnl --- SpamAssassin ---
AC_ARG_ENABLE(spamassassin-plugin,
	[  --enable-spamassassin-plugin      Build SpamAssassin plugin [default=no]],
	[ac_cv_enable_spamassassin_plugin=$enableval], [ac_cv_enable_spamassassin_plugin=no])
if test x"$ac_cv_enable_spamassassin_plugin" = xyes; then
	AC_SPAMASSASSIN
	PLUGINS="spamassassin $PLUGINS"
fi
AM_CONDITIONAL(BUILD_SPAMASSASSIN_PLUGIN, test x"$ac_cv_enable_spamassassin_plugin" = xyes)

dnl --- MathML Viewer ---
AC_ARG_ENABLE(mathml-viewer-plugin,
	[  --disable-mathml-viewer-plugin    Do not build MathML-Viewer plugin],
	[ac_cv_enable_mathml_viewer_plugin=$enableval], [ac_cv_enable_mathml_viewer_plugin=yes])
if test x"$ac_cv_enable_mathml_viewer_plugin" = xyes; then
	PKG_CHECK_MODULES(GTK_MATH_VIEW, gtkmathview >= 0.4.2 gtkmathview < 0.5, :, ac_cv_enable_mathml_viewer_plugin=no)

	if test x"$ac_cv_enable_mathml_viewer_plugin" = xyes; then
		AC_SUBST(GTK_MATH_VIEW_CFLAGS)
		AC_SUBST(GTK_MATH_VIEW_LIBS)

		PLUGINS="mathml-viewer $PLUGINS"
	fi
fi
AM_CONDITIONAL(BUILD_MATHML_VIEWER_PLUGIN, test x"$ac_cv_enable_mathml_viewer_plugin" = xyes)

dnl --- Image Viewer ---
AC_ARG_ENABLE(image-viewer-plugin,
	[  --disable-image-viewer-plugin     Do not build image viewer plugin],
	[ac_cv_enable_image_viewer_plugin=$enableval], [ac_cv_enable_image_viewer_plugin=yes])
if test x"$ac_cv_enable_image_viewer_plugin" = xyes; then
	AC_ARG_ENABLE(gdk-pixbuf,
		[  --disable-gdk-pixbuf              Do not use gdk-pixbuf],
		[ac_cv_enable_gdk_pixbuf=$enableval], [ac_cv_enable_gdk_pixbuf=yes])
	AC_ARG_ENABLE(imlib,
		[  --disable-imlib                   Do not use imlib],
		[ac_cv_enable_imlib=$enableval], [ac_cv_enable_imlib=yes])

	if test "$ac_cv_enable_gdk_pixbuf" = yes; then
		PKG_CHECK_MODULES(GDK_PIXBUF, \
			[ gdk-pixbuf-2.0 ],
			[ AC_DEFINE(HAVE_GDK_PIXBUF, 1, Define if you use gdk-pixbuf to support image viewer) ],
			[ ac_cv_enable_gdk_pixbuf=no ])
	fi

	if test "$ac_cv_enable_gdk_pixbuf" = yes; then
		PLUGINS="image-viewer(gdk-pixbuf) $PLUGINS"
	else
		ac_cv_enable_image_viewer_plugin=no
	fi
fi
AM_CONDITIONAL(BUILD_IMAGE_VIEWER_PLUGIN, test x"$ac_cv_enable_image_viewer_plugin" = xyes)

dnl --- Dillo Viewer ---
AC_ARG_ENABLE(dillo-viewer-plugin,
	[  --disable-dillo-viewer-plugin     Do not build Dillo plugin for html mail rendering],
	[ac_cv_enable_dillo_viewer_plugin=$enableval], [ac_cv_enable_dillo_viewer_plugin=yes])
if test x"$ac_cv_enable_dillo_viewer_plugin" = xyes; then
	PLUGINS="dillo-viewer $PLUGINS"
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
AC_ARG_ENABLE(clamav-plugin,
	[  --disable-clamav-plugin           Do not build Clam AntiVirus plugin],
	[ac_cv_enable_clamav_plugin=$enableval], [ac_cv_enable_clamav_plugin=yes])
if test x"$ac_cv_enable_clamav_plugin" = xyes; then
	AC_CHECK_LIB(clamav, cl_scanfile, clamav_lib=-lclamav, ac_cv_enable_clamav_plugin=no)
	AC_CHECK_HEADERS(clamav.h, :, ac_cv_enable_clamav_plugin=no)
	if test x"$ac_cv_enable_clamav_plugin" = xyes; then
		CLAMAV_LIBS="${clamav_lib}"
		AC_SUBST(CLAMAV_LIBS)

		PLUGINS="clamav $PLUGINS"
	else
		AC_MSG_NOTICE([clamav library not found, will not build clamav plugin])
	fi
fi
AM_CONDITIONAL(BUILD_CLAMAV_PLUGIN, test x"$ac_cv_enable_clamav_plugin" = xyes)

dnl ****************************
dnl ** Final configure output **
dnl ****************************

AC_OUTPUT([
Makefile
sylpheed.spec
ac/Makefile
po/Makefile.in
src/common/version.h
src/Makefile
src/common/Makefile
src/common/passcrypt.h
src/gtk/Makefile
src/plugins/Makefile
src/plugins/demo/Makefile
src/plugins/spamassassin/Makefile
src/plugins/mathml_viewer/Makefile
src/plugins/dillo_viewer/Makefile
src/plugins/image_viewer/Makefile
src/plugins/trayicon/Makefile
src/plugins/trayicon/libeggtrayicon/Makefile
src/plugins/clamav/Makefile
doc/Makefile
doc/faq/Makefile
doc/faq/de/Makefile
doc/faq/en/Makefile
doc/faq/es/Makefile
doc/faq/fr/Makefile
doc/faq/it/Makefile
doc/man/Makefile
doc/manual/Makefile
doc/manual/de/Makefile
doc/manual/en/Makefile
doc/manual/es/Makefile
doc/manual/fr/Makefile
doc/manual/ja/Makefile
tools/Makefile
config/Makefile
sylpheed-claws.pc
])

dnl Output the configuration summary
echo ""
echo "$PACKAGE $VERSION"
echo ""
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
echo "Config dir    : $ac_cv_with_config_dir"
echo ""
echo "The binary will be installed in $prefix/bin"
echo ""
echo "Configure finished, type 'make' to build."
