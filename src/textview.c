/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2025 the Claws Mail team and Hiroyuki Yamamoto
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
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#if HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#include "main.h"
#include "summaryview.h"
#include "procheader.h"
#include "prefs_common.h"
#include "codeconv.h"
#include "utils.h"
#include "gtkutils.h"
#include "procmime.h"
#include "html.h"
#include "enriched.h"
#include "compose.h"
#ifndef USE_ALT_ADDRBOOK
	#include "addressbook.h"
	#include "addrindex.h"
#else
	#include "addressbook-dbus.h"
	#include "addressadd.h"
#endif
#include "displayheader.h"
#include "account.h"
#include "mimeview.h"
#include "alertpanel.h"
#include "menu.h"
#include "image_viewer.h"
#include "filesel.h"
#include "inputdialog.h"
#include "timing.h"
#include "tags.h"
#include "manage_window.h"
#include "folder_item_prefs.h"
#include "hooks.h"
#include "avatars.h"
#include "file-utils.h"

static GdkRGBA quote_colors[3] = {
	{0, 0, 0, 1},
	{0, 0, 0, 1},
	{0, 0, 0, 1}
};

static GdkRGBA quote_bgcolors[3] = {
	{0, 0, 0, 1},
	{0, 0, 0, 1},
	{0, 0, 0, 1}
};
static GdkRGBA signature_color = {
	0.5, 0.5, 0.5, 1
};
	
static GdkRGBA uri_color = {
	0, 0, 0, 1
};

static GdkRGBA emphasis_color = {
	0, 0, 0, 1
};

static GdkRGBA diff_added_color = {
	0, 0, 0, 1
};

static GdkRGBA diff_deleted_color = {
	0, 0, 0, 1
};

static GdkRGBA diff_hunk_color = {
	0, 0, 0, 1
};

static GdkRGBA tags_bgcolor = {
	0, 0, 0, 1
};

static GdkRGBA tags_color = {
	0, 0, 0, 1
};

static GdkCursor *hand_cursor = NULL;
static GdkCursor *text_cursor = NULL;
static GdkCursor *watch_cursor= NULL;

#define TEXTVIEW_FONT_SIZE_STEP 15 /* pango font zoom level change granularity in % */
#define TEXTVIEW_FONT_SIZE_MIN -75 /* this gives 5 zoom out steps at 15% */
#define TEXTVIEW_FONT_SIZE_MAX 3100 /* this gives 200 zoom in steps at 15% */
#define TEXTVIEW_FONT_SIZE_UNSET -666 /* default value when unset (must be lower than TEXTVIEW_FONT_SIZE_MIN */

/* font size in session (will apply to next message views we open */
/* must be lower than TEXTVIEW_FONT_SIZE_MIN */
static gint textview_font_size_percent = TEXTVIEW_FONT_SIZE_UNSET;
static gint textview_font_size_default = TEXTVIEW_FONT_SIZE_UNSET;

static void textview_set_font_zoom(TextView *textview);

#define TEXTVIEW_STATUSBAR_PUSH(textview, str)				    \
{	if (textview->messageview->statusbar)				    \
	gtk_statusbar_push(GTK_STATUSBAR(textview->messageview->statusbar), \
			   textview->messageview->statusbar_cid, str);	    \
}

#define TEXTVIEW_STATUSBAR_POP(textview)				   \
{	if (textview->messageview->statusbar)				   \
	gtk_statusbar_pop(GTK_STATUSBAR(textview->messageview->statusbar), \
			  textview->messageview->statusbar_cid);	   \
}

static void textview_show_ertf		(TextView	*textview,
					 FILE		*fp,
					 CodeConverter	*conv);
static void textview_add_part		(TextView	*textview,
					 MimeInfo	*mimeinfo);
static void textview_add_parts		(TextView	*textview,
					 MimeInfo	*mimeinfo);
static void textview_write_body		(TextView	*textview,
					 MimeInfo	*mimeinfo);
static void textview_show_html		(TextView	*textview,
					 FILE		*fp,
					 CodeConverter	*conv);

static void textview_write_line		(TextView	*textview,
					 const gchar	*str,
					 CodeConverter	*conv,
					 gboolean	 do_quote_folding);
static void textview_write_link		(TextView	*textview,
					 const gchar	*str,
					 const gchar	*uri,
					 CodeConverter	*conv);

static GPtrArray *textview_scan_header	(TextView	*textview,
					 FILE		*fp);
static void textview_show_header	(TextView	*textview,
					 GPtrArray	*headers);

static void textview_zoom(GtkWidget *widget, gboolean zoom_in);
static void textview_zoom_in(GtkWidget *widget, gpointer data);
static void textview_zoom_out(GtkWidget *widget, gpointer data);
static void textview_zoom_reset(GtkWidget *widget, gpointer data);

static gint textview_key_pressed		(GtkWidget	*widget,
						 GdkEventKey	*event,
						 TextView	*textview);
static gboolean textview_scrolled(GtkWidget *widget,
						 GdkEvent *_event,
						 gpointer user_data);
static void textview_populate_popup(GtkTextView *self,
						 GtkMenu *menu,
						 gpointer user_data);
static gboolean textview_motion_notify		(GtkWidget	*widget,
						 GdkEventMotion	*motion,
						 TextView	*textview);
static gboolean textview_leave_notify		(GtkWidget	  *widget,
						 GdkEventCrossing *event,
						 TextView	  *textview);
static gboolean textview_visibility_notify	(GtkWidget	*widget,
						 GdkEventVisibility *event,
						 TextView	*textview);
static void textview_uri_update			(TextView	*textview,
						 gint		x,
						 gint		y);
static gboolean textview_get_uri_range		(TextView	*textview,
						 GtkTextIter	*iter,
						 GtkTextTag	*tag,
						 GtkTextIter	*start_iter,
						 GtkTextIter	*end_iter);
static ClickableText *textview_get_uri_from_range	(TextView	*textview,
						 GtkTextIter	*iter,
						 GtkTextTag	*tag,
						 GtkTextIter	*start_iter,
						 GtkTextIter	*end_iter);
static ClickableText *textview_get_uri		(TextView	*textview,
						 GtkTextIter	*iter,
						 GtkTextTag	*tag);
static gboolean textview_uri_button_pressed	(GtkTextTag 	*tag,
						 GObject 	*obj,
						 GdkEvent 	*event,
						 GtkTextIter	*iter,
						 TextView 	*textview);

static void textview_uri_list_remove_all	(GSList		*uri_list);

static void textview_toggle_quote		(TextView 	*textview, 
						 GSList		*start_list,
						 ClickableText 	*uri,
						 gboolean	 expand_only);

static void open_uri_cb				(GtkAction	*action,
						 TextView	*textview);
static void copy_uri_cb				(GtkAction	*action,
						 TextView	*textview);
static void add_uri_to_addrbook_cb 		(GtkAction	*action,
						 TextView	*textview);
static void reply_to_uri_cb 			(GtkAction	*action,
						 TextView	*textview);
static void mail_to_uri_cb 			(GtkAction	*action,
						 TextView	*textview);
static void copy_mail_to_uri_cb			(GtkAction	*action,
						 TextView	*textview);
static void textview_show_tags(TextView *textview);

static GtkActionEntry textview_link_popup_entries[] = 
{
	{"TextviewPopupLink",			NULL, "TextviewPopupLink", NULL, NULL, NULL },
	{"TextviewPopupLink/Open",		NULL, N_("_Open in web browser"), NULL, NULL, G_CALLBACK(open_uri_cb) },
	{"TextviewPopupLink/Copy",		NULL, N_("Copy this _link"), NULL, NULL, G_CALLBACK(copy_uri_cb) },
};

static GtkActionEntry textview_mail_popup_entries[] = 
{
	{"TextviewPopupMail",			NULL, "TextviewPopupMail", NULL, NULL, NULL },
	{"TextviewPopupMail/Compose",		NULL, N_("Compose _new message"), NULL, NULL, G_CALLBACK(mail_to_uri_cb) },
	{"TextviewPopupMail/ReplyTo",		NULL, N_("_Reply to this address"), NULL, NULL, G_CALLBACK(reply_to_uri_cb) },
	{"TextviewPopupMail/AddAB",		NULL, N_("Add to _Address book"), NULL, NULL, G_CALLBACK(add_uri_to_addrbook_cb) },
	{"TextviewPopupMail/Copy",		NULL, N_("Copy this add_ress"), NULL, NULL, G_CALLBACK(copy_mail_to_uri_cb) },
};

static gboolean move_textview_image_cb (gpointer data)
{
#ifndef WIDTH
#  define WIDTH 48
#  define HEIGHT 48
#endif
	TextView *textview = (TextView *)data;
	GtkAllocation allocation;
	gint x, wx, wy;
	gtk_widget_get_allocation(textview->text, &allocation);
	x = allocation.width - WIDTH - 5;
	gtk_text_view_buffer_to_window_coords(
		GTK_TEXT_VIEW(textview->text),
		GTK_TEXT_WINDOW_TEXT, x, 5, &wx, &wy);
	gtk_text_view_move_child(GTK_TEXT_VIEW(textview->text),
		textview->image, wx, wy);

	return G_SOURCE_REMOVE;
}

static void scrolled_cb (GtkAdjustment *adj, gpointer data)
{
	TextView *textview = (TextView *)data;

	if (textview->image) {
		move_textview_image_cb(textview);
	}
}

static void textview_size_allocate_cb	(GtkWidget	*widget,
					 GtkAllocation	*allocation,
					 gpointer	 data)
{
	TextView *textview = (TextView *)data;

	if (textview->image) {
		g_timeout_add(0, &move_textview_image_cb, textview);
	}
}

