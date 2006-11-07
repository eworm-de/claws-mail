#!/usr/bin/perl

#  * Copyright © 2003 Paul Mangan <claws@thewildbeast.co.uk>
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
#  * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#  *

my $google = "http://www.google.com/search?q";
$_ = <>;

chdir($ENV{HOME} . "/.claws-mail") || die("Can't find your .claws-mail directory\n");

open (SYLRC, "<sylpheedrc") || die("Can't open the sylpheedrc file\n");
	@rclines = <SYLRC>;
close SYLRC;

foreach $rcline (@rclines) {
	if ($rcline =~ m/^uri_open_command/) {
		chomp $rcline;
		@browser = split(/=/, $rcline);
		$browser[1] =~ s/%s/$google=$_/;
	}
}
system("$browser[1]&");

exit;


