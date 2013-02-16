/**
 * @file common.c common routines for Liferea
 * 
 * Copyright (C) 2003-2005  Lars Lindner <lars.lindner@gmx.net>
 * Copyright (C) 2004,2005  Nathan J. Conrad <t98502@users.sourceforge.net>
 * Copyright (C) 2004       Karl Soderstrom <ks@xanadunet.net>
 *
 * parts of the RFC822 timezone decoding were taken from the gmime 
 * source written by 
 *
 * Authors: Michael Zucchi <notzed@helixcode.com>
 *          Jeffrey Stedfast <fejj@helixcode.com>
 *
 * Copyright 2000 Helix Code, Inc. (www.helixcode.com)
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

/* this is needed for strptime() */
#if !defined (__FreeBSD__)
#define _XOPEN_SOURCE 600 /* glibc2 needs this */
#else
#define _XOPEN_SOURCE
#endif

#ifdef USE_PTHREAD
#include <pthread.h>
#endif

#include <time.h>
#include <glib.h>
#include <locale.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#include "procheader.h"

/* converts a ISO 8601 time string to a time_t value */
time_t parseISO8601Date(gchar *date) {
	struct tm	tm;
	time_t		t, t2, offset = 0;
	gboolean	success = FALSE;
#ifdef G_OS_WIN32
	gchar *tmp = g_strdup(date);
	gint result, year, month, day, hour, minute, second;
#else
	gchar *pos;
#endif	
	g_assert(date != NULL);
	
	memset(&tm, 0, sizeof(struct tm));
	
#ifdef G_OS_WIN32
	g_strstrip(tmp);
	result = sscanf((const char *)date, "%d-%d-%dT%d:%d:%d", 
			&year, &month, &day, &hour, &minute, &second);
	if (result < 6)
		second = 0;
	if (result < 5)
		minute = 0;
	if (result < 4)
		hour = 0;
	if (result >= 3) {
		tm.tm_sec = second;
		tm.tm_min = minute;
		tm.tm_hour = hour;
		tm.tm_mday = day;
		tm.tm_mon = month - 1;
		tm.tm_year = year - 1900;
		tm.tm_wday = 0;
		tm.tm_yday = 0;
		tm.tm_isdst = -1;
		success = TRUE;
	}
#else
	/* we expect at least something like "2003-08-07T15:28:19" and
	   don't require the second fractions and the timezone info

	   the most specific format:   YYYY-MM-DDThh:mm:ss.sTZD
	 */
	 
	/* full specified variant */
	if(NULL != (pos = strptime((const char *)date, "%t%Y-%m-%dT%H:%M%t", &tm))) {
		/* Parse seconds */
		if (*pos == ':')
			pos++;
		if (isdigit(pos[0]) && !isdigit(pos[1])) {
			tm.tm_sec = pos[0] - '0';
			pos++;
		} else if (isdigit(pos[0]) && isdigit(pos[1])) {
			tm.tm_sec = 10*(pos[0]-'0') + pos[1] - '0';
			pos +=2;
		}
		/* Parse timezone */
		if (*pos == 'Z')
			offset = 0;
		else if ((*pos == '+' || *pos == '-') && isdigit(pos[1]) && isdigit(pos[2]) && strlen(pos) >= 3) {
			offset = (10*(pos[1] - '0') + (pos[2] - '0')) * 60 * 60;
			
			if (pos[3] == ':' && isdigit(pos[4]) && isdigit(pos[5]))
				offset +=  (10*(pos[4] - '0') + (pos[5] - '0')) * 60;
			else if (isdigit(pos[3]) && isdigit(pos[4]))
				offset +=  (10*(pos[3] - '0') + (pos[4] - '0')) * 60;
			
			offset *= (pos[0] == '+') ? 1 : -1;

		}
		success = TRUE;
	/* only date */
	} else if(NULL != strptime((const char *)date, "%t%Y-%m-%d", &tm))
		success = TRUE;
	/* there were others combinations too... */
#endif
	if(TRUE == success) {
		if((time_t)(-1) != (t = mktime(&tm))) {
			struct tm buft;
			/* Correct for the local timezone*/
			t = t - offset;
			t2 = mktime(gmtime_r(&t, &buft));
			t = t - (t2 - t);
			
			return t;
		} else {
			g_warning("internal error! time conversion error! mktime failed!\n");
		}
	} else {
		g_warning("Invalid ISO8601 date format! Ignoring <dc:date> information!\n");
	}
	
	return 0;
}

gchar *dayofweek[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
gchar *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

gchar *createRFC822Date(const time_t *time) {
	struct tm *tm;
	struct tm buft;
#ifdef G_OS_WIN32
	if (*time < 0) {
		time_t t = 1;
		tm = gmtime_r(&t, &buft);
	} else 
#endif
	{
		tm = gmtime_r(time, &buft); /* No need to free because it is statically allocated */
	}
	return g_strdup_printf("%s, %2d %s %4d %02d:%02d:%02d GMT", dayofweek[tm->tm_wday], tm->tm_mday,
					   months[tm->tm_mon], 1900 + tm->tm_year, tm->tm_hour, tm->tm_min, tm->tm_sec);
}
