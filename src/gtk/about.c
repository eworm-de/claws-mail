/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2005 Hiroyuki Yamamoto
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
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkwindow.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkhseparator.h>
#include <gtk/gtkscrolledwindow.h>
#include <gtk/gtktext.h>
#include <gtk/gtkbutton.h>
#if HAVE_SYS_UTSNAME_H
#  include <sys/utsname.h>
#endif

#include "about.h"
#include "gtkutils.h"
#include "stock_pixmap.h"
#include "prefs_common.h"
#include "utils.h"
#include "version.h"
#include "authors.h"
#include "codeconv.h"
#include "menu.h"
#include "textview.h"

static GtkWidget *window;
static gchar* uri_hover = NULL;
static GtkTextIter uri_hover_start_iter;
static GtkTextIter uri_hover_end_iter;
static GdkCursor *hand_cursor = NULL;
static GdkCursor *text_cursor = NULL;

static void about_create(void);
static gboolean key_pressed(GtkWidget *widget, GdkEventKey *event);
static void about_uri_clicked(GtkButton *button, gpointer data);
static gboolean about_textview_uri_clicked(GtkTextTag *tag, GObject *obj,
					GdkEvent *event, GtkTextIter *iter,
					GtkWidget *textview);
static void about_open_link_cb(GtkWidget *widget, guint action, void *data);
static void about_copy_link_cb(GtkWidget *widget, guint action, void *data);
static gboolean about_textview_motion_notify(GtkWidget *widget,
					GdkEventMotion *event,
					GtkWidget *textview);
static gboolean about_textview_leave_notify(GtkWidget *widget,
					GdkEventCrossing *event,
					GtkWidget *textview);
static void about_textview_uri_update(GtkWidget *textview, gint x, gint y);

static GtkItemFactoryEntry textview_link_popup_entries[] = 
{
	{N_("/_Open with Web browser"),	NULL, about_open_link_cb, 0, NULL},
	{N_("/Copy this _link"),	NULL, about_copy_link_cb, 0, NULL},
};

static GtkWidget *link_popupmenu;


void about_show(void)
{
	if (!window)
		about_create();
	else
		gtk_window_present(GTK_WINDOW(window));
}

static void about_create(void)
{
	GtkWidget *vbox1;
	GtkWidget *table;
	GtkWidget *table2;
	GtkWidget *image;	
 	GtkWidget *vbox2;
	GtkWidget *label;
	GtkWidget *button;
	GtkWidget *scrolledwin;
	GtkWidget *notebook;
	GtkStyle *style;
	GdkColormap *cmap;
	GdkColor uri_color[2] = {{0, 0, 0, 0xffff}, {0, 0xffff, 0, 0}};
	gboolean success[2];
	char *markup;
	GtkWidget *text;
	GtkWidget *confirm_area;
	GtkWidget *close_button;
	GtkTextBuffer *buffer;
	GtkTextIter iter;
	GtkTextTag *tag;

#if HAVE_SYS_UTSNAME_H
	struct utsname utsbuf;
#endif
	gchar buf[1024];
	gint i;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(window), _("About Sylpheed-Claws"));
	gtk_container_set_border_width(GTK_CONTAINER(window), 8);
	gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
	gtk_widget_set_size_request(window, -1, -1);
	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(gtk_widget_hide_on_delete), NULL);
	g_signal_connect(G_OBJECT(window), "key_press_event",
			 G_CALLBACK(key_pressed), NULL);
	gtk_widget_realize(window);

	vbox1 = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(window), vbox1);

	table = gtk_table_new (1, 2, FALSE);
	gtk_box_pack_start(GTK_BOX(vbox1), table, FALSE, FALSE, 0);

	image = stock_pixmap_widget(window, STOCK_PIXMAP_SYLPHEED_LOGO);
	gtk_table_attach(GTK_TABLE(table), image, 0, 1, 0, 1,
			 (GtkAttachOptions) (GTK_SHRINK),
			 (GtkAttachOptions) (GTK_SHRINK), 8, 0);

	vbox2 = gtk_vbox_new (TRUE, 0);
	gtk_table_attach(GTK_TABLE(table), vbox2, 1, 2, 0, 1,
			 (GtkAttachOptions) (GTK_EXPAND),
			 (GtkAttachOptions) (GTK_FILL), 0, 0);

	label = gtk_label_new("");
	gtk_label_set_selectable(GTK_LABEL(label), TRUE);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
	gtk_box_pack_start(GTK_BOX(vbox2), label, FALSE, FALSE, 0);
	markup = g_markup_printf_escaped
		("<span weight=\"bold\" size=\"xx-large\">Sylpheed-Claws</span>\nversion %s",
		 VERSION);
	gtk_label_set_markup(GTK_LABEL(label), markup);
	g_free(markup);

	button = gtk_button_new_with_label(" "HOMEPAGE_URI" ");
	gtk_box_pack_start(GTK_BOX(vbox2), button, FALSE, FALSE, 0);
	gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(about_uri_clicked), NULL);
	buf[0] = ' ';
	for (i = 1; i <= strlen(HOMEPAGE_URI); i++) buf[i] = '_';
	strcpy(buf + i, " ");
	gtk_label_set_pattern(GTK_LABEL(GTK_BIN(button)->child), buf);
	cmap = gdk_drawable_get_colormap(window->window);
	gdk_colormap_alloc_colors(cmap, uri_color, 2, FALSE, TRUE, success);
	if (success[0] == TRUE && success[1] == TRUE) {
		gtk_widget_ensure_style(GTK_BIN(button)->child);
		style = gtk_style_copy
			(gtk_widget_get_style(GTK_BIN(button)->child));
		style->fg[GTK_STATE_NORMAL]   = uri_color[0];
		style->fg[GTK_STATE_ACTIVE]   = uri_color[1];
		style->fg[GTK_STATE_PRELIGHT] = uri_color[0];
		gtk_widget_set_style(GTK_BIN(button)->child, style);
	} else
		g_warning("about_create(): color allocation failed.\n");

