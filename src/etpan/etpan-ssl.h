/*
 * Claws Mail -- a GTK based, lightweight, and fast e-mail client
 * Copyright (C) 1999-2012 Colin Leroy <colin@colino.net> 
 * and the Claws Mail team
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

#ifndef __ETPAN_SSL_H__
#define __ETPAN_SSL_H__

#ifdef HAVE_CONFIG_H
#include "claws-features.h"
#endif

#ifdef USE_GNUTLS
#ifdef HAVE_LIBETPAN

#include <libetpan/libetpan.h>

gboolean etpan_certificate_check(mailstream *imap_stream, const char *host, gint port, gboolean accept_if_valid);
void etpan_connect_ssl_context_cb(struct mailstream_ssl_context * ssl_context, void * data);

#endif /* USE_GNUTLS */
#endif /* HAVE_LIBETPAN */

#endif /* __ETPAN_SSL_H__ */
