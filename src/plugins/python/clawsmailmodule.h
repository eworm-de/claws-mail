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


#ifndef CLAWSMAILMODULE_H
#define CLAWSMAILMODULE_H

#include <Python.h>
#include <glib.h>

#include "compose.h"

#ifndef PyMODINIT_FUNC
# define PyMODINIT_FUNC void
#endif

PyMODINIT_FUNC initclawsmail(void);

PyObject* get_gobj_from_address(gpointer addr);
void put_composewindow_into_module(Compose *compose);

#endif /* CLAWSMAILMODULE_H */
