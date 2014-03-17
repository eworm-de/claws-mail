/*
 * Claws Mail -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2014 Hiroyuki Yamamoto and the Claws Mail Team
 * Copyright (C) 2014 Ricardo Mones
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#include "claws-features.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>

#include <curl/curl.h>

#include "version.h"
#include "libravatar.h"
#include "libravatar_prefs.h"
#include "libravatar_missing.h"
#include "prefs_common.h"
#include "procheader.h"
#include "procmsg.h"
#include "utils.h"
#include "md5.h"

/* indexes of keys are default_mode - 10 if applicable */
static const char *def_mode[] = {
	"404",	/* not used, only useful in web pages */
	"mm",
	"identicon",
	"monsterid",
	"wavatar",
	"retro"
};

static guint update_hook_id;
static guint render_hook_id;
static gchar *cache_dir = NULL; /* dir-separator terminated */

static gboolean libravatar_header_update_hook(gpointer source, gpointer data)
{
	AvatarCaptureData *acd = (AvatarCaptureData *)source;

	debug_print("libravatar avatar_header_update invoked\n");

	if (!strcmp(acd->header, "From:")) {
		gchar *a, *lower;

		a = g_strdup(acd->content);
		extract_address(a);

		/* string to lower */
		for (lower = a; *lower; lower++)
			*lower = g_ascii_tolower(*lower);

		debug_print("libravatar added '%s'\n", a);
		procmsg_msginfo_add_avatar(acd->msginfo, AVATAR_LIBRAVATAR, a);
	}

	return FALSE; /* keep getting */
}

static gchar *federated_base_url_from_address(const gchar *address)
{
	/*
	   TODO: no federation supported right now
	   Details on http://wiki.libravatar.org/running_your_own/
	 */
	return g_strdup(libravatarprefs.base_url);
}

static GtkWidget *image_widget_from_filename(const gchar *filename)
{
	GtkWidget *image = NULL;
	GdkPixbuf *picture = NULL;
	GError *error = NULL;
	gint w, h;

	gdk_pixbuf_get_file_info(filename, &w, &h);

	if (w != AVATAR_SIZE || h != AVATAR_SIZE)
		/* server can provide a different size from the requested in URL */
		picture = gdk_pixbuf_new_from_file_at_scale(
				filename, AVATAR_SIZE, AVATAR_SIZE, TRUE, &error);
	else	/* exact size */
		picture = gdk_pixbuf_new_from_file(filename, &error);

	if (error != NULL) {
		g_warning("Failed to load image '%s': %s\n", filename, error->message);
		g_error_free(error);
	} else {
		if (picture) {
			image = gtk_image_new_from_pixbuf(picture);
			g_object_unref(picture);
		} else
			g_warning("Failed to load image '%s': no error returned!\n", filename);
	}

	return image;
}

static gchar *cache_name_for_md5(const gchar *md5)
{
	if (libravatarprefs.default_mode >= DEF_MODE_MM
			&& libravatarprefs.default_mode <= DEF_MODE_RETRO) {
		/* cache dir for generated avatars */
		return g_strconcat(cache_dir, def_mode[libravatarprefs.default_mode - 10],
				   G_DIR_SEPARATOR_S, md5, NULL);
	}
	/* default cache dir */
	return g_strconcat(cache_dir, md5, NULL);
}

static size_t write_image_data_cb(void *ptr, size_t size, size_t nmemb, void *stream)
{
	size_t written = fwrite(ptr, size, nmemb, (FILE *)stream);
	debug_print("received %zu bytes from avatar server\n", written);

	return written;
}

static GtkWidget *image_widget_from_url(const gchar *url, const gchar *md5)
{
	GtkWidget *image = NULL;
	gchar *filename;
	FILE *file;
	CURL *curl;

	curl = curl_easy_init();
	if (curl == NULL) {
		g_warning("could not initialize curl to get image from url\n");
		return NULL;
	}
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_image_data_cb);

	filename = cache_name_for_md5(md5);
	file = fopen(filename, "wb");
	if (file != NULL) {
		long filesize;

		if (libravatarprefs.allow_redirects) {
			long maxredirs = (libravatarprefs.default_mode == DEF_MODE_MM)? 2L: 1L;

			curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
			curl_easy_setopt(curl, CURLOPT_MAXREDIRS, maxredirs);
		}
		curl_easy_setopt(curl, CURLOPT_FILE, file);
		debug_print("retrieving URL to file: %s -> %s\n", url, filename);
		curl_easy_perform(curl);
		filesize = ftell(file);
    		fclose(file);

		if (filesize < MIN_PNG_SIZE)
			debug_print("not enough data for an avatar image: %ld bytes\n", filesize);
		else
			image = image_widget_from_filename(filename);

		if (!libravatarprefs.cache_icons) {
			if (g_unlink(filename) < 0)
				g_warning("failed to delete cache file %s\n", filename);
		}

		if (filesize == 0)
			missing_add_md5(libravatarmisses, md5);
	} else {
		g_warning("could not open '%s' for writting\n", filename);
	}
	curl_easy_cleanup(curl);
	g_free(filename);

	return image;
}

