#!/usr/bin/perl

#  * Copyright 2002 Paul Mangan <claws@thewildbeast.co.uk>
#  *
#  * This file is free software; you can redistribute it and/or modify it
#  * under the terms of the GNU General Public License as published by
#  * the Free Software Foundation; either version 2 of the License, or
#  * (at your option) any later version.
#  *
#  * This program is distributed in the hope that it will be useful, but
#  * WITHOUT ANY WARRANTY; without even the implied warranty of
#  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#  * General Public License for more details.
#  *
#  * You should have received a copy of the GNU General Public License
#  * along with this program; if not, write to the Free Software
#  * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
#
# OOo2sylpheed.pl	helper script to send documents from OpenOffice.org
#			to sylpheed

use strict;

my $input = <>;

$ARGV =~ s/^"attachment='file:\/\///;
$ARGV =~ s/'"$//;
$ARGV =~ s/%20/ /g;
exec "/usr/local/bin/sylpheed --attach \"$ARGV\"";
## change the line above to point to your sylpheed executable

exit;
