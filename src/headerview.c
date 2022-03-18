/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2020 the Claws Mail team and Hiroyuki Yamamoto
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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#include "claws-features.h"
#endif

#include "defs.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include <gdk-pixbuf/gdk-pixbuf.h>

#include "headerview.h"
#include "prefs_common.h"
#include "codeconv.h"
#include "gtkutils.h"
#include "utils.h"
#include "headers.h"
#include "addrindex.h"
#include "hooks.h"
#include "avatars.h"

static gint headerview_show_avatar	(HeaderView	*headerview,
					 MsgInfo	*msginfo);
static gint headerview_show_contact_pic	(HeaderView	*headerview,
					 MsgInfo	*msginfo);
static void headerview_save_contact_pic	(HeaderView	*headerview,
					 MsgInfo	*msginfo);

HeaderView *headerview_create(void)
{
	HeaderView *headerview;
	GtkWidget *hbox;
	GtkWidget *vbox;
	GtkWidget *hbox1;
	GtkWidget *hbox2;
	GtkWidget *hbox3;
	GtkWidget *from_header_label;
	GtkWidget *from_body_label;
	GtkWidget *to_header_label;
	GtkWidget *to_body_label;
	GtkWidget *ng_header_label;
	GtkWidget *ng_body_label;
	GtkWidget *subject_header_label;
	GtkWidget *subject_body_label;
	GtkWidget *tags_header_label;
	GtkWidget *tags_body_label;

	debug_print("Creating header view...\n");
	headerview = g_new0(HeaderView, 1);

	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_set_name(GTK_WIDGET(hbox), "headerview");
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 2);
	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);

	hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_box_pack_start(GTK_BOX(vbox), hbox1, FALSE, FALSE, 0);
	hbox2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_box_pack_start(GTK_BOX(vbox), hbox2, FALSE, FALSE, 0);
	hbox3 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
	gtk_box_pack_start(GTK_BOX(vbox), hbox3, FALSE, FALSE, 0);

	from_header_label    = gtk_label_new(prefs_common_translated_header_name("From:"));
	from_body_label      = gtk_label_new("");
	to_header_label      = gtk_label_new(prefs_common_translated_header_name("To:"));
	to_body_label        = gtk_label_new("");
	ng_header_label      = gtk_label_new(prefs_common_translated_header_name("Newsgroups:"));
	ng_body_label        = gtk_label_new("");
	subject_header_label = gtk_label_new(prefs_common_translated_header_name("Subject:"));
	subject_body_label   = gtk_label_new("");
	tags_header_label = gtk_label_new(_("Tags:"));
	tags_body_label   = gtk_label_new("");

	gtk_label_set_selectable(GTK_LABEL(from_body_label), TRUE);
	gtk_label_set_selectable(GTK_LABEL(to_body_label), TRUE);
	gtk_label_set_selectable(GTK_LABEL(ng_body_label), TRUE);
	gtk_label_set_selectable(GTK_LABEL(subject_body_label), TRUE);
	gtk_label_set_selectable(GTK_LABEL(tags_body_label), TRUE);

	gtk_widget_set_can_focus(from_body_label, FALSE);
	gtk_widget_set_can_focus(to_body_label, FALSE);
	gtk_widget_set_can_focus(ng_body_label, FALSE);
	gtk_widget_set_can_focus(subject_body_label, FALSE);
	gtk_widget_set_can_focus(tags_body_label, FALSE);

	gtk_box_pack_start(GTK_BOX(hbox1), from_header_label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox1), from_body_label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox1), to_header_label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox1), to_body_label, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox1), ng_header_label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox1), ng_body_label, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox2), subject_header_label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox2), subject_body_label, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox3), tags_header_label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox3), tags_body_label, TRUE, TRUE, 0);

	gtk_label_set_xalign(GTK_LABEL(to_body_label), 0.0);
	gtk_label_set_xalign(GTK_LABEL(ng_body_label), 0.0);
	gtk_label_set_xalign(GTK_LABEL(subject_body_label), 0.0);
	gtk_label_set_xalign(GTK_LABEL(tags_body_label), 0.0);
	gtk_label_set_ellipsize(GTK_LABEL(to_body_label), PANGO_ELLIPSIZE_END);
	gtk_label_set_ellipsize(GTK_LABEL(ng_body_label), PANGO_ELLIPSIZE_END);
	gtk_label_set_ellipsize(GTK_LABEL(subject_body_label), PANGO_ELLIPSIZE_END);
	gtk_label_set_ellipsize(GTK_LABEL(tags_body_label), PANGO_ELLIPSIZE_END);

	headerview->hbox = hbox;
	headerview->from_header_label    = from_header_label;
	headerview->from_body_label      = from_body_label;
	headerview->to_header_label      = to_header_label;
	headerview->to_body_label        = to_body_label;
	headerview->ng_header_label      = ng_header_label;
	headerview->ng_body_label        = ng_body_label;
	headerview->subject_header_label = subject_header_label;
	headerview->subject_body_label   = subject_body_label;
	headerview->tags_header_label = tags_header_label;
	headerview->tags_body_label   = tags_body_label;
	headerview->image = NULL;

	gtk_widget_show_all(hbox);

	return headerview;
}

