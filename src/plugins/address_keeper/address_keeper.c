/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2009-2015 Ricardo Mones and the Claws Mail Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
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

#include "version.h"
#include "address_keeper.h"
#include "address_keeper_prefs.h"
#include "addr_compl.h"
#include "addrbook.h"
#include "codeconv.h"
#include "prefs_common.h"

/** Identifier for the hook. */
static gulong hook_id = HOOK_NONE;

/**
 * Extracts name from an address.
 *
 * @param addr The full address.
 * @return The name found in the address as a newly allocated string, or NULL if
 * not found.
 */
gchar *get_name_from_addr(const gchar *addr)
{
	gchar *name = NULL;

	if (addr == NULL || *addr == '\0')
		return NULL;
	name = strchr(addr, '@');
	if (name == NULL)
		return NULL;
	--name;
	while (name >= addr && !g_ascii_isspace(*name)) --name;
	while (name >= addr && g_ascii_isspace(*name)) --name;
	if (name > addr) {
		++name; /* recover non-space char */
		return g_strndup(addr, name - addr);
	}
	return NULL;
}

/**
 * Extracts comment from an address.
 *
 * @param addr The full address.
 * @return The comment found in the address as a newly allocated string, or NULL if
 * not found.
 */
gchar *get_comment_from_addr(const gchar *addr)
{
	gchar *comm = NULL;

	if (addr == NULL || *addr == '\0')
		return NULL;
	comm = strchr(addr, '@');
	if (comm == NULL)
		return NULL;
	++comm;
	while (*comm && !g_ascii_isspace(*comm)) ++comm;
	while (*comm && g_ascii_isspace(*comm)) ++comm;
	if (*comm)
		return g_strdup(comm);
	return NULL;
}

/**
 * Checks an address for matching a blocked address pattern.
 *
 * @param addr The full address.
 * @param blocked The regexp matching blocked addresses.
 *
 * @return TRUE if given address matches any of the patterns, FALSE otherwise.
 */
gboolean matches_blocked_address(gchar *addr, MatcherList *blocked)
{
	if (blocked != NULL) {
		MsgInfo info;

		info.subject = addr;
		return matcherlist_match(blocked, &info);
	}
	return FALSE;
}

/**
 * Saves an address to the configured addressbook folder if not known.
 *
 * @param abf The address book file containing target folder.
 * @param folder The address book folder where addresses are added.
 * @param addr The address to be added.
 * @param blocked The regexp matching blocked addresses.
 */
void keep_if_unknown(AddressBookFile * abf, ItemFolder * folder, gchar *addr, MatcherList *blocked)
{
	gchar *clean_addr = NULL;
	gchar *keepto = addkeeperprefs.addressbook_folder;

	debug_print("checking addr '%s'\n", addr);
	if (matches_blocked_address(addr, blocked)) {
		debug_print("addr '%s' is blocked by regexp\n", addr);
		return;
	}
	clean_addr = g_strdup(addr);
	extract_address(clean_addr);
	start_address_completion(NULL);
	if (complete_matches_found(clean_addr) == 0) {
		gchar *a_name;
		gchar *a_comment;
		debug_print("adding addr '%s' to addressbook '%s'\n",
			    clean_addr, keepto);
		a_name = get_name_from_addr(addr);
		a_comment = get_comment_from_addr(addr);
		if (!addrbook_add_contact(abf, folder, a_name, clean_addr, a_comment)) {
			g_warning("contact could not be added");
		} else {
			addressbook_refresh();
		}
		if (a_name != NULL)
			g_free(a_name);
		if (a_comment != NULL)
			g_free(a_comment);
	} else {
		debug_print("found addr '%s' in addressbook '%s', skipping\n",
			    clean_addr, keepto);
	}
	end_address_completion();
	g_free(clean_addr);
}

/**
 * Callback function to be called before sending the mail.
 * 
 * @param source The composer to be checked.
 * @param data Additional data.
 *
 * @return FALSE always: we're only annotating addresses.
 */
