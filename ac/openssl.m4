dnl ******************************
dnl OpenSSL
dnl ******************************

AC_DEFUN([AM_PATH_OPENSSL],
[dnl
	USE_OPENSSL=0

	AC_ARG_ENABLE(openssl, [  --enable-openssl        Attempt to use OpenSSL for SSL support.],
		[ac_cv_enable_openssl=$enableval], [ac_cv_enable_openssl=no])

	dnl detect OpenSSL
	if test "x${ac_cv_enable_openssl}" != "xno"; then
		AC_ARG_WITH(openssl-includes, [  --with-openssl-includes=PREFIX     Location of OpenSSL includes.],
			with_openssl_includes="$withval", with_openssl_includes="/usr/include")
		have_openssl_includes="no"
		if test "x${with_openssl_includes}" != "xno"; then
			CPPFLAGS_save="$CPPFLAGS"
		
			AC_MSG_CHECKING(for OpenSSL includes)
			AC_MSG_RESULT("")
		
			CPPFLAGS="$CPPFLAGS -I$with_openssl_includes"
			AC_CHECK_HEADERS(openssl/ssl.h openssl/x509.h, [ openssl_includes="yes" ])
			CPPFLAGS="$CPPFLAGS_save"
		
			if test "x{$openssl_includes}" != "xno" -a "x{$openssl_includes}" != "x"; then
				have_openssl_includes="yes"
				OPENSSL_CFLAGS="-I$with_openssl_includes"
			else
				OPENSSL_CFLAGS=""
			fi
		else
			AC_MSG_CHECKING(for OpenSSL includes)
			AC_MSG_RESULT(no)
		fi
	
		AC_ARG_WITH(openssl-libs, [  --with-openssl-libs=PREFIX         Location of OpenSSL libs.],
			with_openssl_libs="$withval")
		if test "x${with_openssl_libs}" != "xno" -a "x${have_openssl_includes}" != "xno"; then
			case $with_openssl_libs in
			""|-L*) ;;
			*) with_openssl_libs="-L$with_openssl_libs" ;;
			esac
	
			AC_CHECK_LIB(dl, dlopen, DL_LIBS="-ldl", DL_LIBS="")
			AC_CACHE_CHECK([for OpenSSL libraries], openssl_libs,
			[
				LIBS_save="$LIBS"
				LIBS="$LIBS $with_openssl_libs -lssl -lcrypto $DL_LIBS"
				AC_TRY_LINK_FUNC(SSL_read, openssl_libs="yes", openssl_libs="no")
				LIBS="$LIBS_save"
			])
			if test "x${openssl_libs}" != "xno"; then
				AC_DEFINE(USE_OPENSSL, 1, [Define if you use OpenSSL to support SSL])
				USE_OPENSSL=1
				msg_ssl="yes (OpenSSL)"
				OPENSSL_LIBS="$with_openssl_libs -lssl -lcrypto $DL_LIBS"
			else
				OPENSSL_CFLAGS=""
				OPENSSL_LIBS=""
			fi
		else
			AC_MSG_CHECKING(for OpenSSL libraries)
			AC_MSG_RESULT(no)
		fi
	else
		OPENSSL_CFLAGS=""
		OPENSSL_LIBS=""
		ac_cv_enable_openssl="no"
	fi

	AC_SUBST(OPENSSL_CFLAGS)
	AC_SUBST(OPENSSL_LIBS)
])
