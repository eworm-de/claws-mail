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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#include "claws-features.h"
#endif

#include <glib.h>

#ifndef G_OS_WIN32
#include <sys/wait.h>
#else
#define WEXITSTATUS(x) (x)
#endif

#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>

#include "defs.h"
#include "codeconv.h"
#include "timing.h"
#include "file-utils.h"

gboolean prefs_common_get_flush_metadata(void);
gboolean prefs_common_get_use_shred(void);

static int safe_fclose(FILE *fp)
{
	int r;
	START_TIMING("");

	if (fflush(fp) != 0) {
		return EOF;
	}
	if (prefs_common_get_flush_metadata() && fsync(fileno(fp)) != 0) {
		return EOF;
	}

	r = fclose(fp);
	END_TIMING();

	return r;
}

/* Unlock, then safe-close a file pointer
 * Safe close is done using fflush + fsync
 * if the according preference says so.
 */
int claws_safe_fclose(FILE *fp)
{
#if HAVE_FGETS_UNLOCKED
	funlockfile(fp);
#endif
	return safe_fclose(fp);
}

#if HAVE_FGETS_UNLOCKED

/* Open a file and locks it once
 * so subsequent I/O is faster
 */
FILE *claws_fopen(const char *file, const char *mode)
{
	FILE *fp = fopen(file, mode);
	if (!fp)
		return NULL;
	flockfile(fp);
	return fp;
}

FILE *claws_fdopen(int fd, const char *mode)
{
	FILE *fp = fdopen(fd, mode);
	if (!fp)
		return NULL;
	flockfile(fp);
	return fp;
}

/* Unlocks and close a file pointer
 */

int claws_fclose(FILE *fp)
{
	funlockfile(fp);
	return fclose(fp);
}
#endif

int claws_unlink(const char *filename) 
{
	GStatBuf s;
	static int found_shred = -1;
	static const gchar *args[4];

	if (filename == NULL)
		return 0;

	if (prefs_common_get_use_shred()) {
		if (found_shred == -1) {
			/* init */
			args[0] = g_find_program_in_path("shred");
			debug_print("found shred: %s\n", args[0]);
			found_shred = (args[0] != NULL) ? 1:0;
			args[1] = "-f";
			args[3] = NULL;
		}
		if (found_shred == 1) {
			if (g_stat(filename, &s) == 0 && S_ISREG(s.st_mode)) {
				if (s.st_nlink == 1) {
					gint status=0;
					args[2] = filename;
					g_spawn_sync(NULL, (gchar **)args, NULL, 0,
					 NULL, NULL, NULL, NULL, &status, NULL);
					debug_print("%s %s exited with status %d\n",
						args[0], filename, WEXITSTATUS(status));
					if (truncate(filename, 0) < 0)
						g_warning("couldn't truncate: %s", filename);
				}
			}
		}
	}
	return g_unlink(filename);
}

gint file_strip_crs(const gchar *file)
{
	FILE *fp = NULL, *outfp = NULL;
	gchar buf[4096];
	gchar *out = get_tmp_file();
	if (file == NULL)
		goto freeout;

	fp = claws_fopen(file, "rb");
	if (!fp)
		goto freeout;

	outfp = claws_fopen(out, "wb");
	if (!outfp) {
		claws_fclose(fp);
		goto freeout;
	}

	while (claws_fgets(buf, sizeof (buf), fp) != NULL) {
		strcrchomp(buf);
		if (claws_fputs(buf, outfp) == EOF) {
			claws_fclose(fp);
			claws_fclose(outfp);
			goto unlinkout;
		}
	}

	claws_fclose(fp);
	if (claws_safe_fclose(outfp) == EOF) {
		goto unlinkout;
	}
	
	if (move_file(out, file, TRUE) < 0)
		goto unlinkout;
	
	g_free(out);
	return 0;
unlinkout:
	claws_unlink(out);
freeout:
	g_free(out);
	return -1;
}

