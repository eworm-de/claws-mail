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

#include "common/utils.h"

#include "container_linux_images.h"
#include "http.h"

static GdkPixbuf *lh_get_image(const char *url)
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

void get_image_threaded(GTask *task, gpointer source, gpointer task_data, GCancellable *cancellable)
{
	struct FetchCtx *ctx = (struct FetchCtx *)task_data;
	GdkPixbuf *pixbuf = lh_get_image(ctx->url);

	g_task_return_pointer(task, pixbuf, NULL);
}

void get_image_callback(GObject *source, GAsyncResult *res, gpointer user_data)
{
	GdkPixbuf *pixbuf;
	struct FetchCtx *ctx = (struct FetchCtx *)user_data;

	pixbuf = GDK_PIXBUF(g_task_propagate_pointer(G_TASK(res), NULL));

	ctx->container->update_image_cache(ctx->url, pixbuf);
	ctx->container->rerender();

	g_free(ctx->url);
	g_free(ctx);
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
