/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2004 Hiroyuki Yamamoto
 * This file (C) 2005 Andrej Kacian <andrej@kacian.sk>
 *
 * - functions for handling feeds.xml file
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
#endif

#include <glib.h>

#include <libxml/parser.h>
#include <libxml/xpath.h>

#include "folder.h"

#include "feed.h"
#include "feedprops.h"
#include "rssyl.h"
#include "rssyl_prefs.h"

static gchar *rssyl_get_props_path(void)
{
	return g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S, RSSYL_DIR,
			G_DIR_SEPARATOR_S, RSSYL_PROPS_FILE, NULL);
}


/* rssyl_store_feed_props()
 *
 * Stores feed properties into feeds.xml file in the rcdir. Updates if already
 * stored for this feed.
 */
void rssyl_store_feed_props(RSSylFolderItem *ritem)
{
	gchar *path;
	xmlDocPtr doc;
	xmlNodePtr node, rootnode;
	xmlXPathObjectPtr result;
	xmlXPathContextPtr context;
	FolderItem *item = &ritem->item;
	gboolean found = FALSE, def_ri, def_ex;
	gint i;

	g_return_if_fail(ritem != NULL);
	g_return_if_fail(ritem->url != NULL);

	def_ri = ritem->default_refresh_interval;
	if( def_ri )
		ritem->refresh_interval = rssyl_prefs_get()->refresh;

	def_ex = ritem->default_expired_num;
	if( def_ex )
		ritem->expired_num = rssyl_prefs_get()->expired;

	path = rssyl_get_props_path();

	if( !(doc = xmlParseFile(path)) ) {
		debug_print("RSSyl: file %s doesn't exist, creating it\n", path);
		doc = xmlNewDoc("1.0");

		rootnode = xmlNewNode(NULL, "feeds");
		xmlDocSetRootElement(doc, rootnode);
	} else {
		rootnode = xmlDocGetRootElement(doc);
	}

	context = xmlXPathNewContext(doc);
	if( !(result = xmlXPathEvalExpression(RSSYL_PROPS_XPATH, context)) ) {
		debug_print("RSSyl: XML - no result found for %s\n", RSSYL_PROPS_XPATH);
		xmlXPathFreeContext(context);
	} else {
		for( i = 0; i < result->nodesetval->nodeNr; i++ ) {
			gchar *tmp, *t_prop = NULL;
			node = result->nodesetval->nodeTab[i];
			tmp = xmlGetProp(node, RSSYL_PROP_NAME);

			if( !strcmp(tmp, item->name) ) {
				debug_print("RSSyl: XML - updating node for '%s'\n", item->name);

				xmlSetProp(node, RSSYL_PROP_NAME, item->name);
				xmlSetProp(node, RSSYL_PROP_OFFICIAL_NAME, 
						ritem->official_name?ritem->official_name:item->name);
				xmlSetProp(node, RSSYL_PROP_URL, ritem->url);

				xmlSetProp(node, RSSYL_PROP_DEF_REFRESH, (def_ri ? "1" : "0") );
				if( !def_ri ) {
					t_prop = g_strdup_printf("%d", ritem->refresh_interval);
					xmlSetProp(node, RSSYL_PROP_REFRESH,
							t_prop );
					g_free(t_prop);
				}

				xmlSetProp(node, RSSYL_PROP_DEF_EXPIRED, (def_ex ? "1" : "0") );
				if( !def_ex ) {
					t_prop = g_strdup_printf("%d", ritem->expired_num);
					xmlSetProp(node, RSSYL_PROP_EXPIRED,
							t_prop );
					g_free(t_prop);
				}
				xmlSetProp(node, RSSYL_PROP_FETCH_COMMENTS,
						(ritem->fetch_comments ? "1" : "0"));
				t_prop = g_strdup_printf("%d", ritem->fetch_comments_for);
				xmlSetProp(node, RSSYL_PROP_FETCH_COMMENTS_FOR, t_prop);
				g_free(t_prop);

				t_prop = g_strdup_printf("%d", ritem->silent_update);
				xmlSetProp(node, RSSYL_PROP_SILENT_UPDATE, t_prop);
				g_free(t_prop);

				found = TRUE;
			}
			xmlFree(tmp);
		}
	}

	xmlXPathFreeContext(context);
	xmlXPathFreeObject(result);

	/* node for our feed doesn't exist, let's make one */
	if( !found ) {
		debug_print("RSSyl: XML - creating node for '%s', storing URL '%s'\n",
				item->name, ritem->url);
		node = xmlNewTextChild(rootnode, NULL, "feed", NULL);
		xmlSetProp(node, RSSYL_PROP_NAME, item->name);
		xmlSetProp(node, RSSYL_PROP_OFFICIAL_NAME, 
				ritem->official_name?ritem->official_name:item->name);
		xmlSetProp(node, RSSYL_PROP_URL, ritem->url);
		xmlSetProp(node, RSSYL_PROP_DEF_REFRESH, (def_ri ? "1" : "0") );
		if( !def_ri )
			xmlSetProp(node, RSSYL_PROP_REFRESH,
					g_strdup_printf("%d", ritem->refresh_interval) );
		xmlSetProp(node, RSSYL_PROP_DEF_EXPIRED, (def_ex ? "1" : "0") );
		if( !def_ex )
			xmlSetProp(node, RSSYL_PROP_EXPIRED,
					g_strdup_printf("%d", ritem->expired_num) );
	}

	xmlSaveFormatFile(path, doc, 1);
	xmlFreeDoc(doc);
	g_free(path);
}

