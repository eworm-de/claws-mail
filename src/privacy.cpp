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

void PrivacySystem::freePrivacyData(PrivacyData *data)
{
	g_free(data);
}

gboolean PrivacySystem::isSigned(MimeInfo *mimeinfo)
{
	return FALSE;
}

gint PrivacySystem::checkSignature(MimeInfo *mimeinfo)
{
	return -1;
}

SignatureStatus PrivacySystem::getSigStatus(MimeInfo *mimeinfo)
{
	return SIGNATURE_CHECK_FAILED;
}

gchar *PrivacySystem::getSigInfoShort(MimeInfo *mimeinfo)
{
	return g_strdup(_("Error"));
}

gchar *PrivacySystem::getSigInfoFull(MimeInfo *mimeinfo)
{
	return g_strdup(_("Error"));
}

gboolean PrivacySystem::isEncrypted(MimeInfo *mimeinfo)
{
	return FALSE;
}

MimeInfo *PrivacySystem::decrypt(MimeInfo *mimeinfo)
{
	return NULL;
}

gboolean PrivacySystem::canSign()
{
	return FALSE;
}

gboolean PrivacySystem::sign(MimeInfo *mimeinfo, PrefsAccount *account)
{
	return FALSE;
}

gboolean PrivacySystem::canEncrypt()
{
	return FALSE;
}

gchar *PrivacySystem::getEncryptData(GSList *recp_names)
{
	return NULL;
}

gboolean PrivacySystem::encrypt(MimeInfo *mimeinfo, const gchar *encrypt_data)
{
	return FALSE;
}

/**
 * Register a new Privacy System
 *
 * \param system The Privacy System that should be registered
 */
void privacy_register_system(PrivacySystem *system)
{
	systems = g_slist_append(systems, system);
}

/**
 * Unregister a new Privacy System. The system must not be in
 * use anymore when it is unregistered.
 *
 * \param system The Privacy System that should be unregistered
 */
void privacy_unregister_system(PrivacySystem *system)
{
	systems = g_slist_remove(systems, system);
}

/**
 * Free a PrivacyData of a PrivacySystem
 *
 * \param privacydata The data to free
 */
void privacy_free_privacydata(PrivacyData *privacydata)
{
	g_return_if_fail(privacydata != NULL);

	((PrivacySystem *) privacydata->system)->freePrivacyData(privacydata);
}

/**
 * Check if a MimeInfo is signed with one of the available
 * privacy system. If a privacydata is set in the MimeInfo
 * it will directory return the return value by the system
 * set in the privacy data or check all available privacy
 * systems otherwise.
 *
 * \return True if the MimeInfo has a signature
 */
gboolean privacy_mimeinfo_is_signed(MimeInfo *mimeinfo)
{
	GSList *cur;
	g_return_val_if_fail(mimeinfo != NULL, FALSE);

	if (mimeinfo->privacy != NULL) {
		PrivacySystem *system = (PrivacySystem *) (mimeinfo->privacy->system);

		return system->isSigned(mimeinfo);
	}

	for(cur = systems; cur != NULL; cur = g_slist_next(cur)) {
		PrivacySystem *system = (PrivacySystem *) cur->data;

		if(system->isSigned(mimeinfo))
			return TRUE;
	}

	return FALSE;
}

/**
 * Check the signature of a MimeInfo. privacy_mimeinfo_is_signed
 * should be called before otherwise it is done by this function.
 * If the MimeInfo is not signed an error code will be returned.
 *
 * \return Error code indicating the result of the check,
 *         < 0 if an error occured
 */
gint privacy_mimeinfo_check_signature(MimeInfo *mimeinfo)
{
	PrivacySystem *system;

	g_return_val_if_fail(mimeinfo != NULL, -1);

	if (mimeinfo->privacy == NULL)
		privacy_mimeinfo_is_signed(mimeinfo);
	
	if (mimeinfo->privacy == NULL)
		return -1;
	
	system = (PrivacySystem *) mimeinfo->privacy->system;

	return system->checkSignature(mimeinfo);
}

SignatureStatus privacy_mimeinfo_get_sig_status(MimeInfo *mimeinfo)
{
	PrivacySystem *system;

	g_return_val_if_fail(mimeinfo != NULL, SIGNATURE_CHECK_FAILED);

	if (mimeinfo->privacy == NULL)
		privacy_mimeinfo_is_signed(mimeinfo);
	
	if (mimeinfo->privacy == NULL)
		return SIGNATURE_UNCHECKED;
	
	system = (PrivacySystem *) mimeinfo->privacy->system;
	
	return system->getSigStatus(mimeinfo);
}

gchar *privacy_mimeinfo_sig_info_short(MimeInfo *mimeinfo)
{
	PrivacySystem *system;

	g_return_val_if_fail(mimeinfo != NULL, NULL);

	if (mimeinfo->privacy == NULL)
		privacy_mimeinfo_is_signed(mimeinfo);
	
	if (mimeinfo->privacy == NULL)
		return g_strdup(_("No signature found"));
	
	system = (PrivacySystem *) mimeinfo->privacy->system;

	return system->getSigInfoShort(mimeinfo);
}

