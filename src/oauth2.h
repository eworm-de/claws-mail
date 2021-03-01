/*
 * Claws Mail -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2020 the Claws Mail team
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

//Yahoo requires token requests to send POST header Authorization: Basic 
//where the password is Base64 encoding of client_id:client_secret

static gchar *OAUTH2info[4][17]={
  {"accounts.google.com", 
   "G/jjil7/XHfv4mw90hhhFy5hRci8NeOF3w7QtX8hb9yljE+mU0/MvGk3G4RoUWK13phSIZ7+JSSg4R2f1RV2NbaT5DODMMt5", 
   "cABm8Lx5PgnrUOOwNJSamcG8Nlj8g8go", 
   "urn:ietf:wg:oauth:2.0:oob", 
   "/o/oauth2/auth", 
   "/o/oauth2/token", 
   "/o/oauth2/token", 
   "code", 
   "https://mail.google.com", 
   "authorization_code", 
   "refresh_token",
   "",
   "",
   "",
   "",
   "",
   ""},
  {"login.microsoftonline.com", 
   "Srm4tajDIHKiu25KIxOlaqei+AJ8q/DPT7PNOhskKrzIjlGT", 
   "",
   "https://login.microsoftonline.com/common/oauth2/nativeclient",
   "/common/oauth2/v2.0/authorize",
   "/common/oauth2/v2.0/token",
   "/common/oauth2/v2.0/token",
   "code",
   "wl.imap offline_access",
   "authorization_code",
   "refresh_token",
   "common",
   "",
   "offline",
   "wl.imap offline_access",
   "fragment",
   ""},
  {"login.microsoftonline.com", 
   "Srm4tajDIHKiu25KIxOlaqei+AJ8q/DPT7PNOhskKrzIjlGT", 
   "",
   "https://login.microsoftonline.com/common/oauth2/nativeclient",
   "/common/oauth2/v2.0/authorize",
   "/common/oauth2/v2.0/token",
   "/common/oauth2/v2.0/token",
   "code",
   "offline_access https://outlook.office.com/IMAP.AccessAsUser.All https://outlook.office.com/POP.AccessAsUser.All https://outlook.office.com/SMTP.Send",
   "authorization_code",
   "refresh_token",
   "common",
   "",
   "offline",
   "offline_access https://outlook.office.com/IMAP.AccessAsUser.All https://outlook.office.com/POP.AccessAsUser.All https://outlook.office.com/SMTP.Send",
   "fragment",
   ""},
  {"api.login.yahoo.com",
   "TTzJciHB9+id6C5eZ1lhRQJVGy8GNYh+iXh8nhiD3cofx5zi4xHLN7Y/IWASKh4Oy7cghOQCs8Q1kmKB2xRWlKP8/fFNXSBFNYpni83PHGUUKgbTYJUz+3/nLLOJASYf", 
   "T/PyRkrw/ByaZ8mkn6aISpsXhci/fieo+ibj1aRkkqhUKqPKeeH7Xg==", 
   "oob",
   "/oauth2/request_auth",
   "/oauth2/get_token",
   "/oauth2/get_token",
   "code",
   "",
   "authorization_code",
   "refresh_token",
   "",
   "",
   "",
   "",
   "",
   "1"}
};

typedef enum
{
	OAUTH2AUTH_NONE,
	OAUTH2AUTH_GOOGLE,
	OAUTH2AUTH_OUTLOOK,
	OAUTH2AUTH_EXCHANGE,
	OAUTH2AUTH_YAHOO,
	OAUTH2AUTH_LAST = OAUTH2AUTH_YAHOO
} Oauth2Service;

static gchar *OAUTH2CodeMarker[5][2] = {
    {"",""},
    {"google_begin_mark","google_end_mark"},
    {"#code=","&session_state"},
    {"#code=","&session_state"},
    {"yahoo_begin_mark","yahoo_end_mark"}
};

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