/*
 * Append src file body to the tail of dest file.
 * Now keep_backup has no effects.
 */
gint append_file(const gchar *src, const gchar *dest, gboolean keep_backup)
{
	FILE *src_fp, *dest_fp;
	gint n_read;
	gchar buf[BUFSIZ];

	gboolean err = FALSE;

	if ((src_fp = claws_fopen(src, "rb")) == NULL) {
		FILE_OP_ERROR(src, "claws_fopen");
		return -1;
	}

	if ((dest_fp = claws_fopen(dest, "ab")) == NULL) {
		FILE_OP_ERROR(dest, "claws_fopen");
		claws_fclose(src_fp);
		return -1;
	}

	if (change_file_mode_rw(dest_fp, dest) < 0) {
		FILE_OP_ERROR(dest, "chmod");
		g_warning("can't change file mode: %s", dest);
	}

	while ((n_read = claws_fread(buf, sizeof(gchar), sizeof(buf), src_fp)) > 0) {
		if (n_read < sizeof(buf) && claws_ferror(src_fp))
			break;
		if (claws_fwrite(buf, 1, n_read, dest_fp) < n_read) {
			g_warning("writing to %s failed", dest);
			claws_fclose(dest_fp);
			claws_fclose(src_fp);
			claws_unlink(dest);
			return -1;
		}
	}

	if (claws_ferror(src_fp)) {
		FILE_OP_ERROR(src, "claws_fread");
		err = TRUE;
	}
	claws_fclose(src_fp);
	if (claws_fclose(dest_fp) == EOF) {
		FILE_OP_ERROR(dest, "claws_fclose");
		err = TRUE;
	}

	if (err) {
		claws_unlink(dest);
		return -1;
	}

	return 0;
}

gint copy_file(const gchar *src, const gchar *dest, gboolean keep_backup)
{
	FILE *src_fp, *dest_fp;
	gint n_read;
	gchar buf[BUFSIZ];
	gchar *dest_bak = NULL;
	gboolean err = FALSE;

	if ((src_fp = claws_fopen(src, "rb")) == NULL) {
		FILE_OP_ERROR(src, "claws_fopen");
		return -1;
	}
	if (is_file_exist(dest)) {
		dest_bak = g_strconcat(dest, ".bak", NULL);
		if (rename_force(dest, dest_bak) < 0) {
			FILE_OP_ERROR(dest, "rename");
			claws_fclose(src_fp);
			g_free(dest_bak);
			return -1;
		}
	}

	if ((dest_fp = claws_fopen(dest, "wb")) == NULL) {
		FILE_OP_ERROR(dest, "claws_fopen");
		claws_fclose(src_fp);
		if (dest_bak) {
			if (rename_force(dest_bak, dest) < 0)
				FILE_OP_ERROR(dest_bak, "rename");
			g_free(dest_bak);
		}
		return -1;
	}

	if (change_file_mode_rw(dest_fp, dest) < 0) {
		FILE_OP_ERROR(dest, "chmod");
		g_warning("can't change file mode: %s", dest);
	}

	while ((n_read = claws_fread(buf, sizeof(gchar), sizeof(buf), src_fp)) > 0) {
		if (n_read < sizeof(buf) && claws_ferror(src_fp))
			break;
		if (claws_fwrite(buf, 1, n_read, dest_fp) < n_read) {
			g_warning("writing to %s failed", dest);
			claws_fclose(dest_fp);
			claws_fclose(src_fp);
			if (claws_unlink(dest) < 0)
                                FILE_OP_ERROR(dest, "claws_unlink");
			if (dest_bak) {
				if (rename_force(dest_bak, dest) < 0)
					FILE_OP_ERROR(dest_bak, "rename");
				g_free(dest_bak);
			}
			return -1;
		}
	}

	if (claws_ferror(src_fp)) {
		FILE_OP_ERROR(src, "claws_fread");
		err = TRUE;
	}
	claws_fclose(src_fp);
	if (claws_safe_fclose(dest_fp) == EOF) {
		FILE_OP_ERROR(dest, "claws_fclose");
		err = TRUE;
	}

	if (err) {
		if (claws_unlink(dest) < 0)
                        FILE_OP_ERROR(dest, "claws_unlink");
		if (dest_bak) {
			if (rename_force(dest_bak, dest) < 0)
				FILE_OP_ERROR(dest_bak, "rename");
			g_free(dest_bak);
		}
		return -1;
	}

	if (keep_backup == FALSE && dest_bak)
		if (claws_unlink(dest_bak) < 0)
                        FILE_OP_ERROR(dest_bak, "claws_unlink");

	g_free(dest_bak);

	return 0;
}

