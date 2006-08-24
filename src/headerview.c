/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2006 Hiroyuki Yamamoto
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkstyle.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkimage.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#if HAVE_LIBCOMPFACE
#  include <compface.h>
#endif

#include <gdk-pixbuf/gdk-pixbuf.h>

#include "headerview.h"
#include "prefs_common.h"
#include "codeconv.h"
#include "gtkutils.h"
#include "utils.h"
#include "base64.h"

#define TR(str)	(prefs_common.trans_hdr ? gettext(str) : str)

#if HAVE_LIBCOMPFACE
#define XPM_XFACE_HEIGHT	(HEIGHT + 3)  /* 3 = 1 header + 2 colors */

static gchar *xpm_xface[XPM_XFACE_HEIGHT];

static void headerview_show_xface	(HeaderView	*headerview,
					 MsgInfo	*msginfo);
#endif

static gint headerview_show_face	(HeaderView	*headerview,
					 MsgInfo	*msginfo);

HeaderView *headerview_create(void)
{
	HeaderView *headerview;
	GtkWidget *hbox;
	GtkWidget *vbox;
	GtkWidget *hbox1;
	GtkWidget *hbox2;
	GtkWidget *from_header_label;
	GtkWidget *from_body_label;
	GtkWidget *to_header_label;
	GtkWidget *to_body_label;
	GtkWidget *ng_header_label;
	GtkWidget *ng_body_label;
	GtkWidget *subject_header_label;
	GtkWidget *subject_body_label;

	debug_print("Creating header view...\n");
	headerview = g_new0(HeaderView, 1);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);
	vbox = gtk_vbox_new(FALSE, 2);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);

	hbox1 = gtk_hbox_new(FALSE, 4);
	gtk_box_pack_start(GTK_BOX(vbox), hbox1, FALSE, FALSE, 0);
	hbox2 = gtk_hbox_new(FALSE, 4);
	gtk_box_pack_start(GTK_BOX(vbox), hbox2, FALSE, FALSE, 0);

	from_header_label    = gtk_label_new(TR("From:"));
	from_body_label      = gtk_label_new("");
	to_header_label      = gtk_label_new(TR("To:"));
	to_body_label        = gtk_label_new("");
	ng_header_label      = gtk_label_new(TR("Newsgroups:"));
	ng_body_label        = gtk_label_new("");
	subject_header_label = gtk_label_new(TR("Subject:"));
	subject_body_label   = gtk_label_new("");

	gtk_label_set_selectable(GTK_LABEL(from_body_label), TRUE);
	gtk_label_set_selectable(GTK_LABEL(to_body_label), TRUE);
	gtk_label_set_selectable(GTK_LABEL(ng_body_label), TRUE);
	gtk_label_set_selectable(GTK_LABEL(subject_body_label), TRUE);

	GTK_WIDGET_UNSET_FLAGS(from_body_label, GTK_CAN_FOCUS);
	GTK_WIDGET_UNSET_FLAGS(to_body_label, GTK_CAN_FOCUS);
	GTK_WIDGET_UNSET_FLAGS(ng_body_label, GTK_CAN_FOCUS);
	GTK_WIDGET_UNSET_FLAGS(subject_body_label, GTK_CAN_FOCUS);

	gtk_box_pack_start(GTK_BOX(hbox1), from_header_label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox1), from_body_label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox1), to_header_label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox1), to_body_label, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox1), ng_header_label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox1), ng_body_label, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox2), subject_header_label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox2), subject_body_label, TRUE, TRUE, 0);

	gtk_misc_set_alignment(GTK_MISC(to_body_label), 0, 0.5);
	gtk_misc_set_alignment(GTK_MISC(ng_body_label), 0, 0.5);
	gtk_misc_set_alignment(GTK_MISC(subject_body_label), 0, 0.5);
	gtk_label_set_ellipsize(GTK_LABEL(to_body_label), PANGO_ELLIPSIZE_END);
	gtk_label_set_ellipsize(GTK_LABEL(ng_body_label), PANGO_ELLIPSIZE_END);
	gtk_label_set_ellipsize(GTK_LABEL(subject_body_label), PANGO_ELLIPSIZE_END);

	headerview->hbox = hbox;
	headerview->from_header_label    = from_header_label;
	headerview->from_body_label      = from_body_label;
	headerview->to_header_label      = to_header_label;
	headerview->to_body_label        = to_body_label;
	headerview->ng_header_label      = ng_header_label;
	headerview->ng_body_label        = ng_body_label;
	headerview->subject_header_label = subject_header_label;
	headerview->subject_body_label   = subject_body_label;
	headerview->image = NULL;

	gtk_widget_show_all(hbox);

	return headerview;
}

