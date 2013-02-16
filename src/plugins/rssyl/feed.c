/*
 * Claws Mail -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2004 Hiroyuki Yamamoto
 * This file (C) 2005-2008 Andrej Kacian <andrej@kacian.sk> and the Claws Mail team
 *
 * - various feed parsing functions
 * - this file could use some sorting and/or splitting
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#include "claws-features.h"
#endif

#include <glib.h>
#include <glib/gi18n.h>

#include <gtk/gtk.h>

#include <curl/curl.h>
#include <curl/curlver.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <pthread.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "common/claws.h"
#include "common/version.h"
#include "codeconv.h"
#include "procmsg.h"
#include "procheader.h"
#include "alertpanel.h"
#include "folder.h"
#include "mainwindow.h"
#include "statusbar.h"
#include "log.h"
#include "prefs_common.h"
#include "defs.h"
#include "inc.h"
#include "common/utils.h"
#include "main.h"

#include "date.h"
#include "rssyl.h"
#include "rssyl_cb_gtk.h"
#include "feed.h"
#include "feedprops.h"
#include "strreplace.h"
#include "parsers.h"
#include "rssyl_prefs.h"

static int rssyl_curl_progress_function(void *clientp,
		double dltotal, double dlnow, double ultotal, double ulnow)
{
	if (claws_is_exiting()) {
		debug_print("RSSyl: curl_progress_function bailing out, app is exiting\n");
		return 1;
	}

	return 0;
}

struct _RSSylThreadCtx {
	const gchar *url;
	time_t last_update;
	gboolean not_modified;
	gboolean defer_modified_check;
	gboolean ready;
	gchar *error;
};

typedef struct _RSSylThreadCtx RSSylThreadCtx;

static void *rssyl_fetch_feed_threaded(void *arg)
{
	RSSylThreadCtx *ctx = (RSSylThreadCtx *)arg;
	CURL *eh = NULL;
	CURLcode res;
	time_t last_modified;
	gchar *time_str = NULL;
	long response_code;

#ifndef G_OS_WIN32
	gchar *template = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, RSSYL_DIR,
			G_DIR_SEPARATOR_S, RSSYL_TMP_TEMPLATE, NULL);
	int fd = mkstemp(template);
#else
	gchar *template = get_tmp_file();
	int fd = open(template, (O_CREAT|O_RDWR|O_BINARY), (S_IRUSR|S_IWUSR));
#endif
	FILE *f;

#ifdef USE_PTHREAD
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
#endif

	if (fd == -1) {
		perror("mkstemp");
		ctx->ready = TRUE;
		g_free(template);
		return NULL;
	}

	f = fdopen(fd, "w");
	if (f == NULL) {
		perror("fdopen");
		ctx->error = g_strdup(_("Cannot open temporary file"));
		claws_unlink(template);
		g_free(template);
		ctx->ready = TRUE;
		return NULL;
	}

	eh = curl_easy_init();

	if (eh == NULL) {
		g_warning("can't init curl");
		ctx->error = g_strdup(_("Cannot init libCURL"));
		fclose(f);
		claws_unlink(template);
		g_free(template);
		ctx->ready = TRUE;
		return NULL;
	}

	debug_print("TEMPLATE: %s\n", template);

	curl_easy_setopt(eh, CURLOPT_URL, ctx->url);
	curl_easy_setopt(eh, CURLOPT_NOPROGRESS, 0);
	curl_easy_setopt(eh, CURLOPT_PROGRESSFUNCTION, rssyl_curl_progress_function);
#if LIBCURL_VERSION_NUM < 0x071000
	curl_easy_setopt(eh, CURLOPT_MUTE, 1);
#endif
	curl_easy_setopt(eh, CURLOPT_WRITEFUNCTION, NULL);
	curl_easy_setopt(eh, CURLOPT_WRITEDATA, f);
	curl_easy_setopt(eh, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(eh, CURLOPT_MAXREDIRS, 3);
	curl_easy_setopt(eh, CURLOPT_TIMEOUT, prefs_common_get_prefs()->io_timeout_secs);
	curl_easy_setopt(eh, CURLOPT_NOSIGNAL, 1);
	curl_easy_setopt(eh, CURLOPT_ENCODING, "");
#if LIBCURL_VERSION_NUM >= 0x070a00
	curl_easy_setopt(eh, CURLOPT_SSL_VERIFYPEER, 0);
	curl_easy_setopt(eh, CURLOPT_SSL_VERIFYHOST, 0);
#endif
	curl_easy_setopt(eh, CURLOPT_USERAGENT,
		"Claws Mail RSSyl plugin "VERSION
		" (" PLUGINS_URI ")");
#ifdef RSSYL_DEBUG
	curl_easy_setopt(eh, CURLOPT_VERBOSE, 1);
#endif
	curl_easy_setopt(eh, CURLOPT_COOKIEFILE,
			rssyl_prefs_get()->cookies_path);
	
	if( !ctx->defer_modified_check ) {
		if( ctx->last_update != -1 ) {
			time_str = createRFC822Date(&ctx->last_update);
		}
		debug_print("RSSyl: last update %ld (%s)\n",
			(long int)ctx->last_update,
			(ctx->last_update != -1 ? time_str : "unknown") );
		g_free(time_str);
		time_str = NULL;
		if( ctx->last_update != -1 ) {
			curl_easy_setopt(eh, CURLOPT_TIMECONDITION,
				CURL_TIMECOND_IFMODSINCE);
			curl_easy_setopt(eh, CURLOPT_TIMEVALUE, ctx->last_update);
		}
	}
			
	res = curl_easy_perform(eh);

	if (res != 0) {
		if (res == CURLE_OPERATION_TIMEOUTED) {
			log_error(LOG_PROTOCOL, RSSYL_LOG_ERROR_TIMEOUT, ctx->url);
		} else if (res == CURLE_ABORTED_BY_CALLBACK) {
			log_print(LOG_PROTOCOL, RSSYL_LOG_EXITING);
		}
		ctx->error = g_strdup(curl_easy_strerror(res));
		ctx->ready = TRUE;
		curl_easy_cleanup(eh);
		fclose(f);
		claws_unlink(template);
		g_free(template);
		return NULL;
	}
	curl_easy_getinfo(eh, CURLINFO_RESPONSE_CODE, &response_code);

	if( !ctx->defer_modified_check ) {
		if( ctx->last_update != -1 ) {
			curl_easy_getinfo(eh, CURLINFO_FILETIME, &last_modified);

			if( last_modified != -1 ) {
				time_str = createRFC822Date(&last_modified);
			}
			debug_print("RSSyl: got status %ld, last mod %ld (%s)\n",
					response_code, (long int) last_modified, 
					(last_modified != -1 ? time_str : "unknown") );
			g_free(time_str);
			time_str = NULL;
		} else {
			debug_print("RSSyl: got status %ld\n", response_code);
		}
	}

	curl_easy_cleanup(eh);

	fclose(f);

	if( response_code >= 400 && response_code < 500 ) {
		debug_print("RSSyl: got %ld\n", response_code);
		switch(response_code) {
			case 401: 
				ctx->error = g_strdup(_("401 (Authorisation required)"));
				break;
			case 403:
				ctx->error = g_strdup(_("403 (Unauthorised)"));
				break;
			case 404:
				ctx->error = g_strdup(_("404 (Not found)"));
				break;
			default:
				ctx->error = g_strdup_printf(_("Error %ld"), response_code);
				break;
		}
		ctx->ready = TRUE;
		claws_unlink(template);
		g_free(template);
		return NULL;
	}

	if( !ctx->defer_modified_check ) {
		if( response_code == 304 ) {
			debug_print("RSSyl: don't rely on server response 304, defer modified "
					"check\n");
			claws_unlink(template);
			g_free(template);
			ctx->defer_modified_check = TRUE;
			template = rssyl_fetch_feed_threaded(ctx);
			return template;
		}
	}
	ctx->ready = TRUE;

	return template;
}

gchar *rssyl_feed_title_to_dir(const gchar *title)
{
#ifndef G_OS_WIN32
	return rssyl_strreplace(title, "/", "\\");
#else
	gchar *patterns[] = { "/", "\\", ":", "*", "?" , "\"", "<", ">", "|" };
	gchar *sanitized = g_strdup(title);
	int i;

	for (i = 0; i < sizeof(patterns)/sizeof(patterns[0]); i++) {
		gchar *tmp = rssyl_strreplace(sanitized, patterns[i], "-");
		g_free(sanitized);
		sanitized = tmp;
	}
	
	return sanitized;
#endif
}

/* rssyl_fetch_feed()
 *
 * This function utilizes libcurl's easy interface to fetch the feed, pre-parse
 * it for title, create a directory based on the title. It returns a xmlDocPtr
 * for libxml2 to parse further.
 */
