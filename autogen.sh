#!/bin/sh

aclocal -I ac \
  && autoconf \
  && autoheader \
  && automake --add-missing --foreign --copy \
  && ./configure $@
