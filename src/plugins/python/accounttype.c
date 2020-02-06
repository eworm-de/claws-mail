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

#ifdef HAVE_CONFIG_H
#  include "config.h"
#  include "claws-features.h"
#endif

#include "accounttype.h"

#include <structmember.h>

typedef struct {
    PyObject_HEAD
    PrefsAccount *account;
} clawsmail_AccountObject;

static int Account_init(clawsmail_AccountObject *self, PyObject *args, PyObject *kwds)
{
  self->account = NULL;
  return 0;
}


static void Account_dealloc(clawsmail_AccountObject* self)
{
  Py_TYPE(self)->tp_free((PyObject*)self);
}

static int Account_compare(clawsmail_AccountObject *obj1, clawsmail_AccountObject *obj2)
{
  if(obj1->account->account_id < obj2->account->account_id)
    return -1;
  else if(obj1->account->account_id > obj2->account->account_id)
    return 1;
  else
    return 0;
}

static PyObject* Account_str(clawsmail_AccountObject *self)
{
  if(self->account && self->account->account_name)
    return PyUnicode_FromFormat("Account: %s", self->account->account_name);
  Py_RETURN_NONE;
}

static PyObject* get_account_name(clawsmail_AccountObject *self, void *closure)
{
  if(self->account && self->account->account_name)
    return PyUnicode_FromString(self->account->account_name);
  Py_RETURN_NONE;
}

static PyObject* get_address(clawsmail_AccountObject *self, void *closure)
{
  if(self->account && self->account->address)
    return PyUnicode_FromString(self->account->address);
  Py_RETURN_NONE;
}

static PyObject* get_is_default(clawsmail_AccountObject *self, void *closure)
{
  if(self->account->is_default)
    Py_RETURN_TRUE;
  Py_RETURN_FALSE;
}

static PyGetSetDef Account_getset[] = {
    {"account_name", (getter)get_account_name, (setter)NULL,
      "account_name - name of the account", NULL},

    {"address", (getter)get_address, (setter)NULL,
     "address - address of the account", NULL},

    {"is_default", (getter)get_is_default, (setter)NULL,
     "is_default - whether this account is the default account", NULL},

    {NULL}
};

static PyTypeObject clawsmail_AccountType = {
    PyVarObject_HEAD_INIT(NULL, 0)
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
    (reprfunc)Account_str,     /* tp_str*/
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
    0,                         /* tp_methods */
    0,                         /* tp_members */
    Account_getset,            /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)Account_init,    /* tp_init */
    0,                         /* tp_alloc */
    0,                         /* tp_new */
	0,                         /* tp_free */
	0,                         /* tp_is_gc */
	0,                         /* tp_bases */
	0,                         /* tp_mro */
	0,                         /* tp_cache */
	0,                         /* tp_subclasses */
	0,                         /* tp_weaklist */
	0,                         /* tp_del */
#if ((PY_MAJOR_VERSION == 2 && PY_MINOR_VERSION >= 6) || \
     (PY_MAJOR_VERSION == 3))
    0,                         /* tp_version_tag */
#endif
#if (PY_MAJOR_VERSION == 3 && PY_MINOR_VERSION >= 4)
    0,                         /* tp_finalize */
#endif
};

gboolean cmpy_add_account(PyObject *module)
{
  clawsmail_AccountType.tp_new = PyType_GenericNew;
  if(PyType_Ready(&clawsmail_AccountType) < 0)
    return FALSE;

  Py_INCREF(&clawsmail_AccountType);
  return (PyModule_AddObject(module, "Account", (PyObject*)&clawsmail_AccountType) == 0);
}

PyObject* clawsmail_account_new(PrefsAccount *account)
{
  clawsmail_AccountObject *ff;

  if(!account)
    return NULL;

  ff = (clawsmail_AccountObject*) PyObject_CallObject((PyObject*) &clawsmail_AccountType, NULL);
  if(!ff)
    return NULL;

  ff->account = account;
  return (PyObject*)ff;
}

gboolean clawsmail_account_check(PyObject *self)
{
  return (PyObject_TypeCheck(self, &clawsmail_AccountType) != 0);
}

PrefsAccount* clawsmail_account_get_account(PyObject *self)
{
  g_return_val_if_fail(clawsmail_account_check(self), NULL);

  return ((clawsmail_AccountObject*)self)->account;
}
