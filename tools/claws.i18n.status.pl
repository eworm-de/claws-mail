#!/usr/bin/perl
#
# claws.i18n.stats.pl - Generate statistics for Claws Mail po directory.
#
# Copyright (C) 2003-2020 by Ricardo Mones <ricardo@mones.org>,
#                            Paul Mangan <paul@claws-mail.org>
# This program is released under the GNU General Public License.
#
use warnings;
use strict;
use File::Which;

# constants -----------------------------------------------------------------
my %lang = (
	'bg.po' => {
		'out' => 0, 'name' => 'Bulgarian',
		'last' => 'Yasen Pramatarov <yasen@lindeas.com>',
	},
	'ca.po' => {
		'out' => 1, 'name' => 'Catalan',
		'last' => 'David Medina <opensusecatala@gmail.com>',
	},
	'cs.po' => {
		'out' => 1, 'name' => 'Czech',
		'last' => 'David Vachulka <david@konstrukce-cad.com>',
	},
	'da.po' => {
		'out' => 1, 'name' => 'Danish',
		'last' => 'Erik P. Olsen <epodata@gmail.com>',
	},
	'de.po' => {
		'out' => 1, 'name' => 'German',
		'last' => 'Simon Legner <simon.legner@gmail.com>',
	},
	'el_GR.po' => {
		'out' => 1, 'name' => 'Greek',
		'last' => 'Haris Karachristianidis <hariskar@cryptolab.net>',
	},
	'en_GB.po' => {
		'out' => 1, 'name' => 'British English', 'lazy' => 1,
		'last' => 'Paul Mangan <paul@claws-mail.org>',
	},
	'eo.po' => {
		'out' => 0, 'name' => 'Esperanto',
		'last' => 'Sian Mountbatten <poenikatu@fastmail.co.uk>',
	},
	'es.po' => {
		'out' => 1, 'name' => 'Spanish',
		'last' => 'Ricardo Mones <ricardo@mones.org>',
	},
	'fi.po' => {
		'out' => 1, 'name' => 'Finnish',
		'last' => 'Flammie Pirinen <flammie@iki.fi>',
	},
	'fr.po' => {
		'out' => 1, 'name' => 'French',
		'last' => 'Tristan Chabredier <wwp@claws-mail.org>',
	},
	'he.po' => {
		'out' => 0, 'name' => 'Hebrew',
		'last' => 'Isratine Citizen <genghiskhan@gmx.ca>',
	},
	'hu.po' => {
		'out' => 1, 'name' => 'Hungarian',
		'last' => 'P&aacute;der Rezs&#337; <rezso@rezso.net>',
	},
	'id_ID.po' => {
		'out' => 1, 'name' => 'Indonesian',
		'last' => 'MSulchan Darmawan <bleketux@gmail.com>',
	},
	'it.po' => {
		'out' => 1, 'name' => 'Italian',
		'last' => 'Luigi Votta <luigi.vtt@gmail.com>',
	},
	'ja.po' => {
		'out' => 1, 'name' => 'Japanese',
		'last' => 'UTUMI Hirosi <utuhiro78@yahoo.co.jp>',
	},
	'lt.po' => {
		'out' => 0, 'name' => 'Lithuanian',
		'last' => 'Mindaugas Baranauskas <embar@super.lt>',
	},
	'nb.po' => {
		'out' => 1, 'name' => 'Norwegian Bokm&aring;l',
		'last' => 'Petter Adsen <petter@synth.no>',
	},
	'nl.po' => {
		'out' => 1, 'name' => 'Dutch',
		'last' => 'Marcel Pol <mpol@gmx.net>',
	},
	'pl.po' => {
		'out' => 1, 'name' => 'Polish',
		'last' => '&#x141;ukasz Wojni&#x142;owicz <lukasz.wojnilowicz@gmail.com>',
	},
	'pt_BR.po' => {
		'out' => 1, 'name' => 'Brazilian Portuguese',
		'last' => 'Frederico Goncalves Guimaraes <fggdebian@yahoo.com.br>',
	},
	'pt_PT.po' => {
		'out' => 1, 'name' => 'Portuguese',
		'last' => 'Pedro Albuquerque <palbuquerque73@gmail.com>',
	},
	'ro.po' => {
		'out' => 1, 'name' => 'Romanian',
		'last' => 'Cristian Secar&#259; <liste@secarica.ro>',
	},
	'ru.po' => {
		'out' => 1, 'name' => 'Russian',
		'last' => 'Mikhail Kurinnoi <viewizard@viewizard.com>',
	},
	'sk.po' => {
		'out' => 1, 'name' => 'Slovak',
		'last' => 'Slavko <slavino@slavino.sk>',
	},
	'sv.po' => {
		'out' => 1, 'name' => 'Swedish',
		'last' => 'Andreas Rönnquist <gusnan@openmailbox.org>',
	},
	'tr.po' => {
		'out' => 1, 'name' => 'Turkish',
		'last' => 'Numan Demirdöğen <if.gnu.linux@gmail.com>',
	},
	'uk.po' => {
		'out' => 0, 'name' => 'Ukrainian',
		'last' => 'YUP <yupadmin@gmail.com>',
	},
	'zh_CN.po' => {
		'out' => 0, 'name' => 'Simplified Chinese',
		'last' => 'Rob <rbnwmk@gmail.com>',
	},
	'zh_TW.po' => {
		'out' => 1, 'name' => 'Traditional Chinese',
		'last' => 'Mark Chang <mark.cyj@gmail.com>',
	},
);

