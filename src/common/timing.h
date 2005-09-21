/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2005 Colin Leroy <colin@colino.net> & the Sylpheed-Claws team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/*
 * This is a (quite naive) timer, to help determine the speed of various
 * functions of Claws. By default START_TIMING() and END_TIMING() are NOPS,
 * so that nothing gets printed out. If you change the #if, however, you'll
 * be able to get functions timing information. As the implementation is
 * naive, START_TIMING("message"); must be present just at the end of a
 * declaration block (or compilation would fail with gcc 2.x), and the
 * END_TIMING() call must be in the same scope.
 */
#ifndef __TIMING_H__
#define __TIMING_H__

#include <sys/time.h>
#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#if 1 /* set to 0 to measure times at various places */
#define START_TIMING(str) do {} while(0);
#define END_TIMING() do {} while(0);
#else
/* no {} by purpose */
#define START_TIMING(str) 						\
	struct timeval start;						\
	struct timeval end;						\
	struct timeval diff;						\
	const char *timing_name=str;					\
	gettimeofday(&start, NULL);					\

#define END_TIMING()							\
	gettimeofday(&end, NULL);					\
	timersub(&end, &start, &diff);					\
	printf("%s: %ds%dus\n", 					\
		timing_name, (unsigned int)diff.tv_sec, 		\
		(unsigned int)diff.tv_usec);				\

#endif

#endif 
