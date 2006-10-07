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

#include <stddef.h>
#include <glib.h>
#include <glib/gi18n.h>

#include "version.h"
#include "common/sylpheed.h"
#include "mimeview.h"
#include "textview.h"
#include "sgpgme.h"
#include "prefs_common.h"
#include "prefs_gpg.h"
#include "plugin.h"

typedef struct _PgpViewer PgpViewer;

static MimeViewerFactory pgp_viewer_factory;

struct _PgpViewer
{
	MimeViewer	 mimeviewer;
	TextView	*textview;	
};

static gchar *content_types[] = 
	{"application/pgp-signature", NULL};

static GtkWidget *pgp_get_widget(MimeViewer *_viewer)
{
	PgpViewer *viewer = (PgpViewer *) _viewer;

	debug_print("pgp_get_widget\n");

	return GTK_WIDGET(viewer->textview->vbox);
}

static void pgpview_show_mime_part(TextView *textview, MimeInfo *partinfo)
{
	GtkTextView *text;
	GtkTextBuffer *buffer;
	GtkTextIter iter;
	gpgme_data_t sigdata = NULL;
	gpgme_verify_result_t sigstatus = NULL;
	gpgme_ctx_t ctx = NULL;
	gpgme_key_t key = NULL;
	gpgme_signature_t sig = NULL;
	gpgme_error_t err = 0;
	if (!partinfo) return;

	
	textview_set_font(textview, NULL);
	textview_clear(textview);

	text = GTK_TEXT_VIEW(textview->text);
	buffer = gtk_text_view_get_buffer(text);
	gtk_text_buffer_get_start_iter(buffer, &iter);

	err = gpgme_new (&ctx);
	if (err) {
		debug_print("err : %s\n", gpgme_strerror(err));
		textview_show_mime_part(textview, partinfo);
		return;
	}
	
	sigdata = sgpgme_data_from_mimeinfo(partinfo);
	if (!sigdata) {
		g_warning("no sigdata");
		textview_show_mime_part(textview, partinfo);
		return;
	}
	sigstatus = sgpgme_verify_signature(ctx, sigdata, sigdata, NULL);
	if (!sigstatus || sigstatus == GINT_TO_POINTER(-GPG_ERR_SYSTEM_ERROR)) {
		g_warning("no sigstatus");
		textview_show_mime_part(textview, partinfo);
		return;
	}
	sig = sigstatus->signatures;
	if (!sig) {
		g_warning("no sig");
		textview_show_mime_part(textview, partinfo);
		return;
	}
	gpgme_get_key(ctx, sig->fpr, &key, 0);
	TEXTVIEW_INSERT(_("\n  Importing key ID "));
	TEXTVIEW_INSERT(sig->fpr);
	TEXTVIEW_INSERT(":\n\n");
	if (!key) {
		if (prefs_common.work_offline) {
			gchar *cmd = g_strdup_printf("gpg --recv-keys %s", sig->fpr);
			TEXTVIEW_INSERT(_("   This key is not in your keyring.\n"));
			TEXTVIEW_INSERT(_("   It should be possible to import it when working online,\n"));
			TEXTVIEW_INSERT(_("   or with the following command: \n\n     "));
			TEXTVIEW_INSERT(cmd);
			g_free(cmd);
		} else {
#ifndef G_OS_WIN32
			gchar *cmd = g_strdup_printf("gpg --recv-keys %s", sig->fpr);
			int res = 0;
			GTK_EVENTS_FLUSH();
			res = system(cmd);
			if (res == 0) {
				TEXTVIEW_INSERT(_("   This key has been imported to your keyring.\n"));
			} else {
				TEXTVIEW_INSERT(_("   This key couldn't be imported to your keyring.\n"));
				TEXTVIEW_INSERT(_("   You can try to import it manually with the command:\n\n     "));
				TEXTVIEW_INSERT(cmd);
			}
			g_free(cmd);
#else
			TEXTVIEW_INSERT(_("   This key is not in your keyring.\n"));
			TEXTVIEW_INSERT(_("   Key import isn't implemented in Windows.\n"));
#endif
		}
		return;
	} else {
		TEXTVIEW_INSERT(_("   This key is already in your keyring.\n"));

	}
	gpgme_data_release(sigdata);
	gpgme_release(ctx);
	textview_show_icon(textview, GTK_STOCK_DIALOG_AUTHENTICATION);
}


static void pgp_show_mimepart(MimeViewer *_viewer,
				const gchar *infile,
				MimeInfo *partinfo)
{
	PgpViewer *viewer = (PgpViewer *)_viewer;
	debug_print("pgp_show_mimepart\n");
	viewer->textview->messageview = _viewer->mimeview->messageview;
	pgpview_show_mime_part(viewer->textview, partinfo);
}

static void pgp_clear_viewer(MimeViewer *_viewer)
{
	PgpViewer *viewer = (PgpViewer *)_viewer;
	debug_print("pgp_clear_viewer\n");
	textview_clear(viewer->textview);
}

static void pgp_destroy_viewer(MimeViewer *_viewer)
{
	PgpViewer *viewer = (PgpViewer *)_viewer;
	debug_print("pgp_destroy_viewer\n");
	textview_destroy(viewer->textview);
}

static MimeViewer *pgp_viewer_create(void)
{
	PgpViewer *viewer;

	debug_print("pgp_viewer_create\n");
	
	viewer = g_new0(PgpViewer, 1);
	viewer->mimeviewer.factory = &pgp_viewer_factory;
	viewer->mimeviewer.get_widget = pgp_get_widget;
	viewer->mimeviewer.show_mimepart = pgp_show_mimepart;
	viewer->mimeviewer.clear_viewer = pgp_clear_viewer;
	viewer->mimeviewer.destroy_viewer = pgp_destroy_viewer;	
	viewer->mimeviewer.get_selection = NULL;
	viewer->textview = textview_create();
	textview_init(viewer->textview);

	gtk_widget_show_all(viewer->textview->vbox);

	return (MimeViewer *) viewer;
}

static MimeViewerFactory pgp_viewer_factory =
{
	content_types,	
	0,

	pgp_viewer_create,
};

void pgp_viewer_init(void)
{
	mimeview_register_viewer_factory(&pgp_viewer_factory);
}

void pgp_viewer_done(void)
{
	mimeview_unregister_viewer_factory(&pgp_viewer_factory);

}
