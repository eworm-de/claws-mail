/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2005-2023 the Claws Mail Team and Andrej Kacian <andrej@kacian.sk>
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

/* Global includes */
#include <errno.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <string.h>

/* Claws Mail includes */
#include <codeconv.h>
#include <procmsg.h>
#include <common/utils.h>
#include <file-utils.h>

/* Local includes */
#include "libfeed/date.h"
#include "libfeed/feeditem.h"
#include "parse822.h"
#include "rssyl.h"
#include "rssyl_deleted.h"
#include "rssyl_feed.h"
#include "rssyl_parse_feed.h"
#include "strutils.h"

gint rssyl_add_msg(Folder *folder, FolderItem *dest, const gchar *file,
		MsgFlags *flags);

/* rssyl_cb_feed_compare()
 *
 * GCompareFunc function called by glib2's g_slist_find_custom().
 */

static gint rssyl_cb_feed_compare(const FeedItem *a, const FeedItem *b)
{
	gboolean date_eq = FALSE, url_eq = FALSE, title_eq = FALSE;
	gboolean pubdate_eq = FALSE, moddate_eq = FALSE;
	gboolean no_url = FALSE, no_date = FALSE, no_title = FALSE;
	gboolean no_pubdate = FALSE, no_moddate = FALSE;
	gchar *atit = NULL, *btit = NULL;

	g_return_val_if_fail(a != NULL && b != NULL, 1);

	/* ID should be unique. If it matches, we've found what we came for. */
	if( (a->id != NULL) && (b->id != NULL) ) {
			if( !strcmp(a->id, b->id) ) {
				return 0;
			}

			/* If both IDs are present, but they do not match, these are not the
			 * droids we're looking for. */
			return 1;
	}

	/* Ok, we have no ID to aid us. Let's have a look at item timestamps
	 * and item title & url. */
	if( (a->url != NULL) && (b->url != NULL) ) {
		if( !strcmp(a->url, b->url) )
			url_eq = TRUE;
	} else
		no_url = TRUE;

	/* Now we prepare some boolean flags to help us express comparing choices
	 * later on. */

	/* Title */
	if( (a->title != NULL) && (b->title != NULL) ) {
		atit = conv_unmime_header(a->title, CS_UTF_8, FALSE);
		btit = conv_unmime_header(b->title, CS_UTF_8, FALSE);
		if( !strcmp(atit, btit) )
			title_eq = TRUE;
		g_free(atit);
		g_free(btit);
	} else
		no_title = TRUE;

	/* Published date */
	if (b->date_published <= 0) {
		no_pubdate = TRUE;
	} else {
		if (a->date_published == b->date_published)
			pubdate_eq = TRUE;
	}

	/* Modified date */
	if (b->date_modified <= 0) {
		no_moddate = TRUE;
	} else {
		if (a->date_modified == b->date_modified)
			moddate_eq = TRUE;
	}

	if (no_pubdate && no_moddate)
		no_date = TRUE;

	if (pubdate_eq || (no_pubdate && moddate_eq))
		date_eq = TRUE;

	/* If timestamp and url match, it is reasonable to assume
	 * we found our item. */
	if (url_eq && date_eq)
		return 0;

	/* Likewise if timestamp and title match. */
	if (title_eq && date_eq)
		return 0;

	/* Or if the url and title match. */
	if (url_eq && title_eq)
		return 0;

	/* There is no timestamp and the url matches (or there is none),
	 * we need to compare titles, ... */
	if( (no_url || url_eq) && no_date ) {
		if( title_eq )
			return 0;
		else
			return 1;
	}

	/* ... and as a last resort, if there is no title, item texts. */
	if( no_title && a->text && b->text ) {
		if( !strcmp(a->text, b->text) )
			return 0;
		else
			return 1;
	}

	/* We don't know this item. */
	return 1;
}

enum {
	ITEM_UNCHANGED,
	ITEM_CHANGED_TEXTONLY,
	ITEM_CHANGED
};

