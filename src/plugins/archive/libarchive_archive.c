/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2018 Michael Rasmussen and the Claws Mail Team
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
#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include "libarchive_archive.h"

#ifndef _TEST
#	include "archiver.h"
#	include "utils.h"
#	include "mainwindow.h"
#   include "folder.h"
#endif

#include <sys/types.h>
#include <sys/stat.h>

#include <archive.h>
#include <archive_entry.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <libgen.h>

#define READ_BLOCK_SIZE 10240

struct file_info {
	char* path;
	char* name;
};

static GSList* msg_trash_list = NULL;
static GSList* file_list = NULL;
static gboolean stop_action = FALSE;

#ifdef _TEST
static int permissions = 0;
#endif

static void free_msg_trash(MsgTrash* trash) {
    if (trash) {
        gchar *name = folder_item_get_name(trash->item);

        debug_print("Freeing files in %s\n", name);
        g_free(name);
        if (trash->msgs) {
            g_slist_free(trash->msgs);
        }
        g_free(trash);
    }
}

MsgTrash* new_msg_trash(FolderItem* item) {
    MsgTrash* msg_trash;
    FolderType  type;

    g_return_val_if_fail(item != NULL, NULL);

    /* FolderType must be F_MH, F_MBOX, F_MAILDIR or F_IMAP */
    type = item->folder->klass->type;
    if (!(type == F_MH || type == F_MBOX || 
            type == F_MAILDIR || type == F_IMAP))
       return NULL; 
    msg_trash = g_new0(MsgTrash, 1);
    msg_trash->item = item;
    msg_trash->msgs = NULL;
    msg_trash_list = g_slist_prepend(msg_trash_list, msg_trash);
    
    return msg_trash;
    }

void archive_free_archived_files() {
    MsgTrash* mt = NULL;
    gint    res;
    GSList* l = NULL;
   
    for (l = msg_trash_list; l; l = g_slist_next(l)) {
        gchar *name;

        mt = (MsgTrash *) l->data;
        name = folder_item_get_name(mt->item);
        debug_print("Trashing messages in folder: %s\n", 
                name);
        g_free(name);
        res = folder_item_remove_msgs(mt->item, mt->msgs);
        debug_print("Result was %d\n", res);
        free_msg_trash(mt);
    }
    g_slist_free(msg_trash_list);
    msg_trash_list = NULL;
}

void archive_add_msg_mark(MsgTrash* trash, MsgInfo* msg) {
    g_return_if_fail(trash != NULL || msg != NULL);
    debug_print("Marking msg #%d for removal\n", msg->msgnum);
    trash->msgs = g_slist_prepend(trash->msgs, msg);
}

static void free_all(GDate* date, gchar** parts) {
    if (date)
        g_date_free(date);
    if (parts)
        g_strfreev(parts);
}

static gboolean is_iso_string(gchar** items) {
    int i = -1;
    gchar* item;

    while (*items) {
        i++;
        item = *items++;
        debug_print("Date part %d: %s\n", i, item);
        switch(i) {
            case 0:
                if (strlen(item) != 4)
                    return FALSE;
                break;
            case 1:
            case 2:
                if (strlen(item) != 2)
                    return FALSE;
                break;
            default:
                return FALSE;
        }
    }
    debug_print("Leaving\n");
    return (i == 2);
}

static GDate* iso2GDate(const gchar* date) {
    GDate*  gdate;
    gchar** parts;
    int     i;

    g_return_val_if_fail(date != NULL, NULL);

    gdate = g_date_new();
    parts = g_strsplit(date, "-", 3);
    if (!parts)
        return NULL;
    if (! is_iso_string(parts)) {
        free_all(gdate, parts);
        return NULL;
    }
    for (i = 0; i < 3; i++) {
        int t = atoi(parts[i]);
        switch (i) {
            case 0: 
                if (t < 1 || t > 9999) {
                    free_all(gdate, parts);
                    return NULL;
                }
                g_date_set_year(gdate, t);
                break;
            case 1:
                if (t < 1 || t > 12) {
                    free_all(gdate, parts);
                    return NULL;
                }
                g_date_set_month(gdate, t);
                break;
            case 2:
                if (t < 1 || t > 31) {
                    free_all(gdate, parts);
                    return NULL;
                }
                g_date_set_day(gdate, t);
                break;
        }
    }
    g_strfreev(parts);
    return gdate;
}

