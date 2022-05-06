/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 2014-2016 Ricardo Mones and the Claws Mail team
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

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "defs.h"
#include "hooks.h"
#include "gtkutils.h"
#include "procmsg.h"
#include "prefs_common.h"
#include "avatars.h"

static gulong avatar_render_hook_id = HOOK_NONE;

AvatarRender *avatars_avatarrender_new(MsgInfo *msginfo)
{
	AvatarRender *ar = g_new0(AvatarRender, 1);
	ar->full_msginfo = msginfo;
	ar->image = NULL;
	ar->type = 0;

	return ar;
}

void avatars_avatarrender_free(AvatarRender *avrender)
{
	if (avrender == NULL)
		return;

	if (avrender->image != NULL) {
		gtk_widget_destroy(avrender->image);
	}
	g_free(avrender);
}

gboolean avatars_internal_rendering_hook(gpointer source, gpointer data)
{
	AvatarRender *avatarr = (AvatarRender *)source;
	gchar *aface;

	if (!(prefs_common.enable_avatars & AVATARS_ENABLE_RENDER)) {
		debug_print("Internal rendering of avatars is disabled\n");
		return FALSE;
	}

	if (avatarr == NULL) {
		g_warning("internal rendering invoked with NULL argument");
		return FALSE;
	}

	if (avatarr->image != NULL) {
		g_warning("memory leak: image widget not destroyed");
	}

	aface = procmsg_msginfo_get_avatar(avatarr->full_msginfo, AVATAR_FACE);
	if (aface) {
		avatarr->image = face_get_from_header(aface);
		avatarr->type  = AVATAR_FACE;
	}
#if HAVE_LIBCOMPFACE
	else {
		aface = procmsg_msginfo_get_avatar(avatarr->full_msginfo, AVATAR_XFACE);
		if (aface) {
			avatarr->image = xface_get_from_header(aface);
			avatarr->type  = AVATAR_XFACE;
		}
	}
#endif
	return FALSE;
}

void avatars_init(void)
{
	if (avatar_render_hook_id != HOOK_NONE) {
		g_warning("internal avatars rendering already initialized");
		return;
	}
	avatar_render_hook_id = hooks_register_hook(AVATAR_IMAGE_RENDER_HOOKLIST, avatars_internal_rendering_hook, NULL);
	if (avatar_render_hook_id == HOOK_NONE) {
		g_warning("failed to register avatars internal rendering hook");
	}
}

void avatars_done(void)
{
	if (avatar_render_hook_id != HOOK_NONE) {
		hooks_unregister_hook(AVATAR_IMAGE_RENDER_HOOKLIST, avatar_render_hook_id);
		avatar_render_hook_id = HOOK_NONE;
	}
}