xmlDocPtr rssyl_fetch_feed(const gchar *url, time_t last_update, gchar **title, gchar **error) {
	gchar *xpath, *rootnode, *dir;
	xmlDocPtr doc;
	xmlNodePtr node, n, rnode;
	xmlXPathContextPtr context;
	xmlXPathObjectPtr result;
	MainWindow *mainwin = mainwindow_get_mainwindow();
	RSSylThreadCtx *ctx = g_new0(RSSylThreadCtx, 1);
	gchar *template = NULL;
	gboolean defer_modified_check = FALSE;
#ifdef RSSYL_DEBUG
	gchar *unixtime_str = NULL, *debugfname = NULL;
#endif /* RSSYL_DEBUG */

#ifdef USE_PTHREAD
	pthread_t pt;
	pthread_attr_t pta;
#endif
	gchar *msg = NULL, *tmptitle = NULL;
	gchar *content;
	xmlErrorPtr xml_err;

	ctx->url = url;
	ctx->ready = FALSE;
	ctx->last_update = last_update;
	ctx->not_modified = FALSE;
	ctx->defer_modified_check = FALSE;

	*title = NULL;

	g_return_val_if_fail(url != NULL, NULL);

	debug_print("RSSyl: XML - url is '%s'\n", url);

	msg = g_strdup_printf(_("Fetching '%s'..."), url);
	STATUSBAR_PUSH(mainwin, msg );
	g_free(msg);

	GTK_EVENTS_FLUSH();

#ifdef USE_PTHREAD
	if (pthread_attr_init(&pta) != 0 ||
	    pthread_attr_setdetachstate(&pta, PTHREAD_CREATE_JOINABLE) != 0 ||
	    pthread_create(&pt, &pta, rssyl_fetch_feed_threaded,
				(void *)ctx) != 0 ) {
		/* Bummer, couldn't create thread. Continue non-threaded */
		template = rssyl_fetch_feed_threaded(ctx);
	} else {
		/* Thread created, let's wait until it finishes */
		debug_print("RSSyl: waiting for thread to finish\n");
		while( !ctx->ready ) {
			claws_do_idle();
		}

		debug_print("RSSyl: thread finished\n");
		pthread_join(pt, (void *)&template);
	}
#else
	debug_print("RSSyl: no pthreads, run blocking fetch\n");
	template = rssyl_fetch_feed_threaded(ctx);
#endif

	defer_modified_check = ctx->defer_modified_check;

	if (error)
		*error = ctx->error;

	g_free(ctx);
	STATUSBAR_POP(mainwin);

	if( template == NULL ) {
		debug_print("RSSyl: no feed to parse, returning\n");
		log_error(LOG_PROTOCOL, RSSYL_LOG_ERROR_FETCH, url);
		return NULL;
	}
		
	/* Strip ugly \r\n endlines */
#ifndef G_OS_WIN32
	file_strip_crs((gchar *)template);
#endif
	debug_print("parsing %s\n", template);
	doc = xmlParseFile(template);
	
	if( doc == NULL ) {
		claws_unlink(template);
		g_free(template);
		xml_err = xmlGetLastError();
		if (xml_err)
			debug_print("error %s\n", xml_err->message);
		
		g_warning("Couldn't fetch feed '%s', aborting.\n", url);
		log_error(LOG_PROTOCOL, RSSYL_LOG_ERROR_FETCH, url);
		if (error && !(*error)) {
			*error = g_strdup(_("Malformed feed"));
		}
		return NULL;
	}

	node = xmlDocGetRootElement(doc);
	rnode = node;

#ifdef RSSYL_DEBUG
	/* debug mode - get timestamp, add it to returned xmlDoc, and make a copy
	 * of the fetched feed file */
	tmptitle = rssyl_feed_title_to_dir(url);
	unixtime_str = g_strdup_printf("%ld", time(NULL) );
	debugfname = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, RSSYL_DIR,
			G_DIR_SEPARATOR_S, ".", tmptitle, ".", unixtime_str, NULL);

	debug_print("Storing fetched feed in file '%s' for debug purposes.\n",
			debugfname);
	link(template, debugfname);

	debug_print("Adding 'fetched' property to root node: %s\n", unixtime_str);
	xmlSetProp(rnode, "fetched", unixtime_str);
	g_free(unixtime_str);
	g_free(debugfname);
	g_free(tmptitle);