#if HAVE_SYS_UTSNAME_H
	uname(&utsbuf);
	g_snprintf(buf, sizeof(buf),
		   _("GTK+ %d.%d.%d / GLib %d.%d.%d\n"
		   "Operating System: %s %s (%s)"),
		   gtk_major_version, gtk_minor_version, gtk_micro_version,
		   glib_major_version, glib_minor_version, glib_micro_version,
		   utsbuf.sysname, utsbuf.release, utsbuf.machine);
#elif defined(G_OS_WIN32)
	g_snprintf(buf, sizeof(buf),
		   _("GTK+ %d.%d.%d / GLib %d.%d.%d\n"
		   "Operating System: %s"),
		   gtk_major_version, gtk_minor_version, gtk_micro_version,
		   glib_major_version, glib_minor_version, glib_micro_version,
		   "Win32");
#else
	g_snprintf(buf, sizeof(buf),
		   _("GTK+ %d.%d.%d / GLib %d.%d.%d\n"
		   "Operating System: unknown"),
		   gtk_major_version, gtk_minor_version, gtk_micro_version,
		   glib_major_version, glib_minor_version, glib_micro_version);
#endif

	label = gtk_label_new(buf);
	gtk_label_set_selectable(GTK_LABEL(label), TRUE);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_box_pack_start(GTK_BOX(vbox2), label, FALSE, FALSE, 0);

	g_snprintf(buf, sizeof(buf),
		   _("Compiled-in features:\n%s"),
#if USE_THREADS
		   " gthread"
#endif
#if INET6
		   " IPv6"
#endif
#if HAVE_ICONV
		   " iconv"
#endif
#if HAVE_LIBCOMPFACE
		   " compface"
#endif
#if USE_OPENSSL
		   " OpenSSL"
#endif
#if USE_LDAP
		   " LDAP"
#endif
#if USE_JPILOT
		   " JPilot"
#endif
#if USE_ASPELL
		   " GNU/aspell"
#endif
#if HAVE_LIBETPAN
		   " libetpan"
#endif
#if USE_GNOMEPRINT
		   " libgnomeprint"
#endif
	"");

	label = gtk_label_new(buf);
	gtk_label_set_selectable(GTK_LABEL(label), TRUE);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
	gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	gtk_box_pack_start(GTK_BOX(vbox2), label, FALSE, FALSE, 0);

	table2 = gtk_table_new (2, 3, FALSE);
	gtk_box_pack_start(GTK_BOX(vbox1), table2, FALSE, FALSE, 0);

	label = gtk_label_new
		("Copyright (C) 1999-2005 Hiroyuki Yamamoto <hiro-y@kcn.ne.jp>\n"
		 "and the Sylpheed-Claws team");
	gtk_label_set_selectable(GTK_LABEL(label), TRUE);
	gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
	gtk_table_attach(GTK_TABLE(table2), label, 0, 1, 0, 1,
			 (GtkAttachOptions) (GTK_EXPAND),
			 (GtkAttachOptions) (GTK_SHRINK), 0, 6);

	notebook = gtk_notebook_new();
	gtk_widget_set_size_request(notebook, -1, 200);
	gtk_widget_show(notebook);

	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
				       GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledwin),
					    GTK_SHADOW_IN);

	text = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(text), FALSE);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text), GTK_WRAP_WORD);
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(text), 6);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(text), 6);
	gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(text), FALSE);
	gtk_container_add(GTK_CONTAINER(scrolledwin), text);
	gtk_widget_add_events(text, GDK_LEAVE_NOTIFY_MASK);

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text));
	gtk_text_buffer_get_iter_at_offset(buffer, &iter, 0);

	/* textview link style (based upon main prefs) */
	gtkut_convert_int_to_gdk_color(prefs_common.uri_col,
				(GdkColor*)&uri_color);
	tag = gtk_text_buffer_create_tag(buffer, "link",
				"foreground-gdk", &uri_color,
				"wrap-mode", GTK_WRAP_NONE,
				NULL);
	gtk_text_buffer_create_tag(buffer, "link-hover",
				"foreground-gdk", &uri_color,
				"underline", PANGO_UNDERLINE_SINGLE,
				NULL);

	gtk_text_buffer_insert(buffer, &iter, _(
				"Sylpheed-Claws is a lightweight, fast and "
				"highly-configurable e-mail client.\n\n"
				"For further information visit the Sylpheed-"
				"Claws website:\n"), -1);
	gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, HOMEPAGE_URI, -1,
				"link", NULL);
	gtk_text_buffer_insert(buffer, &iter, _("\n\n"
				"Sylpheed-Claws is free software released "
				"under the GPL license. If you wish to donate "
				"to the Sylpheed-Claws project you can do "
				"so at:\n"), -1);
	gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, DONATE_URI, -1,
				"link", NULL);
	gtk_text_buffer_insert(buffer, &iter, _("\n"), -1);

	g_signal_connect(G_OBJECT(tag), "event",
				G_CALLBACK(about_textview_uri_clicked), text);
	g_signal_connect(G_OBJECT(text), "motion-notify-event",
				G_CALLBACK(about_textview_motion_notify), text);
	g_signal_connect(G_OBJECT(text), "leave-notify-event",
				G_CALLBACK(about_textview_leave_notify), text);

	gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
				scrolledwin,
				gtk_label_new(_("Info")));

	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
				       GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledwin),
					    GTK_SHADOW_IN);

	text = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(text), FALSE);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text), GTK_WRAP_WORD);
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(text), 6);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(text), 6);
	gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(text), FALSE);
	gtk_container_add(GTK_CONTAINER(scrolledwin), text);
	gtk_widget_add_events(text, GDK_LEAVE_NOTIFY_MASK);

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text));
	gtk_text_buffer_get_iter_at_offset(buffer, &iter, 0);

	/* init formatting tag: indentation  for list items */
	gtk_text_buffer_create_tag(buffer, "indented-list-item",
				"indent", 24,
				NULL);
	gtk_text_buffer_create_tag(buffer, "underlined-list-title",
				"underline", PANGO_UNDERLINE_SINGLE,
				NULL);

	gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, (_("The Sylpheed-Claws Team\n")), -1,
			"underlined-list-title", NULL);

	for (i = 0; TEAM_LIST[i] != NULL; i++) {
		if (g_utf8_validate(TEAM_LIST[i], -1, NULL))
			gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, TEAM_LIST[i], -1,
					"indented-list-item", NULL);
		else {
			gchar *conv = conv_codeset_strdup(TEAM_LIST[i], CS_ISO_8859_1, CS_UTF_8);
			if (conv)
				gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, conv, -1,
						"indented-list-item", NULL);
			g_free(conv);
		}
		gtk_text_buffer_insert(buffer, &iter, "\n", 1);
	}

	gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, (_("\nPrevious team members\n")), -1,
			"underlined-list-title", NULL);

	for (i = 0; EX_TEAM_LIST[i] != NULL; i++) {
		if (g_utf8_validate(EX_TEAM_LIST[i], -1, NULL))
			gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, EX_TEAM_LIST[i], -1,
					"indented-list-item", NULL);
		else {
			gchar *conv = conv_codeset_strdup(EX_TEAM_LIST[i], CS_ISO_8859_1, CS_UTF_8);
			if (conv)
				gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, conv, -1,
						"indented-list-item", NULL);
			g_free(conv);
		}
		gtk_text_buffer_insert(buffer, &iter, "\n", 1);
	}

	gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, (_("\nThe translation team\n")), -1,
			"underlined-list-title", NULL);

	for (i = 0; TRANS_TEAM_LIST[i] != NULL; i++) {
		if (g_utf8_validate(TRANS_TEAM_LIST[i], -1, NULL))
			gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, TRANS_TEAM_LIST[i], -1,
					"indented-list-item", NULL);
		else {
			gchar *conv = conv_codeset_strdup(TRANS_TEAM_LIST[i], CS_ISO_8859_1, CS_UTF_8);
			if (conv)
				gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, conv, -1,
						"indented-list-item", NULL);
			g_free(conv);
		}
		gtk_text_buffer_insert(buffer, &iter, "\n", 1);
	}

	gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, (_("\nDocumentation team\n")), -1,
			"underlined-list-title", NULL);

	for (i = 0; DOC_TEAM_LIST[i] != NULL; i++) {
		if (g_utf8_validate(DOC_TEAM_LIST[i], -1, NULL))
			gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, DOC_TEAM_LIST[i], -1,
					"indented-list-item", NULL);
		else {
			gchar *conv = conv_codeset_strdup(DOC_TEAM_LIST[i], CS_ISO_8859_1, CS_UTF_8);
			if (conv)
				gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, conv, -1,
						"undeindented-list-itemitle", NULL);
			g_free(conv);
		}
		gtk_text_buffer_insert(buffer, &iter, "\n", 1);
	}

	gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, (_("\nLogo\n")), -1,
			"underlined-list-title", NULL);

	for (i = 0; LOGO_LIST[i] != NULL; i++) {
		if (g_utf8_validate(LOGO_LIST[i], -1, NULL))
			gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, LOGO_LIST[i], -1,
					"indented-list-item", NULL);
		else {
			gchar *conv = conv_codeset_strdup(LOGO_LIST[i], CS_ISO_8859_1, CS_UTF_8);
			if (conv)
				gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, conv, -1,
						"indented-list-item", NULL);
			g_free(conv);
		}
		gtk_text_buffer_insert(buffer, &iter, "\n", 1);
	}

	gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, (_("\nIcons\n")), -1,
			"underlined-list-title", NULL);

	for (i = 0; ICONS_LIST[i] != NULL; i++) {
		if (g_utf8_validate(ICONS_LIST[i], -1, NULL))
			gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, ICONS_LIST[i], -1,
					"indented-list-item", NULL);
		else {
			gchar *conv = conv_codeset_strdup(ICONS_LIST[i], CS_ISO_8859_1, CS_UTF_8);
			if (conv)
				gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, conv, -1,
						"indented-list-item", NULL);
			g_free(conv);
		}
		gtk_text_buffer_insert(buffer, &iter, "\n", 1);
	}

	gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, (_("\nContributors\n")), -1,
			"underlined-list-title", NULL);

	for (i = 0; CONTRIBS_LIST[i] != NULL; i++) {
		if (g_utf8_validate(CONTRIBS_LIST[i], -1, NULL))
			gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, CONTRIBS_LIST[i], -1,
					"indented-list-item", NULL);
		else {
			gchar *conv = conv_codeset_strdup(CONTRIBS_LIST[i], CS_ISO_8859_1, CS_UTF_8);
			if (conv)
				gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, conv, -1,
						"indented-list-item", NULL);
			g_free(conv);
		}
		gtk_text_buffer_insert(buffer, &iter, "\n", 1);
	}

	gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
				 scrolledwin,
				 gtk_label_new(_("Authors")));

	scrolledwin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
				       GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledwin),
					    GTK_SHADOW_IN);

	text = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(text), FALSE);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text), GTK_WRAP_WORD);
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(text), 6);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(text), 6);
	gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(text), FALSE);
	gtk_container_add(GTK_CONTAINER(scrolledwin), text);

	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text));
	gtk_text_buffer_get_iter_at_offset(buffer, &iter, 0);

	gtk_text_buffer_insert(buffer, &iter,
		_("This program is free software; you can redistribute it and/or modify "
		  "it under the terms of the GNU General Public License as published by "
		  "the Free Software Foundation; either version 2, or (at your option) "
		  "any later version.\n\n"), -1);

	gtk_text_buffer_insert(buffer, &iter,
		_("This program is distributed in the hope that it will be useful, "
		  "but WITHOUT ANY WARRANTY; without even the implied warranty of "
		  "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. "
		  "See the GNU General Public License for more details.\n\n"), -1);

	gtk_text_buffer_insert(buffer, &iter,
		_("You should have received a copy of the GNU General Public License "
		  "along with this program; if not, write to the Free Software "
		  "Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, "
		  "MA 02110-1301, USA.\n\n"), -1);