static gboolean is_recent_enough(const gchar *filename)
{
	struct stat s;
	time_t t;

	if (libravatarprefs.cache_icons) {
		t = time(NULL);
		if (t != (time_t)-1 && !g_stat(filename, &s)) {
			if (t - s.st_ctime <= libravatarprefs.cache_interval * 3600)
				return TRUE;
		}
	}

	return FALSE; /* re-download */
}

static GtkWidget *image_widget_from_cached_md5(const gchar *md5)
{
	GtkWidget *image = NULL;
	gchar *filename;

	filename = cache_name_for_md5(md5);
	if (is_file_exist(filename) && is_recent_enough(filename)) {
		debug_print("found cached image for %s\n", md5);
		image = image_widget_from_filename(filename);
	}
	g_free(filename);

	return image;
}

static gchar *libravatar_url_for_md5(const gchar *base, const gchar *md5)
{
	if (libravatarprefs.default_mode >= DEF_MODE_404) {
		return g_strdup_printf("%s/%s?s=%u&d=%s",
				base, md5, AVATAR_SIZE,
				def_mode[libravatarprefs.default_mode - 10]);
	} else if (libravatarprefs.default_mode == DEF_MODE_URL) {
		return g_strdup_printf("%s/%s?s=%u&d=%s",
				base, md5, AVATAR_SIZE,
				libravatarprefs.default_mode_url);
	} else if (libravatarprefs.default_mode == DEF_MODE_NONE) {
		return g_strdup_printf("%s/%s?s=%u",
				base, md5, AVATAR_SIZE);
	}

	g_warning("invalid libravatar default mode: %d\n", libravatarprefs.default_mode);
	return NULL;
}

static gboolean libravatar_image_render_hook(gpointer source, gpointer data)
{
	AvatarRender *ar = (AvatarRender *)source;
	GtkWidget *image = NULL;
	gchar *a = NULL, *url = NULL;
	gchar md5sum[33];

	debug_print("libravatar avatar_image_render invoked\n");

	a = procmsg_msginfo_get_avatar(ar->full_msginfo, AVATAR_LIBRAVATAR);
	if (a != NULL) {
		gchar *base;

		md5_hex_digest(md5sum, a);
		/* try missing cache */
		if (is_missing_md5(libravatarmisses, md5sum)) {
			return FALSE;
		}
		/* try disk cache */
		image = image_widget_from_cached_md5(md5sum);
		if (image != NULL) {
			if (ar->image) /* previous plugin set one */
				gtk_widget_destroy(ar->image);
			ar->image = image;
			return FALSE;
		}
		/* not cached copy: try network */
		if (prefs_common.work_offline) {
			debug_print("working off-line: libravatar network retrieval skipped\n");
			return FALSE;
		}
		base = federated_base_url_from_address(a);
		url = libravatar_url_for_md5(base, md5sum);
		if (url != NULL) {
			image = image_widget_from_url(url, md5sum);
			g_free(url);
			if (image != NULL) {
				if (ar->image) /* previous plugin set one */
					gtk_widget_destroy(ar->image);
				ar->image = image;
			}
		}
		g_free(base);

		return TRUE;
	}

	return FALSE; /* keep rendering */
}

static gint cache_dir_init()
{
	gchar *subdir;
	int i;

	cache_dir = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
				LIBRAVATAR_CACHE_DIR, G_DIR_SEPARATOR_S,
				NULL);
	if (!is_dir_exist(cache_dir)) {
		if (make_dir(cache_dir) < 0) {
			g_free(cache_dir);
			return -1;
		}
	}
	for (i = DEF_MODE_MM; i <= DEF_MODE_RETRO; ++i) {
		subdir = g_strconcat(cache_dir, def_mode[i - 10], NULL);
		if (!is_dir_exist(subdir)) {
			if (make_dir(subdir) < 0) {
				g_warning("cannot create directory %s\n", subdir);
				g_free(subdir);
				return -1;
			}
		}
		g_free(subdir);
	}

	return 0;
}

static gint missing_cache_init()
{
	gchar *cache_file = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
	                                LIBRAVATAR_CACHE_DIR, G_DIR_SEPARATOR_S,
					LIBRAVATAR_MISSING_FILE, NULL);

	libravatarmisses = missing_load_from_file(cache_file);
	g_free(cache_file);

	if (libravatarmisses == NULL)
		return -1;

	return 0;
}

