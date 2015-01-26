/*
 * Claws Mail -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2015 Hiroyuki Yamamoto and the Claws Mail Team
 * Copyright (C) 2015 Ricardo Mones
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#include "claws-features.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>

#include "libravatar.h"

#include <gtk/gtk.h>
#include <gtk/filesel.h>

#include "defs.h"
#include "libravatar_prefs.h"
#include "prefs_common.h"
#include "prefs_gtk.h"

#define PREFS_BLOCK_NAME "Libravatar"
#define NUM_DEF_BUTTONS 7
#define CUSTOM_URL_BUTTON_INDEX 6
/* cache interval goes from 1 hour to 30 days */
#define INTERVAL_MIN_H 1.0
#define INTERVAL_MAX_H 720.0
/* timeout interval goes from 0 seconds (= use general timeout value)
   to (general timeout value - 1) seconds */
#define TIMEOUT_MIN_S 0.0

LibravatarPrefs libravatarprefs;
GHashTable *libravatarmisses;

struct LibravatarPrefsPage
{
	PrefsPage page;

	GtkWidget *cache_interval_spin;
	GtkWidget *cache_icons_check;
	GtkWidget *defm_radio[NUM_DEF_BUTTONS];
	GtkWidget *defm_url_text;
	GtkWidget *allow_redirects_check;
#if (defined USE_GNUTLS && GLIB_CHECK_VERSION(2,22,0))
	GtkWidget *allow_federated_check;
#endif
	GtkWidget *timeout;
};

struct LibravatarPrefsPage libravatarprefs_page;

static PrefParam param[] = {
	{ "base_url", "http://cdn.libravatar.org/avatar",
	  &libravatarprefs.base_url,
          P_STRING, NULL, NULL, NULL },
	{ "cache_interval", "24",
	  &libravatarprefs.cache_interval,
          P_INT, NULL, NULL, NULL },
	{ "cache_icons", "TRUE",
	  &libravatarprefs.cache_icons,
          P_BOOL, NULL, NULL, NULL },
	{ "default_mode", "0",
	  &libravatarprefs.default_mode,
          P_INT, NULL, NULL, NULL },
	{ "default_mode_url", "",
	  &libravatarprefs.default_mode_url,
          P_STRING, NULL, NULL, NULL },
	{ "allow_redirects", "TRUE",
	  &libravatarprefs.allow_redirects,
          P_BOOL, NULL, NULL, NULL },
#if (defined USE_GNUTLS && GLIB_CHECK_VERSION(2,22,0))
	{ "allow_federated", "TRUE",
	  &libravatarprefs.allow_federated,
          P_BOOL, NULL, NULL, NULL },
#endif
	{ "timeout", "0",
	  &libravatarprefs.timeout,
          P_INT, NULL, NULL, NULL },
	{NULL, NULL, NULL, P_OTHER, NULL, NULL, NULL}
};

static GtkWidget *create_checkbox(gchar *label, gchar *hint)
{
	GtkWidget *cb = gtk_check_button_new_with_mnemonic(label);
	CLAWS_SET_TIP(cb, hint);
	gtk_widget_show(cb);

	return cb;
}

static void cache_icons_check_toggled_cb(GtkToggleButton *button, gpointer data)
{
	gtk_widget_set_sensitive(libravatarprefs_page.cache_interval_spin,
				 gtk_toggle_button_get_active(button));
}

static GtkWidget *labeled_spinner_box(gchar *label, GtkWidget *spinner, gchar *units, gchar *hint)
{
	GtkWidget *lbl, *lbla, *hbox;

	lbl = gtk_label_new(label);
	gtk_widget_show(lbl);
	lbla = gtk_label_new(units);
	gtk_widget_show(lbla);
	hbox = gtk_hbox_new(FALSE, 6);
	if (hint != NULL) {
		CLAWS_SET_TIP(spinner, hint);
	}
	gtk_box_pack_start(GTK_BOX(hbox), lbl, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), spinner, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), lbla, FALSE, FALSE, 0);

	return hbox;
}

