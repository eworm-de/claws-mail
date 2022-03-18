/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2014 Colin Leroy <colin@colino.net>
 * and the Claws Mail Team
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#include "claws-features.h"
#endif

#include <unistd.h>
#include <stdio.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include "common/claws.h"
#include "common/version.h"
#include "main.h"
#include "password.h"
#include "plugin.h"
#include "prefs_common.h"
#include "file-utils.h"
#include "utils.h"
#include "spam_report_prefs.h"
#include "statusbar.h"
#include "procmsg.h"
#include "log.h"
#include "inc.h"
#include "menu.h"
#include "defs.h"
#include "procheader.h"

#ifdef USE_PTHREAD
#include <pthread.h>
#endif

#include <curl/curl.h>
#include <curl/curlver.h>

struct CurlReadWrite {
        char *data;
        size_t size;
};

static gboolean check_debian_listid(MsgInfo *msginfo);

/* this interface struct is probably not enough for the various available 
 * reporting places/methods. It'll be extended as necessary. */

#define SSFR_URL  "https://www.signal-spam.fr/api/signaler"
#define SSFR_BODY "message=%claws_mail_body_b64%"

#define DEBL_URL  "https://lists.debian.org/cgi-bin/nominate-for-review.pl?Quiet=on&msgid=%claws_mail_msgid%"

ReportInterface spam_interfaces[] = {
	{ "Signal-Spam.fr", INTF_HTTP_AUTH, SSFR_URL, SSFR_BODY, NULL},
	{ "Spamcop.net", INTF_MAIL, NULL, NULL, NULL},
	{ "Debian Lists", INTF_HTTP_GET, DEBL_URL, NULL, check_debian_listid},
	{ NULL, INTF_NULL, NULL, NULL, NULL}
};

/* From RSSyl. This should be factorized to the core... */
static gchar *spamreport_strreplace(gchar *source, gchar *pattern,
		gchar *replacement)
{
	gchar *new, *w_new, *c;
	guint count = 0, final_length;
	size_t len_pattern, len_replacement;

	if( source == NULL || pattern == NULL ) {
		debug_print("source or pattern is NULL!!!\n");
		return NULL;
	}

	if( !g_utf8_validate(source, -1, NULL) ) {
		debug_print("source is not an UTF-8 encoded text\n");
		return NULL;
	}

	if( !g_utf8_validate(pattern, -1, NULL) ) {
		debug_print("pattern is not an UTF-8 encoded text\n");
		return NULL;
	}

	len_pattern = strlen(pattern);
	len_replacement = replacement ? strlen(replacement) : 0;

	c = source;
	while( ( c = g_strstr_len(c, strlen(c), pattern) ) ) {
		count++;
		c += len_pattern;
	}

	final_length = strlen(source)
		- ( count * len_pattern )
		+ ( count * len_replacement );

	new = malloc(final_length + 1);
	w_new = new;
	memset(new, '\0', final_length + 1);

	c = source;

	while( *c != '\0' ) {
		if( !memcmp(c, pattern, len_pattern) ) {
			gboolean break_after_rep = FALSE;
			size_t i;
			if (*(c + len_pattern) == '\0')
				break_after_rep = TRUE;
			for (i = 0; i < len_replacement; i++) {
				*w_new = replacement[i];
				w_new++;
			}
			if (break_after_rep)
				break;
			c = c + len_pattern;
		} else {
			*w_new = *c;
			w_new++;
			c++;
		}
	}
	return new;
}

static gboolean check_debian_listid(MsgInfo *msginfo)
{
	gchar *buf = NULL;
	if (!procheader_get_header_from_msginfo(msginfo, &buf, "List-Id:") && buf != NULL) {
		if (strstr(buf, "lists.debian.org")) {
			g_free(buf);
			return TRUE;
		}
		g_free(buf);
	}
	return FALSE;
}

static void spamreport_http_response_log(gchar *url, long response)
{
	switch (response) {
	case 400: /* Bad Request */
		log_error(LOG_PROTOCOL, "%s: Bad Request\n", url);
		break;
	case 401: /* Not Authorized */
		log_error(LOG_PROTOCOL, "%s: Wrong login or password\n", url);
		break;
	case 404: /* Not Authorized */
		log_error(LOG_PROTOCOL, "%s: Not found\n", url);
		break;
	}
}

static void *myrealloc(void *pointer, size_t size) {
        /*
         * There might be a realloc() out there that doesn't like reallocing
         * NULL pointers, so we take care of it here.
         */
        if (pointer) {
                return realloc(pointer, size);
        } else {
                return malloc(size);
        }
}

