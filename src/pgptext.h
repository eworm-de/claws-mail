/*
 * Sylpheed -- a GTK+ based, lightweight, and fast e-mail client
 * Copyright (C) 2001 Jens Jahnke <jan0sch@gmx.net>
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

#ifndef PGPTXT_H__ 
#define PGPTXT_H__

#include <glib.h>
#include <stdio.h>

#include "procmime.h"
#include "prefs_account.h"

/*
void pgptext_disable_all (void);
void pgptext_secure_remove (const char *fname);
MimeInfo * pgptext_find_signature (MimeInfo *mimeinfo);
*/
gboolean pgptext_has_signature (MimeInfo *mimeinfo);
void pgptext_check_signature (MimeInfo *mimeinfo, FILE *fp);
int pgptext_is_encrypted (MimeInfo *mimeinfo, MsgInfo *msginfo);
void pgptext_decrypt_message (MsgInfo *msginfo, MimeInfo *mimeinfo, FILE *fp);
int pgptext_encrypt (const char *file, GSList *recp_list);
int pgptext_sign (const char *file, PrefsAccount *ac);

#endif /* PGPTEXT_H__ */
