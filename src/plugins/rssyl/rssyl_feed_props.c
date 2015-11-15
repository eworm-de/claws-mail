/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2004 Hiroyuki Yamamoto
 * This file (C) 2005 Andrej Kacian <andrej@kacian.sk>
 *
 * - Plugin preferences
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

/* Global includes */
#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>

/* Claws Mail includes */
#include <mainwindow.h>

/* Local includes */
#include "rssyl.h"
#include "rssyl_feed.h"
#include "rssyl_feed_props.h"
#include "rssyl_prefs.h"
#include "rssyl_update_feed.h"

static void rssyl_gtk_prop_store(RFolderItem *ritem)
{
	gchar *url, *auth_user, *auth_pass;
	gint x, old_ri, old_fetch_comments;
	gboolean use_default_ri = FALSE, keep_old = FALSE;
	FolderItem *item;

	g_return_if_fail(ritem != NULL);
	g_return_if_fail(ritem->feedprop != NULL);

	url = (gchar *)gtk_entry_get_text(GTK_ENTRY(ritem->feedprop->url));

	if( strlen(url) ) {
		if( ritem->url ) {
			g_free(ritem->url);
		}
		ritem->url = g_strdup(url);
	}

	ritem->auth->type = gtk_combo_box_get_active(GTK_COMBO_BOX(ritem->feedprop->auth_type));

	auth_user = (gchar *)gtk_entry_get_text(GTK_ENTRY(ritem->feedprop->auth_username));
	if (auth_user != NULL) {
		if (ritem->auth->username) {
			g_free(ritem->auth->username);
		}
		ritem->auth->username = g_strdup(auth_user);
	}

	auth_pass = (gchar *)gtk_entry_get_text(GTK_ENTRY(ritem->feedprop->auth_password));
	if (auth_pass != NULL) {
		if (ritem->auth->password) {
			g_free(ritem->auth->password);
		}
		ritem->auth->password = g_strdup(auth_pass);
	}

	use_default_ri = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(ritem->feedprop->default_refresh_interval));
	ritem->default_refresh_interval = use_default_ri;
	debug_print("store: default refresh interval is %s\n",
			( use_default_ri ? "ON" : "OFF" ) );

	/* Use default if checkbutton is set */
	if( use_default_ri )
		x = rssyl_prefs_get()->refresh;
	else
		x = gtk_spin_button_get_value_as_int(
				GTK_SPIN_BUTTON(ritem->feedprop->refresh_interval) );

	old_ri = ritem->refresh_interval;
	ritem->refresh_interval = x;

	/* Set timer for next automatic refresh, if needed. */
	if( x > 0 ) {
		if( old_ri != x || ritem->refresh_id == 0 ) {
			debug_print("RSSyl: GTK - refresh interval changed to %d , updating "
					"timeout\n", ritem->refresh_interval);
			rssyl_feed_start_refresh_timeout(ritem);
		}
	} else
		ritem->refresh_id = 0;

	old_fetch_comments = ritem->fetch_comments;
	ritem->fetch_comments = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(ritem->feedprop->fetch_comments));

	if (!old_fetch_comments && ritem->fetch_comments) {
		/* reset the RFolderItem's mtime to be sure we get all 
		 * available comments */
		 ritem->item.mtime = 0;
	}
	
	ritem->fetch_comments_max_age = gtk_spin_button_get_value_as_int(
			GTK_SPIN_BUTTON(ritem->feedprop->fetch_comments_max_age));

	keep_old = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(ritem->feedprop->keep_old));
	ritem->keep_old = keep_old;

	ritem->silent_update =
		gtk_combo_box_get_active(GTK_COMBO_BOX(ritem->feedprop->silent_update));

	ritem->write_heading = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(ritem->feedprop->write_heading));

	ritem->ignore_title_rename = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(ritem->feedprop->ignore_title_rename));

	ritem->ssl_verify_peer = gtk_toggle_button_get_active(
			GTK_TOGGLE_BUTTON(ritem->feedprop->ssl_verify_peer));

	/* Store updated properties */
	item = &ritem->item;
	item->folder->klass->item_get_xml(item->folder, item);
}

