#!/bin/bash

# textviewer.sh
# Copyright 2003 Luke Plant <L.Plant.98@cantab.net>

# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
# 02111-1307, USA.

##############################################################################
#
# This script is a text viewer designed to be used with sylpheed-claws actions
# Set up an action with the command line:  textviewer.sh %p |
#
# The script will try to detect file type automatically, and then
# invokes a relevant program to print the file in plain text to
# the standard output.
#
# From v 0.9.7claws7, sylpheed-claws sets the temporary file
# of a part to XXXXXX.mimetmp.[filename of attachment]
# This means we can use the extension of the filename for checking.
# Also use the program 'file' if that fails.
#
# To extend the script just follow the patterns that already exist, or
# contact the author if you have problems.

##############################################################################
#
# Change Log
#
# 2003-12-30
#	added the script to sylpheed-claws/tools
#
# 2003-12-30
#	- use 'fold' after 'unrtf' to wrap to a nice width
#	- added basic file sanity checks
#
# 2003-12-29
#	Added recognition for "Zip " from 'file' output
#
# 2003-12-19
#	Initial public release
#
###############################################################################

if [ $# -eq 0 ]
then
  	echo "No filename supplied." >&2 
  	echo "Usage: textviewer.sh FILE" >&2 
  	exit 1
fi

[ -f "$1" ] ||
{
	echo "File \"$1\" does not exist or is not a regular file." >&2
	exit 1
}

[ -r "$1" ] ||
{	
	echo "Cannot read file \"$1\"." >&2
	exit 1
}

FILETYPE=`file --brief "$1"` || 
{
	echo "Please install the command 'file' to use this script." >&2
	exit 1 
};
case "$1" in 
	*.doc)	TYPE=MSWORD	;;
	*.zip)	TYPE=ZIP	;;
	*.tar.gz|*.tgz)	TYPE=TARGZ ;;
	*.tar.bz)	TYPE=TARBZ ;;
	*.gz)	TYPE=GZIP	;;
	*.bz)	TYPE=BZIP	;;
	*.tar)	TYPE=TAR	;;
	*.diff)	TYPE=TEXT	;;
	*.txt)	TYPE=TEXT	;;
	*.rtf)	TYPE=RTF	;;
esac

if [ "$TYPE" == "" ]	
then
	case $FILETYPE in 
		"'diff'*")	TYPE=TEXT	;;
		gzip*)		TYPE=GZIP ;;
		"Zip "*) 	TYPE=ZIP  ;;
		ASCII*)		TYPE=TEXT	;;
		"Rich Text Format"*)	
				TYPE=RTF  ;;
		"smtp mail text"* | "RFC 822 mail text"*)	
				TYPE=TEXT	;;
		"Bourne shell script"* | "Bourne-Again shell script"*)
				TYPE=TEXT	;;
	esac
fi

case $TYPE in
	TARGZ) 	echo -e "Tarball contents:\n" 		; 
		tar -tzvf "$1"				;;

	TARBZ)	echo -e "Tarball contents:\n" 		; 
		tar -tjvf "$1"				;;

	TAR)	echo -e "Tar archive contents:\n" 	; 
		tar -tvf "$1" 				;;

	GZIP)	echo -e "GZip file contents:\n" 	; 
		gunzip -l "$1" 				;;

	ZIP)	unzip -l "$1"				;;

	RTF)	which unrtf > /dev/null  2>&1 || 
		{
			echo "Program 'unrtf' for displaying RTF files not found" >&2
			exit 1
		};
		unrtf -t text "$1" 2>/dev/null | egrep  -v '^### ' | fold -s -w 72  ;;

	TEXT)	cat "$1"				;;

	MSWORD) which antiword  > /dev/null  2>&1 || 
		{
			echo "Program 'antiword' for displaying MS Word files not found" >&2
			exit 1 
		};
		antiword "$1" 				;;

	*)	echo "Unsupported file type \"$FILETYPE\", cannot display.";;
esac