static gboolean addrk_before_send_hook(gpointer source, gpointer data)
{
	Compose *compose = (Compose *)source;
	AddressDataSource *book = NULL;
	AddressBookFile *abf = NULL;
	ItemFolder *folder = NULL;
	gchar *keepto = addkeeperprefs.addressbook_folder;
	GSList *cur;
	const gchar *to_hdr;
	const gchar *cc_hdr;
	const gchar *bcc_hdr;
	MatcherList *blocked = NULL;

	debug_print("address_keeper invoked!\n");
	if (compose->batch)
		return FALSE;	/* do not check while queuing */

	if (keepto == NULL || *keepto == '\0') {
		g_warning("addressbook folder not configured");
		return FALSE;
	}

	if (!addressbook_peek_folder_exists(keepto, &book, &folder)) {
		g_warning("addressbook folder not found '%s'", keepto);
		return FALSE;
	}
	if (!book) {
		g_warning("addressbook_peek_folder_exists: NULL book");
		return FALSE;
	}
	abf = book->rawDataSource;

	to_hdr = prefs_common_translated_header_name("To:");
	cc_hdr = prefs_common_translated_header_name("Cc:");
	bcc_hdr = prefs_common_translated_header_name("Bcc:");

	if (addkeeperprefs.block_matching_addrs != NULL
			&& addkeeperprefs.block_matching_addrs[0] != '\0') {
		blocked = matcherlist_new_from_lines(addkeeperprefs.block_matching_addrs, FALSE, FALSE);
		if (blocked == NULL)
			g_warning("couldn't allocate matcher");
	}
	for (cur = compose->header_list; cur != NULL; cur = cur->next) {
		gchar *header;
		gchar *entry;
		header = gtk_editable_get_chars(GTK_EDITABLE(
				gtk_bin_get_child(GTK_BIN(
					(((ComposeHeaderEntry *)cur->data)->combo)))), 0, -1);
		entry = gtk_editable_get_chars(GTK_EDITABLE(
				((ComposeHeaderEntry *)cur->data)->entry), 0, -1);
		g_strstrip(entry);
		g_strstrip(header);
		if (*entry != '\0') {
			if (!g_ascii_strcasecmp(header, to_hdr)
				&& addkeeperprefs.keep_to_addrs == TRUE) {
				keep_if_unknown(abf, folder, entry, blocked);
			}
			if (!g_ascii_strcasecmp(header, cc_hdr)
				&& addkeeperprefs.keep_cc_addrs == TRUE) {
				keep_if_unknown(abf, folder, entry, blocked);
			}
			if (!g_ascii_strcasecmp(header, bcc_hdr)
				&& addkeeperprefs.keep_bcc_addrs == TRUE) {
				keep_if_unknown(abf, folder, entry, blocked);
			}
		}
		g_free(header);
		g_free(entry);
	}
	if (blocked != NULL)	
		matcherlist_free(blocked);

	return FALSE;	/* continue sending */
}

/**
 * Initialize plugin.
 *
 * @param error  For storing the returned error message.
 *
 * @return 0 if initialization succeeds, -1 on failure.
 */
gint plugin_init(gchar **error)
{
	if (!check_plugin_version(MAKE_NUMERIC_VERSION(2,9,2,72),
				  VERSION_NUMERIC, PLUGIN_NAME, error))
		return -1;

	hook_id = hooks_register_hook(COMPOSE_CHECK_BEFORE_SEND_HOOKLIST, 
				      addrk_before_send_hook, NULL);
	
	if (hook_id == HOOK_NONE) {
		*error = g_strdup(_("Failed to register check before send hook"));
		return -1;
	}

	address_keeper_prefs_init();

	debug_print("Address Keeper plugin loaded\n");

	return 0;
}

/**
 * Destructor for the plugin.
 * Unregister the callback function and frees matcher.
 */
gboolean plugin_done(void)
{	
	hooks_unregister_hook(COMPOSE_CHECK_BEFORE_SEND_HOOKLIST, hook_id);
	address_keeper_prefs_done();
	debug_print("Address Keeper plugin unloaded\n");
	return TRUE;
}

/**
 * Get the name of the plugin.
 *
 * @return The plugin name (maybe translated).
 */
const gchar *plugin_name(void)
{
	return PLUGIN_NAME;
}

/**
 * Get the description of the plugin.
 *
 * @return The plugin description (maybe translated).
 */
const gchar *plugin_desc(void)
{
	return _("Keeps all recipient addresses in an addressbook folder.");
}

/**
 * Get the kind of plugin.
 *
 * @return The "GTK3" constant.
 */
const gchar *plugin_type(void)
{
	return "GTK3";
}

/**
 * Get the license acronym the plugin is released under.
 *
 * @return The "GPL3+" constant.
 */
const gchar *plugin_licence(void)
{
	return "GPL3+";
}

/**
 * Get the version of the plugin.
 *
 * @return The current version string.
 */
const gchar *plugin_version(void)
{
	return VERSION;
}

/**
 * Get the features implemented by the plugin.
 *
 * @return A constant PluginFeature structure with the features.
 */
struct PluginFeature *plugin_provides(void)
{
	static struct PluginFeature features[] = 
		{ {PLUGIN_OTHER, N_("Address Keeper")},
		  {PLUGIN_NOTHING, NULL}};

	return features;
}

