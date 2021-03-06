/*
 * Claws Mail -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2021 the Claws Mail team
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

#include "oauth2.h"
#include "md5.h"
#include "utils.h"
#include "log.h"
#include "time.h"
#include "common/passcrypt.h"

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

static gchar *OAUTH2CodeMarker[5][2] = {
    {"",""},
    {"google_begin_mark","google_end_mark"}, /* Not used since token avalable to user to copy in browser window */
    {"#code=","&session_state"},
    {"#code=","&session_state"},
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
       
       regex = g_regex_new ("\"access_token\": ?\"(.*?)\",?", 0, 0, NULL);
       g_regex_match (regex, json, 0, &matchInfo);
       if (g_match_info_matches (matchInfo)) 
	 g_stpcpy (access_token,g_match_info_fetch (matchInfo, 1));
       else{  
	 g_match_info_free (matchInfo);
	 return (-1);
       }
       
       g_match_info_free (matchInfo);
       
       regex = g_regex_new ("\"expires_in\": ?([0-9]*),?", 0, 0, NULL);
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
       
       regex = g_regex_new ("\"refresh_token\": ?\"(.*?)\",?", 0, 0, NULL);
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
        if (provider == OAUTH2AUTH_YAHOO || provider == OAUTH2AUTH_GOOGLE) {
                /* Providers which display auth token in browser for users to copy */
                token = g_strdup(response);
        } else {
                gchar* start = g_strstr_len(response, strlen(response), OAUTH2CodeMarker[provider][0]);
                start += strlen(OAUTH2CodeMarker[provider][0]);
                if (start == NULL)
                        return NULL;
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
	gchar *header;
	gchar *tmp_hd;
	gchar *access_token;
	gchar *refresh_token;
	gint expiry = 0;
	gint ret;
	SockInfo *sock;
	gchar *client_id;
	gchar *client_secret;
    gchar *token = NULL;

	gint i;

	i = (int)provider - 1;
	if (i < 0 || i > (OAUTH2AUTH_LAST-1))
	  return (1);
    
        token = oauth2_get_token_from_response(provider, authcode);
        debug_print("Auth token: %s\n", token);
        if (token == NULL) {
                log_message(LOG_PROTOCOL, "OAUTH2 missing authentication code\n");
                return (1);
        }

	sock = sock_connect(OAUTH2info[i][OA2_BASE_URL], 443);
	if(sock == NULL || ssl_init_socket(sock) == FALSE){
	  log_message(LOG_PROTOCOL, "OAUTH2 SSL connecion error\n");
      g_free(token);
	  return (1);
	}

	sock_set_nonblocking_mode(sock, FALSE);
	sock_set_io_timeout(10);
	sock->gnutls_priority = "NORMAL:!VERS-SSL3.0:!VERS-TLS1.0:!VERS-TLS1.1";

	refresh_token = g_malloc(OAUTH2BUFSIZE+1);	
	access_token = g_malloc(OAUTH2BUFSIZE+1);
	request = g_malloc(OAUTH2BUFSIZE+1);
	response = g_malloc(OAUTH2BUFSIZE+1);
	body = g_malloc(OAUTH2BUFSIZE+1);

	if(OAUTH2Data->custom_client_id)
	  client_id = g_strdup(OAUTH2Data->custom_client_id);
	else
	  client_id = oauth2_decode(OAUTH2info[i][OA2_CLIENT_ID]);
    
	body = g_strconcat ("client_id=", g_uri_escape_string (client_id, NULL, FALSE), 
			    "&code=",g_uri_escape_string (token, NULL, FALSE), NULL);
    g_free(token);

	if(OAUTH2info[i][OA2_CLIENT_SECRET][0]){
	  //Only allow custom client secret if the service provider would usually expect a client secret
	  if(OAUTH2Data->custom_client_secret)
	    client_secret = g_strdup(OAUTH2Data->custom_client_secret);
	  else
	    client_secret = oauth2_decode(OAUTH2info[i][OA2_CLIENT_SECRET]);
	  body = g_strconcat (body, "&client_secret=", g_uri_escape_string (client_secret, NULL, FALSE), NULL);
	}else{
	  client_secret = g_strconcat ("", NULL);
	}

	if(OAUTH2info[i][OA2_REDIRECT_URI][0])
	  body = g_strconcat (body, "&redirect_uri=",g_uri_escape_string (OAUTH2info[i][OA2_REDIRECT_URI], NULL, FALSE), NULL);
	if(OAUTH2info[i][OA2_GRANT_TYPE_ACCESS][0])
	  body = g_strconcat (body, "&grant_type=", g_uri_escape_string (OAUTH2info[i][OA2_GRANT_TYPE_ACCESS], NULL, FALSE), NULL);
	if(OAUTH2info[i][OA2_TENANT][0])
	  body = g_strconcat (body, "&tenant=", g_uri_escape_string (OAUTH2info[i][OA2_TENANT], NULL, FALSE), NULL);	
	if(OAUTH2info[i][OA2_SCOPE_FOR_ACCESS][0])
	  body = g_strconcat (body, "&scope=", g_uri_escape_string (OAUTH2info[i][OA2_SCOPE_FOR_ACCESS], NULL, FALSE), NULL);
	if(OAUTH2info[i][OA2_STATE][0])
	  body = g_strconcat (body, "&state=", g_uri_escape_string (OAUTH2info[i][OA2_STATE], NULL, FALSE), NULL);

	if(OAUTH2info[i][OA2_HEADER_AUTH_BASIC][0]){
	  tmp_hd = g_strconcat(client_id, ":", client_secret, NULL);	  
	  header = g_strconcat ("Authorization: Basic ", g_base64_encode (tmp_hd, strlen(tmp_hd)), NULL);
	  g_free(tmp_hd);
	}else{
	  header = g_strconcat ("", NULL);
	}

	oauth2_post_request (request, OAUTH2info[i][OA2_BASE_URL], OAUTH2info[i][OA2_ACCESS_RESOURCE], header, body);
	ret = oauth2_contact_server (sock, request, response);

	if(oauth2_filter_access (response, access_token, &expiry) == 0){
	  OAUTH2Data->access_token = access_token;
	  OAUTH2Data->expiry = expiry;
	  OAUTH2Data->expiry_str = g_strdup_printf ("%i", expiry);
	  ret = 0;
	  log_message(LOG_PROTOCOL, "OAUTH2 access token obtained\n");
	}else{
	  log_message(LOG_PROTOCOL, "OAUTH2 access token not obtained\n");
	  debug_print("OAUTH2 - request: %s\n Response: %s", request, response);
	  ret = 1;
	}

	if(oauth2_filter_refresh (response, refresh_token) == 0){
	  OAUTH2Data->refresh_token = refresh_token;
	  log_message(LOG_PROTOCOL, "OAUTH2 refresh token obtained\n");
	}else{
	  log_message(LOG_PROTOCOL, "OAUTH2 refresh token not obtained\n");
	}

	sock_close(sock, TRUE);
	g_free(body);
	g_free(header);
	g_free(request);
	g_free(response);
	g_free(client_id);
	g_free(client_secret);

	return (ret);
}

