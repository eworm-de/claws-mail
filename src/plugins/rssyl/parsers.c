/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2004 Hiroyuki Yamamoto
 * This file (C) 2005 Andrej Kacian <andrej@kacian.sk>
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
#endif

#include <glib.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/HTMLtree.h>

#include "date.h"
#include "feed.h"
#include "strreplace.h"
#include "utils.h"
#include "procheader.h"

gint rssyl_parse_rdf(xmlDocPtr doc, RSSylFolderItem *ritem, gchar *parent)
{
	xmlNodePtr rnode, node, n;
	RSSylFeedItem *fitem = NULL;
	gint count = 0;
	gchar *content = NULL;
	g_return_val_if_fail(doc != NULL, 0);
	g_return_val_if_fail(ritem != NULL, 0);
#ifdef RSSYL_DEBUG
	gchar *fetched = NULL;
#endif	/* RSSYL_DEBUG */

	if( ritem->contents == NULL )
		rssyl_read_existing(ritem);

	rnode = xmlDocGetRootElement(doc);

	for( node = rnode->children; node; node = node->next ) {
		if( !xmlStrcmp(node->name, "item") ) {
			/* We've found an "item" tag, let's poke through its contents */
			fitem = g_new0(RSSylFeedItem, 1);
			fitem->date = 0;
#ifdef RSSYL_DEBUG
			fetched = xmlGetProp(rnode, "fetched");
			fitem->debug_fetched = atoll(fetched);
			xmlFree(fetched);
#endif	/* RSSYL_DEBUG */

			for( n = node->children; n; n = n->next ) {
				/* Title */
				if( !xmlStrcmp(n->name, "title") ) {
					content = xmlNodeGetContent(n);
					fitem->title = rssyl_format_string(content, TRUE, TRUE);
					xmlFree(content);
					debug_print("RSSyl: XML - RDF title is '%s'\n", fitem->title);
				}

				/* Text */
				if( !xmlStrcmp(n->name, "description") ) {
					content = xmlNodeGetContent(n);
					fitem->text = rssyl_format_string(content, FALSE, FALSE);
					xmlFree(content);
					debug_print("RSSyl: XML - got RDF text\n");
				}

				/* URL */
				if( !xmlStrcmp(n->name, "link") ) {
					content = xmlNodeGetContent(n);
					fitem->link = rssyl_format_string(content, FALSE, TRUE);
					xmlFree(content);
					debug_print("RSSyl: XML - RDF link is '%s'\n", fitem->link);
				}

				/* Date - rfc822 format */
				if( !xmlStrcmp(n->name, "pubDate") ) {
					content = xmlNodeGetContent(n);
					fitem->date = procheader_date_parse(NULL, content, 0);
					xmlFree(content);
					if( fitem->date > 0 ) {
						debug_print("RSSyl: XML - RDF pubDate found\n" );
					} else
						fitem->date = 0;
				}
				/* Date - ISO8701 format */
				if( !xmlStrcmp(n->name, "date") &&
						(!xmlStrcmp(n->ns->prefix, "ns")
						 || !xmlStrcmp(n->ns->prefix, "dc")) ) {
					content = xmlNodeGetContent(n);
					fitem->date = parseISO8601Date(content);
					xmlFree(content);
					debug_print("RSSyl: XML - RDF date found\n" );
				}

				/* Author */
				if( !xmlStrcmp(n->name, "creator") ) {
					content = xmlNodeGetContent(n);
					fitem->author = rssyl_format_string(content, TRUE, TRUE);
					xmlFree(content);
					debug_print("RSSyl: XML - RDF author is '%s'\n", fitem->author);
				}
			}
		}

		if( fitem && fitem->link && fitem->title ) {
			if (rssyl_add_feed_item(ritem, fitem) == FALSE) {
				rssyl_free_feeditem(fitem);
				fitem = NULL;
			}
			fitem = NULL;
			count++;
		}
	}

	return count;
}


/* rssyl_parse_rss()
 *
 * This is where we parse the fetched rss document and create a
 * RSSylFolderItem from it. Returns number of parsed items
 */
