/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2020-2022 the Claws Mail team
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
 * 
 */

#ifdef HAVE_CONFIG_H
#include "claws-features.h"
#endif

#ifdef USE_OAUTH2

#include <glib.h>

#include "socket.h"
#include "passwordstore.h"
#include "smtp.h"
#include "prefs_account.h"

#define OAUTH2BUFSIZE		8192

typedef enum
{
	OA2_BASE_URL,
	OA2_CLIENT_ID,
	OA2_CLIENT_SECRET,
	OA2_REDIRECT_URI,
	OA2_AUTH_RESOURCE,
	OA2_ACCESS_RESOURCE,
	OA2_REFRESH_RESOURCE,
	OA2_RESPONSE_TYPE,
	OA2_SCOPE_FOR_AUTH,
	OA2_GRANT_TYPE_ACCESS,
	OA2_GRANT_TYPE_REFRESH,
	OA2_TENANT,
	OA2_STATE,
	OA2_ACCESS_TYPE,
	OA2_SCOPE_FOR_ACCESS,
	OA2_RESPONSE_MODE,
	OA2_HEADER_AUTH_BASIC
} Oauth2Params;

typedef enum
{
	OAUTH2AUTH_NONE,
	OAUTH2AUTH_GOOGLE,
	OAUTH2AUTH_OUTLOOK,
	OAUTH2AUTH_EXCHANGE,
	OAUTH2AUTH_YAHOO,
	OAUTH2AUTH_LAST = OAUTH2AUTH_YAHOO
} Oauth2Service;

typedef struct _OAUTH2Data OAUTH2Data;
struct _OAUTH2Data
{
	gchar *refresh_token;
	gchar *access_token;
        gint expiry;
        gchar *expiry_str;
        gchar *custom_client_id;
        gchar *custom_client_secret;
};

gint oauth2_init (OAUTH2Data *OAUTH2Data);
gint oauth2_check_passwds (PrefsAccount *ac_prefs);
gint oauth2_obtain_tokens (Oauth2Service provider, OAUTH2Data *OAUTH2Data, const gchar *authcode);
gint oauth2_authorisation_url (Oauth2Service provider, gchar **url, const gchar *custom_client_id);
gint oauth2_use_refresh_token (Oauth2Service provider, OAUTH2Data *OAUTH2Data);
guchar* oauth2_decode(const gchar *in);
void oauth2_encode(const gchar *in);

#endif	/* USE_GNUTLS */
