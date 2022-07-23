/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2014-2015 Ricardo Mones and the Claws Mail Team
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

#include "config.h"

#include <stdio.h>

#include "libravatar_federation.h"
#include "utils.h"
#include "gtkutils.h"

#define MISSING "x"

#if defined USE_GNUTLS
static GHashTable *federated = NULL;

/**
 * Get the associated avatar URL for a domain.
 *
 * @param domain Domain to get the URL for.
 *
 * @return The avatar URL for the domain or NULL if not found.
 */
static gchar *get_federated_url_for_domain(const gchar *domain)
{
	gchar *found;

	if (federated == NULL) {
		return NULL;
	}

	found = (gchar *) g_hash_table_lookup(federated, domain);

	if (found != NULL)
		debug_print("cached avatar url for domain %s found: %s\n", domain, found);
	else	
		debug_print("cached avatar url for domain %s not found\n", domain);

	return found;
}

/**
 * Adds a URL for a domain.
 *
 * @param url  The computed avatar URL.
 * @param domain  Associated domain.
 */
static void add_federated_url_for_domain(const gchar *url, const gchar *domain)
{
	if (url == NULL)
		return;

	if (federated == NULL)
		federated = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

	debug_print("new cached avatar url for domain %s: %s\n", domain, url);
	g_hash_table_insert(federated, g_strdup(domain), g_strdup(url)); 
}
#endif

/**
 * Retrieves the federated URL for a given email address.
 *
 * @param address  The email address.
 *
 * @return The avatar URL for the domain of the address. 
 */
gchar *federated_url_for_address(const gchar *address)
{
#if defined USE_GNUTLS
	gchar *domain = NULL, *last = NULL, *addr = NULL, *url = NULL;
	gchar *host = NULL;
	guint16 port = 0;

	if (address == NULL || *address == '\0')
		goto invalid_addr;

	addr = g_strdup(address);
        domain = strchr(addr, '@');
	if (domain == NULL)
		goto invalid_addr;
 
	++domain;
   	if (strlen(domain) < 5)
		goto invalid_addr;

	last = domain;
	while (*last != '\0' && *last != ' ' && *last != '\t' && *last != '>')
		++last;
	*last = '\0';

	/* try cached domains */
	url = get_federated_url_for_domain(domain);
	if (url != NULL) {
		g_free(addr);
		if (!strcmp(url, MISSING)) {
			return NULL;
		}
		return g_strdup(url);
	}

	/* not cached, try secure service first */
	if (auto_configure_service_sync("avatars-sec", domain, &host, &port)) {
		if (port != 443) {
			url = g_strdup_printf("https://%s:%d/avatar", host, port);
		} else {
			url = g_strdup_printf("https://%s/avatar", host);
		}
	} else { /* try standard one if no secure service available */
		if (auto_configure_service_sync("avatars", domain, &host, &port)) {
			if (port != 80) {
				url = g_strdup_printf("http://%s:%d/avatar", host, port);
			} else {
				url = g_strdup_printf("http://%s/avatar", host);
			}
		} else {
			debug_print("libravatar federated domain for %s not found\n", domain);
		}
	}
	if (url != NULL) {
		add_federated_url_for_domain(url, domain);
	} else {
		add_federated_url_for_domain(MISSING, domain);
	}

	g_free(addr);
	return url;

invalid_addr:
	if (addr != NULL)
		g_free(addr);

	debug_print("invalid address for libravatar federated domain\n");
	return NULL;
#else
	debug_print("federated domains disabled (built without GnuTLS support)\n");
	return NULL;
#endif
}