gint oauth2_use_refresh_token (Oauth2Service provider, OAUTH2Data *OAUTH2Data)
{

	gchar *request;
	gchar *response;
	gchar *body;
	gchar *header;
	gchar *tmp_hd;
	gchar *access_token;
	gint expiry = 0;
	gint ret;
	SockInfo *sock;
	gchar *client_id;
	gchar *client_secret;

	gint i;

	i = (int)provider - 1;
	if (i < 0 || i > (OAUTH2AUTH_LAST-1))
	  return (1);

	sock = sock_connect(OAUTH2info[i][OA2_BASE_URL], 443);
	if(sock == NULL || ssl_init_socket(sock) == FALSE){
	  log_message(LOG_PROTOCOL, "OAUTH2 SSL connecion error\n");
	  return (1);
	}

	sock_set_nonblocking_mode(sock, FALSE);
	sock_set_io_timeout(10);
	sock->gnutls_priority = "NORMAL:!VERS-SSL3.0:!VERS-TLS1.0:!VERS-TLS1.1";

	access_token = g_malloc(OAUTH2BUFSIZE+1);
	request = g_malloc(OAUTH2BUFSIZE+1);
	response = g_malloc(OAUTH2BUFSIZE+1);
	body = g_malloc(OAUTH2BUFSIZE+1);

	if(OAUTH2Data->custom_client_id)
	  client_id = g_strdup(OAUTH2Data->custom_client_id);
	else
	  client_id = oauth2_decode(OAUTH2info[i][OA2_CLIENT_ID]);

	body = g_strconcat ("client_id=", g_uri_escape_string (client_id, NULL, FALSE), 
			    "&refresh_token=",OAUTH2Data->refresh_token, NULL); 

	if(OAUTH2info[i][OA2_CLIENT_SECRET][0]){
	  //Only allow custom client secret if the service provider would usually expect a client secret
	  if(OAUTH2Data->custom_client_secret)
	    client_secret = g_strdup(OAUTH2Data->custom_client_secret);
	  else
	    client_secret = oauth2_decode(OAUTH2info[i][OA2_CLIENT_SECRET]);
	  body = g_strconcat (body, "&client_secret=", g_uri_escape_string (client_secret, NULL, FALSE), NULL);
	}else{
	  client_secret = g_strconcat ("", NULL);
	}

	if(OAUTH2info[i][OA2_GRANT_TYPE_REFRESH][0])
	  body = g_strconcat (body, "&grant_type=", g_uri_escape_string (OAUTH2info[i][OA2_GRANT_TYPE_REFRESH], NULL, FALSE), NULL);	
	if(OAUTH2info[i][OA2_SCOPE_FOR_ACCESS][0])
	  body = g_strconcat (body, "&scope=", g_uri_escape_string (OAUTH2info[i][OA2_SCOPE_FOR_ACCESS], NULL, FALSE), NULL);
	if(OAUTH2info[i][OA2_STATE][0])
	  body = g_strconcat (body, "&state=", g_uri_escape_string (OAUTH2info[i][OA2_STATE], NULL, FALSE), NULL);

	if(OAUTH2info[i][OA2_HEADER_AUTH_BASIC][0]){
	  tmp_hd = g_strconcat(client_id, ":", client_secret, NULL);	  
	  header = g_strconcat ("Authorization: Basic ", g_base64_encode (tmp_hd, strlen(tmp_hd)), NULL);
	  g_free(tmp_hd);
	}else{
	  header = g_strconcat ("", NULL);
	}

	oauth2_post_request (request, OAUTH2info[i][OA2_BASE_URL], OAUTH2info[i][OA2_REFRESH_RESOURCE], header, body);
	ret = oauth2_contact_server (sock, request, response);

	if(oauth2_filter_access (response, access_token, &expiry) == 0){
	  OAUTH2Data->access_token = access_token;
	  OAUTH2Data->expiry = expiry;
	  OAUTH2Data->expiry_str = g_strdup_printf ("%i", expiry);
	  ret = 0;
	  log_message(LOG_PROTOCOL, "OAUTH2 access token obtained\n");
	}else{
	  log_message(LOG_PROTOCOL, "OAUTH2 access token not obtained\n");
	  debug_print("OAUTH2 - request: %s\n Response: %s", request, response);
	  ret = 1;
	}

	debug_print("OAUTH2 - access token: %s\n", access_token);
	debug_print("OAUTH2 - access token expiry: %i\n", expiry);
	
	sock_close(sock, TRUE);
	g_free(body);
	g_free(header);
	g_free(request);
	g_free(response);
	g_free(client_id);
	g_free(client_secret);

	return (ret);
}

