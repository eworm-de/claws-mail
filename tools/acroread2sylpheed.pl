#!/usr/bin/perl

#  * Copyright 2005 Paul Mangan <claws@thewildbeast.co.uk>
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
# acroread2sylpheed.pl	helper script to send documents from
#				Adobe Reader 7 to sylpheed

use strict;

my $input = <>;

my $option;
my $pdf;
my $output = $ARGV;

if ($ARGV =~ m/^\//) {
	$option = "KMAIL or MUTT";
} elsif ($ARGV =~ m/^mailto/) {
	$option = "EVOLUTION";
} elsif ($ARGV =~ m/^to/) {
	$option = "MOZILLA or NETSCAPE";
} else {
	$option = "KMAIL";
}

if ($option eq "MOZILLA or NETSCAPE") {
	my @parts = split(/,/, $output);
	$parts[3] =~ s/^attachment=file:\/\///;
	$pdf = $parts[3];
} elsif ($option eq "EVOLUTION") {
	my @parts = split(/&[a-z]*=/, $output);
	$parts[0] =~ s/^mailto:\?attach=//;
	$pdf = $parts[0];
} elsif ($option eq "KMAIL or MUTT") {
	$pdf = $output;
} elsif ($option eq "KMAIL") {
	$pdf = $ENV{HOME}."/".$output;
}

exec "sylpheed --attach \"$pdf\"";
## if necessary, change the line above to point to
## your sylpheed executable

exit;