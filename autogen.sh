#!/bin/sh

cd po
/bin/sh poconv.sh
cd ..

autopoint --force \
  && aclocal -I m4 \
  && libtoolize --force --copy \
  && autoheader \
  && automake --add-missing --foreign --copy \
  && autoconf \
  && ./configure --enable-maintainer-mode $@
