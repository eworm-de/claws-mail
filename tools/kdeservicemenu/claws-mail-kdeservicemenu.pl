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
#  * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

unless ($ARGV[0]) { exit; }

my $claws = "claws-mail --compose --attach";
my $prefix = "/tmp/archive.";
my $command = find_command($ARGV[0]);
my ($sel,$att) = split_parts();

if ($ARGV[0] eq "gz" || $ARGV[0] eq "bz2") {
	exec "$sel$claws $att";
} elsif ($ARGV[0] eq "attachfile") {
	exec "$claws $sel";
} else {
	exec "$command $prefix$ARGV[0] $sel;"
	    ."$claws $prefix$ARGV[0]";
}

exit;

sub find_command {
	local($s) = @_;
	my $com;
	
	if ($s eq "gz") 	{ $com = "gzip -c"; }
	elsif ($s eq "bz2") 	{ $com = "bzip2 -c"; }
	elsif ($s eq "zip") 	{ $com = "$s -r"; }
	elsif ($s eq "tar") 	{ $com = "$s -c -f"; }
	elsif ($s eq "tar.bz2") { $com = "tar -cj -f"; }
	elsif ($s eq "tar.gz") 	{ $com = "tar -cz -f"; }
	
	return $com;
}

sub split_parts {
	my $selectedParts = "";
	my $attachedParts = "";

	for (my $count = $#ARGV; $count > 0; $count--) {
		my @s = split("/", $ARGV[$count]);
		my $p = pop(@s);
		if ($ARGV[0] eq "gz" || $ARGV[0] eq "bz2") {
			my $psub = $p;
			$psub =~ s/\s/_/g;
			my $output = "/tmp/$psub.$ARGV[0]";
			$selectedParts .= "$command \"$p\" > $output;";
			$attachedParts .= "$output ";
		} else {
			$selectedParts .= "\"$p\" ";
		}
	}
	return ($selectedParts,$attachedParts);
}
