/* Python plugin for Claws-Mail
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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#  include "claws-features.h"
#endif

#include "accounttype.h"

#include <structmember.h>

typedef struct {
    PyObject_HEAD
    PyObject *account_name;
    PyObject *address;
    PrefsAccount *account;
} clawsmail_AccountObject;

static int Account_init(clawsmail_AccountObject *self, PyObject *args, PyObject *kwds)
{
  Py_INCREF(Py_None);
  self->account_name = Py_None;

  Py_INCREF(Py_None);
  self->address = Py_None;

  self->account = NULL;
  return 0;
}


static void Account_dealloc(clawsmail_AccountObject* self)
{
  Py_XDECREF(self->account_name);
  Py_XDECREF(self->address);

  self->ob_type->tp_free((PyObject*)self);
}

static PyObject* Account_str(PyObject *self)
{
  PyObject *str;
  str = PyString_FromString("Account: ");
  if(str == NULL)
    return NULL;
  PyString_ConcatAndDel(&str, PyObject_GetAttrString(self, "account_name"));

  return str;
}

static PyMethodDef Account_methods[] = {
    {NULL}
};

static PyMemberDef Account_members[] = {
    {"account_name", T_OBJECT_EX, offsetof(clawsmail_AccountObject, account_name), 0,
     "account name - name of the account"},

     {"address", T_OBJECT_EX, offsetof(clawsmail_AccountObject, address), 0,
      "address - address of the account"},

    {NULL}
};

static PyTypeObject clawsmail_AccountType = {
    PyObject_HEAD_INIT(NULL)
    0,                         /* ob_size*/
    "clawsmail.Account",       /* tp_name*/
    sizeof(clawsmail_AccountObject), /* tp_basicsize*/
    0,                         /* tp_itemsize*/
    (destructor)Account_dealloc, /* tp_dealloc*/
    0,                         /* tp_print*/
    0,                         /* tp_getattr*/
    0,                         /* tp_setattr*/
    0,                         /* tp_compare*/
    0,                         /* tp_repr*/
    0,                         /* tp_as_number*/
    0,                         /* tp_as_sequence*/
    0,                         /* tp_as_mapping*/
    0,                         /* tp_hash */
    0,                         /* tp_call*/
    Account_str,               /* tp_str*/
    0,                         /* tp_getattro*/
    0,                         /* tp_setattro*/
    0,                         /* tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,        /* tp_flags*/
    "Account objects.\n\n"     /* tp_doc */
    "Do not construct objects of this type yourself.",
    0,                         /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    Account_methods,           /* tp_methods */
    Account_members,           /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)Account_init,    /* tp_init */
    0,                         /* tp_alloc */
    0,                         /* tp_new */
};


gboolean cmpy_add_account(PyObject *module)
{
  clawsmail_AccountType.tp_new = PyType_GenericNew;
  if(PyType_Ready(&clawsmail_AccountType) < 0)
    return FALSE;

  Py_INCREF(&clawsmail_AccountType);
  return (PyModule_AddObject(module, "Account", (PyObject*)&clawsmail_AccountType) == 0);
}

static gboolean update_members(clawsmail_AccountObject *self, PrefsAccount *account)
{
  if(account->account_name) {
    Py_XDECREF(self->account_name);
    self->account_name = PyString_FromString(account->account_name);
    if(!self->account_name)
      goto err;
  }

  if(account->address) {
    Py_XDECREF(self->address);
    self->address = PyString_FromString(account->address);
    if(!self->address)
      goto err;
  }

  self->account = account;

  return TRUE;
err:
  Py_XDECREF(self->account_name);
  Py_XDECREF(self->address);
  return FALSE;
}

PyObject* clawsmail_account_new(PrefsAccount *account)
{
  clawsmail_AccountObject *ff;

  if(!account)
    return NULL;

  ff = (clawsmail_AccountObject*) PyObject_CallObject((PyObject*) &clawsmail_AccountType, NULL);
  if(!ff)
    return NULL;

  if(update_members(ff, account))
    return (PyObject*)ff;
  else {
    Py_XDECREF(ff);
    return NULL;
  }
}
