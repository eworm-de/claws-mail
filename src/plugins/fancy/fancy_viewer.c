/*
 * Claws Mail -- A GTK+ based, lightweight, and fast e-mail client
 * == Fancy Plugin ==
 * Copyright(C) 1999-2013 the Claws Mail Team
 * This file Copyright (C) 2009-2013 Salvatore De Paolis
 * <iwkse@claws-mail.org> and the Claws Mail Team
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write tothe Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#include "claws-features.h"
#endif

#include <fancy_viewer.h>
#include <fancy_prefs.h>
#include <alertpanel.h>

#include <printing.h>


static void
load_start_cb (WebKitWebView *view, gint progress, FancyViewer *viewer);

static void
load_finished_cb (WebKitWebView *view, gint progress, FancyViewer *viewer);

static void
over_link_cb (WebKitWebView *view, const gchar *wtf, const gchar *link,
	      FancyViewer *viewer, void *wtfa);

static void
load_progress_cb (WebKitWebView *view, gint progress, FancyViewer *viewer);

static WebKitNavigationResponse
navigation_requested_cb (WebKitWebView *view, WebKitWebFrame *frame,
			 WebKitNetworkRequest *netreq, FancyViewer *viewer);

static MimeViewerFactory fancy_viewer_factory;

static gboolean
fancy_text_search(MimeViewer *_viewer, gboolean backward, const gchar *str,
		  gboolean case_sens);

static void
viewer_menu_handler(GtkWidget *menuitem, FancyViewer *viewer);

static gint keypress_events_cb (GtkWidget *widget, GdkEventKey *event,
								FancyViewer *viewer);
static void zoom_in_cb(GtkWidget *widget, GdkEvent *ev, FancyViewer *viewer);
static void zoom_out_cb(GtkWidget *widget, GdkEvent *ev, FancyViewer *viewer);
static gboolean fancy_prefs_cb(GtkWidget *widget, GdkEventButton *ev, FancyViewer *viewer);
static void zoom_100_cb(GtkWidget *widget, GdkEvent *ev, FancyViewer *viewer);
static void open_in_browser_cb(GtkWidget *widget, FancyViewer *viewer);
static void fancy_create_popup_prefs_menu(FancyViewer *viewer);
static void fancy_show_notice(FancyViewer *viewer, const gchar *message);
static size_t download_file_curl_write_cb(void *buffer, size_t size,
					  size_t nmemb, void *data);
static void *download_file_curl (void *data);
static void download_file_cb(GtkWidget *widget, FancyViewer *viewer);

#if !WEBKIT_CHECK_VERSION (1,5,1)
gchar* webkit_web_view_get_selected_text(WebKitWebView* web_view);
#endif

/*------*/
static GtkWidget *fancy_get_widget(MimeViewer *_viewer)
{
	FancyViewer *viewer = (FancyViewer *) _viewer;
	debug_print("fancy_get_widget: %p\n", viewer->vbox);

	return GTK_WIDGET(viewer->vbox);
}

static void fancy_apply_prefs(FancyViewer *viewer)
{
	g_object_set(viewer->settings,
		"auto-load-images", viewer->override_prefs_images,
		"enable-scripts", viewer->override_prefs_scripts,
		"enable-plugins", viewer->override_prefs_plugins,
		"enable-java-applet", viewer->override_prefs_java,
#ifdef G_OS_WIN32
		"default-font-family", "Arial",
		"cursive-font-family", "Comic Sans MS",
		"fantasy-font-family", "Comic Sans MS",
		"monospace-font-family", "Courier New",
		"sans-serif-font-family", "Arial",
		"serif-font-family", "Times New Roman",
#endif
		NULL); 
	webkit_web_view_set_settings(viewer->view, viewer->settings);
}

static void fancy_auto_load_images_activated(GtkCheckMenuItem *item, FancyViewer *viewer) {
	viewer->override_prefs_images = gtk_check_menu_item_get_active(item);
	fancy_apply_prefs(viewer);
	webkit_web_view_reload (viewer->view);
}

static void fancy_enable_remote_content_activated(GtkCheckMenuItem *item, FancyViewer *viewer) {
	viewer->override_prefs_remote_content = gtk_check_menu_item_get_active(item);
	fancy_apply_prefs(viewer);
	webkit_web_view_reload (viewer->view);
}

static void fancy_enable_scripts_activated(GtkCheckMenuItem *item, FancyViewer *viewer) {
	viewer->override_prefs_scripts = gtk_check_menu_item_get_active(item);
	fancy_apply_prefs(viewer);
	webkit_web_view_reload (viewer->view);
}

static void fancy_enable_plugins_activated(GtkCheckMenuItem *item, FancyViewer *viewer) {
	viewer->override_prefs_plugins = gtk_check_menu_item_get_active(item);
	fancy_apply_prefs(viewer);
	webkit_web_view_reload (viewer->view);
}

static void fancy_enable_java_activated(GtkCheckMenuItem *item, FancyViewer *viewer) {
	viewer->override_prefs_java = gtk_check_menu_item_get_active(item);
	fancy_apply_prefs(viewer);
	webkit_web_view_reload (viewer->view);
}

static void fancy_open_external_activated(GtkCheckMenuItem *item, FancyViewer *viewer) {
	viewer->override_prefs_external = gtk_check_menu_item_get_active(item);
	fancy_apply_prefs(viewer);
}

