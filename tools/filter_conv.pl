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
	if ($split_lines[6] eq "0") {
		$match_type = "matchcase";
		$predicate_one = "~";
	} elsif ($split_lines[6] eq "1") {
		$match_type = "matchcase";
		$predicate_one = "";
	} elsif ($split_lines[6] eq "4") {
		$match_type = "regexpcase";
		$predicate_one = "~";
	} elsif ($split_lines[6] eq "5") {
		$match_type =  "regexpcase";
		$predicate_one = "";
	}
	if ($split_lines[7] eq "0") {
		$match_type = "matchcase";
		$predicate_two = "~";
	} elsif ($split_lines[7] eq "1") {
		$match_type = "matchcase";
		$predicate_two = "";
	} elsif ($split_lines[7] eq "4") {
		$match_type = "regexpcase";
		$predicate_two = "~";
	} elsif ($split_lines[7] eq "5") {
		$match_type =  "regexpcase";
		$predicate_two = "";
	}

	if ($split_lines[2] eq "&") {
		$operator = "&";
		&sort_data;
	}
	elsif ($split_lines[2] eq "|") {
		$operator = "|";
		&sort_data;
	} elsif ($split_lines[0] eq "To") {
		$WRITE_THIS .= "$predicate_one"."to $match_type \"$split_lines[1]\"";
	} elsif ($split_lines[0] eq "Reply-To") {
		$WRITE_THIS .= "$predicate_one"."inreplyto $match_type \"$split_lines[1]\"";
	} elsif ($split_lines[0] eq "Subject") {
		$WRITE_THIS .= "$predicate_one"."subject $match_type \"$split_lines[1]\"";
	} elsif ($split_lines[0] eq "From"){
		$WRITE_THIS .= "$predicate_one"."from $match_type \"$split_lines[1]\"";
	} elsif ($split_lines[0] eq "Sender" || $split_lines[0] eq "List-Id" || 
		 $split_lines[0] eq "X-ML-Name" || $split_lines[0] eq "X-List" ||
		 $split_lines[0] eq "X-Sequence" || $split_lines[0] eq "X-Mailer" ||
		 $split_lines[0] eq "Cc") {
		$WRITE_THIS .= "$predicate_one"."header $split_lines[0] $match_type \"$split_lines[1]\"";
	}
	if ($split_lines[8] eq "n\n") {
		$WRITE_THIS .= " delete";
	} elsif ($split_lines[8] eq "m\n"){
		$WRITE_THIS .= " move \"$split_lines[5]\"";
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
		$WRITE_THIS .= "$predicate_one"."to_or_cc $match_type \"$split_lines[1]\"";		
	}
	elsif ($split_lines[0] eq "To") {
		$WRITE_THIS .= "$predicate_one"."to $match_type \"$split_lines[1]\" $operator ";
	} elsif ($split_lines[0] eq "Reply-To") {
		$WRITE_THIS .= "$predicate_one"."inreplyto $match_type \"$split_lines[1]\" $operator ";
	} elsif ($split_lines[0] eq "Subject") {
		$WRITE_THIS .= "$predicate_one"."subject $match_type \"$split_lines[1]\" $operator ";
	} elsif ($split_lines[0] eq "From") {
		$WRITE_THIS .= "$predicate_one"."from $match_type \"$split_lines[1]\" $operator ";
	} elsif ($split_lines[0] eq "Sender" || $split_lines[0] eq "List-Id" || 
		 $split_lines[0] eq "X-ML-Name" || $split_lines[0] eq "X-List" ||
		 $split_lines[0] eq "X-Sequence" || $split_lines[0] eq "X-Mailer" ||
		 $split_lines[0] eq "Cc") {
		$WRITE_THIS .= "$predicate_one"."header $split_lines[0] $match_type \"$split_lines[1]\" $operator ";
	}

	if ($split_lines[3] eq "To") {
		$WRITE_THIS .= "$predicate_two"."to $match_type \"$split_lines[4]\"";
	} elsif ($split_lines[3] eq "Reply-To") {
		$WRITE_THIS .= "$predicate_two"."inreplyto $match_type \"$split_lines[4]\"";
	} elsif ($split_lines[3] eq "Subject") {
		$WRITE_THIS .= "$predicate_two"."subject $match_type \"$split_lines[4]\"";
	} elsif ($split_lines[3] eq "From") {
		$WRITE_THIS .= "$predicate_two"."from $match_type \"$split_lines[4]\"";
	} elsif ($split_lines[3] eq "Sender" || $split_lines[3] eq "List-Id" || 
		 $split_lines[3] eq "X-ML-Name" || $split_lines[3] eq "X-List" ||
		 $split_lines[3] eq "X-Sequence" || $split_lines[3] eq "X-Mailer" ||
		 $split_lines[3] eq "Cc") {
		$WRITE_THIS .= "$predicate_two"."header $split_lines[3] $match_type \"$split_lines[4]\"";
	} 

}
