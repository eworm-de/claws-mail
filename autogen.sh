#!/bin/sh

cd po
/bin/sh poconv.sh
cd ..

aclocal -I ac \
  && libtoolize --force --copy \
  && autoheader \
  && automake --add-missing --foreign --copy \
  && autoconf \
  && ./configure --enable-maintainer-mode $@
