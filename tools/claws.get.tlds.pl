#!/usr/bin/perl -w
=pod
=head1

claws.get.tlds.pl - IANA TLDs online list to stdout as gchar* array.

Copyright (c) 2015 Ricardo Mones <ricardo@mones.org>

This program is free software: you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation, either version 3 of the License, or (at your
option) any later version.

This program is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program.  If not, see <http://www.gnu.org/licenses/>.

=cut
use 5.012;
use utf8;
use LWP::Simple;
use constant {
  URL => "https://data.iana.org/TLD/tlds-alpha-by-domain.txt"
};

print "/*\n * This is a generated file.\n * See tools/claws.get.tlds.pl\n */\n";
print "#ifndef __TLDS_H__\n#define __TLDS_H__\n\n";
print "static const gchar *toplvl_domains [] = {\n\t"; # open array

my $payload = get URL;
my @lines = split /^/, $payload;
my ($i, $j) = (0, 0);

foreach (@lines) {
  ++$i;
  chomp;
  if (/^#(.*)$/) { # comments
    my $c = $1; $c =~ s/^\s+|\s+$//g;
    print "/* $c */\n\t";
    next;
  }
  next if (/^XN--.*$/); # IDNs not supported yet, see bug #1670
  my $tld = lc $_; # list comes in upper case
  print "\"$tld\""; ++$j;
  print "," unless $i >= scalar @lines;
  print "" . ($j % 5 == 0 or $i >= scalar @lines)? "\n": " ";
  print "\t" if ($j % 5 == 0 and $i < scalar @lines);
}

print "};\n\n"; # close array
print "#endif\n";
