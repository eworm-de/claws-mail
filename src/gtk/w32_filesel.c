/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2016 The Claws Mail Team
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
 *
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#define UNICODE
#define _UNICODE

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdkwin32.h>
#include <pthread.h>

#include <windows.h>
#include <shlobj.h>

#include "claws.h"
#include "alertpanel.h"
#include "manage_window.h"
#include "utils.h"

static OPENFILENAME o;
static BROWSEINFO b;

/* Since running the native dialogs in the same thread stops GTK
 * loop from redrawing other windows on the background, we need
 * to run the dialogs in a separate thread. */

/* TODO: There's a lot of code repeat in this file, it could be
 * refactored to be neater. */

struct _WinChooserCtx {
	void *data;
	gboolean return_value;
	PIDLIST_ABSOLUTE return_value_pidl;
	gboolean done;
};

typedef struct _WinChooserCtx WinChooserCtx;

static void *threaded_GetOpenFileName(void *arg)
{
	WinChooserCtx *ctx = (WinChooserCtx *)arg;

	g_return_val_if_fail(ctx != NULL, NULL);
	g_return_val_if_fail(ctx->data != NULL, NULL);

	ctx->return_value = GetOpenFileName(ctx->data);
	ctx->done = TRUE;

	return NULL;
}

static void *threaded_GetSaveFileName(void *arg)
{
	WinChooserCtx *ctx = (WinChooserCtx *)arg;

	g_return_val_if_fail(ctx != NULL, NULL);
	g_return_val_if_fail(ctx->data != NULL, NULL);

	ctx->return_value = GetSaveFileName(ctx->data);
	ctx->done = TRUE;

	return NULL;
}

static void *threaded_SHBrowseForFolder(void *arg)
{
	WinChooserCtx *ctx = (WinChooserCtx *)arg;

	g_return_val_if_fail(ctx != NULL, NULL);
	g_return_val_if_fail(ctx->data != NULL, NULL);

	ctx->return_value_pidl = SHBrowseForFolder(ctx->data);

	ctx->done = TRUE;

	return NULL;
}

/* This function handles calling GetOpenFilename(), using
 * global static variable o.
 * It expects o.lpstrFile to point to an already allocated buffer,
 * of size at least MAXPATHLEN. */
static const gboolean _file_open_dialog(const gchar *path, const gchar *title,
		const gchar *filter, const gboolean multi)
{
	gboolean ret;
	gunichar2 *path16 = NULL;
	gunichar2 *title16 = NULL;
	gunichar2 *filter16 = NULL;
	gunichar2 *win_filter16 = NULL;
	glong conv_items, sz;
	GError *error = NULL;
	WinChooserCtx *ctx;
#ifdef USE_PTHREAD
	pthread_t pt;
#endif

	/* Path needs to be converted to UTF-16, so that the native chooser
	 * can understand it. */
	path16 = g_utf8_to_utf16(path ? path : "",
			-1, NULL, NULL, &error);
	if (error != NULL) {
		alertpanel_error(_("Could not convert file path to UTF-16:\n\n%s"),
				error->message);
		debug_print("file path '%s' conversion to UTF-16 failed\n", path);
		g_error_free(error);
		error = NULL;
		return FALSE;
	}

	/* Chooser dialog title needs to be UTF-16 as well. */
	title16 = g_utf8_to_utf16(title ? title : "",
			-1, NULL, NULL, &error);
	if (error != NULL) {
		debug_print("dialog title '%s' conversion to UTF-16 failed\n", title);
		g_error_free(error);
		error = NULL;
	}

	o.lStructSize = sizeof(OPENFILENAME);
	if (focus_window != NULL)
		o.hwndOwner = GDK_WINDOW_HWND(gtk_widget_get_window(focus_window));
	else
		o.hwndOwner = NULL;
	o.hInstance = NULL;
	o.lpstrFilter = NULL;
	o.lpstrCustomFilter = NULL;
	o.nFilterIndex = 0;
	o.nMaxFile = MAXPATHLEN;
	o.lpstrFileTitle = NULL;
	o.lpstrInitialDir = path16;
	o.lpstrTitle = title16;
	if (multi)
		o.Flags = OFN_LONGNAMES | OFN_EXPLORER | OFN_ALLOWMULTISELECT;
	else
		o.Flags = OFN_LONGNAMES | OFN_EXPLORER;

	if (filter != NULL && strlen(filter) > 0) {
		debug_print("Setting filter '%s'\n", filter);
		filter16 = g_utf8_to_utf16(filter, -1, NULL, &conv_items, &error);
		/* We're creating a UTF16 (2 bytes for each character) string:
		 * "filter\0filter\0\0"
		 * As g_utf8_to_utf16() will stop on first null byte, even if
		 * we pass string length in its second argument, we have to
		 * construct this string manually.
		 * conv_items contains number of UTF16 characters of our filter.
		 * Therefore we need enough bytes to store the filter string twice
		 * and three null chars. */
		sz = sizeof(gunichar2);
		win_filter16 = g_malloc0(conv_items*sz*2 + sz*3);
		memcpy(win_filter16, filter16, conv_items*sz);
		memcpy(win_filter16 + conv_items + 1, filter16, conv_items*sz);
		g_free(filter16);

		if (error != NULL) {
			debug_print("dialog title '%s' conversion to UTF-16 failed\n", title);
			g_error_free(error);
			error = NULL;
		}
		o.lpstrFilter = (LPCTSTR)win_filter16;
		o.nFilterIndex = 1;
	}

	ctx = g_new0(WinChooserCtx, 1);
	ctx->data = &o;
	ctx->done = FALSE;

#ifdef USE_PTHREAD
	if (pthread_create(&pt, NULL, threaded_GetOpenFileName,
				(void *)ctx) != 0) {
		debug_print("Couldn't run in a thread, continuing unthreaded.\n");
		threaded_GetOpenFileName(ctx);
	} else {
		while (!ctx->done) {
			claws_do_idle();
		}
		pthread_join(pt, NULL);
	}
	ret = ctx->return_value;
#else
	debug_print("No threads available, continuing unthreaded.\n");
	ret = GetOpenFileName(&o);
#endif

	g_free(win_filter16);
	if (path16 != NULL) {
		g_free(path16);
	}
	g_free(title16);
	g_free(ctx);

	return ret;
}

