/*
 * Sylpheed -- regexp pattern matching utilities
 * Copyright (C) 2001 Thomas Link, Hiroyuki Yamamoto
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <glib.h>
#include "string_match.h"


int string_match_regexp(char * txt, char * rexp,
			int size, regmatch_t matches[],
			int cflags, int eflags)
{
	regex_t re;
	int problem;

	g_return_val_if_fail(txt, 0);
	g_return_val_if_fail(rexp, 0);

	problem = regcomp(&re, rexp, cflags);  
	if (problem == 0) {
		problem = regexec(&re, txt, size, matches, eflags);
	}
	regfree(&re);
	return problem == 0;
}

int string_remove_match(char * txt, char * rexp, int cflags, int eflags)
{
	regmatch_t matches[STRING_MATCH_MR_SIZE];
	int foundp;

	g_return_val_if_fail(txt, -1);
	g_return_val_if_fail(rexp, -1);

	if (strlen(txt) > 0 && strlen(rexp) > 0) {
		foundp = string_match_regexp(txt, rexp, STRING_MATCH_MR_SIZE, 
					     matches, cflags, eflags);
	} else {
		return -1;
	}

	if (foundp && matches[0].rm_so != -1 && matches[0].rm_eo != -1) {
		if (matches[0].rm_so == matches[0].rm_eo) {
			/* in case the match is an empty string */
			return matches[0].rm_so + 1;
		} else {
			strcpy(txt + matches[0].rm_so, txt + matches[0].rm_eo);
			return matches[0].rm_so;
		}
	} else {
		return -1;
	}
}

int string_remove_all_matches(char * txt, char * rexp, int cflags, int eflags)
{
	int pos = 0;
	int pos0 = pos;

	g_return_val_if_fail(txt, 0);
	g_return_val_if_fail(rexp, 0);

	while (pos0 >= 0 && pos < strlen(txt) && strlen(txt) > 0) {
		/* printf("%s %d:%d\n", txt, pos0, pos); */
		pos0 = string_remove_match(txt + pos, rexp, cflags, eflags);
		if (pos0 >= 0) {
			pos = pos + pos0;
		}
	}
	return pos;
}
