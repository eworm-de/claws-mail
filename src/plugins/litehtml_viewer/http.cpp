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
#endif

#include <string.h>
#include "http.h"

extern "C" {
#include "ssl.h"
#include "utils.h"
}

struct Data {
  GInputStream *memory;
  size_t size;
};

static size_t write_data(char* ptr, size_t size, size_t nmemb, void* data_ptr) {
	struct Data* data = (struct Data *) data_ptr;
	size_t realsize = size * nmemb;

	g_memory_input_stream_add_data((GMemoryInputStream *)data->memory,
#if !GLIB_CHECK_VERSION (2, 68, 0)
		g_memdup(ptr, realsize),
#else
		g_memdup2(ptr, realsize),
#endif
		realsize,
		g_free);
	data->size += realsize;
    
	return realsize;
}

http::http()
{
    curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, HTTP_GET_TIMEOUT);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE, 120L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPINTVL, 60L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
#ifdef G_OS_WIN32
    curl_easy_setopt(curl, CURLOPT_CAINFO, claws_ssl_get_cert_file());
#endif
    stream = NULL;
}

http::~http()
{
    curl_easy_cleanup(curl);
    destroy_giostream();
}

void http::destroy_giostream() {
    debug_print("destroy_giostream called.\n");
    if (stream) {
	debug_print("Freeing input_stream\n");
	g_input_stream_close(stream, NULL, NULL);
	g_object_unref(stream);
    }
}

GInputStream *http::load_url(const gchar *url, GError **error)
{
	GError* _error = NULL;
	CURLcode res = CURLE_OK;
	gsize len;
	gchar* content;
    
	if (!strncmp(url, "file:///", 8) || g_file_test(url, G_FILE_TEST_EXISTS)) {
		gchar* newurl = g_filename_from_uri(url, NULL, NULL);
		if (g_file_get_contents(newurl ? newurl : url, &content, &len, &_error)) {
			stream = g_memory_input_stream_new_from_data(content, len, g_free);
		} else {
			debug_print("Got error: %s\n", _error->message);
		}
		g_free(newurl);
	} else {
		struct Data data;

		if (!curl) return NULL;

                data.memory = g_memory_input_stream_new();
                data.size = 0;

		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&data);
		res = curl_easy_perform(curl);

		if (res != CURLE_OK) {
			_error = g_error_new_literal(G_FILE_ERROR, res, curl_easy_strerror(res));
			g_object_unref(data.memory);
		} else {
			debug_print("Image size: %" G_GSIZE_FORMAT "\n", data.size);
			stream = data.memory;
		}
	}

	if (error && _error) *error = _error;

	return stream;
}