#endif	/* RSSYL_DEBUG */

	claws_unlink(template);
	g_free(template);

	debug_print("RSSyl: XML - root node is '%s'\n", node->name);

	rootnode = g_ascii_strdown(node->name, -1);

	if( !xmlStrcmp(rootnode, "rss") ) {
		context = xmlXPathNewContext(doc);
		xpath = g_strconcat("/", node->name, "/channel/title",	NULL);
		debug_print("RSSyl: XML - '%s'\n", xpath);
		if( !(result = xmlXPathEvalExpression(xpath, context)) ) {
			debug_print("RSSyl: XML - no result found for '%s'\n", xpath);
			xmlXPathFreeContext(context);
			g_free(rootnode);
			g_free(xpath);
			log_error(LOG_PROTOCOL, RSSYL_LOG_ERROR_PARSE, url);
			return NULL;
		}

		if( xmlXPathNodeSetIsEmpty(result->nodesetval) ) {
			debug_print("RSSyl: XML - nodeset empty for '%s'\n", xpath);
			g_free(rootnode);
			g_free(xpath);
			xmlXPathFreeObject(result);
			xmlXPathFreeContext(context);
			log_error(LOG_PROTOCOL, RSSYL_LOG_ERROR_PARSE, url);
			return NULL;
		}
		g_free(xpath);

		xmlXPathFreeContext(context);
		node = result->nodesetval->nodeTab[0];
		xmlXPathFreeObject(result);
		content = xmlNodeGetContent(node);
		debug_print("RSSyl: XML - title is '%s'\n", content );
		*title = g_strdup(content);
		xmlFree(content);
		debug_print("RSSyl: XML - our title is '%s'\n", *title);

		/* use the feed's pubDate to determine if it's modified */
		if( defer_modified_check ) {
		   time_t pub_date;

		   context = xmlXPathNewContext(doc);
		   node = rnode;
		   xpath = g_strconcat("/", node->name, "/channel/pubDate", NULL);
		   debug_print("RSSyl: XML - '%s'\n", xpath);
		   if( !(result = xmlXPathEvalExpression(xpath, context)) ) {
			   debug_print("RSSyl: XML - no result found for '%s'\n", xpath);
			   xmlXPathFreeContext(context);
			   g_free(rootnode);
			   g_free(xpath);
				 log_error(LOG_PROTOCOL, RSSYL_LOG_ERROR_PARSE, url);
			   return NULL;
		   }

		   if( xmlXPathNodeSetIsEmpty(result->nodesetval) ) {
			   debug_print("RSSyl: XML - nodeset empty for '%s', using current time\n",
						 xpath);
				 pub_date = time(NULL);
		   } else {
			   node = result->nodesetval->nodeTab[0];
			   content = xmlNodeGetContent(node);
			   pub_date = procheader_date_parse(NULL, content, 0);
			   debug_print("RSSyl: XML - pubDate is '%s'\n", content);
				 xmlFree(content);
			 }

			 xmlXPathFreeObject(result);
			 xmlXPathFreeContext(context);
			 g_free(xpath);

		   /* check date validity and perform postponed modified check */
		   if( pub_date > 0 ) {
			   gchar *time_str = NULL;

			   time_str = createRFC822Date(&pub_date);
			   debug_print("RSSyl: XML - item date found: %ld (%s)\n",
					   (long int) pub_date, time_str ? time_str : "unknown");
			   if( !time_str || ( pub_date < last_update && last_update > 0) ) {
				   if( !time_str) {
					   debug_print("RSSyl: XML - invalid item date\n");
				   } else {
					   debug_print("RSSyl: XML - no update needed\n");
				   }
				   g_free(time_str);
				   time_str = NULL;
				   g_free(rootnode);
				   return NULL;
			   }
			   g_free(time_str);
			   time_str = NULL;
		   } else {
			   debug_print("RSSyl: XML - item date not found\n");
			   g_free(rootnode);
			   return NULL;
		   }
		}

	} else if( !xmlStrcmp(rootnode, "rdf") ) {
		node = node->children;
		/* find "channel" */
		while( node && xmlStrcmp(node->name, "channel") )
			node = node->next;
		/* now find "title" */
		for( n = node->children; n; n = n->next ) {
			if( !xmlStrcmp(n->name, "title") ) {
				content = xmlNodeGetContent(n);
				*title = g_strdup(content);
				xmlFree(content);
				debug_print("RSSyl: XML - RDF our title is '%s'\n", *title);
			}
		}
	} else if( !xmlStrcmp(rootnode, "feed") ) {
		/* find "title" */
		for( n = node->children; n; n = n->next ) {
			if( !xmlStrcmp(n->name, "title") ) {
				content = xmlNodeGetContent(n);
				*title = g_strdup(content);
				xmlFree(content);
				debug_print("RSSyl: XML - FEED our title is '%s'\n", *title);
			}
		}
	} else {
		log_error(LOG_PROTOCOL, RSSYL_LOG_ERROR_UNKNOWN, url);
		g_free(rootnode);
		return NULL;
	}

	g_return_val_if_fail(*title != NULL, NULL);

	if (*title[0] == '\0') {
		g_free(*title);
		*title = g_strdup(url);
		subst_for_shellsafe_filename(*title);
	}

	tmptitle = rssyl_feed_title_to_dir(*title);
	dir = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, RSSYL_DIR,
			G_DIR_SEPARATOR_S, tmptitle, NULL);
	g_free(tmptitle);

	if( !is_dir_exist(dir) ) {
		if( make_dir(dir) < 0 ) {
			g_warning("couldn't create directory %s\n", dir);
			g_free(rootnode);
			g_free(dir);
			return NULL;
		}
	}

	g_free(rootnode);
	g_free(dir);

	return doc;
}

typedef struct _RSSyl_HTMLSymbol RSSyl_HTMLSymbol;
struct _RSSyl_HTMLSymbol
{
	gchar *const key;
	gchar *const val;
};

static RSSyl_HTMLSymbol symbol_list[] = {
	{ "&lt;", "<" },
	{ "&gt;", ">" },
	{ "&amp;", "&" },
	{ "&quot;", "\"" },
	{ "&lsquo;",  "'" },
	{ "&rsquo;",  "'" },
	{ "&ldquo;",  "\"" },
	{ "&rdquo;",  "\"" },
	{ "&nbsp;", " " },
	{ "&trade;", "(TM)" },
	{ "&#153;", "(TM)" },
	{ "&#39;", "'" },
	{ "&hellip;", "..." },
	{ "&mdash;", "-" },
	{ "<cite>", "\"" },
	{ "</cite>", "\"" },
	{ "<i>", "" },
	{ "</i>", "" },
	{ "<em>", "" },
	{ "</em>", ""},
	{ "<b>", "" },
	{ "</b>", "" },
	{ "<nobr>", "" },
	{ "</nobr>", "" },
	{ "<wbr>", "" },
	{ NULL, NULL },
};

static gchar *rssyl_replace_html_symbols(const gchar *text)
{
	gchar *tmp = NULL, *wtext = g_strdup(text);
	gint i;

	for( i = 0; symbol_list[i].key != NULL; i++ ) {
		if( g_strstr_len(text, strlen(text), symbol_list[i].key) ) {
			tmp = rssyl_strreplace(wtext, symbol_list[i].key, symbol_list[i].val);
			wtext = g_strdup(tmp);
			g_free(tmp);
		}
	}

	return wtext;
}

gchar *rssyl_format_string(const gchar *str, gboolean replace_html, gboolean replace_returns)
{
	gchar *res = NULL;
	gchar *tmp = NULL;

	g_return_val_if_fail(str != NULL, NULL);

	if (replace_html)
		tmp = rssyl_replace_html_symbols(str);
	else
		tmp = g_strdup(str);

	res = rssyl_sanitize_string(tmp, replace_returns);
	g_free(tmp);

	g_strstrip(res);

	return res;
}

/* this function splits a string into an array of string, by 
 * returning an array of pointers to positions of the delimiter
 * in the original string and replacing this delimiter with a
 * NULL. It does not duplicate memory, hence you should only
 * free the array and not its elements, and you should not
 * free the original string before you're done with the array.
 * maybe could be part of the core (utils.c).
 */
static gchar **strplit_no_copy(gchar *str, char delimiter)
{
	gchar **array = g_new(gchar *, 1);
	int i = 0;
	gchar *cur = str, *next;
	
	array[i] = cur;
	i++;
	while ((next = strchr(cur, delimiter)) != NULL) {
		*(next) = '\0';
		array = g_realloc(array, (sizeof(gchar *)) * (i + 1));
		array[i] = next + 1;
		cur = next + 1;
		i++;
	}
	array = g_realloc(array, (sizeof(gchar *)) * (i + 1));
	array[i] = NULL;
	return array;
}

/* rssyl_parse_folder_item_file()
 *
 * Parse the RFC822-formatted feed item given by "path", and returns a
 * pointer to a RSSylFeedItem struct, which contains all required data.
 */
