#ifndef __RSSYL_PREFS
#define __RSSYL_PREFS

#define PREFS_BLOCK_NAME	"rssyl"

#define PREF_DEFAULT_REFRESH	"180"

typedef struct _RPrefs RPrefs;

struct _RPrefs {
	gboolean refresh_enabled;
	gint refresh;
	gboolean refresh_on_startup;
	gchar *cookies_path;
	gboolean ssl_verify_peer;
};

typedef struct _RPrefsPage RPrefsPage;

struct _RPrefsPage {
	PrefsPage page;
	GtkWidget *refresh_enabled;
	GtkWidget *refresh;
	GtkWidget *refresh_on_startup;
	GtkWidget *cookies_path;
	GtkWidget *ssl_verify_peer;
};

void rssyl_prefs_init(void);
void rssyl_prefs_done(void);
RPrefs *rssyl_prefs_get(void);

#endif /* __RSSYL_PREFS */
