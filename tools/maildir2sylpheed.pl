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

## script name : maildir2sylpheed.pl

## script purpose : convert a Kmail mailbox into a Sylpheed mailbox

## USAGE: maildir2sylpheed.pl --kmaildir=Mail

## tested with Kmail 1.5.2

use Getopt::Long;
use File::Recurse;

$kmaildir  = '';
$iNeedHelp = '';

$sylpheed_tmpdir = "sylpheed_tmp";
$kmail_olddir    = "kmail_junk";

GetOptions("kmaildir=s" => \$kmaildir,
	   "help"	 => \$iNeedHelp);

if ($kmaildir eq "" || $iNeedHelp) {
	if (!$iNeedHelp) {
		print "No directory name given\n";
	}
	print "Use the following format:\n";
	print "\tmaildir2sylpheed.pl --kmaildir=mail_folder_name\n\n";
	print "For example: 'Mail'\n";
	exit;
}

chdir($ENV{HOME});

$MAIL_dir = "$ENV{HOME}/$kmaildir";

mkdir("$sylpheed_tmpdir", 0755);

my %files = Recurse(["$MAIL_dir"], {});

foreach (keys %files) { 
	$dir = $_;
	push(@dirs, "$_");
	foreach (@{ $files{$_} }) { 
		push(@files, "$dir/$_");
	}
}

foreach $direc (@dirs) {
	if ($direc !~ m/^drafts$/
	    && $direc !~ m/^outbox$/
	    && $direc !~ m/^trash$/
	    && $direc !~ m/^inbox$/) {
	    	$tmpdir = $direc;
		$tmpdir =~ s/^$MAIL_dir//;
		$tmpdir =~ s/sent-mail/sent/;
		$tmpdir =~ s/\/cur$//;
		$tmpdir =~ s/\/new$//;
		$tmpdir =~ s/^\///;
		$tmpdir =~ s/\.directory//g;
		$tmpdir =~ s/\.//g;
		mkdir("$sylpheed_tmpdir/$tmpdir");
		opendir(DIR, "$direc")
			|| die("Can't open directory");
		push(@subdirs,(readdir(DIR)));
		closedir DIR;
	}

	foreach $subdir (@subdirs) {
		if ($subdir !~ m/\.directory$/
		    && $subdir!~ m/^\.*$/
	    	    && $subdir !~ m/cur\/$/
	    	    && $subdir !~ m/new\/$/
	    	    && $subdir !~ m/tmp\/$/) {
			$sub_dir =~ s/\.directory//;
			unless (-e "$sylpheed_tmpdir/$tmpdir/$sub_dir") {
				mkdir("$sylpheed_tmpdir/$tmpdir/$sub_dir");
			}
		}		
	}
}

$count = 1;
foreach $file (@files) {
	$tmpfile = $file;
	if ($tmpfile =~ m/\/cur\//
	    || $tmpfile =~ m/\/new\//) {
		$tmpfile =~ s/\/new//;
		$tmpfile =~ s/\/cur//;
		@spl_str = split("/", $tmpfile);
		pop(@spl_str);
		push(@spl_str, "$count");
		foreach $spl_str (@spl_str) {
			$spl_str =~ s/^\.//;
			$spl_str =~ s/\.directory$//;
			$spl_str =~ s/sent-mail/sent/;
		}
		$nfile = join("/", @spl_str);
		$nfile =~ s/\/$kmaildir\//\/$sylpheed_tmpdir\//;
	}

	if (-e "$file" && $nfile ne "") {
		system("cp \"$file\" \"$nfile\"");
		$count++;
	}	
}

system("mv $kmaildir $kmail_olddir");
system("mv $sylpheed_tmpdir $ENV{HOME}/Mail");

print "Sucessfully converted mailbox \"$MAIL_dir\"\n";
print "Start Sylpheed and right-click \"Mailbox (MH)\" and ";
print "select \"Rebuild folder tree\"\n";
print "You may also need to run \"/File/Folder/Check for "
print "new messages in all folders\"\n\n";
print "Your kmail directories have been backed-up to\n";
print "$ENV{HOME}/$kmail_olddir\n\n";

exit;