static RSSylFeedItem *rssyl_parse_folder_item_file(gchar *dir_path, gchar *filename)
{
	gchar *contents, **lines, **line, **splid;
	GError *error = NULL;
	RSSylFeedItem *fitem;
	gint i = 0;
	gboolean parsing_headers, past_html_tag, past_endhtml_tag;
	gboolean started_author = FALSE, started_subject = FALSE;
	gboolean started_link = FALSE, started_clink = FALSE, started_plink = FALSE;
	gchar *full_path = g_strconcat(dir_path, G_DIR_SEPARATOR_S, filename, NULL);
	debug_print("RSSyl: parsing '%s'\n", full_path);

	g_file_get_contents(full_path, &contents, NULL, &error);

	if( error ) {
		g_warning("GError: '%s'\n", error->message);
		g_error_free(error);
		error = NULL;
	}

	if( contents ) {
		lines = strplit_no_copy(contents, '\n');
	} else {
		g_warning("Badly formatted file found, ignoring: '%s'\n", full_path);
		g_free(contents);
		return NULL;
	}

	fitem = g_new0(RSSylFeedItem, 1); /* free that */
	fitem->date = 0;
	fitem->date_published = 0;
	fitem->link = NULL;
	fitem->text = NULL;
	fitem->id = NULL;
	fitem->id_is_permalink = FALSE;
	fitem->realpath = g_strdup(full_path);

	g_free(full_path);

	parsing_headers = TRUE;
	past_html_tag = FALSE;
	past_endhtml_tag = FALSE;
	while(lines[i] ) {
		if( parsing_headers && lines[i] && !strlen(lines[i]) && fitem->link ) {
			parsing_headers = FALSE;
			debug_print("RSSyl: finished parsing headers\n");
		}

		if( parsing_headers ) {
			line = g_strsplit(lines[i], ": ", 2);
			if( line[0] && line[1] && strlen(line[0]) && lines[i][0] != ' ') {
				started_author = FALSE;
				started_subject = FALSE;
				started_link = FALSE;
				started_clink = FALSE;
				started_plink = FALSE;

				/* Author */
				if( !strcmp(line[0], "From") ) {
					fitem->author = g_strdup(line[1]);
					debug_print("RSSyl: got author '%s'\n", fitem->author);
					started_author = TRUE;
				}

				/* Date */
				if( !strcmp(line[0], "Date") ) {
					fitem->date = procheader_date_parse(NULL, line[1], 0);
					debug_print("RSSyl: got date \n" );
				}

				/* Title */
				if( !strcmp(line[0], "Subject") ) {
					fitem->title = g_strdup(line[1]);
					debug_print("RSSyl: got title '%s'\n", fitem->title);
					started_subject = TRUE;
				}

				/* Link */
				if( !strcmp(line[0], "X-RSSyl-URL") ) {
					fitem->link = g_strdup(line[1]);
					debug_print("RSSyl: got link '%s'\n", fitem->link);
					started_link = TRUE;
				}

				/* ID */
				if( !strcmp(line[0], "Message-ID") ) {
					splid = g_strsplit_set(line[1], "<>", 3);
					if( strlen(splid[1]) != 0 ) {
						fitem->id = g_strdup(splid[1]);
						debug_print("RSSyl: got id '%s'\n", fitem->id);
					}
					g_strfreev(splid);
				}

				if( !strcmp(line[0], "X-RSSyl-Comments") ) {
					fitem->comments_link = g_strdup(line[1]);
					debug_print("RSSyl: got clink '%s'\n", fitem->comments_link);
					started_clink = TRUE;
				}
				if( !strcmp(line[0], "X-RSSyl-Parent") ) {
					fitem->parent_link = g_strdup(line[1]);
					debug_print("RSSyl: got plink '%s'\n", fitem->parent_link);
					started_plink = TRUE;
				}
			} else if (lines[i][0] == ' ') {
				gchar *tmp = NULL;
				/* continuation line */
				if (started_author) {
					tmp = g_strdup_printf("%s %s", fitem->author, lines[i]+1);
					g_free(fitem->author);
					fitem->author = tmp;
					debug_print("RSSyl: updated author to '%s'\n", fitem->author);
				} else if (started_subject) {
					tmp = g_strdup_printf("%s %s", fitem->title, lines[i]+1);
					g_free(fitem->title);
					fitem->title = tmp;
					debug_print("RSSyl: updated title to '%s'\n", fitem->title);
				} else if (started_link) {
					tmp = g_strdup_printf("%s%s", fitem->link, lines[i]+1);
					g_free(fitem->link);
					fitem->link = tmp;
					debug_print("RSSyl: updated link to '%s'\n", fitem->link);
				} else if (started_clink) {
					tmp = g_strdup_printf("%s%s", fitem->comments_link, lines[i]+1);
					g_free(fitem->comments_link);
					fitem->comments_link = tmp;
					debug_print("RSSyl: updated comments_link to '%s'\n", fitem->comments_link);
				} else if (started_plink) {
					tmp = g_strdup_printf("%s%s", fitem->parent_link, lines[i]+1);
					g_free(fitem->parent_link);
					fitem->parent_link = tmp;
					debug_print("RSSyl: updated comments_link to '%s'\n", fitem->parent_link);
				}
			}
			g_strfreev(line);
		} else {
			if( !strcmp(lines[i], RSSYL_TEXT_START) ) {
				debug_print("Leading html tag found at line %d\n", i);
				past_html_tag = TRUE;
				i++;
				continue;
			}
			while( past_html_tag && !past_endhtml_tag && lines[i] ) {
				if( !strcmp(lines[i], RSSYL_TEXT_END) ) {
					debug_print("Trailing html tag found at line %d\n", i);
					past_endhtml_tag = TRUE;
					i++;
					continue;
				}
				if( fitem->text != NULL ) {
					gint e_len, n_len;
					e_len = strlen(fitem->text);
					n_len = strlen(lines[i]);
					fitem->text = g_realloc(fitem->text, e_len + n_len + 2);
					*(fitem->text+e_len) = '\n';
					strcpy(fitem->text+e_len+1, lines[i]);
					*(fitem->text+e_len+n_len+1) = '\0';
				} else {
					fitem->text = g_strdup(lines[i]);
				}
				i++;
			}

			if( lines[i] == NULL )
				return fitem;
		}

		i++;
	}
	g_free(lines);
	g_free(contents);
	return fitem;
}

/* rssyl_free_feeditem()
 * frees an RSSylFeedItem
 */
void rssyl_free_feeditem(RSSylFeedItem *item)
{
	if (!item)
		return;
	g_free(item->title);
	item->title = NULL;
	g_free(item->text);
	item->text = NULL;
	g_free(item->link);
	item->link = NULL;
	g_free(item->id);
	item->id = NULL;
	g_free(item->comments_link);
	item->comments_link = NULL;
	g_free(item->parent_link);
	item->parent_link = NULL;
	g_free(item->author);
	item->author = NULL;
	g_free(item->realpath);
	item->realpath = NULL;
	if( item->media != NULL ) {
		g_free(item->media->url);
		g_free(item->media->type);
		g_free(item->media);
	}
	g_free(item);
}

static void *rssyl_read_existing_thr(void *arg)
{
	RSSylParseCtx *ctx = (RSSylParseCtx *)arg;
	RSSylFolderItem *ritem = ctx->ritem;
	FolderItem *item = &ritem->item;
	RSSylFeedItem *fitem = NULL;
	DIR *dp;
	struct dirent *d;
	gint num;
	gchar *path;

	debug_print("RSSyl: rssyl_read_existing_thr()\n");

	path = folder_item_get_path(item);
	if( !path ) {
		debug_print("RSSyl: read_existing - path is NULL, bailing out\n");
		ctx->ready = TRUE;
		return NULL;
	}

	/* create a new GSList, throw away the old one */
	if( ritem->contents != NULL ) {
		GSList *cur;
		for (cur = ritem->contents; cur; cur = cur->next) {
			RSSylFeedItem *olditem = (RSSylFeedItem *)cur->data;
			rssyl_free_feeditem(olditem);
		}
		g_slist_free(ritem->contents); /* leak fix here */
		ritem->contents = NULL;
	}
	ritem->contents = g_slist_alloc();

	if( change_dir(path) < 0 ) {
		g_free(path);
		return NULL;
	}

	if( (dp = opendir(".")) == NULL ) {
		FILE_OP_ERROR(item->path, "opendir");
		g_free(path);
		return NULL;
	}

	while( (d = readdir(dp)) != NULL ) {
		if (claws_is_exiting()) {
			closedir(dp);
			g_free(path);
			debug_print("RSSyl: read_existing is bailing out, app is exiting\n");
			ctx->ready = TRUE;
			return NULL;
		}
		if( (num = to_number(d->d_name)) > 0 && dirent_is_regular_file(d) ) {
			debug_print("RSSyl: starting to parse '%s'\n", d->d_name);
			if( (fitem = rssyl_parse_folder_item_file(path, d->d_name)) != NULL ) {
				debug_print("Appending '%s'\n", fitem->title);
				ritem->contents = g_slist_prepend(ritem->contents, fitem);
			}
		}
	}
	closedir(dp);
	g_free(path);

	ritem->contents = g_slist_reverse(ritem->contents);

	ctx->ready = TRUE;

	debug_print("RSSyl: rssyl_read_existing_thr() is returning\n");
	return NULL;
}

