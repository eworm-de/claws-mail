#!/bin/sh

autopoint --force \
  && aclocal -I m4 \
  && libtoolize --force --copy \
  && autoheader \
  && automake --add-missing --foreign --copy \
  && autoconf \
  && ./configure --enable-maintainer-mode $@
