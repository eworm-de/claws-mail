/* config.h.in.  Generated automatically from configure.in by autoheader 2.13.  */

/* Define to 1 if translation of program messages to the user's native language
   is requested. */
#define ENABLE_NLS 1
//tm/* #undef HAVE_CATGETS */
/* Define if the GNU gettext() function is already present or preinstalled. */
#define HAVE_GETTEXT 1
/* Define if your <locale.h> file defines LC_MESSAGES. */
#undef HAVE_LC_MESSAGES

/* #undef HAVE_STPCPY */

//#define HAVE_GDK_PIXBUF 1
#undef HAVE_GDK_PIXBUF
#undef HAVE_GDK_IMLIB

#undef USE_XIM

/* OK: include gthread-2.0.lib * Whether to use multithread or not  */
#undef USE_THREADS

/* Define if you want IPv6 support.  */
#undef INET6

/* GPGME has no VC project * Define if you use GPGME to support OpenPGP */
//#undef USE_GPGME
#define USE_GPGME 1

/* Define if you use PSPELL to support spell checking */
#define USE_ASPELL 1

/* Define PSPELL's default directory */
#define ASPELL_PATH ""

/* Define if you use OpenSSL to support SSL */
#define USE_OPENSSL 1

/* Define if you want JPilot support in addressbook.  */
#undef USE_JPILOT

/* OPENLDAP doesnt compile * Define if you want LDAP support in addressbook.  */
#undef USE_LDAP

/* Define to `unsigned int' if <stddef.h> or <wchar.h> doesn't define.  */
#undef wint_t

/* Used to test for a u32 typedef */
#undef HAVE_U32_TYPEDEF


#define CLAWS 1

//#define PACKAGE_DATA_DIR "/usr/local/share/sylpheed"

/* Define to one of _getb67, GETB67, getb67 for Cray-2 and Cray-YMP systems.
   This function is required for alloca.c support on those systems.  */

#undef CRAY_STACKSEG_END

/* Define if using alloca.c.  */
#undef C_ALLOCA

/* Define to 1 if translation of program messages to the user's native
   language is requested. */
//#define ENABLE_NLS 1

/* Define if you have alloca, as a function or macro.  */
#undef HAVE_ALLOCA

/* Define if you have <alloca.h> and it should be used (not on Ultrix).  */
#undef HAVE_ALLOCA_H

/* Define if you have the <argz.h> header file.  */
#undef HAVE_ARGZ_H

/* Define if you have the dcgettext function.  */
#undef HAVE_DCGETTEXT

/* Define if you have the <dirent.h> header file.  */
#undef HAVE_DIRENT_H

/* Define if you have the <dlfcn.h> header file.  */
#undef HAVE_DLFCN_H

/* Define if you have the fchmod function.  */
#undef HAVE_FCHMOD

/* Define if you have the <fcntl.h> header file.  */
#define HAVE_FCNTL_H 1

/* Define if you have the feof_unlocked function.  */
#undef HAVE_FEOF_UNLOCKED

/* Define if you have the fgets_unlocked function.  */
#undef HAVE_FGETS_UNLOCKED

/* Define if you have the flock function.  */
#undef HAVE_FLOCK

/* Define if you have the getcwd function.  */
#undef HAVE_GETCWD

/* Define if you have the getegid function.  */
#undef HAVE_GETEGID

/* Define if you have the geteuid function.  */
#undef HAVE_GETEUID

/* Define if you have the getgid function.  */
#undef HAVE_GETGID

/* Define if you have the gethostname function.  */
#undef HAVE_GETHOSTNAME

/* Define if you have the getpagesize function.  */
#undef HAVE_GETPAGESIZE

/* Define if the GNU gettext() function is already present or preinstalled. */
//#define HAVE_GETTEXT 1

/* Define if you have the getuid function.  */
#undef HAVE_GETUID

/* Define if you have the iconv() function. */
#define HAVE_ICONV 1
/* Define as const if the declaration of iconv() needs const. */
#define ICONV_CONST const

/* Define if you have the inet_addr function.  */
#undef HAVE_INET_ADDR