/* rssyl_read_existing()
 *
 * Parse all existing folder items, storing their data in memory. Data is
 * later used for checking for duplicate entries.
 * Of course, actual work is done in a separate thread (if available) in
 * rssyl_read_existing_thr().
 */
void rssyl_read_existing(RSSylFolderItem *ritem)
{
	RSSylParseCtx *ctx = NULL;
#ifdef USE_PTHREAD
	pthread_t pt;
#endif

	g_return_if_fail(ritem != NULL);

	ctx = g_new0(RSSylParseCtx, 1);
	ctx->ritem = ritem;
	ctx->ready = FALSE;

#ifdef USE_PTHREAD
	if( pthread_create(&pt, PTHREAD_CREATE_JOINABLE, rssyl_read_existing_thr,
				(void *)ctx) != 0 ) {
		/* Couldn't create thread, continue nonthreaded */
		rssyl_read_existing_thr(ctx);
	} else {
		debug_print("RSSyl: waiting for read_existing thread to finish\n");
		while( !ctx->ready ) {
			claws_do_idle();
		}

		debug_print("RSSyl: read_existing thread finished\n");
		pthread_join(pt, NULL);
	}
#else
	debug_print("RSSyl: pthreads not available, running read_existing non-threaded\n");
	rssyl_read_existing_thr(ctx);
#endif

	g_free(ctx);
}

/* rssyl_cb_feed_compare()
 *
 * Callback compare function called by glib2's g_slist_find_custom().
 */
static gint rssyl_cb_feed_compare(const RSSylFeedItem *a,
		const RSSylFeedItem *b)
{
	gboolean date_publ_eq = FALSE, date_eq = FALSE;
	gboolean link_eq = FALSE, title_eq = FALSE;
	gboolean no_link = FALSE, no_title = FALSE;
	gchar *atit = NULL, *btit = NULL;

	if( a == NULL || b == NULL )
		return 1;

	/* ID should be unique. If it matches, we've found what we came for. */
	if( (a->id != NULL) && (b->id != NULL) ) {
		if( strcmp(a->id, b->id) == 0 )
			return 0;

		/* If both IDs are present, but they do not match, we need
		 * to look elsewhere. */
		return 1;
	}

	/* Ok, we have no ID to aid us. Let's have a look at item timestamps,
	 * item link and title. */
	if( (a->link != NULL) && (b->link != NULL) ) {
		if( strcmp(a->link, b->link) == 0 )
			link_eq = TRUE;
	} else
		no_link = TRUE;

	if( (a->title != NULL) && (b->title != NULL) ) {
		atit = conv_unmime_header(a->title, CS_UTF_8, FALSE);
		btit = conv_unmime_header(b->title, CS_UTF_8, FALSE);
		if( strcmp(atit, btit) == 0 )
			title_eq = TRUE;
		g_free(atit);
		g_free(btit);
	} else
		no_title = TRUE;

	/* If there's no 'published' timestamp for the item, we can only judge
	 * by item link and title - 'modified' timestamp can have changed if the
	 * item was updated recently. */
	if( b->date_published <= 0 && b->date <= 0) {
		if( link_eq && (title_eq || no_title) )
			return 0;
	}

	if( ((a->date_published > 0) && (b->date_published > 0) &&
			(a->date_published == b->date_published)) ) {
		date_publ_eq = TRUE;
	}

	if( ((a->date > 0) && (b->date > 0) &&
			(a->date == b->date)) ) {
		date_eq = TRUE;
	}

	/* If 'published' time and item link match, it is reasonable to assume
	 * it's this item. */
	if( (link_eq || no_link) && (date_publ_eq || date_eq) )
		return 0;

	/* Last ditch effort - if everything else is missing, at least titles
	 * should match. */
	if( no_link && title_eq )
		return 0;

	/* We don't know this item. */
	return 1;
}

enum {
	ITEM_UNCHANGED,
	ITEM_CHANGED_TEXTONLY,
	ITEM_CHANGED
};

static gint rssyl_feed_item_changed(RSSylFeedItem *old_item, RSSylFeedItem *new_item)
{
	/* if both have title ... */
	if( old_item->title && new_item->title ) {
		gchar *old = conv_unmime_header(old_item->title, CS_UTF_8, FALSE);
		gchar *new = conv_unmime_header(new_item->title, CS_UTF_8, FALSE);
		if( strcmp(old, new) ) {	/* ... compare "unmimed" titles */
			g_free(old);
			g_free(new);
			debug_print("RSSyl: item titles differ\n");
			return ITEM_CHANGED;
		}
		g_free(old);
		g_free(new);
	} else {
		/* if atleast one has a title, they differ */
		if( old_item->title || new_item->title ) {
			debug_print("RSSyl: +/- title\n");
			return ITEM_CHANGED;
		}
	}

	/* if both have author ... */
	if( old_item->author && new_item->author ) {
		gchar *old = conv_unmime_header(old_item->author, CS_UTF_8, TRUE);
		gchar *new = conv_unmime_header(new_item->author, CS_UTF_8, TRUE);
		if( strcmp(old, new) ) {	/* ... compare "unmimed" authors */
			g_free(old);
			g_free(new);
			debug_print("RSSyl: item authors differ\n");
			return ITEM_CHANGED;
		}
		g_free(old);
		g_free(new);
	} else {
		/* if atleast one has author, they differ */
		if( old_item->author || new_item->author ) {
			debug_print("RSSyl: +/- author\n");
			return ITEM_CHANGED;
		}
	}

	/* if both have text ... */
	if( old_item->text && new_item->text ) {
		if( strcmp(old_item->text, new_item->text) ) {	/* ... compare texts */
			debug_print("RSSyl: item texts differ\n");
			return ITEM_CHANGED_TEXTONLY;
		}
	} else {
		/* if atleast one has some text, they differ */
		if( old_item->text || new_item->text ) {
			debug_print("RSSyl: +/- text\n");
			if ( !old_item->text )
				debug_print("RSSyl:   old_item has no text\n");
			else
				debug_print("RSSyl:   new_item has no text\n");
			return ITEM_CHANGED_TEXTONLY;
		}
	}

	/* they don't seem to differ */
	return ITEM_UNCHANGED;
}

/* rssyl_feed_item_exists()
 *
 * Returns 1 if a feed item already exists locally, 2 if there's a changed
 * item with link that already belongs to existing item, 3 if only item's
 * text has changed, 0 if item is new.
 */
enum {
	EXISTS_NEW,
	EXISTS_UNCHANGED,
	EXISTS_CHANGED,
	EXISTS_CHANGED_TEXTONLY
};

static guint rssyl_feed_item_exists(RSSylFolderItem *ritem,
		RSSylFeedItem *fitem, RSSylFeedItem **oldfitem)
{
	gint changed;
	GSList *item = NULL;
	RSSylFeedItem *efitem = NULL;
	g_return_val_if_fail(ritem != NULL, FALSE);
	g_return_val_if_fail(fitem != NULL, FALSE);

	if( ritem->contents == NULL || g_slist_length(ritem->contents) == 0 )
		return 0;

	if( (item = g_slist_find_custom(ritem->contents,
					(gconstpointer)fitem, (GCompareFunc)rssyl_cb_feed_compare)) ) {
		efitem = (RSSylFeedItem *)item->data;
		if( (changed = rssyl_feed_item_changed(efitem, fitem)) > EXISTS_NEW ) {
			*oldfitem = efitem;
			if (changed == ITEM_CHANGED_TEXTONLY)
				return EXISTS_CHANGED_TEXTONLY;
			else
				return EXISTS_CHANGED;
		}
		return EXISTS_UNCHANGED;
	}

	return EXISTS_NEW;
}

