/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999,2000 Hiroyuki Yamamoto
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

#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "intl.h"
#include "recv.h"
#include "socket.h"
#include "utils.h"

#define BUFFSIZE	8192

gint recv_write_to_file(gint sock, const gchar *filename)
{
	FILE *fp;

	g_return_val_if_fail(filename != NULL, -1);

	if ((fp = fopen(filename, "w")) == NULL) {
		FILE_OP_ERROR(filename, "fopen");
		recv_write(sock, NULL);
		return -1;
	}

	if (change_file_mode_rw(fp, filename) < 0)
		FILE_OP_ERROR(filename, "chmod");

	if (recv_write(sock, fp) < 0) {
		fclose(fp);
		unlink(filename);
		return -1;
	}

	if (fclose(fp) == EOF) {
		FILE_OP_ERROR(filename, "fclose");
		unlink(filename);
		return -1;
	}

	return 0;
}

gint recv_bytes_write_to_file(gint sock, glong size, const gchar *filename)
{
	FILE *fp;

	g_return_val_if_fail(filename != NULL, -1);

	if ((fp = fopen(filename, "w")) == NULL) {
		FILE_OP_ERROR(filename, "fopen");
		recv_write(sock, NULL);
		return -1;
	}

	if (change_file_mode_rw(fp, filename) < 0)
		FILE_OP_ERROR(filename, "chmod");

	if (recv_bytes_write(sock, size, fp) < 0) {
		fclose(fp);
		unlink(filename);
		return -1;
	}

	if (fclose(fp) == EOF) {
		FILE_OP_ERROR(filename, "fclose");
		unlink(filename);
		return -1;
	}

	return 0;
}

gint recv_write(gint sock, FILE *fp)
{
	gchar buf[BUFFSIZE];
	gint len;
	gboolean nb;

	nb = sock_is_nonblocking_mode(sock);
	if (nb) sock_set_nonblocking_mode(sock, FALSE);

	for (;;) {
		if (sock_read(sock, buf, sizeof(buf)) < 0) {
			g_warning(_("error occurred while retrieving data.\n"));
			if (nb) sock_set_nonblocking_mode(sock, TRUE);
			return -1;
		}

		len = strlen(buf);
		if (len > 1 && buf[0] == '.' && buf[1] == '\r') break;

		if (len > 1 && buf[len - 1] == '\n' && buf[len - 2] == '\r') {
			buf[len - 2] = '\n';
			buf[len - 1] = '\0';
		}

		if (buf[0] == '.' && buf[1] == '.')
			memmove(buf, buf + 1, strlen(buf));

		if (!strncmp(buf, ">From ", 6))
			memmove(buf, buf + 1, strlen(buf));

		if (fp && fputs(buf, fp) == EOF) {
			perror("fputs");
			g_warning(_("Can't write to file.\n"));
			fp = NULL;
		}
	}

	if (nb) sock_set_nonblocking_mode(sock, TRUE);

	if (!fp) return -1;

	return 0;
}

gint recv_bytes_write(gint sock, glong size, FILE *fp)
{
	gchar *buf;
	gboolean nb;
	glong count = 0;
	gchar *prev, *cur;

	nb = sock_is_nonblocking_mode(sock);
	if (nb) sock_set_nonblocking_mode(sock, FALSE);

	Xalloca(buf, size, return -1);

	do {
		size_t read_count;

		read_count = read(sock, buf + count, size - count);
		if (read_count < 0) {
			if (nb) sock_set_nonblocking_mode(sock, TRUE);
			return -1;
		}
		count += read_count;
	} while (count < size);

	prev = buf;
	while ((cur = memchr(prev, '\r', size)) != NULL) {
		if (cur - buf + 1 < size && *(cur + 1) == '\n') {
			if (fwrite(prev, sizeof(gchar), cur - prev, fp) == EOF ||
			    fwrite("\n", sizeof(gchar), 1, fp) == EOF) {
				perror("fwrite");
				g_warning(_("Can't write to file.\n"));
				if (nb) sock_set_nonblocking_mode(sock, TRUE);
				return -1;
			}
			prev = cur + 2;
			if (prev - buf >= size) break;
		}
	}

	if (prev - buf < size && fwrite(buf, sizeof(gchar),
					size - (prev - buf), fp) == EOF) {
		perror("fwrite");
		g_warning(_("Can't write to file.\n"));
		if (nb) sock_set_nonblocking_mode(sock, TRUE);
		return -1;
	}

	if (nb) sock_set_nonblocking_mode(sock, TRUE);
	return 0;
}