static void fancy_set_defaults(FancyViewer *viewer)
{
	viewer->override_prefs_remote_content = fancy_prefs.enable_remote_content;
	viewer->override_prefs_external = fancy_prefs.open_external;
	viewer->override_prefs_images = fancy_prefs.enable_images;
	viewer->override_prefs_scripts = fancy_prefs.enable_scripts;
	viewer->override_prefs_plugins = fancy_prefs.enable_plugins;
	viewer->override_prefs_java = fancy_prefs.enable_java;

	g_signal_handlers_block_by_func(G_OBJECT(viewer->enable_images),
		fancy_auto_load_images_activated, viewer);
	g_signal_handlers_block_by_func(G_OBJECT(viewer->enable_remote_content),
		fancy_enable_remote_content_activated, viewer);
	g_signal_handlers_block_by_func(G_OBJECT(viewer->enable_scripts),
		fancy_enable_scripts_activated, viewer);
	g_signal_handlers_block_by_func(G_OBJECT(viewer->enable_plugins),
		fancy_enable_plugins_activated, viewer);
	g_signal_handlers_block_by_func(G_OBJECT(viewer->enable_java),
		fancy_enable_java_activated, viewer);
	g_signal_handlers_block_by_func(G_OBJECT(viewer->open_external),
		fancy_open_external_activated, viewer);

	gtk_check_menu_item_set_active(
		GTK_CHECK_MENU_ITEM(viewer->enable_images),
		viewer->override_prefs_images);
	gtk_check_menu_item_set_active(
		GTK_CHECK_MENU_ITEM(viewer->enable_scripts),
		viewer->override_prefs_scripts);
	gtk_check_menu_item_set_active(
		GTK_CHECK_MENU_ITEM(viewer->enable_plugins),
		viewer->override_prefs_plugins);
	gtk_check_menu_item_set_active(
		GTK_CHECK_MENU_ITEM(viewer->enable_java),
		viewer->override_prefs_java);
	gtk_check_menu_item_set_active(
		GTK_CHECK_MENU_ITEM(viewer->enable_remote_content),
		viewer->override_prefs_remote_content);
	gtk_check_menu_item_set_active(
		GTK_CHECK_MENU_ITEM(viewer->open_external),
		viewer->override_prefs_external);

	g_signal_handlers_unblock_by_func(G_OBJECT(viewer->enable_images),
		fancy_auto_load_images_activated, viewer);
	g_signal_handlers_unblock_by_func(G_OBJECT(viewer->enable_remote_content),
		fancy_enable_remote_content_activated, viewer);
	g_signal_handlers_unblock_by_func(G_OBJECT(viewer->enable_scripts),
		fancy_enable_scripts_activated, viewer);
	g_signal_handlers_unblock_by_func(G_OBJECT(viewer->enable_plugins),
		fancy_enable_plugins_activated, viewer);
	g_signal_handlers_unblock_by_func(G_OBJECT(viewer->enable_java),
		fancy_enable_java_activated, viewer);
	g_signal_handlers_unblock_by_func(G_OBJECT(viewer->open_external),
		fancy_open_external_activated, viewer);

	fancy_apply_prefs(viewer);
}

static void fancy_load_uri(FancyViewer *viewer, const gchar *uri)
{
#if WEBKIT_CHECK_VERSION(1,1,1)
	webkit_web_view_load_uri(viewer->view, uri);
#else
	webkit_web_view_open(viewer->view, uri);
#endif
}

static gboolean fancy_show_mimepart_real(MimeViewer *_viewer)
{
	FancyViewer *viewer = (FancyViewer *) _viewer;
	MessageView *messageview = ((MimeViewer *)viewer)->mimeview
					? ((MimeViewer *)viewer)->mimeview->messageview
					: NULL;
	MimeInfo *partinfo = viewer->to_load;

	messageview->updating = TRUE;

	if (viewer->filename != NULL) {
		g_unlink(viewer->filename);
		g_free(viewer->filename);
		viewer->filename = NULL;
	}

	if (messageview) {
		NoticeView *noticeview = messageview->noticeview;
		noticeview_hide(noticeview);
	}
	if (partinfo)
		viewer->filename = procmime_get_tmp_file_name(partinfo);
	debug_print("filename: %s\n", viewer->filename);
	if (!viewer->filename) {
		return FALSE;
	}
	if (procmime_get_part(viewer->filename, partinfo) < 0) {
		g_free(viewer->filename);
		viewer->filename = NULL;
	}
	else {
		const gchar *charset = NULL;
		if (_viewer && _viewer->mimeview &&
			_viewer->mimeview->messageview->forced_charset)
			charset = _viewer->mimeview->messageview->forced_charset;
		else
			charset = procmime_mimeinfo_get_parameter(partinfo, "charset");
		if (!charset)
			charset = conv_get_locale_charset_str();
		debug_print("using %s charset\n", charset);
		g_object_set(viewer->settings, "default-encoding", charset, NULL);
		gchar *tmp = g_filename_to_uri(viewer->filename, NULL, NULL);
		debug_print("zoom_level: %i\n", fancy_prefs.zoom_level);
		webkit_web_view_set_zoom_level(viewer->view, (fancy_prefs.zoom_level / 100.0));

		fancy_set_defaults(viewer);
		fancy_load_uri(viewer, tmp);

		g_free(tmp);
	}
	viewer->loading = FALSE;
	return FALSE;
}
static void fancy_show_notice(FancyViewer *viewer, const gchar *message)
{
	gtk_label_set_text(GTK_LABEL(viewer->l_link), message);
}
static gint fancy_show_mimepart_prepare(MimeViewer *_viewer)
{
	FancyViewer *viewer = (FancyViewer *) _viewer;

	if (viewer->tag > 0) {
		gtk_timeout_remove(viewer->tag);
		viewer->tag = -1;
		if (viewer->loading) {
			viewer->stop_previous = TRUE;
		}
	}

	viewer->tag = g_timeout_add(5, (GSourceFunc)fancy_show_mimepart_real, viewer);
	return FALSE;
}

