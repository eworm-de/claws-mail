#!/usr/bin/perl -w

#  * Copyright 2002 Ricardo Mones Lastra <mones@aic.uniovi.es>
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
# outlook2sylpheed.pl -- perl script to convert an Outlook generated 
# 			 contact list into a Sylpheed XML address book.
# 
# This script is based on:
# 	out2syl.sh by Rafael Lossurdo <mugas@via-rs.net>
# 	kmail2sylpheed.pl by Paul Mangan <claws@thewildbeast.co.uk>
#
# See README file for details and usage.
#  

# parse required parameter
die "Required filename missing\nSyntax: $0 fullpathname\n" unless (defined($ARGV[0]));
$outl_file = $ARGV[0];

$sylconf = ".sylpheed";
$indexname = "$sylconf/addrbook--index.xml";

# the next is mostly Paul's code
$time = time;

chdir;
opendir(SYLPHEED, $sylconf) || die("can't open $sylconf directory\n");
	push(@cached,(readdir(SYLPHEED)));
closedir(SYLPHEED);

foreach $cached (@cached) {
	if ($cached =~ m/^addrbook/ && $cached =~ m/[0-9].xml$/) {
		push(@addr, "$cached");
	}
}

@sorted = sort {$a cmp $b} @addr;
$last_one = pop(@sorted);
$last_one =~ s/^addrbook-//;
$last_one =~ s/.xml$//;
$last_one++;
$new_book = "/addrbook-"."$last_one".".xml";

# ok, was enough, do some more bit bashing now
open(OUTL, $outl_file) 
	or die "can't open $outl_file for reading\n";
open(NEWB, '>', "$sylconf/$new_book") 
	or die "can't open $new_book for writting\n";

$_ = <OUTL>; # skip first line
$count = 0;
# header
print NEWB "<?xml version=\"1.0\" encoding=\"US-ASCII\" ?>\n";
print NEWB "<address-book name=\"Outlook Address Book\" >\n"; 
while (<OUTL>) {
	chomp;
	if (/\s+[0-9]+\s+(.+)/) { $_ = $1; } 
	else { $count += 2 and die "wrong format at line $count \n"; }
	@field = split(';',$_); # first is name, second mail addr
	print NEWB "<person uid=\"", $time, "\" first-name=\"\"";
	print NEWB "last-name=\"\" nick-name=\"\" cn=\"", $field[0];
	print NEWB "\" >\n<address-list>\n";
	++$time;
	$field[1] =~ s/\r//; # beware, dangerous chars inside ;)
	print NEWB "<address uid=\"", $time, "\" alias=\"\" email=\"", $field[1];
	print NEWB "\" /> \n</address-list>\n</person>\n";
	++$time;
	++$count;
}
print NEWB "</address-book>\n";

close NEWB;
close OUTL;

# update index (more Paul's code :)

open(INDX, $indexname) 
	or die "can't open $indexname for reading\n";
@index_file = <INDX>;
close INDX;

foreach $index_line (@index_file) {
	if ($index_line =~ m/<\/book_list>/) {
		$new_index .= "    <book name=\"Outlook Address Book\" file=\"$new_book\" />\n"."  </book_list>\n";							} else {
		$new_index .= "$index_line";
	}
}
open (INDX, '>', $indexname)
	or die "can't open $indexname for writting\n";
print INDX "$new_index";
close INDX;

print "Done. $count address(es) converted successfully.\n";

