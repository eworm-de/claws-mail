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

#include "mailboxtype.h"

#include <glib.h>
#include <glib/gi18n.h>

#include <structmember.h>

typedef struct {
    PyObject_HEAD
    Folder *folder;
} clawsmail_MailboxObject;

static int Mailbox_init(clawsmail_MailboxObject *self, PyObject *args, PyObject *kwds)
{
  self->folder = NULL;
  return 0;
}

static void Mailbox_dealloc(clawsmail_MailboxObject* self)
{
  self->folder = NULL;
  Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject* Mailbox_str(clawsmail_MailboxObject *self)
{
  if(self->folder && self->folder->name)
    return PyUnicode_FromFormat("Mailbox: %s", self->folder->name);
  Py_RETURN_NONE;
}

static PyObject* get_name(clawsmail_MailboxObject *self, void *closure)
{
  if(self->folder && self->folder->name)
    return PyUnicode_FromString(self->folder->name);
  Py_RETURN_NONE;
}

static PyGetSetDef Mailbox_getset[] = {
   {"name", (getter)get_name, (setter)NULL,
    "name - name of the mailbox", NULL},

   {NULL}
};

static PyTypeObject clawsmail_MailboxType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "clawsmail.Mailbox",       /* tp_name*/
    sizeof(clawsmail_MailboxObject), /* tp_basicsize*/
    0,                         /* tp_itemsize*/
    (destructor)Mailbox_dealloc, /* tp_dealloc*/
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
    (reprfunc)Mailbox_str,     /* tp_str*/
    0,                         /* tp_getattro*/
    0,                         /* tp_setattro*/
    0,                         /* tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,        /* tp_flags*/
    "Mailbox objects.\n\n"     /* tp_doc */
    "Do not construct objects of this type yourself.",
    0,                         /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    0,                         /* tp_methods */
    0,                         /* tp_members */
    Mailbox_getset,            /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)Mailbox_init,    /* tp_init */
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

gboolean cmpy_add_mailbox(PyObject *module)
{
  clawsmail_MailboxType.tp_new = PyType_GenericNew;
  if(PyType_Ready(&clawsmail_MailboxType) < 0)
    return FALSE;

  Py_INCREF(&clawsmail_MailboxType);
  return (PyModule_AddObject(module, "Mailbox", (PyObject*)&clawsmail_MailboxType) == 0);
}

PyObject* clawsmail_mailbox_new(Folder *folder)
{
  clawsmail_MailboxObject *ff;

  if(!folder)
    return NULL;

  ff = (clawsmail_MailboxObject*) PyObject_CallObject((PyObject*) &clawsmail_MailboxType, NULL);
  if(!ff)
    return NULL;

  ff->folder = folder;
  return (PyObject*)ff;
}

Folder* clawsmail_mailbox_get_folder(PyObject *self)
{
  return ((clawsmail_MailboxObject*)self)->folder;
}

PyTypeObject* clawsmail_mailbox_get_type_object()
{
  return &clawsmail_MailboxType;
}

gboolean clawsmail_mailbox_check(PyObject *self)
{
  return (PyObject_TypeCheck(self, &clawsmail_MailboxType) != 0);
}