my %barcolornorm = (
	default => 'white',
	partially => 'lightblue',
	completed => 'blue',
);

my %barcoloraged = (
	default => 'white',
	partially => 'lightgrey',	# ligth red '#FFA0A0',
	completed => 'grey',		# darker red '#FF7070',
);

my %barcolorcheat = (	# remarks translations with revision dates in the future
	default => 'white',
	partially => 'yellow',
	completed => 'red',
);

my ($barwidth, $barheight) = (500, 12); # pixels

my $transolddays = 120;	# days to consider a translation is old, so probably unmaintained.
my $transoldmonths = $transolddays / 30;
my $transneedthresold = 0.75; # percent/100

my ($msgfmt, $date, $grep, $cut) = map {
  my $bin = which($_); die "missing '$_' binary" unless defined $bin; $bin
} qw(msgfmt date grep cut);

my $averageitem = {'name' => 'Project average', 'out' => 1, 'last' => ''};
my $contactaddress = 'translations@thewildbeast.co.uk';

# code begins here ----------------------------------------------------------
sub get_current_date {
	my $utc = qx{$date --utc};
	chop $utc;
	$utc =~ /(\S+)(\s+)(\S+)(\s+)(\S+)(\s+)(\S+)(\D+)(\d+)/;
	return "$5-$3-$9 at $7"."$8";
}

sub get_trans_age {
	my ($y, $m, $d) = @_;
	return ($y * 365) + ($m * 31) + $d;
}

my (undef, undef, undef, $mday, $mon, $year, undef, undef) = gmtime(time);
$year += 1900;
$mon++;
my $cage = get_trans_age($year, $mon, $mday); # get current "age"

# drawing a language status row
sub print_lang {
	my ($langmap, $trans, $fuzzy, $untrans, $tage, $oddeven) = @_;
	return if not $langmap->{'out'};
	my $lang = $langmap->{'name'};
	my $person = $langmap->{'last'};
	my $total = $trans + $fuzzy + $untrans;
	if ($tage == 0) { $tage = $cage; } # hack for average translation
	# print STDERR $cage, " ",  $tage, "\n";
	my ($barcolor, $pname, $pemail);
	if (($cage - $tage) < 0) {
		$barcolor = \%barcolorcheat;
	} else {
		$barcolor = (($cage - $tage) > $transolddays)? \%barcoloraged : \%barcolornorm ;
	}
	$_ = $person;
	if (/(.+)\s+\<(.+)\>/) {
		$pname = $1; $pemail = $2;
	} else {
		$pname = $pemail = $contactaddress;
	}
	print '<tr', ($oddeven? ' bgcolor=#EFEFEF': ''), ">\n<td>\n";
	if ($lang eq $averageitem->{'name'}) {
		print "<b>$lang</b>";
	} else {
		print "<a href=\"mailto:%22$pname%22%20<$pemail>\">$lang</a>";
	}
	print "</td>\n";
	print "<td>\n<table style='border: solid 1px black; width: $barwidth'",
		" border='0' cellspacing='0' cellpadding='0'><tr>\n";
	my $barlen = ($trans / $total) * $barwidth; 
	print "<td style='width:$barlen", "px; height:$barheight",
		"px;' bgcolor=\"$$barcolor{completed}\"></td>\n";
	my $barlen2 = ($fuzzy / $total) * $barwidth;
	print "<td style='width:$barlen2", "px' bgcolor=\"$$barcolor{partially}\"></td>\n";
	my $barlen3 = $barwidth - $barlen2 - $barlen;
	print "<td style='width:$barlen3", "px' bgcolor=\"$$barcolor{default}\"></td>\n";
	print "</tr>\n</table>\n</td>\n\n<td style='text-align: right'>",
		int(($trans / $total) * 10000) / 100,  "%</td>\n";
	my $transtatus = (($trans / $total) < $transneedthresold)
		? '<font size="+1" color="red"> * </font>': '';
	print "<td>$transtatus</td>\n</tr>\n";
}