TextView *textview_create(void)
{
	TextView *textview;
	GtkWidget *vbox;
	GtkWidget *scrolledwin;
	GtkWidget *text;
	GtkTextBuffer *buffer;
	GtkClipboard *clipboard;
	GtkAdjustment *adj;

	debug_print("Creating text view...\n");
	textview = g_new0(TextView, 1);

	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
				       GTK_POLICY_AUTOMATIC,
				       GTK_POLICY_AUTOMATIC);
	gtk_widget_set_vexpand(GTK_WIDGET(scrolledwin), TRUE);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledwin),
					    GTK_SHADOW_IN);

	/* create GtkSText widgets for single-byte and multi-byte character */
	text = gtk_text_view_new();
	gtk_widget_add_events(text, GDK_LEAVE_NOTIFY_MASK);
	gtk_widget_show(text);
	gtk_text_view_set_editable(GTK_TEXT_VIEW(text), FALSE);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text), GTK_WRAP_WORD_CHAR);
	gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(text), FALSE);
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(text), 6);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(text), 6);

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text));
	clipboard = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
	gtk_text_buffer_add_selection_clipboard(buffer, clipboard);

	gtk_widget_ensure_style(text);

	g_object_ref(scrolledwin);

	gtk_container_add(GTK_CONTAINER(scrolledwin), text);

	g_signal_connect(G_OBJECT(text), "key-press-event",
			 G_CALLBACK(textview_key_pressed), textview);
	g_signal_connect(G_OBJECT(text), "motion-notify-event",
			 G_CALLBACK(textview_motion_notify), textview);
	g_signal_connect(G_OBJECT(text), "leave-notify-event",
			 G_CALLBACK(textview_leave_notify), textview);
	g_signal_connect(G_OBJECT(text), "visibility-notify-event",
			 G_CALLBACK(textview_visibility_notify), textview);
	adj = gtk_scrolled_window_get_vadjustment(
		GTK_SCROLLED_WINDOW(scrolledwin));
	g_signal_connect(G_OBJECT(adj), "value-changed",
			 G_CALLBACK(scrolled_cb), textview);
	g_signal_connect(G_OBJECT(text), "size_allocate",
			 G_CALLBACK(textview_size_allocate_cb),
			 textview);
	g_signal_connect(G_OBJECT(text), "scroll-event",
			G_CALLBACK(textview_scrolled), textview);
	g_signal_connect(G_OBJECT(text), "populate-popup",
			G_CALLBACK(textview_populate_popup), textview);


	gtk_widget_show(scrolledwin);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_name(GTK_WIDGET(vbox), "textview");
	gtk_box_pack_start(GTK_BOX(vbox), scrolledwin, TRUE, TRUE, 0);

	gtk_widget_show(vbox);

	
	textview->ui_manager = gtk_ui_manager_new();
	textview->link_action_group = cm_menu_create_action_group_full(textview->ui_manager,
			"TextviewPopupLink",
			textview_link_popup_entries,
			G_N_ELEMENTS(textview_link_popup_entries), (gpointer)textview);
	textview->mail_action_group = cm_menu_create_action_group_full(textview->ui_manager,
			"TextviewPopupMail",
			textview_mail_popup_entries,
			G_N_ELEMENTS(textview_mail_popup_entries), (gpointer)textview);

	MENUITEM_ADDUI_MANAGER(textview->ui_manager, "/", "Menus", "Menus", GTK_UI_MANAGER_MENUBAR)
	MENUITEM_ADDUI_MANAGER(textview->ui_manager, 
			"/Menus", "TextviewPopupLink", "TextviewPopupLink", GTK_UI_MANAGER_MENU)
	MENUITEM_ADDUI_MANAGER(textview->ui_manager, 
			"/Menus", "TextviewPopupMail", "TextviewPopupMail", GTK_UI_MANAGER_MENU)

	MENUITEM_ADDUI_MANAGER(textview->ui_manager, 
			"/Menus/TextviewPopupLink", "Open", "TextviewPopupLink/Open", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(textview->ui_manager, 
			"/Menus/TextviewPopupLink", "Copy", "TextviewPopupLink/Copy", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(textview->ui_manager, 
			"/Menus/TextviewPopupMail", "Compose", "TextviewPopupMail/Compose", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(textview->ui_manager, 
			"/Menus/TextviewPopupMail", "ReplyTo", "TextviewPopupMail/ReplyTo", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(textview->ui_manager, 
			"/Menus/TextviewPopupMail", "AddAB", "TextviewPopupMail/AddAB", GTK_UI_MANAGER_MENUITEM)
	MENUITEM_ADDUI_MANAGER(textview->ui_manager, 
			"/Menus/TextviewPopupMail", "Copy", "TextviewPopupMail/Copy", GTK_UI_MANAGER_MENUITEM)

	textview->link_popup_menu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(
				gtk_ui_manager_get_widget(textview->ui_manager, "/Menus/TextviewPopupLink")) );
	textview->mail_popup_menu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(
				gtk_ui_manager_get_widget(textview->ui_manager, "/Menus/TextviewPopupMail")) );

	textview->vbox               = vbox;
	textview->scrolledwin        = scrolledwin;
	textview->text               = text;
	textview->uri_list           = NULL;
	textview->body_pos           = 0;
	textview->last_buttonpress   = GDK_NOTHING;
	textview->image		     = NULL;
	return textview;
}

static void textview_create_tags(GtkTextView *text, TextView *textview)
{
	GtkTextBuffer *buffer;
	GtkTextTag *tag, *qtag;
	static PangoFontDescription *font_desc, *bold_font_desc;
	
	if (!font_desc)
		font_desc = pango_font_description_from_string
			(NORMAL_FONT);

	if (!bold_font_desc) {
		if (prefs_common.derive_from_normal_font || !BOLD_FONT) {
			bold_font_desc = pango_font_description_from_string
				(NORMAL_FONT);
			pango_font_description_set_weight
				(bold_font_desc, PANGO_WEIGHT_BOLD);
		} else {
			bold_font_desc = pango_font_description_from_string
				(BOLD_FONT);
		}
	}

	buffer = gtk_text_view_get_buffer(text);

	gtk_text_buffer_create_tag(buffer, "header",
				   "pixels-above-lines", 0,
				   "pixels-above-lines-set", TRUE,
				   "pixels-below-lines", 0,
				   "pixels-below-lines-set", TRUE,
				   "font-desc", font_desc,
				   "left-margin", 3,
				   "left-margin-set", TRUE,
				   NULL);
	gtk_text_buffer_create_tag(buffer, "header_title",
				   "font-desc", bold_font_desc,
				   NULL);
	tag = gtk_text_buffer_create_tag(buffer, "hlink",
				   "pixels-above-lines", 0,
				   "pixels-above-lines-set", TRUE,
				   "pixels-below-lines", 0,
				   "pixels-below-lines-set", TRUE,
				   "font-desc", font_desc,
				   "left-margin", 3,
				   "left-margin-set", TRUE,
				   "foreground-rgba", &uri_color,
				   NULL);
	g_signal_connect(G_OBJECT(tag), "event",
				   G_CALLBACK(textview_uri_button_pressed), textview);
	if (prefs_common.enable_bgcolor) {
		gtk_text_buffer_create_tag(buffer, "quote0",
				"foreground-rgba", &quote_colors[0],
				"paragraph-background-rgba", &quote_bgcolors[0],
				NULL);
		gtk_text_buffer_create_tag(buffer, "quote1",
				"foreground-rgba", &quote_colors[1],
				"paragraph-background-rgba", &quote_bgcolors[1],
				NULL);
		gtk_text_buffer_create_tag(buffer, "quote2",
				"foreground-rgba", &quote_colors[2],
				"paragraph-background-rgba", &quote_bgcolors[2],
				NULL);
	} else {
		gtk_text_buffer_create_tag(buffer, "quote0",
				"foreground-rgba", &quote_colors[0],
				NULL);
		gtk_text_buffer_create_tag(buffer, "quote1",
				"foreground-rgba", &quote_colors[1],
				NULL);
		gtk_text_buffer_create_tag(buffer, "quote2",
				"foreground-rgba", &quote_colors[2],
				NULL);
	}
	gtk_text_buffer_create_tag(buffer, "tags",
			"foreground-rgba", &tags_color,
			"paragraph-background-rgba", &tags_bgcolor,
			NULL);
	gtk_text_buffer_create_tag(buffer, "emphasis",
			"foreground-rgba", &emphasis_color,
			NULL);
	gtk_text_buffer_create_tag(buffer, "signature",
			"foreground-rgba", &signature_color,
			NULL);
	tag = gtk_text_buffer_create_tag(buffer, "link",
			"foreground-rgba", &uri_color,
			NULL);
	qtag = gtk_text_buffer_create_tag(buffer, "qlink",
			NULL);
	gtk_text_buffer_create_tag(buffer, "link-hover",
			"underline", PANGO_UNDERLINE_SINGLE,
			NULL);
	gtk_text_buffer_create_tag(buffer, "diff-add",
			"foreground-rgba", &diff_added_color,
			NULL);
	gtk_text_buffer_create_tag(buffer, "diff-del",
			"foreground-rgba", &diff_deleted_color,
			NULL);
	gtk_text_buffer_create_tag(buffer, "diff-add-file",
			"foreground-rgba", &diff_added_color,
			"weight", PANGO_WEIGHT_BOLD,
			NULL);
	gtk_text_buffer_create_tag(buffer, "diff-del-file",
			"foreground-rgba", &diff_deleted_color,
			"weight", PANGO_WEIGHT_BOLD,
			NULL);
	gtk_text_buffer_create_tag(buffer, "diff-hunk",
			"foreground-rgba", &diff_hunk_color,
			"weight", PANGO_WEIGHT_BOLD,
			NULL);
	g_signal_connect(G_OBJECT(qtag), "event",
				   G_CALLBACK(textview_uri_button_pressed), textview);
	g_signal_connect(G_OBJECT(tag), "event",
				   G_CALLBACK(textview_uri_button_pressed), textview);
/*	if (font_desc)
		pango_font_description_free(font_desc);
	if (bold_font_desc)
		pango_font_description_free(bold_font_desc);*/
}

void textview_init(TextView *textview)
{
	if (!hand_cursor)
		hand_cursor = gdk_cursor_new_for_display(
				gtk_widget_get_display(textview->vbox), GDK_HAND2);
	if (!text_cursor)
		text_cursor = gdk_cursor_new_for_display(
				gtk_widget_get_display(textview->vbox), GDK_XTERM);
	if (!watch_cursor)
		watch_cursor = gdk_cursor_new_for_display(
				gtk_widget_get_display(textview->vbox), GDK_WATCH);

	textview_reflect_prefs(textview);
	textview_create_tags(GTK_TEXT_VIEW(textview->text), textview);
}

#define CHANGE_TAG_COLOR(tagname, colorfg, colorbg) { \
	tag = gtk_text_tag_table_lookup(tags, tagname); \
	if (tag) \
		g_object_set(G_OBJECT(tag), "foreground-rgba", colorfg, "paragraph-background-rgba", colorbg, NULL); \
}

static void textview_update_message_colors(TextView *textview)
{
	GdkRGBA black = {0, 0, 0, 1};
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview->text));

	GtkTextTagTable *tags = gtk_text_buffer_get_tag_table(buffer);
	GtkTextTag *tag = NULL;

	quote_bgcolors[0] = quote_bgcolors[1] = quote_bgcolors[2] = black;
	quote_colors[0] = quote_colors[1] = quote_colors[2] = black;
	uri_color = emphasis_color = signature_color = diff_added_color =
		diff_deleted_color = diff_hunk_color = black;
	tags_bgcolor = tags_color = black;

	if (prefs_common.enable_color) {
		/* grab the quote colors, converting from an int to a GdkColor */
		quote_colors[0] = prefs_common.color[COL_QUOTE_LEVEL1];
		quote_colors[1] = prefs_common.color[COL_QUOTE_LEVEL2];
		quote_colors[2] = prefs_common.color[COL_QUOTE_LEVEL3];
		uri_color = prefs_common.color[COL_URI];
		signature_color = prefs_common.color[COL_SIGNATURE];
		emphasis_color = prefs_common.color[COL_EMPHASIS];
		diff_added_color = prefs_common.color[COL_DIFF_ADDED];
		diff_deleted_color = prefs_common.color[COL_DIFF_DELETED];
		diff_hunk_color = prefs_common.color[COL_DIFF_HUNK];
	}
	if (prefs_common.enable_color && prefs_common.enable_bgcolor) {
		quote_bgcolors[0] = prefs_common.color[COL_QUOTE_LEVEL1_BG];
		quote_bgcolors[1] = prefs_common.color[COL_QUOTE_LEVEL2_BG];
		quote_bgcolors[2] = prefs_common.color[COL_QUOTE_LEVEL3_BG];
		CHANGE_TAG_COLOR("quote0", &quote_colors[0], &quote_bgcolors[0]);
		CHANGE_TAG_COLOR("quote1", &quote_colors[1], &quote_bgcolors[1]);
		CHANGE_TAG_COLOR("quote2", &quote_colors[2], &quote_bgcolors[2]);
	} else {
		CHANGE_TAG_COLOR("quote0", &quote_colors[0], NULL);
		CHANGE_TAG_COLOR("quote1", &quote_colors[1], NULL);
		CHANGE_TAG_COLOR("quote2", &quote_colors[2], NULL);
	}

	CHANGE_TAG_COLOR("emphasis", &emphasis_color, NULL);
	CHANGE_TAG_COLOR("signature", &signature_color, NULL);
	CHANGE_TAG_COLOR("link", &uri_color, NULL);
	CHANGE_TAG_COLOR("link-hover", &uri_color, NULL);
	CHANGE_TAG_COLOR("diff-add", &diff_added_color, NULL);
	CHANGE_TAG_COLOR("diff-del", &diff_deleted_color, NULL);
	CHANGE_TAG_COLOR("diff-add-file", &diff_added_color, NULL);
	CHANGE_TAG_COLOR("diff-del-file", &diff_deleted_color, NULL);
	CHANGE_TAG_COLOR("diff-hunk", &diff_hunk_color, NULL);

	tags_bgcolor = prefs_common.color[COL_TAGS_BG];
	tags_color = prefs_common.color[COL_TAGS];
}
#undef CHANGE_TAG_COLOR

void textview_reflect_prefs(TextView *textview)
{
	textview_set_font(textview, NULL);
	textview_update_message_colors(textview);
	gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(textview->text),
					 prefs_common.textview_cursor_visible);
}

void textview_show_part(TextView *textview, MimeInfo *mimeinfo, FILE *fp)
{
	START_TIMING("");
	cm_return_if_fail(mimeinfo != NULL);
	cm_return_if_fail(fp != NULL);

	textview->loading = TRUE;
	textview->stop_loading = FALSE;

	textview_clear(textview);

	if (mimeinfo->type == MIMETYPE_MULTIPART ||
	    (mimeinfo->type == MIMETYPE_MESSAGE && !g_ascii_strcasecmp(mimeinfo->subtype, "rfc822"))) {
		textview_add_parts(textview, mimeinfo);
	} else {
		if (fseek(fp, mimeinfo->offset, SEEK_SET) < 0)
			perror("fseek");

		textview_write_body(textview, mimeinfo);
	}

	textview->loading = FALSE;
	textview->stop_loading = FALSE;
	textview_set_position(textview, 0);

	END_TIMING();
}

static void textview_add_part(TextView *textview, MimeInfo *mimeinfo)
{
	GtkAllocation allocation;
	GtkTextView *text;
	GtkTextBuffer *buffer;
	GtkTextIter iter, start_iter;
	gchar buf[BUFFSIZE];
	GPtrArray *headers = NULL;
	const gchar *name;
	gchar *content_type;
	gint charcount;

	START_TIMING("");

	cm_return_if_fail(mimeinfo != NULL);
	text = GTK_TEXT_VIEW(textview->text);
	buffer = gtk_text_view_get_buffer(text);
	charcount = gtk_text_buffer_get_char_count(buffer);
	gtk_text_buffer_get_end_iter(buffer, &iter);
	
	if (textview->stop_loading) {
		return;
	}
	if (mimeinfo->type == MIMETYPE_MULTIPART) {
		END_TIMING();
		return;
	}

	textview->prev_quote_level = -1;

	if ((mimeinfo->type == MIMETYPE_MESSAGE) && !g_ascii_strcasecmp(mimeinfo->subtype, "rfc822")) {
		FILE *fp;
		if (mimeinfo->content == MIMECONTENT_MEM)
			fp = str_open_as_stream(mimeinfo->data.mem);
		else
			fp = claws_fopen(mimeinfo->data.filename, "rb");
		if (!fp) {
			FILE_OP_ERROR(mimeinfo->data.filename, "claws_fopen");
			END_TIMING();
			return;
		}
		if (fseek(fp, mimeinfo->offset, SEEK_SET) < 0) {
			FILE_OP_ERROR(mimeinfo->data.filename, "fseek");
			claws_fclose(fp);
			END_TIMING();
			return;
		}
		headers = textview_scan_header(textview, fp);
		if (headers) {
			if (charcount > 0)
				gtk_text_buffer_insert(buffer, &iter, "\n", 1);
			
			if (procmime_mimeinfo_parent(mimeinfo) == NULL &&
			    !prefs_common.display_header_pane)
				textview_show_tags(textview);
			textview_show_header(textview, headers);
			procheader_header_array_destroy(headers);
		}
		claws_fclose(fp);
		END_TIMING();
		return;
	}

	name = procmime_mimeinfo_get_parameter(mimeinfo, "filename");
	content_type = procmime_get_content_type_str(mimeinfo->type,
						     mimeinfo->subtype);
	if (name == NULL)
		name = procmime_mimeinfo_get_parameter(mimeinfo, "name");
	if (name != NULL)
		g_snprintf(buf, sizeof(buf), _("[%s  %s (%ld bytes)]"),
			   name, content_type, mimeinfo->length);
	else
		g_snprintf(buf, sizeof(buf), _("[%s (%ld bytes)]"),
			   content_type, mimeinfo->length);

	g_free(content_type);			   

	if (mimeinfo->disposition == DISPOSITIONTYPE_ATTACHMENT
	|| (mimeinfo->disposition == DISPOSITIONTYPE_INLINE && 
	    mimeinfo->type != MIMETYPE_TEXT)) {
		gtk_text_buffer_insert(buffer, &iter, "\n", 1);
		TEXTVIEW_INSERT_LINK(buf, "cm://select_attachment", mimeinfo);
		gtk_text_buffer_insert(buffer, &iter, " \n", -1);
		if (mimeinfo->type == MIMETYPE_IMAGE  && mimeinfo->subtype &&
		    g_ascii_strcasecmp(mimeinfo->subtype, "x-eps") &&
		    prefs_common.inline_img ) {
			GdkPixbuf *pixbuf;
			GError *error = NULL;
			ClickableText *uri;

			START_TIMING("inserting image");

			pixbuf = procmime_get_part_as_pixbuf(mimeinfo, &error);
			if (error != NULL) {
				g_warning("can't load the image: %s", error->message);
				g_error_free(error);
				END_TIMING();
				return;
			}

			if (textview->stop_loading) {
				END_TIMING();
				return;
			}

			gtk_widget_get_allocation(textview->scrolledwin, &allocation);
			pixbuf = claws_load_pixbuf_fitting(pixbuf, prefs_common.inline_img,
					prefs_common.fit_img_height, allocation.width,
					allocation.height);

			if (textview->stop_loading) {
				END_TIMING();
				return;
			}

			uri = g_new0(ClickableText, 1);
			uri->uri = g_strdup("");
			uri->filename = g_strdup("cm://select_attachment");
			uri->data = mimeinfo;

			uri->start = gtk_text_iter_get_offset(&iter);
			gtk_text_buffer_insert_pixbuf(buffer, &iter, pixbuf);
			g_object_unref(pixbuf);
			if (textview->stop_loading) {
				g_free(uri);
				return;
			}
			uri->end = gtk_text_iter_get_offset(&iter);

			textview->uri_list =
				g_slist_prepend(textview->uri_list, uri);

			gtk_text_buffer_insert(buffer, &iter, " ", 1);
			gtk_text_buffer_get_iter_at_offset(buffer, &start_iter, uri->start);
			gtk_text_buffer_apply_tag_by_name(buffer, "link",
						&start_iter, &iter);

			END_TIMING();
			GTK_EVENTS_FLUSH();
		}
	} else if (mimeinfo->type == MIMETYPE_TEXT) {
		if (prefs_common.display_header && (charcount > 0))
			gtk_text_buffer_insert(buffer, &iter, "\n", 1);

		if (!gtk_text_buffer_get_mark(buffer, "body_start")) {
			gtk_text_buffer_get_end_iter(buffer, &iter);
			gtk_text_buffer_create_mark(buffer, "body_start", &iter, TRUE);
		}

		textview_write_body(textview, mimeinfo);

		if (!gtk_text_buffer_get_mark(buffer, "body_end")) {
			gtk_text_buffer_get_end_iter(buffer, &iter);
			gtk_text_buffer_create_mark(buffer, "body_end", &iter, TRUE);
		}
	}
	END_TIMING();
}