gchar *filesel_select_file_open_with_filter(const gchar *title, const gchar *path,
		              const gchar *filter)
{
	gchar *str = NULL;
	GError *error = NULL;

	o.lpstrFile = g_malloc0(MAXPATHLEN);
	if (!_file_open_dialog(path, title, filter, FALSE)) {
		g_free(o.lpstrFile);
		return NULL;
	}

	/* Now convert the returned file path back from UTF-16. */
	str = g_utf16_to_utf8(o.lpstrFile, o.nMaxFile, NULL, NULL, &error);
	if (error != NULL) {
		alertpanel_error(_("Could not convert file path back to UTF-8:\n\n%s"),
				error->message);
		debug_print("returned file path conversion to UTF-8 failed\n");
		g_error_free(error);
	}

	g_free(o.lpstrFile);
	return str;
}

GList *filesel_select_multiple_files_open_with_filter(const gchar *title,
		const gchar *path, const gchar *filter)
{
	GList *file_list = NULL;
	gchar *str = NULL;
	gchar *dir = NULL;
	gunichar2 *f;
	GError *error = NULL;
	glong n, items_read;

	o.lpstrFile = g_malloc0(MAXPATHLEN);
	if (!_file_open_dialog(path, title, filter, TRUE)) {
		g_free(o.lpstrFile);
		return NULL;
	}

	/* Now convert the returned directory and file names back from UTF-16.
	 * The content of o.lpstrFile is:
	 * "directory0file0file0...0file00" for multiple files selected,
	 * "fullfilepath0" for single file. */
	str = g_utf16_to_utf8(o.lpstrFile, -1, &items_read, NULL, &error);

	if (error != NULL) {
		alertpanel_error(_("Could not convert file path back to UTF-8:\n\n%s"),
				error->message);
		debug_print("returned file path conversion to UTF-8 failed\n");
		g_error_free(error);
		return NULL;
	}

	/* The part before the first null char is always a full path. If it is
	 * a path to a file, then only this one file has been selected,
	 * and we can bail out early. */
	if (g_file_test(str, G_FILE_TEST_IS_REGULAR)) {
		debug_print("Selected one file: '%s'\n", str);
		file_list = g_list_append(file_list, g_strdup(str));
		g_free(str);
		return file_list;
	}

	/* So the path was to a directory. We need to parse more after
	 * the fist null char, until we get to two null chars in a row. */
	dir = g_strdup(str);
	g_free(str);
	debug_print("Selected multiple files in dir '%s'\n", dir);

	n = items_read + 1;
	f = &o.lpstrFile[n];
	while (items_read > 0) {
		str = g_utf16_to_utf8(f, -1, &items_read, NULL, &error);
		if (error != NULL) {
			alertpanel_error(_("Could not convert file path back to UTF-8:\n\n%s"),
					error->message);
			debug_print("returned file path conversion to UTF-8 failed\n");
			g_error_free(error);
			g_free(o.lpstrFile);
			return NULL;
		}

		if (items_read > 0) {
			debug_print("selected file '%s'\n", str);
			file_list = g_list_append(file_list,
					g_strconcat(dir, G_DIR_SEPARATOR_S, str, NULL));
		}
		g_free(str);

		n += items_read + 1;
		f = &o.lpstrFile[n];
	}

	g_free(o.lpstrFile);
	return file_list;
}

