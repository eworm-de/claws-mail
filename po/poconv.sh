#!/bin/sh
for pofile in *.po; do
    pocharset=`grep "Content-Type: text/plain; charset" $pofile | sed -e 's/\"Content-Type: text\/plain; charset=\\(.*\\)\\\n\"/\\1/'`
    echo "$pofile:"
    if test -z $pocharset; then
        echo "missing charset!"
    elif test "$pocharset" = "UTF-8"; then
        echo "charset is already UTF-8. skipping..."
    else
        echo -n "converting $pocharset to UTF-8..."
        iconv -f $pocharset -t UTF-8 $pofile > $pofile.utf8
        echo "done!"
        echo -n "Replace charset description to UTF-8..."
        cp $pofile $pofile.bak
        sed -e 's/Content-Type: text\/plain; charset=.*\\n/Content-Type: text\/plain; charset=UTF-8\\n/' < $pofile.utf8 > $pofile
        echo "done!"
        rm $pofile.utf8
    fi
    echo
done