static void recursive_add_parts(TextView *textview, GNode *node)
{
        GNode * iter;
	MimeInfo *mimeinfo;
        START_TIMING("");

        mimeinfo = (MimeInfo *) node->data;
        
        textview_add_part(textview, mimeinfo);
#ifdef GENERIC_UMPC
	textview_set_position(textview, 0);
#endif        
        if ((mimeinfo->type != MIMETYPE_MULTIPART) &&
            (mimeinfo->type != MIMETYPE_MESSAGE)) {
	    	END_TIMING();
                return;
        }
        if (g_ascii_strcasecmp(mimeinfo->subtype, "alternative") == 0) {
                GNode * preferred_body;
                int preferred_score;

                /*
                  text/plain : score 3
                  text/ *    : score 2
                  other      : score 1
                */
                preferred_body = NULL;
                preferred_score = 0;

                for (iter = g_node_first_child(node) ; iter != NULL ;
                     iter = g_node_next_sibling(iter)) {
                        int score;
                        MimeInfo * submime;

                        score = 1;
                        submime = (MimeInfo *) iter->data;
                        if (submime->type == MIMETYPE_TEXT)
                                score = 2;
 
                        if (submime->subtype != NULL) {
                                if (g_ascii_strcasecmp(submime->subtype, "plain") == 0)
                                        score = 3;
                        }

                        if (score > preferred_score) {
                                preferred_score = score;
                                preferred_body = iter;
                        }
                }

                if (preferred_body != NULL) {
                        recursive_add_parts(textview, preferred_body);
                }
        }
        else {
                for (iter = g_node_first_child(node) ; iter != NULL ;
                     iter = g_node_next_sibling(iter)) {
                        recursive_add_parts(textview, iter);
                }
        }
	END_TIMING();
}

static void textview_add_parts(TextView *textview, MimeInfo *mimeinfo)
{
	cm_return_if_fail(mimeinfo != NULL);
	cm_return_if_fail(mimeinfo->node != NULL);

	recursive_add_parts(textview, mimeinfo->node);
}

void textview_show_error(TextView *textview)
{
	GtkTextView *text;
	GtkTextBuffer *buffer;
	GtkTextIter iter;

	textview_set_font(textview, NULL);
	textview_clear(textview);

	text = GTK_TEXT_VIEW(textview->text);
	buffer = gtk_text_view_get_buffer(text);
	gtk_text_buffer_get_start_iter(buffer, &iter);

	TEXTVIEW_INSERT(_("\n"
		      "  This message can't be displayed.\n"
		      "  This is probably due to a network error.\n"
		      "\n"
		      "  Use "));
	TEXTVIEW_INSERT_LINK(_("'Network Log'"), "cm://view_log", NULL);
	TEXTVIEW_INSERT(_(" in the Tools menu for more information."));
	textview_show_icon(textview, "dialog-error");
}

void textview_show_info(TextView *textview, const gchar *info_str)
{
	GtkTextView *text;
	GtkTextBuffer *buffer;
	GtkTextIter iter;

	textview_set_font(textview, NULL);
	textview_clear(textview);

	text = GTK_TEXT_VIEW(textview->text);
	buffer = gtk_text_view_get_buffer(text);
	gtk_text_buffer_get_start_iter(buffer, &iter);

	TEXTVIEW_INSERT(info_str);
	textview_show_icon(textview, "dialog-information");
	textview_cursor_normal(textview);
}

void textview_show_mime_part(TextView *textview, MimeInfo *partinfo)
{
	GtkTextView *text;
	GtkTextBuffer *buffer;
	GtkTextIter iter;
	const gchar *name;
	gchar *content_type;
	GtkUIManager *ui_manager;
#ifndef GENERIC_UMPC
	gchar *shortcut;
#endif

	if (!partinfo) return;

	if (textview->messageview->window != NULL)
		ui_manager = textview->messageview->ui_manager;
	else
		ui_manager = textview->messageview->mainwin->ui_manager;

	textview_set_font(textview, NULL);
	textview_clear(textview);

	text = GTK_TEXT_VIEW(textview->text);
	buffer = gtk_text_view_get_buffer(text);
	gtk_text_buffer_get_start_iter(buffer, &iter);

	TEXTVIEW_INSERT("\n");

	name = procmime_mimeinfo_get_parameter(partinfo, "filename");
	if (name == NULL)
		name = procmime_mimeinfo_get_parameter(partinfo, "name");
	if (name != NULL) {
		content_type = procmime_get_content_type_str(partinfo->type,
						     partinfo->subtype);
		TEXTVIEW_INSERT("  ");
		TEXTVIEW_INSERT_BOLD(name);
		TEXTVIEW_INSERT(" (");
		TEXTVIEW_INSERT(content_type);
		TEXTVIEW_INSERT(", ");
		TEXTVIEW_INSERT(to_human_readable((goffset)partinfo->length));
		TEXTVIEW_INSERT("):\n\n");
		
		g_free(content_type);
	}
	TEXTVIEW_INSERT(_("  The following can be performed on this part\n"));
#ifndef GENERIC_UMPC
	TEXTVIEW_INSERT(_("  by right-clicking the icon or list item:"));
#endif
	TEXTVIEW_INSERT("\n");

	TEXTVIEW_INSERT(_("     - To save, select "));
	TEXTVIEW_INSERT_LINK(_("'Save as...'"), "cm://save_as", NULL);
#ifndef GENERIC_UMPC
	TEXTVIEW_INSERT(_(" (Shortcut key: '"));
	shortcut = cm_menu_item_get_shortcut(ui_manager, "Menu/File/SavePartAs");
	TEXTVIEW_INSERT(shortcut);
	g_free(shortcut);
	TEXTVIEW_INSERT("')");
#endif
	TEXTVIEW_INSERT("\n");

	TEXTVIEW_INSERT(_("     - To display as text, select "));
	TEXTVIEW_INSERT_LINK(_("'Display as text'"), "cm://display_as_text", NULL);

#ifndef GENERIC_UMPC
	TEXTVIEW_INSERT(_(" (Shortcut key: '"));
	shortcut = cm_menu_item_get_shortcut(ui_manager, "Menu/View/Part/AsText");
	TEXTVIEW_INSERT(shortcut);
	g_free(shortcut);
	TEXTVIEW_INSERT("')");
#endif
	TEXTVIEW_INSERT("\n");

	TEXTVIEW_INSERT(_("     - To open with an external program, select "));
	TEXTVIEW_INSERT_LINK(_("'Open'"), "cm://open", NULL);

#ifndef GENERIC_UMPC
	TEXTVIEW_INSERT(_(" (Shortcut key: '"));
	shortcut = cm_menu_item_get_shortcut(ui_manager, "Menu/View/Part/Open");
	TEXTVIEW_INSERT(shortcut);
	g_free(shortcut);
	TEXTVIEW_INSERT("')\n");
	TEXTVIEW_INSERT(_("       (alternately double-click, or click the middle "));
	TEXTVIEW_INSERT(_("mouse button)\n"));
#ifndef G_OS_WIN32
	TEXTVIEW_INSERT(_("     - Or use "));
	TEXTVIEW_INSERT_LINK(_("'Open with...'"), "cm://open_with", NULL);
	TEXTVIEW_INSERT(_(" (Shortcut key: '"));
	shortcut = cm_menu_item_get_shortcut(ui_manager, "Menu/View/Part/OpenWith");
	TEXTVIEW_INSERT(shortcut);
	g_free(shortcut);
	TEXTVIEW_INSERT("')");
#endif
#endif
	TEXTVIEW_INSERT("\n");

	textview_show_icon(textview, "dialog-information");
}

static void textview_write_body(TextView *textview, MimeInfo *mimeinfo)
{
	FILE *tmpfp;
	gchar buf[BUFFSIZE];
	CodeConverter *conv;
	const gchar *charset;
#ifndef G_OS_WIN32
	const gchar *p, *cmd;
#endif
	GSList *cur;
	gboolean continue_write = TRUE;
	glong wrote = 0, i = 0;

	if (textview->messageview->forced_charset)
		charset = textview->messageview->forced_charset;
	else {
		/* use supersets transparently when possible */
		charset = procmime_mimeinfo_get_parameter(mimeinfo, "charset");
		if (charset && !strcasecmp(charset, CS_ISO_8859_1))
			charset = CS_WINDOWS_1252;
		else if (charset && !strcasecmp(charset, CS_X_GBK))
			charset = CS_GB18030;
		else if (charset && !strcasecmp(charset, CS_GBK))
			charset = CS_GB18030;
		else if (charset && !strcasecmp(charset, CS_GB2312))
			charset = CS_GB18030;
	}

	textview_set_font(textview, charset);
	textview_set_font_zoom(textview);

	conv = conv_code_converter_new(charset);

	procmime_force_encoding(textview->messageview->forced_encoding);
	
	textview->is_in_signature = FALSE;
	textview->is_diff = FALSE;
	textview->is_attachment = FALSE;
	textview->is_in_git_patch = FALSE;

	procmime_decode_content(mimeinfo);

	account_sigsep_matchlist_create();

	if (!g_ascii_strcasecmp(mimeinfo->subtype, "html") &&
	    prefs_common.render_html) {
		gchar *filename;
		
		filename = procmime_get_tmp_file_name(mimeinfo);
		if (procmime_get_part(filename, mimeinfo) == 0) {
			tmpfp = claws_fopen(filename, "rb");
			if (tmpfp) {
				textview_show_html(textview, tmpfp, conv);
				claws_fclose(tmpfp);
			}
			claws_unlink(filename);
		}
		g_free(filename);
	} else if (!g_ascii_strcasecmp(mimeinfo->subtype, "enriched")) {
		gchar *filename;
		
		filename = procmime_get_tmp_file_name(mimeinfo);
		if (procmime_get_part(filename, mimeinfo) == 0) {
			tmpfp = claws_fopen(filename, "rb");
			if (tmpfp) {
				textview_show_ertf(textview, tmpfp, conv);
				claws_fclose(tmpfp);
			}
			claws_unlink(filename);
		}
		g_free(filename);
#ifndef G_OS_WIN32
	} else if ( g_ascii_strcasecmp(mimeinfo->subtype, "plain") &&
		   (cmd = prefs_common.mime_textviewer) && *cmd &&
		   (p = strchr(cmd, '%')) && *(p + 1) == 's') {
		int pid, pfd[2];
		const gchar *fname;

		fname  = procmime_get_tmp_file_name(mimeinfo);
		if (procmime_get_part(fname, mimeinfo)) goto textview_default;

		g_snprintf(buf, sizeof(buf), cmd, fname);
		debug_print("Viewing text content of type: %s (length: %ld) "
			"using %s\n", mimeinfo->subtype, mimeinfo->length, buf);

		if (pipe(pfd) < 0) {
			g_snprintf(buf, sizeof(buf),
				"pipe failed for textview\n\n%s\n", g_strerror(errno));
			textview_write_line(textview, buf, conv, TRUE);
			goto textview_default;
		}
		pid = fork();
		if (pid < 0) {
			g_snprintf(buf, sizeof(buf),
				"fork failed for textview\n\n%s\n", g_strerror(errno));
			textview_write_line(textview, buf, conv, TRUE);
			close(pfd[0]);
			close(pfd[1]);
			goto textview_default;
		}
		if (pid == 0) { /* child */
			int rc;
			gchar **argv;
			argv = strsplit_with_quote(buf, " ", 0);
			close(1);
			close(pfd[0]);
			rc = dup(pfd[1]);
			rc = execvp(argv[0], argv);
			perror("execvp");
			close(pfd[1]);
			g_print(_("The command to view attachment "
			        "as text failed:\n"
			        "    %s\n"
			        "Exit code %d\n"), buf, rc);
			exit(255);
		}
		close(pfd[1]);
		tmpfp = claws_fdopen(pfd[0], "rb");
		while (claws_fgets(buf, sizeof(buf), tmpfp)) {
			textview_write_line(textview, buf, conv, TRUE);
			
			if (textview->stop_loading) {
				claws_fclose(tmpfp);
				waitpid(pid, pfd, 0);
				g_unlink(fname);
				account_sigsep_matchlist_delete();
				conv_code_converter_destroy(conv);
				return;
			}
		}

		claws_fclose(tmpfp);
		waitpid(pid, pfd, 0);
		g_unlink(fname);
#endif
	} else {
#ifndef G_OS_WIN32
textview_default:
#endif
		if (!g_ascii_strcasecmp(mimeinfo->subtype, "x-patch")
				|| !g_ascii_strcasecmp(mimeinfo->subtype, "x-diff"))
			textview->is_diff = TRUE;

		/* Displayed part is an attachment, but not an attached
		 * e-mail. Set a flag, so that elsewhere in the code we
		 * know not to try making collapsible quotes in it. */
		if (mimeinfo->disposition == DISPOSITIONTYPE_ATTACHMENT &&
				mimeinfo->type != MIMETYPE_MESSAGE)
			textview->is_attachment = TRUE;

		if (mimeinfo->content == MIMECONTENT_MEM)
			tmpfp = str_open_as_stream(mimeinfo->data.mem);
		else
			tmpfp = claws_fopen(mimeinfo->data.filename, "rb");
		if (!tmpfp) {
			FILE_OP_ERROR(mimeinfo->data.filename, "claws_fopen");
			account_sigsep_matchlist_delete();
			conv_code_converter_destroy(conv);
			return;
		}
		if (fseek(tmpfp, mimeinfo->offset, SEEK_SET) < 0) {
			FILE_OP_ERROR(mimeinfo->data.filename, "fseek");
			claws_fclose(tmpfp);
			account_sigsep_matchlist_delete();
			conv_code_converter_destroy(conv);
			return;
		}
		debug_print("Viewing text content of type: %s (length: %ld)\n", mimeinfo->subtype, mimeinfo->length);
		while (((i = ftell(tmpfp)) < mimeinfo->offset + mimeinfo->length) &&
		       (claws_fgets(buf, sizeof(buf), tmpfp) != NULL)
		       && continue_write) {
			textview_write_line(textview, buf, conv, TRUE);
			if (textview->stop_loading) {
				claws_fclose(tmpfp);
				account_sigsep_matchlist_delete();
				conv_code_converter_destroy(conv);
				return;
			}
			wrote += ftell(tmpfp)-i;
			if (mimeinfo->length > 1024*1024 
			&&  wrote > 1024*1024
			&& !textview->messageview->show_full_text) {
				continue_write = FALSE;
			}
		}
		claws_fclose(tmpfp);
	}

	account_sigsep_matchlist_delete();

	conv_code_converter_destroy(conv);
	procmime_force_encoding(0);

	textview->uri_list = g_slist_reverse(textview->uri_list);
	for (cur = textview->uri_list; cur; cur = cur->next) {
		ClickableText *uri = (ClickableText *)cur->data;
		if (!uri->is_quote)
			continue;
		if (!prefs_common.hide_quotes ||
		    uri->quote_level+1 < prefs_common.hide_quotes) {
			textview_toggle_quote(textview, cur, uri, TRUE);
			if (textview->stop_loading) {
				return;
			}
		}
	}
	
	if (continue_write == FALSE) {
		messageview_show_partial_display(
			textview->messageview, 
			textview->messageview->msginfo,
			mimeinfo->length);
	}
	GTK_EVENTS_FLUSH();
}

