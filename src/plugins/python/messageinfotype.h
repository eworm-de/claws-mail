/* Python plugin for Claws Mail
 * Copyright (C) 2009 Holger Berndt
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

#ifndef MESSAGEINFOTYPE_H
#define MESSAGEINFOTYPE_H

#include <Python.h>
#include <glib.h>

#include "procmsg.h"

gboolean cmpy_add_messageinfo(PyObject *module);

PyObject* clawsmail_messageinfo_new(MsgInfo *msginfo);
MsgInfo* clawsmail_messageinfo_get_msginfo(PyObject *self);
PyTypeObject* clawsmail_messageinfo_get_type_object();

#endif /* MESSAGEINFOTYPE_H */
