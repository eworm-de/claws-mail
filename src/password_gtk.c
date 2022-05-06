/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2016-2021 the Claws Mail team and Andrej Kacian
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

static void entry_activated(GtkEntry *entry, gpointer user_data)
{
	const gchar *text = gtk_entry_get_text(entry);

	if (strlen(text) > 0)
		gtk_widget_grab_focus(GTK_WIDGET(user_data));
}

struct _ctx {
	gboolean done;
	GtkWidget *dialog;
	GtkWidget *entry_old;
	GtkWidget *entry_new1;
	GtkWidget *entry_new2;
};

static void ok_button_clicked(GtkButton *button, gpointer user_data)
{
	struct _ctx *ctx = (struct _ctx *)user_data;
	const gchar *old = NULL;
	const gchar *new1 = gtk_entry_get_text(GTK_ENTRY(ctx->entry_new1));
	const gchar *new2 = gtk_entry_get_text(GTK_ENTRY(ctx->entry_new2));

	debug_print("OK button activated\n");

	/* Now we check the new passphrase - same in both entries. */
	if (strcmp(new1, new2)) {
		debug_print("passphrases do not match\n");
		alertpanel_warning(_("New passphrases do not match, try again."));
		gtk_entry_set_text(GTK_ENTRY(ctx->entry_new1), "");
		gtk_entry_set_text(GTK_ENTRY(ctx->entry_new2), "");
		gtk_widget_grab_focus(ctx->entry_new1);
		return;
	}

	/* If there is an existing primary passphrase, check for its correctness
	 * in entry_old. */
	if (primary_passphrase_is_set()
			&& ((old = gtk_entry_get_text(GTK_ENTRY(ctx->entry_old))) == NULL
				|| strlen(old) == 0 || !primary_passphrase_is_correct(old))) {
		debug_print("old passphrase incorrect\n");
		alertpanel_warning(_("Incorrect old primary passphrase entered, try again."));
		gtk_entry_set_text(GTK_ENTRY(ctx->entry_old), "");
		gtk_widget_grab_focus(ctx->entry_old);
		return;
	}

	primary_passphrase_change(old, new1);

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

void primary_passphrase_change_dialog()
{
	static PangoFontDescription *font_desc;
	GtkWidget *dialog;
	GtkWidget *vbox, *hbox;
	GtkWidget *icon, *table, *label;
	GtkWidget *msg_title;
	GtkWidget *entry_old, *entry_new1, *entry_new2;
	GtkWidget *ok_button, *cancel_button;
	struct _ctx *ctx;

	dialog = gtk_dialog_new();

	gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
	gtk_window_set_default_size(GTK_WINDOW(dialog), 375, 100);
	gtk_window_set_title(GTK_WINDOW(dialog), "");

	MANAGE_WINDOW_SIGNALS_CONNECT(dialog);

	vbox = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
	gtk_box_set_spacing(GTK_BOX(vbox), 14);
	hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
	gtk_container_set_border_width(GTK_CONTAINER(hbox), 5);
	gtk_widget_show(hbox);
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

	icon = gtk_image_new_from_icon_name("dialog-password",
			GTK_ICON_SIZE_DIALOG);
	gtk_widget_set_halign(icon, GTK_ALIGN_CENTER);
	gtk_widget_set_valign(icon, GTK_ALIGN_START);
	gtk_box_pack_start(GTK_BOX(hbox), icon, FALSE, FALSE, 0);

	vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
	gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);
	gtk_widget_show(vbox);

	msg_title = gtk_label_new(_("Changing primary passphrase"));
	gtk_label_set_xalign(GTK_LABEL(msg_title), 0.0);
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
		gtk_widget_override_font(msg_title, font_desc);

	label = gtk_label_new(
        _("If a primary passphrase is currently active, it\n"
        "needs to be entered.")
	);
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
	gtk_widget_show(label);

	table = gtk_grid_new();

	/* Old passphrase */
	label = gtk_label_new(_("Old passphrase:"));
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_grid_attach(GTK_GRID(table), label, 0, 0, 1, 1);

	entry_old = gtk_entry_new();
	gtk_entry_set_visibility(GTK_ENTRY(entry_old), FALSE);
	gtk_grid_attach(GTK_GRID(table), entry_old, 1, 0, 1, 1);
	gtk_widget_set_hexpand(entry_old, TRUE);
	gtk_widget_set_halign(entry_old, GTK_ALIGN_FILL);

	/* Separator */
	gtk_grid_attach(GTK_GRID(table), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), 0, 1, 1, 1);

	/* New passphrase */
	label = gtk_label_new(_("New passphrase:"));
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_grid_attach(GTK_GRID(table), label, 0, 2, 1, 1);

	entry_new1 = gtk_entry_new();
	gtk_entry_set_visibility(GTK_ENTRY(entry_new1), FALSE);
	gtk_grid_attach(GTK_GRID(table), entry_new1, 1, 2, 1, 1);
	gtk_widget_set_hexpand(entry_new1, TRUE);
	gtk_widget_set_halign(entry_new1, GTK_ALIGN_FILL);

	/* New passphrase again */
	label = gtk_label_new(_("Confirm passphrase:"));
	gtk_label_set_xalign(GTK_LABEL(label), 0.0);
	gtk_grid_attach(GTK_GRID(table), label, 0, 3, 1, 1);

	entry_new2 = gtk_entry_new();
	gtk_entry_set_visibility(GTK_ENTRY(entry_new2), FALSE);
	gtk_grid_attach(GTK_GRID(table), entry_new2, 1, 3, 1, 1);
	gtk_widget_set_hexpand(entry_new2, TRUE);
	gtk_widget_set_halign(entry_new2, GTK_ALIGN_FILL);

	gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);

	cancel_button = gtk_dialog_add_button(GTK_DIALOG(dialog), _("_Cancel"),
					      GTK_RESPONSE_NONE);
	ok_button = gtk_dialog_add_button(GTK_DIALOG(dialog),_("_OK"),
					  GTK_RESPONSE_NONE);
	gtk_widget_grab_default(ok_button);

	/* If no primary passphrase is set, disable the "old passphrase" entry */
	if (!primary_passphrase_is_set())
		gtk_widget_set_sensitive(entry_old, FALSE);

	g_signal_connect(G_OBJECT(entry_old), "activate",
			G_CALLBACK(entry_activated), entry_new1);
	g_signal_connect(G_OBJECT(entry_new1), "activate",
			G_CALLBACK(entry_activated), entry_new2);
	gtk_entry_set_activates_default(GTK_ENTRY(entry_new2), TRUE);

	ctx = g_new(struct _ctx, 1);
	ctx->done = FALSE;
	ctx->dialog = dialog;
	ctx->entry_old = entry_old;
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

	if (ctx->dialog != NULL)
		gtk_widget_destroy(ctx->dialog);

	GTK_EVENTS_FLUSH();

	g_free(ctx);
}

#endif /* !PASSWORD_CRYPTO_OLD */