void headerview_set_font(HeaderView *headerview)
{
	PangoFontDescription *boldfont = NULL;
	PangoFontDescription *normalfont = NULL;
	
	normalfont = pango_font_description_from_string(NORMAL_FONT);
	if (normalfont) {
		gtk_widget_override_font(headerview->from_body_label, normalfont);
		gtk_widget_override_font(headerview->to_body_label, normalfont);
		gtk_widget_override_font(headerview->ng_body_label, normalfont);
		gtk_widget_override_font(headerview->subject_body_label, normalfont);
		gtk_widget_override_font(headerview->tags_body_label, normalfont);
		pango_font_description_free(normalfont);
	}

	if (prefs_common.derive_from_normal_font || !BOLD_FONT) {
		boldfont = pango_font_description_from_string(NORMAL_FONT);
		pango_font_description_set_weight(boldfont, PANGO_WEIGHT_BOLD);
	} else {
		boldfont = pango_font_description_from_string(BOLD_FONT);
	}
	if (boldfont) {
		gtk_widget_override_font(headerview->from_header_label, boldfont);
		gtk_widget_override_font(headerview->to_header_label, boldfont);
		gtk_widget_override_font(headerview->ng_header_label, boldfont);
		gtk_widget_override_font(headerview->subject_header_label, boldfont);
		gtk_widget_override_font(headerview->tags_header_label, boldfont);
		pango_font_description_free(boldfont);
	}
}

void headerview_init(HeaderView *headerview)
{
	headerview_set_font(headerview);
	headerview_clear(headerview);
	headerview_set_visibility(headerview, prefs_common.display_header_pane);
}

void headerview_show(HeaderView *headerview, MsgInfo *msginfo)
{
	gchar *tags = procmsg_msginfo_get_tags_str(msginfo);

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
	if (tags) {
		gtk_label_set_text(GTK_LABEL(headerview->tags_body_label),
				   tags);
		gtk_widget_show(headerview->tags_header_label);
		gtk_widget_show(headerview->tags_body_label);
		g_free(tags);
	}
	if (!headerview_show_avatar(headerview, msginfo))
		return;

	if (!headerview_show_contact_pic(headerview, msginfo))
		return;

}

