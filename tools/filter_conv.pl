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
#  *

chdir;
chdir '.sylpheed' || die("You don't appear to have Sylpheed installed\n");

open(FOLDERLIST, "<folderlist.xml") || die("Can't find folderlist.xml\n");
@folderlist = <FOLDERLIST>;
close FOLDERLIST;

foreach $folderlist (@folderlist) {
	if ($folderlist =~ m/<folder type="mh"/) {
      		$folderlist =~ s/<folder type="mh" name="//;
                $folderlist =~ s/" path="[A-Z0-9]+">\n//ig;
                $folderlist =~ s/^ +//;
                $mailbox = $folderlist;
        }
}

open (FILTERRC, "<filterrc") || die("Can't find your old filter rules\n");
@filterrc = <FILTERRC>;
close FILTERRC;

if (!@filterrc) {
	print "\nYou don't have any filter rules\n\n";
	exit;
}

$WRITE_THIS = "";
$COUNT      = "0";

if (-e "matcherrc") {
	open (MATCHER, "<matcherrc") || die("Can't open matcherrc\n");
	@matcherrc = <MATCHER>;
	close MATCHER;
		foreach $matcherrc (@matcherrc) {
			$WRITE_THIS .= $matcherrc;
		}
	$WRITE_THIS .= "\n";
}

$WRITE_THIS .= "[global]\n";

foreach $filterrc (@filterrc) {
	$COUNT++;
	@split_lines = split("\t", $filterrc);
	if ($split_lines[2] eq "&") {
		$operator = "&";
		&sort_data;
	}
	elsif ($split_lines[2] eq "|") {
		$operator = "|";
		&sort_data;
	} elsif ($split_lines[0] eq "To") {
		$WRITE_THIS .= "to matchcase \"$split_lines[1]\"";
	} elsif ($split_lines[0] eq "Reply-To") {
		$WRITE_THIS .= "inreplyto matchcase \"$split_lines[1]\"";
	} elsif ($split_lines[0] eq "Subject") {
		$WRITE_THIS .= "subject matchcase \"$split_lines[1]\"";
	} elsif (($split_lines[0] eq "From") || ($split_lines[0] eq "Sender")){
		$WRITE_THIS .= "from matchcase \"$split_lines[1]\"";
	}
	if (!$split_lines[5]) {
		$WRITE_THIS .= " delete";
	} elsif ($split_lines[8] eq "m\n"){
		$WRITE_THIS .= " move \"\#mh/$mailbox/$split_lines[5]\"";
	}
	$WRITE_THIS .= "\n";
	@split_lines = "";
}

open (MATCHERRC, ">matcherrc");
print MATCHERRC $WRITE_THIS;
close MATCHERRC;

rename ("filterrc","filterrc.old");

print "\nYou have sucessfully converted $COUNT filtering rules\n\n";
print "'filterrc' has been renamed 'filterrc.old'\n\n";
exit;

sub sort_data {
	if ($split_lines[0] eq "To" && $split_lines[3] eq "Cc" && 
	$split_lines[1] eq $split_lines[4]) {
		$WRITE_THIS .= "to_or_cc matchcase \"$split_lines[1]\"";		
	}
	elsif ($split_lines[0] eq "To") {
		$WRITE_THIS .= "to matchcase \"$split_lines[1]\" $operator ";
	} elsif ($split_lines[0] eq "Reply-To") {
		$WRITE_THIS .= "inreplyto matchcase \"$split_lines[1]\" $operator ";
	} elsif ($split_lines[0] eq "Subject") {
		$WRITE_THIS .= "subject matchcase \"$split_lines[1]\" $operator ";
	} elsif (($split_lines[0] eq "From") || ($split_lines[0] eq "Sender")) {
		$WRITE_THIS .= "from matchcase \"$split_lines[1]\" $operator ";
	}
	if ($split_lines[3] eq "To") {
		$WRITE_THIS .= "to matchcase \"$split_lines[4]\"";
	} elsif ($split_lines[3] eq "Reply-To") {
		$WRITE_THIS .= "inreplyto matchcase \"$split_lines[4]\"";
	} elsif ($split_lines[3] eq "Subject") {
		$WRITE_THIS .= "subject matchcase \"$split_lines[4]\"";
	} elsif (($split_lines[3] eq "From") || ($split_lines[3] eq "Sender")) {
		$WRITE_THIS .= "from matchcase \"$split_lines[4]\"";
	}
}