static GtkWidget *p_create_frame_cache(struct LibravatarPrefsPage *page)
{
	GtkWidget *vbox, *checkbox, *spinner, *hbox;
	GtkAdjustment *adj;

	vbox =  gtk_vbox_new(FALSE, 6);

	checkbox = create_checkbox(_("_Use cached icons"),
				   _("Keep icons on disk for reusing instead "
				     "of making another network request"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox),
				     libravatarprefs.cache_icons);
	g_signal_connect(checkbox, "toggled",
			 G_CALLBACK(cache_icons_check_toggled_cb), NULL);
	page->cache_icons_check = checkbox;

	adj = (GtkAdjustment *) gtk_adjustment_new(
					libravatarprefs.cache_interval,
					INTERVAL_MIN_H, INTERVAL_MAX_H, 1.0,
					0.0, 0.0);
	spinner = gtk_spin_button_new(adj, 1.0, 0);
	gtk_widget_show(spinner);
	gtk_widget_set_sensitive(spinner, libravatarprefs.cache_icons);
	hbox = labeled_spinner_box(_("Cache refresh interval"), spinner, _("hours"), NULL);
	page->cache_interval_spin = spinner;

	gtk_box_pack_start(GTK_BOX(vbox), checkbox, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	return vbox;
}

static void default_mode_radio_button_cb(GtkToggleButton *button, gpointer data)
{
	guint mode;
	gboolean is_url;

	if (gtk_toggle_button_get_active(button) != TRUE)
		return;

	mode = *((guint *)data);
	is_url = (mode == DEF_MODE_URL)? TRUE: FALSE;

	gtk_widget_set_sensitive(libravatarprefs_page.defm_url_text, is_url);
	if (is_url) /* custom URL requires following redirects */
		gtk_toggle_button_set_active(
			GTK_TOGGLE_BUTTON(libravatarprefs_page.allow_redirects_check),
			TRUE);

	if (mode == DEF_MODE_NONE) {
		prefs_common.enable_avatars = AVATARS_ENABLE_BOTH;
	} else {
		/* don't waste time with headers that won't be displayed */
		prefs_common.enable_avatars = AVATARS_DISABLE;
		/* empty missing cache when switching to generated */
		g_hash_table_remove_all(libravatarmisses);
	}
}

static const guint radio_value[] = {
	DEF_MODE_NONE,
	DEF_MODE_MM,
	DEF_MODE_IDENTICON,
	DEF_MODE_MONSTERID,
	DEF_MODE_WAVATAR,
	DEF_MODE_RETRO,
	DEF_MODE_URL
};

static GtkWidget *p_create_frame_missing(struct LibravatarPrefsPage *page)
{
	GtkWidget *vbox, *radio[NUM_DEF_BUTTONS], *hbox, *entry;
	gboolean enable = FALSE;
	int i, e = 0;
	gchar *radio_label[] = {
		_("None"),
		_("Mystery man"),
		_("Identicon"),
		_("MonsterID"),
		_("Wavatar"),
		_("Retro"),
		_("Custom URL")
	};
	gchar *radio_hint[] = {
		_("A blank image"),
		_("The unobtrusive low-contrast greyish silhouette"),
		_("A generated geometric pattern"),
		_("A generated full-body monster"),
		_("A generated almost unique face"),
		_("A generated 8-bit arcade-style pixelated image"),
		_("Redirect to a user provided URL")
	};

	vbox =  gtk_vbox_new(FALSE, 6);

	for (i = 0; i < NUM_DEF_BUTTONS; ++i) {
		enable = (libravatarprefs.default_mode == radio_value[i])? TRUE: FALSE;
		e += enable? 1: 0;
		radio[i] = gtk_radio_button_new_with_label_from_widget(
				(i > 0)? GTK_RADIO_BUTTON(radio[i - 1]): NULL, radio_label[i]);
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio[i]), enable);
		if (i == CUSTOM_URL_BUTTON_INDEX) {
			/* set related entry next to radio button */
			entry = gtk_entry_new_with_max_length(MAX_URL_LENGTH);
			CLAWS_SET_TIP(entry, _("Enter the URL you want to be "
				"redirected when no user icon is available. "
				"Leave an empty URL to use the default "
				"libravatar orange icon."));
			gtk_widget_show(entry);
			gtk_entry_set_text(GTK_ENTRY(entry),
				libravatarprefs.default_mode_url);
			hbox = gtk_hbox_new(FALSE, 6);
			gtk_box_pack_start(GTK_BOX(hbox), radio[i], FALSE, FALSE, 0);
			gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);
			gtk_widget_set_sensitive(entry,
				(libravatarprefs.default_mode == DEF_MODE_URL)
				? TRUE: FALSE);
			page->defm_url_text = entry;
			gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
		} else {
			gtk_box_pack_start(GTK_BOX(vbox), radio[i], FALSE, FALSE, 0);
		}
		g_signal_connect(radio[i], "toggled",
				 G_CALLBACK(default_mode_radio_button_cb),
				 (gpointer) &(radio_value[i]));
		CLAWS_SET_TIP(radio[i], radio_hint[i]);
		gtk_widget_show(radio[i]);
		page->defm_radio[i] = radio[i];
	}
	if (e == 0) { /* unknown value, go default */
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio[0]), TRUE);
		libravatarprefs.default_mode = DEF_MODE_NONE;
	}
	/* don't waste time with headers that won't be displayed */
	prefs_common.enable_avatars = (libravatarprefs.default_mode == DEF_MODE_NONE)
						? AVATARS_ENABLE_BOTH: AVATARS_DISABLE;



	return vbox;
}

