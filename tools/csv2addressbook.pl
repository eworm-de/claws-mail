#!/usr/bin/perl -w

use strict;
use Getopt::Long qw(:config pass_through);
use Text::CSV_XS;

#  * This file is free software; you can redistribute it and/or modify it
#  * under the terms of the GNU General Public License as published by
#  * the Free Software Foundation; either version 3 of the License, or
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
#  * Copyright 2007 Paul Mangan <paul@claws-mail.org>
#  *

#
# Import CSV exported address books to Claws Mail
# Supported address books: 
#	Becky >= 2.41
#	Thunderbird >= 2.0.0.6
#

# Becky: full export with titles
# thunderbird: export as 'comma separated'

###
my $quote_char = '"';
my $esc_char = '"';
my $sep_char = ',';
###

my $script = "csv2addressbook.pl";
my $type = '';
my $csvfile = '';
my $bookname = '';
my $iNeedHelp = '';

my $known_types = qr/^(?:becky|thunderbird)$/;

GetOptions("type=s" => \$type,
	   "csv=s"  => \$csvfile,
	   "name=s" => \$bookname,
	   "help"   => \$iNeedHelp);

my @becky_fields = ('Name','E-mail Address', 'Nickname (Input shortcut)',
		    'Web Page','Notes','Company','Department','Job Title',
		    'Job Role','Last Name','First Name','Middle Name',
		    'Birthday','Home Phone','Business Phone','Mobile Phone',
		    'Fax','Street','City','State','Postal Code','Country',
		    'Delivery Label');
my @tbird_fields = ('First Name','Last Name','Display Name','Nickname',
		    'Primary Email','Secondary Email','Work Phone',
		    'Home Phone','Fax Number','Pager Number','Mobile Number',
		    'Home Address','Home Address 2','Home City','Home State',
		    'Home ZipCode','Home Country','Work Address','Work Address 2',
		    'Work City','Work State','Work ZipCode','Work Country',
		    'Job Title','Department','Organization','Web Page 1',
		    'Web Page 2','Birth Year','Birth Month','Birth Day',
		    'Custom 1','Custom 2','Custom 3','Custom 4','Notes','junk');

if (grep m/claws-mail/ => `ps -U $ENV{USER}`) {
	die("You must quit claws-mail before running this script\n");
}

if ($csvfile eq "" || $type eq "" || $type !~ m/$known_types/ || $iNeedHelp) {
	if (!$iNeedHelp) {
		if ($csvfile eq "") {
			print "ERROR: Option csv is missing!\n";
		}
		if ($type eq "") {
			print "ERROR: Option type is missing!\n";
		}
		if ($type && $type !~ m/$known_types/) {
			print "ERROR: \"$type\" is an unknown type!\n";
		}
	}
	print qq~
Usage:
	$script [OPTIONS]
Options:
	--help				Show this screen
	--type=becky|thunderbird	Type of exported address book
	--csv=FILENAME			Full path to CSV file
	--name="My new address book"	Name of new Claws address book (optional)
~;
exit;
}

open(INPUT, "<$csvfile") || die("Can't open the CSV file [$csvfile]\n");
	my @csvlines = <INPUT>;
close INPUT;

