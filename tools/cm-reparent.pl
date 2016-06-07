#!/usr/bin/perl

use 5.14.1;
use warnings;

our $VERSION = "1.02 - 2016-06-07";

sub usage {
    my $err = shift and select STDERR;
    say "usage: $0 file ...";
    exit $err;
    } # usage

use Date::Parse;
use Getopt::Long;
GetOptions (
    "help|?"	=> sub { usage (0); },
    "V|version"	=> sub { say $0 =~ s{.*/}{}r, " [$VERSION]"; exit 0; },
    ) or usage (1);

my $p;
my %f;
foreach my $fn (@ARGV) {

    open my $fh, "<", $fn or die "$fn: $!\n";
    my ($hdr, $body) = split m/(?<=\n)(?=\r?\n)/ => do { local $/; <$fh> }, 2;
    close $fh;

    $hdr or next;

    my ($mid) = $hdr =~ m{^Message-Id: (.*)}mi;
    my ($dte) = $hdr =~ m{^Date: (.*)}mi;
    my ($irt) = $hdr =~ m{^In-Reply-To: (.*)}mi;
    my ($ref) = $hdr =~ m{^References: (.*)}mi;

    my $stamp = str2time ($dte) or next;

    $f{$fn} = {
	msg_id	=> $mid,
	refs	=> $ref,
	irt	=> $irt,
	date	=> $dte,
	stamp	=> $stamp,

	hdr	=> $hdr,
	body	=> $body,
	};

    $p //= $fn;

    $stamp < $f{$p}{stamp} and $p = $fn;
    }

# All but the oldest will refer to the oldest as parent

$p or exit 0;
my $pid = $f{$p}{msg_id};

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

    say "$f->{msg_id} => $pid";

    open my $fh, ">", $fn or die "$fn: $!\n";
    print $fh $f->{hdr}, $f->{body};
    close $fh or die "$fn: $!\n";
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
reflect that the oldest mail is the parent of all other mails

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

and not

 A            123AC_12      2016-06-01 12:13:14
 +- B         aFFde2993     2016-06-01 13:14:15
    +- C      0000_1234     2016-06-02 10:18:04
       +- D   foo_bar_12    2016-06-03 04:00:00

=head1 SEE ALSO

L<Date::Parse>, L<Claws Mail|http://www.claws-mail.org>

=head1 AUTHOR

H.Merijn Brand <h.m.brand@xs4all.nl>

=head1 COPYRIGHT AND LICENSE

 Copyright (C) 2016-2016 H.Merijn Brand.  All rights reserved.

This library is free software;  you can redistribute and/or modify it under
the same terms as Perl itself.
See the L<Artistic license|http://dev.perl.org/licenses/artistic.html>.

=cut
