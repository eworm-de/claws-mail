/* Python plugin for Claws Mail
 * Copyright (C) 2013 Holger Berndt <hb@claws-mail.org>
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
 */

#ifndef MAILBOXTYPE_H
#define MAILBOXTYPE_H

#include <Python.h>
#include <glib.h>

#include "folder.h"

gboolean cmpy_add_mailbox(PyObject *module);

PyObject* clawsmail_mailbox_new(Folder *folder);

Folder* clawsmail_mailbox_get_folder(PyObject *self);
PyTypeObject* clawsmail_mailbox_get_type_object();

gboolean clawsmail_mailbox_check(PyObject *self);

#endif /* MAILBOXTYPE_H */
