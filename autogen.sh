#!/bin/sh

aclocal -I ac \
  && libtoolize --force \
  && autoheader \
  && automake --add-missing --foreign --copy \
  && autoconf \
  && ./configure --enable-maintainer-mode $@
