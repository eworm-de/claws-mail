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

my $sylpheed = "sylpheed --compose --attach";
my $prefix = "/tmp/archive.";
my ($suffix,$command) = find_sufncom($ARGV[0]);
my ($sel,$att) = split_parts();

if ($ARGV[0] eq "gzip" || $ARGV[0] eq "bzip2") {
	exec "$sel$sylpheed $att";
} elsif ($ARGV[0] eq "attachfile") {
	exec "$sylpheed $sel";
} else {
	exec "$command $prefix$suffix $sel;"
	    ."$sylpheed $prefix$suffix";
}

exit;

sub find_sufncom {
	local($s) = @_;
	my ($suf,$com);
	
	if ($s eq "gzip") { $suf = "gz"; $com = "$s -c"; }
	elsif ($s eq "bzip2") { $suf = "bz2"; $com = "$s -c"; }
	elsif ($s eq "zip") { $suf = "zip"; $com = "$s -r"; }
	elsif ($s eq "tar") { $suf = "tar"; $com = "$s -c -f"; }
	elsif ($s eq "tarbzip2") { $suf = "tar.bz2"; $com = "tar -cj -f"; }
	elsif ($s eq "targz") { $suf = "tar.gz"; $com = "tar -cz -f"; }
	
	return ($suf,$com);
}

sub split_parts {
	my $selectedParts = "";
	my $attachedParts = "";

	for (my $count = $#ARGV; $count > 0; $count--) {
		my @s = split("/", $ARGV[$count]);
		my $p = pop(@s);
		if ($ARGV[0] eq "gzip" || $ARGV[0] eq "bzip2") {
			my $psub = substitute($p);
			my $output = "/tmp/$psub.$suffix";
			$selectedParts .= "$command \"$p\" > $output;";
			$attachedParts .= "$output ";
		} else {
			$selectedParts .= "\"$p\" ";
		}
	}
	return ($selectedParts,$attachedParts);
}

sub substitute {
	local($s) = @_;
	$s =~ s/\s/_/g;
	return $s;
}