void rssyl_parse_feed(xmlDocPtr doc, RSSylFolderItem *ritem, gchar *parent)
{
	xmlNodePtr node;
	gchar *rootnode;
	MainWindow *mainwin = mainwindow_get_mainwindow();
	gint count;
	gchar *msg;

	if (doc == NULL)
		return;

	rssyl_read_existing(ritem);

	if (claws_is_exiting()) {
		debug_print("RSSyl: parse_feed bailing out, app is exiting\n");
		return;
	}

	node = xmlDocGetRootElement(doc);

	debug_print("RSSyl: XML - root node is '%s'\n", node->name);
	rootnode = g_ascii_strdown(node->name, -1);

	msg = g_strdup_printf(_("Refreshing feed '%s'..."),
				ritem->item.name);
	STATUSBAR_PUSH(mainwin, msg );
	g_free(msg);
	GTK_EVENTS_FLUSH();

	folder_item_update_freeze();

	/* we decide what parser to call, depending on what the root node is */
	if( !strcmp(rootnode, "rss") ) {
		debug_print("RSSyl: XML - calling rssyl_parse_rss()\n");
		count = rssyl_parse_rss(doc, ritem, parent);
	} else if( !strcmp(rootnode, "rdf") ) {
		debug_print("RSSyl: XML - calling rssyl_parse_rdf()\n");
		if (ritem->fetch_comments) {
			log_error(LOG_PROTOCOL, _("RSSyl: Fetching comments is not supported for RDF feeds. "
				    "Cannot fetch comments of '%s'"), ritem->item.name);
			ritem->fetch_comments = FALSE;
		}
		count = rssyl_parse_rdf(doc, ritem, parent);
	} else if( !strcmp(rootnode, "feed") ) {
		debug_print("RSSyl: XML - calling rssyl_parse_atom()\n");
		count = rssyl_parse_atom(doc, ritem, parent);
	} else {
		alertpanel_error(_("This feed format is not supported yet."));
		count = 0;
	}

	if (!parent)
		ritem->last_count = count;

	folder_item_scan(&ritem->item);
	folder_item_update_thaw();

	STATUSBAR_POP(mainwin);

	g_free(rootnode);
}

gboolean rssyl_add_feed_item(RSSylFolderItem *ritem, RSSylFeedItem *fitem)
{
	MsgFlags *flags;
	gchar *template, *tmpurl, *tmpid;
	gchar tmp[10240];
	gint d = -1, fd, dif = 0;
	FILE *f;
	RSSylFeedItem *oldfitem = NULL;
	gchar *meta_charset = NULL, *url_html = NULL;
	gboolean err = FALSE;

	g_return_val_if_fail(ritem != NULL, FALSE);
	g_return_val_if_fail(ritem->item.path != NULL, FALSE);
	g_return_val_if_fail(fitem != NULL, FALSE);

	if( !fitem->author )
		fitem->author = g_strdup(_("N/A"));

	/* Skip if the item already exists */
	dif = rssyl_feed_item_exists(ritem, fitem, &oldfitem);
	if( dif == 1 ) {
		debug_print("RSSyl: This item already exists, skipping...\n");
		return FALSE;
	}
	if( dif >= 2 && oldfitem != NULL ) {
		debug_print("RSSyl: Item changed, removing old one and adding new...\n");
		ritem->contents = g_slist_remove(ritem->contents, oldfitem);
		g_remove(oldfitem->realpath);
		rssyl_free_feeditem(oldfitem);
		oldfitem = NULL;
	}

	/* Adjust some fields */
	if( fitem->date <= 0 )
		fitem->date = time(NULL);

	debug_print("RSSyl: Adding item '%s' (%d)\n", fitem->title, dif);

	ritem->contents = g_slist_append(ritem->contents, fitem);

	flags = g_new(MsgFlags, 1);
#ifndef G_OS_WIN32
	template = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, RSSYL_DIR,
			G_DIR_SEPARATOR_S, RSSYL_TMP_TEMPLATE, NULL);
	fd = mkstemp(template);
#else
	template = get_tmp_file();
	fd = open(template, (O_CREAT|O_RDWR|O_BINARY), (S_IRUSR|S_IWUSR));
#endif

	f = fdopen(fd, "w");
	g_return_val_if_fail(f != NULL, FALSE);

	if( fitem->date != 0 ) {
		gchar *tmpdate = createRFC822Date(&fitem->date);
		err |= (fprintf(f, "Date: %s\n", tmpdate ) < 0);
		g_free(tmpdate);
	}

	if( fitem->author ) {
		if (g_utf8_validate(fitem->author, -1, NULL)) {
			conv_encode_header_full(tmp, 10239, fitem->author, 
				strlen("From: "), TRUE, CS_UTF_8);
			err |= (fprintf(f, "From: %s\n", tmp) < 0);
		} else
			err |= (fprintf(f, "From: %s\n", fitem->author) < 0);
	} 

	if( fitem->title ) {
		if (g_utf8_validate(fitem->title, -1, NULL)) {
			conv_encode_header_full(tmp, 10239, fitem->title, 
				strlen("Subject: "), FALSE, CS_UTF_8);
			err |= (fprintf(f, "Subject: %s\n", tmp) < 0);
		} else
			err |= (fprintf(f, "Subject: %s\n", fitem->title) < 0);
	}

	if( (tmpurl = fitem->link) == NULL ) {
		if( fitem->id != NULL && fitem->id_is_permalink )
			tmpurl = fitem->id;
	}
	if( tmpurl != NULL )
		err |= (fprintf(f, "X-RSSyl-URL: %s\n", tmpurl) < 0);

	if( (tmpid = fitem->id) == NULL )
		tmpid = fitem->link;
	if( tmpid != NULL )
		err |= (fprintf(f, "Message-ID: <%s>\n", tmpid) < 0);

	if( fitem->comments_link ) {
		err |= (fprintf(f, "X-RSSyl-Comments: %s\n", fitem->comments_link) < 0);
	}
	if( fitem->parent_link) {
		err |= (fprintf(f, "X-RSSyl-Parent: %s\n", fitem->parent_link) < 0);
		err |= (fprintf(f, "References: <%s>\n", fitem->parent_link) < 0);
	}

#ifdef RSSYL_DEBUG
	if( fitem->debug_fetched != -1 ) {
		err |= (fprintf(f, "X-RSSyl-Debug-Fetched: %ld\n", fitem->debug_fetched) < 0);
	}