static void textview_show_html(TextView *textview, FILE *fp,
			       CodeConverter *conv)
{
	SC_HTMLParser *parser;
	gchar *str;
	gint lines = 0;

	parser = sc_html_parser_new(fp, conv);
	cm_return_if_fail(parser != NULL);

	account_sigsep_matchlist_create();

	while ((str = sc_html_parse(parser)) != NULL) {
	        if (parser->state == SC_HTML_HREF) {
		        /* first time : get and copy the URL */
		        if (parser->href == NULL) {
				/* ALF - the claws html parser returns an empty string,
				 * if still inside an <a>, but already parsed past HREF */
				str = strtok(str, " ");
				if (str) {
					while (str && *str && g_ascii_isspace(*str))
						str++; 
					parser->href = g_strdup(str);
					/* the URL may (or not) be followed by the
					 * referenced text */
					str = strtok(NULL, "");
				}	
		        }
		        if (str != NULL)
			        textview_write_link(textview, str, parser->href, NULL);
	        } else
		        textview_write_line(textview, str, NULL, FALSE);
		lines++;
		if (lines % 500 == 0)
			GTK_EVENTS_FLUSH();
		if (textview->stop_loading) {
			account_sigsep_matchlist_delete();
			sc_html_parser_destroy(parser);
			return;
		}
	}
	textview_write_line(textview, "\n", NULL, FALSE);

	account_sigsep_matchlist_delete();

	sc_html_parser_destroy(parser);
}

static void textview_show_ertf(TextView *textview, FILE *fp,
			       CodeConverter *conv)
{
	ERTFParser *parser;
	gchar *str;
	gint lines = 0;

	parser = ertf_parser_new(fp, conv);
	cm_return_if_fail(parser != NULL);

	account_sigsep_matchlist_create();

	while ((str = ertf_parse(parser)) != NULL) {
		textview_write_line(textview, str, NULL, FALSE);
		lines++;
		if (lines % 500 == 0)
			GTK_EVENTS_FLUSH();
		if (textview->stop_loading) {
			account_sigsep_matchlist_delete();
			ertf_parser_destroy(parser);
			return;
		}
	}
	
	account_sigsep_matchlist_delete();

	ertf_parser_destroy(parser);
}

#define ADD_TXT_POS(bp_, ep_, pti_) \
	if ((last->next = alloca(sizeof(struct txtpos))) != NULL) { \
		last = last->next; \
		last->bp = (bp_); last->ep = (ep_); last->pti = (pti_); \
		last->next = NULL; \
	} else { \
		g_warning("alloc error scanning URIs"); \
		gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, \
							 linebuf, -1, \
							 fg_tag, NULL); \
		return; \
	}

#define ADD_TXT_POS_LATER(bp_, ep_, pti_) \
	if ((last->next = alloca(sizeof(struct txtpos))) != NULL) { \
		last = last->next; \
		last->bp = (bp_); last->ep = (ep_); last->pti = (pti_); \
		last->next = NULL; \
	} else { \
		g_warning("alloc error scanning URIs"); \
	}

/* textview_make_clickable_parts() - colorizes clickable parts */
static void textview_make_clickable_parts(TextView *textview,
					  const gchar *fg_tag,
					  const gchar *uri_tag,
					  const gchar *linebuf,
					  gboolean hdr)
{
	GtkTextView *text = GTK_TEXT_VIEW(textview->text);
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(text);
	GtkTextIter iter;
	gchar *mybuf = g_strdup(linebuf);
	
	/* parse table - in order of priority */
	struct table {
		const gchar *needle; /* token */

		/* token search function */
		gchar    *(*search)	(const gchar *haystack,
					 const gchar *needle);
		/* part parsing function */
		gboolean  (*parse)	(const gchar *start,
					 const gchar *scanpos,
					 const gchar **bp_,
					 const gchar **ep_,
					 gboolean hdr);
		/* part to URI function */
		gchar    *(*build_uri)	(const gchar *bp,
					 const gchar *ep);
	};

	static struct table parser[] = {
		{"http://",  strcasestr, get_uri_part,   make_uri_string},
		{"https://", strcasestr, get_uri_part,   make_uri_string},
		{"ftp://",   strcasestr, get_uri_part,   make_uri_string},
		{"ftps://",  strcasestr, get_uri_part,   make_uri_string},
		{"sftp://",  strcasestr, get_uri_part,   make_uri_string},
		{"gopher://",strcasestr, get_uri_part,   make_uri_string},
		{"www.",     strcasestr, get_uri_part,   make_http_string},
		{"webcal://",strcasestr, get_uri_part,   make_uri_string},
		{"webcals://",strcasestr, get_uri_part,  make_uri_string},
		{"mailto:",  strcasestr, get_uri_part,   make_uri_string},
		{"@",        strcasestr, get_email_part, make_email_string}
	};
	const gint PARSE_ELEMS = sizeof parser / sizeof parser[0];

	gint  n;
	const gchar *walk, *bp, *ep;

	struct txtpos {
		const gchar	*bp, *ep;	/* text position */
		gint		 pti;		/* index in parse table */
		struct txtpos	*next;		/* next */
	} head = {NULL, NULL, 0,  NULL}, *last = &head;

	if (!g_utf8_validate(linebuf, -1, NULL)) {
		g_free(mybuf);
		mybuf = g_malloc(strlen(linebuf)*2 +1);
		conv_localetodisp(mybuf, strlen(linebuf)*2 +1, linebuf);
	}

	gtk_text_buffer_get_end_iter(buffer, &iter);

	/* parse for clickable parts, and build a list of begin and end positions  */
	for (walk = mybuf;;) {
		gint last_index = PARSE_ELEMS;
		gchar *scanpos = NULL;

		/* FIXME: this looks phony. scanning for anything in the parse table */
		for (n = 0; n < PARSE_ELEMS; n++) {
			gchar *tmp;

			tmp = parser[n].search(walk, parser[n].needle);
			if (tmp) {
				if (scanpos == NULL || tmp < scanpos) {
					scanpos = tmp;
					last_index = n;
				}
			}					
		}

		if (scanpos) {
			/* check if URI can be parsed */
			if (parser[last_index].parse(walk, scanpos, &bp, &ep, hdr)
			    && (size_t) (ep - bp - 1) > strlen(parser[last_index].needle)) {
					ADD_TXT_POS(bp, ep, last_index);
					walk = ep;
			} else
				walk = scanpos +
					strlen(parser[last_index].needle);
		} else
			break;
	}

	/* colorize this line */
	if (head.next) {
		const gchar *normal_text = mybuf;

		/* insert URIs */
		for (last = head.next; last != NULL;
		     normal_text = last->ep, last = last->next) {
			ClickableText *uri;
			uri = g_new0(ClickableText, 1);
			if (last->bp - normal_text > 0)
				gtk_text_buffer_insert_with_tags_by_name
					(buffer, &iter,
					 normal_text,
					 last->bp - normal_text,
					 fg_tag, NULL);
			uri->uri = parser[last->pti].build_uri(last->bp,
							       last->ep);
			uri->start = gtk_text_iter_get_offset(&iter);
			gtk_text_buffer_insert_with_tags_by_name
				(buffer, &iter, last->bp, last->ep - last->bp,
				 uri_tag, fg_tag, NULL);
			uri->end = gtk_text_iter_get_offset(&iter);
			uri->filename = NULL;
			textview->uri_list =
				g_slist_prepend(textview->uri_list, uri);
		}

		if (*normal_text)
			gtk_text_buffer_insert_with_tags_by_name
				(buffer, &iter, normal_text, -1, fg_tag, NULL);
	} else {
		gtk_text_buffer_insert_with_tags_by_name
			(buffer, &iter, mybuf, -1, fg_tag, NULL);
	}
	g_free(mybuf);
}

/* textview_make_clickable_parts() - colorizes clickable parts */
static void textview_make_clickable_parts_later(TextView *textview,
					  gint start, gint end)
{
	GtkTextView *text = GTK_TEXT_VIEW(textview->text);
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(text);
	GtkTextIter start_iter, end_iter;
	gchar *mybuf;
	gint offset = 0;
	/* parse table - in order of priority */
	struct table {
		const gchar *needle; /* token */

		/* token search function */
		gchar    *(*search)	(const gchar *haystack,
					 const gchar *needle);
		/* part parsing function */
		gboolean  (*parse)	(const gchar *start,
					 const gchar *scanpos,
					 const gchar **bp_,
					 const gchar **ep_,
					 gboolean hdr);
		/* part to URI function */
		gchar    *(*build_uri)	(const gchar *bp,
					 const gchar *ep);
	};

	static struct table parser[] = {
		{"http://",  strcasestr, get_uri_part,   make_uri_string},
		{"https://", strcasestr, get_uri_part,   make_uri_string},
		{"ftp://",   strcasestr, get_uri_part,   make_uri_string},
		{"ftps://",  strcasestr, get_uri_part,   make_uri_string},
		{"sftp://",  strcasestr, get_uri_part,   make_uri_string},
		{"www.",     strcasestr, get_uri_part,   make_http_string},
		{"mailto:",  strcasestr, get_uri_part,   make_uri_string},
		{"webcal://",strcasestr, get_uri_part,   make_uri_string},
		{"webcals://",strcasestr, get_uri_part,  make_uri_string},
		{"@",        strcasestr, get_email_part, make_email_string}
	};
	const gint PARSE_ELEMS = sizeof parser / sizeof parser[0];

	gint  n;
	const gchar *walk, *bp, *ep;

	struct txtpos {
		const gchar	*bp, *ep;	/* text position */
		gint		 pti;		/* index in parse table */
		struct txtpos	*next;		/* next */
	} head = {NULL, NULL, 0,  NULL}, *last = &head;

	gtk_text_buffer_get_iter_at_offset(buffer, &start_iter, start);
	gtk_text_buffer_get_iter_at_offset(buffer, &end_iter, end);
	mybuf = gtk_text_buffer_get_text(buffer, &start_iter, &end_iter, FALSE);
	offset = gtk_text_iter_get_offset(&start_iter);

	/* parse for clickable parts, and build a list of begin and end positions  */
	for (walk = mybuf;;) {
		gint last_index = PARSE_ELEMS;
		gchar *scanpos = NULL;

		/* FIXME: this looks phony. scanning for anything in the parse table */
		for (n = 0; n < PARSE_ELEMS; n++) {
			gchar *tmp;

			tmp = parser[n].search(walk, parser[n].needle);
			if (tmp) {
				if (scanpos == NULL || tmp < scanpos) {
					scanpos = tmp;
					last_index = n;
				}
			}					
		}

		if (scanpos) {
			/* check if URI can be parsed */
			if (parser[last_index].parse(walk, scanpos, &bp, &ep, FALSE)
			    && (size_t) (ep - bp - 1) > strlen(parser[last_index].needle)) {
					ADD_TXT_POS_LATER(bp, ep, last_index);
					walk = ep;
			} else
				walk = scanpos +
					strlen(parser[last_index].needle);
		} else
			break;
	}

	/* colorize this line */
	if (head.next) {
		/* insert URIs */
		for (last = head.next; last != NULL; last = last->next) {
			ClickableText *uri;
			gint start_offset, end_offset;
			gchar *tmp_str;
			gchar old_char;
			uri = g_new0(ClickableText, 1);
			uri->uri = parser[last->pti].build_uri(last->bp,
							       last->ep);
			
			tmp_str = mybuf;
			old_char = tmp_str[last->ep - mybuf];
			tmp_str[last->ep - mybuf] = '\0';				       
			end_offset = g_utf8_strlen(tmp_str, -1);
			tmp_str[last->ep - mybuf] = old_char;
			
			old_char = tmp_str[last->bp - mybuf];
			tmp_str[last->bp - mybuf] = '\0';				       
			start_offset = g_utf8_strlen(tmp_str, -1);
			tmp_str[last->bp - mybuf] = old_char;
			
			gtk_text_buffer_get_iter_at_offset(buffer, &start_iter, start_offset + offset);
			gtk_text_buffer_get_iter_at_offset(buffer, &end_iter, end_offset + offset);
			
			uri->start = gtk_text_iter_get_offset(&start_iter);
			
			gtk_text_buffer_apply_tag_by_name(buffer, "link", &start_iter, &end_iter);

			uri->end = gtk_text_iter_get_offset(&end_iter);
			uri->filename = NULL;
			textview->uri_list =
				g_slist_prepend(textview->uri_list, uri);
		}
	} 

	g_free(mybuf);
}

#undef ADD_TXT_POS

