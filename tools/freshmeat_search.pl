#!/usr/bin/perl

my $freshmeat = "http://freshmeat.net/search?q";
$_ = <>;

chdir($ENV{HOME} . "/.sylpheed") or die("Can't find your .sylpheed directory\n");

open (SYLRC, "<sylpheedrc") || die("Can't open the sylpheedrc file\n");
	@rclines = <SYLRC>;
close SYLRC;

foreach $rcline (@rclines) {
	if ($rcline =~ m/^uri_open_command/) {
		chomp $rcline;
		@browser = split(/=/, $rcline);
		$browser[1] =~ s/ '%s'$//;
	}
}

system("$browser[1] '$freshmeat=$_' &");

exit;
