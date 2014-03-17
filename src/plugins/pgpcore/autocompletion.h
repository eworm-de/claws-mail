/*
 * PGP/Core keyring autocompletion
 *
 * Copyright (C) 2014 Christian Hesse <mail@eworm.de> and the Claws Mail team
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

#ifndef AUTOCOMPLETION_H
#define AUTOCOMPLETION_H

gboolean autocompletion_done(void);

gint autocompletion_init(gchar ** error);

#endif /* AUTOCOMPLETION_H */
