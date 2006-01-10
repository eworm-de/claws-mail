#!/usr/bin/perl -w

use strict;

#  * This file is free software; you can redistribute it and/or modify it
#  * under the terms of the GNU General Public License as published by
#  * the Free Software Foundation; either version 2 of the License, or
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
#  * Copyright 2006 Paul Mangan <claws@thewildbeast.co.uk>
#  *

#
# Convert new style Sylpheed filter rules (Sylpheed >= 0.9.99) to
# Sylpheed-Claws filtering rules
#

#
# TABLE OF EQUIVALENTS
#
# SYLPHEED			:	SYLPHEED-CLAWS
#------------------------------------------------------
#
# NAME
#
# name				:	rulename
#
# CONDITION LIST
#
# bool or			:	|
# bool and			:	&
#
# match-header (name From)	:	from
# match-header (name To)	:	to
# match-header (name Cc)	:	cc
# match-header (name Subject)	:	subject
# 	else...
# match-header			:	header
#
# match-header (type contains)	:	[nothing]
# match-header (type not-contain) : 	[append with ~]
# match-header (type is)	:	[no equivalent]	(use type contains)
# match-header (type is-not)	:	[no equivalent] (use type not-contain)
# match-header (type regex)	:	regexpcase
# match-header (type not-regex)	:	regexpcase [append with ~]
#
# matcher-any-header		;	headers-part
# match-to-or-cc		:	to_or_cc
# match-body-text		:	body_part
# command-test			:	test
# size	(type gt)		:	size_greater
# size (type lt)		:	size_smaller
# age (type gt)			:	age_greater
# age (type lt)			:	age_lower	
#
# ACTION LIST
#
# move				:	move
# copy				:	copy
# not-receive			:	[no equivalent] (use type delete)
# delete			:	delete
# mark				:	mark
# color-label			:	color
# mark-as-read			:	mark_as_read
# exec				:	execute
# stop-eval			:	stop
#

use XML::SimpleObject;

my $old_config = "$ENV{HOME}/.sylpheed-2.0/filter.xml";
my $config_dir = `sylpheed-claws --config-dir`;
chomp $config_dir;

chdir($ENV{ HOME } . "/$config_dir")
	or die("You don't appear to have Sylpheed-Claws installed\n");

-e $old_config or die("Can't find old filters [$old_config]\n");

my $parser = XML::Parser->new(ErrorContext => 2, Style => "Tree");
my $xmlobj = XML::SimpleObject->new($parser->parsefile($old_config));

my @conditions = ('match-header','match-to-or-cc','match-any-header',
		  'match-body-text','command-test','size','age');

my @actions = ('move','copy','not-receive','delete','mark','color-label',
	       'mark-as-read','exec','stop-eval');

my $standard_headers = qr/^(?:Subject|From|To|Cc)$/;
my $negative_matches = qr/^(?:not-contain|is-not|not-regex)$/;
my $numeric_matches = qr/^(?:size|age)$/;
my $exact_matches = qr/^(?:move|copy|delete|mark)$/;

my @new_filters = ("[filtering]");

my $bool;

## rules list
foreach my $element ($xmlobj->child("filter")->children("rule")) {
	my $new_filter;
	if ($element->attribute("name")) {
		my $name = $element->attribute("name");
		$name = clean_me($name);
		$new_filter = "\nrulename \"$name\" ";
    	}
	if ($element->attribute("enabled")) {
		if ($element->attribute("enabled") eq "false") {
			next;	# skip disabled rules
		}
    	}
## condition list
	foreach my $parent ($element->children("condition-list")) {
		if ($parent->attribute("bool")) {
			$bool = $parent->attribute("bool");
			$bool =~ s/or/|/;
			$bool =~ s/and/&/;
		}
		foreach my $condition (@conditions) {
			my $new_condition = 0;
			my $type;
			if ($parent->children("$condition")) {
				foreach my $sibling ($parent->children("$condition")) {
					if ($new_condition) {
						$new_filter .= " $bool ";
					}
					if ($sibling->attribute("type")) {
						$type = $sibling->attribute("type");
						if ($type =~ m/$negative_matches/) {
							$new_filter .= '~';
						}
					}
					if ($sibling->attribute("name")) {
						my $name = $sibling->attribute("name");
						if ($condition eq "match-header") {
							if ($name =~ m/$standard_headers/) {
								$new_filter .= lc($name) . " ";
							} else {
								$new_filter .= "header \"$name\" ";
							}
						}
					}
					if ($condition eq "match-any-header") {
						$new_filter .= "headers_part ";
					} elsif ($condition eq "match-to-or-cc") {
						$new_filter .= "to_or_cc ";
					} elsif ($condition eq "match-body-text") {
						$new_filter .= "body_part ";
					} elsif ($condition eq "command-test") {
						$new_filter .= "test ";
					} elsif ($condition eq "size") {
						if ($type eq "gt") {
							$new_filter .= "size_greater ";
						} else {
							$new_filter .= "size_smaller ";
						}
					} elsif ($condition eq "age") {
						if ($type eq "gt") {
							$new_filter .= "age_greater ";
						} else {
							$new_filter .= "age_lower ";
						}
					}
					if ($condition !~ m/$numeric_matches/ &&
					    $condition ne "command-test") {
						if ($type =~ m/regex/) {
							$new_filter .= "regexpcase ";
						} else {
							$new_filter .= "matchcase ";
						}
					}
					my $value = $sibling->value;
					$value = clean_me($value);
					if ($condition =~ m/$numeric_matches/) {
						$new_filter .= "$value";
					} else {
						$new_filter .= "\"$value\"";
					}
					$new_condition++;
				}
			}
		}
	}
## end of condition list
## action list
	foreach my $parent ($element->children("action-list")) {
		foreach my $action (@actions) {
			if ($parent->children("$action")) {
				foreach my $sibling ($parent->children("$action")) {
					if ($action  =~ m/$exact_matches/) {
						$new_filter .= " $action";
					} elsif ($action eq "not-receive") {
						$new_filter .= " delete";
					} elsif ($action eq "color-label") {
						$new_filter .= " color";
					} elsif ($action eq "mark-as-read") {
						$new_filter .= " mark_as_read";
					} elsif ($action eq "exec") {
						$new_filter .= " execute";
					} elsif ($action eq "stop-eval") {
						$new_filter .= " stop";
					}
					if ($sibling->value) {
						my $value = $sibling->value;
						$value = clean_me($value);
						if ($action eq "color-label") {
							$new_filter .= " $value";
						} else {
							$new_filter .= " \"$value\"";
						}
					}
				}
			}
		}
	}
## end of action list
	push(@new_filters, $new_filter) if (defined($new_filter));
}
## end of rules list
push(@new_filters, "\n");

# write new config
open(MATCHERRC, ">>matcherrc");
	print MATCHERRC @new_filters;
close(MATCHERRC);

print "Converted ". ($#new_filters-1) . " filters\n";

exit;

sub clean_me {
	my ($dirty) = @_;

	$dirty =~ s/\"/\\\"/g;
	$dirty =~ s/\n/ /g;

	return $dirty;
}

