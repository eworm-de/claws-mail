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

#ifndef PRIVACY_H
#define PRIVACY_H

typedef struct _PrivacyData PrivacyData;

typedef enum {
	SIGNATURE_UNCHECKED,
	SIGNATURE_OK,
	SIGNATURE_WARN,
	SIGNATURE_INVALID,
	SIGNATURE_CHECK_FAILED,
} SignatureStatus;

#include <glib.h>

#include "procmime.h"
#include "prefs_account.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void privacy_free_privacydata			(PrivacyData *);

gboolean privacy_mimeinfo_is_signed		(MimeInfo *);
gint privacy_mimeinfo_check_signature		(MimeInfo *);
SignatureStatus privacy_mimeinfo_get_sig_status	(MimeInfo *);
gchar *privacy_mimeinfo_sig_info_short		(MimeInfo *);
gchar *privacy_mimeinfo_sig_info_full		(MimeInfo *);

gboolean privacy_mimeinfo_is_encrypted		(MimeInfo *);
gint privacy_mimeinfo_decrypt			(MimeInfo *);

GSList *privacy_get_system_ids			();
const gchar *privacy_system_get_name		(const gchar *);
gboolean privacy_system_can_sign		(const gchar *);
gboolean privacy_system_can_encrypt		(const gchar *);

gboolean privacy_sign				(const gchar  *system,
						 MimeInfo     *mimeinfo,
						 PrefsAccount *account);
gchar *privacy_get_encrypt_data			(const gchar  *system,
						 GSList       *recp_names);
gboolean privacy_encrypt			(const gchar  *system,
						 MimeInfo     *mimeinfo,
						 const gchar  *encdata);

struct _PrivacyData {
	void		*system;
};

#ifdef __cplusplus
}

class PrivacySystem {
        public:
        virtual const gchar             *getId() = 0;
        virtual const gchar             *getName() = 0;

        virtual void             freePrivacyData        (PrivacyData *);

        virtual gboolean         isSigned               (MimeInfo *);
        virtual gint             checkSignature         (MimeInfo *);
        virtual SignatureStatus  getSigStatus           (MimeInfo *);
        virtual gchar           *getSigInfoShort        (MimeInfo *);
        virtual gchar           *getSigInfoFull         (MimeInfo *);

        virtual gboolean         isEncrypted            (MimeInfo *);
        virtual MimeInfo        *decrypt                (MimeInfo *);

	virtual gboolean 	 canSign		();
	virtual gboolean	 sign			(MimeInfo *mimeinfo,
							 PrefsAccount *account);

	virtual gboolean	 canEncrypt		();
	virtual gchar		*getEncryptData		(GSList *recp_names);
	virtual gboolean	 encrypt		(MimeInfo *mimeinfo,
							 const gchar *encrypt_data);
};

void privacy_register_system                   (PrivacySystem *system);
void privacy_unregister_system                 (PrivacySystem *system);

#endif /* __cplusplus */

#endif /* PRIVACY_H */