gboolean before_date(time_t msg_mtime, const gchar* before) {
    gchar*      pos = NULL;
    GDate*      date;
    GDate*      file_t;
    gboolean    res;

    debug_print("Cut-off date: %s\n", before);
    if ((date = iso2GDate(before)) == NULL) {
        g_warning("bad date format: %s", before);
        return FALSE;
    }

    file_t = g_date_new();
    g_date_set_time_t(file_t, msg_mtime);

    if (debug_get_mode()) {
        pos = g_new0(char, 100);
        g_date_strftime(pos, 100, "%F", file_t);
        fprintf(stderr, "File date: %s\n", pos);
        g_free(pos);
    }

    if (! g_date_valid(file_t)) {
        g_warning("invalid msg date");
        return FALSE;
    }

    res = (g_date_compare(file_t, date) >= 0) ? FALSE : TRUE;
    g_date_free(file_t);
    return res;
}
   
static void archive_free_file_info(struct file_info* file) {
	if (! file)
		return;
	if (file->path)
		g_free(file->path);
	if (file->name)
		g_free(file->name);
	g_free(file);
	file = NULL;
}

void stop_archiving() {
	debug_print("stop action set to true\n");
	stop_action = TRUE;
}

void archive_free_file_list(gboolean md5, gboolean rename) {
	struct file_info* file = NULL;
	gchar* path = NULL;

	debug_print("freeing file list\n");
	if (! file_list)
		return;
	while (file_list) {
		file = (struct file_info *) file_list->data;
		if (!rename && md5 && g_str_has_suffix(file->name, ".md5")) {
			path = g_strdup_printf("%s/%s", file->path, file->name);
			debug_print("unlinking %s\n", path);
			if (g_unlink(path) < 0)
                                FILE_OP_ERROR(path, "g_unlink");
			g_free(path);
		}
		if (rename) {
			path = g_strdup_printf("%s/%s", file->path, file->name);
			debug_print("unlinking %s\n", path);
			if (g_unlink(path) < 0)
                                FILE_OP_ERROR(path, "g_unlink");
			g_free(path);
		}
		archive_free_file_info(file);
		file_list->data = NULL;
		file_list = g_slist_next(file_list);
	}
	if (file_list) {
		g_slist_free(file_list);
		file_list = NULL;
	}
}

static struct file_info* archive_new_file_info() {
	struct file_info* new_file_info = malloc(sizeof(struct file_info));

	new_file_info->path = NULL;
	new_file_info->name = NULL;
	return new_file_info;
}

static void archive_add_to_list(struct file_info* file) {
	if (! file)
		return;
	file_list = g_slist_prepend(file_list, (gpointer) file);
}

static gchar* strip_leading_dot_slash(gchar* path) {
	if (path && strlen(path) > 1 && path[0] == '.' && path[1] == '/')
		return g_strdup(&(path[2]));

	return g_strdup(path);
}

static gchar* get_full_path(struct file_info* file) {
	char* path = malloc(PATH_MAX);

	if (file->path && *(file->path))
		sprintf(path, "%s/%s", file->path, file->name);
	else
		sprintf(path, "%s", file->name);
	return path;
}

#ifdef _TEST
static gchar* strip_leading_slash(gchar* path) {
	gchar* stripped = path;
	gchar* result = NULL;

	if (stripped && stripped[0] == '/') {
		++stripped;
		result = g_strdup(stripped);
	}
	else
		result = g_strdup(path);
	return result;
}

static int archive_get_permissions() {
	return permissions;
}


void archive_set_permissions(int perm) {
	permissions = perm;
}

static int archive_copy_data(struct archive* in, struct archive* out) {
	const void* buf;
	size_t size;
	off_t offset;
	int res = ARCHIVE_OK;

	while (res == ARCHIVE_OK) {
		res = archive_read_data_block(in, &buf, &size, &offset);
		if (res == ARCHIVE_OK) {
			res = archive_write_data_block(out, buf, size, offset);
		}
	}
	return (res == ARCHIVE_EOF) ? ARCHIVE_OK : res;
}
#endif

void archive_add_file(gchar* path) {
	struct file_info* file;
	gchar* filename = NULL;

	g_return_if_fail(path != NULL);

#ifndef _TEST
	debug_print("add %s to list\n", path);
#endif
	filename = g_strrstr_len(path, strlen(path), "/");
	if (! filename)
		g_warning("no filename in path '%s'", path);
	g_return_if_fail(filename != NULL);

	filename++;
	file = archive_new_file_info();
	file->name = g_strdup(filename);
	file->path = strip_leading_dot_slash(dirname(path));
	archive_add_to_list(file);
}

