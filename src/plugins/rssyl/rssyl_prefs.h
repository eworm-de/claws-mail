#ifndef __RSSYL_PREFS
#define __RSSYL_PREFS

#define PREFS_BLOCK_NAME	"rssyl"

#define RSSYL_NUM_PREFS		4

#define RSSYL_PREF_DEFAULT_REFRESH	"180"
#define RSSYL_PREF_DEFAULT_EXPIRED	"-1"

typedef struct _RSSylPrefs RSSylPrefs;

struct _RSSylPrefs {
	gint refresh;
	gint expired;
	gboolean refresh_on_startup;
	gchar *cookies_path;
};

typedef struct _RSSylPrefsPage RSSylPrefsPage;

struct _RSSylPrefsPage {
	PrefsPage page;
	GtkWidget *refresh;
	GtkWidget *expired;
	GtkWidget *refresh_on_startup;
	GtkWidget *cookies_path;
};

void rssyl_prefs_init(void);
void rssyl_prefs_done(void);
RSSylPrefs *rssyl_prefs_get(void);

#endif /* __RSSYL_PREFS */