static void fancy_show_mimepart(MimeViewer *_viewer, const gchar *infile,
								MimeInfo *partinfo)
{
	FancyViewer *viewer = (FancyViewer *) _viewer;
	viewer->to_load = partinfo;
	viewer->loading = TRUE;
	g_timeout_add(5, (GtkFunction)fancy_show_mimepart_prepare, viewer);
}

static void fancy_print(MimeViewer *_viewer)
{
	FancyViewer *viewer = (FancyViewer *) _viewer;
	GtkPrintOperationResult res;
	GError *error = NULL;
	GtkPrintOperation *op;

	gtk_widget_realize(GTK_WIDGET(viewer->view));

	while (viewer->loading)
		claws_do_idle();

	op = gtk_print_operation_new();

	/* Config for printing */
	gtk_print_operation_set_print_settings(op, printing_get_settings());
	gtk_print_operation_set_default_page_setup(op, printing_get_page_setup());
#if GTK_CHECK_VERSION(2,18,0)
        /* enable Page Size and Orientation in the print dialog */
	gtk_print_operation_set_embed_page_setup(op, TRUE);
#endif

	/* Start printing process */
	res = webkit_web_frame_print_full(webkit_web_view_get_main_frame(viewer->view),
					  op, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG,
					  &error);

	if (res == GTK_PRINT_OPERATION_RESULT_ERROR) {
		gtk_print_operation_get_error(op, &error);
		debug_print("Error printing message: %s\n",
			    error ? error->message : "no details");
	} else if (res == GTK_PRINT_OPERATION_RESULT_APPLY) {
		/* store settings for next printing session */
		printing_store_settings(gtk_print_operation_get_print_settings(op));
	}
}

static gchar *fancy_get_selection (MimeViewer *_viewer)
{
	debug_print("fancy_get_selection\n");
	FancyViewer *viewer = (FancyViewer *) _viewer;
#if WEBKIT_CHECK_VERSION(1,5,1)
	viewer->doc = webkit_web_view_get_dom_document(WEBKIT_WEB_VIEW(viewer->view));
	viewer->window = webkit_dom_document_get_default_view (viewer->doc);
	viewer->selection = webkit_dom_dom_window_get_selection (viewer->window);
	if (viewer->selection == NULL)
		return NULL;
	viewer->range = webkit_dom_dom_selection_get_range_at(viewer->selection, 0, NULL);
	if (viewer->range == NULL)
		return NULL;
	gchar *sel = webkit_dom_range_get_text (viewer->range);
#else
	gchar *sel = webkit_web_view_get_selected_text(viewer->view);
#endif
	if (!viewer->view || strlen(sel) == 0) {
		g_free(sel);
		return NULL;
	}
	return sel;
}

static void fancy_clear_viewer(MimeViewer *_viewer)
{
	FancyViewer *viewer = (FancyViewer *) _viewer;
	GtkAdjustment *vadj;
	viewer->cur_link = NULL;
	fancy_set_defaults(viewer);

	fancy_load_uri(viewer, "about:blank");

	debug_print("fancy_clear_viewer\n");
	fancy_prefs.zoom_level = webkit_web_view_get_zoom_level(viewer->view) * 100;
	viewer->to_load = NULL;
	vadj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(viewer->scrollwin));
	gtk_adjustment_set_value(vadj, 0.0);
	g_signal_emit_by_name(G_OBJECT(vadj), "value-changed", 0);
}

static void fancy_destroy_viewer(MimeViewer *_viewer)
{
	FancyViewer *viewer = (FancyViewer *) _viewer;
	fancy_prefs.zoom_level = webkit_web_view_get_zoom_level(viewer->view) * 100;
	debug_print("fancy_destroy_viewer\n");
	g_free(viewer->filename);
	g_free(viewer);
}

static WebKitNavigationResponse
navigation_requested_cb(WebKitWebView *view, WebKitWebFrame *frame,
						WebKitNetworkRequest *netreq, FancyViewer *viewer)
{
	const gchar *uri = webkit_network_request_get_uri(netreq);

	debug_print("navigation requested to %s\n", uri);

	if (!strncmp(uri, "mailto:", 7)) {
		compose_new(NULL, uri + 7, NULL);
		return WEBKIT_NAVIGATION_RESPONSE_IGNORE;
	} else if (!strncmp(uri, "file://", 7)) {
		return WEBKIT_NAVIGATION_RESPONSE_ACCEPT;
	} else if (viewer->override_prefs_remote_content)
		return WEBKIT_NAVIGATION_RESPONSE_ACCEPT;
	else {
		fancy_show_notice(viewer, _("Remote content loading is disabled."));
		return WEBKIT_NAVIGATION_RESPONSE_IGNORE;
	}
}

static void resource_request_starting_cb(WebKitWebView		*view,
					 WebKitWebFrame		*frame,
					 WebKitWebResource	*resource,
					 WebKitNetworkRequest	*request,
					 WebKitNetworkResponse	*response,
					 FancyViewer		*viewer)
{
	const gchar *uri = webkit_network_request_get_uri(request);
	gchar *filename;
	gchar *image;
	gint err;
	MimeInfo *partinfo = viewer->to_load;
	
	filename = viewer->filename;
	if ((!g_ascii_strncasecmp(uri, "cid:", 4)) || (!g_ascii_strncasecmp(uri, "mid:", 4))) {
		image = g_strconcat("<", uri + 4, ">", NULL);
		while ((partinfo = procmime_mimeinfo_next(partinfo)) != NULL) {
			if (!g_ascii_strcasecmp(image, partinfo->id)) {
				filename = procmime_get_tmp_file_name(partinfo);
				if (!filename) {
					g_free(image);
					return;
				}
				if ((err = procmime_get_part(filename, partinfo)) < 0)
					alertpanel_error(_("Couldn't save the part of multipart message: %s"),
										strerror(-err));
				gchar *file_uri = g_filename_to_uri(filename, NULL, NULL);
				webkit_network_request_set_uri(request, file_uri);
				g_free(file_uri);
				g_free(filename);
				break;
			}
		}
		g_free(image);
	}
	
	/* refresh URI that may have changed */
	uri = webkit_network_request_get_uri(request);
	if (!viewer->override_prefs_remote_content
	    && strncmp(uri, "file://", 7) && strncmp(uri, "data:", 5)) {
		debug_print("Preventing load of %s\n", uri);
		webkit_network_request_set_uri(request, "about:blank");
	}
	else
		debug_print("Starting request of %d %s\n", strlen(uri), uri);
}

