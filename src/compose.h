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

#ifndef __COMPOSE_H__
#define __COMPOSE_H__

#include <glib.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkitemfactory.h>

typedef struct _Compose		Compose;
typedef struct _AttachInfo	AttachInfo;

#include "procmsg.h"
#include "procmime.h"
#include "addressbook.h"
#include "prefs_account.h"
#include "undo.h"

#ifdef USE_PSPELL
#include "gtkspell.h"
#endif

typedef enum
{
	COMPOSE_TO,
	COMPOSE_CC,
	COMPOSE_BCC,
	COMPOSE_REPLYTO,
	COMPOSE_NEWSGROUPS,
	COMPOSE_FOLLOWUPTO
} ComposeEntryType;

typedef enum
{
	COMPOSE_REPLY,
	COMPOSE_REPLY_WITH_QUOTE,
	COMPOSE_REPLY_WITHOUT_QUOTE,
	COMPOSE_REPLY_TO_SENDER,
	COMPOSE_FOLLOWUP_AND_REPLY_TO,
	COMPOSE_REPLY_TO_SENDER_WITH_QUOTE,
	COMPOSE_REPLY_TO_SENDER_WITHOUT_QUOTE,
	COMPOSE_REPLY_TO_ALL,
	COMPOSE_REPLY_TO_ALL_WITH_QUOTE,
	COMPOSE_REPLY_TO_ALL_WITHOUT_QUOTE,
	COMPOSE_FORWARD,
	COMPOSE_FORWARD_AS_ATTACH,
	COMPOSE_FORWARD_INLINE,
	COMPOSE_NEW,
	COMPOSE_REDIRECT,
	COMPOSE_REEDIT
} ComposeMode;

typedef struct {
	guint headernum;
	Compose *compose;
	GtkWidget *combo;
	GtkWidget *entry;
} ComposeHeaderEntry;

struct _Compose
{
	GtkWidget *window;
	GtkWidget *vbox;
	GtkWidget *menubar;

	GtkWidget *handlebox;
	GtkWidget *toolbar;
	GtkWidget *send_btn;
	GtkWidget *sendl_btn;
	GtkWidget *draft_btn;
	GtkWidget *insert_btn;
	GtkWidget *attach_btn;
	GtkWidget *sig_btn;
	GtkWidget *exteditor_btn;
	GtkWidget *linewrap_btn;
	GtkWidget *addrbook_btn;

	GtkWidget *vbox2;

	/* Header */
	GtkWidget *table_vbox;
	GtkWidget *table;
/*
	GtkWidget *to_hbox;
	GtkWidget *to_entry;
	GtkWidget *newsgroups_hbox;
	GtkWidget *newsgroups_entry;
*/
	GtkWidget *subject_entry;
/*
	GtkWidget *cc_hbox;
	GtkWidget *cc_entry;
	GtkWidget *bcc_hbox;
	GtkWidget *bcc_entry;
	GtkWidget *reply_hbox;
	GtkWidget *reply_entry;
	GtkWidget *followup_hbox;
	GtkWidget *followup_entry;
*/
	GtkWidget *paned;

	/* Attachments */
	GtkWidget *attach_scrwin;
	GtkWidget *attach_clist;

	/* Others */
	GtkWidget *savemsg_checkbtn;
	GtkWidget *savemsg_entry;

	/* Textedit */
	GtkWidget *edit_vbox;
	GtkWidget *ruler_hbox;
	GtkWidget *ruler;
	GtkWidget *scrolledwin;
	GtkWidget *text;

	GtkWidget *focused_editable;

	GtkWidget *popupmenu;

	GtkItemFactory *popupfactory;

	GtkWidget *tmpl_menu;

	ComposeMode mode;

	MsgInfo *targetinfo;
	MsgInfo *replyinfo;

	GtkWidget *header_table;
	GSList    *header_list;
	guint	   header_nextrow;
	ComposeHeaderEntry *header_last;

	gchar	*replyto;
	gchar	*cc;
	gchar	*bcc;
	gchar	*newsgroups;
	gchar	*followup_to;
	gchar	*mailinglist;

	gchar	*inreplyto;
	gchar	*references;
	gchar	*msgid;
	gchar	*boundary;

	gboolean use_to;
	gboolean use_cc;
	gboolean use_bcc;
	gboolean use_replyto;
	gboolean use_followupto;
	gboolean use_mailinglist;
	gboolean use_attach;

	/* privacy settings */
	gboolean use_signing;
	gboolean use_encryption;

	gboolean modified;

	gboolean return_receipt;
	gboolean paste_as_quotation;

	GSList *to_list;
	GSList *newsgroup_list;

	PrefsAccount *account;
	PrefsAccount *orig_account;

	UndoMain *undostruct;

	/* external editor */
	gchar *exteditor_file;
	pid_t  exteditor_pid;
	gint   exteditor_readdes;
	gint   exteditor_tag;

#if USE_PSPELL
        /* Pspell spell checker */
        GtkPspell *gtkpspell;
#endif

 	/* Priority */
 	gint priority;

	gchar *redirect_filename;
};

struct _AttachInfo
{
	gchar *file;
	gchar *content_type;
	EncodingType encoding;
	gchar *name;
	off_t size;
};

Compose *compose_new			(PrefsAccount	*account);

Compose *compose_new_with_recipient	(PrefsAccount	*account,
					 const gchar	*to);

Compose *compose_new_with_folderitem	(PrefsAccount	*account,
					 FolderItem	*item);

void compose_followup_and_reply_to	(MsgInfo	*msginfo,
					 gboolean	 quote,
					 gboolean	 to_all,
					 gboolean	 ignore_replyto,
					 const gchar	*body);
void compose_reply			(MsgInfo	*msginfo,
					 gboolean	 quote,
					 gboolean	 to_all,
					 gboolean	 ignore_replyto,
					 const gchar	*body);
Compose *compose_forward		(PrefsAccount *account,
					 MsgInfo	*msginfo,
					 gboolean	 as_attach,
					 const gchar	*body);
Compose *compose_forward_multiple	(PrefsAccount	*account, 
					 GSList		*msginfo_list);
Compose *compose_redirect		(PrefsAccount	*account,
					 MsgInfo	*msginfo);
void compose_reedit			(MsgInfo	*msginfo);

GList *compose_get_compose_list		(void);

void compose_entry_append		(Compose	  *compose,
					 const gchar	  *address,
					 ComposeEntryType  type);

gint compose_send			(Compose	  *compose);

void compose_reflect_prefs_all			(void);
void compose_reflect_prefs_pixmap_theme	(void);

#endif /* __COMPOSE_H__ */