static void textview_write_line(TextView *textview, const gchar *str,
				CodeConverter *conv, gboolean do_quote_folding)
{
	GtkTextView *text;
	GtkTextBuffer *buffer;
	GtkTextIter iter;
	gchar buf[BUFFSIZE];
	gchar *fg_color;
	gint quotelevel = -1, real_quotelevel = -1;
	gchar quote_tag_str[10];

	text = GTK_TEXT_VIEW(textview->text);
	buffer = gtk_text_view_get_buffer(text);
	gtk_text_buffer_get_end_iter(buffer, &iter);

	if (!conv)
		strncpy2(buf, str, sizeof(buf));
	else if (conv_convert(conv, buf, sizeof(buf), str) < 0)
		conv_localetodisp(buf, sizeof(buf), str);
		
	strcrchomp(buf);
	fg_color = NULL;

	/* change color of quotation
	   >, foo>, _> ... ok, <foo>, foo bar>, foo-> ... ng
	   Up to 3 levels of quotations are detected, and each
	   level is colored using a different color. */
	if (prefs_common.enable_color
	    && !textview->is_attachment
	    && line_has_quote_char(buf, prefs_common.quote_chars)) {
		real_quotelevel = get_quote_level(buf, prefs_common.quote_chars);
		quotelevel = real_quotelevel;
		/* set up the correct foreground color */
		if (quotelevel > 2) {
			/* recycle colors */
			if (prefs_common.recycle_quote_colors)
				quotelevel %= 3;
			else
				quotelevel = 2;
		}
	}

	if (quotelevel == -1)
		fg_color = NULL;
	else {
		g_snprintf(quote_tag_str, sizeof(quote_tag_str),
			   "quote%d", quotelevel);
		fg_color = quote_tag_str;
	}

	if (prefs_common.enable_color) {
		if (textview->is_diff || textview->is_in_git_patch) {
			if (strncmp(buf, "+++ ", 4) == 0)
				fg_color = "diff-add-file";
			else if (buf[0] == '+')
				fg_color = "diff-add";
			else if (strncmp(buf, "--- ", 4) == 0)
				fg_color = "diff-del-file";
			else if (buf[0] == '-')
				fg_color = "diff-del";
			else if (strncmp(buf, "@@ ", 3) == 0 &&
				 strstr(&buf[3], " @@"))
				fg_color = "diff-hunk";

			if (account_sigsep_matchlist_nchar_found(buf, "%s\n")) {
				textview->is_in_git_patch = FALSE;
				textview->is_in_signature = TRUE;
				fg_color = "signature";
			}
		} else if (account_sigsep_matchlist_str_found(buf, "%s\n")
				|| account_sigsep_matchlist_str_found(buf, "- %s\n")
				|| textview->is_in_signature) {
			fg_color = "signature";
			textview->is_in_signature = TRUE;
		} else if (strncmp(buf, "diff --git ", 11) == 0) {
			textview->is_in_git_patch = TRUE;
		}
	}

	if (!textview->is_attachment && real_quotelevel > -1 && do_quote_folding) {
		if (!g_utf8_validate(buf, -1, NULL)) {
			gchar *utf8buf = NULL;
			utf8buf = g_malloc(BUFFSIZE);
			conv_localetodisp(utf8buf, BUFFSIZE, buf);
			strncpy2(buf, utf8buf, BUFFSIZE-1);
			g_free(utf8buf);
		}
do_quote:
		if ( textview->prev_quote_level != real_quotelevel ) {
			ClickableText *uri;
			uri = g_new0(ClickableText, 1);
			uri->uri = g_strdup("");
			uri->data = g_strdup(buf);
			uri->data_len = strlen(uri->data);
			uri->start = gtk_text_iter_get_offset(&iter);
			uri->is_quote = TRUE;
			uri->quote_level = real_quotelevel;
			uri->fg_color = g_strdup(fg_color);

			gtk_text_buffer_insert_with_tags_by_name
					(buffer, &iter, " [...]", -1,
					 "qlink", fg_color, NULL);
			uri->end = gtk_text_iter_get_offset(&iter);
			gtk_text_buffer_insert(buffer, &iter, "  \n", -1);
			
			uri->filename = NULL;
			textview->uri_list =
				g_slist_prepend(textview->uri_list, uri);
		
			textview->prev_quote_level = real_quotelevel;
		} else {
			GSList *last = textview->uri_list;
			ClickableText *lasturi = NULL;
			gint e_len = 0, n_len = 0;
			
			if (textview->uri_list) {
				lasturi = (ClickableText *)last->data;
			} else {
				g_print("oops (%d %d)\n",
					real_quotelevel, textview->prev_quote_level);
			}	
			if (lasturi) {	
				if (lasturi->is_quote == FALSE) {
					textview->prev_quote_level = -1;
					goto do_quote;
				}
				e_len = lasturi->data ? lasturi->data_len:0;
				n_len = strlen(buf);
				lasturi->data = g_realloc((gchar *)lasturi->data, e_len + n_len + 1);
				strcpy((gchar *)lasturi->data + e_len, buf);
				*((gchar *)lasturi->data + e_len + n_len) = '\0';
				lasturi->data_len += n_len;
			}
		}
	} else {
		textview_make_clickable_parts(textview, fg_color, "link", buf, FALSE);
		textview->prev_quote_level = -1;
	}
}

void textview_write_link(TextView *textview, const gchar *str,
			 const gchar *uri, CodeConverter *conv)
{
	GtkTextView *text;
	GtkTextBuffer *buffer;
	GtkTextIter iter;
	gchar buf[BUFFSIZE];
	gchar *bufp;
	ClickableText *r_uri;

	if (!str || *str == '\0')
		return;
	if (!uri)
		return;

	while (uri && *uri && g_ascii_isspace(*uri))
		uri++;
		
	text = GTK_TEXT_VIEW(textview->text);
	buffer = gtk_text_view_get_buffer(text);
	gtk_text_buffer_get_end_iter(buffer, &iter);

	if (!conv)
		strncpy2(buf, str, sizeof(buf));
	else if (conv_convert(conv, buf, sizeof(buf), str) < 0)
		conv_utf8todisp(buf, sizeof(buf), str);

	if (g_utf8_validate(buf, -1, NULL) == FALSE)
		return;

	strcrchomp(buf);

	gtk_text_buffer_get_end_iter(buffer, &iter);
	for (bufp = buf; *bufp != '\0'; bufp = g_utf8_next_char(bufp)) {
		gunichar ch;

		ch = g_utf8_get_char(bufp);
		if (!g_unichar_isspace(ch))
			break;
	}
	if (bufp > buf)
		gtk_text_buffer_insert(buffer, &iter, buf, bufp - buf);

	r_uri = g_new0(ClickableText, 1);
	r_uri->uri = g_strdup(uri);
	r_uri->start = gtk_text_iter_get_offset(&iter);
	gtk_text_buffer_insert_with_tags_by_name
		(buffer, &iter, bufp, -1, "link", NULL);
	r_uri->end = gtk_text_iter_get_offset(&iter);
	r_uri->filename = NULL;
	textview->uri_list = g_slist_prepend(textview->uri_list, r_uri);
}

static void textview_set_cursor(GdkWindow *window, GdkCursor *cursor)
{
	if (GDK_IS_WINDOW(window))
		gdk_window_set_cursor(window, cursor);
}
void textview_clear(TextView *textview)
{
	GtkTextView *text = GTK_TEXT_VIEW(textview->text);
	GtkTextBuffer *buffer;
	GdkWindow *window = gtk_text_view_get_window(text,
				GTK_TEXT_WINDOW_TEXT);

	buffer = gtk_text_view_get_buffer(text);
	gtk_text_buffer_set_text(buffer, "", -1);
	if (gtk_text_buffer_get_mark(buffer, "body_start"))
		gtk_text_buffer_delete_mark_by_name(buffer, "body_start");
	if (gtk_text_buffer_get_mark(buffer, "body_end"))
		gtk_text_buffer_delete_mark_by_name(buffer, "body_end");

	TEXTVIEW_STATUSBAR_POP(textview);
	textview_uri_list_remove_all(textview->uri_list);
	textview->uri_list = NULL;
	textview->uri_hover = NULL;
	textview->prev_quote_level = -1;

	textview->body_pos = 0;
	if (textview->image) 
		gtk_widget_destroy(textview->image);
	textview->image = NULL;
	textview->avatar_type = 0;

	if (textview->messageview->mainwin->cursor_count == 0) {
		textview_set_cursor(window, text_cursor);
	} else {
		textview_set_cursor(window, watch_cursor);
	}
}

void textview_destroy(TextView *textview)
{
	GtkTextBuffer *buffer;
	GtkClipboard *clipboard;

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview->text));
	clipboard = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
	gtk_text_buffer_remove_selection_clipboard(buffer, clipboard);

	textview_uri_list_remove_all(textview->uri_list);
	textview->uri_list = NULL;
	textview->prev_quote_level = -1;

	g_free(textview);
}

#define CHANGE_TAG_FONT(tagname, font) { \
	tag = gtk_text_tag_table_lookup(tags, tagname); \
	if (tag) \
		g_object_set(G_OBJECT(tag), "font-desc", font, NULL); \
}

void textview_set_font(TextView *textview, const gchar *codeset)
{
	GtkTextTag *tag;
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview->text));
	GtkTextTagTable *tags = gtk_text_buffer_get_tag_table(buffer);
	PangoFontDescription *font_desc, *bold_font_desc;

	font_desc = pango_font_description_from_string
					(NORMAL_FONT);
	if (font_desc) {
		gtk_widget_override_font(textview->text, font_desc);
		CHANGE_TAG_FONT("header", font_desc);
		CHANGE_TAG_FONT("hlink", font_desc);
		pango_font_description_free(font_desc);
	}
	if (prefs_common.derive_from_normal_font || !BOLD_FONT) {
		bold_font_desc = pango_font_description_from_string
						(NORMAL_FONT);
		if (bold_font_desc)
			pango_font_description_set_weight
				(bold_font_desc, PANGO_WEIGHT_BOLD);
	} else {
		bold_font_desc = pango_font_description_from_string
						(BOLD_FONT);
	}
	if (bold_font_desc) {
		CHANGE_TAG_FONT("header_title", bold_font_desc);
		pango_font_description_free(bold_font_desc);
	}

	if (prefs_common.textfont) {
		PangoFontDescription *font_desc;

		font_desc = pango_font_description_from_string
						(prefs_common.textfont);
		if (font_desc) {
			gtk_widget_override_font(textview->text, font_desc);
			pango_font_description_free(font_desc);
		}
	}
	gtk_text_view_set_pixels_above_lines(GTK_TEXT_VIEW(textview->text),
					     prefs_common.line_space / 2);
	gtk_text_view_set_pixels_below_lines(GTK_TEXT_VIEW(textview->text),
					     prefs_common.line_space / 2);
}

void textview_set_text(TextView *textview, const gchar *text)
{
	GtkTextView *view;
	GtkTextBuffer *buffer;

	cm_return_if_fail(textview != NULL);
	cm_return_if_fail(text != NULL);

	textview_clear(textview);

	view = GTK_TEXT_VIEW(textview->text);
	buffer = gtk_text_view_get_buffer(view);
	gtk_text_buffer_set_text(buffer, text, strlen(text));
}

enum
{
	H_DATE		= 0,
	H_FROM		= 1,
	H_TO		= 2,
	H_NEWSGROUPS	= 3,
	H_SUBJECT	= 4,
	H_CC		= 5,
	H_REPLY_TO	= 6,
	H_FOLLOWUP_TO	= 7,
	H_X_MAILER	= 8,
	H_X_NEWSREADER	= 9,
	H_USER_AGENT	= 10,
	H_ORGANIZATION	= 11,
};

void textview_set_position(TextView *textview, gint pos)
{
	GtkTextView *text = GTK_TEXT_VIEW(textview->text);

	gtkut_text_view_set_position(text, pos);
}

static GPtrArray *textview_scan_header(TextView *textview, FILE *fp)
{
	gchar buf[BUFFSIZE];
	GPtrArray *headers, *sorted_headers;
	GSList *disphdr_list;
	Header *header;
	gint i;

	cm_return_val_if_fail(fp != NULL, NULL);

	if (prefs_common.show_all_headers) {
		headers = procheader_get_header_array(fp);
		sorted_headers = g_ptr_array_new();
		for (i = 0; i < headers->len; i++) {
			header = g_ptr_array_index(headers, i);
			if (!procheader_header_is_internal(header->name))
				g_ptr_array_add(sorted_headers, header);
			else
				procheader_header_free(header);
		}
		g_ptr_array_free(headers, TRUE);
		return sorted_headers;
	}

	if (!prefs_common.display_header) {
		while (claws_fgets(buf, sizeof(buf), fp) != NULL)
			if (buf[0] == '\r' || buf[0] == '\n') break;
		return NULL;
	}

	headers = procheader_get_header_array(fp);

	sorted_headers = g_ptr_array_new();

	for (disphdr_list = prefs_common.disphdr_list; disphdr_list != NULL;
	     disphdr_list = disphdr_list->next) {
		DisplayHeaderProp *dp =
			(DisplayHeaderProp *)disphdr_list->data;

		for (i = 0; i < headers->len; i++) {
			header = g_ptr_array_index(headers, i);
			if (procheader_headername_equal(header->name,
							dp->name)) {
				if (dp->hidden)
					procheader_header_free(header);
				else
					g_ptr_array_add(sorted_headers, header);

				g_ptr_array_remove_index(headers, i);
				i--;
			}
		}
	}

	if (prefs_common.show_other_header) {
		for (i = 0; i < headers->len; i++) {
			header = g_ptr_array_index(headers, i);
			unfold_line(header->body);
			if (!procheader_header_is_internal(header->name)) {
				g_ptr_array_add(sorted_headers, header);
			} else {
				procheader_header_free(header);
			}
		}
		g_ptr_array_free(headers, TRUE);
	} else
		procheader_header_array_destroy(headers);


	return sorted_headers;
}

