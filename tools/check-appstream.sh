#!/bin/bash
exec 2>&1
files=`find $(dirname $(readlink -f $0))/../ -regextype posix-extended -regex "^.*\.(metainfo|appdata).xml(.in)?"`
appstream-util validate ${files}
