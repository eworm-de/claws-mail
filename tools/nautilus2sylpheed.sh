#!/bin/bash

# nautilus2sylpheed.sh
# Copyright 2004 Reza Pakdel <hrpakdel@cpsc.ucalgary.ca>

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

# This script will recursively attach a number of selected 
# files/directories from Nautilus to a new blank e-mail.

attachments=""

attach_dir()
{   
    if [ -d $1 ] 
    then
	for j in $(ls $1)
	do
	  attach_dir $1"/"$j
	done
    else
	attachments="$attachments $1"
    fi
}

for i in $*
do
  if [ -d $i ]
  then
      for file in $(ls $i)
      do
	attach_dir $i"/"$file
      done
  else
      attachments="$attachments $i";
  fi  
done

echo "-----------"
echo $attachments
sylpheed --compose --attach $attachments

