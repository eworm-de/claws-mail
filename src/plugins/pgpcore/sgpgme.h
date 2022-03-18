/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2021 the Claws Mail team
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

#ifndef SGPGME_H
#define SGPGME_H 1

#include <gpgme.h>

#include "privacy.h"

#define GPGERR_BUFSIZE 128

typedef struct _DetachedSigTaskData
{
	gpgme_protocol_t protocol;
	gchar *boundary;
	gchar *text_filename;
	gchar *sig_filename;
	guint sig_offset;
	guint sig_length;
	EncodingType sig_encoding;
	gchar *(*get_canonical_content)(FILE *, const gchar *);
} DetachedSigTaskData;

void sgpgme_init(void);
void sgpgme_done(void);

void cm_free_detached_sig_task_data(gpointer data);

void cm_check_detached_sig(GTask *task,
	gpointer source_object,
	gpointer _task_data,
	GCancellable *cancellable);

gint cm_check_detached_sig_async(MimeInfo *mimeinfo,
	GCancellable *cancellable,
	GAsyncReadyCallback callback,
	gpointer user_data,
	gpgme_protocol_t protocol,
	gchar *(*get_canonical_content)(FILE *, const gchar *));

gpgme_verify_result_t sgpgme_verify_signature	(gpgme_ctx_t ctx,
				    	 gpgme_data_t sig,
				    	 gpgme_data_t plain,
					 gpgme_data_t dummy);
SignatureStatus sgpgme_sigstat_gpgme_to_privacy
					(gpgme_ctx_t ctx,
					 gpgme_verify_result_t status);
gchar *sgpgme_sigstat_info_short	(gpgme_ctx_t ctx,
					 gpgme_verify_result_t status);
gchar *sgpgme_sigstat_info_full		(gpgme_ctx_t ctx,
					 gpgme_verify_result_t status);
gpgme_data_t sgpgme_data_from_mimeinfo	(MimeInfo *mimeinfo);
gpgme_data_t sgpgme_decrypt_verify	(gpgme_data_t cipher, 
					 gpgme_verify_result_t *status,
					 gpgme_ctx_t ctx);
gchar *sgpgme_get_encrypt_data		(GSList *recp_names,
					 gpgme_protocol_t proto);
gboolean sgpgme_setup_signers(gpgme_ctx_t ctx, PrefsAccount *account,
			      const gchar *from_addr);
void sgpgme_check_create_key(void);
gboolean sgpgme_has_secret_key(void);
void sgpgme_create_secret_key(PrefsAccount *account, gboolean ask_create);
void *sgpgme_data_release_and_get_mem(gpgme_data_t data, size_t *len);

gpgme_error_t cm_gpgme_data_rewind(gpgme_data_t dh);
gchar *get_gpg_executable_name();

#endif /* SGPGME_H */
