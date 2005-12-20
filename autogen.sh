#!/bin/sh

# ***** W32 build script *******
# Used to cross-compile for Windows.
if test "$1" = "--build-w32"; then
    tmp=`dirname $0`
    tsdir=`cd "$tmp"; pwd`
    shift
    if [ ! -f $tsdir/config/config.guess ]; then
        echo "$tsdir/config/config.guess not found" >&2
        exit 1
    fi
    build=`$tsdir/config/config.guess`

    [ -z "$w32root" ] && w32root="$HOME/w32root"
    echo "Using $w32root as standard install directory" >&2
    
    if i586-mingw32msvc-gcc --version >/dev/null 2>&1 ; then
        host=i586-mingw32msvc
        crossbindir=/usr/$host/bin
    else
       echo "Debian's mingw32 cross-compiler packet is required" >&2
       exit 1
    fi
   
    if [ -f "$tsdir/config.log" ]; then
        if ! head $tsdir/config.log | grep "$host" >/dev/null; then
            echo "Pease run a 'make distclean' first" >&2
            exit 1
        fi
    fi

    ./configure --enable-maintainer-mode --prefix=${w32root}  \
             --host=i586-mingw32msvc --build=${build} \
             --with-lib-prefix=${w32root} \
             --with-libiconv-prefix=${w32root} \
             --with-gpg-error-prefix=${w32root} \
	     --with-gpgme-prefix=${w32root} \
             --with-config-dir="Sylpheed-claws" \
             --disable-openssl --disable-dillo-viewer-plugin \
             --disable-nls --disable-libetpan --disable-aspell \
             --disable-trayicon-plugin \
             PKG_CONFIG_LIBDIR="$w32root/lib/pkgconfig"

    rc=$?
    exit $rc
fi
# ***** end W32 build script *******


aclocal -I m4 \
  && libtoolize --force --copy \
  && autoheader \
  && automake --add-missing --foreign --copy \
  && autoconf \
  && ./configure --enable-maintainer-mode $@
