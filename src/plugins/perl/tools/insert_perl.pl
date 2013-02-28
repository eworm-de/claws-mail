#!/usr/bin/perl -w
# parameters: <cmd> what perl-code c-code
#
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
