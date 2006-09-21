#!/bin/sh

# usage: fix_date.sh <filename> [<filename>..]
# It will replace the Date: value w/ the one picked up from more recent
# Received: field if this field resides in one line. Otherwise, it will
# take the file modification time (using a RFC 2822-compliant form).
# If no X-Original-Date already exist, the former Date value will be set
# in such field.

if [ $# -lt 1 ]
then
	echo "usage: ${0##*/} <filename> [<filename> ..]"
	exit 1
fi

TMP="/tmp/${0##*/}.tmp"
while [ -n "$1" ]
do
	test ! -s "$1" && \
		continue

	X_ORIGINAL_DATE=$(grep -Eim 1 '^X-Original-Date: ' "$1" | cut -d ':' -f 2)
	DATE=$(grep -Eim 1 '^Date: ' "$1" | cut -d ':' -f 2)
	RECEIVED_DATE=$(grep -Eim 1 ';( (Mon|Tue|Wed|Thu|Fri|Sat|Sun),)? [0-9]+ (Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dev) [0-9]+ [0-9]+:[0-9]+:[0-9}+ [-+][0-9]+' "$1" | cut -d ';' -f 2)
# strict, day of week needed
#	RECEIVED_DATE=$(grep -Eim 1 '; (Mon|Tue|Wed|Thu|Fri|Sat|Sun), [0-9]+ (Jan|Feb|Mar|Apr|May|Jun|Jul|Aug|Sep|Oct|Nov|Dev) [0-9]+ [0-9]+:[0-9]+:[0-9}+ [-+][0-9]+' "$1" | cut -d ';' -f 2)
	FILE_DATE=$(ls -l --time-style="+%a, %d %b %Y %X %z" "$1" | tr -s ' ' ' ' | cut -d ' ' -f 6-11)
	# we could also use the system date as a possible replacement
	#SYSTEM_DATE="$(date -R)"

	# determine which replacement date to use
	if [ -z "$RECEIVED_DATE" ]
	then
		# don't forget to add the leading whitespace
		REPLACEMENT_DATE=" $FILE_DATE"
	else
		REPLACEMENT_DATE="$RECEIVED_DATE"
	fi

	# ensure that a X-Original-Date is set
	if [ -z "$X_ORIGINAL_DATE" ]
	then
		test -z "$DATE" && \
			echo "X-Original-Date:$REPLACEMENT_DATE" > "$TMP" || \
			echo "X-Original-Date:$DATE" > "$TMP"
	else
		:> "$TMP"
	fi

	# replace/set the date and write all lines
	if [ -z "$DATE" ]
	then
		echo "Date:$REPLACEMENT_DATE" >> "$TMP"
		cat "$1" >> "$TMP"
	else
		sed "s/^Date: .*/Date:$REPLACEMENT_DATE/" "$1" >> "$TMP"
	fi

	# uncomment the following line to backup the original file
	#mv -f "$1" "$1.bak"

	mv -f "$TMP" "$1"
	if [ $? -ne 0 ]
	then
		echo "error while moving $TMP to $1"
		exit 1
	fi

	shift
done
exit 0
