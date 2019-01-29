#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include "http.h"

#include "utils.h"

struct Data {
  GInputStream *memory;
  size_t size;
};

static size_t write_data(char* ptr, size_t size, size_t nmemb, void* data_ptr) {
    struct Data* data = (struct Data *) data_ptr;
    size_t realsize = size * nmemb;

		g_memory_input_stream_add_data((GMemoryInputStream *)data->memory,
				g_memdup(ptr, realsize), realsize,
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
			debug_print("Image size: %d\n", data.size);
			stream = data.memory;
		}
	}

	if (error && _error) *error = _error;

	return stream;
}