gchar *filesel_select_file_open(const gchar *title, const gchar *path)
{
	return filesel_select_file_open_with_filter(title, path, NULL);
}

GList *filesel_select_multiple_files_open(const gchar *title, const gchar *path)
{
	return filesel_select_multiple_files_open_with_filter(title, path, NULL);
}

gchar *filesel_select_file_save(const gchar *title, const gchar *path)
{
	gboolean ret;
	gchar *str, *filename = NULL;
	gunichar2 *filename16, *path16, *title16;
	glong conv_items;
	GError *error = NULL;
	WinChooserCtx *ctx;
#ifdef USE_PTHREAD
	pthread_t pt;
#endif

	/* Find the filename part, if any */
	if (path == NULL || path[strlen(path)-1] == G_DIR_SEPARATOR) {
		filename = "";
	} else if ((filename = strrchr(path, G_DIR_SEPARATOR)) != NULL) {
		filename++;
	} else {
		filename = (char *) path;
	}

	/* Convert it to UTF-16. */
	filename16 = g_utf8_to_utf16(filename, -1, NULL, &conv_items, &error);
	if (error != NULL) {
		alertpanel_error(_("Could not convert attachment name to UTF-16:\n\n%s"),
				error->message);
		debug_print("filename '%s' conversion to UTF-16 failed\n", filename);
		g_error_free(error);
		return NULL;
	}

	/* Path needs to be converted to UTF-16, so that the native chooser
	 * can understand it. */
	path16 = g_utf8_to_utf16(path, -1, NULL, NULL, &error);
	if (error != NULL) {
		alertpanel_error(_("Could not convert file path to UTF-16:\n\n%s"),
				error->message);
		debug_print("file path '%s' conversion to UTF-16 failed\n", path);
		g_error_free(error);
		g_free(filename16);
		return NULL;
	}

	/* Chooser dialog title needs to be UTF-16 as well. */
	title16 = g_utf8_to_utf16(title, -1, NULL, NULL, &error);
	if (error != NULL) {
		debug_print("dialog title '%s' conversion to UTF-16 failed\n", title);
		g_error_free(error);
	}

	o.lStructSize = sizeof(OPENFILENAME);
	if (focus_window != NULL)
		o.hwndOwner = GDK_WINDOW_HWND(gtk_widget_get_window(focus_window));
	else
		o.hwndOwner = NULL;
	o.lpstrFilter = NULL;
	o.lpstrCustomFilter = NULL;
	o.lpstrFile = g_malloc0(MAXPATHLEN);
	if (path16 != NULL)
		memcpy(o.lpstrFile, filename16, conv_items * sizeof(gunichar2));
	o.nMaxFile = MAXPATHLEN;
	o.lpstrFileTitle = NULL;
	o.lpstrInitialDir = path16;
	o.lpstrTitle = title16;
	o.Flags = OFN_LONGNAMES | OFN_EXPLORER;

	ctx = g_new0(WinChooserCtx, 1);
	ctx->data = &o;
	ctx->return_value = FALSE;
	ctx->done = FALSE;

#ifdef USE_PTHREAD
	if (pthread_create(&pt, NULL, threaded_GetSaveFileName,
				(void *)ctx) != 0) {
		debug_print("Couldn't run in a thread, continuing unthreaded.\n");
		threaded_GetSaveFileName(ctx);
	} else {
		while (!ctx->done) {
			claws_do_idle();
		}
		pthread_join(pt, NULL);
	}
	ret = ctx->return_value;
#else
	debug_print("No threads available, continuing unthreaded.\n");
	ret = GetSaveFileName(&o);
#endif

	g_free(filename16);
	g_free(path16);
	g_free(title16);
	g_free(ctx);

	if (!ret) {
		g_free(o.lpstrFile);
		return NULL;
	}

	/* Now convert the returned file path back from UTF-16. */
	str = g_utf16_to_utf8(o.lpstrFile, o.nMaxFile, NULL, NULL, &error);
	if (error != NULL) {
		alertpanel_error(_("Could not convert file path back to UTF-8:\n\n%s"),
				error->message);
		debug_print("returned file path conversion to UTF-8 failed\n");
		g_error_free(error);
	}

	g_free(o.lpstrFile);
	return str;
}