static gboolean fancy_text_search(MimeViewer *_viewer, gboolean backward,
				  const gchar *str, gboolean case_sens)
{
	return webkit_web_view_search_text(((FancyViewer*)_viewer)->view, str,
					   case_sens, !backward, TRUE);
}

static gboolean fancy_prefs_cb(GtkWidget *widget, GdkEventButton *ev, FancyViewer *viewer)
{
	if ((ev->button == 1) && (ev->type == GDK_BUTTON_PRESS)) {
		gtk_menu_popup(GTK_MENU(viewer->fancy_prefs_menu), NULL, NULL, NULL, NULL,
				ev->button, ev->time);
		return TRUE;
	}
	return FALSE;
}

static void fancy_create_popup_prefs_menu(FancyViewer *viewer) {
	GtkWidget *enable_images;
	GtkWidget *enable_remote_content;
	GtkWidget *enable_scripts;
	GtkWidget *enable_plugins;
	GtkWidget *enable_java;
	GtkWidget *open_external;

	enable_images = gtk_check_menu_item_new_with_label(_("Load images"));

	enable_remote_content = gtk_check_menu_item_new_with_label(_("Enable remote content"));

	enable_scripts = gtk_check_menu_item_new_with_label(_("Enable Javascript"));

	enable_plugins = gtk_check_menu_item_new_with_label(_("Enable Plugins"));

	enable_java = gtk_check_menu_item_new_with_label(_("Enable Java"));

	open_external = gtk_check_menu_item_new_with_label(_("Open links with external browser"));

	gtk_menu_shell_append(GTK_MENU_SHELL(viewer->fancy_prefs_menu), enable_images);
	gtk_menu_shell_append(GTK_MENU_SHELL(viewer->fancy_prefs_menu), enable_remote_content);
	gtk_menu_shell_append(GTK_MENU_SHELL(viewer->fancy_prefs_menu), enable_scripts);
	gtk_menu_shell_append(GTK_MENU_SHELL(viewer->fancy_prefs_menu), enable_plugins);
	gtk_menu_shell_append(GTK_MENU_SHELL(viewer->fancy_prefs_menu), enable_java);
	gtk_menu_shell_append(GTK_MENU_SHELL(viewer->fancy_prefs_menu), open_external);

	gtk_menu_attach_to_widget(GTK_MENU(viewer->fancy_prefs_menu), viewer->ev_fancy_prefs, NULL);
	gtk_widget_show_all(viewer->fancy_prefs_menu);

	viewer->enable_images = enable_images;
	viewer->enable_scripts = enable_scripts;
	viewer->enable_plugins = enable_plugins;
	viewer->enable_java = enable_java;
	viewer->enable_remote_content = enable_remote_content;
	viewer->open_external = open_external;

	/* Set sensitivity according to preferences and overrides */

	g_signal_connect(G_OBJECT(enable_images), "toggled",
			 G_CALLBACK (fancy_auto_load_images_activated), viewer);
	g_signal_connect(G_OBJECT(enable_remote_content), "toggled",
			 G_CALLBACK (fancy_enable_remote_content_activated), viewer);
	g_signal_connect(G_OBJECT(enable_scripts), "toggled",
			 G_CALLBACK (fancy_enable_scripts_activated), viewer);
	g_signal_connect(G_OBJECT(enable_plugins), "toggled",
			 G_CALLBACK (fancy_enable_plugins_activated), viewer);
	g_signal_connect(G_OBJECT(enable_java), "toggled",
			 G_CALLBACK (fancy_enable_java_activated), viewer);
	g_signal_connect(G_OBJECT(open_external), "toggled",
			 G_CALLBACK (fancy_open_external_activated), viewer);

	fancy_apply_prefs(viewer);
}

static gboolean fancy_scroll_page(MimeViewer *_viewer, gboolean up)
{
	FancyViewer *viewer = (FancyViewer *)_viewer;
	GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment(
					GTK_SCROLLED_WINDOW(viewer->scrollwin));

	if (viewer->view == NULL)
		return FALSE;

	return gtkutils_scroll_page(GTK_WIDGET(viewer->view), vadj, up);
}

static void fancy_scroll_one_line(MimeViewer *_viewer, gboolean up)
{
	FancyViewer *viewer = (FancyViewer *)_viewer;
	GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment(
					GTK_SCROLLED_WINDOW(viewer->scrollwin));

	if (viewer->view == NULL)
		return;

	gtkutils_scroll_one_line(GTK_WIDGET(viewer->view), vadj, up);
}

static void load_start_cb(WebKitWebView *view, gint progress,
                          FancyViewer *viewer)
{
	gtk_widget_show(viewer->progress);
	gtk_widget_show(viewer->ev_stop_loading);
}

static void load_finished_cb(WebKitWebView *view, gint progress,
			     FancyViewer *viewer)
{
	gtk_widget_hide(viewer->progress);
	gtk_widget_hide(viewer->ev_stop_loading);
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(viewer->progress),
				      (gdouble) 0.0);
	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(viewer->progress), "");
}

