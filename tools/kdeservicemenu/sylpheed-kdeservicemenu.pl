#!/usr/bin/perl

#  * Copyright 2004 Paul Mangan <claws@thewildbeast.co.uk>
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

unless ($ARGV[0]) { exit; }

$count = $#ARGV;

($str,$strpt) = split_parts();

if ($ARGV[0] eq "zip") {
	exec "zip -r archive.zip $str;"
	    ."sylpheed --compose --attach \"archive.zip\"";
} elsif ($ARGV[0] eq "tar") {
	exec "tar -c -f archive.tar $str;"
	    ."sylpheed --compose --attach \"archive.tar\"";
} elsif ($ARGV[0] eq "tarbz2") {
	exec "tar -cj -f archive.tar.bz2 $str;"
	    ."sylpheed --compose --attach \"archive.tar.bz2\"";
} elsif ($ARGV[0] eq "targz") {
	exec "tar -cz -f archive.tar.gz $str;"
	    ."sylpheed --compose --attach \"archive.tar.gz\"";
} elsif ($ARGV[0] eq "gzip") {
	exec "gzip  $str;"
	    ."sylpheed --compose --attach $strpt";
} elsif ($ARGV[0] eq "bzip2") {
	exec "bzip2  $str;"
	    ."sylpheed --compose --attach $strpt";
} elsif ($ARGV[0] eq "attachfile") {
	exec "sylpheed --compose --attach $str";
}
exit;

sub split_parts {
	local $selectedParts = "";
	local $sParts = "";
	while ($count > 0) {
		@s = split("/", $ARGV[$count]);
		$count--;
		$p = pop(@s);
		$selectedParts .= "\"$p\" ";
		if ($ARGV[0] eq "gzip") {
			$sParts .= "\"$p.gz\" ";
		} elsif ($ARGV[0] eq "bzip2") {
			$sParts .= "\"$p.bz2\" ";
		}
	}
	return ($selectedParts,$sParts);
}