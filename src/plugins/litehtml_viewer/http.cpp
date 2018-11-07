#include "http.h"

http::http()
{
    curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, HTTP_GET_TIMEOUT);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE, 120L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPINTVL, 60L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, http::curl_write_data);
    response_data = NULL;
    response_size = 0;;
}

http::~http()
{
    curl_easy_cleanup(curl);
    if (response_data) {
        g_free(response_data);
    }
}

size_t http::curl_write_data(char* ptr, size_t size, size_t nmemb, void* data) {
	if (!response_data)
		response_data = (char *) malloc(size * nmemb);
	else
		response_data = (char *) realloc(response_data, response_size + size * nmemb);
	if (response_data) {
		memcpy(response_data + response_size, ptr, size * nmemb);
		response_size += size * nmemb;
	}
	return size * nmemb;
}

void http::destroy_giostream(gpointer data) {
	GInputStream* gio;
	if (data) {
		gio = G_INPUT_STREAM(data);
		g_input_stream_close(gio, NULL, NULL);
		gio = NULL;
	}
}

GInputStream *http::load_url(const gchar *url, GError **error)
{
	GError* _error = NULL;
	CURLcode res = CURLE_OK;
	gsize len;
	gchar* content;
    GInputStream* stream = NULL;


	if (!strncmp(url, "file:///", 8) || g_file_test(url, G_FILE_TEST_EXISTS)) {
		gchar* newurl = g_filename_from_uri(url, NULL, NULL);
		if (g_file_get_contents(newurl ? newurl : url, &content, &len, &_error)) {
			stream = g_memory_input_stream_new_from_data(content, len, http::destroy_giostream);
		} else {
			g_log(NULL, G_LOG_LEVEL_MESSAGE, "%s", _error->message);
		}
		g_free(newurl);
	} else {
		if (!curl) return NULL;
	    curl_easy_setopt(curl, CURLOPT_URL, url);
	    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_data);
	    res = curl_easy_perform(curl);
	    if (res != CURLE_OK) {
		    _error = g_error_new_literal(G_FILE_ERROR, res, curl_easy_strerror(res));
	    } else {
	        stream = g_memory_input_stream_new_from_data(response_data, response_size, http::destroy_giostream);
	    }
	}

	if (error && _error) *error = _error;

	return stream;
}