/* Define if you have the inet_aton function.  */
#undef HAVE_INET_ATON

/* Define if you have the <inttypes.h> header file. */
//tm#define HAVE_INTTYPES_H 1

/* Define if you have the iswalnum function.  */
#define HAVE_ISWALNUM 1

/* Define if you have the iswspace function.  */
#define HAVE_ISWSPACE 1

/* Define if you have <langinfo.h> and nl_langinfo(CODESET). */
#undef HAVE_LANGINFO_CODESET

/* Define if you have the <lber.h> header file.  */
#undef HAVE_LBER_H

/* Define if your <locale.h> file defines LC_MESSAGES. */
//#define HAVE_LC_MESSAGES 1

/* Define if you have the <ldap.h> header file.  */
#undef HAVE_LDAP_H

/* Define if you have the compface library (-lcompface).  */
#define HAVE_LIBCOMPFACE 1

/* Define if you have the jconv library (-ljconv).  */
#define HAVE_LIBJCONV 1
//#undef HAVE_LIBJCONV

/* Define if you have the <libpisock/pi-address.h> header file.  */
#undef HAVE_LIBPISOCK_PI_ADDRESS_H

/* Define if you have the <libpisock/pi-appinfo.h> header file.  */
#undef HAVE_LIBPISOCK_PI_APPINFO_H

/* Define if you have the <libpisock/pi-args.h> header file.  */
#undef HAVE_LIBPISOCK_PI_ARGS_H

/* Define if you have the xpg4 library (-lxpg4).  */
#undef HAVE_LIBXPG4

/* Define if you have the <limits.h> header file.  */
#undef HAVE_LIMITS_H

/* Define if you have the <locale.h> header file.  */
#define HAVE_LOCALE_H 1

/* Define if you have the lockf function.  */
#undef HAVE_LOCKF

/* Define if you have the <malloc.h> header file.  */
#undef HAVE_MALLOC_H

/* Define if you have the <memory.h> header file. */
//#define HAVE_MEMORY_H 1

/* Define if you have the mempcpy function.  */
#undef HAVE_MEMPCPY

/* Define if you have the mkdir function.  */
#undef HAVE_MKDIR

/* Define if you have the mkstemp function.  */
#undef HAVE_MKSTEMP

/* Define if you have the mktime function.  */
#undef HAVE_MKTIME

/* Define if you have a working `mmap' system call.  */
#undef HAVE_MMAP

/* Define if you have the munmap function.  */
#undef HAVE_MUNMAP

/* Define if you have the <ndir.h> header file.  */
#undef HAVE_NDIR_H

/* Define if you have the <nl_types.h> header file.  */
#undef HAVE_NL_TYPES_H

/* Define if you have the <paths.h> header file.  */
#undef HAVE_PATHS_H

/* Define if you have the <pi-address.h> header file.  */
#undef HAVE_PI_ADDRESS_H

/* Define if you have the <pi-appinfo.h> header file.  */
#undef HAVE_PI_APPINFO_H

/* Define if you have the <pi-args.h> header file.  */
#undef HAVE_PI_ARGS_H

/* Define if you have the <pthread.h> header file.  */
#undef HAVE_PTHREAD_H

/* Define if you have the putenv function.  */
#undef HAVE_PUTENV

/* Define if you have the setenv function.  */
#undef HAVE_SETENV

/* Define if you have the setlocale function.  */
#undef HAVE_SETLOCALE

/* Define if you have the socket function.  */
#undef HAVE_SOCKET

/* Define if you have the <stddef.h> header file.  */
#undef HAVE_STDDEF_H

/* Define if you have the <stdint.h> header file. */
//#define HAVE_STDINT_H 1

/* Define if you have the <stdlib.h> header file.  */
#define HAVE_STDLIB_H 1

/* Define if you have the stpcpy function.  */
#undef HAVE_STPCPY

/* Define if you have the strcasecmp function.  */
#undef HAVE_STRCASECMP

/* Define if you have the strchr function.  */
#undef HAVE_STRCHR

/* Define if you have the strdup function.  */
#undef HAVE_STRDUP

