#!/usr/bin/perl -w
#
# claws.i18n.stats.pl - Generate statistics for Claws Mail po directory.
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
	'nb.po' => 'Norwegian',
	'nl.po' => 'Dutch',
	'pl.po' => 'Polish',
	'pt_BR.po' => 'Brazilian Portuguese',
	'ru.po' => 'Russian',
	'sk.po' => 'Slovak',
	'sr.po' => 'Serbian',
	'sv.po' => 'Swedish',
	'zh_CN.po' => 'Simpilified Chinese',
	'zh_TW.po' => 'Taiwanese',
);

%lasttranslator = (
	'bg.po' => 'Yasen Pramatarov <yasen@lindeas.com>',
	'ca.po'	=> 'Miquel Oliete <ktalanet@yahoo.es>',
	'cs.po' => 'Tim <timbrain@post.cz>',
	'de.po' => 'Stephan Sachse <white@dev-zero.com>',
	'el.po' => 'Stavros Giannouris 	<stavrosg2002@freemail.gr>',
	'en_GB.po' => 'Paul Mangan <claws@thewildbeast.co.uk>',
	'es.po' => 'Ricardo Mones Lastra <ricardo@mones.org>',
	'fi.po'	=> 'Flammie Pirinen <flammie@iki.fi>',
	'fr.po' => 'Fabien Vantard <fzzzzz@gmail.com>',
	'hr.po' => 'Dragan Leskovar <drleskov@inet.hr>',
	'hu.po' => 'G&aacute;l Zolt&aacute;n <galzoli@hu.inter.net>',
	'it.po' => 'Andrea Spadaccini <a.spadaccini@catania.linux.it>',
	'ja.po' => 'Rui Hirokawa <rui_hirokawa@ybb.ne.jp>',
	'ko.po' => 'ChiDeok, Hwang <hwang@mizi.co.kr>',
	'nb.po' => 'Geir Helland <pjallabais@users.sourceforge.net>',
	'nl.po' => 'Tim Dijkstra <tim@famdijkstra.org>',
	'pl.po' => 'Quar <Quar@sylpheed.quar.org>',
	'pt_BR.po' => 'Frederico Goncalves Guimaraes <fggdebian@yahoo.com.br>',
	'ru.po' => 'Pavlo Bohmat <bohm@ukr.net>',
	'sk.po' => 'Andrej Kacian <andrej@kacian.sk>',
	'sr.po' => 'urke <urke@users.sourceforge.net>',
	'sv.po' => 'Anders Trob&auml;ck <sylpheed@troback.com>',
	'zh_CN.po' => 'Ralgh Young <bamanzi@gmail.com>',
	'zh_TW.po' => 'Wei-Lun Chao <chaoweilun@pcmail.com.tw>',
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

$barwidth = 500; # pixels
$barheight = 12; # pixels

$transolddays = 90;	# days to consider a translation is old, so probably unmaintained.
$transoldmonths = $transolddays / 30;
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
	my ($lang, $person, $trans, $fuzzy, $untrans, $tage, $oddeven) = @_;
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
	print "<tr";
	if ($oddeven > 0) { print " bgcolor=#EFEFEF"; }
	print ">\n<td>\n";
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
$_ = `head -1 ../ChangeLog`;
if (/\S+\s+\S+\s+(\S+)/) { $genversion = $1; } else { $genversion = 'Unknown'; }

$numlang = keys(%langname);

# print `cat $pagehead`;
#
# make it a here-doc
#print <<ENDOFHEAD;
# removed for being included
#ENDOFHEAD

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
opendir(PODIR, ".") || die("Error: can't open current directory\n");
push(@pofiles,(readdir(PODIR)));
closedir(PODIR);

@sorted_pofiles = sort(@pofiles);
# iterate them
$alang = $atran = $afuzz = $auntr = $oddeven = 0;
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
		print_lang($langname{$pofile},$lasttranslator{$pofile},$tran,$fuzz,$untr,$transage, $oddeven);
		if ($oddeven == 1) { $oddeven = 0 } else { $oddeven++; } 
	}
}

# average results for the project
print "<tr>\n<td colspan=3 height=8></td>\n<tr>";
print_lang($averagestr,'',$atran,$afuzz,$auntr,0,0);
	
# table footer
print "</table>\n";

# end
# print "<br>Number of languages supported: $alang <br>";
print qq ~<p>
	  Languages marked with <font size="+1" color="red"> *</font>
	  really need your help to be completed.
          <p>
	  The ones with grey bars are <i>probably unmaintained</i> because
          translation is more than $transoldmonths months old, anyway, trying
	  to contact current translator first is usually a good idea before
	  submitting an updated one.<p><b>NOTE</b>: if you are the translator
	  of one of them and don't want to see your language bar in grey you
	  should manually update the <tt>PO-Revision-Date</tt> field in the .po
	  file header (or, alternatively, use a tool which does it for you).
	  <br>
	  </div>
	  </div>~;

# print `cat $pagetail`;
#
# make it a here-doc
#print <<ENDOFTAIL;
# removed for being included
#ENDOFTAIL

# done
