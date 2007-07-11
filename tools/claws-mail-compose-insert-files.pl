#!/usr/bin/perl -w

use strict;

use Getopt::Long;
use URI::Escape;

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

# This script enables inserting files into the message body of a new Claws Mail
# Compose window from the command line. Additionally To, Cc, Subject and files
# to attach to the message can be specified 

my (@inserts,@attachments,@lines,@output) = ();
my $body = "";
my $attach_list = "";
my $to = "";
my $cc = "";
my $subject = "";
my $help = "";

GetOptions("to=s"      => \$to,
	   "cc=s"      => \$cc,
	   "subject=s" => \$subject,
	   "attach=s"  => \@attachments,
	   "insert=s"  => \@inserts,
	   "help|h"    => \$help);

if ($help) {
	help_me();
}

@attachments = split(/,/, join(',', @attachments));
@inserts = split(/,/, join(',', @inserts));

foreach my $attach (@attachments) {
	$attach_list .= "$attach ";
}

foreach my $insert (@inserts) {
	open(FILE, "<$insert") || die("can't open file\n");
		@lines = <FILE>;
		push(@output, @lines);
	close FILE;
}

foreach my $line (@output) {
	$body .= "$line";
}

$body = uri_escape($body);

system("claws-mail --compose \"mailto:$to?subject=$subject&cc=$cc&body=$body\" --attach $attach_list");

exit;

sub help_me {
	print<<'EOH';
Usage:	
	claws-mail-compose-insert-files.pl [options]
Options:
	--help -h
	--to mail@address.net[,mail2@address.net]
	--cc mail@address.net[,mail2@address.net]
	--subject "My subject"
	--attach FILE
	--insert FILE

--attach and --insert can be used multiple times

EOH
exit;
}