gint move_file(const gchar *src, const gchar *dest, gboolean overwrite)
{
	if (overwrite == FALSE && is_file_exist(dest)) {
		g_warning("move_file(): file %s already exists", dest);
		return -1;
	}

	if (rename_force(src, dest) == 0) return 0;

	if (EXDEV != errno) {
		FILE_OP_ERROR(src, "rename");
		return -1;
	}

	if (copy_file(src, dest, FALSE) < 0) return -1;

	claws_unlink(src);

	return 0;
}

gint copy_file_part_to_fp(FILE *fp, off_t offset, size_t length, FILE *dest_fp)
{
	gint n_read;
	gint bytes_left, to_read;
	gchar buf[BUFSIZ];

	if (fseek(fp, offset, SEEK_SET) < 0) {
		perror("fseek");
		return -1;
	}

	bytes_left = length;
	to_read = MIN(bytes_left, sizeof(buf));

	while ((n_read = claws_fread(buf, sizeof(gchar), to_read, fp)) > 0) {
		if (n_read < to_read && claws_ferror(fp))
			break;
		if (claws_fwrite(buf, 1, n_read, dest_fp) < n_read) {
			return -1;
		}
		bytes_left -= n_read;
		if (bytes_left == 0)
			break;
		to_read = MIN(bytes_left, sizeof(buf));
	}

	if (claws_ferror(fp)) {
		perror("claws_fread");
		return -1;
	}

	return 0;
}

gint copy_file_part(FILE *fp, off_t offset, size_t length, const gchar *dest)
{
	FILE *dest_fp;
	gboolean err = FALSE;

	if ((dest_fp = claws_fopen(dest, "wb")) == NULL) {
		FILE_OP_ERROR(dest, "claws_fopen");
		return -1;
	}

	if (change_file_mode_rw(dest_fp, dest) < 0) {
		FILE_OP_ERROR(dest, "chmod");
		g_warning("can't change file mode: %s", dest);
	}

	if (copy_file_part_to_fp(fp, offset, length, dest_fp) < 0)
		err = TRUE;

	if (claws_safe_fclose(dest_fp) == EOF) {
		FILE_OP_ERROR(dest, "claws_fclose");
		err = TRUE;
	}

	if (err) {
		g_warning("writing to %s failed", dest);
		claws_unlink(dest);
		return -1;
	}

	return 0;
}