static void over_link_cb(WebKitWebView *view, const gchar *wtf,
			 const gchar *link, FancyViewer *viewer, void *wtfa)
{
	gtk_label_set_text(GTK_LABEL(viewer->l_link), link);
	g_free(viewer->cur_link);
	viewer->cur_link = NULL;
	if(link) {
		viewer->cur_link = g_strdup(link);
	}
}

static void load_progress_cb(WebKitWebView *view, gint progress,
			     FancyViewer *viewer)
{
	gdouble pbar;
	gchar *label = g_strdup_printf("%d%% Loading...", progress);
	pbar = (gdouble) progress / 100;
	gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(viewer->progress), pbar);
	gtk_progress_bar_set_text(GTK_PROGRESS_BAR(viewer->progress),
				  (const gchar*)label);
	g_free(label);
}
static void stop_loading_cb(GtkWidget *widget, GdkEvent *ev,
							FancyViewer *viewer)
{
	webkit_web_view_stop_loading (viewer->view);
	gtk_widget_hide(viewer->progress);
	gtk_widget_hide(viewer->ev_stop_loading);
}

static void search_the_web_cb(GtkWidget *widget, FancyViewer *viewer)
{
	debug_print("Clicked on Search on Web\n");
	if (webkit_web_view_has_selection(viewer->view)) {
		gchar *search;
#if WEBKIT_CHECK_VERSION(1,5,1)
		viewer->doc = webkit_web_view_get_dom_document(WEBKIT_WEB_VIEW(viewer->view));
		viewer->window = webkit_dom_document_get_default_view (viewer->doc);
		viewer->selection = webkit_dom_dom_window_get_selection (viewer->window);
		viewer->range = webkit_dom_dom_selection_get_range_at(viewer->selection, 0, NULL);
		gchar *tmp = webkit_dom_range_get_text (viewer->range);
#else
		gchar *tmp = webkit_web_view_get_selected_text(viewer->view);
#endif
		search = g_strconcat(GOOGLE_SEARCH, tmp, NULL);
#if WEBKIT_CHECK_VERSION(1,1,1)
		webkit_web_view_load_uri(viewer->view, search);
#else
		webkit_web_view_open(viewer->view, search);
#endif
		g_free(search);
		g_free(tmp);
	}
}

static void open_in_browser_cb(GtkWidget *widget, FancyViewer *viewer)
{
	debug_print("open outer: %s\n", viewer->cur_link);
	if(viewer->cur_link)
		open_uri(viewer->cur_link, prefs_common_get_uri_cmd());
}

static size_t download_file_curl_write_cb(void *buffer, size_t size,
					  size_t nmemb, void *data)
{
	FancyViewer *viewer = (FancyViewer *)data;
	if (!viewer->stream) {
		viewer->stream = fopen(viewer->curlfile, "wb");
		if (!viewer->stream)
			return -1;
	}
	return fwrite(buffer, size, nmemb, viewer->stream);
}
static void *download_file_curl (void *data)
{
	CURL *curl;
	CURLcode res;
	FancyViewer *viewer = (FancyViewer *)data;

	curl_global_init(CURL_GLOBAL_DEFAULT);
	curl = curl_easy_init();

	if (curl) {
		curl_easy_setopt(curl, CURLOPT_URL, viewer->cur_link);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, download_file_curl_write_cb);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, viewer);
		curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
		res = curl_easy_perform(curl);
		curl_easy_cleanup(curl);

		if (CURLE_OK != res)
			alertpanel_error(_("An error occurred: %d\n"), res);
		if (viewer->stream)
			fclose(viewer->stream);
		curl_global_cleanup();
	}
#ifdef USE_PTHREAD
	pthread_exit(NULL);
#else
	return NULL;
#endif
}
static void download_file_cb(GtkWidget *widget, FancyViewer *viewer)
{
#ifdef USE_PTHREAD
	pthread_t curljob;
	gint result;
#endif
	const gchar *link = (const gchar *)viewer->cur_link;
	gchar *filename = g_utf8_strchr(link, -1, g_utf8_get_char("/"));
	filename = g_strconcat(g_get_home_dir(), filename, NULL);
	gchar *fname = filesel_select_file_save(_("Save as"), filename);

	if (viewer->curlfile) viewer->curlfile = NULL;
	if (viewer->stream) viewer->stream = NULL;
	viewer->curlfile = (const gchar *)g_strdup(fname);
	g_free(filename);
	g_free(fname);

	if (!viewer->curlfile) return;

#ifdef USE_PTHREAD
	result = pthread_create(&curljob, NULL, download_file_curl, (void *)viewer);
	if (result)
		alertpanel_error("ERROR; return code from pthread_create() is %d\n", result);
#else
	download_file_curl((void *)viewer);
#endif
}

static void open_image_cb(GtkWidget *widget, FancyViewer *viewer)
{
	debug_print("Not Yet Implemented\n");
}