#ifdef USE_OPENSSL
	tag = gtk_text_buffer_create_tag(buffer, "link",
		"foreground-gdk", &uri_color,
		NULL);
	gtk_text_buffer_create_tag(buffer, "link-hover",
		"foreground-gdk", &uri_color,
		"underline", PANGO_UNDERLINE_SINGLE,
		NULL);

	gtk_text_buffer_insert(buffer, &iter,
		_("This product includes software developed by the OpenSSL Project "
		  "for use in the OpenSSL Toolkit ("), -1);
	gtk_text_buffer_insert_with_tags_by_name(buffer, &iter, "http://www.openssl.org/", -1,
		"link", NULL);
	gtk_text_buffer_insert(buffer, &iter, _(").\n"), -1);

	g_signal_connect(G_OBJECT(tag), "event",
				G_CALLBACK(about_textview_uri_clicked), text);
	g_signal_connect(G_OBJECT(text), "motion-notify-event",
			 G_CALLBACK(about_textview_motion_notify), text);
	g_signal_connect(G_OBJECT(text), "leave-notify-event",
				G_CALLBACK(about_textview_leave_notify), text);
#endif

	gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
				 scrolledwin,
				 gtk_label_new(_("License")));

	gtk_box_pack_start(GTK_BOX(vbox1), notebook, TRUE, TRUE, 0);

	gtkut_stock_button_set_create(&confirm_area, &close_button, GTK_STOCK_CLOSE,
				      NULL, NULL, NULL, NULL);
	gtk_box_pack_end(GTK_BOX(vbox1), confirm_area, FALSE, FALSE, 4);
	gtk_widget_grab_default(close_button);
	gtk_widget_grab_focus(close_button);
	g_signal_connect_closure
		(G_OBJECT(close_button), "clicked",
		 g_cclosure_new_swap(G_CALLBACK(gtk_widget_hide_on_delete),
				     window, NULL), FALSE);

	gtk_widget_show_all(window);
}