/* rssyl_get_feed_props()
 *
 * Retrieves props for feed from feeds.xml file in the rcdir.
 */
void rssyl_get_feed_props(RSSylFolderItem *ritem)
{
	gchar *path, *tmp = NULL;
	xmlDocPtr doc;
	xmlNodePtr node;
	xmlXPathObjectPtr result;
	xmlXPathContextPtr context;
	FolderItem *item = &ritem->item;
	gint i, tmpi;
	gboolean force_update = FALSE;
	RSSylPrefs *rsprefs = NULL;

	g_return_if_fail(ritem != NULL);

	if( ritem->url ) {
		g_free(ritem->url);
		ritem->url = NULL;
	}

	ritem->default_refresh_interval = TRUE;
	ritem->refresh_interval = rssyl_prefs_get()->refresh;
	ritem->expired_num = rssyl_prefs_get()->expired;

	path = rssyl_get_props_path();

	doc = xmlParseFile(path);
	g_return_if_fail(doc != NULL);

	context = xmlXPathNewContext(doc);
	if( !(result = xmlXPathEvalExpression(RSSYL_PROPS_XPATH, context)) ) {
		debug_print("RSSyl: XML - no result found for %s\n", RSSYL_PROPS_XPATH);
		xmlXPathFreeContext(context);
	} else {
		for( i = 0; i < result->nodesetval->nodeNr; i++ ) {
			gchar *property = NULL;
			node = result->nodesetval->nodeTab[i];
			property = xmlGetProp(node, RSSYL_PROP_NAME);
			if( !strcmp(property, item->name) ) {
				/* official name */
				tmp = xmlGetProp(node, RSSYL_PROP_OFFICIAL_NAME);
				ritem->official_name = (tmp ? g_strdup(tmp) : g_strdup(item->name));
				if (tmp == NULL)
					force_update = TRUE;
				xmlFree(tmp);
				tmp = NULL;

				/* URL */
				tmp = xmlGetProp(node, RSSYL_PROP_URL);
				ritem->url = (tmp ? g_strdup(tmp) : NULL);
				xmlFree(tmp);
				tmp = NULL;

				tmp = xmlGetProp(node, RSSYL_PROP_DEF_REFRESH);
				tmpi = 0;
				if( tmp )
					tmpi = atoi(tmp);
				ritem->default_refresh_interval = (tmpi ? TRUE : FALSE );
				xmlFree(tmp);
				tmp = NULL;

				/* refresh_interval */
				tmp = xmlGetProp(node, RSSYL_PROP_REFRESH);
				if( ritem->default_refresh_interval )
					ritem->refresh_interval = rssyl_prefs_get()->refresh;
				else {
					tmpi = -1;
					if( tmp )
						tmpi = atoi(tmp);

					ritem->refresh_interval =
						(tmpi != -1 ? tmpi : rssyl_prefs_get()->refresh);
				}
				
				xmlFree(tmp);
				tmp = NULL;

				/* expired_num */
				tmp = xmlGetProp(node, RSSYL_PROP_DEF_EXPIRED);
				tmpi = 0;
				if( tmp ) {
					tmpi = atoi(tmp);
					ritem->default_expired_num = tmpi;
				}
				xmlFree(tmp);
				tmp = NULL;

				/* fetch_comments */
				tmp = xmlGetProp(node, RSSYL_PROP_FETCH_COMMENTS);
				tmpi = 0;
				if( tmp ) {
					tmpi = atoi(tmp);
					ritem->fetch_comments = tmpi;
				}
				xmlFree(tmp);
				tmp = NULL;

				/* fetch_comments_for */
				tmp = xmlGetProp(node, RSSYL_PROP_FETCH_COMMENTS_FOR);
				tmpi = 0;
				if( tmp ) {
					tmpi = atoi(tmp);
					ritem->fetch_comments_for = tmpi;
				}
				xmlFree(tmp);
				tmp = NULL;

				/* silent_update */
				tmp = xmlGetProp(node, RSSYL_PROP_SILENT_UPDATE);
				tmpi = 0;
				if( tmp ) {
					tmpi = atoi(tmp);
					ritem->silent_update = tmpi;
				}
				xmlFree(tmp);
				tmp = NULL;

				tmp = xmlGetProp(node, RSSYL_PROP_EXPIRED);

				if( ritem->default_expired_num )
					ritem->expired_num = rssyl_prefs_get()->expired;
				else {
					tmpi = -2;
					if( tmp )
						tmpi = atoi(tmp);

					ritem->expired_num = (tmpi != -2 ? tmpi : rssyl_prefs_get()->expired);
				}

				xmlFree(tmp);
				tmp = NULL;

				debug_print("RSSyl: XML - props for '%s' loaded\n", item->name);

				/* Start automatic refresh timer, if necessary */
				if( ritem->refresh_id == 0 ) {
					/* Check if user wants the default for this feed */
					if( ritem->default_refresh_interval ) {
						rsprefs = rssyl_prefs_get();
						ritem->refresh_interval = rsprefs->refresh;
					}

					/* Start the timer, if determined interval is >0 */
					if( ritem->refresh_interval >= 0 )
						rssyl_start_refresh_timeout(ritem);
				}
			}
			xmlFree(property);
		}
	}

	xmlXPathFreeObject(result);
	xmlXPathFreeContext(context);
	xmlFreeDoc(doc);
	g_free(path);
	if (force_update)
		rssyl_store_feed_props(ritem);
}

