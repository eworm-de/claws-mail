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

typedef struct _PrivacySystem PrivacySystem;
typedef struct _PrivacyData PrivacyData;

typedef enum {
	SIGNATURE_UNCHECKED,
	SIGNATURE_OK,
	SIGNATURE_INVALID,
} SignatureStatus;

#include <glib.h>

#include "procmime.h"

void privacy_register_system		(PrivacySystem *system);
void privacy_unregister_system		(PrivacySystem *system);

void privacy_free_privacydata		(PrivacyData *);

gboolean privacy_mimeinfo_is_signed	(MimeInfo *);
const gchar *privacy_get_signer		(MimeInfo *);
SignatureStatus privacy_check_signature	(MimeInfo *);

#if 0 /* NOT YET */
gboolean privacy_mimeinfo_is_encrypted	(MimeInfo *);
gint privacy_decrypt			(MimeInfo *);
#endif

struct _PrivacySystem {
	gchar		 *name;

	void		 (*free_privacydata)	(PrivacyData *);

	gboolean	 (*is_signed)		(MimeInfo *);
	const gchar	*(*get_signer)		(MimeInfo *);
	SignatureStatus	 (*check_signature)	(MimeInfo *);

	/* NOT YET */
	gboolean	 (*is_encrypted)	(MimeInfo *);
	MimeInfo	*(*decrypt)		(MimeInfo *);
};

struct _PrivacyData {
	PrivacySystem	*system;
};

#endif /* PRIVACY_H */
