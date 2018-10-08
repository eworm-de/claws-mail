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

my $p;
my %f;
foreach my $fn (@ARGV) {

    open my $fh, "<", $fn or die "$fn: $!\n";
    my ($hdr, $body) = split m/(?<=\n)(?=\r?\n)/ => do { local $/; <$fh> }, 2;
    close $fh;

    $hdr && $hdr =~ m/\b(?:Date|Received)\b/ or next;

    my ($mid) = $hdr =~ m{^Message-Id:  (?:[\x20\t]*\n)?[\x20\t]+ (\S.*)}xmi;
    my ($dte) = $hdr =~ m{^Date:        (?:[\x20\t]*\n)?[\x20\t]+ (\S.*)}xmi;
    my ($rcv) = $hdr =~ m{\nReceived:   (?:[\x20\t]*\n)?[\x20\t]+ (\S.*(?:\n\s+.*)*+)}xi;
    my ($irt) = $hdr =~ m{^In-Reply-To: (?:[\x20\t]*\n)?[\x20\t]+ (\S.*)}xmi;
    my ($ref) = $hdr =~ m{^References:  (?:[\x20\t]*\n)?[\x20\t]+ (\S.*)}xmi;

    $rcv ||= $dte;
    $rcv =~ s/[\s\r\n]+/ /g;
    $rcv =~ s/\s+$//;
    $rcv =~ s/.*;\s*//;
    $rcv =~ s/.* id \S+\s+//i;
    my $stamp = str2time ($rcv) or die $rcv;
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
	rcvd	=> $rcv,
	stamp	=> $stamp,
	sdate	=> $date,

	hdr	=> $hdr,
	body	=> $body,
	};

    $p //= $fn;
    $stamp < $f{$p}{stamp} and $p = $fn;
    }

# All but the oldest will refer to the oldest as parent

$p or exit 0;
my $pid = $f{$p}{msg_id} or die "Parent file $p has no Message-Id\n";

foreach my $fn (sort keys %f) {

    $fn eq $p and next;

    my $c = 0;

    my $f = $f{$fn};
    if ($f->{refs}) {
	unless ($f->{refs} eq $pid) {
	    $c++;
	    $f->{hdr} =~ s{^(?=References:)}{References: $pid\nX-}mi;
	    }
	}
    else {
	$c++;
	$f->{hdr} =~ s{^(?=Message-Id:)}{References: $pid\n}mi;
	}
    if ($f->{irt}) {
	unless ($f->{irt} eq $pid) {
	    $c++;
	    $f->{hdr} =~ s{^(?=In-Reply-To:)}{In-Reply-To: $pid\nX-}mi;
	    }
	}
    else {
	$c++;
	$f->{hdr} =~ s{^(?=Message-Id:)}{In-Reply-To: $pid\n}mi;
	}

    $c or next;	# No changes required

    unless ($f->{msg_id}) {
	warn "Child message $fn has no Message-Id, skipped\n";
	next;
	}

    say "$f->{msg_id} => $pid";

    my @t = stat $fn;
    open my $fh, ">", $fn or die "$fn: $!\n";
    print   $fh $f->{hdr}, $f->{body};
    close   $fh or die "$fn: $!\n";
    utime $t[8], $t[9], $fn;
    }

__END__

=head1 NAME

cm-reparent.pl - fix mail threading

=head1 SYNOPSIS

 cm-reparent.pl ~/Mail/inbox/23 ~/Mail/inbox/45 ...

=head1 DESCRIPTION

This script should be called from within Claws-Mail as an action

Define an action as

  Menu name:  Reparent (fix threading)
  Command:    cm-reparent.pl %F

Then select from the message list all files that should be re-parented

Then invoke the action

All but the oldest of those mails will be modified (if needed) to
reflect that the oldest mail is the parent of all other mails by
adding or altering the header lines C<In-Reply-To:> and C<References:>

Given 4 files A, B, C, and D like

 File         Message-Id    Date
 A            123AC_12      2016-06-01 12:13:14
 B            aFFde2993     2016-06-01 13:14:15
 C            0000_1234     2016-06-02 10:18:04
 D            foo_bar_12    2016-06-03 04:00:00

The new tree will be like

 A            123AC_12      2016-06-01 12:13:14
 +- B         aFFde2993     2016-06-01 13:14:15
 +- C         0000_1234     2016-06-02 10:18:04
 +- D         foo_bar_12    2016-06-03 04:00:00

and not like

 A            123AC_12      2016-06-01 12:13:14
 +- B         aFFde2993     2016-06-01 13:14:15
    +- C      0000_1234     2016-06-02 10:18:04
       +- D   foo_bar_12    2016-06-03 04:00:00

Existing entries of C<References:> and C<In-Reply-To:> in the header
of any of B, C, or D will be preserved as C<X-References:> or
C<X-In-Reply-To:> respectively.

=head1 SEE ALSO

L<Date::Parse>, L<Claws Mail|http://www.claws-mail.org>
cm-break.pl

=head1 AUTHOR

H.Merijn Brand <h.m.brand@xs4all.nl>

=head1 COPYRIGHT AND LICENSE

 Copyright (C) 2016-2018 H.Merijn Brand.  All rights reserved.

This library is free software;  you can redistribute and/or modify it under
the same terms as Perl itself.
See the L<Artistic license|http://dev.perl.org/licenses/artistic.html>.

=cut