void rssyl_remove_feed_props(RSSylFolderItem *ritem)
{
	gchar *path;
	xmlDocPtr doc;
	xmlNodePtr node;
	xmlXPathObjectPtr result;
	xmlXPathContextPtr context;
	FolderItem *item = &ritem->item;
	gint i;

	g_return_if_fail(ritem != NULL);

	path = rssyl_get_props_path();

	doc = xmlParseFile(path);
	g_return_if_fail(doc != NULL);

	context = xmlXPathNewContext(doc);
	if( !(result = xmlXPathEvalExpression(RSSYL_PROPS_XPATH, context)) ) {
		debug_print("RSSyl: XML - no result found for %s\n", RSSYL_PROPS_XPATH);
		xmlXPathFreeContext(context);
	} else {
		for( i = 0; i < result->nodesetval->nodeNr; i++ ) {
			gchar *tmp;
			node = result->nodesetval->nodeTab[i];
			tmp = xmlGetProp(node, RSSYL_PROP_NAME);
			if( !strcmp(tmp, item->name) ) {
				debug_print("RSSyl: XML - found node for '%s', removing\n", item->name);
				xmlUnlinkNode(node);
			}
			xmlFree(tmp);
		}
	}

	xmlXPathFreeObject(result);
	xmlXPathFreeContext(context);

	xmlSaveFormatFile(path, doc, 1);

	xmlFreeDoc(doc);
	g_free(path);
}

void rssyl_props_update_name(RSSylFolderItem *ritem, gchar *new_name)
{
	gchar *path;
	xmlDocPtr doc;
	xmlNodePtr node, rootnode;
	xmlXPathObjectPtr result;
	xmlXPathContextPtr context;
	FolderItem *item = &ritem->item;
	gboolean found = FALSE;
	gint i;

	g_return_if_fail(ritem != NULL);
	g_return_if_fail(ritem->url != NULL);

	path = rssyl_get_props_path();

	if( !(doc = xmlParseFile(path)) ) {
		debug_print("RSSyl: file %s doesn't exist, creating it\n", path);
		doc = xmlNewDoc("1.0");

		rootnode = xmlNewNode(NULL, "feeds");
		xmlDocSetRootElement(doc, rootnode);
	} else {
		rootnode = xmlDocGetRootElement(doc);
	}

	context = xmlXPathNewContext(doc);
	if( !(result = xmlXPathEvalExpression(RSSYL_PROPS_XPATH, context)) ) {
		debug_print("RSSyl: XML - no result found for %s\n", RSSYL_PROPS_XPATH);
		xmlXPathFreeContext(context);
	} else {
		for( i = 0; i < result->nodesetval->nodeNr; i++ ) {
			gchar *tmp;
			node = result->nodesetval->nodeTab[i];
			tmp = xmlGetProp(node, RSSYL_PROP_NAME);
			if( !strcmp(tmp, item->name) ) {
				debug_print("RSSyl: XML - updating node for '%s'\n", item->name);
				xmlSetProp(node, "name", new_name);
				found = TRUE;
			}
			xmlFree(tmp);
		}
	}

	xmlXPathFreeContext(context);
	xmlXPathFreeObject(result);

	if( !found )
		debug_print("couldn't find feed\n");

	xmlSaveFormatFile(path, doc, 1);
	xmlFreeDoc(doc);
	g_free(path);
}
