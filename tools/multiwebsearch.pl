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
#  * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
#  *

use Getopt::Long;

my $where = '';
my $what  = '';

GetOptions("where=s"	=> \$where,
	   "what=s"	=> \$what);

chdir($ENV{HOME} . "/.sylpheed") 
	|| die("Can't find your ~/.sylpheed directory\n");

open (CONF, "<multisearch.conf") 
	|| die("Can't open ~/.sylpheed/multisearch.conf\n");
	@conflines = <CONF>;
close CONF;

foreach $confline (@conflines) {
	if ($confline =~ m/^$where\|/) {
		chomp $confline;
		@parts = split(/\|/, $confline);
		$url = $parts[1];
		if ($parts[2]) {
			$what .= $parts[2];
		}
	}
}

if (!$url) {
	die("No url found with the alias \"$where\"\n");
} 

open (SYLRC, "<sylpheedrc") 
	|| die("Can't open ~/.sylpheed/sylpheedrc\n");
	@rclines = <SYLRC>;
close SYLRC;

foreach $rcline (@rclines) {
	if ($rcline =~ m/^uri_open_command/) {
		chomp $rcline;
		@browser = split(/=/, $rcline);
		$browser[1] =~ s/%s/$url$what/;
	}
}

if ($ENV{'COMSPEC'}!='') { # windoze
	$browser[1]=~s#\?p#$ENV{'CommonProgramFiles'}\\..#;
	system("start $browser[1]");
} else {
	system("$browser[1]&");
}
exit;
