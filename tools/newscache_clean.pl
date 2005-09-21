#!/usr/bin/perl

#  * Copyright 2001 Paul Mangan <claws@thewildbeast.co.uk>
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
#  *

use File::Path;

chdir;
chdir '.sylpheed' || die("You don't appear to have Sylpheed installed\n");

open(ACCOUNTRC, "<accountrc") || die("Can't find accountrc\n");
	@accountrc = <ACCOUNTRC>;
close ACCOUNTRC;

foreach $accountrc (@accountrc) {
	if ($accountrc =~ m/nntp_server=[A-Za-z0-9]/) {
        	$accountrc =~ s/nntp_server=//;
                chomp $accountrc;
                push(@newsserver, "$accountrc");	
        }
}

%seen = ();
foreach $newsserver (@newsserver) {
        $seen{$newsserver}++;
}

opendir(NEWSCACHE, "newscache") || die("Can't open newscache\n");
	push(@cached,(readdir(NEWSCACHE)));
closedir(NEWSCACHE);
splice(@cached, 0, 2);
foreach $cached (@cached) { ## remove old newsserver directory tree
        rmtree("./newscache/$cached") unless $seen{$cached};
}

open(FOLDERLIST, "<folderlist.xml") || die("Can't find folderlist.xml\n");
	@folderlist = <FOLDERLIST>;
close FOLDERLIST;
%saw = ();
$wegotnews = 0;
foreach $folderlist (@folderlist) { ## remove old newsgroups directory trees
	unless ($wegotnews) {
        	if ($folderlist =~ m/<folder type="news"/) {
        		$wegotnews = 1;
                        $done_grp = 0;
                        $nntpserver = shift(@newsserver);
        	}
        } 
        if ($wegotnews && $folderlist =~ m/<\/folder>\n/ && !$done_grp) {
         	opendir(NEWSGCACHE, "newscache/$nntpserver") || die("Can't open newscache/$nntpserver\n");
		push(@cachedgrp,(readdir(NEWSGCACHE)));
		closedir(NEWSGCACHE);
                splice(@cachedgrp, 0, 2);
		foreach $cachedgrp (@cachedgrp) { 
                 	rmtree("./newscache/$nntpserver/$cachedgrp") unless ($saw{$cachedgrp}) || ($cachedgrp eq ".newsgroup_list");
		}
             	$done_grp = 1;
                $wegotnews = 0;
                @cachedgrp = ();
        }
	if ($wegotnews && $folderlist !~ m/<\/folder>\n/) {
                if ($folderlist =~ m/<folderitem type="normal"/) {
                	$folderlist =~ s/<folderitem type="normal" name="[A-Z0-9.]+" path="//i;
               		$folderlist =~ s/" threaded="[0-1]+//;
			$folderlist =~ s/" hidereadmsgs="[0-1]+//;
			$folderlist =~ s/" mtime="[0-9]+" new="[0-9]+" unread="[0-9]+" total="[0-9]+" \/>//;
                        $folderlist =~ s/ +//;
                        chomp $folderlist;
                        $saw{$folderlist}++;
                }        
        }
}
print "Finished cleaning Sylpheed's newscache\n";
exit;
