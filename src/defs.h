/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999,2000 Hiroyuki Yamamoto
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

#ifndef __DEFS_H__
#define __DEFS_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#if HAVE_PATHS_H
#  include <paths.h>
#endif

#if HAVE_SYS_PARAM_H
#  include <sys/param.h>
#endif

#define PROG_VERSION		"Sylpheed version "VERSION
#define INBOX_DIR		"inbox"
#define OUTBOX_DIR		"outbox"
#define QUEUE_DIR		"queue"
#define DRAFT_DIR		"draft"
#define TRASH_DIR		"trash"
#define RC_DIR			".sylpheed"
#define NEWS_CACHE_DIR		"newscache"
#define GROUPLIST_FILE		"grouplist"
#define IMAP_CACHE_DIR		"imapcache"
#define MIME_TMP_DIR		"mimetmp"
#define COMMON_RC		"sylpheedrc"
#define ACCOUNT_RC		"accountrc"
#define FILTER_RC		"filterrc"
#define HEADERS_RC		"headersrc"
#define HEADERS_DISPLAY_RC	"headersdisplayrc"
#define MENU_RC			"menurc"
#define ADDRESS_BOOK		"addressbook.xml"
#define MANUAL_HTML_INDEX	"sylpheed.html"
#define HOMEPAGE_URI		"http://sylpheed.good-day.net/"
#define FOLDER_LIST		"folderlist.xml"
#define CACHE_FILE		".sylpheed_cache"
#define MARK_FILE		".sylpheed_mark"
#define CACHE_VERSION		17
#define MARK_VERSION		2

#define DEFAULT_SIGNATURE	".signature"
#define DEFAULT_INC_PATH	"/usr/bin/mh/inc"
#define DEFAULT_INC_PROGRAM	"inc"
/* #define DEFAULT_INC_PATH	"/usr/bin/imget" */
/* #define DEFAULT_INC_PROGRAM	"imget" */

#ifdef _PATH_MAILDIR
#  define DEFAULT_SPOOL_PATH	_PATH_MAILDIR
#else
#  define DEFAULT_SPOOL_PATH	"/var/spool/mail"
#endif

#define BUFFSIZE			8192

#ifndef MAXPATHLEN
#  define MAXPATHLEN			4095
#endif

#define DEFAULT_HEIGHT			460
#define DEFAULT_FOLDERVIEW_WIDTH	179
#define DEFAULT_MAINVIEW_WIDTH		600
#define DEFAULT_SUMMARY_HEIGHT		140
#define DEFAULT_HEADERVIEW_HEIGHT	40
#define DEFAULT_COMPOSE_HEIGHT		560
#define BORDER_WIDTH			2
#define CTREE_INDENT			18
#define FOLDER_SPACING			4
#define COLOR_DIM			35000

#define NORMAL_FONT	"-*-helvetica-medium-r-normal--12-*," \
			"-*-*-medium-r-normal--12-*-*-*-*-*-*-*,*"
#define BOLD_FONT	"-*-helvetica-bold-r-normal--12-*," \
			"-*-*-bold-r-normal--12-*-*-*-*-*-*-*,*"
#define SMALL_FONT	"-*-helvetica-medium-r-normal--10-*," \
			"-*-*-medium-r-normal--10-*-*-*-*-*-*-*,*"

#endif /* __DEFS_H__ */
