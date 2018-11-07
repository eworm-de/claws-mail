#include <glib.h>
#include <glib/gstdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <curl/curl.h>
#include <gio/gio.h>

#define HTTP_GET_TIMEOUT 5L

class http
{
    CURL*           curl;

public:
    http();
    ~http();

    GInputStream *load_url(const gchar *url, GError **error);

private:
    static size_t curl_write_data(char* ptr, size_t size, size_t nmemb, void* data_ptr);
    static void destroy_giostream(gpointer data);
};


