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

#ifndef __TEXTVIEW_H__
#define __TEXTVIEW_H__

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include <glib.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtktextiter.h>

typedef struct _TextView	TextView;
typedef struct _RemoteURI	RemoteURI;

#include "messageview.h"
#include "procmime.h"

struct _TextView
{
	GtkWidget *vbox;
	GtkWidget *scrolledwin;
	GtkWidget *text;

	GtkWidget *link_popup_menu;
	GtkItemFactory *link_popup_factory;
	GtkWidget *mail_popup_menu;
	GtkItemFactory *mail_popup_factory;
	GtkWidget *file_popup_menu;
	GtkItemFactory *file_popup_factory;

	gboolean default_text;
	gboolean is_in_signature;
	
	GSList *uri_list;
	gint body_pos;

	gboolean show_all_headers;

	MessageView *messageview;
	gint last_buttonpress;

	RemoteURI *uri_hover;
	GtkTextIter uri_hover_start_iter;
	GtkTextIter uri_hover_end_iter;
	GtkWidget *image;
};

TextView *textview_create		(void);
void textview_init			(TextView	*textview);
void textview_reflect_prefs		(TextView	*textview);

void textview_show_message	(TextView	*textview,
				 MimeInfo	*mimeinfo,
				 const gchar	*file);
void textview_show_part		(TextView	*textview,
				 MimeInfo	*mimeinfo,
				 FILE		*fp);
void textview_show_error	(TextView	*textview);
void textview_show_mime_part	(TextView	*textview,
				 MimeInfo	*partinfo);
void textview_clear		(TextView	*textview);
void textview_destroy		(TextView	*textview);
void textview_set_all_headers	(TextView	*textview,
				 gboolean	 all_headers);
void textview_set_font		(TextView	*textview,
				 const gchar	*codeset);
void textview_set_text		(TextView	*textview,
				 const gchar	*text);
void textview_set_position	(TextView	*textview,
				 gint		 pos);
void textview_scroll_one_line	(TextView	*textview,
				 gboolean	 up);
gboolean textview_scroll_page	(TextView	*textview,
				 gboolean	 up);

gboolean textview_search_string			(TextView	*textview,
						 const gchar	*str,
						 gboolean	 case_sens);
gboolean textview_search_string_backward	(TextView	*textview,
						 const gchar	*str,
						 gboolean	 case_sens);

#endif /* __TEXTVIEW_H__ */
