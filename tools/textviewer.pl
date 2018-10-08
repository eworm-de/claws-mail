#!/usr/bin/perl

# COPYRIGHT AND LICENSE
#        Copyright (C) 2005-2018 H.Merijn Brand
#
#        This script is free software; you can redistribute it and/or modify it
#        under the same terms as Perl and/or Claws Mail itself. (GPL)

use 5.14.1;
use warnings;

our $VERSION = "1.01 - 2018-10-08";
our $CMD = $0 =~ s{.*/}{}r;

sub usage {
    my ($err, $str) = (@_, "");
    $err and select STDERR;
    say "usage: $CMD [--html] [--type=<type>] file\n",
	"       --html    Generate HTML (if supported)\n",
	"       --type=X  X as mimetype (msword => doc)\n",
	"  $CMD --list will show all implemented conversions";
    $str and say $str;
    exit $err;
    } # usage

use Getopt::Long qw(:config bundling nopermute);
my $opt_v = 0;
my $opt_h = "text";
GetOptions (
    "help|?"		=> sub { usage (0); },
    "V|version"		=> sub { say "$CMD [$VERSION]"; exit 0; },

    "v|verbose:1"	=> \$opt_v,
    "t|type|mimetype=s"	=> \my $opt_t,
    "h|html"		=> sub { $opt_h = "html" },
    "l|list!"		=> \my $opt_l,
    ) or usage (1);

$opt_v and say "$0 @ARGV";

# anon-list contains all possible commands to show content
# plain text is a reference to same type (alias)
# %f will be replaced with file. If no %f, file will be the last arg
my %fh = (
    text => {
	bin	=> [ "strings"		], # fallback for binary files

	txt	=> [ "cat"		], # Plain text

	html	=> [ "htm2txt",
		     "html2text"	], # HTML

	msword	=> "doc",
	doc	=> [ "catdoc -x -dutf-8",
		     "wvText",
		     "antiword -w 72"	], # M$ Word
	"vnd.ms-excel" => "xls",
	"ms-excel"     => "xls",
	docx	=> [ "unoconv -f text --stdout"	], # MS Word
	xlsx	=> "xls",
	xls	=> [ "xlscat -L",
		     "catdoc -x -dutf-8",
		     "wvText"		], # M$ Excel
#	ppt	=> [ "ppthtml"		], # M$ PowerPoint
#			ppthtml "$1" | html2text
	csv	=> "xls",		   # Comma Separated Values

	ics	=> [ "ics2txt"		], # ICS calendar request

	rtf	=> [ "rtf2text",
		     "unrtf -t text"	], # RTF
	pdf	=> [ "pdftotext %f -"	], # Adobe PDF

	ods	=> "xls",		   # OpenOffice spreadsheet
	sxc	=> "xls",		   # OpenOffice spreadsheet
	odt	=> [ "oo2pod %f | pod2text",
		     "ooo2txt"		], # OpenOffice writer
	rtf	=> [ "rtf2text"		], # RTF

	pl	=> [ "perltidy -st -se",
		     "cat"		], # Perl
	pm	=> "pl",

	jsn	=> [ "json_pp"		], # JSON
	json	=> "jsn",

	xml	=> [ "xml_pp"		], # XML

	( map { $_ => "txt" } qw(
	    patch diff
	    c h ic ec cc
	    sh sed awk
	    plain
	    yml yaml
	    )),

	bz2	=> [ "bzip2 -d < %f | strings" ],

	zip	=> [ "unzip -l %f"	], # ZIP

	test	=> [ \&test		], # Internal

	tgz	=> [ "tar tvf"		], # Tar     uncompressed
	tgz	=> [ "tar tzvf"		], # Tar GZ    compressed
	tbz	=> [ "tar tjvf"		], # Tar BZip2 compressed
	txz	=> [ "tar tJvf"		], # Tar XZ    compressed

	rar	=> [ "unrar l"		], # RAR
	},

    html => {
	rtf	=> [ "rtf2html"		],
	},
    );

if ($opt_l) {
    my %tc = %{$fh{text}};
    foreach my $ext (sort keys %tc) {
	my $exe = $tc{$ext};
	ref $exe or $exe = $tc{$exe};
	printf "  .%-12s %s\n", $ext, $_ for @$exe;
	}
    exit 0;
    }

my $file = shift or usage (1, "File argument is missing");
-f $file         or usage (1, "File argument is not a plain file");
-r $file         or usage (1, "File argument is not a readable file");
-s $file         or usage (1, "File argument is an empty file");

my $ext = $file =~ m/\.(\w+)$/ ? lc $1 : "";
$opt_t && exists $fh{text}{lc $opt_t} and $ext = lc$opt_t;
unless (exists $fh{text}{$ext}) {
    my $ftype = `file --brief $file`;
    $ext =
	$ftype =~ m/^pdf doc/i					? "pdf" :
	$ftype =~ m/^ascii( english)? text/i			? "txt" :
	$ftype =~ m/^(utf-8 unicode|iso-\d+)( english)? text/i	? "txt" :
	$ftype =~ m/^xml doc/i					? "xml" :
	$ftype =~ m/^\w+ compress/i				? "bin" :
								  "bin" ;
    # \w+ archive
    # \w+ image
    # ...
    }
$ext ||= "txt";
exists $fh{$opt_h}{$ext} or $opt_h = "text";
exists $fh{$opt_h}{$ext} or $ext   = "txt";
my          $ref = $fh{$opt_h}{$ext};
ref $ref or $ref = $fh{$opt_h}{$ref};

$opt_v and warn "[ @$ref ] $file\n";

sub which {
    (my $cmd = shift) =~ s/\s.*//; # Only the command. Discard arguments here
    foreach my $path (split m/:+/, $ENV{PATH}) {
	-x "$path/$cmd" and return "$path/$cmd";
	}
    return 0;
    } # which

my $cmd = "cat -ve";
foreach my $c (@$ref) {
    if (ref $c) {
	$c->($file);
	exit;
	}

    my $cp = which ($c) or next;
    $cmd = $c;
    last;
    }

my @cmd = split m/ +/ => $cmd;
grep { s/%f\b/$file/ } @cmd or push @cmd, $file;
#$cmd =~ s/%f\b/$file/g or $cmd .= " $file";
$opt_v and say "@cmd";
exec @cmd;