static gint rssyl_feed_item_changed(FeedItem *new_item, FeedItem *old_item )
{
	debug_print("RSSyl: comparing '%s' and '%s'\n",
			new_item->title, old_item->title);

	/* if both have title ... */
	if( old_item->title && new_item->title ) {
		gchar *old = conv_unmime_header(old_item->title, CS_UTF_8, FALSE);
		gchar *new = conv_unmime_header(new_item->title, CS_UTF_8, FALSE);
		if( strcmp(old, new) != 0 ) { /* ... compare "unmimed" titles */
			debug_print("RSSyl:\t\titem titles differ:\nOLD: '%s'\nNEW: '%s'\n",
					old, new);
			g_free(old);
			g_free(new);
			return ITEM_CHANGED;
		}
		g_free(old);
		g_free(new);
	} else {
		/* if atleast one has a title, they differ */
		if( old_item->title || new_item->title ) {
			debug_print("RSSyl:\t\t+/- title\n");
			return ITEM_CHANGED;
		}
	}

	if( old_item->author && new_item->author ) {
		gchar *old = conv_unmime_header(old_item->author, CS_UTF_8, TRUE);
		gchar *new = conv_unmime_header(new_item->author, CS_UTF_8, TRUE);
		if( strcmp(old, new) ) {  /* ... compare "unmimed" authors */
			g_free(old);
			g_free(new);
			debug_print("RSSyl:\t\titem authors differ\n");
			return ITEM_CHANGED;
		}
		g_free(old);
		g_free(new);
	} else {
		/* if atleast one has author, they differ */
		if( old_item->author || new_item->author ) {
			debug_print("RSSyl:\t\t+/- author\n");
			return ITEM_CHANGED;
		}
	}

	/* if both have text ... */
	if( old_item->text && new_item->text ) {
		if( strcmp(old_item->text, new_item->text) ) { /* ... compare them */
			debug_print("RSSyl:\t\titem texts differ\n");
			debug_print("\nOLD: '%s'\n", old_item->text);
			debug_print("\nNEW: '%s'\n", new_item->text);

			return ITEM_CHANGED_TEXTONLY;
		}
	} else {
		/* if at least one has some text, they differ */
		if( old_item->text || new_item->text ) {
			debug_print("RSSyl:\t\t+/- text\n");
			return ITEM_CHANGED_TEXTONLY;
		}
	}

	/* they don't seem to differ */
	return ITEM_UNCHANGED;
}

enum {
	EXISTS_NEW,
	EXISTS_UNCHANGED,
	EXISTS_CHANGED,
	EXISTS_CHANGED_TEXTONLY
};

/* rssyl_feed_item_exists()
 *
 * Returns 1 if a feed item already exists locally, 2 if there's a changed
 * item with link that already belongs to existing item, 3 if only item's
 * text has changed, 0 if item is new.
 */

