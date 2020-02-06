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

#include "folderpropertiestype.h"
#include "accounttype.h"

#include <structmember.h>

typedef struct {
    PyObject_HEAD
    FolderItemPrefs *folderitem_prefs;
} clawsmail_FolderPropertiesObject;

static int FolderProperties_init(clawsmail_FolderPropertiesObject *self, PyObject *args, PyObject *kwds)
{
  self->folderitem_prefs = NULL;
  return 0;
}

static void FolderProperties_dealloc(clawsmail_FolderPropertiesObject* self)
{
  Py_TYPE(self)->tp_free((PyObject*)self);
}

static PyObject* get_default_account(clawsmail_FolderPropertiesObject *self, void *closure)
{
  if(self->folderitem_prefs && self->folderitem_prefs->enable_default_account) {
    PrefsAccount *account;
    account = account_find_from_id(self->folderitem_prefs->default_account);
    if(account) {
      return clawsmail_account_new(account);
    }
  }
  Py_RETURN_NONE;
}

static PyGetSetDef FolderProperties_getset[] = {
    {"default_account", (getter)get_default_account, (setter)NULL,
     "default_account - the default account when composing from this folder", NULL},

    {NULL}
};

static PyTypeObject clawsmail_FolderPropertiesType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "clawsmail.FolderProperties", /* tp_name*/
    sizeof(clawsmail_FolderPropertiesObject), /* tp_basicsize*/
    0,                         /* tp_itemsize*/
    (destructor)FolderProperties_dealloc, /* tp_dealloc*/
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
    0,                         /* tp_str*/
    0,                         /* tp_getattro*/
    0,                         /* tp_setattro*/
    0,                         /* tp_as_buffer*/
    Py_TPFLAGS_DEFAULT,        /* tp_flags*/
    "FolderProperties objects.\n\n" /* tp_doc */
    "Do not construct objects of this type yourself.",
    0,                         /* tp_traverse */
    0,                         /* tp_clear */
    0,                         /* tp_richcompare */
    0,                         /* tp_weaklistoffset */
    0,                         /* tp_iter */
    0,                         /* tp_iternext */
    0,                         /* tp_methods */
    0,                         /* tp_members */
    FolderProperties_getset,   /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)FolderProperties_init, /* tp_init */
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

gboolean cmpy_add_folderproperties(PyObject *module)
{
  clawsmail_FolderPropertiesType.tp_new = PyType_GenericNew;
  if(PyType_Ready(&clawsmail_FolderPropertiesType) < 0)
    return FALSE;

  Py_INCREF(&clawsmail_FolderPropertiesType);
  return (PyModule_AddObject(module, "FolderProperties", (PyObject*)&clawsmail_FolderPropertiesType) == 0);
}

static gboolean update_members(clawsmail_FolderPropertiesObject *self, FolderItemPrefs *folderitem_prefs)
{
  self->folderitem_prefs = folderitem_prefs;
  return TRUE;
}

PyObject* clawsmail_folderproperties_new(FolderItemPrefs *folderitem_prefs)
{
  clawsmail_FolderPropertiesObject *ff;

  if(!folderitem_prefs)
    return NULL;

  ff = (clawsmail_FolderPropertiesObject*) PyObject_CallObject((PyObject*) &clawsmail_FolderPropertiesType, NULL);
  if(!ff)
    return NULL;

  if(update_members(ff, folderitem_prefs))
    return (PyObject*)ff;
  else {
    Py_XDECREF(ff);
    return NULL;
  }
}
