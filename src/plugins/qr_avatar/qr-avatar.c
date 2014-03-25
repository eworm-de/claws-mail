/*
 * qr-avatar - A plugin for Claws Mail
 *
 * Copyright (C) 2014 Christian Hesse <mail@eworm.de> and the Claws Mail team
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <stdlib.h>
#include <inttypes.h>

#include <errno.h>

#include <glib.h>
#include <glib/gi18n.h>

#include <qrencode.h>

#include <common/version.h>
#include <common/claws.h>
#include <common/plugin.h>
#include <common/utils.h>
#include <common/hooks.h>
#include <common/ssl.h>
#include <procmsg.h>
#include <procheader.h>
#include <avatars.h>

#define QR_AVATAR_QR_AVATAR 4
#define QR_AVATAR_SIZE 48

static guint update_hook_id;
static guint render_hook_id;

static gboolean qr_avatar_update_hook(gpointer source, gpointer data)
{
        AvatarCaptureData *acd = (AvatarCaptureData *)source;

        debug_print("qr-avatar qr_avatar_update_hook() invoked\n");

        if (!strcmp(acd->header, "From:")) {
                gchar *addr;

                addr = g_strdup(acd->content);
                extract_address(addr);

                debug_print("qr-avatar added '%s'\n", addr);
                procmsg_msginfo_add_avatar(acd->msginfo, QR_AVATAR_QR_AVATAR, addr);
        }

        return FALSE; /* keep getting */
}

static gboolean qr_avatar_render_hook(gpointer source, gpointer data)
{
	AvatarRender *ar = (AvatarRender *)source;
	GtkWidget *image = NULL;
	gchar * text;

	QRcode *qrcode;
	unsigned char * qrdata;
	GdkPixbuf *pixbuf, *pixbuf_scaled;
	int i, j, k, rowstride, channels;
	guchar *pixel;

	debug_print("qr-avatar qr_avatar_render_hook() invoked\n");

	if ((text = procmsg_msginfo_get_avatar(ar->full_msginfo, QR_AVATAR_QR_AVATAR)) != NULL) {

		if ((qrcode = QRcode_encodeString8bit(text, 0, QR_ECLEVEL_L)) == NULL)
			return FALSE;

		qrdata = qrcode->data;

		pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8,
				qrcode->width, qrcode->width);

		pixel = gdk_pixbuf_get_pixels(pixbuf);
		rowstride = gdk_pixbuf_get_rowstride(pixbuf);
		channels = gdk_pixbuf_get_n_channels(pixbuf);

		gdk_pixbuf_fill(pixbuf, 0xffffffff);

		for (i = 0; i < qrcode->width; i++)
			for (j = 0; j < qrcode->width; j++) {
				for (k = 0; k < channels; k++)
					pixel[i * rowstride + j * channels + k] = !(*qrdata & 0x1) * 0xff;
				qrdata++;
			}

		/* claws-mail is limited to avatar with 48x48 pixels
		 * TODO: skip scaling as soon as different sizes are supported */
		pixbuf_scaled = gdk_pixbuf_scale_simple(pixbuf, QR_AVATAR_SIZE, QR_AVATAR_SIZE, GDK_INTERP_BILINEAR);

		QRcode_free(qrcode);
		g_object_unref(pixbuf);

		image = gtk_image_new_from_pixbuf(pixbuf_scaled);

		g_object_unref(pixbuf_scaled);

		if (ar->image != NULL) /* previous plugin set one */
			gtk_widget_destroy(ar->image);

		ar->image = image;

                return TRUE;
        }

        return FALSE; /* keep rendering */
}

gboolean plugin_done(void)
{
	hooks_unregister_hook(AVATAR_HEADER_UPDATE_HOOKLIST, update_hook_id);
	hooks_unregister_hook(AVATAR_IMAGE_RENDER_HOOKLIST, render_hook_id);

	debug_print("QR-Avatar plugin unloaded\n");

	return TRUE;
}

gint plugin_init(gchar **error)
{
	if (!check_plugin_version(MAKE_NUMERIC_VERSION(3,9,3,29),
				VERSION_NUMERIC, _("QR-Avatar"), error))
		return -1;

	if ((update_hook_id = hooks_register_hook(AVATAR_HEADER_UPDATE_HOOKLIST, qr_avatar_update_hook, NULL)) == -1)
	{
		*error = g_strdup(_("Failed to register QR-Avatar update hook"));
		return -1;
	}

	if ((render_hook_id = hooks_register_hook(AVATAR_IMAGE_RENDER_HOOKLIST, qr_avatar_render_hook, NULL)) == -1)
	{
		*error = g_strdup(_("Failed to register QR-Avatar render hook"));
		return -1;
	}

	debug_print("QR-Avatar plugin loaded\n");

	return EXIT_SUCCESS;
}

const gchar *plugin_name(void)
{
	return _("QR-Avatar");
}

const gchar *plugin_desc(void)
{
	return _("This plugin shows QR-Code for Avatar.\n\n"
		"Copyright 2014 by Christian Hesse <mail@eworm.de>");
}

const gchar *plugin_type(void)
{
	return "Common";
}

const gchar *plugin_licence(void)
{
	return "GPL3+";
}

const gchar *plugin_version(void)
{
	return VERSION;
}

struct PluginFeature *plugin_provides(void)
{
	static struct PluginFeature features[] =
		{ {PLUGIN_UTILITY, N_("QR-Avatar")},
		  {PLUGIN_NOTHING, NULL}};
	return features;
}
