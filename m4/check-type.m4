dnl CLAWS_CHECK_TYPE(TYPE, DEFAULT [, INCLUDES, COMMENT])
dnl
dnl Like AC_CHECK_TYPE, but in addition to `sys/types.h', `stdlib.h' and
dnl `stddef.h' checks files included by INCLUDES, which should be a
dnl series of #include statements.  If TYPE is not defined, define it
dnl to DEFAULT.
dnl
dnl Copyright (C) 2003 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.
AC_DEFUN([CLAWS_CHECK_TYPE],
[AC_MSG_CHECKING(for $1)
AC_CACHE_VAL(claws_cv_type_$1,
[AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <sys/types.h>
#if STDC_HEADERS
#include <stdlib.h>
#include <stddef.h>
#endif
$3
]], [[
#undef $1
int a = sizeof($1);
]])],[claws_cv_type_$1=yes],[claws_cv_type_$1=no])])dnl
AC_MSG_RESULT($claws_cv_type_$1)
if test $claws_cv_type_$1 = no; then
  AC_DEFINE($1, $2, $4)
fi
])
