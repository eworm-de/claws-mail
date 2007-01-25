/*
 * Claws Mail -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2005-2007 DINH Viet Hoa and the Claws Mail team
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

#ifndef ETPAN_LOG_H

#define ETPAN_LOG_H

#define ENABLE_LOG

#ifdef ENABLE_LOG

/*
  an environment variable ETPAN_LOG must be set
  and will contains the log identifier separated with spaces.
*/

#define ETPAN_GTK_TREE_MODEL_LOG(...) ETPAN_LOG_ID("GTKTREEMODEL", __VA_ARGS__)
#define ETPAN_TABBED_LOG(...) ETPAN_LOG_ID("TABBED", __VA_ARGS__)
#define ETPAN_MSGVIEW_LOG(...) ETPAN_LOG_ID("MSGVIEW", __VA_ARGS__)
#define ETPAN_MSGLIST_LOG(...) ETPAN_LOG_ID("MSGLIST", __VA_ARGS__)
#define ETPAN_FOLDERLIST_LOG(...) ETPAN_LOG_ID("FOLDERLIST", __VA_ARGS__)
#define ETPAN_MAINWINDOW_LOG(...) ETPAN_LOG_ID("MAINWINDOW", __VA_ARGS__)
#define ETPAN_SIGNAL_LOG(...) ETPAN_LOG_ID("SIGNAL", __VA_ARGS__)
#define ETPAN_MEM_LOG(...) ETPAN_LOG_ID("MEM", __VA_ARGS__)
#define ETPAN_UI_LOG(...) ETPAN_LOG_ID("UI", __VA_ARGS__)
#define ETPAN_PROGRESS_LOG(...) ETPAN_LOG_ID("PROGRESS", __VA_ARGS__)
#define ETPAN_DEBUG_LOG(...) ETPAN_LOG_ID("DEBUG", __VA_ARGS__)
#define ETPAN_STACK_LOG(...) ETPAN_LOG_ID("STACK", __VA_ARGS__)
#define ETPAN_LOG(...) ETPAN_LOG_ID("LOG", __VA_ARGS__)

#else

#define ETPAN_GTK_TREE_MODEL_LOG(...)
#define ETPAN_TABBED_LOG(...)
#define ETPAN_MSGVIEW_LOG(...)
#define ETPAN_MSGLIST_LOG(...)
#define ETPAN_FOLDERLIST_LOG(...)
#define ETPAN_MAINWINDOW_LOG(...)
#define ETPAN_SIGNAL_LOG(...)
#define ETPAN_MEM_LOG(...)
#define ETPAN_UI_LOG(...)
#define ETPAN_PROGRESS_LOG(...)
#define ETPAN_DEBUG_LOG(...)
#define ETPAN_STACK_LOG(...)
#define ETPAN_LOG(...) ETPAN_LOG_ID("LOG", __VA_ARGS__)

#endif

#define ETPAN_LOG_ID(...) etpan_log(__VA_ARGS__)

void etpan_log_init(void);
void etpan_log_done(void);
void etpan_log(char * log_id, char * format, ...);

void etpan_log_stack(void);

#endif
