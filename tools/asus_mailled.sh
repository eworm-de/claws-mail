#!/bin/bash
# Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
# Copyright (C) 2005 Colin Leroy <colin@colino.net
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

# This script, when used as newmail_notify_cmd (Configuration/Receive),
# makes the mail LED blink on new or unread mail for ASUS laptops with
# asus_acpi compiled in the kernel or loaded as module.

[ -f /tmp/mailcheck-$LOGNAME ] && exit
touch /tmp/mailcheck-$LOGNAME
last=0
while true; do
	sleep 1
	num=`sylpheed --status | cut -d' ' -f2`
	if [ "$num" == "Sylpheed" ]; then
		#not running
		rm /tmp/mailcheck-$LOGNAME
		echo 0 > /proc/acpi/asus/mled
		exit
	fi
	if [ "$num" != "0" ]; then
		echo $last > /proc/acpi/asus/mled
		if [ "$last" == "0" ]; then
			last=1;
		else	
			last=0;
		fi
	else
		echo 0 > /proc/acpi/asus/mled
	fi
done;
