/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2003 Hiroyuki Yamamoto & the Sylpheed-Claws team
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

#include <glib.h>

#include "intl.h"
#include "privacy.h"
#include "procmime.h"

static GSList *systems = NULL;

void privacy_register_system(PrivacySystem *system)
{
	systems = g_slist_append(systems, system);
}

void privacy_unregister_system(PrivacySystem *system)
{
	systems = g_slist_remove(systems, system);
}

void privacy_free_privacydata(PrivacyData *privacydata)
{
	g_return_if_fail(privacydata != NULL);

	privacydata->system->free_privacydata(privacydata);
}

gboolean privacy_mimeinfo_is_signed(MimeInfo *mimeinfo)
{
	GSList *cur;
	g_return_val_if_fail(mimeinfo != NULL, FALSE);

	if (mimeinfo->privacy != NULL) {
		PrivacySystem *system = mimeinfo->privacy->system;

		if (system->is_signed != NULL)
			return system->is_signed(mimeinfo);
		else
			return FALSE;
	}

	for(cur = systems; cur != NULL; cur = g_slist_next(cur)) {
		PrivacySystem *system = (PrivacySystem *) cur->data;

		if(system->is_signed != NULL && system->is_signed(mimeinfo))
			return TRUE;
	}

	return FALSE;
}

gint privacy_mimeinfo_check_signature(MimeInfo *mimeinfo)
{
	PrivacySystem *system;

	g_return_val_if_fail(mimeinfo != NULL, -1);

	if (mimeinfo->privacy == NULL)
		privacy_mimeinfo_is_signed(mimeinfo);
	
	if (mimeinfo->privacy == NULL)
		return -1;
	
	system = mimeinfo->privacy->system;
	if (system->check_signature == NULL)
		return -1;
	
	return system->check_signature(mimeinfo);
}

SignatureStatus privacy_mimeinfo_get_sig_status(MimeInfo *mimeinfo)
{
	PrivacySystem *system;

	g_return_val_if_fail(mimeinfo != NULL, -1);

	if (mimeinfo->privacy == NULL)
		privacy_mimeinfo_is_signed(mimeinfo);
	
	if (mimeinfo->privacy == NULL)
		return SIGNATURE_UNCHECKED;
	
	system = mimeinfo->privacy->system;
	if (system->get_sig_status == NULL)
		return SIGNATURE_UNCHECKED;
	
	return system->get_sig_status(mimeinfo);
}

gchar *privacy_mimeinfo_sig_info_short(MimeInfo *mimeinfo)
{
	PrivacySystem *system;

	g_return_val_if_fail(mimeinfo != NULL, NULL);

	if (mimeinfo->privacy == NULL)
		privacy_mimeinfo_is_signed(mimeinfo);
	
	if (mimeinfo->privacy == NULL)
		return g_strdup(_("No signature found"));
	
	system = mimeinfo->privacy->system;
	if (system->get_sig_info_short == NULL)
		return g_strdup(_("No information available"));
	
	return system->get_sig_info_short(mimeinfo);
}

gchar *privacy_mimeinfo_sig_info_full(MimeInfo *mimeinfo)
{
	PrivacySystem *system;

	g_return_val_if_fail(mimeinfo != NULL, NULL);

	if (mimeinfo->privacy == NULL)
		privacy_mimeinfo_is_signed(mimeinfo);
	
	if (mimeinfo->privacy == NULL)
		return g_strdup(_("No signature found"));
	
	system = mimeinfo->privacy->system;
	if (system->get_sig_info_full == NULL)
		return g_strdup(_("No information available"));
	
	return system->get_sig_info_full(mimeinfo);
}

gboolean privacy_mimeinfo_is_encrypted(MimeInfo *mimeinfo)
{
	g_return_val_if_fail(mimeinfo != NULL, FALSE);

	return FALSE;
}