gint canonicalize_file(const gchar *src, const gchar *dest)
{
	FILE *src_fp, *dest_fp;
	gchar buf[BUFFSIZE];
	gint len;
	gboolean err = FALSE;
	gboolean last_linebreak = FALSE;

	if (src == NULL || dest == NULL)
		return -1;

	if ((src_fp = claws_fopen(src, "rb")) == NULL) {
		FILE_OP_ERROR(src, "claws_fopen");
		return -1;
	}

	if ((dest_fp = claws_fopen(dest, "wb")) == NULL) {
		FILE_OP_ERROR(dest, "claws_fopen");
		claws_fclose(src_fp);
		return -1;
	}

	if (change_file_mode_rw(dest_fp, dest) < 0) {
		FILE_OP_ERROR(dest, "chmod");
		g_warning("can't change file mode: %s", dest);
	}

	while (claws_fgets(buf, sizeof(buf), src_fp) != NULL) {
		gint r = 0;

		len = strlen(buf);
		if (len == 0) break;
		last_linebreak = FALSE;

		if (buf[len - 1] != '\n') {
			last_linebreak = TRUE;
			r = claws_fputs(buf, dest_fp);
		} else if (len > 1 && buf[len - 1] == '\n' && buf[len - 2] == '\r') {
			r = claws_fputs(buf, dest_fp);
		} else {
			if (len > 1) {
				r = claws_fwrite(buf, 1, len - 1, dest_fp);
				if (r != (len -1))
					r = EOF;
			}
			if (r != EOF)
				r = claws_fputs("\r\n", dest_fp);
		}

		if (r == EOF) {
			g_warning("writing to %s failed", dest);
			claws_fclose(dest_fp);
			claws_fclose(src_fp);
			claws_unlink(dest);
			return -1;
		}
	}

	if (last_linebreak == TRUE) {
		if (claws_fputs("\r\n", dest_fp) == EOF)
			err = TRUE;
	}

	if (claws_ferror(src_fp)) {
		FILE_OP_ERROR(src, "claws_fgets");
		err = TRUE;
	}
	claws_fclose(src_fp);
	if (claws_safe_fclose(dest_fp) == EOF) {
		FILE_OP_ERROR(dest, "claws_fclose");
		err = TRUE;
	}

	if (err) {
		claws_unlink(dest);
		return -1;
	}

	return 0;
}

gint canonicalize_file_replace(const gchar *file)
{
	gchar *tmp_file;

	tmp_file = get_tmp_file();

	if (canonicalize_file(file, tmp_file) < 0) {
		g_free(tmp_file);
		return -1;
	}

	if (move_file(tmp_file, file, TRUE) < 0) {
		g_warning("can't replace file: %s", file);
		claws_unlink(tmp_file);
		g_free(tmp_file);
		return -1;
	}

	g_free(tmp_file);
	return 0;
}


gint str_write_to_file(const gchar *str, const gchar *file, gboolean safe)
{
	FILE *fp;
	size_t len;
	int r;

	cm_return_val_if_fail(str != NULL, -1);
	cm_return_val_if_fail(file != NULL, -1);

	if ((fp = claws_fopen(file, "wb")) == NULL) {
		FILE_OP_ERROR(file, "claws_fopen");
		return -1;
	}

	len = strlen(str);
	if (len == 0) {
		claws_fclose(fp);
		return 0;
	}

	if (claws_fwrite(str, 1, len, fp) != len) {
		FILE_OP_ERROR(file, "claws_fwrite");
		claws_fclose(fp);
		claws_unlink(file);
		return -1;
	}

	if (safe) {
		r = claws_safe_fclose(fp);
	} else {
		r = claws_fclose(fp);
	}

	if (r == EOF) {
		FILE_OP_ERROR(file, "claws_fclose");
		claws_unlink(file);
		return -1;
	}

	return 0;
}

static gchar *file_read_stream_to_str_full(FILE *fp, gboolean recode)
{
	GByteArray *array;
	guchar buf[BUFSIZ];
	gint n_read;
	gchar *str;

	cm_return_val_if_fail(fp != NULL, NULL);

	array = g_byte_array_new();

	while ((n_read = claws_fread(buf, sizeof(gchar), sizeof(buf), fp)) > 0) {
		if (n_read < sizeof(buf) && claws_ferror(fp))
			break;
		g_byte_array_append(array, buf, n_read);
	}

	if (claws_ferror(fp)) {
		FILE_OP_ERROR("file stream", "claws_fread");
		g_byte_array_free(array, TRUE);
		return NULL;
	}

	buf[0] = '\0';
	g_byte_array_append(array, buf, 1);
	str = (gchar *)array->data;
	g_byte_array_free(array, FALSE);

	if (recode && !g_utf8_validate(str, -1, NULL)) {
		const gchar *src_codeset, *dest_codeset;
		gchar *tmp = NULL;
		src_codeset = conv_get_locale_charset_str();
		dest_codeset = CS_UTF_8;
		tmp = conv_codeset_strdup(str, src_codeset, dest_codeset);
		g_free(str);
		str = tmp;
	}

	return str;
}