static void copy_image_cb(GtkWidget *widget, FancyViewer *viewer)
{
	debug_print("Not Yet Implemented\n");
}
static void import_feed_cb(GtkWidget *widget, FancyViewer *viewer)
{
	if (!folder_subscribe(viewer->cur_link))
		alertpanel_error(_("%s is a malformed or not supported feed"), viewer->cur_link);
}
static void viewer_menu_handler(GtkWidget *menuitem, FancyViewer *viewer)
{
	const gchar *g_name = gtk_widget_get_name(GTK_WIDGET(menuitem));
	if (!g_ascii_strcasecmp(g_name, "GtkImageMenuItem")) {

		GtkWidget *menul = gtk_bin_get_child(GTK_BIN(menuitem));

        if (!g_ascii_strcasecmp(gtk_label_get_text(GTK_LABEL(menul)),
				"Search the Web")) {
			gtk_label_set_text(GTK_LABEL(menul), _("Search the Web"));
			viewer->cur_link = NULL;
			GtkImageMenuItem *m_search = GTK_IMAGE_MENU_ITEM(menuitem);
			g_signal_connect(G_OBJECT(m_search), "activate",
					 G_CALLBACK(search_the_web_cb),
					 (gpointer *) viewer);
		}

		if (!g_ascii_strcasecmp(gtk_label_get_text(GTK_LABEL(menul)),
					"Open Link" )) {

			if (viewer->override_prefs_remote_content)
				gtk_label_set_text(GTK_LABEL(menul), _("Open in Viewer"));
			else
				gtk_label_set_text(GTK_LABEL(menul), _("Open in Viewer (enable remote content)"));

			GtkImageMenuItem *m_new = GTK_IMAGE_MENU_ITEM(menuitem);
			gtk_widget_set_sensitive(GTK_WIDGET(m_new), viewer->override_prefs_remote_content);
		}

		if (!g_ascii_strcasecmp(gtk_label_get_text(GTK_LABEL(menul)),
								"Open Link in New Window" )) {

			gtk_label_set_text(GTK_LABEL(menul), _("Open in Browser"));

			GtkImageMenuItem *m_new = GTK_IMAGE_MENU_ITEM(menuitem);
			g_signal_connect(G_OBJECT(m_new), "activate",
					 G_CALLBACK(open_in_browser_cb),
					 (gpointer *) viewer);
		}

		if (!g_ascii_strcasecmp(gtk_label_get_text(GTK_LABEL(menul)),
					"Open Image in New Window" )) {
			gtk_label_set_text(GTK_LABEL(menul), _("Open Image"));
			GtkImageMenuItem *m_image = GTK_IMAGE_MENU_ITEM(menuitem);
			g_signal_connect(G_OBJECT(m_image), "activate",
					 G_CALLBACK(open_image_cb),
					 (gpointer *) viewer);
		}

		if (!g_ascii_strcasecmp(gtk_label_get_text(GTK_LABEL(menul)),
					"Copy Link Location" )) {
			gtk_label_set_text(GTK_LABEL(menul), _("Copy Link"));
		}
        if (!g_ascii_strcasecmp(gtk_label_get_text(GTK_LABEL(menul)),
				"Download Linked File" )) {
			gtk_label_set_text(GTK_LABEL(menul), _("Download Link"));

			GtkImageMenuItem *m_dlink = GTK_IMAGE_MENU_ITEM(menuitem);
			g_signal_connect(G_OBJECT(m_dlink), "activate",
					 G_CALLBACK(download_file_cb),
					 (gpointer *) viewer);
		}

		if (!g_ascii_strcasecmp(gtk_label_get_text(GTK_LABEL(menul)),
					"Save Image As" )) {

			gtk_label_set_text(GTK_LABEL(menul), _("Save Image As"));

			GtkImageMenuItem *m_simage = GTK_IMAGE_MENU_ITEM(menuitem);
			g_signal_connect(G_OBJECT(m_simage), "activate",
					 G_CALLBACK(download_file_cb),
					 (gpointer *) viewer);
		}

		if (!g_ascii_strcasecmp(gtk_label_get_text(GTK_LABEL(menul)),
					"Copy Image" )) {
			gtk_label_set_text(GTK_LABEL(menul), _("Copy Image"));
			GtkImageMenuItem *m_cimage = GTK_IMAGE_MENU_ITEM(menuitem);
			g_signal_connect(G_OBJECT(m_cimage), "activate",
					 G_CALLBACK(copy_image_cb),
					 (gpointer *) viewer);
		}

	}
}

static gboolean populate_popup_cb (WebKitWebView *view, GtkWidget *menu,
				   FancyViewer *viewer)
{
	Plugin *plugin = plugin_get_loaded_by_name("RSSyl");
	gtk_container_foreach(GTK_CONTAINER(menu),
			      (GtkCallback)viewer_menu_handler,
			      viewer);
	if (plugin) {
		GtkWidget *rssyl = gtk_image_menu_item_new_with_label(_("Import feed"));
		GtkWidget *img = gtk_image_new_from_stock(GTK_STOCK_ADD, GTK_ICON_SIZE_MENU);
		gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(rssyl), img);
		gtk_widget_show(GTK_WIDGET(rssyl));
		gtk_menu_shell_append(GTK_MENU_SHELL(menu), rssyl);
		g_signal_connect(G_OBJECT(rssyl), "activate",
				 G_CALLBACK(import_feed_cb),
				 (gpointer *) viewer);
	}
	return TRUE;
}

static gint keypress_events_cb (GtkWidget *widget, GdkEventKey *event,
								FancyViewer *viewer)
{
	if (event->state == CTRL_KEY) {
		switch (event->keyval) {
		case GDK_plus:
			zoom_in_cb(viewer->ev_zoom_in, NULL, viewer);
			break;
		case GDK_period:
			zoom_100_cb(viewer->ev_zoom_100, NULL, viewer);
			break;
		case GDK_minus:
			zoom_out_cb(viewer->ev_zoom_out, NULL, viewer);
			break;
		}
	}
	return FALSE;
}

static gboolean release_button_cb (WebKitWebView *view, GdkEvent *ev,
				   FancyViewer *viewer)
{
	if (ev->button.button == 1 && viewer->cur_link && viewer->override_prefs_external) {
		open_uri(viewer->cur_link, prefs_common_get_uri_cmd());
		return TRUE;
	}
	return FALSE;
}
static void zoom_100_cb(GtkWidget *widget, GdkEvent *ev, FancyViewer *viewer)
{
	gtk_widget_grab_focus(widget);
	webkit_web_view_set_zoom_level(viewer->view, 1);
}