static void textview_show_avatar(TextView *textview)
{
	GtkAllocation allocation;
	GtkTextView *text = GTK_TEXT_VIEW(textview->text);
	MsgInfo *msginfo = textview->messageview->msginfo;
	gint x, wx, wy;
	AvatarRender *avatarr;
	
	if (prefs_common.display_header_pane || !prefs_common.display_xface)
		goto bail;
	
	avatarr = avatars_avatarrender_new(msginfo);
	hooks_invoke(AVATAR_IMAGE_RENDER_HOOKLIST, avatarr);

	if (!avatarr->image) {
		avatars_avatarrender_free(avatarr);
		goto bail;
	}

	if (textview->image) 
		gtk_widget_destroy(textview->image);
	
	textview->image = avatarr->image;
	textview->avatar_type = avatarr->type;
	avatarr->image = NULL; /* avoid destroying */
	avatars_avatarrender_free(avatarr);

	gtk_widget_set_name(GTK_WIDGET(textview->image), "textview_avatar");
	gtk_widget_show(textview->image);
	
	gtk_widget_get_allocation(textview->text, &allocation);
	x = allocation.width - WIDTH - 5;

	gtk_text_view_buffer_to_window_coords(
		GTK_TEXT_VIEW(textview->text),
		GTK_TEXT_WINDOW_TEXT, x, 5, &wx, &wy);

	gtk_text_view_add_child_in_window(text, textview->image, 
		GTK_TEXT_WINDOW_TEXT, wx, wy);

	gtk_widget_show_all(textview->text);

	return;
bail:
	if (textview->image) 
		gtk_widget_destroy(textview->image);
	textview->image = NULL;	
	textview->avatar_type = 0;
}

void textview_show_icon(TextView *textview, const gchar *stock_id)
{
	GtkAllocation allocation;
	GtkTextView *text = GTK_TEXT_VIEW(textview->text);
	gint x, wx, wy;
	
	if (textview->image) 
		gtk_widget_destroy(textview->image);
	
	textview->image = gtk_image_new_from_icon_name(stock_id, GTK_ICON_SIZE_DIALOG);
	cm_return_if_fail(textview->image != NULL);

	gtk_widget_set_name(GTK_WIDGET(textview->image), "textview_icon");
	gtk_widget_show(textview->image);
	
	gtk_widget_get_allocation(textview->text, &allocation);
	x = allocation.width - WIDTH - 5;

	gtk_text_view_buffer_to_window_coords(
		GTK_TEXT_VIEW(textview->text),
		GTK_TEXT_WINDOW_TEXT, x, 5, &wx, &wy);

	gtk_text_view_add_child_in_window(text, textview->image, 
		GTK_TEXT_WINDOW_TEXT, wx, wy);

	gtk_widget_show_all(textview->text);
	

	return;
}

static void textview_save_contact_pic(TextView *textview)
{
#ifndef USE_ALT_ADDRBOOK
	MsgInfo *msginfo = textview->messageview->msginfo;
	gchar *filename = NULL;
	GError *error = NULL;
	GdkPixbuf *picture = NULL;

	if (!msginfo->extradata || !msginfo->extradata->avatars)
		return;

	if (textview->avatar_type > AVATAR_FACE)
		return;

	if (textview->image) 
		picture = gtk_image_get_pixbuf(GTK_IMAGE(textview->image));

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

static void textview_show_contact_pic(TextView *textview)
{
#ifndef USE_ALT_ADDRBOOK
	MsgInfo *msginfo = textview->messageview->msginfo;
	GtkTextView *text = GTK_TEXT_VIEW(textview->text);
	gint x, wx, wy;
	gchar *filename = NULL;
	GError *error = NULL;
	GdkPixbuf *picture = NULL;
	gint w, h;
	GtkAllocation allocation;

	if (prefs_common.display_header_pane
		|| !prefs_common.display_xface)
		goto bail;
	
	if (msginfo->extradata && msginfo->extradata->avatars)
		return;

	if (textview->image) 
		gtk_widget_destroy(textview->image);

	filename = addrindex_get_picture_file(msginfo->from);
	
	if (!filename)
		goto bail;
	if (!is_file_exist(filename)) {
		g_free(filename);
		goto bail;
	}

	gdk_pixbuf_get_file_info(filename, &w, &h);
	
	if (w > 48 || h > 48)
		picture = gdk_pixbuf_new_from_file_at_scale(filename, 
						48, 48, TRUE, &error);
	else
		picture = gdk_pixbuf_new_from_file(filename, &error);

	if (error) {
		debug_print("Failed to import image: %s\n",
				error->message);
		g_error_free(error);
		goto bail;
	}
	g_free(filename);

	if (picture) {
		textview->image = gtk_image_new_from_pixbuf(picture);
		g_object_unref(picture);
	}
	cm_return_if_fail(textview->image != NULL);

	gtk_widget_set_name(GTK_WIDGET(textview->image), "textview_contact_pic");
	gtk_widget_show(textview->image);
	
	gtk_widget_get_allocation(textview->text, &allocation);
	x = allocation.width - WIDTH - 5;

	gtk_text_view_buffer_to_window_coords(
		GTK_TEXT_VIEW(textview->text),
		GTK_TEXT_WINDOW_TEXT, x, 5, &wx, &wy);

	gtk_text_view_add_child_in_window(text, textview->image, 
		GTK_TEXT_WINDOW_TEXT, wx, wy);

	gtk_widget_show_all(textview->text);
	
	return;
bail:
	if (textview->image) 
		gtk_widget_destroy(textview->image);
	textview->image = NULL;
	textview->avatar_type = 0;
#else
	/* new address book */
#endif	
}

static gint textview_tag_cmp_list(gconstpointer a, gconstpointer b)
{
	gint id_a = GPOINTER_TO_INT(a);
	gint id_b = GPOINTER_TO_INT(b);
	const gchar *tag_a = tags_get_tag(id_a);
	const gchar *tag_b = tags_get_tag(id_b);
	
	if (tag_a == NULL)
		return tag_b == NULL ? 0:1;
	
	if (tag_b == NULL)
		return 1;

	return g_utf8_collate(tag_a, tag_b);
}


static void textview_show_tags(TextView *textview)
{
	MsgInfo *msginfo = textview->messageview->msginfo;
	GtkTextView *text = GTK_TEXT_VIEW(textview->text);
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(text);
	GtkTextIter iter;
	ClickableText *uri;
	GSList *cur, *orig;
	gboolean found_tag = FALSE;
	
	if (!msginfo->tags)
		return;
	
	cur = orig = g_slist_sort(g_slist_copy(msginfo->tags), textview_tag_cmp_list);

	for (; cur; cur = cur->next) {
		if (tags_get_tag(GPOINTER_TO_INT(cur->data)) != NULL) {
			found_tag = TRUE;
			break;
		}
	}
	if (!found_tag) {
		g_slist_free(orig);
		return;
	}

	gtk_text_buffer_get_end_iter (buffer, &iter);
	gtk_text_buffer_insert_with_tags_by_name(buffer,
		&iter, _("Tags: "), -1,
		"header_title", "header", "tags", NULL);

	for (cur = orig; cur; cur = cur->next) {
		const gchar *cur_tag = tags_get_tag(GPOINTER_TO_INT(cur->data));
		if (!cur_tag)
			continue;
		uri = g_new0(ClickableText, 1);
		uri->uri = g_strdup("");
		uri->start = gtk_text_iter_get_offset(&iter);
		gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, 
			cur_tag, -1,
			"link", "header", "tags", NULL);
		uri->end = gtk_text_iter_get_offset(&iter);
		uri->filename = g_strdup_printf("cm://search_tags:%s", cur_tag);
		uri->data = NULL;
		textview->uri_list =
			g_slist_prepend(textview->uri_list, uri);
		if (cur->next && tags_get_tag(GPOINTER_TO_INT(cur->next->data)))
			gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, ", ", 2,
				"header", "tags", NULL);
		else
			gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, " ", 1,
				"header", "tags", NULL);
	}
	g_slist_free(orig);

	gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, "\n", 1,
		"header", "tags", NULL);
}

static void textview_show_header(TextView *textview, GPtrArray *headers)
{
	GtkTextView *text = GTK_TEXT_VIEW(textview->text);
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(text);
	GtkTextIter iter;
	Header *header;
	gint i;

	cm_return_if_fail(headers != NULL);

	for (i = 0; i < headers->len; i++) {
		header = g_ptr_array_index(headers, i);
		cm_return_if_fail(header->name != NULL);

		gtk_text_buffer_get_end_iter (buffer, &iter);
		if(prefs_common.trans_hdr == TRUE) {
			gchar *hdr = g_strndup(header->name, strlen(header->name) - 1);
			gchar *trans_hdr = gettext(hdr);
			gtk_text_buffer_insert_with_tags_by_name(buffer,
				&iter, trans_hdr, -1,
				"header_title", "header", NULL);
			gtk_text_buffer_insert_with_tags_by_name(buffer,
				&iter, ":", 1, "header_title", "header", NULL);
			g_free(hdr);
		} else {
			gtk_text_buffer_insert_with_tags_by_name(buffer,
				&iter, header->name,
				-1, "header_title", "header", NULL);
		}
		if (header->name[strlen(header->name) - 1] != ' ')
		gtk_text_buffer_insert_with_tags_by_name
				(buffer, &iter, " ", 1,
				 "header_title", "header", NULL);

		if (procheader_headername_equal(header->name, "Subject") ||
		    procheader_headername_equal(header->name, "From") ||
		    procheader_headername_equal(header->name, "To") ||
		    procheader_headername_equal(header->name, "Cc") ||
		    procheader_headername_equal(header->name, "Bcc") ||
		    procheader_headername_equal(header->name, "Reply-To") ||
		    procheader_headername_equal(header->name, "Sender") ||
		    procheader_headername_equal(header->name, "Resent-From") ||
		    procheader_headername_equal(header->name, "Resent-To"))
			unfold_line(header->body);
		
		if (procheader_headername_equal(header->name, "Date") &&
		    prefs_common.msgview_date_format) {
			gchar hbody[80];
			
			procheader_date_parse(hbody, header->body, sizeof(hbody));
			gtk_text_buffer_get_end_iter (buffer, &iter);
			gtk_text_buffer_insert_with_tags_by_name
				(buffer, &iter, hbody, -1, "header", NULL);
		} else if ((procheader_headername_equal(header->name, "X-Mailer") ||
				procheader_headername_equal(header->name,
						 "X-Newsreader")) &&
				(strstr(header->body, "Claws Mail") != NULL ||
				strstr(header->body, "Sylpheed-Claws") != NULL)) {
			gtk_text_buffer_get_end_iter (buffer, &iter);
			gtk_text_buffer_insert_with_tags_by_name
				(buffer, &iter, header->body, -1,
				 "header", "emphasis", NULL);
		} else {
			gboolean hdr = 
			  procheader_headername_equal(header->name, "From") ||
			  procheader_headername_equal(header->name, "To") ||
			  procheader_headername_equal(header->name, "Cc") ||
			  procheader_headername_equal(header->name, "Bcc") ||
			  procheader_headername_equal(header->name, "Reply-To") ||
			  procheader_headername_equal(header->name, "Sender") ||
			  procheader_headername_equal(header->name, "Resent-From") ||
			  procheader_headername_equal(header->name, "Resent-To");
			textview_make_clickable_parts(textview, "header", 
						      "hlink", header->body, 
						      hdr);
		}
		gtk_text_buffer_get_end_iter (buffer, &iter);
		gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, "\n", 1,
							 "header", NULL);
	}
	
	textview_show_avatar(textview);
	if (prefs_common.save_xface)
		textview_save_contact_pic(textview);
	textview_show_contact_pic(textview);
}

gboolean textview_search_string(TextView *textview, const gchar *str,
				gboolean case_sens)
{
	GtkTextView *text = GTK_TEXT_VIEW(textview->text);

	return gtkut_text_view_search_string(text, str, case_sens);
}

gboolean textview_search_string_backward(TextView *textview, const gchar *str,
					 gboolean case_sens)
{
	GtkTextView *text = GTK_TEXT_VIEW(textview->text);

	return gtkut_text_view_search_string_backward(text, str, case_sens);
}

void textview_scroll_one_line(TextView *textview, gboolean up)
{
	GtkTextView *text = GTK_TEXT_VIEW(textview->text);
	GtkAdjustment *vadj = gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(text));

	gtkutils_scroll_one_line(GTK_WIDGET(text), vadj, up);
}

gboolean textview_scroll_page(TextView *textview, gboolean up)
{
	GtkTextView *text = GTK_TEXT_VIEW(textview->text);
	GtkAdjustment *vadj = gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(text));

	return gtkutils_scroll_page(GTK_WIDGET(text), vadj, up);
}

void textview_scroll_max(TextView *textview, gboolean up)
{
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview->text));
	GtkTextIter iter;
	
	if (up) {
		gtk_text_buffer_get_start_iter(buffer, &iter);
		gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(textview->text),
						&iter, 0.0, TRUE, 0.0, 1.0);
	
	} else {
		gtk_text_buffer_get_end_iter(buffer, &iter);
		gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(textview->text),
						&iter, 0.0, TRUE, 0.0, 0.0);
	}
}

#define KEY_PRESS_EVENT_STOP() \
	g_signal_stop_emission_by_name(G_OBJECT(widget), \
				       "key_press_event");

static gint textview_key_pressed(GtkWidget *widget, GdkEventKey *event,
				 TextView *textview)
{
	GdkWindow *window = NULL;
	SummaryView *summaryview = NULL;
	MessageView *messageview = textview->messageview;
	gboolean mod_pressed;

	if (!event) return FALSE;
	if (messageview->mainwin)
		summaryview = messageview->mainwin->summaryview;

	switch (event->keyval) {
	case GDK_KEY_Tab:
	case GDK_KEY_Left:
	case GDK_KEY_Up:
	case GDK_KEY_Right:
	case GDK_KEY_Down:
	case GDK_KEY_Control_L:
	case GDK_KEY_Control_R:
		return FALSE;
	case GDK_KEY_Home:
	case GDK_KEY_End:
		textview_scroll_max(textview,(event->keyval == GDK_KEY_Home));
		return TRUE;
	case GDK_KEY_space:
		mod_pressed = ((event->state & (GDK_SHIFT_MASK|GDK_MOD1_MASK)) != 0);
		if (!mimeview_scroll_page(messageview->mimeview, mod_pressed) &&
				summaryview != NULL) {
			if (mod_pressed)
				summary_select_prev_unread(summaryview);
			else
				summary_select_next_unread(summaryview);
		}
		break;
	case GDK_KEY_Page_Down:
		mimeview_scroll_page(messageview->mimeview, FALSE);
		break;
	case GDK_KEY_Page_Up:
	case GDK_KEY_BackSpace:
		mimeview_scroll_page(messageview->mimeview, TRUE);
		break;
	case GDK_KEY_Return:
	case GDK_KEY_KP_Enter:
		mimeview_scroll_one_line
			(messageview->mimeview, (event->state &
				    (GDK_SHIFT_MASK|GDK_MOD1_MASK)) != 0);
		break;
	case GDK_KEY_Delete:
		if (summaryview)
			summary_pass_key_press_event(summaryview, event);
		break;
	default:
		if (messageview->mainwin) {
			window = gtk_widget_get_window(messageview->mainwin->window);
			if (summaryview &&
			    event->window != window) {
				GdkEventKey tmpev = *event;

				tmpev.window = window;
				KEY_PRESS_EVENT_STOP();
				gtk_widget_event(messageview->mainwin->window,
						 (GdkEvent *)&tmpev);
			}
		}
		break;
	}

	return TRUE;
}