static gint oauth2_contact_server (SockInfo *sock, gchar *request, gchar *response)
{
	gint len;
	gint ret;
	gchar *token;
	gint toread = OAUTH2BUFSIZE;	
	time_t startplus = time(NULL);
	
	len = strlen(request);
	
	startplus += 10;
	
	if (sock_write (sock, request, len+1) < 0) {
	  log_message(LOG_PROTOCOL, "OAUTH2 socket write error\n");
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
	  token = g_strconcat(token, response, NULL);
	} while ((toread > 0) && (time(NULL) < startplus)); 
	
	if(time(NULL) >= startplus)
	  log_message(LOG_PROTOCOL, "OAUTH2 socket timeout error \n");
	
	g_free(token);
	
	return (0);
}

gint oauth2_authorisation_url (Oauth2Service provider, gchar **url, const gchar *custom_client_id)
{
        gint i;
	const gchar *client_id;

	i = (int)provider - 1;
	if (i < 0 || i > (OAUTH2AUTH_LAST-1))
	  return (1);
	
	if(custom_client_id)
	  client_id = custom_client_id;
	else
	  client_id = oauth2_decode(OAUTH2info[i][OA2_CLIENT_ID]);
	
	*url = g_strconcat ("https://", OAUTH2info[i][OA2_BASE_URL],OAUTH2info[i][OA2_AUTH_RESOURCE], "?client_id=",
			    g_uri_escape_string (client_id, NULL, FALSE), NULL);
			    
	if(OAUTH2info[i][OA2_REDIRECT_URI][0])
	  *url = g_strconcat (*url, "&redirect_uri=", g_uri_escape_string (OAUTH2info[i][OA2_REDIRECT_URI], NULL, FALSE), NULL);
	if(OAUTH2info[i][OA2_RESPONSE_TYPE][0])
	  *url = g_strconcat (*url, "&response_type=",g_uri_escape_string (OAUTH2info[i][OA2_RESPONSE_TYPE], NULL, FALSE), NULL);
	if(OAUTH2info[i][OA2_SCOPE_FOR_AUTH][0])
	  *url = g_strconcat (*url, "&scope=", g_uri_escape_string (OAUTH2info[i][OA2_SCOPE_FOR_AUTH], NULL, FALSE), NULL);
	if(OAUTH2info[i][OA2_TENANT][0])
	  *url = g_strconcat (*url, "&tenant=", g_uri_escape_string (OAUTH2info[i][OA2_TENANT], NULL, FALSE), NULL);
	if(OAUTH2info[i][OA2_RESPONSE_MODE][0])
	  *url = g_strconcat (*url, "&response_mode=", g_uri_escape_string (OAUTH2info[i][OA2_RESPONSE_MODE], NULL, FALSE), NULL);
	if(OAUTH2info[i][OA2_STATE][0])
	  *url = g_strconcat (*url, "&state=", g_uri_escape_string (OAUTH2info[i][OA2_STATE], NULL, FALSE), NULL);

	return (0);
}