#endif	/* RSSYL_DEBUG */

	if (fitem->text && g_utf8_validate(fitem->text, -1, NULL)) {
		/* if it passes UTF-8 validation, specify it. */
		err |= (fprintf(f, "Content-Type: text/html; charset=UTF-8\n\n") < 0);
		meta_charset = g_strdup("<meta http-equiv=\"Content-Type\" "
			       "content=\"text/html; charset=UTF-8\">");
	} else {
		/* make sure Claws Mail displays it as html */
		err |= (fprintf(f, "Content-Type: text/html\n\n") < 0);
	}

	if( tmpurl )
		url_html = g_strdup_printf("<p>URL: <a href=\"%s\">%s</a></p>\n<br>\n",
				tmpurl, tmpurl);

	err |= (fprintf(f, "<html><head>"
			"%s\n"
			"<base href=\"%s\">\n"
			"</head><body>\n"
			"%s"
			RSSYL_TEXT_START"\n"
			"%s%s"
			RSSYL_TEXT_END"\n\n",

			meta_charset ? meta_charset:"",
			fitem->link,
			url_html?url_html:"",
			(fitem->text ? fitem->text : ""),
			(fitem->text ? "\n" : "") ) < 0 );

	g_free(meta_charset);
	g_free(url_html);	
	if( fitem->media ) {
		if( fitem->media->size > 0 ) {
			tmpid = g_strdup_printf(ngettext("%ld byte", "%ld bytes",
						fitem->media->size), fitem->media->size);
		} else {
			tmpid = g_strdup(_("size unknown"));
		}

		fprintf(f, "<p><a href=\"%s\">Attached media file</a> [%s] (%s)</p>\n",
				fitem->media->url, fitem->media->type, tmpid);
		
		g_free(tmpid);
	}

	if( fitem->media )
		err |= (fprintf(f,
					"<p><a href=\"%s\">Attached media file</a> [%s] (%ld bytes)</p>\n",
				fitem->media->url, fitem->media->type, fitem->media->size) < 0);

	err |= (fprintf(f, "</body></html>\n") < 0);

	err |= (fclose(f) == EOF);

	if (!err) {
		g_return_val_if_fail(template != NULL, FALSE);
		d = folder_item_add_msg(&ritem->item, template, flags, TRUE);
	}
	g_free(template);

	if (ritem->silent_update == 2
			|| (ritem->silent_update == 1 && dif == EXISTS_CHANGED_TEXTONLY))
		procmsg_msginfo_unset_flags(
				folder_item_get_msginfo((FolderItem *)ritem, d), MSG_NEW | MSG_UNREAD, 0);
	else
	
	debug_print("RSSyl: folder_item_add_msg(): %d, err %d\n", d, err);

	return err ? FALSE:TRUE;
}

MsgInfo *rssyl_parse_feed_item_to_msginfo(gchar *file, MsgFlags flags,
		gboolean a, gboolean b, FolderItem *item)
{
	MsgInfo *msginfo;

	g_return_val_if_fail(item != NULL, NULL);

	msginfo = procheader_parse_file(file, flags, a, b);
	if (msginfo)
		msginfo->folder = item;

	return msginfo;
}

void rssyl_remove_feed_cache(FolderItem *item)
{
	gchar *path;
	DIR *dp;
	struct dirent *d;
	gint num = 0;

	g_return_if_fail(item != NULL);

	debug_print("Removing local cache for '%s'\n", item->name);

	path = folder_item_get_path(item);
	g_return_if_fail(path != NULL);
	if( change_dir(path) < 0 ) {
		g_free(path);
		return;
	}

	debug_print("Emptying '%s'\n", path);

	if( (dp = opendir(".")) == NULL ) {
		FILE_OP_ERROR(item->path, "opendir");
		return;
	}

	while( (d = readdir(dp)) != NULL ) {
		g_remove(d->d_name);
		num++;
	}
	closedir(dp);

	debug_print("Removed %d files\n", num);

	g_remove(path);
	g_free(path);
}

void rssyl_update_comments(RSSylFolderItem *ritem)
{
	FolderItem *item = &ritem->item;
	RSSylFeedItem *fitem = NULL;
	DIR *dp;
	struct dirent *d;
	gint num;
	gchar *path;

	g_return_if_fail(ritem != NULL);

	if (ritem->fetch_comments == FALSE)
		return;

	path = folder_item_get_path(item);
	g_return_if_fail(path != NULL);
	if( change_dir(path) < 0 ) {
		g_free(path);
		return;
	}

	if( (dp = opendir(".")) == NULL ) {
		FILE_OP_ERROR(item->path, "opendir");
		g_free(path);
		return;
	}

	while( (d = readdir(dp)) != NULL ) {
		if (claws_is_exiting()) {
			g_free(path);
			closedir(dp);
			debug_print("RSSyl: update_comments bailing out, app is exiting\n");
			return;
		}

		if( (num = to_number(d->d_name)) > 0 && dirent_is_regular_file(d) ) {
			debug_print("RSSyl: starting to parse '%s'\n", d->d_name);
			if( (fitem = rssyl_parse_folder_item_file(path, d->d_name)) != NULL ) {
				xmlDocPtr doc;
				gchar *title;
				if (fitem->comments_link && fitem->id && 
				    (ritem->fetch_comments_for == -1 ||
				     time(NULL) - fitem->date <= ritem->fetch_comments_for*86400)) {
					debug_print("RSSyl: fetching comments '%s'\n", fitem->comments_link);
					doc = rssyl_fetch_feed(fitem->comments_link, ritem->item.mtime, &title, NULL);
					rssyl_parse_feed(doc, ritem, fitem->id);
					xmlFreeDoc(doc);
					g_free(title);
				}
				rssyl_free_feeditem(fitem);
			}
		}
	}
	closedir(dp);
	g_free(path);

	debug_print("RSSyl: rssyl_update_comments() is returning\n");
}

void rssyl_update_feed(RSSylFolderItem *ritem)
{
	gchar *title = NULL, *dir = NULL, *error = NULL, *dir2, *tmp;
	xmlDocPtr doc = NULL;

	g_return_if_fail(ritem != NULL);

	if( !ritem->url )
		rssyl_get_feed_props(ritem);
	g_return_if_fail(ritem->url != NULL);

	log_print(LOG_PROTOCOL, RSSYL_LOG_UPDATING, ritem->url);

	doc = rssyl_fetch_feed(ritem->url, ritem->item.mtime, &title, &error);

	if (claws_is_exiting()) {
		debug_print("RSSyl: Claws-Mail is exiting, aborting feed parsing\n");
		log_print(LOG_PROTOCOL, RSSYL_LOG_EXITING);
		if (error)
			g_free(error);
		if (doc)
			xmlFreeDoc(doc);
		g_free(title);
		g_free(dir);
		return;
	}

	if (error) {
		log_error(LOG_PROTOCOL, _("RSSyl: Cannot update feed %s:\n%s\n"), ritem->url, error);
	}
	g_free(error);

	if (doc && title) {
		tmp = rssyl_feed_title_to_dir(title);
		dir = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, RSSYL_DIR,
				G_DIR_SEPARATOR_S, tmp, NULL);
		g_free(tmp);
		if( strcmp(title, ritem->official_name) ) {
			tmp = rssyl_feed_title_to_dir((&ritem->item)->name);
			dir2 = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, RSSYL_DIR,
					G_DIR_SEPARATOR_S, tmp,
					NULL);
			g_free(tmp);
			if( g_rename(dir2, dir) == -1 ) {
				g_warning("couldn't rename directory '%s'\n", dir2);
				g_free(dir);
				g_free(dir2);
				g_free(title);
				xmlFreeDoc(doc);
				return;
			}
			g_free(dir2);

			rssyl_props_update_name(ritem, title);

			g_free((&ritem->item)->name);
			(&ritem->item)->name = g_strdup(title);
			g_free(ritem->official_name);
			ritem->official_name = g_strdup(title);
			folder_item_rename(&ritem->item, title);
			rssyl_store_feed_props(ritem);
		} 

		rssyl_parse_feed(doc, ritem, NULL);

		if (claws_is_exiting()) {
			debug_print("RSSyl: Claws-Mail is exiting, aborting feed parsing\n");
			log_print(LOG_PROTOCOL, RSSYL_LOG_EXITING);
			if (error)
				g_free(error);
			if (doc)
				xmlFreeDoc(doc);
			g_free(title);
			g_free(dir);
			return;
		}

		rssyl_expire_items(ritem);
	}

	if (claws_is_exiting()) {
		g_free(title);
		g_free(dir);
		if (doc)
			xmlFreeDoc(doc);
		return;
	}

	if( ritem->fetch_comments == TRUE)
		rssyl_update_comments(ritem);

	ritem->item.mtime = time(NULL);
	debug_print("setting %s mtime to %ld\n", ritem->item.name, (long int)time(NULL));

	if (doc)
		xmlFreeDoc(doc);
	g_free(title);
	g_free(dir);

	log_print(LOG_PROTOCOL, RSSYL_LOG_UPDATED, ritem->url);
}