GSList* archive_get_file_list() {
	return file_list;
}

#ifdef _TEST
const gchar* archive_extract(const char* archive_name, int flags) {
	struct archive* in;
	struct archive* out;
	struct archive_entry* entry;
	int res = ARCHIVE_OK;
	gchar* buf = NULL;
	const char* result == NULL;

	g_return_val_if_fail(archive_name != NULL, ARCHIVE_FATAL);

	fprintf(stdout, "%s: extracting\n", archive_name);
	in = archive_read_new();
	if ((res = archive_read_support_format_tar(in)) == ARCHIVE_OK) {
		if ((res = archive_read_support_compression_gzip(in)) == ARCHIVE_OK) {
#if ARCHIVE_VERSION_NUMBER < 3000000
			if ((res = archive_read_open_file(
#else
			if ((res = archive_read_open_filename(
#endif
				in, archive_name, READ_BLOCK_SIZE)) != ARCHIVE_OK) {
				g_warning("%s: %s", archive_name, archive_error_string(in));
				result = archive_error_string(in);
			}
			else {
				out = archive_write_disk_new();
				if ((res = archive_write_disk_set_options(
								out, flags)) == ARCHIVE_OK) {
					res = archive_read_next_header(in, &entry);
					while (res == ARCHIVE_OK) {
						res = archive_write_header(out, entry);
						if (res != ARCHIVE_OK) {
							g_warning("%s", archive_error_string(out));
							/* skip this file an continue */
							res = ARCHIVE_OK;
						}
						else {
							res = archive_copy_data(in, out);
							if (res != ARCHIVE_OK) {
								g_warning("%s", archive_error_string(in));
								/* skip this file an continue */
								res = ARCHIVE_OK;
							}
							else
								res = archive_read_next_header(in, &entry);
						}
					}
					if (res == ARCHIVE_EOF)
						res = ARCHIVE_OK;
					if (res != ARCHIVE_OK) {
						gchar *e = archive_error_string(in);
						g_warning("%s: %s", archive_name, e? e: "unknown error");
						result = e;
					}
				}
				else
					result = archive_error_string(out);
				archive_read_close(in);
			}
#if ARCHIVE_VERSION_NUMBER < 3000000
			archive_read_finish(in);
#else
			archive_read_free(in);
#endif
		}
		else
			result = archive_error_string(in);
	}
	else
		result = archive_error_string(in);
	return result;
}
#endif

const gchar* archive_create(const char* archive_name, GSList* files,
			COMPRESS_METHOD method, ARCHIVE_FORMAT format) {
	struct archive* arch;

#ifndef _TEST
	gint num = 0;
	gint total = g_slist_length (files);
#endif

	g_return_val_if_fail(files != NULL, "No files for archiving");

	debug_print("File: %s\n", archive_name);
	arch = archive_write_new();
	switch (method) {
		case GZIP:
#if ARCHIVE_VERSION_NUMBER < 3000000
			if (archive_write_set_compression_gzip(arch) != ARCHIVE_OK)
#else
			if (archive_write_add_filter_gzip(arch) != ARCHIVE_OK)
#endif
				return archive_error_string(arch);
			break;
		case BZIP2:
#if ARCHIVE_VERSION_NUMBER < 3000000
			if (archive_write_set_compression_bzip2(arch) != ARCHIVE_OK)
#else
			if (archive_write_add_filter_bzip2(arch) != ARCHIVE_OK)
#endif
				return archive_error_string(arch);
			break;
		case COMPRESS:
#if ARCHIVE_VERSION_NUMBER < 3000000
			if (archive_write_set_compression_compress(arch) != ARCHIVE_OK)
#else
			if (archive_write_add_filter_compress(arch) != ARCHIVE_OK)
#endif
    			        return archive_error_string(arch);
			break;
#if ARCHIVE_VERSION_NUMBER >= 2006990
		case LZMA:
#if ARCHIVE_VERSION_NUMBER < 3000000
			if (archive_write_set_compression_lzma(arch) != ARCHIVE_OK)
#else
			if (archive_write_add_filter_lzma(arch) != ARCHIVE_OK)
#endif
				return archive_error_string(arch);
			break;
		case XZ:
#if ARCHIVE_VERSION_NUMBER < 3000000
			if (archive_write_set_compression_xz(arch) != ARCHIVE_OK)
#else
			if (archive_write_add_filter_xz(arch) != ARCHIVE_OK)
#endif
				return archive_error_string(arch);
			break;
#endif
#if ARCHIVE_VERSION_NUMBER >= 3000000
		case LZIP:
			if (archive_write_add_filter_lzip(arch) != ARCHIVE_OK)
				return archive_error_string(arch);
			break;
#endif
#if ARCHIVE_VERSION_NUMBER >= 3001000
		case LRZIP:
			if (archive_write_add_filter_lrzip(arch) != ARCHIVE_OK)
				return archive_error_string(arch);
			break;
		case LZOP:
			if (archive_write_add_filter_lzop(arch) != ARCHIVE_OK)
				return archive_error_string(arch);
			break;
		case GRZIP:
			if (archive_write_add_filter_grzip(arch) != ARCHIVE_OK)
				return archive_error_string(arch);
			break;
#endif
#if ARCHIVE_VERSION_NUMBER >= 3001900
		case LZ4:
			if (archive_write_add_filter_lz4(arch) != ARCHIVE_OK)
				return archive_error_string(arch);
			break;
#endif
		case NO_COMPRESS:
#if ARCHIVE_VERSION_NUMBER < 3000000
			if (archive_write_set_compression_none(arch) != ARCHIVE_OK)
#else
			if (archive_write_add_filter_none(arch) != ARCHIVE_OK)
#endif
				return archive_error_string(arch);
			break;
	}
	switch (format) {
		case TAR:
			if (archive_write_set_format_ustar(arch) != ARCHIVE_OK)
				return archive_error_string(arch);
			break;
		case SHAR:
			if (archive_write_set_format_shar(arch) != ARCHIVE_OK)
				return archive_error_string(arch);
			break;
		case PAX:
			if (archive_write_set_format_pax(arch) != ARCHIVE_OK)
				return archive_error_string(arch);
			break;
		case CPIO:
			if (archive_write_set_format_cpio(arch) != ARCHIVE_OK)
				return archive_error_string(arch);
			break;
		case NO_FORMAT:
			return "Missing archive format";
	}
#if ARCHIVE_VERSION_NUMBER < 3000000
	if (archive_write_open_file(arch, archive_name) != ARCHIVE_OK)
#else
	if (archive_write_open_filename(arch, archive_name) != ARCHIVE_OK)
#endif
		return archive_error_string(arch);

	while (files && ! stop_action) {
		struct file_info* file;
		gchar* filename = NULL;

#ifndef _TEST
		set_progress_print_all(num++, total, 30);
#endif
		file = (struct file_info *) files->data;
		if (!file)
			continue;
		filename = get_full_path(file);
		/* libarchive will crash if instructed to add archive to it self */
		if (g_utf8_collate(archive_name, filename) == 0) {
			g_warning("%s: not dumping to '%s'", archive_name, filename);
#ifndef _TEST
			debug_print("%s: not dumping to '%s'\n", archive_name, filename);
#endif
		}
		else {
			struct archive_entry* entry;
			char* buf = NULL;
			ssize_t len;
			GError* err = NULL;
			GStatBuf st;
			int fd;
			gchar* msg = NULL;

#ifndef _TEST
			debug_print("Adding: %s\n", filename);
			msg = g_strdup_printf("%s", filename);
			set_progress_file_label(msg);
			g_free(msg);
#endif
			if ((fd = g_open(filename, O_RDONLY, 0)) == -1) {
				FILE_OP_ERROR(filename, "g_open");
			}
			else {
				if (g_stat(filename, &st) == -1) {
					FILE_OP_ERROR(filename, "g_stat");
				} else {
					entry = archive_entry_new();
					archive_entry_copy_stat(entry, &st);
					archive_entry_set_pathname(entry, filename);
					if (S_ISLNK(st.st_mode)) {

						buf = g_file_read_link(filename, &err);
						if (err) {
							FILE_OP_ERROR(filename, "g_file_read_link");
							g_clear_error(&err);
						} else {
							archive_entry_set_symlink(entry, buf);
							g_free(buf);
							archive_entry_set_size(entry, 0);
							archive_write_header(arch, entry);
						}
					}
					else {
						if (archive_write_header(arch, entry) != ARCHIVE_OK)
							g_warning("%s", archive_error_string(arch));
						if ((buf = malloc(READ_BLOCK_SIZE)) != NULL) {
							len = read(fd, buf, READ_BLOCK_SIZE);
							while (len > 0) {
								if (archive_write_data(arch, buf, len) == -1)
									g_warning("%s", archive_error_string(arch));
								memset(buf, 0, READ_BLOCK_SIZE);
								len = read(fd, buf, READ_BLOCK_SIZE);
							}
							g_free(buf);
						}
					}
					archive_entry_free(entry);
				}
				if (!g_close(fd, &err) || err) {
					FILE_OP_ERROR(filename, "g_close");
					if (err)
						g_clear_error(&err);
                }
			}
		}
		g_free(filename);
		files = g_slist_next(files);
	}
#ifndef _TEST
	if (stop_action)
		unlink(archive_name);
	stop_action = FALSE;
#endif
	archive_write_close(arch);
#if ARCHIVE_VERSION_NUMBER < 3000000
	archive_write_finish(arch);
#else
	archive_write_free(arch);
#endif
	return NULL;
}

#ifdef _TEST
void archive_scan_folder(const char* dir) {
	GStatBuf st;
	DIR* root;
	struct dirent* ent;
	gchar cwd[PATH_MAX];
	gchar path[PATH_MAX];
	
	getcwd(cwd, PATH_MAX);

	if (g_stat(dir, &st) == -1)
		return;
	if (! S_ISDIR(st.st_mode))
		return;
	if (!(root = opendir(dir)))
		return;
	chdir(dir);

	while ((ent = readdir(root)) != NULL) {
		if (strcmp(".", ent->d_name) == 0 || strcmp("..", ent->d_name) == 0)
			continue;
		if (g_stat(ent->d_name, &st) == -1) {
			FILE_OP_ERROR(filename, "g_stat");
			continue;
		}
		sprintf(path, "%s/%s", dir, ent->d_name);
		if (S_ISREG(st.st_mode) || S_ISLNK(st.st_mode)) {
			archive_add_file(path);
		}
		else if (S_ISDIR(st.st_mode)) {
			archive_scan_folder(path);
		}
	}
	chdir(cwd);
	closedir(root);
}

int main(int argc, char** argv) {
	char* archive = NULL;
	char buf[PATH_MAX];
	int pid;
	int opt;
	int perm = ARCHIVE_EXTRACT_PERM | ARCHIVE_EXTRACT_TIME |
		ARCHIVE_EXTRACT_ACL | ARCHIVE_EXTRACT_FFLAGS | ARCHIVE_EXTRACT_SECURE_SYMLINKS;
	gchar cwd[PATH_MAX];
	gboolean remove = FALSE;
	const char *p = NULL;
	int res;

	getcwd(cwd, PATH_MAX);

	while (*++argv && **argv == '-') {
		p = *argv + 1;

		while ((opt = *p++) != '\0') {
			switch(opt) {
				case 'a':
					if (*p != '\0')
						archive = (char *) p;
					else
						archive = *++argv;
					p += strlen(p);
					break;
				case 'r':
					remove = TRUE;
					break;
			}
		}
	}
	if (! archive) {
		fprintf(stderr, "Missing archive name!\n");
		return EXIT_FAILURE;
	}
	if (!*argv) {
		fprintf(stderr, "Expected arguments after options!\n");
		return EXIT_FAILURE;
	}
	
	while (*argv) {
		archive_scan_folder(*argv++);
		res = archive_create(archive, file_list);
		if (res != ARCHIVE_OK) {
			fprintf(stderr, "%s: Creating archive failed\n", archive);
			return EXIT_FAILURE;
		}
	}
	pid = (int) getpid();
	sprintf(buf, "/tmp/%d", pid);
	fprintf(stdout, "Creating: %s\n", buf);
	mkdir(buf, 0700);
	chdir(buf);
	if (strcmp(dirname(archive), ".") == 0) 
		sprintf(buf, "%s/%s", cwd, basename(archive));
	else
		sprintf(buf, "%s", archive);
	archive_extract(buf, perm);
	chdir(cwd);
	if (remove) {
		sprintf(buf, "rm -rf /tmp/%d", pid);
		fprintf(stdout, "Executing: %s\n", buf);
		system(buf);
	}
	archive_free_list(file_list);
	return EXIT_SUCCESS;
}
#endif

void archiver_set_tooltip(GtkWidget* widget, gchar* text) {
    gtk_widget_set_tooltip_text(widget, text);
    g_free(text);
}