/* This callback function is used to set the folder browse dialog
 * selection from filesel_select_file_open_folder() to set
 * chosen starting folder ("path" argument to that function. */
static int CALLBACK _open_folder_callback(HWND hwnd, UINT uMsg,
		LPARAM lParam, LPARAM lpData)
{
	if (uMsg != BFFM_INITIALIZED)
		return 0;

	SendMessage(hwnd, BFFM_SETSELECTION, TRUE, lpData);
	return 0;
}

gchar *filesel_select_file_open_folder(const gchar *title, const gchar *path)
{
	PIDLIST_ABSOLUTE pidl;
	gchar *str;
	gunichar2 *path16, *title16;
	glong conv_items;
	GError *error = NULL;
	WinChooserCtx *ctx;
#ifdef USE_PTHREAD
	pthread_t pt;
#endif

	/* Path needs to be converted to UTF-16, so that the native chooser
	 * can understand it. */
	path16 = g_utf8_to_utf16(path ? path : "",
			-1, NULL, &conv_items, &error);
	if (error != NULL) {
		alertpanel_error(_("Could not convert file path to UTF-16:\n\n%s"),
				error->message);
		debug_print("file path '%s' conversion to UTF-16 failed\n", path);
		g_error_free(error);
		return NULL;
	}

	/* Chooser dialog title needs to be UTF-16 as well. */
	title16 = g_utf8_to_utf16(title ? title : "",
			-1, NULL, NULL, &error);
	if (error != NULL) {
		debug_print("dialog title '%s' conversion to UTF-16 failed\n", title);
		g_error_free(error);
	}

	if (focus_window != NULL)
		b.hwndOwner = GDK_WINDOW_HWND(gtk_widget_get_window(focus_window));
	else
		b.hwndOwner = NULL;
	b.pszDisplayName = g_malloc(MAXPATHLEN);
	b.lpszTitle = title16;
	b.ulFlags = 0;
	b.pidlRoot = NULL;
	b.lpfn = _open_folder_callback;
	b.lParam = (LPARAM)path16;

	CoInitialize(NULL);

	ctx = g_new0(WinChooserCtx, 1);
	ctx->data = &b;
	ctx->done = FALSE;

#ifdef USE_PTHREAD
	if (pthread_create(&pt, NULL, threaded_SHBrowseForFolder,
				(void *)ctx) != 0) {
		debug_print("Couldn't run in a thread, continuing unthreaded.\n");
		threaded_SHBrowseForFolder(ctx);
	} else {
		while (!ctx->done) {
			claws_do_idle();
		}
		pthread_join(pt, NULL);
	}
	pidl = ctx->return_value_pidl;
#else
	debug_print("No threads available, continuing unthreaded.\n");
	pidl = SHBrowseForFolder(&b);
#endif

	g_free(b.pszDisplayName);
	g_free(title16);
	g_free(path16);

	if (pidl == NULL) {
		CoUninitialize();
		g_free(ctx);
		return NULL;
	}

	path16 = malloc(MAX_PATH);
	if (!SHGetPathFromIDList(pidl, path16)) {
		CoTaskMemFree(pidl);
		CoUninitialize();
		g_free(path16);
		g_free(ctx);
		return NULL;
	}

	/* Now convert the returned file path back from UTF-16. */
	/* Unfortunately, there is no field in BROWSEINFO struct to indicate
	 * actual length of string in pszDisplayName, so we have to assume
	 * the string is null-terminated. */
	str = g_utf16_to_utf8(path16, -1, NULL, NULL, &error);
	if (error != NULL) {
		alertpanel_error(_("Could not convert file path back to UTF-8:\n\n%s"),
				error->message);
		debug_print("returned file path conversion to UTF-8 failed\n");
		g_error_free(error);
	}
	CoTaskMemFree(pidl);
	CoUninitialize();
	g_free(ctx);
	g_free(path16);

	return str;
}

gchar *filesel_select_file_save_folder(const gchar *title, const gchar *path)
{
	return filesel_select_file_open_folder(title, path);
}
