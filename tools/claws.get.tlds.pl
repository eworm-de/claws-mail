#!/usr/bin/perl -w
=pod
=head1

claws.get.tlds.pl - IANA TLDs online list to stdout as gchar* array.

Syntax:
  claws.get.tlds.pl [extra-domains.txt] > src/common/tlds.h

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
die "Unable to retrieve IANA list of TLDs\n" unless defined $payload;
my @lines = map { chomp; $_ } split /^/, $payload;
my ($i, $j) = (0, 0);

if (defined $ARGV[0] and -f $ARGV[0]) {
  my %domains = ();
  foreach (@lines) { $domains{$_} = "" unless (/^#.*$/) }
  open my $fh, '<', $ARGV[0] or die "Unable to open $ARGV[0] for reading\n";
  while (<$fh>) {
    chomp;
    push @lines, $_ if (/^#.*/ or not defined $domains{$_});
  }
  close $fh;
}

foreach (@lines) {
  ++$i;
  if (/^#(.*)$/) { # comments
    my $c = $1; $c =~ s/^\s+|\s+$//g;
    print "/* $c */\n\t";
    next;
  }
  next if (/^XN--.*$/); # IDNs not supported yet, see bug #1670
  my $tld = lc $_; # list comes in upper case
  print "\"$tld\""; ++$j;
  print ",\n\t" unless $i >= scalar @lines;
  print "\n" if $i >= scalar @lines;
}

print "};\n\n"; # close array
print "#endif\n";