/* Define if you have the <strings.h> header file. */
//#define HAVE_STRINGS_H 1

/* Define if you have the <string.h> header file.  */
#define HAVE_STRING_H 1

/* Define if you have the strstr function.  */
#undef HAVE_STRSTR

/* Define if you have the strtoul function.  */
#undef HAVE_STRTOUL

/* Define if you have the <sys/dir.h> header file.  */
#undef HAVE_SYS_DIR_H

/* Define if you have the <sys/file.h> header file.  */
#undef HAVE_SYS_FILE_H

/* Define if you have the <sys/ndir.h> header file.  */
#undef HAVE_SYS_NDIR_H

/* Define if you have the <sys/param.h> header file.  */
#undef HAVE_SYS_PARAM_H

/* Define if you have the <sys/stat.h> header file. */
//#define HAVE_SYS_STAT_H 1

/* Define if you have the <sys/types.h> header file. */
//#define HAVE_SYS_TYPES_H 1

/* Define if you have the <sys/utsname.h> header file.  */
#undef HAVE_SYS_UTSNAME_H

/* Define if you have <sys/wait.h> that is POSIX.1 compatible.  */
#undef HAVE_SYS_WAIT_H

/* Define if you have the towlower function.  */
#define HAVE_TOWLOWER 1

/* Define if you have the tsearch function.  */
#undef HAVE_TSEARCH

/* Define if you have the uname function.  */
#undef HAVE_UNAME

/* Define if you have the <unistd.h> header file.  */
#undef HAVE_UNISTD_H

/* Define if you have the <wchar.h> header file.  */
#define HAVE_WCHAR_H 1

/* Define if you have the wcscpy function.  */
#define HAVE_WCSCPY 1

/* Define if you have the wcslen function.  */
#define HAVE_WCSLEN 1

/* Define if you have the wcsncpy function.  */
#define HAVE_WCSNCPY 1

/* Define if you have the wcsstr function.  */
#define HAVE_WCSSTR 1

/* Define if you have the wcswcs function.  */
#define HAVE_WCSWCS 1

/* Define if you have the <wctype.h> header file.  */
#define HAVE_WCTYPE_H 1

/* Define if you have the __argz_count function.  */
#undef HAVE___ARGZ_COUNT

/* Define if you have the __argz_next function.  */
#undef HAVE___ARGZ_NEXT

/* Define if you have the __argz_stringify function.  */
#undef HAVE___ARGZ_STRINGIFY

/* The number of bytes in a unsigned int.  */
#undef SIZEOF_UNSIGNED_INT

/* The number of bytes in a unsigned long.  */
#undef SIZEOF_UNSIGNED_LONG

/* The number of bytes in a unsigned short.  */
#undef SIZEOF_UNSIGNED_SHORT

/* If using the C implementation of alloca, define if you know the
   direction of stack growth for your system; otherwise it will be
   automatically deduced at run-time.
 STACK_DIRECTION > 0 => grows toward higher addresses
 STACK_DIRECTION < 0 => grows toward lower addresses
 STACK_DIRECTION = 0 => direction of growth unknown */
#undef STACK_DIRECTION

/* Define if you have the ANSI C header files.  */
#undef STDC_HEADERS

/* Define if your <sys/time.h> declares struct tm.  */
#undef TM_IN_SYS_TIME

/* Define if lex declares yytext as a char * by default, not a char[].  */
#undef YYTEXT_POINTER

/* Define to empty if the keyword does not work.  */
#undef const

/* Define as __inline if that's what the C compiler calls it.  */
#undef inline

/* Define to `long' if <sys/types.h> doesn't define.  */
#undef off_t

/* Define to `int' if <sys/types.h> doesn't define.  */
#undef pid_t

/* Define to `unsigned' if <sys/types.h> doesn't define.  */
#undef size_t

/* Others */
#define HAVE_DOSISH_SYSTEM	1
#define TARGET_ALIAS			"Win32"
#define MANUALDIR			"doc\\manual"
#define SYSCONFDIR			"etc"
#define LOCALEDIR			"locale"
#define FAQDIR				"doc\\faq"
