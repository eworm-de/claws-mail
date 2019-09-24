#!/usr/bin/perl

#  * Copyright 2003-2019 Paul Mangan <paul@claws-mail.org>
#  *
#  * This file is free software; you can redistribute it and/or modify it
#  * under the terms of the GNU General Public License as published by
#  * the Free Software Foundation; either version 3 of the License, or
#  * (at your option) any later version.
#  *
#  * This program is distributed in the hope that it will be useful, but
#  * WITHOUT ANY WARRANTY; without even the implied warranty of
#  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#  * General Public License for more details.
#  *
#  * You should have received a copy of the GNU General Public License
#  * along with this program; if not, write to the Free Software
#  * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#  *

# Changes:
#	Feb 2007: add support for non ISO-8859-1 compatible locales
#		  by Alex Gorbachenko <agent_007@immo.ru>
#

use URI::Escape;
use POSIX qw(locale_h);
use locale;
use Text::Iconv;

my $ddg = "https://duckduckgo.com/?q";
$_ = <>;

$locale = setlocale(LC_CTYPE);
$locale =~ s/\S+\.//;

$converter = Text::Iconv->new("$locale", "UTF-8");
$safe=uri_escape($converter->convert("$_"));

my $config_dir = `claws-mail --config-dir` or die("ERROR:
	You don't appear to have Claws Mail installed\n");
chomp $config_dir;

chdir($ENV{HOME} . "/$config_dir") or die("ERROR:
	Claws Mail config directory not found [~/$config_dir]
	You need to run Claws Mail once, quit it, and then rerun this script\n");

open (CMRC, "<clawsrc") || die("Can't open the clawsrc file\n");
	@rclines = <CMRC>;
close CMRC;

foreach $rcline (@rclines) {
	if ($rcline =~ m/^uri_open_command/) {
		chomp $rcline;
		@browser = split(/=/, $rcline);
		$browser[1] =~ s/%s/$ddg=$safe/;
	}
}
system("$browser[1]&");

exit;


