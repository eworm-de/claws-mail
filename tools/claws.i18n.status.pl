#!/usr/bin/perl -w
#
# claws.i18n.stats.pl - Generate statistics for Sylpheed Claws po directory.
# 
# (c) 2003 by Ricardo Mones Lastra <mones@aic.uniovi.es>
# This program is released under the GNU General Public License.
#
# constants -----------------------------------------------------------------

%langname = (
	'bg.po' => 'Bulgarian',
	'ca.po'	=> 'Catalan',
	'cs.po' => 'Czech',
	'de.po' => 'German',
	'el.po' => 'Greek',
	'en_GB.po' => 'British English',
	'es.po' => 'Spanish',
	'fi.po'	=> 'Finnish',
	'fr.po' => 'French',
	'hr.po' => 'Croatian',
	'hu.po' => 'Hungarian',
	'it.po' => 'Italian',
	'ja.po' => 'Japanese',
	'ko.po' => 'Korean',
	'nl.po' => 'Dutch',
	'pl.po' => 'Polish',
	'pt_BR.po' => 'Brazilian Portuguese',
	'ru.po' => 'Russian',
	'sk.po' => 'Slovak',
	'sr.po' => 'Serbian',
	'sv.po' => 'Swedish',
	'zh_CN.po' => 'Simpilified Chinese',
	'zh_TW.Big5.po' => 'Taiwanese',
);

%lasttranslator = (
	'bg.po' => 'George Danchev <danchev@spnet.net>',
	'ca.po'	=> 'Miquel Oliete <miqueloliete@softhome.net>',
	'cs.po' => 'Radek Vybíral <Radek.Vybiral@vsb.cz>',
	'de.po' => 'Thomas Gilgin <thg1@karate-muellheim.de>',
	'el.po' => 'Michalis Kabrianis <Michalis@bigfoot.com>',
	'en_GB.po' => 'Paul Mangan <claws@thewildbeast.co.uk>',
	'es.po' => 'Ricardo Mones Lastra <mones@aic.uniovi.es>',
	'fi.po'	=> 'Flammie Pirinen <flammie@iki.fi>',
	'fr.po' => 'Fabien Vantard <fzzzzz@gmail.com>',
	'hr.po' => 'Dragan Leskovar <drleskov@inet.hr>',
	'hu.po' => 'Gál Zoltán <galzoli@hu.inter.net>',
	'it.po' => 'Andrea Spadaccini <a.spadaccini@catania.linux.it>',
	'ja.po' => 'Rui Hirokawa <rui_hirokawa@ybb.ne.jp>',
	'ko.po' => 'ChiDeok, Hwang <hwang@mizi.co.kr>',
	'nl.po' => 'Wilbert Berendsen <wbsoft@xs4all.nl>',
	'pl.po' => 'Emilian Nowak <eminowbl@posejdon.wpk.p.lodz.pl>',
	'pt_BR.po' => 'Frederico Goncalves Guimaraes <fggdebian@yahoo.com.br>',
	'ru.po' => 'Pavlo Bohmat <bohm@ukr.net>',
	'sk.po' => 'Andrej Kacian <andrej@kacian.sk>',
	'sr.po' => 'urke <urke@users.sourceforge.net>',
	'sv.po' => 'Joakim Andreasson <joakim.andreasson@gmx.net>',
	'zh_CN.po' => 'Hansom Young <glyoung@users.sourceforge.net>',
	'zh_TW.Big5.po' => 'Tsu-Fan Cheng <tscheng@ic.sunysb.edu>',
);

%barcolornorm = (
	default => 'white',
	partially => 'lightblue',	
	completed => 'blue',		
);

%barcoloraged = (	
	default => 'white',
	partially => 'lightgrey',	# ligth red '#FFA0A0',
	completed => 'grey',		# darker red '#FF7070',
);

%barcolorcheat = (	# remarks translations with revision dates in the future
	default => 'white',
	partially => 'yellow',
	completed => 'red',
);

$barwidth = 300; # pixels
$barheight = 12; # pixels

$transolddays = 90;	# days to consider a translation is old, so probably unmaintained. 
$transneedthresold = 0.75; # percent/100

$msgfmt = '/usr/bin/msgfmt';

$averagestr = 'Project average';
$contactaddress = 'translations@thewildbeast.co.uk';

# $pagehead = '../../claws.i18n.head.php';
# $pagetail = '../../claws.i18n.tail.php';

# code begins here ----------------------------------------------------------
sub get_current_date {
	$date = `date --utc`;
	chop $date;
	$date =~ /(\S+)(\s+)(\S+)(\s+)(\S+)(\s+)(\S+)(\D+)(\d+)/;
	$datetimenow   = "$5-$3-$9 at $7"."$8";
}

sub get_trans_age {
	my ($y, $m, $d) = @_;
	return ($y * 365) + ($m * 31) + $d;
}

($sec,$min,$hour,$mday,$mon,$year,$wday,$yday) = gmtime(time);
$year += 1900;
$mon++;
$cage = get_trans_age($year,$mon,$mday); # get current "age"