static void missing_cache_done()
{
	gchar *cache_file;

	if (libravatarmisses != NULL) {
		cache_file = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
					LIBRAVATAR_CACHE_DIR, G_DIR_SEPARATOR_S,
					LIBRAVATAR_MISSING_FILE, NULL);
		missing_save_to_file(libravatarmisses, cache_file);
		g_free(cache_file);
		g_hash_table_destroy(libravatarmisses);
	}
}

/**
 * Initialize plugin.
 *
 * @param error  For storing the returned error message.
 *
 * @return 0 if initialization succeeds, -1 on failure.
 */
gint plugin_init(gchar **error)
{
	if (!check_plugin_version(MAKE_NUMERIC_VERSION(3,9,3,29),
				  VERSION_NUMERIC, _("Libravatar"), error))
		return -1;
	/* get info from headers */
	update_hook_id = hooks_register_hook(AVATAR_HEADER_UPDATE_HOOKLIST,
					     libravatar_header_update_hook,
					     NULL);
	if (update_hook_id == -1) {
		*error = g_strdup(_("Failed to register avatar header update hook"));
		return -1;
	}
	/* get image for displaying */
	render_hook_id = hooks_register_hook(AVATAR_IMAGE_RENDER_HOOKLIST,
					     libravatar_image_render_hook,
					     NULL);
	if (render_hook_id == -1) {
		*error = g_strdup(_("Failed to register avatar image render hook"));
		return -1;
	}
	/* cache dir */
	if (cache_dir_init() == -1) {
		*error = g_strdup(_("Failed to create avatar image cache directory"));
		return -1;
	}
	/* preferences page */
	libravatar_prefs_init();
	/* curl library */
	curl_global_init(CURL_GLOBAL_DEFAULT);
	/* missing cache */
	if (missing_cache_init() == -1) {
		*error = g_strdup(_("Failed to load missing items cache"));
		return -1;
	}
	debug_print("Libravatar plugin loaded\n");

	return 0;
}

/**
 * Destructor for the plugin.
 * Unregister the callback function and frees matcher.
 *
 * @return Always TRUE.
 */
gboolean plugin_done(void)
{
	if (render_hook_id != -1) {
		hooks_unregister_hook(AVATAR_IMAGE_RENDER_HOOKLIST,
				      render_hook_id);
		render_hook_id = -1;
	}
	if (update_hook_id != -1) {
		hooks_unregister_hook(AVATAR_HEADER_UPDATE_HOOKLIST,
				      update_hook_id);
		update_hook_id = -1;
	}
	libravatar_prefs_done();
	missing_cache_done();
	if (cache_dir != NULL)
		g_free(cache_dir);
	debug_print("Libravatar plugin unloaded\n");

	return TRUE;
}

/**
 * Get the name of the plugin.
 *
 * @return The plugin's name, maybe translated.
 */
const gchar *plugin_name(void)
{
	return _("Libravatar");
}

/**
 * Get the description of the plugin.
 *
 * @return The plugin's description, maybe translated.
 */
const gchar *plugin_desc(void)
{
	return _("Display libravatar profiles' images for mail messages. More\n"
		 "info about libravatar at http://www.libravatar.org/. If you have\n"
		 "a gravatar.com profile but not a libravatar one, those will also\n"
		 "be retrieved (when redirections are allowed in plugin config).\n"
		 "Plugin config page it's available from main window at:\n"
		 "/Configuration/Preferences/Plugins/Libravatar.\n\n"
		 "This plugin uses libcurl to retrieve images, so if you're behind a\n"
		 "proxy please refer to curl(1) manpage for details on 'http_proxy'\n"
		 "configuration. More details about this and others on README file.\n\n"
		 "Feedback to <ricardo@mones.org> is welcome.\n");
}

/**
 * Get the kind of plugin.
 *
 * @return The "GTK2" constant.
 */
const gchar *plugin_type(void)
{
	return "GTK2";
}

/**
 * Get the license acronym the plugin is released under.
 *
 * @return The "GPL3+" constant.
 */
const gchar *plugin_licence(void)
{
	return "GPL3+";
}

/**
 * Get the version of the plugin.
 *
 * @return The current version string.
 */
const gchar *plugin_version(void)
{
	return VERSION;
}

/**
 * Get the features implemented by the plugin.
 *
 * @return A constant PluginFeature structure with the features.
 */
struct PluginFeature *plugin_provides(void)
{
	static struct PluginFeature features[] =
		{ {PLUGIN_OTHER, N_("Libravatar")},
		  {PLUGIN_NOTHING, NULL}};

	return features;
}