static gboolean key_pressed(GtkWidget *widget, GdkEventKey *event)
{
	if (event && event->keyval == GDK_Escape)
		gtk_widget_hide(window);
	return FALSE;
}

static void about_uri_clicked(GtkButton *button, gpointer data)
{
	open_uri(HOMEPAGE_URI, prefs_common.uri_cmd);
}

static gboolean about_textview_uri_clicked(GtkTextTag *tag, GObject *obj,
					GdkEvent *event, GtkTextIter *iter,
					GtkWidget *textview)
{
	GtkTextIter start_iter, end_iter;
	GdkEventButton *bevent;
	gchar *link = NULL;

	if (!event || !tag) {
		return FALSE;
	}

	if (event->type != GDK_BUTTON_PRESS && event->type != GDK_2BUTTON_PRESS
		&& event->type != GDK_BUTTON_RELEASE) {
		return FALSE;
	}

	/* get link text from tag */
	if (get_tag_range(iter, tag, &start_iter,
				   &end_iter) == FALSE) {
		return FALSE;
	}
	link = gtk_text_iter_get_text(&start_iter, &end_iter);
	if (link == NULL) {
		return FALSE;
	}

	bevent = (GdkEventButton *) event;
	if (bevent->button == 1 && event->type == GDK_BUTTON_RELEASE) {
		GtkTextBuffer *buffer;

		/* we shouldn't follow a link if the user has selected something */
		buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
		gtk_text_buffer_get_selection_bounds(buffer, &start_iter, &end_iter);
		if (gtk_text_iter_get_offset(&start_iter) != gtk_text_iter_get_offset(&end_iter)) {
			return FALSE;
		}
		/* open link and do *not* return TRUE so that
		   further gtk processing of the signal is done */
		open_uri(link, prefs_common.uri_cmd);

	} else {
		if (bevent->button == 3 && event->type == GDK_BUTTON_PRESS) {
			GtkItemFactory *link_popupfactory;
			gint n_entries;

			n_entries = sizeof(textview_link_popup_entries) /
					sizeof(textview_link_popup_entries[0]);
			link_popupmenu = menu_create_items(
							textview_link_popup_entries, n_entries,
				    		"<UriPopupMenu>", &link_popupfactory,
				    		textview);

			g_object_set_data(
					G_OBJECT(link_popupmenu),
					"menu_button", link);
			gtk_menu_popup(GTK_MENU(link_popupmenu), 
					NULL, NULL, NULL, NULL, 
					bevent->button, bevent->time);

			return TRUE;
		}
	}
	return FALSE;
}

