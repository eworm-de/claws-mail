/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2003 Hiroyuki Yamamoto
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

#define INBOX_DIR		"inbox"
#define OUTBOX_DIR		"sent"
#define QUEUE_DIR		"queue"
#define DRAFT_DIR		"draft"
#define TRASH_DIR		"trash"
#define RC_DIR			".sylpheed" /**/
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
#define ACTIONS_RC		"actionsrc"
#define RENDERER_RC		"rendererrc"
#define COMMAND_HISTORY		"command_history"
#define TEMPLATE_DIR		"templates"
#define TMP_DIR			"tmp"
#define NEWSGROUP_LIST		".newsgroup_list"
#define ADDRESS_BOOK		"addressbook.xml"
#define MANUAL_HTML_INDEX	"sylpheed.html"
#define FAQ_HTML_INDEX		"sylpheed-faq.html"
#define HOMEPAGE_URI		"http://claws.sylpheed.org/"
#define FOLDER_LIST		"folderlist.xml"
#define CACHE_FILE		".sylpheed_cache"
#define MARK_FILE		".sylpheed_mark"
#define CACHE_VERSION		20
#define MARK_VERSION		2

#define DEFAULT_SIGNATURE	".signature"
#define DEFAULT_INC_PATH	"/usr/bin/mh/inc"
#define DEFAULT_INC_PROGRAM	"inc"
/* #define DEFAULT_INC_PATH	"/usr/bin/imget" */
/* #define DEFAULT_INC_PROGRAM	"imget" */
#define DEFAULT_SENDMAIL_CMD	"/usr/sbin/sendmail -t -i"
#ifdef WIN32
#define DEFAULT_BROWSER_CMD	"?p\\mozilla.org\\Mozilla\\mozilla.exe -remote openURL \"%s\""
#else
#define DEFAULT_BROWSER_CMD	"mozilla -remote 'openURL(%s, new-window)'"
#endif

#ifdef _PATH_MAILDIR
#  define DEFAULT_SPOOL_PATH	_PATH_MAILDIR
#else
#  define DEFAULT_SPOOL_PATH	"/var/spool/mail"
#endif

#ifdef WIN32
#define BUFFSIZE			8191
#else
#define BUFFSIZE			8192
#endif

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
#define SESSION_TIMEOUT			60	/* sec */
#define MAX_HISTORY_SIZE		16

#ifdef WIN32
  #define LOCK_PORT			54321	// (-_-;;
#endif
 
#ifdef jjjWIN32
#  define NORMAL_FONT		"-*-*-normal-r-normal--12-*-*-*-m-*-jisx0208.1983-0," \
				"-*-*-normal-r-normal--12-*-*-*-*-*-*-*"
#  define BOLD_FONT		"-*-*-bold-r-normal--12-*-*-*-m-*-jisx0208.1983-0," \
				"-*-*-bold-r-normal--12-*-*-*-*-*-*-*"
#  define SMALL_FONT		"-*-*-normal-r-normal--10-*-*-*-m-*-jisx0208.1983-0," \
				"-*-*-normal-r-normal--10-*-*-*-*-*-*-*"
 // Added
#  define DEFAULT_MESSAGE_FONT	"-*-*-normal-r-normal--12-*-*-*-m-*-jisx0208.1983-0," \
				"-*-*-normal-r-normal--12-*-*-*-*-*-*-*"
#  define DEFAULT_SPACING_FONT	"-*-*-normal-r-normal--6-*-*-*-m-*-jisx0208.1983-0," \
				"-*-*-normal-r-normal--6-*-*-*-*-*-*-*"
#endif
//-----------------------------------------------------------------------------
#ifdef WIN32
#  define NORMAL_FONT		"-*-Arial-normal-r-normal--15-*-*-*-*-*-*-*"
#  define BOLD_FONT		"-*-Arial-bold-r-normal--15-*-*-*-*-*-*-*"
#  define SMALL_FONT		"-*-Arial-normal-r-normal--14-*-*-*-*-*-*-*"
//#  define NORMAL_FONT		"-*-Microsoft Sans Serif-normal-r-normal--14-*-*-*-*-*-iso8859-1"
//#  define BOLD_FONT		"-*-Microsoft Sans Serif-bold-r-normal--14-*-*-*-*-*-iso8859-1"
//#  define SMALL_FONT		"-*-Microsoft Sans Serif-normal-r-normal--14-*-*-*-*-*-iso8859-1"
 // Added
#  define DEFAULT_MESSAGE_FONT	"-*-*-normal-r-normal--18-*-*-*-m-*-*-*"
#  define DEFAULT_SPACING_FONT	"-*-*-normal-r-normal--12-*-*-*-m-*-*-*"
#else
#  define NORMAL_FONT	prefs_common.normalfont
#  define BOLD_FONT	prefs_common.boldfont
#  define SMALL_FONT	prefs_common.smallfont
#endif

#define DEFAULT_PIXMAP_THEME	"INTERNAL_DEFAULT"
#define PIXMAP_THEME_DIR	"themes"

#ifdef WIN32
//XXX:tm   w32lib.h ?
//used in: prefs.c
#  define F_EXISTS	00 //Existence only 
#  define W_OK		02 //Write permission 
#  define R_OK		04 //Read permission 
#  define F_OK		06 //Read and write permission 
#endif

#ifdef WIN32
//XXX:tm  put to w32lib.h ?
//used in: mh.h summaryview.c
#  define S_IRGRP	_S_IREAD
#  define S_IWGRP	_S_IWRITE
#  define S_IXGRP	_S_IEXEC
#  define S_IRWXG	(_S_IREAD|_S_IWRITE|_S_IEXEC)
#  define S_IROTH	_S_IREAD
#  define S_IWOTH	_S_IWRITE
#  define S_IXOTH	_S_IEXEC
#  define S_IRWXO	(_S_IREAD|_S_IWRITE|_S_IEXEC)
#endif

#ifdef WIN32
#  define W32_MAILCAP_NAME "mailcap.win32"
#endif /* WIN32 */


#endif /* __DEFS_H__ */