static gboolean textview_motion_notify(GtkWidget *widget,
				       GdkEventMotion *event,
				       TextView *textview)
{
	if (textview->loading)
		return FALSE;
	textview_uri_update(textview, event->x, event->y);

	return FALSE;
}

static gboolean textview_leave_notify(GtkWidget *widget,
				      GdkEventCrossing *event,
				      TextView *textview)
{
	if (textview->loading)
		return FALSE;
	textview_uri_update(textview, -1, -1);

	return FALSE;
}

static gboolean textview_visibility_notify(GtkWidget *widget,
					   GdkEventVisibility *event,
					   TextView *textview)
{
	gint wx, wy;
	GdkWindow *window;
	GdkDisplay *display;
	GdkSeat *seat;

	if (textview->loading)
		return FALSE;

	window = gtk_text_view_get_window(GTK_TEXT_VIEW(widget),
					  GTK_TEXT_WINDOW_TEXT);

	/* check if occurred for the text window part */
	if (window != event->window)
		return FALSE;
	
	display = gdk_window_get_display(window);
	seat = gdk_display_get_default_seat(display);
	gdk_device_get_position(gdk_seat_get_pointer(seat),
			NULL, &wx, &wy);
	textview_uri_update(textview, wx, wy);

	return FALSE;
}

void textview_cursor_wait(TextView *textview)
{
	GdkWindow *window = gtk_text_view_get_window(
			GTK_TEXT_VIEW(textview->text),
			GTK_TEXT_WINDOW_TEXT);
	textview_set_cursor(window, watch_cursor);
}

void textview_cursor_normal(TextView *textview)
{
	GdkWindow *window = gtk_text_view_get_window(
			GTK_TEXT_VIEW(textview->text),
			GTK_TEXT_WINDOW_TEXT);
	textview_set_cursor(window, text_cursor);
}

static void textview_uri_update(TextView *textview, gint x, gint y)
{
	GtkTextBuffer *buffer;
	GtkTextIter start_iter, end_iter;
	ClickableText *uri = NULL;
	
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview->text));

	if (x != -1 && y != -1) {
		gint bx, by;
		GtkTextIter iter;
		GSList *tags;
		GSList *cur;
		
		gtk_text_view_window_to_buffer_coords(GTK_TEXT_VIEW(textview->text), 
						      GTK_TEXT_WINDOW_WIDGET,
						      x, y, &bx, &by);
		gtk_text_view_get_iter_at_location(GTK_TEXT_VIEW(textview->text),
						   &iter, bx, by);

		tags = gtk_text_iter_get_tags(&iter);
		for (cur = tags; cur != NULL; cur = cur->next) {
			GtkTextTag *tag = cur->data;
			char *name;

			g_object_get(G_OBJECT(tag), "name", &name, NULL);

			if ((!strcmp(name, "link") || !strcmp(name, "hlink"))
			    && textview_get_uri_range(textview, &iter, tag,
						      &start_iter, &end_iter)) {

				uri = textview_get_uri_from_range(textview,
								  &iter, tag,
								  &start_iter,
								  &end_iter);
			}
			g_free(name);
			if (uri)
				break;
		}
		g_slist_free(tags);
	}
	
	if (uri != textview->uri_hover) {
		GdkWindow *window;

		if (textview->uri_hover)
			gtk_text_buffer_remove_tag_by_name(buffer,
							   "link-hover",
							   &textview->uri_hover_start_iter,
							   &textview->uri_hover_end_iter);
		    
		textview->uri_hover = uri;
		if (uri) {
			textview->uri_hover_start_iter = start_iter;
			textview->uri_hover_end_iter = end_iter;
		}
		
		window = gtk_text_view_get_window(GTK_TEXT_VIEW(textview->text),
						  GTK_TEXT_WINDOW_TEXT);
		if (textview->messageview->mainwin->cursor_count == 0) {
			textview_set_cursor(window, uri ? hand_cursor : text_cursor);
		} else {
			textview_set_cursor(window, watch_cursor);
		}

		TEXTVIEW_STATUSBAR_POP(textview);

		if (uri) {
			if (!uri->is_quote)
				gtk_text_buffer_apply_tag_by_name(buffer,
							  "link-hover",
							  &start_iter,
							  &end_iter);
			TEXTVIEW_STATUSBAR_PUSH(textview, uri->uri);
		}
	}
}

static void textview_set_font_zoom(TextView *textview)
{
	PangoFontDescription *font;
	gint size;

	/* do nothing if no zoom level has been set */
	if (textview_font_size_percent == TEXTVIEW_FONT_SIZE_UNSET)
		return;

	font = pango_font_description_from_string
						(prefs_common.textfont);
	cm_return_if_fail(font);

	if (textview_font_size_default == TEXTVIEW_FONT_SIZE_UNSET)
		textview_font_size_default = pango_font_description_get_size(font);

	size = textview_font_size_default + ( textview_font_size_default / 100 * textview_font_size_percent );

	pango_font_description_set_size(font, size);
	gtk_widget_override_font(textview->text, font);
	pango_font_description_free(font);
}

static void textview_zoom(GtkWidget *widget, gboolean zoom_in)
{
	PangoContext *pctx;
	PangoFontDescription *font;
	gint size;

	pctx = gtk_widget_get_pango_context(widget);
	font = pango_context_get_font_description(pctx);
	size = pango_font_description_get_size(font);

	/* save the default font size first time before zooming */
	if (textview_font_size_default == TEXTVIEW_FONT_SIZE_UNSET)
		textview_font_size_default = size;

	if (textview_font_size_percent == TEXTVIEW_FONT_SIZE_UNSET)
		textview_font_size_percent = 0;

	if (zoom_in) {
		if ((textview_font_size_percent + TEXTVIEW_FONT_SIZE_STEP ) <= TEXTVIEW_FONT_SIZE_MAX)
			textview_font_size_percent += TEXTVIEW_FONT_SIZE_STEP;
	} else {
		if ((textview_font_size_percent - TEXTVIEW_FONT_SIZE_STEP ) >= TEXTVIEW_FONT_SIZE_MIN)
			textview_font_size_percent -= TEXTVIEW_FONT_SIZE_STEP;
	}
	size = textview_font_size_default + ( textview_font_size_default / 100 * textview_font_size_percent );

	pango_font_description_set_size(font, size);
	gtk_widget_override_font(widget, font);
	gtk_widget_show(widget);
}

static void textview_zoom_in(GtkWidget *widget, gpointer data)
{
	textview_zoom(GTK_WIDGET(data), TRUE);
}

static void textview_zoom_out(GtkWidget *widget, gpointer data)
{
	textview_zoom(GTK_WIDGET(data), FALSE);
}

static void textview_zoom_reset(GtkWidget *widget, gpointer data)
{
	PangoContext *pctx;
	PangoFontDescription *font;
	gint size;

	pctx = gtk_widget_get_pango_context(GTK_WIDGET(data));
	font = pango_context_get_font_description(pctx);

	/* reset and save the value for current session */
	if (textview_font_size_default == TEXTVIEW_FONT_SIZE_UNSET || textview_font_size_percent == TEXTVIEW_FONT_SIZE_UNSET) 
		return;

	textview_font_size_percent = 0;
	size = textview_font_size_default + ( textview_font_size_default / 100 * textview_font_size_percent );

	pango_font_description_set_size(font, size);
	gtk_widget_override_font(GTK_WIDGET(data), font);
	gtk_widget_show(GTK_WIDGET(data));
}

static void textview_populate_popup(GtkTextView* textview,
						 GtkMenu *menu,
						 gpointer user_data)
{
	GtkWidget *menuitem;
	
	cm_return_if_fail(menu != NULL);
	cm_return_if_fail(GTK_IS_MENU_SHELL(menu));

	menuitem = gtk_separator_menu_item_new();
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	gtk_widget_show(menuitem);

	menuitem = gtk_menu_item_new_with_mnemonic(_("Zoom _In"));
	g_signal_connect(G_OBJECT(menuitem), "activate",
			 G_CALLBACK(textview_zoom_in), textview);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	gtk_widget_set_sensitive(menuitem,
			textview_font_size_percent == TEXTVIEW_FONT_SIZE_UNSET ||
			((textview_font_size_percent + TEXTVIEW_FONT_SIZE_STEP ) <= TEXTVIEW_FONT_SIZE_MAX));
	gtk_widget_show(menuitem);

	menuitem = gtk_menu_item_new_with_mnemonic(_("Zoom _Out"));
	g_signal_connect(G_OBJECT(menuitem), "activate",
			 G_CALLBACK(textview_zoom_out), textview);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	gtk_widget_set_sensitive(menuitem,
			textview_font_size_percent == TEXTVIEW_FONT_SIZE_UNSET ||
			((textview_font_size_percent - TEXTVIEW_FONT_SIZE_STEP ) >= TEXTVIEW_FONT_SIZE_MIN));
	gtk_widget_show(menuitem);

	menuitem = gtk_menu_item_new_with_mnemonic(_("Reset _zoom"));
	g_signal_connect(G_OBJECT(menuitem), "activate",
			 G_CALLBACK(textview_zoom_reset), textview);
	gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
	gtk_widget_set_sensitive(menuitem,
			(textview_font_size_percent != TEXTVIEW_FONT_SIZE_UNSET && textview_font_size_percent != 0));
	gtk_widget_show(menuitem);
}

static gboolean textview_scrolled(GtkWidget *widget,
				      GdkEvent *_event,
				      gpointer user_data)
{
	GdkEventScroll *event = (GdkEventScroll *)_event;

	/* Ctrl+mousewheel changes font size */
	if (event->state & GDK_CONTROL_MASK || event->state & GDK_SHIFT_MASK) {

		if (event->direction == GDK_SCROLL_UP) {
			textview_zoom(widget, TRUE);
		} else if (event->direction == GDK_SCROLL_DOWN) {
			textview_zoom(widget, FALSE);
		} else {
			gdouble x, y;

			if ((event->direction == GDK_SCROLL_SMOOTH) &&
					gdk_event_get_scroll_deltas((GdkEvent *)event, &x, &y)) {
				if (y < 0)
					textview_zoom(widget, TRUE);
				else
					if (y >0)
						textview_zoom(widget, FALSE);
			} else
				return FALSE; /* Scrolling left or right */
		}
		return TRUE;
	}

	return FALSE;
}

static gboolean textview_get_uri_range(TextView *textview,
				       GtkTextIter *iter,
				       GtkTextTag *tag,
				       GtkTextIter *start_iter,
				       GtkTextIter *end_iter)
{
	return get_tag_range(iter, tag, start_iter, end_iter);
}

static ClickableText *textview_get_uri_from_range(TextView *textview,
					      GtkTextIter *iter,
					      GtkTextTag *tag,
					      GtkTextIter *start_iter,
					      GtkTextIter *end_iter)
{
	gint start_pos, end_pos, cur_pos;
	ClickableText *uri = NULL;
	GSList *cur;

	start_pos = gtk_text_iter_get_offset(start_iter);
	end_pos = gtk_text_iter_get_offset(end_iter);
	cur_pos = gtk_text_iter_get_offset(iter);

	for (cur = textview->uri_list; cur != NULL; cur = cur->next) {
		ClickableText *uri_ = (ClickableText *)cur->data;
		if (start_pos == uri_->start &&
		    end_pos ==  uri_->end) {
			uri = uri_;
			break;
		} 
	}
	for (cur = textview->uri_list; uri == NULL && cur != NULL; cur = cur->next) {
		ClickableText *uri_ = (ClickableText *)cur->data;
		if (start_pos == uri_->start ||
			   end_pos == uri_->end) {
			/* in case of contiguous links, textview_get_uri_range
			 * returns a broader range (start of 1st link to end
			 * of last link).
			 * In that case, correct link is the one covering
			 * current iter.
			 */
			if (uri_->start <= cur_pos && cur_pos <= uri_->end) {
				uri = uri_;
				break;
			}
		} 
	}

	return uri;
}

static ClickableText *textview_get_uri(TextView *textview,
				   GtkTextIter *iter,
				   GtkTextTag *tag)
{
	GtkTextIter start_iter, end_iter;
	ClickableText *uri = NULL;

	if (textview_get_uri_range(textview, iter, tag, &start_iter,
				   &end_iter))
		uri = textview_get_uri_from_range(textview, iter, tag,
						  &start_iter, &end_iter);

	return uri;
}

static void textview_shift_uris_after(TextView *textview, GSList *start_list, gint start, gint shift)
{
	GSList *cur;
	if (!start_list)
		start_list = textview->uri_list;

	for (cur = start_list; cur; cur = cur->next) {
		ClickableText *uri = (ClickableText *)cur->data;
		if (uri->start <= start)
			continue;
		uri->start += shift;
		uri->end += shift;
	}
}

static void textview_remove_uris_in(TextView *textview, gint start, gint end)
{
	GSList *cur;
	for (cur = textview->uri_list; cur; ) {
		ClickableText *uri = (ClickableText *)cur->data;
		if (uri->start > start && uri->end < end) {
			cur = cur->next;
			textview->uri_list = g_slist_remove(textview->uri_list, uri);
			g_free(uri->uri);
			g_free(uri->filename);
			if (uri->is_quote) {
				g_free(uri->fg_color);
				g_free(uri->data); 
				/* (only free data in quotes uris) */
			}
			g_free(uri);
		} else {
			cur = cur->next;
		}
		
	}
}

