/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2002 Hiroyuki Yamamoto
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

#ifndef __INC_H__
#define __INC_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>

#include "mainwindow.h"
#include "progressdialog.h"
#include "prefs_account.h"
#include "pop.h"
#include "automaton.h"
#include "socket.h"

typedef struct _IncProgressDialog	IncProgressDialog;
typedef struct _IncSession		IncSession;
typedef struct _Pop3State		Pop3State;

typedef enum
{
	INC_SUCCESS,
	INC_CONNECT_ERROR,
	INC_AUTH_FAILED,
	INC_ERROR,
	INC_NOSPACE,
	INC_CANCEL
} IncState;

struct _IncProgressDialog
{
	ProgressDialog *dialog;

	MainWindow *mainwin;

	gboolean show_dialog;

	GList *queue_list;	/* list of IncSession */
};

struct _IncSession
{
	Pop3State *pop3_state;
	Automaton *atm;

	gpointer data;
};

struct _Pop3State
{
	gboolean cancelled;
	PrefsAccount *ac_prefs;
	GHashTable *folder_table;

	gchar *prev_folder;

	SockInfo *sockinfo;

	gchar *greeting;
	gchar *user;
	gchar *pass;
	gint count;
	gint new;
	gint total_bytes;
	gint cur_msg;
	gint cur_total_num;
	gint cur_total_bytes;

	Pop3MsgInfo *msg;

	GHashTable *uidl_table;
	GSList *uidl_todelete_list;
	
	gboolean uidl_is_valid;

	gint error_val;
	IncState inc_state;

	IncSession *session;
};

#define TIMEOUT_ITV	200

void inc_mail			(MainWindow	*mainwin,
				 gboolean notify);
void inc_all_account_mail	(MainWindow	*mainwin,
				 gboolean notify);
void inc_selective_download     (MainWindow	*mainwin, 
				 PrefsAccount *acc,
				 gint         session);
void inc_progress_update	(Pop3State	*state,
				 Pop3Phase	 phase);
gint inc_drop_message		(const gchar	*file,
				 Pop3State	*state);

gboolean inc_is_active		(void);

void inc_cancel_all		(void);

void inc_lock			(void);
void inc_unlock			(void);

void inc_autocheck_timer_init	(MainWindow	*mainwin);
void inc_autocheck_timer_set	(void);
void inc_autocheck_timer_remove	(void);

#endif /* __INC_H__ */