sub tens {
	my ($i) = @_;
	return (($i > 9)? "$i" : "0$i");
}

my $datetimenow = get_current_date();

# get project version from changelog (project dependent code :-/ )
my $genversion = 'Unknown';
my $changelog = '../Changelog';
if (-s $changelog) {
	my $head = which('head');
	if (defined $head) {
		$_ = qx{$head -1 $changelog};
		if (/\S+\s+\S+\s+(\S+)/) { $genversion = $1; }
	}
} else {
	my $git = which('git');
	if (defined $git) {
		$_ = qx{$git describe --abbrev=0};
		if (/(\d+\.\d+\.\d)/) { $genversion = $1; }
	}
}

# start
print qq ~<div class=indent>
	  <b>Translation Status (on $datetimenow for $genversion)</b>
	  <div class=indent>
	  	<table cellspacing=0 cellpadding=2>~;

# table header
print qq ~<tr bgcolor=#cccccc>
	  <th align=left>Language</th>
	  <th>Translated|Fuzzy|Untranslated</th>
	  <th>Percent</th>
	  <th></th>
	  </tr>~;

# get files
my @pofiles;
opendir(PODIR, ".") || die("Error: can't open current directory\n");
push(@pofiles,(readdir(PODIR)));
closedir(PODIR);

my @sorted_pofiles = sort(@pofiles);
# iterate them
my ($alang, $atran, $afuzz, $auntr, $oddeven) = (0, 0, 0, 0, 0);
foreach my $pofile (@sorted_pofiles) {
	$_ = $pofile;
	if (/.+\.po$/ && defined($lang{$pofile}) ) {
		print STDERR "Processing $_\n"; # be a little informative
		++$alang;
		my ($transage, $tran, $fuzz, $untr) = (0, 0, 0, 0);
		$_ = qx{$msgfmt -c --statistics -o /dev/null $pofile 2>&1};
		if (/([0-9]+)\s+translated/) {
			$tran = $1;
		}
		if (/([0-9]+)\s+fuzzy/) {
			$fuzz = $1;
		}
		if (/([0-9]+)\s+untranslated/) {
			$untr = $1;
		}
		# print STDERR "Translated [$tran] Fuzzy [$fuzz] Untranslated [$untr]\n";
		$atran += $tran;
		$afuzz += $fuzz;
		$auntr += $untr;
		if ($lang{$pofile}->{'lazy'}) {
			$tran = $tran + $fuzz;
			$untr = "0";
			$fuzz = "0";
			$transage = $cage;
		} else {
			$_ = qx{$grep 'PO-Revision-Date:' $pofile | $cut -f2 -d:};
			if (/\s+(\d+)\-(\d+)\-(\d+)/) {
				$transage = get_trans_age($1, $2, $3);
			}
		}
		print_lang($lang{$pofile}, $tran, $fuzz, $untr, $transage, $oddeven);
		$oddeven = $oddeven? 0: 1;
	}
}

# average results for the project
print "<tr>\n<td colspan=3 height=8></td>\n<tr>";
print_lang($averageitem, $atran, $afuzz, $auntr, 0, 0);

# table footer
print "</table>\n";
print qq ~</div>
	  </div>~;

# done