# drawing a language status row
sub print_lang {
	my ($lang, $person, $trans, $fuzzy, $untrans, $tage) = @_;
	$total = $trans + $fuzzy + $untrans;
	if ($tage == 0) { $tage = $cage; } # hack for average translation
	print STDERR $cage, " ",  $tage, "\n";
	if (($cage - $tage) < 0) {
		$barcolor = \%barcolorcheat;
	} else {
		$barcolor = (($cage - $tage) > $transolddays)? \%barcoloraged : \%barcolornorm ;
	}
	$_ = $person;
	if (/(.+)\s+\<(.+)\>/) { $pname = $1; $pemail = $2; } else { $pname = $pemail = $contactaddress; }
	print "<tr>\n<td>\n";
	if ($lang eq $averagestr) {
		print "<b>$lang</b>";
	} else {
		print "<a href=\"mailto:%22$pname%22%20<$pemail>\">$lang</a>";
	}
	print "</td>\n";
	print "<td>\n<table style='border: solid 1px black; width: $barwidth' border='0' cellspacing='0' cellpadding='0'><tr>\n";
	$barlen = ($trans / $total) * $barwidth; 
	print "<td style='width:$barlen", "px; height:$barheight", "px;' bgcolor=\"$$barcolor{completed}\"></td>\n";
	$barlen2 = ($fuzzy / $total) * $barwidth;
	print "<td style='width:$barlen2", "px' bgcolor=\"$$barcolor{partially}\"></td>\n";
	$barlen3 = $barwidth - $barlen2 - $barlen;
	print "<td style='width:$barlen3", "px' bgcolor=\"$$barcolor{default}\"></td>\n";
	print "</tr>\n</table>\n</td>\n\n<td style='text-align: right'>", int(($trans / $total) * 10000) / 100,  "%</td>\n";
	if (($lang eq $langname{'en_GB.po'}) or ($lang eq $averagestr)) { $trans = $total; } # hack for en_GB and average results
	$transtatus = (($trans / $total) < $transneedthresold)? '<font size="+1" color="red"> * </font>': '';
	print "<td>$transtatus</td>\n</tr>\n";
}

sub tens {
	my ($i) = @_;
	return (($i > 9)? "$i" : "0$i");
}

get_current_date();

# get project version from changelog (project dependent code :-/ )
$_ = `head -1 ../ChangeLog-gtk2.claws`;
if (/\S+\s+\S+\s+(\S+)/) { $genversion = $1; } else { $genversion = 'Unknown'; }

$numlang = keys(%langname);

# print `cat $pagehead`;
#
# make it a here-doc
#print <<ENDOFHEAD;
# removed for being included
#ENDOFHEAD

# start
print "<div class=indent>\n<b>Translation Status (on $datetimenow for $genversion)</b>\n<div class=indent>\n<table cellspacing=0 cellpadding=2>\n";

# table header
print "<tr bgcolor=#cccccc>\n<th align=left>Language</th>\n<th>Translated|Fuzzy|Untranslated</th>\n<th>Percent</th>\n<th>\n</th>\n</tr>\n";

# get files
opendir(PODIR, ".") || die("Error: can't open current directory\n");
push(@pofiles,(readdir(PODIR)));
closedir(PODIR);

@sorted_pofiles = sort(@pofiles);
# iterate them
$alang = $atran = $afuzz = $auntr = 0;
foreach $pofile (@sorted_pofiles) {
	$_ = $pofile;
	if (/.+\.po$/) {
		print STDERR "Processing $_\n"; # be a little informative
		++$alang;
		$transage = $tran = $fuzz = $untr = 0;
		$_ = `$msgfmt -c --statistics -o /dev/null $pofile 2>&1`;
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
		$_ = `grep 'PO-Revision-Date:' $pofile | cut -f2 -d:`;
		if (/\s+(\d+)\-(\d+)\-(\d+)/) { 
			$transage = get_trans_age($1,$2,$3); 
		}
		print_lang($langname{$pofile},$lasttranslator{$pofile},$tran,$fuzz,$untr,$transage);
	}
}

# average results for the project
print "<tr>\n<td colspan=3 height=8></td>\n<tr>";
print_lang($averagestr,'',$atran,$afuzz,$auntr,0);
	
# table footer
print "</table>\n";

# end
# print "<br>Number of languages supported: $alang <br>";
print "<p>\nLanguages marked with <font size=\"+1\" color=\"red\"> *</font> really need your help to be completed.<br>";
print "<p>The ones with grey bars are <i>probably unmaintained</i> because translation is more than ", $transolddays / 30, " months old, anyway, trying to contact current translator first is usually a good idea before submitting an updated one.<p><b>NOTE</b>: if you are the translator of one of them and don't want to see your language bar in grey you should manually update the <tt>PO-Revision-Date</tt> field in the .po file header (or, alternatively, use a tool which does it for you).<br>";
# print "If you want to help those or contribute any new language please contact <a href=\"mailto:$contactaddress\">us</a>.";
print "</div></div>";

# print `cat $pagetail`;
#
# make it a here-doc
#print <<ENDOFTAIL;
# removed for being included
#ENDOFTAIL

# done
