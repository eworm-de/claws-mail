#!/usr/bin/perl
# convert_mbox.pl
# perl script to convert mbox file to files in a new MH directory
# aka another mbox -> MH conversion tool
# 29 April 2003  
# Fred Marton <Fred.Marton@uni-bayreuth.de>
# 
# Fixed (hopefully) to account for From lines
# that are of various length and that might have
# time zone info at the end
# 20 January 2004
#
# Note: Running this with the -w flag generates the following warnings:
# Scalar value @word[1] better written as $word[1] at /path/to/convert_mbox.pl line 54
# Scalar value @word[0] better written as $word[1] at /path/to/convert_mbox.pl line 56
# Making these changes requires further changes in the script
# that results in much longer run-times.  
#
# Copyright © 2003 Fred Marton
# 
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 3
# of the License, or (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
# 02111-1307, USA.

# check for both arguments
&usage if ($#ARGV < 1);
$mbox = $ARGV[0];
$mh = $ARGV[1];
# check to make sure there isn't something named MH already
if (-e $mh) {
   die (" The directory \"$mh\" already exists.  Exiting.\n");
}
else {
   mkdir $mh;
}
# start numbering
$i = 0;
# open the mbox file
open (IN, $mbox);
while ($line = <IN>) {
# check for the beginning of an e-mail
   @word = split(/ +/m,$line);
# some lines might start with "From ", so check
# to see if the [second-to-]last word is a year
   @word2 = split(/:/,$line);
   chomp($word2[$#word2]);
   @word3 = split(/ /,$word2[2]);
   $year = @word3[1];
# ignore the MAILER-DAEMON message from pine
   if (@word[1] ne "MAILER-DAEMON") {
# start a new file, assuming $year is > 1970
      if (@word[0] eq "From" && $year > 1970) {
         $i++;
         close (OUT);
         open (OUT, ">$mh/$i");
         print OUT $line;
      }
      else {
# continue the file
         print OUT $line;
      }
   }
}
close (OUT);
close (IN);
# and we're done
print "\n If it isn't there already, please move the directory \"$mh\"\n"
    . " into your MH directory and rebuild your folder tree.\n\n";

sub usage
{
   die ( " usage: convert_mbox.pl MBOX MH_DIR\n");
}
