#!/bin/sh
# Copyright 1999-2014 the Claws Mail team.
# This file is part of Claws Mail package, and distributed under the
# terms of the General Public License version 3 (or later).
# See COPYING file for license details.

bisonver=`bison --version`

if [ "$bisonver" = "" ]; then
	echo Bison is needed to compile Claws Mail git
	exit 1
fi

if [ "$LEX" != "" ]; then
	flexver=`$LEX --version|sed "s/.* //"`
else
	flexver=`flex --version|sed "s/.* //"`
fi

if [ "$flexver" = "" ]; then
	echo Flex 2.5.31 or greater is needed to compile Claws Mail git
	exit 1
else
	flex_major=`echo $flexver|sed "s/\..*//"`
	flex_minor=`echo $flexver|sed "s/$flex_major\.\(.*\)\..*/\1/"`
	flex_micro=`echo $flexver|sed "s/$flex_major\.$flex_minor\.\(.*\)/\1/"`

	flex_numversion=$(expr \
		$flex_major \* 10000 + \
		$flex_minor \* 100 + \
		$flex_micro)

	if [ $flex_numversion -lt 20531 ]; then
		echo Flex 2.5.31 or greater is needed to compile Claws Mail git
		exit 1
	fi
fi

aclocal -I m4 \
  && libtoolize --force --copy \
  && autoheader \
  && automake --add-missing --foreign --copy \
  && autoconf 
if test -z "$NOCONFIGURE"; then
exec ./configure --enable-maintainer-mode $@
fi   
