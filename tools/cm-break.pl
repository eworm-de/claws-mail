#!/usr/bin/perl

use 5.14.1;
use warnings;

our $VERSION = "1.05 - 2018-10-08";
our $cmd = $0 =~ s{.*/}{}r;

sub usage {
    my $err = shift and select STDERR;
    say "usage: $cmd file ...";
    exit $err;
    } # usage

use Date::Parse;
use Getopt::Long;
GetOptions (
    "help|?"	=> sub { usage (0); },
    "V|version"	=> sub { say "$cmd [$VERSION]"; exit 0; },
    ) or usage (1);

my %f;
foreach my $fn (@ARGV) {

    open my $fh, "<", $fn or die "$fn: $!\n";
    my ($hdr, $body) = split m/(?<=\n)(?=\r?\n)/ => do { local $/; <$fh> }, 2;
    close $fh;

    $hdr && $hdr =~ m/\b(?:In-Reply-To|References)\b/i or next;

    my ($mid) = $hdr =~ m{^Message-Id:  (?:[\x20\t]*\n)?[\x20\t]+ (\S.*)}xmi;
    my ($dte) = $hdr =~ m{^Date:        (?:[\x20\t]*\n)?[\x20\t]+ (\S.*)}xmi;
    my ($irt) = $hdr =~ m{^In-Reply-To: (?:[\x20\t]*\n)?[\x20\t]+ (\S.*)}xmi;
    my ($ref) = $hdr =~ m{^References:  (?:[\x20\t]*\n)?[\x20\t]+ (\S.*)}xmi;

    my $stamp = str2time ($dte) or die $dte;
    my $date = $stamp ? do {
	my @d = localtime $stamp;
	sprintf "%4d-%02d-%02d %02d:%02d:%02d", $d[5] + 1900, ++$d[4], @d[3,2,1,0];
	} : "-";
    #printf "%12s %-20s %s\n", $stamp // "-", $date, $rcv;

    $f{$fn} = {
	msg_id	=> $mid,
	refs	=> $ref,
	irt	=> $irt,
	date	=> $dte,
	stamp	=> $stamp,
	sdate	=> $date,

	hdr	=> $hdr,
	body	=> $body,
	};
    }

foreach my $fn (sort keys %f) {

    my $c = 0;

    my $f = $f{$fn};
    if ($f->{refs}) {
	$c++;
	$f->{hdr} =~ s{\nReferences:.*(?:\n\s+.*)*+}{}ig;
	}
    if ($f->{irt}) {
	$c++;
	$f->{hdr} =~ s{\nIn-Reply-To:.*(?:\n\s+.*)*+}{}ig;
	}

    $c or next;	# No changes required

    say "$f->{msg_id} => -";

    my @t = stat $fn;
    open my $fh, ">", $fn or die "$fn: $!\n";
    print   $fh $f->{hdr}, $f->{body};
    close   $fh or die "$fn: $!\n";
    utime $t[8], $t[9], $fn;
    }

__END__

=head1 NAME

cm-break.pl - remove mail from thread

=head1 SYNOPSIS

 cm-break.pl ~/Mail/inbox/23 ~/Mail/inbox/45 ...

=head1 DESCRIPTION

This script should be called from within Claws-Mail as an action

Define an action as

  Menu name:  Unthread (break threading)
  Command:    cm-break.pl %F

Then select from the message list all files that should be un-threaded

Then invoke the action

All of those mails will be modified (if needed): their C<In-Reply-To:>
and C<References:> header tags are removed from the header.

=head1 SEE ALSO

L<Date::Parse>, L<Claws Mail|http://www.claws-mail.org>
cm-reparent.pl

=head1 AUTHOR

H.Merijn Brand <h.m.brand@xs4all.nl>

=head1 COPYRIGHT AND LICENSE

 Copyright (C) 2018-2018 H.Merijn Brand.  All rights reserved.

This library is free software;  you can redistribute and/or modify it under
the same terms as Perl itself.
See the L<Artistic license|http://dev.perl.org/licenses/artistic.html>.

=cut
