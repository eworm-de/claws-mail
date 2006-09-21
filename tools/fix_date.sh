#!/bin/sh

# usage: fix_date.sh <filename> [<filename>..]
# It will replace the Date: value w/ the one picked up from more recent
# Received: field if this field resides in one line. Otherwise, it will
# take the file modification time (using a RFC 2822-compliant form).
# If no X-Original-Date already exist, the former Date value will be set
# in such field.

# TODO: add a switch to replace only non RFC-compliant Date: headers

VERSION="0.0.3"


function version()
{
	echo "$VERSION"
	exit 0
}

function usage()
{
	echo "usage:"
	echo "  ${0##*/} [<switches>] <filename> [<filename> ..]"
	echo "switches:"
	echo "  --help     display this help then exit"
	echo "  --version  display version information then exit"
	echo "  --force    force writting of Date: header even if it already exists"
	echo "  --debug    turn on debug information (be more verbose)"
	echo "  --         end of switches (in case a filename starts with a -)"
	exit $1
}


# use --force to always write the Date header
# otherwise, the Date header will be written if only it doesn't already
# exist
FORCE=0
# use --debug to display more information about what's performed
DEBUG=0

while [ -n "$1" ]
do
	case "$1" in
	--help)		usage 0;;
	--version)	version;;
	--force)	FORCE=1;;
	--debug)	DEBUG=1;;
	--)			shift
				break;;
	-*)			echo "error: unrecognized switch '$1'"
				usage 1;;
	*)			break;;
	esac
	shift
done

test $# -lt 1 && \
	usage 1

TMP="/tmp/${0##*/}.tmp"

while [ -n "$1" ]
do
	# skip if file is empty or doesn't exist
	if [ ! -s "$1" ]
	then
		shift
		continue
	fi

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
		# don't forget the leading whitespace here
		REPLACEMENT_DATE=" $FILE_DATE"
		REPLACEMENT="file date"
#		REPLACEMENT_DATE=" $SYSTEM_DATE"
#		REPLACEMENT="system date"
	else
		REPLACEMENT_DATE="$RECEIVED_DATE"
		REPLACEMENT="received date"
	fi

	# ensure that a X-Original-Date is set (but don't override it)
	if [ -z "$X_ORIGINAL_DATE" ]
	then
		if [ -z "$DATE" ]
		then
			echo "X-Original-Date:$REPLACEMENT_DATE" > "$TMP"
		else
			test $FORCE -eq 1 && \
				echo "X-Original-Date:$DATE" > "$TMP"
		fi
	else
		:> "$TMP"
	fi

	# replace/set the date and write all lines
	if [ -z "$DATE" ]
	then
		test $DEBUG -eq 1 && \
			echo "$1: date not found, using $REPLACEMENT now"
		echo "Date:$REPLACEMENT_DATE" >> "$TMP"
		cat "$1" >> "$TMP"
	else
		if [ $FORCE -eq 1 ]
		then
			test $DEBUG -eq 1 && \
				echo "$1: date already found, replacing with $REPLACEMENT"
			sed "s/^Date: .*/Date:$REPLACEMENT_DATE/" "$1" >> "$TMP"
		else
			test $DEBUG -eq 1 && \
				echo "$1: date already found, skipping"
			cat "$1" >> "$TMP"
		fi
	fi

	# uncomment the following line to backup the original file
	#mv -f "$1" "$1.bak"

	mv -f "$TMP" "$1"
	if [ $? -ne 0 ]
	then
		echo "error while moving '$TMP' to '$1'"
		exit 1
	fi

	shift
done
exit 0
