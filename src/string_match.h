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

#ifndef STRING_MATCH_H__
#define STRING_MATCH_H__

/* remove substring matching REXP from TXT. Destructive! */
/* for documentation of CFLAGS and EFLAGS see "man regex" */
/* returns -1 = not found; N = next find pos */
/* if the match is an empty string (e.g. "x\?"), the find position will
   be increased by 1 */
int string_remove_match	(char *txt, char *rexp, int cflags, int eflags);

/* remove all substrings matching REXP from TXT. Destructive! */
/* for documentation of CFLAGS and EFLAGS see "man regex" */
/* returns position of last replacement (i.e. TXT has been modified up
   to this position, use this as the starting point for further mangling) */
int string_remove_all_matches	(char *txt, char *rexp, int cflags, int eflags);


#endif /* STRING_MATCH_H__ */