static void about_open_link_cb(GtkWidget *widget, guint action, void *data)
{
	gchar *link = g_object_get_data(G_OBJECT(link_popupmenu),
					   "menu_button");

	if (link == NULL) {
		return;
	}

	open_uri(link, prefs_common.uri_cmd);
	g_object_set_data(G_OBJECT(link_popupmenu), "menu_button",
			  NULL);
}

static void about_copy_link_cb(GtkWidget *widget, guint action, void *data)
{
	gchar *link = g_object_get_data(G_OBJECT(link_popupmenu),
					   "menu_button");

	if (link == NULL) {
		return;
	}

	gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_PRIMARY), link, -1);
	gtk_clipboard_set_text(gtk_clipboard_get(GDK_SELECTION_CLIPBOARD), link, -1);
	g_object_set_data(G_OBJECT(link_popupmenu), "menu_button", NULL);
}

static gboolean about_textview_motion_notify(GtkWidget *widget,
					GdkEventMotion *event,
					GtkWidget *textview)
{
	about_textview_uri_update(textview, event->x, event->y);
	gdk_window_get_pointer(widget->window, NULL, NULL, NULL);

	return FALSE;
}

static gboolean about_textview_leave_notify(GtkWidget *widget,
					GdkEventCrossing *event,
					GtkWidget *textview)
{
	about_textview_uri_update(textview, -1, -1);

	return FALSE;
}

