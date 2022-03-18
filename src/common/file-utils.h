/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2018 Colin Leroy and the Claws Mail team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __CLAWS_IO_H__
#define __CLAWS_IO_H__

#ifdef HAVE_CONFIG_H
#include "claws-features.h"
#endif

#include <stdio.h>
#include <glib.h>

#if HAVE_FGETS_UNLOCKED
#define claws_fgets fgets_unlocked
#define claws_fgetc fgetc_unlocked
#define claws_fputs fputs_unlocked
#define claws_fputc fputc_unlocked
#define claws_fread fread_unlocked
#define claws_fwrite fwrite_unlocked
#define claws_feof feof_unlocked
#define claws_ferror ferror_unlocked

FILE *claws_fopen		(const char *file,
				 const char *mode);
FILE *claws_fdopen		(int fd,
				 const char *mode);
int claws_fclose		(FILE *fp);

#else

#define claws_fgets fgets
#define claws_fgetc fgetc
#define claws_fputs fputs
#define claws_fputc fputc
#define claws_fread fread
#define claws_fwrite fwrite
#define claws_feof feof
#define claws_ferror ferror
#define claws_fopen g_fopen
#define claws_fdopen fdopen
#define claws_fclose fclose
#endif

int claws_safe_fclose		(FILE *fp);
int claws_unlink		(const char	*filename);

gint file_strip_crs		(const gchar	*file);
gint append_file		(const gchar	*src,
				 const gchar	*dest,
				 gboolean	 keep_backup);
gint copy_file			(const gchar	*src,
				 const gchar	*dest,
				 gboolean	 keep_backup);
gint move_file			(const gchar	*src,
				 const gchar	*dest,
				 gboolean	 overwrite);
gint copy_file_part_to_fp	(FILE		*fp,
				 off_t		 offset,
				 size_t		 length,
				 FILE		*dest_fp);
gint copy_file_part		(FILE		*fp,
				 off_t		 offset,
				 size_t		 length,
				 const gchar	*dest);
gint canonicalize_file		(const gchar	*src,
				 const gchar	*dest);
gint canonicalize_file_replace	(const gchar	*file);
gchar *file_read_to_str		(const gchar	*file);
gchar *file_read_to_str_no_recode(const gchar	*file);
gchar *file_read_stream_to_str	(FILE		*fp);
gchar *file_read_stream_to_str_no_recode(FILE	*fp);

gint rename_force		(const gchar	*oldpath,
				 const gchar	*newpath);
gint copy_dir			(const gchar	*src,
				 const gchar	*dest);
gint change_file_mode_rw	(FILE		*fp,
				 const gchar	*file);
FILE *my_tmpfile		(void);
FILE *get_tmpfile_in_dir	(const gchar 	*dir,
				 gchar	       **filename);
FILE *str_open_as_stream	(const gchar	*str);
gint str_write_to_file		(const gchar	*str,
				 const gchar	*file,
				 gboolean	 safe);

#endif