static size_t curl_writefunction_cb(void *pointer, size_t size, size_t nmemb, void *data) {
        size_t realsize = size * nmemb;
        struct CurlReadWrite *mem = (struct CurlReadWrite *)data;

        mem->data = myrealloc(mem->data, mem->size + realsize + 1);
        if (mem->data) {
                memcpy(&(mem->data[mem->size]), pointer, realsize);
                mem->size += realsize;
                mem->data[mem->size] = 0;
        }
        return realsize;
}

static void report_spam(gint id, ReportInterface *intf, MsgInfo *msginfo, gchar *contents)
{
	gchar *reqbody = NULL, *tmp = NULL, *auth = NULL, *b64 = NULL, *geturl = NULL;
	size_t len_contents;
	CURL *curl;
	CURLcode res;
	long response;
	struct CurlReadWrite chunk;

	chunk.data = NULL;
	chunk.size = 0;
	
	if (spamreport_prefs.enabled[id] == FALSE) {
		debug_print("not reporting via %s (disabled)\n", intf->name);
		return;
	}
	if (intf->should_report != NULL && (intf->should_report)(msginfo) == FALSE) {
		debug_print("not reporting via %s (unsuitable)\n", intf->name);
		return;
	}

	debug_print("reporting via %s\n", intf->name);
	tmp = spamreport_strreplace(intf->body, "%claws_mail_body%", contents);
	len_contents = strlen(contents);
	b64 = g_base64_encode(contents, len_contents);
	reqbody = spamreport_strreplace(tmp, "%claws_mail_body_b64%", b64);
	geturl = spamreport_strreplace(intf->url, "%claws_mail_msgid%", msginfo->msgid);
	g_free(b64);
	g_free(tmp);
	
	switch(intf->type) {
	case INTF_HTTP_AUTH:
		if (spamreport_prefs.user[id] && *(spamreport_prefs.user[id])) {
			gchar *pass = spamreport_passwd_get(spam_interfaces[id].name);
			auth = g_strdup_printf("%s:%s", spamreport_prefs.user[id], (pass != NULL ? pass : ""));
			if (pass != NULL) {
				memset(pass, 0, strlen(pass));
			}
			g_free(pass);

			curl = curl_easy_init();
			curl_easy_setopt(curl, CURLOPT_URL, intf->url);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, reqbody);
			curl_easy_setopt(curl, CURLOPT_USERPWD, auth);
			curl_easy_setopt(curl, CURLOPT_TIMEOUT, prefs_common_get_prefs()->io_timeout_secs);
			curl_easy_setopt(curl, CURLOPT_USERAGENT,
                		SPAM_REPORT_USERAGENT "(" PLUGINS_URI ")");
#ifdef G_OS_WIN32
			curl_easy_setopt(curl, CURLOPT_CAINFO, claws_ssl_get_cert_file());
#endif
			res = curl_easy_perform(curl);
			if (res != CURLE_OK)
				debug_print("curl_easy_perfom failed: %s", curl_easy_strerror(res));
			curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response);
			curl_easy_cleanup(curl);
			spamreport_http_response_log(intf->url, response);
			g_free(auth);
		}
		break;
	case INTF_MAIL:
		if (spamreport_prefs.user[id] && *(spamreport_prefs.user[id])) {
			Compose *compose = compose_forward(NULL, msginfo, TRUE, NULL, TRUE, TRUE);
			compose->use_signing = FALSE;
			compose_entry_append(compose, spamreport_prefs.user[id], COMPOSE_TO, PREF_NONE);
			compose_send(compose);
		}
		break;
	case INTF_HTTP_GET:
		curl = curl_easy_init();
		curl_easy_setopt(curl, CURLOPT_URL, geturl);
		curl_easy_setopt(curl, CURLOPT_USERAGENT,
                		SPAM_REPORT_USERAGENT "(" PLUGINS_URI ")");
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_writefunction_cb);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
#ifdef G_OS_WIN32
		curl_easy_setopt(curl, CURLOPT_CAINFO, claws_ssl_get_cert_file());
#endif
		res = curl_easy_perform(curl);
		if (res != CURLE_OK)
			debug_print("curl_easy_perfom failed: %s", curl_easy_strerror(res));
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response);
		curl_easy_cleanup(curl);
		spamreport_http_response_log(geturl, response);
		/* On success the page should return "OK: nominated <msgid>" */
		if (chunk.size < 13 || strstr(chunk.data, "OK: nominated") == NULL) {
			if (chunk.size > 0) {
				log_error(LOG_PROTOCOL, "%s: response was %s\n", geturl, chunk.data);
			}
			else {
				log_error(LOG_PROTOCOL, "%s: response was empty\n", geturl);
			}
		}
		break;
	default:
		g_warning("unknown method");
	}
	g_free(reqbody);
	g_free(geturl);
}