static void zoom_in_cb(GtkWidget *widget, GdkEvent *ev, FancyViewer *viewer)
{
	gtk_widget_grab_focus(widget);
	webkit_web_view_zoom_in(viewer->view);
}
static void zoom_out_cb(GtkWidget *widget, GdkEvent *ev, FancyViewer *viewer)
{
	gtk_widget_grab_focus(widget);
	webkit_web_view_zoom_out(viewer->view);
}

static MimeViewer *fancy_viewer_create(void)
{
	FancyViewer    *viewer;
	GtkWidget      *hbox;

	debug_print("fancy_viewer_create\n");

	viewer = g_new0(FancyViewer, 1);
	viewer->mimeviewer.factory = &fancy_viewer_factory;
	viewer->mimeviewer.get_widget = fancy_get_widget;
	viewer->mimeviewer.get_selection = fancy_get_selection;
	viewer->mimeviewer.show_mimepart = fancy_show_mimepart;
	viewer->mimeviewer.print = fancy_print;
	viewer->mimeviewer.clear_viewer = fancy_clear_viewer;
	viewer->mimeviewer.destroy_viewer = fancy_destroy_viewer;
	viewer->mimeviewer.text_search = fancy_text_search;
	viewer->mimeviewer.scroll_page = fancy_scroll_page;
	viewer->mimeviewer.scroll_one_line = fancy_scroll_one_line;
	viewer->view = WEBKIT_WEB_VIEW(webkit_web_view_new());

#ifdef HAVE_LIBSOUP_GNOME
 /* Use GNOME proxy settings through libproxy */
	if (fancy_prefs.enable_gnome_proxy) {
		SoupSession *session = webkit_get_default_session();
		soup_session_add_feature_by_type (session, SOUP_TYPE_PROXY_RESOLVER_GNOME);
	}
#endif
	
	if (fancy_prefs.enable_proxy) {
		SoupSession *session = webkit_get_default_session();
		SoupURI* pURI = soup_uri_new(fancy_prefs.proxy_str);
		g_object_set(session, "proxy-uri", pURI, NULL);
	}

	viewer->settings = webkit_web_settings_new();
	g_object_set(viewer->settings, "user-agent", "Fancy Viewer", NULL);
	viewer->scrollwin = gtk_scrolled_window_new(NULL, NULL);
	viewer->tag = -1;
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(viewer->scrollwin),
				       GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(viewer->scrollwin),
					    GTK_SHADOW_IN);
	gtk_container_add(GTK_CONTAINER(viewer->scrollwin),
			  GTK_WIDGET(viewer->view));

	viewer->vbox = gtk_vbox_new(FALSE, 0);
	hbox = gtk_hbox_new(FALSE, 0);
	viewer->progress = gtk_progress_bar_new();
	/* Zoom Widgets */
	viewer->zoom_100 = gtk_image_new_from_stock(GTK_STOCK_ZOOM_100, GTK_ICON_SIZE_MENU);
	viewer->zoom_in = gtk_image_new_from_stock(GTK_STOCK_ZOOM_IN, GTK_ICON_SIZE_MENU);
	viewer->zoom_out = gtk_image_new_from_stock(GTK_STOCK_ZOOM_OUT, GTK_ICON_SIZE_MENU);
	viewer->stop_loading = gtk_image_new_from_stock(GTK_STOCK_CANCEL, GTK_ICON_SIZE_MENU);
	/* Event Widgets for the Zoom Widgets  */
	viewer->ev_zoom_100 = gtk_event_box_new();
	viewer->ev_zoom_in = gtk_event_box_new();
	viewer->ev_zoom_out = gtk_event_box_new();
	viewer->ev_stop_loading = gtk_event_box_new();

	/* Link Label */
	viewer->l_link = gtk_label_new("");

	/* Preferences Widgets to override preferences on the fly  */
	viewer->fancy_prefs = gtk_image_new_from_stock(GTK_STOCK_PREFERENCES, GTK_ICON_SIZE_MENU);
	viewer->ev_fancy_prefs = gtk_event_box_new();

	/* Popup Menu for preferences  */
	viewer->fancy_prefs_menu = gtk_menu_new();
	fancy_create_popup_prefs_menu(viewer);

	gtk_event_box_set_visible_window(GTK_EVENT_BOX(viewer->ev_zoom_100), FALSE);
	gtk_event_box_set_visible_window(GTK_EVENT_BOX(viewer->ev_zoom_in), FALSE);
	gtk_event_box_set_visible_window(GTK_EVENT_BOX(viewer->ev_zoom_out), FALSE);
	gtk_event_box_set_visible_window(GTK_EVENT_BOX(viewer->ev_fancy_prefs), FALSE);
	gtk_event_box_set_visible_window(GTK_EVENT_BOX(viewer->ev_stop_loading), FALSE);

	gtk_container_add(GTK_CONTAINER(viewer->ev_zoom_100), viewer->zoom_100);
	gtk_container_add(GTK_CONTAINER(viewer->ev_zoom_in), viewer->zoom_in);
	gtk_container_add(GTK_CONTAINER(viewer->ev_zoom_out), viewer->zoom_out);
	gtk_container_add(GTK_CONTAINER(viewer->ev_fancy_prefs), viewer->fancy_prefs);
	gtk_container_add(GTK_CONTAINER(viewer->ev_stop_loading), viewer->stop_loading);

	gtk_box_pack_start(GTK_BOX(hbox), viewer->ev_zoom_100, FALSE, FALSE, 1);
	gtk_box_pack_start(GTK_BOX(hbox), viewer->ev_zoom_in, FALSE, FALSE, 2);
	gtk_box_pack_start(GTK_BOX(hbox), viewer->ev_zoom_out, FALSE, FALSE, 2);
	gtk_box_pack_start(GTK_BOX(hbox), viewer->ev_fancy_prefs, FALSE, FALSE, 2);
	gtk_box_pack_start(GTK_BOX(hbox), viewer->l_link, FALSE, FALSE, 8);
	gtk_box_pack_end(GTK_BOX(hbox), viewer->progress, FALSE, FALSE, 0);
	gtk_box_pack_end(GTK_BOX(hbox), viewer->ev_stop_loading, FALSE, FALSE, 0);

	gtk_box_pack_start(GTK_BOX(viewer->vbox), viewer->scrollwin, TRUE, TRUE,
                       1);
	gtk_box_pack_start(GTK_BOX(viewer->vbox), hbox, FALSE, FALSE, 0);

	gtk_widget_show(viewer->ev_zoom_100);
	gtk_widget_show(viewer->ev_zoom_in);
	gtk_widget_show(viewer->ev_zoom_out);
	gtk_widget_show(viewer->ev_fancy_prefs);

	gtk_widget_show(viewer->scrollwin);
	gtk_widget_show(viewer->zoom_100);
	gtk_widget_show(viewer->zoom_in);
	gtk_widget_show(viewer->zoom_out);
	gtk_widget_show(viewer->fancy_prefs);
	gtk_widget_show(viewer->stop_loading);

	gtk_widget_show(viewer->l_link);
	gtk_widget_show(viewer->vbox);
	gtk_widget_show(hbox);
	gtk_widget_show(GTK_WIDGET(viewer->view));

	g_signal_connect(G_OBJECT(viewer->view), "load-started",
			 G_CALLBACK(load_start_cb), viewer);
	g_signal_connect(G_OBJECT(viewer->view), "load-finished",
			 G_CALLBACK(load_finished_cb), viewer);
	g_signal_connect(G_OBJECT(viewer->view), "hovering-over-link",
			 G_CALLBACK(over_link_cb), viewer);
	g_signal_connect(G_OBJECT(viewer->view), "load-progress-changed",
			 G_CALLBACK(load_progress_cb), viewer);
	g_signal_connect(G_OBJECT(viewer->view), "navigation-requested",
			 G_CALLBACK(navigation_requested_cb), viewer);
	g_signal_connect(G_OBJECT(viewer->view), "resource-request-starting",
			G_CALLBACK(resource_request_starting_cb), viewer);
	g_signal_connect(G_OBJECT(viewer->view), "populate-popup",
			 G_CALLBACK(populate_popup_cb), viewer);
	g_signal_connect(G_OBJECT(viewer->view), "button-release-event",
			 G_CALLBACK(release_button_cb), viewer);
	g_signal_connect(G_OBJECT(viewer->ev_zoom_100), "button-press-event",
			 G_CALLBACK(zoom_100_cb), (gpointer*)viewer);
	g_signal_connect(G_OBJECT(viewer->ev_zoom_in), "button-press-event",
			 G_CALLBACK(zoom_in_cb), (gpointer *)viewer);
	g_signal_connect(G_OBJECT(viewer->ev_zoom_out), "button-press-event",
			 G_CALLBACK(zoom_out_cb), (gpointer *)viewer);
	g_signal_connect(G_OBJECT(viewer->ev_fancy_prefs), "button-press-event",
			 G_CALLBACK(fancy_prefs_cb), (gpointer *)viewer);
	g_signal_connect(G_OBJECT(viewer->ev_stop_loading), "button-press-event",
			 G_CALLBACK(stop_loading_cb), viewer);
	g_signal_connect(G_OBJECT(viewer->view), "key_press_event",
			 G_CALLBACK(keypress_events_cb), viewer);

	viewer->filename = NULL;
	return (MimeViewer *) viewer;
}