static void textview_toggle_quote(TextView *textview, GSList *start_list, ClickableText *uri, gboolean expand_only)
{
	GtkTextIter start, end;
	GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview->text));
	
	if (!uri->is_quote)
		return;
	
	if (uri->q_expanded && expand_only)
		return;

	gtk_text_buffer_get_iter_at_offset(buffer, &start, uri->start);
	gtk_text_buffer_get_iter_at_offset(buffer, &end,   uri->end);
	if (textview->uri_hover)
		gtk_text_buffer_remove_tag_by_name(buffer,
						   "link-hover",
						   &textview->uri_hover_start_iter,
						   &textview->uri_hover_end_iter);
	textview->uri_hover = NULL;
	gtk_text_buffer_remove_tag_by_name(buffer,
					   "qlink",
					   &start,
					   &end);
	/* when shifting URIs start and end, we have to do it per-UTF8-char
	 * so use g_utf8_strlen(). OTOH, when inserting in the text buffer, 
	 * we have to pass a number of bytes, so use strlen(). disturbing. */
	 
	if (!uri->q_expanded) {
		gtk_text_buffer_get_iter_at_offset(buffer, &start, uri->start);
		gtk_text_buffer_get_iter_at_offset(buffer, &end,   uri->end);
		textview_shift_uris_after(textview, start_list, uri->start, 
			g_utf8_strlen((gchar *)uri->data, -1)-strlen(" [...]\n"));
		gtk_text_buffer_delete(buffer, &start, &end);
		gtk_text_buffer_get_iter_at_offset(buffer, &start, uri->start);
		gtk_text_buffer_insert_with_tags_by_name
				(buffer, &start, (gchar *)uri->data, 
				 strlen((gchar *)uri->data)-1,
				 "qlink", (gchar *)uri->fg_color, NULL);
		uri->end = gtk_text_iter_get_offset(&start);
		textview_make_clickable_parts_later(textview,
					  uri->start, uri->end);
		uri->q_expanded = TRUE;
	} else {
		gtk_text_buffer_get_iter_at_offset(buffer, &start, uri->start);
		gtk_text_buffer_get_iter_at_offset(buffer, &end,   uri->end);
		textview_remove_uris_in(textview, uri->start, uri->end);
		textview_shift_uris_after(textview, start_list, uri->start, 
			strlen(" [...]\n")-g_utf8_strlen((gchar *)uri->data, -1));
		gtk_text_buffer_delete(buffer, &start, &end);
		gtk_text_buffer_get_iter_at_offset(buffer, &start, uri->start);
		gtk_text_buffer_insert_with_tags_by_name
				(buffer, &start, " [...]", -1,
				 "qlink", (gchar *)uri->fg_color, NULL);
		uri->end = gtk_text_iter_get_offset(&start);
		uri->q_expanded = FALSE;
	}
	if (textview->messageview->mainwin->cursor_count == 0) {
		textview_cursor_normal(textview);
	} else {
		textview_cursor_wait(textview);
	}
}
static gboolean textview_uri_button_pressed(GtkTextTag *tag, GObject *obj,
					    GdkEvent *event, GtkTextIter *iter,
					    TextView *textview)
{
	GdkEventButton *bevent;
	ClickableText *uri = NULL;
	char *tagname;
	gboolean qlink = FALSE;

	if (!event)
		return FALSE;

	if (event->type != GDK_BUTTON_PRESS && event->type != GDK_2BUTTON_PRESS
		&& event->type != GDK_MOTION_NOTIFY)
		return FALSE;

	uri = textview_get_uri(textview, iter, tag);
	if (!uri)
		return FALSE;

	g_object_get(G_OBJECT(tag), "name", &tagname, NULL);
	
	if (!strcmp(tagname, "qlink"))
		qlink = TRUE;

	g_free(tagname);
	
	bevent = (GdkEventButton *) event;
	
	/* doubleclick: open compose / add address / browser */
	if (qlink && event->type == GDK_BUTTON_PRESS && bevent->button != 1) {
		/* pass rightclick through */
		return FALSE;
	} else if ((event->type == (qlink ? GDK_2BUTTON_PRESS:GDK_BUTTON_PRESS) && bevent->button == 1) ||
		bevent->button == 2 || bevent->button == 3) {
		if (uri->filename && !g_ascii_strncasecmp(uri->filename, "cm://", 5)) {
			MimeView *mimeview = 
				(textview->messageview)?
					textview->messageview->mimeview:NULL;
			if (mimeview && bevent->button == 1) {
				mimeview_handle_cmd(mimeview, uri->filename, NULL, uri->data);
			} else if (mimeview && bevent->button == 2 && 
				!g_ascii_strcasecmp(uri->filename, "cm://select_attachment")) {
				mimeview_handle_cmd(mimeview, "cm://open_attachment", NULL, uri->data);
			} else if (mimeview && bevent->button == 3 && 
				!g_ascii_strcasecmp(uri->filename, "cm://select_attachment")) {
				mimeview_handle_cmd(mimeview, "cm://menu_attachment", bevent, uri->data);
			} 
			return TRUE;
		} else if (qlink && bevent->button == 1) {
			if (prefs_common.hide_quoted) {
				textview_toggle_quote(textview, NULL, uri, FALSE);
				return TRUE;
			} else
				return FALSE;
		} else if (!g_ascii_strncasecmp(uri->uri, "mailto:", 7)) {
			if (bevent->button == 3) {
				g_object_set_data(
					G_OBJECT(textview->mail_popup_menu),
					"menu_button", uri);
				gtk_menu_popup_at_pointer(GTK_MENU(textview->mail_popup_menu), NULL);
			} else {
				PrefsAccount *account = NULL;
				FolderItem   *folder_item = NULL;
				Compose *compose;
				
				if (textview->messageview && textview->messageview->msginfo &&
				    textview->messageview->msginfo->folder) {
					

					folder_item = textview->messageview->msginfo->folder;
					if (folder_item->prefs && folder_item->prefs->enable_default_account)
						account = account_find_from_id(folder_item->prefs->default_account);
					if (!account)
						account = account_find_from_item(folder_item);
				}
				compose = compose_new_with_folderitem(account,
								folder_item, uri->uri + 7);
				compose_check_for_email_account(compose);
			}
			return TRUE;
		} else if (g_ascii_strncasecmp(uri->uri, "file:", 5)) {
			if (bevent->button == 1 &&
			    textview_uri_security_check(textview, uri, FALSE) == TRUE) 
					open_uri(uri->uri,
						 prefs_common_get_uri_cmd());
			else if (bevent->button == 3 && !qlink) {
				g_object_set_data(
					G_OBJECT(textview->link_popup_menu),
					"menu_button", uri);
				gtk_menu_popup_at_pointer(GTK_MENU(textview->link_popup_menu), NULL);
			}
			return TRUE;
		} else {
			if (bevent->button == 3 && !qlink) {
				g_object_set_data(
					G_OBJECT(textview->file_popup_menu),
					"menu_button", uri);
				gtk_menu_popup_at_pointer(GTK_MENU(textview->file_popup_menu), NULL);
				return TRUE;
			}
		}
	}

	return FALSE;
}

gchar *textview_get_visible_uri		(TextView 	*textview, 
					 ClickableText 	*uri)
{
	GtkTextBuffer *buffer;
	GtkTextIter start, end;

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview->text));

	gtk_text_buffer_get_iter_at_offset(buffer, &start, uri->start);
	gtk_text_buffer_get_iter_at_offset(buffer, &end,   uri->end);

	return gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
}

/*!
 *\brief    Check to see if a web URL has been disguised as a different
 *          URL (possible with HTML email).
 *
 *\param    uri The uri to check
 *
 *\param    textview The TextView the URL is contained in
 *
 *\return   gboolean TRUE if the URL is ok, or if the user chose to open
 *          it anyway, otherwise FALSE          
 */
gboolean textview_uri_security_check(TextView *textview, ClickableText *uri, gboolean copied)
{
	gchar *visible_str;
	gboolean retval = TRUE;

	if (is_uri_string(uri->uri) == FALSE)
		return FALSE;

	visible_str = textview_get_visible_uri(textview, uri);
	if (visible_str == NULL)
		return TRUE;

	g_strstrip(visible_str);

	if (strcmp(visible_str, uri->uri) != 0 && is_uri_string(visible_str)) {
		gchar *uri_path;
		gchar *visible_uri_path;

		uri_path = get_uri_path(uri->uri);
		visible_uri_path = get_uri_path(visible_str);
		if (path_cmp(uri_path, visible_uri_path) != 0)
			retval = FALSE;
	}

	if (retval == FALSE) {
		gchar *open_or_cp;
		gchar *open_or_cp_btn;
		gchar *msg;
		AlertValue aval;
		
		open_or_cp = copied? _("Copy it anyway?") : _("Open it anyway?");
		open_or_cp_btn = copied? _("Co_py URL") : _("_Open URL");

		msg = g_markup_printf_escaped("%s\n\n"
						"<b>%s</b> %s\n\n"
						"<b>%s</b> %s\n\n"
						"%s",
						_("The real URL is different from the displayed URL."),
						_("Displayed URL:"), visible_str,
						_("Real URL:"), uri->uri,
						open_or_cp);
		aval = alertpanel_full(_("Phishing attempt warning"), msg,
				       NULL, _("_Cancel"), NULL, open_or_cp_btn, NULL, NULL,
				       ALERTFOCUS_FIRST, FALSE, NULL, ALERT_WARNING);
		g_free(msg);
		if (aval == G_ALERTALTERNATE)
			retval = TRUE;
	}
	if (strlen(uri->uri) > get_uri_len(uri->uri))
		retval = FALSE;

	g_free(visible_str);

	return retval;
}

static void textview_uri_list_remove_all(GSList *uri_list)
{
	GSList *cur;

	for (cur = uri_list; cur != NULL; cur = cur->next) {
		if (cur->data) {
			g_free(((ClickableText *)cur->data)->uri);
			g_free(((ClickableText *)cur->data)->filename);
			if (((ClickableText *)cur->data)->is_quote) {
				g_free(((ClickableText *)cur->data)->fg_color);
				g_free(((ClickableText *)cur->data)->data); 
				/* (only free data in quotes uris) */
			}
			g_free(cur->data);
		}
	}

	g_slist_free(uri_list);
}

static void open_uri_cb (GtkAction *action, TextView *textview)
{
	ClickableText *uri = g_object_get_data(G_OBJECT(textview->link_popup_menu),
					   "menu_button");
	const gchar *raw_url = g_object_get_data(G_OBJECT(textview->link_popup_menu),
					   "raw_url");

	if (uri) {
		if (textview_uri_security_check(textview, uri, FALSE) == TRUE) 
			open_uri(uri->uri,
				 prefs_common_get_uri_cmd());
		g_object_set_data(G_OBJECT(textview->link_popup_menu), "menu_button",
				  NULL);
	}
	if (raw_url) {
		open_uri(raw_url, prefs_common_get_uri_cmd());
		g_object_set_data(G_OBJECT(textview->link_popup_menu), "raw_url",
				  NULL);
	}
}

static void copy_uri_cb	(GtkAction *action, TextView *textview)
{
	ClickableText *uri = g_object_get_data(G_OBJECT(textview->link_popup_menu),
					   "menu_button");
	const gchar *raw_url =  g_object_get_data(G_OBJECT(textview->link_popup_menu),
					   "raw_url");
	if (uri) {
		if (textview_uri_security_check(textview, uri, TRUE) == TRUE) {
			gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_PRIMARY), uri->uri, -1);
			gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), uri->uri, -1);
			g_object_set_data(G_OBJECT(textview->link_popup_menu), "menu_button", NULL);
		}
	}
	if (raw_url) {
		gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_PRIMARY), raw_url, -1);
		gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), raw_url, -1);
		g_object_set_data(G_OBJECT(textview->link_popup_menu), "raw_url", NULL);
	}
}

static void add_uri_to_addrbook_cb (GtkAction *action, TextView *textview)
{
	gchar *fromname, *fromaddress;
	ClickableText *uri = g_object_get_data(G_OBJECT(textview->mail_popup_menu),
					   "menu_button");
	AvatarRender *avatarr = NULL;
	GdkPixbuf *picture = NULL;
	gboolean use_picture = FALSE;

	if (uri == NULL)
		return;

	/* extract url */
	fromaddress = g_strdup(uri->uri + 7);
	
	if (textview->messageview->msginfo &&
	   !g_strcmp0(fromaddress, textview->messageview->msginfo->from))
		use_picture = TRUE;

	fromname = procheader_get_fromname(fromaddress);
	extract_address(fromaddress);

	if (use_picture) {
		avatarr = avatars_avatarrender_new(textview->messageview->msginfo);
		hooks_invoke(AVATAR_IMAGE_RENDER_HOOKLIST, avatarr);
	}

	if (avatarr && avatarr->image) {
		picture = gtk_image_get_pixbuf(GTK_IMAGE(avatarr->image));
	}
	if (avatarr) {
		avatars_avatarrender_free(avatarr);
	}

#ifndef USE_ALT_ADDRBOOK
	addressbook_add_contact( fromname, fromaddress, NULL, picture);
#else
	if (addressadd_selection(fromname, fromaddress, NULL, picture)) {
		debug_print( "addressbook_add_contact - added\n" );
	}
#endif

	g_free(fromaddress);
	g_free(fromname);
}

static void reply_to_uri_cb (GtkAction *action, TextView *textview)
{
	ClickableText *uri = g_object_get_data(G_OBJECT(textview->mail_popup_menu),
					   "menu_button");
	if (!textview->messageview || !uri)
		return;

	compose_reply_to_address (textview->messageview,
				  textview->messageview->msginfo, uri->uri+7);
}

static void mail_to_uri_cb (GtkAction *action, TextView *textview)
{
	PrefsAccount *account = NULL;
	Compose *compose;
	ClickableText *uri = g_object_get_data(G_OBJECT(textview->mail_popup_menu),
					   "menu_button");
	if (uri == NULL)
		return;

	if (textview->messageview && textview->messageview->msginfo &&
	    textview->messageview->msginfo->folder) {
		FolderItem   *folder_item;

		folder_item = textview->messageview->msginfo->folder;
		if (folder_item->prefs && folder_item->prefs->enable_default_account)
			account = account_find_from_id(folder_item->prefs->default_account);
		
		compose = compose_new_with_folderitem(account, folder_item, uri->uri+7);
	} else {
		compose = compose_new(account, uri->uri + 7, NULL);
	}
	compose_check_for_email_account(compose);
}

static void copy_mail_to_uri_cb	(GtkAction *action, TextView *textview)
{
	ClickableText *uri = g_object_get_data(G_OBJECT(textview->mail_popup_menu),
					   "menu_button");
	if (uri == NULL)
		return;

	gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_PRIMARY), uri->uri +7, -1);
	gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), uri->uri +7, -1);
	g_object_set_data(G_OBJECT(textview->mail_popup_menu), "menu_button",
			  NULL);
}

void textview_get_selection_offsets(TextView *textview, gint *sel_start, gint *sel_end)
{
		GtkTextView *text = GTK_TEXT_VIEW(textview->text);
		GtkTextBuffer *buffer = gtk_text_view_get_buffer(text);
		GtkTextIter start, end;
		if (gtk_text_buffer_get_selection_bounds(buffer, &start, &end)) {
			if (sel_start)
				*sel_start = gtk_text_iter_get_offset(&start);
			if (sel_end)
				*sel_end = gtk_text_iter_get_offset(&end);
		} else {
			if (sel_start)
				*sel_start = -1;
			if (sel_end)
				*sel_end = -1;
		}
}
