/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2006 Hiroyuki Yamamoto and the Claws Mail team
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

#ifndef LOG_H
#define LOG_H

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>

#define LOG_APPEND_TEXT_HOOKLIST "log_append_text"
#define LOG_TIME_LEN 11

typedef enum
{
	LOG_NORMAL,
	LOG_MSG,
	LOG_WARN,
	LOG_ERROR
} LogType;

typedef struct _LogText LogText;

struct _LogText
{
	gchar		*text;
	LogType		 type;	
};

/* logging */
void set_log_file	(const gchar *filename);
void close_log_file	(void);
void log_verbosity_set	(gboolean verbose);
void log_print		(const gchar *format, ...) G_GNUC_PRINTF(1, 2);
void log_message	(const gchar *format, ...) G_GNUC_PRINTF(1, 2);
void log_warning	(const gchar *format, ...) G_GNUC_PRINTF(1, 2);
void log_error		(const gchar *format, ...) G_GNUC_PRINTF(1, 2);

#endif /* LOG_H */