my $config_dir = `claws-mail --config-dir` || die("ERROR:
	You don't appear to have Claws Mail installed\n");
chomp $config_dir;

my $claws_version = `claws-mail --version`;
$claws_version =~ s/^Claws Mail version //;

my ($major, $minor) = split(/\./, $claws_version);

my $addr_dir;

if (($major == 3 && $minor >= 1) || $major > 3) {
	$addr_dir = "$config_dir/addrbook";
} else {
	$addr_dir = $config_dir;
}

my $addr_index = "$addr_dir/addrbook--index.xml";
my $csv = Text::CSV_XS->new({binary => 1,
			     quote_char => $quote_char, 
			     escape_char => $esc_char,
			     sep_char => $sep_char});

my $csvtitles = shift(@csvlines);

$csv->parse($csvtitles);
my @csvfields = $csv->fields;

check_fields();

my $new_addrbook = $bookname || get_book_name();

my $xmlobject = write_xml();

chdir;

my @filelist = ();
opendir(ADDR_DIR, $addr_dir) || die("Can't open $addr_dir directory\n");
	push(@filelist, (readdir(ADDR_DIR)));
closedir(ADDR_DIR);

my @files = ();
foreach my $file (@filelist) {
	if ($file =~ m/^addrbook/ && $file =~ m/[0-9].xml$/) {
		push(@files, "$file");
	}
}

my @sorted_files = sort {$a cmp $b} @files;
my $latest_file = pop(@sorted_files);
$latest_file =~ s/^addrbook-//;
$latest_file =~ s/.xml$//;
$latest_file++;
my $new_addrbk = "addrbook-"."$latest_file".".xml";

open (NEWADDR, ">$addr_dir/$new_addrbk");
print NEWADDR $xmlobject;
close NEWADDR;

open (ADDRIN, "<$addr_index") || die("can't open $addr_index for reading");
	my @addrindex_file = <ADDRIN>;
close ADDRIN;

my $rw_addrindex;
foreach my $addrindex_line (@addrindex_file) {
	if ($addrindex_line =~ m/<\/book_list>/) {
		$rw_addrindex .= "    <book name=\"$new_addrbook\" "
			."file=\"$new_addrbk\" />\n  </book_list>\n";
	} else {
		$rw_addrindex .= "$addrindex_line";
	}
}

open (NEWADDRIN, ">$addr_index") || die("Can't open $addr_index for writing");
print NEWADDRIN "$rw_addrindex";
close NEWADDRIN;

print "Done. Address book imported successfully.\n";

exit;

sub get_book_name {
	if ($type eq "becky") {
		return("Becky address book");
	} elsif ($type eq "thunderbird") {
		return("Thunderbird address book");
	}
}

sub check_fields {
	if ($type eq "becky") {
		if ($#csvfields != $#becky_fields) {
			die("ERROR:\n\tNot enough fields!\n"
			   ."\tYou need to do a Full Export With Titles\n");
		}
	} elsif ($type eq "thunderbird") {
		if ($#csvfields != $#tbird_fields) {
			die("ERROR:\n\tNot enough fields!\n"
		    	   ."\tProblem with your exported CSV file\n");
		}
	}
}

sub write_xml {
	my @std_items = get_items();
	my @input_fields = get_fields();

	my $time = time;
	my $xml = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n"
		 ."<address-book name=\"$new_addrbook\" >\n  ";

	my $prev_line;

	foreach my $line (@csvlines) {
		$csv->parse($line);
      		my @fields = $csv->fields;
		# check if an entry contains line breaks
		if ($#fields != $#input_fields) {
			if ($prev_line) {
				my $concat_line = $prev_line.$line;
				$csv->parse($concat_line);
				@fields = $csv->fields;
				if ($#fields != $#input_fields) {
					$concat_line =~ s/\r\n$/ /;
					$concat_line =~ s/\n$/ /;
					$prev_line = $concat_line;
					next;
				}
			} else {
				$line =~ s/\r\n$/ /;
				$line =~ s/\n$/ /;
				$prev_line = $line;
				next;
			}
		}
		$prev_line = '';

		@fields = escape_fields(@fields);

		$xml .= "<person uid=\"$time\" "
		       ."first-name=\"$fields[$std_items[0]]\" "
		       ."last-name=\"$fields[$std_items[1]]\" "
		       ."nick-name=\"$fields[$std_items[2]]\" "
		       ."cn=\"$fields[$std_items[3]]\">\n    ";
		$time++;
		if ($type eq "thunderbird") {
			$xml .= "<address-list>\n      "
			       ."<address uid=\"$time\" alias=\"\" "
			       ."email=\"$fields[$std_items[4]]\" "
			       ."remarks=\"\" />    \n";
			$time++;
			if ($fields[$std_items[5]]) {
				$xml .="      <address uid=\"$time\" alias=\"\" "
				      ."email=\"$fields[$std_items[5]]\" "
				      ."remarks=\"\" />    \n";
			}
			$xml .= "    </address-list>    \n";
		} else {
			$xml .= "<address-list>\n      "
			       ."<address uid=\"$time\" alias=\"\" "
			       ."email=\"$fields[$std_items[4]]\" "
			       ."remarks=\"$fields[$std_items[5]]\" />    \n"
			       ."</address-list>    \n";
		}
		$xml .= "<attribute-list>\n";
		foreach my $item (@std_items) {
			delete($fields[$item]);
		}
		foreach my $field (0 .. $#fields) {
			if ($fields[$field]) { 
				$time++;
				$xml .= "      <attribute uid=\"$time\" "
			    	       ."name=\"$input_fields[$field]\">"
				       ."$fields[$field]</attribute>\n";
			}
		}
		$xml .= "    </attribute-list>\n  "
		       ."</person>\n";
		$time++;
	}

	$xml .= "</address-book>\n";

	return $xml;
}

sub get_items {
	if ($type eq "becky") {
		return ('10','9','2','0','1','4');
	} elsif ($type eq "thunderbird") {
		return ('0','1','3','2','4','5','38');
	}
}

sub get_fields {
	if ($type eq "becky") {
		return(@becky_fields);
	} elsif ($type eq "thunderbird") {
		return(@tbird_fields);
	}
}

sub escape_fields {
	my (@fields) = @_;

	for (my $item = 0; $item <= $#fields; $item++) {
		$fields[$item] =~ s/^"//;
		$fields[$item] =~ s/"$//;
		$fields[$item] =~ s/"/&quot;/g;
		$fields[$item] =~ s/&/&amp;/g;
		$fields[$item] =~ s/'/&apos;/g;
		$fields[$item] =~ s/</&lt;/g;
		$fields[$item] =~ s/>/&gt;/g;
	}
	
	return @fields;
}