static gboolean
rssyl_feedprop_togglebutton_toggled_cb(GtkToggleButton *tb,
		gpointer data)
{
	gboolean active = gtk_toggle_button_get_active(tb);
	RFeedProp *feedprop = (RFeedProp *)data;
	GtkWidget *sb = NULL;

	if( (GtkWidget *)tb == feedprop->default_refresh_interval ) {
		active = !active;
		sb = feedprop->refresh_interval;
	} else if( (GtkWidget *)tb == feedprop->fetch_comments ) {
		sb = feedprop->fetch_comments_max_age;
	}

	g_return_val_if_fail(sb != NULL, FALSE);

	gtk_widget_set_sensitive(sb, active);

	return FALSE;
}

static void
rssyl_feedprop_auth_type_changed_cb(GtkComboBox *cb, gpointer data)
{
	RFeedProp *feedprop = (RFeedProp *)data;
	gboolean enable = (FEED_AUTH_NONE != gtk_combo_box_get_active(cb));
	gtk_widget_set_sensitive(GTK_WIDGET(feedprop->auth_username), enable);
	gtk_widget_set_sensitive(GTK_WIDGET(feedprop->auth_password), enable);
}

static gboolean
rssyl_props_cancel_cb(GtkWidget *widget, gpointer data)
{
	RFolderItem *ritem = (RFolderItem *)data;

	debug_print("RSSyl: Cancel pressed\n");

	gtk_widget_destroy(ritem->feedprop->window);

	return FALSE;
}

static gboolean
rssyl_props_ok_cb(GtkWidget *widget, gpointer data)
{
	RFolderItem *ritem = (RFolderItem *)data;

	debug_print("RSSyl: OK pressed\n");
	rssyl_gtk_prop_store(ritem);

	gtk_widget_destroy(ritem->feedprop->window);

	return FALSE;
}

static gboolean
rssyl_props_trim_cb(GtkWidget *widget, gpointer data)
{
	RFolderItem *ritem = (RFolderItem *)data;
	gboolean k = ritem->keep_old;

	if( k )
		ritem->keep_old = FALSE;

	rssyl_update_feed(ritem, FALSE);

	if( k )
		ritem->keep_old = TRUE;

	return FALSE;
}

static gboolean
rssyl_props_key_press_cb(GtkWidget *widget, GdkEventKey *event,
		gpointer data)
{
	if (event) {
		switch (event->keyval) {
			case GDK_Escape:
				rssyl_props_cancel_cb(widget, data);
				return TRUE;
			case GDK_Return:
			case GDK_KP_Enter:
				rssyl_props_ok_cb(widget, data);
				return TRUE;
			default:
				break;
		}
	}

	return FALSE;
}


void rssyl_gtk_prop(RFolderItem *ritem)
{
	MainWindow *mainwin = mainwindow_get_mainwindow();
	RFeedProp *feedprop;
	GtkWidget *vbox, *urllabel, *urlframe, *urlalign, *table, *label,
		*inner_vbox, *auth_hbox, *auth_user_label, *auth_pass_label,
						*hsep, *sep, *bbox, *cancel_button, *cancel_align,
						*cancel_hbox, *cancel_image, *cancel_label, *ok_button, *ok_align,
						*ok_hbox, *ok_image, *ok_label, *trim_button, *silent_update_label;
	GtkObject *adj;
	gint refresh;
	gint row = 0;

	g_return_if_fail(ritem != NULL);

	feedprop = g_new0(RFeedProp, 1);

	/**** Create required widgets ****/

	/* Window */
	feedprop->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	/* URL entry */
	feedprop->url = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(feedprop->url), ritem->url);

	/* URL auth type combo */
#if !GTK_CHECK_VERSION(2, 24, 0)
	feedprop->auth_type = gtk_combo_box_new_text();
	gtk_combo_box_append_text(GTK_COMBO_BOX(feedprop->auth_type),
#else
	feedprop->auth_type = gtk_combo_box_text_new();
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(feedprop->auth_type),
#endif
			_("No authentication"));