void headerview_set_font(HeaderView *headerview)
{
	PangoFontDescription *boldfont = NULL;
	PangoFontDescription *normalfont = NULL;
	
	if (!boldfont) {
		normalfont = pango_font_description_from_string(NORMAL_FONT);
		boldfont = pango_font_description_from_string(NORMAL_FONT);
		pango_font_description_set_weight(boldfont, PANGO_WEIGHT_BOLD);
	}

	if (boldfont) {
		gtk_widget_modify_font(headerview->from_header_label, boldfont);
		gtk_widget_modify_font(headerview->to_header_label, boldfont);
		gtk_widget_modify_font(headerview->ng_header_label, boldfont);
		gtk_widget_modify_font(headerview->subject_header_label, boldfont);
		pango_font_description_free(boldfont);

		gtk_widget_modify_font(headerview->from_body_label, normalfont);
		gtk_widget_modify_font(headerview->to_body_label, normalfont);
		gtk_widget_modify_font(headerview->ng_body_label, normalfont);
		gtk_widget_modify_font(headerview->subject_body_label, normalfont);
		pango_font_description_free(normalfont);
	}
}

void headerview_init(HeaderView *headerview)
{
	headerview_set_font(headerview);
	headerview_clear(headerview);
	headerview_set_visibility(headerview, prefs_common.display_header_pane);

#if HAVE_LIBCOMPFACE
	{
		gint i;

		for (i = 0; i < XPM_XFACE_HEIGHT; i++) {
			xpm_xface[i] = g_malloc(WIDTH + 1);
			*xpm_xface[i] = '\0';
		}
	}
#endif
}

void headerview_show(HeaderView *headerview, MsgInfo *msginfo)
{
	headerview_clear(headerview);

	gtk_label_set_text(GTK_LABEL(headerview->from_body_label),
			   msginfo->from ? msginfo->from : _("(No From)"));
	if (msginfo->to) {
		gtk_label_set_text(GTK_LABEL(headerview->to_body_label),
				   msginfo->to);
		gtk_widget_show(headerview->to_header_label);
		gtk_widget_show(headerview->to_body_label);
	}
	if (msginfo->newsgroups) {
		gtk_label_set_text(GTK_LABEL(headerview->ng_body_label),
				   msginfo->newsgroups);
		gtk_widget_show(headerview->ng_header_label);
		gtk_widget_show(headerview->ng_body_label);
	}
	gtk_label_set_text(GTK_LABEL(headerview->subject_body_label),
			   msginfo->subject ? msginfo->subject :
			   _("(No Subject)"));

	if (!headerview_show_face(headerview, msginfo))
		return;

#if HAVE_LIBCOMPFACE
	headerview_show_xface(headerview, msginfo);
#endif
}

#if HAVE_LIBCOMPFACE
static void headerview_show_xface(HeaderView *headerview, MsgInfo *msginfo)
{
	GtkWidget *hbox = headerview->hbox;
	GtkWidget *image;

	if (!msginfo->extradata || 
	    !msginfo->extradata->xface || 
	    strlen(msginfo->extradata->xface) < 5) {
		if (headerview->image &&
		    GTK_WIDGET_VISIBLE(headerview->image)) {
			gtk_widget_hide(headerview->image);
			gtk_widget_queue_resize(hbox);
		}
		return;
	}
	if (!GTK_WIDGET_VISIBLE(headerview->hbox)) return;

	if (headerview->image) {
		gtk_widget_destroy(headerview->image);
		headerview->image = NULL;
	}
	

	image = xface_get_from_header(msginfo->extradata->xface, &hbox->style->white,
				hbox->window);

	if (image) {
		gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);
		gtk_widget_show(image);
	}

	headerview->image = image;
}
#endif

static gint headerview_show_face (HeaderView *headerview, MsgInfo *msginfo)
{
	GtkWidget *hbox = headerview->hbox;
	GtkWidget *image;

	if (!msginfo->extradata || !msginfo->extradata->face) {
		if (headerview->image &&
		    GTK_WIDGET_VISIBLE(headerview->image)) {
			gtk_widget_hide(headerview->image);
			gtk_widget_queue_resize(hbox);
		}
		return -1;
	}
	if (!GTK_WIDGET_VISIBLE(headerview->hbox)) return -1;

	if (headerview->image) {
		gtk_widget_destroy(headerview->image);
		headerview->image = NULL;
	}
	

	image = face_get_from_header(msginfo->extradata->face);

	if (image) {
		gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);
		gtk_widget_show(image);
	}

	headerview->image = image;
	if (image == NULL)
		return -1;
	else 
		return 0;
}

void headerview_clear(HeaderView *headerview)
{
	if (headerview == NULL)
		return;

	gtk_label_set_text(GTK_LABEL(headerview->from_body_label), "");
	gtk_label_set_text(GTK_LABEL(headerview->to_body_label), "");
	gtk_label_set_text(GTK_LABEL(headerview->ng_body_label), "");
	gtk_label_set_text(GTK_LABEL(headerview->subject_body_label), "");
	gtk_widget_hide(headerview->to_header_label);
	gtk_widget_hide(headerview->to_body_label);
	gtk_widget_hide(headerview->ng_header_label);
	gtk_widget_hide(headerview->ng_body_label);

	if (headerview->image && GTK_WIDGET_VISIBLE(headerview->image)) {
		gtk_widget_hide(headerview->image);
		gtk_widget_queue_resize(headerview->hbox);
	}
}

void headerview_set_visibility(HeaderView *headerview, gboolean visibility)
{
	if (visibility)
		gtk_widget_show(headerview->hbox);
	else
		gtk_widget_hide(headerview->hbox);
}

void headerview_destroy(HeaderView *headerview)
{
	g_free(headerview);
}
