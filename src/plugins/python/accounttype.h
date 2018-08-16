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

#ifndef ACCOUNTTYPE_H
#define ACCOUNTTYPE_H

#include <Python.h>
#include <glib.h>

#include "account.h"

gboolean cmpy_add_account(PyObject *module);

PyObject* clawsmail_account_new(PrefsAccount *account);

gboolean clawsmail_account_check(PyObject *self);
PrefsAccount* clawsmail_account_get_account(PyObject *self);

#endif /* ACCOUNTTYPE_H */
