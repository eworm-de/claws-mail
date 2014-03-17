/*
 * PGP/Core keyring autocompletion
 *
 * Copyright (C) 2014 Christian Hesse <mail@eworm.de>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef GPGMEGTK_AUTOCOMPLETION_H
#define GPGMEGTK_AUTOCOMPLETION_H

#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>

#include <glib.h>
#include <glib/gi18n.h>

#include <gpgme.h>
#include <plugins/pgpcore/prefs_gpg.h>

#include "addr_compl.h"
#include "claws.h"
#include "hooks.h"
#include "procmsg.h"
#include "utils.h"

static gboolean pgp_autocompletion_hook(gpointer source, gpointer data);

gboolean autocompletion_done(void);

gint autocompletion_init(gchar ** error);

#endif /* GPGMEGTK_AUTOCOMPLETION_H */
