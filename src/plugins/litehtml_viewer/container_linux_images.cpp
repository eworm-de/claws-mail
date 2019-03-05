/*
 * Claws Mail -- A GTK+ based, lightweight, and fast e-mail client
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

#include "container_linux.h"
#include "http.h"
#include "lh_prefs.h"

static GdkPixbuf *lh_get_image(const litehtml::tchar_t* url)
{
	GError *error = NULL;
	GdkPixbuf *pixbuf = NULL;
	http* http_loader = NULL;

	if (!lh_prefs_get()->enable_remote_content) {
		debug_print("blocking download of image from '%s'\n", url);
		return NULL;
	}

	debug_print("allowing download of image from '%s'\n", url);

	http_loader = new http();
	GInputStream *image = http_loader->load_url(url, &error);

	if (error || !image) {
		if (error) {
			g_warning("lh_get_image: Could not create pixbuf %s",
					error->message);
			g_clear_error(&error);
		}
		goto theend;
	}

	pixbuf = gdk_pixbuf_new_from_stream(image, NULL, &error);
	if (error) {
		g_warning("lh_get_image: Could not create pixbuf %s",
				error->message);
		pixbuf = NULL;
		g_clear_error(&error);
	}

theend:
	if (http_loader) {
		delete http_loader;
	}

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

	if (pixbuf != NULL) {
		ctx->container->add_image_to_cache(ctx->url, pixbuf);
		ctx->container->redraw(true);
	}

	g_free(ctx->url);
	g_free(ctx);
}

void container_linux::load_image( const litehtml::tchar_t* src, const litehtml::tchar_t* baseurl, bool redraw_on_ready )
{
	litehtml::tstring url;
	make_url(src, baseurl, url);
	bool found = false;

	lock_images_cache();

	for (auto ii = m_images.cbegin(); ii != m_images.cend(); ++ii) {
		const image *i = &(*ii);

		if (!strcmp(i->first.c_str(), url.c_str())) {
			found = true;
			break;
		}
	}

	unlock_images_cache();

	if(!found) {
		struct FetchCtx *ctx = g_new(struct FetchCtx, 1);
		ctx->url = g_strdup(url.c_str());
		ctx->container = this;

		GTask *task = g_task_new(this, NULL, get_image_callback, ctx);
		g_task_set_task_data(task, ctx, NULL);
		g_task_run_in_thread(task, get_image_threaded);
	} else {
		debug_print("found image in cache: '%s'\n", url.c_str());
	}
}

void container_linux::get_image_size( const litehtml::tchar_t* src, const litehtml::tchar_t* baseurl, litehtml::size& sz )
{
	litehtml::tstring url;
	make_url(src, baseurl, url);
	bool found = false;
	const image *img = NULL;

	lock_images_cache();

	for (auto ii = m_images.cbegin(); ii != m_images.cend(); ++ii) {
		const image *i = &(*ii);
		if (i->first == url) {
			img = i;
			found = true;
			break;
		}
	}

	if(img != NULL)
	{
		sz.width	= gdk_pixbuf_get_width(img->second);
		sz.height	= gdk_pixbuf_get_height(img->second);
	} else
	{
		sz.width	= 0;
		sz.height	= 0;
	}

	unlock_images_cache();
}

void container_linux::add_image_to_cache(const gchar *url, GdkPixbuf *image)
{
	g_return_if_fail(url != NULL);
	g_return_if_fail(image != NULL);

	debug_print("adding image to cache: '%s'\n", url);
	lock_images_cache();
	m_images.push_back(std::make_pair(url, image));
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
		image *img = &(*i);

		if (img->second) {
			g_object_unref(img->second);
		}
	}

	m_images.clear();

	unlock_images_cache();
}

gint container_linux::clear_images(gint desired_size)
{
	gint size = 0;
	gint num = 0;

	lock_images_cache();

	/* First, tally up size of all the stored GdkPixbufs and
	 * deallocate those which make the total size be above
	 * the desired_size limit. We will remove their list
	 * elements later. */
	for (auto i = m_images.rbegin(); i != m_images.rend(); ++i) {
		image *img = &(*i);
		gint cursize;

		if (img->second == NULL)
			continue;

		cursize = gdk_pixbuf_get_byte_length(img->second);

		if (size + cursize > desired_size) {
			g_object_unref(img->second);
			img->second = NULL;
			num++;
		} else {
			size += cursize;
		}
	}

	/* Remove elements whose GdkPixbuf pointers point to NULL. */
	m_images.remove_if([&](image _img) -> bool {
			if (_img.second == NULL)
				return true;
			return false;
			});

	unlock_images_cache();

	return num;
}