static void report_spam_cb_ui(GtkAction *action, gpointer data)
{
	MainWindow *mainwin = mainwindow_get_mainwindow();
	SummaryView *summaryview = mainwin->summaryview;
	GSList *msglist = summary_get_selected_msg_list(summaryview);
	GSList *cur;
	gint curnum=0, total=0;
	if (summary_is_locked(summaryview) || !msglist) {
		if (msglist)
			g_slist_free(msglist);
		return;
	}
	main_window_cursor_wait(summaryview->mainwin);
	gtk_cmclist_freeze(GTK_CMCLIST(summaryview->ctree));
	folder_item_update_freeze();
	inc_lock();

	STATUSBAR_PUSH(mainwin, _("Reporting spam..."));
	total = g_slist_length(msglist);

	for (cur = msglist; cur; cur = cur->next) {
		MsgInfo *msginfo = (MsgInfo *)cur->data;
		gchar *file = procmsg_get_message_file(msginfo);
		gchar *contents = NULL;
		int i = 0;
		if (!file)
			continue;
		debug_print("reporting message %d (%s)\n", msginfo->msgnum, file);
		statusbar_progress_all(curnum, total, 1);
		GTK_EVENTS_FLUSH();
		curnum++;

		contents = file_read_to_str(file);
		
		for (i = 0; i < INTF_LAST; i++)
			report_spam(i, &(spam_interfaces[i]), msginfo, contents);
		
		g_free(contents);
		g_free(file);
	}

	statusbar_progress_all(0,0,0);
	STATUSBAR_POP(mainwin);
	inc_unlock();
	folder_item_update_thaw();
	gtk_cmclist_thaw(GTK_CMCLIST(summaryview->ctree));
	main_window_cursor_normal(summaryview->mainwin);
	g_slist_free(msglist);
}

static GtkActionEntry spamreport_main_menu[] = {{
	"Message/ReportSpam",
	NULL, N_("Report spam online..."), NULL, NULL, G_CALLBACK(report_spam_cb_ui)
}};

static guint context_menu_id = 0;
static guint main_menu_id = 0;

gint plugin_init(gchar **error)
{
	MainWindow *mainwin = mainwindow_get_mainwindow();

	if (!check_plugin_version(MAKE_NUMERIC_VERSION(3,13,2,39),
				VERSION_NUMERIC, _("SpamReport"), error))
		return -1;

	spamreport_prefs_init();

	curl_global_init(CURL_GLOBAL_DEFAULT);

	gtk_action_group_add_actions(mainwin->action_group, spamreport_main_menu,
			1, (gpointer)mainwin);
	MENUITEM_ADDUI_ID_MANAGER(mainwin->ui_manager, "/Menu/Message", "ReportSpam", 
			  "Message/ReportSpam", GTK_UI_MANAGER_MENUITEM,
			  main_menu_id)
	MENUITEM_ADDUI_ID_MANAGER(mainwin->ui_manager, "/Menus/SummaryViewPopup", "ReportSpam", 
			  "Message/ReportSpam", GTK_UI_MANAGER_MENUITEM,
			  context_menu_id)
	return 0;
}

gboolean plugin_done(void)
{
	MainWindow *mainwin = mainwindow_get_mainwindow();

	if (mainwin == NULL)
		return TRUE;

	MENUITEM_REMUI_MANAGER(mainwin->ui_manager,mainwin->action_group, "Message/ReportSpam", main_menu_id);
	main_menu_id = 0;

	MENUITEM_REMUI_MANAGER(mainwin->ui_manager,mainwin->action_group, "Message/ReportSpam", context_menu_id);
	context_menu_id = 0;

	spamreport_prefs_done();

	return TRUE;
}

const gchar *plugin_name(void)
{
	return _("SpamReport");
}

const gchar *plugin_desc(void)
{
	return _("This plugin reports spam to various places.\n"
		 "Currently the following sites or methods are supported:\n\n"
		 " * spam-signal.fr\n"
                 " * spamcop.net\n"
		 " * lists.debian.org nomination system");
}

const gchar *plugin_type(void)
{
	return "GTK3";
}

const gchar *plugin_licence(void)
{
	return "GPL3+";
}

const gchar *plugin_version(void)
{
	return VERSION;
}

struct PluginFeature *plugin_provides(void)
{
	static struct PluginFeature features[] = 
		{ {PLUGIN_UTILITY, N_("Spam reporting")},
		  {PLUGIN_NOTHING, NULL}};
	return features;
}
