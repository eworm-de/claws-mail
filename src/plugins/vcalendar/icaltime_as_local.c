/* Taken as is from http://dev.w3.org/cvsweb/Ical2html/icaltime_as_local.c
W3C® SOFTWARE NOTICE AND LICENSE


Copyright © 1994-2002 World Wide Web Consortium, (Massachusetts
Institute of Technology, Institut National de Recherche en
Informatique et en Automatique, Keio University). All Rights
Reserved. http://www.w3.org/Consortium/Legal/

This W3C work (including software, documents, or other related items)
is being provided by the copyright holders under the following
license. By obtaining, using and/or copying this work, you (the
licensee) agree that you have read, understood, and will comply with
the following terms and conditions:

Permission to use, copy, modify, and distribute this software and its
documentation, with or without modification, for any purpose and
without fee or royalty is hereby granted, provided that you include
the following on ALL copies of the software and documentation or
portions thereof, including modifications, that you make:

The full text of this NOTICE in a location viewable to users of the
redistributed or derivative work.  Any pre-existing intellectual
property disclaimers, notices, or terms and conditions. If none exist,
a short notice of the following form (hypertext is preferred, text is
permitted) should be used within the body of any redistributed or
derivative code: "Copyright © [$date-of-software] World Wide Web
Consortium, (Massachusetts Institute of Technology, Institut National
de Recherche en Informatique et en Automatique, Keio University). All
Rights Reserved. http://www.w3.org/Consortium/Legal/" Notice of any
changes or modifications to the W3C files, including the date changes
were made. (We recommend you provide URIs to the location from which
the code is derived.)

THIS SOFTWARE AND DOCUMENTATION IS PROVIDED "AS IS," AND COPYRIGHT
HOLDERS MAKE NO REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED,
INCLUDING BUT NOT LIMITED TO, WARRANTIES OF MERCHANTABILITY OR FITNESS
FOR ANY PARTICULAR PURPOSE OR THAT THE USE OF THE SOFTWARE OR
DOCUMENTATION WILL NOT INFRINGE ANY THIRD PARTY PATENTS, COPYRIGHTS,
TRADEMARKS OR OTHER RIGHTS.

COPYRIGHT HOLDERS WILL NOT BE LIABLE FOR ANY DIRECT, INDIRECT, SPECIAL
OR CONSEQUENTIAL DAMAGES ARISING OUT OF ANY USE OF THE SOFTWARE OR
DOCUMENTATION.

The name and trademarks of copyright holders may NOT be used in
advertising or publicity pertaining to the software without specific,
written prior permission. Title to copyright in this software and any
associated documentation will at all times remain with copyright
holders.

____________________________________

This formulation of W3C's notice and license became active on August
14 1998 so as to improve compatibility with GPL. This version ensures
that W3C software licensing terms are no more restrictive than GPL and
consequently W3C software may be distributed in GPL packages. See the
older formulation for the policy prior to this date. Please see our
Copyright FAQ for common questions about using materials from our
site, including specific terms and conditions for packages like
libwww, Amaya, and Jigsaw. Other questions about this notice can be
directed to site-policy@w3.org.
*/
/*
 * Convert a date/time in UTC to a date/time in the local time zone.
 * (Seems to be missing from libical v 0.23)
 *
 * Author: Bert Bos <bert@w3.org>
 * Created: 18 Dec 2002
 * Version: $Id$
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#include "claws-features.h"
#endif

#include <stddef.h>
#include <glib.h>
#include <glib/gi18n.h>

#ifdef USE_PTHREAD
#include <pthread.h>
#endif

#include "icaltime_as_local.h"

#if !HAVE_ICALTIME_AS_LOCAL

struct icaltimetype icaltime_as_local(struct icaltimetype tt)
{
  time_t t;
  struct tm *tm;
  struct icaltimetype h;
  struct tm buft;

  t = icaltime_as_timet(tt);                    /* Convert to epoch */
#ifdef G_OS_WIN32
  if (t < 0)
	  t = 1;
#endif
  tm = localtime_r(&t, &buft);                  /* Convert to local time */
  h.year = tm->tm_year + 1900;                  /* Make an icaltimetype */
  h.month = tm->tm_mon + 1;
  h.day = tm->tm_mday;
  h.hour = tt.is_date ? 0 : tm->tm_hour;
  h.minute = tt.is_date ? 0 : tm->tm_min;
  h.second = tt.is_date ? 0 : tm->tm_sec;
  h.is_utc = 0;
  h.is_date = tt.is_date;
  return h;
}

#endif