gint rssyl_parse_rss(xmlDocPtr doc, RSSylFolderItem *ritem, gchar *parent)
{
	xmlXPathContextPtr context;
	xmlXPathObjectPtr result;
	xmlNodePtr node, n, rnode;
	gint i, count = 0;
	RSSylFeedItem *fitem = NULL;
	gchar *xpath;
	gboolean got_encoded, got_author;
	gchar *rootnode = NULL;
	RSSylFeedItemMedia *media;
	gchar *media_url, *media_type;
	gulong media_size = 0;
#ifdef RSSYL_DEBUG
	gchar *fetched = NULL;
#endif	/* RSSYL_DEBUG */

	g_return_val_if_fail(doc != NULL, 0);
	g_return_val_if_fail(ritem != NULL, 0);

	if( ritem->contents == NULL )
		rssyl_read_existing(ritem);

	rnode = xmlDocGetRootElement(doc);

	rootnode = g_ascii_strdown(rnode->name, -1);
	xpath = g_strconcat("/", rootnode,
				"/channel/item",	NULL);
	g_free(rootnode);
	context = xmlXPathNewContext(doc);
	if( !(result = xmlXPathEvalExpression(xpath, context)) ){
		debug_print("RSSyl: XML - no result found for '%s'\n", xpath);
		xmlXPathFreeContext(context);
		g_free(xpath);
		return 0;
	}

	g_free(xpath);

	for( i = 0; i < result->nodesetval->nodeNr; i++ ) {
		node = result->nodesetval->nodeTab[i];
		
		if ((n = node->children) == NULL)
			continue;

		fitem = g_new0(RSSylFeedItem, 1);
		fitem->media = NULL;
		fitem->date = 0;
#ifdef RSSYL_DEBUG
		fetched = xmlGetProp(rnode, "fetched");
		fitem->debug_fetched = atoll(fetched);
		xmlFree(fetched);
#endif	/* RSSYL_DEBUG */
		fitem->text = NULL;
		
		if (parent)
			fitem->parent_link = g_strdup(parent);

		got_encoded = FALSE;
		got_author = FALSE;
		do {
			gchar *content = NULL;

			/* Title */
			if( !xmlStrcmp(n->name, "title") ) {
				content = xmlNodeGetContent(n);
				fitem->title = rssyl_format_string(content, TRUE, TRUE);
				xmlFree(content);
				debug_print("RSSyl: XML - item title: '%s'\n", fitem->title);
			}

			/* Text */
			if( !xmlStrcmp(n->name, "description") ) {
				if( (fitem->text == NULL) && (got_encoded == FALSE) ) {
					content = xmlNodeGetContent(n);
					debug_print("RSSyl: XML - item text (description) caught\n");
					fitem->text = rssyl_format_string(content, FALSE, FALSE);
					xmlFree(content);
				}
			}
			if( !xmlStrcmp(n->name, "encoded")
					&& !xmlStrcmp(n->ns->prefix, "content") ) {
				debug_print("RSSyl: XML - item text (content) caught\n");

				if (fitem->text != NULL)
					g_free(fitem->text); /* free "description" */
					
				content = xmlNodeGetContent(n);
				fitem->text = rssyl_format_string(content, FALSE, FALSE);
				xmlFree(content);
				got_encoded = TRUE;
			}

			/* URL link to the original post */
			if( !xmlStrcmp(n->name, "link") ) {
				content = xmlNodeGetContent(n);
				fitem->link = rssyl_format_string(content, FALSE, TRUE);
				xmlFree(content);
				debug_print("RSSyl: XML - item link: '%s'\n", fitem->link);
			}

			/* GUID - sometimes used as link */
			if( !xmlStrcmp(n->name, "guid") ) {
				gchar *tmp = xmlGetProp(n, "isPermaLink");
				content = xmlNodeGetContent(n);
				fitem->id_is_permalink = FALSE;
				if( !tmp || xmlStrcmp(tmp, "false") )	/* permalink? */
					fitem->id_is_permalink = TRUE;
				fitem->id = rssyl_format_string(content, FALSE, TRUE);
				xmlFree(content);
				debug_print("RSSyl: XML - item guid: '%s'\n", fitem->id);
				xmlFree(tmp);
			}

			/* Date - rfc822 format */
			if( !xmlStrcmp(n->name, "pubDate") ) {
				content = xmlNodeGetContent(n);
				fitem->date = procheader_date_parse(NULL, content, 0);
				xmlFree(content);
				if( fitem->date > 0 ) {
					debug_print("RSSyl: XML - item date found: %d\n", (gint)fitem->date);
				} else
					fitem->date = 0;
			}
			/* Date - ISO8701 format */
			if( !xmlStrcmp(n->name, "date") && !xmlStrcmp(n->ns->prefix, "dc") ) {
				content = xmlNodeGetContent(n);
				fitem->date = parseISO8601Date(content);
				xmlFree(content);
				debug_print("RSSyl: XML - item date found\n" );
			}

			/* Author */
			if( !xmlStrcmp(n->name, "author") ) {
				content = xmlNodeGetContent(n);
				fitem->author = rssyl_format_string(content, TRUE, TRUE);
				xmlFree(content);
				debug_print("RSSyl: XML - item author: '%s'\n", fitem->author);
				got_author = TRUE;
			}

			if( !xmlStrcmp(n->name, "creator")
					&& !xmlStrcmp(n->ns->prefix, "dc") && !got_author) {
				content = xmlNodeGetContent(n);
				fitem->author = rssyl_format_string(content, TRUE, TRUE);
				xmlFree(content);
				debug_print("RSSyl: XML - item author (creator): '%s'\n", fitem->author);
			}

			/* Media enclosure */
			if( !xmlStrcmp(n->name, "enclosure") ) {
				gchar *tmp = xmlGetProp(n, "length");
				media_url = xmlGetProp(n, "url");
				media_type = xmlGetProp(n, "type");
				media_size = (tmp ? atoi(tmp) : 0);
				xmlFree(tmp);

				if( media_url != NULL &&
						media_type != NULL &&
						media_size != 0 ) {
					debug_print("RSSyl: XML - enclosure: '%s' [%s] (%ld)\n",
							media_url, media_type, media_size);
					media = g_new(RSSylFeedItemMedia, 1);
					media->url = media_url;
					media->type = media_type;
					media->size = media_size;
					fitem->media = media;
				} else {
					debug_print("RSSyl: XML - enclosure found, but some data is missing\n");
					g_free(media_url);
					g_free(media_type);
				}
			}

			/* Comments */
			if( !xmlStrcmp(n->name, "commentRSS") || !xmlStrcmp(n->name, "commentRss") ) {
				content = xmlNodeGetContent(n);
				fitem->comments_link = rssyl_format_string(content, FALSE, TRUE);
				xmlFree(content);
				debug_print("RSSyl: XML - comments_link: '%s'\n", fitem->comments_link);
			}
		} while( (n = n->next) != NULL);

		if( (fitem->link || fitem->id) && fitem->title ) {
			if (rssyl_add_feed_item(ritem, fitem) == FALSE) {
				rssyl_free_feeditem(fitem);
				fitem = NULL;
			}
			count++;
		}
	}

	xmlXPathFreeObject(result);
	xmlXPathFreeContext(context);

	return count;
}