static void about_textview_uri_update(GtkWidget *textview, gint x, gint y)
{
	GtkTextBuffer *buffer;
	GtkTextIter start_iter, end_iter;
	gchar *uri = NULL;
	gboolean same;
	
	buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));

	if (x != -1 && y != -1) {
		gint bx, by;
		GtkTextIter iter;
		GSList *tags;
		GSList *cur;
	    
		gtk_text_view_window_to_buffer_coords(GTK_TEXT_VIEW(textview), 
				GTK_TEXT_WINDOW_WIDGET,
				x, y, &bx, &by);
		gtk_text_view_get_iter_at_location(GTK_TEXT_VIEW(textview),
				&iter, bx, by);

		tags = gtk_text_iter_get_tags(&iter);
		for (cur = tags; cur != NULL; cur = cur->next) {
			GtkTextTag *tag = cur->data;
			char *name;

			g_object_get(G_OBJECT(tag), "name", &name, NULL);
			if (strcmp(name, "link") == 0
			    && get_tag_range(&iter, tag, &start_iter, &end_iter)) {
				uri = gtk_text_iter_get_text(&start_iter, &end_iter);
			}
			g_free(name);

			if (uri) {
				break;
			}
		}
		g_slist_free(tags);
	}

	/* compare previous hovered link and this one
	   (here links must be unique in text buffer otherwise RemoteURI structures should be
	   used as in textview.c) */
	same = (uri != NULL && uri_hover != NULL
		&& strcmp((char*)uri, (char*)uri_hover) == 0);

	if (same == FALSE) {
		GdkWindow *window;

		if (uri_hover) {
			gtk_text_buffer_remove_tag_by_name(buffer,
					"link-hover",
					&uri_hover_start_iter,
					&uri_hover_end_iter);
		}
		    
		uri_hover = uri;
		if (uri) {
			uri_hover_start_iter = start_iter;
			uri_hover_end_iter = end_iter;

			gtk_text_buffer_apply_tag_by_name(buffer,
					"link-hover",
					&start_iter,
					&end_iter);
		}
		
		window = gtk_text_view_get_window(GTK_TEXT_VIEW(textview),
						GTK_TEXT_WINDOW_TEXT);
		if (!hand_cursor)
			hand_cursor = gdk_cursor_new(GDK_HAND2);
		if (!text_cursor)
			text_cursor = gdk_cursor_new(GDK_XTERM);
		gdk_window_set_cursor(window, uri ? hand_cursor : text_cursor);
	}
}
