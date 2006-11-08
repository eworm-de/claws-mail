#!/usr/bin/perl

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
#  * Copyright © 2003 Paul Mangan <claws@thewildbeast.co.uk>
#  *
#  * 2003-10-01: add --debug and --dry-run options
#  * 2003-09-30: updated/improved by Matthias Förste <itsjustme@users.sourceforge.net>
#  * 2003-05-27: version one

## script name : maildir2sylpheed.pl

## script purpose : convert a Kmail mailbox into a Claws Mail mailbox

## USAGE: maildir2claws-mail.pl --kmaildir=Mail

## tested with Kmail version 1.5.2

use strict;

use Getopt::Long;
use File::Find;

my $kmaildir  = '';
my $iNeedHelp = '';
# dont actually change anything if set(useful in conjunction with debug)
my $PRETEND = '';
# print debug info if set
my $DEBUG = '';

my $claws_tmpdir = "$ENV{HOME}/claws_tmp";
my $kmail_olddir = "$ENV{HOME}/kmail_junk";

GetOptions("kmaildir=s" => \$kmaildir,
	   "help"	=> \$iNeedHelp,
	   "dry-run"	=> \$PRETEND,
	   "debug"	=> \$DEBUG);

if ($kmaildir eq "" || $iNeedHelp) {
	if (!$iNeedHelp) {
		print "No directory name given\n";
	}
	print "Use the following format:\n";
	print "\tmaildir2claws-mail.pl --kmaildir=mail_folder_name\n\n";
	print "For example: 'Mail'\n";
	exit;
}

$kmaildir = "$ENV{PWD}/$kmaildir" unless '/' eq substr($kmaildir,0,1);

my $count = 1;
my $MAIL_dir = "$kmaildir";

my $find_opts = { wanted => \&process };

if (-d $MAIL_dir) {
	find($find_opts , ($MAIL_dir));
} else {
	print "\n$MAIL_dir is not a directory !\n";
	exit;
}

unless ($PRETEND) {
	mkdir("$claws_tmpdir", 0755);
	system("mv $kmaildir $kmail_olddir");
	system("mv $claws_tmpdir $ENV{HOME}/Mail");

	print "\n\nSucessfully converted mailbox \"$MAIL_dir\"\n";
	print "Start claws-mail and right-click \"Mailbox (MH)\" and ";
	print "select \"Rebuild folder tree\"\n";
	print "You may also need to run \"/File/Folder/Check for ";
	print "new messages in all folders\"\n\n";
	print "Your kmail directories have been backed-up to\n";
	print "$kmail_olddir\n\n";
}

print "\n";
exit;

sub process() {
  	if (-d) {
		process_dir($File::Find::dir);
	} else {
		process_file($File::Find::name);
	}
}

sub process_dir() {
	my $direc = shift();
  	$DEBUG && print "\nDIR $direc";

	if ($direc !~ m/^drafts$/ &&
	    $direc !~ m/^outbox$/ &&
      	    $direc !~ m/^trash$/  && 
    	    $direc !~ m/^inbox$/) {
		my $tmpdir = $direc;
		$tmpdir =~ s/^$MAIL_dir//;
		$tmpdir =~ s/sent-mail/sent/;
		$tmpdir =~ s/\/cur$//;
		$tmpdir =~ s/\/new$//;
		$tmpdir =~ s/^\///;
		$tmpdir =~ s/\.directory//g;
		$tmpdir =~ s/\.//g;
		
		my $newdir = "$claws_tmpdir/$tmpdir";
		$DEBUG && print qq{\n>>> -e "$newdir" || mkdir("$newdir")};
		$PRETEND || -e "$newdir" || mkdir("$newdir");
	}

}

sub process_file {
	my $file = shift;
  	$DEBUG && print "\nFILE $file";

  	my $nfile;
  	my $tmpfile = $file;

  	if ($tmpfile =~ m/\/cur\// || 
	    $tmpfile =~ m/\/new\//) {

    		$tmpfile =~ s/\/new//;
    		$tmpfile =~ s/\/cur//;

    		my @spl_str = split("/", $tmpfile);
    		pop(@spl_str);
    		push(@spl_str, "$count");

    		foreach my $spl_str (@spl_str) {
			$spl_str =~ s/^\.//;
			$spl_str =~ s/\.directory$//;
			$spl_str =~ s/sent-mail/sent/;
		}

    		$nfile = join("/", @spl_str);
    		$nfile =~ s|$kmaildir|$claws_tmpdir/|;
	}

	if (-e "$file" && $nfile ne "") {
    		$DEBUG && print qq{\n+++ cp "$file" "$nfile"};
    		$PRETEND || system("cp \"$file\" \"$nfile\"");
    		$count++;
  	}

}