static gchar *file_read_to_str_full(const gchar *file, gboolean recode)
{
	FILE *fp;
	gchar *str;
	GStatBuf s;
#ifndef G_OS_WIN32
	gint fd, err;
	struct timeval timeout = {1, 0};
	fd_set fds;
	int fflags = 0;
#endif

	cm_return_val_if_fail(file != NULL, NULL);

	if (g_stat(file, &s) != 0) {
		FILE_OP_ERROR(file, "stat");
		return NULL;
	}
	if (S_ISDIR(s.st_mode)) {
		g_warning("%s: is a directory", file);
		return NULL;
	}

#ifdef G_OS_WIN32
	fp = claws_fopen (file, "rb");
	if (fp == NULL) {
		FILE_OP_ERROR(file, "open");
		return NULL;
	}
#else	  
	/* test whether the file is readable without blocking */
	fd = g_open(file, O_RDONLY | O_NONBLOCK, 0);
	if (fd == -1) {
		FILE_OP_ERROR(file, "open");
		return NULL;
	}

	FD_ZERO(&fds);
	FD_SET(fd, &fds);

	/* allow for one second */
	err = select(fd+1, &fds, NULL, NULL, &timeout);
	if (err <= 0 || !FD_ISSET(fd, &fds)) {
		if (err < 0) {
			FILE_OP_ERROR(file, "select");
		} else {
			g_warning("%s: doesn't seem readable", file);
		}
		close(fd);
		return NULL;
	}
	
	/* Now clear O_NONBLOCK */
	if ((fflags = fcntl(fd, F_GETFL)) < 0) {
		FILE_OP_ERROR(file, "fcntl (F_GETFL)");
		close(fd);
		return NULL;
	}
	if (fcntl(fd, F_SETFL, (fflags & ~O_NONBLOCK)) < 0) {
		FILE_OP_ERROR(file, "fcntl (F_SETFL)");
		close(fd);
		return NULL;
	}
	
	/* get the FILE pointer */
	fp = claws_fdopen(fd, "rb");

	if (fp == NULL) {
		FILE_OP_ERROR(file, "claws_fdopen");
		close(fd); /* if fp isn't NULL, we'll use claws_fclose instead! */
		return NULL;
	}
#endif

	str = file_read_stream_to_str_full(fp, recode);

	claws_fclose(fp);

	return str;
}

gchar *file_read_to_str(const gchar *file)
{
	return file_read_to_str_full(file, TRUE);
}
gchar *file_read_stream_to_str(FILE *fp)
{
	return file_read_stream_to_str_full(fp, TRUE);
}

gchar *file_read_to_str_no_recode(const gchar *file)
{
	return file_read_to_str_full(file, FALSE);
}
gchar *file_read_stream_to_str_no_recode(FILE *fp)
{
	return file_read_stream_to_str_full(fp, FALSE);
}

gint rename_force(const gchar *oldpath, const gchar *newpath)
{
#ifndef G_OS_UNIX
	if (!is_file_entry_exist(oldpath)) {
		errno = ENOENT;
		return -1;
	}
	if (is_file_exist(newpath)) {
		if (claws_unlink(newpath) < 0)
			FILE_OP_ERROR(newpath, "unlink");
	}
#endif
	return g_rename(oldpath, newpath);
}

