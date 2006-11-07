#!/usr/bin/perl -w
#
# sylprint.pl - process a Sylpheed mail and print it using enscript or lpr
# 
# (c) 2001 by Ricardo Mones Lastra <mones@aic.uniovi.es>
# This program is released under the GNU General Public License.
# See README.sylprint file for details and usage.

# NOTE: If you want to to change configuration edit sylprint.rc file,
#       all options are further explained in that file.

# hardwired config
$headerformat = '|%W|$%/$='; 
$printer = 'lp'; 
$papersize = 'A4'; 
$encoding = 'latin1';
$pageheaderfont = 'Times-Roman@11';
$mailfont = 'Courier@9/13';
$separator = '_';
$usenscript = 1;
$translate = 1;
$signature = 1;
$headers = 1;
$preview = 0;
$remquoted = '';
$wrapping = 79;
# programs
$_=`which enscript`; chomp; $ENS=$_;
$_=`which lpr`;      chomp; $LPR=$_;
$_=`which gv`;       chomp; $GPR=$_;
$_=`which gless`;    chomp; $TPR=$_;
$rc='sylprint.rc';
# parse parameters
die "required filename missing\n" unless (defined($ARGV[0]));
$a = 1;
# get user config
if (defined($ARGV[1]) && $ARGV[1] eq '-r') { $a++; }
else {
	@spp = split('/',$0); 
	$spp[$#spp] = ''; 
	$spp = join('/',@spp);  
	$rcf="$spp$rc"; 
	if (-x $rcf) { do $rcf; }
	$rcf="$ENV{'HOME'}/.claws-mail/$rc";
	if (-x $rcf) { do $rcf; }
}
@forens = ();
while (defined($ARGV[$a])) {
	for ($ARGV[$a]) {
		/-p/ && do { 
			$a++; 
			$printer = (defined($ARGV[$a]))? $ARGV[$a]: $printer;
			last;
		};
		/-f/ && do {
			$a++;
			$mailfont = (defined($ARGV[$a]))? $ARGV[$a]: $mailfont;
			$_ = $mailfont;
			die "$0: invalid font\n" unless (/\w+(\@\d+(\.\d+)?(\/\d+(\.\d+)?)?)?/);
			last;
		};
		/-s/ && do {
			$a++;
			$separator = (defined($ARGV[$a]))? $ARGV[$a]: '';
			if ($separator) {
				$_ = $separator;
				if (/-./) { $separator = ''; $a--; }
			}
			last; 
		};
		/-h/ && do { $headerformat = ''; last; };
		/-t/ && do { $translate = 0; last; };
		/-e/ && do { $usenscript = 0; last; };
		/-v/ && do { $preview++; last; };
		/-w/ && do {
			$a++;
			$wrapping = (defined($ARGV[$a]))? $ARGV[$a]: 0;
			if ($wrapping) {
				$_ = $wrapping;
				if (/-./) { $wrapping = 0; $a--; }
				else { die "$0: invalid number\n" unless (/\d+/); }
			}
			last;
		};
		/-Q/ && do {
			$remquoted = '>';
			if (defined($ARGV[$a + 1])) {
				$_ = $ARGV[$a + 1];
				do { $remquoted = $_; $a++ ; } unless (/-./);
			}
			last; 
		};
		/-S/ && do { $signature = 0; last; };
		/-H/ && do { $headers = 0; last; };
		/--/ && do { $a++; @forens = splice(@ARGV,$a); last; };
	};
	$a++;
}
# translations/encoding
$lang = (defined($ENV{'LANG'}) && $translate)? $ENV{'LANG'}: 'en';
for ($lang) {
	/cs.*/ && do {
		@cabl=("Datum","Od","Komu","Kopie","Diskusní skupiny","Pøedmìt");
		$encoding = 'latin2'; # Czech (iso-8859-2)
		last; 
	};
	/da.*/ && do {
		@cabl=("Dato","Fra","Til","Cc","Newsgroups","Emne");
		last;
	};
	/de.*/ && do {
		@cabl=("Datum","Von","An","Cc","Newsgruppen","Betreff");
		$headerformat = '|%W|Seite $% vom $=';
		last; 
	};
	/el.*/ && do {
		@cabl=("Çìåñïìçíßá","Áðü", "Ðñïò","Êïéíïðïßçóç","Newsgroups","ÈÝìá");
		$encoding = 'greek'; # Greek (iso-8859-7)
		last; 
	}; 
	/es.*/ && do {
		@cabl=("Fecha","Desde","Para","Copia","Grupos de noticias","Asunto");
		$headerformat = '|%W|Pág. $% de $=';
		last; 
	};
	/et.*/ && do {
		@cabl=("Kuupäev","Kellelt","Kellele","Koopia","Uudistegrupid","Pealkiri");
		last;
	};
	/fr.*/ && do { 
		@cabl=("Date","De","À","Cc","Groupe de discussion","Sujet"); 
		$headerformat = '|%W|Page $% des $=';
		last; 
	};
	/hr.*/ && do {
		@cabl=("Datum","Od","Za","Cc","News grupe","Tema");
		$encoding = 'latin2'; # Croatian (iso-8859-2)
		last; 
	};
	/hu.*/ && do {
		@cabl=("Dátum","Feladó","Címzett","Másolat","Üzenet-azonosító","Tárgy");
		$encoding = 'latin2'; # Hungarian (iso-8859-2)
		last;	
	};
	/it.*/ && do {
		@cabl=("Data","Da","A","Cc","Gruppo di notizie","Oggetto");
		$headerformat = '|%W|Pag. $% di $=';
		last; 
	};
	/ja.*/ && do {
		@cabl=("ÆüÉÕ","º¹½Ð¿Í","°¸Àè","Cc","¥Ë¥å¡¼¥¹¥°¥ë¡¼¥×","·ïÌ¾");
		warn "$0: charset not supported by enscript: using lpr\n";
		$usenscript = 0;
		last;
	};
	/ko.*/ && do {
		@cabl=("³¯Â¥","º¸³½ »ç¶÷","¹Þ´Â »ç¶÷","ÂüÁ¶","´º½º±×·ì","Á¦¸ñ");
		warn "$0: charset not supported by enscript: using lpr\n";
		$usenscript = 0;
		last;
	};
	/nl.*/ && do {
		@cabl=("Datum","Afzender","Aan","Cc","Nieuwsgroepen","Onderwerp"); 
		last; 
	};
	/pl.*/ && do {
		@cabl=("Data","Od","Do","Kopia","Grupy news","Temat");
		$encoding = 'latin2'; # Polish (iso-8859-2)
		last;
	};
	/pt.*/ && do {
		@cabl=("Data","De","Para","Cc","Grupos de notícias","Assunto"); 
		last; 
	};
	/ru.*/ && do {
		@cabl=("äÁÔÁ","ïÔ","ëÏÍÕ","ëÏÐÉÑ","çÒÕÐÐÙ ÎÏ×ÏÓÔÅÊ","ôÅÍÁ");
		$encoding = 'koi8'; # Russian (koi8-r)
		last;
	};
	/sv.*/ && do {
		@cabl=("Datum","Från","Till","Cc","Nyhetsgrupper","Ärende"); 
		last; 
	};
	/tr.*/ && do {
		@cabl=("Tarih","Kimden","Kime","Kk","Haber gruplarý","Konu");
		warn "$0: charset not supported by enscript: using lpr\n";
		$usenscript = 0;
		last;
	};
	/zh_CN\.GB2312/ && do {
		@cabl=("ÈÕÆÚ","·¢¼þÈË£º","ÖÂ(To)£º","³­ËÍ(Cc)£º","ÐÂÎÅ×é£º","±êÌâ£º");
		warn "$0: charset not supported by enscript: using lpr\n";
		$usenscript = 0;
		last;
	};
	/zh_TW\.Big5/ && do {
		@cabl=("¤é´Á","¨Ó¦Û¡G","¦¬¥ó¤H","°Æ¥»","·s»D¸s²Õ¡G","¼ÐÃD¡G");
		warn "$0: charset not supported by enscript: using lpr\n";
		$usenscript = 0;
		last;
	};
        /.*/ && do {
                @cabl=("Date","From","To","Cc","Newsgroups","Subject");
                last;
        };
}
# headers as given by Sylpheed
%cabs = ("Date",0,"From",1,"To",2,"Cc",3,"Newsgroups",4,"Subject",5);
@cabn = ("Date","From","To","Cc","Newsgroups","Subject");
@cont = ("","","","","","");
$body = "";
# go
$tmpfn="/tmp/sylprint.$ENV{'USER'}.$$";
open(TMP,">$tmpfn");
open(FIN,"<$ARGV[0]");
LN: while (<FIN>) {
	$ln = $_;
	foreach $n (@cabn) {
		$ix = $cabs{$n};
		if ($cont[$ix] eq "") {
			$_ = $ln;
			if (/^$n:\s+(.+)$/) {
				$cont[$ix]=$1;
				next LN;
			}
		}
	}
	if ($remquoted ne '' && /^\Q$remquoted\E(.+)$/) { next LN; }
	if (!$signature && /^--\s*$/) { last; }
	$body = join('',$body,$ln);
}
close(FIN);

# alignment
$ml = 0;
foreach $n (@cabn) {
	$lci = length($cabl[$cabs{$n}]);
	$ml = (($cont[$cabs{$n}] ne "") && ($lci > $ml))? $lci: $ml;
}
$ml++;

# print headers
if ($headers) {
	print TMP "\n\n";
	foreach $n (@cabn) {
		$ix = $cabs{$n};
		if ($cont[$ix] ne "") {
			print TMP "$cabl[$ix]", " " x ($ml - length($cabl[$ix])), ": ";
			if ($wrapping) {
				my $kk = 1; $wl = $wrapping;
				$l = $cont[$ix];
				while (length($l) > ($wl - $ml)) {
					$ll = substr($l,0,$wl);
					$jx = $wl - 1;
					while ((substr($ll,$jx,1) ne ' ') && $jx) { $jx--; }
					$ll = substr($l,0,($jx)? $jx: $wl,'');
					if ($kk) { print TMP $ll, "\n"; $kk--; }
					else { print TMP " ", " " x $ml, $ll, "\n"; }
				}
				if ($kk) { print TMP $l, "\n"; }
				else { print TMP " ", " " x $ml, $l, "\n"; }
			}
			else {
				print TMP $cont[$ix], "\n";
			}
		}
	}
	if ($separator) { 
		print TMP $separator x (($wrapping)? $wrapping: 79), "\n"; 
	};
}

# mail body
if ($wrapping) {
	$wl = $wrapping;
	@bodyl = split(/\n/,$body);
	foreach $l (@bodyl) {
		while (length($l) > $wl) {
			$ll = substr($l,0,$wl);
			$ix = $wl - 1;
			while ((substr($ll,$ix,1) ne ' ') && $ix) { $ix--; }
			$ll = substr($l,0,($ix)? $ix: $wl,''); 
			print TMP $ll,"\n";
		}
		print TMP $l,"\n";
	}
}
else { 
	print TMP "\n$body\n"; 
}
close(TMP);

# let enscript do its job
if (-x $ENS and $usenscript) {
	@ecmd = ($ENS,'','','-b',$headerformat,'-M',$papersize,'-X',$encoding,
		'-i','1c','-h','-f',$mailfont,'-F',$pageheaderfont,@forens,
		$tmpfn);
	if ($preview) {
		$ecmd[1] = '-p'; $ecmd[2] = "$tmpfn.ps";
		system(@ecmd);
		@vcmd = (split(' ',$GPR),"$tmpfn.ps");
		system(@vcmd);
		unlink("$tmpfn.ps");
	}
	if ($preview < 2) {
		$ecmd[1] = '-P'; $ecmd[2] = $printer;
		system(@ecmd);
	}
}
else { # no enscript, try lpr 
	if ($usenscript) { warn "$ENS not found, using lpr\n"; }
	die "$LPR not found\n" unless (-x $LPR);
	if ($preview) {
		@vcmd = (split(' ',$TPR),$tmpfn);
		system(@vcmd);
	}
	if ($preview < 2) {
		@lprcmd = ($LPR,'-T','Claws Mail Mail',
			($headerformat eq '')? '-l': '-p','-P',$printer,@forens,			$tmpfn);
		die "trying lpr: $! \n" unless (system(@lprcmd) != -1);
	}
}

# remove tmp stuff 
unlink($tmpfn);
