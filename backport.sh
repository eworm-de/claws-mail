#!/bin/sh
if [ "$1" == "" ]; then
	echo syntax: $0 version
	exit
fi
cmd=`grep -w $1 ../sylpheed-claws/PATCHSETS`
if [ "$cmd" == "" ]; then
	echo no patchset found
	exit
fi
sh -c "$cmd"
patch -p0 < $1.patchset || exit
rm -f commitHelper.msg
echo -e "\t\tFrom $1" > commitHelper.msg
files=`grep Index: $1.patchset|sed "s/Index://"`
./commitHelper $files
rm -f commitHelper.msg