void rssyl_start_refresh_timeout(RSSylFolderItem *ritem)
{
	RSSylRefreshCtx *ctx;
	guint source_id;
	RSSylPrefs *rsprefs = NULL;

	g_return_if_fail(ritem != NULL);

	if( ritem->default_refresh_interval ) {
		rsprefs = rssyl_prefs_get();
		ritem->refresh_interval = rsprefs->refresh;
	}

	/* Do not start refreshing if the interval is set to 0 */
	if( ritem->refresh_interval == 0 )
		return;

	ctx = g_new0(RSSylRefreshCtx, 1);
	ctx->ritem = ritem;

	source_id = g_timeout_add(ritem->refresh_interval * 60 * 1000,
			(GSourceFunc)rssyl_refresh_timeout_cb, ctx );
	ritem->refresh_id = source_id;
	ctx->id = source_id;

	debug_print("RSSyl: start_refresh_timeout - %d min (id %d)\n",
			ritem->refresh_interval, ctx->id);
}

static void rssyl_find_feed_by_url_func(FolderItem *item, gpointer data)
{
	RSSylFolderItem *ritem;
	RSSylFindByUrlCtx *ctx = (RSSylFindByUrlCtx *)data;

	/* If we've already found a feed with desired URL, don't do anything */
	if( ctx->ritem != NULL )
		return;

	/* Only check rssyl folders */
	if( !IS_RSSYL_FOLDER_ITEM(item) )
		return;

	/* Don't bother with the root folder */
	if( folder_item_parent(item) == NULL )
		return;

	ritem = (RSSylFolderItem *)item;

	if( ritem->url && ctx->url && !strcmp(ritem->url, ctx->url) )
		ctx->ritem = ritem;
}

static RSSylFolderItem *rssyl_find_feed_by_url(gchar *url)
{
	RSSylFindByUrlCtx *ctx = NULL;
	RSSylFolderItem *ritem = NULL;

	g_return_val_if_fail(url != NULL, NULL);

	ctx = g_new0(RSSylFindByUrlCtx, 1);
	ctx->url = url;
	ctx->ritem = NULL;

	folder_func_to_all_folders(
			(FolderItemFunc)rssyl_find_feed_by_url_func, ctx);

	if( ctx->ritem != NULL )
		ritem = ctx->ritem;

	g_free(ctx);

	return ritem;
}

FolderItem *rssyl_subscribe_new_feed(FolderItem *parent, const gchar *url, 
				  gboolean verbose)
{
	gchar *title = NULL;
	xmlDocPtr doc;
	FolderItem *new_item;
	RSSylFolderItem *ritem;
	gchar *myurl = NULL;
	gchar *error = NULL;

	g_return_val_if_fail(parent != NULL, NULL);
	g_return_val_if_fail(url != NULL, NULL);

	if (!strncmp(url, "feed://", 7))
		myurl = g_strdup(url+7);
	else if (!strncmp(url, "feed:", 5))
		myurl = g_strdup(url+5);
	else
		myurl = g_strdup(url);

	myurl = g_strchomp(myurl);

	if( rssyl_find_feed_by_url(myurl) != NULL ) {
		if (verbose)
			alertpanel_error(_("You are already subscribed to this feed."));
		g_free(myurl);
		return NULL;
	}
	
	main_window_cursor_wait(mainwindow_get_mainwindow());
	doc = rssyl_fetch_feed(myurl, -1, &title, &error);
	main_window_cursor_normal(mainwindow_get_mainwindow());
	if( !doc || !title ) {
		if (verbose) {
			gchar *tmp;
			tmp = g_markup_printf_escaped(_("Couldn't fetch URL '%s':\n%s"),
						myurl, error ? error:_("Unknown error"));
			alertpanel_error("%s", tmp);
			g_free(tmp);
		} else
			log_error(LOG_PROTOCOL, _("Couldn't fetch URL '%s':\n%s\n"), myurl, error ? error:_("Unknown error"));
		g_free(myurl);
		g_free(error);
		g_free(title);
		if (doc)
			xmlFreeDoc(doc);
		return NULL;
	}

	g_free(error);

	new_item = folder_create_folder(parent, title);
	if( !new_item ) {
		if (verbose) {
			gchar *tmp;
			tmp = g_markup_printf_escaped("%s", title);
			alertpanel_error(_("Can't subscribe feed '%s'."), title);
			g_free(tmp);
		}
		g_free(myurl);
		xmlFreeDoc(doc);
		return NULL;
	}

	ritem = (RSSylFolderItem *)new_item;
	ritem->url = myurl;
	
	ritem->default_refresh_interval = TRUE;
	ritem->default_expired_num = TRUE;

	rssyl_store_feed_props(ritem);

	folder_write_list();

	rssyl_parse_feed(doc, ritem, NULL);
	xmlFreeDoc(doc);

	rssyl_expire_items(ritem);

	/* TODO: allow user to enable this when adding the feed */
	if( ritem->fetch_comments )
		rssyl_update_comments(ritem);

	/* update official_title */
	rssyl_store_feed_props(ritem);
	rssyl_start_refresh_timeout(ritem);

	folder_item_scan(new_item);
	return new_item;
}

static gint rssyl_expire_sort_func(RSSylFeedItem *a, RSSylFeedItem *b)
{
	if( !a || !b )
		return 0;

	return (b->date - a->date);
}

void rssyl_expire_items(RSSylFolderItem *ritem)
{
	int num;
	RSSylFeedItem *fitem;
	GSList *i;

	g_return_if_fail(ritem != NULL);

	rssyl_read_existing(ritem);

	g_return_if_fail(ritem->contents != NULL);

	num = ritem->expired_num;
	if( num == -1 ||
			(num > (g_slist_length(ritem->contents) - ritem->last_count)) )
		return;

	debug_print("RSSyl: rssyl_expire_items()\n");

	ritem->contents = g_slist_sort(ritem->contents,
			(GCompareFunc)rssyl_expire_sort_func);

	debug_print("RSSyl: finished sorting\n");

	while( (i = g_slist_nth(ritem->contents, ritem->last_count + num + 1)) ) {
		fitem = (RSSylFeedItem *)i->data;
		debug_print("RSSyl: expiring '%s'\n", fitem->realpath);
		g_remove(fitem->realpath);
		rssyl_free_feeditem(fitem);
		fitem = NULL;
		ritem->contents = g_slist_remove(ritem->contents, i->data);
	}

	folder_item_scan(&ritem->item);

	debug_print("RSSyl: finished expiring\n");
}


void rssyl_refresh_all_func(FolderItem *item, gpointer data)
{
	RSSylFolderItem *ritem = (RSSylFolderItem *)item;
	/* Only try to refresh our feed folders */
	if( !IS_RSSYL_FOLDER_ITEM(item) )
		return;

	/* Don't try to refresh the root folder */
	if( folder_item_parent(item) == NULL )
		return;
	/* Don't try to update normal folders */
	if (ritem->url == NULL)
		return;
	rssyl_update_feed((RSSylFolderItem *)item);
}

void rssyl_refresh_all_feeds(void)
{
	if (prefs_common_get_prefs()->work_offline && 
	    !inc_offline_should_override(TRUE,
			ngettext("Claws Mail needs network access in order "
			    "to update the feed.",
			    "Claws Mail needs network access in order "
			    "to update the feeds.", 2))) {
			return;
	}

	folder_func_to_all_folders((FolderItemFunc)rssyl_refresh_all_func, NULL);
}