static GtkWidget *p_create_frame_network(struct LibravatarPrefsPage *page)
{
	GtkWidget *vbox, *chk_redirects, *spinner, *hbox;
	GtkAdjustment *adj;
#if (defined USE_GNUTLS && GLIB_CHECK_VERSION(2,22,0))
	GtkWidget *chk_federated;
#endif

	vbox =  gtk_vbox_new(FALSE, 6);

	chk_redirects = create_checkbox(_("_Allow redirects to other sites"),
				   _("Follow redirect responses received from "
				     "libravatar server to other avatar "
				     "services like gravatar.com"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chk_redirects),
				     libravatarprefs.allow_redirects);
	page->allow_redirects_check = chk_redirects;
	gtk_box_pack_start(GTK_BOX(vbox), chk_redirects, FALSE, FALSE, 0);

#if (defined USE_GNUTLS && GLIB_CHECK_VERSION(2,22,0))
	chk_federated = create_checkbox(_("_Enable federated servers"),
				_("Try to get avatar from sender's domain "
				  "libravatar server"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chk_federated),
				     libravatarprefs.allow_federated);
	page->allow_federated_check = chk_federated;
	gtk_box_pack_start(GTK_BOX(vbox), chk_federated, FALSE, FALSE, 0);
#endif

	adj = (GtkAdjustment *) gtk_adjustment_new(
					libravatarprefs.timeout,
					TIMEOUT_MIN_S,
					(prefs_common_get_prefs()->io_timeout_secs > 0)
					? (prefs_common_get_prefs()->io_timeout_secs - 1)
					: 0,
					1.0, 0.0, 0.0);
	spinner = gtk_spin_button_new(adj, 1.0, 0);
	gtk_widget_show(spinner);
	hbox = labeled_spinner_box(_("Request timeout"), spinner, _("seconds"),
		_("Set to 0 to use global socket I/O timeout. "
                  "Maximum value must be also less than global socket "
                  "I/O timeout."));
	page->timeout = spinner;
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	return vbox;
}

/*
  ┌─Icon cache───────────────────────────────────────────┐
  │ [✔] Use cached icons                                 │
  │ Cache refresh interval [ 24 |⬘] hours                │
  └──────────────────────────────────────────────────────┘
  ┌─Default missing icon mode────────────────────────────┐
  │ (•) None                                             │
  │ ( ) Mystery man                                      │
  │ ( ) Identicon                                        │
  │ ( ) MonsterID                                        │
  │ ( ) Wavatar                                          │
  │ ( ) Retro                                            │
  │ ( ) Custom URL [___________________________________] │
  └──────────────────────────────────────────────────────┘
  ┌─Network──────────────────────────────────────────────┐
  │ [✔] Allow redirects                                  │
  │ [✔] Federated servers                                │
  │ Timeout [ 10 |⬘] seconds                             │
  └──────────────────────────────────────────────────────┘
 */