/* rssyl_parse_atom()
 *
 * This is where we parse the fetched atom document and create a
 * RSSylFolderItem from it. Returns number of parsed items
 */
gint rssyl_parse_atom(xmlDocPtr doc, RSSylFolderItem *ritem, gchar *parent)
{
	xmlNodePtr node, n, h;
	xmlBufferPtr buf = NULL;
	gint count = 0;
	RSSylFeedItem *fitem = NULL;
	RSSylFeedItemMedia *media = NULL;
	gchar *link_type, *link_href, *link_rel, *tmp, *content = NULL;
	gulong link_size;

	g_return_val_if_fail(doc != NULL, 0);
	g_return_val_if_fail(ritem != NULL, 0);

	if( ritem->contents == NULL )
		rssyl_read_existing(ritem);

	node = xmlDocGetRootElement(doc);

	if (node == NULL)
		return 0;

	node = node->children;

	for (; node; node = node->next) {
		gboolean got_content = FALSE;
		if (xmlStrcmp(node->name, "entry")) {
			continue;
		}
	
		n = node->children;
		fitem = g_new0(RSSylFeedItem, 1);
		fitem->date = 0;
		fitem->date_published = 0;
		fitem->text = NULL;
		
		if (parent)
			fitem->parent_link = g_strdup(parent);

		do {
			/* Title */
			if( !xmlStrcmp(n->name, "title") ) {
				content = xmlNodeGetContent(n);
				fitem->title = rssyl_format_string(content, TRUE, TRUE);
				xmlFree(content);
				debug_print("RSSyl: XML - Atom item title: '%s'\n", fitem->title);
			}

			/* ID */
			if( !xmlStrcmp(n->name, "id") ) {
				content = xmlNodeGetContent(n);
				fitem->id = g_strdup_printf("%s%s", (parent?"comment-":""), content);
				xmlFree(content);
				debug_print("RSSyl: XML - Atom id: '%s'\n", fitem->id);
			}

			/* Text */
			if( !xmlStrcmp(n->name, "summary") && !got_content ) {
				content = xmlNodeGetContent(n);
				debug_print("RSSyl: XML - Atom item text (summary) caught\n");
				fitem->text = rssyl_format_string(content, FALSE, FALSE);
				xmlFree(content);
			}

			if( !xmlStrcmp(n->name, "content") ) {
				gchar *tmp = xmlGetProp(n, "type");
				debug_print("RSSyl: XML - Atom item text (content) caught\n");
				if (fitem->text)
					g_free(fitem->text);
				if( !xmlStrcmp(tmp, "xhtml")) {
					for( h = n->children; h; h = h->next ) {
						if( !xmlStrcmp(h->name, "div") ) {
							buf = xmlBufferCreate();
							htmlNodeDump(buf, doc, h);
							content = g_strdup((gchar *)xmlBufferContent(buf));
							xmlBufferFree(buf);
						}
					}
				} else
					content = xmlNodeGetContent(n);
				xmlFree(tmp);
				fitem->text = rssyl_format_string(content, FALSE, FALSE);
				xmlFree(content);
				got_content = TRUE;
			}

			/* link */
			if( !xmlStrcmp(n->name, "link") ) {
				link_type = xmlGetProp(n, "type");
				link_rel = xmlGetProp(n, "rel");
				link_href = xmlGetProp(n, "href");
				tmp = xmlGetProp(n, "length");
				link_size = (tmp ? atoi(tmp) : 0);
				g_free(tmp);

				if( !link_rel || (link_rel && !xmlStrcmp(link_rel, "alternate")) ) {
					fitem->link = link_href;
					debug_print("RSSyl: XML - Atom item link: '%s'\n", fitem->link);
					xmlFree(link_type);
					xmlFree(link_rel);
				} else if( link_rel && !xmlStrcmp(link_rel, "enclosure") ) {
					debug_print("RSSyl: XML - Atom item enclosure: '%s' (%ld) [%s]\n",
							link_href, link_size, link_type);
					media = g_new(RSSylFeedItemMedia, 1);
					media->url = link_href;
					media->type = link_type;
					media->size = link_size;
					fitem->media = media;
					xmlFree(link_rel);
				} else {
					xmlFree(link_type);
					xmlFree(link_rel);
					xmlFree(link_href);
				}
			}

			/* Date published - ISO8701 format */
			if( !xmlStrcmp(n->name, "published") ) {
				content = xmlNodeGetContent(n);
				fitem->date_published = parseISO8601Date(content);
				xmlFree(content);
				debug_print("RSSyl: XML - Atom item 'issued' date found\n" );
			}

			/* Date modified - ISO8701 format */
			if( !xmlStrcmp(n->name, "updated") ) {
				content = xmlNodeGetContent(n);
				fitem->date = parseISO8601Date(content);
				xmlFree(content);
				debug_print("RSSyl: XML - Atom item 'updated' date found\n" );
			}

			/* Author */
			if( !xmlStrcmp(n->name, "author") ) {
				xmlNodePtr subnode;
				gchar *name = NULL, *mail = NULL;
				gchar *tmp;
				for (subnode = n->children; subnode; subnode = subnode->next) {
					content = xmlNodeGetContent(subnode);
					if (!xmlStrcmp(subnode->name, "name") && !name)
						name = g_strdup(content);
					if (!xmlStrcmp(subnode->name, "email") && !mail)
						mail = g_strdup(content);
					xmlFree(content);
				}
				tmp = g_strdup_printf("%s%s%s%s%s",
							name ? name:"",
							name && mail ? " <":(mail?"<":""),
							mail ? mail:"",
							mail ? ">":"",
							!name && !mail ? "N/A":"");
				fitem->author = rssyl_format_string(tmp, TRUE, TRUE);
				g_free(tmp);
				g_free(name);
				g_free(mail);
				debug_print("RSSyl: XML - Atom item author: '%s'\n", fitem->author);
			}

			/* Comments */
			if( !xmlStrcmp(n->name, "commentRSS") || !xmlStrcmp(n->name, "commentRss")) {
				content = xmlNodeGetContent(n);
				fitem->comments_link = rssyl_format_string(content, FALSE, TRUE);
				xmlFree(content);
				debug_print("RSSyl: XML - comments_link: '%s'\n", fitem->comments_link);
			}
		} while( (n = n->next) != NULL);

		if( fitem->id && fitem->title && fitem->date ) {

			/* If no link is available, and we can safely guess ID
			 * might be a (perma)link, mark it so. */
			if (!fitem->link && fitem->id	/* no url, but we have id */
					&& (!strncmp(fitem->id, "http:", 5) /* id looks like an url */
						|| !strncmp(fitem->id, "https:", 6))) {
				if (!ritem->url || strcmp(ritem->url, fitem->id)) {
					/* id is different from feed url (good chance it is a permalink) */
					debug_print("RSSyl: Marking ID as permalink\n");
					fitem->id_is_permalink = TRUE;
				}
			}

			if (rssyl_add_feed_item(ritem, fitem) == FALSE) {
				rssyl_free_feeditem(fitem);
				fitem = NULL;
			}
			count++;
		} else
			debug_print("RSSyl: Incomplete Atom entry, need at least 'id', 'title' and 'updated' tags\n");
	}

	return count;
}
