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

#ifndef PGPMIME_H
#define PGPMIME_H 1

#ifdef __cplusplus
extern "C" {
#endif

void pgpmime_init(void);
void pgpmime_done(void);

#ifdef __cplusplus
}

class PGPMIME: public PrivacySystem {
        public:
                                 PGPMIME                ();

	virtual const gchar	*getId			();
	virtual const gchar	*getName		();

        virtual void             freePrivacyData        (PrivacyData *);

        virtual gboolean         isSigned               (MimeInfo *);
        virtual gint             checkSignature         (MimeInfo *);
        virtual SignatureStatus  getSigStatus           (MimeInfo *);
        virtual gchar           *getSigInfoShort        (MimeInfo *);
        virtual gchar           *getSigInfoFull         (MimeInfo *);

        virtual gboolean         isEncrypted            (MimeInfo *);
        virtual MimeInfo        *decrypt                (MimeInfo *);
};

#endif

#endif /* PGPMIME_H */
