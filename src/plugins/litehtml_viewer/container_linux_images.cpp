/*
 * Claws Mail -- A GTK based, lightweight, and fast e-mail client
 * Copyright(C) 2019 the Claws Mail Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write tothe Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#include "claws-features.h"
#endif

#include <set>
#include "common/utils.h"

#include "container_linux.h"
#include "http.h"
#include "lh_prefs.h"

typedef std::pair<litehtml::tstring, struct timeval> lru_entry;

static GdkPixbuf *lh_get_image(const litehtml::tchar_t* url)
{
	GError *error = NULL;
	GdkPixbuf *pixbuf = NULL;

	http* http_loader = new http();
	GInputStream *image = http_loader->load_url(url, &error);

	if (error || !image) {
		if (error) {
			g_warning("lh_get_image: Could not create pixbuf for '%s': %s",
				url, error->message);
			g_clear_error(&error);
		}
		goto theend;
	}

	pixbuf = gdk_pixbuf_new_from_stream(image, NULL, &error);
	if (error) {
		g_warning("lh_get_image: Could not create pixbuf for '%s': %s",
			url, error->message);
		pixbuf = NULL;
		g_clear_error(&error);
	}

theend:
	delete http_loader;

	return pixbuf;
}

struct FetchCtx {
	container_linux *container;
	gchar *url;
};

static void get_image_threaded(GTask *task, gpointer source, gpointer task_data, GCancellable *cancellable)
{
	struct FetchCtx *ctx = (struct FetchCtx *)task_data;
	GdkPixbuf *pixbuf = lh_get_image(ctx->url);

	g_task_return_pointer(task, pixbuf, NULL);
}

static void get_image_callback(GObject *source, GAsyncResult *res, gpointer user_data)
{
	GdkPixbuf *pixbuf;
	struct FetchCtx *ctx = (struct FetchCtx *)user_data;

	pixbuf = GDK_PIXBUF(g_task_propagate_pointer(G_TASK(res), NULL));

	ctx->container->update_image_cache(ctx->url, pixbuf);
	ctx->container->rerender();

	g_free(ctx->url);
	g_free(ctx);
}

void container_linux::load_image( const litehtml::tchar_t* src, const litehtml::tchar_t* baseurl, bool redraw_on_ready )
{
	litehtml::tstring url;
	make_url(src, baseurl, url);
	bool request = false;
	struct timeval last;

	gettimeofday(&last, NULL);

	lock_images_cache();

	auto i = m_images.find(url);
	if(i == m_images.end()) {
		/* Attached images can be loaded into cache right here. */
		if (!strncmp(src, "cid:", 4)) {
			GdkPixbuf *pixbuf = get_local_image(src);

			if (pixbuf != NULL)
				m_images.insert(std::make_pair(src, std::make_pair(pixbuf, last)));

			unlock_images_cache();
			return;
		} else {
			if (!lh_prefs_get()->enable_remote_content) {
				debug_print("blocking download of image from '%s'\n", src);
				unlock_images_cache();
				return;
			}

			request = true;
			m_images.insert(std::make_pair(url, std::make_pair((GdkPixbuf *)NULL, last)));
		}
	} else {
		debug_print("found image cache entry: %p '%s'\n", i->second.first, url.c_str());
		i->second.second = last;
	}

	unlock_images_cache();

	if (request) {
		struct FetchCtx *ctx;

		debug_print("allowing download of image from '%s'\n", src);

		ctx = g_new(struct FetchCtx, 1);
		ctx->url = g_strdup(url.c_str());
		ctx->container = this;

		GTask *task = g_task_new(NULL, NULL, get_image_callback, ctx);
		g_task_set_task_data(task, ctx, NULL);
		g_task_run_in_thread(task, get_image_threaded);
	}
}

