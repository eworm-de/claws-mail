/*
 * Claws Mail -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2016 The Claws Mail Team
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
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#include "claws-features.h"
#endif

#ifndef PASSWORD_CRYPTO_OLD

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "common/utils.h"
#include "gtk/manage_window.h"
#include "gtk/gtkutils.h"
#include "account.h"
#include "alertpanel.h"
#include "password.h"
#include "prefs_common.h"

static void entry_new1_activated(GtkEntry *entry, gpointer user_data)
{
	const gchar *text = gtk_entry_get_text(entry);

	if (strlen(text) > 0)
		gtk_widget_grab_focus(GTK_WIDGET(user_data));
}

struct _ctx {
	gboolean done;
	GtkWidget *dialog;
	GtkWidget *entry_new1;
	GtkWidget *entry_new2;
};

static void ok_button_clicked(GtkButton *button, gpointer user_data)
{
	struct _ctx *ctx = (struct _ctx *)user_data;
	const gchar *new1 = gtk_entry_get_text(GTK_ENTRY(ctx->entry_new1));
	const gchar *new2 = gtk_entry_get_text(GTK_ENTRY(ctx->entry_new2));

	debug_print("OK button activated\n");

	/* Now we check the new password - same in both entries. */
	if (strcmp(new1, new2)) {
		debug_print("passwords do not match\n");
		alertpanel_warning(_("New passwords do not match, try again."));
		gtk_entry_set_text(GTK_ENTRY(ctx->entry_new1), "");
		gtk_entry_set_text(GTK_ENTRY(ctx->entry_new2), "");
		gtk_widget_grab_focus(ctx->entry_new1);
		return;
	}

	master_password_change(new1);

	ctx->done = TRUE;
	gtk_widget_destroy(ctx->dialog);
	ctx->dialog = NULL;
}

static void cancel_button_clicked(GtkButton *button, gpointer user_data)
{
	struct _ctx *ctx = (struct _ctx *)user_data;
	ctx->done = TRUE;
	gtk_widget_destroy(ctx->dialog);
	ctx->dialog = NULL;
}

static void dialog_destroy(GtkWidget *widget, gpointer user_data)
{
	struct _ctx *ctx = (struct _ctx *)user_data;
	ctx->done = TRUE;
	ctx->dialog = NULL;
}

void master_password_change_dialog()
{
	static PangoFontDescription *font_desc;
	GtkWidget *dialog;
	GtkWidget *vbox, *hbox;
	GtkWidget *icon, *table, *label;
	GtkWidget *msg_title;
	GtkWidget *entry_new1, *entry_new2;
	GtkWidget *confirm_area;
	GtkWidget *ok_button, *cancel_button;
	struct _ctx *ctx;

	dialog = gtk_dialog_new();

	gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
	gtk_window_set_default_size(GTK_WINDOW(dialog), 375, 100);
	gtk_window_set_title(GTK_WINDOW(dialog), "");

	MANAGE_WINDOW_SIGNALS_CONNECT(dialog);

	vbox = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	gtk_box_set_spacing(GTK_BOX(vbox), 14);
	hbox = gtk_hbox_new(FALSE, 12);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 5);
	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	icon = gtk_image_new_from_stock(GTK_STOCK_DIALOG_AUTHENTICATION,
			GTK_ICON_SIZE_DIALOG);
	gtk_misc_set_alignment(GTK_MISC(icon), 0.5, 0.0);
	gtk_box_pack_start(GTK_BOX(hbox), icon, FALSE, FALSE, 0);

	vbox = gtk_vbox_new(FALSE, 12);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);
	gtk_widget_show(vbox);

	msg_title = gtk_label_new(_("Changing master password"));
	gtk_misc_set_alignment(GTK_MISC(msg_title), 0, 0.5);
	gtk_label_set_justify(GTK_LABEL(msg_title), GTK_JUSTIFY_LEFT);
	gtk_label_set_use_markup (GTK_LABEL (msg_title), TRUE);
	gtk_box_pack_start(GTK_BOX(vbox), msg_title, FALSE, FALSE, 0);
	gtk_label_set_line_wrap(GTK_LABEL(msg_title), TRUE);
	if (!font_desc) {
		gint size;

		size = pango_font_description_get_size
			(gtk_widget_get_style(msg_title)->font_desc);
		font_desc = pango_font_description_new();
		pango_font_description_set_weight
			(font_desc, PANGO_WEIGHT_BOLD);
		pango_font_description_set_size
			(font_desc, size * PANGO_SCALE_LARGE);
	}
	if (font_desc)
		gtk_widget_modify_font(msg_title, font_desc);

	label = gtk_label_new(
        _("If a master password is currently active the\n"
        "current password is required to change password.\n"
        "After pressing the 'Ok' button you will be prompted.")
	);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	table = gtk_table_new(2, 2, FALSE);

	label = gtk_label_new(_("New password:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 0, 1,
			GTK_EXPAND | GTK_FILL, 0, 0, 0);

	entry_new1 = gtk_entry_new();
	gtk_entry_set_visibility(GTK_ENTRY(entry_new1), FALSE);
	gtk_table_attach(GTK_TABLE(table), entry_new1, 1, 2, 0, 1,
			GTK_FILL | GTK_EXPAND, 0, 0, 0);

	label = gtk_label_new(_("Confirm password:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2,
			GTK_EXPAND | GTK_FILL, 0, 0, 0);

	entry_new2 = gtk_entry_new();
	gtk_entry_set_visibility(GTK_ENTRY(entry_new2), FALSE);
	gtk_table_attach(GTK_TABLE(table), entry_new2, 1, 2, 1, 2,
			GTK_FILL | GTK_EXPAND, 0, 0, 0);

	gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);

	gtkut_stock_button_set_create(&confirm_area,
			&cancel_button, GTK_STOCK_CANCEL,
			&ok_button, GTK_STOCK_OK,
			NULL, NULL);

	gtk_box_pack_end(GTK_BOX(gtk_dialog_get_action_area(GTK_DIALOG(dialog))),
			confirm_area, FALSE, FALSE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(confirm_area), 5);

	gtk_widget_grab_default(ok_button);

	g_signal_connect(G_OBJECT(entry_new1), "activate",
			G_CALLBACK(entry_new1_activated), entry_new2);
	gtk_entry_set_activates_default(GTK_ENTRY(entry_new2), TRUE);

	ctx = g_new(struct _ctx, 1);
	ctx->done = FALSE;
	ctx->dialog = dialog;
	ctx->entry_new1 = entry_new1;
	ctx->entry_new2 = entry_new2;

	g_signal_connect(G_OBJECT(ok_button), "clicked",
			G_CALLBACK(ok_button_clicked), ctx);
	g_signal_connect(G_OBJECT(cancel_button), "clicked",
			G_CALLBACK(cancel_button_clicked), ctx);

	g_signal_connect(G_OBJECT(dialog), "destroy",
			G_CALLBACK(dialog_destroy), ctx);

	gtk_widget_show_all(gtk_dialog_get_content_area(GTK_DIALOG(dialog)));
	gtk_window_present(GTK_WINDOW(dialog));

	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
	manage_window_set_transient(GTK_WINDOW(dialog));

	while (!ctx->done)
		gtk_main_iteration();

	manage_window_focus_out(dialog, NULL, NULL);

	if (ctx->dialog != NULL)
		gtk_widget_destroy(ctx->dialog);

	GTK_EVENTS_FLUSH();

	g_free(ctx);
}

#endif /* !PASSWORD_CRYPTO_OLD */