static gint headerview_show_avatar (HeaderView *headerview, MsgInfo *msginfo)
{
	AvatarRender *avatarr = avatars_avatarrender_new(msginfo);
	GtkWidget *hbox = headerview->hbox;

	hooks_invoke(AVATAR_IMAGE_RENDER_HOOKLIST, avatarr);

	if (!avatarr->image) {
		if (headerview->image
				&& gtk_widget_get_visible(headerview->image)) {
			gtk_widget_hide(headerview->image);
			gtk_widget_queue_resize(hbox);
		}
		avatars_avatarrender_free(avatarr);
		return -1;
	}
	if (!gtk_widget_get_visible(hbox)) {
		avatars_avatarrender_free(avatarr);
		return -1;
	}

	if (headerview->image) {
		gtk_widget_destroy(headerview->image);
		headerview->image = NULL;
	}

	gtk_box_pack_start(GTK_BOX(hbox), avatarr->image, FALSE, FALSE, 0);
	gtk_widget_show(avatarr->image);

	headerview->image = avatarr->image;
	avatarr->image = NULL; /* avoid destroying */
	avatars_avatarrender_free(avatarr);

	if (prefs_common.save_xface)
		headerview_save_contact_pic(headerview, msginfo);
	return 0;
}

static void headerview_save_contact_pic (HeaderView *headerview, MsgInfo *msginfo)
{
#ifndef USE_ALT_ADDRBOOK
	gchar *filename = NULL;
	GError *error = NULL;
	GdkPixbuf *picture = NULL;

	if (!gtk_widget_get_visible(headerview->hbox)) return;

	if (headerview->image) {
		picture = gtk_image_get_pixbuf(GTK_IMAGE(headerview->image));
	}
	
	filename = addrindex_get_picture_file(msginfo->from);
	if (!filename)
		return;
	if (!is_file_exist(filename)) {
		gdk_pixbuf_save(picture, filename, "png", &error, NULL);
		if (error) {
			g_warning("failed to save image: %s",
					error->message);
			g_error_free(error);
		}
	}
	g_free(filename);
#else
	/* new address book */
#endif
}	

static gint headerview_show_contact_pic (HeaderView *headerview, MsgInfo *msginfo)
{
#ifndef USE_ALT_ADDRBOOK
	GtkWidget *hbox = headerview->hbox;
	GtkWidget *image;
	gchar *filename = NULL;
	GError *error = NULL;
	GdkPixbuf *picture = NULL;
	gint w, h;

	if (!gtk_widget_get_visible(headerview->hbox)) return -1;

	if (headerview->image) {
		gtk_widget_destroy(headerview->image);
		headerview->image = NULL;
	}
	
	filename = addrindex_get_picture_file(msginfo->from);
	
	if (!filename)
		return -1;
	if (!is_file_exist(filename)) {
		g_free(filename);
		return -1;
	}
	gdk_pixbuf_get_file_info(filename, &w, &h);
	
	if (w > 48 || h > 48)
		picture = gdk_pixbuf_new_from_file_at_scale(filename, 
						48, 48, TRUE, &error);
	else
		picture = gdk_pixbuf_new_from_file(filename, &error);

	g_free(filename);
	if (error) {
		debug_print("Failed to import image: %s\n",
				error->message);
		g_error_free(error);
		return -1;
	}
	if (picture)
		image = gtk_image_new_from_pixbuf(picture);
	else 
		return -1;

	g_object_unref(picture);
	if (image) {
		gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);
		gtk_widget_show(image);
	}

	headerview->image = image;
	if (image == NULL)
		return -1;
	else 
		return 0;
#else
	/* new address book */
	return -1;
#endif
}

void headerview_clear(HeaderView *headerview)
{
	if (headerview == NULL)
		return;

	gtk_label_set_text(GTK_LABEL(headerview->from_body_label), "");
	gtk_label_set_text(GTK_LABEL(headerview->to_body_label), "");
	gtk_label_set_text(GTK_LABEL(headerview->ng_body_label), "");
	gtk_label_set_text(GTK_LABEL(headerview->subject_body_label), "");
	gtk_label_set_text(GTK_LABEL(headerview->tags_body_label), "");
	gtk_widget_hide(headerview->to_header_label);
	gtk_widget_hide(headerview->to_body_label);
	gtk_widget_hide(headerview->ng_header_label);
	gtk_widget_hide(headerview->ng_body_label);
	gtk_widget_hide(headerview->tags_header_label);
	gtk_widget_hide(headerview->tags_body_label);

	if (headerview->image && gtk_widget_get_visible(headerview->image)) {
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