#if !GTK_CHECK_VERSION(2, 24, 0)
	gtk_combo_box_append_text(GTK_COMBO_BOX(feedprop->auth_type),
#else
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(feedprop->auth_type),
#endif
			_("HTTP Basic authentication"));
	gtk_combo_box_set_active(GTK_COMBO_BOX(feedprop->auth_type),
			ritem->auth->type);

	/* Auth username */
	feedprop->auth_username = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(feedprop->auth_username),
			ritem->auth->username);

	/* Auth password */
	feedprop->auth_password = gtk_entry_new();
	gtk_entry_set_visibility(GTK_ENTRY(feedprop->auth_password), FALSE);
	gtk_entry_set_text(GTK_ENTRY(feedprop->auth_password),
			ritem->auth->password);

	/* "Use default refresh interval" checkbutton */
	feedprop->default_refresh_interval = gtk_check_button_new_with_mnemonic(
			_("Use default refresh interval"));
	gtk_toggle_button_set_active(
			GTK_TOGGLE_BUTTON(feedprop->default_refresh_interval),
			ritem->default_refresh_interval);

	if( ritem->refresh_interval >= 0 && !ritem->default_refresh_interval )
		refresh = ritem->refresh_interval;
	else
		refresh = rssyl_prefs_get()->refresh;

	/* "Keep old items" checkbutton */
	feedprop->keep_old = gtk_check_button_new_with_mnemonic(
			_("Keep old items"));
	gtk_toggle_button_set_active(
			GTK_TOGGLE_BUTTON(feedprop->keep_old),
			ritem->keep_old);

	/* "Trim" button */
	trim_button = gtk_button_new_with_mnemonic(_("_Trim"));
	gtk_widget_set_tooltip_text(trim_button,
			_("Update feed, deleting items which are no longer in the source feed"));

	feedprop->fetch_comments = gtk_check_button_new_with_mnemonic(
			_("Fetch comments if possible"));
	gtk_toggle_button_set_active(
			GTK_TOGGLE_BUTTON(feedprop->fetch_comments),
			ritem->fetch_comments);

	/* Fetch comments max age spinbutton */
	adj = gtk_adjustment_new(ritem->fetch_comments_max_age,
			-1, 100000, 1, 10, 0);
	feedprop->fetch_comments_max_age = gtk_spin_button_new(GTK_ADJUSTMENT(adj),
			1, 0);

	/* Refresh interval spinbutton */
	adj = gtk_adjustment_new(refresh,
			0, 100000, 10, 100, 0);
	feedprop->refresh_interval = gtk_spin_button_new(GTK_ADJUSTMENT(adj),
			1, 0);

	/* Silent update - combobox */
#if !GTK_CHECK_VERSION(2, 24, 0)
	feedprop->silent_update = gtk_combo_box_new_text();
	gtk_combo_box_append_text(GTK_COMBO_BOX(feedprop->silent_update),
#else
	feedprop->silent_update = gtk_combo_box_text_new();
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(feedprop->silent_update),
#endif
			_("Always mark as new"));
#if !GTK_CHECK_VERSION(2, 24, 0)
	gtk_combo_box_append_text(GTK_COMBO_BOX(feedprop->silent_update),
#else
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(feedprop->silent_update),
#endif
			_("If only its text changed"));
