/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2002 Hiroyuki Yamamoto
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "defs.h"

#include <stdio.h>
#include <glib.h>

#include "utils.h"
#include "log.h"
#include "hooks.h"

static FILE *log_fp = NULL;

gboolean invoke_hook_cb (gpointer data)
{
	LogText *logtext = (LogText *)data;
	hooks_invoke(LOG_APPEND_TEXT_HOOKLIST, logtext);
	g_free(logtext->text);
	g_free(logtext);
	return FALSE;
}

void set_log_file(const gchar *filename)
{
	if (log_fp)
		return;

	/* backup old logfile if existing */
	if (is_file_exist(filename)) {
		gchar *backupname;
		
		backupname = g_strconcat(filename, ".bak", NULL);
		if (rename(filename, backupname) < 0)
			FILE_OP_ERROR(filename, "rename");
		g_free(backupname);
	}

	log_fp = fopen(filename, "wb");
	if (!log_fp)
		FILE_OP_ERROR(filename, "fopen");
}

void close_log_file(void)
{
	if (log_fp) {
		fclose(log_fp);
		log_fp = NULL;
	}
}

void log_print(const gchar *format, ...)
{
	va_list args;
	gchar buf[BUFFSIZE + LOG_TIME_LEN];
	time_t t;
	LogText *logtext = g_new0(LogText, 1);
	
	time(&t);
	strftime(buf, LOG_TIME_LEN + 1, "[%H:%M:%S] ", localtime(&t));

	va_start(args, format);
	g_vsnprintf(buf + LOG_TIME_LEN, BUFFSIZE, format, args);
	va_end(args);

	if (debug_get_mode()) fputs(buf, stdout);

	logtext->text = g_strdup(buf);
	logtext->type = LOG_NORMAL;
	
	g_timeout_add(0, invoke_hook_cb, logtext);
	
	if (log_fp) {
		fputs(buf, log_fp);
		fflush(log_fp);
	}
}

void log_message(const gchar *format, ...)
{
	va_list args;
	gchar buf[BUFFSIZE + LOG_TIME_LEN];
	time_t t;
	LogText *logtext = g_new0(LogText, 1);

	time(&t);
	strftime(buf, LOG_TIME_LEN + 1, "[%H:%M:%S] ", localtime(&t));

	va_start(args, format);
	g_vsnprintf(buf + LOG_TIME_LEN, BUFFSIZE, format, args);
	va_end(args);

	if (debug_get_mode()) g_message("%s", buf + LOG_TIME_LEN);
	logtext->text = g_strdup(buf + LOG_TIME_LEN);
	logtext->type = LOG_MSG;
	
	g_timeout_add(0, invoke_hook_cb, logtext);

	if (log_fp) {
		fwrite(buf, LOG_TIME_LEN, 1, log_fp);
		fputs("* message: ", log_fp);
		fputs(buf + LOG_TIME_LEN, log_fp);
		fflush(log_fp);
	}
}

void log_warning(const gchar *format, ...)
{
	va_list args;
	gchar buf[BUFFSIZE + LOG_TIME_LEN];
	time_t t;
	LogText *logtext = g_new0(LogText, 1);

	time(&t);
	strftime(buf, LOG_TIME_LEN + 1, "[%H:%M:%S] ", localtime(&t));

	va_start(args, format);
	g_vsnprintf(buf + LOG_TIME_LEN, BUFFSIZE, format, args);
	va_end(args);

	g_warning("%s", buf);
	logtext->text = g_strdup(buf + LOG_TIME_LEN);
	logtext->type = LOG_WARN;
	
	g_timeout_add(0, invoke_hook_cb, logtext);

	if (log_fp) {
		fwrite(buf, LOG_TIME_LEN, 1, log_fp);
		fputs("** warning: ", log_fp);
		fputs(buf + LOG_TIME_LEN, log_fp);
		fflush(log_fp);
	}
}

void log_error(const gchar *format, ...)
{
	va_list args;
	gchar buf[BUFFSIZE + LOG_TIME_LEN];
	time_t t;
	LogText *logtext = g_new0(LogText, 1);

	time(&t);
	strftime(buf, LOG_TIME_LEN + 1, "[%H:%M:%S] ", localtime(&t));

	va_start(args, format);
	g_vsnprintf(buf + LOG_TIME_LEN, BUFFSIZE, format, args);
	va_end(args);

	g_warning("%s", buf);
	logtext->text = g_strdup(buf + LOG_TIME_LEN);
	logtext->type = LOG_ERROR;
	
	g_timeout_add(0, invoke_hook_cb, logtext);

	if (log_fp) {
		fwrite(buf, LOG_TIME_LEN, 1, log_fp);
		fputs("*** error: ", log_fp);
		fputs(buf + LOG_TIME_LEN, log_fp);
		fflush(log_fp);
	}
}
