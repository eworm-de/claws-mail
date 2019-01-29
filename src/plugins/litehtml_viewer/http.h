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
    GInputStream*   stream;

public:
    http();
    ~http();

    GInputStream *load_url(const gchar *url, GError **error);

private:
    void destroy_giostream();
};