#if !GTK_CHECK_VERSION(2, 24, 0)
	gtk_combo_box_append_text(GTK_COMBO_BOX(feedprop->silent_update),
#else
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(feedprop->silent_update),
#endif
			_("Never mark as new"));
	gtk_combo_box_set_active(GTK_COMBO_BOX(feedprop->silent_update),
			ritem->silent_update);

	feedprop->write_heading = gtk_check_button_new_with_mnemonic(
			_("Add item title to top of message"));
	gtk_toggle_button_set_active(
			GTK_TOGGLE_BUTTON(feedprop->write_heading),
			ritem->write_heading);

	/* Ignore title rename - checkbox */
	feedprop->ignore_title_rename = gtk_check_button_new_with_mnemonic(
			_("Ignore title rename"));
	gtk_toggle_button_set_active(
			GTK_TOGGLE_BUTTON(feedprop->ignore_title_rename),
			ritem->ignore_title_rename);
	gtk_widget_set_tooltip_text(feedprop->ignore_title_rename,
			_("Enable this to keep current folder name, even if feed author changes title of the feed."));

	/* Verify SSL peer certificate */
	feedprop->ssl_verify_peer = gtk_check_button_new_with_label(
			_("Verify SSL certificate validity"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(feedprop->ssl_verify_peer),
			ritem->ssl_verify_peer);

	/* === Now pack all the widgets */
	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(feedprop->window), vbox);

	/* URL frame */
	urlframe = gtk_frame_new(NULL);
	gtk_container_set_border_width(GTK_CONTAINER(urlframe), 5);
	gtk_frame_set_label_align(GTK_FRAME(urlframe), 0.05, 0.5);
	gtk_frame_set_shadow_type(GTK_FRAME(urlframe), GTK_SHADOW_ETCHED_OUT);
	gtk_box_pack_start(GTK_BOX(vbox), urlframe, FALSE, FALSE, 0);

	/* Label for URL frame */
	urllabel = gtk_label_new(g_strconcat("<b>",_("Source URL:"),"</b>", NULL));
	gtk_label_set_use_markup(GTK_LABEL(urllabel), TRUE);
	gtk_misc_set_padding(GTK_MISC(urllabel), 5, 0);
	gtk_frame_set_label_widget(GTK_FRAME(urlframe), urllabel);

	/* URL entry (+ alignment in frame) */
	urlalign = gtk_alignment_new(0, 0.5, 1, 1);
	gtk_alignment_set_padding(GTK_ALIGNMENT(urlalign), 5, 5, 5, 5);
	gtk_container_add(GTK_CONTAINER(urlframe), urlalign);

	inner_vbox = gtk_vbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(inner_vbox), feedprop->url, FALSE, FALSE, 0);
	gtk_entry_set_activates_default(GTK_ENTRY(feedprop->url), TRUE);
	gtk_container_add(GTK_CONTAINER(urlalign), inner_vbox);

	/* Auth combo + user (label + entry) + pass (label + entry) */
	auth_hbox = gtk_hbox_new(FALSE, 5);
	gtk_box_pack_start(GTK_BOX(auth_hbox), feedprop->auth_type, FALSE, FALSE, 0);
	g_signal_connect(G_OBJECT(feedprop->auth_type), "changed",
			G_CALLBACK(rssyl_feedprop_auth_type_changed_cb),
			(gpointer) feedprop);
	g_signal_emit_by_name(G_OBJECT(feedprop->auth_type), "changed");
	auth_user_label = gtk_label_new(_("User name"));
	gtk_box_pack_start(GTK_BOX(auth_hbox), auth_user_label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(auth_hbox), feedprop->auth_username, FALSE, FALSE, 0);
	auth_pass_label = gtk_label_new(_("Password"));
	gtk_box_pack_start(GTK_BOX(auth_hbox), auth_pass_label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(auth_hbox), feedprop->auth_password, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(inner_vbox), auth_hbox, FALSE, FALSE, 0);

	/* Table for remaining properties */
	table = gtk_table_new(11, 2, FALSE);
	gtk_box_pack_start(GTK_BOX(vbox), table, TRUE, TRUE, 0);

	/* Fetch comments - checkbutton */
	gtk_table_attach(GTK_TABLE(table), feedprop->fetch_comments,
			0, 2, row, row+1,
			(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			(GtkAttachOptions) (0), 10, 0);
	g_signal_connect(G_OBJECT(feedprop->fetch_comments), "toggled",
			G_CALLBACK(rssyl_feedprop_togglebutton_toggled_cb),
			(gpointer)feedprop);

	row++;
	/* Fetch comments max age - label */
	label = gtk_label_new(g_strconcat("<b>",_("Fetch comments on posts aged less than:"),"</b>\n"
				"<small>",_("(In days; set to -1 to fetch all comments)"), "</small>",
				NULL));
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1,
			(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			(GtkAttachOptions) (0), 10, 5);

	/* Fetch comments max age - spinbutton */
	gtk_widget_set_sensitive(feedprop->fetch_comments_max_age,
			ritem->fetch_comments);
	gtk_table_attach(GTK_TABLE(table), feedprop->fetch_comments_max_age,
			1, 2, row, row+1,
			(GtkAttachOptions) (0),
			(GtkAttachOptions) (0), 10, 5);

	row++;
	/* Separator below comments max age */
	hsep = gtk_hseparator_new();
	gtk_widget_set_size_request(hsep, -1, 10);
	gtk_table_attach(GTK_TABLE(table), hsep, 0, 2, row, row+1,
			(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			(GtkAttachOptions) (0), 10, 5);

	row++;
	/* Keep old items - checkbutton */
	gtk_table_attach(GTK_TABLE(table), feedprop->keep_old,
			0, 1, row, row+1,
			(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			(GtkAttachOptions) (0), 10, 0);

	/* 'Trim' - button */
	gtk_table_attach(GTK_TABLE(table), trim_button,
			1, 2, row, row+1,
			(GtkAttachOptions) (0),
			(GtkAttachOptions) (0), 10, 0);
	g_signal_connect(G_OBJECT(trim_button), "clicked",
			G_CALLBACK(rssyl_props_trim_cb), ritem);

	row++;
	hsep = gtk_hseparator_new();
	gtk_widget_set_size_request(hsep, -1, 10);
	gtk_table_attach(GTK_TABLE(table), hsep, 0, 2, row, row+1,
			(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			(GtkAttachOptions) (0), 10, 5);

	row++;
	/* Use default refresh interval - checkbutton */
	gtk_table_attach(GTK_TABLE(table), feedprop->default_refresh_interval,
			0, 2, row, row+1,
			(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			(GtkAttachOptions) (0), 10, 0);
	g_signal_connect(G_OBJECT(feedprop->default_refresh_interval), "toggled",
			G_CALLBACK(rssyl_feedprop_togglebutton_toggled_cb),
			(gpointer)feedprop);

	row++;
	/* Refresh interval - label */
	label = gtk_label_new(g_strconcat("<b>",_("Refresh interval in minutes:"),"</b>\n"
			"<small>",_("(Set to 0 to disable automatic refreshing for this feed)"),
			"</small>", NULL));
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, row, row+1,
			(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			(GtkAttachOptions) (0), 10, 5);

	/* Refresh interval - spinbutton */
	gtk_widget_set_sensitive(feedprop->refresh_interval,
			!ritem->default_refresh_interval);
	gtk_table_attach(GTK_TABLE(table), feedprop->refresh_interval, 1, 2, row, row+1,
			(GtkAttachOptions) (0),
			(GtkAttachOptions) (0), 10, 5);

	row++;
	hsep = gtk_hseparator_new();
	gtk_widget_set_size_request(hsep, -1, 10);
	gtk_table_attach(GTK_TABLE(table), hsep, 0, 2, row, row+1,
			(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			(GtkAttachOptions) (0), 10, 5);

	row++;
	/* Silent update - label */
	silent_update_label =
		gtk_label_new(g_strconcat("<b>",_("If an item changes, do not mark it as new:"),"</b>", NULL));
	gtk_label_set_use_markup(GTK_LABEL(silent_update_label), TRUE);
	gtk_misc_set_alignment(GTK_MISC(silent_update_label), 0, 0.5);
	gtk_table_attach(GTK_TABLE(table), silent_update_label, 0, 1, row, row+1,
			(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			(GtkAttachOptions) (0), 10, 5);

	gtk_table_attach(GTK_TABLE(table), feedprop->silent_update, 1, 2, row, row+1,
			(GtkAttachOptions) (0),
			(GtkAttachOptions) (0), 10, 5);

	row++;
	hsep = gtk_hseparator_new();
	gtk_widget_set_size_request(hsep, -1, 10);
	gtk_table_attach(GTK_TABLE(table), hsep, 0, 2, row, row+1,
			(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			(GtkAttachOptions) (0), 10, 5);

	row++;

	/* Write heading - checkbox */
	gtk_table_attach(GTK_TABLE(table), feedprop->write_heading,
			0, 2, row, row+1,
			(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			(GtkAttachOptions) (0), 10, 0);


	row++;

	/* Ignore title rename - checkbutton */
	gtk_table_attach(GTK_TABLE(table), feedprop->ignore_title_rename,
			0, 1, row, row+1,
			(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			(GtkAttachOptions) (0), 10, 0);

	row++;

	/* Verify SSL peer certificate - checkbutton */
	gtk_table_attach(GTK_TABLE(table), feedprop->ssl_verify_peer,
			0, 1, row, row+1,
			(GtkAttachOptions) (GTK_EXPAND | GTK_FILL),
			(GtkAttachOptions) (0), 10, 0);

	row++;

	/* Separator above the button box */
	sep = gtk_hseparator_new();
	gtk_widget_set_size_request(sep, -1, 10);
	gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 0);

	/* Buttonbox */
	bbox = gtk_hbutton_box_new();
	gtk_container_set_border_width(GTK_CONTAINER(bbox), 5);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_END);
	gtk_box_set_spacing(GTK_BOX(bbox), 5);
	gtk_box_pack_end(GTK_BOX(vbox), bbox, FALSE, FALSE, 0);

	/* Cancel button */
	cancel_button = gtk_button_new();
	gtk_container_add(GTK_CONTAINER(bbox), cancel_button);

	cancel_align = gtk_alignment_new(0.5, 0.5, 0, 0);
	gtk_container_add(GTK_CONTAINER(cancel_button), cancel_align);

	cancel_hbox = gtk_hbox_new(FALSE, 2);
	gtk_container_add(GTK_CONTAINER(cancel_align), cancel_hbox);

	cancel_image = gtk_image_new_from_stock(GTK_STOCK_CANCEL,
			GTK_ICON_SIZE_BUTTON);
	gtk_box_pack_start(GTK_BOX(cancel_hbox), cancel_image, FALSE, FALSE, 0);

	cancel_label = gtk_label_new_with_mnemonic(_("_Cancel"));
	gtk_box_pack_end(GTK_BOX(cancel_hbox), cancel_label, FALSE, FALSE, 0);

	g_signal_connect(G_OBJECT(cancel_button), "clicked",
			G_CALLBACK(rssyl_props_cancel_cb), ritem);

	/* OK button */
	ok_button = gtk_button_new();
	gtk_container_add(GTK_CONTAINER(bbox), ok_button);
	GTK_WIDGET_SET_FLAGS(ok_button, GTK_CAN_DEFAULT );

	ok_align = gtk_alignment_new(0.5, 0.5, 0, 0);
	gtk_container_add(GTK_CONTAINER(ok_button), ok_align);

	ok_hbox = gtk_hbox_new(FALSE, 2);
	gtk_container_add(GTK_CONTAINER(ok_align), ok_hbox);

	ok_image = gtk_image_new_from_stock(GTK_STOCK_OK,
			GTK_ICON_SIZE_BUTTON);
	gtk_box_pack_start(GTK_BOX(ok_hbox), ok_image, FALSE, FALSE, 0);

	ok_label = gtk_label_new_with_mnemonic(_("_OK"));
	gtk_box_pack_end(GTK_BOX(ok_hbox), ok_label, FALSE, FALSE, 0);

	g_signal_connect(G_OBJECT(ok_button), "clicked",
			G_CALLBACK(rssyl_props_ok_cb), ritem);

	/* Set some misc. stuff */
	gtk_window_set_title(GTK_WINDOW(feedprop->window),
			g_strdup(_("Set feed properties")) );
	gtk_window_set_modal(GTK_WINDOW(feedprop->window), TRUE);
	gtk_window_set_transient_for(GTK_WINDOW(feedprop->window),
			GTK_WINDOW(mainwin->window) );

	/* Attach callbacks to handle Enter and Escape keys */
	g_signal_connect(G_OBJECT(feedprop->window), "key_press_event",
			G_CALLBACK(rssyl_props_key_press_cb), ritem);

	/* ...and voila! */
	gtk_widget_show_all(feedprop->window);
	gtk_widget_grab_default(ok_button);

	/* Unselect the text in URL entry */
	gtk_editable_select_region(GTK_EDITABLE(feedprop->url), 0, 0);

	ritem->feedprop = feedprop;
}
