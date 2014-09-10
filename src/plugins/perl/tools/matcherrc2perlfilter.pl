#!/usr/bin/perl -w
#
## script purpose : convert matcherrc filtering rules into
##                  perl_filter rules
#
# This conversion-tool doesn't produce nice Perl code and is just
# intended to get you started. If you choose to use the Perl plugin,
# consider rewriting your rules.
#
# Copyright (C) 2004-2014 Holger Berndt
#
#
# This file is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.

use strict;

our $warnings = 0;
our $lines    = 0;
our $tokens   = 0;

my $home_dir   = $ENV{"HOME"}; $home_dir ||= ".";
my $sylph_dir  = `claws-mail --config-dir`;
my $matcherrc  = "matcherrc";
my $perlfilter = "perl_filter";
my $dirsep     = "/";

chomp($sylph_dir); $sylph_dir =~ s/.*\n(.*)$/$1/;
my $inpath  = $home_dir.$dirsep.$sylph_dir.$dirsep.$matcherrc;
my $outpath = $home_dir.$dirsep.$sylph_dir.$dirsep.$perlfilter;
open IN,      $inpath  or die "Cannot open $inpath: $!";
open OUT,">>",$outpath or die "Cannot open $outpath: $!";

print "Filtering rules are read from `$inpath', converted to Perl\n";
print "syntax and appended to `$outpath'\n";
print "`$inpath' is not changed, so you might want to make a backup\n";
print "copy of it and then remove your former filtering rules\n";
print "---\n";
my $date = `date`;
chomp($date);
print OUT "### Begin: Rules converted by matcherrc2perlfilter.pl $date ###\n";
while(my $line = <IN>) {
    $line =~ s/^\s*(.*)\s*$/$1/;
    if($line =~ /^\[filtering\]$/i) {
	while($line = <IN>) {
	    $line =~ s/^\s*(.*)\s*$/$1/;
	    next if $line =~ /^$/;
	    if($line =~ /^\[(.+)\]$/) {
		last unless ($1 =~ /filtering/i);
	    }
	    my @fields = splitline($line);
	    $lines++;
	    convert(@fields);
	}
    }
}
print "---\n" if $warnings;
print "Finished conversion of $lines rules with $warnings warnings.\n";
print OUT "### End: Rules converted by matcherrc2perlfilter.pl $date ###\n";

# convert a rule
sub convert {
    my $act = 0;
    my $output="(";
    while(my $token = shift) {
	$tokens++;
	if($token eq "&") {
	    $token = shift;
	}
	elsif($token eq "|") {
	    $output =~ s/&& $/\|\| /;
	    $token = shift;
	}
	elsif($tokens != 1 and $act == 0) {
	    $act = 1;
	    if($output =~ / (&&|\|\|) $/) {
		$output =~ s/ (&&|\|\|) $/\) $1 /;
	    }
	    else {
		$output .= ")";
	    }
	}

	if($token eq "~") {
	    $output .= "!";
	    $token = shift;
	}

	if($token eq "all"           or
	   $token eq "marked"        or
	   $token eq "deleted"       or
	   $token eq "replied"       or
	   $token eq "forwarded"     or
	   $token eq "locked"        or
	   $token eq "unread"        or
	   $token eq "new"           or
	   $token eq "partial"       or
	   $token eq "ignore_thread" or
	   $token eq "mark"          or
	   $token eq "unmark"        or
	   $token eq "lock"          or
	   $token eq "unlock"        or
	   $token eq "stop"          or
	   $token eq "hide"          or
	   $token eq "mark_as_read"  or
	   $token eq "mark_as_unread") {
	    $output .= qq|($token) && |;
	}
	elsif($token eq "delete") {
	    $output .= qq|(dele) && |;
	}
	elsif($token eq "subject"       or
	      $token eq "from"          or
	      $token eq "to"            or
	      $token eq "cc"            or
	      $token eq "to_or_cc"      or
	      $token eq "newsgroups"    or
	      $token eq "inreplyto"     or
	      $token eq "references"    or
	      $token eq "headers_part"  or
	      $token eq "headers_cont"  or
	      $token eq "body_part"     or
	      $token eq "message") {
	    my $match = shift;
	    my $what  = shift;
	    $what =~ s/\\"/"/g;$what =~ s/'/\\'/g;
	    $what =~ s/^"(.*)"$/'$1'/;
	    $output .= qq|($match($token,$what)) && |;
	}
	elsif($token eq "age_greater"   or
	      $token eq "age_lower"     or
	      $token eq "colorlabel"    or
	      $token eq "score_greater" or
	      $token eq "score_lower"   or
	      $token eq "score_equal"   or
	      $token eq "size_greater"  or
	      $token eq "size_smaller"  or
	      $token eq "size_equal"    or
	      $token eq "move"          or
	      $token eq "copy"          or
	      $token eq "execute"       or
	      $token eq "color"         or
	      $token eq "test"          or
	      $token eq "change_score"  or
	      $token eq "set_score") {
	    my $arg = shift;
	    $arg =~ s/\\"/"/g;$arg =~ s/'/\\'/g;
	    $arg =~ s/^"(.*)"$/'$1'/;
	    $output .= qq|($token($arg)) && |;
	}
	elsif($token eq "header") {
	    my $headername = shift;
	    $headername =~ s/\\"/"/g;$headername =~ s/'/\\'/g;
	    $headername =~ s/^"(.*)"$/'$1'/;
	    my $match = shift;
	    my $what = shift;
	    $what =~ s/\\"/"/g;$what =~ s/'/\\'/g;	    
	    $what =~ s/^"(.*)"$/'$1'/;
	    $output .= qq|($match($headername,$what)) && |;
	}
	elsif($token eq "stop") {
	    $output .= qq|(return) && |;
	}
	else {
	    print STDERR "WARNING: unknown token in $inpath ignored: $token\n";
	    $warnings++;
	}
    }
    $output =~ s| && $|;\n|;
    print OUT $output;
    $tokens = 0;
}

# split the input line
sub splitline {
    my @fields;
    my $line = shift;
    while($line) {
	$line =~ s/^\s+//;
	if($line =~ m#^"#) {
	   $line =~ s#^(".*?[^\\]")##;
	   push @fields,$1;
        }
	elsif($line =~ /^~/) {
	    $line =~ s#^(~)##;
	    push @fields,$1;
	}
	else {
	    $line =~ s#^(\S+)##;
	    push @fields,$1;
	}
    }
    return @fields;
}
