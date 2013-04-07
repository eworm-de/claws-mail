#!/usr/bin/perl -w
# parameters: <cmd> what perl-code c-code
#
# The purpose of this script is to be able to develop the embedded perl
# code outside of the c-code. This script then updates the c-code, escaping
# the perl code as needed and putting it into the c code as strings.
#
# usage:
#  - go to directory of perl plugin sources
#    - tools/insert_perl.pl perl_filter_action perl_filter_action.pl perl_plugin.c
#    - tools/insert_perl.pl perl_filter_matcher perl_filter_matcher.pl perl_plugin.c
#    - tools/insert_perl.pl perl_persistent perl_persistent.pl perl_plugin.c
#    - tools/insert_perl.pl perl_utils perl_utils.pl perl_plugin.c
use strict;
use File::Copy;

die "Wrong parameters\n" if $#ARGV != 2;

my ($what,$perl_code,$c_code) = @ARGV;

copy($c_code,$c_code.".bak") or die "Copy failed: $!";

open FH,"<",$perl_code or die "Cannot open $perl_code: $!";
my @perl_code = <FH>; close FH;

foreach (@perl_code) {
    s|\\|\\\\|g;
    s|\"|\\\"|g;
    s|(.*)|\"$1\\n\"|;
}

open FH,"<",$c_code or die "Cannot open $c_code: $!";
my @c_code = <FH>; close FH;

my (@c_code_new,$line);

while($line = shift @c_code) {
    if($line =~ /const\s+char\s+$what\s*\[\s*\]\s*=\s*\{/) {
	push @c_code_new,$line;
	push @c_code_new,$_ foreach (@perl_code);
	$line = shift @c_code while(not ($line =~ m/^\s*\}\s*;\s*$/));
	push @c_code_new,$line;
    }
    else {
	push @c_code_new,$line;
    }
}

open FH,">",$c_code or die "Cannot open $c_code: $!";
print FH "$_" foreach (@c_code_new);
