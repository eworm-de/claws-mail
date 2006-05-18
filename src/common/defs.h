/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2006 Hiroyuki Yamamoto and the Sylpheed-Claws team
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __DEFS_H__
#define __DEFS_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glibconfig.h>

#ifdef G_OS_WIN32
#  include <glib/gwin32.h>
#endif

#if HAVE_PATHS_H
#  include <paths.h>
#endif

#if HAVE_SYS_PARAM_H
#  include <sys/param.h>
#endif

#define INBOX_DIR		"inbox"
#define OUTBOX_DIR		"sent"
#define QUEUE_DIR		"queue"
#define DRAFT_DIR		"draft"
#define TRASH_DIR		"trash"
#define RC_DIR			CFG_RC_DIR
#define OLD_GTK2_RC_DIR		".sylpheed-gtk2"
#define OLD_GTK1_RC_DIR		".sylpheed"
#define NEWS_CACHE_DIR		"newscache"
#define IMAP_CACHE_DIR		"imapcache"
#define MBOX_CACHE_DIR		"mboxcache"
#define HEADER_CACHE_DIR        "headercache" 
#define MIME_TMP_DIR		"mimetmp"
#define COMMON_RC		"sylpheedrc"
#define ACCOUNT_RC		"accountrc"
#define CUSTOM_HEADER_RC	"customheaderrc"
#define DISPLAY_HEADER_RC	"dispheaderrc"
#define FOLDERITEM_RC           "folderitemrc"
#define SCORING_RC              "scoringrc"
#define FILTERING_RC		"filteringrc"
#define MATCHER_RC		"matcherrc"
#define MENU_RC			"menurc"
#define RENDERER_RC		"rendererrc"
#define QUICKSEARCH_HISTORY	"quicksearch_history"
#define TEMPLATE_DIR		"templates"
#define TMP_DIR			"tmp"
#define UIDL_DIR		"uidl"
#define NEWSGROUP_LIST		".newsgroup_list"
#define ADDRESS_BOOK		"addressbook.xml"
#define MANUAL_HTML_INDEX	"sylpheed-claws-manual.html"
#define HOMEPAGE_URI		"http://www.sylpheed-claws.net/"
#define MANUAL_URI		"http://www.sylpheed-claws.net/documentation.php"
#define FAQ_URI			"http://www.sylpheed-claws.net/faq/index.php"
#define PLUGINS_URI		"http://www.sylpheed-claws.net/plugins.php"
#define BUGZILLA_URI		"http://www.thewildbeast.co.uk/sylpheed-claws/bugzilla/enter_bug.cgi"
#define THEMES_URI		"http://www.sylpheed-claws.net/themes.php"
#define MAILING_LIST_URI	"http://www.sylpheed-claws.net/MLs.php"
#define USERS_ML_ADDR		"sylpheed-claws-users@dotsrc.org"
#define GPL_URI			"http://www.gnu.org/licenses/gpl.html"
#define DONATE_URI		"https://sourceforge.net/donate/index.php?group_id=25528"
#define OPENSSL_URI		"http://www.openssl.org/"
#define THEMEINFO_FILE		".sylpheed_themeinfo"
#define FOLDER_LIST		"folderlist.xml"
#define CACHE_FILE		".sylpheed_claws_cache"
#define MARK_FILE		".sylpheed_mark"
#define CACHE_VERSION		24
#define MARK_VERSION		2

#ifdef G_OS_WIN32
#  define ACTIONS_RC		"actionswinrc"
#  define COMMAND_HISTORY	"command_history_win"
#  define DEFAULT_SIGNATURE	"signature.txt"
#else
#  define ACTIONS_RC		"actionsrc"
#  define COMMAND_HISTORY	"command_history"
#  define DEFAULT_SIGNATURE	".signature"
#endif

#define DEFAULT_INC_PATH	"/usr/bin/mh/inc"
#define DEFAULT_INC_PROGRAM	"inc"
/* #define DEFAULT_INC_PATH	"/usr/bin/imget" */
/* #define DEFAULT_INC_PROGRAM	"imget" */
#define DEFAULT_SENDMAIL_CMD	"/usr/sbin/sendmail -t -i"
#define DEFAULT_BROWSER_CMD	"mozilla-firefox -remote 'openURL(%s,new-window)'"
#define DEFAULT_EDITOR_CMD	"gedit %s"
#define DEFAULT_MIME_CMD	"metamail -d -b -x -c %s '%s'"
#define DEFAULT_IMAGE_VIEWER_CMD "display '%s'"
#define DEFAULT_AUDIO_PLAYER_CMD "play '%s'"

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
#define MAX_ENTRY_LENGTH		8191
#define COLOR_DIM			35000
#define UI_REFRESH_INTERVAL		50000	/* usec */
#define FOLDER_UPDATE_INTERVAL		1500	/* msec */
#define PROGRESS_UPDATE_INTERVAL	200	/* msec */
#define SESSION_TIMEOUT_INTERVAL	60	/* sec */
#define MAX_HISTORY_SIZE		16

#define NORMAL_FONT prefs_common.normalfont
#define SMALL_FONT	prefs_common.smallfont

#define DEFAULT_PIXMAP_THEME	"INTERNAL_DEFAULT"
#define PIXMAP_THEME_DIR		"themes"

#endif /* __DEFS_H__ */
