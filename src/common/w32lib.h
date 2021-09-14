/* w32lib.h  - Posix emulation layer for Sylpheed (Claws)
 *
 * This file is part of w32lib.
 *
 * w32lib is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * w32lib is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * The code has been taken from the package
 *    http://claws.sylpheed.org/win32/sylpheed/w32lib-dev-2004.2.3.0.zip
 * on 2005-11-17 by Werner Koch <wk@gnupg.org>. There are no regular
 * copyright notices but the file version.rc from the ZIP archive
 * claims:
 *
 *     #define FILEVER "2004.2.3.0\0"
 *     #define PRODVER "2004.2.3\0"
 *
 *     [...]
 *      VALUE "FileDescription", "Posix emulation layer for Sylpheed (Claws)\0"
 *      VALUE "FileVersion", FILEVER
 *      VALUE "ProductVersion", PRODVER
 *      VALUE "LegalCopyright", "GPL\0"
 *      VALUE "CompanyName", "GNU / Free Software Foundation\0"
 *      VALUE "ProductName", "w32lib\0"
 *
 * Along with the fact that Sylpheed is under the GPL we can assume
 * that this code is under the GPL.  No author information or
 * changelogs have been found.
 * Files taken form the package are:
 *    w32_dirent.c w32_reg.c w32_stat.c w32_stdlib.c w32_time.c w32_wait.c
 *    w32_gettext.c w32_signal.c w32_stdio.c w32_string.c w32_unistd.c
 */

/* Changes are:

2007-05-21  Werner Koch  <wk@g10code.com>

	* src/common/w32_account.c: New.

	* src/common/w32lib.h: Undef "interface".

2005-11-17  Werner Koch  <wk@g10code.com>

	Add boilerplate text to all files and explain legal status.

	* w32_reg.c: Replaced g_free and g_strdup by regular C functions.
	(get_content_type_from_registry_with_ext): Ditto.
	* w32_dirent.c (readdir): Ditto. 
	(opendir): Ditto.
	(closedir): Reformatted.
	(readdir): Reformatted, replaced use of g_strdup_printf and other
	g-style malloc function by regular ones.  Use DIR structure from mingw.
        * w32lib.h: Don't define finddata_t for mingw. Replaced replacement
        DIR structure by the one form mingw.  Allocate filename in dirent
        statically to match the defintion ussed by mingw.
	* w32_reg.c (read_w32_registry_string): Return error for invalid root
        key.

  */


#ifndef _W32LIB_H_
#define _W32LIB_H_

#include <windows.h>

/*** misc ***/
int write_w32_registry_string( char *parent, char *section, char *value, char *data );
int write_w32_registry_dword( char *parent, char *section, char *value, int data );
char *read_w32_registry_string( char *parent, char *section, char *key );
char *get_content_type_from_registry_with_ext( char *ext );

#endif