void container_linux::get_image_size( const litehtml::tchar_t* src, const litehtml::tchar_t* baseurl, litehtml::size& sz )
{
	litehtml::tstring url;
	make_url(src, baseurl, url);
	const GdkPixbuf *img = NULL;

	lock_images_cache();

	auto i = m_images.find(url);
	if (i != m_images.end() && i->second.first) {
		img = i->second.first;
		sz.width = gdk_pixbuf_get_width(img);
		sz.height = gdk_pixbuf_get_height(img);
	} else {
		sz.width = 0;
		sz.height = 0;
	}

	unlock_images_cache();
}

void container_linux::update_image_cache(const gchar *url, GdkPixbuf *image)
{
	g_return_if_fail(url != NULL);

	debug_print("updating image cache: %p '%s'\n", image, url);
	lock_images_cache();
	auto i = m_images.find(url);
	if(i == m_images.end()) {
		g_warning("image '%s' was not found in pixbuf cache", url);
		unlock_images_cache();
		return;
	}

	if(i->second.first != NULL && i->second.first != image) {
		g_warning("pixbuf pointer for image '%s' changed", url);
		g_object_unref(i->second.first);
	}

	if(image == NULL) {
		/* A null pixbuf pointer presumably means the download failed,
		 * so remove the cache entry to allow for future retries. */
		debug_print("warning - new pixbuf for '%s' is null\n", url);
		m_images.erase(i);
		unlock_images_cache();
		return;
	}

	i->second.first = image;
	unlock_images_cache();
}

void container_linux::lock_images_cache(void)
{
	g_rec_mutex_lock(&m_images_lock);
}

void container_linux::unlock_images_cache(void)
{
	g_rec_mutex_unlock(&m_images_lock);
}

void container_linux::clear_images()
{
	lock_images_cache();

	for(auto i = m_images.begin(); i != m_images.end(); ++i) {
		if (i->second.first) {
			g_object_unref(i->second.first);
		}
	}

	m_images.clear();

	unlock_images_cache();
}

gint container_linux::clear_images(gsize desired_size)
{
	gsize size = 0;
	gint num = 0;

	lock_images_cache();

	/* First, remove all local images - the ones with "cid:" URL. */
	for (auto i = m_images.begin(); i != m_images.end(); ) {
		if (!strncmp(i->first.c_str(), "cid:", 4)) {
			g_object_unref(i->second.first);
			i = m_images.erase(i);
			num++;
		} else {
			++i;
		}
	}

	/* Second, build an LRU list */
	auto lru_comp_func = [](const lru_entry& l1, const lru_entry& l2) {
		return timercmp(&l1.second, &l2.second, <);
	};
	std::set<lru_entry, decltype(lru_comp_func)> lru(lru_comp_func);

	for (auto i = m_images.begin(); i != m_images.end(); ++i) {
		lru.insert(std::make_pair(i->first, i->second.second));
	}

	/*
	for (auto l = lru.begin(); l != lru.end(); l++) {
		debug_print("lru dump: %d %d %s\n", l->second.tv_sec, l->second.tv_usec, l->first.c_str());
	}
	*/

	/* Last, walk the LRU list and remove the oldest entries that push it over
	 * the desired size limit */
	for (auto l = lru.rbegin(); l != lru.rend(); ++l) {
		gsize cursize;

		auto i = m_images.find(l->first);

		if(i == m_images.end()) {
			g_warning("failed to find '%s' in m_images", l->first.c_str());
			continue;
		}

		if(i->second.first == NULL) {
			/* This should mean that the download is still in progress */
			debug_print("warning - trying to prune a null pixbuf for %s\n", i->first.c_str());
			continue;
		}

		cursize = gdk_pixbuf_get_byte_length(i->second.first);
		/*
		debug_print("clear_images: desired_size %d - size %d - cursize %d - %d %d %s\n",
			desired_size, size, cursize, l->second.tv_sec, l->second.tv_usec, l->first.c_str());
		*/
		if (size + cursize > desired_size) {
			debug_print("pruning %s from image cache\n", i->first.c_str());
			g_object_unref(i->second.first);
			m_images.erase(i);
			num++;
		} else {
			size += cursize;
		}
	}

	unlock_images_cache();

	return num;
}
