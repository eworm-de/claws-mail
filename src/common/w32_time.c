/* w32_time.c  - Posix emulation layer for Sylpheed (Claws)
 *
 * This file is part of w32lib.
 *
 * w32lib is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * w32lib is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,
 * USA.
 *
 * For more information and a list of changes, see w32lib.h
 */

#include <stdlib.h>
#include <string.h>
#include <sys/timeb.h>

#include "w32lib.h"

int gettimeofday( struct timeval *tv, struct timezone *tz ){
  struct _timeb tstruct;
  _ftime( &tstruct );
  tv->tv_sec = tstruct.time ;
  tv->tv_usec = tstruct.millitm;
  return 1;
}
