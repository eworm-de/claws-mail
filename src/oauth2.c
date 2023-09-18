/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2021-2023 the Claws Mail team
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
#  include "config.h"
#include "claws-features.h"
#endif

#ifdef USE_OAUTH2

#include <glib.h>
#ifdef ENABLE_NLS
#include <glib/gi18n.h>
#else
#define _(a) (a)
#define N_(a) (a)
#endif
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "imap.h"
#include "oauth2.h"
#include "md5.h"
#include "utils.h"
#include "log.h"
#include "time.h"
#include "common/passcrypt.h"
#include "prefs_common.h"

#define GNUTLS_PRIORITY "NORMAL:!VERS-SSL3.0:!VERS-TLS1.0:!VERS-TLS1.1"
//Yahoo requires token requests to send POST header Authorization: Basic
//where the password is Base64 encoding of client_id:client_secret

static gchar *OAUTH2info[4][17]={
  {"accounts.google.com",
   "",
   ".",
   "http://127.0.0.1:8888",
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
   "",
   "",
   "http://127.0.0.1:8888",
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
   "query",
   ""},
  {"login.microsoftonline.com",
   "",
   "",
   "http://127.0.0.1:8888",
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
   "query",
   ""},
  {"api.login.yahoo.com",
   "",
   ".",
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

static gchar *OAUTH2CodeMarker[5][2] = {
    {"",""},
    {"code=","&scope="},
    {"code="," HTTP"},
    {"code=","&session_state="},
    {"yahoo_begin_mark","yahoo_end_mark"} /* Not used since token avalable to user to copy in browser window */
};

static gint oauth2_post_request (gchar *buf, gchar *host, gchar *resource, gchar *header, gchar *body);
static gint oauth2_filter_refresh (gchar *json, gchar *refresh_token);
static gint oauth2_filter_access (gchar *json, gchar *access_token, gint *expiry);
static gint oauth2_contact_server (SockInfo *sock, gchar *request, gchar *response);


static gint oauth2_post_request (gchar *buf, gchar *host, gchar *resource, gchar *header, gchar *body)
{
       gint len;
  
       len = strlen(body);
       if (header[0])
	 return snprintf(buf, OAUTH2BUFSIZE, "POST %s HTTP/1.1\r\nContent-Type: application/x-www-form-urlencoded\r\nAccept: text/html,application/json\r\nContent-Length: %i\r\nHost: %s\r\nConnection: close\r\nUser-Agent: ClawsMail\r\n%s\r\n\r\n%s", resource, len, host, header, body);
       else
	 return snprintf(buf, OAUTH2BUFSIZE, "POST %s HTTP/1.1\r\nContent-Type: application/x-www-form-urlencoded\r\nAccept: text/html,application/json\r\nContent-Length: %i\r\nHost: %s\r\nConnection: close\r\nUser-Agent: ClawsMail\r\n\r\n%s", resource, len, host, body);
}

static gint oauth2_filter_access (gchar *json, gchar *access_token, gint *expiry)
{
       GMatchInfo *matchInfo;
       GRegex *regex;
       
       regex = g_regex_new ("\"access_token\": ?\"(.*?)\",?", G_REGEX_RAW, 0, NULL);
       g_regex_match (regex, json, 0, &matchInfo);
       if (g_match_info_matches (matchInfo)) 
	 g_stpcpy (access_token,g_match_info_fetch (matchInfo, 1));
       else{  
	 g_match_info_free (matchInfo);
	 return (-1);
       }
       
       g_match_info_free (matchInfo);
       
       regex = g_regex_new ("\"expires_in\": ?([0-9]*),?", G_REGEX_RAW, 0, NULL);
       g_regex_match (regex, json, 0, &matchInfo);
       if (g_match_info_matches (matchInfo)){
	 // Reduce available token life to avoid attempting connections with (near) expired tokens
	 *expiry = (g_get_real_time () / G_USEC_PER_SEC) + atoi(g_match_info_fetch (matchInfo, 1)) - 120; 
       }else{
	 g_match_info_free (matchInfo);
	 return (-2);
       }
       
       g_match_info_free (matchInfo);
       
       return(0);
}

static gint oauth2_filter_refresh (gchar *json, gchar *refresh_token)
{
       GMatchInfo *matchInfo;
       GRegex *regex;
       
       regex = g_regex_new ("\"refresh_token\": ?\"(.*?)\",?", G_REGEX_RAW, 0, NULL);
       g_regex_match (regex, json, 0, &matchInfo);
       if (g_match_info_matches (matchInfo)) 
	 g_stpcpy (refresh_token,g_match_info_fetch (matchInfo, 1));
       else{  
	 g_match_info_free (matchInfo);
	 return (-1);
       }
       
       g_match_info_free (matchInfo);
       
       return(0);
}

static gchar* oauth2_get_token_from_response(Oauth2Service provider, const gchar* response) {
	gchar* token = NULL;
	
        debug_print("Auth response: %s\n", response);
        if (provider == OAUTH2AUTH_YAHOO) {
                /* Providers which display auth token in browser for users to copy */
                token = g_strdup(response);
        } else {
                gchar* start = g_strstr_len(response, strlen(response), OAUTH2CodeMarker[provider][0]);
                if (start == NULL)
                        return NULL;
                start += strlen(OAUTH2CodeMarker[provider][0]);
                gchar* stop = g_strstr_len(response, strlen(response), OAUTH2CodeMarker[provider][1]);
                if (stop == NULL)
                        return NULL;
                token = g_strndup(start, stop - start);
        }
	
	return token;
}

int oauth2_obtain_tokens (Oauth2Service provider, OAUTH2Data *OAUTH2Data, const gchar *authcode)
{
	gchar *request;
	gchar *response;
	gchar *body;
	gchar *uri;
	gchar *header;
	gchar *tmp_hd, *tmp_hd_encoded;
	gchar *access_token;
	gchar *refresh_token;
	gint expiry = 0;
	gint ret;
	SockInfo *sock;
	gchar *client_id;
	gchar *client_secret;
        gchar *token = NULL;
        gchar *tmp;
	gint i;

	i = (int)provider - 1;
	if (i < 0 || i > (OAUTH2AUTH_LAST-1))
	  return (1);
    
        token = oauth2_get_token_from_response(provider, authcode);
        debug_print("Auth token: %s\n", token);
        if (token == NULL) {
                log_message(LOG_PROTOCOL, _("OAuth2 missing authorization code\n"));
                return (1);
        }
        debug_print("Connect: %s:443\n", OAUTH2info[i][OA2_BASE_URL]);
	sock = sock_connect(OAUTH2info[i][OA2_BASE_URL], 443);
	if (sock == NULL) {
                log_message(LOG_PROTOCOL, _("OAuth2 connection error\n"));
                g_free(token);
                return (1);
        }
        sock->ssl_cert_auto_accept = TRUE;
	sock->use_tls_sni = TRUE;
	sock_set_nonblocking_mode(sock, FALSE);
	gint timeout_secs = prefs_common_get_prefs()->io_timeout_secs;
	debug_print("Socket timeout: %i sec(s)\n", timeout_secs);
	sock_set_io_timeout(timeout_secs);
	sock->gnutls_priority = GNUTLS_PRIORITY;
        if (ssl_init_socket(sock) == FALSE) {
                log_message(LOG_PROTOCOL, _("OAuth2 TLS connection error\n"));
                g_free(token);
                return (1);
        }

	refresh_token = g_malloc(OAUTH2BUFSIZE+1);	
	access_token = g_malloc(OAUTH2BUFSIZE+1);
	request = g_malloc(OAUTH2BUFSIZE+1);
	response = g_malloc0(OAUTH2BUFSIZE+1);

	if(OAUTH2Data->custom_client_id)
	  client_id = g_strdup(OAUTH2Data->custom_client_id);
	else
	  client_id = oauth2_decode(OAUTH2info[i][OA2_CLIENT_ID]);

        body = g_strconcat ("client_id=", client_id, "&code=", token, NULL);
        debug_print("Body: %s\n", body);
        g_free(token);

	if(OAUTH2info[i][OA2_CLIENT_SECRET][0]){
	  //Only allow custom client secret if the service provider would usually expect a client secret
	  if(OAUTH2Data->custom_client_secret)
	    client_secret = g_strdup(OAUTH2Data->custom_client_secret);
	  else
	    client_secret = oauth2_decode(OAUTH2info[i][OA2_CLIENT_SECRET]);
	  uri = g_uri_escape_string (client_secret, NULL, FALSE);
	  tmp = g_strconcat (body, "&client_secret=", uri, NULL);
	  g_free(body);
          g_free(uri);
	  body = tmp;
	}else{
	  client_secret = g_strconcat ("", NULL);
	}

	if(OAUTH2info[i][OA2_REDIRECT_URI][0]) {
          tmp = g_strconcat(body, "&redirect_uri=", OAUTH2info[i][OA2_REDIRECT_URI], NULL);
	  g_free(body);
	  body = tmp;
	}
	if(OAUTH2info[i][OA2_GRANT_TYPE_ACCESS][0]) {
          tmp = g_strconcat(body, "&grant_type=", OAUTH2info[i][OA2_GRANT_TYPE_ACCESS], NULL);
	  g_free(body);
	  body = tmp;
	}
	if(OAUTH2info[i][OA2_TENANT][0]) {
          tmp = g_strconcat(body, "&tenant=", OAUTH2info[i][OA2_TENANT], NULL);
	  g_free(body);
	  body = tmp;
	}
	if(OAUTH2info[i][OA2_SCOPE_FOR_ACCESS][0]) {
          tmp = g_strconcat(body, "&scope=", OAUTH2info[i][OA2_SCOPE_FOR_ACCESS], NULL);
	  g_free(body);
	  body = tmp;
	}
	if(OAUTH2info[i][OA2_STATE][0]) {
          tmp = g_strconcat(body, "&state=", OAUTH2info[i][OA2_STATE], NULL);
	  g_free(body);
	  body = tmp;
	}

	if(OAUTH2info[i][OA2_HEADER_AUTH_BASIC][0]){
	  tmp_hd = g_strconcat(client_id, ":", client_secret, NULL);
	  tmp_hd_encoded = g_base64_encode (tmp_hd, strlen(tmp_hd));
	  header = g_strconcat ("Authorization: Basic ", tmp_hd_encoded, NULL);
	  g_free(tmp_hd_encoded);
	  g_free(tmp_hd);
	}else{
	  header = g_strconcat ("", NULL);
	}

        debug_print("Complete body: %s\n", body);
	oauth2_post_request (request, OAUTH2info[i][OA2_BASE_URL], OAUTH2info[i][OA2_ACCESS_RESOURCE], header, body);
	ret = oauth2_contact_server (sock, request, response);

	if(oauth2_filter_access (response, access_token, &expiry) == 0){
	  OAUTH2Data->access_token = g_strdup(access_token);
	  OAUTH2Data->expiry = expiry;
	  OAUTH2Data->expiry_str = g_strdup_printf ("%i", expiry);
	  ret = 0;
	  log_message(LOG_PROTOCOL, _("OAuth2 access token obtained\n"));
	}else{
	  log_message(LOG_PROTOCOL, _("OAuth2 access token not obtained\n"));
	  debug_print("OAuth2 - request: %s\n Response: %s", request, response);
	  ret = 1;
	}

	if(oauth2_filter_refresh (response, refresh_token) == 0){
	  OAUTH2Data->refresh_token = g_strdup(refresh_token);
	  log_message(LOG_PROTOCOL, _("OAuth2 refresh token obtained\n"));
	}else{
	  log_message(LOG_PROTOCOL, _("OAuth2 refresh token not obtained\n"));
	}

	sock_close(sock, TRUE);
	g_free(body);
	g_free(header);
	g_free(request);
	g_free(response);
	g_free(client_id);
	g_free(client_secret);
	g_free(access_token);
	g_free(refresh_token);

	return (ret);
}

gint oauth2_use_refresh_token (Oauth2Service provider, OAUTH2Data *OAUTH2Data)
{

	gchar *request;
	gchar *response;
	gchar *body;
	gchar *uri;
	gchar *header;
	gchar *tmp_hd, *tmp_hd_encoded;
	gchar *access_token;
	gchar *refresh_token;
	gint expiry = 0;
	gint ret;
	SockInfo *sock;
	gchar *client_id;
	gchar *client_secret;
	gchar *tmp;
	gint i;

	i = (int)provider - 1;
	if (i < 0 || i > (OAUTH2AUTH_LAST-1))
	  return (1);

	sock = sock_connect(OAUTH2info[i][OA2_BASE_URL], 443);
	if (sock == NULL) {
                log_message(LOG_PROTOCOL, _("OAuth2 connection error\n"));
                return (1);
        }
        sock->ssl_cert_auto_accept = TRUE;
	sock->use_tls_sni = TRUE;
	sock_set_nonblocking_mode(sock, FALSE);
	gint timeout_secs = prefs_common_get_prefs()->io_timeout_secs;
	debug_print("Socket timeout: %i sec(s)\n", timeout_secs);
	sock_set_io_timeout(timeout_secs);
	sock->gnutls_priority = GNUTLS_PRIORITY;
        if (ssl_init_socket(sock) == FALSE) {
                log_message(LOG_PROTOCOL, _("OAuth2 TLS connection error\n"));
                return (1);
        }

	access_token = g_malloc(OAUTH2BUFSIZE+1);
	refresh_token = g_malloc(OAUTH2BUFSIZE+1);
	request = g_malloc(OAUTH2BUFSIZE+1);
	response = g_malloc(OAUTH2BUFSIZE+1);

	if(OAUTH2Data->custom_client_id)
	  client_id = g_strdup(OAUTH2Data->custom_client_id);
	else
	  client_id = oauth2_decode(OAUTH2info[i][OA2_CLIENT_ID]);

	uri = g_uri_escape_string (client_id, NULL, FALSE);
	body = g_strconcat ("client_id=", uri, "&refresh_token=", OAUTH2Data->refresh_token, NULL); 
	g_free(uri);

	if(OAUTH2info[i][OA2_CLIENT_SECRET][0]){
	  //Only allow custom client secret if the service provider would usually expect a client secret
	  if(OAUTH2Data->custom_client_secret)
	    client_secret = g_strdup(OAUTH2Data->custom_client_secret);
	  else
	    client_secret = oauth2_decode(OAUTH2info[i][OA2_CLIENT_SECRET]);
	  uri = g_uri_escape_string (client_secret, NULL, FALSE);
	  tmp = g_strconcat (body, "&client_secret=", uri, NULL);
	  g_free(body);
	  g_free(uri);
	  body = tmp;
	}else{
	  client_secret = g_strconcat ("", NULL);
	}

	if(OAUTH2info[i][OA2_GRANT_TYPE_REFRESH][0]) {
	  uri = g_uri_escape_string (OAUTH2info[i][OA2_GRANT_TYPE_REFRESH], NULL, FALSE);
	  tmp = g_strconcat (body, "&grant_type=", uri, NULL);	
	  g_free(body);
	  g_free(uri);
	  body = tmp;
	}
	if(OAUTH2info[i][OA2_SCOPE_FOR_ACCESS][0]) {
	  uri = g_uri_escape_string (OAUTH2info[i][OA2_SCOPE_FOR_ACCESS], NULL, FALSE);
	  tmp = g_strconcat (body, "&scope=", uri, NULL);
	  g_free(body);
	  g_free(uri);
	  body = tmp;
	}
	if(OAUTH2info[i][OA2_STATE][0]) {
	  uri = g_uri_escape_string (OAUTH2info[i][OA2_STATE], NULL, FALSE);
	  tmp = g_strconcat (body, "&state=", uri, NULL);
	  g_free(body);
	  g_free(uri);
	  body = tmp;
	}

	if(OAUTH2info[i][OA2_HEADER_AUTH_BASIC][0]){
	  tmp_hd = g_strconcat(client_id, ":", client_secret, NULL);
	  tmp_hd_encoded = g_base64_encode (tmp_hd, strlen(tmp_hd));
	  header = g_strconcat ("Authorization: Basic ", tmp_hd_encoded, NULL);
	  g_free(tmp_hd_encoded);
	  g_free(tmp_hd);
	}else{
	  header = g_strconcat ("", NULL);
	}

	oauth2_post_request (request, OAUTH2info[i][OA2_BASE_URL], OAUTH2info[i][OA2_REFRESH_RESOURCE], header, body);
	ret = oauth2_contact_server (sock, request, response);

	if(oauth2_filter_access (response, access_token, &expiry) == 0){
	  OAUTH2Data->access_token = g_strdup(access_token);
	  OAUTH2Data->expiry = expiry;
	  OAUTH2Data->expiry_str = g_strdup_printf ("%i", expiry);
	  ret = 0;
	  log_message(LOG_PROTOCOL, _("OAuth2 access token obtained\n"));
	}else{
	  log_message(LOG_PROTOCOL, _("OAuth2 access token not obtained\n"));
	  debug_print("OAuth2 - request: %s\n Response: %s", request, response);
	  ret = 1;
	}

	if (oauth2_filter_refresh (response, refresh_token) == 0) {
		OAUTH2Data->refresh_token = g_strdup(refresh_token);
		log_message(LOG_PROTOCOL, _("OAuth2 replacement refresh token provided\n"));
	} else
		log_message(LOG_PROTOCOL, _("OAuth2 replacement refresh token not provided\n"));

	debug_print("OAuth2 - access token: %s\n", access_token);
	debug_print("OAuth2 - access token expiry: %i\n", expiry);
	
	sock_close(sock, TRUE);
	g_free(body);
	g_free(header);
	g_free(request);
	g_free(response);
	g_free(client_id);
	g_free(client_secret);
	g_free(access_token);
	g_free(refresh_token);

	return (ret);
}

static gint oauth2_contact_server (SockInfo *sock, gchar *request, gchar *response)
{
	gint len;
	gint ret;
	gchar *token;
	gint toread = OAUTH2BUFSIZE;	
	time_t startplus = time(NULL);
	gchar *tmp;
	len = strlen(request);

	gint timeout_secs = prefs_common_get_prefs()->io_timeout_secs;
	startplus += timeout_secs;
	
	if (sock_write (sock, request, len+1) < 0) {
	  log_message(LOG_PROTOCOL, _("OAuth2 socket write error\n"));
	  return (1);
	}
	
	token = g_strconcat ("", NULL);
	do {
	  
	  ret = sock_read  (sock, response, OAUTH2BUFSIZE);
	  if (ret < 0 && errno == EAGAIN)
	    continue;
	  if (ret < 0) 
	    break;
	  if (ret == 0)
	    break;
	  
	  toread -= ret;
	  tmp = g_strconcat(token, response, NULL);
	  g_free(token);
	  token = tmp;
	} while ((toread > 0) && (time(NULL) < startplus)); 
	
	if(time(NULL) >= startplus)
	  log_message(LOG_PROTOCOL, _("OAuth2 socket timeout error\n"));
	
	g_free(token);
	
	return (0);
}

gint oauth2_authorisation_url (Oauth2Service provider, gchar **url, const gchar *custom_client_id)
{
	gint i;
	gchar *client_id = NULL;
	gchar *tmp;
	gchar *uri;

	i = (int)provider - 1;
	if (i < 0 || i > (OAUTH2AUTH_LAST-1))
	  return (1);
	
	if(!custom_client_id)
	  client_id = oauth2_decode(OAUTH2info[i][OA2_CLIENT_ID]);
	
	uri = g_uri_escape_string (custom_client_id ? custom_client_id : client_id, NULL, FALSE);
	*url = g_strconcat ("https://", OAUTH2info[i][OA2_BASE_URL],OAUTH2info[i][OA2_AUTH_RESOURCE], "?client_id=",
			    uri, NULL);
	g_free(uri);
	if (client_id)
	  g_free(client_id);

	if(OAUTH2info[i][OA2_REDIRECT_URI][0]) {
	  uri = g_uri_escape_string (OAUTH2info[i][OA2_REDIRECT_URI], NULL, FALSE);
	  tmp = g_strconcat (*url, "&redirect_uri=", uri, NULL);
	  g_free(*url);
	  *url = tmp;
	  g_free(uri);
    
	}  
	if(OAUTH2info[i][OA2_RESPONSE_TYPE][0]) {
	  uri = g_uri_escape_string (OAUTH2info[i][OA2_RESPONSE_TYPE], NULL, FALSE);
	  tmp = g_strconcat (*url, "&response_type=", uri, NULL);
	  g_free(*url);
	  *url = tmp;
	  g_free(uri);
	}  
	if(OAUTH2info[i][OA2_SCOPE_FOR_AUTH][0]) {
	  uri = g_uri_escape_string (OAUTH2info[i][OA2_SCOPE_FOR_AUTH], NULL, FALSE);
	  tmp = g_strconcat (*url, "&scope=", uri, NULL);
	  g_free(*url);
	  *url = tmp;
	  g_free(uri);
	}  
	if(OAUTH2info[i][OA2_TENANT][0]) {
	  uri = g_uri_escape_string (OAUTH2info[i][OA2_TENANT], NULL, FALSE);
	  tmp = g_strconcat (*url, "&tenant=", uri, NULL);
	  g_free(*url);
	  *url = tmp;
	  g_free(uri);
	}  
	if(OAUTH2info[i][OA2_RESPONSE_MODE][0]) {
	  uri = g_uri_escape_string (OAUTH2info[i][OA2_RESPONSE_MODE], NULL, FALSE);
	  tmp = g_strconcat (*url, "&response_mode=", uri, NULL);
	  g_free(*url);
	  *url = tmp;
	  g_free(uri);
	}  
	if(OAUTH2info[i][OA2_STATE][0]) {
	  uri = g_uri_escape_string (OAUTH2info[i][OA2_STATE], NULL, FALSE);
	  tmp = g_strconcat (*url, "&state=", uri, NULL);
	  g_free(*url);
	  *url = tmp;
	  g_free(uri);
	}  

	return (0);
}

gint oauth2_check_passwds (PrefsAccount *ac_prefs)
{
	gchar *uid = g_strdup_printf("%d", ac_prefs->account_id);
	gint expiry;
	OAUTH2Data *OAUTH2Data = g_malloc(sizeof(* OAUTH2Data)); 
	gint ret;
	gchar *acc;

	oauth2_init (OAUTH2Data);

	OAUTH2Data->custom_client_id = ac_prefs->oauth2_client_id;
	OAUTH2Data->custom_client_secret = ac_prefs->oauth2_client_secret;
	
	if (passwd_store_has_password(PWS_ACCOUNT, uid, PWS_ACCOUNT_OAUTH2_EXPIRY)) {
		acc = passwd_store_get_account(ac_prefs->account_id, PWS_ACCOUNT_OAUTH2_EXPIRY);
		expiry = atoi(acc);
		g_free(acc);
		if (expiry >  (g_get_real_time () / G_USEC_PER_SEC)) {
			g_free(OAUTH2Data);
			log_message(LOG_PROTOCOL, _("OAuth2 access token still fresh\n"));
			g_free(uid);
			return (0);
		}
	}
	
	if (passwd_store_has_password(PWS_ACCOUNT, uid, PWS_ACCOUNT_OAUTH2_REFRESH)) {
		log_message(LOG_PROTOCOL, _("OAuth2 obtaining access token using refresh token\n"));
		OAUTH2Data->refresh_token = passwd_store_get_account(ac_prefs->account_id, PWS_ACCOUNT_OAUTH2_REFRESH);
		ret = oauth2_use_refresh_token (ac_prefs->oauth2_provider, OAUTH2Data);
	} else if (passwd_store_has_password(PWS_ACCOUNT, uid, PWS_ACCOUNT_OAUTH2_AUTH)) {
		log_message(LOG_PROTOCOL, _("OAuth2 trying for fresh access token with authorization code\n"));
		acc = passwd_store_get_account(ac_prefs->account_id, PWS_ACCOUNT_OAUTH2_AUTH);
		ret = oauth2_obtain_tokens (ac_prefs->oauth2_provider, OAUTH2Data, acc);
		g_free(acc);
	} else
		ret = 1;
	
	if (ret)
		log_message(LOG_PROTOCOL, _("OAuth2 access token not obtained\n"));
	else {
		if (ac_prefs->imap_auth_type == IMAP_AUTH_OAUTH2 ||
		    (ac_prefs->use_pop_auth && ac_prefs->pop_auth_type == POPAUTH_OAUTH2))
			passwd_store_set_account(ac_prefs->account_id, PWS_ACCOUNT_RECV, OAUTH2Data->access_token, FALSE);
		if (ac_prefs->use_smtp_auth && ac_prefs->smtp_auth_type == SMTPAUTH_OAUTH2)
			passwd_store_set_account(ac_prefs->account_id, PWS_ACCOUNT_SEND, OAUTH2Data->access_token, FALSE);
		passwd_store_set_account(ac_prefs->account_id, PWS_ACCOUNT_OAUTH2_EXPIRY, OAUTH2Data->expiry_str, FALSE);
		//Some providers issue replacement refresh tokens with each access token. Re-store whether replaced or not. 
		passwd_store_set_account(ac_prefs->account_id, PWS_ACCOUNT_OAUTH2_REFRESH, OAUTH2Data->refresh_token, FALSE);
		passwd_store_write_config();
		log_message(LOG_PROTOCOL, _("OAuth2 access and refresh token updated\n"));  
	}

	g_free(OAUTH2Data);
	g_free(uid);
	
	return (ret);
}

/* returns allocated string which must be freed */
guchar* oauth2_decode(const gchar *in)
{
         guchar *tmp;
	 gsize len;
	 
	 tmp = g_base64_decode(in, &len);
	 passcrypt_decrypt(tmp, len);
	 return tmp;
}

/* For testing */
void oauth2_encode(const gchar *in)
{
         guchar *tmp = g_strdup(in);
	 guchar *tmp2 = g_strdup(in);
	 gchar *result;
	 gsize len = strlen(in);
	 
	 passcrypt_encrypt(tmp, len);
	 result = g_base64_encode(tmp, len); 
	 tmp2 = oauth2_decode(result);
	 
	 log_message(LOG_PROTOCOL, _("OAuth2 original: %s\n"), in);
	 log_message(LOG_PROTOCOL, _("OAuth2 encoded: %s\n"), result);
	 log_message(LOG_PROTOCOL, _("OAuth2 decoded: %s\n\n"), tmp2);
	 
	 g_free(tmp);  
	 g_free(tmp2);
	 g_free(result);
}

gint oauth2_init (OAUTH2Data *OAUTH2Data)
{ 
	 OAUTH2Data->refresh_token = NULL;
	 OAUTH2Data->access_token = NULL;
	 OAUTH2Data->expiry_str = NULL;
	 OAUTH2Data->expiry = 0;
	 OAUTH2Data->custom_client_id = NULL;
	 OAUTH2Data->custom_client_secret = NULL;
	 
	 return (0);
}

#endif	/* USE_GNUTLS */