static void libravatar_prefs_create_widget_func(PrefsPage * _page,
					        GtkWindow * window,
					        gpointer data)
{
	struct LibravatarPrefsPage *page = (struct LibravatarPrefsPage *) _page;
	GtkWidget *vbox, *vbox1, *vbox2, *vbox3, *frame;

	vbox1 = p_create_frame_cache(page);
	vbox2 = p_create_frame_missing(page);
	vbox3 = p_create_frame_network(page);

	vbox = gtk_vbox_new(FALSE, 6);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), VBOX_BORDER);

	PACK_FRAME (vbox, frame, _("Icon cache"));
	gtk_container_set_border_width(GTK_CONTAINER(vbox1), 6);
	gtk_container_add(GTK_CONTAINER(frame), vbox1);

	PACK_FRAME (vbox, frame, _("Default missing icon mode"));
	gtk_container_set_border_width(GTK_CONTAINER(vbox2), 6);
	gtk_container_add(GTK_CONTAINER(frame), vbox2);

	PACK_FRAME (vbox, frame, _("Network"));
	gtk_container_set_border_width(GTK_CONTAINER(vbox3), 6);
	gtk_container_add(GTK_CONTAINER(frame), vbox3);

	gtk_widget_show_all(vbox);
	page->page.widget = vbox;
}

static void libravatar_prefs_destroy_widget_func(PrefsPage *_page)
{
	/* nothing */
}

static void libravatar_save_config(void)
{
	PrefFile *pfile;
	gchar *rcpath;

	debug_print("Saving Libravatar Page\n");

	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, COMMON_RC, NULL);
	pfile = prefs_write_open(rcpath);
	g_free(rcpath);
	if (!pfile || (prefs_set_block_label(pfile, PREFS_BLOCK_NAME) < 0))
		return;

	if (prefs_write_param(param, pfile->fp) < 0) {
		g_warning("Failed to write Libravatar configuration to file\n");
		prefs_file_close_revert(pfile);
		return;
	}
        if (fprintf(pfile->fp, "\n") < 0) {
		FILE_OP_ERROR(rcpath, "fprintf");
		prefs_file_close_revert(pfile);
	} else
	        prefs_file_close(pfile);
}

static void libravatar_prefs_save_func(PrefsPage * _page)
{
	struct LibravatarPrefsPage *page = (struct LibravatarPrefsPage *) _page;
	int i;

	/* cache */
	libravatarprefs.cache_icons = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->cache_icons_check));
	/* cache interval */
	libravatarprefs.cache_interval = gtk_spin_button_get_value_as_int(
		GTK_SPIN_BUTTON(page->cache_interval_spin));
	/* default mode */
	for (i = 0; i < NUM_DEF_BUTTONS; ++i) {
		if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(page->defm_radio[i]))) {
			libravatarprefs.default_mode = radio_value[i];
			break;
		}
	}
	/* custom url */
	if (libravatarprefs.default_mode_url != NULL) {
		g_free(libravatarprefs.default_mode_url);
	}
	libravatarprefs.default_mode_url = gtk_editable_get_chars(
		GTK_EDITABLE(page->defm_url_text), 0, -1);
	/* redirects */
	libravatarprefs.allow_redirects = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->allow_redirects_check));
	/* federation */
#if (defined USE_GNUTLS && GLIB_CHECK_VERSION(2,22,0))
	libravatarprefs.allow_federated = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(page->allow_federated_check));
#endif
	/* timeout */
	libravatarprefs.timeout = gtk_spin_button_get_value_as_int(
		GTK_SPIN_BUTTON(page->timeout));

	libravatar_save_config();
}

void libravatar_prefs_init(void)
{
	static gchar *path[3];
	gchar *rcpath;

	path[0] = _("Plugins");
	path[1] = _("Libravatar");
	path[2] = NULL;

	prefs_set_default(param);
	rcpath = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, COMMON_RC, NULL);
	prefs_read_config(param, PREFS_BLOCK_NAME, rcpath, NULL);
	g_free(rcpath);

	libravatarprefs_page.page.path = path;
	libravatarprefs_page.page.create_widget = libravatar_prefs_create_widget_func;
	libravatarprefs_page.page.destroy_widget = libravatar_prefs_destroy_widget_func;
	libravatarprefs_page.page.save_page = libravatar_prefs_save_func;

	prefs_gtk_register_page((PrefsPage *) &libravatarprefs_page);
}

void libravatar_prefs_done(void)
{
	prefs_gtk_unregister_page((PrefsPage *) &libravatarprefs_page);
}