gchar *privacy_mimeinfo_sig_info_full(MimeInfo *mimeinfo)
{
	PrivacySystem *system;

	g_return_val_if_fail(mimeinfo != NULL, NULL);

	if (mimeinfo->privacy == NULL)
		privacy_mimeinfo_is_signed(mimeinfo);
	
	if (mimeinfo->privacy == NULL)
		return g_strdup(_("No signature found"));
	
	system = (PrivacySystem *) mimeinfo->privacy->system;

	return system->getSigInfoFull(mimeinfo);
}

gboolean privacy_mimeinfo_is_encrypted(MimeInfo *mimeinfo)
{
	GSList *cur;
	g_return_val_if_fail(mimeinfo != NULL, FALSE);

	for(cur = systems; cur != NULL; cur = g_slist_next(cur)) {
		PrivacySystem *system = (PrivacySystem *) cur->data;

		if(system->isEncrypted(mimeinfo))
			return TRUE;
	}

	return FALSE;
}

static gint decrypt(MimeInfo *mimeinfo, PrivacySystem *system)
{
	MimeInfo *decryptedinfo, *parentinfo;
	gint childnumber;
	
	decryptedinfo = system->decrypt(mimeinfo);
	if (decryptedinfo == NULL)
		return -1;

	parentinfo = procmime_mimeinfo_parent(mimeinfo);
	childnumber = g_node_child_index(parentinfo->node, mimeinfo);
	
	procmime_mimeinfo_free_all(mimeinfo);

	g_node_insert(parentinfo->node, childnumber, decryptedinfo->node);

	return 0;
}

gint privacy_mimeinfo_decrypt(MimeInfo *mimeinfo)
{
	GSList *cur;
	g_return_val_if_fail(mimeinfo != NULL, FALSE);

	for(cur = systems; cur != NULL; cur = g_slist_next(cur)) {
		PrivacySystem *system = (PrivacySystem *) cur->data;

		if(system->isEncrypted(mimeinfo))
			return decrypt(mimeinfo, system);
	}

	return -1;
}

GSList *privacy_get_system_ids()
{
	GSList *cur;
	GSList *ret = NULL;

	for(cur = systems; cur != NULL; cur = g_slist_next(cur)) {
		PrivacySystem *system = (PrivacySystem *) cur->data;

		ret = g_slist_append(ret, g_strdup(system->getId()));
	}

	return ret;
}

static PrivacySystem *privacy_get_system(const gchar *id)
{
	GSList *cur;

	g_return_val_if_fail(id != NULL, NULL);

	for(cur = systems; cur != NULL; cur = g_slist_next(cur)) {
		PrivacySystem *system = (PrivacySystem *) cur->data;

		if(strcmp(id, system->getId()) == 0)
			return system;
	}

	return NULL;
}

const gchar *privacy_system_get_name(const gchar *id)
{
	PrivacySystem *system;

	g_return_val_if_fail(id != NULL, NULL);

	system = privacy_get_system(id);
	if (system == NULL)
		return NULL;

	return system->getName();
}

gboolean privacy_system_can_sign(const gchar *id)
{
	PrivacySystem *system;

	g_return_val_if_fail(id != NULL, FALSE);

	system = privacy_get_system(id);
	if (system == NULL)
		return FALSE;

	return system->canSign();
}

gboolean privacy_system_can_encrypt(const gchar *id)
{
	PrivacySystem *system;

	g_return_val_if_fail(id != NULL, FALSE);

	system = privacy_get_system(id);
	if (system == NULL)
		return FALSE;

	return system->canEncrypt();
}

gboolean privacy_sign(const gchar *id, MimeInfo *target, PrefsAccount *account)
{
	PrivacySystem *system;

	g_return_val_if_fail(id != NULL, FALSE);
	g_return_val_if_fail(target != NULL, FALSE);

	system = privacy_get_system(id);
	if (system == NULL)
		return FALSE;
	if (!system->canSign())
		return FALSE;

	return system->sign(target, account);
}

gchar *privacy_get_encrypt_data(const gchar *id, GSList *recp_names)
{
	PrivacySystem *system;

	g_return_val_if_fail(id != NULL, NULL);
	g_return_val_if_fail(recp_names != NULL, NULL);

	system = privacy_get_system(id);
	if (system == NULL)
		return NULL;
	if (!system->canEncrypt())
		return NULL;

	return system->getEncryptData(recp_names);
}

gboolean privacy_encrypt(const gchar *id, MimeInfo *mimeinfo, const gchar *encdata)
{
	PrivacySystem *system;

	g_return_val_if_fail(id != NULL, FALSE);
	g_return_val_if_fail(mimeinfo != NULL, FALSE);
	g_return_val_if_fail(encdata != NULL, FALSE);

	system = privacy_get_system(id);
	if (system == NULL)
		return FALSE;
	if (!system->canEncrypt())
		return FALSE;

	return system->encrypt(mimeinfo, encdata);
}
