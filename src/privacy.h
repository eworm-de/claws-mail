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
	SIGNATURE_WARN,
	SIGNATURE_INVALID,
	SIGNATURE_CHECK_FAILED,
} SignatureStatus;

#include <glib.h>

#include "procmime.h"

void privacy_register_system			(PrivacySystem *system);
void privacy_unregister_system			(PrivacySystem *system);

void privacy_free_privacydata			(PrivacyData *);

gboolean privacy_mimeinfo_is_signed		(MimeInfo *);
gint privacy_mimeinfo_check_signature		(MimeInfo *);
SignatureStatus privacy_mimeinfo_get_sig_status	(MimeInfo *);
gchar *privacy_mimeinfo_sig_info_short		(MimeInfo *);
gchar *privacy_mimeinfo_sig_info_full		(MimeInfo *);

gboolean privacy_mimeinfo_is_encrypted		(MimeInfo *);
gint privacy_mimeinfo_decrypt			(MimeInfo *);

struct _PrivacySystem {
	/** Identifier for the PrivacySystem that can use in config files */
	gchar		 *id;
	/** Human readable name for the PrivacySystem for the user interface */
	gchar		 *name;

	void		 (*free_privacydata)	(PrivacyData *);

	gboolean	 (*is_signed)		(MimeInfo *);
	gint		 (*check_signature)	(MimeInfo *);
	SignatureStatus	 (*get_sig_status)	(MimeInfo *);
	gchar		*(*get_sig_info_short)	(MimeInfo *);
	gchar		*(*get_sig_info_full)	(MimeInfo *);

	gboolean	 (*is_encrypted)	(MimeInfo *);
	MimeInfo	*(*decrypt)		(MimeInfo *);
};

struct _PrivacyData {
	PrivacySystem	*system;
};

#endif /* PRIVACY_H */
