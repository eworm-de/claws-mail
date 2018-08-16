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

#ifndef FOLDERTYPE_H
#define FOLDERTYPE_H

#include <Python.h>
#include <glib.h>

#include "folder.h"



gboolean cmpy_add_folder(PyObject *module);

PyObject* clawsmail_folder_new(FolderItem *folderitem);
FolderItem* clawsmail_folder_get_item(PyObject *self);
PyTypeObject* clawsmail_folder_get_type_object();

gboolean clawsmail_folder_check(PyObject *self);

#endif /* FOLDERTYPE_H */