static guint rssyl_feed_item_exists(RFolderItem *ritem, FeedItem *fitem,
		FeedItem **oldfitem)
{
	GSList *item = NULL;
	FeedItem *efitem = NULL;
	gint changed;

	g_return_val_if_fail(ritem != NULL, FALSE);
	g_return_val_if_fail(fitem != NULL, FALSE);

	if( ritem->items == NULL || g_slist_length(ritem->items) == 0 )
		return EXISTS_NEW;

	if( (item = g_slist_find_custom(ritem->items,
					(gconstpointer)fitem, (GCompareFunc)rssyl_cb_feed_compare)) ) {
		efitem = (FeedItem *)item->data;
		if( (changed = rssyl_feed_item_changed(fitem, efitem)) > ITEM_UNCHANGED ) {
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

/* =============================================================== */

void rssyl_add_item(RFolderItem *ritem, FeedItem *feed_item)
{
	FeedItem *old_item = NULL;
	MsgFlags *flags;
	MsgPermFlags oldperm_flags = 0;
	MsgInfo *msginfo;
	FILE *f;
	gint fd, d, dif;
	time_t tmpd;
	gchar *meta_charset = NULL;
	gchar *baseurl = NULL;
	gchar *template = NULL;
	gchar *tmp = NULL, *tmpurl = NULL, *tmpid = NULL;
	gchar *dirname = NULL;
	gchar *text = NULL;
	gchar *heading = NULL;
	gchar *pathbasename = NULL;
	gchar hdr[1024];
	FeedItemEnclosure *enc = NULL;
	RFeedCtx *ctx;

	g_return_if_fail(ritem != NULL);

	/* If item title is empty, try to fill it from source title (Atom only). */
	tmp = feed_item_get_sourcetitle(feed_item);
	if( feed_item_get_title(feed_item) == NULL ||
			strlen(feed_item->title) == 0 ) {
		if( tmp != NULL && strlen(tmp) > 0 )
			feed_item_set_title(feed_item, tmp);
		else
			feed_item_set_title(feed_item, C_("Empty RSS feed title placeholder", "(empty)"));
	}

/*
	if (feed_item_get_id(feed_item) == NULL) {
		debug_print("RSSyl: item ID empty, using its URL as ID.\n");
		feed_item_set_id(feed_item, feed_item_get_url(feed_item));
	}
*/

	/* If one of the timestamps is empty, set it to value of the other one. */
	if( feed_item_get_date_modified(feed_item) == -1 &&
			feed_item_get_date_published(feed_item) >= 0 ) {
		debug_print("RSSyl: setting missing moddate to pubdate %"CM_TIME_FORMAT"\n",
				feed_item_get_date_published(feed_item));
		feed_item_set_date_modified(feed_item,
				feed_item_get_date_published(feed_item));
	} else if( feed_item_get_date_published(feed_item) == -1 &&
			feed_item_get_date_modified(feed_item) >= 0 ) {
		debug_print("RSSyl: setting missing pubdate to modddate %"CM_TIME_FORMAT"\n",
				feed_item_get_date_modified(feed_item));
		feed_item_set_date_published(feed_item,
				feed_item_get_date_modified(feed_item));
	} else if( feed_item_get_date_modified(feed_item) == -1 &&
			feed_item_get_date_published(feed_item) == -1 &&
			feed_item_get_sourcedate(feed_item) >= 0 ) {
		/* If neither item date is set, use date from source (Atom only). */
		debug_print("RSSyl: setting missing pubdate and moddate to feed source date %"CM_TIME_FORMAT"\n",
				feed_item_get_sourcedate(feed_item));
		feed_item_set_date_modified(feed_item,
				feed_item_get_sourcedate(feed_item));
		feed_item_set_date_published(feed_item,
				feed_item_get_sourcedate(feed_item));
	}

	/* Fix up subject, url and ID (rssyl_format_string()) so that
	 * comparing doesn't break. */
	debug_print("RSSyl: fixing up subject '%s'\n", feed_item_get_title(feed_item));
	tmp = rssyl_format_string(feed_item_get_title(feed_item), TRUE, TRUE);
	feed_item_set_title(feed_item, tmp);
	g_free(tmp);
	debug_print("RSSyl: fixing up URL\n");
	tmp = rssyl_format_string(feed_item_get_url(feed_item), FALSE, TRUE);
	feed_item_set_url(feed_item, tmp);
	g_free(tmp);
	if( feed_item_get_id(feed_item) != NULL ) {
		debug_print("RSSyl: fixing up ID\n");
		tmp = rssyl_format_string(feed_item_get_id(feed_item), FALSE, TRUE);
		feed_item_set_id(feed_item, tmp);
		g_free(tmp);
	}

	/* If there's a summary, but no text, use summary as text. */
	if( feed_item_get_text(feed_item) == NULL &&
			(tmp = feed_item_get_summary(feed_item)) != NULL ) {
		feed_item_set_text(feed_item, tmp);
		g_free(feed_item->summary);	/* We do not need summary in rssyl now. */
		feed_item->summary = NULL;
	}

	/* If there is still no text, use an empty string for consistency. */
	if( feed_item_get_text(feed_item) == NULL )
		feed_item_set_text(feed_item, "");

	/* Do not add if the item already exists, update if it does exist, but
	 * has changed. */
	dif = rssyl_feed_item_exists(ritem, feed_item, &old_item);
	debug_print("RSSyl: rssyl_feed_item_exists returned %d\n", dif);

	if( dif == EXISTS_UNCHANGED ) {
		debug_print("RSSyl: This item already exists, skipping...\n");
		return;
	}

	/* Item is already in the list, but has changed */
	if( dif >= EXISTS_CHANGED && old_item != NULL ) {
		debug_print("RSSyl: Item changed, removing old one and adding new.\n");

		/* Store permflags of the old item. */
		ctx = (RFeedCtx *)old_item->data;
		pathbasename = g_path_get_basename(ctx->path);
		msginfo = folder_item_get_msginfo((FolderItem *)ritem,
						atoi(pathbasename));
		g_free(pathbasename);
		oldperm_flags = msginfo->flags.perm_flags;
		procmsg_msginfo_free(&msginfo);

		ritem->items = g_slist_remove(ritem->items, old_item);
		if (g_unlink(ctx->path) != 0) {
			debug_print("RSSyl: Error, could not delete file '%s': %s\n",
					ctx->path, g_strerror(errno));
		}

		g_free(ctx->path);
		feed_item_free(old_item);
		old_item = NULL;
	}

	/* Check against list of deleted items. */
	if (rssyl_deleted_check(ritem, feed_item)) {
		debug_print("RSSyl: Item '%s' found among deleted items, NOT adding it.\n",
				feed_item_get_title(feed_item));
		return;
	}

	/* Add a new item, formatting its title along the way */
	debug_print("RSSyl: Adding item '%s'\n", feed_item_get_title(feed_item));
	ritem->items = g_slist_prepend(ritem->items, feed_item_copy(feed_item));

	dirname = folder_item_get_path(&ritem->item);
	template = g_strconcat(dirname, G_DIR_SEPARATOR_S,
			RSSYL_TMP_TEMPLATE, NULL);
	if ((fd = g_mkstemp(template)) < 0) {
		g_warning("couldn't g_mkstemp('%s'), not adding message!", template);
		g_free(dirname);
		g_free(template);
		return;
	}

	f = claws_fdopen(fd, "w");
	if (f == NULL) {
		g_warning("couldn't open file '%s', not adding message!", template);
		g_free(dirname);
		g_free(template);
		return;
	}

	/* From */
	if( (tmp = feed_item_get_author(feed_item)) != NULL ) {
		if( g_utf8_validate(tmp, -1, NULL)) {
			conv_encode_header_full(hdr, 1023, tmp, strlen("From: "),
					TRUE, CS_UTF_8);
			fprintf(f, "From: %s\n", hdr);
		} else
			fprintf(f, "From: %s\n", tmp);
	}

	/* Date */
	if( (tmpd = feed_item_get_date_modified(feed_item)) != -1 ) {
		tmp = createRFC822Date(&tmpd);
		debug_print("RSSyl: using date_modified: '%s'\n", tmp);
	} else if( (tmpd = feed_item_get_date_published(feed_item)) != -1 ) {
		tmp = createRFC822Date(&tmpd);
		debug_print("RSSyl: using date_published: '%s'\n", tmp);
	} else {
		tmpd = time(NULL);
		tmp = createRFC822Date(&tmpd);
	}

	if( tmp != NULL ) {
		fprintf(f, "Date: %s\n", tmp);
		g_free(tmp);
	}

	if( (tmp = feed_item_get_title(feed_item)) != NULL ) {

		/* (Atom only) Strip HTML markup from title for the Subject line. */
		if( feed_item_get_title_format(feed_item) == FEED_ITEM_TITLE_HTML
				|| feed_item_get_title_format(feed_item) == FEED_ITEM_TITLE_XHTML) {
			debug_print("RSSyl: item title is HTML/XHTML, stripping tags for Subject line\n");
			tmp = g_strdup(tmp);
			strip_html(tmp);
		}

		if( g_utf8_validate(tmp, -1, NULL) ) {
			conv_encode_header_full(hdr, 1023, tmp, strlen("Subject: "),
					FALSE, CS_UTF_8);
			debug_print("RSSyl: Subject: %s\n", hdr);
			fprintf(f, "Subject: %s\n", hdr);
		} else
			fprintf(f, "Subject: %s\n", tmp);

		if( feed_item_get_title_format(feed_item) == FEED_ITEM_TITLE_HTML
				|| feed_item_get_title_format(feed_item) == FEED_ITEM_TITLE_XHTML) {
			g_free(tmp);
			fprintf(f, "X-RSSyl-OrigTitle: %s\n", feed_item_get_title(feed_item));
		}
	} else {
		debug_print("RSSyl: No feed title, it seems\n");
		fprintf(f, "Subject: (empty)\n");
	}

	/* X-RSSyl-URL */
	if( (tmpurl = feed_item_get_url(feed_item)) == NULL ) {
		if( feed_item_get_id(feed_item) != NULL &&
				feed_item_id_is_permalink(feed_item) ) {
			tmpurl = feed_item_get_id(feed_item);
		}
	}

	if( tmpurl != NULL )
		fprintf(f, "X-RSSyl-URL: %s\n", tmpurl);

	if( ritem->last_update > 0) {
		fprintf(f, "X-RSSyl-Last-Seen: %" CM_TIME_FORMAT "\n", ritem->last_update);
	}

	/* Message-ID */
	if( (tmpid = feed_item_get_id(feed_item)) == NULL )
		tmpid = feed_item_get_url(feed_item);
	if( tmpid != NULL )
		fprintf(f, "Message-ID: <%s>\n", tmpid);

	/* X-RSSyl-Comments */
	if( (text = feed_item_get_comments_url(feed_item)) != NULL )
		fprintf(f, "X-RSSyl-Comments: %s\n", text);

	/* References */
	if( (text = feed_item_get_parent_id(feed_item)) != NULL )
		fprintf(f, "References: <%s>\n", text);

	/* Content-Type */
	text = feed_item_get_text(feed_item);
	if( text && g_utf8_validate(text, -1, NULL) ) {
		fprintf(f, "Content-Type: text/html; charset=UTF-8\n\n");
		meta_charset = g_strdup("<meta http-equiv=\"Content-Type\" "
				"content=\"text/html; charset=UTF-8\">");
	} else {
		fprintf(f, "Content-Type: text/html\n\n");
	}

	/* construct base href */
	if( feed_item_get_url(feed_item) != NULL )
		baseurl = g_strdup_printf("<base href=\"%s\">\n",
			feed_item_get_url(feed_item) );

	if( ritem->write_heading )
		heading = g_strdup_printf("<h2>%s</h2>\n<br><br>\n",
				feed_item_get_title(feed_item));

	/* Message body */
	fprintf(f, "<html><head>"
			"%s\n"
			"%s"
			"</head>\n<body>\n"
			"%s\n"
			"URL: <a href=\"%s\">%s</a>\n\n<br><br>\n"
			RSSYL_TEXT_START"\n"
			"%s%s"
			RSSYL_TEXT_END"\n\n",
			(meta_charset ? meta_charset : ""),
			(baseurl ? baseurl : ""),
			(heading ? heading : ""),
			(tmpurl ? tmpurl : ""),
			(tmpurl ? tmpurl : "n/a"),
			(text ? text : ""),
			(text && strlen(text) > 0 ? "\n" : "") );

	g_free(meta_charset);
	g_free(baseurl);
	g_free(heading);

	if( (enc = feed_item_get_enclosure(feed_item)) != NULL )
		fprintf(f, "<p><a href=\"%s\">Attached media file</a> [%s] (%ld bytes)</p>\n",
				feed_item_enclosure_get_url(enc),
				feed_item_enclosure_get_type(enc),
				feed_item_enclosure_get_size(enc) );

	fprintf(f, "</body></html>\n");
	claws_safe_fclose(f);

	g_return_if_fail(template != NULL);

	flags = g_new(MsgFlags, 1);
	flags->perm_flags = MSG_NEW | MSG_UNREAD;
	flags->tmp_flags = 0;

	d = rssyl_add_msg(ritem->item.folder, &ritem->item, template, flags);

	g_free(flags);

	if (claws_unlink(template) < 0)
		FILE_OP_ERROR(template, "unlink");

	g_free(template);

	ctx = g_new0(RFeedCtx, 1);
	ctx->path = (gpointer)g_strdup_printf("%s%c%d", dirname,
			G_DIR_SEPARATOR, d);
	ctx->last_seen = ritem->last_update;
	((FeedItem *)ritem->items->data)->data = (gpointer)ctx;

	g_free(dirname);

	/* Unset unread+new if the changed item wasn't set unread and user
	 * doesn't want to see it unread because of the change. */
	if (dif != EXISTS_NEW) {
		if (!(oldperm_flags & MSG_UNREAD) && (ritem->silent_update == 2
				|| (ritem->silent_update == 1 && dif == EXISTS_CHANGED_TEXTONLY))) {
			msginfo = folder_item_get_msginfo((FolderItem *)ritem, d);
			procmsg_msginfo_unset_flags(msginfo, MSG_NEW | MSG_UNREAD, 0);
			procmsg_msginfo_free(&msginfo);
		}
	}

	debug_print("RSSyl: rssyl_add_msg(): %d\n", d);
}
