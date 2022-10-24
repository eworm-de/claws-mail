/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2022 the Claws Mail team
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#define G_LOG_DOMAIN "Fancy-Web-Ext"

#include <glib-object.h>
#include <webkit2/webkit-web-extension.h>

static gboolean load_remote_content = FALSE;

static gboolean web_page_send_request_cb(WebKitWebPage *web_page,
	WebKitURIRequest *request,
	WebKitURIResponse *redirected_response,
	gpointer user_data)
{
	gboolean is_remote = TRUE;
	gboolean should_block;
	const char *request_uri = webkit_uri_request_get_uri(request);
#if GLIB_CHECK_VERSION(2,66,0)
	const char *scheme = g_uri_peek_scheme(request_uri);
#else
	gchar *scheme = g_uri_parse_scheme(request_uri);
#endif
	if (scheme == NULL)
		return TRUE;
#if GLIB_CHECK_VERSION(2,66,0)
	if (strcmp(scheme, "cid") == 0 ||
	    strcmp(scheme, "file") == 0 ||
	    strcmp(scheme, "about") == 0)
 		is_remote = FALSE;
#else
	if (g_ascii_strcasecmp(scheme, "cid") == 0 ||
		g_ascii_strcasecmp(scheme, "file") == 0 ||
		g_ascii_strcasecmp(scheme, "about") == 0)
		is_remote = FALSE;
	g_free(scheme);
#endif
	if (is_remote)
		should_block = !load_remote_content;
	else
		should_block = FALSE;

	g_debug("Filter page: %s request: %s => %s uri %s",
		webkit_web_page_get_uri(web_page),
		request_uri,
		is_remote ? "remote" : "local",
		should_block ? "blocked" : "allowed");

	return should_block;
}

static gboolean web_page_user_message_received_cb(WebKitWebPage *page,
	WebKitUserMessage *message,
	gpointer user_data)
{
	const char *name = webkit_user_message_get_name(message);

	g_debug("WebPage user message received callback - %s", name);

	if (g_strcmp0(name, "LoadRemoteContent") == 0) {
		GVariant *parameters;

		parameters = webkit_user_message_get_parameters(message);
		if (!parameters)
			return FALSE;

		load_remote_content = g_variant_get_boolean(parameters);

		g_debug("LoadRemoteContent param - %d", load_remote_content);

		return TRUE;
	}

	return FALSE;
}

static void web_page_created_cb(WebKitWebExtension *extension,
	WebKitWebPage *web_page,
	gpointer user_data)
{
	g_debug("Page %ld created for %s",
		webkit_web_page_get_id(web_page),
		webkit_web_page_get_uri(web_page));

	g_signal_connect_object(web_page,
		"send-request",
		G_CALLBACK(web_page_send_request_cb),
		NULL,
		0);

	g_signal_connect_object(web_page,
		"user-message-received",
		G_CALLBACK(web_page_user_message_received_cb),
		NULL,
		0);
}

G_MODULE_EXPORT void webkit_web_extension_initialize(WebKitWebExtension *extension)
{
	if (!g_setenv("G_MESSAGES_DEBUG", G_LOG_DOMAIN, FALSE))
		g_warning("could not set G_MESSAGES_DEBUG\n");

	g_debug("Initializing Fancy web process extension");

	g_signal_connect(extension,
		"page-created",
		G_CALLBACK(web_page_created_cb),
		NULL);
}