gint oauth2_check_passwds (PrefsAccount *ac_prefs)
{
	gchar *uid = g_strdup_printf("%d", ac_prefs->account_id);
	gint expiry;
	OAUTH2Data *OAUTH2Data = g_malloc(sizeof(* OAUTH2Data)); 
	gint ret;

	oauth2_init (OAUTH2Data);

	if (ac_prefs->oauth2_use_custom_id) {
	  OAUTH2Data->custom_client_id = ac_prefs->oauth2_cust_client_id;
	  OAUTH2Data->custom_client_secret = ac_prefs->oauth2_cust_client_secret;
	}
	
	if(passwd_store_has_password(PWS_ACCOUNT, uid, PWS_ACCOUNT_OAUTH2_EXPIRY)) {
	  expiry = atoi(passwd_store_get_account(ac_prefs->account_id, PWS_ACCOUNT_OAUTH2_EXPIRY));
	  if (expiry >  (g_get_real_time () / G_USEC_PER_SEC)){
	    g_free(OAUTH2Data);
	    log_message(LOG_PROTOCOL, "OAUTH2 access token still fresh\n");
	    return (0);
	  }
	}
	
	if(passwd_store_has_password(PWS_ACCOUNT, uid, PWS_ACCOUNT_OAUTH2_REFRESH)) {
	  log_message(LOG_PROTOCOL, "OAUTH2 obtaining access token using refresh token\n");
	  OAUTH2Data->refresh_token = passwd_store_get_account(ac_prefs->account_id, PWS_ACCOUNT_OAUTH2_REFRESH);
	  ret = oauth2_use_refresh_token (ac_prefs->oauth2_provider, OAUTH2Data);
	}else if (passwd_store_has_password(PWS_ACCOUNT, uid, PWS_ACCOUNT_OAUTH2_AUTH)) {
	  log_message(LOG_PROTOCOL, "OAUTH2 trying for fresh access token with auth code\n");
	  ret = oauth2_obtain_tokens (ac_prefs->oauth2_provider, OAUTH2Data, 
				      passwd_store_get_account(ac_prefs->account_id, PWS_ACCOUNT_OAUTH2_AUTH));
	}else{
	  ret = 1;
	}
	
	if (ret){
	  log_message(LOG_PROTOCOL, "OAUTH2 access token not obtained\n");
	}else{
	  passwd_store_set_account(ac_prefs->account_id, PWS_ACCOUNT_RECV, OAUTH2Data->access_token, FALSE);
      if (ac_prefs->use_smtp_auth && ac_prefs->smtp_auth_type == SMTPAUTH_OAUTH2)
	        passwd_store_set_account(ac_prefs->account_id, PWS_ACCOUNT_SEND, OAUTH2Data->access_token, FALSE);
	  passwd_store_set_account(ac_prefs->account_id, PWS_ACCOUNT_OAUTH2_EXPIRY, OAUTH2Data->expiry_str, FALSE);
	  log_message(LOG_PROTOCOL, "OAUTH2 access token updated\n");
	}

	g_free(OAUTH2Data);
	
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
	 
	 log_message(LOG_PROTOCOL, "OAUTH2 original: %s\n", in);
	 log_message(LOG_PROTOCOL, "OAUTH2 encoded: %s\n", result);
	 log_message(LOG_PROTOCOL, "OAUTH2 decoded: %s\n\n", tmp2);
	 
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