gint copy_dir(const gchar *src, const gchar *dst)
{
	GDir *dir;
	const gchar *name;

	if ((dir = g_dir_open(src, 0, NULL)) == NULL) {
		g_warning("failed to open directory: %s", src);
		return -1;
	}

	if (make_dir(dst) < 0) {
		g_dir_close(dir);
		return -1;
	}

	while ((name = g_dir_read_name(dir)) != NULL) {
		gchar *old_file, *new_file;
		gint r = 0;
		old_file = g_strconcat(src, G_DIR_SEPARATOR_S, name, NULL);
		new_file = g_strconcat(dst, G_DIR_SEPARATOR_S, name, NULL);
		debug_print("copying: %s -> %s\n", old_file, new_file);
		if (g_file_test(old_file, G_FILE_TEST_IS_REGULAR)) {
			r = copy_file(old_file, new_file, TRUE);
		}
#ifndef G_OS_WIN32
		/* Windows has no symlinks.  Or well, Vista seems to
		   have something like this but the semantics might be
		   different. Thus we don't use it under Windows. */
		else if (g_file_test(old_file, G_FILE_TEST_IS_SYMLINK)) {
			GError *error = NULL;
			gchar *target = g_file_read_link(old_file, &error);
			if (error) {
				g_warning("couldn't read link: %s", error->message);
				g_error_free(error);
			}
			if (target) {
				r = symlink(target, new_file);
				g_free(target);
			}
		}
#endif /*G_OS_WIN32*/
		else if (g_file_test(old_file, G_FILE_TEST_IS_DIR)) {
			r = copy_dir(old_file, new_file);
		}
		g_free(old_file);
		g_free(new_file);
		if (r < 0) {
			g_dir_close(dir);
			return r;
		}
	}
	g_dir_close(dir);
	return 0;
}

gint change_file_mode_rw(FILE *fp, const gchar *file)
{
#if HAVE_FCHMOD
	return fchmod(fileno(fp), S_IRUSR|S_IWUSR);
#else
	return g_chmod(file, S_IRUSR|S_IWUSR);
#endif
}

FILE *my_tmpfile(void)
{
	const gchar suffix[] = ".XXXXXX";
	const gchar *tmpdir;
	guint tmplen;
	const gchar *progname;
	guint proglen;
	gchar *fname;
	gint fd;
	FILE *fp;
#ifndef G_OS_WIN32
	gchar buf[2]="\0";
#endif

	tmpdir = get_tmp_dir();
	tmplen = strlen(tmpdir);
	progname = g_get_prgname();
	if (progname == NULL)
		progname = "claws-mail";
	proglen = strlen(progname);
	Xalloca(fname, tmplen + 1 + proglen + sizeof(suffix),
		return tmpfile());

	memcpy(fname, tmpdir, tmplen);
	fname[tmplen] = G_DIR_SEPARATOR;
	memcpy(fname + tmplen + 1, progname, proglen);
	memcpy(fname + tmplen + 1 + proglen, suffix, sizeof(suffix));

	fd = g_mkstemp(fname);
	if (fd < 0)
		return tmpfile();

#ifndef G_OS_WIN32
	claws_unlink(fname);
	
	/* verify that we can write in the file after unlinking */
	if (write(fd, buf, 1) < 0) {
		close(fd);
		return tmpfile();
	}
	
#endif

	fp = claws_fdopen(fd, "w+b");
	if (!fp)
		close(fd);
	else {
		rewind(fp);
		return fp;
	}

	return tmpfile();
}

FILE *get_tmpfile_in_dir(const gchar *dir, gchar **filename)
{
	int fd;
	*filename = g_strdup_printf("%s%cclaws.XXXXXX", dir, G_DIR_SEPARATOR);
	fd = g_mkstemp(*filename);
	if (fd < 0)
		return NULL;
	return claws_fdopen(fd, "w+");
}

FILE *str_open_as_stream(const gchar *str)
{
	FILE *fp;
	size_t len;

	cm_return_val_if_fail(str != NULL, NULL);

	fp = my_tmpfile();
	if (!fp) {
		FILE_OP_ERROR("str_open_as_stream", "my_tmpfile");
		return NULL;
	}

	len = strlen(str);
	if (len == 0) return fp;

	if (claws_fwrite(str, 1, len, fp) != len) {
		FILE_OP_ERROR("str_open_as_stream", "claws_fwrite");
		claws_fclose(fp);
		return NULL;
	}

	rewind(fp);
	return fp;
}
