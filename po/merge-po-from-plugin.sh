#!/bin/bash
if [ "$1" = "" -o "$2" = "" ]; then
	echo usage: $0 path/to/oldplugin/po/ ../src/plugins/newplugin
	exit 1
fi

#Check if new files are in POTFILES

sources=`find $2 -name '*.[ch]'`
translatedfiles=`grep -rwl '_(' $sources`
for file in $translatedfiles; do
	file=`echo $file|sed "s/^\.\.\///"`
	inPOTFILESin=`grep $file POTFILES.in`
	inPOTFILES=`grep $file POTFILES`
	if [ "$inPOTFILESin" = "" ]; then
		echo "$file not in POTFILES.in, please add it"
		err=1
	elif [ "$inPOTFILES" = "" ]; then
		echo "$file not in POTFILES, please autogen.sh"
		err=1
	fi
done
if [ "$err" = "1" ]; then
	exit 1
fi

#update all with new files
make update-po

#Merge with old plugin po files
for pluginpo in $1/*.po; do
	corepo=`basename $pluginpo`
	msgcat --use-first $corepo $pluginpo > $corepo.new && mv $corepo.new $corepo
done;