static gchar *content_types[] = {"text/html", NULL};

static MimeViewerFactory fancy_viewer_factory =
{
	content_types,
	0,
	fancy_viewer_create,
};

gint plugin_init(gchar **error)
{
	if (!check_plugin_version(MAKE_NUMERIC_VERSION(2,9,2,72),
				  VERSION_NUMERIC, _("Fancy"), error))
		return -1;
	gchar *directory = g_strconcat(get_rc_dir(), G_DIR_SEPARATOR_S,
				"fancy", NULL);
	if (!is_dir_exist(directory))
		make_dir (directory);
	g_free(directory);

	fancy_prefs_init();

	mimeview_register_viewer_factory(&fancy_viewer_factory);

	return 0;
}

gboolean plugin_done(void)
{
	mimeview_unregister_viewer_factory(&fancy_viewer_factory);
	fancy_prefs_done();
	return FALSE;
}

const gchar *plugin_name(void)
{
	/* i18n: 'Fancy' here is name of the plugin, not the english word. */
	return _("Fancy HTML Viewer");
}

const gchar *plugin_desc(void)
{
	return g_strdup_printf(_("This plugin renders HTML mail using the WebKit "
			       "%d.%d.%d library.\nBy default all remote content is "
			       "blocked and images are not automatically loaded. Options "
			       "can be found in /Configuration/Preferences/Plugins/Fancy"),
			       WEBKIT_MAJOR_VERSION, WEBKIT_MINOR_VERSION,
			       WEBKIT_MICRO_VERSION);
}

const gchar *plugin_type(void)
{
	return "GTK2";
}

const gchar *plugin_licence(void)
{
	return "GPL3";
}

const gchar *plugin_version(void)
{
	return VERSION;
}

struct PluginFeature *plugin_provides(void)
{
	static struct PluginFeature features[] = {
					{PLUGIN_MIMEVIEWER, "text/html"},
         			{PLUGIN_NOTHING, NULL}
	};
	return features;
}
